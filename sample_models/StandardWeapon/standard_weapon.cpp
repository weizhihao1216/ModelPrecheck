#include <cmath>
#include <cstring>
#include <iostream>

#define WEAPON_EXPORT __declspec(dllexport)

#pragma pack(push, 8)
struct WeaponModelParams {
    double init_lat;
    double init_lon;
    double init_alt;
    double init_speed;
    double init_heading;
    double init_pitch;
    double init_roll;
    double step_dt;
};

struct WeaponModelOutput {
    double sim_time;
    double lat;
    double lon;
    double alt;
    double vx;
    double vy;
    double vz;
    double pitch;
    double roll;
    double yaw;
    int status;
};
#pragma pack(pop)

// Internal state of sample weapon model
static WeaponModelParams g_params;
static double g_currentTime = 0.0;
static double g_curLat = 0.0;
static double g_curLon = 0.0;
static double g_curAlt = 0.0;
static double g_curSpeed = 0.0;
static double g_curPitch = 0.0;
static double g_curYaw = 0.0;
static double g_curRoll = 0.0;
static bool g_initialized = false;

extern "C" {

WEAPON_EXPORT int Model_Init(const WeaponModelParams* params) {
    if (!params) return -1;
    g_params = *params;
    g_currentTime = 0.0;
    g_curLat = params->init_lat;
    g_curLon = params->init_lon;
    g_curAlt = params->init_alt;
    g_curSpeed = params->init_speed;
    g_curPitch = params->init_pitch;
    g_curYaw = params->init_heading;
    g_curRoll = params->init_roll;
    g_initialized = true;
    return 0;
}

WEAPON_EXPORT int Model_Step(WeaponModelOutput* output) {
    if (!output || !g_initialized) return -1;

    double dt = g_params.step_dt > 0 ? g_params.step_dt : 0.02;
    g_currentTime += dt;

    // Simulate ballistic flight dynamics
    double radPitch = g_curPitch * 3.14159265358979323846 / 180.0;
    double radYaw = g_curYaw * 3.14159265358979323846 / 180.0;

    double vx = g_curSpeed * std::cos(radPitch) * std::cos(radYaw);
    double vy = g_curSpeed * std::cos(radPitch) * std::sin(radYaw);
    double vz = g_curSpeed * std::sin(radPitch) - 9.8 * g_currentTime * 0.1;

    // Kinematic updates
    g_curLat += (vx * dt) / 111000.0;
    g_curLon += (vy * dt) / (111000.0 * std::cos(g_curLat * 3.14159265 / 180.0));
    g_curAlt += vz * dt;

    if (g_curAlt < 0.0) {
        g_curAlt = 0.0;
    }

    g_curPitch -= 0.05 * dt; // Gravity arc pitch drop

    output->sim_time = g_currentTime;
    output->lat = g_curLat;
    output->lon = g_curLon;
    output->alt = g_curAlt;
    output->vx = vx;
    output->vy = vy;
    output->vz = vz;
    output->pitch = g_curPitch;
    output->roll = g_curRoll;
    output->yaw = g_curYaw;
    output->status = (g_curAlt <= 0.0) ? 1 : 0;

    return 0;
}

WEAPON_EXPORT void Model_Destroy() {
    g_initialized = false;
}

WEAPON_EXPORT const char* Model_GetInfo() {
    return "Standard Compliant Weapon Model DLL v1.0 [extern C]";
}

} // extern "C"
