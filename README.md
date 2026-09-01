# 第三方武器模型 DLL 集成预检工具 (Model Validator)

## 1. 项目概述

本工具面向仿真引擎集成前的第三方武器模型 C++ 动态库（DLL）预检，提供桌面端一站式能力：

- 模型包静态检查（头文件 / LIB / PE 依赖与导出）
- 受控动态加载与 SEH 硬件异常隔离
- 可编译的用户脚本 `UserMain`（驱动 Init / Step / Destroy）
- UserMain 性能压测、内存增长监测与实时性判定
- 多型号并行 / 单型号多线程稳定性测试
- 二维经纬度轨迹试跑与路径点表
- HTML 预检报告（综合判定矩阵 + 按型号明细）

---

## 2. 技术栈与开发环境

| 项 | 说明 |
| :--- | :--- |
| 编译器 | MSVC（推荐 VS2017+ / VS2022），`/utf-8`，`NOMINMAX` |
| UI | Qt 5.x Widgets + Qt Charts（优先 `QTDIR`，其次本机 Qt 5.14.2 / 5.11 路径） |
| 构建 | CMake 3.12+；一键脚本 `build.bat`（默认 VS2022 x64） |
| 系统 API | Win32 PE 解析、`LoadLibraryExW`、SEH（`__try/__except`）、`psapi` 内存统计 |

---

## 3. 使用说明

### 3.1 构建与启动

```powershell
# 一键构建并安装到 dist/
.\build.bat

# 或手动构建
cmake -G "Visual Studio 17 2022" -A x64 -B build
cmake --build build --config Release --target ModelValidator
```

可执行文件：`build\Release\ModelValidator.exe`（或 `dist\ModelValidator.exe`）。

样例模型 DLL 一般在：

```text
build\sample_models\<型号名>\Release\
```

例如 `CompatibleWeaponE`：

```text
build\sample_models\CompatibleWeaponE\Release\
  CompatibleWeaponE.dll
  WeaponModel.h
```

### 3.2 准备第三方模型包

每个「型号」对应一个模型包目录，建议结构：

```text
YourModelPackage\
  WeaponModel.h          # 或厂商头文件
  YourModel.dll          # 武器模型 DLL
  （可选）.lib / 授权文件或授权文件夹
```

**授权文件放置：**

> 授权文件或文件夹请放在第三方模型 dll 同级目录；如不可用可复制一份放在本 exe 同级目录下。

加载使用 `LOAD_WITH_ALTERED_SEARCH_PATH`，优先从 DLL 所在目录解析依赖。

### 3.3 基本操作流程

1. **添加型号**  
   左侧「型号与 UserMain」→「添加型号」→ 填写名称 →「浏览」选择模型包根目录。

2. **勾选头文件**  
   「刷新列表」后勾选要参与编译的 `.h`（可多选）。

3. **编写 / 确认 UserMain**  
   默认模板完成 `Model_Init` → 循环 `Model_Step` → `Model_Destroy`。  
   每步用 `out_lat` / `out_lon` 接收经纬度，并调用：

   ```cpp
   RecordTrajectoryPoint(out_lat, out_lon);
   ```

   供「试跑并绘制轨迹」采集二维轨迹。

4. **配置随机变量 `R.*`**  
   如 `lat` / `lon` / `alt` / `speed` 等，压测与试跑时按范围随机采样。

5. **编译型号**  
   「编译当前型号」或「编译全部型号」。  
   生成物在 exe 旁：`TestModel\<型号名>\<型号名>.dll`（Harness，不是厂商 DLL）。

6. **一键预检全部型号**  
   扫描各型号包内头文件 / LIB / DLL，做静态 PE、导出、动态加载检查，并刷新「预检报告」。

7. **按需专项测试**（需先编译成功）  
   - **性能压测**：选型号 → 次数与 Hz →「执行性能压测」  
   - **二维轨迹**：选型号 →「试跑并绘制轨迹」（左图 + 右侧经纬度表）  
   - **多型号并行**：各型号设实例数 →「并行测试（一起跑）」  
   - **多线程**：选型号与线程数 →「执行多线程测试」

8. **查看 / 导出报告**  
   「预检报告」页查看 HTML；「导出预检报告」保存。  
   多型号会话报告含：综合判定矩阵、型号总览、按型号 PE、压测/并行明细与日志。未执行项显示 **N/A**。

### 3.4 界面分区一览

| 区域 | 作用 |
| :--- | :--- |
| 预检控制 | 一键预检全部型号、导出报告 |
| 状态 Badges | 头文件 / LIB / DLL 通过情况 |
| 左侧型号面板 | 型号列表、包路径、头文件、UserMain、随机变量、编译 |
| 静态 PE 分析 | 按型号/DLL 查看架构、CRT、导入依赖、导出符号 |
| 性能压测 | 耗时/内存曲线；二维 Lon×Lat 轨迹与路径点表 |
| 多型号并行 | 各型号实例数与并行结果 |
| 多线程测试 | 同型号多线程 UserMain 结果 |
| 预检报告 | 内嵌 HTML 总报告 |
| 底部日志 | 运行过程日志 |

### 3.5 样例模型说明

| 样例 | 用途 |
| :--- | :--- |
| `StandardWeapon` | 标准合规样本 |
| `FaultyWeapon` | 故意故障样本 |
| `MultiInstanceWeapon` | 多实例 API 样本 |
| `ConflictWeaponA/B` | 共享内核对象冲突；同跑多型号易 FAIL |
| `CompatibleWeaponC/D/E` | `thread_local`，多线程/多型号宜 PASS；E 经纬度扰动更大，便于轨迹预览 |

---

## 4. 功能实现对照

| 能力 | 说明 | 主要实现 |
| :--- | :--- | :--- |
| 模型包扫描 | 递归发现 `.h` / `.lib` / `.dll` | `PackageScanner` |
| 头文件规范 | 编码、`extern "C"`、接口原型等 | `HeaderAnalyzer` |
| LIB 检查 | COFF 架构、导入库类型 | `LibAnalyzer` |
| PE 静态分析 | x86/x64、CRT MD/MT、Import/Export | `PeAnalyzer` |
| 动态加载 + SEH | `LoadLibraryExW`、符号绑定、硬件异常隔离 | `DllLoader`、`SehHelper` |
| UserMain 编译运行 | 生成 Harness 源码、`cl.exe` 编译、采样与 `RunOnce` | `UserCodeHarness` |
| 轨迹采集 | `RecordTrajectoryPoint` / `SetTrajectoryCapture` / `GetTrajectoryPoint` | `UserCodeHarness` 生成代码 |
| 性能压测 | 重复 UserMain：耗时、抖动、Working Set、帧预算判定 | `PerfProfiler`、`ChartViewerWidget` |
| 二维轨迹 UI | Lon×Lat 折线 + 路径点表 | `TrajectoryViewWidget`、`MainWindow` |
| 多型号并行 | 多路径 Harness 按实例数并发跑 UserMain | `ConcurrencyTester` |
| 多线程稳定性 | 单型号多线程各跑一次 UserMain | `ConcurrencyTester` |
| 轨迹逻辑校验 | NaN/边界/单位等（引擎侧能力） | `FunctionalVerifier` |
| HTML 报告 | 单报告 / 双构建 / 多型号 Fleet；矩阵 + PE + 压测/并行 + 日志 | `ReportGenerator` |
| 主界面编排 | 型号管理、一键预检、各 Tab 联调 | `MainWindow` |

需求编号对照（历史 F1–F13 仍适用，实现已演进为「多型号 + UserMain」工作流）：

| 编号 | 功能 | 现状 |
| :--- | :--- | :--- |
| F1–F3 | 依赖扫描、架构/CRT、导出校验 | 一键预检 +「静态 PE 分析」按型号查看 |
| F4–F6 | 安全加载、符号绑定、加载内存开销 | 一键预检阶段对包内 DLL 执行 |
| F7–F9 | 耗时/内存/实时性 | 「性能压测」对已编译 UserMain 连跑 |
| F10–F11 | 参数与曲线 | 随机变量表 + Charts；轨迹为二维经纬度 |
| F12 | 坐标一致性 | `FunctionalVerifier`（UserMain 自行驱动时以试跑采集为主） |
| F13 | 报告导出 | 预检报告页 + 导出 HTML |

---

## 5. 目录结构

```text
ModelPrecheck/
├── README.md
├── AGENTS.md
├── build.bat
├── CMakeLists.txt
├── src/
│   ├── main.cpp
│   ├── core/
│   │   ├── PeAnalyzer.*
│   │   ├── HeaderAnalyzer.*
│   │   ├── LibAnalyzer.*
│   │   ├── PackageScanner.*
│   │   ├── DllLoader.*
│   │   ├── UserCodeHarness.*      # UserMain 编译 / 轨迹采集 API
│   │   ├── PerfProfiler.*
│   │   ├── ConcurrencyTester.*    # 多型号 / 多线程
│   │   ├── FunctionalVerifier.*
│   │   └── ReportGenerator.*
│   ├── utils/
│   │   ├── SehHelper.*
│   │   ├── MemoryUtils.*
│   │   └── QtEncoding.h
│   └── ui/
│       ├── MainWindow.*
│       ├── ChartViewerWidget.*
│       ├── TrajectoryViewWidget.* # 二维 Lon×Lat 轨迹
│       ├── LogConsoleWidget.*
│       └── themestyle.qss
└── sample_models/
    ├── StandardWeapon/
    ├── FaultyWeapon/
    ├── MultiInstanceWeapon/
    ├── ConflictWeaponA/ B/
    └── CompatibleWeaponC/ D/ E/
```

---

## 6. 二次开发提示

### 6.1 扩展模型接口结构体

修改 `src/utils/SehHelper.h` 中 `WeaponModelParams` / `WeaponModelOutput` 及函数指针；同步样例头文件与 `DllLoader` 调用约定。

### 6.2 扩展 UserMain / 轨迹

- 默认模板：`UserCodeHarness::DefaultUserMainTemplate()`
- 轨迹导出符号由 `GenerateSource` 注入；改签名后需重新「编译型号」

### 6.3 扩展校验与报告

- 物理/坐标规则：`FunctionalVerifier::VerifyTrajectory`
- 报告章节：`ReportGenerator::GenerateHtml` / `GenerateFleetHtml`

### 6.4 强制约束（见 AGENTS.md）

1. 包含 `<windows.h>` 前定义 `NOMINMAX`
2. 对 DLL 调用使用 SEH 包装（`SehHelper`）
3. 勿在头文件中引入 `QT_CHARTS_USE_NAMESPACE`；Charts 仅放 `.cpp`
