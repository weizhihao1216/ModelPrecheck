#include "WeaponModel.h"
#include <cmath>
#include <cstring>
#include <new>

struct InstanceState {
    WeaponModelParams params;
    double currentTime;
    double curLat;
    double curLon;
    double curAlt;
    double curSpeed;
    double curPitch;
    double curYaw;
    double curRoll;
    bool initialized;
};

extern "C" {

WEAPON_API void* WPN_AllocInstance(void) {
    InstanceState* s = new (std::nothrow) InstanceState();
    if (!s) return nullptr;
    std::memset(s, 0, sizeof(InstanceState));
    return s;
}

WEAPON_API int WPN_BootModel(void* handle, const WeaponModelParams* params) {
    if (!handle || !params) return -1;
    InstanceState* s = static_cast<InstanceState*>(handle);
    s->params = *params;
    s->currentTime = 0.0;
    s->curLat = params->init_lat;
    s->curLon = params->init_lon;
    s->curAlt = params->init_alt;
    s->curSpeed = params->init_speed;
    s->curPitch = params->init_pitch;
    s->curYaw = params->init_heading;
    s->curRoll = params->init_roll;
    s->initialized = true;
    return 0;
}

WEAPON_API int WPN_AdvanceModel(void* handle, WeaponModelOutput* output) {
    if (!handle || !output) return -1;
    InstanceState* s = static_cast<InstanceState*>(handle);
    if (!s->initialized) return -1;

    double dt = s->params.step_dt > 0 ? s->params.step_dt : 0.02;
    s->currentTime += dt;

    const double pi = 3.14159265358979323846;
    double radPitch = s->curPitch * pi / 180.0;
    double radYaw = s->curYaw * pi / 180.0;

    double vx = s->curSpeed * std::cos(radPitch) * std::cos(radYaw);
    double vy = s->curSpeed * std::cos(radPitch) * std::sin(radYaw);
    double vz = s->curSpeed * std::sin(radPitch) - 9.8 * s->currentTime * 0.1;

    s->curLat += (vx * dt) / 111000.0;
    s->curLon += (vy * dt) / (111000.0 * std::cos(s->curLat * pi / 180.0));
    s->curAlt += vz * dt;
    if (s->curAlt < 0.0) s->curAlt = 0.0;
    s->curPitch -= 0.05 * dt;

    output->sim_time = s->currentTime;
    output->lat = s->curLat;
    output->lon = s->curLon;
    output->alt = s->curAlt;
    output->vx = vx;
    output->vy = vy;
    output->vz = vz;
    output->pitch = s->curPitch;
    output->roll = s->curRoll;
    output->yaw = s->curYaw;
    output->status = (s->curAlt <= 0.0) ? 1 : 0;
    return 0;
}

WEAPON_API void WPN_ReleaseInstance(void* handle) {
    delete static_cast<InstanceState*>(handle);
}

WEAPON_API const char* WPN_GetInfo(void) {
    return "RenamedInterfaceWeapon v1.0";
}

} // extern "C"
