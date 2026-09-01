#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <cmath>
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
    double baseYaw = 0.0;
    int stepIndex = 0;
    bool initialized = false;
};

static thread_local ThreadState tls;

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
    tls.baseYaw = params->init_heading + 8.0;
    tls.yaw = tls.baseYaw;
    tls.roll = params->init_roll;
    tls.stepIndex = 0;
    tls.initialized = true;
    return 0;
}

WEAPON_EXPORT int Model_Step(WeaponModelOutput* output) {
    if (!output || !tls.initialized) return -1;

    const double dt = tls.params.step_dt > 0 ? tls.params.step_dt : 0.02;
    tls.simTime += dt;
    ++tls.stepIndex;

    const double pi = 3.14159265358979323846;
    // D：前半段弧线前进，后半段掉头反方向跑，并带侧向摆动
    const double reverse = (tls.stepIndex >= 50) ? 180.0 : 0.0;
    const double sweep = (tls.stepIndex < 50)
        ? (tls.stepIndex * 1.6)   // 逐渐左转
        : ((tls.stepIndex - 50) * -1.8);
    tls.yaw = tls.baseYaw + reverse + sweep + 12.0 * std::sin(tls.simTime * 2.0);
    tls.pitch = tls.params.init_pitch + 3.0 * std::cos(tls.simTime * 1.1);

    const double radPitch = tls.pitch * pi / 180.0;
    const double radYaw = tls.yaw * pi / 180.0;
    const double groundScale = 65.0;
    const double vx = tls.speed * std::cos(radPitch) * std::cos(radYaw) * 1.02 * groundScale;
    const double vy = tls.speed * std::cos(radPitch) * std::sin(radYaw) * 1.02 * groundScale;
    const double vz = tls.speed * std::sin(radPitch) - 0.5;

    tls.lat += (vx * dt) / 111000.0;
    tls.lon += (vy * dt) / (111000.0 * std::cos(tls.lat * pi / 180.0) + 1e-9);
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
    return "CompatibleWeaponD v1.2 — arc then reverse lat/lon (out_lat/out_lon)";
}

} // extern "C"
