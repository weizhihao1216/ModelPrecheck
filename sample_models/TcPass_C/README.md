# TcPass_C

Clean reference model with **type / namespace isolation** so it can be co-selected with `TcPass_A` without struct-name collisions.

## Isolation design

| TcPass_A (global) | TcPass_C (isolated) |
|-------------------|---------------------|
| `WeaponModelParams` | `TcPass_C_ModelParams` |
| `WeaponModelOutput` | `TcPass_C_ModelOutput` |
| `class WeaponObject` | `TcPass_C::WeaponObject` |

## UserMain example

```cpp
void* handle = Model_Create();
TcPass_C_ModelParams p{};
p.init_lat = R.lat;
p.init_lon = R.lon;
p.init_alt = R.alt;
p.init_speed = R.speed;
p.init_heading = 45.0;
p.init_pitch = 15.0;
p.init_roll = 0.0;
p.step_dt = 0.02;
Model_Init(handle, &p);
TcPass_C_ModelOutput o{};
Model_Step(handle, &o);
RecordTrajectoryPoint(o.lat, o.lon);
Model_Destroy(handle);
```

## Mo* example

```cpp
using MoModelType = TcPass_C::WeaponObject;
```

## Package layout

```
TcPass_C/
  include/WeaponModel.h
  include/WeaponObject.h
  lib/TcPass_C.lib
  models/TcPass_C.dll
```
