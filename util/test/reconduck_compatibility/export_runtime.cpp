#include <windows.h>
#include <stddef.h>

void *operator new(size_t bytes) { return HeapAlloc(GetProcessHeap(), 0, bytes); }
void *operator new[](size_t bytes) { return HeapAlloc(GetProcessHeap(), 0, bytes); }
void operator delete(void *value) noexcept { if(value) HeapFree(GetProcessHeap(), 0, value); }
void operator delete[](void *value) noexcept { if(value) HeapFree(GetProcessHeap(), 0, value); }

extern "C" void *memmove(void *destination, const void *source, size_t count)
{
  volatile unsigned char *dst = (volatile unsigned char *)destination;
  const volatile unsigned char *src = (const volatile unsigned char *)source;
  if(dst < src)
    for(size_t i = 0; i < count; ++i) dst[i] = src[i];
  else
    for(size_t i = count; i > 0; --i) dst[i - 1] = src[i - 1];
  return destination;
}
