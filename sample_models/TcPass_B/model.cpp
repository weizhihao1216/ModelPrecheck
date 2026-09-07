#define TcPass_B_EXPORTS
#include "WeaponModel.h"
#include <cmath>
#include <cstring>
#include <mutex>
#include <new>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {
struct State {
    double lat, lon, alt;
    double speed, heading_deg, pitch_deg, roll_deg;
    double dt, sim_time;
    bool inited;
};

std::mutex g_mu;
std::vector<State*> g_live;

void Integrate(State* s, WeaponModelOutput* o) {
    const double h = s->heading_deg * M_PI / 180.0;
    const double p = s->pitch_deg * M_PI / 180.0;
    const double vn = s->speed * std::cos(p) * std::cos(h);
    const double ve = s->speed * std::cos(p) * std::sin(h);
    const double vd = s->speed * std::sin(p);
    const double meters_per_deg_lat = 111320.0;
    const double meters_per_deg_lon = 111320.0 * std::cos(s->lat * M_PI / 180.0);
    s->lat += (vn * s->dt) / meters_per_deg_lat;
    s->lon += (ve * s->dt) / (meters_per_deg_lon == 0.0 ? 1.0 : meters_per_deg_lon);
    s->alt += vd * s->dt;
    s->sim_time += s->dt;
    if (o) {
        std::memset(o, 0, sizeof(*o));
        o->sim_time = s->sim_time;
        o->lat = s->lat;
        o->lon = s->lon;
        o->alt = s->alt;
        o->vx = vn; o->vy = ve; o->vz = vd;
        o->pitch = s->pitch_deg; o->roll = s->roll_deg; o->yaw = s->heading_deg;
        o->status = 0;
    }
}
} // namespace

extern "C" {

void* Model_Create(void) {
    try {
        State* s = new State();
        std::memset(s, 0, sizeof(*s));
        std::lock_guard<std::mutex> lk(g_mu);
        g_live.push_back(s);
        return s;
    } catch (...) { return nullptr; }
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
    Integrate(s, output);
    return 0;
}

void Model_Destroy(void* handle) {
    if (!handle) return;
    State* s = static_cast<State*>(handle);
    {
        std::lock_guard<std::mutex> lk(g_mu);
        for (size_t i = 0; i < g_live.size(); ++i) {
            if (g_live[i] == s) { g_live.erase(g_live.begin() + static_cast<std::ptrdiff_t>(i)); break; }
        }
    }
    delete s;
}

const char* Model_GetInfo(void) {
    return "TcPass_B | thread-safe multi-instance | matching CRT | no shared kernel";
}

} // extern "C"