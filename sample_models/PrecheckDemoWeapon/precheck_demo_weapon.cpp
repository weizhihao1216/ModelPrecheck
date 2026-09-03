#include "WeaponModel.h"
#include <cmath>
#include <cstring>
#include <new>

namespace {

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
    int instanceId;
};

void advanceState(InstanceState* state) {
    double dt = state->params.step_dt > 0 ? state->params.step_dt : 0.02;
    state->currentTime += dt;

    const double pi = 3.14159265358979323846;
    const double phase = state->instanceId * 0.37;
    const double headingBias = std::sin(state->currentTime * 0.15 + phase) * 12.0;
    state->curYaw += headingBias * dt;

    double radPitch = state->curPitch * pi / 180.0;
    double radYaw = state->curYaw * pi / 180.0;
    double vx = state->curSpeed * std::cos(radPitch) * std::cos(radYaw);
    double vy = state->curSpeed * std::cos(radPitch) * std::sin(radYaw);
    double vz = state->curSpeed * std::sin(radPitch) - 9.8 * state->currentTime * 0.08;

    state->curLat += (vx * dt) / 111000.0;
    state->curLon += (vy * dt) / (111000.0 * std::cos(state->curLat * pi / 180.0));
    state->curAlt += vz * dt;
    if (state->curAlt < 0.0) state->curAlt = 0.0;
    state->curPitch -= 0.04 * dt;
}

void fillOutput(const InstanceState* state, WeaponModelOutput* output) {
    const double pi = 3.14159265358979323846;
    double radPitch = state->curPitch * pi / 180.0;
    double radYaw = state->curYaw * pi / 180.0;
    double vx = state->curSpeed * std::cos(radPitch) * std::cos(radYaw);
    double vy = state->curSpeed * std::cos(radPitch) * std::sin(radYaw);
    double vz = state->curSpeed * std::sin(radPitch) - 9.8 * state->currentTime * 0.08;

    output->sim_time = state->currentTime;
    output->lat = state->curLat;
    output->lon = state->curLon;
    output->alt = state->curAlt;
    output->vx = vx;
    output->vy = vy;
    output->vz = vz;
    output->pitch = state->curPitch;
    output->roll = state->curRoll;
    output->yaw = state->curYaw;
    output->status = (state->curAlt <= 0.0) ? 1 : 0;
}

} // namespace

extern "C" {

PRECHECK_DEMO_API void* Model_Create(void) {
    InstanceState* state = new (std::nothrow) InstanceState();
    if (!state) return nullptr;
    std::memset(state, 0, sizeof(InstanceState));
    return state;
}

PRECHECK_DEMO_API int Model_Init(void* handle, const WeaponModelParams* params) {
    if (!handle || !params) return -1;
    InstanceState* state = static_cast<InstanceState*>(handle);
    state->params = *params;
    state->currentTime = 0.0;
    state->curLat = params->init_lat;
    state->curLon = params->init_lon;
    state->curAlt = params->init_alt;
    state->curSpeed = params->init_speed;
    state->curPitch = params->init_pitch;
    state->curYaw = params->init_heading;
    state->curRoll = params->init_roll;
    state->instanceId = static_cast<int>(params->init_heading) % 16;
    state->initialized = true;
    return 0;
}

PRECHECK_DEMO_API int Model_Step(void* handle, WeaponModelOutput* output) {
    if (!handle || !output) return -1;
    InstanceState* state = static_cast<InstanceState*>(handle);
    if (!state->initialized) return -1;
    advanceState(state);
    fillOutput(state, output);
    return 0;
}

PRECHECK_DEMO_API void Model_Destroy(void* handle) {
    delete static_cast<InstanceState*>(handle);
}

PRECHECK_DEMO_API const char* Model_GetInfo(void) {
    return "PrecheckDemoWeapon v1.0 — UserMain + multi-object demo DLL";
}

} // extern "C"
