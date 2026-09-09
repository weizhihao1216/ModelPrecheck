# 第三方武器模型 DLL 集成预检工具 (Model Verification)

本工具面向仿真引擎（如 XSIM）集成前的第三方武器模型 C++ 动态库（DLL）预检：对**规范模型包**做静态检查、受控加载、UserMain / 多对象脚本编译运行，以及性能、并发、轨迹与报告输出。

对照表与样例实测见：`dist/模型集成问题.xlsx`（含「模型集成问题」「测试用例索引」「实测矩阵」）。

---

## 0. 模型集成问题对照（来源：集成问题表）

对应历史「模型集成问题」清单。工具侧能力以**能否发现 / 隔离 / 复现 / 报告**为主；厂家缺库、隐含依赖等根因仍需模型侧整改。

### 0.1 已解决（工具可检测或已提供对策）

#### 静态 · 构建 / 链接配置
- **现象**：编译 Debug 报错、Release 成功  
- **场景 / 原因**：厂家常忽略或不提供 Debug 链接库  
- **工具操作**：添加型号 → 选模型包 → **一键预检**；顶部徽章「Release/Debug」、弹窗与各导航页「可能导致」会标明无法编 Debug  
- **功能实现**：扫描包内 Debug/Release DLL·LIB；缺 Debug 库时提示无法编译 Debug 及后果；识别 MD/MTd CRT  
- **实现方式**：`PackageScanner`；`PrecheckSummary`；`PeAnalyzer`；`PrecheckSummaryDialog` / 各页结果面板  
- **样例**：`ReleaseOnly_A` / `ReleaseOnly_B`（实测：`R=PASS D=FAIL`）

#### 静态 + 动态 · 头文件 / 符号冲突
- **现象**：同厂家多模型集成后偶发崩溃  
- **场景 / 原因**：结构体重名、无命名空间隔离，合进同一模型库易崩  
- **工具操作**：多型号加入列表 → **一键预检** → 头文件冲突表 / 报告「跨型号头文件冲突」  
- **功能实现**：同包及跨型号头文件集合冲突分析（**手工扫描，避免 MSVC `std::regex` 大头文件栈溢出闪退**）；导出与 PE 交叉比对  
- **实现方式**：`HeaderAnalyzer::AnalyzeHeaderSet` / `VerifyConsistency`；`ReportGenerator` Fleet 章节  
- **样例**：`HeaderClash_A` + `HeaderClash_B`（成对冲突 FAIL）；对照 `TcPass_A` + `TcPass_C`（隔离命名可 PASS）

#### 动态 · DLL 接口与加载
- **现象**：主程序无法加载模型或初始化即崩  
- **工具操作**：一键预检 / 「DLL 接口与加载」单项  
- **功能实现**：Release 宿主对 **Release DLL 执行 `LoadLibrary`**；**Debug（`/MDd`）DLL 仅做 PE 并标记 `SKIP:`**（避免 CRT 冲突整进程闪退）；汇总时 SKIP 不计入失败分母  
- **实现方式**：`DllLoader` + SEH；`PrecheckSummary`（`dll_load`）  
- **说明**：要在匹配 CRT 下验证 Debug 库，请使用 Debug 构建的预检工具或 Harness

#### 动态 · 多线程安全（SEH / 返回码）
- **现象**：XSIM 勾选多线程后崩溃  
- **场景 / 原因**：模型未做线程安全  
- **工具操作**：编译型号 →「多线程稳定性」→ **执行多线程测试**；或一键预检自动跑  
- **功能实现**：多线程各跑独立 UserMain；记录成功/失败/SEH  
- **实现方式**：`ConcurrencyTester::Run`（MultiThread）  
- **样例 / 实测**：`SehFault_*` 可必现 SEH；`ThreadUnsafe_*` 多为**静默竞态**——Create 可返回同一全局句柄，但 4 线程生命周期仍可能 **SEH/返回码全 0**（见 0.2）

#### 动态 · 多实例 / 状态隔离
- **现象**：同一武器发射多枚崩溃或状态串扰  
- **场景 / 原因**：不支持多实例或集成越界  
- **工具操作**：「单线程多对象」写 `MoCreate/Init/Step/Destroy` → 编译 → **基线与交错**；或「多型号并行」设实例数；可选「跨型号对象交错」  
- **功能实现**：基线 vs 单线程 Step 交错；轨迹偏差、返回码、SEH  
- **实现方式**：`MultiObjectHarness` + `SingleThreadMultiObjectTester` + `FleetSingleThreadMultiObjectTester`  
- **样例 / 实测**：`SingleInstance_*` 二次 Create 属 UB，**未必当场 SEH**——宜用多对象 `objectCount≥2` 强化暴露；`TcPass_*` 双 Create / 生命周期宜 PASS

#### 动态 · 模型间干扰 / 内存破坏
- **现象**：同厂家不同模型同场景冲突、数据异常  
- **场景 / 原因**：共享全局/单例环境，或越界写坏公共数据  
- **工具操作**：多型号同时添加 → 全部编译 → **多型号并行** / **跨型号对象交错**  
- **功能实现**：多路径 Harness 同进程并发；单线程跨型号 Step 交错比轨迹  
- **实现方式**：`ConcurrencyTester::RunMultiModel`；`FleetSingleThreadMultiObjectTester`  
- **样例 / 实测**：`SharedKernel_A/B` 单包生命周期可 PASS，**需 A+B 同进程**才易暴露；对照 `TcPass_A/B/C`

### 0.2 部分解决（可检测风险，尚无完整专项或无法根治）

#### 动态 · 静默数据竞态（无 SEH）
- **现象**：多线程下偶发数据错乱但不崩溃  
- **工具现状**：多线程项主要看 **SEH + UserMain 返回码**，不比轨迹指纹  
- **样例**：`ThreadUnsafe_A/B`（实测：单例句柄可检出；mt4 仍可能 fail=0）  
- **仍缺**：串行基线 vs 并发同种子指纹比对（产品级增强项）

#### 动态 · CRT / ABI 与跨模块内存
- **现象**：初始化路径参数带 `c_str()` 读不到路径  
- **场景 / 原因**：双方 CRT 不匹配、堆隔离  
- **工具操作**：一键预检 / PE 页看 **CRT MD/MT**；UserMain 由本工具 MSVC 统一编 Harness  
- **样例**：`CrtMd_A`（/MD）、`CrtMt_B`（/MT）——实测均可 Load+短生命周期 PASS，**仅为 PE 对照**，无跨堆 FAIL 判定  
- **仍缺**：不能改写厂家 DLL 的 CRT；无「跨堆传 `c_str`」自动化专项

#### 动态 · 倍速调度 / 数值稳定性
- **现象**：同一枚弹加速到约 50 倍速崩溃  
- **工具操作**：「UserMain 性能压测」设次数与目标 Hz / **内存上限(MB)**；可在 UserMain 中放大 `step_dt`  
- **功能实现**：帧预算 PASS/WARN/FAIL；WorkingSet/PrivateUsage 增长超限可提前结束（防 OOM 闪退）  
- **仍缺**：无内置「N 倍速想定」专项

#### 动态 · 内存泄漏监测
- **工具操作**：性能压测页看泄漏率 / 内存上限  
- **样例**：`MemLeak_A`（实测约 `0.25MB@200step`，可被监测捕获）

#### 环境兼容 · 依赖 / ABI / 版本
- **工具操作**：PE 页看导入与架构；授权放 DLL 同级；一键预检看缺失依赖  
- **仍缺**：无法覆盖全部主程序 ABI 与授权加密逻辑

#### 架构与集成设计 · 隐含依赖
- **工具操作**：用 **UserMain / Mo\*** 自行编排；报告对未测项标 N/A  
- **仍缺**：无法消除未文档化隐含依赖

### 0.3 问题类型索引

- **静态（构建/链接）** — 已解决：Debug 库缺失检测（`ReleaseOnly_*`）  
- **静态+动态（头文件/符号）** — 已解决：跨型号冲突检测（`HeaderClash_*` / `TcPass_A+C`）  
- **动态（加载）** — 已解决：Release 加载 + Debug SKIP 汇总；部分：Debug 须匹配宿主 CRT  
- **动态（多线程）** — 已解决：SEH/返回码崩溃复现；部分解决：静默竞态  
- **动态（多实例/隔离）** — 已解决：多对象交错 + 并行实例；部分：纯 UB 越界未必 SEH  
- **动态（模型间干扰）** — 已解决：多型号并行 / 跨型号交错；需成对样例同测  
- **动态（CRT/ABI）** — 部分解决：CRT 识别；无跨堆专项  
- **动态（倍速/数值/泄漏）** — 部分解决：帧预算压测 + 内存上限；`MemLeak_A`  
- **环境兼容 / 架构** — 部分解决：依赖扫描 + 灵活编排  

### 0.4 三类并发测试如何区分（不冗余）

- **多型号并行**：多线程；各型号完整 `UserMain`；侧重跨 DLL 同进程 SEH/返回码  
- **多线程稳定性**：多线程；单型号多次 `UserMain`；侧重线程安全冒烟（SEH/返回码）  
- **单线程多对象**（含跨型号）：**单线程**；`Mo*` / Create·Init·**Step 交错**·Destroy；侧重**基线 vs 交错轨迹偏差**（串扰）  

多线程 PASS ≠ 多对象 PASS；多对象 PASS ≠ 多型号并行 PASS。

---

## 1. 项目概述

桌面端一站式能力：

- 规范模型包静态检查（`include/` / `lib/` / `models/`）
- 受控动态加载与 SEH 硬件异常隔离（一键预检：Release 加载，Debug 跳过）
- 可编译 `UserMain` 与 `Mo*` 多对象 Harness（「编译当前 / 编译全部」）
- UserMain 性能压测、内存增长监测（含内存上限）、实时性判定
- 多型号并行 / 单型号多线程稳定性
- 单线程多对象基线/交错，以及**跨型号对象交错**
- 二维经纬度轨迹；HTML 预检报告；预检总览四列表（状态 / 原因 / 可能导致）
- QScintilla 编辑器；BusyOverlay 等候提示
- **关闭自动保存会话**；启动时可选择是否还原

---

## 2. 技术栈与开发环境

- **编译器**：MSVC（推荐 VS2017+ / VS2022），`/utf-8`，`NOMINMAX`  
- **UI**：Qt 5.x Widgets + Qt Charts + **QScintilla 2.14.1（静态链入）**  
- **构建**：CMake 3.12+；一键脚本 `build.bat`  
- **系统 API**：Win32 PE、`LoadLibraryExW`、SEH、`psapi`  
- **QScintilla**：见 `third_party/README_QScintilla.md`  

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

可执行文件：`build\Release\ModelValidator.exe`（或 `dist\ModelValidator.exe`）。若 `dist` 被占用无法覆盖，请直接运行 `build\Release` 下新编译产物。

样例模型包：`dist\sample_models\<型号名>\{include,lib,models}\`。

### 3.1.1 换机部署（拷贝 dist）

整夹拷贝 `dist/` 即可。目标机说明见 **`dist/README.md`**：

- 缺 VC 运行库 → `dist/prerequisites/vc_redist.x64.exe`
- 需本机编译 Harness → `dist/prerequisites/install_build_tools.bat`

```powershell
powershell -ExecutionPolicy Bypass -File .\deploy\prerequisites\download_prerequisites.ps1
```

### 3.1.2 样例矩阵冒烟（无 GUI）

对全部 `sample_models` 跑静态 + Release 加载 + 短生命周期 + 双 Create（及部分多线程轻测）：

```powershell
cmake --build build --config Release --target SampleModelsMatrix
$env:PATH = "<Qt5>\bin;" + $env:PATH
.\build\Release\SampleModelsMatrix.exe .\dist\sample_models
```

输出：`dist/sample_models_matrix.csv`、控制台配对头冲突结果。静态-only 另有目标 `StaticPrecheckSmoke`。

### 3.2 准备第三方模型包（固定目录结构）

```text
YourModelPackage\
  include\          # 头文件 .h / .hpp
  lib\              # 导入库 / 静态库 .lib
  models\           # 模型动态库 .dll（授权建议与 dll 同级）
```

加载使用 `LOAD_WITH_ALTERED_SEARCH_PATH`。

### 3.3 基本操作流程

1. **添加型号** → 浏览模型包根目录（须含 `include/lib/models`）  
2. **勾选头文件** → 参与编译与冲突分析  
3. **编写 UserMain**（Create/Init → Step → Destroy；轨迹需 `RecordTrajectoryPoint`）  
4. **配置随机变量 `R.*`**  
5. **编译当前 / 全部** → 输出到 `TestModel\<型号名>\`  
6. **一键预检** → 静态（含头冲突）+ Release DLL 加载 + 已编译项的压测/并发/多对象等；结束后弹出总览并同步各导航页  
7. **专项测试**：性能/内存/轨迹、多型号并行、多线程、单线程多对象（含跨型号）  
8. **导出 HTML 报告**  

会话：关闭写入 `exe同级/session/last_session.json`；启动可询问还原。若一键预检异常退出，可查看同级 `precheck_crash_trail.txt` 最后阶段。

### 3.4 界面分区一览

- **预检控制**：一键预检、导出报告  
- **状态 Badges** / **操作流程条** / **功能导航**  
- **型号与 UserMain**、**测试工作区**、**底部日志**  
- 性能页含 **内存上限(MB)**（默认 256；0=不限制）

### 3.5 样例模型（`dist/sample_models` 测试用例包）

由 `sample_models/build_testcase_models.ps1` 生成，与 `dist/模型集成问题.xlsx`「测试用例索引」对应：

- `ReleaseOnly_A/B` — 仅 Release；实测 `D=FAIL`  
- `HeaderClash_A/B` — 头冲突；成对冲突 FAIL  
- `ThreadUnsafe_A/B` — 非线程安全 / 全局单例；同句柄可检出，mt SEH 难必现  
- `SingleInstance_A/B` — 单槽位越界；双 Create 未必 SEH  
- `SharedKernel_A/B` — 共享映射干扰；需成对同进程  
- `CrtMd_A` / `CrtMt_B` — CRT 对照；PE 差异，非跨堆专项  
- `MemLeak_A` — 故意泄漏；压测可见内存增长  
- `SehFault_A/B` — 故意 SEH；Step/Init 异常可捕获  
- `TcPass_A/B/C` — 干净对照；单包宜 PASS，A+C 头可共存  

历史演示包名（`CompatibleWeapon*` / `ConflictWeapon*` / `PrecheckDemoWeapon` 等）若仍在树中，用法与上列同类，优先以 `sample_models` 测试包为准。

---

## 4. 功能实现对照

### 4.1 核心能力

- **模型包扫描** — `PackageScanner`  
- **头文件规范与冲突** — `HeaderAnalyzer`（无危险 `std::regex` 全量扫描）  
- **LIB / PE** — `LibAnalyzer`、`PeAnalyzer`（含文件尺寸保护）  
- **动态加载 + SEH** — `DllLoader`、`SehHelper`（UTF-8 路径；Load/Free SEH）  
- **UserMain / 多对象 Harness** — `UserCodeHarness`、`MultiObjectHarness`  
- **性能压测** — `PerfProfiler`（次数封顶、内存上限、PrivateUsage）  
- **多型号 / 多线程** — `ConcurrencyTester`  
- **单线程多对象 / 跨型号交错** — `SingleThreadMultiObjectTester`、`FleetSingleThreadMultiObjectTester`  
- **预检总览** — `PrecheckSummary`（`dll_load` 区分 SKIP）  
- **HTML / 会话 / UI** — `ReportGenerator`、`SessionStore`、`BusyOverlayWidget`、`MainWindow`  

### 4.2 需求编号对照（历史 F1–F13）

- **F1–F3** 依赖/架构/CRT/导出 → 一键预检 + PE 页  
- **F4–F6** 安全加载与符号 → 一键预检对 **Release DLL**；Debug 见 SKIP  
- **F7–F9** 耗时/内存/实时性 → 性能压测 / 内存监测  
- **F10–F11** 参数与曲线 → 随机变量 + Charts；二维轨迹  
- **F12** 坐标一致性 → `FunctionalVerifier` + 试跑采集  
- **F13** 报告导出 → 预检报告页 + HTML  

---

## 5. 目录结构

```text
ModelPrecheck/
├── README.md
├── AGENTS.md
├── build.bat
├── CMakeLists.txt
├── third_party/
├── src/
│   ├── core/
│   ├── utils/
│   └── ui/
├── sample_models/               # 源码 + build_testcase_models.ps1
├── tests/                       # StaticPrecheckSmoke、SampleModelsMatrix…
├── deploy/prerequisites/
└── dist/                        # 安装输出、模型集成问题.xlsx、sample_models/
```

运行时旁路（exe 同级）：

```text
TestModel/<型号名>/
session/last_session.json
precheck_crash_trail.txt         # 一键预检阶段轨迹（异常闪退时查阅）
sample_models/...
```

---

## 6. 二次开发提示

### 6.1 扩展模型接口结构体

修改 `src/utils/SehHelper.h`；同步样例头文件与调用约定。

### 6.2 扩展 UserMain / 多对象 / 轨迹

- `UserCodeHarness::DefaultUserMainTemplate()`  
- `MultiObjectHarness::DefaultUserMultiObjectTemplate()`  
- 改签名后重新编译 Harness  

### 6.3 扩展校验与报告

- `FunctionalVerifier::VerifyTrajectory`  
- `ReportGenerator::GenerateHtml` / `GenerateFleetHtml`  
- 集成问题表与实测：更新 `dist/模型集成问题.xlsx` 后可同步本节 0.x  

### 6.4 会话与 UI

- `SessionStore` / `SessionRestoreDialog`  
- BusyOverlay：`runBlocking` 在工作线程跑任务，避免在 `setBusyText` 中深度 `processEvents`  

### 6.5 强制约束（见 AGENTS.md）

1. 包含 `<windows.h>` 前定义 `NOMINMAX`  
2. DLL 调用经 SEH（`SehHelper`）  
3. 勿在头文件引入 `QT_CHARTS_USE_NAMESPACE`；Charts 仅放 `.cpp`  
4. 头文件冲突分析避免对大文件使用 MSVC 易栈溢出的 `std::regex`  
5. Release 宿主不要对 `/MDd` Debug 模型 DLL 做进程内 `LoadLibrary`（用 SKIP 或 Debug 宿主）  
