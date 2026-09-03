#include "MultiObjectHarness.h"
#include "../utils/MemoryUtils.h"
#include "../utils/QtEncoding.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileInfo>
#include <QProcess>
#include <QStringList>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cctype>
#include <cstring>
#include <numeric>
#include <random>
#include <set>
#include <sstream>

namespace {

QString qPath(const std::string& value) {
    return QString::fromUtf8(value.c_str());
}

QString quoteArg(const QString& value) {
    return QStringLiteral("\"%1\"").arg(value);
}

std::wstring toWide(const std::string& value) {
    if (value.empty()) return std::wstring();
    const int size = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
    std::wstring result(size > 1 ? static_cast<size_t>(size - 1) : 0, L'\0');
    if (size > 1) MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, &result[0], size);
    return result;
}

typedef int (*RunRaw)(const double*, int, const int*, int,
                      int, int, double, double, int, unsigned int);
typedef int (*GetResultRaw)(int, int*, int*, int*, unsigned long*,
                            int*, double*, const char**, int*, int*);
typedef int (*GetTrackRaw)(int, int, int, double*, double*);
typedef double (*GetMetricRaw)();

int safeRun(RunRaw fn, const double* doubles, int doubleCount,
            const int* ints, int intCount, const MultiObjectTestConfig& config,
            DWORD* exceptionCode) {
    __try {
        if (exceptionCode) *exceptionCode = 0;
        return fn(doubles, doubleCount, ints, intCount,
                  config.objectCount, config.stepCount, config.stepDt,
                  config.tolerance, static_cast<int>(config.schedule),
                  config.randomSeed);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        if (exceptionCode) *exceptionCode = GetExceptionCode();
        return -1;
    }
}

int safeGetResult(GetResultRaw fn, int objectId, int* baselineRc,
                  int* interleavedRc, int* exception, unsigned long* exceptionCode,
                  int* faultStep, double* deviation, const char** detail,
                  int* baselineCount, int* interleavedCount) {
    __try {
        return fn(objectId, baselineRc, interleavedRc, exception, exceptionCode,
                  faultStep, deviation, detail, baselineCount, interleavedCount);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

int safeGetTrack(GetTrackRaw fn, int objectId, int baseline, int pointIndex,
                 double* latitude, double* longitude) {
    __try {
        return fn(objectId, baseline, pointIndex, latitude, longitude);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

double safeGetMetric(GetMetricRaw fn) {
    __try {
        return fn();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0.0;
    }
}

std::string mappingBaseType(std::string type) {
    const char* qualifiers[] = {
        "const", "volatile", "struct", "class", "__restrict", "__restrict__"
    };
    for (const char* qualifier : qualifiers) {
        size_t position = std::string::npos;
        while ((position = type.find(qualifier)) != std::string::npos)
            type.erase(position, std::strlen(qualifier));
    }
    type.erase(std::remove(type.begin(), type.end(), '*'), type.end());
    type.erase(std::remove(type.begin(), type.end(), '&'), type.end());
    const size_t first = type.find_first_not_of(" \t\r\n");
    const size_t last = type.find_last_not_of(" \t\r\n");
    return first == std::string::npos ? std::string()
        : type.substr(first, last - first + 1);
}

std::string mappingVariableName(const std::string& type) {
    std::string name = "mapped_";
    for (char character : mappingBaseType(type)) {
        name += std::isalnum(static_cast<unsigned char>(character))
            ? character : '_';
    }
    return name;
}

std::string escapeCppString(const std::string& value) {
    std::string escaped;
    for (char character : value) {
        if (character == '\\' || character == '"') escaped += '\\';
        escaped += character;
    }
    return escaped;
}

std::string memberAccess(const std::string& variable,
                         const std::string& fieldPath) {
    return variable + "." + fieldPath;
}

} // namespace

MultiObjectHarness::MultiObjectHarness() = default;
MultiObjectHarness::~MultiObjectHarness() { Unload(); }

CompileResult MultiObjectHarness::CompileMapping(
    const InterfaceMappingProfile& profile,
    const std::vector<RandomVarDef>& randomVars,
    const std::string& workDir,
    const std::string& outputBaseName) {
    MultiObjectHarnessConfig config;
    config.headerPaths.push_back(profile.headerPath);
    config.randomVars = randomVars;
    config.workDir = workDir;
    config.outputBaseName = outputBaseName;
    const QString headerDirectory =
        QFileInfo(qPath(profile.headerPath)).absolutePath();
    config.includeDirs.push_back(qToUtf8(headerDirectory));

    std::set<std::string> structureTypes;
    auto collectStructures = [&](const FunctionBinding& function) {
        for (const auto& parameter : function.parameters) {
            if (parameter.source == MappingValueSource::InputStructPointer
                || parameter.source == MappingValueSource::InputStructValue
                || parameter.source == MappingValueSource::OutputStructPointer
                || parameter.source == MappingValueSource::OutputStructValue) {
                const std::string type = mappingBaseType(parameter.typeName);
                if (!type.empty()) structureTypes.insert(type);
            }
        }
    };
    collectStructures(profile.createFunction);
    collectStructures(profile.initFunction);
    collectStructures(profile.stepFunction);
    collectStructures(profile.destroyFunction);

    auto argumentExpression = [&](const ParameterBinding& parameter,
                                  bool inStep) {
        const std::string structure = mappingVariableName(parameter.typeName);
        switch (parameter.source) {
        case MappingValueSource::Handle:
            return std::string("reinterpret_cast<") + profile.handleType
                + ">(m_handle)";
        case MappingValueSource::InputStructPointer:
        case MappingValueSource::OutputStructPointer:
            return std::string("&") + structure;
        case MappingValueSource::InputStructValue:
        case MappingValueSource::OutputStructValue:
            return structure;
        case MappingValueSource::FixedValue:
            return parameter.value.empty() ? std::string("0") : parameter.value;
        case MappingValueSource::RandomVariable:
            return std::string("R.") + parameter.value;
        case MappingValueSource::DeltaTime:
            return std::string("dt");
        case MappingValueSource::StepIndex:
            return inStep ? std::string("stepIndex") : std::string("0");
        case MappingValueSource::ObjectId:
            return std::string("objectId");
        case MappingValueSource::PreviousOutput:
            return parameter.value.empty() ? std::string("0")
                : mappingVariableName(profile.outputStructType) + "."
                    + parameter.value;
        case MappingValueSource::NullPointer:
            return std::string("nullptr");
        }
        return std::string("nullptr");
    };
    auto arguments = [&](const FunctionBinding& function, bool inStep) {
        std::string result;
        for (size_t i = 0; i < function.parameters.size(); ++i) {
            if (i) result += ", ";
            result += argumentExpression(function.parameters[i], inStep);
        }
        return result;
    };
    auto fieldExpression = [](const StructFieldBinding& field, bool inStep) {
        std::string expression;
        switch (field.source) {
        case MappingValueSource::FixedValue:
            expression = field.value.empty() ? "0" : field.value;
            break;
        case MappingValueSource::RandomVariable:
            expression = "R." + field.value;
            break;
        case MappingValueSource::DeltaTime:
            expression = "dt";
            break;
        case MappingValueSource::StepIndex:
            expression = inStep ? "stepIndex" : "0";
            break;
        case MappingValueSource::ObjectId:
            expression = "objectId";
            break;
        case MappingValueSource::PreviousOutput:
            expression = field.value;
            break;
        default:
            expression = field.value.empty() ? "0" : field.value;
            break;
        }
        if (field.objectOffset != 0.0)
            expression += " + objectId * " + std::to_string(field.objectOffset);
        return expression;
    };

    std::ostringstream adapter;
    for (const auto& structure : structureTypes) {
        adapter << "static_assert(std::is_standard_layout<" << structure
                << ">::value, \"Mapped ABI structure must be standard-layout: "
                << structure << "\");\n";
    }
    adapter << "class ModelObjectAdapter {\npublic:\n"
            << " ModelObjectAdapter() : m_module(nullptr), m_handle(nullptr), "
               "m_create(nullptr), m_init(nullptr), m_step(nullptr), m_destroy(nullptr) {}\n"
            << " int Initialize(int objectId, const RandomBag& R, double dt) {\n"
            << "  m_module = LoadLibraryW(L\"" << escapeCppString(profile.dllPath)
            << "\"); if (!m_module) return -1001;\n"
            << "  m_create = reinterpret_cast<CreateFn>(GetProcAddress(m_module, \""
            << profile.createFunction.functionName << "\"));\n"
            << "  m_init = reinterpret_cast<InitFn>(GetProcAddress(m_module, \""
            << profile.initFunction.functionName << "\"));\n"
            << "  m_step = reinterpret_cast<StepFn>(GetProcAddress(m_module, \""
            << profile.stepFunction.functionName << "\"));\n"
            << "  m_destroy = reinterpret_cast<DestroyFn>(GetProcAddress(m_module, \""
            << profile.destroyFunction.functionName << "\"));\n"
            << "  if (!m_create || !m_init || !m_step || !m_destroy) return -1002;\n";
    for (const auto& field : profile.fieldBindings) {
        adapter << "  "
                << memberAccess(mappingVariableName(field.structType),
                                field.fieldPath)
                << " = " << fieldExpression(field, false) << ";\n";
    }
    adapter << "  auto created = m_create("
            << arguments(profile.createFunction, false) << ");\n"
            << "  m_handle = reinterpret_cast<void*>(created);"
               " if (!m_handle) return -1003;\n"
            << "  return static_cast<int>(m_init("
            << arguments(profile.initFunction, false) << "));\n"
            << " }\n"
            << " int Step(int stepIndex, double dt, double& outLat, double& outLon) {\n"
            << "  (void)stepIndex; (void)dt;\n";
    for (const auto& field : profile.fieldBindings) {
        if (field.source == MappingValueSource::DeltaTime
            || field.source == MappingValueSource::StepIndex) {
            adapter << "  "
                    << memberAccess(mappingVariableName(field.structType),
                                    field.fieldPath)
                    << " = " << fieldExpression(field, true) << ";\n";
        }
    }
    adapter << "  int rc = static_cast<int>(m_step("
            << arguments(profile.stepFunction, true) << "));\n"
            << "  outLat = static_cast<double>("
            << memberAccess(mappingVariableName(profile.outputStructType),
                            profile.latitudeField)
            << ");\n"
            << "  outLon = static_cast<double>("
            << memberAccess(mappingVariableName(profile.outputStructType),
                            profile.longitudeField)
            << ");\n"
            << "  return rc;\n }\n"
            << " void Destroy() { if (m_handle && m_destroy) m_destroy("
            << arguments(profile.destroyFunction, false)
            << "); m_handle = nullptr; if (m_module) FreeLibrary(m_module);"
               " m_module = nullptr; }\n"
            << "private:\n"
            << " using CreateFn = decltype(&" << profile.createFunction.functionName << ");\n"
            << " using InitFn = decltype(&" << profile.initFunction.functionName << ");\n"
            << " using StepFn = decltype(&" << profile.stepFunction.functionName << ");\n"
            << " using DestroyFn = decltype(&" << profile.destroyFunction.functionName << ");\n"
            << " HMODULE m_module; void* m_handle; CreateFn m_create;"
               " InitFn m_init; StepFn m_step; DestroyFn m_destroy;\n";
    for (const auto& structure : structureTypes)
        adapter << " " << structure << " " << mappingVariableName(structure) << "{};\n";
    adapter << "};\n";
    config.adapterCode = adapter.str();
    return Compile(config);
}

std::string MultiObjectHarness::DefaultAdapterTemplate() {
    return
        "// ModelObjectAdapter 是多对象测试与真实业务类之间的唯一适配层。\n"
        "// 可在此类中直接持有你的业务封装类，并填充厂家要求的固定参数。\n"
        "class ModelObjectAdapter {\n"
        "public:\n"
        "    ModelObjectAdapter() : m_handle(nullptr) {}\n"
        "\n"
        "    int Initialize(int objectId, const RandomBag& R, double dt) {\n"
        "        m_handle = Model_Create();\n"
        "        if (!m_handle) return -1;\n"
        "        WeaponModelParams p{};\n"
        "        p.init_lat = R.lat + objectId * 0.001;\n"
        "        p.init_lon = R.lon + objectId * 0.001;\n"
        "        p.init_alt = R.alt;\n"
        "        p.init_speed = R.speed;\n"
        "        p.init_heading = 45.0;\n"
        "        p.init_pitch = 15.0;\n"
        "        p.init_roll = 0.0;\n"
        "        p.step_dt = dt;\n"
        "        return Model_Init(m_handle, &p);\n"
        "    }\n"
        "\n"
        "    int Step(int stepIndex, double dt, double& outLat, double& outLon) {\n"
        "        (void)stepIndex; (void)dt;\n"
        "        WeaponModelOutput output{};\n"
        "        int rc = Model_Step(m_handle, &output);\n"
        "        outLat = output.lat;\n"
        "        outLon = output.lon;\n"
        "        return rc;\n"
        "    }\n"
        "\n"
        "    void Destroy() {\n"
        "        if (m_handle) Model_Destroy(m_handle);\n"
        "        m_handle = nullptr;\n"
        "    }\n"
        "\n"
        "private:\n"
        "    void* m_handle;\n"
        "};\n";
}

std::string MultiObjectHarness::DefaultUserMultiObjectTemplate() {
    return
        "// ========== 使用说明（可删除） ==========\n"
        "// 1. 无需先编译 UserMain；添加型号并勾选头文件后即可编写本段代码。\n"
        "// 2. 界面「对象数」决定创建几个实例；你只需写「单个对象」的逻辑，工具会自动循环调用。\n"
        "// 3. 每个对象从界面随机变量范围独立抽样一份 R；同一对象的 MoInit/MoStep 共用该 R。\n"
        "//    objectId：对象下标；dt/stepIndex：步长参数。\n"
        "// 4. 推荐只写下面四个 Mo* 函数即可，不必使用 ModelObjPool。\n"
        "// 5. MoStep 的 out_lat / out_lon 由工具自动采集用于二维轨迹（无需 RecordTrajectoryPoint）。\n"
        "// ==========================================\n"
        "\n"
        "// 将 WeaponObject 换成你的第三方封装类（需在型号页勾选对应头文件）\n"
        "using MoModelType = WeaponObject;\n"
        "\n"
        "// 【创建】只写 new 一个对象；工具会按界面「对象数」自动调用本函数多次\n"
        "static MoModelType* MoCreate(int objectId, const RandomBag& R) {\n"
        "    (void)objectId;\n"
        "    (void)R;\n"
        "    return new MoModelType();\n"
        "}\n"
        "\n"
        "// 【初始化】只写对一个对象的 init；工具会对每个实例各调用一次\n"
        "static int MoInit(MoModelType* obj, int objectId, const RandomBag& R, double dt) {\n"
        "    return obj->Initialize(objectId, R.lat, R.lon, R.alt, R.speed, dt);\n"
        "}\n"
        "\n"
        "// 【步进】只写对一个对象推进一步（如 obj->run()）；工具按步数×对象数自动调用\n"
        "// R：与 MoInit 相同的随机变量包，可填入第三方 Step 所需的自定义结构体字段\n"
        "// out_lat / out_lon：本步经纬度，工具自动记录并用于右侧二维轨迹预览（勿需另写采集函数）\n"
        "static int MoStep(MoModelType* obj, int objectId, int stepIndex, double dt,\n"
        "                  const RandomBag& R,\n"
        "                  double& out_lat, double& out_lon) {\n"
        "    (void)stepIndex;\n"
        "    (void)R;\n"
        "    (void)dt;\n"
        "    // 示例：第三方每步需要初始位置/速度时 ——\n"
        "    // MyStepParams in{}; in.lat0 = R.lat; in.speed0 = R.speed; in.dt = dt;\n"
        "    // int rc = obj->Step(in, out_lat, out_lon);\n"
        "    int rc = obj->Step(out_lat, out_lon);\n"
        "    return rc;\n"
        "}\n"
        "\n"
        "// 【销毁】只写销毁一个对象；工具会对每个实例各调用一次\n"
        "static void MoDestroy(MoModelType* obj, int objectId) {\n"
        "    (void)objectId;\n"
        "    obj->Shutdown();\n"
        "    delete obj;\n"
        "}\n"
        "\n"
        "// ========== 可选高级：ModelObjPool（一般不需要） ==========\n"
        "// ModelObjPool 是工具内置的「对象指针数组」，类似 std::vector<MoModelType*>。\n"
        "// 标准用法已由 MoCreate/MoInit/MoStep/MoDestroy 代劳，请勿在 Mo* 里再写一套循环。\n"
        "// 仅当你要在自定义辅助函数里手动管理多个指针时，才考虑以下写法：\n"
        "//\n"
        "// ModelObjPool<MoModelType> g_models;   // 声明一个空对象池\n"
        "//\n"
        "// g_models.CreateAll(数量, [&](int objectId) { ... });\n"
        "//   ^ CreateAll 是 ModelObjPool 的成员函数，用于按数量创建并放入池中。\n"
        "//   ^ 「数量」须自己传入整数；不要写 objectCount——该名字只在工具内部存在，\n"
        "//     你的代码里拿不到界面「对象数」。若硬要写死，须与界面设置保持一致，易出错。\n"
        "//   ^ 示例（不推荐写死 4）：g_models.CreateAll(4, [&](int objectId) {\n"
        "//         return MoCreate(objectId, R);  // 注意：此处 R 也须在你的函数参数里可见\n"
        "//     });\n"
        "//\n"
        "// MODEL_CALL_ALL(g_models, obj->Initialize(objectId, R.lat, dt));\n"
        "//   ^ 对池中每个对象执行同一条语句；obj=当前对象，objectId=下标。\n"
        "//   ^ 等价于 for 循环，与 MoInit 被工具逐个调用的效果重复，通常多余。\n";
}

std::string MultiObjectHarness::LoadModelObjectKitHeader() {
    QFileInfo self(QString::fromUtf8(__FILE__));
    QFile file(self.absoluteDir().filePath(QStringLiteral("ModelObjectKit.h")));
    if (!file.open(QIODevice::ReadOnly))
        return std::string("#error \"ModelObjectKit.h not found\"\n");
    return file.readAll().toStdString();
}

std::string MultiObjectHarness::GeneratePerObjectRandomPreamble() const {
    int doubleVarsPerObject = 0;
    int intVarsPerObject = 0;
    for (const auto& variable : m_enabledVars) {
        if (variable.type == RandomVarType::Int) ++intVarsPerObject;
        else ++doubleVarsPerObject;
    }
    std::ostringstream source;
    source << "    static const int kDoubleVarsPerObject = " << doubleVarsPerObject << ";\n"
           << "    static const int kIntVarsPerObject = " << intVarsPerObject << ";\n"
           << "    auto fillObjectRandom = [&](int objectId, RandomBag& bag) {\n";
    int doubleIndex = 0;
    int intIndex = 0;
    for (const auto& variable : m_enabledVars) {
        if (variable.type == RandomVarType::Int) {
            source << "        if (objectId * kIntVarsPerObject + " << intIndex
                   << " < ni) bag." << variable.name
                   << " = ivals[objectId * kIntVarsPerObject + " << intIndex << "];\n";
            ++intIndex;
        } else {
            source << "        if (objectId * kDoubleVarsPerObject + " << doubleIndex
                   << " < nd) bag." << variable.name
                   << " = dvals[objectId * kDoubleVarsPerObject + " << doubleIndex << "];\n";
            ++doubleIndex;
        }
    }
    source << "    };\n"
           << "    std::vector<RandomBag> objectRandoms(static_cast<size_t>(objectCount));\n"
           << "    for (int objectId = 0; objectId < objectCount; ++objectId)\n"
           << "        fillObjectRandom(objectId, objectRandoms[static_cast<size_t>(objectId)]);\n";
    return source.str();
}

std::string MultiObjectHarness::GenerateUserPoolSource(
    const UserHarnessConfig& config) const {
    std::ostringstream source;
    source << "#ifndef NOMINMAX\n#define NOMINMAX\n#endif\n"
           << "#include <windows.h>\n#include <algorithm>\n#include <chrono>\n"
           << "#include <cmath>\n#include <functional>\n#include <numeric>\n"
           << "#include <random>\n#include <string>\n#include <utility>\n"
           << "#include <vector>\n\n"
           << LoadModelObjectKitHeader() << "\n";
    for (const auto& header : config.headerPaths) {
        std::string path = header;
        for (char& character : path)
            if (character == '\\') character = '/';
        source << "#include \"" << path << "\"\n";
    }
    source << "\nstruct RandomBag {\n";
    for (const auto& variable : m_enabledVars) {
        source << "    " << (variable.type == RandomVarType::Int ? "int " : "double ")
               << variable.name << "{};\n";
    }
    if (m_enabledVars.empty()) source << "    int _placeholder;\n";
    source << "};\n\n" << config.userMainBody << "\n\n";
    source << R"CPP(
struct TrackPoint { double lat; double lon; };
struct ObjectResult {
    int baselineRc = 0;
    int interleavedRc = 0;
    int exception = 0;
    unsigned long exceptionCode = 0;
    int faultStep = -1;
    double maxDeviation = 0.0;
    std::string detail;
    std::vector<TrackPoint> baseline;
    std::vector<TrackPoint> interleaved;
};
static std::vector<ObjectResult> g_results;
static double g_maxFrameMs = 0.0;

static int SafeMoInit(decltype(MoCreate(0, std::declval<const RandomBag&>())) object,
                      int objectId, const RandomBag* random,
                      double dt, unsigned long* exceptionCode) {
    __try {
        *exceptionCode = 0;
        return MoInit(object, objectId, *random, dt);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        *exceptionCode = GetExceptionCode();
        return -32000;
    }
}
static int SafeMoStep(decltype(MoCreate(0, std::declval<const RandomBag&>())) object,
                      int objectId, int step, double dt,
                      const RandomBag* random,
                      double* lat, double* lon, unsigned long* exceptionCode) {
    __try {
        *exceptionCode = 0;
        return MoStep(object, objectId, step, dt, *random, *lat, *lon);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        *exceptionCode = GetExceptionCode();
        return -32000;
    }
}
static void SafeMoDestroy(decltype(MoCreate(0, std::declval<const RandomBag&>())) object,
                          int objectId, unsigned long* exceptionCode) {
    __try {
        *exceptionCode = 0;
        MoDestroy(object, objectId);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        *exceptionCode = GetExceptionCode();
    }
}

extern "C" __declspec(dllexport) int RunInterleavedMultiObjectTest(
    const double* dvals, int nd, const int* ivals, int ni,
    int objectCount, int stepCount, double dt, double tolerance,
    int schedule, unsigned int randomSeed) {
    if (objectCount < 1 || stepCount < 1 || dt <= 0.0) return -1;
    (void)randomSeed;
)CPP";
    source << GeneratePerObjectRandomPreamble();
    source << R"CPP(
    g_results.assign(static_cast<size_t>(objectCount), ObjectResult{});
    g_maxFrameMs = 0.0;

    for (int objectId = 0; objectId < objectCount; ++objectId) {
        auto object = MoCreate(objectId, objectRandoms[static_cast<size_t>(objectId)]);
        unsigned long exceptionCode = 0;
        if (!object) {
            g_results[objectId].baselineRc = -1003;
            g_results[objectId].detail = "创建对象失败";
            continue;
        }
        int rc = SafeMoInit(object, objectId, &objectRandoms[static_cast<size_t>(objectId)], dt, &exceptionCode);
        if (rc != 0) {
            g_results[objectId].baselineRc = rc;
            g_results[objectId].exceptionCode = exceptionCode;
            g_results[objectId].exception = exceptionCode != 0;
            g_results[objectId].detail = "基线初始化失败";
            SafeMoDestroy(object, objectId, &exceptionCode);
            continue;
        }
        for (int step = 0; step < stepCount; ++step) {
            double lat = 0.0, lon = 0.0;
            rc = SafeMoStep(object, objectId, step, dt,
                            &objectRandoms[static_cast<size_t>(objectId)], &lat, &lon, &exceptionCode);
            if (rc != 0 || exceptionCode != 0) {
                g_results[objectId].baselineRc = rc;
                g_results[objectId].exceptionCode = exceptionCode;
                g_results[objectId].exception = exceptionCode != 0;
                g_results[objectId].faultStep = step;
                g_results[objectId].detail = "基线步进失败";
                break;
            }
            g_results[objectId].baseline.push_back({lat, lon});
        }
        SafeMoDestroy(object, objectId, &exceptionCode);
    }

    using MoObject = decltype(MoCreate(0, std::declval<const RandomBag&>()));
    std::vector<MoObject> objects(static_cast<size_t>(objectCount), MoObject{});
    for (int objectId = 0; objectId < objectCount; ++objectId) {
        objects[objectId] = MoCreate(objectId, objectRandoms[static_cast<size_t>(objectId)]);
        unsigned long exceptionCode = 0;
        if (!objects[objectId]) {
            g_results[objectId].interleavedRc = -1003;
            g_results[objectId].detail = "交错创建对象失败";
            continue;
        }
        int rc = SafeMoInit(objects[objectId], objectId,
                             &objectRandoms[static_cast<size_t>(objectId)], dt, &exceptionCode);
        if (rc != 0 || exceptionCode != 0) {
            g_results[objectId].interleavedRc = rc;
            g_results[objectId].exceptionCode = exceptionCode;
            g_results[objectId].exception = exceptionCode != 0;
            g_results[objectId].detail = "交错初始化失败";
        }
    }

    std::vector<int> order(static_cast<size_t>(objectCount));
    std::iota(order.begin(), order.end(), 0);
    std::mt19937 random(randomSeed);
    for (int step = 0; step < stepCount; ++step) {
        if (schedule == 1) std::reverse(order.begin(), order.end());
        else if (schedule == 2) std::shuffle(order.begin(), order.end(), random);
        const auto frameStart = std::chrono::high_resolution_clock::now();
        for (int objectId : order) {
            ObjectResult& result = g_results[objectId];
            if (result.interleavedRc != 0 || result.exception || !objects[objectId]) continue;
            double lat = 0.0, lon = 0.0;
            unsigned long exceptionCode = 0;
            const int rc = SafeMoStep(objects[objectId], objectId, step, dt,
                                      &objectRandoms[static_cast<size_t>(objectId)],
                                      &lat, &lon, &exceptionCode);
            if (rc != 0 || exceptionCode != 0) {
                result.interleavedRc = rc;
                result.exceptionCode = exceptionCode;
                result.exception = exceptionCode != 0;
                result.faultStep = step;
                result.detail = "交错步进失败";
                continue;
            }
            result.interleaved.push_back({lat, lon});
        }
        const auto frameEnd = std::chrono::high_resolution_clock::now();
        const double frameMs = std::chrono::duration<double, std::milli>(frameEnd - frameStart).count();
        if (frameMs > g_maxFrameMs) g_maxFrameMs = frameMs;
        if (schedule == 1) std::reverse(order.begin(), order.end());
    }

    for (int objectId = 0; objectId < objectCount; ++objectId) {
        if (!objects[objectId]) continue;
        unsigned long exceptionCode = 0;
        SafeMoDestroy(objects[objectId], objectId, &exceptionCode);
        if (exceptionCode != 0) {
            g_results[objectId].exception = 1;
            g_results[objectId].exceptionCode = exceptionCode;
            g_results[objectId].detail = "销毁对象时发生异常";
        }
        const size_t count = (std::min)(g_results[objectId].baseline.size(),
                                        g_results[objectId].interleaved.size());
        for (size_t i = 0; i < count; ++i) {
            const double dLat = g_results[objectId].baseline[i].lat
                              - g_results[objectId].interleaved[i].lat;
            const double dLon = g_results[objectId].baseline[i].lon
                              - g_results[objectId].interleaved[i].lon;
            const double deviation = std::sqrt(dLat * dLat + dLon * dLon);
            if (deviation > g_results[objectId].maxDeviation)
                g_results[objectId].maxDeviation = deviation;
        }
        if (g_results[objectId].detail.empty()) {
            if (g_results[objectId].baseline.size() != g_results[objectId].interleaved.size())
                g_results[objectId].detail = "基线与交错轨迹点数不一致";
            else if (g_results[objectId].maxDeviation > tolerance)
                g_results[objectId].detail = "交错运行结果偏离单独运行基线";
            else
                g_results[objectId].detail = "状态隔离正常";
        }
    }
    return 0;
}

extern "C" __declspec(dllexport) int GetMultiObjectResult(
    int index, int* baselineRc, int* interleavedRc, int* exception,
    unsigned long* exceptionCode, int* faultStep, double* maxDeviation,
    const char** detail, int* baselineCount, int* interleavedCount) {
    if (index < 0 || index >= static_cast<int>(g_results.size())) return 0;
    const ObjectResult& result = g_results[static_cast<size_t>(index)];
    *baselineRc = result.baselineRc;
    *interleavedRc = result.interleavedRc;
    *exception = result.exception;
    *exceptionCode = result.exceptionCode;
    *faultStep = result.faultStep;
    *maxDeviation = result.maxDeviation;
    *detail = result.detail.c_str();
    *baselineCount = static_cast<int>(result.baseline.size());
    *interleavedCount = static_cast<int>(result.interleaved.size());
    return 1;
}
extern "C" __declspec(dllexport) int GetMultiObjectTrackPoint(
    int objectId, int baseline, int pointIndex, double* lat, double* lon) {
    if (objectId < 0 || objectId >= static_cast<int>(g_results.size())) return 0;
    const auto& points = baseline ? g_results[objectId].baseline : g_results[objectId].interleaved;
    if (pointIndex < 0 || pointIndex >= static_cast<int>(points.size())) return 0;
    *lat = points[pointIndex].lat;
    *lon = points[pointIndex].lon;
    return 1;
}
extern "C" __declspec(dllexport) double GetMultiObjectMaxFrameMs() {
    return g_maxFrameMs;
}
)CPP";
    return source.str();
}

CompileResult MultiObjectHarness::CompileUserPool(const UserHarnessConfig& config) {
    CompileResult result;
    Unload();
    const std::string body = config.userMainBody;
    if (body.find("MoModelType") == std::string::npos
        || body.find("MoCreate") == std::string::npos
        || body.find("MoInit") == std::string::npos
        || body.find("MoStep") == std::string::npos
        || body.find("MoDestroy") == std::string::npos) {
        result.log =
            "ERROR: 多对象代码须定义 MoModelType、MoCreate、MoInit、MoStep、MoDestroy\n";
        return result;
    }
    m_enabledVars.clear();
    for (const auto& variable : config.randomVars) {
        if (variable.enabled && !variable.name.empty())
            m_enabledVars.push_back(variable);
    }
    const QString workDir = QDir(qPath(config.workDir)).absolutePath();
    if (!QDir().mkpath(workDir)) {
        result.log = "ERROR: 无法创建多对象 Harness 输出目录\n";
        return result;
    }
    const QString base = qPath(config.outputBaseName.empty()
        ? std::string("UserMultiObjectHarness") : config.outputBaseName);
    const QString sourcePath = QDir(workDir).filePath(base + QStringLiteral(".cpp"));
    const QString dllPath = QDir(workDir).filePath(base + QStringLiteral(".dll"));

    MultiObjectHarnessConfig compileConfig;
    compileConfig.headerPaths = config.headerPaths;
    compileConfig.includeDirs = config.includeDirs;
    compileConfig.libPaths = config.libPaths;
    compileConfig.linkLibs = config.linkLibs;
    compileConfig.workDir = qToUtf8(QDir::toNativeSeparators(workDir));
    auto makeAbsolute = [](std::vector<std::string>& paths) {
        for (auto& path : paths) {
            path = qToUtf8(QDir::toNativeSeparators(
                QFileInfo(qPath(path)).absoluteFilePath()));
        }
    };
    makeAbsolute(compileConfig.headerPaths);
    makeAbsolute(compileConfig.includeDirs);
    makeAbsolute(compileConfig.libPaths);
    makeAbsolute(compileConfig.linkLibs);
    m_searchDirs = compileConfig.libPaths;
    for (const auto& lib : compileConfig.linkLibs) {
        const QString directory = QFileInfo(qPath(lib)).absolutePath();
        if (!directory.isEmpty()) m_searchDirs.push_back(qToUtf8(directory));
    }

    QFile sourceFile(sourcePath);
    if (!sourceFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        result.log = "ERROR: 无法写入多对象 Harness 源文件\n";
        return result;
    }
    sourceFile.write(QByteArray::fromStdString(GenerateUserPoolSource(config)));
    sourceFile.close();
    QFile::remove(dllPath);
    result.sourcePath = qToUtf8(sourcePath);
    if (!InvokeCl(compileConfig, result.sourcePath, qToUtf8(dllPath), result.log))
        return result;
    std::string error;
    if (!LoadCompiledDll(qToUtf8(dllPath), error)) {
        result.log += error;
        return result;
    }
    result.success = true;
    result.dllPath = qToUtf8(dllPath);
    result.log += "SUCCESS: 用户多对象 Harness 编译并加载成功\n";
    return result;
}

std::string MultiObjectHarness::GenerateSource(const MultiObjectHarnessConfig& config) const {
    std::ostringstream source;
    source << "#ifndef NOMINMAX\n#define NOMINMAX\n#endif\n"
           << "#include <windows.h>\n#include <algorithm>\n#include <chrono>\n"
           << "#include <cmath>\n#include <numeric>\n#include <random>\n"
           << "#include <string>\n#include <type_traits>\n#include <vector>\n";
    for (const auto& header : config.headerPaths) {
        source << "#include \"" << header << "\"\n";
    }
    source << "\nstruct RandomBag {\n";
    for (const auto& variable : m_enabledVars) {
        source << "    " << (variable.type == RandomVarType::Int ? "int " : "double ")
               << variable.name << "{};\n";
    }
    source << "};\n\n" << config.adapterCode << "\n";
    source << R"CPP(
struct TrackPoint { double lat; double lon; };
struct ObjectResult {
    int baselineRc = 0;
    int interleavedRc = 0;
    int exception = 0;
    unsigned long exceptionCode = 0;
    int faultStep = -1;
    double maxDeviation = 0.0;
    std::string detail;
    std::vector<TrackPoint> baseline;
    std::vector<TrackPoint> interleaved;
};
static std::vector<ObjectResult> g_results;
static double g_maxFrameMs = 0.0;

static int SafeInitialize(ModelObjectAdapter* object, int objectId,
                          const RandomBag* random, double dt, unsigned long* exceptionCode) {
    __try {
        *exceptionCode = 0;
        return object->Initialize(objectId, *random, dt);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        *exceptionCode = GetExceptionCode();
        return -32000;
    }
}
static int SafeStep(ModelObjectAdapter* object, int step, double dt,
                    double* lat, double* lon, unsigned long* exceptionCode) {
    __try {
        *exceptionCode = 0;
        return object->Step(step, dt, *lat, *lon);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        *exceptionCode = GetExceptionCode();
        return -32000;
    }
}
static void SafeDestroy(ModelObjectAdapter* object, unsigned long* exceptionCode) {
    __try {
        *exceptionCode = 0;
        object->Destroy();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        *exceptionCode = GetExceptionCode();
    }
}

extern "C" __declspec(dllexport) int RunInterleavedMultiObjectTest(
    const double* dvals, int nd, const int* ivals, int ni,
    int objectCount, int stepCount, double dt, double tolerance,
    int schedule, unsigned int randomSeed) {
    if (objectCount < 1 || stepCount < 1 || dt <= 0.0) return -1;
    (void)randomSeed;
)CPP";
    source << GeneratePerObjectRandomPreamble();
    source << R"CPP(
    g_results.assign(static_cast<size_t>(objectCount), ObjectResult{});
    g_maxFrameMs = 0.0;

    for (int objectId = 0; objectId < objectCount; ++objectId) {
        ModelObjectAdapter* object = new ModelObjectAdapter();
        unsigned long exceptionCode = 0;
        int rc = SafeInitialize(object, objectId,
                                &objectRandoms[static_cast<size_t>(objectId)], dt, &exceptionCode);
        if (rc != 0) {
            g_results[objectId].baselineRc = rc;
            g_results[objectId].exceptionCode = exceptionCode;
            g_results[objectId].exception = exceptionCode != 0;
            g_results[objectId].detail = "基线初始化失败";
        } else {
            for (int step = 0; step < stepCount; ++step) {
                double lat = 0.0, lon = 0.0;
                rc = SafeStep(object, step, dt, &lat, &lon, &exceptionCode);
                if (rc != 0 || exceptionCode != 0) {
                    g_results[objectId].baselineRc = rc;
                    g_results[objectId].exceptionCode = exceptionCode;
                    g_results[objectId].exception = exceptionCode != 0;
                    g_results[objectId].faultStep = step;
                    g_results[objectId].detail = "基线步进失败";
                    break;
                }
                g_results[objectId].baseline.push_back({lat, lon});
            }
        }
        SafeDestroy(object, &exceptionCode);
        delete object;
    }

    std::vector<ModelObjectAdapter*> objects(static_cast<size_t>(objectCount), nullptr);
    for (int objectId = 0; objectId < objectCount; ++objectId) {
        objects[objectId] = new ModelObjectAdapter();
        unsigned long exceptionCode = 0;
        int rc = SafeInitialize(objects[objectId], objectId,
                                &objectRandoms[static_cast<size_t>(objectId)], dt, &exceptionCode);
        if (rc != 0 || exceptionCode != 0) {
            g_results[objectId].interleavedRc = rc;
            g_results[objectId].exceptionCode = exceptionCode;
            g_results[objectId].exception = exceptionCode != 0;
            g_results[objectId].detail = "交错初始化失败";
        }
    }

    std::vector<int> order(static_cast<size_t>(objectCount));
    std::iota(order.begin(), order.end(), 0);
    std::mt19937 random(randomSeed);
    for (int step = 0; step < stepCount; ++step) {
        if (schedule == 1) std::reverse(order.begin(), order.end());
        else if (schedule == 2) std::shuffle(order.begin(), order.end(), random);
        const auto frameStart = std::chrono::high_resolution_clock::now();
        for (int objectId : order) {
            ObjectResult& result = g_results[objectId];
            if (result.interleavedRc != 0 || result.exception) continue;
            double lat = 0.0, lon = 0.0;
            unsigned long exceptionCode = 0;
            const int rc = SafeStep(objects[objectId], step, dt, &lat, &lon, &exceptionCode);
            if (rc != 0 || exceptionCode != 0) {
                result.interleavedRc = rc;
                result.exceptionCode = exceptionCode;
                result.exception = exceptionCode != 0;
                result.faultStep = step;
                result.detail = "交错步进失败";
                continue;
            }
            result.interleaved.push_back({lat, lon});
        }
        const auto frameEnd = std::chrono::high_resolution_clock::now();
        const double frameMs = std::chrono::duration<double, std::milli>(frameEnd - frameStart).count();
        if (frameMs > g_maxFrameMs) g_maxFrameMs = frameMs;
        if (schedule == 1) std::reverse(order.begin(), order.end());
    }

    for (int objectId = 0; objectId < objectCount; ++objectId) {
        unsigned long exceptionCode = 0;
        SafeDestroy(objects[objectId], &exceptionCode);
        if (exceptionCode != 0) {
            g_results[objectId].exception = 1;
            g_results[objectId].exceptionCode = exceptionCode;
            g_results[objectId].detail = "销毁对象时发生异常";
        }
        delete objects[objectId];
        const size_t count = (std::min)(g_results[objectId].baseline.size(),
                                        g_results[objectId].interleaved.size());
        for (size_t i = 0; i < count; ++i) {
            const double dLat = g_results[objectId].baseline[i].lat
                              - g_results[objectId].interleaved[i].lat;
            const double dLon = g_results[objectId].baseline[i].lon
                              - g_results[objectId].interleaved[i].lon;
            const double deviation = std::sqrt(dLat * dLat + dLon * dLon);
            if (deviation > g_results[objectId].maxDeviation)
                g_results[objectId].maxDeviation = deviation;
        }
        if (g_results[objectId].detail.empty()) {
            if (g_results[objectId].baseline.size() != g_results[objectId].interleaved.size())
                g_results[objectId].detail = "基线与交错轨迹点数不一致";
            else if (g_results[objectId].maxDeviation > tolerance)
                g_results[objectId].detail = "交错运行结果偏离单独运行基线";
            else
                g_results[objectId].detail = "状态隔离正常";
        }
    }
    return 0;
}

extern "C" __declspec(dllexport) int GetMultiObjectResult(
    int index, int* baselineRc, int* interleavedRc, int* exception,
    unsigned long* exceptionCode, int* faultStep, double* maxDeviation,
    const char** detail, int* baselineCount, int* interleavedCount) {
    if (index < 0 || index >= static_cast<int>(g_results.size())) return 0;
    const ObjectResult& result = g_results[static_cast<size_t>(index)];
    *baselineRc = result.baselineRc;
    *interleavedRc = result.interleavedRc;
    *exception = result.exception;
    *exceptionCode = result.exceptionCode;
    *faultStep = result.faultStep;
    *maxDeviation = result.maxDeviation;
    *detail = result.detail.c_str();
    *baselineCount = static_cast<int>(result.baseline.size());
    *interleavedCount = static_cast<int>(result.interleaved.size());
    return 1;
}
extern "C" __declspec(dllexport) int GetMultiObjectTrackPoint(
    int objectId, int baseline, int pointIndex, double* lat, double* lon) {
    if (objectId < 0 || objectId >= static_cast<int>(g_results.size())) return 0;
    const auto& points = baseline ? g_results[objectId].baseline : g_results[objectId].interleaved;
    if (pointIndex < 0 || pointIndex >= static_cast<int>(points.size())) return 0;
    *lat = points[pointIndex].lat;
    *lon = points[pointIndex].lon;
    return 1;
}
extern "C" __declspec(dllexport) double GetMultiObjectMaxFrameMs() {
    return g_maxFrameMs;
}
)CPP";
    return source.str();
}

bool MultiObjectHarness::InvokeCl(const MultiObjectHarnessConfig& config,
                                  const std::string& sourcePath,
                                  const std::string& outputDll,
                                  std::string& log) const {
    const std::string vcvars = UserCodeHarness::FindVcVars64Bat();
    if (vcvars.empty()) {
        log = "ERROR: 未找到 Visual Studio C++ 编译环境";
        return false;
    }
    QString command = QStringLiteral("cl.exe /nologo /LD /EHa /std:c++14 /O2 /utf-8 /DNOMINMAX /Fe%1")
        .arg(quoteArg(qPath(outputDll)));
    for (const auto& include : config.includeDirs)
        command += QStringLiteral(" /I%1").arg(quoteArg(qPath(include)));
    for (const auto& header : config.headerPaths) {
        const QString directory = QFileInfo(qPath(header)).absolutePath();
        command += QStringLiteral(" /I%1").arg(quoteArg(directory));
    }
    command += QStringLiteral(" %1").arg(quoteArg(qPath(sourcePath)));
    for (const auto& source : config.sourcePaths)
        command += QStringLiteral(" %1").arg(quoteArg(qPath(source)));
    command += QStringLiteral(" /link psapi.lib");
    for (const auto& libPath : config.libPaths)
        command += QStringLiteral(" /LIBPATH:%1").arg(quoteArg(qPath(libPath)));
    for (const auto& lib : config.linkLibs)
        command += QStringLiteral(" %1").arg(quoteArg(qPath(lib)));

    const QString batchPath = QDir(qPath(config.workDir)).filePath(
        QStringLiteral("build_multiobject_harness.bat"));
    QFile batchFile(batchPath);
    if (!batchFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        log += "ERROR: 无法写入多对象编译脚本\n";
        return false;
    }
    batchFile.write("@echo off\r\ncall \"");
    batchFile.write(QByteArray::fromStdString(vcvars));
    batchFile.write("\" >nul\r\nif errorlevel 1 exit /b %errorlevel%\r\n");
    batchFile.write(command.toLocal8Bit());
    batchFile.write("\r\n");
    batchFile.close();

    QProcess process;
    process.setWorkingDirectory(qPath(config.workDir));
    process.start(QStringLiteral("cmd.exe"),
                  { QStringLiteral("/D"), QStringLiteral("/C"), batchPath });
    if (!process.waitForStarted(10000) || !process.waitForFinished(120000)) {
        process.kill();
        log += "ERROR: 多对象 Harness 编译超时或无法启动\n";
        return false;
    }
    log += qToUtf8(QString::fromLocal8Bit(process.readAllStandardOutput()));
    log += qToUtf8(QString::fromLocal8Bit(process.readAllStandardError()));
    return process.exitCode() == 0 && QFileInfo::exists(qPath(outputDll));
}

CompileResult MultiObjectHarness::Compile(const MultiObjectHarnessConfig& config) {
    CompileResult result;
    Unload();
    if (config.adapterCode.find("class ModelObjectAdapter") == std::string::npos) {
        result.log = "ERROR: 适配代码必须定义 class ModelObjectAdapter\n";
        return result;
    }
    m_enabledVars.clear();
    for (const auto& variable : config.randomVars)
        if (variable.enabled && !variable.name.empty()) m_enabledVars.push_back(variable);
    const QString workDir = QDir(qPath(config.workDir)).absolutePath();
    if (!QDir().mkpath(workDir)) {
        result.log = "ERROR: 无法创建多对象 Harness 输出目录\n";
        return result;
    }
    const QString base = qPath(config.outputBaseName.empty()
        ? std::string("MultiObjectHarness") : config.outputBaseName);
    const QString sourcePath = QDir(workDir).filePath(base + QStringLiteral(".cpp"));
    const QString dllPath = QDir(workDir).filePath(base + QStringLiteral(".dll"));
    MultiObjectHarnessConfig actual = config;
    auto makeAbsolute = [](std::vector<std::string>& paths) {
        for (auto& path : paths) {
            path = qToUtf8(QDir::toNativeSeparators(
                QFileInfo(qPath(path)).absoluteFilePath()));
        }
    };
    makeAbsolute(actual.headerPaths);
    makeAbsolute(actual.sourcePaths);
    makeAbsolute(actual.includeDirs);
    makeAbsolute(actual.libPaths);
    makeAbsolute(actual.linkLibs);
    actual.workDir = qToUtf8(QDir::toNativeSeparators(workDir));
    m_searchDirs = actual.libPaths;
    for (const auto& lib : actual.linkLibs) {
        const QString directory = QFileInfo(qPath(lib)).absolutePath();
        if (!directory.isEmpty()) m_searchDirs.push_back(qToUtf8(directory));
    }

    QFile sourceFile(sourcePath);
    if (!sourceFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        result.log = "ERROR: 无法写入多对象 Harness 源文件\n";
        return result;
    }
    sourceFile.write(QByteArray::fromStdString(GenerateSource(actual)));
    sourceFile.close();
    QFile::remove(dllPath);
    result.sourcePath = qToUtf8(sourcePath);
    if (!InvokeCl(actual, result.sourcePath, qToUtf8(dllPath), result.log)) return result;
    std::string error;
    if (!LoadCompiledDll(qToUtf8(dllPath), error)) {
        result.log += error;
        return result;
    }
    result.success = true;
    result.dllPath = qToUtf8(dllPath);
    result.log += "SUCCESS: 多对象 Harness 编译并加载成功\n";
    return result;
}

bool MultiObjectHarness::LoadModelDll(
    const std::string& dllPath,
    const std::vector<RandomVarDef>& randomVars,
    std::string& error) {
    Unload();
    const QString absolutePath = QFileInfo(qPath(dllPath)).absoluteFilePath();
    if (!QFileInfo::exists(absolutePath)) {
        error = "DLL 文件不存在: " + dllPath;
        return false;
    }
    m_enabledVars.clear();
    for (const auto& variable : randomVars)
        if (variable.enabled && !variable.name.empty()) m_enabledVars.push_back(variable);

    const std::string directory = qToUtf8(
        QDir::toNativeSeparators(QFileInfo(absolutePath).absolutePath()));
    SetDllDirectoryA(directory.c_str());
    const std::string utf8Path = qToUtf8(QDir::toNativeSeparators(absolutePath));
    const std::wstring widePath = toWide(utf8Path);
    m_hModule = LoadLibraryExW(widePath.c_str(), NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!m_hModule) {
        error = "无法加载第三方模型 DLL，GetLastError="
              + std::to_string(GetLastError());
        SetDllDirectoryA(nullptr);
        return false;
    }
    m_pfnCreate = reinterpret_cast<FnModelCreate>(
        GetProcAddress(m_hModule, "Model_Create"));
    m_pfnInitEx = reinterpret_cast<FnModelInitEx>(
        GetProcAddress(m_hModule, "Model_Init"));
    m_pfnStepEx = reinterpret_cast<FnModelStepEx>(
        GetProcAddress(m_hModule, "Model_Step"));
    m_pfnDestroyEx = reinterpret_cast<FnModelDestroyEx>(
        GetProcAddress(m_hModule, "Model_Destroy"));
    if (!m_pfnCreate || !m_pfnInitEx || !m_pfnStepEx || !m_pfnDestroyEx) {
        error = "该 DLL 不是可自动测试的 Handle 型接口：需要导出 "
                "Model_Create / Model_Init / Model_Step / Model_Destroy";
        Unload();
        return false;
    }
    m_directModelMode = true;
    m_dllPath = utf8Path;
    return true;
}

bool MultiObjectHarness::LoadCompiledDll(const std::string& path, std::string& error) {
    Unload();
    if (!m_searchDirs.empty()) SetDllDirectoryA(m_searchDirs.front().c_str());
    const std::wstring widePath = toWide(path);
    m_hModule = LoadLibraryExW(widePath.c_str(), NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!m_hModule) m_hModule = LoadLibraryW(widePath.c_str());
    if (!m_hModule) {
        error = "ERROR: 无法加载多对象 Harness DLL，GetLastError="
              + std::to_string(GetLastError()) + "\n";
        return false;
    }
    m_pfnRun = reinterpret_cast<FnRunMultiObject>(
        GetProcAddress(m_hModule, "RunInterleavedMultiObjectTest"));
    m_pfnGetResult = reinterpret_cast<FnGetObjectResult>(
        GetProcAddress(m_hModule, "GetMultiObjectResult"));
    m_pfnGetTrackPoint = reinterpret_cast<FnGetTrackPoint>(
        GetProcAddress(m_hModule, "GetMultiObjectTrackPoint"));
    m_pfnGetMaxFrameMs = reinterpret_cast<FnGetMetric>(
        GetProcAddress(m_hModule, "GetMultiObjectMaxFrameMs"));
    if (!m_pfnRun || !m_pfnGetResult || !m_pfnGetTrackPoint || !m_pfnGetMaxFrameMs) {
        error = "ERROR: 多对象 Harness 缺少必需导出接口\n";
        Unload();
        return false;
    }
    m_dllPath = path;
    m_directModelMode = false;
    return true;
}

void MultiObjectHarness::Unload() {
    if (m_hModule) FreeLibrary(m_hModule);
    m_hModule = NULL;
    m_pfnRun = nullptr;
    m_pfnGetResult = nullptr;
    m_pfnGetTrackPoint = nullptr;
    m_pfnGetMaxFrameMs = nullptr;
    m_pfnCreate = nullptr;
    m_pfnInitEx = nullptr;
    m_pfnStepEx = nullptr;
    m_pfnDestroyEx = nullptr;
    m_directModelMode = false;
    m_dllPath.clear();
    SetDllDirectoryA(nullptr);
}

RandomValueBlob MultiObjectHarness::Sample(uint32_t seed, int objectCount) const {
    RandomValueBlob blob;
    if (objectCount < 1) objectCount = 1;
    blob.objectCount = objectCount;
    std::mt19937 random(seed);
    std::ostringstream summary;
    for (int objectId = 0; objectId < objectCount; ++objectId) {
        if (objectCount > 1)
            summary << "obj" << objectId << ":{";
        for (const auto& variable : m_enabledVars) {
            if (variable.type == RandomVarType::Int) {
                int low = static_cast<int>(std::floor(variable.minValue));
                int high = static_cast<int>(std::floor(variable.maxValue));
                if (high < low) std::swap(low, high);
                const int value = std::uniform_int_distribution<int>(low, high)(random);
                blob.ints.push_back(value);
                summary << variable.name << "=" << value << " ";
            } else {
                double low = variable.minValue, high = variable.maxValue;
                if (high < low) std::swap(low, high);
                const double value = std::uniform_real_distribution<double>(low, high)(random);
                blob.doubles.push_back(value);
                summary << variable.name << "=" << value << " ";
            }
        }
        if (objectCount > 1)
            summary << "} ";
    }
    blob.summary = summary.str();
    return blob;
}

bool MultiObjectHarness::RunDirectModel(
    const MultiObjectTestConfig& config,
    const RandomValueBlob& values,
    MultiObjectTestReport& report,
    std::string& error) const {
    report = MultiObjectTestReport();
    report.objectCount = config.objectCount;
    report.stepCount = config.stepCount;
    report.tolerance = config.tolerance;
    report.usedSingleThread = true;
    report.objectResults.resize(static_cast<size_t>(config.objectCount));
    for (int i = 0; i < config.objectCount; ++i)
        report.objectResults[static_cast<size_t>(i)].objectId = i;

    auto parametersFor = [&](int objectId) {
        WeaponModelParams parameters{};
        parameters.init_lat = 30.0;
        parameters.init_lon = 110.0;
        parameters.init_alt = 3000.0;
        parameters.init_speed = 500.0;
        parameters.init_heading = 35.0 + objectId * 7.0;
        parameters.init_pitch = 12.0;
        parameters.init_roll = 0.0;
        parameters.step_dt = config.stepDt;
        int doubleVarsPerObject = 0;
        int intVarsPerObject = 0;
        for (const auto& variable : m_enabledVars) {
            if (variable.type == RandomVarType::Int) ++intVarsPerObject;
            else ++doubleVarsPerObject;
        }
        size_t doubleIndex = 0, intIndex = 0;
        for (const auto& variable : m_enabledVars) {
            double value = 0.0;
            if (variable.type == RandomVarType::Int) {
                const size_t idx = static_cast<size_t>(objectId) * intVarsPerObject + intIndex;
                if (idx < values.ints.size()) value = values.ints[idx];
                ++intIndex;
            } else {
                const size_t idx = static_cast<size_t>(objectId) * doubleVarsPerObject + doubleIndex;
                if (idx < values.doubles.size()) value = values.doubles[idx];
                ++doubleIndex;
            }
            if (variable.name == "lat") parameters.init_lat = value;
            else if (variable.name == "lon") parameters.init_lon = value;
            else if (variable.name == "alt") parameters.init_alt = value;
            else if (variable.name == "speed") parameters.init_speed = value;
            else if (variable.name == "heading") parameters.init_heading = value;
            else if (variable.name == "pitch") parameters.init_pitch = value;
            else if (variable.name == "roll") parameters.init_roll = value;
        }
        return parameters;
    };
    auto recordFailure = [](MultiObjectResult& result, int returnCode,
                            DWORD exceptionCode, int step, bool baseline,
                            const char* phase) {
        if (baseline) result.baselineReturnCode = returnCode;
        else result.interleavedReturnCode = returnCode;
        result.exceptionOccurred = exceptionCode != 0;
        result.exceptionCode = exceptionCode;
        result.faultStep = step;
        result.detail = phase;
    };

    const ProcessMemoryStats before = MemoryUtils::GetCurrentProcessMemory();
    for (int objectId = 0; objectId < config.objectCount; ++objectId) {
        MultiObjectResult& result = report.objectResults[static_cast<size_t>(objectId)];
        ModelHandle handle = nullptr;
        DWORD exceptionCode = 0;
        if (!SafeCallCreate(m_pfnCreate, &handle, &exceptionCode) || !handle) {
            recordFailure(result, -1, exceptionCode, -1, true, "基线创建对象失败");
            continue;
        }
        WeaponModelParams parameters = parametersFor(objectId);
        int returnCode = 0;
        if (!SafeCallInitEx(m_pfnInitEx, handle, &parameters,
                            &returnCode, &exceptionCode)
            || returnCode != 0) {
            recordFailure(result, returnCode, exceptionCode, -1, true, "基线初始化失败");
        } else {
            for (int step = 0; step < config.stepCount; ++step) {
                WeaponModelOutput output{};
                if (!SafeCallStepEx(m_pfnStepEx, handle, &output,
                                    &returnCode, &exceptionCode)
                    || returnCode != 0) {
                    recordFailure(result, returnCode, exceptionCode, step,
                                  true, "基线步进失败");
                    break;
                }
                result.baselineTrajectory.push_back({ output.lat, output.lon });
            }
        }
        DWORD destroyException = 0;
        if (!SafeCallDestroyEx(m_pfnDestroyEx, handle, &destroyException)
            && !result.exceptionOccurred) {
            recordFailure(result, result.baselineReturnCode, destroyException,
                          -1, true, "基线销毁对象失败");
        }
    }

    std::vector<ModelHandle> handles(static_cast<size_t>(config.objectCount), nullptr);
    for (int objectId = 0; objectId < config.objectCount; ++objectId) {
        MultiObjectResult& result = report.objectResults[static_cast<size_t>(objectId)];
        DWORD exceptionCode = 0;
        if (!SafeCallCreate(m_pfnCreate, &handles[static_cast<size_t>(objectId)],
                            &exceptionCode)
            || !handles[static_cast<size_t>(objectId)]) {
            recordFailure(result, -1, exceptionCode, -1, false, "交错创建对象失败");
            continue;
        }
        WeaponModelParams parameters = parametersFor(objectId);
        int returnCode = 0;
        if (!SafeCallInitEx(m_pfnInitEx, handles[static_cast<size_t>(objectId)],
                            &parameters, &returnCode, &exceptionCode)
            || returnCode != 0) {
            recordFailure(result, returnCode, exceptionCode, -1,
                          false, "交错初始化失败");
        }
    }

    std::vector<int> order(static_cast<size_t>(config.objectCount));
    std::iota(order.begin(), order.end(), 0);
    if (config.schedule == MultiObjectSchedule::Reverse)
        std::reverse(order.begin(), order.end());
    std::mt19937 random(config.randomSeed);
    for (int step = 0; step < config.stepCount; ++step) {
        if (config.schedule == MultiObjectSchedule::DeterministicRandom)
            std::shuffle(order.begin(), order.end(), random);
        const auto frameStart = std::chrono::high_resolution_clock::now();
        for (int objectId : order) {
            MultiObjectResult& result =
                report.objectResults[static_cast<size_t>(objectId)];
            ModelHandle handle = handles[static_cast<size_t>(objectId)];
            if (!handle || result.interleavedReturnCode != 0
                || result.exceptionOccurred) continue;
            WeaponModelOutput output{};
            int returnCode = 0;
            DWORD exceptionCode = 0;
            if (!SafeCallStepEx(m_pfnStepEx, handle, &output,
                                &returnCode, &exceptionCode)
                || returnCode != 0) {
                recordFailure(result, returnCode, exceptionCode, step,
                              false, "交错步进失败");
                continue;
            }
            result.interleavedTrajectory.push_back({ output.lat, output.lon });
        }
        const auto frameEnd = std::chrono::high_resolution_clock::now();
        report.maxFrameTimeMs = (std::max)(
            report.maxFrameTimeMs,
            std::chrono::duration<double, std::milli>(
                frameEnd - frameStart).count());
    }
    for (int objectId = 0; objectId < config.objectCount; ++objectId) {
        ModelHandle handle = handles[static_cast<size_t>(objectId)];
        if (!handle) continue;
        DWORD exceptionCode = 0;
        if (!SafeCallDestroyEx(m_pfnDestroyEx, handle, &exceptionCode)) {
            MultiObjectResult& result =
                report.objectResults[static_cast<size_t>(objectId)];
            recordFailure(result, result.interleavedReturnCode, exceptionCode,
                          -1, false, "交错销毁对象失败");
        }
    }

    for (auto& result : report.objectResults) {
        const size_t count = (std::min)(result.baselineTrajectory.size(),
                                        result.interleavedTrajectory.size());
        for (size_t i = 0; i < count; ++i) {
            const double dLat = result.baselineTrajectory[i].lat
                              - result.interleavedTrajectory[i].lat;
            const double dLon = result.baselineTrajectory[i].lon
                              - result.interleavedTrajectory[i].lon;
            result.maxPositionDeviation = (std::max)(
                result.maxPositionDeviation,
                std::sqrt(dLat * dLat + dLon * dLon));
        }
        if (result.detail.empty()) {
            if (result.baselineTrajectory.size() != result.interleavedTrajectory.size())
                result.detail = "基线与交错轨迹点数不一致";
            else if (result.maxPositionDeviation > config.tolerance)
                result.detail = "交错结果偏离基线，疑似状态串扰";
            else
                result.detail = "状态隔离正常";
        }
    }
    const ProcessMemoryStats after = MemoryUtils::GetCurrentProcessMemory();
    report.memoryDeltaMB = MemoryUtils::BytesToMB(after.workingSetBytes)
                         - MemoryUtils::BytesToMB(before.workingSetBytes);
    error.clear();
    return true;
}

bool MultiObjectHarness::Run(const MultiObjectTestConfig& config,
                             const RandomValueBlob& values,
                             MultiObjectTestReport& report,
                             std::string& error) const {
    if (!IsLoaded()) {
        error = "多对象 Harness 尚未编译或加载";
        return false;
    }
    if (m_directModelMode) {
        return RunDirectModel(config, values, report, error);
    }
    const ProcessMemoryStats before = MemoryUtils::GetCurrentProcessMemory();
    DWORD exceptionCode = 0;
    const int returnCode = safeRun(
        reinterpret_cast<RunRaw>(m_pfnRun),
        values.doubles.empty() ? nullptr : values.doubles.data(),
        static_cast<int>(values.doubles.size()),
        values.ints.empty() ? nullptr : values.ints.data(),
        static_cast<int>(values.ints.size()), config, &exceptionCode);
    const ProcessMemoryStats after = MemoryUtils::GetCurrentProcessMemory();
    if (exceptionCode != 0 || returnCode != 0) {
        std::ostringstream stream;
        stream << "多对象 Harness 执行失败，返回码=" << returnCode
               << "，SEH=0x" << std::hex << exceptionCode;
        error = stream.str();
        return false;
    }

    report = MultiObjectTestReport();
    report.objectCount = config.objectCount;
    report.stepCount = config.stepCount;
    report.tolerance = config.tolerance;
    report.maxFrameTimeMs = safeGetMetric(
        reinterpret_cast<GetMetricRaw>(m_pfnGetMaxFrameMs));
    report.memoryDeltaMB = MemoryUtils::BytesToMB(after.workingSetBytes)
                         - MemoryUtils::BytesToMB(before.workingSetBytes);
    for (int objectId = 0; objectId < config.objectCount; ++objectId) {
        MultiObjectResult result;
        result.objectId = objectId;
        int exception = 0, baselineCount = 0, interleavedCount = 0;
        const char* detail = nullptr;
        if (!safeGetResult(
                reinterpret_cast<GetResultRaw>(m_pfnGetResult), objectId,
                &result.baselineReturnCode, &result.interleavedReturnCode,
                &exception, &result.exceptionCode, &result.faultStep,
                &result.maxPositionDeviation, &detail,
                &baselineCount, &interleavedCount)) continue;
        result.exceptionOccurred = exception != 0;
        result.detail = detail ? detail : "";
        for (int i = 0; i < baselineCount; ++i) {
            TrajectorySample sample;
            if (safeGetTrack(reinterpret_cast<GetTrackRaw>(m_pfnGetTrackPoint),
                             objectId, 1, i, &sample.lat, &sample.lon))
                result.baselineTrajectory.push_back(sample);
        }
        for (int i = 0; i < interleavedCount; ++i) {
            TrajectorySample sample;
            if (safeGetTrack(reinterpret_cast<GetTrackRaw>(m_pfnGetTrackPoint),
                             objectId, 0, i, &sample.lat, &sample.lon))
                result.interleavedTrajectory.push_back(sample);
        }
        report.objectResults.push_back(result);
    }
    return true;
}
