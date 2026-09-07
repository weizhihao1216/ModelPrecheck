#ifndef SEHFAULT_B_MODEL_H
#define SEHFAULT_B_MODEL_H

#ifdef SehFault_B_EXPORTS
#define SehFault_B_API __declspec(dllexport)
#else
#define SehFault_B_API __declspec(dllimport)
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
 * SehFault_B — INTENTIONAL: integer divide-by-zero in Model_Init
 */
SehFault_B_API void* Model_Create(void);
SehFault_B_API int Model_Init(void* handle, const WeaponModelParams* params);
SehFault_B_API int Model_Step(void* handle, WeaponModelOutput* output);
SehFault_B_API void Model_Destroy(void* handle);
SehFault_B_API const char* Model_GetInfo(void);

#ifdef __cplusplus
}
#endif

#endif /* SEHFAULT_B_MODEL_H */
