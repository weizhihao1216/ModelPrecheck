#ifndef SEH_HELPER_H
#define SEH_HELPER_H

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <string>

// Weapon model data structures
#pragma pack(push, 8)
struct WeaponModelParams {
    double init_lat;       // Initial latitude (deg) [-90, 90]
    double init_lon;       // Initial longitude (deg) [-180, 180]
    double init_alt;       // Initial altitude (m)
    double init_speed;     // Initial velocity (m/s)
    double init_heading;   // Initial heading/yaw angle (deg)
    double init_pitch;     // Initial pitch angle (deg)
    double init_roll;      // Initial roll angle (deg)
    double step_dt;        // Simulation step size (s), e.g., 0.02 for 50Hz
};

struct WeaponModelOutput {
    double sim_time;       // Current simulation time (s)
    double lat;            // Current latitude (deg)
    double lon;            // Current longitude (deg)
    double alt;            // Current altitude (m)
    double vx;             // North velocity (m/s)
    double vy;             // East velocity (m/s)
    double vz;             // Vertical velocity (m/s)
    double pitch;          // Pitch angle (deg or rad)
    double roll;           // Roll angle (deg or rad)
    double yaw;            // Yaw angle (deg or rad)
    int status;            // Model status: 0=Normal, 1=Target Hit/Finished, <0=Error
};
#pragma pack(pop)

// Function pointer prototypes — Singleton (global-state) API
typedef int (*FnModelInit)(const WeaponModelParams* params);
typedef int (*FnModelStep)(WeaponModelOutput* output);
typedef void (*FnModelDestroy)();
typedef const char* (*FnModelGetInfo)();

// Function pointer prototypes — Handle-based (multi-instance) API
typedef void* ModelHandle;
typedef ModelHandle (*FnModelCreate)();
typedef int (*FnModelInitEx)(ModelHandle handle, const WeaponModelParams* params);
typedef int (*FnModelStepEx)(ModelHandle handle, WeaponModelOutput* output);
typedef void (*FnModelDestroyEx)(ModelHandle handle);

// SEH Helper functions to isolate hardware exceptions (Access Violation, Divide-by-Zero, Stack Overflow)
std::string SehCodeToString(DWORD code);

typedef void (*FnVoidNoArg)();
typedef int (*FnIntNoArg)();

bool SafeCallVoidNoArg(FnVoidNoArg fn, DWORD* outExceptionCode);
bool SafeCallIntNoArg(FnIntNoArg fn, int* outResult, DWORD* outExceptionCode);

bool SafeCallInit(FnModelInit fn, const WeaponModelParams* params, int* outResult, DWORD* outExceptionCode);
bool SafeCallStep(FnModelStep fn, WeaponModelOutput* output, int* outResult, DWORD* outExceptionCode);
bool SafeCallDestroy(FnModelDestroy fn, DWORD* outExceptionCode);
bool SafeCallGetInfo(FnModelGetInfo fn, std::string& outInfo, DWORD* outExceptionCode);

bool SafeCallCreate(FnModelCreate fn, ModelHandle* outHandle, DWORD* outExceptionCode);
bool SafeCallInitEx(FnModelInitEx fn, ModelHandle handle, const WeaponModelParams* params, int* outResult, DWORD* outExceptionCode);
bool SafeCallStepEx(FnModelStepEx fn, ModelHandle handle, WeaponModelOutput* output, int* outResult, DWORD* outExceptionCode);
bool SafeCallDestroyEx(FnModelDestroyEx fn, ModelHandle handle, DWORD* outExceptionCode);

/**
 * 加载被测模块的私有副本（永不卸载）。
 *
 * 为什么不能对被测模块调用 FreeLibrary：模型在测试中可能破坏自身状态（例如单实例模型被要求
 * 创建多个对象时越界写入自己的静态数据），其 DETACH 代码可能死循环。DETACH 是在装载锁
 * （loader lock）内执行的，一旦卡住，本进程之后任何 LoadLibrary / FreeLibrary——包括打开
 * 文件对话框时加载 Shell 扩展——都会永久阻塞，表现为整个程序假死且无法恢复。
 * 因此这里改为：把模块复制成唯一路径的副本再加载，并且从不卸载它。
 * 既保证每次加载都是全新的模块状态，又完全不执行被测模块的 DETACH 代码。
 *
 * @param sourcePath  源 DLL 路径（UTF-8）
 * @param copyDir     副本目录（UTF-8；传空则使用源 DLL 所在目录，便于依赖 DLL 被找到）
 * @param outCopyPath 实际加载的副本路径（UTF-8，可为空）
 * @param error       失败原因（可为空）
 * @return 模块句柄；失败返回 nullptr
 */
HMODULE LoadPrivateModuleCopy(const std::string& sourcePath, const std::string& copyDir,
                              std::string* outCopyPath = nullptr, std::string* error = nullptr);

/** 进程内是否加载过私有副本；为真时进程退出需直接终止，否则会卡在副本的 DETACH 上。 */
bool HasLoadedPrivateModule();

/** 清理上次运行遗留的私有副本（请在进程启动、尚未加载任何副本时调用）。 */
void CleanupPrivateModuleCopies(const std::string& modelsRootDir);

#endif // SEH_HELPER_H
