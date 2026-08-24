#include <cmath>
#include <cstdlib>
#include <cstring>

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

static int g_stepCount = 0;

extern "C" {

WEAPON_EXPORT int Model_Init(const WeaponModelParams* params) {
    g_stepCount = 0;
    return 0;
}

WEAPON_EXPORT int Model_Step(WeaponModelOutput* output) {
    g_stepCount++;

    // Intentional memory leak demo
    char* leak = new char[1024 * 64]; // Allocate 64KB memory every step without deleting!
    std::memset(leak, 0xAB, 1024 * 64);

    // Intentional crash (Access Violation / Null Pointer Dereference) after 500 steps
    if (g_stepCount == 500) {
        int* crashPtr = nullptr;
        *crashPtr = 42; // Trigger Access Violation 0xC0000005!
    }

    if (output) {
        output->sim_time = g_stepCount * 0.02;
        output->lat = 39.9 + g_stepCount * 0.0001;
        output->lon = 116.4 + g_stepCount * 0.0001;
        output->alt = 5000.0 - g_stepCount * 2.0;
        output->vx = 500.0;
        output->vy = 0.0;
        output->vz = -10.0;
        output->pitch = 10.0;
        output->roll = 0.0;
        output->yaw = 45.0;
        output->status = 0;
    }

    return 0;
}

WEAPON_EXPORT void Model_Destroy() {
}

}
