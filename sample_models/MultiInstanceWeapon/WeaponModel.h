#ifndef WEAPON_MODEL_MULTI_H
#define WEAPON_MODEL_MULTI_H

#ifdef WEAPON_EXPORTS
#define WEAPON_API __declspec(dllexport)
#else
#define WEAPON_API __declspec(dllimport)
#endif

#ifdef __cplusplus
extern "C" {
#endif

#pragma pack(push, 8)
typedef struct WeaponModelParams {
    double init_lat;
    double init_lon;
    double init_alt;
    double init_speed;
    double init_heading;
    double init_pitch;
    double init_roll;
    double step_dt;
} WeaponModelParams;

typedef struct WeaponModelOutput {
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
} WeaponModelOutput;
#pragma pack(pop)

/* Handle-based multi-instance API — one handle per object; threads may own distinct handles */
WEAPON_API void* Model_Create(void);
WEAPON_API int Model_Init(void* handle, const WeaponModelParams* params);
WEAPON_API int Model_Step(void* handle, WeaponModelOutput* output);
WEAPON_API void Model_Destroy(void* handle);
WEAPON_API const char* Model_GetInfo(void);

#ifdef __cplusplus
}
#endif

#endif /* WEAPON_MODEL_MULTI_H */
