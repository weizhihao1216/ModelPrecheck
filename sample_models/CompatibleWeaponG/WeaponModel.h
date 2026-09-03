#ifndef COMPATIBLE_WEAPON_G_MODEL_H
#define COMPATIBLE_WEAPON_G_MODEL_H

#ifdef COMPATIBLE_WEAPON_G_EXPORTS
#define COMPATIBLE_WEAPON_G_API __declspec(dllexport)
#else
#define COMPATIBLE_WEAPON_G_API __declspec(dllimport)
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
 * CompatibleWeaponG — 型号 G（Handle 多实例，无跨 DLL 共享状态）
 * 与 CompatibleWeaponF 搭配做跨型号对象交错；轨迹偏左转弯。
 */
COMPATIBLE_WEAPON_G_API void* Model_Create(void);
COMPATIBLE_WEAPON_G_API int Model_Init(void* handle, const WeaponModelParams* params);
COMPATIBLE_WEAPON_G_API int Model_Step(void* handle, WeaponModelOutput* output);
COMPATIBLE_WEAPON_G_API void Model_Destroy(void* handle);
COMPATIBLE_WEAPON_G_API const char* Model_GetInfo(void);

#ifdef __cplusplus
}
#endif

#endif /* COMPATIBLE_WEAPON_G_MODEL_H */
