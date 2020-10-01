/*
	THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF 
	ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO 
	THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A 
	PARTICULAR PURPOSE.

	(c) 2020 DD

	cdaudio player using MCI Strings with the
	objective of fixing regression issue in cdaudio
	playback starting with Windows Vista. Mainly the
	lack of a working mode update after playing has
	finished (missing MCI_NOTIFY_SUCCESSFUL msg).
*/

#include <windows.h>
#include <stdio.h>
#include <direct.h>
#include <sys/stat.h>
#include <dirent.h>
#include <ctype.h>

#pragma comment(lib, "winmm.lib")
#define GetCurrentDir _getcwd

/* debug logging */
#ifdef _DEBUG
	#define dprintf(...) if (fh) { fprintf(fh, __VA_ARGS__); fflush(NULL); }
	FILE * fh = NULL;
#else
	#define dprintf(...)
#endif
/****/

/* Win32 GUI stuff:*/
#define IDC_MAIN_EDIT	101
#define ID_FILE_EXIT 9001
#define ID_VOLUME_0 9002
#define ID_VOLUME_10 9003
#define ID_VOLUME_20 9004
#define ID_VOLUME_30 9005
#define ID_VOLUME_40 9006
#define ID_VOLUME_50 9007
#define ID_VOLUME_60 9008
#define ID_VOLUME_70 9009
#define ID_VOLUME_80 9010
#define ID_VOLUME_90 9011
#define ID_VOLUME_100 9012
#define ID_VIEW_WDIR 9013
#define ID_HELP_INST 9014
#define ID_HELP_ABOUT 9015
/************************/

/* Mailslot header stuff: */
DWORD NumberOfBytesRead;
DWORD BytesWritten;
CHAR ServerName[] = "\\\\.\\Mailslot\\winmm_Mailslot";
char buffer[256];
char name[256];
int value;
/********/

const char g_szClassName[] = "myWindowClass";
char pos1[64] = "-";
char pos2[64] = "-";
char mode[64] = "";
char media[64] = "";
char tracks_s[64] = "";
char play_cmd[128] = "";
char ini_dir[256] = "";

int play_from = 0;
int play_from2;
int play_to;
int tracks = 0;
int play = 0;
int paused_track = 100;
int notify_msg = 0;
int quit = 0;
int mp3done = 0;

FILE * fp;
HANDLE reader = NULL;
HANDLE player = NULL;
HWND hEdit;
HWND g_hMainWindow = NULL;

/* Mailslot reader thread: */
int reader_main( void )
{
	HANDLE Mailslot;
	/* Create mailslot: */
	if ((Mailslot = CreateMailslot("\\\\.\\Mailslot\\cdaudioplr_Mailslot", 0, MAILSLOT_WAIT_FOREVER, NULL)) == INVALID_HANDLE_VALUE)
	{
		dprintf("mailslot error %d\n", GetLastError());
		return 0;
	}

	/* Loop to read mailslot: */
	while(ReadFile(Mailslot, buffer, 256, &NumberOfBytesRead, NULL) != 0)
	{

		if (NumberOfBytesRead > 0){
			sscanf(buffer,"%d %s", &value, name);
			dprintf("Mailslot stored name = %s | value = %d\n", name, value);

				/* Handle notify message: */
				if(strcmp(name,"mci_notify")==0){
					notify_msg = value;
				}

				/* Read track to play: */
				if(strcmp(name,"mci_from")==0){
					play_from = value;
					SetWindowText(hEdit, TEXT("Command: MCI_PLAY"));
				}

				if(strcmp(name,"mci_to")==0){
					/* play_to = value; */
					/* mci_to is converted to mci_from. */
					if(value != play_from && value < 99)
					{
						play_from = value - 1;
					}
				}

				/* Read stop (play_from = 0): */
				if(strcmp(name,"mci_stop")==0){
					play_from = 0;
					SetWindowText(hEdit, TEXT("Command: MCI_STOP"));
				}

				/* Read pause (play_from = 100): */
				if(strcmp(name,"mci_pause")==0){
					play_from = 100;
					SetWindowText(hEdit, TEXT("Command: MCI_PAUSE"));
				}

				/* Read mci_resume */
				if(strcmp(name,"mci_resume")==0){
					play_from = paused_track;
				}

				/* Read exit message */
				if(strcmp(name,"exit")==0){
					quit = 1;
				}
		}
	}
}

/* Player thread: */
int player_main( void )
{
	/* Get the working directory: */
	char wdir[FILENAME_MAX];
	GetCurrentDir(wdir, FILENAME_MAX);
	sprintf(ini_dir, "%s\\cdaudio_vol.ini", wdir);

	/*
	Set volume:
	(Note: Since we have a separate player app 
	the volume can be adjusted also from the 
	Windows mixer with better accuracy...)
	*/
	int cdaudio_vol = 70; /* Default volume set at 70% */

	fp = fopen (ini_dir, "r");
			/* If not null read values */
			if (fp!=NULL){
			fscanf(fp, "%d", &cdaudio_vol);
			fclose(fp);
		}
		/* Else write new ini file */
		else{
		fp = fopen (ini_dir, "w+");
		fprintf(fp, "%d\n"
			"#\n"
			"# cdaudioplr CD music volume control.\n"
			"# Change the number to the desired volume level (0-100).", cdaudio_vol);
		fclose(fp);
	}

	if (cdaudio_vol < 0) cdaudio_vol = 0;
	if (cdaudio_vol > 100) cdaudio_vol = 100;
	if (cdaudio_vol <= 0) waveOutSetVolume(NULL, 0x0);
	if (cdaudio_vol <= 10 && cdaudio_vol > 1) waveOutSetVolume(NULL, 0x1F401F40); /* 8000 */
	if (cdaudio_vol <= 20 && cdaudio_vol > 10) waveOutSetVolume(NULL, 0x32C832C8); /* 13000 */
	if (cdaudio_vol <= 30 && cdaudio_vol > 20) waveOutSetVolume(NULL, 0x4A384A38); /* 19000 */
	if (cdaudio_vol <= 40 && cdaudio_vol > 30) waveOutSetVolume(NULL, 0x652C652C); /* 25900 */
	if (cdaudio_vol <= 50 && cdaudio_vol > 40) waveOutSetVolume(NULL, 0x7D007D00); /* 32000 */
	if (cdaudio_vol <= 60 && cdaudio_vol > 50) waveOutSetVolume(NULL, 0x9A4C9A4C); /* 39500 */
	if (cdaudio_vol <= 70 && cdaudio_vol > 60) waveOutSetVolume(NULL, 0xAFC8AFC8); /* 45000 */
	if (cdaudio_vol <= 80 && cdaudio_vol > 70) waveOutSetVolume(NULL, 0xCB20CB20); /* 52000 */
	if (cdaudio_vol <= 90 && cdaudio_vol > 80) waveOutSetVolume(NULL, 0xE290E290); /* 58000 */
	if (cdaudio_vol >= 100 && cdaudio_vol > 90) waveOutSetVolume(NULL, 0xFFFFFFFF); /* 65000 */

  /* -------------------- */
 /* mp3/wav player code: */
/* -------------------- */

	/* HWND hwnd; */
	DIR *dir;
	struct dirent *list;
	char tempname[256] = "";
	int mp3playback = 0;
	int lookforwav = 0;

	/* Try to open music dir */
	if ((dir = opendir (".\\music\\")) == NULL){
		dprintf ("Music dir not found.\n");
		}
		else{
		/* Look for .mp3 */
		while ((list = readdir (dir)) != NULL){
			/* Get file name sring */
			sprintf (tempname, "%s", list->d_name);
			/* Make it lower case */
			for(int i = 0; tempname[i]; i++){
			tempname[i] = tolower(tempname[i]);
			}
			/* Look for "track" and ".mp3" in the file name */
			if ((strstr(tempname, "track")) && (strstr(tempname, ".mp3"))){
			tracks++;
			}
		}
		closedir (dir);
		if(!tracks){
			dprintf ("No mp3 tracks found!\n");
			lookforwav = 1;
		}
		else{
		dprintf ("Found %d mp3 tracks.\n", tracks);
		SetWindowText(hEdit, TEXT("Using .mp3 files for music playback."));
		tracks++; /* Add data track */
		mp3playback = 1;

		/* convert to string */
		sprintf(tracks_s, "%d", tracks);
		/* Write no. of tracks for winmm wrapper: */
		HANDLE Mailslot = CreateFile(ServerName, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		WriteFile(Mailslot, strcat(tracks_s, " tracks"), 64, &BytesWritten, NULL);
		CloseHandle(Mailslot);
		}

		/* Look for .wav instead */
		if(lookforwav == 1){
			dir = opendir (".\\music\\");
			while ((list = readdir (dir)) != NULL){
				/* Get file name sring */
				sprintf (tempname, "%s", list->d_name);
				/* Make it lower case */
				for(int i = 0; tempname[i]; i++){
				tempname[i] = tolower(tempname[i]);
				}
				/* Look for "track" and ".wav" in the file name */
				if ((strstr(tempname, "track")) && (strstr(tempname, ".wav"))){
				tracks++;
				}
			}
			closedir (dir);
			if(!tracks){
				dprintf ("No wav tracks found!\n");
			}
			else{
			dprintf ("Found %d wav tracks.\n", tracks);
			SetWindowText(hEdit, TEXT("Using .wav files for music playback."));
			tracks++; /* Add data track */
			mp3playback = 1;
        	
			/* convert to string */
			sprintf(tracks_s, "%d", tracks);
			/* Write no. of tracks for winmm wrapper: */
			HANDLE Mailslot = CreateFile(ServerName, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
			WriteFile(Mailslot, strcat(tracks_s, " tracks"), 64, &BytesWritten, NULL);
			CloseHandle(Mailslot);
			}
		}
	}

	/* mp3(and wav) player loop: */
	while(mp3playback == 1){

		if(play_from > 0 && play_from < 100)
		{
			play = 1;
			play_from2 = play_from; /* For comparison. */
		}
    	
		while(play == 1)
		{
			/* Handle MCI_PAUSE: */
			if(play_from == paused_track)
			{
				mciSendStringA("resume mp3track", NULL, 0, NULL);
				paused_track = 100;
				dprintf("RESUME:\n");
				SetWindowText(hEdit, TEXT("Command: MCI_RESUME"));
			}
			else
			{
				/* Open .mp3 file */
				if(lookforwav == 1){
				sprintf(play_cmd, "open \".\\music\\track%02d.wav\" type mpegvideo alias mp3track", play_from);
				}
				/* Open .wav file */
				else{
				sprintf(play_cmd, "open \".\\music\\track%02d.mp3\" type mpegvideo alias mp3track", play_from);
				}
				mciSendStringA(play_cmd, NULL, 0, NULL);
				dprintf("%s\n", play_cmd);
				
				/* Issue play command: */
				mciSendStringA("play mp3track notify", NULL, 0, g_hMainWindow);
				mp3done = 0;
				dprintf("playing track: %d\n", play_from);
    	
				/* Write mode playing for winmm wrapper: */
				HANDLE Mailslot = CreateFile(ServerName, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
				WriteFile(Mailslot, "2 mode", 64, &BytesWritten, NULL);
				CloseHandle(Mailslot);
			}

		while (mp3done == 0){
			Sleep (100);
			/* Check if track has changed: */
			if(play_from != play_from2){
				notify_msg = 0; /* If playback is interrupted do not send notify success msg. */
				break;
			}
		}
		/* Write MODE stopped for winmm wrapper: */
		HANDLE Mailslot = CreateFile(ServerName, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		WriteFile(Mailslot, "1 mode", 64, &BytesWritten, NULL);
		CloseHandle(Mailslot);

		/* Handle MCI_PAUSE: */
		if (play_from == 100)
		{
			mciSendStringA("pause mp3track", NULL, 0, NULL);
			dprintf("  PAUSE:\n");
			paused_track = play_from2;
		}
		else
		{
		mciSendStringA("stop mp3track", NULL, 0, NULL);
		mciSendStringA("close mp3track", NULL, 0, NULL);
		}

		/* Write notify success message: */
		if (notify_msg == 1){
			HANDLE Mailslot = CreateFile(ServerName, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
			WriteFile(Mailslot, "1 notify_s", 64, &BytesWritten, NULL);
			CloseHandle(Mailslot);

			notify_msg = 0;
			dprintf("Notify SUCCESS!\n");
		}

		play = 0;
		
	}

	Sleep(100);
	}

  /* --------------------------- */
 /* End of mp3/wav player code. */
/* --------------------------- */

	mciSendStringA("status cdaudio media present", media, 64, NULL);
	/* Loop to check if CD is inserted: */
	while(strcmp(media,"true")!=0)
	{
		SetWindowText(hEdit, TEXT("Looking for CD."));
		Sleep(500);
		mciSendStringA("status cdaudio media present", media, 64, NULL);
		SetWindowText(hEdit, TEXT("Looking for CD.."));
		Sleep(500);
		SetWindowText(hEdit, TEXT("Looking for CD..."));
		Sleep(500);
	}
	SetWindowText(hEdit, TEXT("CD Found... READY."));

	/* Get the number of CD tracks: */
	mciSendStringA("status cdaudio number of tracks", tracks_s, 64, NULL);
	/* convert to int: */
	sscanf(tracks_s, "%d", &tracks); /* tracks = atoi(tracks_s); */
	dprintf("Tracks: %d\n", tracks);

	/* Write no. of tracks for winmm wrapper: */
	HANDLE Mailslot = CreateFile(ServerName, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	WriteFile(Mailslot, strcat(tracks_s, " tracks"), 64, &BytesWritten, NULL);
	CloseHandle(Mailslot);

	/* Open cdaudio & set time format: */
	mciSendStringA("close cdaudio", NULL, 0, NULL); /* Important! */
	mciSendStringA("open cdaudio", NULL, 0, NULL);
	mciSendStringA("set cdaudio time format tmsf", NULL, 0, NULL);

	/* cdaudio player loop: */
	while(1){

	if(play_from > 0 && play_from < 100)
	{
		play = 1;
		play_from2 = play_from; /* For comparison. */
	}

	while(play == 1)
	{
		/* A temporary work around: */
		play_to = play_from+1;

		/* Handle MCI_PAUSE: */
		if(play_from == paused_track)
		{
			/*
			MCI_RESUME is new for cdaudio in WinVista+.
			Note: In Win9x/XP you could resume only with "MCI_TO" or "MCI_PLAY NULL".
			*/
			mciSendStringA("resume cdaudio", NULL, 0, NULL);
			paused_track = 100;
			dprintf("RESUME:\n");
			SetWindowText(hEdit, TEXT("Command: MCI_RESUME"));
			mciSendStringA("status cdaudio mode", mode, 64, NULL); /* Get the mode. */

			/*
			Handle CD spin-up with a loop:
			(While mode is playing and position does not change.)
			*/
			while(strcmp(pos1,pos2)==0 && strcmp(mode,"playing")==0 && play == 1)
			{
				mciSendStringA("status cdaudio position", pos1, 64, NULL);
				Sleep (100);
				mciSendStringA("status cdaudio position", pos2, 64, NULL);
			}
		}

		else
		{
			/* Handle last track: */
			if(play_to > tracks)
			{
				sprintf(play_cmd, "play cdaudio from %d", play_from);
			}

			else
			{
				sprintf(play_cmd, "play cdaudio from %d to %d", play_from, play_to);
			}

			/* Issue play command: */
			mciSendStringA(play_cmd, NULL, 0, NULL);
			mciSendStringA("status cdaudio mode", mode, 64, NULL); /* Get the initial mode. */
			dprintf("%s track: %d\n", mode, play_from);

			/* Write mode playing for winmm wrapper: */
			HANDLE Mailslot = CreateFile(ServerName, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
			WriteFile(Mailslot, "2 mode", 64, &BytesWritten, NULL);
			CloseHandle(Mailslot);

			/*
			Handle CD spin-up with a loop:
			(While mode is playing and position does not change.)
			*/
			while(strcmp(pos1,pos2)==0 && strcmp(mode,"playing")==0 && play == 1)
			{
				mciSendStringA("status cdaudio position", pos1, 64, NULL);
				Sleep (100);
				mciSendStringA("status cdaudio position", pos2, 64, NULL);
			}
		}

		/*
		cdaudio play loop:
		(While positions differ track must be playing.)
		*/
		while(strcmp(pos1,pos2)!=0 && strcmp(mode,"playing")==0 && play == 1)
		{
			mciSendStringA("status cdaudio position", pos1, 64, NULL);
			/* dprintf("    POS: %s\r", pos1); */
			Sleep (500);
			mciSendStringA("status cdaudio position", pos2, 64, NULL);
			/* dprintf("    POS: %s\r", pos2); */
			/* Check for mode change: */
			mciSendStringA("status cdaudio mode", mode, 64, NULL);

			/* Check if track has changed: */
			if(play_from != play_from2)
			{
				notify_msg = 0; /* If playback is interrupted do not send notify success msg. */
				break;
			}
		}

		/* Write MODE stopped for winmm wrapper: */
		HANDLE Mailslot = CreateFile(ServerName, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		WriteFile(Mailslot, "1 mode", 64, &BytesWritten, NULL);
		CloseHandle(Mailslot);

		/* Handle MCI_PAUSE: */
		if (play_from == 100)
		{
			mciSendStringA("pause cdaudio", NULL, 0, NULL);
			dprintf("  PAUSE:\n");
			paused_track = play_from2;
		}
		else
		{
		/* Mode will stay "playing" so we need a "stop cdaudio" to update it. */
		mciSendStringA("stop cdaudio", NULL, 0, NULL);
		}

		/* Get last mode: */
		mciSendStringA("status cdaudio mode", mode, 64, NULL);
		dprintf("%s\n", mode);

		/* Write notify success message: */
		if (notify_msg == 1)
		{
			HANDLE Mailslot = CreateFile(ServerName, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
			WriteFile(Mailslot, "1 notify_s", 64, &BytesWritten, NULL);
			CloseHandle(Mailslot);

			notify_msg = 0;
			dprintf("Notify SUCCESS!\n");
		}

		/* Reset pos strings for next run: */
		strcpy (pos1, "-");
		strcpy (pos2, "-");
		play = 0;
	}

	Sleep(100);
	}

	return 0;
}

/* Message handling: */
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch(msg)
	{
		case MM_MCINOTIFY:
		{
			/* if (msg==MM_MCINOTIFY && wParam==MCI_NOTIFY_SUCCESSFUL) */
			mp3done = 1;
		}
		break;
		case WM_CREATE:
		{
			HFONT hfDefault;

			/* Static text display: */
			hEdit = CreateWindow("STATIC", "", WS_VISIBLE | WS_CHILD | SS_LEFT, 0,0,100,100, hwnd, (HMENU)IDC_MAIN_EDIT, GetModuleHandle(NULL), NULL);

			HMENU hMenu, hSubMenu;
			HICON hIcon, hIconSm;

			hMenu = CreateMenu();

			hSubMenu = CreatePopupMenu();
			AppendMenu(hSubMenu, MF_STRING, ID_FILE_EXIT, "E&xit");
			AppendMenu(hMenu, MF_STRING | MF_POPUP, (UINT)hSubMenu, "&File");

			hSubMenu = CreatePopupMenu();
			AppendMenu(hSubMenu, MF_STRING, ID_VOLUME_0, "&Mute");
			AppendMenu(hSubMenu, MF_STRING, ID_VOLUME_10, "&10%");
			AppendMenu(hSubMenu, MF_STRING, ID_VOLUME_20, "&20%");
			AppendMenu(hSubMenu, MF_STRING, ID_VOLUME_30, "&30%");
			AppendMenu(hSubMenu, MF_STRING, ID_VOLUME_40, "&40%");
			AppendMenu(hSubMenu, MF_STRING, ID_VOLUME_50, "&50%");
			AppendMenu(hSubMenu, MF_STRING, ID_VOLUME_60, "&60%");
			AppendMenu(hSubMenu, MF_STRING, ID_VOLUME_70, "&70%");
			AppendMenu(hSubMenu, MF_STRING, ID_VOLUME_80, "&80%");
			AppendMenu(hSubMenu, MF_STRING, ID_VOLUME_90, "&90%");
			AppendMenu(hSubMenu, MF_STRING, ID_VOLUME_100, "&100%");
			/* TODO: Check marks: */
			/*
			CheckMenuItem(hSubMenu, ID_VOLUME_100, MF_CHECKED);
			CheckMenuItem(hSubMenu, ID_VOLUME_100, MF_UNCHECKED);
			*/
			AppendMenu(hMenu, MF_STRING | MF_POPUP, (UINT)hSubMenu, "&Volume");

			hSubMenu = CreatePopupMenu();
			AppendMenu(hSubMenu, MF_STRING, ID_VIEW_WDIR, "&Running from...");
			AppendMenu(hMenu, MF_STRING | MF_POPUP, (UINT)hSubMenu, "&View");

			hSubMenu = CreatePopupMenu();
			AppendMenu(hSubMenu, MF_STRING, ID_HELP_INST, "&Instructions");
			AppendMenu(hSubMenu, MF_STRING, ID_HELP_ABOUT, "&About");
			AppendMenu(hMenu, MF_STRING | MF_POPUP, (UINT)hSubMenu, "&Help");

			SetMenu(hwnd, hMenu);
		}
		break;
		case WM_SIZE:
		{
			/* Resize text area: */
			HWND hEdit;
			RECT rcClient;

			GetClientRect(hwnd, &rcClient);

			hEdit = GetDlgItem(hwnd, IDC_MAIN_EDIT);
			SetWindowPos(hEdit, NULL, 0, 0, rcClient.right, rcClient.bottom, SWP_NOZORDER);
		}
		break;
		case WM_COMMAND:
			switch(LOWORD(wParam))
			{
				case ID_FILE_EXIT:
					PostMessage(hwnd, WM_CLOSE, 0, 0);
				break;
				/* GUI adjust volume and write to .ini */
				case ID_VOLUME_0:
					waveOutSetVolume(NULL, 0x0);
					fp = fopen (ini_dir, "w+");
					fprintf(fp, "0");
					fclose(fp);
					MessageBox(hwnd, "Music is muted.", " ", MB_OK);
				break;
				case ID_VOLUME_10:
					waveOutSetVolume(NULL, 0x1F401F40);
					fp = fopen (ini_dir, "w+");
					fprintf(fp, "10");
					fclose(fp);
					MessageBox(hwnd, "Volume is now at 10%", " ", MB_OK);
				break;
				case ID_VOLUME_20:
					waveOutSetVolume(NULL, 0x32C832C8);
					fp = fopen (ini_dir, "w+");
					fprintf(fp, "20");
					fclose(fp);
					MessageBox(hwnd, "Volume is now at 20%", " ", MB_OK);
				break;
				case ID_VOLUME_30:
					waveOutSetVolume(NULL, 0x4A384A38);
					fp = fopen (ini_dir, "w+");
					fprintf(fp, "30");
					fclose(fp);
					MessageBox(hwnd, "Volume is now at 30%", " ", MB_OK);
				break;
				case ID_VOLUME_40:
					waveOutSetVolume(NULL, 0x652C652C);
					fp = fopen (ini_dir, "w+");
					fprintf(fp, "40");
					fclose(fp);
					MessageBox(hwnd, "Volume is now at 40%", " ", MB_OK);
				break;
				case ID_VOLUME_50:
					waveOutSetVolume(NULL, 0x7D007D00);
					fp = fopen (ini_dir, "w+");
					fprintf(fp, "50");
					fclose(fp);
					MessageBox(hwnd, "Volume is now at 50%", " ", MB_OK);
				break;
				case ID_VOLUME_60:
					waveOutSetVolume(NULL, 0x9A4C9A4C);
					fp = fopen (ini_dir, "w+");
					fprintf(fp, "60");
					fclose(fp);
					MessageBox(hwnd, "Volume is now at 60%", " ", MB_OK);
				break;
				case ID_VOLUME_70:
					waveOutSetVolume(NULL, 0xAFC8AFC8);
					fp = fopen (ini_dir, "w+");
					fprintf(fp, "70");
					fclose(fp);
					MessageBox(hwnd, "Volume is now at 70%", " ", MB_OK);
				break;
				case ID_VOLUME_80:
					waveOutSetVolume(NULL, 0xCB20CB20);
					fp = fopen (ini_dir, "w+");
					fprintf(fp, "80");
					fclose(fp);
					MessageBox(hwnd, "Volume is now at 80%", " ", MB_OK);
				break;
				case ID_VOLUME_90:
					waveOutSetVolume(NULL, 0xE290E290);
					fp = fopen (ini_dir, "w+");
					fprintf(fp, "90");
					fclose(fp);
					MessageBox(hwnd, "Volume is now at 90%", " ", MB_OK);
				break;
				case ID_VOLUME_100:
					waveOutSetVolume(NULL, 0xFFFFFFFF);
					fp = fopen (ini_dir, "w+");
					fprintf(fp, "100");
					fclose(fp);
					MessageBox(hwnd, "Volume is now at 100%", " ", MB_OK);
				break;
				case ID_VIEW_WDIR:
				{
					char szFileName[MAX_PATH];
					HINSTANCE hInstance = GetModuleHandle(NULL);

					GetModuleFileName(hInstance, szFileName, MAX_PATH);
					MessageBox(hwnd, szFileName, "cdaudio-winmm player is running from:", MB_OK | MB_ICONINFORMATION);
				}
				break;
				case ID_HELP_INST:
					MessageBox(hwnd, TEXT("1. Place winmm.dll wrapper\n"
					"    into the game folder.\n2. Place cdaudioplr.exe\n"
					"    in 'mcicda' -subfolder.\n3. Run the game normally.\n\n"
					"Additional tips:\n\n-You can also start cdaudioplr.exe\n"
					"manually before running the game.\n\n-Do not place cdaudioplr.exe and\n"
					"winmm.dll in the same folder.\n\n- mp3 or wav tracks can be placed\n"
					"in 'music' folder (track02 ...)"), TEXT("Instructions"), MB_OK);
				break;
				case ID_HELP_ABOUT:
					MessageBox(hwnd, TEXT("cdaudio-winmm player\nversion 0.3 Beta (c) 2020\n\nRestores track repeat\nand volume control\nin Vista and later."), TEXT("About"), MB_OK);
				break;
			}
		break;
		case WM_CLOSE:
			DestroyWindow(hwnd);
		break;
		case WM_DESTROY:
			PostQuitMessage(0);
		break;
		default:
			return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return 0;
}

/* Main Window process: */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, 
	LPSTR lpCmdLine, int nCmdShow)
{
	/* Checks that program is not already running: */
	CreateMutexA(NULL, TRUE, "cdaudioplrMutex");
	if (GetLastError() == ERROR_ALREADY_EXISTS)
	{
		return 0;
	}

	WNDCLASSEX wc;
	HWND hwnd;
	MSG Msg;

	wc.cbSize        = sizeof(WNDCLASSEX);
	wc.style         = 0;
	wc.lpfnWndProc   = WndProc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = hInstance;
	wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
	wc.lpszMenuName  = NULL;
	wc.lpszClassName = g_szClassName;
	wc.hIconSm       = LoadIcon(NULL, IDI_APPLICATION);

	if(!RegisterClassEx(&wc))
	{
		MessageBox(NULL, "Window Registration Failed!", "Error!",
			MB_ICONEXCLAMATION | MB_OK);
		return 0;
	}

	/* Check that winmm.dll & cdaudioplr.exe are not in the same dir: */
	struct stat sb;
	char file[] = ".\\cdaudioplr.exe";
	char file2[] = ".\\winmm.dll";

	if (stat(file, &sb) == 0 && S_ISREG(sb.st_mode) && stat(file2, &sb) == 0 && S_ISREG(sb.st_mode)) 
	{
		MessageBox(NULL, "cdaudioplr.exe and winmm.dll can not be in the same dir.", "Error!",
			MB_ICONEXCLAMATION | MB_OK);
		return 0;
	}

	/* Check for Win version. If less than Vista quit. */
	/*
	DWORD dwVersion = 0; 
	dwVersion = GetVersion();
	if (dwVersion < 590000000)
	{
		MessageBox(NULL, "Needs WinVista or newer.", "Error!",
		return 0;
	}
	*/

	hwnd = CreateWindowEx(
		WS_EX_CLIENTEDGE,
		g_szClassName,
		"cdaudio-winmm player",
		WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
		CW_USEDEFAULT, CW_USEDEFAULT, 340, 220,
		NULL, NULL, hInstance, NULL);

	if(hwnd == NULL)
	{
		MessageBox(NULL, "Window Creation Failed!", "Error!",
			MB_ICONEXCLAMATION | MB_OK);
		return 0;
	}

	g_hMainWindow = hwnd;

	ShowWindow(hwnd, SW_SHOWNOACTIVATE); /* Start with inactive window. */
	UpdateWindow(hwnd);

	/* debug logging */
	#ifdef _DEBUG
	fh = fopen("cdaudioplr.log", "w");
	#endif
	dprintf("Beginning of debug log:\n");

	/* Start threads: */
	reader = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)reader_main, NULL, 0, NULL);
	player = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)player_main, NULL, 0, NULL);

	/* Message Loop: */
	while(GetMessage(&Msg, NULL, 0, 0) > 0 && quit == 0)
	{
		TranslateMessage(&Msg);
		DispatchMessage(&Msg);
	}

	/* debug logging */
	dprintf("End of debug log.\n");
	#ifdef _DEBUG
	if (fh)
	{
		fclose(fh);
		fh = NULL;
	}
	#endif

	return Msg.wParam;
}
