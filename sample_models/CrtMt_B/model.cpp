#define CrtMt_B_EXPORTS
#include "WeaponModel.h"
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <new>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct State {
    double lat, lon, alt, speed, heading_deg, pitch_deg, roll_deg, dt, sim_time;
    bool inited;
    char* path_buf;
};

extern "C" {

void* Model_Create(void) {
    State* s = new (std::nothrow) State();
    if (!s) return nullptr;
    std::memset(s, 0, sizeof(*s));
    s->path_buf = static_cast<char*>(std::malloc(256));
    if (s->path_buf) {
        std::memset(s->path_buf, 0, 256);
        std::memcpy(s->path_buf, "CrtMt_B_heap", 12);
    }
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
    if (s->path_buf) {
        std::memcpy(s->path_buf, "init_ok", 8);
    }
    return 0;
}

int Model_Step(void* handle, WeaponModelOutput* output) {
    if (!handle || !output) return -1;
    State* s = static_cast<State*>(handle);
    if (!s->inited) return -2;
    const double h = s->heading_deg * M_PI / 180.0;
    const double p = s->pitch_deg * M_PI / 180.0;
    const double vn = s->speed * std::cos(p) * std::cos(h);
    const double ve = s->speed * std::cos(p) * std::sin(h);
    const double vd = s->speed * std::sin(p);
    const double mlat = 111320.0;
    const double mlon = 111320.0 * std::cos(s->lat * M_PI / 180.0);
    s->lat += (vn * s->dt) / mlat;
    s->lon += (ve * s->dt) / (mlon == 0.0 ? 1.0 : mlon);
    s->alt += vd * s->dt;
    s->sim_time += s->dt;
    std::memset(output, 0, sizeof(*output));
    output->sim_time = s->sim_time;
    output->lat = s->lat; output->lon = s->lon; output->alt = s->alt;
    output->vx = vn; output->vy = ve; output->vz = vd;
    output->pitch = s->pitch_deg; output->roll = s->roll_deg; output->yaw = s->heading_deg;
    output->status = 0;
    return 0;
}

void Model_Destroy(void* handle) {
    if (!handle) return;
    State* s = static_cast<State*>(handle);
    if (s->path_buf) {
        std::free(s->path_buf);
        s->path_buf = nullptr;
    }
    delete s;
}

const char* Model_GetInfo(void) {
    return "CrtMt_B | CRT demo (MT) | heap alloc inside DLL";
}

}