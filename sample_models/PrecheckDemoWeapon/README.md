# PrecheckDemoWeapon — UserMain 与单线程多对象通用测试包

本目录为**开箱即用**的第三方模型样例，同一套 DLL / 头文件 / LIB 可同时用于：

- **UserMain** 性能、轨迹、多线程等测试  
- **单线程多对象**（MoCreate / MoInit / MoStep / MoDestroy）

## 目录内容

| 文件 | 说明 |
|------|------|
| `PrecheckDemoWeapon.dll` | 模型动态库 |
| `PrecheckDemoWeapon.lib` | 链接库（编译 UserMain / 多对象 Harness 时需要） |
| `WeaponModel.h` | C 接口：`Model_Create` / `Model_Init` / `Model_Step` / `Model_Destroy` |
| `WeaponObject.h` | C++ 封装类（头文件内联，无需 .cpp） |

## 在预检工具中使用

1. 点击 **添加型号**，模型包路径选择**本文件夹**（含 dll 与 lib 的目录）。  
2. 在 **型号与 UserMain** 中勾选：`WeaponModel.h`、`WeaponObject.h`（至少勾选 `WeaponModel.h`）。  
3. **UserMain** 与 **单线程多对象** 使用下面示例代码，分别 **编译** 后运行测试。

## UserMain 示例

```cpp
void* handle = Model_Create();
if (!handle) return -1;

WeaponModelParams p{};
p.init_lat = R.lat;
p.init_lon = R.lon;
p.init_alt = R.alt;
p.init_speed = R.speed;
p.init_heading = 45.0;
p.init_pitch = 15.0;
p.init_roll = 0.0;
p.step_dt = 0.02;

if (Model_Init(handle, &p) != 0) { Model_Destroy(handle); return -1; }

for (int i = 0; i < 100; ++i) {
    WeaponModelOutput o{};
    if (Model_Step(handle, &o) != 0) { Model_Destroy(handle); return -2; }
    RecordTrajectoryPoint(o.lat, o.lon);
}
Model_Destroy(handle);
return 0;
```

## 单线程多对象示例

`MoModelType` 使用本目录的 `WeaponObject`：

```cpp
using MoModelType = WeaponObject;

static MoModelType* MoCreate(int objectId, const RandomBag& R) {
    (void)R;
    return new MoModelType();
}

static int MoInit(MoModelType* obj, int objectId, const RandomBag& R, double dt) {
    return obj->Initialize(objectId, R.lat, R.lon, R.alt, R.speed, dt);
}

static int MoStep(MoModelType* obj, int objectId, int stepIndex, double dt,
                  const RandomBag& R,
                  double& out_lat, double& out_lon) {
    (void)objectId; (void)stepIndex; (void)dt; (void)R;
    // out_lat / out_lon：本步经纬度，工具自动记录并用于二维轨迹预览
    return obj->Step(out_lat, out_lon);
}

static void MoDestroy(MoModelType* obj, int objectId) {
    (void)objectId;
    obj->Shutdown();
    delete obj;
}
```

## 构建（开发者）

```powershell
cmake --build build --config Release --target PrecheckDemoWeapon
cmake --build build --config Release --target install
```

安装后本包位于：`dist/sample_models/PrecheckDemoWeapon/`
