/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 Storlek <storlek@rigelseven.com>
 * copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
 * copyright (c) 2009 Storlek & Mrs. Brisby
 * copyright (c) 2010-2012 Storlek
 * URL: http://schismtracker.org/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* Predominantly this file is keyboard crap, but we also get the network configured here */

#include "headers.h"

#include "config.h"
#include "it.h"
#include "osdefs.h"
#include "fmt.h"
#include "charset.h"
#include "loadso.h"
#include "mem.h"

#include <ws2tcpip.h>
#include <windows.h>
#include <winerror.h>
#include <process.h>
#include <shlobj.h>

#include <direct.h>

#include <tchar.h>

#define IDM_FILE_NEW  101
#define IDM_FILE_LOAD 102
#define IDM_FILE_SAVE_CURRENT 103
#define IDM_FILE_SAVE_AS 104
#define IDM_FILE_EXPORT 105
#define IDM_FILE_MESSAGE_LOG 106
#define IDM_FILE_QUIT 107
#define IDM_PLAYBACK_SHOW_INFOPAGE 201
#define IDM_PLAYBACK_PLAY_SONG 202
#define IDM_PLAYBACK_PLAY_PATTERN 203
#define IDM_PLAYBACK_PLAY_FROM_ORDER 204
#define IDM_PLAYBACK_PLAY_FROM_MARK_CURSOR 205
#define IDM_PLAYBACK_STOP 206
#define IDM_PLAYBACK_CALCULATE_LENGTH 207
#define IDM_SAMPLES_SAMPLE_LIST 301
#define IDM_SAMPLES_SAMPLE_LIBRARY 302
#define IDM_SAMPLES_RELOAD_SOUNDCARD 303
#define IDM_INSTRUMENTS_INSTRUMENT_LIST 401
#define IDM_INSTRUMENTS_INSTRUMENT_LIBRARY 402
#define IDM_VIEW_HELP 501
#define IDM_VIEW_VIEW_PATTERNS 502
#define IDM_VIEW_ORDERS_PANNING 503
#define IDM_VIEW_VARIABLES 504
#define IDM_VIEW_MESSAGE_EDITOR 505
#define IDM_VIEW_TOGGLE_FULLSCREEN 506
#define IDM_SETTINGS_PREFERENCES 601
#define IDM_SETTINGS_MIDI_CONFIGURATION 602
#define IDM_SETTINGS_PALETTE_EDITOR 603
#define IDM_SETTINGS_FONT_EDITOR 604
#define IDM_SETTINGS_SYSTEM_CONFIGURATION 605

/* global menu object */
static HMENU menu = NULL;

void win32_get_modkey(schism_keymod_t *mk)
{
	// Translation from virtual keys to keymods. We have to do
	// this because SDL's key modifier stuff is quite buggy
	// and has caused weird modifier shenanigans in the past.

	static const struct {
		uint8_t vk;
		schism_keymod_t km;

		// whether this key is a held modifier (i.e.
		// ctrl, alt, shift, win) or is toggled (i.e.
		// numlock, scrolllock)
		int toggle;

		// does this key work on win9x?
		int win9x;
	} conv[] = {
		{VK_NUMLOCK, SCHISM_KEYMOD_NUM, 1, 1},
		{VK_CAPITAL, SCHISM_KEYMOD_CAPS, 1, 1},
		{VK_CAPITAL, SCHISM_KEYMOD_CAPS_PRESSED, 0, 1},
		{VK_LSHIFT, SCHISM_KEYMOD_LSHIFT, 0, 0},
		{VK_RSHIFT, SCHISM_KEYMOD_RSHIFT, 0, 0},
		{VK_LMENU, SCHISM_KEYMOD_LALT, 0, 0},
		{VK_RMENU, SCHISM_KEYMOD_RALT, 0, 0},
		{VK_LCONTROL, SCHISM_KEYMOD_LCTRL, 0, 0},
		{VK_RCONTROL, SCHISM_KEYMOD_RCTRL, 0, 0},
		{VK_LWIN, SCHISM_KEYMOD_LGUI, 0, 0},
		{VK_RWIN, SCHISM_KEYMOD_RGUI, 0, 0},
	};

	const int on_windows_9x = (GetVersion() & UINT32_C(0x80000000));

	// Sometimes GetKeyboardState is out of date and calling GetKeyState
	// fixes it. Any random key will work.
	(void)GetKeyState(VK_CAPITAL);

	BYTE ks[256] = {0};
	if (!GetKeyboardState(ks)) return;

	for (int i = 0; i < ARRAY_SIZE(conv); i++) {
		// FIXME: Some keys (notably left and right variations) simply
		// do not get filled by windows 9x and as such get completely
		// ignored by this code. In that case just use whatever values
		// SDL gave and punt.
		if (on_windows_9x && !conv[i].win9x)
			continue;

		// Clear the original value
		(*mk) &= ~(conv[i].km);

		// Put in our result
		if (ks[conv[i].vk] & (conv[i].toggle ? 0x01 : 0x80))
			(*mk) |= conv[i].km;
	}
}

void win32_sysinit(SCHISM_UNUSED int *pargc, SCHISM_UNUSED char ***pargv)
{
	/* Initialize winsocks */
	static WSADATA ignored = {0};

	if (WSAStartup(0x202, &ignored) == SOCKET_ERROR) {
		WSACleanup(); /* ? */
		status.flags |= NO_NETWORK;
	}

	/* Build the menus */
	menu = CreateMenu();
	{
		HMENU file = CreatePopupMenu();
		AppendMenuA(file, MF_STRING, IDM_FILE_NEW, "&New\tCtrl+N");
		AppendMenuA(file, MF_STRING, IDM_FILE_LOAD, "&Load\tF9");
		AppendMenuA(file, MF_STRING, IDM_FILE_SAVE_CURRENT, "&Save Current\tCtrl+S");
		AppendMenuA(file, MF_STRING, IDM_FILE_SAVE_AS, "Save &As...\tF10");
		AppendMenuA(file, MF_STRING, IDM_FILE_EXPORT, "&Export...\tShift+F10");
		AppendMenuA(file, MF_STRING, IDM_FILE_MESSAGE_LOG, "&Message Log\tCtrl+F11");
		AppendMenuA(file, MF_SEPARATOR, 0, NULL);
		AppendMenuA(file, MF_STRING, IDM_FILE_QUIT, "&Quit\tCtrl+Q");
		AppendMenuA(menu, MF_POPUP, (uintptr_t)file, "&File");
	}
	{
		/* this is equivalent to the "Schism Tracker" menu on Mac OS X */
		HMENU view = CreatePopupMenu();
		AppendMenuA(view, MF_STRING, IDM_VIEW_HELP, "Help\tF1");
		AppendMenuA(view, MF_SEPARATOR, 0, NULL);
		AppendMenuA(view, MF_STRING, IDM_VIEW_VIEW_PATTERNS, "View Patterns\tF2");
		AppendMenuA(view, MF_STRING, IDM_VIEW_ORDERS_PANNING, "Orders/Panning\tF11");
		AppendMenuA(view, MF_STRING, IDM_VIEW_VARIABLES, "Variables\tF12");
		AppendMenuA(view, MF_STRING, IDM_VIEW_MESSAGE_EDITOR, "Message Editor\tShift+F9");
		AppendMenuA(view, MF_SEPARATOR, 0, NULL);
		AppendMenuA(view, MF_STRING, IDM_VIEW_TOGGLE_FULLSCREEN, "Toggle Fullscreen\tCtrl+Alt+Return");
		AppendMenuA(menu, MF_POPUP, (uintptr_t)view, "&View");
	}
	{
		HMENU playback = CreatePopupMenu();
		AppendMenuA(playback, MF_STRING, IDM_PLAYBACK_SHOW_INFOPAGE, "Show Infopage\tF5");
		AppendMenuA(playback, MF_STRING, IDM_PLAYBACK_PLAY_SONG, "Play Song\tCtrl+F5");
		AppendMenuA(playback, MF_STRING, IDM_PLAYBACK_PLAY_PATTERN, "Play Pattern\tF6");
		AppendMenuA(playback, MF_STRING, IDM_PLAYBACK_PLAY_FROM_ORDER, "Play from Order\tShift+F6");
		AppendMenuA(playback, MF_STRING, IDM_PLAYBACK_PLAY_FROM_MARK_CURSOR, "Play from Mark/Cursor\tF7");
		AppendMenuA(playback, MF_STRING, IDM_PLAYBACK_STOP, "Stop\tF8");
		AppendMenuA(playback, MF_STRING, IDM_PLAYBACK_CALCULATE_LENGTH, "Calculate Length\tCtrl+P");
		AppendMenuA(menu, MF_POPUP, (uintptr_t)playback, "&Playback");
	}
	{
		HMENU samples = CreatePopupMenu();
		AppendMenuA(samples, MF_STRING, IDM_SAMPLES_SAMPLE_LIST, "&Sample List\tF3");
		AppendMenuA(samples, MF_STRING, IDM_SAMPLES_SAMPLE_LIBRARY, "Sample &Library\tShift+F3");
		AppendMenuA(samples, MF_STRING, IDM_SAMPLES_RELOAD_SOUNDCARD, "&Reload Soundcard\tCtrl+G");
		AppendMenuA(menu, MF_POPUP, (uintptr_t)samples, "&Samples");
	}
	{
		HMENU instruments = CreatePopupMenu();
		AppendMenuA(instruments, MF_STRING, IDM_INSTRUMENTS_INSTRUMENT_LIST, "Instrument List\tF4");
		AppendMenuA(instruments, MF_STRING, IDM_INSTRUMENTS_INSTRUMENT_LIBRARY, "Instrument Library\tShift+F4");
		AppendMenuA(menu, MF_POPUP, (uintptr_t)instruments, "&Instruments");
	}
	{
		HMENU settings = CreatePopupMenu();
		AppendMenuA(settings, MF_STRING, IDM_SETTINGS_PREFERENCES, "Preferences\tShift+F5");
		AppendMenuA(settings, MF_STRING, IDM_SETTINGS_MIDI_CONFIGURATION, "MIDI Configuration\tShift+F1");
		AppendMenuA(settings, MF_STRING, IDM_SETTINGS_PALETTE_EDITOR, "Palette Editor\tCtrl+F12");
		AppendMenuA(settings, MF_STRING, IDM_SETTINGS_FONT_EDITOR, "Font Editor\tShift+F12");
		AppendMenuA(settings, MF_STRING, IDM_SETTINGS_SYSTEM_CONFIGURATION, "System Configuration\tCtrl+F1");
		AppendMenuA(menu, MF_POPUP, (uintptr_t)settings, "S&ettings");
	}

#ifdef USE_MEDIAFOUNDATION
	win32mf_init();
#endif

	/* Convert command line arguments to UTF-8 */
	{
		char **utf8_argv;
		int utf8_argc;

		int i;

		// Windows NT: use Unicode arguments if available
		LPWSTR cmdline = GetCommandLineW();
		if (cmdline) {
			LPWSTR *argvw = CommandLineToArgvW(cmdline, &utf8_argc);

			if (argvw) {
				// now we have Unicode arguments, so convert them to UTF-8
				utf8_argv = mem_alloc(sizeof(char *) * utf8_argc);

				for (i = 0; i < utf8_argc; i++) {
					charset_iconv(argvw[i], &utf8_argv[i], CHARSET_WCHAR_T, CHARSET_CHAR, SIZE_MAX);
					if (!utf8_argv[i])
						utf8_argv[i] = str_dup(""); // ...
				}

				LocalFree(argvw);

				goto have_utf8_args;
			}
		}

		// well, that didn't work, fallback to ANSI.
		utf8_argc = *pargc;
		utf8_argv = mem_alloc(sizeof(char *) * utf8_argc);

		for (i = 0; i < utf8_argc; i++) {
			charset_iconv((*pargv)[i], &utf8_argv[i], CHARSET_ANSI, CHARSET_CHAR, SIZE_MAX);
			if (!utf8_argv[i])
				utf8_argv[i] = str_dup(""); // ...
		}

have_utf8_args: ;
		*pargv = utf8_argv;
		*pargc = utf8_argc;
	}
}

void win32_sysexit(void)
{
#ifdef USE_MEDIAFOUNDATION
	win32mf_quit();
#endif
}

int win32_event(schism_event_t *event)
{
	if (event->type == SCHISM_EVENT_WM_MSG) {
		if (event->wm_msg.subsystem != SCHISM_WM_MSG_SUBSYSTEM_WINDOWS)
			return 1;

		if (event->wm_msg.msg.win.msg == WM_COMMAND) {
			schism_event_t e = {0};
			e.type = SCHISM_EVENT_NATIVE_SCRIPT;
			switch (LOWORD(event->wm_msg.msg.win.wparam)) {
			case IDM_FILE_NEW:
				e.script.which = str_dup("new");
				break;
			case IDM_FILE_LOAD:
				e.script.which = str_dup("load");
				break;
			case IDM_FILE_SAVE_CURRENT:
				e.script.which = str_dup("save");
				break;
			case IDM_FILE_SAVE_AS:
				e.script.which = str_dup("save_as");
				break;
			case IDM_FILE_EXPORT:
				e.script.which = str_dup("export_song");
				break;
			case IDM_FILE_MESSAGE_LOG:
				e.script.which = str_dup("logviewer");
				break;
			case IDM_FILE_QUIT:
				e.type = SCHISM_QUIT;
				break;
			case IDM_PLAYBACK_SHOW_INFOPAGE:
				e.script.which = str_dup("info");
				break;
			case IDM_PLAYBACK_PLAY_SONG:
				e.script.which = str_dup("play");
				break;
			case IDM_PLAYBACK_PLAY_PATTERN:
				e.script.which = str_dup("play_pattern");
				break;
			case IDM_PLAYBACK_PLAY_FROM_ORDER:
				e.script.which = str_dup("play_order");
				break;
			case IDM_PLAYBACK_PLAY_FROM_MARK_CURSOR:
				e.script.which = str_dup("play_mark");
				break;
			case IDM_PLAYBACK_STOP:
				e.script.which = str_dup("stop");
				break;
			case IDM_PLAYBACK_CALCULATE_LENGTH:
				e.script.which = str_dup("calc_length");
				break;
			case IDM_SAMPLES_SAMPLE_LIST:
				e.script.which = str_dup("sample_page");
				break;
			case IDM_SAMPLES_SAMPLE_LIBRARY:
				e.script.which = str_dup("sample_library");
				break;
			case IDM_SAMPLES_RELOAD_SOUNDCARD:
				e.script.which = str_dup("init_sound");
				break;
			case IDM_INSTRUMENTS_INSTRUMENT_LIST:
				e.script.which = str_dup("inst_page");
				break;
			case IDM_INSTRUMENTS_INSTRUMENT_LIBRARY:
				e.script.which = str_dup("inst_library");
				break;
			case IDM_VIEW_HELP:
				e.script.which = str_dup("help");
				break;
			case IDM_VIEW_VIEW_PATTERNS:
				e.script.which = str_dup("pattern");
				break;
			case IDM_VIEW_ORDERS_PANNING:
				e.script.which = str_dup("orders");
				break;
			case IDM_VIEW_VARIABLES:
				e.script.which = str_dup("variables");
				break;
			case IDM_VIEW_MESSAGE_EDITOR:
				e.script.which = str_dup("message_edit");
				break;
			case IDM_VIEW_TOGGLE_FULLSCREEN:
				e.script.which = str_dup("fullscreen");
				break;
			case IDM_SETTINGS_PREFERENCES:
				e.script.which = str_dup("preferences");
				break;
			case IDM_SETTINGS_MIDI_CONFIGURATION:
				e.script.which = str_dup("midi_config");
				break;
			case IDM_SETTINGS_PALETTE_EDITOR:
				e.script.which = str_dup("palette_page");
				break;
			case IDM_SETTINGS_FONT_EDITOR:
				e.script.which = str_dup("font_editor");
				break;
			case IDM_SETTINGS_SYSTEM_CONFIGURATION:
				e.script.which = str_dup("system_config");
				break;
			default:
				return 0;
			}

			*event = e;
			return 1;
		} else if (event->wm_msg.msg.win.msg == WM_DROPFILES) {
			/* Drag and drop support */
			schism_event_t e = {0};
			e.type = SCHISM_DROPFILE;

			HDROP drop = (HDROP)event->wm_msg.msg.win.wparam;

#ifdef SCHISM_WIN32_COMPILE_ANSI
			if (GetVersion() & UINT32_C(0x80000000)) {
				int needed = DragQueryFileA(drop, 0, NULL, 0);

				char *f = mem_alloc((needed + 1) * sizeof(char));
				DragQueryFileA(drop, 0, f, needed);
				f[needed] = 0;

				charset_iconv(f, &e.drop.file, CHARSET_ANSI, CHARSET_CHAR, needed + 1);
			} else
#endif
			{
				int needed = DragQueryFileW(drop, 0, NULL, 0);

				wchar_t *f = mem_alloc((needed + 1) * sizeof(wchar_t));
				DragQueryFileW(drop, 0, f, needed + 1);
				f[needed] = 0;

				charset_iconv(f, &e.drop.file, CHARSET_WCHAR_T, CHARSET_CHAR, (needed + 1) * sizeof(wchar_t));
			}

			if (!e.drop.file)
				return 0;

			*event = e;
			return 1;
		}

		return 0;
	} else if (event->type == SCHISM_KEYDOWN || event->type == SCHISM_KEYUP) {
		// We get bogus keydowns for Ctrl-Pause.
		// As a workaround, we can check what Windows thinks, but only for Right Ctrl.
		// Left Ctrl just gets completely ignored and there's nothing we can do about it.
		if (event->key.sym == SCHISM_KEYSYM_SCROLLLOCK && (event->key.mod & SCHISM_KEYMOD_RCTRL) && !(GetKeyState(VK_SCROLL) & 0x80))
			event->key.sym = SCHISM_KEYSYM_PAUSE;

		return 1;
	}

	return 1;
}

void win32_toggle_menu(void *window, int on)
{
	SetMenu((HWND)window, (cfg_video_want_menu_bar && on) ? menu : NULL);
	DrawMenuBar((HWND)window);
}

/* -------------------------------------------------------------------- */

void win32_show_message_box(const char *title, const char *text)
{
#ifdef SCHISM_WIN32_COMPILE_ANSI
	if (GetVersion() & UINT32_C(0x80000000)) {
		char *title_a = NULL, *text_a = NULL;
		if (!charset_iconv(title, &title_a, CHARSET_UTF8, CHARSET_ANSI, SIZE_MAX)
			&& !charset_iconv(text, &text_a, CHARSET_UTF8, CHARSET_ANSI, SIZE_MAX))
			MessageBoxA(NULL, text_a, title_a, MB_OK | MB_ICONINFORMATION);
		free(title_a);
		free(text_a);
	} else
#endif
	{
		wchar_t *title_w = NULL, *text_w = NULL;
		if (!charset_iconv(title, &title_w, CHARSET_UTF8, CHARSET_WCHAR_T, SIZE_MAX)
			&& !charset_iconv(text, &text_w, CHARSET_UTF8, CHARSET_WCHAR_T, SIZE_MAX))
			MessageBoxW(NULL, text_w, title_w, MB_OK | MB_ICONINFORMATION);
		free(title_w);
		free(text_w);
	}
}

/* -------------------------------------------------------------------- */
/* Key repeat */

int win32_get_key_repeat(int *pdelay, int *prate)
{
	DWORD speed, delay;
	if (!SystemParametersInfoA(SPI_GETKEYBOARDSPEED, 0, &speed, 0)
		|| !SystemParametersInfoA(SPI_GETKEYBOARDDELAY, 0, &delay, 0))
		return 0;

	// Sanity check
	if (speed > 31 || delay > 3)
		return 0;

	// This value is somewhat odd; it's a value from
	// 0 - 31, and even weirder is that it's non-linear,
	// that is, 0 is about a repeat every 400 ms and 31
	// is a repeat every ~33.33 ms.
	//
	// Eventually I came up with this formula to translate
	// it to the repeat rate in milliseconds.
	*prate = (int)(1000.0/((speed/(62.0/55.0)) + 2.5));

	// This one is much simpler.
	*pdelay = (delay + 1) * 250;

	return 1;
}

/* -------------------------------------------------------------------- */

// Checks for at least some NT kernel version.
// Note that this does NOT work for checking above Windows 8.1 because
// Microsoft. This also fails for Windows 9x, because it doesn't
// have an NT kernel at all.
int win32_ntver_atleast(int major, int minor, int build)
{
	DWORD version = GetVersion();
	if (version & 0x80000000)
		return 0;

	return SCHISM_SEMVER_ATLEAST(major, minor, build,
		LOBYTE(LOWORD(version)), HIBYTE(LOWORD(version)), HIWORD(version));
}

/* -------------------------------------------------------------------- */

// By default, waveout devices are limited to 31 chars, which means we get
// lovely device names like
//  > Headphones (USB-C to 3.5mm Head
// Doing this gives us access to longer and more "general" devices names,
// such as
//  > G432 Gaming Headset
// but only if the device supports it!
//
// We do this for DirectSound as well mainly to provide some sort of
// reliability with the device names.
int win32_audio_lookup_device_name(const void *nameguid, char **result)
{
	// format for printing GUIDs with printf
#define GUIDF "%08" PRIx32 "-%04" PRIx16 "-%04" PRIx16 "-%02" PRIx8 "%02" PRIx8 "-%02" PRIx8 "%02" PRIx8 "%02" PRIx8 "%02" PRIx8 "%02" PRIx8 "%02" PRIx8
#define GUIDX(x) (x).Data1, (x).Data2, (x).Data3, (x).Data4[0], (x).Data4[1], (x).Data4[2], (x).Data4[3], (x).Data4[4], (x).Data4[5], (x).Data4[6], (x).Data4[7]
	// Set this to NULL before doing anything
	*result = NULL;

	WCHAR *strw = NULL;
	DWORD len = 0;

	static const GUID nullguid = {0};
	if (!memcmp(nameguid, &nullguid, sizeof(nullguid)))
		return 0;

	{
		HKEY hkey;
		DWORD type;

		WCHAR keystr[256] = {0};
		_snwprintf(keystr, ARRAY_SIZE(keystr) - 1, L"System\\CurrentControlSet\\Control\\MediaCategories\\{" GUIDF "}", GUIDX(*(const GUID *)nameguid));

		if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, keystr, 0, KEY_QUERY_VALUE, &hkey) != ERROR_SUCCESS)
			return 0;

		if (RegQueryValueExW(hkey, L"Name", NULL, &type, NULL, &len) != ERROR_SUCCESS || type != REG_SZ) {
			RegCloseKey(hkey);
			return 0;
		}

		strw = mem_alloc(len + sizeof(WCHAR));

		if (RegQueryValueExW(hkey, L"Name", NULL, NULL, (LPBYTE)strw, &len) != ERROR_SUCCESS) {
			RegCloseKey(hkey);
			free(strw);
			return 0;
		}

		RegCloseKey(hkey);
	}

	// force NUL terminate
	strw[len >> 1] = L'\0';

	if (charset_iconv(strw, result, CHARSET_WCHAR_T, CHARSET_UTF8, len + sizeof(WCHAR))) {
		free(strw);
		return 0;
	}

	free(strw);
	return 1;

#undef GUIDF
#undef GUIDX
}

/* -------------------------------------------------------------------- */

static inline SCHISM_ALWAYS_INLINE void win32_stat_conv(struct _stat *mst, struct stat *st)
{
	st->st_gid = mst->st_gid;
	st->st_atime = mst->st_atime;
	st->st_ctime = mst->st_ctime;
	st->st_dev = mst->st_dev;
	st->st_ino = mst->st_ino;
	st->st_mode = mst->st_mode;
	st->st_mtime = mst->st_mtime;
	st->st_nlink = mst->st_nlink;
	st->st_rdev = mst->st_rdev;
	st->st_size = mst->st_size;
	st->st_uid = mst->st_uid;
}

int win32_stat(const char* path, struct stat* st)
{
	struct _stat mst;

#ifdef SCHISM_WIN32_COMPILE_ANSI
	if (GetVersion() & UINT32_C(0x80000000)) {
		// Windows 9x
		char* ac = NULL;

		if (!charset_iconv(path, &ac, CHARSET_UTF8, CHARSET_ANSI, SIZE_MAX)) {
			int ret = _stat(ac, &mst);
			free(ac);
			win32_stat_conv(&mst, st);
			return ret;
		}
	} else
#endif
	{
		wchar_t* wc = NULL;

		if (!charset_iconv(path, &wc, CHARSET_UTF8, CHARSET_WCHAR_T, SIZE_MAX)) {
			int ret = _wstat(wc, &mst);
			free(wc);
			win32_stat_conv(&mst, st);
			return ret;
		}
	}

	return -1;
}

FILE* win32_fopen(const char* path, const char* flags)
{
#ifdef SCHISM_WIN32_COMPILE_ANSI
	if (GetVersion() & UINT32_C(0x80000000)) {
		// Windows 9x
		char *ac = NULL, *ac_flags = NULL;
		if (charset_iconv(path, &ac, CHARSET_UTF8, CHARSET_ANSI, SIZE_MAX)
			|| charset_iconv(flags, &ac_flags, CHARSET_UTF8, CHARSET_ANSI, SIZE_MAX))
			return NULL;

		FILE *ret = fopen(ac, ac_flags);
		free(ac);
		free(ac_flags);
		return ret;
	} else
#endif
	{
		// Windows NT
		wchar_t* wc = NULL, *wc_flags = NULL;
		if (charset_iconv(path, &wc, CHARSET_UTF8, CHARSET_WCHAR_T, SIZE_MAX)
			|| charset_iconv(flags, &wc_flags, CHARSET_UTF8, CHARSET_WCHAR_T, SIZE_MAX))
			return NULL;

		FILE* ret = _wfopen(wc, wc_flags);
		free(wc);
		free(wc_flags);
		return ret;
	}

	// err
	return NULL;
}

int win32_mkdir(const char *path, SCHISM_UNUSED mode_t mode)
{
#ifdef SCHISM_WIN32_COMPILE_ANSI
	if (GetVersion() & UINT32_C(0x80000000)) {
		char* ac = NULL;
		if (charset_iconv(path, &ac, CHARSET_UTF8, CHARSET_ANSI, SIZE_MAX))
			return -1;

		int ret = mkdir(ac);
		free(ac);
		return ret;
	} else
#endif
	{
		wchar_t* wc = NULL;
		if (charset_iconv(path, &wc, CHARSET_UTF8, CHARSET_WCHAR_T, SIZE_MAX))
			return -1;

		int ret = _wmkdir(wc);
		free(wc);
		return ret;
	}

	return -1;
}

/* ------------------------------------------------------------------------------- */
/* run hook */

#define WIN32_RUN_HOOK_VARIANT(name, charset, char_type, char_len_func, char_getcwd, char_chdir, char_getenv, char_spawnlp, char_stat, const_prefix) \
	static inline SCHISM_ALWAYS_INLINE int _win32_run_hook_##name(const char *dir, const char *name, const char *maybe_arg) \
	{ \
		char_type cwd[MAX_PATH] = {0}; \
		if (!char_getcwd(cwd, MAX_PATH)) \
			return 0; \
	\
		char_type batch_file[MAX_PATH] = {0}; \
	\
		{ \
			char_type *name_w; \
			if (charset_iconv(name, &name_w, CHARSET_UTF8, charset, SIZE_MAX)) \
				return 0; \
	\
			size_t name_len = char_len_func(name_w); \
			if ((name_len * sizeof(char_type)) + sizeof(const_prefix##".bat") >= sizeof(batch_file)) { \
				free(name_w); \
				return 0; \
			} \
	\
			memcpy(batch_file, name_w, name_len * sizeof(char_type)); \
			memcpy(batch_file + name_len, const_prefix##".bat", sizeof(const_prefix##".bat")); \
	\
			free(name_w); \
		} \
	\
		{ \
			char_type *dir_w; \
			if (charset_iconv(dir, &dir_w, CHARSET_UTF8, charset, SIZE_MAX)) \
				return 0; \
	\
			if (char_chdir(dir_w) == -1) { \
				free(dir_w); \
				return 0; \
			} \
	\
			free(dir_w); \
		} \
	\
		intptr_t r; \
	\
		{ \
			char_type *maybe_arg_w = NULL; \
			charset_iconv(maybe_arg, &maybe_arg_w, CHARSET_UTF8, charset, SIZE_MAX); \
	\
			struct _stat sb; \
			if (char_stat(batch_file, &sb) < 0) { \
				r = 0; \
			} else { \
				const char_type *cmd; \
	\
				cmd = char_getenv(const_prefix##"COMSPEC"); \
				if (!cmd) \
					cmd = const_prefix##"command.com"; \
	\
				r = char_spawnlp(_P_WAIT, cmd, cmd, const_prefix##"/c", batch_file, maybe_arg_w, NULL); \
			} \
	\
			free(maybe_arg_w); \
		} \
	\
		char_chdir(cwd); \
		return (r == 0); \
	}

WIN32_RUN_HOOK_VARIANT(wide, CHARSET_WCHAR_T, WCHAR, wcslen, _wgetcwd, _wchdir, _wgetenv, _wspawnlp, _wstat, L)
#ifdef SCHISM_WIN32_COMPILE_ANSI
WIN32_RUN_HOOK_VARIANT(ansi, CHARSET_ANSI, char, strlen, getcwd, _chdir, getenv, _spawnlp, _stat, /* none */)
#endif

#undef WIN32_RUN_HOOK_VARIANT

int win32_run_hook(const char *dir, const char *name, const char *maybe_arg)
{
#ifdef SCHISM_WIN32_COMPILE_ANSI
	if (GetVersion() & UINT32_C(0x80000000)) {
		return _win32_run_hook_ansi(dir, name, maybe_arg);
	} else
#endif
	{
		return _win32_run_hook_wide(dir, name, maybe_arg);
	}
}
