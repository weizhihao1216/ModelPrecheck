#ifndef WEAPON_MODEL_RENAMED_H
#define WEAPON_MODEL_RENAMED_H

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

/* Renamed lifecycle API — requires interface mapping wizard */
WEAPON_API void* WPN_AllocInstance(void);
WEAPON_API int WPN_BootModel(void* handle, const WeaponModelParams* params);
WEAPON_API int WPN_AdvanceModel(void* handle, WeaponModelOutput* output);
WEAPON_API void WPN_ReleaseInstance(void* handle);
WEAPON_API const char* WPN_GetInfo(void);

#ifdef __cplusplus
}
#endif

#endif /* WEAPON_MODEL_RENAMED_H */
