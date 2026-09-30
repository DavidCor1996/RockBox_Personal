/*
	OSGLURB.c

	Mini vMac host glue for Rockbox.

	This file is a small Rockbox platform layer for the Mini vMac 36.04
	Macintosh Plus core. It intentionally starts without sound, clipboard,
	drag/drop, or the desktop Mini vMac UI.
*/

#include "CNFGRAPI.h"
#include "fixedpoint.h"
#include "lib/helper.h"
#include "SYSDEPNS.h"
#include "ENDIANAC.h"
#include "MYOSGLUE.h"
#include "STRCONST.h"

GLOBALOSGLUPROC MyMoveBytes(anyp srcPtr, anyp destPtr, si5b byteCount)
{
	(void) rb->memmove((char *)destPtr, (char *)srcPtr, byteCount);
}

#define NeedCell2PlainAsciiMap 1
#include "INTLCHAR.h"

#define WantColorTransValid 0
#include "COMOSGLU.h"

#include "CONTROLM.h"

#define MINIVMAC_DIR "/.rockbox/minivmac"
#define MINIVMAC_ROM MINIVMAC_DIR "/vMac.ROM"
#define MINIVMAC_DISK1 MINIVMAC_DIR "/disk1.dsk"
#define MINIVMAC_DISK2 MINIVMAC_DIR "/disk2.dsk"
#define MINIVMAC_SCREEN_W 512
#define MINIVMAC_SCREEN_H 342
#define MINIVMAC_VIEW_W 320
#define MINIVMAC_PANEL_H 240
#define MINIVMAC_UNDOCKED_FIT_H 214
#define MINIVMAC_VIEW_BYTES (MINIVMAC_VIEW_W / 8)
#define MINIVMAC_FRAME_TICKS (HZ / 12)
#define MINIVMAC_TICKS_PER_SECOND 60
#define MINIVMAC_WHEEL_POSITIONS 96
#define MINIVMAC_WHEEL_SUBPIXEL 16
#define MINIVMAC_WHEEL_PACKET_CAP 6
#define MINIVMAC_POINTER_SPEED 2
#define MINIVMAC_POINTER_UPDATE_TICKS MAX(1, HZ / 50)
#define MINIVMAC_GLIDE_RAMP (HZ * 3 / 4)
#define MINIVMAC_GLIDE_SUBPIXEL 16
#define MINIVMAC_GLIDE_MIN_RATE 5
#define MINIVMAC_GLIDE_MAX_RATE 14
#define MINIVMAC_HELD_DIR_STEP_TICKS MAX(1, HZ / 20)
#define MINIVMAC_HELD_DIR_TIMEOUT_TICKS MAX(1, HZ / 3)
#define MINIVMAC_VIDEOOUT_TOKEN "videoout-active"
#define MINIVMAC_SIMULATOR_DOCKED_TOKEN "simulator-docked"
#define MINIVMAC_ABS(value) ((value) < 0 ? -(value) : (value))

#define NotAfileRef (-1)

LOCALVAR int Drives[NumDrives];
LOCALVAR char drive_names[NumDrives][MAX_PATH];
LOCALVAR const char *initial_disk_path;
LOCALVAR ui3p screencomparebuff;
LOCALVAR fb_data rb_screen[MINIVMAC_VIEW_W * MINIVMAC_PANEL_H];
LOCALVAR fb_data mono_lut[256][8];
LOCALVAR fb_data gray_lut[5];
LOCALVAR ui4r fit_xmap_a[MINIVMAC_VIEW_W];
LOCALVAR ui4r fit_xmap_b[MINIVMAC_VIEW_W];
LOCALVAR ui4r fit_ymap_a[MINIVMAC_PANEL_H];
LOCALVAR ui4r fit_ymap_b[MINIVMAC_PANEL_H];
LOCALVAR long last_frame_tick;
LOCALVAR long last_emu_tick;
LOCALVAR ui4b last_true_time;
LOCALVAR ui5b true_time_accum;
LOCALVAR ui4r rb_mouse_h;
LOCALVAR ui4r rb_mouse_v;
LOCALVAR ui4r rb_view_h;
LOCALVAR ui4r rb_view_v;
LOCALVAR blnr rb_mouse_down;
LOCALVAR blnr keyboard_mode;
LOCALVAR blnr fit_view;
LOCALVAR blnr force_redraw;
LOCALVAR blnr exit_confirm;
LOCALVAR blnr menu_long_handled;
LOCALVAR blnr play_long_handled;
LOCALVAR blnr videoout_event_registered;
LOCALVAR blnr active_docked_layout;
LOCALVAR volatile blnr docked_output;
LOCALVAR volatile blnr video_layout_dirty;
LOCALVAR long hint_until;
LOCALVAR long view_hint_until;
LOCALVAR int key_index;
LOCALVAR unsigned int held_buttons;

struct rb_pointer_state {
	int last_wheel;
	int wheel_velocity;
	int wheel_direction;
	long last_wheel_tick;
	long glide_since;
	long glide_tick;
	long glide_emit;
	int glide_x;
	int glide_y;
	int frac_x;
	int frac_y;
	int held_dir_x;
	int held_dir_y;
	long held_seen_tick;
	long held_emit_tick;
	blnr wheel_available;
};

LOCALVAR struct rb_pointer_state pointer_state;

struct rb_key {
	char label[4];
	ui3r code;
};

LOCALVAR const struct rb_key rb_keys[] = {
	{ "ret", MKC_Return },
	{ "esc", MKC_Escape },
	{ "tab", MKC_Tab },
	{ "spc", MKC_Space },
	{ "del", MKC_BackSpace },
	{ "cmd", MKC_Command },
	{ "shf", MKC_Shift },
	{ "opt", MKC_Option },
	{ "up", MKC_Up },
	{ "dn", MKC_Down },
	{ "lt", MKC_Left },
	{ "rt", MKC_Right },
	{ "a", MKC_A },
	{ "b", MKC_B },
	{ "c", MKC_C },
	{ "d", MKC_D },
	{ "e", MKC_E },
	{ "f", MKC_F },
	{ "g", MKC_G },
	{ "h", MKC_H },
	{ "i", MKC_I },
	{ "j", MKC_J },
	{ "k", MKC_K },
	{ "l", MKC_L },
	{ "m", MKC_M },
	{ "n", MKC_N },
	{ "o", MKC_O },
	{ "p", MKC_P },
	{ "q", MKC_Q },
	{ "r", MKC_R },
	{ "s", MKC_S },
	{ "t", MKC_T },
	{ "u", MKC_U },
	{ "v", MKC_V },
	{ "w", MKC_W },
	{ "x", MKC_X },
	{ "y", MKC_Y },
	{ "z", MKC_Z },
};

LOCALPROC InitDrives(void)
{
	tDrive i;

	for (i = 0; i < NumDrives; ++i) {
		Drives[i] = NotAfileRef;
		drive_names[i][0] = '\0';
	}
}

LOCALPROC show_status(const char *line1, const char *line2)
{
	rb->lcd_clear_display();
	rb->lcd_putsxy(0, 0, line1);
	if (line2 != NULL) {
		rb->lcd_putsxy(0, 14, line2);
	}
	rb->lcd_update();
}

GLOBALOSGLUFUNC tMacErr vSonyTransfer(blnr IsWrite, ui3p Buffer,
	tDrive Drive_No, ui5r Sony_Start, ui5r Sony_Count,
	ui5r *Sony_ActCount)
{
	ssize_t count;
	tMacErr err = mnvm_miscErr;
	int fd = Drives[Drive_No];
	ui5r NewSony_Count = 0;

	if (fd >= 0) {
		if (rb->lseek(fd, Sony_Start, SEEK_SET) >= 0) {
			if (IsWrite) {
				count = rb->write(fd, Buffer, Sony_Count);
			} else {
				count = rb->read(fd, Buffer, Sony_Count);
			}

			if (count > 0) {
				NewSony_Count = count;
			}
			if (NewSony_Count == Sony_Count) {
				err = mnvm_noErr;
			}
		}
	}

	if (nullpr != Sony_ActCount) {
		*Sony_ActCount = NewSony_Count;
	}

	return err;
}

GLOBALOSGLUFUNC tMacErr vSonyGetSize(tDrive Drive_No, ui5r *Sony_Count)
{
	long size;
	int fd = Drives[Drive_No];

	if (fd < 0) {
		return mnvm_miscErr;
	}

	size = rb->filesize(fd);
	if (size < 0) {
		return mnvm_miscErr;
	}

	*Sony_Count = size;
	return mnvm_noErr;
}

LOCALFUNC tMacErr vSonyEject0(tDrive Drive_No)
{
	int fd = Drives[Drive_No];

	DiskEjectedNotify(Drive_No);

	if (fd >= 0) {
		rb->close(fd);
	}
	Drives[Drive_No] = NotAfileRef;
	drive_names[Drive_No][0] = '\0';

	return mnvm_noErr;
}

GLOBALOSGLUFUNC tMacErr vSonyEject(tDrive Drive_No)
{
	return vSonyEject0(Drive_No);
}

LOCALPROC UnInitDrives(void)
{
	tDrive i;

	for (i = 0; i < NumDrives; ++i) {
		if (vSonyIsInserted(i)) {
			(void) vSonyEject(i);
		}
	}
}

LOCALFUNC blnr Sony_InsertPath(const char *drivepath, blnr silentfail)
{
	tDrive Drive_No;
	blnr locked = falseblnr;
	int fd;

	if (! FirstFreeDisk(&Drive_No)) {
		if (! silentfail) {
			show_status("Mini vMac", "Too many disks");
		}
		return falseblnr;
	}

	fd = rb->open(drivepath, O_RDWR);
	if (fd < 0) {
		locked = trueblnr;
		fd = rb->open(drivepath, O_RDONLY);
	}
	if (fd < 0) {
		if (! silentfail) {
			show_status("Mini vMac", "Disk image open failed");
		}
		return falseblnr;
	}

	Drives[Drive_No] = fd;
	rb->strlcpy(drive_names[Drive_No], drivepath, sizeof(drive_names[Drive_No]));
	DiskInsertNotify(Drive_No, locked);
	return trueblnr;
}

LOCALFUNC tMacErr LoadMacRomFrom(const char *path)
{
	int fd;
	ssize_t got;
	tMacErr err;

	fd = rb->open(path, O_RDONLY);
	if (fd < 0) {
		return mnvm_fnfErr;
	}

	got = rb->read(fd, ROM, kROM_Size);
	rb->close(fd);

	if (got != kROM_Size) {
		show_status("Mini vMac", "vMac.ROM is short");
		return mnvm_eofErr;
	}

	err = ROM_IsValid();
	if (err != mnvm_noErr) {
		show_status("Mini vMac", "Bad Mac Plus ROM");
	}
	return err;
}

LOCALFUNC blnr LoadMacRom(void)
{
	if (mnvm_noErr != LoadMacRomFrom(MINIVMAC_ROM)) {
		show_status("Missing ROM", MINIVMAC_ROM);
		return falseblnr;
	}

	ROM_loaded = trueblnr;
	return trueblnr;
}

LOCALFUNC blnr LoadInitialImages(void)
{
	if (initial_disk_path != NULL && initial_disk_path[0] != '\0') {
		(void) Sony_InsertPath(initial_disk_path, falseblnr);
	}

	if (! AnyDiskInserted()) {
		(void) Sony_InsertPath(MINIVMAC_DISK1, trueblnr);
	}

	if (! AnyDiskInserted()) {
		show_status("Missing disk", "Open a .dsk/.img file");
		return falseblnr;
	}

	(void) Sony_InsertPath(MINIVMAC_DISK2, trueblnr);

	return trueblnr;
}

LOCALFUNC int clamp_int(int value, int low, int high)
{
	if (value < low) {
		return low;
	}
	if (value > high) {
		return high;
	}
	return value;
}

LOCALPROC InitVideoTables(void)
{
	int i;
	int bit;
	fb_data white = LCD_RGBPACK(0xff, 0xff, 0xff);
	fb_data black = LCD_RGBPACK(0x00, 0x00, 0x00);

	for (i = 0; i < 256; ++i) {
		for (bit = 0; bit < 8; ++bit) {
			mono_lut[i][bit] =
				(i & (0x80 >> bit)) != 0 ? black : white;
		}
	}

	gray_lut[0] = white;
	gray_lut[1] = LCD_RGBPACK(0xcc, 0xcc, 0xcc);
	gray_lut[2] = LCD_RGBPACK(0x88, 0x88, 0x88);
	gray_lut[3] = LCD_RGBPACK(0x44, 0x44, 0x44);
	gray_lut[4] = black;

	for (i = 0; i < MINIVMAC_VIEW_W; ++i) {
		fit_xmap_a[i] = ((i * 4 + 1) * MINIVMAC_SCREEN_W)
			/ (MINIVMAC_VIEW_W * 4);
		fit_xmap_b[i] = ((i * 4 + 3) * MINIVMAC_SCREEN_W)
			/ (MINIVMAC_VIEW_W * 4);
	}
}

LOCALFUNC int fit_frame_height(void)
{
	return active_docked_layout ? MINIVMAC_PANEL_H :
		MINIVMAC_UNDOCKED_FIT_H;
}

LOCALPROC rebuild_fit_y_map(void)
{
	int y;
	int height = fit_frame_height();

	for (y = 0; y < height; ++y) {
		fit_ymap_a[y] = ((y * 4 + 1) * MINIVMAC_SCREEN_H)
			/ (height * 4);
		fit_ymap_b[y] = ((y * 4 + 3) * MINIVMAC_SCREEN_H)
			/ (height * 4);
	}
}

LOCALPROC apply_video_layout(void)
{
	if (video_layout_dirty) {
		video_layout_dirty = falseblnr;
		active_docked_layout = docked_output;
		rebuild_fit_y_map();
		force_redraw = trueblnr;
	}
}

LOCALPROC update_viewport(void)
{
	int max_h = vMacScreenWidth - MINIVMAC_VIEW_W;
	int max_v = vMacScreenHeight - MINIVMAC_PANEL_H;
	int new_h;
	int new_v;

	if (max_h < 0) {
		max_h = 0;
	}
	if (max_v < 0) {
		max_v = 0;
	}

	new_h = clamp_int((int)rb_mouse_h - MINIVMAC_VIEW_W / 2, 0, max_h);
	new_v = clamp_int((int)rb_mouse_v - MINIVMAC_PANEL_H / 2, 0, max_v);
	new_h &= ~7;
	if (new_h > max_h) {
		new_h = max_h & ~7;
	}

	if (rb_view_h != (ui4r)new_h || rb_view_v != (ui4r)new_v) {
		force_redraw = trueblnr;
	}

	rb_view_h = new_h;
	rb_view_v = new_v;
}

LOCALPROC draw_native_frame(ui3p screencurrentbuff)
{
	int y;
	int byte_x;
	int src_x_byte = rb_view_h >> 3;

	for (y = 0; y < MINIVMAC_PANEL_H; ++y) {
		ui3p src = screencurrentbuff
			+ ((int)rb_view_v + y) * vMacScreenMonoByteWidth
			+ src_x_byte;
		fb_data *dst = rb_screen + y * MINIVMAC_VIEW_W;

		for (byte_x = 0; byte_x < MINIVMAC_VIEW_BYTES; ++byte_x) {
			fb_data *pix = mono_lut[src[byte_x]];

			dst[0] = pix[0];
			dst[1] = pix[1];
			dst[2] = pix[2];
			dst[3] = pix[3];
			dst[4] = pix[4];
			dst[5] = pix[5];
			dst[6] = pix[6];
			dst[7] = pix[7];
			dst += 8;
		}
	}
}

LOCALPROC draw_fit_frame(ui3p screencurrentbuff, int height)
{
	int y;
	int x;

	for (y = 0; y < height; ++y) {
		ui3p row_a = screencurrentbuff
			+ fit_ymap_a[y] * vMacScreenMonoByteWidth;
		ui3p row_b = screencurrentbuff
			+ fit_ymap_b[y] * vMacScreenMonoByteWidth;

		for (x = 0; x < MINIVMAC_VIEW_W; ++x) {
			int sx_a = fit_xmap_a[x];
			int sx_b = fit_xmap_b[x];
			int black_pixels = 0;

			black_pixels += (row_a[sx_a >> 3]
				& (0x80 >> (sx_a & 7))) != 0;
			black_pixels += (row_a[sx_b >> 3]
				& (0x80 >> (sx_b & 7))) != 0;
			black_pixels += (row_b[sx_a >> 3]
				& (0x80 >> (sx_a & 7))) != 0;
			black_pixels += (row_b[sx_b >> 3]
				& (0x80 >> (sx_b & 7))) != 0;
			rb_screen[y * MINIVMAC_VIEW_W + x] =
				gray_lut[black_pixels];
		}
	}
}

LOCALPROC draw_centered_text(int y, const char *text)
{
	int width;

	rb->lcd_getstringsize(text, &width, NULL);
	rb->lcd_putsxy(MAX(2, (MINIVMAC_VIEW_W - width) / 2), y, text);
}

LOCALPROC draw_black_panel(int x, int y, int width, int height)
{
	rb->lcd_set_foreground(LCD_RGBPACK(0x00, 0x00, 0x00));
	rb->lcd_fillrect(x, y, width, height);
	rb->lcd_set_foreground(LCD_RGBPACK(0xff, 0xff, 0xff));
	rb->lcd_drawrect(x, y, width, height);
	rb->lcd_set_background(LCD_RGBPACK(0x00, 0x00, 0x00));
}

LOCALPROC draw_controls_overlay(long now)
{
	int font_height;
	char label[48];

	rb->lcd_getstringsize("M", NULL, &font_height);
	if (keyboard_mode) {
		int panel_height = font_height + 6;

		draw_black_panel(0, MINIVMAC_PANEL_H - panel_height,
			MINIVMAC_VIEW_W, panel_height);
		rb->snprintf(label, sizeof(label), "Keyboard: %s   Play: done",
			rb_keys[key_index].label);
		draw_centered_text(MINIVMAC_PANEL_H - panel_height + 3, label);
	} else if (hint_until != 0 && TIME_BEFORE(now, hint_until)) {
		int panel_height = font_height * 2 + 8;
		int y = MINIVMAC_PANEL_H - panel_height;

		draw_black_panel(0, y, MINIVMAC_VIEW_W, panel_height);
		draw_centered_text(y + 3, "Wheel: pointer   Select: click");
		draw_centered_text(y + font_height + 3,
			"Play: keyboard   Menu: back");
	}

	if (view_hint_until != 0 && TIME_BEFORE(now, view_hint_until)) {
		int panel_height = font_height + 6;
		const char *view_label;

		if (active_docked_layout) {
			view_label = "Docked: full 512x342 frame";
		} else {
			view_label = fit_view ? "Full-frame overview" :
				"Sharp 320x240 viewport";
		}
		draw_black_panel(18, 8, MINIVMAC_VIEW_W - 36, panel_height);
		draw_centered_text(11, view_label);
	}

	if (exit_confirm) {
		int panel_height = font_height * 2 + 14;
		int y = (MINIVMAC_PANEL_H - panel_height) / 2;

		draw_black_panel(24, y, MINIVMAC_VIEW_W - 48, panel_height);
		draw_centered_text(y + 4, "Return to iPod?");
		draw_centered_text(y + font_height + 7,
			"Select: return   Menu: cancel");
	}
}

LOCALPROC draw_mac_frame(ui3p screencurrentbuff)
{
	long now = *rb->current_tick;
	int frame_height;
	int frame_y;
	blnr overview;

	apply_video_layout();

	if (! force_redraw
		&& TIME_BEFORE(now, last_frame_tick + MINIVMAC_FRAME_TICKS))
	{
		return;
	}
	last_frame_tick = now;
	update_viewport();
	overview = fit_view || active_docked_layout;
	frame_height = overview ? fit_frame_height() : MINIVMAC_PANEL_H;
	frame_y = (MINIVMAC_PANEL_H - frame_height) / 2;

	if (overview) {
		draw_fit_frame(screencurrentbuff, frame_height);
	} else {
		draw_native_frame(screencurrentbuff);
	}

	rb->lcd_set_background(LCD_RGBPACK(0x00, 0x00, 0x00));
	rb->lcd_clear_display();
	rb->lcd_bitmap(rb_screen, 0, frame_y, MINIVMAC_VIEW_W, frame_height);
	draw_controls_overlay(now);

	rb->lcd_set_foreground(LCD_DEFAULT_FG);
	rb->lcd_set_background(LCD_DEFAULT_BG);
	rb->lcd_update();
	force_redraw = falseblnr;
}

LOCALPROC HaveChangedScreenBuff(ui4r top, ui4r left,
	ui4r bottom, ui4r right)
{
	(void) top;
	(void) left;
	(void) bottom;
	(void) right;

	draw_mac_frame(GetCurDrawBuff());
}

LOCALPROC MyDrawChangesAndClear(void)
{
	if (force_redraw || ScreenChangedBottom > ScreenChangedTop) {
		HaveChangedScreenBuff(ScreenChangedTop, ScreenChangedLeft,
			ScreenChangedBottom, ScreenChangedRight);
		ScreenClearChanges();
	}
}

GLOBALOSGLUPROC DoneWithDrawingForTick(void)
{
	MyDrawChangesAndClear();
}

LOCALPROC post_key(ui3r key)
{
	Keyboard_UpdateKeyMap(key, trueblnr);
	Keyboard_UpdateKeyMap(key, falseblnr);
}

LOCALPROC set_mouse_pos(int h, int v)
{
	ui4r new_h = clamp_int(h, 0, vMacScreenWidth - 1);
	ui4r new_v = clamp_int(v, 0, vMacScreenHeight - 1);

	if (new_h == rb_mouse_h && new_v == rb_mouse_v) {
		return;
	}
	rb_mouse_h = new_h;
	rb_mouse_v = new_v;
	MyMousePositionSet(rb_mouse_h, rb_mouse_v);
	force_redraw = trueblnr;
}

LOCALPROC release_mouse(void)
{
	if (rb_mouse_down) {
		MyMouseButtonSet(falseblnr);
		rb_mouse_down = falseblnr;
	}
}

LOCALFUNC blnr move_mouse_by(int dx, int dy)
{
	int old_h = rb_mouse_h;
	int old_v = rb_mouse_v;

	set_mouse_pos(old_h + dx, old_v + dy);
	return old_h != (int)rb_mouse_h || old_v != (int)rb_mouse_v;
}

#ifdef HAVE_WHEEL_POSITION
LOCALFUNC blnr pointer_ring_button_down(void)
{
	long status = rb->button_status();

	return (status & (BUTTON_MENU | BUTTON_PLAY | BUTTON_LEFT |
		BUTTON_RIGHT | BUTTON_SELECT)) != 0;
}

LOCALPROC pointer_glide_reset(long now)
{
	pointer_state.glide_since = now;
	pointer_state.glide_tick = now;
	pointer_state.glide_emit = now;
	pointer_state.glide_x = 0;
	pointer_state.glide_y = 0;
}
#endif

LOCALPROC pointer_wheel_reset(void)
{
	pointer_state.last_wheel = -1;
	pointer_state.wheel_velocity = 0;
	pointer_state.wheel_direction = 0;
	pointer_state.last_wheel_tick = 0;
	pointer_state.glide_since = 0;
	pointer_state.glide_tick = 0;
	pointer_state.glide_emit = 0;
	pointer_state.glide_x = 0;
	pointer_state.glide_y = 0;
	pointer_state.frac_x = 0;
	pointer_state.frac_y = 0;
}

LOCALPROC cycle_keyboard(int direction)
{
	int count = (int)(sizeof(rb_keys) / sizeof(rb_keys[0]));

	key_index += direction;
	if (key_index < 0) {
		key_index = count - 1;
	} else if (key_index >= count) {
		key_index = 0;
	}
	force_redraw = trueblnr;
}

#ifdef HAVE_WHEEL_POSITION
LOCALFUNC blnr glide_wheel_pointer(int wheel, long now)
{
	long held;
	long ticks;
	int angle;
	int rate;
	int moved_x;
	int moved_y;

	if (pointer_state.glide_since == 0) {
		pointer_glide_reset(now);
	}
	if (pointer_ring_button_down() || rb_mouse_down || keyboard_mode ||
		exit_confirm)
	{
		pointer_glide_reset(now);
		return falseblnr;
	}
	held = now - pointer_state.glide_since;
	ticks = now - pointer_state.glide_tick;
	if (ticks <= 0) {
		return falseblnr;
	}
	pointer_state.glide_tick = now;

	rate = MINIVMAC_GLIDE_MIN_RATE +
		(int)(held * (MINIVMAC_GLIDE_MAX_RATE - MINIVMAC_GLIDE_MIN_RATE)
		/ MAX(1, MINIVMAC_GLIDE_RAMP));
	rate = MIN(MINIVMAC_GLIDE_MAX_RATE, rate) *
		(MINIVMAC_POINTER_SPEED + 2) / 4;
	rate = (int)MIN((long)rate * ticks,
		(long)MINIVMAC_GLIDE_MAX_RATE * HZ);
	angle = wheel * 360 / MINIVMAC_WHEEL_POSITIONS - 90;
	pointer_state.glide_x += fp14_cos(angle) * rate / 16384;
	pointer_state.glide_y += fp14_sin(angle) * rate / 16384;
	if (TIME_BEFORE(now,
		pointer_state.glide_emit + MINIVMAC_POINTER_UPDATE_TICKS))
	{
		return falseblnr;
	}
	moved_x = pointer_state.glide_x / MINIVMAC_GLIDE_SUBPIXEL;
	moved_y = pointer_state.glide_y / MINIVMAC_GLIDE_SUBPIXEL;
	if (moved_x == 0 && moved_y == 0) {
		return falseblnr;
	}
	pointer_state.glide_emit = now;
	pointer_state.glide_x -= moved_x * MINIVMAC_GLIDE_SUBPIXEL;
	pointer_state.glide_y -= moved_y * MINIVMAC_GLIDE_SUBPIXEL;
	return move_mouse_by(moved_x, moved_y);
}

LOCALFUNC blnr poll_wheel_pointer(void)
{
	int wheel = rb->wheel_status();
	int old;
	int delta;
	long now = *rb->current_tick;

	if (wheel < 0 || exit_confirm) {
		pointer_wheel_reset();
		return falseblnr;
	}
	pointer_state.wheel_available = trueblnr;
	if (pointer_state.last_wheel < 0) {
		int angle = wheel * 360 / MINIVMAC_WHEEL_POSITIONS - 90;
		int touch_dx = fp14_cos(angle);
		int touch_dy = fp14_sin(angle);

		pointer_state.last_wheel = wheel;
		pointer_state.last_wheel_tick = now;
		pointer_glide_reset(now);
		if (keyboard_mode) {
			return falseblnr;
		}
		touch_dx = touch_dx > 4096 ? 1 : touch_dx < -4096 ? -1 : 0;
		touch_dy = touch_dy > 4096 ? 1 : touch_dy < -4096 ? -1 : 0;
		return move_mouse_by(touch_dx, touch_dy);
	}
	old = pointer_state.last_wheel;
	if (old == wheel) {
		return glide_wheel_pointer(wheel, now);
	}
	delta = wheel - old;
	if (delta > MINIVMAC_WHEEL_POSITIONS / 2) {
		delta -= MINIVMAC_WHEEL_POSITIONS;
	} else if (delta < -(MINIVMAC_WHEEL_POSITIONS / 2)) {
		delta += MINIVMAC_WHEEL_POSITIONS;
	}
	pointer_state.last_wheel = wheel;
	if (keyboard_mode) {
		pointer_state.last_wheel_tick = now;
		pointer_glide_reset(now);
		cycle_keyboard(delta < 0 ? -1 : 1);
		return trueblnr;
	}

	{
		int direction = delta < 0 ? -1 : 1;
		long elapsed = now - pointer_state.last_wheel_tick;
		int angle_old = old * 360 / MINIVMAC_WHEEL_POSITIONS - 90;
		int angle_new = wheel * 360 / MINIVMAC_WHEEL_POSITIONS - 90;
		int dx = fp14_cos(angle_new) - fp14_cos(angle_old);
		int dy = fp14_sin(angle_new) - fp14_sin(angle_old);
		int gain16;
		int moved_x;
		int moved_y;

		pointer_glide_reset(now);
		if (pointer_state.wheel_direction != 0 &&
			direction != pointer_state.wheel_direction)
		{
			pointer_state.wheel_velocity = 0;
		}
		if (elapsed > HZ / 4) {
			pointer_state.wheel_velocity = 0;
		}
		pointer_state.wheel_direction = direction;
		pointer_state.last_wheel_tick = now;
		if (elapsed <= 0) {
			elapsed = 1;
		}
		pointer_state.wheel_velocity = MIN(4,
			(pointer_state.wheel_velocity * 2 +
			MIN(4, MINIVMAC_ABS(delta) * HZ / elapsed / 12)) / 3);
		gain16 = 32 + (MINIVMAC_POINTER_SPEED - 1) * 12 *
			MIN(8, MAX(0, MINIVMAC_ABS(delta) - 4)) / 8 +
			pointer_state.wheel_velocity * 6;
		pointer_state.frac_x += dx * gain16 / 4096;
		pointer_state.frac_y += dy * gain16 / 4096;
		moved_x = pointer_state.frac_x / MINIVMAC_WHEEL_SUBPIXEL;
		moved_y = pointer_state.frac_y / MINIVMAC_WHEEL_SUBPIXEL;
		moved_x = MAX(-MINIVMAC_WHEEL_PACKET_CAP,
			MIN(MINIVMAC_WHEEL_PACKET_CAP, moved_x));
		moved_y = MAX(-MINIVMAC_WHEEL_PACKET_CAP,
			MIN(MINIVMAC_WHEEL_PACKET_CAP, moved_y));
		if (moved_x == 0 && moved_y == 0) {
			return falseblnr;
		}
		pointer_state.frac_x -= moved_x * MINIVMAC_WHEEL_SUBPIXEL;
		pointer_state.frac_y -= moved_y * MINIVMAC_WHEEL_SUBPIXEL;
		return move_mouse_by(moved_x, moved_y);
	}
}
#endif

LOCALPROC held_direction_touch(int dx, int dy, long now)
{
	if ((dx != 0 && pointer_state.held_dir_x != dx) ||
		(dy != 0 && pointer_state.held_dir_y != dy))
	{
		pointer_state.held_emit_tick =
			now - MINIVMAC_HELD_DIR_STEP_TICKS;
	}
	if (dx != 0) {
		pointer_state.held_dir_x = dx;
	}
	if (dy != 0) {
		pointer_state.held_dir_y = dy;
	}
	pointer_state.held_seen_tick = now;
}

LOCALPROC held_direction_release_x(int dx)
{
	if (pointer_state.held_dir_x == dx) {
		pointer_state.held_dir_x = 0;
	}
}

LOCALPROC held_direction_release_y(int dy)
{
	if (pointer_state.held_dir_y == dy) {
		pointer_state.held_dir_y = 0;
	}
}

LOCALPROC advance_held_direction(long now)
{
	int step;

	if (keyboard_mode || exit_confirm ||
		(pointer_state.held_dir_x == 0 && pointer_state.held_dir_y == 0))
	{
		return;
	}
	if (! TIME_BEFORE(now,
		pointer_state.held_seen_tick + MINIVMAC_HELD_DIR_TIMEOUT_TICKS))
	{
		pointer_state.held_dir_x = 0;
		pointer_state.held_dir_y = 0;
		return;
	}
	if (TIME_BEFORE(now,
		pointer_state.held_emit_tick + MINIVMAC_HELD_DIR_STEP_TICKS))
	{
		return;
	}
	pointer_state.held_emit_tick = now;
	step = pointer_state.wheel_available ? 1 : MINIVMAC_POINTER_SPEED * 2;
	(void) move_mouse_by(pointer_state.held_dir_x * step,
		pointer_state.held_dir_y * step);
}

LOCALFUNC long translate_remote_button(long button)
{
#ifdef BUTTON_RC_PLAY
	long flags = button & (BUTTON_REPEAT | BUTTON_REL);
	long base = button & ~(BUTTON_REPEAT | BUTTON_REL);

	switch (base) {
	case BUTTON_RC_LEFT:
		return BUTTON_LEFT | flags;
	case BUTTON_RC_RIGHT:
		return BUTTON_RIGHT | flags;
	case BUTTON_RC_PLAY:
	case BUTTON_RC_SELECT:
		return BUTTON_SELECT | flags;
	case BUTTON_RC_MENU:
	case BUTTON_RC_STOP:
		return BUTTON_MENU | flags;
	default:
		break;
	}
#else
	(void) button;
#endif
	return button;
}

LOCALPROC reset_direction_buttons(void)
{
	pointer_state.held_dir_x = 0;
	pointer_state.held_dir_y = 0;
}

LOCALPROC toggle_keyboard_mode(void)
{
	release_mouse();
	keyboard_mode = ! keyboard_mode;
	reset_direction_buttons();
	pointer_wheel_reset();
	force_redraw = trueblnr;
}

LOCALPROC open_exit_confirmation(void)
{
	release_mouse();
	reset_direction_buttons();
	pointer_wheel_reset();
	exit_confirm = trueblnr;
	force_redraw = trueblnr;
}

LOCALPROC update_transient_overlays(long now)
{
	if (hint_until != 0 && ! TIME_BEFORE(now, hint_until)) {
		hint_until = 0;
		force_redraw = trueblnr;
	}
	if (view_hint_until != 0 && ! TIME_BEFORE(now, view_hint_until)) {
		view_hint_until = 0;
		force_redraw = trueblnr;
	}
}

LOCALPROC handle_input(void)
{
	int button;
	long now = *rb->current_tick;

	apply_video_layout();
	update_transient_overlays(now);

#ifdef HAS_BUTTON_HOLD
	if (rb->button_hold()) {
		release_mouse();
		reset_direction_buttons();
		pointer_wheel_reset();
		held_buttons = 0;
		while ((button = rb->button_get(false)) != BUTTON_NONE) {
			if (rb->default_event_handler(button) == SYS_USB_CONNECTED) {
				ForceMacOff = trueblnr;
			}
		}
		return;
	}
#endif

#ifdef HAVE_WHEEL_POSITION
	(void) poll_wheel_pointer();
#endif
	advance_held_direction(now);

	while ((button = rb->button_get(false)) != BUTTON_NONE) {
		unsigned int bare;
		blnr repeated;

		if (rb->default_event_handler(button) == SYS_USB_CONNECTED) {
			ForceMacOff = trueblnr;
			return;
		}

#ifdef BUTTON_RC_UP
		bare = button & ~(BUTTON_REPEAT | BUTTON_REL);
		if (bare == BUTTON_RC_UP || bare == BUTTON_RC_DOWN) {
			int direction = bare == BUTTON_RC_UP ? -1 : 1;

			if (button & BUTTON_REL) {
				held_direction_release_y(direction);
			} else {
				held_direction_touch(0, direction, now);
				advance_held_direction(now);
			}
			continue;
		}
#endif

		button = translate_remote_button(button);
		bare = button & ~(BUTTON_REPEAT | BUTTON_REL);
		repeated = (button & BUTTON_REPEAT) != 0;

		if (button & BUTTON_REL) {
			held_buttons &= ~bare;
			if (bare & BUTTON_SELECT) {
				release_mouse();
			}
			if (bare & BUTTON_LEFT) {
				held_direction_release_x(-1);
			}
			if (bare & BUTTON_RIGHT) {
				held_direction_release_x(1);
			}
			if (bare & BUTTON_SCROLL_BACK) {
				held_direction_release_y(-1);
			}
			if (bare & BUTTON_SCROLL_FWD) {
				held_direction_release_y(1);
			}
			if (bare & BUTTON_MENU) {
				if (menu_long_handled) {
					menu_long_handled = falseblnr;
				} else {
					exit_confirm = ! exit_confirm;
					force_redraw = trueblnr;
				}
			}
			if (bare & BUTTON_PLAY) {
				if (play_long_handled) {
					play_long_handled = falseblnr;
				} else if (exit_confirm) {
					exit_confirm = falseblnr;
					force_redraw = trueblnr;
				} else {
					toggle_keyboard_mode();
				}
			}
			continue;
		}

		held_buttons |= bare;
		if (exit_confirm) {
			if ((bare & BUTTON_SELECT) && ! repeated) {
				ForceMacOff = trueblnr;
				return;
			}
			continue;
		}

		if ((held_buttons & BUTTON_SELECT) && (bare & BUTTON_MENU) &&
			! repeated)
		{
			toggle_keyboard_mode();
			menu_long_handled = trueblnr;
			continue;
		}
		if ((bare & BUTTON_MENU) && repeated) {
			if (! menu_long_handled) {
				if (! active_docked_layout) {
					fit_view = ! fit_view;
				}
				view_hint_until = now + HZ * 3 / 2;
				force_redraw = trueblnr;
				menu_long_handled = trueblnr;
			}
			continue;
		}
		if ((bare & BUTTON_PLAY) && repeated) {
			if (! play_long_handled) {
				open_exit_confirmation();
				play_long_handled = trueblnr;
			}
			continue;
		}
		if (bare & BUTTON_LEFT) {
			held_direction_touch(-1, 0, now);
			advance_held_direction(now);
			continue;
		}
		if (bare & BUTTON_RIGHT) {
			held_direction_touch(1, 0, now);
			advance_held_direction(now);
			continue;
		}
		if (bare & (BUTTON_SCROLL_BACK | BUTTON_SCROLL_FWD)) {
			if (! pointer_state.wheel_available) {
				held_direction_touch(0,
					(bare & BUTTON_SCROLL_FWD) ? 1 : -1, now);
				advance_held_direction(now);
			}
			continue;
		}
		if ((bare & BUTTON_SELECT) && ! repeated) {
			if (keyboard_mode) {
				post_key(rb_keys[key_index].code);
			} else if (! rb_mouse_down) {
				MyMouseButtonSet(trueblnr);
				rb_mouse_down = trueblnr;
			}
		}
	}
}

LOCALVAR long extra_time_start;

GLOBALOSGLUFUNC blnr ExtraTimeNotOver(void)
{
	return TIME_BEFORE(*rb->current_tick,
		extra_time_start + ((HZ / 120) > 0 ? (HZ / 120) : 1));
}

GLOBALOSGLUPROC WaitForNextTick(void)
{
	long now;
	long elapsed;
	ui5b advance;

	handle_input();
	if (ForceMacOff) {
		return;
	}

	now = *rb->current_tick;
	elapsed = now - last_emu_tick;
	if (elapsed <= 0) {
		rb->yield();
		return;
	}

	last_emu_tick = now;
	true_time_accum += elapsed * MINIVMAC_TICKS_PER_SECOND;
	advance = true_time_accum / HZ;
	true_time_accum %= HZ;
	if (advance == 0) {
		rb->yield();
		return;
	}

	last_true_time += advance;
	OnTrueTime = last_true_time;
	extra_time_start = now;
}

#include "PROGMAIN.h"

LOCALPROC ReserveAllocAll(void)
{
	ReserveAllocOneBlock(&ROM, kROM_Size, 5, falseblnr);
	ReserveAllocOneBlock(&screencomparebuff, vMacScreenNumBytes, 5, trueblnr);
#if UseControlKeys
	ReserveAllocOneBlock(&CntrlDisplayBuff, vMacScreenNumBytes, 5, falseblnr);
#endif
	EmulationReserveAlloc();
}

LOCALFUNC blnr AllocMyMemory(void)
{
	uimr n;
	size_t arena_size;

	ReserveAllocOffset = 0;
	ReserveAllocBigBlock = nullpr;
	ReserveAllocAll();
	n = ReserveAllocOffset;

	ReserveAllocBigBlock = rb->plugin_get_audio_buffer(&arena_size);
	if (ReserveAllocBigBlock == nullpr || arena_size < n) {
		show_status("Mini vMac", "Not enough memory");
		return falseblnr;
	}

	rb->memset(ReserveAllocBigBlock, 0, n);
	ReserveAllocOffset = 0;
	ReserveAllocAll();
	return n == ReserveAllocOffset;
}

LOCALPROC UnallocMyMemory(void)
{
	rb->plugin_release_audio_buffer();
	ReserveAllocBigBlock = nullpr;
}

#if defined(IPOD_6G) && !defined(SIMULATOR)
LOCALPROC minivmac_videoout_event(unsigned short id, void *data)
{
	if (id == SYS_EVENT_VIDEOOUT_CHANGED) {
		docked_output = data != NULL ? trueblnr : falseblnr;
		video_layout_dirty = trueblnr;
	}
}
#endif

LOCALFUNC blnr InitOSGLU(void)
{
	rb->lcd_setfont(FONT_SYSFIXED);
	show_status("Mini vMac", "Starting");

	InitVideoTables();
	InitDrives();
	InitKeyCodes();
	rb_mouse_h = vMacScreenWidth / 2;
	rb_mouse_v = vMacScreenHeight / 2;
	rb_view_h = 0;
	rb_view_v = 0;
	fit_view = falseblnr;
	keyboard_mode = falseblnr;
	exit_confirm = falseblnr;
	menu_long_handled = falseblnr;
	play_long_handled = falseblnr;
	held_buttons = 0;
	key_index = 0;
	rb->memset(&pointer_state, 0, sizeof(pointer_state));
	pointer_wheel_reset();
	video_layout_dirty = trueblnr;
	apply_video_layout();
	force_redraw = trueblnr;
	MyMousePositionSet(rb_mouse_h, rb_mouse_v);

	if (! AllocMyMemory()) {
		return falseblnr;
	}
	if (! LoadMacRom()) {
		return falseblnr;
	}
	if (! LoadInitialImages()) {
		return falseblnr;
	}

	last_frame_tick = *rb->current_tick - MINIVMAC_FRAME_TICKS;
	last_emu_tick = *rb->current_tick;
	last_true_time = 0;
	true_time_accum = 0;
	hint_until = *rb->current_tick + HZ * 4;
	view_hint_until = active_docked_layout ?
		*rb->current_tick + HZ * 2 : 0;
	ForceMacOff = falseblnr;
	return trueblnr;
}

LOCALPROC UnInitOSGLU(void)
{
	if (videoout_event_registered) {
#if defined(IPOD_6G) && !defined(SIMULATOR)
		rb->remove_event(SYS_EVENT_VIDEOOUT_CHANGED,
			minivmac_videoout_event);
#endif
		videoout_event_registered = falseblnr;
	}
	release_mouse();
	UnInitDrives();
	UnallocMyMemory();
	backlight_use_settings();
#ifdef HAVE_ADJUSTABLE_CPU_FREQ
	rb->cpu_boost(false);
#endif
#ifdef HAVE_WHEEL_POSITION
	rb->wheel_send_events(true);
#endif
}

enum plugin_status plugin_start(const void *parameter)
{
	const char *launch_parameter = (const char *)parameter;

	initial_disk_path = launch_parameter;
	docked_output = falseblnr;
	videoout_event_registered = falseblnr;
#ifdef SIMULATOR
	if (launch_parameter != NULL &&
		! rb->strcmp(launch_parameter, MINIVMAC_SIMULATOR_DOCKED_TOKEN))
	{
		docked_output = trueblnr;
		initial_disk_path = NULL;
	}
#elif defined(IPOD_6G)
	if (launch_parameter != NULL &&
		! rb->strcmp(launch_parameter, MINIVMAC_VIDEOOUT_TOKEN))
	{
		docked_output = trueblnr;
		initial_disk_path = NULL;
	}
	videoout_event_registered = rb->add_event(
		SYS_EVENT_VIDEOOUT_CHANGED, minivmac_videoout_event) ?
		trueblnr : falseblnr;
#endif

#ifdef HAVE_ADJUSTABLE_CPU_FREQ
	rb->cpu_boost(true);
#endif
#ifdef HAVE_WHEEL_POSITION
	rb->wheel_send_events(false);
#endif
	backlight_ignore_timeout();

	if (InitOSGLU()) {
		ProgramMain();
	}

	if (SavedBriefMsg != nullpr) {
		show_status(SavedBriefMsg, SavedLongMsg);
		rb->sleep(HZ * 2);
	}

	UnInitOSGLU();

	return PLUGIN_OK;
}
