#include "DllLoader.h"
#include <iostream>

DllLoader::DllLoader()
    : m_hModule(NULL)
    , m_pfnInit(nullptr)
    , m_pfnStep(nullptr)
    , m_pfnDestroy(nullptr)
    , m_pfnGetInfo(nullptr) {
}

DllLoader::~DllLoader() {
    Unload();
}

LoadResult DllLoader::Load(const std::string& dllPath, const InterfaceMapping& mapping) {
    Unload();

    LoadResult result;
    m_dllPath = dllPath;
    m_mapping = mapping;

    // Convert path to wide string
    std::wstring wPath(dllPath.begin(), dllPath.end());

    // Record memory before loading
    ProcessMemoryStats memBefore = MemoryUtils::GetCurrentProcessMemory();

    // Use LOAD_WITH_ALTERED_SEARCH_PATH so dependencies in DLL directory are resolved
    m_hModule = LoadLibraryExW(wPath.c_str(), NULL, LOAD_WITH_ALTERED_SEARCH_PATH);

    ProcessMemoryStats memAfter = MemoryUtils::GetCurrentProcessMemory();
    result.initialMemoryDeltaKB = MemoryUtils::BytesToKB(
        memAfter.workingSetBytes > memBefore.workingSetBytes ? 
        (memAfter.workingSetBytes - memBefore.workingSetBytes) : 0
    );

    if (!m_hModule) {
        DWORD err = GetLastError();
        result.isLoaded = false;
        result.exceptionCode = err;
        result.errorLog = "LoadLibraryExW failed with Win32 Error Code: " + std::to_string(err);
        return result;
    }

    result.isLoaded = true;

    // Bind function pointers
    m_pfnInit = reinterpret_cast<FnModelInit>(GetProcAddress(m_hModule, m_mapping.initFuncName.c_str()));
    m_pfnStep = reinterpret_cast<FnModelStep>(GetProcAddress(m_hModule, m_mapping.stepFuncName.c_str()));
    m_pfnDestroy = reinterpret_cast<FnModelDestroy>(GetProcAddress(m_hModule, m_mapping.destroyFuncName.c_str()));
    m_pfnGetInfo = reinterpret_cast<FnModelGetInfo>(GetProcAddress(m_hModule, m_mapping.getInfoFuncName.c_str()));

    result.initBound = (m_pfnInit != nullptr);
    result.stepBound = (m_pfnStep != nullptr);
    result.destroyBound = (m_pfnDestroy != nullptr);
    result.getInfoBound = (m_pfnGetInfo != nullptr);

    if (!result.initBound) result.errorLog += "Warning: Symbol '" + m_mapping.initFuncName + "' not found. ";
    if (!result.stepBound) result.errorLog += "Error: Symbol '" + m_mapping.stepFuncName + "' not found. ";

    return result;
}

void DllLoader::Unload() {
    if (m_hModule) {
        // Call destroy before unloading if available
        if (m_pfnDestroy) {
            DWORD exc = 0;
            SafeCallDestroy(m_pfnDestroy, &exc);
        }
        FreeLibrary(m_hModule);
        m_hModule = NULL;
    }
    m_pfnInit = nullptr;
    m_pfnStep = nullptr;
    m_pfnDestroy = nullptr;
    m_pfnGetInfo = nullptr;
}

bool DllLoader::CallInit(const WeaponModelParams& params, int& outResult, std::string& outErrorStr) {
    if (!m_pfnInit) {
        outErrorStr = "Model_Init pointer is null (not bound).";
        return false;
    }
    DWORD excCode = 0;
    bool success = SafeCallInit(m_pfnInit, &params, &outResult, &excCode);
    if (!success) {
        outErrorStr = "Hardware Exception in Model_Init: " + SehCodeToString(excCode);
    }
    return success;
}

bool DllLoader::CallStep(WeaponModelOutput& output, int& outResult, std::string& outErrorStr) {
    if (!m_pfnStep) {
        outErrorStr = "Model_Step pointer is null (not bound).";
        return false;
    }
    DWORD excCode = 0;
    bool success = SafeCallStep(m_pfnStep, &output, &outResult, &excCode);
    if (!success) {
        outErrorStr = "Hardware Exception in Model_Step: " + SehCodeToString(excCode);
    }
    return success;
}

bool DllLoader::CallDestroy(std::string& outErrorStr) {
    if (!m_pfnDestroy) {
        outErrorStr = "Model_Destroy pointer is null.";
        return true; // Not fatal
    }
    DWORD excCode = 0;
    bool success = SafeCallDestroy(m_pfnDestroy, &excCode);
    if (!success) {
        outErrorStr = "Hardware Exception in Model_Destroy: " + SehCodeToString(excCode);
    }
    return success;
}

bool DllLoader::CallGetInfo(std::string& outInfo, std::string& outErrorStr) {
    if (!m_pfnGetInfo) {
        outInfo = "N/A";
        return true;
    }
    DWORD excCode = 0;
    bool success = SafeCallGetInfo(m_pfnGetInfo, outInfo, &excCode);
    if (!success) {
        outErrorStr = "Hardware Exception in Model_GetInfo: " + SehCodeToString(excCode);
    }
    return success;
}
