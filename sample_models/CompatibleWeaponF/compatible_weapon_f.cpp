#include "WeaponModel.h"
#include <cmath>
#include <cstring>
#include <new>

namespace {

struct InstanceState {
    WeaponModelParams params{};
    double currentTime = 0.0;
    double curLat = 0.0;
    double curLon = 0.0;
    double curAlt = 0.0;
    double curSpeed = 0.0;
    double curPitch = 0.0;
    double curYaw = 0.0;
    double curRoll = 0.0;
    bool initialized = false;
    int instanceId = 0;
};

void advanceState(InstanceState* state) {
    const double dt = state->params.step_dt > 0 ? state->params.step_dt : 0.02;
    state->currentTime += dt;

    const double pi = 3.14159265358979323846;
    // F：右弯（偏航递增）+ 轻微振荡，便于与 G 区分轨迹
    const double phase = state->instanceId * 0.41;
    state->curYaw += (18.0 + 6.0 * std::sin(state->currentTime * 0.9 + phase)) * dt;
    state->curPitch = state->params.init_pitch
        + 3.0 * std::sin(state->currentTime * 0.7 + phase);

    const double radPitch = state->curPitch * pi / 180.0;
    const double radYaw = state->curYaw * pi / 180.0;
    const double groundScale = 55.0;
    const double vx = state->curSpeed * std::cos(radPitch) * std::cos(radYaw) * groundScale;
    const double vy = state->curSpeed * std::cos(radPitch) * std::sin(radYaw) * groundScale;
    const double vz = state->curSpeed * std::sin(radPitch) - 0.35;

    state->curLat += (vx * dt) / 111000.0;
    state->curLon += (vy * dt) / (111000.0 * std::cos(state->curLat * pi / 180.0) + 1e-9);
    state->curAlt += vz * dt;
    if (state->curAlt < 0.0) state->curAlt = 0.0;
}

void fillOutput(const InstanceState* state, WeaponModelOutput* output) {
    const double pi = 3.14159265358979323846;
    const double radPitch = state->curPitch * pi / 180.0;
    const double radYaw = state->curYaw * pi / 180.0;
    const double groundScale = 55.0;
    output->sim_time = state->currentTime;
    output->lat = state->curLat;
    output->lon = state->curLon;
    output->alt = state->curAlt;
    output->vx = state->curSpeed * std::cos(radPitch) * std::cos(radYaw) * groundScale;
    output->vy = state->curSpeed * std::cos(radPitch) * std::sin(radYaw) * groundScale;
    output->vz = state->curSpeed * std::sin(radPitch) - 0.35;
    output->pitch = state->curPitch;
    output->roll = state->curRoll;
    output->yaw = state->curYaw;
    output->status = (state->curAlt <= 0.0) ? 1 : 0;
}

} // namespace

extern "C" {

COMPATIBLE_WEAPON_F_API void* Model_Create(void) {
    InstanceState* state = new (std::nothrow) InstanceState();
    if (!state) return nullptr;
    std::memset(state, 0, sizeof(InstanceState));
    return state;
}

COMPATIBLE_WEAPON_F_API int Model_Init(void* handle, const WeaponModelParams* params) {
    if (!handle || !params) return -1;
    InstanceState* state = static_cast<InstanceState*>(handle);
    state->params = *params;
    state->currentTime = 0.0;
    state->curLat = params->init_lat;
    state->curLon = params->init_lon;
    state->curAlt = params->init_alt;
    state->curSpeed = params->init_speed > 1.0 ? params->init_speed : 250.0;
    state->curPitch = params->init_pitch;
    state->curYaw = params->init_heading;
    state->curRoll = params->init_roll;
    state->instanceId = static_cast<int>(std::fabs(params->init_heading)) % 16;
    state->initialized = true;
    return 0;
}

COMPATIBLE_WEAPON_F_API int Model_Step(void* handle, WeaponModelOutput* output) {
    if (!handle || !output) return -1;
    InstanceState* state = static_cast<InstanceState*>(handle);
    if (!state->initialized) return -1;
    advanceState(state);
    fillOutput(state, output);
    return 0;
}

COMPATIBLE_WEAPON_F_API void Model_Destroy(void* handle) {
    delete static_cast<InstanceState*>(handle);
}

COMPATIBLE_WEAPON_F_API const char* Model_GetInfo(void) {
    return "CompatibleWeaponF v1.0 — fleet cross-model sample F (right-turn)";
}

} // extern "C"
