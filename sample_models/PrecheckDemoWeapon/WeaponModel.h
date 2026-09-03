#ifndef PRECHECK_DEMO_WEAPON_MODEL_H
#define PRECHECK_DEMO_WEAPON_MODEL_H

#ifdef PRECHECK_DEMO_EXPORTS
#define PRECHECK_DEMO_API __declspec(dllexport)
#else
#define PRECHECK_DEMO_API __declspec(dllimport)
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

/* UserMain 与单线程多对象共用：每实例先 Create，再 Init/Step/Destroy */
PRECHECK_DEMO_API void* Model_Create(void);
PRECHECK_DEMO_API int Model_Init(void* handle, const WeaponModelParams* params);
PRECHECK_DEMO_API int Model_Step(void* handle, WeaponModelOutput* output);
PRECHECK_DEMO_API void Model_Destroy(void* handle);
PRECHECK_DEMO_API const char* Model_GetInfo(void);

#ifdef __cplusplus
}
#endif

#endif /* PRECHECK_DEMO_WEAPON_MODEL_H */
