windres cdaudioplr.rc -O coff -o cdaudioplr.res
gcc cdaudioplr.c cdaudioplr.res -lwinmm -D_DEBUG -mwindows -o cdaudioplr.exe
pause
