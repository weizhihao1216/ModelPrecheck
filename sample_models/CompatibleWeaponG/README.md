# CompatibleWeaponG — 型号 G（跨型号对象交错对照样例）

与 **CompatibleWeaponF** 配对做「跨型号对象交错」。Handle 多实例，轨迹偏左转弯。

## 目录结构

```
CompatibleWeaponG/
  include/
    WeaponModel.h
    WeaponObject.h
  lib/
    CompatibleWeaponG.lib
  models/
    CompatibleWeaponG.dll
```

## 预检工具用法

1. 添加型号，模型包路径选本目录根。
2. 勾选 `include/WeaponModel.h`、`include/WeaponObject.h`。
3. UserMain / Mo* 与型号 F 相同模板即可（见下方）。
4. 跨型号：F、G 各自编译多对象 Harness → 勾选两者 →「执行跨型号对象交错」。

## UserMain

```cpp
void* handle = Model_Create();
if (!handle) return -1;

WeaponModelParams p{};
p.init_lat = R.lat;
p.init_lon = R.lon;
p.init_alt = R.alt;
p.init_speed = R.speed;
p.init_heading = 50.0;
p.init_pitch = 11.0;
p.init_roll = 0.0;
p.step_dt = 0.02;

if (Model_Init(handle, &p) != 0) { Model_Destroy(handle); return -1; }

for (int i = 0; i < 100; ++i) {
    WeaponModelOutput o{};
    if (Model_Step(handle, &o) != 0) { Model_Destroy(handle); return -2; }
    out_lat = o.lat;
    out_lon = o.lon;
    RecordTrajectoryPoint(out_lat, out_lon);
}
Model_Destroy(handle);
return 0;
```

## 单线程多对象（Mo*）

```cpp
using MoModelType = WeaponObject;

static MoModelType* MoCreate(int objectId, const RandomBag& R) {
    (void)objectId;
    (void)R;
    return new MoModelType();
}

static int MoInit(MoModelType* obj, int objectId, const RandomBag& R, double dt) {
    return obj->Initialize(objectId, R.lat, R.lon, R.alt, R.speed, dt);
}

static int MoStep(MoModelType* obj, int objectId, int stepIndex, double dt,
                  const RandomBag& R,
                  double& out_lat, double& out_lon) {
    (void)objectId;
    (void)stepIndex;
    (void)dt;
    (void)R;
    return obj->Step(out_lat, out_lon);
}

static void MoDestroy(MoModelType* obj, int objectId) {
    (void)objectId;
    delete obj;
}
```
