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
#ifndef SCHISM_VIDEO_H_
#define SCHISM_VIDEO_H_

#include "headers.h"

/* video output routines */
const char *video_driver_name(void);

void video_set_hardware(int hardware);

int video_is_input_grabbed(void);
void video_set_input_grabbed(int enabled);

/* -------------------------------------------------- */

void video_warp_mouse(unsigned int x, unsigned int y);
void video_get_mouse_coordinates(unsigned int *x, unsigned int *y);
void video_show_cursor(int enabled);

/* -------------------------------------------------- */
/* menu toggling */

int video_have_menu(void);
void video_toggle_menu(int on);

/* -------------------------------------------------- */

void video_rgb_to_yuv(uint32_t *y, uint32_t *u, uint32_t *v, unsigned char rgb[3]);
void video_setup(const char *quality);
int video_startup(void);
void video_shutdown(void);
void video_report(void);
void video_refresh(void);
void video_update(void);
void video_colors(unsigned char palette[16][3]);
void video_resize(unsigned int width, unsigned int height);
void video_fullscreen(int new_fs_flag);
void video_translate(unsigned int vx, unsigned int vy, unsigned int *x, unsigned int *y);
SCHISM_HOT void video_blit(void);

int video_is_screensaver_enabled(void);
void video_toggle_screensaver(int enabled);

/* cursor-specific stuff */
enum video_mousecursor_shape {
	CURSOR_SHAPE_ARROW,
	CURSOR_SHAPE_CROSSHAIR,
};

void video_mousecursor(int z); /* takes in the MOUSE_* enum from it.h (why is it there?) */
int video_mousecursor_visible(void);
void video_mousecursor_changed(void); // used for each backend to do optimizations
void video_set_mousecursor_shape(enum video_mousecursor_shape shape);

/* getters, will sometimes poll the backend */

int video_is_fullscreen(void);
int video_is_wm_available(void);
int video_is_focused(void);
int video_is_visible(void);
int video_is_hardware(void);
int video_width(void);
int video_height(void);

int video_gl_bilinear(void);

void video_get_logical_coordinates(int x, int y, int *trans_x, int *trans_y);

int xpmdata(const char *data[], uint32_t **pixels, int *w, int *h);

/* --------------------------------------------------------- */

/* function to callback to map an RGB value; used for the linear blitter only */
typedef uint32_t (*schism_map_rgb_func_t)(void *data, uint8_t r, uint8_t g, uint8_t b);

/* YUV blitters */
SCHISM_HOT void video_blitYY(unsigned char *pixels, unsigned int pitch, uint32_t tpal[256]);
SCHISM_HOT void video_blitUV(unsigned char *pixels, unsigned int pitch, uint32_t tpal[256]);
SCHISM_HOT void video_blitTV(unsigned char *pixels, unsigned int pitch, uint32_t tpal[256]);

/* RGB blitters;
 *
 * video_blitLN is special because it does all of its internal processing with 24-bit color, so it needs
 * a callback function to convert RGB values into a proper format. */
SCHISM_HOT void video_blit11(unsigned int bpp, unsigned char *pixels, unsigned int pitch, uint32_t tpal[256]);
SCHISM_HOT void video_blitNN(unsigned int bpp, unsigned char *pixels, unsigned int pitch, uint32_t tpal[256], int width, int height);
SCHISM_HOT void video_blitLN(unsigned int bpp, unsigned char *pixels, unsigned int pitch, schism_map_rgb_func_t map_rgb, void *map_rgb_data, int width, int height);

/* --------------------------------------------------------- */

typedef struct {
	enum {
		VIDEO_WM_DATA_SUBSYSTEM_WINDOWS = 0,
		VIDEO_WM_DATA_SUBSYSTEM_X11 = 1,
	} subsystem;

	union {
		struct {
			void *hwnd; // type is actually HWND
		} windows;
		struct {
			void *display; // type is actually Display *
			uint32_t window; // type is actually Window

			// These can (and will) be NULL
			void (*lock_func)(void);
			void (*unlock_func)(void);
		} x11;
	} data;
} video_wm_data_t;

int video_get_wm_data(video_wm_data_t *wm_data);

#endif /* SCHISM_VIDEO_H_ */
