#ifndef USER_CODE_HARNESS_H
#define USER_CODE_HARNESS_H

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <string>
#include <vector>
#include <cstdint>

enum class RandomVarType {
    Double = 0,
    Int = 1
};

struct RandomVarDef {
    std::string name;
    RandomVarType type = RandomVarType::Double;
    double minValue = 0.0;
    double maxValue = 1.0;
    bool enabled = true;
};

struct UserHarnessConfig {
    std::vector<std::string> headerPaths;   // absolute paths, multi-select
    std::vector<std::string> includeDirs;   // extra -I
    std::vector<std::string> libPaths;      // /LIBPATH
    std::vector<std::string> linkLibs;      // .lib file names or paths
    std::string userMainBody;               // body of UserMain(const RandomBag& R)
    std::vector<RandomVarDef> randomVars;
    std::string workDir;                    // generated sources / dll output
    std::string outputBaseName;             // base name for .cpp/.dll (default UserHarness)
};

struct CompileResult {
    bool success = false;
    std::string dllPath;
    std::string sourcePath;
    std::string log;
};

// Packed random values written in declaration order (enabled vars only).
// For multi-object tests, values are packed per object:
//   doubles: [obj0 vars...][obj1 vars...]...
//   ints:    [obj0 vars...][obj1 vars...]...
struct RandomValueBlob {
    std::vector<double> doubles; // also used to store ints as double then cast
    std::vector<int> ints;
    std::string summary; // human readable
    int objectCount = 1;
};

struct TrajectorySample {
    double lat = 0.0;
    double lon = 0.0;
};

class UserCodeHarness {
public:
    UserCodeHarness();
    ~UserCodeHarness();

    // Generate RandomBag + wrapper source, compile to DLL via MSVC cl.exe
    CompileResult Compile(const UserHarnessConfig& config);

    bool IsLoaded() const { return m_hModule != NULL; }
    std::string DllPath() const { return m_dllPath; }
    const std::vector<RandomVarDef>& EnabledVars() const { return m_enabledVars; }

    bool LoadCompiledDll(const std::string& dllPath, std::string& err);
    void Unload();

    // Sample random values according to enabled var defs (thread-safe if each call has own rng seed)
    RandomValueBlob Sample(uint32_t seed) const;

    // Execute user main once. Returns false on SEH / loader failure.
    // outUserReturn: return value from UserMain; outSeh: true if hardware exception
    bool RunOnce(const RandomValueBlob& values, int* outUserReturn, bool* outSeh, std::string& err) const;

    // Trajectory capture (single-thread). Enable before RunOnce; UserMain should call RecordTrajectoryPoint.
    bool SetTrajectoryCapture(bool enabled) const;
    bool FetchTrajectory(std::vector<TrajectorySample>& out) const;

    static std::string DefaultUserMainTemplate();
    static std::string FindVcVars64Bat();

private:
    std::string GenerateSource(const UserHarnessConfig& config, const std::vector<RandomVarDef>& enabled) const;
    bool InvokeCl(const UserHarnessConfig& config, const std::string& srcPath, const std::string& outDll, std::string& log) const;

    void ApplyDllSearchPaths() const;

    HMODULE m_hModule = NULL;
    std::string m_dllPath;
    std::vector<RandomVarDef> m_enabledVars;
    std::vector<std::string> m_dllSearchDirs;

    // Export: int RunUserTest(const double* dvals, int nd, const int* ivals, int ni);
    typedef int (*FnRunUserTest)(const double*, int, const int*, int);
    typedef void (*FnSetTrajectoryCapture)(int);
    typedef int (*FnGetTrajectoryCount)();
    typedef int (*FnGetTrajectoryPoint)(int, double*, double*);

    FnRunUserTest m_pfnRun = nullptr;
    FnSetTrajectoryCapture m_pfnSetTraj = nullptr;
    FnGetTrajectoryCount m_pfnGetTrajCount = nullptr;
    FnGetTrajectoryPoint m_pfnGetTrajPoint = nullptr;
};

#endif // USER_CODE_HARNESS_H
