#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cmath>
#include <cstring>

#define WEAPON_EXPORT __declspec(dllexport)

static const wchar_t* kSharedMapping = L"Local\\ModelPrecheck_ConflictWeaponShared";
static const unsigned int kOwnerTag = 0x00000042u;
static const unsigned int kMagic = 0x434F4E46u;
static const DWORD kOverlapWaitMs = 400;

#pragma pack(push, 8)
struct WeaponModelParams {
    double init_lat, init_lon, init_alt, init_speed;
    double init_heading, init_pitch, init_roll, step_dt;
};
struct WeaponModelOutput {
    double sim_time, lat, lon, alt, vx, vy, vz, pitch, roll, yaw;
    int status;
};
#pragma pack(pop)

struct SharedBlock {
    unsigned int magic;
    volatile LONG countA;
    volatile LONG countB;
    unsigned int lastOwnerTag;
    unsigned int stepCount;
    double lat, lon, alt, speed, pitch, yaw, roll, simTime, dt;
};

static HANDLE g_map = NULL;
static SharedBlock* g_shared = nullptr;
static LONG g_viewRef = 0;
static bool g_initialized = false;

static bool MapShared() {
    if (g_shared) {
        InterlockedIncrement(&g_viewRef);
        return true;
    }
    g_map = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
                               0, sizeof(SharedBlock), kSharedMapping);
    if (!g_map) return false;
    g_shared = static_cast<SharedBlock*>(
        MapViewOfFile(g_map, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedBlock)));
    if (!g_shared) {
        CloseHandle(g_map);
        g_map = NULL;
        return false;
    }
    InterlockedExchange(&g_viewRef, 1);
    return true;
}

static void UnmapShared() {
    if (InterlockedDecrement(&g_viewRef) > 0) return;
    if (g_shared) {
        UnmapViewOfFile(g_shared);
        g_shared = nullptr;
    }
    if (g_map) {
        CloseHandle(g_map);
        g_map = nullptr;
    }
}

extern "C" {

WEAPON_EXPORT int Model_Init(const WeaponModelParams* params) {
    if (!params) return -1;
    if (!MapShared()) return -12;

    if (g_shared->magic != kMagic) {
        ZeroMemory(g_shared, sizeof(SharedBlock));
        g_shared->magic = kMagic;
    }

    InterlockedIncrement(&g_shared->countB);
    g_shared->lastOwnerTag = kOwnerTag;

    bool sawA = false;
    const DWORD t0 = GetTickCount();
    while (GetTickCount() - t0 < kOverlapWaitMs) {
        if (g_shared->countA > 0) sawA = true;
        Sleep(10);
    }
    if (sawA) {
        InterlockedDecrement(&g_shared->countB);
        UnmapShared();
        return -2;
    }

    g_shared->stepCount = 0;
    g_shared->lat = params->init_lat;
    g_shared->lon = params->init_lon;
    g_shared->alt = params->init_alt;
    g_shared->speed = params->init_speed;
    g_shared->pitch = params->init_pitch;
    g_shared->yaw = params->init_heading;
    g_shared->roll = params->init_roll;
    g_shared->simTime = 0.0;
    g_shared->dt = params->step_dt > 0 ? params->step_dt : 0.02;
    g_initialized = true;
    return 0;
}

WEAPON_EXPORT int Model_Step(WeaponModelOutput* output) {
    if (!output || !g_initialized || !g_shared) return -1;
    if (g_shared->magic != kMagic) return -98;
    if (g_shared->countA > 0) return -99;
    if (g_shared->lastOwnerTag != 0 && g_shared->lastOwnerTag != kOwnerTag) return -99;

    double dt = g_shared->dt;
    g_shared->simTime += dt;
    g_shared->stepCount++;
    g_shared->lastOwnerTag = kOwnerTag;

    const double pi = 3.14159265358979323846;
    // B：周期性掉头反跑 + 侧向正弦，轨迹与 A 明显不同
    if ((g_shared->stepCount % 28) == 1) {
        g_shared->yaw += 180.0;
    }
    g_shared->yaw += 15.0 * std::cos(g_shared->simTime * 1.5);

    double radPitch = g_shared->pitch * pi / 180.0;
    double radYaw = g_shared->yaw * pi / 180.0;
    double vx = g_shared->speed * std::cos(radPitch) * std::cos(radYaw) * 1.05;
    double vy = g_shared->speed * std::cos(radPitch) * std::sin(radYaw) * 1.05;
    double vz = g_shared->speed * std::sin(radPitch) - 0.6;

    g_shared->lat += (vx * dt) / 111000.0 * 60.0;
    g_shared->lon += (vy * dt) / (111000.0 * std::cos(g_shared->lat * pi / 180.0) + 1e-9) * 60.0;
    g_shared->alt += vz * dt;
    if (g_shared->alt < 0.0) g_shared->alt = 0.0;
    g_shared->pitch -= 0.06 * dt;

    const double out_lat = g_shared->lat;
    const double out_lon = g_shared->lon;

    output->sim_time = g_shared->simTime;
    output->lat = out_lat;
    output->lon = out_lon;
    output->alt = g_shared->alt;
    output->vx = vx;
    output->vy = vy;
    output->vz = vz;
    output->pitch = g_shared->pitch;
    output->roll = g_shared->roll;
    output->yaw = g_shared->yaw;
    output->status = (g_shared->alt <= 0.0) ? 1 : 0;
    return 0;
}

WEAPON_EXPORT void Model_Destroy() {
    if (g_initialized && g_shared) {
        InterlockedDecrement(&g_shared->countB);
        g_initialized = false;
        UnmapShared();
    } else {
        g_initialized = false;
    }
}

WEAPON_EXPORT const char* Model_GetInfo() {
    return "ConflictWeaponB v1.4 — reverse runs; FAIL if A active";
}

} // extern "C"
