# Endfield 注入失败：旧版 MSVCP140 兼容性修复

## 证据与原因

Windows Application 事件在 2026-10-12 17:30:17 和 17:33:45 记录了相同崩溃：`Endfield.exe`、游戏目录的 `MSVCP140.dll` 14.29.30139.0、异常 `0xc0000005`、模块偏移 `0x13020`。

ReconDuck 当前使用 MSVC 14.44 编译。独立 mutex 测试加载游戏自带的运行库副本后，复现了完全相同的异常和偏移。对实际 ReconDuck DLL 的加载测试返回 `1114`（DLL 初始化失败）；通过该测试的异常栈和本地 PDB 解析得到：

```text
EGLHook::EGLHook
WrappedOpenGL::WrappedOpenGL
rdcspv::Init
glslang::InitializeProcess
ShInitialize
std::lock_guard<std::mutex>
std::_Mutex_base::lock
MSVCP140.dll + 0x13020
```

因此，ReconDuck 的 glslang 初始化使用了新工具链生成的 mutex 构造，而捕获进程使用旧运行库，造成 DLL 初始化失败。弹窗里的工作目录、全局构造和 `/LARGEADDRESSAWARE:NO` 是统一错误提示；它们并非本次已经查出的原因。该失败发生在正常图形 API 捕获之前。

[微软 STL 的 VS 2022 17.10 变更说明](https://github.com/microsoft/STL/wiki/VS-2022-Changelog#vs-2022-1710) 明确说明这个 mutex 兼容性问题，并提供 `_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR` 作为兼容办法。

## 修复与验证

仅在 SPIR-V/glslang 编译目标增加 `_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR`：

- `reconduck/driver/shaders/spirv/reconduck_spirv.vcxproj`，覆盖两种架构和配置。
- `reconduck/driver/shaders/spirv/CMakeLists.txt`，仅 MSVC，定义只作用于该目标。

没有修改游戏目录。新增 `util/test/reconduck_compatibility/runtime_compatibility.{cpp,py}`，回归复制旧运行库到独立测试目录。

验证结果：

- 未启用兼容定义：复现 `0xc0000005`，偏移 `0x13020`。
- 使用实际工程兼容定义：mutex 构造和加锁成功。
- 旧运行库下加载实际新 `reconduck.dll`：成功，原先 `1114` 错误消失。
- x64 核心/GUI、x86 核心/命令行完整构建通过。
- 修复后再次验证 x64/x86 真实 WARP 捕获、回放及全部 4096 个像素。

构建日志在外层 `build-temp/runtime-compat-build-x64.log`、`runtime-compat-build-x86.log`。回归产物在 `build-temp/runtime-compatibility-validation/`。

关闭当前 GUI 后使用 `x64/Development/qreconduck.exe` 重试。本次已修复并复现验证运行库初始化问题；Endfield 的实际注入和捕获仍需在游戏中复测，不能将 DLL 加载成功等同于完整游戏捕获成功。
