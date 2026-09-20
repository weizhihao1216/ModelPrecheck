# ModelValidator 换机部署 — 环境依赖说明

把整个 `dist` 文件夹拷到目标电脑后，先双击运行 **`ModelValidator.exe`**。  
若程序打不开，或「编译」相关功能失败，按下面对照说明安装对应程序（安装包在 `prerequisites/` 目录）。

---

## 缺什么 → 点什么

### 1. 程序无法启动 / 提示缺少运行库 DLL

**典型现象**

- 双击 `ModelValidator.exe` 后立刻闪退  
- 弹窗提示找不到 `vcruntime140.dll`、`msvcp140.dll`、`vcruntime140_1.dll` 等  

**请双击安装**

`prerequisites\vc_redist.x64.exe`

- 按提示完成安装（可能需要管理员权限）  
- 装完后重新打开 `ModelValidator.exe`  
- 仅运行界面、加载已有 DLL、查看报告时，一般装这一项即可  

---

### 2. UserMain / 多对象「编译」失败

**典型现象**

- 日志出现「未找到 Visual Studio C++ 编译环境」  
- 日志出现「未找到 vcvars64.bat」  
- 日志出现找不到 `cl.exe` 或 `cl.exe` 退出失败  

**推荐：一键勾选 C++ 工具集**

双击 `prerequisites\install_build_tools.bat`

**或手动安装**

1. 双击 `prerequisites\vs_BuildTools.exe`  
2. 勾选工作负载 **「使用 C++ 的桌面开发」**（至少含 MSVC 工具集、Windows SDK）  
3. 安装完成后重启电脑（或至少重新打开本工具）  
4. 再试「编译当前型号 / 编译全部型号」  

> `vs_BuildTools.exe` 是微软官方引导程序，安装需联网，组件体积较大（数 GB）。  
> 若本机已安装完整 Visual Studio（带「使用 C++ 的桌面开发」），则不必再装 Build Tools。

---

## 安装顺序建议

1. 先装 `vc_redist.x64.exe`（几乎所有换机场景都需要）  
2. 只有要用 UserMain / 多对象 **编译** 时，再运行 `install_build_tools.bat`（或手动装 `vs_BuildTools.exe`）  
3. 再运行 `ModelValidator.exe`  

---

## prerequisites 目录文件说明

- **`vc_redist.x64.exe`** — Visual C++ 运行库（x64），解决缺 DLL 无法启动  
- **`vs_BuildTools.exe`** — Visual Studio Build Tools 安装引导程序  
- **`install_build_tools.bat`** — 自动勾选 C++ 工具集并启动上述引导程序  
- **`README.md`** — 与本文相同的说明副本  

---

## 不需要再装的情况

- 目标机已装过较新的 Visual C++ Redistributable x64 → 可跳过 `vc_redist.x64.exe`  
- 目标机已装 Visual Studio 2017/2022 且含 C++ 桌面开发 → 可跳过 Build Tools  
- 只做静态检查 / 加载现成 Harness DLL、从不点「编译」→ 通常只需 VC++ 运行库  

---

## 其他注意

- 系统需为 **64 位 Windows**  
- 第三方模型包、授权文件请按工具要求一并拷贝（`include` / `lib` / `models`）  
- `session` 目录为会话缓存，可不拷；不拷则相当于全新启动  
