#ifndef HEADERCLASH_A_MODEL_H
#define HEADERCLASH_A_MODEL_H

#ifdef HeaderClash_A_EXPORTS
#define HeaderClash_A_API __declspec(dllexport)
#else
#define HeaderClash_A_API __declspec(dllimport)
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

typedef struct VendorConfig {
    double scale;
    int mode;
} VendorConfig;
#pragma pack(pop)

/*
 * HeaderClash_A 鈥?Header clash pair A (standard layout)
 */
HeaderClash_A_API void* Model_Create(void);
HeaderClash_A_API int Model_Init(void* handle, const WeaponModelParams* params);
HeaderClash_A_API int Model_Step(void* handle, WeaponModelOutput* output);
HeaderClash_A_API void Model_Destroy(void* handle);
HeaderClash_A_API const char* Model_GetInfo(void);

#ifdef __cplusplus
}
#endif

#endif /* HEADERCLASH_A_MODEL_H */