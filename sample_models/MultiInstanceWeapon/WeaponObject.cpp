#include "WeaponObject.h"

WeaponObject::WeaponObject() : m_handle(nullptr) {}
WeaponObject::~WeaponObject() { Shutdown(); }

int WeaponObject::Initialize(int objectId, double lat, double lon, double alt,
                             double speed, double dt) {
    Shutdown();
    m_handle = Model_Create();
    if (!m_handle) return -1;
    WeaponModelParams parameters{};
    parameters.init_lat = lat + objectId * 0.001;
    parameters.init_lon = lon + objectId * 0.001;
    parameters.init_alt = alt;
    parameters.init_speed = speed;
    parameters.init_heading = 35.0 + objectId * 7.0;
    parameters.init_pitch = 12.0;
    parameters.init_roll = 0.0;
    parameters.step_dt = dt;
    return Model_Init(m_handle, &parameters);
}

int WeaponObject::Step(double& latitude, double& longitude) {
    if (!m_handle) return -1;
    WeaponModelOutput output{};
    const int result = Model_Step(m_handle, &output);
    latitude = output.lat;
    longitude = output.lon;
    return result;
}

void WeaponObject::Shutdown() {
    if (m_handle) Model_Destroy(m_handle);
    m_handle = nullptr;
}
