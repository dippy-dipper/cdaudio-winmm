windres cdaudio-winmm.rc.in -O coff -o cdaudio-winmm.rc.o
gcc -std=gnu99 -Wl,--enable-stdcall-fixup -Ilibs/include -O2 -shared -s -o cdaudio-winmm.dll cdaudio-winmm.c stubs.c cdaudio-winmm.def cdaudio-winmm.rc.o -L. -lwinmm -D_DEBUG -static-libgcc
pause