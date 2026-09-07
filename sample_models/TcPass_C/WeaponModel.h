#ifndef TCPASS_C_MODEL_H
#define TCPASS_C_MODEL_H

#ifdef TcPass_C_EXPORTS
#define TcPass_C_API __declspec(dllexport)
#else
#define TcPass_C_API __declspec(dllimport)
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Namespace / unique-type isolation vs TcPass_A:
 * Does NOT define global WeaponModelParams / WeaponModelOutput.
 * Layout is ABI-compatible with the standard 8-double params + output fields.
 */
#pragma pack(push, 8)
typedef struct TcPass_C_ModelParams {
    double init_lat;
    double init_lon;
    double init_alt;
    double init_speed;
    double init_heading;
    double init_pitch;
    double init_roll;
    double step_dt;
} TcPass_C_ModelParams;

typedef struct TcPass_C_ModelOutput {
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
} TcPass_C_ModelOutput;
#pragma pack(pop)

/*
 * TcPass_C - all-pass C (namespaced / unique types, no clash with TcPass_A)
 */
TcPass_C_API void* Model_Create(void);
TcPass_C_API int Model_Init(void* handle, const TcPass_C_ModelParams* params);
TcPass_C_API int Model_Step(void* handle, TcPass_C_ModelOutput* output);
TcPass_C_API void Model_Destroy(void* handle);
TcPass_C_API const char* Model_GetInfo(void);

#ifdef __cplusplus
}
#endif

#endif /* TCPASS_C_MODEL_H */
