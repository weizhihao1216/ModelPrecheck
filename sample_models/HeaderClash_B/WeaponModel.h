#ifndef HEADERCLASH_B_MODEL_H
#define HEADERCLASH_B_MODEL_H

#ifdef HeaderClash_B_EXPORTS
#define HeaderClash_B_API __declspec(dllexport)
#else
#define HeaderClash_B_API __declspec(dllimport)
#endif

#ifdef __cplusplus
extern "C" {
#endif

#pragma pack(push, 8)
typedef struct WeaponModelParams {
    double init_speed;
    double init_heading;
    double init_pitch;
    double init_roll;
    double step_dt;
    double init_lat;
    double init_lon;
    double init_alt;
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
    int mode;
    char tag[32];
    double scale;
} VendorConfig;
#pragma pack(pop)

/*
 * HeaderClash_B 鈥?Header clash pair B (REORDERED params + different VendorConfig)
 */
HeaderClash_B_API void* Model_Create(void);
HeaderClash_B_API int Model_Init(void* handle, const WeaponModelParams* params);
HeaderClash_B_API int Model_Step(void* handle, WeaponModelOutput* output);
HeaderClash_B_API void Model_Destroy(void* handle);
HeaderClash_B_API const char* Model_GetInfo(void);

#ifdef __cplusplus
}
#endif

#endif /* HEADERCLASH_B_MODEL_H */