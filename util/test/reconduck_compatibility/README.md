# ReconDuck Windows 捕获兼容性回归

测试直接编译实际源代码、抽取的实际函数或实际事务头文件。Global Hook 和等待启动测试使用隔离的系统接口，不修改本机 AppInit 注册表。

当前测试环境：Windows、Python 3、MSVC 14.44.35207、Windows SDK 10.0.26100.0、LLVM，以及仓库自带的 Qt 5。脚本中的默认工具链路径对应本次开发环境；迁移到其他机器时需要调整路径。

在仓库根目录运行：

```powershell
python util/test/reconduck_compatibility/run_export_detours.py
python util/test/reconduck_compatibility/run_export_failures.py
python util/test/reconduck_compatibility/d3d11_tests.py
python util/test/reconduck_compatibility/d3d11_syntax.py
python util/test/reconduck_compatibility/manual_launch_tests.py
python util/test/reconduck_compatibility/manual_launch_lifecycle.py
python util/test/reconduck_compatibility/global_hook_tests.py
```

Qt 界面和进程枚举编译检查：

```powershell
python util/test/reconduck_compatibility/manual_launch_ui_checks.py --compiler D:/Code/VS2026/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64/cl.exe
python util/test/reconduck_compatibility/manual_launch_ui_checks.py --arch x86 --compiler D:/Code/VS2026/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x86/cl.exe
```

完成相应架构的 Development 构建后，再验证真实 D3D11/WARP 捕获与回放：

```powershell
python util/test/reconduck_compatibility/d3d11_warp_smoke.py x64
python util/test/reconduck_compatibility/d3d11_warp_smoke.py x86
```

该测试先缓存 D3D11 导出地址，再加载 ReconDuck；验证注册表层限制标志、仅返回上下文/交换链的调用、真实 Present、捕获文件和回放后的全部 4096 个像素。临时产物位于外层工作区的 `build-temp/`。

验证目标应用自带旧版 x64 VC 运行库时，完成 x64 构建后运行：

```powershell
python util/test/reconduck_compatibility/runtime_compatibility.py --runtime-dir '<旧版 x64 VC 运行库目录>'
```

脚本把运行库复制到 `build-temp/` 独立测试。baseline 进程预期发生访问异常；读取实际工程兼容定义的 configured 进程和加载真实新 DLL 的 load 进程必须成功。此回归覆盖 VS 2022 17.10 起的 mutex 构造与旧版 MSVCP140 不兼容问题。

Detours 测试包含并发安装和静止后的卸载。现有应用 API 明确要求 `RemoveHooks` 在尚未发生图形 API 工作时调用，不能将这些结果解释为支持绘制过程中热卸载、重装。

真实 AppInit 辅助进程注入和具体启动器应用仍需在具有相应权限、组件齐全的环境中验证；隔离恢复测试不等价于这部分系统集成测试。
