#ifndef SAMPLE_WEAPON_OBJECT_H
#define SAMPLE_WEAPON_OBJECT_H

#if defined(__has_include)
#  if __has_include("WeaponModel_MultiInstance.h")
#    include "WeaponModel_MultiInstance.h"
#  else
#    include "WeaponModel.h"
#  endif
#else
#  include "WeaponModel.h"
#endif

class WeaponObject {
public:
    WeaponObject();
    ~WeaponObject();

    int Initialize(int objectId, double lat, double lon, double alt,
                   double speed, double dt);
    int Step(double& latitude, double& longitude);
    void Shutdown();

private:
    void* m_handle;
};

#endif // SAMPLE_WEAPON_OBJECT_H
