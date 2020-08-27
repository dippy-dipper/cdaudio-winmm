REV=$(shell sh -c 'git rev-parse --short @{0}')

all: cdaudio-winmm.dll

cdaudio-winmm.rc.o: cdaudio-winmm.rc.in
	sed 's/__REV__/$(REV)/g' cdaudio-winmm.rc.in | sed 's/__FILE__/cdaudio-winmm/g' | windres -O coff -o cdaudio-winmm.rc.o

cdaudio-winmm.dll: cdaudio-winmm.c cdaudio-winmm.rc.o cdaudio-winmm.def stubs.c
	mingw32-gcc -std=gnu99 -Wl,--enable-stdcall-fixup -Ilibs/include -O2 -shared -s -o cdaudio-winmm.dll cdaudio-winmm.c stubs.c cdaudio-winmm.def cdaudio-winmm.rc.o -L. -lwinmm -D_DEBUG -static-libgcc

clean:
	rm -f cdaudio-winmm.dll cdaudio-winmm.rc.o
