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

#endif // SEH_HELPER_H
