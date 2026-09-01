#ifndef DLL_LOADER_H
#define DLL_LOADER_H

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <string>
#include <vector>
#include "../utils/SehHelper.h"
#include "../utils/MemoryUtils.h"

enum class ModelApiStyle {
    Unknown = 0,
    Singleton,
    HandleBased,
    CustomSequence
};

enum class CallPhase {
    Setup = 0,    // 整个生命周期开始时执行一次（Create / Init 等）
    Step = 1,     // 每个仿真步长执行一次
    Teardown = 2  // 全部结束后执行一次
};

enum class CallSignature {
    VoidNoArg = 0,
    IntNoArg,
    IntParams,
    IntOutput,
    CreateHandle,
    IntHandleParams,
    IntHandleOutput,
    VoidHandle,
    GetInfoStr
};

struct CallMappingEntry {
    std::string symbolName;
    CallPhase phase = CallPhase::Setup;
    CallSignature signature = CallSignature::IntParams;
    int handleSlot = 0;
    bool enabled = true;
};

struct InterfaceMapping {
    std::vector<CallMappingEntry> entries;

    static InterfaceMapping DefaultSingleton();
    static InterfaceMapping DefaultHandleBased();
    static const char* PhaseName(CallPhase p);
    static const char* SignatureName(CallSignature s);
    ModelApiStyle InferApiStyle() const;
};

struct CallContext {
    WeaponModelParams params{};
    WeaponModelOutput lastOutput{};
    std::vector<ModelHandle> handleSlots;
    std::string lastInfo;
    int lastIntResult = 0;
};

struct LoadResult {
    bool isLoaded = false;
    double initialMemoryDeltaKB = 0.0;
    std::string errorLog;
    DWORD exceptionCode = 0;
    int boundSymbolCount = 0;
    int missingSymbolCount = 0;
    ModelApiStyle apiStyle = ModelApiStyle::Unknown;
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
    ModelApiStyle GetApiStyle() const { return m_apiStyle; }
    const InterfaceMapping& GetMapping() const { return m_mapping; }

    bool RunPhase(CallPhase phase, CallContext& ctx, std::string& outErrorStr);
    bool RunLifecycle(CallContext& ctx, int stepCount, std::string& outErrorStr,
                      std::vector<WeaponModelOutput>* outHistory = nullptr,
                      std::vector<double>* outStepTimesMs = nullptr);

    FnModelInit GetInitFn() const;
    FnModelStep GetStepFn() const;
    FnModelDestroy GetDestroyFn() const;
    FnModelCreate GetCreateFn() const;
    FnModelInitEx GetInitExFn() const;
    FnModelStepEx GetStepExFn() const;
    FnModelDestroyEx GetDestroyExFn() const;

private:
    struct BoundEntry {
        CallMappingEntry meta;
        FARPROC proc = nullptr;
    };

    bool InvokeBound(const BoundEntry& be, CallContext& ctx, std::string& outErrorStr);
    void EnsureHandleSlot(CallContext& ctx, int slot) const;
    ModelHandle GetHandle(const CallContext& ctx, int slot) const;

    HMODULE m_hModule = NULL;
    std::string m_dllPath;
    InterfaceMapping m_mapping;
    ModelApiStyle m_apiStyle = ModelApiStyle::Unknown;
    std::vector<BoundEntry> m_bound;
};

#endif // DLL_LOADER_H
