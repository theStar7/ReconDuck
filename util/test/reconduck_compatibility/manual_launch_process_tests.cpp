#include <windows.h>
#include <stdint.h>

// The harness has no MSVC runtime dependency.
extern "C" void *memcpy(void *dst, const void *src, size_t count)
{
  volatile unsigned char *out = (volatile unsigned char *)dst;
  const volatile unsigned char *in = (const volatile unsigned char *)src;
  while(count--)
    *out++ = *in++;
  return dst;
}

static int failures = 0;
static void Check(bool value, const char *name)
{
  if(!value)
  {
    DWORD written = 0;
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), name, (DWORD)lstrlenA(name), &written, NULL);
    failures++;
  }
}

#if __has_include("../../../qreconduck/Code/manual_launch_process.h")
#include "../../../qreconduck/Code/manual_launch_process.h"
using namespace ManualLaunch;
extern "C" void entry()
{
  Process before[] = {{10, 0, 100, L"c:/game/game.exe"}};
  Process current[] = {{10, 0, 100, L"c:/game/game.exe"},
                       {11, 0, 200, L"c:/game/game.exe"},
                       {12, 0, 201, L"d:/other/game.exe"},
                       {13, 11, 202, L"c:/game/helper.exe"},
                       {14, 13, 203, L"c:/game/render.exe"},
                       {15, 0, 204, L""}};
  const wchar_t *target = L"c:/game/game.exe";
  Check(!IsNew(current[0], before, 1), "FAIL existing PID selected\n");
  Check(IsNew(current[1], before, 1), "FAIL new target rejected\n");
  Check(IsExactTarget(current[1], target), "FAIL full path target rejected\n");
  Check(!IsExactTarget(current[2], target), "FAIL same name different path selected\n");
  Check(!IsExactTarget(current[3], target), "FAIL same directory helper selected\n");
  Check(!IsExactTarget(current[5], target), "FAIL unknown path selected\n");
  Check(HasTargetAncestor(current[4], current, 6, before, 1, target),
        "FAIL true grandchild rejected\n");
  Check(!HasTargetAncestor(current[2], current, 6, before, 1, target),
        "FAIL unrelated process treated as child\n");
  Check(!CanConnect(false, true, true, false), "FAIL nonpresenting child selected\n");
  Check(!CanConnect(false, true, false, true), "FAIL unsupported child selected\n");
  Check(CanConnect(false, true, true, true), "FAIL presenting child rejected\n");
  Check(CanConnect(true, false, false, false), "FAIL exact control rejected\n");
  Process reused = {10, 0, 300, target};
  Check(IsNew(reused, before, 1), "FAIL reused PID lifetime rejected\n");
  reused.started = 0;
  Check(!IsNew(reused, before, 1), "FAIL ambiguous existing PID selected\n");
  Process cycle[] = {{20, 21, 400, L""}, {21, 20, 400, L""}};
  Check(!HasTargetAncestor(cycle[0], cycle, 2, before, 1, target),
        "FAIL cyclic ancestry accepted\n");
  Process wrongLifetime[] = {{11, 0, 500, target}, {14, 11, 203, L""}};
  Check(!HasTargetAncestor(wrongLifetime[1], wrongLifetime, 2, before, 1, target),
        "FAIL reused parent PID accepted\n");
  Check(CanInject(current[1], before, 1, target, 2, false), "FAIL retry rejected\n");
  Check(!CanInject(current[1], before, 1, target, 3, false), "FAIL retry bound exceeded\n");
  Check(!CanInject(current[1], before, 1, target, 0, true), "FAIL cancellation ignored\n");
  Check(!CanInject(current[3], before, 1, target, 0, false), "FAIL helper injection allowed\n");
  Process unknownLifetime = {40, 0, 0, target};
  Check(!CanInject(unknownLifetime, before, 1, target, 0, false),
        "FAIL injection with unknown lifetime allowed\n");
  ExitProcess(failures ? 1 : 0);
}
#else
extern "C" void entry()
{
  Check(false, "FAIL manual-launch exact-path process selection is unavailable\n");
  ExitProcess(1);
}
#endif
