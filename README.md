# cdaudio-winmm player (beta v.0.3):

This is a winmm wrapper to a separate cdaudio player that handles the track repeat that is broken from Windows Vista onwards. Unlike the ogg-winmm wrapper which plays ripped .ogg files cdaudio-winmm instead tries to play the cdtracks on a physical disc (or cdimage) using a separate player program. Communication between winmm.dll and the player is done using [mailslots.](https://docs.microsoft.com/en-us/windows/win32/ipc/mailslots)

The trick is to handle the broken MCI mode change by monitoring POSTION and MODE. If MODE = "playing" and POSITION has not changed then we can determine that the track has finished playing and can update the MODE and send the MM_NOTIFY_SUCCESSFUL message (if requested). The cdaudio player code might also be useful to anyone wanting to write an MCI cdaudio player on newer Windows systems.

![screenshot](screenshot-v04.png)

0.4 changes:
- MciSendCommand improvements from ogg-winmm project. Support for more games.
- MciSendString improvements copied from ogg-winmm.
- AuxVolume control enabled. (should use in-game volume sliders. Manual volume override available for problematic cases)
- SetCurrentDirectory fix for cdaudioplr.exe when started from winmm.dll.
Limitations:
- Plays only single tracks which is fine most of the time but causes problems if the game issues a single "from -> to" command to play multiple tracks.
- All tracks are reported as 2 minutes long. This may cause issues if a game relies on an accurate response for the track length query in order to determine when the track has finished playing.

0.3 changes:
- mp3/wav support
- cleared up some naming inconsistency


# Building:

Build with make.cmd if path variables are set (C:\MinGW\bin).
Or build from msys with command: mingw32-make

# Usage:

- Place winmm.dll wrapper into the game folder.
- Place cdaudioplr.exe in 'mcicda' -sub-folder.
- Run the game normally.

NOTE:
- You can start cdaudioplr.exe manually before running the game. Sometimes this may be necessary since the game may query the cd device before the wrapper has time to initialize the player.
- Do not place cdaudioplr.exe and winmm.dll in the same folder!
- v.0.3 now supports mp3 and wav playback if a music folder is found containing the tracks in the correct format. (track02.mp3/wav ...)
