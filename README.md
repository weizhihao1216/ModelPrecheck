# 第三方武器模型 DLL 集成预检工具 (Model Verification)

本工具面向仿真引擎（如 XSIM）集成前的第三方武器模型 C++ 动态库（DLL）预检：对**规范模型包**做静态检查、受控加载、UserMain / 多对象脚本编译运行，以及性能、并发、轨迹与报告输出。

---

## 0. 模型集成问题对照（来源：集成问题表）

对应历史「模型集成问题」清单。工具侧能力以**能否发现 / 隔离 / 复现 / 报告**为主；厂家缺库、隐含依赖等根因仍需模型侧整改。

### 0.1 已解决（工具可检测或已提供对策）

#### 静态 · 构建 / 链接配置
- **现象**：编译 Debug 报错、Release 成功  
- **场景 / 原因**：厂家常忽略或不提供 Debug 链接库  
- **工具操作**：添加型号 → 选模型包 → **一键预检**；顶部徽章「Release/Debug」、弹窗与各导航页「可能导致」会标明无法编 Debug  
- **功能实现**：扫描包内 Debug/Release DLL·LIB；缺 Debug 库时提示无法编译 Debug 及后果；识别 MD/MDd CRT  
- **实现方式**：`PackageScanner`；`PrecheckSummary`；`PeAnalyzer`；`PrecheckSummaryDialog` / 各页结果面板  

#### 静态 + 动态 · 头文件 / 符号冲突
- **现象**：同厂家多模型集成后偶发崩溃  
- **场景 / 原因**：结构体重名、无命名空间隔离，合进同一模型库易崩  
- **工具操作**：多型号加入列表并勾选头文件 → **一键预检** → 头文件冲突表 / 报告「跨型号头文件冲突」  
- **功能实现**：同包及跨型号头文件集合冲突分析；导出声明与 PE 导出交叉比对  
- **实现方式**：`HeaderAnalyzer::AnalyzeHeaderSet` / `VerifyConsistency`；`ReportGenerator` Fleet 章节

#### 动态 · 多线程安全
- **现象**：XSIM 勾选多线程后崩溃  
- **场景 / 原因**：模型未做线程安全  
- **工具操作**：编译型号 →「多线程稳定性」选型号与线程数 → **执行多线程测试**；或一键预检自动跑  
- **功能实现**：多线程各跑独立 UserMain；记录成功/失败/SEH；PASS/WARN/FAIL  
- **实现方式**：`ConcurrencyTester::Run`（MultiThread）；样例 `CompatibleWeaponC/D/E`（`thread_local`）

#### 动态 · 多实例 / 状态隔离
- **现象**：同一武器发射多枚崩溃  
- **场景 / 原因**：不支持多实例或集成越界  
- **工具操作**：「单线程多对象」写 `MoCreate/Init/Step/Destroy` → 编译当前/全部 → **基线与交错**；或「多型号并行」设实例数；也可「跨型号对象交错」  
- **功能实现**：逐对象基线 vs 单线程交错；偏差、返回码、SEH、多轨迹对比  
- **实现方式**：`MultiObjectHarness` + `SingleThreadMultiObjectTester` + `FleetSingleThreadMultiObjectTester`；样例 `MultiInstanceWeapon` / `CompatibleWeaponF/G` / `ConflictWeapon*`

#### 动态 · 模型间干扰 / 内存破坏
- **现象**：同厂家不同模型同场景冲突、数据异常  
- **场景 / 原因**：共享全局/单例环境，或越界写坏公共数据  
- **工具操作**：多型号同时添加 → 全部编译 → **多型号并行**；对照 Conflict vs Compatible 样例  
- **功能实现**：多路径 Harness 同进程并发；报告串扰/崩溃；轨迹/日志辅助定位  
- **实现方式**：`ConcurrencyTester::RunMultiModel`；样例 `ConflictWeaponA/B`（易 FAIL）、`CompatibleWeaponC/D/E`（宜 PASS）

### 0.2 部分解决（可检测风险，尚无完整专项或无法根治）

#### 动态 · CRT / ABI 与跨模块内存
- **现象**：初始化路径参数带 `c_str()` 读不到路径  
- **场景 / 原因**：双方 CRT 不匹配、堆隔离  
- **工具操作**：一键预检 / PE 页看 **CRT MD/MT**；UserMain 由本工具 MSVC 统一编 Harness  
- **功能实现**：报告 CRT；加载失败写日志  
- **实现方式**：`PeAnalyzer`；`DllLoader` + SEH；`UserCodeHarness`  
- **仍缺**：不能改写厂家 DLL 的 CRT；跨堆传 `std::string` 需双方约定

#### 动态 · 倍速调度 / 数值稳定性
- **现象**：同一枚弹加速到约 50 倍速崩溃  
- **场景 / 原因**：固定步长/调用频率，高倍速导致发散或越界  
- **工具操作**：「UserMain 性能压测」设次数与目标 Hz → 看耗时/抖动/实时性；若头文件有 `step_dt`，可在 UserMain 中放大后再测  
- **功能实现**：相对帧预算 PASS/WARN/FAIL；曲线展示  
- **实现方式**：`PerfProfiler` + `ChartViewerWidget`  
- **仍缺**：无内置「N 倍速想定」专项（故意放大 `dt` / 单帧多次 Step）；无 `dt` 的厂家接口无法从外部改物理步长

#### 环境兼容 · 依赖 / ABI / 版本
- **现象**：不同电脑 / XSIM5·6 表现不一  
- **场景 / 原因**：版本、依赖 DLL、VC 运行库、授权路径、ABI 不一致  
- **工具操作**：PE 页看导入与架构；授权放 DLL 同级（或 exe 旁备份）；一键预检看缺失依赖  
- **功能实现**：缺失 DLL、架构、导出扫描；授权目录提示  
- **实现方式**：`PeAnalyzer` Import；`LOAD_WITH_ALTERED_SEARCH_PATH`  
- **仍缺**：无法覆盖全部主程序 ABI 与授权加密逻辑

#### 架构与集成设计 · 隐含依赖
- **现象**：模型从原系统拆出后集成繁琐  
- **场景 / 原因**：仍依赖原系统全局状态、初始化顺序、资源  
- **工具操作**：用 **UserMain / Mo\*** 自行编排；改名接口可用映射样例；报告对未测项标 N/A  
- **功能实现**：用户脚本驱动生命周期；标准 Handle 自动识别；HTML 矩阵  
- **实现方式**：`UserCodeHarness`、`MultiObjectHarness`、`InterfaceMappingProfile`、`ReportGenerator`  
- **仍缺**：无法消除未文档化隐含依赖，需厂家提供独立 API

### 0.3 问题类型索引

- **静态（构建/链接）** — 已解决：Debug 库缺失的**检测**；部分解决：—  
- **静态+动态（头文件/符号）** — 已解决：跨型号头文件冲突检测；部分解决：—  
- **动态（多线程）** — 已解决：多线程 UserMain 稳定性；部分解决：—  
- **动态（多实例/隔离）** — 已解决：单线程多对象 + 跨型号交错 + 多型号并行；部分解决：—  
- **动态（模型间干扰）** — 已解决：同进程多型号并发复现；部分解决：—  
- **动态（CRT/ABI）** — 已解决：—；部分解决：CRT 识别与加载失败日志  
- **动态（倍速/数值）** — 已解决：—；部分解决：帧预算压测，无专用倍速用例  
- **环境兼容** — 已解决：—；部分解决：依赖/架构扫描 + 授权提示  
- **架构与集成设计** — 已解决：—；部分解决：UserMain/Mo\* 灵活编排，根因在厂家  

---

## 1. 项目概述

桌面端一站式能力：

- 规范模型包静态检查（`include/` / `lib/` / `models/`）
- 受控动态加载与 SEH 硬件异常隔离
- 可编译 `UserMain` 与 `Mo*` 多对象 Harness（支持「编译当前 / 编译全部」）
- UserMain 性能压测、内存增长监测与实时性判定
- 多型号并行 / 单型号多线程稳定性
- 单线程多对象基线/交错，以及**跨型号对象交错**
- 二维经纬度轨迹（UserMain：`RecordTrajectoryPoint`；多对象：`MoStep` 的 `out_lat/out_lon`）
- HTML 预检报告（综合判定矩阵 + 按型号明细；未测项 N/A）
- QScintilla 代码编辑器；编译/预检等候动画与结果提示
- **关闭自动保存会话**；启动时可选择是否还原上次编辑

---

## 2. 技术栈与开发环境

- **编译器**：MSVC（推荐 VS2017+ / VS2022），`/utf-8`，`NOMINMAX`  
- **UI**：Qt 5.x Widgets + Qt Charts + **QScintilla 2.14.1（静态链入）**  
- **构建**：CMake 3.12+；一键脚本 `build.bat`（默认 VS2022 x64）  
- **系统 API**：Win32 PE、`LoadLibraryExW`、SEH、`psapi`  
- **QScintilla**：见 `third_party/README_QScintilla.md`（GPL-3 / 商业双许可）  

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

若缺少 QScintilla 静态库，先执行：

```bat
third_party\build_qscintilla.bat
```

样例模型包一般安装到：`dist\sample_models\<型号名>\{include,lib,models}\`（构建安装后）。

### 3.1.1 换机部署（拷贝 dist）

整夹拷贝 `dist/` 即可携带程序与 Qt 依赖。目标机说明见 **`dist/README.md`**：

- 缺 `vcruntime140.dll` / `msvcp140.dll` → 运行 `dist/prerequisites/vc_redist.x64.exe`
- UserMain / 多对象「编译」需要 `cl.exe` → 运行 `dist/prerequisites/install_build_tools.bat`

首次准备安装包（或安装包丢失时）可执行：

```powershell
powershell -ExecutionPolicy Bypass -File .\deploy\prerequisites\download_prerequisites.ps1
```

`build.bat` 会在打包末尾把 `deploy/prerequisites` 同步到 `dist/prerequisites`。

### 3.2 准备第三方模型包（固定目录结构）

工具**要求**模型包根目录包含以下三个子目录（各自可再含子文件夹，递归扫描）：

```text
YourModelPackage\
  include\          # 头文件 .h / .hpp
  lib\              # 导入库 / 静态库 .lib
  models\           # 模型动态库 .dll（授权文件建议与 dll 同级）
```

- **授权文件**：优先放在 `models/` 下第三方 `.dll` 同级；不可用时可复制到本 exe 同级。  
- 加载使用 `LOAD_WITH_ALTERED_SEARCH_PATH`。

### 3.3 基本操作流程

1. **添加型号** → 填名称 → 浏览模型包根目录（须含 `include/lib/models`）  
2. **勾选头文件** →「刷新列表」后从 `include/` 勾选参与编译的 `.h`  
3. **编写 UserMain**（默认 Create/Init → 循环 Step → Destroy）  
   - 轨迹试跑需：`out_lat` / `out_lon` + `RecordTrajectoryPoint(out_lat, out_lon)`  
   - 若接口有 `WeaponModelParams::step_dt`，可在 Init 前赋值以调节仿真步长  
4. **配置随机变量 `R.*`**  
5. **编译当前型号** 或 **编译全部型号** → Harness 输出到 exe 旁 `TestModel\<型号名>\`  
6. **一键预检全部型号** → 静态检查 + 已编译项的压测/并发等；结束后弹出四列表格总览（测试项 / 状态 / 原因 / 可能导致），并同步到各导航页与「查看报告」  
7. **专项测试**（多数需已编译）  
   - UserMain 性能压测 / 内存泄漏监测 / 运行轨迹查看  
   - 多型号并行 / 多线程稳定性  
   - 单线程多对象：写 Mo\* → 编译当前/全部 → 基线与交错；可选跨型号交错  
8. **预检报告**查看 / 导出 HTML  

启动时若检测到上次会话，会弹出深色风格询问框，可选择**还原会话**或**暂不还原**。关闭窗口时自动写入 `exe同级/session/last_session.json`。

### 3.4 界面分区一览

- **预检控制**：一键预检、导出报告  
- **状态 Badges**：头文件 / LIB / DLL 通过情况  
- **操作流程条**：添加型号 → 选包 → 配置 → 编译 → 测试 → 报告  
- **功能导航**：静态检查、性能/内存/轨迹、多型号、多线程、单线程多对象、报告  
- **型号列表**：添加/删除型号  
- **型号与 UserMain 配置**：包路径、头文件、UserMain、随机变量、编译与状态  
- **测试工作区**：各专项测试页（无标签栏，由左侧导航切换）  
- **底部日志**：过程与编译日志  

### 3.5 样例模型说明

- **`StandardWeapon`**：标准合规（偏单例）  
- **`FaultyWeapon`**：故意故障  
- **`MultiInstanceWeapon`**：Handle 多实例 + 多对象示例  
- **`ConflictWeaponA/B`**：共享内核冲突，多型号易 FAIL  
- **`CompatibleWeaponC/D/E`**：`thread_local`，多线程/多型号宜 PASS  
- **`CompatibleWeaponF/G`**：Handle API + `WeaponObject`，适合 UserMain / Mo\* / 跨型号交错  
- **`PrecheckDemoWeapon`**：统一演示 Handle API（含推荐目录结构说明）  
- **`RenamedInterfaceWeapon`**：改名接口 / 映射样例  

---

## 4. 功能实现对照

### 4.1 核心能力

- **模型包扫描** — 按 `include/lib/models` 递归发现 `.h` / `.lib` / `.dll` → `PackageScanner`  
- **头文件规范与冲突** — 编码、`extern "C"`、跨型号冲突 → `HeaderAnalyzer`  
- **LIB / PE** — 架构、CRT、Import/Export → `LibAnalyzer`、`PeAnalyzer`  
- **动态加载 + SEH** — 安全加载、硬件异常隔离 → `DllLoader`、`SehHelper`  
- **UserMain 编译运行** — Harness 生成、`cl` 编译、`RunOnce` → `UserCodeHarness`  
- **多对象 Harness** — `Mo*` 用户池 / 映射 / 对象会话 API → `MultiObjectHarness`  
- **轨迹采集** — UserMain：`RecordTrajectoryPoint`；多对象：`MoStep` 经纬度 → 上述 Harness  
- **性能压测** — 耗时、抖动、Working Set、帧预算 → `PerfProfiler`、`ChartViewerWidget`  
- **二维轨迹 UI** — Lon×Lat 折线 + 点表 → `TrajectoryViewWidget`  
- **多型号 / 多线程** — 并发 UserMain → `ConcurrencyTester`  
- **单线程多对象** — 基线/交错、串扰与 SEH → `SingleThreadMultiObjectTester`  
- **跨型号对象交错** — 多型号对象单线程 Step 交错 → `FleetSingleThreadMultiObjectTester`  
- **接口映射** — 改名接口 JSON 映射 → `InterfaceMappingProfile`  
- **预检总览** — 测试项通过/未通过/未检测 + 集成问题对照 → `PrecheckSummary`  
- **HTML 报告** — 单报告 / Fleet 矩阵 → `ReportGenerator`  
- **代码编辑器** — 高亮、补全、缩放 → `CppCodeEditor`（QScintilla）  
- **等候 / 结果提示** — 编译与测试非阻塞转圈；成功/失败居中提示 → `BusyOverlayWidget`  
- **会话缓存** — 关闭保存、启动询问还原 → `SessionStore`、`SessionRestoreDialog`  
- **主界面编排** — 型号管理、导航、一键预检 → `MainWindow`  

### 4.2 需求编号对照（历史 F1–F13）

- **F1–F3** 依赖扫描、架构/CRT、导出 → 一键预检 + PE 页  
- **F4–F6** 安全加载、符号、加载内存 → 一键预检对包内 DLL  
- **F7–F9** 耗时/内存/实时性 → 性能压测 / 内存监测  
- **F10–F11** 参数与曲线 → 随机变量 + Charts；二维轨迹  
- **F12** 坐标一致性 → `FunctionalVerifier` + 试跑采集  
- **F13** 报告导出 → 预检报告页 + 导出 HTML  

---

## 5. 目录结构

```text
ModelPrecheck/
├── README.md
├── AGENTS.md
├── build.bat
├── CMakeLists.txt
├── third_party/                 # QScintilla 源码与静态库安装
├── src/
│   ├── core/                    # Pe/Header/Lib/Package、Harness、Tester、Session、Report
│   ├── utils/                   # SehHelper、MemoryUtils
│   └── ui/                      # MainWindow、Charts、Trajectory、BusyOverlay、SessionRestore…
├── sample_models/
└── docs/                        # 主题预览等
```

运行时旁路目录（exe 同级，构建安装后常见于 `dist/`）：

```text
TestModel/<型号名>/     # UserMain / 多对象 Harness 编译输出
session/last_session.json
sample_models/...
```

---

## 6. 二次开发提示

### 6.1 扩展模型接口结构体

修改 `src/utils/SehHelper.h` 中参数/输出结构及函数指针；同步样例头文件与调用约定。

### 6.2 扩展 UserMain / 多对象 / 轨迹

- UserMain 模板：`UserCodeHarness::DefaultUserMainTemplate()`  
- 多对象：`MultiObjectHarness::DefaultUserMultiObjectTemplate()`（`MoCreate/MoInit/MoStep/MoDestroy`）  
- 改签名后需重新编译对应 Harness  

### 6.3 扩展校验与报告

- `FunctionalVerifier::VerifyTrajectory`  
- `ReportGenerator::GenerateHtml` / `GenerateFleetHtml`  

### 6.4 会话与 UI

- 会话字段扩展：`SessionStore` / `SessionSnapshot`  
- 还原弹框样式：`SessionRestoreDialog`  

### 6.5 强制约束（见 AGENTS.md）

1. 包含 `<windows.h>` 前定义 `NOMINMAX`  
2. DLL 调用经 SEH（`SehHelper`）  
3. 勿在头文件引入 `QT_CHARTS_USE_NAMESPACE`；Charts 仅放 `.cpp`  
