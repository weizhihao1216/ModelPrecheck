#include "../src/core/MultiObjectHarness.h"
#include "../src/core/SingleThreadMultiObjectTester.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <iostream>

namespace {

std::vector<RandomVarDef> defaultRandomVars() {
    std::vector<RandomVarDef> variables;
    auto add = [&](const char* name, double value) {
        RandomVarDef variable;
        variable.name = name;
        variable.minValue = value;
        variable.maxValue = value;
        variable.enabled = true;
        variables.push_back(variable);
    };
    add("lat", 30.0);
    add("lon", 110.0);
    add("alt", 3000.0);
    add("speed", 500.0);
    return variables;
}

// 同一份 Mo* 代码，既用于多对象 Harness，也用于单对象 Harness
std::string demoMoBody() {
    return
        "using MoModelType = void*;\n"
        "static MoModelType MoCreate(int objectId, const RandomBag& R) {\n"
        "    (void)objectId; (void)R; return Model_Create();\n"
        "}\n"
        "static int MoInit(MoModelType obj, int objectId, const RandomBag& R, double dt) {\n"
        "  WeaponModelParams p{};\n"
        "  p.init_lat = R.lat;\n"
        "  p.init_lon = R.lon;\n"
        "  p.init_alt = R.alt; p.init_speed = R.speed;\n"
        "  p.init_heading = 35.0; p.init_pitch = 12.0; p.step_dt = dt;\n"
        "  return Model_Init(obj, &p);\n"
        "}\n"
        "static int MoStep(MoModelType obj, int objectId, int stepIndex, double dt,\n"
        "                  const RandomBag& R,\n"
        "                  double& out_lat, double& out_lon) {\n"
        "  (void)objectId; (void)stepIndex; (void)dt; (void)R;\n"
        "  WeaponModelOutput o{}; int rc = Model_Step(obj, &o);\n"
        "  out_lat = o.lat; out_lon = o.lon; return rc;\n"
        "}\n"
        "static void MoDestroy(MoModelType obj, int objectId) {\n"
        "  (void)objectId; Model_Destroy(obj);\n"
        "}\n";
}

UserHarnessConfig makeDemoConfig(const std::string& headerPath,
                                 const std::string& workDir,
                                 const std::string& baseName) {
    UserHarnessConfig config;
    config.headerPaths = { headerPath };
    const QDir includeDir(QFileInfo(QString::fromStdString(headerPath)).absolutePath());
    config.includeDirs.push_back(
        QDir::toNativeSeparators(includeDir.absolutePath()).toStdString());
    // 模型包结构：根/include、根/lib、根/models
    const QDir packageDir(includeDir.absoluteFilePath(QStringLiteral("..")));
    const QDir libDir(packageDir.absoluteFilePath(QStringLiteral("lib")));
    const QDir modelsDir(packageDir.absoluteFilePath(QStringLiteral("models")));
    if (modelsDir.exists())
        config.libPaths.push_back(QDir::toNativeSeparators(modelsDir.absolutePath()).toStdString());
    if (libDir.exists()) {
        config.libPaths.push_back(QDir::toNativeSeparators(libDir.absolutePath()).toStdString());
        for (const QString& name : libDir.entryList({ QStringLiteral("*.lib") }, QDir::Files)) {
            if (name.endsWith(QStringLiteral("d.lib"))) continue;  // 跳过 Debug 版
            config.linkLibs.push_back(
                QDir::toNativeSeparators(libDir.absoluteFilePath(name)).toStdString());
        }
    }
    config.workDir = workDir;
    config.outputBaseName = baseName;
    config.randomVars = defaultRandomVars();
    config.userMainBody = demoMoBody();
    return config;
}

int runSingleObjectHarness(const std::string& headerPath, const std::string& workDir) {
    UserHarnessConfig config = makeDemoConfig(headerPath, workDir, "SingleObjectSmokeHarness");
    config.stepCount = 80;
    config.stepDt = 0.02;

    UserCodeHarness harness;
    const CompileResult compile = harness.Compile(config);
    std::cout << compile.log;
    if (!compile.success) return 21;

    if (!harness.SetTrajectoryCapture(true)) return 22;
    const RandomValueBlob blob = harness.Sample(42);
    int userReturn = 0;
    bool seh = false;
    std::string error;
    if (!harness.RunOnce(blob, &userReturn, &seh, error)) {
        std::cerr << "RunOnce failed: " << error << "\n";
        return 23;
    }
    std::vector<TrajectorySample> trajectory;
    harness.FetchTrajectory(trajectory);
    std::cout << "single: return=" << userReturn << " points=" << trajectory.size() << "\n";
    return userReturn == 0 && trajectory.size() == 80 ? 0 : 24;
}

int runUserPoolHarness(const std::string& headerPath, const std::string& workDir) {
    UserHarnessConfig config = makeDemoConfig(headerPath, workDir, "UserPoolSmokeHarness");

    // 生产路径：UserCodeHarness 编译一次，产出同时支持单对象与多对象的 DLL。
    UserCodeHarness compiler;
    const CompileResult compile = compiler.Compile(config);
    std::cout << compile.log;
    if (!compile.success) return 11;

    MultiObjectHarness harness;
    std::string loadError;
    if (!harness.LoadCompiledDll(compile.dllPath, loadError)) {
        std::cerr << "LoadCompiledDll failed: " << loadError << "\n";
        return 13;
    }

    MultiObjectTestConfig testConfig;
    testConfig.objectCount = 4;
    testConfig.stepCount = 80;
    testConfig.stepDt = 0.02;
    testConfig.tolerance = 1e-12;
    testConfig.schedule = MultiObjectSchedule::DeterministicRandom;
    const MultiObjectTestReport report = SingleThreadMultiObjectTester::Run(
        harness, testConfig, harness.Sample(42));
    std::cout << "userpool: " << report.verdict << ": " << report.summary << "\n";
    return report.verdict == "PASS" && report.completedObjects == 4 ? 0 : 12;
}

} // namespace

int main(int argc, char* argv[]) {
    QCoreApplication application(argc, argv);
    if (argc < 4) {
        std::cerr << "usage: smoke <userpool|single> <model_header> <workdir>\n";
        return 2;
    }
    const std::string mode = argv[1];
    if (mode == "userpool") return runUserPoolHarness(argv[2], argv[3]);
    if (mode == "single") return runSingleObjectHarness(argv[2], argv[3]);
    std::cerr << "unknown mode: " << mode << "\n";
    return 2;
}
