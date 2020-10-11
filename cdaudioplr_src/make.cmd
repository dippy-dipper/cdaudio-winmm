windres cdaudioplr.rc -O coff -o cdaudioplr.res
g++ cdaudioplr.c cdaudioplr.res -lwinmm -static-libgcc -D_DEBUG -mwindows -o cdaudioplr.exe
pause
