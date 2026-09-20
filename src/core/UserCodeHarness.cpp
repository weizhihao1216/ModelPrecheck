#include "UserCodeHarness.h"
#include "../utils/SehHelper.h"
#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <fstream>
#include <sstream>
#include <random>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstring>
#include <set>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace {

// 与多对象 Harness 使用同一份 ModelObjectKit.h，保证同一段用户代码两边都能编译
std::string LoadModelObjectKitHeader() {
    QFileInfo self(QString::fromUtf8(__FILE__));
    QFile file(self.absoluteDir().filePath(QStringLiteral("ModelObjectKit.h")));
    if (!file.open(QIODevice::ReadOnly))
        return std::string("#error \"ModelObjectKit.h not found\"\n");
    return file.readAll().toStdString();
}

} // namespace

std::string UserCodeHarness::BuildPerObjectRandomPreamble(const std::vector<RandomVarDef>& enabled) {
    int doubleVarsPerObject = 0;
    int intVarsPerObject = 0;
    for (const auto& variable : enabled) {
        if (variable.type == RandomVarType::Int) ++intVarsPerObject;
        else ++doubleVarsPerObject;
    }
    std::ostringstream source;
    source << "    static const int kDoubleVarsPerObject = " << doubleVarsPerObject << ";\n"
           << "    static const int kIntVarsPerObject = " << intVarsPerObject << ";\n"
           << "    auto fillObjectRandom = [&](int objectId, RandomBag& bag) {\n";
    int doubleIndex = 0;
    int intIndex = 0;
    for (const auto& variable : enabled) {
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

UserCodeHarness::UserCodeHarness() = default;

UserCodeHarness::~UserCodeHarness() {
    Unload();
}

std::string UserCodeHarness::DefaultUserMainTemplate() {
    return
        "// ========== 使用说明（可删除） ==========\n"
        "// 一个型号只写这一份代码，预检 / 性能压测 / 并发 / 单线程多对象 / 专项测试全部共用。\n"
        "// 你只需写「单个对象」的生命周期：创建、初始化、推进一步、销毁。\n"
        "// 步数、步长由工具控制（与单线程多对象页面设置一致），无需自己写循环。\n"
        "// 每个对象独立抽样一份随机变量包 R，同一对象的 MoInit / MoStep 共用该 R。\n"
        "// objectId：对象下标（单对象测试时恒为 0）。\n"
        "// MoStep 的 out_lat / out_lon 由工具自动采集为二维轨迹，不必再调用采集函数。\n"
        "// 模型包目录结构：根目录/include（头文件）、根目录/lib（.lib）、根目录/models（.dll）。\n"
        "// 授权文件请与 models/ 下的第三方 .dll 同级放置。\n"
        "// ==========================================\n"
        "\n"
        "// 将 WeaponObject 换成你的第三方封装类（需在型号页勾选对应头文件）\n"
        "using MoModelType = WeaponObject;\n"
        "\n"
        "// 【创建】只写 new 一个对象\n"
        "static MoModelType* MoCreate(int objectId, const RandomBag& R) {\n"
        "    (void)objectId;\n"
        "    (void)R;\n"
        "    return new MoModelType();\n"
        "}\n"
        "\n"
        "// 【初始化】只写对一个对象的 init\n"
        "static int MoInit(MoModelType* obj, int objectId, const RandomBag& R, double dt) {\n"
        "    return obj->Initialize(objectId, R.lat, R.lon, R.alt, R.speed, dt);\n"
        "}\n"
        "\n"
        "// 【步进】只写对一个对象推进一步（如 obj->run()）\n"
        "// out_lat / out_lon：本步经纬度，工具自动记录并用于二维轨迹预览\n"
        "static int MoStep(MoModelType* obj, int objectId, int stepIndex, double dt,\n"
        "                  const RandomBag& R,\n"
        "                  double& out_lat, double& out_lon) {\n"
        "    (void)objectId;\n"
        "    (void)stepIndex;\n"
        "    (void)dt;\n"
        "    (void)R;\n"
        "    int rc = obj->Step(out_lat, out_lon);\n"
        "    return rc;\n"
        "}\n"
        "\n"
        "// 【销毁】只写销毁一个对象\n"
        "static void MoDestroy(MoModelType* obj, int objectId) {\n"
        "    (void)objectId;\n"
        "    obj->Shutdown();\n"
        "    delete obj;\n"
        "}\n";
}

std::string UserCodeHarness::FindVcVars64Bat() {
    wchar_t* pf86 = nullptr;
    size_t len = 0;
    _wdupenv_s(&pf86, &len, L"ProgramFiles(x86)");
    std::wstring base = pf86 ? pf86 : L"C:\\Program Files (x86)";
    if (pf86) free(pf86);

    std::wstring vswhere = base + L"\\Microsoft Visual Studio\\Installer\\vswhere.exe";
    if (GetFileAttributesW(vswhere.c_str()) == INVALID_FILE_ATTRIBUTES) {
        // fallback common paths
        const wchar_t* candidates[] = {
            L"C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat",
            L"C:\\Program Files\\Microsoft Visual Studio\\2022\\Professional\\VC\\Auxiliary\\Build\\vcvars64.bat",
            L"C:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat"
        };
        for (const wchar_t* c : candidates) {
            if (GetFileAttributesW(c) != INVALID_FILE_ATTRIBUTES) {
                int n = WideCharToMultiByte(CP_UTF8, 0, c, -1, nullptr, 0, nullptr, nullptr);
                std::string s(n > 0 ? n - 1 : 0, '\0');
                if (n > 1) WideCharToMultiByte(CP_UTF8, 0, c, -1, &s[0], n, nullptr, nullptr);
                return s;
            }
        }
        return {};
    }

    // vswhere -latest -property installationPath
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    HANDLE hRead = NULL, hWrite = NULL;
    CreatePipe(&hRead, &hWrite, &sa, 0);
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    std::wstring cmd = L"\"" + vswhere + L"\" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath";
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;
    PROCESS_INFORMATION pi{};
    std::wstring mutableCmd = cmd;
    if (!CreateProcessW(nullptr, &mutableCmd[0], nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        CloseHandle(hRead); CloseHandle(hWrite);
        return {};
    }
    CloseHandle(hWrite);
    std::string out;
    char buf[512];
    DWORD rd = 0;
    while (ReadFile(hRead, buf, sizeof(buf), &rd, nullptr) && rd > 0) {
        out.append(buf, buf + rd);
    }
    WaitForSingleObject(pi.hProcess, 15000);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread); CloseHandle(hRead);

    // trim
    while (!out.empty() && (out.back() == '\n' || out.back() == '\r' || out.back() == ' ')) out.pop_back();
    size_t b = 0;
    while (b < out.size() && (out[b] == ' ' || out[b] == '\n' || out[b] == '\r')) ++b;
    out = out.substr(b);
    if (out.empty()) return {};
    return out + "\\VC\\Auxiliary\\Build\\vcvars64.bat";
}

std::string UserCodeHarness::GenerateSource(const UserHarnessConfig& config, const std::vector<RandomVarDef>& enabled) const {
    return GenerateUnifiedSource(config, enabled);
}

// 单对象与多对象共用同一个 Harness：一份用户代码、一个 DLL、一次编译
std::string UserCodeHarness::GenerateUnifiedSource(const UserHarnessConfig& config, const std::vector<RandomVarDef>& enabled) {
    std::ostringstream ss;
    ss << "// Auto-generated by ModelPrecheck UserCodeHarness — do not edit permanently\n";
    ss << "#ifndef NOMINMAX\n#define NOMINMAX\n#endif\n";
    ss << "#include <windows.h>\n";
    ss << "#include <cstdint>\n";
    ss << "#include <cstring>\n";
    ss << "#include <cmath>\n";
    ss << "#include <algorithm>\n";
    ss << "#include <chrono>\n";
    ss << "#include <functional>\n";
    ss << "#include <numeric>\n";
    ss << "#include <random>\n";
    ss << "#include <string>\n";
    ss << "#include <utility>\n";
    ss << "#include <vector>\n\n";
    ss << LoadModelObjectKitHeader() << "\n";

    for (const auto& h : config.headerPaths) {
        // Use forward slashes in include string
        std::string p = h;
        for (char& c : p) if (c == '\\') c = '/';
        ss << "#include \"" << p << "\"\n";
    }
    ss << "\n";

    ss << "struct RandomBag {\n";
    for (const auto& v : enabled) {
        if (v.type == RandomVarType::Int)
            ss << "    int " << v.name << ";\n";
        else
            ss << "    double " << v.name << ";\n";
    }
    if (enabled.empty()) {
        ss << "    int _placeholder;\n";
    }
    ss << "};\n\n";

    // 用户代码：MoModelType / MoCreate / MoInit / MoStep / MoDestroy
    ss << config.userMainBody << "\n\n";

    // Trajectory buffer available to the harness via RecordTrajectoryPoint(lat, lon)
    ss << "static std::vector<double> g_trajLat;\n";
    ss << "static std::vector<double> g_trajLon;\n";
    ss << "static int g_trajCapture = 0;\n\n";
    ss << "static void RecordTrajectoryPoint(double lat, double lon) {\n";
    ss << "    if (!g_trajCapture) return;\n";
    ss << "    g_trajLat.push_back(lat);\n";
    ss << "    g_trajLon.push_back(lon);\n";
    ss << "}\n\n";
    ss << "extern \"C\" __declspec(dllexport) void SetTrajectoryCapture(int on) {\n";
    ss << "    g_trajCapture = on ? 1 : 0;\n";
    ss << "    if (on) { g_trajLat.clear(); g_trajLon.clear(); }\n";
    ss << "}\n\n";
    ss << "extern \"C\" __declspec(dllexport) int GetTrajectoryCount() {\n";
    ss << "    return static_cast<int>(g_trajLat.size());\n";
    ss << "}\n\n";
    ss << "extern \"C\" __declspec(dllexport) int GetTrajectoryPoint(int i, double* lat, double* lon) {\n";
    ss << "    if (i < 0 || i >= static_cast<int>(g_trajLat.size())) return 0;\n";
    ss << "    if (lat) *lat = g_trajLat[static_cast<size_t>(i)];\n";
    ss << "    if (lon) *lon = g_trajLon[static_cast<size_t>(i)];\n";
    ss << "    return 1;\n";
    ss << "}\n\n";

    // 与多对象 Harness 完全相同的分阶段 SEH 包装
    ss << "using UserMoObject = decltype(MoCreate(0, std::declval<const RandomBag&>()));\n\n";
    ss << "static int SafeMoInit(UserMoObject object, int objectId, const RandomBag* random,\n";
    ss << "                      double dt, unsigned long* exceptionCode) {\n";
    ss << "    __try {\n";
    ss << "        *exceptionCode = 0;\n";
    ss << "        return MoInit(object, objectId, *random, dt);\n";
    ss << "    } __except (EXCEPTION_EXECUTE_HANDLER) {\n";
    ss << "        *exceptionCode = GetExceptionCode();\n";
    ss << "        return -32000;\n";
    ss << "    }\n";
    ss << "}\n\n";
    ss << "static int SafeMoStep(UserMoObject object, int objectId, int step, double dt,\n";
    ss << "                      const RandomBag* random,\n";
    ss << "                      double* lat, double* lon, unsigned long* exceptionCode) {\n";
    ss << "    __try {\n";
    ss << "        *exceptionCode = 0;\n";
    ss << "        return MoStep(object, objectId, step, dt, *random, *lat, *lon);\n";
    ss << "    } __except (EXCEPTION_EXECUTE_HANDLER) {\n";
    ss << "        *exceptionCode = GetExceptionCode();\n";
    ss << "        return -32000;\n";
    ss << "    }\n";
    ss << "}\n\n";
    ss << "static void SafeMoDestroy(UserMoObject object, int objectId,\n";
    ss << "                          unsigned long* exceptionCode) {\n";
    ss << "    __try {\n";
    ss << "        *exceptionCode = 0;\n";
    ss << "        MoDestroy(object, objectId);\n";
    ss << "    } __except (EXCEPTION_EXECUTE_HANDLER) {\n";
    ss << "        *exceptionCode = GetExceptionCode();\n";
    ss << "    }\n";
    ss << "}\n\n";

    // ---------- 单对象生命周期（步数/步长运行期传入，与多对象语义一致） ----------
    ss << "extern \"C\" __declspec(dllexport) int RunUserTest(\n";
    ss << "    const double* dvals, int nd,\n";
    ss << "    const int* ivals, int ni,\n";
    ss << "    int stepCount, double dt)\n";
    ss << "{\n";
    ss << "    if (stepCount < 1) stepCount = 1;\n";
    ss << "    if (dt <= 0.0) dt = 0.02;\n";
    ss << "    RandomBag R{};\n";
    int di = 0, ii = 0;
    for (const auto& v : enabled) {
        if (v.type == RandomVarType::Int) {
            ss << "    if (" << ii << " < ni) R." << v.name << " = ivals[" << ii << "];\n";
            ++ii;
        } else {
            ss << "    if (" << di << " < nd) R." << v.name << " = dvals[" << di << "];\n";
            ++di;
        }
    }
    ss << "    UserMoObject object = MoCreate(0, R);\n";
    ss << "    unsigned long exceptionCode = 0;\n";
    ss << "    if (!object) return -1003;\n";
    ss << "    int rc = SafeMoInit(object, 0, &R, dt, &exceptionCode);\n";
    ss << "    if (rc != 0 || exceptionCode != 0) {\n";
    ss << "        unsigned long destroyCode = 0;\n";
    ss << "        SafeMoDestroy(object, 0, &destroyCode);\n";
    ss << "        return rc != 0 ? rc : -32000;\n";
    ss << "    }\n";
    ss << "    for (int step = 0; step < stepCount; ++step) {\n";
    ss << "        double lat = 0.0;\n";
    ss << "        double lon = 0.0;\n";
    ss << "        rc = SafeMoStep(object, 0, step, dt, &R, &lat, &lon, &exceptionCode);\n";
    ss << "        if (rc != 0 || exceptionCode != 0) break;\n";
    ss << "        RecordTrajectoryPoint(lat, lon);\n";
    ss << "    }\n";
    ss << "    unsigned long destroyCode = 0;\n";
    ss << "    SafeMoDestroy(object, 0, &destroyCode);\n";
    ss << "    if (exceptionCode != 0) return -32000;\n";
    ss << "    return rc;\n";
    ss << "}\n\n";

    // ---------- 多对象：基线 + 单线程交错 ----------
    ss << R"CPP(
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

extern "C" __declspec(dllexport) int RunInterleavedMultiObjectTest(
    const double* dvals, int nd, const int* ivals, int ni,
    int objectCount, int stepCount, double dt, double tolerance,
    int schedule, unsigned int randomSeed) {
    if (objectCount < 1 || stepCount < 1 || dt <= 0.0) return -1;
    (void)randomSeed;
)CPP";
    ss << BuildPerObjectRandomPreamble(enabled);
    ss << R"CPP(
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

    std::vector<UserMoObject> objects(static_cast<size_t>(objectCount), UserMoObject{});
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

static std::vector<RandomBag> g_sessionRandoms;
static double g_sessionDt = 0.02;

extern "C" __declspec(dllexport) int MoPool_Prepare(
    const double* dvals, int nd, const int* ivals, int ni,
    int objectCount, double dt) {
    if (objectCount < 1 || dt <= 0.0) return -1;
    g_sessionDt = dt;
)CPP";
    ss << BuildPerObjectRandomPreamble(enabled);
    ss << R"CPP(
    g_sessionRandoms = std::move(objectRandoms);
    return 0;
}

extern "C" __declspec(dllexport) void* MoPool_Create(int objectId) {
    if (objectId < 0 || objectId >= static_cast<int>(g_sessionRandoms.size())) return nullptr;
    return MoCreate(objectId, g_sessionRandoms[static_cast<size_t>(objectId)]);
}

extern "C" __declspec(dllexport) int MoPool_Init(void* object, int objectId,
                                                 unsigned long* exceptionCode) {
    if (exceptionCode) *exceptionCode = 0;
    if (!object || objectId < 0 || objectId >= static_cast<int>(g_sessionRandoms.size()))
        return -1;
    unsigned long localSeh = 0;
    const int rc = SafeMoInit(reinterpret_cast<UserMoObject>(object), objectId,
                              &g_sessionRandoms[static_cast<size_t>(objectId)],
                              g_sessionDt, &localSeh);
    if (exceptionCode) *exceptionCode = localSeh;
    return rc;
}

extern "C" __declspec(dllexport) int MoPool_Step(
    void* object, int objectId, int step, double* lat, double* lon,
    unsigned long* exceptionCode) {
    if (exceptionCode) *exceptionCode = 0;
    if (!object || !lat || !lon
        || objectId < 0 || objectId >= static_cast<int>(g_sessionRandoms.size()))
        return -1;
    unsigned long localSeh = 0;
    const int rc = SafeMoStep(reinterpret_cast<UserMoObject>(object), objectId, step, g_sessionDt,
                              &g_sessionRandoms[static_cast<size_t>(objectId)],
                              lat, lon, &localSeh);
    if (exceptionCode) *exceptionCode = localSeh;
    return rc;
}

extern "C" __declspec(dllexport) void MoPool_Destroy(void* object, int objectId,
                                                     unsigned long* exceptionCode) {
    if (exceptionCode) *exceptionCode = 0;
    if (!object) return;
    unsigned long localSeh = 0;
    SafeMoDestroy(reinterpret_cast<UserMoObject>(object), objectId, &localSeh);
    if (exceptionCode) *exceptionCode = localSeh;
}
)CPP";
    return ss.str();
}

static std::wstring Utf8ToWide(const std::string& utf8) {
    if (utf8.empty()) return std::wstring();
    int n = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    if (n <= 1) return std::wstring();
    std::wstring w(static_cast<size_t>(n - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &w[0], n);
    return w;
}

/** Copy *.dll under searchDirs into destDir so LoadLibrary(ALTERED_PATH) finds deps.
 *  Never overwrite harnessDllPath (same basename as model DLL is a common collision). */
static void CopyDependencyDllsBesideHarness(const std::vector<std::string>& searchDirs,
                                            const std::string& destDir,
                                            const std::string& harnessDllPath,
                                            std::string& log) {
    if (destDir.empty()) return;
    std::string harnessName;
    {
        const size_t slash = harnessDllPath.find_last_of("/\\");
        harnessName = (slash == std::string::npos)
            ? harnessDllPath : harnessDllPath.substr(slash + 1);
    }
    std::set<std::string> copiedNames;
    for (const auto& root : searchDirs) {
        if (root.empty()) continue;
        std::vector<std::string> stack;
        stack.push_back(root);
        int visited = 0;
        while (!stack.empty() && visited < 64) {
            const std::string dir = stack.back();
            stack.pop_back();
            ++visited;
            WIN32_FIND_DATAA data{};
            const std::string pattern = dir + "\\*";
            HANDLE find = FindFirstFileA(pattern.c_str(), &data);
            if (find == INVALID_HANDLE_VALUE) continue;
            do {
                if (strcmp(data.cFileName, ".") == 0 || strcmp(data.cFileName, "..") == 0)
                    continue;
                const std::string full = dir + "\\" + data.cFileName;
                if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    if (visited < 64) stack.push_back(full);
                    continue;
                }
                const char* ext = strrchr(data.cFileName, '.');
                if (!ext || _stricmp(ext, ".dll") != 0) continue;
                if (!harnessName.empty() && _stricmp(data.cFileName, harnessName.c_str()) == 0) {
                    log += "INFO: 跳过与 Harness 同名的模型 DLL（避免覆盖）: ";
                    log += data.cFileName;
                    log += "\n";
                    continue;
                }
                if (!copiedNames.insert(data.cFileName).second) continue;
                const std::string dest = destDir + "\\" + data.cFileName;
                if (!harnessDllPath.empty()
                    && _stricmp(dest.c_str(), harnessDllPath.c_str()) == 0) {
                    continue;
                }
                if (CopyFileA(full.c_str(), dest.c_str(), FALSE)) {
                    log += "INFO: 已复制依赖 DLL 到 Harness 目录: ";
                    log += data.cFileName;
                    log += "\n";
                }
            } while (FindNextFileA(find, &data));
            FindClose(find);
        }
    }
}

void UserCodeHarness::ApplyDllSearchPaths() const {
    // Prefer a directory that actually contains .dll (models/), not only .lib.
    for (const auto& dir : m_dllSearchDirs) {
        WIN32_FIND_DATAA data{};
        const std::string pattern = dir + "\\*.dll";
        HANDLE find = FindFirstFileA(pattern.c_str(), &data);
        if (find != INVALID_HANDLE_VALUE) {
            FindClose(find);
            SetDllDirectoryA(dir.c_str());
            return;
        }
    }
    if (!m_dllSearchDirs.empty()) {
        SetDllDirectoryA(m_dllSearchDirs.front().c_str());
    }
}

bool UserCodeHarness::InvokeCl(const UserHarnessConfig& config, const std::string& srcPath,
                               const std::string& outDll, std::string& log) const {
    std::string vcvars = FindVcVars64Bat();
    if (vcvars.empty()) {
        log += "ERROR: 未找到 vcvars64.bat，请确认已安装 Visual Studio C++ 工具集。\n";
        return false;
    }

    std::ostringstream cmd;
    cmd << "cmd.exe /C \"call \"" << vcvars << "\" >nul && ";
    cmd << "cl.exe /nologo /LD /EHa /std:c++14 /O2 /utf-8 /DNOMINMAX ";
    cmd << "/Fe\"" << outDll << "\" ";
    for (const auto& inc : config.includeDirs) {
        cmd << "/I\"" << inc << "\" ";
    }
    for (const auto& h : config.headerPaths) {
        size_t pos = h.find_last_of("/\\");
        if (pos != std::string::npos) {
            cmd << "/I\"" << h.substr(0, pos) << "\" ";
        }
    }
    cmd << "\"" << srcPath << "\" /link ";
    for (const auto& lp : config.libPaths) {
        cmd << "/LIBPATH:\"" << lp << "\" ";
    }
    for (const auto& lib : config.linkLibs) {
        cmd << "\"" << lib << "\" ";
    }
    cmd << "\"";

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    HANDLE hRead = NULL, hWrite = NULL;
    CreatePipe(&hRead, &hWrite, &sa, 0);
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;
    PROCESS_INFORMATION pi{};
    std::string mutableCmd = cmd.str();
    log += "CMD: " + mutableCmd + "\n";

    std::wstring wCmd = Utf8ToWide(mutableCmd);
    std::wstring wDir = config.workDir.empty() ? std::wstring() : Utf8ToWide(config.workDir);
    BOOL ok = CreateProcessW(nullptr, wCmd.empty() ? nullptr : &wCmd[0], nullptr, nullptr, TRUE,
                             CREATE_NO_WINDOW, nullptr,
                             wDir.empty() ? nullptr : wDir.c_str(),
                             &si, &pi);
    CloseHandle(hWrite);
    if (!ok) {
        log += "ERROR: CreateProcess 失败，GetLastError=" + std::to_string(GetLastError()) + "\n";
        CloseHandle(hRead);
        return false;
    }

    // 编译期间必须保持事件循环运转，否则等候弹框的进度动画会整段停住（绿条不动）。
    // 因此用 PeekNamedPipe 轮询编译器输出，等待时处理 Qt 事件。
    char buf[1024];
    const DWORD waitSliceMs = 60;
    DWORD waitedMs = 0;
    for (;;) {
        DWORD available = 0;
        if (!PeekNamedPipe(hRead, nullptr, 0, nullptr, &available, nullptr))
            break;  // 写端已关闭，输出结束
        if (available > 0) {
            const DWORD want = available < sizeof(buf) - 1
                ? available : static_cast<DWORD>(sizeof(buf) - 1);
            DWORD rd = 0;
            if (!ReadFile(hRead, buf, want, &rd, nullptr) || rd == 0) break;
            buf[rd] = 0;
            log += buf;
            continue;
        }
        if (WaitForSingleObject(pi.hProcess, waitSliceMs) == WAIT_OBJECT_0) {
            // 进程已退出：把管道中剩余输出读完
            while (PeekNamedPipe(hRead, nullptr, 0, nullptr, &available, nullptr)
                   && available > 0) {
                const DWORD rest = available < sizeof(buf) - 1
                    ? available : static_cast<DWORD>(sizeof(buf) - 1);
                DWORD rd = 0;
                if (!ReadFile(hRead, buf, rest, &rd, nullptr) || rd == 0) break;
                buf[rd] = 0;
                log += buf;
            }
            break;
        }
        waitedMs += waitSliceMs;
        if (waitedMs >= 120000) {
            log += "ERROR: cl.exe 编译超时（120 秒）\n";
            TerminateProcess(pi.hProcess, 1);
            break;
        }
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }
    WaitForSingleObject(pi.hProcess, 5000);
    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hRead);

    if (exitCode != 0) {
        log += "ERROR: cl.exe 退出码 " + std::to_string(exitCode) + "\n";
        // 被测模型若在 DETACH 阶段死循环，其 DLL 无法卸载（仍被本进程占用），
        // 链接器会报 LNK1104；这里给出可操作的提示，避免用户无从下手。
        if (log.find("LNK1104") != std::string::npos)
            log += "提示：输出 DLL 无法覆盖，通常是该型号的模块在卸载时卡死导致文件仍被占用，"
                   "重启工具后即可重新编译。\n";
        return false;
    }
    std::wstring wOut = Utf8ToWide(outDll);
    if (wOut.empty() || GetFileAttributesW(wOut.c_str()) == INVALID_FILE_ATTRIBUTES) {
        log += "ERROR: 未生成 DLL: " + outDll + "\n";
        return false;
    }
    return true;
}

static bool EnsureDirTreeUtf8(const std::string& utf8Dir) {
    std::wstring w = Utf8ToWide(utf8Dir);
    if (w.empty()) return false;
    // Create intermediate directories
    for (size_t i = 0; i < w.size(); ++i) {
        if (w[i] == L'\\' || w[i] == L'/') {
            if (i == 0) continue;
            // skip "D:" drive root
            if (i == 2 && w[1] == L':') continue;
            wchar_t old = w[i];
            w[i] = L'\0';
            CreateDirectoryW(w.c_str(), nullptr);
            w[i] = old;
        }
    }
    return CreateDirectoryW(w.c_str(), nullptr) != 0 || GetLastError() == ERROR_ALREADY_EXISTS;
}

static bool WriteTextFileUtf8Path(const std::string& utf8Path, const std::string& content, std::string& err) {
    std::wstring w = Utf8ToWide(utf8Path);
    if (w.empty()) {
        err = "路径无效\n";
        return false;
    }
    HANDLE h = CreateFileW(w.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        err = "无法写入文件, GetLastError=" + std::to_string(GetLastError()) + "\n";
        return false;
    }
    DWORD written = 0;
    BOOL ok = WriteFile(h, content.data(), static_cast<DWORD>(content.size()), &written, nullptr);
    CloseHandle(h);
    if (!ok) {
        err = "WriteFile 失败, GetLastError=" + std::to_string(GetLastError()) + "\n";
        return false;
    }
    return true;
}

CompileResult UserCodeHarness::Compile(const UserHarnessConfig& config) {
    CompileResult result;
    Unload();

    if (config.userMainBody.find_first_not_of(" \t\r\n") == std::string::npos) {
        result.log = "ERROR: 用户 main 代码为空。\n";
        return result;
    }

    // 所有测试项共用同一份对象生命周期代码：必须定义 MoModelType 与四个 Mo* 函数
    if (config.userMainBody.find("MoModelType") == std::string::npos
        || config.userMainBody.find("MoCreate") == std::string::npos
        || config.userMainBody.find("MoInit") == std::string::npos
        || config.userMainBody.find("MoStep") == std::string::npos
        || config.userMainBody.find("MoDestroy") == std::string::npos) {
        result.log =
            "ERROR: 代码须定义 MoModelType、MoCreate、MoInit、MoStep、MoDestroy\n";
        return result;
    }

    m_enabledVars.clear();
    for (const auto& v : config.randomVars) {
        if (!v.enabled || v.name.empty()) continue;
        // validate identifier
        bool okName = (isalpha(static_cast<unsigned char>(v.name[0])) || v.name[0] == '_');
        for (char c : v.name) {
            if (!isalnum(static_cast<unsigned char>(c)) && c != '_') okName = false;
        }
        if (!okName) {
            result.log += "ERROR: 非法变量名: " + v.name + "\n";
            return result;
        }
        m_enabledVars.push_back(v);
    }
    SetStepParams(config.stepCount, config.stepDt);

    m_dllSearchDirs.clear();
    std::set<std::string> seen;
    auto addDir = [&](const std::string& d) {
        if (d.empty() || !seen.insert(d).second) return;
        m_dllSearchDirs.push_back(d);
    };
    for (const auto& d : config.libPaths) addDir(d);
    for (const auto& d : config.includeDirs) addDir(d);
    for (const auto& lib : config.linkLibs) {
        size_t pos = lib.find_last_of("/\\");
        if (pos != std::string::npos) addDir(lib.substr(0, pos));
    }

    std::string work = config.workDir;
    if (work.empty()) {
        char tmp[MAX_PATH];
        GetTempPathA(MAX_PATH, tmp);
        work = std::string(tmp) + "ModelPrecheck_Harness";
    }
    if (!EnsureDirTreeUtf8(work)) {
        result.log = "ERROR: 无法创建输出目录: " + work + " (GetLastError=" + std::to_string(GetLastError()) + ")\n";
        // still try if already exists
        std::wstring ww = Utf8ToWide(work);
        DWORD attr = GetFileAttributesW(ww.c_str());
        if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
            return result;
        }
        result.log.clear();
    }

    std::string base = config.outputBaseName.empty() ? "UserHarness" : config.outputBaseName;
    std::string srcPath = work + "\\" + base + ".cpp";
    std::string dllPath = work + "\\" + base + ".dll";
    result.sourcePath = srcPath;

    std::string src = GenerateSource(config, m_enabledVars);
    {
        std::string werr;
        if (!WriteTextFileUtf8Path(srcPath, src, werr)) {
            result.log = "ERROR: " + werr;
            return result;
        }
    }

    {
        std::wstring wDll = Utf8ToWide(dllPath);
        if (!wDll.empty()) DeleteFileW(wDll.c_str());
    }
    UserHarnessConfig cfg = config;
    cfg.workDir = work;
    if (!InvokeCl(cfg, srcPath, dllPath, result.log)) {
        return result;
    }

    CopyDependencyDllsBesideHarness(m_dllSearchDirs, work, dllPath, result.log);

    std::string err;
    if (!LoadCompiledDll(dllPath, err)) {
        result.log += err;
        return result;
    }

    result.success = true;
    result.dllPath = dllPath;
    result.log += "SUCCESS: 编译并加载 " + base + ".dll\n";
    return result;
}

bool UserCodeHarness::LoadCompiledDll(const std::string& dllPath, std::string& err) {
    Unload();
    ApplyDllSearchPaths();

    std::string loadError;
    // 加载私有副本且从不卸载：既保证每次都是全新的模块状态，又不会执行被测模块的 DETACH。
    m_hModule = LoadPrivateModuleCopy(dllPath, std::string(), nullptr, &loadError);
    if (!m_hModule) {
        err = loadError + "（请确认 models/ 下的第三方 .dll 存在；工具会尝试复制到 Harness 输出目录）\n";
        return false;
    }
    m_pfnRun = reinterpret_cast<FnRunUserTest>(GetProcAddress(m_hModule, "RunUserTest"));
    if (!m_pfnRun) {
        err = "GetProcAddress(RunUserTest) 失败\n";
        Unload();
        return false;
    }
    m_pfnSetTraj = reinterpret_cast<FnSetTrajectoryCapture>(GetProcAddress(m_hModule, "SetTrajectoryCapture"));
    m_pfnGetTrajCount = reinterpret_cast<FnGetTrajectoryCount>(GetProcAddress(m_hModule, "GetTrajectoryCount"));
    m_pfnGetTrajPoint = reinterpret_cast<FnGetTrajectoryPoint>(GetProcAddress(m_hModule, "GetTrajectoryPoint"));
    m_dllPath = dllPath;
    return true;
}

void UserCodeHarness::Unload() {
    // 不调用 FreeLibrary：被测模块的 DETACH 可能死循环并永久占用装载锁（详见 LoadPrivateModuleCopy 注释）。
    m_hModule = NULL;
    m_pfnRun = nullptr;
    m_pfnSetTraj = nullptr;
    m_pfnGetTrajCount = nullptr;
    m_pfnGetTrajPoint = nullptr;
    m_dllPath.clear();
    SetDllDirectoryA(nullptr);
}

void UserCodeHarness::SetEnabledRandomVars(const std::vector<RandomVarDef>& vars) {
    m_enabledVars.clear();
    for (const auto& v : vars) {
        if (!v.enabled || v.name.empty()) continue;
        m_enabledVars.push_back(v);
    }
}

RandomValueBlob UserCodeHarness::Sample(uint32_t seed) const {
    RandomValueBlob blob;
    std::mt19937 rng(seed);
    std::ostringstream sum;
    for (const auto& v : m_enabledVars) {
        if (v.type == RandomVarType::Int) {
            int lo = static_cast<int>(std::floor(v.minValue));
            int hi = static_cast<int>(std::floor(v.maxValue));
            if (hi < lo) std::swap(hi, lo);
            std::uniform_int_distribution<int> dist(lo, hi);
            int x = dist(rng);
            blob.ints.push_back(x);
            sum << v.name << "=" << x << " ";
        } else {
            double lo = v.minValue, hi = v.maxValue;
            if (hi < lo) std::swap(hi, lo);
            std::uniform_real_distribution<double> dist(lo, hi);
            double x = dist(rng);
            blob.doubles.push_back(x);
            sum << v.name << "=" << x << " ";
        }
    }
    blob.summary = sum.str();
    return blob;
}

typedef int (*FnRunUserTestRaw)(const double*, int, const int*, int, int, double);

static int InvokeRunUserTestRaw(FnRunUserTestRaw fn, const double* d, int nd, const int* iv, int ni,
                                int stepCount, double dt, DWORD* outExc) {
    if (!fn) {
        if (outExc) *outExc = static_cast<DWORD>(-1);
        return 0;
    }
    __try {
        if (outExc) *outExc = 0;
        return fn(d, nd, iv, ni, stepCount, dt);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        if (outExc) *outExc = GetExceptionCode();
        return 0;
    }
}

void UserCodeHarness::SetStepParams(int stepCount, double stepDt) {
    m_stepCount = stepCount > 0 ? stepCount : 1;
    m_stepDt = stepDt > 0.0 ? stepDt : 0.02;
}

bool UserCodeHarness::RunOnce(const RandomValueBlob& values, int* outUserReturn, bool* outSeh, std::string& err) const {
    if (!m_pfnRun) {
        err = "Harness DLL 未加载";
        return false;
    }
    DWORD exc = 0;
    int ret = InvokeRunUserTestRaw(
        reinterpret_cast<FnRunUserTestRaw>(m_pfnRun),
        values.doubles.empty() ? nullptr : values.doubles.data(),
        static_cast<int>(values.doubles.size()),
        values.ints.empty() ? nullptr : values.ints.data(),
        static_cast<int>(values.ints.size()),
        m_stepCount, m_stepDt,
        &exc);

    if (exc != 0) {
        if (outSeh) *outSeh = true;
        std::ostringstream oss;
        oss << "SEH 异常 0x" << std::hex << exc;
        err = oss.str();
        return false;
    }
    if (outSeh) *outSeh = false;
    if (outUserReturn) *outUserReturn = ret;
    return true;
}

bool UserCodeHarness::SetTrajectoryCapture(bool enabled) const {
    if (!m_pfnSetTraj) return false;
    m_pfnSetTraj(enabled ? 1 : 0);
    return true;
}

bool UserCodeHarness::FetchTrajectory(std::vector<TrajectorySample>& out) const {
    out.clear();
    if (!m_pfnGetTrajCount || !m_pfnGetTrajPoint) return false;
    const int n = m_pfnGetTrajCount();
    if (n <= 0) return true;
    out.reserve(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
        TrajectorySample s;
        if (!m_pfnGetTrajPoint(i, &s.lat, &s.lon)) continue;
        out.push_back(s);
    }
    return true;
}
