;--------------------------------------------------------
; File Created by SDCC : free open source ISO C Compiler
; Version 4.5.1 #15267 (Linux)
;--------------------------------------------------------
	.module music_menu
	
;--------------------------------------------------------
; Public variables in this module
;--------------------------------------------------------
	.globl _menu_screen
	.globl _show_message
	.globl b_dt_audio_sync
	.globl _dt_audio_sync
	.globl b_dt_audio_get_home_record
	.globl _dt_audio_get_home_record
	.globl b_dt_audio_set_home_record
	.globl _dt_audio_set_home_record
	.globl b_dt_audio_play_sfx
	.globl _dt_audio_play_sfx
	.globl b_dt_record_from_media
	.globl _dt_record_from_media
	.globl b_dt_record_name
	.globl _dt_record_name
	.globl b_dt_home_record_menu
	.globl _dt_home_record_menu
;--------------------------------------------------------
; special function registers
;--------------------------------------------------------
	.area _HRAM
;--------------------------------------------------------
; ram data
;--------------------------------------------------------
	.area _DATA
;--------------------------------------------------------
; ram data
;--------------------------------------------------------
	.area _INITIALIZED
;--------------------------------------------------------
; absolute external ram data
;--------------------------------------------------------
	.area _DABS (ABS)
;--------------------------------------------------------
; global & static initialisations
;--------------------------------------------------------
	.area _HOME
	.area _GSINIT
	.area _GSFINAL
	.area _GSINIT
;--------------------------------------------------------
; Home
;--------------------------------------------------------
	.area _HOME
	.area _HOME
;--------------------------------------------------------
; code
;--------------------------------------------------------
	.area _CODE_255
;src/music_menu.c:9: uint8_t dt_record_from_media(uint8_t media_flag) BANKED {
;	---------------------------------
; Function dt_record_from_media
; ---------------------------------
	b_dt_record_from_media	= 255
_dt_record_from_media::
;src/music_menu.c:10: if (media_flag == 0x01u) return RECORD_RAIN_LOOP;
	ldhl	sp,	#6
	ld	a, (hl)
	dec	a
	jr	NZ, 00102$
	ld	a, #0x01
	ret
00102$:
;src/music_menu.c:11: if (media_flag == 0x02u) return RECORD_VENDING_DREAMS;
	ldhl	sp,	#6
	ld	a, (hl)
	sub	a, #0x02
	jr	NZ, 00104$
	ld	a, #0x02
	ret
00104$:
;src/music_menu.c:12: if (media_flag == 0x04u) return RECORD_ROOFTOP_SET;
	ldhl	sp,	#6
	ld	a, (hl)
	sub	a, #0x04
;src/music_menu.c:13: return RECORD_NONE;
	ld	a, #0x03
	ret	Z
	xor	a, a
;src/music_menu.c:14: }
	ret
;src/music_menu.c:16: const char *dt_record_name(uint8_t record_id) BANKED {
;	---------------------------------
; Function dt_record_name
; ---------------------------------
	b_dt_record_name	= 255
_dt_record_name::
;src/music_menu.c:17: switch (record_id) {
	ldhl	sp,	#6
	ld	a, (hl)
	dec	a
	jr	Z, 00101$
	ldhl	sp,	#6
	ld	a, (hl)
	sub	a, #0x02
	jr	Z, 00102$
	ldhl	sp,	#6
	ld	a, (hl)
	sub	a, #0x03
	jr	Z, 00103$
	jr	00104$
;src/music_menu.c:18: case RECORD_RAIN_LOOP: return "Rain Loop";
00101$:
	ld	bc, #___str_0
	ret
;src/music_menu.c:19: case RECORD_VENDING_DREAMS: return "Vending Dreams";
00102$:
	ld	bc, #___str_1
	ret
;src/music_menu.c:20: case RECORD_ROOFTOP_SET: return "Rooftop Set";
00103$:
	ld	bc, #___str_2
	ret
;src/music_menu.c:21: default: return "Silent";
00104$:
	ld	bc, #___str_3
;src/music_menu.c:22: }
;src/music_menu.c:23: }
	ret
___str_0:
	.ascii "Rain Loop"
	.db 0x00
___str_1:
	.ascii "Vending Dreams"
	.db 0x00
___str_2:
	.ascii "Rooftop Set"
	.db 0x00
___str_3:
	.ascii "Silent"
	.db 0x00
;src/music_menu.c:25: void dt_home_record_menu(uint8_t media, uint8_t room_id) BANKED {
;	---------------------------------
; Function dt_home_record_menu
; ---------------------------------
	b_dt_home_record_menu	= 255
_dt_home_record_menu::
	add	sp, #-21
;src/music_menu.c:28: uint8_t count = 0u;
	ldhl	sp,	#20
	ld	(hl), #0x00
;src/music_menu.c:29: uint8_t selected = 0u;
	ldhl	sp,	#15
	ld	(hl), #0x00
;src/music_menu.c:32: if (media == 0u) {
	ldhl	sp,	#27
	ld	a, (hl)
	or	a, a
	jr	NZ, 00102$
;src/music_menu.c:33: show_message("HOME DECK", "No records yet.\nFinish favors to bring home tiny songs.", "SELECT CLOSE");
	ld	de, #___str_6
	push	de
	ld	bc, #___str_5
	ld	de, #___str_4
	call	_show_message
;src/music_menu.c:34: return;
	jp	00119$
00102$:
;src/music_menu.c:37: if (media & 0x01u) {
	ldhl	sp,	#27
	ld	a, (hl)
	ldhl	sp,	#19
	ld	(hl), a
	push	hl
	bit	0, (hl)
	pop	hl
	jr	Z, 00106$
;src/music_menu.c:38: items[count] = "Rain Loop";
	ldhl	sp,	#0
	ld	a, #<(___str_7)
	ld	(hl+), a
	ld	(hl), #>(___str_7)
;src/music_menu.c:39: values[count] = RECORD_RAIN_LOOP;
	ldhl	sp,	#10
	ld	(hl), #0x01
;src/music_menu.c:40: if (dt_audio_get_home_record() == RECORD_RAIN_LOOP) selected = count;
	ld	e, #b_dt_audio_get_home_record
	ld	hl, #_dt_audio_get_home_record
	call	___sdcc_bcall_ehl
	dec	a
	jr	NZ, 00104$
	ldhl	sp,	#15
	ld	(hl), #0x00
00104$:
;src/music_menu.c:41: ++count;
	ldhl	sp,	#20
	ld	(hl), #0x01
00106$:
;src/music_menu.c:43: if (media & 0x02u) {
	push	hl
	ldhl	sp,	#21
	bit	1, (hl)
	pop	hl
	jr	Z, 00110$
;src/music_menu.c:44: items[count] = "Vending Dreams";
	ldhl	sp,	#0
	ld	c, l
	ld	b, h
	ldhl	sp,	#20
	ld	e, (hl)
	xor	a, a
	ld	l, e
	ld	h, a
	add	hl, hl
	add	hl, bc
	ld	c, l
	ld	b, h
	ld	(hl), #<(___str_8)
	inc	bc
	ld	a, #>(___str_8)
	ld	(bc), a
;src/music_menu.c:45: values[count] = RECORD_VENDING_DREAMS;
	ldhl	sp,	#10
	ld	c, l
	ld	b, h
	ldhl	sp,	#20
	ld	l, (hl)
	ld	h, #0x00
	add	hl, bc
	ld	c, l
	ld	b, h
	ld	(hl), #0x02
;src/music_menu.c:46: if (dt_audio_get_home_record() == RECORD_VENDING_DREAMS) selected = count;
	ld	e, #b_dt_audio_get_home_record
	ld	hl, #_dt_audio_get_home_record
	call	___sdcc_bcall_ehl
	sub	a, #0x02
	jr	NZ, 00108$
	ldhl	sp,	#20
	ld	a, (hl)
	ldhl	sp,	#15
	ld	(hl), a
00108$:
;src/music_menu.c:47: ++count;
	ldhl	sp,	#20
	inc	(hl)
00110$:
;src/music_menu.c:49: if (media & 0x04u) {
	push	hl
	ldhl	sp,	#21
	bit	2, (hl)
	pop	hl
	jr	Z, 00114$
;src/music_menu.c:50: items[count] = "Rooftop Set";
	ldhl	sp,	#20
	ld	a, (hl-)
	dec	hl
	ld	(hl+), a
	xor	a, a
	ld	(hl-), a
	ld	a, (hl-)
	dec	hl
	ld	(hl+), a
	xor	a, a
	ld	(hl-), a
	sla	(hl)
	inc	hl
	rl	(hl)
	push	hl
	ld	hl, #2
	add	hl, sp
	ld	e, l
	ld	d, h
	pop	hl
	ldhl	sp,	#16
	ld	a,	(hl+)
	ld	h, (hl)
	ld	l, a
	add	hl, de
	push	hl
	ld	a, l
	ldhl	sp,	#20
	ld	(hl), a
	pop	hl
	ld	a, h
	ldhl	sp,	#19
	ld	(hl-), a
	ld	a, (hl+)
	ld	h, (hl)
	ld	l, a
	ld	a, #<(___str_9)
	ld	(hl+), a
	ld	(hl), #>(___str_9)
;src/music_menu.c:51: values[count] = RECORD_ROOFTOP_SET;
	ldhl	sp,	#10
	ld	c, l
	ld	b, h
	ldhl	sp,	#20
	ld	l, (hl)
	ld	h, #0x00
	add	hl, bc
	ld	c, l
	ld	b, h
	ld	(hl), #0x03
;src/music_menu.c:52: if (dt_audio_get_home_record() == RECORD_ROOFTOP_SET) selected = count;
	ld	e, #b_dt_audio_get_home_record
	ld	hl, #_dt_audio_get_home_record
	call	___sdcc_bcall_ehl
	sub	a, #0x03
	jr	NZ, 00112$
	ldhl	sp,	#20
	ld	a, (hl)
	ldhl	sp,	#15
	ld	(hl), a
00112$:
;src/music_menu.c:53: ++count;
	ldhl	sp,	#20
	inc	(hl)
00114$:
;src/music_menu.c:56: items[count] = "Stop Deck";
	ldhl	sp,	#20
	ld	c, (hl)
	xor	a, a
	ld	b, a
	sla	c
	rl	b
	ld	hl, #0
	add	hl, sp
	add	hl, bc
	ld	c, l
	ld	b, h
	ld	(hl), #<(___str_10)
	inc	bc
	ld	a, #>(___str_10)
	ld	(bc), a
;src/music_menu.c:57: values[count] = RECORD_NONE;
	push	hl
	ld	hl, #12
	add	hl, sp
	ld	e, l
	ld	d, h
	pop	hl
	ldhl	sp,	#20
	ld	l, (hl)
	ld	h, #0x00
	add	hl, de
	ld	c, l
	ld	b, h
	xor	a, a
	ld	(bc), a
;src/music_menu.c:58: if (dt_audio_get_home_record() == RECORD_NONE) selected = count;
	ld	e, #b_dt_audio_get_home_record
	ld	hl, #_dt_audio_get_home_record
	call	___sdcc_bcall_ehl
	or	a, a
	jr	NZ, 00116$
	ldhl	sp,	#20
	ld	a, (hl)
	ldhl	sp,	#15
	ld	(hl), a
00116$:
;src/music_menu.c:59: ++count;
	ldhl	sp,	#20
	inc	(hl)
;src/music_menu.c:61: items[count] = "Back";
	ld	a, (hl)
	ld	d, #0x00
	add	a, a
	rl	d
	ld	e, a
	ld	hl, #0
	add	hl, sp
	add	hl, de
	push	hl
	ld	a, l
	ldhl	sp,	#20
	ld	(hl), a
	pop	hl
	ld	a, h
	ldhl	sp,	#19
	ld	(hl-), a
	ld	a, (hl+)
	ld	h, (hl)
	ld	l, a
	ld	a, #<(___str_11)
	ld	(hl+), a
	ld	(hl), #>(___str_11)
;src/music_menu.c:62: values[count] = 0xFFu;
	push	hl
	ld	hl, #12
	add	hl, sp
	ld	e, l
	ld	d, h
	pop	hl
	ldhl	sp,	#20
	ld	l, (hl)
	ld	h, #0x00
	add	hl, de
	push	hl
	ld	a, l
	ldhl	sp,	#20
	ld	(hl), a
	pop	hl
	ld	a, h
	ldhl	sp,	#19
	ld	(hl-), a
	ld	a, (hl+)
	ld	h, (hl)
	ld	l, a
	ld	(hl), #0xff
;src/music_menu.c:63: ++count;
	ldhl	sp,	#20
	inc	(hl)
;src/music_menu.c:65: choice = menu_screen("HOME DECK", "Load a found record", items, count, selected);
	ldhl	sp,	#15
	ld	a, (hl)
	push	af
	inc	sp
	ldhl	sp,	#21
	ld	a, (hl)
	push	af
	inc	sp
	ld	hl, #2
	add	hl, sp
	push	hl
	ld	bc, #___str_12
	ld	de, #___str_4
	call	_menu_screen
;src/music_menu.c:66: if (values[choice] == 0xFFu) {
	ld	e, a
	ld	d, #0x00
	ld	hl, #10
	add	hl, sp
	add	hl, de
	push	hl
	ld	a, l
	ldhl	sp,	#21
	ld	(hl), a
	pop	hl
	ld	a, h
	ldhl	sp,	#20
	ld	(hl-), a
	ld	a, (hl+)
	ld	e, a
	ld	a, (hl-)
	dec	hl
	ld	d, a
	ld	a, (de)
	ld	(hl), a
	inc	a
	jr	Z, 00119$
;src/music_menu.c:67: return;
;src/music_menu.c:70: dt_audio_set_home_record(values[choice]);
	ldhl	sp,	#18
	ld	a, (hl)
	push	af
	inc	sp
	ld	e, #b_dt_audio_set_home_record
	ld	hl, #_dt_audio_set_home_record
	call	___sdcc_bcall_ehl
	inc	sp
;src/music_menu.c:71: dt_audio_sync(room_id);
	ldhl	sp,	#28
	ld	a, (hl)
	push	af
	inc	sp
	ld	e, #b_dt_audio_sync
	ld	hl, #_dt_audio_sync
	call	___sdcc_bcall_ehl
	inc	sp
;src/music_menu.c:72: dt_audio_play_sfx((values[choice] == RECORD_NONE) ? SFX_CANCEL : SFX_CONFIRM);
	ldhl	sp,#19
	ld	a, (hl+)
	ld	e, a
	ld	d, (hl)
	ld	a, (de)
	ld	(hl), a
	or	a, a
	ld	a, #0x02
	jr	Z, 00122$
	ld	a, #0x01
00122$:
	push	af
	inc	sp
	ld	e, #b_dt_audio_play_sfx
	ld	hl, #_dt_audio_play_sfx
	call	___sdcc_bcall_ehl
	inc	sp
00119$:
;src/music_menu.c:73: }
	add	sp, #21
	ret
___str_4:
	.ascii "HOME DECK"
	.db 0x00
___str_5:
	.ascii "No records yet."
	.db 0x0a
	.ascii "Finish favors to bring home tiny songs."
	.db 0x00
___str_6:
	.ascii "SELECT CLOSE"
	.db 0x00
___str_7:
	.ascii "Rain Loop"
	.db 0x00
___str_8:
	.ascii "Vending Dreams"
	.db 0x00
___str_9:
	.ascii "Rooftop Set"
	.db 0x00
___str_10:
	.ascii "Stop Deck"
	.db 0x00
___str_11:
	.ascii "Back"
	.db 0x00
___str_12:
	.ascii "Load a found record"
	.db 0x00
	.area _CODE_255
	.area _INITIALIZER
	.area _CABS (ABS)
