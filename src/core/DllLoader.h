#ifndef DLL_LOADER_H
#define DLL_LOADER_H

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <string>
#include <map>
#include "../utils/SehHelper.h"
#include "../utils/MemoryUtils.h"

struct InterfaceMapping {
    std::string initFuncName = "Model_Init";
    std::string stepFuncName = "Model_Step";
    std::string destroyFuncName = "Model_Destroy";
    std::string getInfoFuncName = "Model_GetInfo";
};

struct LoadResult {
    bool isLoaded = false;
    double initialMemoryDeltaKB = 0.0;
    std::string errorLog;
    DWORD exceptionCode = 0;
    bool initBound = false;
    bool stepBound = false;
    bool destroyBound = false;
    bool getInfoBound = false;
};

class DllLoader {
public:
    DllLoader();
    ~DllLoader();

    LoadResult Load(const std::string& dllPath, const InterfaceMapping& mapping = InterfaceMapping());
    void Unload();

    bool IsLoaded() const { return m_hModule != NULL; }
    HMODULE GetModuleHandle() const { return m_hModule; }
    std::string GetDllPath() const { return m_dllPath; }

    // Safe Invocation API wrapped in Win32 SEH
    bool CallInit(const WeaponModelParams& params, int& outResult, std::string& outErrorStr);
    bool CallStep(WeaponModelOutput& output, int& outResult, std::string& outErrorStr);
    bool CallDestroy(std::string& outErrorStr);
    bool CallGetInfo(std::string& outInfo, std::string& outErrorStr);

private:
    HMODULE m_hModule;
    std::string m_dllPath;
    InterfaceMapping m_mapping;

    FnModelInit m_pfnInit;
    FnModelStep m_pfnStep;
    FnModelDestroy m_pfnDestroy;
    FnModelGetInfo m_pfnGetInfo;
};

#endif // DLL_LOADER_H
