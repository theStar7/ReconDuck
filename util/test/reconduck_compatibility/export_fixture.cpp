#include <windows.h>

extern "C" __declspec(dllexport) __declspec(noinline) int WINAPI CachedTarget(int value)
{
  volatile int scale = 3;
  return value * scale + 7;
}

BOOL WINAPI DllMain(HINSTANCE, DWORD, LPVOID)
{
  return TRUE;
}
