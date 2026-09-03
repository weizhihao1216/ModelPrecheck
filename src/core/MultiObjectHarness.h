#ifndef MULTI_OBJECT_HARNESS_H
#define MULTI_OBJECT_HARNESS_H

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <string>
#include <vector>
#include "UserCodeHarness.h"
#include "SingleThreadMultiObjectTester.h"
#include "InterfaceMappingProfile.h"
#include "../utils/SehHelper.h"

struct MultiObjectHarnessConfig {
    std::vector<std::string> headerPaths;
    std::vector<std::string> sourcePaths;
    std::vector<std::string> includeDirs;
    std::vector<std::string> libPaths;
    std::vector<std::string> linkLibs;
    std::vector<RandomVarDef> randomVars;
    std::string adapterCode;
    std::string workDir;
    std::string outputBaseName;
};

class MultiObjectHarness {
public:
    MultiObjectHarness();
    ~MultiObjectHarness();

    CompileResult Compile(const MultiObjectHarnessConfig& config);
    CompileResult CompileMapping(const InterfaceMappingProfile& profile,
                                 const std::vector<RandomVarDef>& randomVars,
                                 const std::string& workDir,
                                 const std::string& outputBaseName);
    CompileResult CompileUserPool(const UserHarnessConfig& config);
    bool LoadModelDll(const std::string& dllPath,
                      const std::vector<RandomVarDef>& randomVars,
                      std::string& error);
    void Unload();
    bool IsLoaded() const {
        return m_hModule != NULL
            && (m_pfnRun != nullptr
                || (m_pfnCreate && m_pfnInitEx && m_pfnStepEx && m_pfnDestroyEx));
    }
    bool IsDirectModelMode() const { return m_directModelMode; }
    std::string DllPath() const { return m_dllPath; }
    const std::vector<RandomVarDef>& EnabledVars() const { return m_enabledVars; }
    RandomValueBlob Sample(uint32_t seed, int objectCount = 1) const;

    bool Run(const MultiObjectTestConfig& config, const RandomValueBlob& values,
             MultiObjectTestReport& report, std::string& error) const;

    /// Host-driven object session (for cross-model interleaving).
    /// Direct-model mode always supports this; user-pool mode requires MoPool_* exports
    /// (recompile multi-object Harness after upgrading the tool).
    bool SupportsObjectSession() const;
    bool PrepareObjectSession(const RandomValueBlob& values, int objectCount, double dt,
                              std::string& error);
    void* CreateLiveObject(int objectId, unsigned long* exceptionCode) const;
    int InitLiveObject(void* object, int objectId, unsigned long* exceptionCode) const;
    int StepLiveObject(void* object, int objectId, int stepIndex,
                       double* outLat, double* outLon,
                       unsigned long* exceptionCode) const;
    void DestroyLiveObject(void* object, int objectId, unsigned long* exceptionCode) const;

    static std::string DefaultAdapterTemplate();
    static std::string DefaultUserMultiObjectTemplate();

private:
    std::string GenerateSource(const MultiObjectHarnessConfig& config) const;
    std::string GenerateUserPoolSource(const UserHarnessConfig& config) const;
    static std::string LoadModelObjectKitHeader();
    std::string GeneratePerObjectRandomPreamble() const;
    bool InvokeCl(const MultiObjectHarnessConfig& config,
                  const std::string& sourcePath,
                  const std::string& outputDll,
                  std::string& log) const;
    bool LoadCompiledDll(const std::string& path, std::string& error);
    bool RunDirectModel(const MultiObjectTestConfig& config,
                        const RandomValueBlob& values,
                        MultiObjectTestReport& report,
                        std::string& error) const;

    typedef int (*FnRunMultiObject)(const double*, int, const int*, int,
                                    int, int, double, double, int, unsigned int);
    typedef int (*FnGetObjectResult)(int, int*, int*, int*, unsigned long*,
                                     int*, double*, const char**, int*, int*);
    typedef int (*FnGetTrackPoint)(int, int, int, double*, double*);
    typedef double (*FnGetMetric)();
    typedef int (*FnMoPoolPrepare)(const double*, int, const int*, int, int, double);
    typedef void* (*FnMoPoolCreate)(int);
    typedef int (*FnMoPoolInit)(void*, int, unsigned long*);
    typedef int (*FnMoPoolStep)(void*, int, int, double*, double*, unsigned long*);
    typedef void (*FnMoPoolDestroy)(void*, int, unsigned long*);

    HMODULE m_hModule = NULL;
    std::string m_dllPath;
    std::vector<RandomVarDef> m_enabledVars;
    std::vector<std::string> m_searchDirs;
    FnRunMultiObject m_pfnRun = nullptr;
    FnGetObjectResult m_pfnGetResult = nullptr;
    FnGetTrackPoint m_pfnGetTrackPoint = nullptr;
    FnGetMetric m_pfnGetMaxFrameMs = nullptr;
    FnMoPoolPrepare m_pfnMoPoolPrepare = nullptr;
    FnMoPoolCreate m_pfnMoPoolCreate = nullptr;
    FnMoPoolInit m_pfnMoPoolInit = nullptr;
    FnMoPoolStep m_pfnMoPoolStep = nullptr;
    FnMoPoolDestroy m_pfnMoPoolDestroy = nullptr;
    FnModelCreate m_pfnCreate = nullptr;
    FnModelInitEx m_pfnInitEx = nullptr;
    FnModelStepEx m_pfnStepEx = nullptr;
    FnModelDestroyEx m_pfnDestroyEx = nullptr;
    bool m_directModelMode = false;

    mutable RandomValueBlob m_sessionValues;
    mutable int m_sessionObjectCount = 0;
    mutable double m_sessionDt = 0.02;

    WeaponModelParams BuildDirectParams(int objectId) const;
};

#endif // MULTI_OBJECT_HARNESS_H
