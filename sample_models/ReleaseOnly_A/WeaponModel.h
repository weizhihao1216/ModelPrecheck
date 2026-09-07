#ifndef RELEASEONLY_A_MODEL_H
#define RELEASEONLY_A_MODEL_H

#ifdef ReleaseOnly_A_EXPORTS
#define ReleaseOnly_A_API __declspec(dllexport)
#else
#define ReleaseOnly_A_API __declspec(dllimport)
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

/*
 * ReleaseOnly_A 鈥?Release-only package A (no Debug lib/dll shipped)
 */
ReleaseOnly_A_API void* Model_Create(void);
ReleaseOnly_A_API int Model_Init(void* handle, const WeaponModelParams* params);
ReleaseOnly_A_API int Model_Step(void* handle, WeaponModelOutput* output);
ReleaseOnly_A_API void Model_Destroy(void* handle);
ReleaseOnly_A_API const char* Model_GetInfo(void);

#ifdef __cplusplus
}
#endif

#endif /* RELEASEONLY_A_MODEL_H */