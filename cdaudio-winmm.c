/*
  Copyright (c) 2012 Toni Spets <toni.spets@iki.fi>
 
  Permission to use, copy, modify, and distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.
 
  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
  ________________________________________________________________________
  
  This code has been modified to work with the stand-alone cdaudioplr.exe
  to fix issues with cdaudio playback in old games starting with Win Vista.
  Edits by: DD (2020)
  ________________________________________________________________________
*/

#include <windows.h>
#include <stdio.h>
#include <ctype.h>
#include <dirent.h>

/* Mailslot header stuff: */
DWORD NumberOfBytesRead;
DWORD BytesWritten;
CHAR ServerName[] = "\\\\.\\Mailslot\\cdaudioplr_Mailslot";
char buffer[256];
char name[256];
char dwVolume_str[32] = "";
char dwFrom_str[32] = "";
char dwTo_str[32] = "";
int value;
int mode = 1; /* 1=stopped, 2=playing */
/********/

#define MAGIC_DEVICEID 0xBEEF

#ifdef _DEBUG
	#define dprintf(...) if (fh) { fprintf(fh, __VA_ARGS__); fflush(NULL); }
	FILE *fh = NULL;
#else
	#define dprintf(...)
#endif

char alias_s[100] = "cdaudio";
int paused = 0;
int once = 1;
int counter = 0;
int playing = 0;
int time_format = MCI_FORMAT_TMSF;
int numTracks = 99; /* default: MAX 99 tracks */
CRITICAL_SECTION cs;
HANDLE reader = NULL;

/* Mailslot reader thread: */
int reader_main( void )
{
	HANDLE Mailslot;

	/* Create mailslot: */
	if ((Mailslot = CreateMailslot("\\\\.\\Mailslot\\winmm_Mailslot", 0, MAILSLOT_WAIT_FOREVER, NULL)) == INVALID_HANDLE_VALUE)
	{
		dprintf("mailslot error %d\n", GetLastError());
		return 0;
	}

	/* Start cdaudio player: */
	//Sleep(10);
	ShellExecuteA(NULL, "open", ".\\mcicda\\cdaudioplr.exe", NULL, NULL, SW_SHOWNOACTIVATE);

	/* Loop to read mailslot: */
	while(ReadFile(Mailslot, buffer, 256, &NumberOfBytesRead, NULL) != 0)
	{

		if (NumberOfBytesRead > 0){
			sscanf(buffer,"%d %s", &value, name);
			dprintf("-[ Mailslot stored name = %s | value = %d ]-\n", name, value);

				/* Read mode */
				if(strcmp(name,"mode")==0){
					mode = value;
				}

				/* Handle notify message: */
				if(strcmp(name,"notify_s")==0){
					SendMessageA((HWND)0xffff, MM_MCINOTIFY, MCI_NOTIFY_SUCCESSFUL, 0xBEEF);
				}

				/* Read no. of tracks: */
				if(strcmp(name,"tracks")==0){
					numTracks = value;
				}
		}
	}
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	if (fdwReason == DLL_PROCESS_ATTACH)
	{
#ifdef _DEBUG
		fh = fopen("winmm.log", "w");
#endif

		InitializeCriticalSection(&cs);

		/* Start Mailslot reader thread: */
		reader = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)reader_main, NULL, 0, NULL);
	}

	if (fdwReason == DLL_PROCESS_DETACH)
	{
#ifdef _DEBUG
		if (fh)
		{
			fclose(fh);
			fh = NULL;
		}
#endif

		/* Read winmm.ini */
		int bAutoClose = GetPrivateProfileInt("winmm", "AutoClose", 0, ".\\winmm.ini");
		
		if(bAutoClose)
		{
			/* Write exit message for cdaudioplr.exe: */
			HANDLE Mailslot = CreateFile(ServerName, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
			WriteFile(Mailslot, "1 exit", 64, &BytesWritten, NULL);
			CloseHandle(Mailslot);
		}
	}

	return TRUE;
}

MCIERROR WINAPI fake_mciSendCommandA(MCIDEVICEID IDDevice, UINT uMsg, DWORD_PTR fdwCommand, DWORD_PTR dwParam)
{
	if(once){
		Sleep(100); /* Sleep a bit to ensure cdaudioplr.exe is initialized. */
		once = 0;
	}
	char cmdbuf[1024];

	dprintf("mciSendCommandA(IDDevice=%p, uMsg=%p, fdwCommand=%p, dwParam=%p)\r\n", IDDevice, uMsg, fdwCommand, dwParam);

	if (fdwCommand & MCI_NOTIFY)
	{
		dprintf("  MCI_NOTIFY\r\n");

		/* Write notify message reguest: */
		HANDLE Mailslot = CreateFile(ServerName, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		WriteFile(Mailslot, "1 mci_notify", 64, &BytesWritten, NULL);
		CloseHandle(Mailslot);
	}

	if (fdwCommand & MCI_WAIT)
	{
		dprintf("  MCI_WAIT\r\n");
	}

	if (uMsg == MCI_OPEN)
	{
		LPMCI_OPEN_PARMS parms = (LPVOID)dwParam;

		dprintf("  MCI_OPEN\r\n");

		if (fdwCommand & MCI_OPEN_ALIAS)
		{
			dprintf("    MCI_OPEN_ALIAS\r\n");
		}

		if (fdwCommand & MCI_OPEN_SHAREABLE)
		{
			dprintf("    MCI_OPEN_SHAREABLE\r\n");
		}

		if (fdwCommand & MCI_OPEN_TYPE_ID)
		{
			dprintf("    MCI_OPEN_TYPE_ID\r\n");

			if (LOWORD(parms->lpstrDeviceType) == MCI_DEVTYPE_CD_AUDIO)
			{
				dprintf("  Returning magic device id for MCI_DEVTYPE_CD_AUDIO\r\n");
				parms->wDeviceID = MAGIC_DEVICEID;
				return 0;
			}
		}

		if (fdwCommand & MCI_OPEN_TYPE && !(fdwCommand & MCI_OPEN_TYPE_ID))
		{
			dprintf("    MCI_OPEN_TYPE\r\n");
			dprintf("        -> %s\r\n", parms->lpstrDeviceType);

			if (strcmp(parms->lpstrDeviceType, "cdaudio") == 0)
			{
				dprintf("  Returning magic device id for MCI_DEVTYPE_CD_AUDIO\r\n");
				parms->wDeviceID = MAGIC_DEVICEID;
				return 0;
			}
		}
	}

	if (IDDevice == MAGIC_DEVICEID || IDDevice == 0 || IDDevice == 0xFFFFFFFF)
	{
		if (uMsg == MCI_SET)
		{
			LPMCI_SET_PARMS parms = (LPVOID)dwParam;

			dprintf("  MCI_SET\r\n");

			if (fdwCommand & MCI_SET_TIME_FORMAT)
			{
				dprintf("    MCI_SET_TIME_FORMAT\r\n");

				time_format = parms->dwTimeFormat;

				if (parms->dwTimeFormat == MCI_FORMAT_BYTES)
				{
					dprintf("      MCI_FORMAT_BYTES\r\n");
				}

				if (parms->dwTimeFormat == MCI_FORMAT_FRAMES)
				{
					dprintf("      MCI_FORMAT_FRAMES\r\n");
				}

				if (parms->dwTimeFormat == MCI_FORMAT_HMS)
				{
					dprintf("      MCI_FORMAT_HMS\r\n");
				}

				if (parms->dwTimeFormat == MCI_FORMAT_MILLISECONDS)
				{
					dprintf("      MCI_FORMAT_MILLISECONDS\r\n");
				}

				if (parms->dwTimeFormat == MCI_FORMAT_MSF)
				{
					dprintf("      MCI_FORMAT_MSF\r\n");
				}

				if (parms->dwTimeFormat == MCI_FORMAT_SAMPLES)
				{
					dprintf("      MCI_FORMAT_SAMPLES\r\n");
				}

				if (parms->dwTimeFormat == MCI_FORMAT_TMSF)
				{
					dprintf("      MCI_FORMAT_TMSF\r\n");
				}
			}
		}

		if (uMsg == MCI_CLOSE)
		{
			dprintf("  MCI_CLOSE\r\n");
		}

		if (uMsg == MCI_PLAY)
		{
			LPMCI_PLAY_PARMS parms = (LPVOID)dwParam;

			dprintf("  MCI_PLAY\r\n");

			if (fdwCommand & MCI_FROM)
			{
				dprintf("    dwFrom: %d\r\n", parms->dwFrom);
				
				/* FIXME: rounding to nearest track */
				if (time_format == MCI_FORMAT_TMSF)
				{
					/* Write MCI_FROM: */
					HANDLE Mailslot = CreateFile(ServerName, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
					snprintf(dwFrom_str, 32, "%d", MCI_TMSF_TRACK(parms->dwFrom));
					WriteFile(Mailslot, strcat(dwFrom_str, " mci_from"), 64, &BytesWritten, NULL);
					CloseHandle(Mailslot);
					
					dprintf("      TRACK  %d\n", MCI_TMSF_TRACK(parms->dwFrom));
					dprintf("      MINUTE %d\n", MCI_TMSF_MINUTE(parms->dwFrom));
					dprintf("      SECOND %d\n", MCI_TMSF_SECOND(parms->dwFrom));
					dprintf("      FRAME  %d\n", MCI_TMSF_FRAME(parms->dwFrom));
				}
				else if (time_format == MCI_FORMAT_MILLISECONDS)
				{
					int track_ms;
					track_ms = parms->dwFrom / 60000;
					
					HANDLE Mailslot = CreateFile(ServerName, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
					snprintf(dwFrom_str, 32, "%d", track_ms);
					WriteFile(Mailslot, strcat(dwFrom_str, " mci_from"), 64, &BytesWritten, NULL);
					CloseHandle(Mailslot);
					
					dprintf("      mapped milliseconds from %d\n", track_ms);
				}
			}

			if (fdwCommand & MCI_TO)
			{
				dprintf("    dwTo:   %d\r\n", parms->dwTo);
				
				if (time_format == MCI_FORMAT_TMSF)
				{
					/* Write MCI_TO: */
					HANDLE Mailslot = CreateFile(ServerName, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
					snprintf(dwTo_str, 32, "%d", MCI_TMSF_TRACK(parms->dwTo));
					WriteFile(Mailslot, strcat(dwTo_str, " mci_to"), 64, &BytesWritten, NULL);
					CloseHandle(Mailslot);
					
					dprintf("      TRACK  %d\n", MCI_TMSF_TRACK(parms->dwTo));
					dprintf("      MINUTE %d\n", MCI_TMSF_MINUTE(parms->dwTo));
					dprintf("      SECOND %d\n", MCI_TMSF_SECOND(parms->dwTo));
					dprintf("      FRAME  %d\n", MCI_TMSF_FRAME(parms->dwTo));
				}
				else if (time_format == MCI_FORMAT_MILLISECONDS)
				{
					dprintf("      mapped milliseconds to ?\n");
				}
			}

			/* MCI_PLAY NULL = resume playback */
			if((fdwCommand & (MCI_FROM|MCI_TO)) == 0 && paused)
			{
				/* Write MCI_RESUME: */
				HANDLE Mailslot = CreateFile(ServerName, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
				WriteFile(Mailslot, "1 mci_resume", 64, &BytesWritten, NULL);
				CloseHandle(Mailslot);
			}
		/* Wait for mode change. Max 3sec sleep. */
		while(mode == 1 && counter < 300)
		{
			Sleep(10); /* Wait for mode change. */
			counter ++;
		}
		counter = 0;
		playing = 1;
		paused = 0;
		}

		if (uMsg == MCI_STOP)
		{
			dprintf("  MCI_STOP\r\n");
			playing = 0;

			/* Write MCI_STOP: */
			HANDLE Mailslot = CreateFile(ServerName, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
			WriteFile(Mailslot, "1 mci_stop", 64, &BytesWritten, NULL);
			CloseHandle(Mailslot);
			
			/* Wait for mode change. Max 3sec sleep. */
			while(mode == 2 && counter < 300)
			{
				Sleep(10); /* Wait for mode change. */
				counter ++;
			}
			counter = 0;
		}

		if (uMsg == MCI_PAUSE)
		{
			dprintf("  MCI_PAUSE\r\n");
			playing = 0;
			paused = 1;

			/* Write MCI_PAUSE: */
			HANDLE Mailslot = CreateFile(ServerName, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
			WriteFile(Mailslot, "1 mci_pause", 64, &BytesWritten, NULL);
			CloseHandle(Mailslot);
			
			/* Wait for mode change. Max 3sec sleep. */
			while(mode == 2 && counter < 300)
			{
				Sleep(10); /* Wait for mode change. */
				counter ++;
			}
			counter = 0;
		}

		/* Handling of MCI_SYSINFO (Heavy Gear, Battlezone2, Interstate 76) */
		if (uMsg == MCI_SYSINFO)
		{
			dprintf("  MCI_SYSINFO\r\n");
			LPMCI_SYSINFO_PARMSA parms = (LPVOID)dwParam;

			if(fdwCommand & MCI_SYSINFO_QUANTITY)
			{
				dprintf("	 MCI_SYSINFO_QUANTITY\r\n");
				memcpy((LPVOID)(parms->lpstrReturn), (LPVOID)&"1", 2); /* quantity = 1 */
				parms->dwRetSize = sizeof(DWORD);
				parms->dwNumber = MAGIC_DEVICEID;
				dprintf("        Return: %s\r\n", parms->lpstrReturn);
			}

			if(fdwCommand & MCI_SYSINFO_NAME)
			{
				dprintf("	 MCI_SYSINFO_NAME\r\n");
				memcpy((LPVOID)(parms->lpstrReturn), (LPVOID)&"cdaudio", 8); /* name = cdaudio */
				parms->dwRetSize = sizeof(DWORD);
				parms->dwNumber = MAGIC_DEVICEID;
				dprintf("        Return: %s\r\n", parms->lpstrReturn);
			}
		}

		if (uMsg == MCI_STATUS)
		{
			LPMCI_STATUS_PARMS parms = (LPVOID)dwParam;

			dprintf("  MCI_STATUS\r\n");

			parms->dwReturn = 0;

			if (fdwCommand & MCI_TRACK)
			{
				dprintf("    MCI_TRACK\r\n");
				dprintf("      dwTrack = %d\r\n", parms->dwTrack);
			}

			if (fdwCommand & MCI_STATUS_ITEM)
			{
				dprintf("    MCI_STATUS_ITEM\r\n");

				if (parms->dwItem == MCI_STATUS_CURRENT_TRACK)
				{
					dprintf("      MCI_STATUS_CURRENT_TRACK\r\n");
				}

				if (parms->dwItem == MCI_STATUS_LENGTH)
				{
					dprintf("	   MCI_STATUS_LENGTH\r\n");

					/* Get track length */
					if(fdwCommand & MCI_TRACK)
					{
						if (time_format == MCI_FORMAT_MILLISECONDS)
						{
							parms->dwReturn = 60000; /*fake 1 min in ms*/
						}
						else
						{
							parms->dwReturn = 1; /* Fake track length (1min)*/
						}
					}
					/* Get full length */
					else
					{
						if (time_format == MCI_FORMAT_MILLISECONDS)
						{
							parms->dwReturn = 60000 * numTracks;
						}
						else
						{
							parms->dwReturn = numTracks;
						}
					}
				}

				if (parms->dwItem == MCI_CDA_STATUS_TYPE_TRACK) {
					/* ref. by WinQuake */
					dprintf("> MCI_CDA_STATUS_TYPE_TRACK\n");
					parms->dwReturn = MCI_CDA_TRACK_AUDIO; 
				}

				if (parms->dwItem == MCI_STATUS_MEDIA_PRESENT)
				{
					dprintf("      MCI_STATUS_MEDIA_PRESENT\r\n");
					parms->dwReturn = TRUE;
				}

				if (parms->dwItem == MCI_STATUS_NUMBER_OF_TRACKS)
				{
					dprintf("      MCI_STATUS_NUMBER_OF_TRACKS\r\n");
					parms->dwReturn = numTracks;
				}

				if (parms->dwItem == MCI_STATUS_POSITION)
				{
					/* Track position */
					dprintf("	   MCI_STATUS_POSITION\r\n");

					if (fdwCommand & MCI_TRACK)
					{
						if (time_format == MCI_FORMAT_MILLISECONDS)
							/* FIXME: implying milliseconds */
							parms->dwReturn = parms->dwTrack * 60000;
						else /* TMSF */
							parms->dwReturn = MCI_MAKE_TMSF(parms->dwTrack, 0, 0, 0);
					}
					else {
						/* Current position */
						if (time_format == MCI_FORMAT_MILLISECONDS)
							parms->dwReturn = 2001; /* Default pos for ms*/
						else /* TMSF */
						{
							int tmp_num = atoi(dwFrom_str);
							if(tmp_num <= 0) tmp_num = 1;
							parms->dwReturn = tmp_num; /* Default position */
						}
					}
				}

				if (parms->dwItem == MCI_STATUS_MODE)
				{
					dprintf("      MCI_STATUS_MODE\r\n");
					if(mode == 1 || playing == 0){
						parms->dwReturn = MCI_MODE_STOP;
						dprintf("        Stopped\r\n");
					}
					else{
						parms->dwReturn = MCI_MODE_PLAY;
						dprintf("        Playing\r\n");
					}
				}

				if (parms->dwItem == MCI_STATUS_READY) {
					/* referenced by Quake/cd_win.c */
					dprintf("> MCI_STATUS_READY\n");
					parms->dwReturn = TRUE;
				}

				if (parms->dwItem == MCI_STATUS_TIME_FORMAT)
				{
					dprintf("      MCI_STATUS_TIME_FORMAT\r\n");
				}

				if (parms->dwItem == MCI_STATUS_START)
				{
					dprintf("      MCI_STATUS_START\r\n");
				}
			}

			dprintf("  dwReturn %d\n", parms->dwReturn);

		}

		return 0;
	}

	/* fallback */
	return MCIERR_UNRECOGNIZED_COMMAND;

}

MCIERROR WINAPI fake_mciSendStringA(LPCTSTR cmd, LPTSTR ret, UINT cchReturn, HANDLE hwndCallback)
{
	char cmdbuf[1024];
	char cmp_str[1024];

	dprintf("[MCI String = %s]\n", cmd);

	/* copy cmd into cmdbuf */
	strcpy (cmdbuf,cmd);
	/* change cmdbuf into lower case */
	for (int i = 0; cmdbuf[i]; i++)
	{
		cmdbuf[i] = tolower(cmdbuf[i]);
	}

	if (strstr(cmdbuf, "sysinfo cdaudio quantity"))
	{
		dprintf("  Returning quantity: 1\r\n");
		strcpy(ret, "1");
		return 0;
	}

	/* Example: "sysinfo cdaudio name 1 open" returns "cdaudio" or the alias.*/
	if (strstr(cmdbuf, "sysinfo cdaudio name"))
	{
		dprintf("  Returning name: cdaudio\r\n");
		sprintf(ret, "%s", alias_s);
		return 0;
	}

	/* Handle "stop cdaudio/alias" */
	sprintf(cmp_str, "stop %s", alias_s);
	if (strstr(cmdbuf, cmp_str))
	{
		fake_mciSendCommandA(MAGIC_DEVICEID, MCI_STOP, 0, (DWORD_PTR)NULL);
		return 0;
	}

	/* Handle "pause cdaudio/alias" */
	sprintf(cmp_str, "pause %s", alias_s);
	if (strstr(cmdbuf, cmp_str))
	{
		fake_mciSendCommandA(MAGIC_DEVICEID, MCI_PAUSE, 0, (DWORD_PTR)NULL);
		return 0;
	}

	/* Look for the use of an alias */
	/* Example: "open d: type cdaudio alias cd1" */
	if (strstr(cmdbuf, "type cdaudio alias"))
	{
		char *tmp_s = strrchr(cmdbuf, ' ');
		if (tmp_s && *(tmp_s +1))
		{
			sprintf(alias_s, "%s", tmp_s +1);
		}
		fake_mciSendCommandA(MAGIC_DEVICEID, MCI_OPEN, 0, (DWORD_PTR)NULL);
		return 0;
	}

	if (strstr(cmdbuf, "open cdaudio"))
	{
		fake_mciSendCommandA(MAGIC_DEVICEID, MCI_OPEN, 0, (DWORD_PTR)NULL);
		return 0;
	}

	/* reset alias with "close alias" string */
	sprintf(cmp_str, "close %s", alias_s);
	if (strstr(cmdbuf, cmp_str))
	{
		sprintf(alias_s, "cdaudio");
		return 0;
	}

	/* Handle "set cdaudio/alias time format" */

	sprintf(cmp_str, "set %s time format", alias_s);
	if (strstr(cmdbuf, cmp_str)){
		if (strstr(cmdbuf, "milliseconds"))
		{
			static MCI_SET_PARMS parms;
			parms.dwTimeFormat = MCI_FORMAT_MILLISECONDS;
			fake_mciSendCommandA(MAGIC_DEVICEID, MCI_SET, MCI_SET_TIME_FORMAT, (DWORD_PTR)&parms);
			return 0;
		}
		if (strstr(cmdbuf, "tmsf"))
		{
			static MCI_SET_PARMS parms;
			parms.dwTimeFormat = MCI_FORMAT_TMSF;
			fake_mciSendCommandA(MAGIC_DEVICEID, MCI_SET, MCI_SET_TIME_FORMAT, (DWORD_PTR)&parms);
			return 0;
		}
		if (strstr(cmdbuf, "msf"))
		{
			static MCI_SET_PARMS parms;
			parms.dwTimeFormat = MCI_FORMAT_MSF;
			fake_mciSendCommandA(MAGIC_DEVICEID, MCI_SET, MCI_SET_TIME_FORMAT, (DWORD_PTR)&parms);
			return 0;
		}
	}

	/* Handle "status cdaudio/alias" */
	sprintf(cmp_str, "status %s", alias_s);
	if (strstr(cmdbuf, cmp_str)){
		if (strstr(cmdbuf, "number of tracks"))
		{
			dprintf("  Returning number of tracks (%d)\r\n", numTracks);
			sprintf(ret, "%d", numTracks);
			return 0;
		}
		int track = 0;
		if (sscanf(cmdbuf, "status %*s length track %d", &track) == 1)
		{
			static MCI_STATUS_PARMS parms;
			parms.dwItem = MCI_STATUS_LENGTH;
			parms.dwTrack = track;
			fake_mciSendCommandA(MAGIC_DEVICEID, MCI_STATUS, MCI_STATUS_ITEM|MCI_TRACK, (DWORD_PTR)&parms);
			sprintf(ret, "%d", parms.dwReturn);
			return 0;
		}
		if (strstr(cmdbuf, "length"))
		{
			static MCI_STATUS_PARMS parms;
			parms.dwItem = MCI_STATUS_LENGTH;
			fake_mciSendCommandA(MAGIC_DEVICEID, MCI_STATUS, MCI_STATUS_ITEM, (DWORD_PTR)&parms);
			sprintf(ret, "%d", parms.dwReturn);
			return 0;
		}
		if (sscanf(cmdbuf, "status %*s position track %d", &track) == 1)
		{
			static MCI_STATUS_PARMS parms;
			parms.dwItem = MCI_STATUS_POSITION;
			parms.dwTrack = track;
			fake_mciSendCommandA(MAGIC_DEVICEID, MCI_STATUS, MCI_STATUS_ITEM|MCI_TRACK, (DWORD_PTR)&parms);
			sprintf(ret, "%d", parms.dwReturn);
			return 0;
		}
		if (strstr(cmdbuf, "position"))
		{
			static MCI_STATUS_PARMS parms;
			parms.dwItem = MCI_STATUS_POSITION;
			fake_mciSendCommandA(MAGIC_DEVICEID, MCI_STATUS, MCI_STATUS_ITEM, (DWORD_PTR)&parms);
			sprintf(ret, "%d", parms.dwReturn);
			return 0;
		}
		if (strstr(cmdbuf, "media present"))
		{
			strcpy(ret, "TRUE");
			return 0;
		}
		/* Add: Mode handling */
		if (strstr(cmdbuf, "mode"))
		{
			if(mode == 1){
				dprintf("   -> stopped\r\n");
				strcpy(ret, "stopped");
				}
			else{
				dprintf("   -> playing\r\n");
				strcpy(ret, "playing");
			}
			return 0;
		}
	}

	/* Handle "play cdaudio/alias" */
	int from = -1, to = -1;
	sprintf(cmp_str, "play %s", alias_s);
	if (strstr(cmdbuf, cmp_str)){
		if (strstr(cmdbuf, "notify")){
		/* Write notify message reguest: */
		HANDLE Mailslot = CreateFile(ServerName, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		WriteFile(Mailslot, "1 mci_notify", 64, &BytesWritten, NULL);
		CloseHandle(Mailslot);
		}
		if (sscanf(cmdbuf, "play %*s from %d to %d", &from, &to) == 2)
		{
			static MCI_PLAY_PARMS parms;
			parms.dwFrom = from;
			parms.dwTo = to;
			fake_mciSendCommandA(MAGIC_DEVICEID, MCI_PLAY, MCI_FROM|MCI_TO, (DWORD_PTR)&parms);
			return 0;
		}
		if (sscanf(cmdbuf, "play %*s from %d", &from) == 1)
		{
			static MCI_PLAY_PARMS parms;
			parms.dwFrom = from;
			fake_mciSendCommandA(MAGIC_DEVICEID, MCI_PLAY, MCI_FROM, (DWORD_PTR)&parms);
			return 0;
		}
		if (sscanf(cmdbuf, "play %*s to %d", &to) == 1)
		{
			static MCI_PLAY_PARMS parms;
			parms.dwTo = to;
			fake_mciSendCommandA(MAGIC_DEVICEID, MCI_PLAY, MCI_TO, (DWORD_PTR)&parms);
			return 0;
		}
	}

	return 0;
}

UINT WINAPI fake_auxGetNumDevs()
{
	dprintf("fake_auxGetNumDevs()\r\n");
	return 1;
}

MMRESULT WINAPI fake_auxGetDevCapsA(UINT_PTR uDeviceID, LPAUXCAPS lpCaps, UINT cbCaps)
{
	dprintf("fake_auxGetDevCapsA(uDeviceID=%08X, lpCaps=%p, cbCaps=%08X\n", uDeviceID, lpCaps, cbCaps);

	lpCaps->wMid = 2 /*MM_CREATIVE*/;
	lpCaps->wPid = 401 /*MM_CREATIVE_AUX_CD*/;
	lpCaps->vDriverVersion = 1;
	strcpy(lpCaps->szPname, "ogg-winmm virtual CD");
	lpCaps->wTechnology = AUXCAPS_CDAUDIO;
	lpCaps->dwSupport = AUXCAPS_VOLUME;

	return MMSYSERR_NOERROR;
}


MMRESULT WINAPI fake_auxGetVolume(UINT uDeviceID, LPDWORD lpdwVolume)
{
	dprintf("fake_auxGetVolume(uDeviceId=%08X, lpdwVolume=%p)\r\n", uDeviceID, lpdwVolume);
	*lpdwVolume = 0x00000000;
	return MMSYSERR_NOERROR;
}

MMRESULT WINAPI fake_auxSetVolume(UINT uDeviceID, DWORD dwVolume)
{
	if(once){
		Sleep(100); /* Sleep a bit to ensure cdaudioplr.exe is initialized. */
		once = 0;
	}
	static DWORD oldVolume = -1;
	char cmdbuf[256];

	dprintf("fake_auxSetVolume(uDeviceId=%08X, dwVolume=%08X)\r\n", uDeviceID, dwVolume);

	if (dwVolume == oldVolume)
	{
		return MMSYSERR_NOERROR;
	}

	oldVolume = dwVolume;

	unsigned short left = LOWORD(dwVolume);
	unsigned short right = HIWORD(dwVolume);

	dprintf("    left : %ud (%04X)\n", left, left);
	dprintf("    right: %ud (%04X)\n", right, right);
	
	/* Write volume: */
	HANDLE Mailslot = CreateFile(ServerName, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	snprintf(dwVolume_str, 32, "%d", dwVolume);
	WriteFile(Mailslot, strcat(dwVolume_str, " aux_vol"), 64, &BytesWritten, NULL);
	CloseHandle(Mailslot);

	return MMSYSERR_NOERROR;
}
