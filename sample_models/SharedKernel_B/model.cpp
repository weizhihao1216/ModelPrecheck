#define SharedKernel_B_EXPORTS
#include "WeaponModel.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cmath>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#pragma pack(push, 8)
struct SharedHub {
    double poison[32];
    struct Slot {
        int used;
        double lat, lon, alt, speed, heading_deg, pitch_deg, roll_deg, dt, sim_time;
    } slots[2];
};
#pragma pack(pop)

static SharedHub* g_hub = nullptr;
static HANDLE g_map = nullptr;
static int g_local_seq = 0;

static SharedHub* EnsureHub() {
    if (g_hub) return g_hub;
    const wchar_t* kName = L"Local\\ModelPrecheck_SharedKernel_v1";
    g_map = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, kName);
    if (!g_map) {
        g_map = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
                                   0, sizeof(SharedHub), kName);
    }
    if (!g_map) return nullptr;
    g_hub = static_cast<SharedHub*>(MapViewOfFile(g_map, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedHub)));
    return g_hub;
}

extern "C" {

void* Model_Create(void) {
    SharedHub* hub = EnsureHub();
    if (!hub) return nullptr;
    const int idx = g_local_seq++;
    SharedHub::Slot* slot = &hub->slots[idx];
    std::memset(slot, 0, sizeof(*slot));
    slot->used = 1;
    return slot;
}

int Model_Init(void* handle, const WeaponModelParams* params) {
    if (!handle || !params) return -1;
    SharedHub::Slot* s = static_cast<SharedHub::Slot*>(handle);
    s->lat = params->init_lat;
    s->lon = params->init_lon;
    s->alt = params->init_alt;
    s->speed = params->init_speed;
    s->heading_deg = params->init_heading;
    s->pitch_deg = params->init_pitch;
    s->roll_deg = params->init_roll;
    s->dt = (params->step_dt > 0.0) ? params->step_dt : 0.02;
    s->sim_time = 0.0;
    s->used = 1;
    return 0;
}

int Model_Step(void* handle, WeaponModelOutput* output) {
    if (!handle || !output) return -1;
    SharedHub::Slot* s = static_cast<SharedHub::Slot*>(handle);
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
    if (g_hub) {
        for (int i = 0; i < 32; ++i) g_hub->poison[i] = s->lat + i;
    }
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
    SharedHub::Slot* s = static_cast<SharedHub::Slot*>(handle);
    s->used = 0;
    if (g_local_seq > 0) g_local_seq -= 1;
}

const char* Model_GetInfo(void) {
    return "SharedKernel_B | INTENTIONAL BUG: shared named section + OOB slot write";
}

}