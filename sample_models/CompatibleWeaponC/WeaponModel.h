#ifndef WEAPON_MODEL_H
#define WEAPON_MODEL_H

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
    double lat;   /* UserMain: out_lat = o.lat */
    double lon;   /* UserMain: out_lon = o.lon */
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
 * CompatibleWeaponC — 线程局部状态，无跨 DLL 共享对象。
 * Model_Step 每步写入 lat/lon（UserMain 中对应 out_lat / out_lon）。
 * 单型号多线程 / 与 CompatibleWeaponD 多型号并行均应 PASS。
 */
WEAPON_API int Model_Init(const WeaponModelParams* params);
WEAPON_API int Model_Step(WeaponModelOutput* output);
WEAPON_API void Model_Destroy(void);
WEAPON_API const char* Model_GetInfo(void);

#ifdef __cplusplus
}
#endif

#endif /* WEAPON_MODEL_H */
