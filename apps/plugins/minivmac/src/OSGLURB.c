/*
	OSGLURB.c

	Mini vMac host glue for Rockbox.

	This file is a small Rockbox platform layer for the Mini vMac 36.04
	Macintosh Plus core. It intentionally starts without sound, clipboard,
	drag/drop, or the desktop Mini vMac UI.
*/

#include "CNFGRAPI.h"
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
#define MINIVMAC_VIEW_H 214
#define MINIVMAC_VIEW_BYTES (MINIVMAC_VIEW_W / 8)
#define MINIVMAC_FRAME_TICKS (HZ / 12)
#define MINIVMAC_CURSOR_STEP 4
#define MINIVMAC_WHEEL_STEP 2
#define MINIVMAC_TICKS_PER_SECOND 60
#define MINIVMAC_WHEEL_POSITIONS 96

#define NotAfileRef (-1)

LOCALVAR int Drives[NumDrives];
LOCALVAR char drive_names[NumDrives][MAX_PATH];
LOCALVAR const char *initial_disk_path;
LOCALVAR ui3p screencomparebuff;
LOCALVAR fb_data rb_screen[MINIVMAC_VIEW_W * MINIVMAC_VIEW_H];
LOCALVAR fb_data mono_lut[256][8];
LOCALVAR ui4r fit_xmap[MINIVMAC_VIEW_W];
LOCALVAR ui4r fit_ymap[MINIVMAC_VIEW_H];
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
LOCALVAR blnr vertical_axis;
LOCALVAR blnr force_redraw;
LOCALVAR int key_index;
#ifdef HAVE_WHEEL_POSITION
LOCALVAR int last_wheel_pos;
#endif

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

	for (i = 0; i < MINIVMAC_VIEW_W; ++i) {
		fit_xmap[i] = (i * MINIVMAC_SCREEN_W) / MINIVMAC_VIEW_W;
	}
	for (i = 0; i < MINIVMAC_VIEW_H; ++i) {
		fit_ymap[i] = (i * MINIVMAC_SCREEN_H) / MINIVMAC_VIEW_H;
	}
}

LOCALPROC update_viewport(void)
{
	int max_h = vMacScreenWidth - MINIVMAC_VIEW_W;
	int max_v = vMacScreenHeight - MINIVMAC_VIEW_H;
	int new_h;
	int new_v;

	if (max_h < 0) {
		max_h = 0;
	}
	if (max_v < 0) {
		max_v = 0;
	}

	new_h = clamp_int((int)rb_mouse_h - MINIVMAC_VIEW_W / 2, 0, max_h);
	new_v = clamp_int((int)rb_mouse_v - MINIVMAC_VIEW_H / 2, 0, max_v);
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

	for (y = 0; y < MINIVMAC_VIEW_H; ++y) {
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

LOCALPROC draw_fit_frame(ui3p screencurrentbuff)
{
	int y;
	int x;

	for (y = 0; y < MINIVMAC_VIEW_H; ++y) {
		ui3p row = screencurrentbuff
			+ fit_ymap[y] * vMacScreenMonoByteWidth;

		for (x = 0; x < MINIVMAC_VIEW_W; ++x) {
			int sx = fit_xmap[x];
			ui3b b = row[sx >> 3];
			rb_screen[y * MINIVMAC_VIEW_W + x] =
				mono_lut[b][sx & 7];
		}
	}
}

LOCALPROC draw_mac_frame(ui3p screencurrentbuff)
{
	long now = *rb->current_tick;

	if (! force_redraw
		&& TIME_BEFORE(now, last_frame_tick + MINIVMAC_FRAME_TICKS))
	{
		return;
	}
	last_frame_tick = now;
	update_viewport();

	if (fit_view) {
		draw_fit_frame(screencurrentbuff);
	} else {
		draw_native_frame(screencurrentbuff);
	}

	rb->lcd_set_background(LCD_RGBPACK(0x00, 0x00, 0x00));
	rb->lcd_clear_display();
	rb->lcd_bitmap(rb_screen, 0, 0, MINIVMAC_VIEW_W, MINIVMAC_VIEW_H);

	if (keyboard_mode) {
		char label[32];
		rb->snprintf(label, sizeof(label), "key:%s",
			rb_keys[key_index].label);
		rb->lcd_set_foreground(LCD_RGBPACK(0xff, 0xff, 0xff));
		rb->lcd_set_background(LCD_RGBPACK(0x00, 0x00, 0x00));
		rb->lcd_putsxy(0, MINIVMAC_VIEW_H + 4, label);
	} else {
		rb->lcd_set_foreground(LCD_RGBPACK(0xff, 0xff, 0xff));
		rb->lcd_set_background(LCD_RGBPACK(0x00, 0x00, 0x00));
		rb->lcd_putsxy(0, MINIVMAC_VIEW_H + 4,
			fit_view ? "fit menu:sharp sel:click" :
			vertical_axis ? "sharp wheel:y play:x sel:click" :
			"sharp wheel:x play:y sel:click");
	}

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
	rb_mouse_h = clamp_int(h, 0, vMacScreenWidth - 1);
	rb_mouse_v = clamp_int(v, 0, vMacScreenHeight - 1);
	MyMousePositionSet(rb_mouse_h, rb_mouse_v);
	force_redraw = trueblnr;
}

LOCALPROC handle_wheel_delta(int delta, unsigned int held_buttons)
{
	int dir = delta > 0 ? 1 : -1;
	int mag = delta > 0 ? delta : -delta;
	int pixels;

	if (keyboard_mode) {
		int n = (int)(sizeof(rb_keys) / sizeof(rb_keys[0]));
		key_index += dir;
		if (key_index < 0) {
			key_index = n - 1;
		} else if (key_index >= n) {
			key_index = 0;
		}
		force_redraw = trueblnr;
		return;
	}

	if (mag < 1) {
		mag = 1;
	}
	if (mag > 12) {
		mag = 12;
	}
	pixels = dir * mag * MINIVMAC_WHEEL_STEP;

	if (vertical_axis) {
		set_mouse_pos(rb_mouse_h, (int)rb_mouse_v + pixels);
	} else {
		set_mouse_pos((int)rb_mouse_h + pixels, rb_mouse_v);
	}
}

LOCALPROC handle_input(void)
{
	int button;
	static unsigned int held_buttons;

#ifdef HAVE_WHEEL_POSITION
	{
		int wheel = rb->wheel_status();

		if (wheel >= 0 && last_wheel_pos >= 0 && wheel != last_wheel_pos) {
			int delta = wheel - last_wheel_pos;

			if (delta < -(MINIVMAC_WHEEL_POSITIONS / 2)) {
				delta += MINIVMAC_WHEEL_POSITIONS;
			} else if (delta > (MINIVMAC_WHEEL_POSITIONS / 2)) {
				delta -= MINIVMAC_WHEEL_POSITIONS;
			}
			handle_wheel_delta(delta, held_buttons);
		}
		if (wheel >= 0) {
			last_wheel_pos = wheel;
		}
	}
#endif

	while ((button = rb->button_get(false)) != BUTTON_NONE) {
		unsigned int bare = button & ~(BUTTON_REPEAT | BUTTON_REL);
		blnr repeated = (button & BUTTON_REPEAT) != 0;

		if (rb->default_event_handler(button) == SYS_USB_CONNECTED) {
			ForceMacOff = trueblnr;
			return;
		}

		if (button & BUTTON_REL) {
			held_buttons &= ~bare;
			if ((bare & BUTTON_SELECT) && rb_mouse_down) {
				MyMouseButtonSet(falseblnr);
				rb_mouse_down = falseblnr;
			}
			continue;
		}

		held_buttons |= bare;

		if ((held_buttons & BUTTON_MENU) && (held_buttons & BUTTON_PLAY)) {
			ForceMacOff = trueblnr;
			return;
		}

		if (bare & (BUTTON_SCROLL_BACK | BUTTON_SCROLL_FWD)) {
			handle_wheel_delta((bare & BUTTON_SCROLL_FWD) ? 4 : -4,
				held_buttons);
		} else if ((held_buttons & BUTTON_SELECT) && (bare & BUTTON_MENU)) {
			if (! repeated) {
				keyboard_mode = ! keyboard_mode;
				force_redraw = trueblnr;
			}
		} else if (bare & BUTTON_MENU) {
			if (! repeated) {
				fit_view = ! fit_view;
				force_redraw = trueblnr;
			}
		} else if (bare & BUTTON_LEFT) {
			set_mouse_pos((int)rb_mouse_h - MINIVMAC_CURSOR_STEP * 4,
				rb_mouse_v);
		} else if (bare & BUTTON_RIGHT) {
			set_mouse_pos((int)rb_mouse_h + MINIVMAC_CURSOR_STEP * 4,
				rb_mouse_v);
		} else if (bare & BUTTON_PLAY) {
			if (! repeated) {
				vertical_axis = ! vertical_axis;
				force_redraw = trueblnr;
			}
		} else if (bare & BUTTON_SELECT) {
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
	vertical_axis = falseblnr;
	force_redraw = trueblnr;
	MyMousePositionSet(rb_mouse_h, rb_mouse_v);
#ifdef HAVE_WHEEL_POSITION
	last_wheel_pos = rb->wheel_status();
#endif

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
	ForceMacOff = falseblnr;
	return trueblnr;
}

LOCALPROC UnInitOSGLU(void)
{
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
	initial_disk_path = (const char *)parameter;

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
