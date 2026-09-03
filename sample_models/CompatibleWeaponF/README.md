# CompatibleWeaponF — 型号 F（跨型号对象交错对照样例）

Handle 多实例接口，状态在各自 `Model_Create` 句柄内，无跨 DLL 共享静态。
与 **CompatibleWeaponG** 搭配做「跨型号对象交错」；单独也可用于 UserMain / 单型号多对象。

## 目录结构

```
CompatibleWeaponF/
  include/
    WeaponModel.h
    WeaponObject.h
  lib/
    CompatibleWeaponF.lib
  models/
    CompatibleWeaponF.dll
```

## 预检工具用法

1. 添加型号，模型包路径选本目录根（含 include/lib/models）。
2. 勾选 `include/WeaponModel.h`、`include/WeaponObject.h`。
3. UserMain / 多对象代码见下方；编译后即可跑测试。
4. 跨型号交错：F、G 都编译好多对象 Harness 后，在参与列表勾选两者执行。

## UserMain

```cpp
void* handle = Model_Create();
if (!handle) return -1;

WeaponModelParams p{};
p.init_lat = R.lat;
p.init_lon = R.lon;
p.init_alt = R.alt;
p.init_speed = R.speed;
p.init_heading = 45.0;
p.init_pitch = 12.0;
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
