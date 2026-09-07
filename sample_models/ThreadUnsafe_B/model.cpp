#define ThreadUnsafe_B_EXPORTS
#include "WeaponModel.h"
#include <cmath>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// BUG: all instances share one unprotected global state (not thread-safe).
static struct {
    double lat, lon, alt, speed, heading_deg, pitch_deg, roll_deg, dt, sim_time;
    int alive;
    bool inited;
} g_world = {};

extern "C" {

void* Model_Create(void) {
    g_world.alive += 1;
    return &g_world;
}

int Model_Init(void* handle, const WeaponModelParams* params) {
    if (!handle || !params) return -1;
    g_world.lat = params->init_lat;
    g_world.lon = params->init_lon;
    g_world.alt = params->init_alt;
    g_world.speed = params->init_speed;
    g_world.heading_deg = params->init_heading;
    g_world.pitch_deg = params->init_pitch;
    g_world.roll_deg = params->init_roll;
    g_world.dt = (params->step_dt > 0.0) ? params->step_dt : 0.02;
    g_world.sim_time = 0.0;
    g_world.inited = true;
    return 0;
}

int Model_Step(void* handle, WeaponModelOutput* output) {
    if (!handle || !output || !g_world.inited) return -1;
    const double h = g_world.heading_deg * M_PI / 180.0;
    const double p = g_world.pitch_deg * M_PI / 180.0;
    const double vn = g_world.speed * std::cos(p) * std::cos(h);
    const double ve = g_world.speed * std::cos(p) * std::sin(h);
    const double vd = g_world.speed * std::sin(p);
    const double mlat = 111320.0;
    const double mlon = 111320.0 * std::cos(g_world.lat * M_PI / 180.0);
    g_world.lat += (vn * g_world.dt) / mlat;
    g_world.lon += (ve * g_world.dt) / (mlon == 0.0 ? 1.0 : mlon);
    g_world.alt += vd * g_world.dt;
    g_world.sim_time += g_world.dt;
    std::memset(output, 0, sizeof(*output));
    output->sim_time = g_world.sim_time;
    output->lat = g_world.lat;
    output->lon = g_world.lon;
    output->alt = g_world.alt;
    output->vx = vn; output->vy = ve; output->vz = vd;
    output->pitch = g_world.pitch_deg; output->roll = g_world.roll_deg; output->yaw = g_world.heading_deg;
    output->status = 0;
    return 0;
}

void Model_Destroy(void* handle) {
    (void)handle;
    if (g_world.alive > 0) g_world.alive -= 1;
    if (g_world.alive <= 0) {
        g_world.alive = 0;
        g_world.inited = false;
    }
}

const char* Model_GetInfo(void) {
    return "ThreadUnsafe_B | INTENTIONAL BUG: shared global state, not thread-safe";
}

}