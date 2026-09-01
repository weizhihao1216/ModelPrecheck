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
 * CompatibleWeaponE — 线程局部状态；Step 输出随机扰动的经纬度，便于二维轨迹预览。
 * 单型号多线程 / 多型号并行均应 PASS。API 与 StandardWeapon 相同。
 */
WEAPON_API int Model_Init(const WeaponModelParams* params);
WEAPON_API int Model_Step(WeaponModelOutput* output);
WEAPON_API void Model_Destroy(void);
WEAPON_API const char* Model_GetInfo(void);

#ifdef __cplusplus
}
#endif

#endif /* WEAPON_MODEL_H */
