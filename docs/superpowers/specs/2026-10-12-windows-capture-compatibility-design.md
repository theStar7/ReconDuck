# ReconDuck Windows 捕获兼容性改进设计

状态：用户已确认；实现和回归验证已完成。实际结果见 `../2026-10-12-reconduck-compatibility-verification.md`。

## 目标与依据

以本地提交 `022c5c76a` 为起点，参考 RenderDuck 的导出函数 Detours 拦截、D3D11 包装和等待手动启动流程，提高 Windows 应用的捕获兼容性。保留 ReconDuck 命名、当前 1.47 基线和已有启动线程 DLL 注入实现。

参考资料：

- [RenderDuck 技术文章](https://www.floraison.io/2026/09/15/)，2026-09-15。
- [RenderDuck 源码](https://github.com/LesFloraison/RenderDuck)，本地已取得的参考提交 `dcbb5b7ba7e64a785400f263c1d4bad04b8e372b`。
- 参考提交 `8d2ced6d1` 的 D3D11、DXGI 和 NVAPI 兼容修正，以及 `dcbb5b7ba` 的 NVAPI/NvCamera 冲突修正。后两项晚于文章，不能只按文章中的初版实现移植。

## 方案选择

1. **选择性移植，推荐。** 在现有挂钩框架和捕获界面增加所需机制，保留当前上游更新和注入实现。需要适配 Detours 生命周期和两版界面的差异，但改动范围明确。
2. 仅修复 D3D11 包装。改动最小，但无法解决程序缓存原始导出地址后绕过 IAT 的问题，也不提供平台启动流程。
3. 整体合并 RenderDuck 分支。现有基线为 1.47，参考分支为 1.44，且存在大量品牌和路径变化，整体合并会引入无关回退和冲突，不采用。

## 1. 导出函数入口拦截

保留当前 IAT 和 GetProcAddress 拦截，在已注册的图形 API 导出入口增加 Detours，使注入前缓存的函数地址也能经过现有捕获函数。业务捕获函数继续通过 trampoline 调用原实现。

涉及 `reconduck/os/win32/win32_hook.cpp`、新增 `reconduck/3rdparty/detours/` 和 Windows 核心工程的源文件、包含路径及 filters 配置。保留第三方版权和 MIT 许可，记录移植来源。

约束：

- 对加载器函数和 NVAPI 保留原有 IAT/GetProcAddress 路径；不为 kernel32、kernelbase、libraryloader API sets、nvapi 和 nvapi64 安装入口 Detours。
- NvCamera 模块保留其原生驱动查询，不改写它的导入表。
- 登记并保持 trampoline，重复扫描、GetProcAddress 查询和手动挂钩不得把它覆盖成已经被打补丁的入口地址。
- 同一导出入口不得重复安装或形成递归链；安装失败保留可用的原始函数和现有 IAT 路径。
- 卸载必须先恢复已安装的入口再释放对应状态；失败需保留错误信息，避免虚报恢复成功。
- 遵守现有 `RemoveHooks` API 的静止期约定；不支持应用持续调用图形 API 时热卸载、重装。Detours 无法重写调用栈或寄存器中已缓存的函数指针。
- 导入目录扫描增加映像范围和描述符数量检查，避免越界访问。
- 跨平台工程不引入 Windows Detours 编译依赖。

## 2. D3D11、DXGI 和 NVAPI 兼容性

调整 `reconduck/driver/d3d11/d3d11_hooks.cpp`：设备创建成功后，`D3D11_CREATE_DEVICE_PREVENT_ALTERING_LAYER_SETTINGS_FROM_REGISTRY` 不再使捕获包装被跳过。保持原始 HRESULT 和设备创建参数语义，核对设备、立即上下文和交换链的可选输出及引用计数。

调整 `reconduck/driver/dxgi/dxgi_wrapped.cpp` 和 `reconduck/driver/ihv/nv/nvapi_hooks.cpp`：对未包装设备正确转发 DXGI 查询；NVAPI 扩展查询正确处理空参数，保留必要的驱动内部原生调用。加入有限的 D3D11 创建/呈现失败诊断，记录移除原因，避免改变 API 返回值或无限刷日志。

## 3. 等待启动与 Global Hook 生命周期

在 `qreconduck/Windows/Dialogs/CaptureDialog.{cpp,h,ui}` 增加“等待启动”入口，面向本地 Windows 捕获：选择实际渲染 EXE，进入等待，再由用户通过 Steam、Epic 或其他启动器启动应用。

扩展 `qreconduck/Code/qprocessinfo.{cpp,h}`，取得父 PID 和可执行文件完整路径。等待开始时记录已存在的进程和控制连接；按规范化后的完整路径识别新目标。实际渲染进程和启动器辅助进程要区分，不能仅凭同目录或同名就批量注入。相关子进程连接需要确认其图形 API/呈现状态。

等待期间显示已发现 PID 和当前状态，支持取消。后台枚举和连接操作不直接更新 UI；线程退出后再释放其共享状态。每个目标的注入重试有上限，目标退出、取消和错误均有明确结束路径。

Global Hook 的启动和清理由 `reconduck/os/win32/win32_process.cpp` 与独立的 `win32_global_hook.h` 管理：组件路径验证、原始注册表值保存、原有 AppInit 条目保留、分步启动失败回滚，以及 x86/x64 辅助进程和管道关闭。启动成功后才发布会话指针；原子活动状态同时覆盖启动、运行、清理和待恢复阶段，界面查询不会阻塞。主窗口在等待线程完成前拒绝退出并请求取消。测试使用隔离的系统接口，避免改动本机全局注册表。

等待启动不适用于远程和 Android 捕获；现有直接启动、附加进程和启动线程注入流程继续可用。等待成功、取消或失败后，只清理本次流程拥有的 Global Hook 会话。

## 验证和验收

1. 先建立回归用例并观察当前行为失败，再移植对应功能。
2. Windows 原生 x86/x64 导出函数测试：预先缓存地址、安装后拦截、trampoline 调用原函数、重复安装、恢复入口、失败回退和多线程调用。
3. D3D11 创建参数测试：注册表层标志、空输出参数、立即上下文、重复包装、创建失败和引用释放；具备完整构建后增加 WARP 设备的捕获与回放测试。
4. 进程筛选测试：完整路径、同名不同路径、已存在进程、启动器辅助进程、真实子进程、PID 生命周期和取消。
5. Global Hook 路径及恢复测试：缺失组件、非法路径、不同注册表视图、启动中途失败、保留原值和重复清理。
6. 重跑已有 x86/x64/CET/跨位数启动线程注入测试，验证命名、工程引用和 Windows 资源。

执行时已找到 `D:/Code/VS2026` 的完整 MSVC 工具链，使用 MSVC 14.44 和 Windows SDK 10.0.26100.0 验证完整构建与真实 WARP 捕获回放。交付报告记录各项实际结果，不将专项测试或 WARP 测试外推为所有应用均可捕获。
