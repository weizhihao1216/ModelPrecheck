#ifndef COMPATIBLE_WEAPON_F_OBJECT_H
#define COMPATIBLE_WEAPON_F_OBJECT_H

#include "WeaponModel.h"

// 头文件内联：UserMain / 多对象 Harness 勾选本头并链接 CompatibleWeaponF.lib
class WeaponObject {
public:
    WeaponObject() : m_handle(nullptr) {}
    ~WeaponObject() { Shutdown(); }

    int Initialize(int objectId, double lat, double lon, double alt,
                   double speed, double dt) {
        Shutdown();
        m_handle = Model_Create();
        if (!m_handle) return -1;
        WeaponModelParams parameters{};
        parameters.init_lat = lat + objectId * 0.001;
        parameters.init_lon = lon + objectId * 0.001;
        parameters.init_alt = alt;
        parameters.init_speed = speed;
        parameters.init_heading = 40.0 + objectId * 6.0;
        parameters.init_pitch = 10.0;
        parameters.init_roll = 0.0;
        parameters.step_dt = dt;
        return Model_Init(m_handle, &parameters);
    }

    int Step(double& latitude, double& longitude) {
        if (!m_handle) return -1;
        WeaponModelOutput output{};
        const int result = Model_Step(m_handle, &output);
        latitude = output.lat;
        longitude = output.lon;
        return result;
    }

    void Shutdown() {
        if (m_handle) Model_Destroy(m_handle);
        m_handle = nullptr;
    }

private:
    void* m_handle;
};

#endif /* COMPATIBLE_WEAPON_F_OBJECT_H */
