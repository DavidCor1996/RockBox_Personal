;--------------------------------------------------------
; File Created by SDCC : free open source ISO C Compiler
; Version 4.5.1 #15267 (Linux)
;--------------------------------------------------------
	.module main
	
;--------------------------------------------------------
; Public variables in this module
;--------------------------------------------------------
	.globl _main
	.globl _menu_screen
	.globl _show_message
	.globl _font_set
	.globl _font_load
	.globl _font_init
	.globl b_dt_home_record_menu
	.globl _dt_home_record_menu
	.globl b_dt_record_name
	.globl _dt_record_name
	.globl b_dt_record_from_media
	.globl _dt_record_from_media
	.globl b_dt_audio_sync
	.globl _dt_audio_sync
	.globl b_dt_audio_get_home_record
	.globl _dt_audio_get_home_record
	.globl b_dt_audio_set_home_record
	.globl _dt_audio_set_home_record
	.globl b_dt_audio_stop_music
	.globl _dt_audio_stop_music
	.globl b_dt_audio_play_title_theme
	.globl _dt_audio_play_title_theme
	.globl b_dt_audio_play_sfx
	.globl _dt_audio_play_sfx
	.globl b_dt_audio_update
	.globl _dt_audio_update
	.globl b_dt_audio_init
	.globl _dt_audio_init
	.globl b_dt_init_assets
	.globl _dt_init_assets
	.globl _cls
	.globl _setchar
	.globl _posy
	.globl _posx
	.globl _gotoxy
	.globl _set_bkg_palette
	.globl _set_sprite_data
	.globl _set_bkg_tiles
	.globl _set_bkg_data
	.globl _display_off
	.globl _vsync
	.globl _waitpadup
	.globl _joypad
;--------------------------------------------------------
; special function registers
;--------------------------------------------------------
	.area _HRAM
;--------------------------------------------------------
; ram data
;--------------------------------------------------------
	.area _DATA
_ui_font:
	.ds 2
_room_id:
	.ds 1
_player_x:
	.ds 1
_player_y:
	.ds 1
_facing:
	.ds 1
_credits:
	.ds 1
_day_count:
	.ds 1
_task_id:
	.ds 1
_task_done:
	.ds 1
_inventory:
	.ds 1
_media:
	.ds 1
_decor_slots:
	.ds 3
_ride_level:
	.ds 1
_palette_mood:
	.ds 1
_frame_clock:
	.ds 1
_map_buffer:
	.ds 360
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
	.area _CODE
;src/main.c:88: static uint8_t tile_for_char(char c) {
;	---------------------------------
; Function tile_for_char
; ---------------------------------
_tile_for_char:
;src/main.c:89: switch (c) {
	cp	a, #0x23
	jp	Z, 00104$
	cp	a, #0x2c
	jp	Z, 00102$
	cp	a, #0x2e
	jp	Z, 00101$
	cp	a, #0x31
	jp	Z, 00132$
	cp	a, #0x32
	jp	Z, 00132$
	cp	a, #0x33
	jp	Z, 00132$
	cp	a, #0x3d
	jr	Z, 00103$
	cp	a, #0x47
	jp	Z, 00118$
	cp	a, #0x50
	jp	Z, 00116$
	cp	a, #0x52
	jr	Z, 00110$
	cp	a, #0x61
	jr	Z, 00113$
	cp	a, #0x62
	jr	Z, 00115$
	cp	a, #0x63
	jr	Z, 00107$
	cp	a, #0x64
	jr	Z, 00106$
	cp	a, #0x66
	jp	Z, 00124$
	cp	a, #0x67
	jr	Z, 00118$
	cp	a, #0x68
	jr	Z, 00120$
	cp	a, #0x69
	jp	Z, 00129$
	cp	a, #0x6b
	jp	Z, 00127$
	cp	a, #0x6c
	jr	Z, 00122$
	cp	a, #0x6d
	jr	Z, 00119$
	cp	a, #0x6e
	jr	Z, 00112$
	cp	a, #0x6f
	jr	Z, 00126$
	cp	a, #0x70
	jr	Z, 00121$
	cp	a, #0x71
	jr	Z, 00128$
	cp	a, #0x72
	jr	Z, 00110$
	cp	a, #0x73
	jr	Z, 00108$
	cp	a, #0x74
	jr	Z, 00125$
	cp	a, #0x76
	jr	Z, 00111$
	cp	a, #0x77
	jr	Z, 00105$
	cp	a, #0x78
	jr	Z, 00123$
	sub	a, #0x7c
	jr	Z, 00114$
	jr	00133$
;src/main.c:90: case '.': return 0u;
00101$:
	xor	a, a
	ret
;src/main.c:91: case ',': return 1u;
00102$:
	ld	a, #0x01
	ret
;src/main.c:92: case '=': return 2u;
00103$:
	ld	a, #0x02
	ret
;src/main.c:93: case '#': return 3u;
00104$:
	ld	a, #0x03
	ret
;src/main.c:94: case 'w': return 4u;
00105$:
	ld	a, #0x04
	ret
;src/main.c:95: case 'd': return 5u;
00106$:
	ld	a, #0x05
	ret
;src/main.c:96: case 'c': return 6u;
00107$:
	ld	a, #0x06
	ret
;src/main.c:97: case 's': return 7u;
00108$:
	ld	a, #0x07
	ret
;src/main.c:99: case 'R': return 8u;
00110$:
	ld	a, #0x08
	ret
;src/main.c:100: case 'v': return 9u;
00111$:
	ld	a, #0x09
	ret
;src/main.c:101: case 'n': return 10u;
00112$:
	ld	a, #0x0a
	ret
;src/main.c:102: case 'a': return 11u;
00113$:
	ld	a, #0x0b
	ret
;src/main.c:103: case '|': return 12u;
00114$:
	ld	a, #0x0c
	ret
;src/main.c:104: case 'b': return 13u;
00115$:
	ld	a, #0x0d
	ret
;src/main.c:105: case 'P': return 14u;
00116$:
	ld	a, #0x0e
	ret
;src/main.c:107: case 'G': return 15u;
00118$:
	ld	a, #0x0f
	ret
;src/main.c:108: case 'm': return 16u;
00119$:
	ld	a, #0x10
	ret
;src/main.c:109: case 'h': return 17u;
00120$:
	ld	a, #0x11
	ret
;src/main.c:110: case 'p': return 18u;
00121$:
	ld	a, #0x12
	ret
;src/main.c:111: case 'l': return 19u;
00122$:
	ld	a, #0x13
	ret
;src/main.c:112: case 'x': return 20u;
00123$:
	ld	a, #0x14
	ret
;src/main.c:113: case 'f': return 21u;
00124$:
	ld	a, #0x15
	ret
;src/main.c:114: case 't': return 22u;
00125$:
	ld	a, #0x16
	ret
;src/main.c:115: case 'o': return 23u;
00126$:
	ld	a, #0x17
	ret
;src/main.c:116: case 'k': return 24u;
00127$:
	ld	a, #0x18
	ret
;src/main.c:117: case 'q': return 30u;
00128$:
	ld	a, #0x1e
	ret
;src/main.c:118: case 'i': return 31u;
00129$:
	ld	a, #0x1f
	ret
;src/main.c:121: case '3':
00132$:
;src/main.c:122: return 0u;
	xor	a, a
	ret
;src/main.c:123: default:
00133$:
;src/main.c:124: return 0u;
	xor	a, a
;src/main.c:125: }
;src/main.c:126: }
	ret
;src/main.c:128: static uint8_t decor_tile(uint8_t decor_id) {
;	---------------------------------
; Function decor_tile
; ---------------------------------
_decor_tile:
	ld	c, a
;src/main.c:129: switch (decor_id) {
	ld	a, #0x05
	sub	a, c
	jr	C, 00106$
	ld	b, #0x00
	ld	hl, #00117$
	add	hl, bc
	add	hl, bc
	ld	c, (hl)
	inc	hl
	ld	h, (hl)
	ld	l, c
	jp	(hl)
00117$:
	.dw	00106$
	.dw	00101$
	.dw	00102$
	.dw	00103$
	.dw	00104$
	.dw	00105$
;src/main.c:130: case DECOR_POSTER: return 14u;
00101$:
	ld	a, #0x0e
	ret
;src/main.c:131: case DECOR_LAMP: return 19u;
00102$:
	ld	a, #0x13
	ret
;src/main.c:132: case DECOR_SHELF: return 16u;
00103$:
	ld	a, #0x10
	ret
;src/main.c:133: case DECOR_TANK: return 21u;
00104$:
	ld	a, #0x15
	ret
;src/main.c:134: case DECOR_DOCK: return 24u;
00105$:
	ld	a, #0x18
	ret
;src/main.c:135: default: return 0u;
00106$:
	xor	a, a
;src/main.c:136: }
;src/main.c:137: }
	ret
;src/main.c:139: static uint8_t has_item(uint8_t item) {
;	---------------------------------
; Function has_item
; ---------------------------------
_has_item:
;src/main.c:140: return (inventory & item) != 0u;
	ld	hl, #_inventory
	and	a, (hl)
	sub	a, #0x01
	ld	a, #0x00
	rla
	xor	a, #0x01
;src/main.c:141: }
	ret
;src/main.c:143: static void add_item(uint8_t item) {
;	---------------------------------
; Function add_item
; ---------------------------------
_add_item:
;src/main.c:144: inventory |= item;
	ld	hl, #_inventory
	or	a, (hl)
	ld	(hl), a
;src/main.c:145: }
	ret
;src/main.c:147: static uint8_t invert_dmg_palette_reg(uint8_t palette) {
;	---------------------------------
; Function invert_dmg_palette_reg
; ---------------------------------
_invert_dmg_palette_reg:
;src/main.c:149: (3u - (palette & 0x03u)) |
	ld	b, a
	and	a, #0x03
	ld	c, a
	ld	a, #0x03
	sub	a, c
	ld	c, a
;src/main.c:150: ((3u - ((palette >> 2u) & 0x03u)) << 2u) |
	ld	a, b
	rrca
	rrca
	and	a, #0x3
	ld	e, a
	ld	a, #0x03
	sub	a, e
	add	a, a
	add	a, a
	or	a, c
	ld	c, a
;src/main.c:151: ((3u - ((palette >> 4u) & 0x03u)) << 4u) |
	ld	a, b
	swap	a
	and	a, #0x3
	ld	e, a
	ld	a, #0x03
	sub	a, e
	swap	a
	and	a, #0xf0
	or	a, c
	ld	c, a
;src/main.c:152: ((3u - ((palette >> 6u) & 0x03u)) << 6u)
	ld	a, b
	rlca
	rlca
	and	a, #0x3
	ld	b, a
	ld	a, #0x03
	sub	a, b
	rrca
	rrca
	and	a, #0xc0
	or	a, c
;src/main.c:154: }
	ret
;src/main.c:156: static void set_inverted_bkg_palette(const palette_color_t *palettes) {
;	---------------------------------
; Function set_inverted_bkg_palette
; ---------------------------------
_set_inverted_bkg_palette:
	add	sp, #-8
	ld	c, e
	ld	b, d
;src/main.c:159: inverted[0] = palettes[3];
	ld	hl, #0x0006
	add	hl, bc
	ld	a,	(hl+)
	ld	h, (hl)
	ld	l, a
	inc	sp
	inc	sp
	push	hl
;src/main.c:160: inverted[1] = palettes[2];
	ld	hl, #0x0004
	add	hl, bc
	ld	a,	(hl+)
	ld	h, (hl)
	ld	l, a
	push	hl
	ld	a, l
	ldhl	sp,	#4
	ld	(hl), a
	pop	hl
	ld	a, h
	ldhl	sp,	#3
	ld	(hl), a
;src/main.c:161: inverted[2] = palettes[1];
	ld	l, c
	ld	h, b
	inc	hl
	inc	hl
	ld	a,	(hl+)
	ld	h, (hl)
	ld	l, a
	push	hl
	ld	a, l
	ldhl	sp,	#6
	ld	(hl), a
	pop	hl
	ld	a, h
	ldhl	sp,	#5
	ld	(hl), a
;src/main.c:162: inverted[3] = palettes[0];
	ld	l, c
	ld	h, b
	ld	a, (hl+)
	ld	c, a
	ld	b, (hl)
	ldhl	sp,	#6
	ld	a, c
	ld	(hl+), a
	ld	(hl), b
;src/main.c:163: set_bkg_palette(0u, 1u, inverted);
	ld	hl, #0
	add	hl, sp
	push	hl
	xor	a, a
	inc	a
	push	af
	call	_set_bkg_palette
;src/main.c:164: }
	add	sp, #12
	ret
;src/main.c:166: static void begin_display_reload(void) {
;	---------------------------------
; Function begin_display_reload
; ---------------------------------
_begin_display_reload:
;src/main.c:167: vsync();
	call	_vsync
;src/main.c:168: DISPLAY_OFF;
	call	_display_off
;src/main.c:169: HIDE_WIN;
	ldh	a, (_LCDC_REG + 0)
	and	a, #0xdf
	ldh	(_LCDC_REG + 0), a
;src/main.c:170: HIDE_SPRITES;
	ldh	a, (_LCDC_REG + 0)
	and	a, #0xfd
	ldh	(_LCDC_REG + 0), a
;src/main.c:171: SHOW_BKG;
	ldh	a, (_LCDC_REG + 0)
	or	a, #0x01
	ldh	(_LCDC_REG + 0), a
;src/main.c:172: }
	ret
;src/main.c:174: static void finish_display_reload(uint8_t show_sprites) {
;	---------------------------------
; Function finish_display_reload
; ---------------------------------
_finish_display_reload:
	ld	c, a
;src/main.c:175: SHOW_BKG;
	ldh	a, (_LCDC_REG + 0)
	or	a, #0x01
	ldh	(_LCDC_REG + 0), a
;src/main.c:176: if (show_sprites) {
	ld	a, c
	or	a, a
	jr	Z, 00102$
;src/main.c:177: SHOW_SPRITES;
	ldh	a, (_LCDC_REG + 0)
	or	a, #0x02
	ldh	(_LCDC_REG + 0), a
	jr	00103$
00102$:
;src/main.c:179: HIDE_SPRITES;
	ldh	a, (_LCDC_REG + 0)
	and	a, #0xfd
	ldh	(_LCDC_REG + 0), a
00103$:
;src/main.c:181: DISPLAY_ON;
	ldh	a, (_LCDC_REG + 0)
	or	a, #0x80
	ldh	(_LCDC_REG + 0), a
;src/main.c:182: }
	ret
;src/main.c:184: static void apply_palette(void) {
;	---------------------------------
; Function apply_palette
; ---------------------------------
_apply_palette:
;src/main.c:186: uint8_t outdoor = (uint8_t)(room_id == ROOM_DISTRICT || room_id == ROOM_SOUTHLINE);
	ld	hl, #_room_id
	ld	a, (hl)
	or	a, a
	jr	Z, 00107$
	ld	a, (hl)
	sub	a, #0x06
	ld	c, #0x00
	jr	NZ, 00108$
00107$:
	ld	c, #0x01
00108$:
;src/main.c:188: if (palette_mood == 0u) {
	ld	a, (#_palette_mood)
;src/main.c:189: bgp = outdoor ? 0xD2u : 0xE4u;
	or	a, a
	jr	NZ, 00102$
	or	a, c
	jr	Z, 00109$
	ld	a, #0xd2
	jr	00103$
00109$:
	ld	a, #0xe4
	jr	00103$
00102$:
;src/main.c:191: bgp = outdoor ? 0xC9u : 0xD8u;
	ld	a, c
	or	a, a
	ld	a, #0xc9
	jr	NZ, 00112$
	ld	a, #0xd8
00112$:
00103$:
;src/main.c:193: BGP_REG = outdoor ? invert_dmg_palette_reg(bgp) : bgp;
	inc	c
	dec	c
	jr	Z, 00113$
	call	_invert_dmg_palette_reg
00113$:
	ldh	(_BGP_REG + 0), a
;src/main.c:194: OBP0_REG = 0xE4u;
	ld	a, #0xe4
	ldh	(_OBP0_REG + 0), a
;src/main.c:195: OBP1_REG = 0xE4u;
	ld	a, #0xe4
	ldh	(_OBP1_REG + 0), a
;src/main.c:196: }
	ret
;src/main.c:198: static uint8_t tile_to_sprite_x(uint8_t tile_x) {
;	---------------------------------
; Function tile_to_sprite_x
; ---------------------------------
_tile_to_sprite_x:
;src/main.c:199: return (uint8_t)(tile_x * 8u + 8u);
	add	a, a
	add	a, a
	add	a, a
	add	a, #0x08
;src/main.c:200: }
	ret
;src/main.c:202: static uint8_t tile_to_sprite_y(uint8_t tile_y) {
;	---------------------------------
; Function tile_to_sprite_y
; ---------------------------------
_tile_to_sprite_y:
;src/main.c:203: return (uint8_t)(tile_y * 8u + 16u);
	add	a, a
	add	a, a
	add	a, a
	add	a, #0x10
;src/main.c:204: }
	ret
;src/main.c:206: static void set_quad_sprite(uint8_t oam_base, uint8_t tile_base, uint8_t x, uint8_t y, uint8_t flip_x) {
;	---------------------------------
; Function set_quad_sprite
; ---------------------------------
_set_quad_sprite:
	add	sp, #-7
	ld	c, a
	ld	b, e
;src/main.c:207: uint8_t top_left = tile_base;
	ldhl	sp,	#4
;src/main.c:208: uint8_t top_right = (uint8_t)(tile_base + 1u);
;src/main.c:209: uint8_t bottom_left = (uint8_t)(tile_base + 2u);
	ld	a, b
	ld	(hl+), a
	ld	e, b
	inc	e
	ld	a, b
	add	a, #0x02
;src/main.c:210: uint8_t bottom_right = (uint8_t)(tile_base + 3u);
	ld	(hl+), a
	ld	a, b
	add	a, #0x03
	ld	(hl), a
;src/main.c:211: uint8_t prop = flip_x ? S_FLIPX : 0u;
	ldhl	sp,	#11
	ld	a, (hl)
	or	a, a
	jr	Z, 00122$
	ld	b, #0x20
	jr	00123$
00122$:
	ld	b, #0x00
00123$:
;src/main.c:213: if (flip_x) {
	ldhl	sp,	#11
	ld	a, (hl)
	or	a, a
	jr	Z, 00102$
;/tmp/gbdk-4.5.0/include/gb/gb.h:1887: shadow_OAM[nb].tile=tile;
	ld	l, c
	ld	h, #0x00
	add	hl, hl
	add	hl, hl
	ld	a, #<(_shadow_OAM)
	add	a, l
	ld	l, a
	ld	a, #>(_shadow_OAM)
	adc	a, h
	ld	h, a
	inc	hl
	inc	hl
	ld	(hl), e
;src/main.c:215: set_sprite_tile(oam_base + 1u, top_left);
	ld	e, c
	inc	e
;/tmp/gbdk-4.5.0/include/gb/gb.h:1887: shadow_OAM[nb].tile=tile;
	ld	l, e
	ld	h, #0x00
	add	hl, hl
	add	hl, hl
	ld	de, #_shadow_OAM
	add	hl, de
	inc	hl
	inc	hl
	ld	e, l
	ld	d, h
	ldhl	sp,	#4
	ld	a, (hl)
	ld	(de), a
;src/main.c:216: set_sprite_tile(oam_base + 2u, bottom_right);
	ld	e, c
	inc	e
	inc	e
;/tmp/gbdk-4.5.0/include/gb/gb.h:1887: shadow_OAM[nb].tile=tile;
	ld	l, e
	ld	h, #0x00
	add	hl, hl
	add	hl, hl
	ld	de, #_shadow_OAM
	add	hl, de
	inc	hl
	inc	hl
	ld	e, l
	ld	d, h
	ldhl	sp,	#6
	ld	a, (hl)
	ld	(de), a
;src/main.c:217: set_sprite_tile(oam_base + 3u, bottom_left);
	ld	e, c
	inc	e
	inc	e
	inc	e
;/tmp/gbdk-4.5.0/include/gb/gb.h:1887: shadow_OAM[nb].tile=tile;
	ld	l, e
	ld	h, #0x00
	add	hl, hl
	add	hl, hl
	ld	de, #_shadow_OAM
	add	hl, de
	inc	hl
	inc	hl
	ld	e, l
	ld	d, h
	ldhl	sp,	#5
	ld	a, (hl)
	ld	(de), a
;src/main.c:217: set_sprite_tile(oam_base + 3u, bottom_left);
	jr	00103$
00102$:
;/tmp/gbdk-4.5.0/include/gb/gb.h:1887: shadow_OAM[nb].tile=tile;
	ld	l, c
	xor	a, a
	ld	h, a
	add	hl, hl
	add	hl, hl
	push	de
	ld	de, #_shadow_OAM
	add	hl, de
	inc	hl
	inc	hl
	pop	de
	push	hl
	ldhl	sp,	#6
	ld	a, (hl)
	pop	hl
	ld	(hl), a
;src/main.c:220: set_sprite_tile(oam_base + 1u, top_right);
	ld	d, c
	inc	d
;/tmp/gbdk-4.5.0/include/gb/gb.h:1887: shadow_OAM[nb].tile=tile;
	xor	a, a
	ld	l, d
	ld	h, a
	add	hl, hl
	add	hl, hl
	push	de
	ld	de, #_shadow_OAM
	add	hl, de
	inc	hl
	inc	hl
	pop	de
	ld	(hl), e
;src/main.c:221: set_sprite_tile(oam_base + 2u, bottom_left);
	ld	e, c
	inc	e
	inc	e
;/tmp/gbdk-4.5.0/include/gb/gb.h:1887: shadow_OAM[nb].tile=tile;
	xor	a, a
	ld	l, e
	ld	h, a
	add	hl, hl
	add	hl, hl
	ld	de, #_shadow_OAM
	add	hl, de
	inc	hl
	inc	hl
	ld	e, l
	ld	d, h
	ldhl	sp,	#5
	ld	a, (hl)
	ld	(de), a
;src/main.c:222: set_sprite_tile(oam_base + 3u, bottom_right);
	ld	e, c
	inc	e
	inc	e
	inc	e
;/tmp/gbdk-4.5.0/include/gb/gb.h:1887: shadow_OAM[nb].tile=tile;
	xor	a, a
	ld	l, e
	ld	h, a
	add	hl, hl
	add	hl, hl
	ld	de, #_shadow_OAM
	add	hl, de
	inc	hl
	inc	hl
	ld	e, l
	ld	d, h
	ldhl	sp,	#6
	ld	a, (hl)
	ld	(de), a
;src/main.c:222: set_sprite_tile(oam_base + 3u, bottom_right);
00103$:
;/tmp/gbdk-4.5.0/include/gb/gb.h:1946: shadow_OAM[nb].prop=prop;
	ld	e, c
	xor	a, a
	sla	e
	adc	a, a
	sla	e
	adc	a, a
	ldhl	sp,	#0
	ld	(hl), e
	inc	hl
	ld	(hl), a
	ld	de, #_shadow_OAM
	pop	hl
	push	hl
	add	hl, de
	inc	hl
	inc	hl
	inc	hl
	ld	e, l
	ld	d, h
	ld	a, b
	ld	(de), a
;src/main.c:226: set_sprite_prop(oam_base + 1u, prop);
	ld	a, c
	inc	a
	ldhl	sp,	#5
	ld	(hl), a
	ld	e, (hl)
;/tmp/gbdk-4.5.0/include/gb/gb.h:1946: shadow_OAM[nb].prop=prop;
	xor	a, a
	ld	l, e
	ld	h, a
	add	hl, hl
	add	hl, hl
	ld	de, #_shadow_OAM
	add	hl, de
	inc	hl
	inc	hl
	inc	hl
	ld	(hl), b
;src/main.c:227: set_sprite_prop(oam_base + 2u, prop);
	ld	a, c
	add	a, #0x02
	ldhl	sp,	#2
	ld	(hl), a
	ld	e, (hl)
;/tmp/gbdk-4.5.0/include/gb/gb.h:1946: shadow_OAM[nb].prop=prop;
	xor	a, a
	ld	l, e
	ld	h, a
	add	hl, hl
	add	hl, hl
	ld	de, #_shadow_OAM
	add	hl, de
	inc	hl
	inc	hl
	inc	hl
	ld	(hl), b
;src/main.c:228: set_sprite_prop(oam_base + 3u, prop);
	inc	c
	inc	c
	inc	c
	ld	e, c
;/tmp/gbdk-4.5.0/include/gb/gb.h:1946: shadow_OAM[nb].prop=prop;
	xor	a, a
	ld	l, e
	ld	h, a
	add	hl, hl
	add	hl, hl
	ld	de, #_shadow_OAM
	add	hl, de
	inc	hl
	inc	hl
	inc	hl
	ld	(hl), b
;src/main.c:230: move_sprite(oam_base + 0u, x, y);
	ldhl	sp,	#10
	ld	a, (hl-)
	ld	b, a
	ld	a, (hl)
	ldhl	sp,	#3
	ld	(hl), a
;/tmp/gbdk-4.5.0/include/gb/gb.h:1973: OAM_item_t * itm = &shadow_OAM[nb];
	ld	de, #_shadow_OAM
	pop	hl
	push	hl
	add	hl, de
	ld	e, l
	ld	d, h
;/tmp/gbdk-4.5.0/include/gb/gb.h:1974: itm->y=y, itm->x=x;
	ld	a, b
	ld	(de), a
	inc	de
	ldhl	sp,	#3
	ld	a, (hl)
	ld	(de), a
;src/main.c:231: move_sprite(oam_base + 1u, (uint8_t)(x + 8u), y);
	ld	a, (hl+)
	add	a, #0x08
	ld	(hl), a
	ld	a, (hl+)
	inc	hl
;/tmp/gbdk-4.5.0/include/gb/gb.h:1973: OAM_item_t * itm = &shadow_OAM[nb];
	ld	(hl-), a
	ld	e, (hl)
	xor	a, a
	ld	l, e
	ld	h, a
	add	hl, hl
	add	hl, hl
	ld	a, l
	add	a, #<(_shadow_OAM)
	ld	e, a
	ld	a, h
	adc	a, #>(_shadow_OAM)
	ld	d, a
;/tmp/gbdk-4.5.0/include/gb/gb.h:1974: itm->y=y, itm->x=x;
	ld	a, b
	ld	(de), a
	inc	de
	ldhl	sp,	#6
;src/main.c:232: move_sprite(oam_base + 2u, x, (uint8_t)(y + 8u));
	ld	a, (hl-)
	ld	(de), a
	ld	a, b
	add	a, #0x08
	ld	(hl), a
	ld	a, (hl+)
	ld	(hl), a
;/tmp/gbdk-4.5.0/include/gb/gb.h:1973: OAM_item_t * itm = &shadow_OAM[nb];
	ld	de, #_shadow_OAM+0
	ldhl	sp,	#2
	ld	b, (hl)
	xor	a, a
	ld	l, b
	ld	h, a
	add	hl, hl
	add	hl, hl
	add	hl, de
	ld	e, l
	ld	d, h
;/tmp/gbdk-4.5.0/include/gb/gb.h:1974: itm->y=y, itm->x=x;
	ldhl	sp,	#6
	ld	a, (hl)
	ld	(de), a
	inc	de
	ldhl	sp,	#3
	ld	a, (hl)
	ld	(de), a
;/tmp/gbdk-4.5.0/include/gb/gb.h:1973: OAM_item_t * itm = &shadow_OAM[nb];
	ld	de, #_shadow_OAM+0
	xor	a, a
	ld	l, c
	ld	h, a
	add	hl, hl
	add	hl, hl
	add	hl, de
	ld	c, l
	ld	b, h
;/tmp/gbdk-4.5.0/include/gb/gb.h:1974: itm->y=y, itm->x=x;
	ldhl	sp,	#5
	ld	a, (hl-)
	ld	(bc), a
	inc	bc
	ld	a, (hl)
	ld	(bc), a
;src/main.c:233: move_sprite(oam_base + 3u, (uint8_t)(x + 8u), (uint8_t)(y + 8u));
;src/main.c:234: }
	add	sp, #7
	pop	hl
	add	sp, #3
	jp	(hl)
;src/main.c:236: static uint8_t player_frame_base(uint8_t walk_phase) {
;	---------------------------------
; Function player_frame_base
; ---------------------------------
_player_frame_base:
	ld	c, a
;src/main.c:237: if (facing == DIR_DOWN) {
	ld	a, (#_facing)
;src/main.c:238: return walk_phase ? PLAYER_FRAME_DOWN_WALK : PLAYER_FRAME_DOWN_STAND;
	or	a, a
	jr	NZ, 00102$
	or	a, c
	jr	Z, 00107$
	ld	a, #0x04
	ret
00107$:
	xor	a, a
	ret
00102$:
;src/main.c:240: if (facing == DIR_UP) {
	ld	a, (#_facing)
	dec	a
	jr	NZ, 00104$
;src/main.c:241: return walk_phase ? PLAYER_FRAME_UP_WALK : PLAYER_FRAME_UP_STAND;
	ld	a, c
	or	a, a
	jr	Z, 00109$
	ld	a, #0x0c
	ret
00109$:
	ld	a, #0x08
	ret
00104$:
;src/main.c:243: return walk_phase ? PLAYER_FRAME_SIDE_WALK : PLAYER_FRAME_SIDE_STAND;
	ld	a, c
	or	a, a
	ld	a, #0x14
	ret	NZ
	ld	a, #0x10
;src/main.c:244: }
	ret
;src/main.c:246: static uint8_t current_room_tile_count(void) {
;	---------------------------------
; Function current_room_tile_count
; ---------------------------------
_current_room_tile_count:
;src/main.c:247: switch (room_id) {
	ld	a, #0x06
	ld	hl, #_room_id
	sub	a, (hl)
	jr	C, 00108$
	ld	c, (hl)
	ld	b, #0x00
	ld	hl, #00119$
	add	hl, bc
	add	hl, bc
	ld	c, (hl)
	inc	hl
	ld	h, (hl)
	ld	l, c
	jp	(hl)
00119$:
	.dw	00101$
	.dw	00102$
	.dw	00103$
	.dw	00104$
	.dw	00105$
	.dw	00106$
	.dw	00107$
;src/main.c:248: case ROOM_DISTRICT: return luma_lane_district_v2_TILE_COUNT;
00101$:
	ld	a, #0xa2
	ret
;src/main.c:249: case ROOM_APARTMENT: return (uint8_t)(luma_lane_apartment_TILE_COUNT + APARTMENT_DECOR_TILE_COUNT);
00102$:
	ld	a, #0xdd
	ret
;src/main.c:250: case ROOM_RECORDS: return needle_and_neon_records_TILE_COUNT;
00103$:
	ld	a, #0x55
	ret
;src/main.c:251: case ROOM_TECH: return rook_repair_tech_TILE_COUNT;
00104$:
	ld	a, #0x53
	ret
;src/main.c:252: case ROOM_RAMEN: return soma_ramen_TILE_COUNT;
00105$:
	ld	a, #0x4f
	ret
;src/main.c:253: case ROOM_ROOFTOP: return iona_rooftop_TILE_COUNT;
00106$:
	ld	a, #0x44
	ret
;src/main.c:254: case ROOM_SOUTHLINE: return southline_overpass_TILE_COUNT;
00107$:
	ld	a, #0x61
	ret
;src/main.c:255: default: return BG_TILE_COUNT;
00108$:
	ld	a, #0x20
;src/main.c:256: }
;src/main.c:257: }
	ret
;src/main.c:259: static uint8_t current_sprite_tile_base(void) {
;	---------------------------------
; Function current_sprite_tile_base
; ---------------------------------
_current_sprite_tile_base:
;src/main.c:260: return current_room_tile_count();
;src/main.c:261: }
	jp	_current_room_tile_count
;src/main.c:263: static uint8_t current_ride_level(void) {
;	---------------------------------
; Function current_ride_level
; ---------------------------------
_current_ride_level:
;src/main.c:264: if ((room_id == ROOM_DISTRICT) || (room_id == ROOM_SOUTHLINE)) {
	ld	hl, #_room_id
	ld	a, (hl)
	or	a, a
	jr	Z, 00101$
	ld	a, (hl)
	sub	a, #0x06
	jr	NZ, 00102$
00101$:
;src/main.c:265: return ride_level;
	ld	a, (_ride_level)
	ret
00102$:
;src/main.c:267: return RIDE_FOOT;
	xor	a, a
;src/main.c:268: }
	ret
;src/main.c:270: static uint8_t movement_step_frames(void) {
;	---------------------------------
; Function movement_step_frames
; ---------------------------------
_movement_step_frames:
;src/main.c:271: switch (current_ride_level()) {
	call	_current_ride_level
	cp	a, #0x01
	jr	Z, 00101$
	cp	a, #0x02
	jr	Z, 00102$
	sub	a, #0x03
	jr	Z, 00103$
	jr	00104$
;src/main.c:272: case RIDE_BICYCLE: return 5u;
00101$:
	ld	a, #0x05
	ret
;src/main.c:273: case RIDE_SCOOTER: return 4u;
00102$:
	ld	a, #0x04
	ret
;src/main.c:274: case RIDE_MOTORCYCLE: return 4u;
00103$:
	ld	a, #0x04
	ret
;src/main.c:275: default: return 6u;
00104$:
	ld	a, #0x06
;src/main.c:276: }
;src/main.c:277: }
	ret
;src/main.c:279: static uint8_t movement_repeat_delay(void) {
;	---------------------------------
; Function movement_repeat_delay
; ---------------------------------
_movement_repeat_delay:
;src/main.c:280: switch (current_ride_level()) {
	call	_current_ride_level
	cp	a, #0x01
	jr	Z, 00101$
	cp	a, #0x02
	jr	Z, 00102$
	sub	a, #0x03
	jr	Z, 00103$
	jr	00104$
;src/main.c:281: case RIDE_BICYCLE: return 1u;
00101$:
	ld	a, #0x01
	ret
;src/main.c:282: case RIDE_SCOOTER: return 0u;
00102$:
	xor	a, a
	ret
;src/main.c:283: case RIDE_MOTORCYCLE: return 0u;
00103$:
	xor	a, a
	ret
;src/main.c:284: default: return 2u;
00104$:
	ld	a, #0x02
;src/main.c:285: }
;src/main.c:286: }
	ret
;src/main.c:288: static uint8_t movement_repeat_interval(void) {
;	---------------------------------
; Function movement_repeat_interval
; ---------------------------------
_movement_repeat_interval:
;src/main.c:289: switch (current_ride_level()) {
	call	_current_ride_level
	sub	a, #0x03
	jr	NZ, 00102$
;src/main.c:290: case RIDE_MOTORCYCLE: return 0u;
	xor	a, a
	ret
;src/main.c:291: default: return 0u;
00102$:
	xor	a, a
;src/main.c:292: }
;src/main.c:293: }
	ret
;src/main.c:295: static uint8_t current_sprite_tile_count(void) {
;	---------------------------------
; Function current_sprite_tile_count
; ---------------------------------
_current_sprite_tile_count:
;src/main.c:296: if (room_id == ROOM_APARTMENT) {
	ld	a, (#_room_id)
	dec	a
	jr	NZ, 00102$
;src/main.c:297: return PLAYER_SPRITE_TILE_COUNT;
	ld	a, #0x18
	ret
00102$:
;src/main.c:299: if ((room_id == ROOM_DISTRICT) || (room_id == ROOM_SOUTHLINE)) {
	ld	hl, #_room_id
	ld	a, (hl)
	or	a, a
	jr	Z, 00103$
	ld	a, (hl)
	sub	a, #0x06
	jr	NZ, 00104$
00103$:
;src/main.c:300: return SPRITE_TILE_COUNT;
	ld	a, #0x34
	ret
00104$:
;src/main.c:302: return SPRITE_TILE_COUNT;
	ld	a, #0x34
;src/main.c:303: }
	ret
;src/main.c:305: static void hide_actor(uint8_t oam_base) {
;	---------------------------------
; Function hide_actor
; ---------------------------------
_hide_actor:
	ld	e, a
;/tmp/gbdk-4.5.0/include/gb/gb.h:1973: OAM_item_t * itm = &shadow_OAM[nb];
	ld	bc, #_shadow_OAM+0
	ld	l, e
	xor	a, a
	ld	h, a
	add	hl, hl
	add	hl, hl
	add	hl, bc
;/tmp/gbdk-4.5.0/include/gb/gb.h:1974: itm->y=y, itm->x=x;
	xor	a, a
	ld	(hl+), a
	ld	(hl), a
;src/main.c:307: move_sprite(oam_base + 1u, 0u, 0u);
	ld	d, e
	inc	d
;/tmp/gbdk-4.5.0/include/gb/gb.h:1973: OAM_item_t * itm = &shadow_OAM[nb];
	ld	bc, #_shadow_OAM+0
	ld	l, d
	ld	h, #0x00
	add	hl, hl
	add	hl, hl
	add	hl, bc
;/tmp/gbdk-4.5.0/include/gb/gb.h:1974: itm->y=y, itm->x=x;
	xor	a, a
	ld	(hl+), a
	ld	(hl), a
;src/main.c:308: move_sprite(oam_base + 2u, 0u, 0u);
	ld	d, e
	inc	d
	inc	d
;/tmp/gbdk-4.5.0/include/gb/gb.h:1973: OAM_item_t * itm = &shadow_OAM[nb];
	ld	bc, #_shadow_OAM+0
	ld	l, d
	ld	h, #0x00
	add	hl, hl
	add	hl, hl
	add	hl, bc
;/tmp/gbdk-4.5.0/include/gb/gb.h:1974: itm->y=y, itm->x=x;
	xor	a, a
	ld	(hl+), a
	ld	(hl), a
;src/main.c:309: move_sprite(oam_base + 3u, 0u, 0u);
	inc	e
	inc	e
	inc	e
;/tmp/gbdk-4.5.0/include/gb/gb.h:1973: OAM_item_t * itm = &shadow_OAM[nb];
	ld	bc, #_shadow_OAM+0
	xor	a, a
	ld	l, e
	ld	h, a
	add	hl, hl
	add	hl, hl
	add	hl, bc
;/tmp/gbdk-4.5.0/include/gb/gb.h:1974: itm->y=y, itm->x=x;
	xor	a, a
	ld	(hl+), a
	ld	(hl), a
;src/main.c:309: move_sprite(oam_base + 3u, 0u, 0u);
;src/main.c:310: }
	ret
;src/main.c:312: static void set_single_sprite(uint8_t oam, uint8_t tile, uint8_t x, uint8_t y, uint8_t props) {
;	---------------------------------
; Function set_single_sprite
; ---------------------------------
_set_single_sprite:
;/tmp/gbdk-4.5.0/include/gb/gb.h:1887: shadow_OAM[nb].tile=tile;
	ld	l, a
	ld	h, #0x00
	add	hl, hl
	add	hl, hl
	ld	c, l
	ld	b, h
	ld	hl,#_shadow_OAM + 1
	add	hl,bc
	inc	hl
	ld	(hl), e
;src/main.c:314: set_sprite_prop(oam, props);
	ldhl	sp,	#4
	ld	e, (hl)
;/tmp/gbdk-4.5.0/include/gb/gb.h:1946: shadow_OAM[nb].prop=prop;
	ld	hl,#_shadow_OAM + 1
	add	hl,bc
	inc	hl
	inc	hl
	ld	(hl), e
;src/main.c:315: move_sprite(oam, x, y);
	ldhl	sp,	#3
	ld	d, (hl)
	dec	hl
	ld	e, (hl)
;/tmp/gbdk-4.5.0/include/gb/gb.h:1973: OAM_item_t * itm = &shadow_OAM[nb];
	ld	hl, #_shadow_OAM
	add	hl, bc
;/tmp/gbdk-4.5.0/include/gb/gb.h:1974: itm->y=y, itm->x=x;
	ld	(hl), d
	inc	hl
	ld	(hl), e
;src/main.c:315: move_sprite(oam, x, y);
;src/main.c:316: }
	pop	hl
	add	sp, #3
	jp	(hl)
;src/main.c:318: static uint8_t traffic_tile(uint8_t type, uint8_t vertical, uint8_t phase) {
;	---------------------------------
; Function traffic_tile
; ---------------------------------
_traffic_tile:
;src/main.c:319: if (type == TRAFFIC_TYPE_BIKE) {
	or	a, a
	jr	NZ, 00102$
;src/main.c:320: return (uint8_t)(current_sprite_tile_base() + TRAFFIC_SPRITE_TILE_OFFSET + (vertical ? 2u : 0u) + phase);
	push	de
	call	_current_sprite_tile_base
	pop	de
	add	a, #0x2c
	inc	e
	dec	e
	ld	c, #0x02
	jr	NZ, 00106$
	ld	c, #0x00
00106$:
	add	a, c
	ldhl	sp,	#2
	add	a, (hl)
	jr	00103$
00102$:
;src/main.c:322: return (uint8_t)(current_sprite_tile_base() + TRAFFIC_SPRITE_TILE_OFFSET + 4u + (vertical ? 2u : 0u) + phase);
	push	de
	call	_current_sprite_tile_base
	pop	de
	add	a, #0x30
	inc	e
	dec	e
	ld	c, #0x02
	jr	NZ, 00108$
	ld	c, #0x00
00108$:
	add	a, c
	ldhl	sp,	#2
	add	a, (hl)
00103$:
;src/main.c:323: }
	pop	hl
	inc	sp
	jp	(hl)
;src/main.c:325: static void draw_traffic_sprites(void) {
;	---------------------------------
; Function draw_traffic_sprites
; ---------------------------------
_draw_traffic_sprites:
;src/main.c:326: uint8_t phase = (uint8_t)((frame_clock >> 3u) & 1u);
	ld	hl, #_frame_clock
	ld	a, (hl)
	rrca
	rrca
	rrca
	and	a, #0x01
	ld	b, a
;src/main.c:327: uint8_t travel = (uint8_t)(frame_clock >> 1u);
	ld	c, (hl)
	srl	c
;/tmp/gbdk-4.5.0/include/gb/gb.h:1973: OAM_item_t * itm = &shadow_OAM[nb];
	ld	hl, #(_shadow_OAM + 96)
;/tmp/gbdk-4.5.0/include/gb/gb.h:1974: itm->y=y, itm->x=x;
	xor	a, a
	ld	(hl+), a
	ld	(hl), a
;/tmp/gbdk-4.5.0/include/gb/gb.h:1973: OAM_item_t * itm = &shadow_OAM[nb];
	ld	hl, #(_shadow_OAM + 100)
;/tmp/gbdk-4.5.0/include/gb/gb.h:1974: itm->y=y, itm->x=x;
	xor	a, a
	ld	(hl+), a
	ld	(hl), a
;src/main.c:336: if (room_id == ROOM_DISTRICT) {
	ld	a, (#_room_id)
	or	a, a
	jr	NZ, 00108$
;src/main.c:338: x = (uint8_t)(((uint16_t)travel + 20u) % 184u);
	ld	a, c
	add	a, #0x14
	push	bc
	ld	e, #0xb8
	call	__moduchar
	ld	h, c
	pop	bc
;src/main.c:340: if ((x >= 8u) && (x <= 168u)) {
	ld	a, h
	sub	a, #0x08
	jr	C, 00102$
;src/main.c:341: set_single_sprite(oam0, traffic_tile(TRAFFIC_TYPE_MOTOR, 0u, phase), x, y, 0u);
	push	hl
	push	bc
	push	bc
	inc	sp
	ld	e, #0x00
	ld	a, #0x01
	call	_traffic_tile
	ld	e, a
	pop	bc
	pop	hl
	push	bc
	ld	bc, #0x6c
	push	bc
	push	hl
	inc	sp
	ld	a, #0x18
	call	_set_single_sprite
	pop	bc
00102$:
;src/main.c:344: x = (uint8_t)(176u - (((uint16_t)travel + 76u) % 184u));
	ld	a, c
	add	a, #0x4c
	push	bc
	ld	e, #0xb8
	call	__moduchar
	pop	af
	ld	b, a
	ld	a, #0xb0
	sub	a, c
;src/main.c:346: if ((x >= 8u) && (x <= 168u)) {
	ld	d, a
	sub	a, #0x08
	ret	C
	ld	a, #0xa8
	sub	a, d
	ret	C
;src/main.c:347: set_single_sprite(oam1, traffic_tile(TRAFFIC_TYPE_MOTOR, 0u, phase), x, y, S_FLIPX);
	push	de
	push	bc
	inc	sp
	ld	e, #0x00
	ld	a, #0x01
	call	_traffic_tile
	pop	de
	ld	h, #0x20
	push	hl
	inc	sp
	ld	h, #0x74
	ld	l, d
	push	hl
	ld	e, a
	ld	a, #0x19
	call	_set_single_sprite
;src/main.c:349: return;
	ret
00108$:
;src/main.c:352: if (room_id == ROOM_SOUTHLINE) {
	ld	a, (#_room_id)
	sub	a, #0x06
	ret	NZ
;src/main.c:353: y = (uint8_t)(((uint16_t)travel + 28u) % 176u);
	ld	a, c
	add	a, #0x1c
	push	bc
	ld	e, #0xb0
	call	__moduchar
	ld	h, c
	pop	bc
;src/main.c:355: if ((y >= 16u) && (y <= 152u)) {
	ld	a, h
	sub	a, #0x10
	ret	C
	ld	a, #0x98
	sub	a, h
	ret	C
;src/main.c:356: set_single_sprite(oam0, traffic_tile(TRAFFIC_TYPE_MOTOR, 1u, phase), x, y, 0u);
	push	hl
	push	bc
	inc	sp
	ld	a,#0x01
	ld	e,a
	call	_traffic_tile
	ld	e, a
	pop	hl
	xor	a, a
	push	af
	inc	sp
	ld	l, #0x58
	push	hl
	ld	a, #0x18
	call	_set_single_sprite
;src/main.c:359: }
	ret
;src/main.c:361: static void draw_world_sprites(uint8_t player_sx, uint8_t player_sy, uint8_t walk_phase) {
;	---------------------------------
; Function draw_world_sprites
; ---------------------------------
_draw_world_sprites:
	add	sp, #-3
	ld	d, a
;src/main.c:365: uint8_t sprite_base = current_sprite_tile_base();
	push	de
	call	_current_sprite_tile_base
	ld	c, a
	pop	de
;src/main.c:372: (facing == DIR_LEFT)
	ld	a, (#_facing)
	sub	a, #0x02
	ld	a, #0x01
	jr	Z, 00132$
	xor	a, a
00132$:
	ldhl	sp,	#2
	ld	(hl), a
;src/main.c:369: (uint8_t)(sprite_base + player_frame_base(walk_phase)),
	push	bc
	push	de
	ldhl	sp,	#9
	ld	a, (hl)
	call	_player_frame_base
	pop	de
	pop	bc
	add	a, c
;src/main.c:368: PLAYER_OAM_BASE,
	push	bc
	ldhl	sp,	#4
	ld	h, (hl)
	push	hl
	inc	sp
	ld	h, e
	ld	l, d
	push	hl
	ld	e, a
	xor	a, a
	call	_set_quad_sprite
	pop	bc
;src/main.c:375: for (i = 0u; i != NPC_COUNT; ++i) {
	ld	a, c
	add	a, #0x18
	ldhl	sp,	#0
	ld	(hl), a
	ld	c, #0x00
00105$:
;src/main.c:376: npc_oam = (uint8_t)(NPC_OAM_BASE + i * SPRITES_PER_ACTOR);
	ld	a, c
	add	a, a
	add	a, a
	ldhl	sp,	#1
	ld	(hl+), a
	add	a, #0x04
	ld	(hl), a
;src/main.c:377: if (npcs[i].room_id == room_id) {
	ld	b, #0x00
	ld	l, c
	ld	h, b
	add	hl, hl
	add	hl, bc
	add	hl, hl
	ld	a, #<(_npcs)
	add	a, l
	ld	e, a
	ld	a, #>(_npcs)
	adc	a, h
	ld	d, a
	ld	a, (de)
	ld	hl, #_room_id
	sub	a, (hl)
	jr	NZ, 00102$
;src/main.c:378: bob = (uint8_t)(((frame_clock >> 4u) + i) & 1u);
	ld	a, (#_frame_clock)
	swap	a
	and	a, #0x0f
	add	a, c
	and	a, #0x01
	ld	b, a
;src/main.c:383: (uint8_t)(tile_to_sprite_y(npcs[i].y) - bob),
	ld	l, e
	ld	h, d
	inc	hl
	inc	hl
	ld	a, (hl)
	push	bc
	push	de
	call	_tile_to_sprite_y
	pop	de
	pop	bc
	sub	a, b
	ld	b, a
;src/main.c:382: tile_to_sprite_x(npcs[i].x),
	inc	de
	ld	a, (de)
	push	bc
	call	_tile_to_sprite_x
	ld	d, a
	pop	bc
;src/main.c:381: (uint8_t)(sprite_base + NPC_SPRITE_TILE_OFFSET + i * 4u),
	ldhl	sp,	#0
	ld	a, (hl+)
	add	a, (hl)
;src/main.c:380: npc_oam,
	inc	hl
	ld	e, a
	push	bc
	xor	a, a
	push	af
	inc	sp
	ld	c, d
	push	bc
	ld	a, (hl)
	call	_set_quad_sprite
	pop	bc
	jr	00106$
00102$:
;src/main.c:387: hide_actor(npc_oam);
	push	bc
	ldhl	sp,	#4
	ld	a, (hl)
	call	_hide_actor
	pop	bc
00106$:
;src/main.c:375: for (i = 0u; i != NPC_COUNT; ++i) {
	inc	c
	ld	a, c
	sub	a, #0x05
	jr	NZ, 00105$
;src/main.c:391: draw_traffic_sprites();
	call	_draw_traffic_sprites
;src/main.c:392: }
	add	sp, #3
	pop	hl
	inc	sp
	jp	(hl)
;src/main.c:394: static void stamp_apartment_decor(void) {
;	---------------------------------
; Function stamp_apartment_decor
; ---------------------------------
_stamp_apartment_decor:
;src/main.c:399: for (i = 0u; i != 3u; ++i) {
	ld	c, #0x00
00102$:
;src/main.c:400: map_buffer[(uint16_t)slot_y[i] * MAP_W + slot_x[i]] = decor_tile(decor_slots[i]);
	ld	hl, #_stamp_apartment_decor_slot_y_10000_401
	ld	b, #0x00
	add	hl, bc
	ld	l, (hl)
	ld	h, #0x00
	ld	e, l
	ld	d, h
	add	hl, hl
	add	hl, hl
	add	hl, de
	add	hl, hl
	add	hl, hl
	ld	a, #<(_stamp_apartment_decor_slot_x_10000_401)
	add	a, c
	ld	e, a
	ld	a, #>(_stamp_apartment_decor_slot_x_10000_401)
	adc	a, #0x00
	ld	d, a
	ld	a, (de)
	ld	d, #0x00
	ld	e, a
	add	hl, de
	ld	de, #_map_buffer
	add	hl, de
	ld	a, #<(_decor_slots)
	add	a, c
	ld	e, a
	ld	a, #>(_decor_slots)
	adc	a, #0x00
	ld	d, a
	ld	a, (de)
	push	hl
	push	bc
	call	_decor_tile
	pop	bc
	pop	hl
	ld	(hl), a
;src/main.c:399: for (i = 0u; i != 3u; ++i) {
	inc	c
	ld	a, c
	sub	a, #0x03
	jr	NZ, 00102$
;src/main.c:402: }
	ret
_stamp_apartment_decor_slot_x_10000_401:
	.db #0x05	; 5
	.db #0x0a	; 10
	.db #0x0f	; 15
_stamp_apartment_decor_slot_y_10000_401:
	.db #0x05	; 5
	.db #0x05	; 5
	.db #0x05	; 5
;src/main.c:404: static void load_room(void) {
;	---------------------------------
; Function load_room
; ---------------------------------
_load_room:
	add	sp, #-5
;src/main.c:408: for (y = 0u; y != MAP_H; ++y) {
	ldhl	sp,	#3
	ld	(hl), #0x00
;src/main.c:409: for (x = 0u; x != MAP_W; ++x) {
00111$:
	ldhl	sp,	#3
	ld	a, (hl)
	add	a, a
	ldhl	sp,	#0
	ld	(hl), a
	ldhl	sp,	#4
	ld	(hl), #0x00
00105$:
;src/main.c:410: map_buffer[(uint16_t)y * MAP_W + x] = tile_for_char(room_maps[room_id][y][x]);
	ldhl	sp,	#3
	ld	c, (hl)
	ld	b, #0x00
	ld	l, c
	ld	h, b
	add	hl, hl
	add	hl, hl
	add	hl, bc
	add	hl, hl
	add	hl, hl
	ld	c, l
	ld	b, h
	ldhl	sp,	#4
	ld	l, (hl)
	ld	h, #0x00
	add	hl, bc
	ld	bc, #_map_buffer
	add	hl, bc
	push	hl
	ld	a, l
	ldhl	sp,	#3
	ld	(hl), a
	pop	hl
	ld	a, h
	ldhl	sp,	#2
	ld	(hl), a
	ld	hl, #_room_id
	ld	c, (hl)
	ld	b, #0x00
	ld	l, c
	ld	h, b
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, bc
	add	hl, hl
	add	hl, hl
	ld	bc, #_room_maps
	add	hl, bc
	ld	c, l
	ld	b, h
	ldhl	sp,	#0
	ld	l, (hl)
	ld	h, #0x00
	add	hl, bc
	ld	a, (hl+)
	ld	c, a
	ld	b, (hl)
	ldhl	sp,	#4
	ld	l, (hl)
	ld	h, #0x00
	add	hl, bc
	ld	c, l
	ld	b, h
	ld	a, (bc)
	call	_tile_for_char
	ldhl	sp,	#1
	ld	e, (hl)
	inc	hl
	ld	h, (hl)
	ld	l, e
	ld	(hl), a
;src/main.c:409: for (x = 0u; x != MAP_W; ++x) {
	ldhl	sp,	#4
	inc	(hl)
	ld	a, (hl)
	sub	a, #0x14
	jr	NZ, 00105$
;src/main.c:408: for (y = 0u; y != MAP_H; ++y) {
	ldhl	sp,	#3
	inc	(hl)
	ld	a, (hl)
	sub	a, #0x12
	jr	NZ, 00111$
;src/main.c:414: if (room_id == ROOM_APARTMENT) {
	ld	a, (#_room_id)
	dec	a
	jr	NZ, 00104$
;src/main.c:415: stamp_apartment_decor();
	call	_stamp_apartment_decor
00104$:
;src/main.c:418: sanitize_player_position();
	call	_sanitize_player_position
;src/main.c:419: }
	add	sp, #5
	ret
;src/main.c:421: static uint8_t apartment_overlay_tile(uint8_t decor_id) {
;	---------------------------------
; Function apartment_overlay_tile
; ---------------------------------
_apartment_overlay_tile:
	ld	c, a
;src/main.c:422: switch (decor_id) {
	ld	a, #0x05
	sub	a, c
	jr	C, 00106$
	ld	b, #0x00
	ld	hl, #00117$
	add	hl, bc
	add	hl, bc
	ld	c, (hl)
	inc	hl
	ld	h, (hl)
	ld	l, c
	jp	(hl)
00117$:
	.dw	00106$
	.dw	00101$
	.dw	00102$
	.dw	00103$
	.dw	00104$
	.dw	00105$
;src/main.c:423: case DECOR_POSTER: return (uint8_t)(luma_lane_apartment_TILE_COUNT + 0u);
00101$:
	ld	a, #0xd8
	ret
;src/main.c:424: case DECOR_LAMP: return (uint8_t)(luma_lane_apartment_TILE_COUNT + 1u);
00102$:
	ld	a, #0xd9
	ret
;src/main.c:425: case DECOR_SHELF: return (uint8_t)(luma_lane_apartment_TILE_COUNT + 2u);
00103$:
	ld	a, #0xda
	ret
;src/main.c:426: case DECOR_TANK: return (uint8_t)(luma_lane_apartment_TILE_COUNT + 3u);
00104$:
	ld	a, #0xdb
	ret
;src/main.c:427: case DECOR_DOCK: return (uint8_t)(luma_lane_apartment_TILE_COUNT + 4u);
00105$:
	ld	a, #0xdc
	ret
;src/main.c:428: default: return 0u;
00106$:
	xor	a, a
;src/main.c:429: }
;src/main.c:430: }
	ret
;src/main.c:432: static void overlay_apartment_decor(void) {
;	---------------------------------
; Function overlay_apartment_decor
; ---------------------------------
_overlay_apartment_decor:
	dec	sp
;src/main.c:438: set_bkg_data(luma_lane_apartment_TILE_COUNT, APARTMENT_DECOR_TILE_COUNT, apartment_decor_tiles);
	ld	de, #_apartment_decor_tiles
	push	de
	ld	hl, #0x5d8
	push	hl
	call	_set_bkg_data
	add	sp, #4
;src/main.c:439: for (i = 0u; i != 3u; ++i) {
	ld	c, #0x00
00104$:
;src/main.c:440: if (decor_slots[i] != DECOR_NONE) {
	ld	hl, #_decor_slots
	ld	b, #0x00
	add	hl, bc
	ld	a, (hl)
	or	a, a
	jr	Z, 00105$
;src/main.c:441: tile = apartment_overlay_tile(decor_slots[i]);
	push	bc
	call	_apartment_overlay_tile
	pop	bc
	ldhl	sp,	#0
	ld	(hl), a
;src/main.c:442: set_bkg_tiles(slot_x[i], slot_y[i], 1u, 1u, &tile);
	ld	hl, #_overlay_apartment_decor_slot_y_10000_417
	ld	b, #0x00
	add	hl, bc
	ld	d, (hl)
	ld	hl, #_overlay_apartment_decor_slot_x_10000_417
	ld	b, #0x00
	add	hl, bc
	ld	a, (hl)
	ld	hl, #0
	add	hl, sp
	push	hl
	ld	h, #0x01
	push	hl
	inc	sp
	ld	h, #0x01
	ld	l, d
	push	hl
	push	af
	inc	sp
	call	_set_bkg_tiles
	add	sp, #6
00105$:
;src/main.c:439: for (i = 0u; i != 3u; ++i) {
	inc	c
	ld	a, c
	sub	a, #0x03
	jr	NZ, 00104$
;src/main.c:445: }
	inc	sp
	ret
_overlay_apartment_decor_slot_x_10000_417:
	.db #0x05	; 5
	.db #0x0a	; 10
	.db #0x0f	; 15
_overlay_apartment_decor_slot_y_10000_417:
	.db #0x05	; 5
	.db #0x05	; 5
	.db #0x05	; 5
;src/main.c:447: static const palette_color_t *current_room_palettes(void) {
;	---------------------------------
; Function current_room_palettes
; ---------------------------------
_current_room_palettes:
;src/main.c:448: switch (room_id) {
	ld	a, #0x06
	ld	hl, #_room_id
	sub	a, (hl)
	jr	C, 00108$
	ld	c, (hl)
	ld	b, #0x00
	ld	hl, #00119$
	add	hl, bc
	add	hl, bc
	ld	c, (hl)
	inc	hl
	ld	h, (hl)
	ld	l, c
	jp	(hl)
00119$:
	.dw	00101$
	.dw	00102$
	.dw	00103$
	.dw	00104$
	.dw	00105$
	.dw	00106$
	.dw	00107$
;src/main.c:449: case ROOM_DISTRICT: return luma_lane_district_v2_palettes;
00101$:
	ld	bc, #_luma_lane_district_v2_palettes
	ret
;src/main.c:450: case ROOM_APARTMENT: return luma_lane_apartment_palettes;
00102$:
	ld	bc, #_luma_lane_apartment_palettes
	ret
;src/main.c:451: case ROOM_RECORDS: return needle_and_neon_records_palettes;
00103$:
	ld	bc, #_needle_and_neon_records_palettes
	ret
;src/main.c:452: case ROOM_TECH: return rook_repair_tech_palettes;
00104$:
	ld	bc, #_rook_repair_tech_palettes
	ret
;src/main.c:453: case ROOM_RAMEN: return soma_ramen_palettes;
00105$:
	ld	bc, #_soma_ramen_palettes
	ret
;src/main.c:454: case ROOM_ROOFTOP: return iona_rooftop_palettes;
00106$:
	ld	bc, #_iona_rooftop_palettes
	ret
;src/main.c:455: case ROOM_SOUTHLINE: return southline_overpass_palettes;
00107$:
	ld	bc, #_southline_overpass_palettes
	ret
;src/main.c:456: default: return 0;
00108$:
	ld	bc, #0x0000
;src/main.c:457: }
;src/main.c:458: }
	ret
;src/main.c:460: static void render_banked_room(void) {
;	---------------------------------
; Function render_banked_room
; ---------------------------------
_render_banked_room:
;src/main.c:461: switch (room_id) {
	ld	a, #0x06
	ld	hl, #_room_id
	sub	a, (hl)
	jp	C, 00108$
	ld	c, (hl)
	ld	b, #0x00
	ld	hl, #00119$
	add	hl, bc
	add	hl, bc
	ld	c, (hl)
	inc	hl
	ld	h, (hl)
	ld	l, c
	jp	(hl)
00119$:
	.dw	00101$
	.dw	00102$
	.dw	00103$
	.dw	00104$
	.dw	00105$
	.dw	00106$
	.dw	00107$
;src/main.c:462: case ROOM_DISTRICT:
00101$:
;src/main.c:468: BANK(luma_lane_district_v2)
	ld	b, #<(___bank_luma_lane_district_v2)
;src/main.c:467: luma_lane_district_v2_palettes,
;src/main.c:466: luma_lane_district_v2_map,
;src/main.c:464: luma_lane_district_v2_tiles,
	push	bc
	inc	sp
	ld	de, #_luma_lane_district_v2_palettes
	push	de
	ld	de, #_luma_lane_district_v2_map
	push	de
	ld	a, #0xa2
	ld	de, #_luma_lane_district_v2_tiles
	call	_show_banked_image_card
;src/main.c:470: break;
	ret
;src/main.c:471: case ROOM_APARTMENT:
00102$:
;src/main.c:477: BANK(luma_lane_apartment)
	ld	b, #<(___bank_luma_lane_apartment)
;src/main.c:476: luma_lane_apartment_palettes,
;src/main.c:475: luma_lane_apartment_map,
;src/main.c:473: luma_lane_apartment_tiles,
	ld	de, #_luma_lane_apartment_tiles+0
	push	bc
	inc	sp
	ld	bc, #_luma_lane_apartment_palettes
	push	bc
	ld	bc, #_luma_lane_apartment_map
	push	bc
	ld	a, #0xd8
	call	_show_banked_image_card
;src/main.c:479: overlay_apartment_decor();
;src/main.c:480: break;
	jp	_overlay_apartment_decor
;src/main.c:481: case ROOM_RECORDS:
00103$:
;src/main.c:487: BANK(needle_and_neon_records)
	ld	b, #<(___bank_needle_and_neon_records)
;src/main.c:486: needle_and_neon_records_palettes,
;src/main.c:485: needle_and_neon_records_map,
;src/main.c:483: needle_and_neon_records_tiles,
	push	bc
	inc	sp
	ld	de, #_needle_and_neon_records_palettes
	push	de
	ld	de, #_needle_and_neon_records_map
	push	de
	ld	a, #0x55
	ld	de, #_needle_and_neon_records_tiles
	call	_show_banked_image_card
;src/main.c:489: break;
	ret
;src/main.c:490: case ROOM_TECH:
00104$:
;src/main.c:496: BANK(rook_repair_tech)
	ld	b, #<(___bank_rook_repair_tech)
;src/main.c:495: rook_repair_tech_palettes,
;src/main.c:494: rook_repair_tech_map,
;src/main.c:492: rook_repair_tech_tiles,
	push	bc
	inc	sp
	ld	de, #_rook_repair_tech_palettes
	push	de
	ld	de, #_rook_repair_tech_map
	push	de
	ld	a, #0x53
	ld	de, #_rook_repair_tech_tiles
	call	_show_banked_image_card
;src/main.c:498: break;
	ret
;src/main.c:499: case ROOM_RAMEN:
00105$:
;src/main.c:505: BANK(soma_ramen)
	ld	b, #<(___bank_soma_ramen)
;src/main.c:504: soma_ramen_palettes,
;src/main.c:503: soma_ramen_map,
;src/main.c:501: soma_ramen_tiles,
	push	bc
	inc	sp
	ld	de, #_soma_ramen_palettes
	push	de
	ld	de, #_soma_ramen_map
	push	de
	ld	a, #0x4f
	ld	de, #_soma_ramen_tiles
	call	_show_banked_image_card
;src/main.c:507: break;
	ret
;src/main.c:508: case ROOM_ROOFTOP:
00106$:
;src/main.c:514: BANK(iona_rooftop)
	ld	b, #<(___bank_iona_rooftop)
;src/main.c:513: iona_rooftop_palettes,
;src/main.c:512: iona_rooftop_map,
;src/main.c:510: iona_rooftop_tiles,
	push	bc
	inc	sp
	ld	de, #_iona_rooftop_palettes
	push	de
	ld	de, #_iona_rooftop_map
	push	de
	ld	a, #0x44
	ld	de, #_iona_rooftop_tiles
	call	_show_banked_image_card
;src/main.c:516: break;
	ret
;src/main.c:517: case ROOM_SOUTHLINE:
00107$:
;src/main.c:523: BANK(southline_overpass)
	ld	b, #<(___bank_southline_overpass)
;src/main.c:522: southline_overpass_palettes,
;src/main.c:521: southline_overpass_map,
;src/main.c:519: southline_overpass_tiles,
	push	bc
	inc	sp
	ld	de, #_southline_overpass_palettes
	push	de
	ld	de, #_southline_overpass_map
	push	de
	ld	a, #0x61
	ld	de, #_southline_overpass_tiles
	call	_show_banked_image_card
;src/main.c:525: break;
	ret
;src/main.c:526: default:
00108$:
;src/main.c:527: set_bkg_data(0u, BG_TILE_COUNT, bg_tiles);
	ld	de, #_bg_tiles
	push	de
	ld	hl, #0x2000
	push	hl
	call	_set_bkg_data
	add	sp, #4
;src/main.c:528: set_bkg_tiles(0u, 0u, MAP_W, MAP_H, map_buffer);
	ld	de, #_map_buffer
	push	de
	ld	hl, #0x1214
	push	hl
	xor	a, a
	rrca
	push	af
	call	_set_bkg_tiles
	add	sp, #6
;src/main.c:530: }
;src/main.c:531: }
	ret
;src/main.c:533: static void render_room(void) {
;	---------------------------------
; Function render_room
; ---------------------------------
_render_room:
;src/main.c:536: begin_display_reload();
	call	_begin_display_reload
;src/main.c:537: render_banked_room();
	call	_render_banked_room
;src/main.c:538: if (_cpu == CGB_TYPE) {
	ld	a, (#__cpu)
	sub	a, #0x11
	jr	NZ, 00108$
;src/main.c:539: palettes = current_room_palettes();
	call	_current_room_palettes
;src/main.c:540: if (palettes != 0) {
	ld	a, b
	or	a, c
	jr	Z, 00108$
;src/main.c:541: if (room_id == ROOM_DISTRICT || room_id == ROOM_SOUTHLINE) {
	ld	hl, #_room_id
	ld	a, (hl)
	or	a, a
	jr	Z, 00101$
	ld	a, (hl)
	sub	a, #0x06
	jr	NZ, 00102$
00101$:
;src/main.c:542: set_inverted_bkg_palette(palettes);
	ld	e, c
	ld	d, b
	call	_set_inverted_bkg_palette
	jr	00108$
00102$:
;src/main.c:544: set_bkg_palette(0u, 1u, palettes);
	push	bc
	xor	a, a
	inc	a
	push	af
	call	_set_bkg_palette
	add	sp, #4
00108$:
;src/main.c:548: set_sprite_data(current_sprite_tile_base(), current_sprite_tile_count(), sprite_tiles);
	call	_current_sprite_tile_count
	ld	b, a
	push	bc
	call	_current_sprite_tile_base
	pop	bc
	ld	de, #_sprite_tiles
	push	de
	push	bc
	inc	sp
	push	af
	inc	sp
	call	_set_sprite_data
	add	sp, #4
;src/main.c:549: apply_palette();
	call	_apply_palette
;src/main.c:550: draw_world_sprites(tile_to_sprite_x(player_x), tile_to_sprite_y(player_y), 0u);
	ld	a, (_player_y)
	call	_tile_to_sprite_y
	ld	e, a
	push	de
	ld	a, (_player_x)
	call	_tile_to_sprite_x
	pop	de
	ld	h, #0x00
	push	hl
	inc	sp
	call	_draw_world_sprites
;src/main.c:551: dt_audio_sync(room_id);
	ld	a, (_room_id)
	push	af
	inc	sp
	ld	e, #b_dt_audio_sync
	ld	hl, #_dt_audio_sync
	call	___sdcc_bcall_ehl
	inc	sp
;src/main.c:552: finish_display_reload(1u);
	ld	a, #0x01
;src/main.c:553: }
	jp	_finish_display_reload
;src/main.c:555: static void use_text_screen(void) {
;	---------------------------------
; Function use_text_screen
; ---------------------------------
_use_text_screen:
;src/main.c:556: begin_display_reload();
	call	_begin_display_reload
;src/main.c:557: font_init();
	call	_font_init
;src/main.c:558: ui_font = font_load(font_ibm);
	ld	de, #_font_ibm
	push	de
	call	_font_load
	pop	hl
	ld	hl, #_ui_font
	ld	a, e
	ld	(hl+), a
	ld	(hl), d
;src/main.c:559: apply_palette();
	call	_apply_palette
;src/main.c:560: font_set(ui_font);
	ld	a, (_ui_font)
	ld	e, a
	ld	hl, #_ui_font + 1
	ld	d, (hl)
	push	de
	call	_font_set
	pop	hl
;src/main.c:561: cls();
	call	_cls
;src/main.c:562: gotoxy(0u, 0u);
	xor	a, a
	rrca
	push	af
	call	_gotoxy
	pop	hl
;src/main.c:563: finish_display_reload(0u);
	xor	a, a
;src/main.c:564: }
	jp	_finish_display_reload
;src/main.c:566: static void draw_text(const char *text) {
;	---------------------------------
; Function draw_text
; ---------------------------------
_draw_text:
;src/main.c:567: uint8_t x = posx();
	push	de
	call	_posx
	ld	b, e
	pop	de
;src/main.c:568: uint8_t y = posy();
	push	bc
	push	de
	call	_posy
	ld	c, e
	pop	de
	pop	af
	ld	b, a
;src/main.c:571: while ((c = *text++) != '\0') {
00106$:
	ld	a, (de)
	inc	de
	ld	l, a
	or	a, a
	jr	Z, 00108$
;src/main.c:574: ++y;
	ld	h, c
	inc	h
;src/main.c:572: if (c == '\n') {
	ld	a, l
;src/main.c:573: x = 0u;
	sub	a, #0x0a
	jr	NZ, 00104$
	ld	b, a
;src/main.c:574: ++y;
	ld	c, h
	jr	00106$
00104$:
;src/main.c:576: gotoxy(x, y);
	push	hl
	push	bc
	push	de
	ld	a, c
	push	af
	inc	sp
	push	bc
	inc	sp
	call	_gotoxy
	pop	hl
	pop	de
	pop	bc
	pop	hl
;src/main.c:577: setchar(c);
	push	hl
	push	bc
	push	de
	ld	a, l
	push	af
	inc	sp
	call	_setchar
	inc	sp
	pop	de
	pop	bc
	pop	hl
;src/main.c:578: ++x;
	inc	b
;src/main.c:579: if (x >= MAP_W) {
	ld	a, b
	sub	a, #0x14
	jr	C, 00106$
;src/main.c:580: x = 0u;
	ld	b, #0x00
;src/main.c:581: ++y;
	ld	c, h
	jr	00106$
00108$:
;src/main.c:585: gotoxy(x, y);
	ld	a, c
	push	af
	inc	sp
	push	bc
	inc	sp
	call	_gotoxy
	pop	hl
;src/main.c:586: }
	ret
;src/main.c:588: static void draw_text_xy(uint8_t x, uint8_t y, const char *text) {
;	---------------------------------
; Function draw_text_xy
; ---------------------------------
_draw_text_xy:
	ld	b, e
;src/main.c:589: gotoxy(x, y);
	push	bc
	inc	sp
	push	af
	inc	sp
	call	_gotoxy
	pop	hl
;src/main.c:590: draw_text(text);
	ldhl	sp,	#2
	ld	a, (hl+)
	ld	e, a
	ld	d, (hl)
	call	_draw_text
;src/main.c:591: }
	pop	hl
	pop	af
	jp	(hl)
;src/main.c:593: static void draw_u8(uint8_t value) {
;	---------------------------------
; Function draw_u8
; ---------------------------------
_draw_u8:
	dec	sp
;src/main.c:594: uint8_t started = 0u;
	ld	c, #0x00
;src/main.c:597: if (value >= 100u) {
	cp	a, #0x64
	jr	C, 00102$
;src/main.c:598: digit = value / 100u;
	ldhl	sp,	#0
	ld	(hl), a
	ld	e, #0x64
	call	__divuchar
	ld	a, c
;src/main.c:599: setchar((char)('0' + digit));
	add	a, #0x30
	push	af
	inc	sp
	call	_setchar
	inc	sp
;src/main.c:600: gotoxy((uint8_t)(posx() + 1u), posy());
	call	_posy
	ld	b, e
	push	bc
	call	_posx
	ld	a, e
	pop	bc
	inc	a
	push	bc
	inc	sp
	push	af
	inc	sp
	call	_gotoxy
	pop	hl
;src/main.c:601: value %= 100u;
	ld	e, #0x64
	ldhl	sp,	#0
	ld	a, (hl)
	call	__moduchar
	ld	a, c
;src/main.c:602: started = 1u;
	ld	c, #0x01
00102$:
;src/main.c:604: if (started || value >= 10u) {
	inc	c
	dec	c
	jr	NZ, 00103$
	cp	a, #0x0a
	jr	C, 00104$
00103$:
;src/main.c:605: digit = value / 10u;
	ldhl	sp,	#0
	ld	(hl), a
	ld	e, #0x0a
	call	__divuchar
	ld	a, c
;src/main.c:606: setchar((char)('0' + digit));
	add	a, #0x30
	push	af
	inc	sp
	call	_setchar
	inc	sp
;src/main.c:607: gotoxy((uint8_t)(posx() + 1u), posy());
	call	_posy
	ld	b, e
	push	bc
	call	_posx
	ld	a, e
	pop	bc
	inc	a
	push	bc
	inc	sp
	push	af
	inc	sp
	call	_gotoxy
	pop	hl
;src/main.c:608: value %= 10u;
	ld	e, #0x0a
	ldhl	sp,	#0
	ld	a, (hl)
	call	__moduchar
	ld	a, c
00104$:
;src/main.c:610: setchar((char)('0' + value));
	add	a, #0x30
	push	af
	inc	sp
	call	_setchar
	inc	sp
;src/main.c:611: gotoxy((uint8_t)(posx() + 1u), posy());
	call	_posy
	ld	b, e
	push	bc
	call	_posx
	ld	a, e
	pop	bc
	inc	a
	push	bc
	inc	sp
	push	af
	inc	sp
	call	_gotoxy
	pop	hl
;src/main.c:612: }
	inc	sp
	ret
;src/main.c:614: static void show_banked_image_card(const uint8_t *tiles, uint8_t tile_count, const unsigned char *map, const palette_color_t *palettes, uint8_t bank) {
;	---------------------------------
; Function show_banked_image_card
; ---------------------------------
_show_banked_image_card:
	dec	sp
	ldhl	sp,	#0
	ld	(hl), a
;src/main.c:615: uint8_t old_bank = CURRENT_BANK;
	ldh	a, (__current_bank + 0)
	ld	c, a
;src/main.c:617: if (bank != old_bank) {
	ldhl	sp,	#7
	ld	a, (hl)
	sub	a, c
	ld	a, #0x01
	jr	Z, 00132$
	xor	a, a
00132$:
	ld	b, a
	bit	0, b
	jr	NZ, 00102$
;src/main.c:618: SWITCH_ROM(bank);
	ldhl	sp,	#7
	ld	a, (hl)
	ldh	(__current_bank + 0), a
	ld	(#_rROMB0),a
00102$:
;src/main.c:620: set_bkg_data(0u, tile_count, tiles);
	push	de
	ldhl	sp,	#2
	ld	h, (hl)
	ld	l, #0x00
	push	hl
	call	_set_bkg_data
	add	sp, #4
;src/main.c:621: set_bkg_tiles(0u, 0u, MAP_W, MAP_H, map);
	ldhl	sp,	#3
	ld	a, (hl+)
	ld	e, a
	ld	d, (hl)
	push	de
	ld	hl, #0x1214
	push	hl
	xor	a, a
	rrca
	push	af
	call	_set_bkg_tiles
	add	sp, #6
;src/main.c:622: if (_cpu == CGB_TYPE) {
	ld	a, (#__cpu)
	sub	a, #0x11
	jr	NZ, 00104$
;src/main.c:623: set_bkg_palette(0u, 1u, palettes);
	push	bc
	ldhl	sp,	#7
	ld	a, (hl+)
	ld	e, a
	ld	d, (hl)
	push	de
	xor	a, a
	inc	a
	push	af
	call	_set_bkg_palette
	add	sp, #4
	pop	bc
	jr	00105$
00104$:
;src/main.c:625: BGP_REG = 0xE4u;
	ld	a, #0xe4
	ldh	(_BGP_REG + 0), a
00105$:
;src/main.c:627: if (bank != old_bank) {
	bit	0, b
	jr	NZ, 00108$
;src/main.c:628: SWITCH_ROM(old_bank);
	ld	a, c
	ldh	(__current_bank + 0), a
	ld	hl, #_rROMB0
	ld	(hl), c
00108$:
;src/main.c:630: }
	inc	sp
	pop	hl
	add	sp, #5
	jp	(hl)
;src/main.c:632: static uint8_t wait_card_input(uint8_t accept_start_only) {
;	---------------------------------
; Function wait_card_input
; ---------------------------------
_wait_card_input:
	ld	c, a
;src/main.c:633: uint8_t keys = joypad();
	call	_joypad
;src/main.c:636: return (keys & J_START) != 0u;
	ld	b, a
	and	a, #0x80
;src/main.c:635: if (accept_start_only) {
	inc	c
	dec	c
	jr	Z, 00102$
;src/main.c:636: return (keys & J_START) != 0u;
	sub	a, #0x01
	ld	a, #0x00
	rla
	xor	a, #0x01
	ret
00102$:
;src/main.c:638: return ((keys & J_START) != 0u) || ((keys & J_A) != 0u) || ((keys & J_B) != 0u);
	or	a, a
	jr	NZ, 00106$
	bit	4, b
	jr	NZ, 00106$
	bit	5, b
	jr	NZ, 00106$
	xor	a, a
	ret
00106$:
	ld	a, #0x01
;src/main.c:639: }
	ret
;src/main.c:641: static void show_company_card(void) {
;	---------------------------------
; Function show_company_card
; ---------------------------------
_show_company_card:
;src/main.c:644: begin_display_reload();
	call	_begin_display_reload
;src/main.c:650: BANK(startup_scott_stinks_a)
	ld	b, #<(___bank_startup_scott_stinks_a)
;src/main.c:649: startup_scott_stinks_a_palettes,
;src/main.c:648: startup_scott_stinks_a_map,
;src/main.c:646: startup_scott_stinks_a_tiles,
	push	bc
	inc	sp
	ld	de, #_startup_scott_stinks_a_palettes
	push	de
	ld	de, #_startup_scott_stinks_a_map
	push	de
	ld	a, #0x40
	ld	de, #_startup_scott_stinks_a_tiles
	call	_show_banked_image_card
;src/main.c:652: finish_display_reload(0u);
	xor	a, a
	call	_finish_display_reload
;src/main.c:653: dt_audio_play_sfx(SFX_FART);
	ld	a, #0x05
	push	af
	inc	sp
	ld	e, #b_dt_audio_play_sfx
	ld	hl, #_dt_audio_play_sfx
	call	___sdcc_bcall_ehl
	inc	sp
;src/main.c:655: for (frame = 0u; frame != 84u; ++frame) {
	ld	c, #0x00
00105$:
;src/main.c:656: if (_cpu != CGB_TYPE) {
	ld	a, (#__cpu)
	sub	a, #0x11
	jr	Z, 00102$
;src/main.c:657: BGP_REG = ((frame & 0x08u) == 0u) ? 0xE4u : 0xD2u;
	bit	3, c
	ld	a, #0xe4
	jr	Z, 00110$
	ld	a, #0xd2
00110$:
	ldh	(_BGP_REG + 0), a
	jr	00103$
00102$:
;src/main.c:659: set_bkg_palette(0u, 1u, startup_scott_stinks_a_palettes);
	push	bc
	ld	de, #_startup_scott_stinks_a_palettes
	push	de
	xor	a, a
	inc	a
	push	af
	call	_set_bkg_palette
	add	sp, #4
	pop	bc
00103$:
;src/main.c:661: vsync();
	call	_vsync
;src/main.c:662: dt_audio_update();
	push	bc
	ld	e, #b_dt_audio_update
	ld	hl, #_dt_audio_update
	call	___sdcc_bcall_ehl
	pop	bc
;src/main.c:663: ++frame_clock;
	ld	hl, #_frame_clock
	inc	(hl)
;src/main.c:655: for (frame = 0u; frame != 84u; ++frame) {
	inc	c
	ld	a, c
	sub	a, #0x54
	jr	NZ, 00105$
;src/main.c:665: }
	ret
;src/main.c:667: static void show_bimpson_card(void) {
;	---------------------------------
; Function show_bimpson_card
; ---------------------------------
_show_bimpson_card:
;src/main.c:670: begin_display_reload();
	call	_begin_display_reload
;src/main.c:676: BANK(startup_bimpson_productions_a)
	ld	b, #<(___bank_startup_bimpson_productions_a)
;src/main.c:675: startup_bimpson_productions_a_palettes,
;src/main.c:674: startup_bimpson_productions_a_map,
;src/main.c:672: startup_bimpson_productions_a_tiles,
	push	bc
	inc	sp
	ld	de, #_startup_bimpson_productions_a_palettes
	push	de
	ld	de, #_startup_bimpson_productions_a_map
	push	de
	ld	a, #0x75
	ld	de, #_startup_bimpson_productions_a_tiles
	call	_show_banked_image_card
;src/main.c:678: finish_display_reload(0u);
	xor	a, a
	call	_finish_display_reload
;src/main.c:680: for (frame = 0u; frame != 72u; ++frame) {
	ld	c, #0x00
00105$:
;src/main.c:681: if (_cpu != CGB_TYPE) {
	ld	a, (#__cpu)
	sub	a, #0x11
	jr	Z, 00102$
;src/main.c:682: BGP_REG = ((frame & 0x10u) == 0u) ? 0xE4u : 0xD8u;
	bit	4, c
	ld	a, #0xe4
	jr	Z, 00110$
	ld	a, #0xd8
00110$:
	ldh	(_BGP_REG + 0), a
	jr	00103$
00102$:
;src/main.c:684: set_bkg_palette(0u, 1u, startup_bimpson_productions_a_palettes);
	push	bc
	ld	de, #_startup_bimpson_productions_a_palettes
	push	de
	xor	a, a
	inc	a
	push	af
	call	_set_bkg_palette
	add	sp, #4
	pop	bc
00103$:
;src/main.c:686: vsync();
	call	_vsync
;src/main.c:687: dt_audio_update();
	push	bc
	ld	e, #b_dt_audio_update
	ld	hl, #_dt_audio_update
	call	___sdcc_bcall_ehl
	pop	bc
;src/main.c:688: ++frame_clock;
	ld	hl, #_frame_clock
	inc	(hl)
;src/main.c:680: for (frame = 0u; frame != 72u; ++frame) {
	inc	c
	ld	a, c
	sub	a, #0x48
	jr	NZ, 00105$
;src/main.c:690: }
	ret
;src/main.c:692: static void print_footer(const char *footer) {
;	---------------------------------
; Function print_footer
; ---------------------------------
_print_footer:
;src/main.c:693: draw_text_xy(0u, 16u, "--------------------");
	push	de
	ld	bc, #___str_0
	push	bc
	ld	e, #0x10
	xor	a, a
	call	_draw_text_xy
;src/main.c:694: draw_text_xy(0u, 17u, "                    ");
	ld	bc, #___str_1
	push	bc
	ld	e, #0x11
	xor	a, a
	call	_draw_text_xy
;src/main.c:695: draw_text_xy(0u, 17u, footer);
	ld	e, #0x11
	xor	a, a
	call	_draw_text_xy
;src/main.c:696: }
	ret
___str_0:
	.ascii "--------------------"
	.db 0x00
___str_1:
	.ascii "                    "
	.db 0x00
;src/main.c:698: void show_message(const char *title, const char *body, const char *footer) {
;	---------------------------------
; Function show_message
; ---------------------------------
_show_message::
	dec	sp
;src/main.c:700: uint8_t last = 0u;
	ldhl	sp,	#0
	ld	(hl), #0x00
;src/main.c:702: use_text_screen();
	push	bc
	push	de
	call	_use_text_screen
	pop	de
;src/main.c:703: draw_text(title);
	call	_draw_text
;src/main.c:704: draw_text("\n\n");
	ld	de, #___str_2
	call	_draw_text
;src/main.c:705: draw_text(body);
	pop	de
	call	_draw_text
;src/main.c:706: print_footer(footer);
	ldhl	sp,	#3
	ld	a, (hl+)
	ld	e, a
	ld	d, (hl)
	call	_print_footer
;src/main.c:708: while (1) {
00109$:
;src/main.c:709: vsync();
	call	_vsync
;src/main.c:710: dt_audio_update();
	ld	e, #b_dt_audio_update
	ld	hl, #_dt_audio_update
	call	___sdcc_bcall_ehl
;src/main.c:711: keys = joypad();
	call	_joypad
;src/main.c:712: if (((keys & J_A) && !(last & J_A)) ||
	bit	4, a
	jr	Z, 00105$
	push	hl
	ldhl	sp,	#2
	bit	4, (hl)
	pop	hl
	jr	Z, 00101$
00105$:
;src/main.c:713: ((keys & J_B) && !(last & J_B)) ||
	bit	5, a
	jr	Z, 00107$
	push	hl
	ldhl	sp,	#2
	bit	5, (hl)
	pop	hl
	jr	Z, 00101$
00107$:
;src/main.c:714: ((keys & J_START) && !(last & J_START))) {
	bit	7, a
	jr	Z, 00102$
	push	hl
	ldhl	sp,	#2
	bit	7, (hl)
	pop	hl
	jr	NZ, 00102$
00101$:
;src/main.c:715: waitpadup();
	call	_waitpadup
;src/main.c:716: break;
	jr	00110$
00102$:
;src/main.c:718: last = keys;
	ldhl	sp,	#0
	ld	(hl), a
	jr	00109$
00110$:
;src/main.c:721: load_room();
	call	_load_room
;src/main.c:722: render_room();
	call	_render_room
;src/main.c:723: }
	inc	sp
	pop	hl
	pop	af
	jp	(hl)
___str_2:
	.db 0x0a
	.db 0x0a
	.db 0x00
;src/main.c:725: uint8_t menu_screen(const char *title, const char *subtitle, const char *const *items, uint8_t count, uint8_t selected) {
;	---------------------------------
; Function menu_screen
; ---------------------------------
_menu_screen::
	add	sp, #-7
	ldhl	sp,	#4
	ld	a, e
	ld	(hl+), a
	ld	(hl), d
	ldhl	sp,	#2
	ld	a, c
	ld	(hl+), a
	ld	(hl), b
;src/main.c:728: uint8_t last = 0u;
	ldhl	sp,	#0
	ld	(hl), #0x00
;src/main.c:730: while (1) {
	ldhl	sp,	#11
	ld	a, (hl)
	dec	a
	ldhl	sp,	#1
	ld	(hl), a
00118$:
;src/main.c:731: use_text_screen();
	call	_use_text_screen
;src/main.c:732: draw_text(title);
	ldhl	sp,	#4
	ld	a, (hl+)
	ld	e, a
	ld	d, (hl)
	call	_draw_text
;src/main.c:733: draw_text("\n");
	ld	de, #___str_3
	call	_draw_text
;src/main.c:734: draw_text(subtitle);
	ldhl	sp,	#2
	ld	a, (hl+)
	ld	e, a
	ld	d, (hl)
	call	_draw_text
;src/main.c:735: draw_text("\n\n");
	ld	de, #___str_4
	call	_draw_text
;src/main.c:736: for (i = 0u; i != count; ++i) {
	ldhl	sp,	#6
	ld	(hl), #0x00
00121$:
	ldhl	sp,	#11
	ld	a, (hl)
	ldhl	sp,	#6
	sub	a, (hl)
	jr	Z, 00101$
;src/main.c:737: setchar((i == selected) ? '>' : ' ');
	ldhl	sp,	#12
	ld	a, (hl)
	ldhl	sp,	#6
	sub	a, (hl)
	ld	a, #0x3e
	jr	Z, 00126$
	ld	a, #0x20
00126$:
	push	af
	inc	sp
	call	_setchar
	inc	sp
;src/main.c:738: gotoxy(1u, (uint8_t)(posy()));
	call	_posy
	ld	h, e
	ld	l, #0x01
	push	hl
	call	_gotoxy
	pop	hl
;src/main.c:739: draw_text(items[i]);
	ldhl	sp,	#6
	ld	c, (hl)
	ld	b, #0x00
	sla	c
	rl	b
	ldhl	sp,	#9
	ld	a,	(hl+)
	ld	h, (hl)
	ld	l, a
	add	hl, bc
	ld	a, (hl+)
	ld	c, a
	ld	a, (hl)
	ld	e, c
	ld	d, a
	call	_draw_text
;src/main.c:740: draw_text("\n");
	ld	de, #___str_3
	call	_draw_text
;src/main.c:736: for (i = 0u; i != count; ++i) {
	ldhl	sp,	#6
	inc	(hl)
	jr	00121$
00101$:
;src/main.c:742: print_footer("WHEEL MOVE  SELECT");
	ld	de, #___str_5
	call	_print_footer
;src/main.c:744: while (1) {
00115$:
;src/main.c:745: vsync();
	call	_vsync
;src/main.c:746: dt_audio_update();
	ld	e, #b_dt_audio_update
	ld	hl, #_dt_audio_update
	call	___sdcc_bcall_ehl
;src/main.c:747: keys = joypad();
	call	_joypad
;src/main.c:748: if ((keys & J_UP) && !(last & J_UP)) {
	bit	2, a
	jr	Z, 00103$
	push	hl
	ldhl	sp,	#2
	bit	2, (hl)
	pop	hl
	jr	NZ, 00103$
;src/main.c:749: dt_audio_play_sfx(SFX_MENU_MOVE);
	xor	a, a
	push	af
	inc	sp
	ld	e, #b_dt_audio_play_sfx
	ld	hl, #_dt_audio_play_sfx
	call	___sdcc_bcall_ehl
	inc	sp
;src/main.c:750: selected = (selected == 0u) ? (count - 1u) : (selected - 1u);
	ldhl	sp,	#12
	ld	a, (hl)
	or	a, a
	jr	NZ, 00127$
	ldhl	sp,	#1
	ld	a, (hl)
	jr	00128$
00127$:
	ldhl	sp,	#12
	ld	a, (hl)
	dec	a
00128$:
	ldhl	sp,	#12
	ld	(hl), a
;src/main.c:751: break;
	jr	00116$
00103$:
;src/main.c:753: if ((keys & J_DOWN) && !(last & J_DOWN)) {
	bit	3, a
	jr	Z, 00106$
	push	hl
	ldhl	sp,	#2
	bit	3, (hl)
	pop	hl
	jr	NZ, 00106$
;src/main.c:754: dt_audio_play_sfx(SFX_MENU_MOVE);
	xor	a, a
	push	af
	inc	sp
	ld	e, #b_dt_audio_play_sfx
	ld	hl, #_dt_audio_play_sfx
	call	___sdcc_bcall_ehl
	inc	sp
;src/main.c:755: selected = (selected + 1u) % count;
	ldhl	sp,	#12
	ld	a, (hl-)
	ld	e, a
	ld	d, #0x00
	inc	de
	ld	c, (hl)
	xor	a, a
	ld	b, a
	call	__moduint
	ldhl	sp,	#12
	ld	(hl), c
;src/main.c:756: break;
	jr	00116$
00106$:
;src/main.c:758: if ((keys & J_A) && !(last & J_A)) {
	bit	4, a
	jr	Z, 00109$
	push	hl
	ldhl	sp,	#2
	bit	4, (hl)
	pop	hl
	jr	NZ, 00109$
;src/main.c:759: dt_audio_play_sfx(SFX_CONFIRM);
	ld	a, #0x01
	push	af
	inc	sp
	ld	e, #b_dt_audio_play_sfx
	ld	hl, #_dt_audio_play_sfx
	call	___sdcc_bcall_ehl
	inc	sp
;src/main.c:760: waitpadup();
	call	_waitpadup
;src/main.c:761: return selected;
	ldhl	sp,	#12
	ld	a, (hl)
	jr	00123$
00109$:
;src/main.c:763: if ((keys & J_START) && !(last & J_START)) {
	bit	7, a
	jr	Z, 00112$
	push	hl
	ldhl	sp,	#2
	bit	7, (hl)
	pop	hl
	jr	NZ, 00112$
;src/main.c:764: dt_audio_play_sfx(SFX_CANCEL);
	ld	a, #0x02
	push	af
	inc	sp
	ld	e, #b_dt_audio_play_sfx
	ld	hl, #_dt_audio_play_sfx
	call	___sdcc_bcall_ehl
	inc	sp
;src/main.c:765: waitpadup();
	call	_waitpadup
;src/main.c:766: return count - 1u;
	ldhl	sp,	#1
	ld	a, (hl)
	jr	00123$
00112$:
;src/main.c:768: last = keys;
	ldhl	sp,	#0
	ld	(hl), a
	jp	00115$
00116$:
;src/main.c:770: waitpadup();
	call	_waitpadup
;src/main.c:771: last = 0u;
	ldhl	sp,	#0
	ld	(hl), #0x00
	jp	00118$
00123$:
;src/main.c:773: }
	add	sp, #7
	pop	hl
	add	sp, #4
	jp	(hl)
___str_3:
	.db 0x0a
	.db 0x00
___str_4:
	.db 0x0a
	.db 0x0a
	.db 0x00
___str_5:
	.ascii "WHEEL MOVE  SELECT"
	.db 0x00
;src/main.c:775: static const task_t *current_task(void) {
;	---------------------------------
; Function current_task
; ---------------------------------
_current_task:
;src/main.c:776: return &tasks[task_id];
	ld	hl, #_task_id
	ld	c, (hl)
	ld	b, #0x00
	ld	l, c
	ld	h, b
	add	hl, hl
	add	hl, hl
	add	hl, bc
	add	hl, hl
	ld	bc, #_tasks
	add	hl, bc
	ld	c, l
	ld	b, h
;src/main.c:777: }
	ret
;src/main.c:779: static void assign_daily_task(void) {
;	---------------------------------
; Function assign_daily_task
; ---------------------------------
_assign_daily_task:
;src/main.c:780: task_id = (uint8_t)((day_count - 1u) % TASK_COUNT);
	ld	a, (_day_count)
	ld	d, #0x00
	ld	e, a
	dec	de
	ld	bc, #0x0003
	call	__moduint
	ld	hl, #_task_id
	ld	(hl), c
;src/main.c:781: task_done = 0u;
	xor	a, a
	ld	(#_task_done),a
;src/main.c:782: }
	ret
;src/main.c:784: static void setup_new_game(void) {
;	---------------------------------
; Function setup_new_game
; ---------------------------------
_setup_new_game:
;src/main.c:785: room_id = ROOM_DISTRICT;
	xor	a, a
	ld	(#_room_id),a
;src/main.c:786: player_x = 10u;
	ld	hl, #_player_x
	ld	(hl), #0x0a
;src/main.c:787: player_y = 12u;
	ld	hl, #_player_y
	ld	(hl), #0x0c
;src/main.c:788: facing = DIR_DOWN;
	xor	a, a
	ld	(#_facing),a
;src/main.c:789: credits = 18u;
	ld	hl, #_credits
	ld	(hl), #0x12
;src/main.c:790: day_count = 1u;
	ld	hl, #_day_count
	ld	(hl), #0x01
;src/main.c:791: inventory = 0u;
;src/main.c:792: media = 0u;
	xor	a, a
	ld	(#_inventory), a
	ld	(#_media),a
;src/main.c:793: decor_slots[0] = DECOR_NONE;
	ld	hl, #_decor_slots
;src/main.c:794: decor_slots[1] = DECOR_NONE;
	xor	a, a
	ld	(hl+), a
	ld	(hl), a
;src/main.c:795: decor_slots[2] = DECOR_NONE;
	ld	hl, #_decor_slots + 2
	ld	(hl), #0x00
;src/main.c:796: ride_level = RIDE_FOOT;
;src/main.c:797: palette_mood = 0u;
	xor	a, a
	ld	(#_ride_level), a
	ld	(#_palette_mood),a
;src/main.c:798: assign_daily_task();
;src/main.c:799: }
	jp	_assign_daily_task
;src/main.c:801: static void print_task_summary(void) {
;	---------------------------------
; Function print_task_summary
; ---------------------------------
_print_task_summary:
;src/main.c:802: const task_t *task = current_task();
	call	_current_task
;src/main.c:804: draw_text("Day ");
	push	bc
	ld	de, #___str_6
	call	_draw_text
;src/main.c:805: draw_u8(day_count);
	ld	a, (_day_count)
	call	_draw_u8
;src/main.c:806: draw_text("  Cr ");
	ld	de, #___str_7
	call	_draw_text
;src/main.c:807: draw_u8(credits);
	ld	a, (_credits)
	call	_draw_u8
;src/main.c:808: draw_text("  ");
	ld	de, #___str_8
	call	_draw_text
;src/main.c:809: draw_text(ride_name(ride_level));
	ld	a, (_ride_level)
	call	_ride_name
	ld	e, c
	ld	d, b
	call	_draw_text
;src/main.c:810: draw_text("\n");
	ld	de, #___str_9
	call	_draw_text
	pop	bc
;src/main.c:811: draw_text(task->title);
	ld	hl, #0x0004
	add	hl, bc
	ld	a, (hl+)
	ld	l, (hl)
	push	bc
	ld	e, a
	ld	d, l
	call	_draw_text
;src/main.c:812: draw_text("\n");
	ld	de, #___str_9
	call	_draw_text
	pop	bc
;src/main.c:813: if (task_done) {
	ld	a, (#_task_done)
	or	a, a
	jr	Z, 00102$
;src/main.c:814: draw_text("Done for tonight.\n");
	ld	de, #___str_10
	call	_draw_text
;src/main.c:815: draw_text("Sleep in your room when\nyou want the next errand.");
	ld	de, #___str_11
	jp	_draw_text
00102$:
;src/main.c:817: draw_text(task->brief);
	ld	hl, #0x0006
	add	hl, bc
	ld	a, (hl+)
	ld	c, (hl)
	ld	e, a
	ld	d, c
;src/main.c:819: }
	jp	_draw_text
___str_6:
	.ascii "Day "
	.db 0x00
___str_7:
	.ascii "  Cr "
	.db 0x00
___str_8:
	.ascii "  "
	.db 0x00
___str_9:
	.db 0x0a
	.db 0x00
___str_10:
	.ascii "Done for tonight."
	.db 0x0a
	.db 0x00
___str_11:
	.ascii "Sleep in your room when"
	.db 0x0a
	.ascii "you want the next errand."
	.db 0x00
;src/main.c:821: static const char *decor_name(uint8_t decor_id) {
;	---------------------------------
; Function decor_name
; ---------------------------------
_decor_name:
	ld	c, a
;src/main.c:822: switch (decor_id) {
	ld	a, #0x05
	sub	a, c
	jr	C, 00106$
	ld	b, #0x00
	ld	hl, #00117$
	add	hl, bc
	add	hl, bc
	ld	c, (hl)
	inc	hl
	ld	h, (hl)
	ld	l, c
	jp	(hl)
00117$:
	.dw	00106$
	.dw	00101$
	.dw	00102$
	.dw	00103$
	.dw	00104$
	.dw	00105$
;src/main.c:823: case DECOR_POSTER: return "Poster";
00101$:
	ld	bc, #___str_12
	ret
;src/main.c:824: case DECOR_LAMP: return "Lamp";
00102$:
	ld	bc, #___str_13
	ret
;src/main.c:825: case DECOR_SHELF: return "Shelf";
00103$:
	ld	bc, #___str_14
	ret
;src/main.c:826: case DECOR_TANK: return "Tank";
00104$:
	ld	bc, #___str_15
	ret
;src/main.c:827: case DECOR_DOCK: return "Dock";
00105$:
	ld	bc, #___str_16
	ret
;src/main.c:828: default: return "Empty";
00106$:
	ld	bc, #___str_17
;src/main.c:829: }
;src/main.c:830: }
	ret
___str_12:
	.ascii "Poster"
	.db 0x00
___str_13:
	.ascii "Lamp"
	.db 0x00
___str_14:
	.ascii "Shelf"
	.db 0x00
___str_15:
	.ascii "Tank"
	.db 0x00
___str_16:
	.ascii "Dock"
	.db 0x00
___str_17:
	.ascii "Empty"
	.db 0x00
;src/main.c:832: static const char *ride_name(uint8_t level) {
;	---------------------------------
; Function ride_name
; ---------------------------------
_ride_name:
;src/main.c:833: switch (level) {
	cp	a, #0x01
	jr	Z, 00101$
	cp	a, #0x02
	jr	Z, 00102$
	sub	a, #0x03
	jr	Z, 00103$
	jr	00104$
;src/main.c:834: case RIDE_BICYCLE: return "Bike";
00101$:
	ld	bc, #___str_18
	ret
;src/main.c:835: case RIDE_SCOOTER: return "Scoot";
00102$:
	ld	bc, #___str_19
	ret
;src/main.c:836: case RIDE_MOTORCYCLE: return "Moto";
00103$:
	ld	bc, #___str_20
	ret
;src/main.c:837: default: return "Foot";
00104$:
	ld	bc, #___str_21
;src/main.c:838: }
;src/main.c:839: }
	ret
___str_18:
	.ascii "Bike"
	.db 0x00
___str_19:
	.ascii "Scoot"
	.db 0x00
___str_20:
	.ascii "Moto"
	.db 0x00
___str_21:
	.ascii "Foot"
	.db 0x00
;src/main.c:841: static void inventory_screen(uint8_t page) {
;	---------------------------------
; Function inventory_screen
; ---------------------------------
_inventory_screen:
	dec	sp
	dec	sp
	ldhl	sp,	#1
;src/main.c:843: uint8_t last = 0u;
	ld	(hl-), a
	ld	(hl), #0x00
;src/main.c:845: while (1) {
00150$:
;src/main.c:846: use_text_screen();
	call	_use_text_screen
;src/main.c:847: if (page == 0u) {
	ldhl	sp,	#1
	ld	a, (hl)
	or	a, a
	jp	NZ, 00133$
;src/main.c:848: draw_text("PACK\n\n");
	ld	de, #___str_22
	call	_draw_text
;src/main.c:849: print_task_summary();
	call	_print_task_summary
;src/main.c:850: draw_text("\nBag:\n");
	ld	de, #___str_23
	call	_draw_text
;src/main.c:851: if (inventory == 0u) {
	ld	a, (#_inventory)
	or	a, a
	jr	NZ, 00118$
;src/main.c:852: draw_text("  nothing yet");
	ld	de, #___str_24
	call	_draw_text
	jp	00134$
00118$:
;src/main.c:854: if (has_item(ITEM_SOFTPHONES)) draw_text("  softphones\n");
	ld	a, #0x01
	call	_has_item
	or	a, a
	jr	Z, 00102$
	ld	de, #___str_25
	call	_draw_text
00102$:
;src/main.c:855: if (has_item(ITEM_BROTH)) draw_text("  night broth\n");
	ld	a, #0x02
	call	_has_item
	or	a, a
	jr	Z, 00104$
	ld	de, #___str_26
	call	_draw_text
00104$:
;src/main.c:856: if (has_item(ITEM_MINIDISC)) draw_text("  mini-disc\n");
	ld	a, #0x04
	call	_has_item
	or	a, a
	jr	Z, 00106$
	ld	de, #___str_27
	call	_draw_text
00106$:
;src/main.c:857: if (has_item(ITEM_POSTER)) draw_text("  glow poster\n");
	ld	a, #0x08
	call	_has_item
	or	a, a
	jr	Z, 00108$
	ld	de, #___str_28
	call	_draw_text
00108$:
;src/main.c:858: if (has_item(ITEM_LAMP)) draw_text("  neon lamp\n");
	ld	a, #0x10
	call	_has_item
	or	a, a
	jr	Z, 00110$
	ld	de, #___str_29
	call	_draw_text
00110$:
;src/main.c:859: if (has_item(ITEM_SHELF)) draw_text("  crate shelf\n");
	ld	a, #0x20
	call	_has_item
	or	a, a
	jr	Z, 00112$
	ld	de, #___str_30
	call	_draw_text
00112$:
;src/main.c:860: if (has_item(ITEM_TANK)) draw_text("  fish tank\n");
	ld	a, #0x40
	call	_has_item
	or	a, a
	jr	Z, 00114$
	ld	de, #___str_31
	call	_draw_text
00114$:
;src/main.c:861: if (has_item(ITEM_DOCK)) draw_text("  pocket dock\n");
	ld	a, #0x80
	call	_has_item
	or	a, a
	jp	Z, 00134$
	ld	de, #___str_32
	call	_draw_text
	jp	00134$
00133$:
;src/main.c:863: } else if (page == 1u) {
	ldhl	sp,	#1
	ld	a, (hl)
	dec	a
	jr	NZ, 00130$
;src/main.c:864: draw_text("TUNES\n\nCollected:\n");
	ld	de, #___str_33
	call	_draw_text
;src/main.c:865: if (media == 0u) {
	ld	a, (#_media)
	or	a, a
	jr	NZ, 00127$
;src/main.c:866: draw_text("  no tapes yet\n\n");
	ld	de, #___str_34
	call	_draw_text
;src/main.c:867: draw_text("Finish errands and keep\nan ear out for bootlegs.");
	ld	de, #___str_35
	call	_draw_text
	jp	00134$
00127$:
;src/main.c:869: if (media & MEDIA_RAIN_LOOP) draw_text("  Rain Loop\n");
	ld	a, (_media)
	rrca
	jr	NC, 00121$
	ld	de, #___str_36
	call	_draw_text
00121$:
;src/main.c:870: if (media & MEDIA_VENDING_DREAMS) draw_text("  Vending Dreams\n");
	ld	a, (_media)
	bit	1, a
	jr	Z, 00123$
	ld	de, #___str_37
	call	_draw_text
00123$:
;src/main.c:871: if (media & MEDIA_ROOFTOP_SET) draw_text("  Rooftop Set\n");
	ld	a, (_media)
	bit	2, a
	jr	Z, 00125$
	ld	de, #___str_38
	call	_draw_text
00125$:
;src/main.c:872: draw_text("\nDeck: ");
	ld	de, #___str_39
	call	_draw_text
;src/main.c:873: draw_text(dt_record_name(dt_audio_get_home_record()));
	ld	e, #b_dt_audio_get_home_record
	ld	hl, #_dt_audio_get_home_record
	call	___sdcc_bcall_ehl
	push	af
	inc	sp
	ld	e, #b_dt_record_name
	ld	hl, #_dt_record_name
	call	___sdcc_bcall_ehl
	inc	sp
	ld	e, c
	ld	d, b
	call	_draw_text
;src/main.c:874: draw_text("\nUse your home deck\nto swap records.");
	ld	de, #___str_40
	call	_draw_text
	jr	00134$
00130$:
;src/main.c:877: draw_text("ROOM\n\n");
	ld	de, #___str_41
	call	_draw_text
;src/main.c:878: draw_text("Slot 1: ");
	ld	de, #___str_42
	call	_draw_text
;src/main.c:879: draw_text(decor_name(decor_slots[0]));
	ld	a, (#_decor_slots + 0)
	call	_decor_name
	ld	e, c
	ld	d, b
	call	_draw_text
;src/main.c:880: draw_text("\n");
	ld	de, #___str_43
	call	_draw_text
;src/main.c:881: draw_text("Slot 2: ");
	ld	de, #___str_44
	call	_draw_text
;src/main.c:882: draw_text(decor_name(decor_slots[1]));
	ld	a, (#(_decor_slots + 1) + 0)
	call	_decor_name
	ld	e, c
	ld	d, b
	call	_draw_text
;src/main.c:883: draw_text("\n");
	ld	de, #___str_43
	call	_draw_text
;src/main.c:884: draw_text("Slot 3: ");
	ld	de, #___str_45
	call	_draw_text
;src/main.c:885: draw_text(decor_name(decor_slots[2]));
	ld	a, (#(_decor_slots + 2) + 0)
	call	_decor_name
	ld	e, c
	ld	d, b
	call	_draw_text
;src/main.c:886: draw_text("\n\nUse your apartment shelf\npads to place decor.");
	ld	de, #___str_46
	call	_draw_text
00134$:
;src/main.c:889: print_footer("LEFT/RIGHT TAB  MENU");
	ld	de, #___str_47
	call	_print_footer
;src/main.c:890: while (1) {
00147$:
;src/main.c:891: vsync();
	call	_vsync
;src/main.c:892: dt_audio_update();
	ld	e, #b_dt_audio_update
	ld	hl, #_dt_audio_update
	call	___sdcc_bcall_ehl
;src/main.c:893: keys = joypad();
	call	_joypad
;src/main.c:894: if ((keys & J_B) && !(last & J_B)) {
	bit	5, a
	jr	Z, 00136$
	push	hl
	ldhl	sp,	#2
	bit	5, (hl)
	pop	hl
	jr	NZ, 00136$
;src/main.c:895: dt_audio_play_sfx(SFX_MENU_MOVE);
	xor	a, a
	push	af
	inc	sp
	ld	e, #b_dt_audio_play_sfx
	ld	hl, #_dt_audio_play_sfx
	call	___sdcc_bcall_ehl
	inc	sp
;src/main.c:896: page = (page == 0u) ? 2u : (page - 1u);
	ldhl	sp,	#1
	ld	a, (hl)
	or	a, a
	jr	NZ, 00154$
	ld	a, #0x02
	jr	00155$
00154$:
	ldhl	sp,	#1
	ld	a, (hl)
	dec	a
00155$:
	ldhl	sp,	#1
	ld	(hl), a
;src/main.c:897: break;
	jr	00148$
00136$:
;src/main.c:899: if ((keys & J_SELECT) && !(last & J_SELECT)) {
	bit	6, a
	jr	Z, 00139$
	push	hl
	ldhl	sp,	#2
	bit	6, (hl)
	pop	hl
	jr	NZ, 00139$
;src/main.c:900: dt_audio_play_sfx(SFX_MENU_MOVE);
	xor	a, a
	push	af
	inc	sp
	ld	e, #b_dt_audio_play_sfx
	ld	hl, #_dt_audio_play_sfx
	call	___sdcc_bcall_ehl
	inc	sp
;src/main.c:901: page = (page + 1u) % 3u;
	ldhl	sp,	#1
	ld	e, (hl)
	ld	d, #0x00
	inc	de
	ld	bc, #0x0003
	call	__moduint
	ldhl	sp,	#1
	ld	(hl), c
;src/main.c:902: break;
	jr	00148$
00139$:
;src/main.c:904: if (((keys & J_START) && !(last & J_START)) || ((keys & J_A) && !(last & J_A))) {
	bit	7, a
	jr	Z, 00145$
	push	hl
	ldhl	sp,	#2
	bit	7, (hl)
	pop	hl
	jr	Z, 00141$
00145$:
	bit	4, a
	jr	Z, 00142$
	push	hl
	ldhl	sp,	#2
	bit	4, (hl)
	pop	hl
	jr	NZ, 00142$
00141$:
;src/main.c:905: dt_audio_play_sfx(SFX_CANCEL);
	ld	a, #0x02
	push	af
	inc	sp
	ld	e, #b_dt_audio_play_sfx
	ld	hl, #_dt_audio_play_sfx
	call	___sdcc_bcall_ehl
;src/main.c:906: waitpadup();
	add	sp, #3
	jp	_waitpadup
;src/main.c:907: return;
	jr	00152$
00142$:
;src/main.c:909: last = keys;
	ldhl	sp,	#0
	ld	(hl), a
	jp	00147$
00148$:
;src/main.c:911: waitpadup();
	call	_waitpadup
;src/main.c:912: last = 0u;
	ldhl	sp,	#0
	ld	(hl), #0x00
	jp	00150$
00152$:
;src/main.c:914: }
	inc	sp
	inc	sp
	ret
___str_22:
	.ascii "PACK"
	.db 0x0a
	.db 0x0a
	.db 0x00
___str_23:
	.db 0x0a
	.ascii "Bag:"
	.db 0x0a
	.db 0x00
___str_24:
	.ascii "  nothing yet"
	.db 0x00
___str_25:
	.ascii "  softphones"
	.db 0x0a
	.db 0x00
___str_26:
	.ascii "  night broth"
	.db 0x0a
	.db 0x00
___str_27:
	.ascii "  mini-disc"
	.db 0x0a
	.db 0x00
___str_28:
	.ascii "  glow poster"
	.db 0x0a
	.db 0x00
___str_29:
	.ascii "  neon lamp"
	.db 0x0a
	.db 0x00
___str_30:
	.ascii "  crate shelf"
	.db 0x0a
	.db 0x00
___str_31:
	.ascii "  fish tank"
	.db 0x0a
	.db 0x00
___str_32:
	.ascii "  pocket dock"
	.db 0x0a
	.db 0x00
___str_33:
	.ascii "TUNES"
	.db 0x0a
	.db 0x0a
	.ascii "Collected:"
	.db 0x0a
	.db 0x00
___str_34:
	.ascii "  no tapes yet"
	.db 0x0a
	.db 0x0a
	.db 0x00
___str_35:
	.ascii "Finish errands and keep"
	.db 0x0a
	.ascii "an ear out for bootlegs."
	.db 0x00
___str_36:
	.ascii "  Rain Loop"
	.db 0x0a
	.db 0x00
___str_37:
	.ascii "  Vending Dreams"
	.db 0x0a
	.db 0x00
___str_38:
	.ascii "  Rooftop Set"
	.db 0x0a
	.db 0x00
___str_39:
	.db 0x0a
	.ascii "Deck: "
	.db 0x00
___str_40:
	.db 0x0a
	.ascii "Use your home deck"
	.db 0x0a
	.ascii "to swap records."
	.db 0x00
___str_41:
	.ascii "ROOM"
	.db 0x0a
	.db 0x0a
	.db 0x00
___str_42:
	.ascii "Slot 1: "
	.db 0x00
___str_43:
	.db 0x0a
	.db 0x00
___str_44:
	.ascii "Slot 2: "
	.db 0x00
___str_45:
	.ascii "Slot 3: "
	.db 0x00
___str_46:
	.db 0x0a
	.db 0x0a
	.ascii "Use your apartment shelf"
	.db 0x0a
	.ascii "pads to place decor."
	.db 0x00
___str_47:
	.ascii "LEFT/RIGHT TAB  MENU"
	.db 0x00
;src/main.c:916: static void help_screen(void) {
;	---------------------------------
; Function help_screen
; ---------------------------------
_help_screen:
;src/main.c:921: );
;src/main.c:919: "WHEEL moves.\nSELECT talks.\nLEFT pack.\nRIGHT tunes.\nSouth road leads to wheels.",
;src/main.c:918: "HELP",
	ld	de, #___str_50
	push	de
	ld	bc, #___str_49
	ld	de, #___str_48
	call	_show_message
;src/main.c:922: }
	ret
___str_48:
	.ascii "HELP"
	.db 0x00
___str_49:
	.ascii "WHEEL moves."
	.db 0x0a
	.ascii "SELECT talks."
	.db 0x0a
	.ascii "LEFT pack."
	.db 0x0a
	.ascii "RIGHT tunes."
	.db 0x0a
	.ascii "South road leads to wheels."
	.db 0x00
___str_50:
	.ascii "SELECT CLOSE"
	.db 0x00
;src/main.c:924: static void options_screen(void) {
;	---------------------------------
; Function options_screen
; ---------------------------------
_options_screen:
;src/main.c:932: choice = menu_screen("OPTIONS", "Pick a screen mood", items, 3u, palette_mood);
	ld	a, (_palette_mood)
	ld	h, a
	ld	l, #0x03
	push	hl
	ld	de, #_options_screen_items_10000_559
	push	de
	ld	bc, #___str_55
	ld	de, #___str_54
	call	_menu_screen
;src/main.c:933: if (choice < 2u) {
	cp	a, #0x02
	ret	NC
;src/main.c:934: palette_mood = choice;
	ld	(#_palette_mood),a
;src/main.c:936: }
	ret
_options_screen_items_10000_559:
	.dw ___str_51
	.dw ___str_52
	.dw ___str_53
___str_51:
	.ascii "Soft night tint"
	.db 0x00
___str_52:
	.ascii "Sharp contrast"
	.db 0x00
___str_53:
	.ascii "Back"
	.db 0x00
___str_54:
	.ascii "OPTIONS"
	.db 0x00
___str_55:
	.ascii "Pick a screen mood"
	.db 0x00
;src/main.c:938: static void decor_screen(void) {
;	---------------------------------
; Function decor_screen
; ---------------------------------
_decor_screen:
	dec	sp
;src/main.c:959: slot = menu_screen("DECOR", "Pick a room slot", slot_items, 4u, 0u);
	ld	hl, #0x04
	push	hl
	ld	de, #_decor_screen_slot_items_10000_563
	push	de
	ld	bc, #___str_67
	ld	de, #___str_66
	call	_menu_screen
;src/main.c:960: if (slot == 3u) {
	ldhl	sp,#0
	ld	(hl), a
	sub	a, #0x03
	jp	Z, 00128$
;src/main.c:961: return;
;src/main.c:964: choice = menu_screen("DECOR", "Pick an item", decor_items, 7u, 0u);
	ld	hl, #0x07
	push	hl
	ld	de, #_decor_screen_decor_items_10000_563
	push	de
	ld	bc, #___str_68
	ld	de, #___str_66
	call	_menu_screen
;src/main.c:965: if (choice == 6u) {
	ld	c, a
	sub	a, #0x06
	jp	Z, 00128$
;src/main.c:966: return;
;src/main.c:969: if (choice == 0u) {
	ld	a, c
	or	a, a
	jr	NZ, 00106$
;src/main.c:970: decor_slots[slot] = DECOR_NONE;
	ld	bc, #_decor_slots+0
	ldhl	sp,	#0
	ld	l, (hl)
	ld	h, #0x00
	add	hl, bc
	ld	c, l
	ld	b, h
	xor	a, a
	ld	(bc), a
;src/main.c:971: load_room();
	inc	sp
	jp	_load_room
;src/main.c:972: return;
	jr	00128$
00106$:
;src/main.c:975: needed_item = 0u;
	ld	b, #0x00
;src/main.c:976: if (choice == DECOR_POSTER) needed_item = ITEM_POSTER;
	ld	a, c
	dec	a
	jr	NZ, 00119$
	ld	b, #0x08
	jr	00120$
00119$:
;src/main.c:977: else if (choice == DECOR_LAMP) needed_item = ITEM_LAMP;
	ld	a, c
	sub	a, #0x02
	jr	NZ, 00116$
	ld	b, #0x10
	jr	00120$
00116$:
;src/main.c:978: else if (choice == DECOR_SHELF) needed_item = ITEM_SHELF;
	ld	a, c
	sub	a, #0x03
	jr	NZ, 00113$
	ld	b, #0x20
	jr	00120$
00113$:
;src/main.c:979: else if (choice == DECOR_TANK) needed_item = ITEM_TANK;
	ld	a, c
	sub	a, #0x04
	jr	NZ, 00110$
	ld	b, #0x40
	jr	00120$
00110$:
;src/main.c:980: else if (choice == DECOR_DOCK) needed_item = ITEM_DOCK;
	ld	a, c
	sub	a, #0x05
	jr	NZ, 00120$
	ld	b, #0x80
00120$:
;src/main.c:982: if (!has_item(needed_item)) {
	push	bc
	ld	a, b
	call	_has_item
	pop	bc
	or	a, a
	jr	NZ, 00140$
;src/main.c:983: show_message("ROOM", "You do not own that piece yet.", "SELECT CLOSE");
	ld	de, #___str_71
	push	de
	ld	bc, #___str_70
	ld	de, #___str_69
	call	_show_message
;src/main.c:984: return;
	jr	00128$
;src/main.c:987: for (i = 0u; i != 3u; ++i) {
00140$:
	ld	de, #_decor_slots+0
	ld	b, #0x00
00126$:
;src/main.c:988: if (decor_slots[i] == choice) {
	ld	l, b
	ld	h, #0x00
	add	hl, de
	ld	a, (hl)
;src/main.c:989: decor_slots[i] = DECOR_NONE;
	sub	a, c
	jr	NZ, 00127$
	ld	(hl), a
00127$:
;src/main.c:987: for (i = 0u; i != 3u; ++i) {
	inc	b
	ld	a, b
	sub	a, #0x03
	jr	NZ, 00126$
;src/main.c:992: decor_slots[slot] = choice;
	ldhl	sp,	#0
	ld	l, (hl)
	ld	h, #0x00
	add	hl, de
	ld	e, l
	ld	d, h
	ld	a, c
	ld	(de), a
;src/main.c:993: load_room();
	inc	sp
	jp	_load_room
00128$:
;src/main.c:994: }
	inc	sp
	ret
_decor_screen_slot_items_10000_563:
	.dw ___str_56
	.dw ___str_57
	.dw ___str_58
	.dw ___str_59
_decor_screen_decor_items_10000_563:
	.dw ___str_60
	.dw ___str_61
	.dw ___str_62
	.dw ___str_63
	.dw ___str_64
	.dw ___str_65
	.dw ___str_59
___str_56:
	.ascii "Slot 1"
	.db 0x00
___str_57:
	.ascii "Slot 2"
	.db 0x00
___str_58:
	.ascii "Slot 3"
	.db 0x00
___str_59:
	.ascii "Back"
	.db 0x00
___str_60:
	.ascii "Clear"
	.db 0x00
___str_61:
	.ascii "Poster"
	.db 0x00
___str_62:
	.ascii "Lamp"
	.db 0x00
___str_63:
	.ascii "Shelf"
	.db 0x00
___str_64:
	.ascii "Tank"
	.db 0x00
___str_65:
	.ascii "Dock"
	.db 0x00
___str_66:
	.ascii "DECOR"
	.db 0x00
___str_67:
	.ascii "Pick a room slot"
	.db 0x00
___str_68:
	.ascii "Pick an item"
	.db 0x00
___str_69:
	.ascii "ROOM"
	.db 0x00
___str_70:
	.ascii "You do not own that piece yet."
	.db 0x00
___str_71:
	.ascii "SELECT CLOSE"
	.db 0x00
;src/main.c:996: static void pause_menu(void) {
;	---------------------------------
; Function pause_menu
; ---------------------------------
_pause_menu:
;src/main.c:1006: choice = menu_screen("PAUSE", "Block breather", items, 5u, 0u);
	ld	hl, #0x05
	push	hl
	ld	de, #_pause_menu_items_10000_582
	push	de
	ld	bc, #___str_78
	ld	de, #___str_77
	call	_menu_screen
;src/main.c:1007: if (choice == 1u) inventory_screen(0u);
	cp	a, #0x01
	jr	NZ, 00110$
	xor	a, a
	jp	_inventory_screen
00110$:
;src/main.c:1008: else if (choice == 2u) inventory_screen(1u);
	cp	a, #0x02
	jr	NZ, 00107$
	ld	a, #0x01
	jp	_inventory_screen
00107$:
;src/main.c:1009: else if (choice == 3u) help_screen();
	cp	a, #0x03
	jp	Z, _help_screen
;src/main.c:1010: else if (choice == 4u) options_screen();
	sub	a, #0x04
	jp	Z, _options_screen
;src/main.c:1011: }
	ret
_pause_menu_items_10000_582:
	.dw ___str_72
	.dw ___str_73
	.dw ___str_74
	.dw ___str_75
	.dw ___str_76
___str_72:
	.ascii "Resume"
	.db 0x00
___str_73:
	.ascii "Pack"
	.db 0x00
___str_74:
	.ascii "Tunes"
	.db 0x00
___str_75:
	.ascii "Help"
	.db 0x00
___str_76:
	.ascii "Options"
	.db 0x00
___str_77:
	.ascii "PAUSE"
	.db 0x00
___str_78:
	.ascii "Block breather"
	.db 0x00
;src/main.c:1013: static void reward_task(const task_t *task) {
;	---------------------------------
; Function reward_task
; ---------------------------------
_reward_task:
;src/main.c:1014: task_done = 1u;
	ld	hl, #_task_done
	ld	(hl), #0x01
;src/main.c:1015: inventory &= (uint8_t)~task->required_item;
	ld	l, e
	ld	h, d
	inc	hl
	ld	a, (hl)
	cpl
	ld	hl, #_inventory
	and	a, (hl)
	ld	(hl), a
;src/main.c:1016: credits += task->reward_credits;
	ld	l, e
	ld	h, d
	inc	hl
	inc	hl
	inc	hl
	ld	a, (hl)
	ld	hl, #_credits
	add	a, (hl)
	ld	(hl), a
;src/main.c:1017: media |= task->reward_media;
	inc	de
	inc	de
	ld	a, (de)
	ld	hl, #_media
	or	a, (hl)
	ld	(hl), a
;src/main.c:1018: if (dt_audio_get_home_record() == RECORD_NONE) {
	push	de
	ld	e, #b_dt_audio_get_home_record
	ld	hl, #_dt_audio_get_home_record
	call	___sdcc_bcall_ehl
	pop	de
	or	a, a
	jr	NZ, 00102$
;src/main.c:1019: dt_audio_set_home_record(dt_record_from_media(task->reward_media));
	ld	a, (de)
	push	af
	inc	sp
	ld	e, #b_dt_record_from_media
	ld	hl, #_dt_record_from_media
	call	___sdcc_bcall_ehl
	inc	sp
	push	af
	inc	sp
	ld	e, #b_dt_audio_set_home_record
	ld	hl, #_dt_audio_set_home_record
	call	___sdcc_bcall_ehl
	inc	sp
00102$:
;src/main.c:1021: dt_audio_play_sfx(SFX_REWARD);
	ld	a, #0x04
	push	af
	inc	sp
	ld	e, #b_dt_audio_play_sfx
	ld	hl, #_dt_audio_play_sfx
	call	___sdcc_bcall_ehl
	inc	sp
;src/main.c:1022: }
	ret
;src/main.c:1024: static void give_item_message(const char *title, const char *body, uint8_t item) {
;	---------------------------------
; Function give_item_message
; ---------------------------------
_give_item_message:
;src/main.c:1025: add_item(item);
	push	bc
	push	de
	ldhl	sp,	#6
	ld	a, (hl)
	call	_add_item
	pop	de
	pop	bc
;src/main.c:1026: show_message(title, body, "SELECT CLOSE");
	ld	hl, #___str_79
	push	hl
	call	_show_message
;src/main.c:1027: }
	pop	hl
	inc	sp
	jp	(hl)
___str_79:
	.ascii "SELECT CLOSE"
	.db 0x00
;src/main.c:1029: static void buy_item(uint8_t item, uint8_t price, const char *title, const char *body) {
;	---------------------------------
; Function buy_item
; ---------------------------------
_buy_item:
	dec	sp
	ldhl	sp,	#0
	ld	(hl), a
	ld	c, e
;src/main.c:1030: if (has_item(item)) {
	push	bc
	ld	a, (hl)
	call	_has_item
	ld	e, a
	pop	bc
	ld	a, e
	or	a, a
	jr	Z, 00102$
;src/main.c:1031: show_message(title, "You already own one.", "SELECT CLOSE");
	ld	de, #___str_81
	push	de
	ld	bc, #___str_80
	ldhl	sp,	#5
	ld	a, (hl+)
	ld	e, a
	ld	d, (hl)
	call	_show_message
;src/main.c:1032: return;
	jr	00105$
00102$:
;src/main.c:1034: if (credits < price) {
	ld	a, (#_credits)
	sub	a, c
	jr	NC, 00104$
;src/main.c:1035: show_message(title, "Not enough credits tonight.", "SELECT CLOSE");
	ld	de, #___str_81
	push	de
	ld	bc, #___str_82
	ldhl	sp,	#5
	ld	a, (hl+)
	ld	e, a
	ld	d, (hl)
	call	_show_message
;src/main.c:1036: return;
	jr	00105$
00104$:
;src/main.c:1038: credits -= price;
	ld	hl, #_credits
	ld	a, (hl)
	sub	a, c
	ld	(hl), a
;src/main.c:1039: add_item(item);
	ldhl	sp,	#0
	ld	a, (hl)
	call	_add_item
;src/main.c:1040: show_message(title, body, "SELECT CLOSE");
	ld	de, #___str_81
	push	de
	ldhl	sp,	#7
	ld	a, (hl+)
	ld	c, a
	ld	b, (hl)
	ldhl	sp,	#5
	ld	a, (hl+)
	ld	e, a
	ld	d, (hl)
	call	_show_message
00105$:
;src/main.c:1041: }
	inc	sp
	pop	hl
	add	sp, #4
	jp	(hl)
___str_80:
	.ascii "You already own one."
	.db 0x00
___str_81:
	.ascii "SELECT CLOSE"
	.db 0x00
___str_82:
	.ascii "Not enough credits tonight."
	.db 0x00
;src/main.c:1043: static void buy_ride(uint8_t level, uint8_t price, const char *title, const char *body) {
;	---------------------------------
; Function buy_ride
; ---------------------------------
_buy_ride:
	ld	c, a
	ld	b, e
;src/main.c:1044: if (ride_level >= level) {
	ld	a, (#_ride_level)
	sub	a, c
	jr	C, 00102$
;src/main.c:1045: show_message(title, "You already own that ride tier.", "SELECT CLOSE");
	ld	de, #___str_84
	push	de
	ld	bc, #___str_83
	ldhl	sp,	#4
	ld	a, (hl+)
	ld	e, a
	ld	d, (hl)
	call	_show_message
;src/main.c:1046: return;
	jr	00105$
00102$:
;src/main.c:1048: if (credits < price) {
	ld	a, (#_credits)
	sub	a, b
	jr	NC, 00104$
;src/main.c:1049: show_message(title, "Not enough credits tonight.", "SELECT CLOSE");
	ld	de, #___str_84
	push	de
	ld	bc, #___str_85
	ldhl	sp,	#4
	ld	a, (hl+)
	ld	e, a
	ld	d, (hl)
	call	_show_message
;src/main.c:1050: return;
	jr	00105$
00104$:
;src/main.c:1052: credits -= price;
	ld	hl, #_credits
	ld	a, (hl)
	sub	a, b
	ld	(hl), a
;src/main.c:1053: ride_level = level;
	ld	hl, #_ride_level
	ld	(hl), c
;src/main.c:1054: show_message(title, body, "SELECT CLOSE");
	ld	de, #___str_84
	push	de
	ldhl	sp,	#6
	ld	a, (hl+)
	ld	c, a
	ld	b, (hl)
	ldhl	sp,	#4
	ld	a, (hl+)
	ld	e, a
	ld	d, (hl)
	call	_show_message
00105$:
;src/main.c:1055: }
	pop	hl
	add	sp, #4
	jp	(hl)
___str_83:
	.ascii "You already own that ride tier."
	.db 0x00
___str_84:
	.ascii "SELECT CLOSE"
	.db 0x00
___str_85:
	.ascii "Not enough credits tonight."
	.db 0x00
;src/main.c:1057: static void vehicle_shop(void) {
;	---------------------------------
; Function vehicle_shop
; ---------------------------------
_vehicle_shop:
;src/main.c:1072: if (ride_level == RIDE_FOOT) {
	ld	a, (#_ride_level)
	or	a, a
	jr	NZ, 00104$
;src/main.c:1073: choice = menu_screen("WHEELS", "Used city bicycle", bike_items, 2u, 0u);
	ld	hl, #0x02
	push	hl
	ld	de, #_vehicle_shop_bike_items_10000_606
	push	de
	ld	bc, #___str_91
	ld	de, #___str_90
	call	_menu_screen
;src/main.c:1074: if (choice == 0u) {
	or	a, a
	ret	NZ
;src/main.c:1075: buy_ride(RIDE_BICYCLE, 18u, "GARAGE", "A tuned bicycle. The block feels smaller already.");
	ld	de, #___str_93
	push	de
	ld	de, #___str_92
	push	de
	ld	e, #0x12
	ld	a, #0x01
	call	_buy_ride
;src/main.c:1077: return;
	ret
00104$:
;src/main.c:1080: if (ride_level == RIDE_BICYCLE) {
	ld	a, (#_ride_level)
	dec	a
	jr	NZ, 00110$
;src/main.c:1081: if (media == 0u) {
	ld	a, (#_media)
	or	a, a
	jr	NZ, 00106$
;src/main.c:1082: show_message("GARAGE", "Bring back at least one tape.\nThen they'll trust you with a scooter.", "SELECT CLOSE");
	ld	de, #___str_95
	push	de
	ld	bc, #___str_94
	ld	de, #___str_92
	call	_show_message
;src/main.c:1083: return;
	ret
00106$:
;src/main.c:1085: choice = menu_screen("WHEELS", "Courier scooter", scooter_items, 2u, 0u);
	ld	hl, #0x02
	push	hl
	ld	de, #_vehicle_shop_scooter_items_10000_606
	push	de
	ld	bc, #___str_96
	ld	de, #___str_90
	call	_menu_screen
;src/main.c:1086: if (choice == 0u) {
	ld	c, a
	or	a, a
	ret	NZ
;src/main.c:1087: buy_ride(RIDE_SCOOTER, 34u, "GARAGE", "A soft electric scooter. Quicker starts, smoother glide.");
	ld	de, #___str_97
	push	de
	ld	de, #___str_92
	push	de
	ld	e, #0x22
	ld	a, #0x02
	call	_buy_ride
;src/main.c:1089: return;
	ret
00110$:
;src/main.c:1092: if (ride_level == RIDE_SCOOTER) {
	ld	a, (#_ride_level)
	sub	a, #0x02
	jr	NZ, 00116$
;src/main.c:1093: if ((media & (MEDIA_RAIN_LOOP | MEDIA_VENDING_DREAMS | MEDIA_ROOFTOP_SET)) !=
	ld	a, (_media)
	and	a, #0x07
	sub	a, #0x07
	jr	Z, 00112$
;src/main.c:1095: show_message("GARAGE", "Earn a little name on the block.\nThen the motorcycle comes out.", "SELECT CLOSE");
	ld	de, #___str_95
	push	de
	ld	bc, #___str_98
	ld	de, #___str_92
	call	_show_message
;src/main.c:1096: return;
	ret
00112$:
;src/main.c:1098: choice = menu_screen("WHEELS", "Street motorcycle", moto_items, 2u, 0u);
	ld	hl, #0x02
	push	hl
	ld	de, #_vehicle_shop_moto_items_10000_606
	push	de
	ld	bc, #___str_99
	ld	de, #___str_90
	call	_menu_screen
;src/main.c:1099: if (choice == 0u) {
	or	a, a
	ret	NZ
;src/main.c:1100: buy_ride(RIDE_MOTORCYCLE, 58u, "GARAGE", "A tiny motorcycle with a patient idle and a fast lane.");
	ld	de, #___str_100
	push	de
	ld	de, #___str_92
	push	de
	ld	e, #0x3a
	ld	a, #0x03
	call	_buy_ride
;src/main.c:1102: return;
	ret
00116$:
;src/main.c:1105: show_message("GARAGE", "Your motorcycle is already the best thing in the lot.", "SELECT CLOSE");
	ld	de, #___str_95
	push	de
	ld	bc, #___str_101
	ld	de, #___str_92
	call	_show_message
;src/main.c:1106: }
	ret
_vehicle_shop_bike_items_10000_606:
	.dw ___str_86
	.dw ___str_87
_vehicle_shop_scooter_items_10000_606:
	.dw ___str_88
	.dw ___str_87
_vehicle_shop_moto_items_10000_606:
	.dw ___str_89
	.dw ___str_87
___str_86:
	.ascii "Bicycle   18c"
	.db 0x00
___str_87:
	.ascii "Leave"
	.db 0x00
___str_88:
	.ascii "Scooter   34c"
	.db 0x00
___str_89:
	.ascii "Motorcycle 58c"
	.db 0x00
___str_90:
	.ascii "WHEELS"
	.db 0x00
___str_91:
	.ascii "Used city bicycle"
	.db 0x00
___str_92:
	.ascii "GARAGE"
	.db 0x00
___str_93:
	.ascii "A tuned bicycle. The block feels smaller already."
	.db 0x00
___str_94:
	.ascii "Bring back at least one tape."
	.db 0x0a
	.ascii "Then they'll trust you with a scooter."
	.db 0x00
___str_95:
	.ascii "SELECT CLOSE"
	.db 0x00
___str_96:
	.ascii "Courier scooter"
	.db 0x00
___str_97:
	.ascii "A soft electric scooter. Quicker starts, smoother glide."
	.db 0x00
___str_98:
	.ascii "Earn a little name on the block."
	.db 0x0a
	.ascii "Then the motorcycle comes out."
	.db 0x00
___str_99:
	.ascii "Street motorcycle"
	.db 0x00
___str_100:
	.ascii "A tiny motorcycle with a patient idle and a fast lane."
	.db 0x00
___str_101:
	.ascii "Your motorcycle is already the best thing in the lot."
	.db 0x00
;src/main.c:1108: static void mina_shop(void) {
;	---------------------------------
; Function mina_shop
; ---------------------------------
_mina_shop:
;src/main.c:1118: choice = menu_screen("NEEDLE & NEON", "Room pieces", items, 5u, 0u);
	ld	hl, #0x05
	push	hl
	ld	de, #_mina_shop_items_10000_624
	push	de
	ld	bc, #___str_108
	ld	de, #___str_107
	call	_menu_screen
;src/main.c:1119: if (choice == 0u) buy_item(ITEM_POSTER, 6u, "MINA", "A folded glow poster for your wall.");
	ld	c, a
	or	a, a
	jr	NZ, 00110$
	ld	de, #___str_110
	push	de
	ld	de, #___str_109
	push	de
	ld	e, #0x06
	ld	a, #0x08
	call	_buy_item
	ret
00110$:
;src/main.c:1120: else if (choice == 1u) buy_item(ITEM_LAMP, 9u, "MINA", "A lamp with a soft rainy bloom.");
	ld	a, c
	dec	a
	jr	NZ, 00107$
	ld	de, #___str_111
	push	de
	ld	de, #___str_109
	push	de
	ld	e, #0x09
	ld	a, #0x10
	call	_buy_item
	ret
00107$:
;src/main.c:1121: else if (choice == 2u) buy_item(ITEM_SHELF, 11u, "MINA", "Crate shelf, stickered and sturdy.");
	ld	a, c
	sub	a, #0x02
	jr	NZ, 00104$
	ld	de, #___str_112
	push	de
	ld	de, #___str_109
	push	de
	ld	e, #0x0b
	ld	a, #0x20
	call	_buy_item
	ret
00104$:
;src/main.c:1122: else if (choice == 3u) buy_item(ITEM_MINIDISC, 7u, "MINA", "One blank mini-disc in a foggy sleeve.");
	ld	a, c
	sub	a, #0x03
	ret	NZ
	ld	de, #___str_113
	push	de
	ld	de, #___str_109
	push	de
	ld	e, #0x07
	ld	a, #0x04
	call	_buy_item
;src/main.c:1123: }
	ret
_mina_shop_items_10000_624:
	.dw ___str_102
	.dw ___str_103
	.dw ___str_104
	.dw ___str_105
	.dw ___str_106
___str_102:
	.ascii "Glow Poster  6c"
	.db 0x00
___str_103:
	.ascii "Neon Lamp   9c"
	.db 0x00
___str_104:
	.ascii "Crate Shelf 11c"
	.db 0x00
___str_105:
	.ascii "Mini-Disc   7c"
	.db 0x00
___str_106:
	.ascii "Leave"
	.db 0x00
___str_107:
	.ascii "NEEDLE & NEON"
	.db 0x00
___str_108:
	.ascii "Room pieces"
	.db 0x00
___str_109:
	.ascii "MINA"
	.db 0x00
___str_110:
	.ascii "A folded glow poster for your wall."
	.db 0x00
___str_111:
	.ascii "A lamp with a soft rainy bloom."
	.db 0x00
___str_112:
	.ascii "Crate shelf, stickered and sturdy."
	.db 0x00
___str_113:
	.ascii "One blank mini-disc in a foggy sleeve."
	.db 0x00
;src/main.c:1125: static void rook_shop(void) {
;	---------------------------------
; Function rook_shop
; ---------------------------------
_rook_shop:
;src/main.c:1134: choice = menu_screen("ROOK REPAIR", "Bench specials", items, 4u, 0u);
	ld	hl, #0x04
	push	hl
	ld	de, #_rook_shop_items_10000_630
	push	de
	ld	bc, #___str_119
	ld	de, #___str_118
	call	_menu_screen
;src/main.c:1135: if (choice == 0u) buy_item(ITEM_SOFTPHONES, 8u, "ROOK", "Fresh softphones. Quiet cups, no hiss.");
	or	a, a
	jr	NZ, 00107$
	ld	de, #___str_121
	push	de
	ld	de, #___str_120
	push	de
	ld	e, #0x08
	ld	a, #0x01
	call	_buy_item
	ret
00107$:
;src/main.c:1136: else if (choice == 1u) buy_item(ITEM_DOCK, 14u, "ROOK", "A tiny dock with patient LEDs.");
	cp	a, #0x01
	jr	NZ, 00104$
	ld	de, #___str_122
	push	de
	ld	de, #___str_120
	push	de
	ld	e, #0x0e
	ld	a, #0x80
	call	_buy_item
	ret
00104$:
;src/main.c:1137: else if (choice == 2u) vehicle_shop();
	sub	a, #0x02
	jp	Z, _vehicle_shop
;src/main.c:1138: }
	ret
_rook_shop_items_10000_630:
	.dw ___str_114
	.dw ___str_115
	.dw ___str_116
	.dw ___str_117
___str_114:
	.ascii "Softphones  8c"
	.db 0x00
___str_115:
	.ascii "Pocket Dock 14c"
	.db 0x00
___str_116:
	.ascii "Tune Wheels"
	.db 0x00
___str_117:
	.ascii "Leave"
	.db 0x00
___str_118:
	.ascii "ROOK REPAIR"
	.db 0x00
___str_119:
	.ascii "Bench specials"
	.db 0x00
___str_120:
	.ascii "ROOK"
	.db 0x00
___str_121:
	.ascii "Fresh softphones. Quiet cups, no hiss."
	.db 0x00
___str_122:
	.ascii "A tiny dock with patient LEDs."
	.db 0x00
;src/main.c:1140: static void soma_shop(void) {
;	---------------------------------
; Function soma_shop
; ---------------------------------
_soma_shop:
;src/main.c:1148: choice = menu_screen("SOMA", "Steam counter", items, 3u, 0u);
	ld	hl, #0x03
	push	hl
	ld	de, #_soma_shop_items_10000_635
	push	de
	ld	bc, #___str_127
	ld	de, #___str_126
	call	_menu_screen
	ld	e, a
;src/main.c:1149: if (choice == 0u) buy_item(ITEM_BROTH, 4u, "SOMA", "One sealed broth cup for the walk home.");
	or	a, a
	jr	NZ, 00104$
	ld	de, #___str_128
	push	de
	ld	de, #___str_126
	push	de
	ld	e, #0x04
	ld	a, #0x02
	call	_buy_item
	ret
00104$:
;src/main.c:1150: else if (choice == 1u) buy_item(ITEM_TANK, 12u, "SOMA", "A tiny glowing tank with sleepy bubbles.");
	dec	e
	ret	NZ
	ld	de, #___str_129
	push	de
	ld	de, #___str_126
	push	de
	ld	e, #0x0c
	ld	a, #0x40
	call	_buy_item
;src/main.c:1151: }
	ret
_soma_shop_items_10000_635:
	.dw ___str_123
	.dw ___str_124
	.dw ___str_125
___str_123:
	.ascii "Night Broth 4c"
	.db 0x00
___str_124:
	.ascii "Fish Tank  12c"
	.db 0x00
___str_125:
	.ascii "Leave"
	.db 0x00
___str_126:
	.ascii "SOMA"
	.db 0x00
___str_127:
	.ascii "Steam counter"
	.db 0x00
___str_128:
	.ascii "One sealed broth cup for the walk home."
	.db 0x00
___str_129:
	.ascii "A tiny glowing tank with sleepy bubbles."
	.db 0x00
;src/main.c:1153: static void npc_dialogue(uint8_t npc_id) {
;	---------------------------------
; Function npc_dialogue
; ---------------------------------
_npc_dialogue:
	dec	sp
	dec	sp
	ld	e, a
;src/main.c:1154: const task_t *task = current_task();
	push	de
	call	_current_task
	pop	de
;src/main.c:1160: show_message("IONA", task->complete, "SELECT CLOSE");
	ld	hl, #0x0008
	add	hl, bc
	inc	sp
	inc	sp
	push	hl
;src/main.c:1156: if (npc_id == NPC_IONA) {
	ld	a, e
	sub	a, #0x03
	jr	NZ, 00108$
;src/main.c:1157: if ((task_id == TASK_MUSIC_DROP) && !task_done) {
	ld	a, (#_task_id)
	or	a, a
	jr	NZ, 00105$
	ld	a, (#_task_done)
	or	a, a
	jr	NZ, 00105$
;src/main.c:1158: if (has_item(ITEM_SOFTPHONES)) {
	push	bc
	ld	a, #0x01
	call	_has_item
	ld	e, a
	pop	bc
	ld	a, e
	or	a, a
	jr	Z, 00102$
;src/main.c:1159: reward_task(task);
	ld	e, c
	ld	d, b
	call	_reward_task
;src/main.c:1160: show_message("IONA", task->complete, "SELECT CLOSE");
	pop	de
	push	de
	ld	a, (de)
	ld	c, a
	inc	de
	ld	a, (de)
	ld	b, a
	ld	de, #___str_131
	push	de
	ld	de, #___str_130
	call	_show_message
	jp	00130$
00102$:
;src/main.c:1162: show_message("IONA", "My booth crackles tonight.\nRook still has softphones.", "SELECT CLOSE");
	ld	de, #___str_131
	push	de
	ld	bc, #___str_132
	ld	de, #___str_130
	call	_show_message
;src/main.c:1164: return;
	jp	00130$
00105$:
;src/main.c:1166: show_message("IONA", "When the block gets lonely,\nI leave synth-pop in the air.", "SELECT CLOSE");
	ld	de, #___str_131
	push	de
	ld	bc, #___str_133
	ld	de, #___str_130
	call	_show_message
;src/main.c:1167: return;
	jp	00130$
00108$:
;src/main.c:1170: if (npc_id == NPC_MINA) {
	ld	a, e
	or	a, a
	jr	NZ, 00116$
;src/main.c:1171: if ((task_id == TASK_SOUP_RUN) && !task_done) {
	ld	a, (#_task_id)
	dec	a
	jr	NZ, 00113$
	ld	a, (#_task_done)
	or	a, a
	jr	NZ, 00113$
;src/main.c:1172: if (has_item(ITEM_BROTH)) {
	push	bc
	ld	a, #0x02
	call	_has_item
	ld	e, a
	pop	bc
	ld	a, e
	or	a, a
	jr	Z, 00110$
;src/main.c:1173: reward_task(task);
	ld	e, c
	ld	d, b
	call	_reward_task
;src/main.c:1174: show_message("MINA", task->complete, "SELECT CLOSE");
	pop	de
	push	de
	ld	a, (de)
	ld	c, a
	inc	de
	ld	a, (de)
	ld	b, a
	ld	de, #___str_131
	push	de
	ld	de, #___str_134
	call	_show_message
	jp	00130$
00110$:
;src/main.c:1176: show_message("MINA", "I sorted imports all night.\nSoma still owes me dinner.", "SELECT CLOSE");
	ld	de, #___str_131
	push	de
	ld	bc, #___str_135
	ld	de, #___str_134
	call	_show_message
;src/main.c:1178: return;
	jp	00130$
00113$:
;src/main.c:1180: mina_shop();
	inc	sp
	inc	sp
	jp	_mina_shop
;src/main.c:1181: return;
	jr	00130$
00116$:
;src/main.c:1184: if (npc_id == NPC_ROOK) {
	ld	a, e
	dec	a
	jr	NZ, 00124$
;src/main.c:1185: if ((task_id == TASK_DISC_SWAP) && !task_done) {
	ld	a, (#_task_id)
	sub	a, #0x02
	jr	NZ, 00121$
	ld	a, (#_task_done)
	or	a, a
	jr	NZ, 00121$
;src/main.c:1186: if (has_item(ITEM_MINIDISC)) {
	push	bc
	ld	a, #0x04
	call	_has_item
	ld	e, a
	pop	bc
	ld	a, e
	or	a, a
	jr	Z, 00118$
;src/main.c:1187: reward_task(task);
	ld	e, c
	ld	d, b
	call	_reward_task
;src/main.c:1188: show_message("ROOK", task->complete, "SELECT CLOSE");
	pop	de
	push	de
	ld	a, (de)
	ld	c, a
	inc	de
	ld	a, (de)
	ld	b, a
	ld	de, #___str_131
	push	de
	ld	de, #___str_136
	call	_show_message
	jr	00130$
00118$:
;src/main.c:1190: show_message("ROOK", "Needle & Neon keeps blank mini-discs.\nI just need one tonight.", "SELECT CLOSE");
	ld	de, #___str_131
	push	de
	ld	bc, #___str_137
	ld	de, #___str_136
	call	_show_message
;src/main.c:1192: return;
	jr	00130$
00121$:
;src/main.c:1194: rook_shop();
	inc	sp
	inc	sp
	jp	_rook_shop
;src/main.c:1195: return;
	jr	00130$
00124$:
;src/main.c:1198: if (npc_id == NPC_SOMA) {
	ld	a, e
	sub	a, #0x02
	jr	NZ, 00126$
;src/main.c:1199: soma_shop();
	inc	sp
	inc	sp
	jp	_soma_shop
;src/main.c:1200: return;
	jr	00130$
00126$:
;src/main.c:1203: if (!has_item(ITEM_POSTER)) {
	ld	a, #0x08
	call	_has_item
	or	a, a
	jr	NZ, 00128$
;src/main.c:1204: give_item_message("PIX", "Welcome to Luma Lane.\nTake this glow poster for your wall.", ITEM_POSTER);
	ld	a, #0x08
	push	af
	inc	sp
	ld	bc, #___str_139
	ld	de, #___str_138
	call	_give_item_message
	jr	00130$
00128$:
;src/main.c:1206: show_message("PIX", "Tiny room, good rain, records nearby.\nYou're settling in fine.", "SELECT CLOSE");
	ld	de, #___str_131
	push	de
	ld	bc, #___str_140
	ld	de, #___str_138
	call	_show_message
00130$:
;src/main.c:1208: }
	inc	sp
	inc	sp
	ret
___str_130:
	.ascii "IONA"
	.db 0x00
___str_131:
	.ascii "SELECT CLOSE"
	.db 0x00
___str_132:
	.ascii "My booth crackles tonight."
	.db 0x0a
	.ascii "Rook still has softphones."
	.db 0x00
___str_133:
	.ascii "When the block gets lonely,"
	.db 0x0a
	.ascii "I leave synth-pop in the air."
	.db 0x00
___str_134:
	.ascii "MINA"
	.db 0x00
___str_135:
	.ascii "I sorted imports all night."
	.db 0x0a
	.ascii "Soma still owes me dinner."
	.db 0x00
___str_136:
	.ascii "ROOK"
	.db 0x00
___str_137:
	.ascii "Needle & Neon keeps blank mini-discs."
	.db 0x0a
	.ascii "I just need one tonight."
	.db 0x00
___str_138:
	.ascii "PIX"
	.db 0x00
___str_139:
	.ascii "Welcome to Luma Lane."
	.db 0x0a
	.ascii "Take this glow poster for your wall."
	.db 0x00
___str_140:
	.ascii "Tiny room, good rain, records nearby."
	.db 0x0a
	.ascii "You're settling in fine."
	.db 0x00
;src/main.c:1210: static uint8_t npc_at(uint8_t x, uint8_t y, uint8_t *npc_id) {
;	---------------------------------
; Function npc_at
; ---------------------------------
_npc_at:
	add	sp, #-4
	ldhl	sp,	#2
	ld	(hl-), a
;src/main.c:1213: for (i = 0u; i != NPC_COUNT; ++i) {
	ld	a, e
	ld	(hl-), a
	ld	(hl), #0x00
	ldhl	sp,	#3
	ld	(hl), #0x00
00106$:
;src/main.c:1214: if ((npcs[i].room_id == room_id) && (npcs[i].x == x) && (npcs[i].y == y)) {
	ldhl	sp,	#3
	ld	c, (hl)
	ld	b, #0x00
	ld	l, c
	ld	h, b
	add	hl, hl
	add	hl, bc
	add	hl, hl
	ld	bc, #_npcs
	add	hl, bc
	ld	c, l
	ld	b, h
	ld	a, (bc)
	ld	hl, #_room_id
	sub	a, (hl)
	jr	NZ, 00107$
	ld	l, c
	ld	h, b
	inc	hl
	ld	e, (hl)
	ldhl	sp,	#2
	ld	a, (hl)
	sub	a, e
	jr	NZ, 00107$
	inc	bc
	inc	bc
	ld	a, (bc)
	ld	c, a
	ldhl	sp,	#1
	ld	a, (hl)
	sub	a, c
	jr	NZ, 00107$
;src/main.c:1215: *npc_id = i;
	ldhl	sp,	#6
	ld	a, (hl+)
	ld	c, a
	ld	b, (hl)
	ldhl	sp,	#0
	ld	a, (hl)
	ld	(bc), a
;src/main.c:1216: return 1u;
	ld	a, #0x01
	jr	00108$
00107$:
;src/main.c:1213: for (i = 0u; i != NPC_COUNT; ++i) {
	ldhl	sp,	#3
	inc	(hl)
	ld	a, (hl)
	ldhl	sp,	#0
	ld	(hl), a
	ldhl	sp,	#3
	ld	a, (hl)
	sub	a, #0x05
	jr	NZ, 00106$
;src/main.c:1219: return 0u;
	xor	a, a
00108$:
;src/main.c:1220: }
	add	sp, #4
	pop	hl
	pop	bc
	jp	(hl)
;src/main.c:1222: static uint8_t npc_blocks(uint8_t x, uint8_t y) {
;	---------------------------------
; Function npc_blocks
; ---------------------------------
_npc_blocks:
	add	sp, #-3
	ldhl	sp,	#1
	ld	(hl-), a
;src/main.c:1225: for (i = 0u; i != NPC_COUNT; ++i) {
	ld	a, e
	ld	(hl+), a
	inc	hl
	ld	(hl), #0x00
00106$:
;src/main.c:1226: if ((npcs[i].room_id == room_id) && (npcs[i].x == x) && (npcs[i].y == y)) {
	ldhl	sp,	#2
	ld	c, (hl)
	ld	b, #0x00
	ld	l, c
	ld	h, b
	add	hl, hl
	add	hl, bc
	add	hl, hl
	ld	bc, #_npcs
	add	hl, bc
	ld	c, l
	ld	b, h
	ld	a, (bc)
	ld	hl, #_room_id
	sub	a, (hl)
	jr	NZ, 00107$
	ld	l, c
	ld	h, b
	inc	hl
	ld	e, (hl)
	ldhl	sp,	#1
	ld	a, (hl)
	sub	a, e
	jr	NZ, 00107$
	inc	bc
	inc	bc
	ld	a, (bc)
	ld	c, a
	ldhl	sp,	#0
	ld	a, (hl)
	sub	a, c
	jr	NZ, 00107$
;src/main.c:1227: return 1u;
	ld	a, #0x01
	jr	00108$
00107$:
;src/main.c:1225: for (i = 0u; i != NPC_COUNT; ++i) {
	ldhl	sp,	#2
	inc	(hl)
	ld	a, (hl)
	sub	a, #0x05
	jr	NZ, 00106$
;src/main.c:1230: return 0u;
	xor	a, a
00108$:
;src/main.c:1231: }
	add	sp, #3
	ret
;src/main.c:1233: static uint8_t hotspot_at(uint8_t x, uint8_t y, uint8_t *hotspot_id) {
;	---------------------------------
; Function hotspot_at
; ---------------------------------
_hotspot_at:
	dec	sp
	dec	sp
	ldhl	sp,	#1
	ld	(hl-), a
	ld	(hl), e
;src/main.c:1236: for (i = 0u; i != (sizeof(hotspots) / sizeof(hotspots[0])); ++i) {
	ld	c, #0x00
00106$:
;src/main.c:1237: if ((hotspots[i].room_id == room_id) && (hotspots[i].x == x) && (hotspots[i].y == y)) {
	ld	e, c
	ld	d, #0x00
	ld	l, e
	ld	h, d
	add	hl, hl
	add	hl, hl
	ld	a, #<(_hotspots)
	add	a, l
	ld	e, a
	ld	a, #>(_hotspots)
	adc	a, h
	ld	d, a
	ld	a, (de)
	ld	hl, #_room_id
	sub	a, (hl)
	jr	NZ, 00107$
	ld	l, e
	ld	h, d
	inc	hl
	ld	b, (hl)
	ldhl	sp,	#1
	ld	a, (hl)
	sub	a, b
	jr	NZ, 00107$
	ld	l, e
	ld	h, d
	inc	hl
	inc	hl
	ld	b, (hl)
	ldhl	sp,	#0
	ld	a, (hl)
	sub	a, b
	jr	NZ, 00107$
;src/main.c:1238: *hotspot_id = hotspots[i].id;
	ldhl	sp,	#4
	ld	a, (hl+)
	ld	c, a
	ld	b, (hl)
	inc	de
	inc	de
	inc	de
	ld	a, (de)
	ld	(bc), a
;src/main.c:1239: return 1u;
	ld	a, #0x01
	jr	00108$
00107$:
;src/main.c:1236: for (i = 0u; i != (sizeof(hotspots) / sizeof(hotspots[0])); ++i) {
	inc	c
	ld	a, c
	sub	a, #0x06
	jr	NZ, 00106$
;src/main.c:1242: return 0u;
	xor	a, a
00108$:
;src/main.c:1243: }
	inc	sp
	inc	sp
	pop	hl
	pop	bc
	jp	(hl)
;src/main.c:1245: static void room_fallback_position(uint8_t *x, uint8_t *y) {
;	---------------------------------
; Function room_fallback_position
; ---------------------------------
_room_fallback_position:
;src/main.c:1246: switch (room_id) {
	ld	a, #0x06
	ld	hl, #_room_id
	sub	a, (hl)
	jr	C, 00108$
	push	de
	ld	e, (hl)
	ld	d, #0x00
	ld	hl, #00119$
	add	hl, de
	add	hl, de
	ld	e, (hl)
	inc	hl
	ld	h, (hl)
	ld	l, e
	pop	de
	jp	(hl)
00119$:
	.dw	00108$
	.dw	00101$
	.dw	00102$
	.dw	00103$
	.dw	00104$
	.dw	00105$
	.dw	00106$
;src/main.c:1247: case ROOM_APARTMENT:
00101$:
;src/main.c:1248: *x = 16u;
	ld	a, #0x10
	ld	(de), a
;src/main.c:1249: *y = 14u;
	ld	a, #0x0e
	ld	(bc), a
;src/main.c:1250: break;
	ret
;src/main.c:1251: case ROOM_RECORDS:
00102$:
;src/main.c:1252: *x = 9u;
	ld	a, #0x09
	ld	(de), a
;src/main.c:1253: *y = 14u;
	ld	a, #0x0e
	ld	(bc), a
;src/main.c:1254: break;
	ret
;src/main.c:1255: case ROOM_TECH:
00103$:
;src/main.c:1256: *x = 9u;
	ld	a, #0x09
	ld	(de), a
;src/main.c:1257: *y = 14u;
	ld	a, #0x0e
	ld	(bc), a
;src/main.c:1258: break;
	ret
;src/main.c:1259: case ROOM_RAMEN:
00104$:
;src/main.c:1260: *x = 9u;
	ld	a, #0x09
	ld	(de), a
;src/main.c:1261: *y = 14u;
	ld	a, #0x0e
	ld	(bc), a
;src/main.c:1262: break;
	ret
;src/main.c:1263: case ROOM_ROOFTOP:
00105$:
;src/main.c:1264: *x = 10u;
	ld	a, #0x0a
	ld	(de), a
;src/main.c:1265: *y = 14u;
	ld	a, #0x0e
	ld	(bc), a
;src/main.c:1266: break;
	ret
;src/main.c:1267: case ROOM_SOUTHLINE:
00106$:
;src/main.c:1268: *x = 9u;
	ld	a, #0x09
	ld	(de), a
;src/main.c:1269: *y = 2u;
	ld	a, #0x02
	ld	(bc), a
;src/main.c:1270: break;
	ret
;src/main.c:1272: default:
00108$:
;src/main.c:1273: *x = 10u;
	ld	a, #0x0a
	ld	(de), a
;src/main.c:1274: *y = 12u;
	ld	a, #0x0c
	ld	(bc), a
;src/main.c:1276: }
;src/main.c:1277: }
	ret
;src/main.c:1279: static uint8_t position_is_walkable(uint8_t x, uint8_t y) {
;	---------------------------------
; Function position_is_walkable
; ---------------------------------
_position_is_walkable:
	ld	c, a
	ld	b, e
;src/main.c:1282: if ((x >= MAP_W) || (y >= MAP_H)) {
	ld	a, c
	sub	a, #0x14
	jr	NC, 00101$
	ld	a, b
	sub	a, #0x12
	jr	C, 00102$
00101$:
;src/main.c:1283: return 0u;
	xor	a, a
	ret
00102$:
;src/main.c:1286: tile = map_buffer[(uint16_t)y * MAP_W + x];
	ld	l, b
	ld	h, #0x00
	ld	e, l
	ld	d, h
	add	hl, hl
	add	hl, hl
	add	hl, de
	add	hl, hl
	add	hl, hl
	ld	e, c
	ld	d, #0x00
	add	hl, de
	ld	de, #_map_buffer
	add	hl, de
	ld	a, (hl)
;src/main.c:1287: return (uint8_t)(!is_solid(tile) && !npc_blocks(x, y));
	push	bc
	call	_is_solid
	pop	bc
	or	a, a
	jr	NZ, 00106$
	ld	e, b
	ld	a, c
	call	_npc_blocks
	or	a, a
	jr	Z, 00107$
00106$:
	xor	a, a
	ret
00107$:
	ld	a, #0x01
;src/main.c:1288: }
	ret
;src/main.c:1290: static void sanitize_player_position(void) {
;	---------------------------------
; Function sanitize_player_position
; ---------------------------------
_sanitize_player_position:
	dec	sp
	dec	sp
;src/main.c:1296: if (position_is_walkable(player_x, player_y)) {
	ld	a, (_player_y)
	ld	e, a
	ld	a, (_player_x)
	call	_position_is_walkable
	ld	c, a
	or	a, a
;src/main.c:1297: return;
	jr	NZ, 00113$
;src/main.c:1300: room_fallback_position(&fallback_x, &fallback_y);
	ldhl	sp,	#1
	ld	c, l
	ld	b, h
	ldhl	sp,	#0
	ld	e, l
	ld	d, h
	call	_room_fallback_position
;src/main.c:1301: if (position_is_walkable(fallback_x, fallback_y)) {
	ldhl	sp,	#1
	ld	a, (hl-)
	ld	e, a
	ld	a, (hl)
	call	_position_is_walkable
	or	a, a
	jr	Z, 00104$
;src/main.c:1302: player_x = fallback_x;
	ldhl	sp,	#0
	ld	a, (hl)
	ld	(#_player_x),a
;src/main.c:1303: player_y = fallback_y;
	ldhl	sp,	#1
	ld	a, (hl)
	ld	(#_player_y),a
;src/main.c:1304: return;
	jr	00113$
00104$:
;src/main.c:1307: for (y = 1u; y != (MAP_H - 1u); ++y) {
	ld	c, #0x01
	ld	e, c
00111$:
;src/main.c:1308: for (x = 1u; x != (MAP_W - 1u); ++x) {
	ld	b, #0x01
	ld	d, b
00109$:
;src/main.c:1309: if (position_is_walkable(x, y)) {
	push	bc
	push	de
	ld	a, d
	call	_position_is_walkable
	pop	de
	pop	bc
	or	a, a
	jr	Z, 00110$
;src/main.c:1310: player_x = x;
	ld	hl, #_player_x
	ld	(hl), b
;src/main.c:1311: player_y = y;
	ld	hl, #_player_y
	ld	(hl), c
;src/main.c:1312: return;
	jr	00113$
00110$:
;src/main.c:1308: for (x = 1u; x != (MAP_W - 1u); ++x) {
	inc	d
	ld	a,d
	ld	b,a
	sub	a, #0x13
	jr	NZ, 00109$
;src/main.c:1307: for (y = 1u; y != (MAP_H - 1u); ++y) {
	inc	e
	ld	a,e
	ld	c,a
	sub	a, #0x11
	jr	NZ, 00111$
;src/main.c:1317: player_x = 1u;
	ld	hl, #_player_x
	ld	(hl), #0x01
;src/main.c:1318: player_y = 1u;
	ld	hl, #_player_y
	ld	(hl), #0x01
00113$:
;src/main.c:1319: }
	inc	sp
	inc	sp
	ret
;src/main.c:1321: static uint8_t is_solid(uint8_t tile) {
;	---------------------------------
; Function is_solid
; ---------------------------------
_is_solid:
;src/main.c:1322: return (tile == 3u) || (tile == 4u) || (tile == 6u) || (tile == 8u) ||
	cp	a, #0x03
	jr	Z, 00104$
	cp	a, #0x04
	jr	Z, 00104$
	cp	a, #0x06
	jr	Z, 00104$
	cp	a, #0x08
	jr	Z, 00104$
;src/main.c:1323: (tile == 9u) || (tile == 10u) || (tile == 11u) || (tile == 12u) ||
	cp	a, #0x09
	jr	Z, 00104$
	cp	a, #0x0a
	jr	Z, 00104$
	cp	a, #0x0b
	jr	Z, 00104$
	cp	a, #0x0c
	jr	Z, 00104$
;src/main.c:1324: (tile == 13u) || (tile == 14u) || (tile == 15u) || (tile == 16u) ||
	cp	a, #0x0d
	jr	Z, 00104$
	cp	a, #0x0e
	jr	Z, 00104$
	cp	a, #0x0f
	jr	Z, 00104$
	cp	a, #0x10
	jr	Z, 00104$
;src/main.c:1325: (tile == 17u) || (tile == 18u) || (tile == 19u) || (tile == 20u) ||
	cp	a, #0x11
	jr	Z, 00104$
	cp	a, #0x12
	jr	Z, 00104$
	cp	a, #0x13
	jr	Z, 00104$
	cp	a, #0x14
	jr	Z, 00104$
;src/main.c:1326: (tile == 21u) || (tile == 22u) || (tile == 23u) || (tile == 24u);
	cp	a, #0x15
	jr	Z, 00104$
	cp	a, #0x16
	jr	Z, 00104$
	cp	a, #0x17
	jr	Z, 00104$
	sub	a, #0x18
	jr	Z, 00104$
	xor	a, a
	ret
00104$:
	ld	a, #0x01
;src/main.c:1327: }
	ret
;src/main.c:1329: static void enter_room(uint8_t new_room, uint8_t new_x, uint8_t new_y) {
;	---------------------------------
; Function enter_room
; ---------------------------------
_enter_room:
	ld	(#_room_id),a
	ld	hl, #_player_x
	ld	(hl), e
;src/main.c:1332: player_y = new_y;
	ldhl	sp,	#2
	ld	a, (hl)
	ld	(#_player_y),a
;src/main.c:1333: dt_audio_play_sfx(SFX_DOOR);
	ld	a, #0x03
	push	af
	inc	sp
	ld	e, #b_dt_audio_play_sfx
	ld	hl, #_dt_audio_play_sfx
	call	___sdcc_bcall_ehl
	inc	sp
;src/main.c:1334: load_room();
	call	_load_room
;src/main.c:1335: render_room();
	call	_render_room
;src/main.c:1336: }
	pop	hl
	inc	sp
	jp	(hl)
;src/main.c:1338: static void check_door(void) {
;	---------------------------------
; Function check_door
; ---------------------------------
_check_door:
;src/main.c:1341: for (i = 0u; i != (sizeof(doors) / sizeof(doors[0])); ++i) {
	ld	c, #0x00
00106$:
;src/main.c:1342: if ((doors[i].room_id == room_id) &&
	ld	b, #0x00
	ld	l, c
	ld	h, b
	add	hl, hl
	add	hl, bc
	add	hl, hl
	ld	de, #_doors
	add	hl, de
	ld	a, (hl)
	ld	hl, #_room_id
	sub	a, (hl)
	jr	NZ, 00107$
;src/main.c:1343: (doors[i].x == player_x) &&
	inc	hl
	ld	e, l
	ld	d, h
	ld	a, (de)
	ld	hl, #_player_x
	sub	a, (hl)
	jr	NZ, 00107$
;src/main.c:1344: (doors[i].y == player_y)) {
	inc	hl
	inc	hl
	ld	e, l
	ld	d, h
	ld	a, (de)
	ld	hl, #_player_y
	sub	a, (hl)
	jr	NZ, 00107$
;src/main.c:1345: enter_room(doors[i].target_room, doors[i].target_x, doors[i].target_y);
	ld	a, l
	add	a, #0x05
	ld	c, a
	ld	a, h
	adc	a, #0x00
	ld	b, a
	ld	a, (bc)
	ld	b, a
	ld	a, l
	add	a, #0x04
	ld	e, a
	ld	a, h
	inc	hl
	inc	hl
	inc	hl
	adc	a, #0x00
	ld	d, a
	ld	a, (de)
	ld	e, a
	ld	a, (hl)
	push	bc
	inc	sp
	call	_enter_room
;src/main.c:1346: return;
	ret
00107$:
;src/main.c:1341: for (i = 0u; i != (sizeof(doors) / sizeof(doors[0])); ++i) {
	inc	c
	ld	a, c
	sub	a, #0x0c
	jr	NZ, 00106$
;src/main.c:1349: }
	ret
;src/main.c:1351: static void animate_step(int8_t dx, int8_t dy) {
;	---------------------------------
; Function animate_step
; ---------------------------------
_animate_step:
	add	sp, #-7
	ldhl	sp,	#5
	ld	(hl-), a
	ld	(hl), e
;src/main.c:1353: uint8_t frames = movement_step_frames();
	call	_movement_step_frames
	ldhl	sp,	#0
	ld	(hl), a
;src/main.c:1354: uint8_t start_x = tile_to_sprite_x(player_x);
	ld	a, (_player_x)
	call	_tile_to_sprite_x
	ldhl	sp,	#1
	ld	(hl), a
;src/main.c:1355: uint8_t start_y = tile_to_sprite_y(player_y);
	ld	a, (_player_y)
	call	_tile_to_sprite_y
	ldhl	sp,	#2
	ld	(hl), a
;src/main.c:1359: for (step = 1u; step <= frames; ++step) {
	ldhl	sp,	#6
	ld	(hl), #0x01
00103$:
	ldhl	sp,	#0
	ld	a, (hl)
	ldhl	sp,	#6
	sub	a, (hl)
	jr	C, 00105$
;src/main.c:1360: walk_phase = (uint8_t)(step & 1u);
	ld	a, (hl)
	and	a, #0x01
	ldhl	sp,	#3
	ld	(hl), a
;src/main.c:1361: pixel = (uint8_t)(((uint16_t)step * 8u) / frames);
	ldhl	sp,	#6
	ld	e, (hl)
	xor	a, a
	sla	e
	adc	a, a
	sla	e
	adc	a, a
	sla	e
	adc	a, a
	ldhl	sp,	#0
	ld	c, (hl)
	ld	b, #0x00
	ld	d, a
	call	__divuint
	ld	e, c
;src/main.c:1364: (uint8_t)(start_y + dy * pixel),
	push	de
	ldhl	sp,	#6
	ld	a, (hl)
	call	__muluschar
	pop	de
	ldhl	sp,	#2
	ld	a, (hl)
	add	a, c
	ld	c, a
;src/main.c:1363: (uint8_t)(start_x + dx * pixel),
	push	bc
	ldhl	sp,	#7
	ld	a, (hl)
	call	__muluschar
	ld	a, c
	pop	bc
	ldhl	sp,	#1
	ld	b, (hl)
	inc	hl
	inc	hl
	add	a, b
	ld	h, (hl)
	push	hl
	inc	sp
	ld	e, c
	call	_draw_world_sprites
;src/main.c:1367: vsync();
	call	_vsync
;src/main.c:1368: dt_audio_update();
	ld	e, #b_dt_audio_update
	ld	hl, #_dt_audio_update
	call	___sdcc_bcall_ehl
;src/main.c:1369: ++frame_clock;
	ld	hl, #_frame_clock
	inc	(hl)
;src/main.c:1359: for (step = 1u; step <= frames; ++step) {
	ldhl	sp,	#6
	inc	(hl)
	jr	00103$
00105$:
;src/main.c:1371: }
	add	sp, #7
	ret
;src/main.c:1373: static void try_move(int8_t dx, int8_t dy, uint8_t new_facing) {
;	---------------------------------
; Function try_move
; ---------------------------------
_try_move:
	dec	sp
	dec	sp
	ldhl	sp,	#1
	ld	(hl-), a
	ld	(hl), e
;src/main.c:1378: facing = new_facing;
	ldhl	sp,	#4
	ld	a, (hl)
	ld	(#_facing),a
;src/main.c:1379: nx = (int8_t)player_x + dx;
	ld	a, (_player_x)
	ldhl	sp,	#1
	add	a, (hl)
;src/main.c:1380: ny = (int8_t)player_y + dy;
	dec	hl
	ld	c, a
	ld	a, (_player_y)
	add	a, (hl)
	ld	b, a
;src/main.c:1381: if (nx < 0 || ny < 0 || nx >= MAP_W || ny >= MAP_H) {
	bit	7, c
	jr	NZ, 00101$
	bit	7, b
	jr	NZ, 00101$
	ld	a, c
	xor	a, #0x80
	sub	a, #0x94
	jr	NC, 00101$
	ld	a, b
	xor	a, #0x80
	sub	a, #0x92
	jr	C, 00102$
00101$:
;src/main.c:1382: draw_world_sprites(tile_to_sprite_x(player_x), tile_to_sprite_y(player_y), 0u);
	ld	a, (_player_y)
	call	_tile_to_sprite_y
	ld	e, a
	push	de
	ld	a, (_player_x)
	call	_tile_to_sprite_x
	pop	de
	ld	h, #0x00
	push	hl
	inc	sp
	call	_draw_world_sprites
;src/main.c:1383: return;
	jr	00109$
00102$:
;src/main.c:1386: tile = map_buffer[(uint16_t)ny * MAP_W + (uint8_t)nx];
	ld	l, b
	ld	h, #0x00
	ld	e, l
	ld	d, h
	add	hl, hl
	add	hl, hl
	add	hl, de
	add	hl, hl
	add	hl, hl
	ld	e, c
	ld	d, #0x00
	add	hl, de
	ld	de, #_map_buffer
	add	hl, de
	ld	a, (hl)
;src/main.c:1387: if (is_solid(tile) || npc_blocks((uint8_t)nx, (uint8_t)ny)) {
	push	bc
	call	_is_solid
	pop	bc
	or	a, a
	jr	NZ, 00106$
	push	bc
	ld	e, b
	ld	a, c
	call	_npc_blocks
	pop	bc
	or	a, a
	jr	Z, 00107$
00106$:
;src/main.c:1388: draw_world_sprites(tile_to_sprite_x(player_x), tile_to_sprite_y(player_y), 0u);
	ld	a, (_player_y)
	call	_tile_to_sprite_y
	ld	e, a
	push	de
	ld	a, (_player_x)
	call	_tile_to_sprite_x
	pop	de
	ld	h, #0x00
	push	hl
	inc	sp
	call	_draw_world_sprites
;src/main.c:1389: return;
	jr	00109$
00107$:
;src/main.c:1392: animate_step(dx, dy);
	push	bc
	ldhl	sp,	#2
	ld	a, (hl+)
	ld	e, a
	ld	a, (hl)
	call	_animate_step
	pop	bc
;src/main.c:1393: player_x = (uint8_t)nx;
	ld	hl, #_player_x
	ld	(hl), c
;src/main.c:1394: player_y = (uint8_t)ny;
	ld	hl, #_player_y
	ld	(hl), b
;src/main.c:1395: check_door();
	call	_check_door
;src/main.c:1396: draw_world_sprites(tile_to_sprite_x(player_x), tile_to_sprite_y(player_y), 0u);
	ld	a, (_player_y)
	call	_tile_to_sprite_y
	ld	e, a
	push	de
	ld	a, (_player_x)
	call	_tile_to_sprite_x
	pop	de
	ld	h, #0x00
	push	hl
	inc	sp
	call	_draw_world_sprites
00109$:
;src/main.c:1397: }
	inc	sp
	inc	sp
	pop	hl
	inc	sp
	jp	(hl)
;src/main.c:1399: static void interact_hotspot(uint8_t hotspot_id) {
;	---------------------------------
; Function interact_hotspot
; ---------------------------------
_interact_hotspot:
;src/main.c:1400: if (hotspot_id == HOTSPOT_BED) {
	cp	a, #0x01
	jr	NZ, 00105$
;src/main.c:1401: if (task_done) {
	ld	a, (#_task_done)
	or	a, a
	jr	Z, 00102$
;src/main.c:1402: ++day_count;
	ld	hl, #_day_count
	inc	(hl)
;src/main.c:1403: assign_daily_task();
	call	_assign_daily_task
;src/main.c:1404: show_message("ROOM", "You sleep under CRT glow.\nA new favor waits tomorrow.", "SELECT CLOSE");
	ld	de, #___str_143
	push	de
	ld	bc, #___str_142
	ld	de, #___str_141
	call	_show_message
	ret
00102$:
;src/main.c:1406: show_message("ROOM", "The bed looks perfect, but the block still needs one thing tonight.", "SELECT CLOSE");
	ld	de, #___str_143
	push	de
	ld	bc, #___str_144
	ld	de, #___str_141
	call	_show_message
;src/main.c:1408: return;
	ret
00105$:
;src/main.c:1411: if (hotspot_id == HOTSPOT_DECOR) {
	cp	a, #0x02
	jr	NZ, 00107$
;src/main.c:1412: decor_screen();
	call	_decor_screen
;src/main.c:1413: load_room();
	call	_load_room
;src/main.c:1414: render_room();
;src/main.c:1415: return;
	jp	_render_room
00107$:
;src/main.c:1418: if (hotspot_id == HOTSPOT_GARAGE) {
	sub	a, #0x04
	jr	NZ, 00109$
;src/main.c:1419: vehicle_shop();
	call	_vehicle_shop
;src/main.c:1420: load_room();
	call	_load_room
;src/main.c:1421: render_room();
;src/main.c:1422: return;
	jp	_render_room
00109$:
;src/main.c:1425: dt_home_record_menu(media, room_id);
	ld	a, (_room_id)
	ld	h, a
	ld	a, (_media)
	ld	l, a
	push	hl
	ld	e, #b_dt_home_record_menu
	ld	hl, #_dt_home_record_menu
	call	___sdcc_bcall_ehl
	pop	hl
;src/main.c:1426: load_room();
	call	_load_room
;src/main.c:1427: render_room();
;src/main.c:1428: }
	jp	_render_room
___str_141:
	.ascii "ROOM"
	.db 0x00
___str_142:
	.ascii "You sleep under CRT glow."
	.db 0x0a
	.ascii "A new favor waits tomorrow."
	.db 0x00
___str_143:
	.ascii "SELECT CLOSE"
	.db 0x00
___str_144:
	.ascii "The bed looks perfect, but the block still needs one thing t"
	.ascii "onight."
	.db 0x00
;src/main.c:1430: static void try_interact(void) {
;	---------------------------------
; Function try_interact
; ---------------------------------
_try_interact:
	dec	sp
	dec	sp
;src/main.c:1431: int8_t tx = player_x;
	ld	a, (_player_x)
	ld	b, a
;src/main.c:1432: int8_t ty = player_y;
	ld	a, (_player_y)
	ld	c, a
;src/main.c:1436: if (facing == DIR_UP) ty -= 1;
	ld	a, (#_facing)
	dec	a
	jr	NZ, 00108$
	dec	c
	jr	00109$
00108$:
;src/main.c:1437: else if (facing == DIR_DOWN) ty += 1;
	ld	a, (#_facing)
	or	a, a
	jr	NZ, 00105$
	inc	c
	jr	00109$
00105$:
;src/main.c:1438: else if (facing == DIR_LEFT) tx -= 1;
	ld	a, (#_facing)
	sub	a, #0x02
	jr	NZ, 00102$
	dec	b
	jr	00109$
00102$:
;src/main.c:1439: else tx += 1;
	inc	b
00109$:
;src/main.c:1441: if (tx < 0 || ty < 0 || tx >= MAP_W || ty >= MAP_H) {
	bit	7, b
	jp	NZ, 00159$
	bit	7, c
	jp	NZ, 00159$
	ld	a, b
	xor	a, #0x80
	sub	a, #0x94
	jp	NC, 00159$
	ld	a, c
	xor	a, #0x80
	sub	a, #0x92
;src/main.c:1442: return;
	jp	NC, 00159$
;src/main.c:1445: if (npc_at((uint8_t)tx, (uint8_t)ty, &npc_id)) {
	ld	hl, #0
	add	hl, sp
	ld	e, c
	ld	d, b
	push	bc
	push	de
	push	hl
	ld	a, d
	call	_npc_at
	pop	de
	pop	bc
	or	a, a
	jr	Z, 00116$
;src/main.c:1446: npc_dialogue(npc_id);
	ldhl	sp,	#0
	ld	a, (hl)
	call	_npc_dialogue
;src/main.c:1447: return;
	jp	00159$
00116$:
;src/main.c:1450: if (hotspot_at((uint8_t)tx, (uint8_t)ty, &hotspot_id)) {
	push	bc
	ld	hl, #3
	add	hl, sp
	push	hl
	ld	a, d
	call	_hotspot_at
	pop	bc
	or	a, a
	jr	Z, 00118$
;src/main.c:1451: interact_hotspot(hotspot_id);
	ldhl	sp,	#1
	ld	a, (hl)
	call	_interact_hotspot
;src/main.c:1452: return;
	jp	00159$
00118$:
;src/main.c:1455: if ((room_id == ROOM_DISTRICT) && (tx == 13) && (ty == 15)) {
	ld	a, b
	sub	a, #0x0d
	ld	a, #0x01
	jr	Z, 00432$
	xor	a, a
00432$:
	ld	d, a
	ld	a, c
	sub	a, #0x0f
	ld	a, #0x01
	jr	Z, 00434$
	xor	a, a
00434$:
	ld	e, a
	ld	a, (#_room_id)
	or	a, a
	jr	NZ, 00120$
	or	a, d
	jr	Z, 00120$
	ld	a, e
	or	a, a
	jr	Z, 00120$
;src/main.c:1456: show_message("TERMINAL", "Delays, karaoke specials,\nand a pirate radio station.", "SELECT CLOSE");
	ld	de, #___str_147
	push	de
	ld	bc, #___str_146
	ld	de, #___str_145
	call	_show_message
;src/main.c:1457: return;
	jp	00159$
00120$:
;src/main.c:1460: if ((room_id == ROOM_DISTRICT) && (tx == 8) && (ty == 13)) {
	ld	a, (#_room_id)
	or	a, a
	jr	NZ, 00124$
	ld	a, b
	sub	a, #0x08
	jr	NZ, 00124$
	ld	a, c
	sub	a, #0x0d
	jr	NZ, 00124$
;src/main.c:1461: show_message("STALL", "A cart sells synth carts,\nbatteries, and stickers.", "SELECT CLOSE");
	ld	de, #___str_147
	push	de
	ld	bc, #___str_149
	ld	de, #___str_148
	call	_show_message
;src/main.c:1462: return;
	jp	00159$
00124$:
;src/main.c:1465: if ((room_id == ROOM_APARTMENT) && (tx == 13) && (ty == 9)) {
	ld	hl, #_room_id
	ld	a, (hl)
	dec	a
	ld	a, #0x01
	jr	Z, 00440$
	xor	a, a
00440$:
	ld	l, a
	or	a, a
	jr	Z, 00128$
	ld	a, d
	or	a, a
	jr	Z, 00128$
	ld	a, c
	sub	a, #0x09
	jr	NZ, 00128$
;src/main.c:1466: show_message("DESK", "Your dock blinks by the CRT.\nTiny room. Good rain.", "SELECT CLOSE");
	ld	de, #___str_147
	push	de
	ld	bc, #___str_151
	ld	de, #___str_150
	call	_show_message
;src/main.c:1467: return;
	jp	00159$
00128$:
;src/main.c:1470: if ((room_id == ROOM_APARTMENT) && (tx == 13) && (ty == 3)) {
	ld	a, l
	or	a, a
	jr	Z, 00132$
	ld	a, d
	or	a, a
	jr	Z, 00132$
	ld	a, c
	sub	a, #0x03
	jr	NZ, 00132$
;src/main.c:1471: show_message("FISH TANK", "Blue light makes the room feel larger.", "SELECT CLOSE");
	ld	de, #___str_147
	push	de
	ld	bc, #___str_153
	ld	de, #___str_152
	call	_show_message
;src/main.c:1472: return;
	jp	00159$
00132$:
;src/main.c:1475: if ((room_id == ROOM_RECORDS) && (tx == 3) && (ty == 7)) {
	ld	a, b
	sub	a, #0x03
	ld	a, #0x01
	jr	Z, 00446$
	xor	a, a
00446$:
	ld	l, a
	ld	a, c
	sub	a, #0x07
	ld	a, #0x01
	jr	Z, 00448$
	xor	a, a
00448$:
	ld	d, a
	push	hl
	ld	a, (#_room_id)
	sub	a, #0x02
	pop	hl
	jr	NZ, 00136$
	ld	a, l
	or	a, a
	jr	Z, 00136$
	ld	a, d
	or	a, a
	jr	Z, 00136$
;src/main.c:1476: show_message("LISTENING POST", "Someone left a hand-labeled tape:\nVENDING DREAMS / A", "SELECT CLOSE");
	ld	de, #___str_147
	push	de
	ld	bc, #___str_155
	ld	de, #___str_154
	call	_show_message
;src/main.c:1477: return;
	jp	00159$
00136$:
;src/main.c:1480: if ((room_id == ROOM_TECH) && (tx == 3) && (ty == 7)) {
	push	hl
	ld	a, (#_room_id)
	sub	a, #0x03
	pop	hl
	jr	NZ, 00140$
	ld	a, l
	or	a, a
	jr	Z, 00140$
	ld	a, d
	or	a, a
	jr	Z, 00140$
;src/main.c:1481: show_message("BENCH", "Half-restored handhelds glow here.\nEach one hums a new startup chime.", "SELECT CLOSE");
	ld	de, #___str_147
	push	de
	ld	bc, #___str_157
	ld	de, #___str_156
	call	_show_message
;src/main.c:1482: return;
	jp	00159$
00140$:
;src/main.c:1485: if ((room_id == ROOM_RAMEN) && (tx == 7) && (ty == 8)) {
	ld	a, (#_room_id)
	sub	a, #0x04
	jr	NZ, 00144$
	ld	a, b
	sub	a, #0x07
	jr	NZ, 00144$
	ld	a, c
	sub	a, #0x08
	jr	NZ, 00144$
;src/main.c:1486: show_message("STOOL", "A warm seat, steamed glass,\nand just enough quiet.", "SELECT CLOSE");
	ld	de, #___str_147
	push	de
	ld	bc, #___str_159
	ld	de, #___str_158
	call	_show_message
;src/main.c:1487: return;
	jr	00159$
00144$:
;src/main.c:1490: if ((room_id == ROOM_ROOFTOP) && (tx == 9) && (ty == 6)) {
	ld	a, b
	sub	a, #0x09
	ld	a, #0x01
	jr	Z, 00460$
	xor	a, a
00460$:
	ld	d, a
	ld	a, c
	sub	a, #0x06
	ld	a, #0x01
	jr	Z, 00462$
	xor	a, a
00462$:
	ld	c, a
	ld	a, (#_room_id)
	sub	a, #0x05
	jr	NZ, 00148$
	or	a, d
	jr	Z, 00148$
	ld	a, c
	or	a, a
	jr	Z, 00148$
;src/main.c:1491: show_message("BOOTH", "Rain beads on the mixer.\nThe whole block softens up here.", "SELECT CLOSE");
	ld	de, #___str_147
	push	de
	ld	bc, #___str_161
	ld	de, #___str_160
	call	_show_message
;src/main.c:1492: return;
	jr	00159$
00148$:
;src/main.c:1495: if ((room_id == ROOM_SOUTHLINE) && (tx == 15) && (ty == 6)) {
	ld	a, (#_room_id)
	sub	a, #0x06
	ld	a, #0x01
	jr	Z, 00466$
	xor	a, a
00466$:
	ld	l, a
	or	a, a
	jr	Z, 00152$
	ld	a, b
	sub	a, #0x0f
	jr	NZ, 00152$
	or	a, c
	jr	Z, 00152$
;src/main.c:1496: show_message("SHELTER", "Couriers wait here between runs.\nThe whole lane hums a little faster.", "SELECT CLOSE");
	ld	de, #___str_147
	push	de
	ld	bc, #___str_163
	ld	de, #___str_162
	call	_show_message
;src/main.c:1497: return;
	jr	00159$
00152$:
;src/main.c:1500: if ((room_id == ROOM_SOUTHLINE) && (tx == 9) && (ty == 15)) {
	ld	a, l
	or	a, a
	jr	Z, 00159$
	ld	a, d
	or	a, a
	jr	Z, 00159$
	ld	a, e
	or	a, a
	jr	Z, 00159$
;src/main.c:1501: show_message("UNDERPASS", "The road keeps going south.\nMore of the city can open later.", "SELECT CLOSE");
	ld	de, #___str_147
	push	de
	ld	bc, #___str_165
	ld	de, #___str_164
	call	_show_message
00159$:
;src/main.c:1503: }
	inc	sp
	inc	sp
	ret
___str_145:
	.ascii "TERMINAL"
	.db 0x00
___str_146:
	.ascii "Delays, karaoke specials,"
	.db 0x0a
	.ascii "and a pirate radio station."
	.db 0x00
___str_147:
	.ascii "SELECT CLOSE"
	.db 0x00
___str_148:
	.ascii "STALL"
	.db 0x00
___str_149:
	.ascii "A cart sells synth carts,"
	.db 0x0a
	.ascii "batteries, and stickers."
	.db 0x00
___str_150:
	.ascii "DESK"
	.db 0x00
___str_151:
	.ascii "Your dock blinks by the CRT."
	.db 0x0a
	.ascii "Tiny room. Good rain."
	.db 0x00
___str_152:
	.ascii "FISH TANK"
	.db 0x00
___str_153:
	.ascii "Blue light makes the room feel larger."
	.db 0x00
___str_154:
	.ascii "LISTENING POST"
	.db 0x00
___str_155:
	.ascii "Someone left a hand-labeled tape:"
	.db 0x0a
	.ascii "VENDING DREAMS / A"
	.db 0x00
___str_156:
	.ascii "BENCH"
	.db 0x00
___str_157:
	.ascii "Half-restored handhelds glow here."
	.db 0x0a
	.ascii "Each one hums a new startup chime."
	.db 0x00
___str_158:
	.ascii "STOOL"
	.db 0x00
___str_159:
	.ascii "A warm seat, steamed glass,"
	.db 0x0a
	.ascii "and just enough quiet."
	.db 0x00
___str_160:
	.ascii "BOOTH"
	.db 0x00
___str_161:
	.ascii "Rain beads on the mixer."
	.db 0x0a
	.ascii "The whole block softens up here."
	.db 0x00
___str_162:
	.ascii "SHELTER"
	.db 0x00
___str_163:
	.ascii "Couriers wait here between runs."
	.db 0x0a
	.ascii "The whole lane hums a little faster."
	.db 0x00
___str_164:
	.ascii "UNDERPASS"
	.db 0x00
___str_165:
	.ascii "The road keeps going south."
	.db 0x0a
	.ascii "More of the city can open later."
	.db 0x00
;src/main.c:1505: static void show_title(void) {
;	---------------------------------
; Function show_title
; ---------------------------------
_show_title:
	dec	sp
;src/main.c:1507: uint8_t last_blink = 0xFFu;
	ldhl	sp,	#0
	ld	(hl), #0xff
;src/main.c:1509: waitpadup();
	call	_waitpadup
;src/main.c:1510: dt_audio_play_title_theme();
	ld	e, #b_dt_audio_play_title_theme
	ld	hl, #_dt_audio_play_title_theme
	call	___sdcc_bcall_ehl
;src/main.c:1511: while (1) {
00109$:
;src/main.c:1512: blink = (uint8_t)((frame_clock >> 4u) & 1u);
	ld	a, (#_frame_clock)
	swap	a
	and	a, #0x01
	ld	c, a
;src/main.c:1513: if (blink != last_blink) {
	ldhl	sp,	#0
	ld	a, (hl)
	sub	a, c
	jr	Z, 00105$
;src/main.c:1514: begin_display_reload();
	push	bc
	call	_begin_display_reload
	pop	bc
;src/main.c:1515: if (blink) {
	ld	a, c
	or	a, a
	jr	Z, 00102$
;src/main.c:1521: BANK(startup_dialtone_title_b)
	ld	a, #<(___bank_startup_dialtone_title_b)
;src/main.c:1520: startup_dialtone_title_b_palettes,
;src/main.c:1519: startup_dialtone_title_b_map,
;src/main.c:1517: startup_dialtone_title_b_tiles,
	push	bc
	push	af
	inc	sp
	ld	de, #_startup_dialtone_title_b_palettes
	push	de
	ld	de, #_startup_dialtone_title_b_map
	push	de
	ld	a, #0x8c
	ld	de, #_startup_dialtone_title_b_tiles
	call	_show_banked_image_card
	pop	bc
	jr	00103$
00102$:
;src/main.c:1529: BANK(startup_dialtone_title_a)
	ld	a, #<(___bank_startup_dialtone_title_a)
;src/main.c:1528: startup_dialtone_title_a_palettes,
	ld	hl, #_startup_dialtone_title_a_palettes
;src/main.c:1527: startup_dialtone_title_a_map,
;src/main.c:1525: startup_dialtone_title_a_tiles,
	ld	de, #_startup_dialtone_title_a_tiles
	push	bc
	push	af
	inc	sp
	push	hl
	ld	hl, #_startup_dialtone_title_a_map
	push	hl
	ld	a, #0x82
	call	_show_banked_image_card
	pop	bc
00103$:
;src/main.c:1532: finish_display_reload(0u);
	push	bc
	xor	a, a
	call	_finish_display_reload
	pop	bc
;src/main.c:1533: last_blink = blink;
	ldhl	sp,	#0
	ld	(hl), c
00105$:
;src/main.c:1536: vsync();
	call	_vsync
;src/main.c:1537: dt_audio_update();
	ld	e, #b_dt_audio_update
	ld	hl, #_dt_audio_update
	call	___sdcc_bcall_ehl
;src/main.c:1538: ++frame_clock;
	ld	hl, #_frame_clock
	inc	(hl)
;src/main.c:1539: if (wait_card_input(1u)) {
	ld	a, #0x01
	call	_wait_card_input
	or	a, a
	jr	Z, 00109$
;src/main.c:1540: waitpadup();
	call	_waitpadup
;src/main.c:1541: dt_audio_stop_music();
	ld	e, #b_dt_audio_stop_music
	ld	hl, #_dt_audio_stop_music
	call	___sdcc_bcall_ehl
;src/main.c:1542: return;
;src/main.c:1545: }
	inc	sp
	ret
;src/main.c:1547: void main(void) {
;	---------------------------------
; Function main
; ---------------------------------
_main::
	add	sp, #-4
;src/main.c:1549: uint8_t last = 0u;
	ldhl	sp,	#2
	ld	(hl), #0x00
;src/main.c:1550: uint8_t move_dir = 0u;
	dec	hl
	dec	hl
	ld	(hl), #0x00
;src/main.c:1551: uint8_t move_repeat = 0u;
	ldhl	sp,	#3
	ld	(hl), #0x00
;src/main.c:1553: dt_audio_init();
	ld	e, #b_dt_audio_init
	ld	hl, #_dt_audio_init
	call	___sdcc_bcall_ehl
;src/main.c:1554: dt_init_assets();
	ld	e, #b_dt_init_assets
	ld	hl, #_dt_init_assets
	call	___sdcc_bcall_ehl
;src/main.c:1555: SPRITES_8x8;
	ldh	a, (_LCDC_REG + 0)
	and	a, #0xfb
	ldh	(_LCDC_REG + 0), a
;src/main.c:1557: setup_new_game();
	call	_setup_new_game
;src/main.c:1558: show_company_card();
	call	_show_company_card
;src/main.c:1559: show_bimpson_card();
	call	_show_bimpson_card
;src/main.c:1560: show_title();
	call	_show_title
;src/main.c:1565: );
;src/main.c:1563: "You moved into a tiny room in Luma Lane.\nMeet the block, run one favor,\nbring something cozy home.",
;src/main.c:1562: "ARRIVAL",
	ld	de, #___str_168
	push	de
	ld	bc, #___str_167
	ld	de, #___str_166
	call	_show_message
;src/main.c:1566: load_room();
	call	_load_room
;src/main.c:1567: render_room();
	call	_render_room
;src/main.c:1569: while (1) {
00169$:
;src/main.c:1570: vsync();
	call	_vsync
;src/main.c:1571: dt_audio_update();
	ld	e, #b_dt_audio_update
	ld	hl, #_dt_audio_update
	call	___sdcc_bcall_ehl
;src/main.c:1572: keys = joypad();
	call	_joypad
	ldhl	sp,	#1
	ld	(hl), a
;src/main.c:1574: if ((keys & J_A) && !(last & J_A)) try_interact();
	push	hl
	ldhl	sp,	#3
	bit	4, (hl)
	pop	hl
	jr	Z, 00165$
	push	hl
	ldhl	sp,	#4
	bit	4, (hl)
	pop	hl
	jr	NZ, 00165$
	call	_try_interact
	jp	00166$
00165$:
;src/main.c:1575: else if ((keys & J_B) && !(last & J_B)) {
	push	hl
	ldhl	sp,	#3
	bit	5, (hl)
	pop	hl
	jr	Z, 00161$
	push	hl
	ldhl	sp,	#4
	bit	5, (hl)
	pop	hl
	jr	NZ, 00161$
;src/main.c:1576: inventory_screen(0u);
	xor	a, a
	call	_inventory_screen
;src/main.c:1577: load_room();
	call	_load_room
;src/main.c:1578: render_room();
	call	_render_room
	jp	00166$
00161$:
;src/main.c:1579: } else if ((keys & J_SELECT) && !(last & J_SELECT)) {
	push	hl
	ldhl	sp,	#3
	bit	6, (hl)
	pop	hl
	jr	Z, 00157$
	push	hl
	ldhl	sp,	#4
	bit	6, (hl)
	pop	hl
	jr	NZ, 00157$
;src/main.c:1580: inventory_screen(1u);
	ld	a, #0x01
	call	_inventory_screen
;src/main.c:1581: load_room();
	call	_load_room
;src/main.c:1582: render_room();
	call	_render_room
	jp	00166$
00157$:
;src/main.c:1583: } else if ((keys & J_START) && !(last & J_START)) {
	push	hl
	ldhl	sp,	#3
	bit	7, (hl)
	pop	hl
	jr	Z, 00153$
	push	hl
	ldhl	sp,	#4
	bit	7, (hl)
	pop	hl
	jr	NZ, 00153$
;src/main.c:1584: pause_menu();
	call	_pause_menu
;src/main.c:1585: load_room();
	call	_load_room
;src/main.c:1586: render_room();
	call	_render_room
	jp	00166$
00153$:
;src/main.c:1588: uint8_t pressed = keys & (uint8_t)~last;
	ldhl	sp,	#2
	ld	a, (hl-)
	cpl
	ld	c, a
	ld	a, (hl)
	and	a, c
	ld	b, a
;src/main.c:1589: uint8_t active_dir = 0u;
	ld	c, #0x00
;src/main.c:1591: if (pressed & J_UP) active_dir = J_UP;
	bit	2, b
	jr	Z, 00122$
	ld	c, #0x04
	jr	00123$
00122$:
;src/main.c:1592: else if (pressed & J_DOWN) active_dir = J_DOWN;
	bit	3, b
	jr	Z, 00119$
	ld	c, #0x08
	jr	00123$
00119$:
;src/main.c:1593: else if (pressed & J_LEFT) active_dir = J_LEFT;
	bit	1, b
	jr	Z, 00116$
	ld	c, #0x02
	jr	00123$
00116$:
;src/main.c:1594: else if (pressed & J_RIGHT) active_dir = J_RIGHT;
	bit	0, b
	jr	Z, 00113$
	ld	c, #0x01
	jr	00123$
00113$:
;src/main.c:1595: else if (keys & J_UP) active_dir = J_UP;
	push	hl
	ldhl	sp,	#3
	bit	2, (hl)
	pop	hl
	jr	Z, 00110$
	ld	c, #0x04
	jr	00123$
00110$:
;src/main.c:1596: else if (keys & J_DOWN) active_dir = J_DOWN;
	push	hl
	ldhl	sp,	#3
	bit	3, (hl)
	pop	hl
	jr	Z, 00107$
	ld	c, #0x08
	jr	00123$
00107$:
;src/main.c:1597: else if (keys & J_LEFT) active_dir = J_LEFT;
	push	hl
	ldhl	sp,	#3
	bit	1, (hl)
	pop	hl
	jr	Z, 00104$
	ld	c, #0x02
	jr	00123$
00104$:
;src/main.c:1598: else if (keys & J_RIGHT) active_dir = J_RIGHT;
	push	hl
	ldhl	sp,	#3
	bit	0, (hl)
	pop	hl
	jr	Z, 00123$
	ld	c, #0x01
00123$:
;src/main.c:1600: if (active_dir == 0u) {
	ld	a, c
	or	a, a
	jr	NZ, 00150$
;src/main.c:1601: move_dir = 0u;
	ldhl	sp,	#0
	ld	(hl), #0x00
;src/main.c:1602: move_repeat = 0u;
	ldhl	sp,	#3
	ld	(hl), #0x00
	jp	00166$
00150$:
;src/main.c:1606: if (active_dir == J_UP) try_move(0, -1, DIR_UP);
	ld	a, c
	sub	a, #0x04
	ld	a, #0x01
	jr	Z, 00376$
	xor	a, a
00376$:
	ldhl	sp,	#2
	ld	(hl), a
;src/main.c:1607: else if (active_dir == J_DOWN) try_move(0, 1, DIR_DOWN);
	ld	a, c
	sub	a, #0x08
	ld	a, #0x01
	jr	Z, 00378$
	xor	a, a
00378$:
	ld	e, a
;src/main.c:1608: else if (active_dir == J_LEFT) try_move(-1, 0, DIR_LEFT);
	ld	a, c
	sub	a, #0x02
	ld	a, #0x01
	jr	Z, 00380$
	xor	a, a
00380$:
	ld	d, a
;src/main.c:1603: } else if ((active_dir != move_dir) || (pressed & active_dir)) {
	ldhl	sp,	#0
	ld	a, (hl)
	sub	a, c
	jr	NZ, 00145$
	ld	a, b
	and	a, c
	jr	Z, 00146$
00145$:
;src/main.c:1604: move_dir = active_dir;
	ldhl	sp,	#0
	ld	(hl), c
;src/main.c:1605: move_repeat = movement_repeat_delay();
	push	de
	call	_movement_repeat_delay
	ldhl	sp,	#5
	ld	(hl), a
	pop	de
;src/main.c:1606: if (active_dir == J_UP) try_move(0, -1, DIR_UP);
	ldhl	sp,	#2
	ld	a, (hl)
	or	a, a
	jr	Z, 00131$
	ld	a, #0x01
	push	af
	inc	sp
	ld	e, #0xff
	xor	a, a
	call	_try_move
	jr	00166$
00131$:
;src/main.c:1607: else if (active_dir == J_DOWN) try_move(0, 1, DIR_DOWN);
	ld	a, e
	or	a, a
	jr	Z, 00128$
	xor	a, a
	push	af
	inc	sp
	ld	e, #0x01
	xor	a, a
	call	_try_move
	jr	00166$
00128$:
;src/main.c:1608: else if (active_dir == J_LEFT) try_move(-1, 0, DIR_LEFT);
	ld	a, d
	or	a, a
	jr	Z, 00125$
	ld	a, #0x02
	push	af
	inc	sp
	ld	e, #0x00
	ld	a, #0xff
	call	_try_move
	jr	00166$
00125$:
;src/main.c:1609: else try_move(1, 0, DIR_RIGHT);
	ld	a, #0x03
	push	af
	inc	sp
	ld	e, #0x00
	ld	a, #0x01
	call	_try_move
	jr	00166$
00146$:
;src/main.c:1610: } else if (move_repeat != 0u) {
	ldhl	sp,	#3
	ld	a, (hl)
	or	a, a
	jr	Z, 00143$
;src/main.c:1611: --move_repeat;
	dec	(hl)
	jr	00166$
00143$:
;src/main.c:1613: move_repeat = movement_repeat_interval();
	push	de
	call	_movement_repeat_interval
	ldhl	sp,	#5
	ld	(hl), a
	pop	de
;src/main.c:1614: if (active_dir == J_UP) try_move(0, -1, DIR_UP);
	ldhl	sp,	#2
	ld	a, (hl)
	or	a, a
	jr	Z, 00140$
	ld	a, #0x01
	push	af
	inc	sp
	ld	e, #0xff
	xor	a, a
	call	_try_move
	jr	00166$
00140$:
;src/main.c:1615: else if (active_dir == J_DOWN) try_move(0, 1, DIR_DOWN);
	ld	a, e
	or	a, a
	jr	Z, 00137$
	xor	a, a
	push	af
	inc	sp
	ld	e, #0x01
	xor	a, a
	call	_try_move
	jr	00166$
00137$:
;src/main.c:1616: else if (active_dir == J_LEFT) try_move(-1, 0, DIR_LEFT);
	ld	a, d
	or	a, a
	jr	Z, 00134$
	ld	a, #0x02
	push	af
	inc	sp
	ld	e, #0x00
	ld	a, #0xff
	call	_try_move
	jr	00166$
00134$:
;src/main.c:1617: else try_move(1, 0, DIR_RIGHT);
	ld	a, #0x03
	push	af
	inc	sp
	ld	e, #0x00
	ld	a, #0x01
	call	_try_move
00166$:
;src/main.c:1621: ++frame_clock;
	ld	hl, #_frame_clock
	inc	(hl)
;src/main.c:1622: draw_world_sprites(tile_to_sprite_x(player_x), tile_to_sprite_y(player_y), 0u);
	ld	a, (_player_y)
	call	_tile_to_sprite_y
	ldhl	sp,	#2
	ld	(hl), a
	ld	a, (_player_x)
	call	_tile_to_sprite_x
	ld	h, #0x00
	push	hl
	inc	sp
	ldhl	sp,	#3
	ld	e, (hl)
	call	_draw_world_sprites
;src/main.c:1623: last = keys;
	ldhl	sp,	#1
	ld	a, (hl+)
	ld	(hl), a
	jp	00169$
;src/main.c:1625: }
	add	sp, #4
	ret
___str_166:
	.ascii "ARRIVAL"
	.db 0x00
___str_167:
	.ascii "You moved into a tiny room in Luma Lane."
	.db 0x0a
	.ascii "Meet the block, run one favor,"
	.db 0x0a
	.ascii "bring something cozy home."
	.db 0x00
___str_168:
	.ascii "SELECT WALK"
	.db 0x00
	.area _CODE
	.area _INITIALIZER
	.area _CABS (ABS)
