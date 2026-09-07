#ifndef SHAREDKERNEL_B_MODEL_H
#define SHAREDKERNEL_B_MODEL_H

#ifdef SharedKernel_B_EXPORTS
#define SharedKernel_B_API __declspec(dllexport)
#else
#define SharedKernel_B_API __declspec(dllimport)
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
 * SharedKernel_B 鈥?Shared global/singleton env B
 */
SharedKernel_B_API void* Model_Create(void);
SharedKernel_B_API int Model_Init(void* handle, const WeaponModelParams* params);
SharedKernel_B_API int Model_Step(void* handle, WeaponModelOutput* output);
SharedKernel_B_API void Model_Destroy(void* handle);
SharedKernel_B_API const char* Model_GetInfo(void);

#ifdef __cplusplus
}
#endif

#endif /* SHAREDKERNEL_B_MODEL_H */