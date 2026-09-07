#ifndef TCPASS_A_MODEL_H
#define TCPASS_A_MODEL_H

#ifdef TcPass_A_EXPORTS
#define TcPass_A_API __declspec(dllexport)
#else
#define TcPass_A_API __declspec(dllimport)
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
 * TcPass_A 鈥?all-pass A (clean reference)
 */
TcPass_A_API void* Model_Create(void);
TcPass_A_API int Model_Init(void* handle, const WeaponModelParams* params);
TcPass_A_API int Model_Step(void* handle, WeaponModelOutput* output);
TcPass_A_API void Model_Destroy(void* handle);
TcPass_A_API const char* Model_GetInfo(void);

#ifdef __cplusplus
}
#endif

#endif /* TCPASS_A_MODEL_H */