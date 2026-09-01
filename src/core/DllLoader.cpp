#include "DllLoader.h"
#include <chrono>
#include <algorithm>

InterfaceMapping InterfaceMapping::DefaultSingleton() {
    InterfaceMapping m;
    m.entries.push_back({ "Model_Init", CallPhase::Setup, CallSignature::IntParams, 0, true });
    m.entries.push_back({ "Model_Step", CallPhase::Step, CallSignature::IntOutput, 0, true });
    m.entries.push_back({ "Model_Destroy", CallPhase::Teardown, CallSignature::VoidNoArg, 0, true });
    return m;
}

InterfaceMapping InterfaceMapping::DefaultHandleBased() {
    InterfaceMapping m;
    m.entries.push_back({ "Model_Create", CallPhase::Setup, CallSignature::CreateHandle, 0, true });
    m.entries.push_back({ "Model_Init", CallPhase::Setup, CallSignature::IntHandleParams, 0, true });
    m.entries.push_back({ "Model_Step", CallPhase::Step, CallSignature::IntHandleOutput, 0, true });
    m.entries.push_back({ "Model_Destroy", CallPhase::Teardown, CallSignature::VoidHandle, 0, true });
    return m;
}

const char* InterfaceMapping::PhaseName(CallPhase p) {
    switch (p) {
    case CallPhase::Setup: return "Setup(仅一次)";
    case CallPhase::Step: return "Step(每步长一次)";
    case CallPhase::Teardown: return "Teardown(结束一次)";
    default: return "Unknown";
    }
}

const char* InterfaceMapping::SignatureName(CallSignature s) {
    switch (s) {
    case CallSignature::VoidNoArg: return "void f()";
    case CallSignature::IntNoArg: return "int f()";
    case CallSignature::IntParams: return "int f(Params*)";
    case CallSignature::IntOutput: return "int f(Output*)";
    case CallSignature::CreateHandle: return "void* f() [Create]";
    case CallSignature::IntHandleParams: return "int f(handle, Params*)";
    case CallSignature::IntHandleOutput: return "int f(handle, Output*)";
    case CallSignature::VoidHandle: return "void f(handle)";
    case CallSignature::GetInfoStr: return "const char* f()";
    default: return "Unknown";
    }
}

ModelApiStyle InterfaceMapping::InferApiStyle() const {
    bool hasCreate = false;
    bool hasHandleCall = false;
    bool hasSingleton = false;
    for (const auto& e : entries) {
        if (!e.enabled) continue;
        if (e.signature == CallSignature::CreateHandle) hasCreate = true;
        if (e.signature == CallSignature::IntHandleParams
            || e.signature == CallSignature::IntHandleOutput
            || e.signature == CallSignature::VoidHandle) hasHandleCall = true;
        if (e.signature == CallSignature::IntParams
            || e.signature == CallSignature::IntOutput
            || e.signature == CallSignature::VoidNoArg) hasSingleton = true;
    }
    if (hasCreate || hasHandleCall) {
        if (hasSingleton && !hasCreate) return ModelApiStyle::CustomSequence;
        return ModelApiStyle::HandleBased;
    }
    if (hasSingleton) return ModelApiStyle::Singleton;
    if (!entries.empty()) return ModelApiStyle::CustomSequence;
    return ModelApiStyle::Unknown;
}

DllLoader::DllLoader() = default;

DllLoader::~DllLoader() {
    Unload();
}

LoadResult DllLoader::Load(const std::string& dllPath, const InterfaceMapping& mapping) {
    Unload();

    LoadResult result;
    m_dllPath = dllPath;
    m_mapping = mapping.entries.empty() ? InterfaceMapping::DefaultSingleton() : mapping;
    m_apiStyle = m_mapping.InferApiStyle();

    std::wstring wPath(dllPath.begin(), dllPath.end());
    ProcessMemoryStats memBefore = MemoryUtils::GetCurrentProcessMemory();
    m_hModule = LoadLibraryExW(wPath.c_str(), NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    ProcessMemoryStats memAfter = MemoryUtils::GetCurrentProcessMemory();
    result.initialMemoryDeltaKB = MemoryUtils::BytesToKB(
        memAfter.workingSetBytes > memBefore.workingSetBytes ?
        (memAfter.workingSetBytes - memBefore.workingSetBytes) : 0);

    if (!m_hModule) {
        DWORD err = GetLastError();
        result.isLoaded = false;
        result.exceptionCode = err;
        result.errorLog = "LoadLibraryExW failed with Win32 Error Code: " + std::to_string(err);
        return result;
    }

    result.isLoaded = true;
    result.apiStyle = m_apiStyle;

    for (const auto& e : m_mapping.entries) {
        if (!e.enabled || e.symbolName.empty()) continue;
        BoundEntry be;
        be.meta = e;
        be.proc = GetProcAddress(m_hModule, e.symbolName.c_str());
        if (be.proc) {
            result.boundSymbolCount++;
        } else {
            result.missingSymbolCount++;
            result.errorLog += "Warning: Symbol '" + e.symbolName + "' not found. ";
        }
        m_bound.push_back(be);
    }

    if (result.boundSymbolCount == 0) {
        result.errorLog += "Error: No mapped symbols could be bound. ";
    }

    result.errorLog += "INFO: Bound " + std::to_string(result.boundSymbolCount)
        + " symbol(s), missing " + std::to_string(result.missingSymbolCount)
        + ", apiStyle=" + std::to_string(static_cast<int>(m_apiStyle)) + ". ";

    return result;
}

void DllLoader::Unload() {
    if (m_hModule) {
        FreeLibrary(m_hModule);
        m_hModule = NULL;
    }
    m_bound.clear();
    m_apiStyle = ModelApiStyle::Unknown;
}

void DllLoader::EnsureHandleSlot(CallContext& ctx, int slot) const {
    if (slot < 0) return;
    if (static_cast<int>(ctx.handleSlots.size()) <= slot) {
        ctx.handleSlots.resize(static_cast<size_t>(slot) + 1, nullptr);
    }
}

ModelHandle DllLoader::GetHandle(const CallContext& ctx, int slot) const {
    if (slot < 0 || slot >= static_cast<int>(ctx.handleSlots.size())) return nullptr;
    return ctx.handleSlots[static_cast<size_t>(slot)];
}

bool DllLoader::InvokeBound(const BoundEntry& be, CallContext& ctx, std::string& outErrorStr) {
    if (!be.proc) {
        outErrorStr = "Symbol '" + be.meta.symbolName + "' is not bound.";
        return false;
    }

    DWORD exc = 0;
    int intRes = 0;
    const int slot = be.meta.handleSlot;

    switch (be.meta.signature) {
    case CallSignature::VoidNoArg: {
        if (!SafeCallVoidNoArg(reinterpret_cast<FnVoidNoArg>(be.proc), &exc)) {
            outErrorStr = be.meta.symbolName + " SEH: " + SehCodeToString(exc);
            return false;
        }
        return true;
    }
    case CallSignature::IntNoArg: {
        if (!SafeCallIntNoArg(reinterpret_cast<FnIntNoArg>(be.proc), &intRes, &exc)) {
            outErrorStr = be.meta.symbolName + " SEH: " + SehCodeToString(exc);
            return false;
        }
        ctx.lastIntResult = intRes;
        return true;
    }
    case CallSignature::IntParams: {
        if (!SafeCallInit(reinterpret_cast<FnModelInit>(be.proc), &ctx.params, &intRes, &exc)) {
            outErrorStr = be.meta.symbolName + " SEH: " + SehCodeToString(exc);
            return false;
        }
        ctx.lastIntResult = intRes;
        return true;
    }
    case CallSignature::IntOutput: {
        WeaponModelOutput out{};
        if (!SafeCallStep(reinterpret_cast<FnModelStep>(be.proc), &out, &intRes, &exc)) {
            outErrorStr = be.meta.symbolName + " SEH: " + SehCodeToString(exc);
            return false;
        }
        ctx.lastOutput = out;
        ctx.lastIntResult = intRes;
        return true;
    }
    case CallSignature::CreateHandle: {
        ModelHandle h = nullptr;
        if (!SafeCallCreate(reinterpret_cast<FnModelCreate>(be.proc), &h, &exc) || !h) {
            outErrorStr = (exc != 0)
                ? (be.meta.symbolName + " SEH: " + SehCodeToString(exc))
                : (be.meta.symbolName + " returned null handle");
            return false;
        }
        EnsureHandleSlot(ctx, slot);
        ctx.handleSlots[static_cast<size_t>(slot)] = h;
        return true;
    }
    case CallSignature::IntHandleParams: {
        ModelHandle h = GetHandle(ctx, slot);
        if (!h) {
            outErrorStr = be.meta.symbolName + ": handle slot " + std::to_string(slot) + " is null (Create first?)";
            return false;
        }
        if (!SafeCallInitEx(reinterpret_cast<FnModelInitEx>(be.proc), h, &ctx.params, &intRes, &exc)) {
            outErrorStr = be.meta.symbolName + " SEH: " + SehCodeToString(exc);
            return false;
        }
        ctx.lastIntResult = intRes;
        return true;
    }
    case CallSignature::IntHandleOutput: {
        ModelHandle h = GetHandle(ctx, slot);
        if (!h) {
            outErrorStr = be.meta.symbolName + ": handle slot " + std::to_string(slot) + " is null";
            return false;
        }
        WeaponModelOutput out{};
        if (!SafeCallStepEx(reinterpret_cast<FnModelStepEx>(be.proc), h, &out, &intRes, &exc)) {
            outErrorStr = be.meta.symbolName + " SEH: " + SehCodeToString(exc);
            return false;
        }
        ctx.lastOutput = out;
        ctx.lastIntResult = intRes;
        return true;
    }
    case CallSignature::VoidHandle: {
        ModelHandle h = GetHandle(ctx, slot);
        if (!h) {
            outErrorStr = be.meta.symbolName + ": handle slot " + std::to_string(slot) + " is null";
            return false;
        }
        if (!SafeCallDestroyEx(reinterpret_cast<FnModelDestroyEx>(be.proc), h, &exc)) {
            outErrorStr = be.meta.symbolName + " SEH: " + SehCodeToString(exc);
            return false;
        }
        ctx.handleSlots[static_cast<size_t>(slot)] = nullptr;
        return true;
    }
    case CallSignature::GetInfoStr: {
        std::string info;
        if (!SafeCallGetInfo(reinterpret_cast<FnModelGetInfo>(be.proc), info, &exc)) {
            outErrorStr = be.meta.symbolName + " SEH: " + SehCodeToString(exc);
            return false;
        }
        ctx.lastInfo = info;
        return true;
    }
    default:
        outErrorStr = "Unsupported signature for " + be.meta.symbolName;
        return false;
    }
}

bool DllLoader::RunPhase(CallPhase phase, CallContext& ctx, std::string& outErrorStr) {
    for (const auto& be : m_bound) {
        if (!be.meta.enabled) continue;
        if (be.meta.phase != phase) continue;
        if (!InvokeBound(be, ctx, outErrorStr)) {
            return false;
        }
    }
    return true;
}

bool DllLoader::RunLifecycle(CallContext& ctx, int stepCount, std::string& outErrorStr,
                             std::vector<WeaponModelOutput>* outHistory,
                             std::vector<double>* outStepTimesMs) {
    // Setup once at the beginning (may be empty)
    if (!RunPhase(CallPhase::Setup, ctx, outErrorStr)) {
        return false;
    }

    const bool hasStep = std::any_of(m_bound.begin(), m_bound.end(), [](const BoundEntry& b) {
        return b.meta.enabled && b.meta.phase == CallPhase::Step;
    });

    if (hasStep) {
        for (int i = 0; i < stepCount; ++i) {
            auto t0 = std::chrono::high_resolution_clock::now();
            if (!RunPhase(CallPhase::Step, ctx, outErrorStr)) {
                return false;
            }
            auto t1 = std::chrono::high_resolution_clock::now();
            if (outStepTimesMs) {
                outStepTimesMs->push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
            }
            if (outHistory && static_cast<int>(outHistory->size()) < 100) {
                outHistory->push_back(ctx.lastOutput);
            }
        }
    }

    std::string teardownErr;
    RunPhase(CallPhase::Teardown, ctx, teardownErr);
    if (!teardownErr.empty() && outErrorStr.empty()) {
        outErrorStr = teardownErr;
    }
    return true;
}

FnModelInit DllLoader::GetInitFn() const {
    for (const auto& be : m_bound) {
        if (be.meta.enabled && be.meta.signature == CallSignature::IntParams && be.proc)
            return reinterpret_cast<FnModelInit>(be.proc);
    }
    return nullptr;
}

FnModelStep DllLoader::GetStepFn() const {
    for (const auto& be : m_bound) {
        if (be.meta.enabled && be.meta.signature == CallSignature::IntOutput && be.proc)
            return reinterpret_cast<FnModelStep>(be.proc);
    }
    return nullptr;
}

FnModelDestroy DllLoader::GetDestroyFn() const {
    for (const auto& be : m_bound) {
        if (be.meta.enabled && be.meta.signature == CallSignature::VoidNoArg
            && be.meta.phase == CallPhase::Teardown && be.proc)
            return reinterpret_cast<FnModelDestroy>(be.proc);
    }
    return nullptr;
}

FnModelCreate DllLoader::GetCreateFn() const {
    for (const auto& be : m_bound) {
        if (be.meta.enabled && be.meta.signature == CallSignature::CreateHandle && be.proc)
            return reinterpret_cast<FnModelCreate>(be.proc);
    }
    return nullptr;
}

FnModelInitEx DllLoader::GetInitExFn() const {
    for (const auto& be : m_bound) {
        if (be.meta.enabled && be.meta.signature == CallSignature::IntHandleParams && be.proc)
            return reinterpret_cast<FnModelInitEx>(be.proc);
    }
    return nullptr;
}

FnModelStepEx DllLoader::GetStepExFn() const {
    for (const auto& be : m_bound) {
        if (be.meta.enabled && be.meta.signature == CallSignature::IntHandleOutput && be.proc)
            return reinterpret_cast<FnModelStepEx>(be.proc);
    }
    return nullptr;
}

FnModelDestroyEx DllLoader::GetDestroyExFn() const {
    for (const auto& be : m_bound) {
        if (be.meta.enabled && be.meta.signature == CallSignature::VoidHandle && be.proc)
            return reinterpret_cast<FnModelDestroyEx>(be.proc);
    }
    return nullptr;
}
