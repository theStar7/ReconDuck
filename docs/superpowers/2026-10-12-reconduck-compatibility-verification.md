# ReconDuck Windows 捕获兼容性改进结果

2026-10-12。基线提交 `022c5c76a`，保留 ReconDuck 命名、1.47 上游基线及启动线程注入。修改位于当前工作区。

后续修复：针对 Endfield 崩溃记录，复现并修复新 MSVC mutex 构造与游戏自带旧 MSVCP140 的兼容性问题，见 [运行库诊断报告](2026-10-12-endfield-runtime-compatibility.md)。

## 实现

- **导出入口 Detours**：保留 IAT/GetProcAddress 拦截，加入已注册 API 的入口拦截，覆盖注入前缓存的函数指针。登记 trampoline、共享入口和冲突回退，卸载失败保留状态。加载器和 NVAPI 不安装入口补丁，NvCamera 导入保持原生调用，导入描述符扫描受映像范围和目录大小限制。
- **D3D11 包装**：注册表层限制标志不再跳过包装；处理设备、上下文和交换链可选输出及引用释放。补充未包装设备的 DXGI 转发、NVAPI 空参数处理和限量失败日志。创建失败时不读取可能残留的设备输出指针。
- **等待启动**：选择真实渲染 EXE，点击 `Wait for Launch`，再通过启动器启动应用。按完整路径和进程创建时间识别新目标，相关子进程须报告受支持且正在呈现的 API；每个目标最多注入三次。取消、恢复失败和窗口关闭都有明确状态。
- **Global Hook 恢复**：保留原有 AppInit 值及条目，快照两种注册表视图，验证辅助进程就绪，失败回滚并保留可重试的恢复状态。Start/Stop 串行执行；活动查询通过原子状态覆盖启动、运行和清理，界面不会等待该锁。

Detours 的事务记录改用 VirtualAlloc/VirtualFree，避免暂停其他线程后争用共享堆。第三方来源、许可和这个本地补丁记录在 `reconduck/3rdparty/detours/README.md`。

## 验证结果

工具链：MSVC 14.44.35207、Windows SDK 10.0.26100.0、LLVM、仓库自带 Qt。

| 验证 | 结果 |
|---|---|
| x64 核心 DLL、命令行、GUI 完整编译与链接 | 通过 |
| x86 核心 DLL、命令行完整编译与链接 | 通过 |
| x86/x64 Global Hook shim 构建 | 通过 |
| 双架构真实 D3D11/WARP 捕获及回放 | 通过，逐一核对全部 4096 个像素 |
| 缓存导出、trampoline、冲突回退、共享堆锁、并发安装、100 次静止期重装 | 双架构通过 |
| Detours 安装/卸载失败、事务所有权、别名和模块引用 | 20 项通过 |
| D3D11/DXGI/NVAPI 输出、错误与诊断 | 42 项通过 |
| 实际等待线程、取消和实际关闭方法的隔离运行回归 | 24 项通过 |
| 实际 Qt 源文件、UI/metaobject 编译及真实子进程信息 | 双架构通过，包含 MainWindow.cpp |
| Global Hook 原值保存、双视图、回滚、就绪、恢复重试、并发查询 | 双架构通过 |
| 原有启动线程注入、CET、AVX 状态和跨位数委托 | 通过 |
| 命名、当前工程引用、Windows 资源、CI 路径和差异空白检查 | 通过 |

真实 WARP 测试先缓存 D3D11 导出，再加载新 DLL；覆盖注册表层标志、仅上下文/仅交换链输出、Present、生成 RDC、打开回放及读取像素。捕获与回放并非模拟结果。

红测也验证了原有缓存地址绕过拦截、共享堆分配死锁、Begin 失败未释放事务所有权，以及取消/关闭期间遗留恢复状态的问题；相应修复后的测试通过。

命名迁移的旧 `verify_reconduck.py` 要求 XML 节点数保持不变，新增源文件引用会使这个历史条件失败。本次用外层 `build-temp/check_compatibility_rename.py` 重用其命名/API/资源相关验证并检查当前工程引用，不将旧节点数限制作为兼容性改进的验收条件。

## 产物与复现

- GUI：`x64/Development/qreconduck.exe`。
- 核心、命令行及 shim：`x64/Development/` 与 `Win32/Development/`。
- 真实测试捕获：外层 `build-temp/warp-smoke-x64/warp_capture.rdc` 和 `build-temp/warp-smoke-x86/warp_capture.rdc`。
- 构建日志：外层 `build-temp/final-build-x64.log`、`final-build-x86.log`、`cmd-build-x64.log`、`shim-build-x64.log`、`shim-build-x86.log`。
- 回归命令与环境：`util/test/reconduck_compatibility/README.md`。

具体 Steam/Epic 应用的交互等待流程和真实 AppInit 系统注入尚未实测；Global Hook 恢复测试使用隔离接口，不修改本机注册表。现有 `RemoveHooks` API 要求在没有图形 API 工作时卸载，不支持持续绘制期间热卸载、重装。

## 参考

- [RenderDuck 技术文章](https://www.floraison.io/2026/09/15/)。
- [RenderDuck 参考源码](https://github.com/LesFloraison/RenderDuck/tree/dcbb5b7ba7e64a785400f263c1d4bad04b8e372b)，并采用后续 NVAPI/NvCamera 兼容修正，避免直接覆盖当前 1.47 实现。
