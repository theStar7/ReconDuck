# Global Hook transaction regression

Run `python util/test/reconduck_compatibility/global_hook_tests.py` on Windows. The runner uses
the discovered MSVC 14.44 STL/compiler, Windows SDK 10.0.26100.0 and LLVM lld-link, compiling
the actual `win32_global_hook.h` as x86 and x64 with `/W4 /WX`. It also checks the real
`win32_process.cpp` translation unit using `/Zs` with its core precompiled header and defines.
Adjust the runner's tool paths
for another installation. Build products and fixture files are removed automatically.

Registry APIs are replaced by an in-memory two-view implementation. Helper processes are
represented by event handles, with controlled readiness/exit failures. Path validation,
file access, the backup file, pipes, process attributes, and the C++ standard library use
their real implementations. Backup files are confined to each temporary test directory;
neither AppInit registry view is accessed. Simulated helper waits advance immediately.

Before product edits, the runner failed against the old AppInit write because it replaced
the DLL list without including the saved entries. Native regression now covers:

- Relative, UNC, device, Unicode, whitespace, comma, metacharacter and overlong paths;
  missing alternate components and directory components fail before registry access.
- Explicit x86/x64 registry views; all open/read/write failure positions; exact original
  value types/bytes/existence and preservation of existing AppInit entries during startup.
- Missing, malformed and absent original values; recovery-file creation failure;
  pipe, inheritance and first/second helper launch failures; existing shared mappings;
  helper early exit or failure to publish readiness.
- Repeated cleanup; failed registry restoration, pipe closure or helper termination;
  retained snapshots/handles/lease and successful later retries, including failed startup
  followed by failed rollback. The actual public activity-query body is compiled and tested
  to report unresolved recovery.
- The actual startup/stop synchronization guard and atomic activity query are compiled
  directly from `win32_process.cpp`. Two real threads verify exclusive mutation, startup
  visibility before pointer publication, and 10,000 nonblocking activity reads while another
  thread holds the operation lock. Unowned failures clear activity; pending recovery keeps it.

The atomic startup regression was first run against the old pointer-only activity query and
failed with `public query reports startup before session pointer publication`.

These tests do not establish administrator access, real AppInit execution, real helper
startup, short-path/drive/junction policy on every filesystem, GUI behavior, or capture/replay.
Those require application integration and an explicitly controlled Windows test environment.
