Microsoft Detours 4.0.1, MIT licensed.

Source: https://github.com/microsoft/Detours
Imported from RenderDuck commit dcbb5b7ba7e64a785400f263c1d4bad04b8e372b:
https://github.com/LesFloraison/RenderDuck/tree/dcbb5b7ba7e64a785400f263c1d4bad04b8e372b/riderduck/3rdparty/detours

ReconDuck uses Detours for Windows export entry interception. Library loading
and DLL injection continue through ReconDuck's existing implementations.

Local change in detours.cpp: DetourThread and DetourOperation bookkeeping uses
VirtualAlloc/VirtualFree instead of the shared CRT heap. Transactions enrol
other process threads, so allocating or freeing through their shared heap while
they are suspended could deadlock if one of them owns its lock. The transaction
algorithm and trampoline allocation are otherwise unchanged. Each bookkeeping
allocation occupies a virtual-memory page and is released by commit or abort.
