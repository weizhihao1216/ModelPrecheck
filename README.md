# 第三方武器模型 DLL 集成预检工具 (Model Validator)

## 1. 项目概述

本工具为面向仿真引擎集成的专用桌面自动化预检系统，旨在对第三方提供的武器模型 C++ 动态库 (DLL) 进行**静态 PE 结构检查**、**受控安全动态加载与 SEH 崩溃隔离**、**高精度性能压测与内存泄漏监测**、**运动轨迹/坐标系逻辑校验**以及**自动化预检报告生成**。

---

## 2. 技术栈与开发环境

* **开发环境**：Visual Studio 2017 (MSVC v141 Toolset)
* **界面框架**：Qt 5.11 (x64) (`D:\HR\DEV\ThirdParty\Qt5.11\vc140\x64`)
* **图表组件**：`Qt5::Charts` (`QChart`, `QChartView`, `QLineSeries`)
* **构建系统**：CMake 3.12+ (指定 `Visual Studio 15 2017 Win64` 生成器)
* **系统 API**：Win32 PE Parsing API, `psapi.lib` (Process Memory Counters), Structured Exception Handling (SEH `__try / __except`)
* **关键编译项**：`add_compile_definitions(NOMINMAX)`, `/utf-8`

---

## 3. 已实现需求与功能模块对照

| 需求模块 | 编号 | 功能说明 | 对应实现源码文件 |
| :--- | :--- | :--- | :--- |
| **静态环境检查** | **F1** | 依赖项扫描 (Import Directory 扫描，丢失 DLL 标红) | [`src/core/PeAnalyzer.h`](file:///d:/HR/Game/ModelPrecheck/src/core/PeAnalyzer.h) / [`PeAnalyzer.cpp`](file:///d:/HR/Game/ModelPrecheck/src/core/PeAnalyzer.cpp) |
| | **F2** | 二进制属性提取 (x64/x86 位数、MD/MT CRT 链接识别) | `PeAnalyzer.cpp` (PE DOS/NT Header Parsing) |
| | **F3** | 导出符号校验 (Export Directory 比对 `Init/Step/Destroy`) | `PeAnalyzer.cpp` |
| **动态加载测试** | **F4** | 安全加载试验 (`LoadLibraryExW` + SEH 保护) | [`src/utils/SehHelper.cpp`](file:///d:/HR/Game/ModelPrecheck/src/utils/SehHelper.cpp) / [`src/core/DllLoader.cpp`](file:///d:/HR/Game/ModelPrecheck/src/core/DllLoader.cpp) |
| | **F5** | 接口指针绑定 (`GetProcAddress` 动态绑定与自定义别名映射) | `DllLoader.cpp` / [`src/ui/PropertyEditorWidget.cpp`](file:///d:/HR/Game/ModelPrecheck/src/ui/PropertyEditorWidget.cpp) |
| | **F6** | 静态内存开销 (记录 DLL 加载前后 Working Set 差值) | [`src/utils/MemoryUtils.cpp`](file:///d:/HR/Game/ModelPrecheck/src/utils/MemoryUtils.cpp) |
| **性能压力评估** | **F7** | 步进耗时统计 (1k~100k 步进, 纳秒级记录 Min/Max/Avg/Jitter) | [`src/core/PerfProfiler.h`](file:///d:/HR/Game/ModelPrecheck/src/core/PerfProfiler.h) / [`PerfProfiler.cpp`](file:///d:/HR/Game/ModelPrecheck/src/core/PerfProfiler.cpp) |
| | **F8** | 内存增长监测 (连续步进采样，估算 10,000 步内存泄露速率) | `PerfProfiler.cpp` / `MemoryUtils.cpp` |
| | **F9** | 实时性评估 (对比 50Hz/100Hz/1000Hz 帧预算，PASS/WARN/FAIL) | `PerfProfiler.cpp` |
| **逻辑与坐标校验** | **F10** | 交互式参数输入 (初始经纬高、速度、姿态角及别名映射表) | `PropertyEditorWidget.cpp` |
| | **F11** | 数据记录与曲线 (输出表格与 QtCharts 耗时/内存/轨迹图) | [`src/ui/ChartViewerWidget.cpp`](file:///d:/HR/Game/ModelPrecheck/src/ui/ChartViewerWidget.cpp) |
| | **F12** | 坐标系一致性 (NaN检测、物理边界、弧度/角度判断、跳变识别) | [`src/core/FunctionalVerifier.cpp`](file:///d:/HR/Game/ModelPrecheck/src/core/FunctionalVerifier.cpp) |
| **报告生成导出** | **F13** | 预检报告导出 (自动生成自包含样式 HTML 报告并支持导出) | [`src/core/ReportGenerator.cpp`](file:///d:/HR/Game/ModelPrecheck/src/core/ReportGenerator.cpp) |

---

## 4. 目录架构说明

```
d:/HR/Game/ModelPrecheck/
├── README.md                            # 本项目技术规格、需求及二次开发文档
├── AGENTS.md                            # AI 智能体项目上下文与指令
├── CMakeLists.txt                       # 根 CMake 构建文件 (支持 VS2017 + Qt 5.11)
├── src/
│   ├── main.cpp                         # 程序主入口
│   ├── core/                            # 预检引擎核心逻辑层
│   │   ├── PeAnalyzer.h / .cpp          # 静态 PE 格式与依赖关系分析器
│   │   ├── DllLoader.h / .cpp           # 安全 LoadLibrary 与 SEH 符号绑定器
│   │   ├── PerfProfiler.h / .cpp        # 多线程性能与内存泄露分析器
│   │   ├── FunctionalVerifier.h / .cpp  # Trajectory 轨迹平滑度与坐标系/单位校验器
│   │   └── ReportGenerator.h / .cpp     # 预检报告 HTML 渲染与导出器
│   ├── utils/                           # Win32 底层工具库
│   │   ├── MemoryUtils.h / .cpp         # Win32 Process Memory (psapi.lib) 内存统计
│   │   └── SehHelper.h / .cpp           # Windows 原生 SEH (__try/__except) 崩溃捕获器
│   └── ui/                              # Qt 5.11 UI 界面层
│       ├── MainWindow.h / .cpp          # 主窗口界面 (Tab页, 状态Badges, 仪表盘)
│       ├── PropertyEditorWidget.h / .cpp# 参数设置与接口符号映射面板
│       ├── ChartViewerWidget.h / .cpp   # QtCharts 耗时/内存/轨迹图表组件
│       └── LogConsoleWidget.h / .cpp    # 格式化彩标控制台日志组件
└── sample_models/                       # 开箱即用的测试 DLL 源码
    ├── StandardWeapon/                  # 满足合规标准的样本模型 (StandardWeapon.dll)
    └── FaultyWeapon/                    # 故意制造内存泄漏与除零崩溃的测试样本 (FaultyWeapon.dll)
```

---

## 5. 构建与编译步骤

在 Windows PowerShell 中执行以下命令：

```powershell
# 1. 配置 CMake 工程 (生成 Visual Studio 2017 64位解决方案)
cmake -G "Visual Studio 15 2017 Win64" -B build

# 2. 编译 Release 目标
cmake --build build --config Release
```

编译生成文件存放于 `build/Release/`：
- `ModelValidator.exe`
- `StandardWeapon.dll`
- `FaultyWeapon.dll`

---

## 6. 二次开发与需求扩展指南 (Extension Guide)

下次需扩展功能时，可参考以下文件进行扩充：

### 6.1 扩充新的模型接口标准或结构体
- 在 [`src/utils/SehHelper.h`](file:///d:/HR/Game/ModelPrecheck/src/utils/SehHelper.h) 中修改 `WeaponModelParams` / `WeaponModelOutput` 结构体定义及函数指针原型。
- 在 [`src/core/DllLoader.cpp`](file:///d:/HR/Game/ModelPrecheck/src/core/DllLoader.cpp) 中更新调用包装函数。

### 6.2 增加新的坐标系或物理逻辑校验规则
- 在 [`src/core/FunctionalVerifier.cpp`](file:///d:/HR/Game/ModelPrecheck/src/core/FunctionalVerifier.cpp) 的 `VerifyTrajectory` 函数中增加自定义规则（例如加速度阈值检查、姿态角变化率限制等）。

### 6.3 扩充报告导出格式 (如 PDF / JSON)
- 在 [`src/core/ReportGenerator.cpp`](file:///d:/HR/Game/ModelPrecheck/src/core/ReportGenerator.cpp) 中新增 `GenerateJson()` 或使用 Qt `QPrinter` 打印成 PDF。
