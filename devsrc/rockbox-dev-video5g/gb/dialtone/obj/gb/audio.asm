;--------------------------------------------------------
; File Created by SDCC : free open source ISO C Compiler
; Version 4.5.1 #15267 (Linux)
;--------------------------------------------------------
	.module audio
	
;--------------------------------------------------------
; Public variables in this module
;--------------------------------------------------------
	.globl b_dt_audio_init
	.globl _dt_audio_init
	.globl b_dt_audio_update
	.globl _dt_audio_update
	.globl b_dt_audio_play_sfx
	.globl _dt_audio_play_sfx
	.globl b_dt_audio_play_title_theme
	.globl _dt_audio_play_title_theme
	.globl b_dt_audio_stop_music
	.globl _dt_audio_stop_music
	.globl b_dt_audio_set_home_record
	.globl _dt_audio_set_home_record
	.globl b_dt_audio_get_home_record
	.globl _dt_audio_get_home_record
	.globl b_dt_audio_sync
	.globl _dt_audio_sync
;--------------------------------------------------------
; special function registers
;--------------------------------------------------------
	.area _HRAM
;--------------------------------------------------------
; ram data
;--------------------------------------------------------
	.area _DATA
_home_record:
	.ds 1
_current_track:
	.ds 1
_note_index:
	.ds 1
_note_frames_left:
	.ds 1
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
;src/audio.c:100: static const note_event_t *track_for_record(uint8_t record_id) {
;	---------------------------------
; Function track_for_record
; ---------------------------------
_track_for_record:
;src/audio.c:101: switch (record_id) {
	cp	a, #0x01
	jr	Z, 00102$
	cp	a, #0x02
	jr	Z, 00103$
	cp	a, #0x03
	jr	Z, 00104$
	sub	a, #0xfe
	jr	NZ, 00105$
;src/audio.c:102: case TRACK_TITLE_THEME: return title_theme_track;
	ld	bc, #_title_theme_track
	ret
;src/audio.c:103: case RECORD_RAIN_LOOP: return rain_loop_track;
00102$:
	ld	bc, #_rain_loop_track
	ret
;src/audio.c:104: case RECORD_VENDING_DREAMS: return vending_dreams_track;
00103$:
	ld	bc, #_vending_dreams_track
	ret
;src/audio.c:105: case RECORD_ROOFTOP_SET: return rooftop_set_track;
00104$:
	ld	bc, #_rooftop_set_track
	ret
;src/audio.c:106: default: return 0;
00105$:
	ld	bc, #0x0000
;src/audio.c:107: }
;src/audio.c:108: }
	ret
_note_freqs:
	.dw #0x0706
	.dw #0x0714
	.dw #0x0721
	.dw #0x072d
	.dw #0x0739
	.dw #0x0744
	.dw #0x074f
	.dw #0x0759
	.dw #0x0762
	.dw #0x076b
	.dw #0x0773
	.dw #0x077b
	.dw #0x0783
	.dw #0x078a
	.dw #0x0790
	.dw #0x0797
	.dw #0x079d
	.dw #0x07a2
	.dw #0x07a7
	.dw #0x07ac
	.dw #0x07b1
	.dw #0x07b6
	.dw #0x07ba
	.dw #0x07be
	.dw #0x07c1
	.dw #0x07c4
	.dw #0x07c8
	.dw #0x07cb
	.dw #0x07ce
	.dw #0x07d1
	.dw #0x07d4
	.dw #0x07d6
_rain_loop_track:
	.db #0x04	; 4
	.db #0x0a	; 10
	.db #0xfe	; 254
	.db #0x02	; 2
	.db #0x07	; 7
	.db #0x06	; 6
	.db #0x09	; 9
	.db #0x0a	; 10
	.db #0x07	; 7
	.db #0x06	; 6
	.db #0x04	; 4
	.db #0x08	; 8
	.db #0x02	; 2
	.db #0x08	; 8
	.db #0xfe	; 254
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x0a	; 10
	.db #0xfe	; 254
	.db #0x02	; 2
	.db #0x07	; 7
	.db #0x06	; 6
	.db #0x0b	; 11
	.db #0x0a	; 10
	.db #0x09	; 9
	.db #0x06	; 6
	.db #0x07	; 7
	.db #0x08	; 8
	.db #0x04	; 4
	.db #0x08	; 8
	.db #0xfe	; 254
	.db #0x0a	; 10
	.db #0xff	; 255
	.db #0x00	; 0
_vending_dreams_track:
	.db #0x0c	; 12
	.db #0x06	; 6
	.db #0x10	; 16
	.db #0x06	; 6
	.db #0x13	; 19
	.db #0x06	; 6
	.db #0x15	; 21
	.db #0x06	; 6
	.db #0x13	; 19
	.db #0x06	; 6
	.db #0x10	; 16
	.db #0x06	; 6
	.db #0x0e	; 14
	.db #0x06	; 6
	.db #0x10	; 16
	.db #0x06	; 6
	.db #0x0c	; 12
	.db #0x08	; 8
	.db #0xfe	; 254
	.db #0x04	; 4
	.db #0x10	; 16
	.db #0x06	; 6
	.db #0x13	; 19
	.db #0x06	; 6
	.db #0x17	; 23
	.db #0x06	; 6
	.db #0x15	; 21
	.db #0x06	; 6
	.db #0x13	; 19
	.db #0x06	; 6
	.db #0x10	; 16
	.db #0x06	; 6
	.db #0x0e	; 14
	.db #0x0a	; 10
	.db #0xfe	; 254
	.db #0x08	; 8
	.db #0xff	; 255
	.db #0x00	; 0
_rooftop_set_track:
	.db #0x09	; 9
	.db #0x08	; 8
	.db #0x0c	; 12
	.db #0x08	; 8
	.db #0x10	; 16
	.db #0x08	; 8
	.db #0x13	; 19
	.db #0x08	; 8
	.db #0x10	; 16
	.db #0x08	; 8
	.db #0x0c	; 12
	.db #0x08	; 8
	.db #0x0e	; 14
	.db #0x08	; 8
	.db #0xfe	; 254
	.db #0x04	; 4
	.db #0x09	; 9
	.db #0x08	; 8
	.db #0x0e	; 14
	.db #0x08	; 8
	.db #0x11	; 17
	.db #0x08	; 8
	.db #0x13	; 19
	.db #0x08	; 8
	.db #0x11	; 17
	.db #0x08	; 8
	.db #0x0e	; 14
	.db #0x08	; 8
	.db #0x0c	; 12
	.db #0x08	; 8
	.db #0xfe	; 254
	.db #0x08	; 8
	.db #0xff	; 255
	.db #0x00	; 0
_title_theme_track:
	.db #0x10	; 16
	.db #0x08	; 8
	.db #0x13	; 19
	.db #0x08	; 8
	.db #0x17	; 23
	.db #0x08	; 8
	.db #0x1b	; 27
	.db #0x08	; 8
	.db #0x17	; 23
	.db #0x06	; 6
	.db #0x13	; 19
	.db #0x06	; 6
	.db #0x12	; 18
	.db #0x08	; 8
	.db #0xfe	; 254
	.db #0x04	; 4
	.db #0x10	; 16
	.db #0x08	; 8
	.db #0x15	; 21
	.db #0x08	; 8
	.db #0x18	; 24
	.db #0x08	; 8
	.db #0x1c	; 28
	.db #0x08	; 8
	.db #0x18	; 24
	.db #0x06	; 6
	.db #0x15	; 21
	.db #0x06	; 6
	.db #0x13	; 19
	.db #0x08	; 8
	.db #0xfe	; 254
	.db #0x06	; 6
	.db #0x0e	; 14
	.db #0x08	; 8
	.db #0x12	; 18
	.db #0x08	; 8
	.db #0x15	; 21
	.db #0x08	; 8
	.db #0x18	; 24
	.db #0x08	; 8
	.db #0x15	; 21
	.db #0x06	; 6
	.db #0x12	; 18
	.db #0x06	; 6
	.db #0x10	; 16
	.db #0x08	; 8
	.db #0xfe	; 254
	.db #0x08	; 8
	.db #0xff	; 255
	.db #0x00	; 0
;src/audio.c:110: static uint8_t track_duty(uint8_t record_id) {
;	---------------------------------
; Function track_duty
; ---------------------------------
_track_duty:
;src/audio.c:111: switch (record_id) {
	cp	a, #0x01
	jr	Z, 00102$
	cp	a, #0x02
	jr	Z, 00103$
	cp	a, #0x03
	jr	Z, 00104$
	sub	a, #0xfe
	jr	NZ, 00105$
;src/audio.c:112: case TRACK_TITLE_THEME: return 0x40u;
	ld	a, #0x40
	ret
;src/audio.c:113: case RECORD_RAIN_LOOP: return 0x40u;
00102$:
	ld	a, #0x40
	ret
;src/audio.c:114: case RECORD_VENDING_DREAMS: return 0x40u;
00103$:
	ld	a, #0x40
	ret
;src/audio.c:115: case RECORD_ROOFTOP_SET: return 0x40u;
00104$:
	ld	a, #0x40
	ret
;src/audio.c:116: default: return 0x40u;
00105$:
	ld	a, #0x40
;src/audio.c:117: }
;src/audio.c:118: }
	ret
;src/audio.c:120: static uint8_t track_envelope(uint8_t record_id) {
;	---------------------------------
; Function track_envelope
; ---------------------------------
_track_envelope:
;src/audio.c:121: switch (record_id) {
	cp	a, #0x01
	jr	Z, 00102$
	cp	a, #0x02
	jr	Z, 00103$
	cp	a, #0x03
	jr	Z, 00104$
	sub	a, #0xfe
	jr	NZ, 00105$
;src/audio.c:122: case TRACK_TITLE_THEME: return 0x52u;
	ld	a, #0x52
	ret
;src/audio.c:123: case RECORD_RAIN_LOOP: return 0x32u;
00102$:
	ld	a, #0x32
	ret
;src/audio.c:124: case RECORD_VENDING_DREAMS: return 0x42u;
00103$:
	ld	a, #0x42
	ret
;src/audio.c:125: case RECORD_ROOFTOP_SET: return 0x33u;
00104$:
	ld	a, #0x33
	ret
;src/audio.c:126: default: return 0x30u;
00105$:
	ld	a, #0x30
;src/audio.c:127: }
;src/audio.c:128: }
	ret
;src/audio.c:130: static void stop_music_channel(void) {
;	---------------------------------
; Function stop_music_channel
; ---------------------------------
_stop_music_channel:
;src/audio.c:131: NR22_REG = 0x00u;
	xor	a, a
	ldh	(_NR22_REG + 0), a
;src/audio.c:132: NR24_REG = 0x00u;
	xor	a, a
	ldh	(_NR24_REG + 0), a
;src/audio.c:133: }
	ret
;src/audio.c:135: static void play_music_note(uint8_t record_id, uint8_t note) {
;	---------------------------------
; Function play_music_note
; ---------------------------------
_play_music_note:
	dec	sp
	ldhl	sp,	#0
	ld	(hl), a
;src/audio.c:136: uint16_t freq = note_freqs[note];
	ld	bc, #_note_freqs+0
	ld	l, e
	ld	h, #0x00
	add	hl, hl
	add	hl, bc
	ld	a, (hl+)
	ld	c, a
	ld	b, (hl)
;src/audio.c:138: NR21_REG = track_duty(record_id);
	push	bc
	ldhl	sp,	#2
	ld	a, (hl)
	call	_track_duty
	pop	bc
	ldh	(_NR21_REG + 0), a
;src/audio.c:139: NR22_REG = track_envelope(record_id);
	push	bc
	ldhl	sp,	#2
	ld	a, (hl)
	call	_track_envelope
	pop	bc
	ldh	(_NR22_REG + 0), a
;src/audio.c:140: NR23_REG = (uint8_t)(freq & 0xFFu);
	ld	a, c
	ldh	(_NR23_REG + 0), a
;src/audio.c:141: NR24_REG = (uint8_t)(0x80u | ((freq >> 8u) & 0x07u));
	ld	a, b
	and	a, #0x07
	or	a, #0x80
	ldh	(_NR24_REG + 0), a
;src/audio.c:142: }
	inc	sp
	ret
;src/audio.c:144: static void play_pulse_sfx(uint16_t freq, uint8_t duty, uint8_t envelope, uint8_t sweep) {
;	---------------------------------
; Function play_pulse_sfx
; ---------------------------------
_play_pulse_sfx:
	ld	c, a
;src/audio.c:145: NR10_REG = sweep;
	ldhl	sp,	#3
;src/audio.c:146: NR11_REG = duty;
;src/audio.c:147: NR12_REG = envelope;
	ld	a, (hl-)
	ldh	(_NR10_REG + 0), a
	ld	a, c
	ldh	(_NR11_REG + 0), a
	ld	a, (hl)
	ldh	(_NR12_REG + 0), a
;src/audio.c:148: NR13_REG = (uint8_t)(freq & 0xFFu);
	ld	a, e
	ldh	(_NR13_REG + 0), a
;src/audio.c:149: NR14_REG = (uint8_t)(0x80u | ((freq >> 8u) & 0x07u));
	ld	a, d
	and	a, #0x07
	or	a, #0x80
	ldh	(_NR14_REG + 0), a
;src/audio.c:150: }
	pop	hl
	pop	af
	jp	(hl)
;src/audio.c:152: static void play_noise_sfx(uint8_t envelope, uint8_t noise) {
;	---------------------------------
; Function play_noise_sfx
; ---------------------------------
_play_noise_sfx:
	ld	c, a
;src/audio.c:153: NR41_REG = 0x00u;
	xor	a, a
	ldh	(_NR41_REG + 0), a
;src/audio.c:154: NR42_REG = envelope;
	ld	a, c
	ldh	(_NR42_REG + 0), a
;src/audio.c:155: NR43_REG = noise;
	ld	a, e
	ldh	(_NR43_REG + 0), a
;src/audio.c:156: NR44_REG = 0x80u;
	ld	a, #0x80
	ldh	(_NR44_REG + 0), a
;src/audio.c:157: }
	ret
;src/audio.c:159: void dt_audio_init(void) BANKED {
;	---------------------------------
; Function dt_audio_init
; ---------------------------------
	b_dt_audio_init	= 255
_dt_audio_init::
;src/audio.c:160: NR52_REG = 0x80u;
	ld	a, #0x80
	ldh	(_NR52_REG + 0), a
;src/audio.c:161: NR50_REG = 0x33u;
	ld	a, #0x33
	ldh	(_NR50_REG + 0), a
;src/audio.c:162: NR51_REG = 0xFFu;
	ld	a, #0xff
	ldh	(_NR51_REG + 0), a
;src/audio.c:163: home_record = RECORD_NONE;
;src/audio.c:164: current_track = RECORD_NONE;
	xor	a, a
	ld	(#_home_record), a
	ld	(#_current_track),a
;src/audio.c:165: note_index = 0u;
;src/audio.c:166: note_frames_left = 0u;
	xor	a, a
	ld	(#_note_index), a
	ld	(#_note_frames_left),a
;src/audio.c:167: stop_music_channel();
;src/audio.c:168: }
	jp	_stop_music_channel
;src/audio.c:170: void dt_audio_update(void) BANKED {
;	---------------------------------
; Function dt_audio_update
; ---------------------------------
	b_dt_audio_update	= 255
_dt_audio_update::
	add	sp, #-4
;src/audio.c:174: if (current_track == RECORD_NONE) {
	ld	a, (#_current_track)
	or	a, a
;src/audio.c:175: return;
	jr	Z, 00114$
;src/audio.c:178: if (note_frames_left != 0u) {
	ld	hl, #_note_frames_left
	ld	a, (hl)
	or	a, a
	jr	Z, 00106$
;src/audio.c:179: --note_frames_left;
;src/audio.c:180: if (note_frames_left != 0u) {
	dec	(hl)
	ld	a, (hl)
;src/audio.c:181: return;
	jr	NZ, 00114$
00106$:
;src/audio.c:185: track = track_for_record(current_track);
	ld	a, (_current_track)
	call	_track_for_record
	ldhl	sp,	#2
	ld	a, c
	ld	(hl+), a
	ld	(hl), b
;src/audio.c:186: if (track == 0) {
	ldhl	sp,	#3
	ld	a, (hl-)
;src/audio.c:187: current_track = RECORD_NONE;
	or	a,(hl)
	jr	NZ, 00108$
	ld	(#_current_track),a
;src/audio.c:188: stop_music_channel();
	call	_stop_music_channel
;src/audio.c:189: return;
	jr	00114$
00108$:
;src/audio.c:192: event = track[note_index];
	ld	a, (_note_index)
	ld	l, a
	ld	h, #0x00
	add	hl, hl
	ld	c, l
	ld	b, h
	ldhl	sp,	#2
	ld	a,	(hl+)
	ld	h, (hl)
	ld	l, a
	add	hl, bc
	ld	c, l
	ld	b, h
	ld	de, #0x0002
	push	de
	ld	hl, #2
	add	hl, sp
	ld	e, l
	ld	d, h
	call	___memcpy
;src/audio.c:193: if (event.note == NOTE_END) {
	ldhl	sp,	#0
	ld	a, (hl)
;src/audio.c:194: note_index = 0u;
	inc	a
	jr	NZ, 00110$
	ld	(#_note_index),a
;src/audio.c:195: event = track[0];
	ldhl	sp,	#2
	ld	a, (hl+)
	ld	c, a
	ld	b, (hl)
	ld	de, #0x0002
	push	de
	ld	hl, #2
	add	hl, sp
	ld	e, l
	ld	d, h
	call	___memcpy
00110$:
;src/audio.c:197: ++note_index;
	ld	hl, #_note_index
	inc	(hl)
;src/audio.c:198: note_frames_left = event.frames;
	ldhl	sp,	#1
	ld	a, (hl)
	ld	(#_note_frames_left),a
;src/audio.c:200: if (event.note == NOTE_REST) {
	ldhl	sp,	#0
	ld	a, (hl)
	cp	a, #0xfe
	jr	NZ, 00112$
;src/audio.c:201: stop_music_channel();
	call	_stop_music_channel
	jr	00114$
00112$:
;src/audio.c:203: play_music_note(current_track, event.note);
	ld	e, a
	ld	a, (_current_track)
	call	_play_music_note
00114$:
;src/audio.c:205: }
	add	sp, #4
	ret
;src/audio.c:207: void dt_audio_play_sfx(uint8_t sfx_id) BANKED {
;	---------------------------------
; Function dt_audio_play_sfx
; ---------------------------------
	b_dt_audio_play_sfx	= 255
_dt_audio_play_sfx::
;src/audio.c:208: switch (sfx_id) {
	ld	a, #0x05
	ldhl	sp,	#6
	sub	a, (hl)
	ret	C
	ld	c, (hl)
	ld	b, #0x00
	ld	hl, #00118$
	add	hl, bc
	add	hl, bc
	ld	c, (hl)
	inc	hl
	ld	h, (hl)
	ld	l, c
	jp	(hl)
00118$:
	.dw	00101$
	.dw	00102$
	.dw	00103$
	.dw	00104$
	.dw	00105$
	.dw	00106$
;src/audio.c:209: case SFX_MENU_MOVE:
00101$:
;src/audio.c:210: play_noise_sfx(0x12u, 0x20u);
	ld	e, #0x20
;src/audio.c:211: break;
	ld	a, #0x12
	jp	_play_noise_sfx
;src/audio.c:212: case SFX_CONFIRM:
00102$:
;src/audio.c:213: play_pulse_sfx(note_freqs[NOTE_G5], 0x40u, 0x62u, 0x00u);
	ld	hl, #(_note_freqs + 62)
	ld	a, (hl+)
	ld	c, a
	ld	b, (hl)
	ld	hl, #0x62
	push	hl
	ld	a, #0x40
	ld	e, c
	ld	d, b
	call	_play_pulse_sfx
;src/audio.c:214: break;
	ret
;src/audio.c:215: case SFX_CANCEL:
00103$:
;src/audio.c:216: play_noise_sfx(0x31u, 0x36u);
	ld	e, #0x36
;src/audio.c:217: break;
	ld	a, #0x31
	jp	_play_noise_sfx
;src/audio.c:218: case SFX_DOOR:
00104$:
;src/audio.c:219: play_pulse_sfx(note_freqs[NOTE_D4], 0x40u, 0x54u, 0x16u);
	ld	hl, #(_note_freqs + 28)
	ld	a, (hl+)
	ld	c, a
	ld	b, (hl)
	ld	hl, #0x1654
	push	hl
	ld	a, #0x40
	ld	e, c
	ld	d, b
	call	_play_pulse_sfx
;src/audio.c:220: break;
	ret
;src/audio.c:221: case SFX_REWARD:
00105$:
;src/audio.c:222: play_pulse_sfx(note_freqs[NOTE_B4], 0x40u, 0x72u, 0x00u);
	ld	hl, #(_note_freqs + 46)
	ld	a, (hl+)
	ld	c, a
	ld	b, (hl)
	ld	hl, #0x72
	push	hl
	ld	a, #0x40
	ld	e, c
	ld	d, b
	call	_play_pulse_sfx
;src/audio.c:223: break;
	ret
;src/audio.c:224: case SFX_FART:
00106$:
;src/audio.c:225: play_pulse_sfx(note_freqs[NOTE_D3], 0x40u, 0x82u, 0x1Eu);
	ld	hl, #(_note_freqs + 4)
	ld	a, (hl+)
	ld	c, a
	ld	b, (hl)
	ld	hl, #0x1e82
	push	hl
	ld	a, #0x40
	ld	e, c
	ld	d, b
	call	_play_pulse_sfx
;src/audio.c:226: play_noise_sfx(0x43u, 0x5Du);
	ld	e, #0x5d
	ld	a, #0x43
;src/audio.c:230: }
;src/audio.c:231: }
	jp	_play_noise_sfx
;src/audio.c:233: void dt_audio_play_title_theme(void) BANKED {
;	---------------------------------
; Function dt_audio_play_title_theme
; ---------------------------------
	b_dt_audio_play_title_theme	= 255
_dt_audio_play_title_theme::
;src/audio.c:234: if (current_track != TRACK_TITLE_THEME) {
	ld	a, (#_current_track)
	sub	a, #0xfe
	ret	Z
;src/audio.c:235: current_track = TRACK_TITLE_THEME;
	ld	hl, #_current_track
	ld	(hl), #0xfe
;src/audio.c:236: note_index = 0u;
;src/audio.c:237: note_frames_left = 0u;
	xor	a, a
	ld	(#_note_index), a
	ld	(#_note_frames_left),a
;src/audio.c:239: }
	ret
;src/audio.c:241: void dt_audio_stop_music(void) BANKED {
;	---------------------------------
; Function dt_audio_stop_music
; ---------------------------------
	b_dt_audio_stop_music	= 255
_dt_audio_stop_music::
;src/audio.c:242: current_track = RECORD_NONE;
;src/audio.c:243: note_index = 0u;
	xor	a, a
	ld	(#_current_track), a
	ld	(#_note_index),a
;src/audio.c:244: note_frames_left = 0u;
	xor	a, a
	ld	(#_note_frames_left),a
;src/audio.c:245: stop_music_channel();
;src/audio.c:246: }
	jp	_stop_music_channel
;src/audio.c:248: void dt_audio_set_home_record(uint8_t record_id) BANKED {
;	---------------------------------
; Function dt_audio_set_home_record
; ---------------------------------
	b_dt_audio_set_home_record	= 255
_dt_audio_set_home_record::
;src/audio.c:249: home_record = record_id;
	ldhl	sp,	#6
	ld	a, (hl)
	ld	(#_home_record),a
;src/audio.c:250: note_index = 0u;
;src/audio.c:251: note_frames_left = 0u;
	xor	a, a
	ld	(#_note_index), a
	ld	(#_note_frames_left),a
;src/audio.c:252: }
	ret
;src/audio.c:254: uint8_t dt_audio_get_home_record(void) BANKED {
;	---------------------------------
; Function dt_audio_get_home_record
; ---------------------------------
	b_dt_audio_get_home_record	= 255
_dt_audio_get_home_record::
;src/audio.c:255: return home_record;
	ld	a, (_home_record)
;src/audio.c:256: }
	ret
;src/audio.c:258: void dt_audio_sync(uint8_t room_id) BANKED {
;	---------------------------------
; Function dt_audio_sync
; ---------------------------------
	b_dt_audio_sync	= 255
_dt_audio_sync::
;src/audio.c:259: if ((room_id == ROOM_APARTMENT) && (home_record != RECORD_NONE)) {
	ldhl	sp,	#6
	ld	a, (hl)
	dec	a
	jr	NZ, 00104$
	ld	a, (#_home_record)
	or	a, a
	jr	Z, 00104$
;src/audio.c:260: if (current_track != home_record) {
	ld	a, (#_current_track)
	ld	hl, #_home_record
	sub	a, (hl)
	ret	Z
;src/audio.c:261: current_track = home_record;
	ld	a, (#_home_record)
	ld	(#_current_track),a
;src/audio.c:262: note_index = 0u;
;src/audio.c:263: note_frames_left = 0u;
	xor	a, a
	ld	(#_note_index), a
	ld	(#_note_frames_left),a
	ret
00104$:
;src/audio.c:266: dt_audio_stop_music();
	ld	e, #b_dt_audio_stop_music
	ld	hl, #_dt_audio_stop_music
;src/audio.c:268: }
	jp  ___sdcc_bcall_ehl
	.area _CODE_255
	.area _INITIALIZER
	.area _CABS (ABS)
