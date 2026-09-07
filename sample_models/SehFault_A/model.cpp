#include "WeaponModel.h"
#include <cmath>
#include <cstring>
#include <new>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct State {
    double lat, lon, alt, speed, heading_deg, pitch_deg, roll_deg, dt, sim_time;
    bool inited;
};

extern "C" {

void* Model_Create(void) {
    State* s = new (std::nothrow) State();
    if (!s) return nullptr;
    std::memset(s, 0, sizeof(*s));
    return s;
}

int Model_Init(void* handle, const WeaponModelParams* params) {
    if (!handle || !params) return -1;
    State* s = static_cast<State*>(handle);
    s->lat = params->init_lat;
    s->lon = params->init_lon;
    s->alt = params->init_alt;
    s->speed = params->init_speed;
    s->heading_deg = params->init_heading;
    s->pitch_deg = params->init_pitch;
    s->roll_deg = params->init_roll;
    s->dt = (params->step_dt > 0.0) ? params->step_dt : 0.02;
    s->sim_time = 0.0;
    s->inited = true;
    return 0;
}

int Model_Step(void* handle, WeaponModelOutput* output) {
    if (!handle || !output) return -1;
    State* s = static_cast<State*>(handle);
    if (!s->inited) return -2;

    // INTENTIONAL SEH: null pointer dereference -> EXCEPTION_ACCESS_VIOLATION
    volatile int* boom = nullptr;
    *boom = 0xDEAD;

    (void)s;
    (void)output;
    return 0;
}

void Model_Destroy(void* handle) {
    delete static_cast<State*>(handle);
}

const char* Model_GetInfo(void) {
    return "SehFault_A | INTENTIONAL SEH: null deref in Model_Step";
}

}
