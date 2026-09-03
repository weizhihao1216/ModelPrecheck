# 第三方武器模型 DLL 集成预检工具 (Model Validator)

本工具面向仿真引擎（如 XSIM）集成前的第三方武器模型 C++ 动态库（DLL）预检，对模型包做静态检查、受控加载、UserMain / 多对象脚本编译运行，以及性能、并发、轨迹与报告输出。

---

## 0. 模型集成问题对照（来源：集成问题表）

下表对应历史「模型集成问题」清单。工具侧能力以**能否发现 / 隔离 / 复现 / 报告**为主；厂家缺库、隐含依赖等根因仍需模型侧整改。

### 0.1 已解决（工具可检测或已提供对策）

| 类型 | 现象 | 场景 / 原因 | 工具操作流程 | 功能实现 | 实现方式 |
| :--- | :--- | :--- | :--- | :--- | :--- |
| 静态（构建/链接配置） | 编译 Debug 报错、Release 成功 | 厂家常忽略或不提供 Debug 链接库 | 添加型号 → 选模型包 → **一键预检**；在「DLL 文件与依赖检查 / LIB 检查」看 Debug·Release 包与 CRT | 扫描包内 Debug/Release 产物；识别 MD/MT CRT；报告标明构建配置 | `PackageScanner` 递归找 `.dll/.lib`；`PeAnalyzer` 判架构/CRT；`LibAnalyzer` 查导入库；`MainWindow::precheckOneModel` 分配置汇总 |
| 静态+动态（头文件/符号冲突） | 同厂家多模型集成后偶发崩溃 | 头文件结构体重名、无命名空间隔离，合进同一模型库易崩 | 多型号都加进列表并勾选头文件 → **一键预检** → 看头文件冲突表 / 报告「跨型号头文件冲突」 | 同包及跨型号头文件集合冲突分析；导出声明与 PE 导出交叉比对 | `HeaderAnalyzer::AnalyzeHeaderSet` / `VerifyConsistency`；报告写入 `ReportGenerator` Fleet 章节 |
| 动态（多线程安全） | XSIM 勾选多线程后崩溃 | 模型未做线程安全 | 编译型号 →「多线程稳定性」选型号与线程数 → **执行多线程测试**；或一键预检中自动跑 | 多线程各跑独立 UserMain；记录成功/失败/SEH；PASS/WARN/FAIL | `ConcurrencyTester::Run`（MultiThread）；样例 `CompatibleWeaponC/D/E` 用 `thread_local` 作对照 PASS |
| 动态（多实例/状态隔离） | 同一武器发射多枚崩溃 | 不支持多实例或集成越界 | 「单线程多对象测试」写 `MoCreate/Init/Step/Destroy` → 编译 Harness → **执行基线与交错测试**；或「多型号并行」设实例数 | 逐对象基线 vs 单线程交错推进；偏差、返回码、SEH 与多轨迹对比 | `MultiObjectHarness` + `SingleThreadMultiObjectTester`；`ConcurrencyTester` 多型号并行；样例 `MultiInstanceWeapon` / `ConflictWeapon*` |
| 动态（模型间干扰/内存破坏） | 同厂家不同模型同场景冲突、数据异常 | 共享全局/单例环境参数，或越界写坏公共数据 | 多型号同时添加 → 全部编译 → **多型号并行**；对照 Conflict vs Compatible 样例 | 多路径 Harness 同进程并发；报告串扰/崩溃；轨迹/日志辅助定位 | `ConcurrencyTester::RunMultiModel`；样例 `ConflictWeaponA/B`（易 FAIL）与 `CompatibleWeaponC/D/E`（宜 PASS） |

### 0.2 部分解决（可检测风险，尚无完整专项用例或无法根治）

| 类型 | 现象 | 场景 / 原因 | 工具操作流程 | 功能实现 | 实现方式 | 仍缺什么 |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| 动态（CRT/ABI与跨模块内存） | 初始化路径参数带 `c_str()` 读不到路径 | 双方 CRT 不匹配、堆隔离 | 一键预检 / PE 页查看 **CRT MD/MT**；UserMain 在本工具侧统一用 MSVC 编译 Harness | 报告 CRT 类型；加载失败写入日志 | `PeAnalyzer` CRT 识别；`DllLoader` + SEH；`UserCodeHarness` 本机 `cl` 编 Harness | 不能自动改写厂家 DLL 的 CRT；跨堆传 `std::string` 仍需厂家/引擎约定 |
| 动态（倍速调度/数值稳定性） | 同一枚弹加速到约 50 倍速崩溃 | 固定步长/调用频率，高倍速导致发散或越界 | 「性能压测」设次数与目标 Hz → 看耗时/抖动/实时性判定 | 相对帧预算给出 PASS/WARN/FAIL；曲线展示 | `PerfProfiler` + `ChartViewerWidget` | 尚无「N 倍速想定」专项（故意放大 `dt` / 单帧多次 Step） |
| 环境兼容（依赖/ABI/版本） | 不同电脑 / XSIM5·6 表现不一 | 版本、依赖 DLL、VC 运行库、授权路径、ABI 不一致 | PE 页看导入依赖与架构；授权放 DLL 同级（或 exe 旁备份）；一键预检看缺失依赖 | 缺失 DLL、架构、导出扫描；授权目录提示 | `PeAnalyzer` Import；`LOAD_WITH_ALTERED_SEARCH_PATH`；UI 授权提示 | 无法覆盖全部 XSIM 主程序 ABI 差异与授权加密逻辑 |
| 架构与集成设计（隐含依赖） | 模型从原系统拆出，集成繁琐、用例冗杂 | 仍依赖原系统全局状态、初始化顺序、资源 | 用 **UserMain / Mo\*** 自行编排调用；改名接口可用映射样例；报告标 N/A 未测项 | 用户脚本驱动生命周期；标准 Handle 自动识别；HTML 矩阵 | `UserCodeHarness`、`MultiObjectHarness`、`InterfaceMappingProfile`、`ReportGenerator` | 无法消除厂家未文档化的隐含依赖，需厂家提供独立 API |

### 0.3 问题类型索引

| 问题类型 | 已解决 | 部分解决 |
| :--- | :--- | :--- |
| 静态（构建/链接） | Debug 库缺失的**检测** | — |
| 静态+动态（头文件/符号） | 跨型号头文件冲突检测 | — |
| 动态（多线程） | 多线程 UserMain 稳定性测试 | — |
| 动态（多实例/隔离） | 单线程多对象 + 多型号并行 | — |
| 动态（模型间干扰） | 同进程多型号并发复现 | — |
| 动态（CRT/ABI） | — | CRT 识别与加载失败日志 |
| 动态（倍速/数值） | — | 帧预算压测，无专用倍速用例 |
| 环境兼容 | — | 依赖/架构扫描 + 授权提示 |
| 架构与集成设计 | — | UserMain/Mo\* 灵活编排，根因在厂家 |

---

## 1. 项目概述

桌面端一站式能力：

- 模型包静态检查（头文件 / LIB / PE 依赖与导出）
- 受控动态加载与 SEH 硬件异常隔离
- 可编译用户脚本 `UserMain`（驱动 Init / Step / Destroy）与 `Mo*` 多对象 Harness
- UserMain 性能压测、内存增长监测与实时性判定
- 多型号并行 / 单型号多线程稳定性测试
- 单线程多对象基线/交错推进与状态串扰检测
- 二维经纬度轨迹试跑（UserMain：`RecordTrajectoryPoint`；多对象：`MoStep` 的 `out_lat/out_lon`）
- HTML 预检报告（综合判定矩阵 + 按型号明细；未测项 N/A）
- 代码编辑器：QScintilla（语法高亮、补全、Ctrl+滚轮缩放）

---

## 2. 技术栈与开发环境

| 项 | 说明 |
| :--- | :--- |
| 编译器 | MSVC（推荐 VS2017+ / VS2022），`/utf-8`，`NOMINMAX` |
| UI | Qt 5.x Widgets + Qt Charts + **QScintilla 2.14.1（静态链入）** |
| 构建 | CMake 3.12+；一键脚本 `build.bat`（默认 VS2022 x64） |
| 系统 API | Win32 PE、`LoadLibraryExW`、SEH、`psapi` |
| QScintilla | 见 `third_party/README_QScintilla.md`（GPL-3 / 商业双许可） |

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

样例 DLL 一般在：`build\sample_models\<型号名>\Release\`。

### 3.2 准备第三方模型包

```text
YourModelPackage\
  WeaponModel.h          # 或厂商头文件
  YourModel.dll
  （可选）.lib / 授权文件或授权文件夹
```

**授权文件：** 放在第三方模型 DLL 同级目录；不可用时可复制一份到本 exe 同级目录。加载使用 `LOAD_WITH_ALTERED_SEARCH_PATH`。

### 3.3 基本操作流程

1. **添加型号** → 填名称 → 浏览模型包根目录  
2. **勾选头文件** →「刷新列表」后勾选参与编译的 `.h`  
3. **编写 UserMain**（默认 `Model_Init` → 循环 `Model_Step` → `Model_Destroy`）  
   - 轨迹试跑需：`out_lat` / `out_lon` + `RecordTrajectoryPoint(out_lat, out_lon)`  
4. **配置随机变量 `R.*`**  
5. **编译当前/全部型号** → 生成 `TestModel\<型号名>\<型号名>.dll`（Harness）  
6. **一键预检全部型号** → 静态 PE / 头文件 / LIB / 加载 + 已编译项的压测/并发等  
7. **专项测试**（需已编译，除非说明）  
   - 性能压测 / 二维轨迹试跑  
   - 多型号并行 / 多线程  
   - 单线程多对象：写 `MoCreate/MoInit/MoStep/MoDestroy`（`MoStep` 输出 `out_lat/out_lon`）→ 编译 → 基线与交错测试  
8. **预检报告**查看 / 导出 HTML  

### 3.4 界面分区一览

| 区域 | 作用 |
| :--- | :--- |
| 预检控制 | 一键预检、导出报告 |
| 状态 Badges | 头文件 / LIB / DLL 通过情况 |
| 左侧型号面板 | 型号、包路径、头文件、UserMain（QScintilla）、随机变量、编译 |
| 静态检查页 | 头文件 / LIB / PE / 加载 |
| 性能 / 轨迹 | 压测曲线；UserMain 二维 Lon×Lat |
| 多型号并行 | 实例数与并行结果 |
| 多线程测试 | 同型号多线程 UserMain |
| 单线程多对象 | Mo\* 编辑、基线/交错、多轨迹（MoStep 经纬度） |
| 预检报告 | HTML 总报告 |
| 底部日志 | 过程日志 |

### 3.5 样例模型说明

| 样例 | 用途 |
| :--- | :--- |
| `StandardWeapon` | 标准合规（偏单例） |
| `FaultyWeapon` | 故意故障 |
| `MultiInstanceWeapon` | Handle 多实例 + 多对象示例 |
| `ConflictWeaponA/B` | 共享内核冲突，多型号易 FAIL |
| `CompatibleWeaponC/D/E` | `thread_local`，多线程/多型号宜 PASS |
| `PrecheckDemoWeapon` | 统一演示 Handle API |
| `RenamedInterfaceWeapon` | 改名接口 / 映射样例 |

---

## 4. 功能实现对照

| 能力 | 说明 | 主要实现 |
| :--- | :--- | :--- |
| 模型包扫描 | 递归发现 `.h` / `.lib` / `.dll` | `PackageScanner` |
| 头文件规范与冲突 | 编码、`extern "C"`、跨型号冲突 | `HeaderAnalyzer` |
| LIB / PE | 架构、CRT、Import/Export | `LibAnalyzer`、`PeAnalyzer` |
| 动态加载 + SEH | 安全加载、硬件异常隔离 | `DllLoader`、`SehHelper` |
| UserMain 编译运行 | Harness 生成、`cl` 编译、`RunOnce` | `UserCodeHarness` |
| 轨迹采集 | UserMain：`RecordTrajectoryPoint`；多对象：`MoStep` 的 `out_lat/out_lon` | `UserCodeHarness`、`MultiObjectHarness` |
| 性能压测 | 耗时、抖动、Working Set、帧预算 | `PerfProfiler`、`ChartViewerWidget` |
| 二维轨迹 UI | Lon×Lat 折线 + 点表 | `TrajectoryViewWidget` |
| 多型号 / 多线程 | 并发 UserMain | `ConcurrencyTester` |
| 单线程多对象 | 基线/交错、串扰与 SEH | `MultiObjectHarness`、`SingleThreadMultiObjectTester` |
| 接口映射 | 改名接口 JSON 映射（legacy/样例） | `InterfaceMappingProfile` |
| HTML 报告 | 单报告 / Fleet 矩阵 | `ReportGenerator` |
| 代码编辑器 | 高亮、补全、缩放、可拖拽高度 | `CppCodeEditor`（QScintilla） |
| 等候动画 | 编译/预检/测试非阻塞转圈 | `BusyOverlayWidget` |
| 主界面编排 | 型号管理、导航、一键预检 | `MainWindow` |

需求编号对照（历史 F1–F13）：

| 编号 | 功能 | 现状 |
| :--- | :--- | :--- |
| F1–F3 | 依赖扫描、架构/CRT、导出 | 一键预检 + PE 页 |
| F4–F6 | 安全加载、符号、加载内存 | 一键预检对包内 DLL |
| F7–F9 | 耗时/内存/实时性 | 性能压测 |
| F10–F11 | 参数与曲线 | 随机变量 + Charts；二维轨迹 |
| F12 | 坐标一致性 | `FunctionalVerifier` + 试跑采集 |
| F13 | 报告导出 | 预检报告页 + 导出 HTML |

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
│   ├── core/                    # Pe/Header/Lib/Package、Harness、Tester、Report
│   ├── utils/                   # SehHelper、MemoryUtils
│   └── ui/                      # MainWindow、Charts、Trajectory、CppCodeEditor…
├── sample_models/
└── docs/                        # 主题预览等
```

---

## 6. 二次开发提示

### 6.1 扩展模型接口结构体

修改 `src/utils/SehHelper.h` 中参数/输出结构及函数指针；同步样例头文件与调用约定。

### 6.2 扩展 UserMain / 多对象 / 轨迹

- UserMain 模板：`UserCodeHarness::DefaultUserMainTemplate()`
- 多对象：`MultiObjectHarness` 的 `MoCreate/MoInit/MoStep/MoDestroy`
- 改签名后需重新编译对应 Harness

### 6.3 扩展校验与报告

- `FunctionalVerifier::VerifyTrajectory`
- `ReportGenerator::GenerateHtml` / `GenerateFleetHtml`

### 6.4 强制约束（见 AGENTS.md）

1. 包含 `<windows.h>` 前定义 `NOMINMAX`  
2. DLL 调用经 SEH（`SehHelper`）  
3. 勿在头文件引入 `QT_CHARTS_USE_NAMESPACE`；Charts 仅放 `.cpp`
