#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <cmath>
#include <cstdint>
#include <cstring>

#define WEAPON_EXPORT __declspec(dllexport)

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

struct ThreadState {
    WeaponModelParams params{};
    double simTime = 0.0;
    double lat = 0.0, lon = 0.0, alt = 0.0, speed = 0.0;
    double pitch = 0.0, yaw = 0.0, roll = 0.0;
    uint32_t rng = 1;
    int stepIndex = 0;
    bool initialized = false;
};

static thread_local ThreadState tls;

static uint32_t NextRand(uint32_t& s) {
    s = s * 1664525u + 1013904223u;
    return s;
}

static double RandUnit(uint32_t& s) {
    return (NextRand(s) & 0xFFFFFFu) / 16777215.0; // [0,1]
}

extern "C" {

WEAPON_EXPORT int Model_Init(const WeaponModelParams* params) {
    if (!params) return -1;
    tls.params = *params;
    tls.simTime = 0.0;
    tls.lat = params->init_lat;
    tls.lon = params->init_lon;
    tls.alt = params->init_alt;
    tls.speed = params->init_speed > 1.0 ? params->init_speed : 200.0;
    tls.pitch = params->init_pitch;
    tls.yaw = params->init_heading;
    tls.roll = params->init_roll;
    tls.stepIndex = 0;
    // Seed from init pose so不同随机参数 → 不同轨迹
    const uint32_t a = static_cast<uint32_t>(std::fabs(params->init_lat) * 1e6);
    const uint32_t b = static_cast<uint32_t>(std::fabs(params->init_lon) * 1e6);
    const uint32_t c = static_cast<uint32_t>(std::fabs(params->init_alt));
    tls.rng = a ^ (b << 3) ^ (c << 7) ^ 0xA5A5E001u;
    if (tls.rng == 0) tls.rng = 1;
    tls.initialized = true;
    return 0;
}

WEAPON_EXPORT int Model_Step(WeaponModelOutput* output) {
    if (!output || !tls.initialized) return -1;

    const double dt = tls.params.step_dt > 0 ? tls.params.step_dt : 0.02;
    tls.simTime += dt;
    ++tls.stepIndex;

    const double pi = 3.14159265358979323846;

    // 随机扰动 + 周期性大角度掉头，避免单向直线
    const double dYaw = (RandUnit(tls.rng) - 0.5) * 30.0;
    const double dPitch = (RandUnit(tls.rng) - 0.5) * 6.0;
    tls.yaw += dYaw;
    tls.pitch += dPitch;
    if (tls.pitch > 40.0) tls.pitch = 40.0;
    if (tls.pitch < -5.0) tls.pitch = -5.0;

    if ((tls.stepIndex % 18) == 0) {
        tls.yaw += 180.0; // 反方向跑一段
    } else if ((tls.stepIndex % 9) == 0) {
        tls.yaw += (RandUnit(tls.rng) > 0.5) ? 90.0 : -90.0;
    }

    const double radPitch = tls.pitch * pi / 180.0;
    const double radYaw = tls.yaw * pi / 180.0;

    const double groundScale = 80.0;
    const double vx = tls.speed * std::cos(radPitch) * std::cos(radYaw) * groundScale;
    const double vy = tls.speed * std::cos(radPitch) * std::sin(radYaw) * groundScale;
    const double vz = tls.speed * std::sin(radPitch) - 0.5
                    + (RandUnit(tls.rng) - 0.5) * 2.0;

    const double jitterLat = (RandUnit(tls.rng) - 0.5) * 0.0015;
    const double jitterLon = (RandUnit(tls.rng) - 0.5) * 0.0015;

    tls.lat += (vx * dt) / 111000.0 + jitterLat;
    tls.lon += (vy * dt) / (111000.0 * std::cos(tls.lat * pi / 180.0) + 1e-9) + jitterLon;
    tls.alt += vz * dt;
    if (tls.alt < 0.0) tls.alt = 0.0;

    const double out_lat = tls.lat;
    const double out_lon = tls.lon;

    output->sim_time = tls.simTime;
    output->lat = out_lat;
    output->lon = out_lon;
    output->alt = tls.alt;
    output->vx = vx;
    output->vy = vy;
    output->vz = vz;
    output->pitch = tls.pitch;
    output->roll = tls.roll;
    output->yaw = tls.yaw;
    output->status = (tls.alt <= 0.0) ? 1 : 0;
    return 0;
}

WEAPON_EXPORT void Model_Destroy() {
    tls.initialized = false;
    std::memset(&tls.params, 0, sizeof(tls.params));
}

WEAPON_EXPORT const char* Model_GetInfo() {
    return "CompatibleWeaponE v1.2 — zig-zag / reverse lat/lon walk";
}

} // extern "C"
