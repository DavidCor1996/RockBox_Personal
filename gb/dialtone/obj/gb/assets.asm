;--------------------------------------------------------
; File Created by SDCC : free open source ISO C Compiler
; Version 4.5.1 #15267 (Linux)
;--------------------------------------------------------
	.module assets
	
;--------------------------------------------------------
; Public variables in this module
;--------------------------------------------------------
	.globl _sprite_tiles
	.globl _apartment_decor_tiles
	.globl _bg_tiles
	.globl b_dt_init_assets
	.globl _dt_init_assets
;--------------------------------------------------------
; special function registers
;--------------------------------------------------------
	.area _HRAM
;--------------------------------------------------------
; ram data
;--------------------------------------------------------
	.area _DATA
_bg_tiles::
	.ds 512
_apartment_decor_tiles::
	.ds 80
_sprite_tiles::
	.ds 832
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
;src/assets.c:859: static uint8_t shade_bits(char c) {
;	---------------------------------
; Function shade_bits
; ---------------------------------
_shade_bits:
;src/assets.c:860: switch (c) {
	cp	a, #0x23
	jr	Z, 00103$
	cp	a, #0x2a
	jr	Z, 00102$
	sub	a, #0x2b
	jr	NZ, 00104$
;src/assets.c:861: case '+': return 1u;
	ld	a, #0x01
	ret
;src/assets.c:862: case '*': return 2u;
00102$:
	ld	a, #0x02
	ret
;src/assets.c:863: case '#': return 3u;
00103$:
	ld	a, #0x03
	ret
;src/assets.c:864: default: return 0u;
00104$:
	xor	a, a
;src/assets.c:865: }
;src/assets.c:866: }
	ret
_apartment_decor_sources:
	.db #0x0e	; 14
	.db #0x13	; 19
	.db #0x10	; 16
	.db #0x15	; 21
	.db #0x18	; 24
_bg_patterns:
	.dw __str_0
	.dw __str_1
	.dw __str_2
	.dw __str_3
	.dw __str_4
	.dw __str_5
	.dw __str_6
	.dw __str_7
	.dw __str_3
	.dw __str_8
	.dw __str_9
	.dw __str_9
	.dw __str_10
	.dw __str_9
	.dw __str_8
	.dw __str_3
	.dw __str_3
	.dw __str_11
	.dw __str_12
	.dw __str_13
	.dw __str_11
	.dw __str_12
	.dw __str_13
	.dw __str_3
	.dw __str_12
	.dw __str_14
	.dw __str_15
	.dw __str_14
	.dw __str_15
	.dw __str_14
	.dw __str_15
	.dw __str_12
	.dw __str_12
	.dw __str_16
	.dw __str_16
	.dw __str_17
	.dw __str_16
	.dw __str_16
	.dw __str_17
	.dw __str_12
	.dw __str_12
	.dw __str_18
	.dw __str_19
	.dw __str_16
	.dw __str_16
	.dw __str_20
	.dw __str_18
	.dw __str_12
	.dw __str_12
	.dw __str_21
	.dw __str_21
	.dw __str_21
	.dw __str_18
	.dw __str_21
	.dw __str_21
	.dw __str_12
	.dw __str_22
	.dw __str_23
	.dw __str_24
	.dw __str_18
	.dw __str_18
	.dw __str_24
	.dw __str_23
	.dw __str_22
	.dw __str_11
	.dw __str_25
	.dw __str_26
	.dw __str_12
	.dw __str_12
	.dw __str_22
	.dw __str_27
	.dw __str_28
	.dw __str_28
	.dw __str_28
	.dw __str_29
	.dw __str_29
	.dw __str_28
	.dw __str_28
	.dw __str_30
	.dw __str_31
	.dw __str_22
	.dw __str_23
	.dw __str_18
	.dw __str_16
	.dw __str_16
	.dw __str_18
	.dw __str_23
	.dw __str_22
	.dw __str_29
	.dw __str_22
	.dw __str_32
	.dw __str_22
	.dw __str_32
	.dw __str_22
	.dw __str_32
	.dw __str_29
	.dw __str_33
	.dw __str_33
	.dw __str_33
	.dw __str_33
	.dw __str_33
	.dw __str_33
	.dw __str_33
	.dw __str_34
	.dw __str_12
	.dw __str_18
	.dw __str_18
	.dw __str_24
	.dw __str_18
	.dw __str_18
	.dw __str_35
	.dw __str_36
	.dw __str_22
	.dw __str_32
	.dw __str_37
	.dw __str_32
	.dw __str_32
	.dw __str_37
	.dw __str_32
	.dw __str_22
	.dw __str_22
	.dw __str_32
	.dw __str_31
	.dw __str_22
	.dw __str_27
	.dw __str_35
	.dw __str_3
	.dw __str_3
	.dw __str_28
	.dw __str_29
	.dw __str_30
	.dw __str_31
	.dw __str_22
	.dw __str_28
	.dw __str_27
	.dw __str_36
	.dw __str_35
	.dw __str_38
	.dw __str_39
	.dw __str_35
	.dw __str_27
	.dw __str_29
	.dw __str_28
	.dw __str_3
	.dw __str_22
	.dw __str_23
	.dw __str_18
	.dw __str_23
	.dw __str_29
	.dw __str_27
	.dw __str_35
	.dw __str_3
	.dw __str_22
	.dw __str_23
	.dw __str_21
	.dw __str_18
	.dw __str_24
	.dw __str_16
	.dw __str_18
	.dw __str_22
	.dw __str_28
	.dw __str_29
	.dw __str_28
	.dw __str_28
	.dw __str_28
	.dw __str_30
	.dw __str_32
	.dw __str_30
	.dw __str_22
	.dw __str_32
	.dw __str_40
	.dw __str_32
	.dw __str_22
	.dw __str_27
	.dw __str_35
	.dw __str_3
	.dw __str_22
	.dw __str_23
	.dw __str_41
	.dw __str_42
	.dw __str_41
	.dw __str_18
	.dw __str_23
	.dw __str_22
	.dw __str_22
	.dw __str_23
	.dw __str_16
	.dw __str_41
	.dw __str_42
	.dw __str_18
	.dw __str_31
	.dw __str_27
	.dw __str_22
	.dw __str_23
	.dw __str_18
	.dw __str_18
	.dw __str_24
	.dw __str_23
	.dw __str_36
	.dw __str_36
	.dw __str_22
	.dw __str_32
	.dw __str_22
	.dw __str_27
	.dw __str_35
	.dw __str_36
	.dw __str_36
	.dw __str_3
	.dw __str_22
	.dw __str_23
	.dw __str_18
	.dw __str_16
	.dw __str_18
	.dw __str_23
	.dw __str_30
	.dw __str_35
	.dw __str_3
	.dw __str_29
	.dw __str_31
	.dw __str_23
	.dw __str_31
	.dw __str_29
	.dw __str_28
	.dw __str_33
	.dw __str_8
	.dw __str_43
	.dw __str_22
	.dw __str_44
	.dw __str_29
	.dw __str_43
	.dw __str_36
	.dw __str_45
	.dw __str_29
	.dw __str_22
	.dw __str_32
	.dw __str_32
	.dw __str_44
	.dw __str_32
	.dw __str_35
	.dw __str_36
	.dw __str_3
	.dw __str_28
	.dw __str_29
	.dw __str_31
	.dw __str_31
	.dw __str_29
	.dw __str_28
	.dw __str_3
	.dw __str_22
	.dw __str_32
	.dw __str_22
	.dw __str_44
	.dw __str_22
	.dw __str_44
	.dw __str_22
	.dw __str_33
_sprite_patterns:
	.dw __str_46
	.dw __str_47
	.dw __str_48
	.dw __str_49
	.dw __str_50
	.dw __str_51
	.dw __str_52
	.dw __str_50
	.dw __str_53
	.dw __str_54
	.dw __str_55
	.dw __str_56
	.dw __str_57
	.dw __str_58
	.dw __str_59
	.dw __str_57
	.dw __str_60
	.dw __str_61
	.dw __str_62
	.dw __str_63
	.dw __str_63
	.dw __str_64
	.dw __str_28
	.dw __str_3
	.dw __str_65
	.dw __str_66
	.dw __str_67
	.dw __str_68
	.dw __str_68
	.dw __str_69
	.dw __str_28
	.dw __str_3
	.dw __str_46
	.dw __str_47
	.dw __str_48
	.dw __str_49
	.dw __str_50
	.dw __str_51
	.dw __str_52
	.dw __str_50
	.dw __str_53
	.dw __str_54
	.dw __str_55
	.dw __str_56
	.dw __str_57
	.dw __str_58
	.dw __str_59
	.dw __str_57
	.dw __str_60
	.dw __str_61
	.dw __str_62
	.dw __str_63
	.dw __str_70
	.dw __str_71
	.dw __str_33
	.dw __str_3
	.dw __str_65
	.dw __str_66
	.dw __str_67
	.dw __str_72
	.dw __str_73
	.dw __str_74
	.dw __str_75
	.dw __str_3
	.dw __str_46
	.dw __str_47
	.dw __str_76
	.dw __str_77
	.dw __str_78
	.dw __str_79
	.dw __str_80
	.dw __str_78
	.dw __str_53
	.dw __str_54
	.dw __str_81
	.dw __str_82
	.dw __str_83
	.dw __str_84
	.dw __str_85
	.dw __str_83
	.dw __str_61
	.dw __str_86
	.dw __str_62
	.dw __str_63
	.dw __str_63
	.dw __str_64
	.dw __str_28
	.dw __str_3
	.dw __str_66
	.dw __str_87
	.dw __str_67
	.dw __str_68
	.dw __str_68
	.dw __str_69
	.dw __str_28
	.dw __str_3
	.dw __str_46
	.dw __str_47
	.dw __str_76
	.dw __str_77
	.dw __str_78
	.dw __str_79
	.dw __str_80
	.dw __str_78
	.dw __str_53
	.dw __str_54
	.dw __str_81
	.dw __str_82
	.dw __str_83
	.dw __str_84
	.dw __str_85
	.dw __str_83
	.dw __str_61
	.dw __str_86
	.dw __str_62
	.dw __str_88
	.dw __str_89
	.dw __str_71
	.dw __str_33
	.dw __str_3
	.dw __str_66
	.dw __str_87
	.dw __str_67
	.dw __str_68
	.dw __str_90
	.dw __str_74
	.dw __str_75
	.dw __str_3
	.dw __str_46
	.dw __str_47
	.dw __str_91
	.dw __str_92
	.dw __str_50
	.dw __str_93
	.dw __str_51
	.dw __str_50
	.dw __str_94
	.dw __str_95
	.dw __str_96
	.dw __str_97
	.dw __str_66
	.dw __str_98
	.dw __str_87
	.dw __str_99
	.dw __str_60
	.dw __str_61
	.dw __str_100
	.dw __str_88
	.dw __str_63
	.dw __str_64
	.dw __str_28
	.dw __str_3
	.dw __str_101
	.dw __str_102
	.dw __str_103
	.dw __str_104
	.dw __str_104
	.dw __str_105
	.dw __str_28
	.dw __str_3
	.dw __str_46
	.dw __str_47
	.dw __str_91
	.dw __str_92
	.dw __str_50
	.dw __str_93
	.dw __str_51
	.dw __str_50
	.dw __str_94
	.dw __str_95
	.dw __str_96
	.dw __str_97
	.dw __str_66
	.dw __str_98
	.dw __str_87
	.dw __str_99
	.dw __str_60
	.dw __str_61
	.dw __str_100
	.dw __str_88
	.dw __str_106
	.dw __str_69
	.dw __str_107
	.dw __str_3
	.dw __str_101
	.dw __str_102
	.dw __str_103
	.dw __str_108
	.dw __str_109
	.dw __str_110
	.dw __str_28
	.dw __str_3
	.dw __str_75
	.dw __str_111
	.dw __str_112
	.dw __str_113
	.dw __str_114
	.dw __str_115
	.dw __str_93
	.dw __str_51
	.dw __str_33
	.dw __str_116
	.dw __str_117
	.dw __str_118
	.dw __str_119
	.dw __str_120
	.dw __str_56
	.dw __str_87
	.dw __str_50
	.dw __str_86
	.dw __str_121
	.dw __str_122
	.dw __str_123
	.dw __str_64
	.dw __str_28
	.dw __str_3
	.dw __str_99
	.dw __str_124
	.dw __str_125
	.dw __str_126
	.dw __str_98
	.dw __str_127
	.dw __str_28
	.dw __str_3
	.dw __str_128
	.dw __str_129
	.dw __str_91
	.dw __str_92
	.dw __str_50
	.dw __str_130
	.dw __str_93
	.dw __str_115
	.dw __str_131
	.dw __str_132
	.dw __str_133
	.dw __str_119
	.dw __str_97
	.dw __str_134
	.dw __str_135
	.dw __str_87
	.dw __str_92
	.dw __str_136
	.dw __str_63
	.dw __str_63
	.dw __str_137
	.dw __str_138
	.dw __str_28
	.dw __str_3
	.dw __str_99
	.dw __str_139
	.dw __str_72
	.dw __str_68
	.dw __str_140
	.dw __str_141
	.dw __str_28
	.dw __str_3
	.dw __str_46
	.dw __str_142
	.dw __str_61
	.dw __str_86
	.dw __str_50
	.dw __str_106
	.dw __str_93
	.dw __str_115
	.dw __str_94
	.dw __str_143
	.dw __str_102
	.dw __str_124
	.dw __str_66
	.dw __str_98
	.dw __str_144
	.dw __str_120
	.dw __str_145
	.dw __str_145
	.dw __str_145
	.dw __str_146
	.dw __str_121
	.dw __str_111
	.dw __str_28
	.dw __str_3
	.dw __str_147
	.dw __str_147
	.dw __str_147
	.dw __str_148
	.dw __str_126
	.dw __str_149
	.dw __str_28
	.dw __str_3
	.dw __str_150
	.dw __str_28
	.dw __str_149
	.dw __str_151
	.dw __str_115
	.dw __str_93
	.dw __str_106
	.dw __str_50
	.dw __str_152
	.dw __str_28
	.dw __str_153
	.dw __str_154
	.dw __str_120
	.dw __str_144
	.dw __str_98
	.dw __str_66
	.dw __str_60
	.dw __str_114
	.dw __str_88
	.dw __str_63
	.dw __str_63
	.dw __str_64
	.dw __str_28
	.dw __str_3
	.dw __str_65
	.dw __str_119
	.dw __str_72
	.dw __str_68
	.dw __str_68
	.dw __str_127
	.dw __str_28
	.dw __str_3
	.dw __str_75
	.dw __str_111
	.dw __str_91
	.dw __str_113
	.dw __str_114
	.dw __str_115
	.dw __str_93
	.dw __str_51
	.dw __str_33
	.dw __str_116
	.dw __str_155
	.dw __str_118
	.dw __str_119
	.dw __str_120
	.dw __str_135
	.dw __str_87
	.dw __str_50
	.dw __str_86
	.dw __str_121
	.dw __str_122
	.dw __str_156
	.dw __str_64
	.dw __str_28
	.dw __str_3
	.dw __str_99
	.dw __str_124
	.dw __str_125
	.dw __str_126
	.dw __str_98
	.dw __str_69
	.dw __str_110
	.dw __str_28
	.dw __str_33
	.dw __str_157
	.dw __str_158
	.dw __str_159
	.dw __str_159
	.dw __str_158
	.dw __str_157
	.dw __str_33
	.dw __str_33
	.dw __str_160
	.dw __str_158
	.dw __str_161
	.dw __str_161
	.dw __str_158
	.dw __str_160
	.dw __str_33
	.dw __str_28
	.dw __str_30
	.dw __str_32
	.dw __str_31
	.dw __str_30
	.dw __str_31
	.dw __str_32
	.dw __str_30
	.dw __str_30
	.dw __str_31
	.dw __str_32
	.dw __str_30
	.dw __str_31
	.dw __str_30
	.dw __str_32
	.dw __str_31
	.dw __str_34
	.dw __str_162
	.dw __str_158
	.dw __str_163
	.dw __str_164
	.dw __str_23
	.dw __str_31
	.dw __str_29
	.dw __str_34
	.dw __str_162
	.dw __str_165
	.dw __str_166
	.dw __str_167
	.dw __str_147
	.dw __str_31
	.dw __str_29
	.dw __str_29
	.dw __str_31
	.dw __str_32
	.dw __str_168
	.dw __str_32
	.dw __str_31
	.dw __str_30
	.dw __str_29
	.dw __str_29
	.dw __str_32
	.dw __str_31
	.dw __str_32
	.dw __str_168
	.dw __str_32
	.dw __str_31
	.dw __str_29
__str_0:
	.ascii "..+....."
	.db 0x00
__str_1:
	.ascii ".+..+..."
	.db 0x00
__str_2:
	.ascii "...+..+."
	.db 0x00
__str_3:
	.ascii "........"
	.db 0x00
__str_4:
	.ascii ".+....+."
	.db 0x00
__str_5:
	.ascii "...+...."
	.db 0x00
__str_6:
	.ascii "....+..."
	.db 0x00
__str_7:
	.ascii ".+......"
	.db 0x00
__str_8:
	.ascii "..++++.."
	.db 0x00
__str_9:
	.ascii ".+****+."
	.db 0x00
__str_10:
	.ascii "++****++"
	.db 0x00
__str_11:
	.ascii "++++++++"
	.db 0x00
__str_12:
	.ascii "########"
	.db 0x00
__str_13:
	.ascii "********"
	.db 0x00
__str_14:
	.ascii "##**##**"
	.db 0x00
__str_15:
	.ascii "#*##**##"
	.db 0x00
__str_16:
	.ascii "#++##++#"
	.db 0x00
__str_17:
	.ascii "#******#"
	.db 0x00
__str_18:
	.ascii "#++++++#"
	.db 0x00
__str_19:
	.ascii "#+####+#"
	.db 0x00
__str_20:
	.ascii "#++##*+#"
	.db 0x00
__str_21:
	.ascii "#*+##+*#"
	.db 0x00
__str_22:
	.ascii ".######."
	.db 0x00
__str_23:
	.ascii "##++++##"
	.db 0x00
__str_24:
	.ascii "#*####*#"
	.db 0x00
__str_25:
	.ascii "##++##++"
	.db 0x00
__str_26:
	.ascii "..######"
	.db 0x00
__str_27:
	.ascii "..#..#.."
	.db 0x00
__str_28:
	.ascii "...##..."
	.db 0x00
__str_29:
	.ascii "..####.."
	.db 0x00
__str_30:
	.ascii "..#++#.."
	.db 0x00
__str_31:
	.ascii ".##++##."
	.db 0x00
__str_32:
	.ascii ".#++++#."
	.db 0x00
__str_33:
	.ascii "..##...."
	.db 0x00
__str_34:
	.ascii ".####..."
	.db 0x00
__str_35:
	.ascii ".##..##."
	.db 0x00
__str_36:
	.ascii ".#....#."
	.db 0x00
__str_37:
	.ascii ".#*++*#."
	.db 0x00
__str_38:
	.ascii "##+##+##"
	.db 0x00
__str_39:
	.ascii "#++..++#"
	.db 0x00
__str_40:
	.ascii ".#+##+#."
	.db 0x00
__str_41:
	.ascii "#++**++#"
	.db 0x00
__str_42:
	.ascii "#*++++*#"
	.db 0x00
__str_43:
	.ascii ".++##++."
	.db 0x00
__str_44:
	.ascii ".#*##*#."
	.db 0x00
__str_45:
	.ascii "#......#"
	.db 0x00
__str_46:
	.ascii ".....###"
	.db 0x00
__str_47:
	.ascii "....##**"
	.db 0x00
__str_48:
	.ascii "...##*##"
	.db 0x00
__str_49:
	.ascii "...#*#++"
	.db 0x00
__str_50:
	.ascii "..##*+++"
	.db 0x00
__str_51:
	.ascii "..#*+++#"
	.db 0x00
__str_52:
	.ascii "..#*++#+"
	.db 0x00
__str_53:
	.ascii "####...."
	.db 0x00
__str_54:
	.ascii "***##..."
	.db 0x00
__str_55:
	.ascii "+##*##.."
	.db 0x00
__str_56:
	.ascii "+++#*#.."
	.db 0x00
__str_57:
	.ascii "++++*##."
	.db 0x00
__str_58:
	.ascii "#++++*#."
	.db 0x00
__str_59:
	.ascii "+#++*#.."
	.db 0x00
__str_60:
	.ascii "...#**+#"
	.db 0x00
__str_61:
	.ascii "...##*++"
	.db 0x00
__str_62:
	.ascii "..##*+*#"
	.db 0x00
__str_63:
	.ascii "..#*++*#"
	.db 0x00
__str_64:
	.ascii "..#**##."
	.db 0x00
__str_65:
	.ascii "#+**#..."
	.db 0x00
__str_66:
	.ascii "+++*##.."
	.db 0x00
__str_67:
	.ascii "#*+*##.."
	.db 0x00
__str_68:
	.ascii "#*++*#.."
	.db 0x00
__str_69:
	.ascii ".##**#.."
	.db 0x00
__str_70:
	.ascii ".##*++##"
	.db 0x00
__str_71:
	.ascii ".#**##.."
	.db 0x00
__str_72:
	.ascii "#*++##.."
	.db 0x00
__str_73:
	.ascii "..#++*#."
	.db 0x00
__str_74:
	.ascii "..##**#."
	.db 0x00
__str_75:
	.ascii "....##.."
	.db 0x00
__str_76:
	.ascii "...##***"
	.db 0x00
__str_77:
	.ascii "...#**##"
	.db 0x00
__str_78:
	.ascii "..##**++"
	.db 0x00
__str_79:
	.ascii "..#**+++"
	.db 0x00
__str_80:
	.ascii "..#**++*"
	.db 0x00
__str_81:
	.ascii "****##.."
	.db 0x00
__str_82:
	.ascii "###**#.."
	.db 0x00
__str_83:
	.ascii "+++**##."
	.db 0x00
__str_84:
	.ascii "++++**#."
	.db 0x00
__str_85:
	.ascii "**++**#."
	.db 0x00
__str_86:
	.ascii "...#*++#"
	.db 0x00
__str_87:
	.ascii "#+++*#.."
	.db 0x00
__str_88:
	.ascii "..##++*#"
	.db 0x00
__str_89:
	.ascii ".#*++#.."
	.db 0x00
__str_90:
	.ascii "##++*##."
	.db 0x00
__str_91:
	.ascii "...##**#"
	.db 0x00
__str_92:
	.ascii "...#**++"
	.db 0x00
__str_93:
	.ascii "..#*++++"
	.db 0x00
__str_94:
	.ascii "###....."
	.db 0x00
__str_95:
	.ascii "**##...."
	.db 0x00
__str_96:
	.ascii "#+*##..."
	.db 0x00
__str_97:
	.ascii "+++*#..."
	.db 0x00
__str_98:
	.ascii "##++*#.."
	.db 0x00
__str_99:
	.ascii "++++##.."
	.db 0x00
__str_100:
	.ascii "...#*+*#"
	.db 0x00
__str_101:
	.ascii "#+*#...."
	.db 0x00
__str_102:
	.ascii "++*##..."
	.db 0x00
__str_103:
	.ascii "#*+*#..."
	.db 0x00
__str_104:
	.ascii "#*++#..."
	.db 0x00
__str_105:
	.ascii ".##*#..."
	.db 0x00
__str_106:
	.ascii "..#*++##"
	.db 0x00
__str_107:
	.ascii ".##....."
	.db 0x00
__str_108:
	.ascii "#*+##..."
	.db 0x00
__str_109:
	.ascii "..#+*#.."
	.db 0x00
__str_110:
	.ascii "..##*#.."
	.db 0x00
__str_111:
	.ascii "...#**#."
	.db 0x00
__str_112:
	.ascii "...#***#"
	.db 0x00
__str_113:
	.ascii "....#*++"
	.db 0x00
__str_114:
	.ascii "...##+++"
	.db 0x00
__str_115:
	.ascii "..##*++#"
	.db 0x00
__str_116:
	.ascii ".#**#..."
	.db 0x00
__str_117:
	.ascii "#***#..."
	.db 0x00
__str_118:
	.ascii "++*#...."
	.db 0x00
__str_119:
	.ascii "+++##..."
	.db 0x00
__str_120:
	.ascii "#++*##.."
	.db 0x00
__str_121:
	.ascii "...##++*"
	.db 0x00
__str_122:
	.ascii "..##+++*"
	.db 0x00
__str_123:
	.ascii "..#*+++*"
	.db 0x00
__str_124:
	.ascii "#++*#..."
	.db 0x00
__str_125:
	.ascii "##+##..."
	.db 0x00
__str_126:
	.ascii "##++##.."
	.db 0x00
__str_127:
	.ascii "...**#.."
	.db 0x00
__str_128:
	.ascii "......##"
	.db 0x00
__str_129:
	.ascii "....###*"
	.db 0x00
__str_130:
	.ascii "..#**+##"
	.db 0x00
__str_131:
	.ascii "##......"
	.db 0x00
__str_132:
	.ascii "*##....."
	.db 0x00
__str_133:
	.ascii "++##...."
	.db 0x00
__str_134:
	.ascii "##+*##.."
	.db 0x00
__str_135:
	.ascii "+++**#.."
	.db 0x00
__str_136:
	.ascii "..##*+##"
	.db 0x00
__str_137:
	.ascii "..##+*#."
	.db 0x00
__str_138:
	.ascii "...#**.."
	.db 0x00
__str_139:
	.ascii "##+*#..."
	.db 0x00
__str_140:
	.ascii ".#*+##.."
	.db 0x00
__str_141:
	.ascii "..**#..."
	.db 0x00
__str_142:
	.ascii "....##*#"
	.db 0x00
__str_143:
	.ascii "#*##...."
	.db 0x00
__str_144:
	.ascii "++++*#.."
	.db 0x00
__str_145:
	.ascii "..#+++++"
	.db 0x00
__str_146:
	.ascii "..##++++"
	.db 0x00
__str_147:
	.ascii "##++++#."
	.db 0x00
__str_148:
	.ascii "##+++##."
	.db 0x00
__str_149:
	.ascii "...#*#.."
	.db 0x00
__str_150:
	.ascii "....#..."
	.db 0x00
__str_151:
	.ascii "...#*###"
	.db 0x00
__str_152:
	.ascii "...#...."
	.db 0x00
__str_153:
	.ascii "..#*#..."
	.db 0x00
__str_154:
	.ascii "###*#..."
	.db 0x00
__str_155:
	.ascii "#**##..."
	.db 0x00
__str_156:
	.ascii "..#*++#*"
	.db 0x00
__str_157:
	.ascii ".#++##.."
	.db 0x00
__str_158:
	.ascii "#+++++#."
	.db 0x00
__str_159:
	.ascii ".#++#++#"
	.db 0x00
__str_160:
	.ascii ".##++#.."
	.db 0x00
__str_161:
	.ascii "#++#++#."
	.db 0x00
__str_162:
	.ascii "##++++.."
	.db 0x00
__str_163:
	.ascii "##****##"
	.db 0x00
__str_164:
	.ascii ".#+++++#"
	.db 0x00
__str_165:
	.ascii "#+++++##"
	.db 0x00
__str_166:
	.ascii "##****+#"
	.db 0x00
__str_167:
	.ascii ".#++++##"
	.db 0x00
__str_168:
	.ascii "##+**+##"
	.db 0x00
;src/assets.c:868: static void encode_tile(uint8_t *dst, const char *const rows[8]) {
;	---------------------------------
; Function encode_tile
; ---------------------------------
_encode_tile:
	add	sp, #-12
	ldhl	sp,	#8
	ld	a, e
	ld	(hl+), a
	ld	(hl), d
	ldhl	sp,	#6
	ld	a, c
	ld	(hl+), a
	ld	(hl), b
;src/assets.c:872: for (y = 0u; y != 8u; ++y) {
	ldhl	sp,	#10
	ld	(hl), #0x00
00105$:
;src/assets.c:873: uint8_t lo = 0u;
	ldhl	sp,	#3
;src/assets.c:874: uint8_t hi = 0u;
	xor	a, a
	ld	(hl-), a
	ld	(hl), a
;src/assets.c:875: for (x = 0u; x != 8u; ++x) {
	ldhl	sp,	#11
	ld	(hl), #0x00
00103$:
;src/assets.c:876: uint8_t shade = shade_bits(rows[y][x]);
	ldhl	sp,	#10
	ld	a, (hl)
	ld	d, #0x00
	add	a, a
	rl	d
	ld	e, a
	ldhl	sp,	#6
	ld	a,	(hl+)
	ld	h, (hl)
	ld	l, a
	add	hl, de
	ld	a, (hl+)
	ld	c, a
	ld	b, (hl)
	ldhl	sp,	#11
	ld	l, (hl)
	ld	h, #0x00
	add	hl, bc
	ld	c, l
	ld	b, h
	ld	a, (bc)
	call	_shade_bits
	ldhl	sp,	#5
	ld	(hl), a
;src/assets.c:877: lo <<= 1;
	ldhl	sp,	#3
	sla	(hl)
;src/assets.c:878: hi <<= 1;
	dec	hl
	ld	a, (hl+)
	inc	hl
	add	a, a
;src/assets.c:879: lo |= (shade & 1u);
	ld	(hl+), a
	ld	a, (hl-)
	dec	hl
	and	a, #0x01
	or	a, (hl)
;src/assets.c:880: hi |= (shade >> 1);
	ld	(hl+), a
	inc	hl
	ld	a, (hl)
	srl	a
	ld	(hl-), a
	ld	a, (hl+)
	or	a, (hl)
	ldhl	sp,	#2
	ld	(hl), a
;src/assets.c:875: for (x = 0u; x != 8u; ++x) {
	ldhl	sp,	#11
	inc	(hl)
	ld	a, (hl)
	sub	a, #0x08
	jr	NZ, 00103$
;src/assets.c:882: dst[y * 2u] = lo;
	ldhl	sp,	#10
	ld	c, (hl)
	xor	a, a
	sla	c
	adc	a, a
	ldhl	sp,	#0
	ld	(hl), c
	inc	hl
	ld	(hl), a
	ldhl	sp,#8
	ld	a, (hl+)
	ld	e, a
	ld	d, (hl)
	pop	hl
	push	hl
	add	hl, de
	push	hl
	ld	a, l
	ldhl	sp,	#6
	ld	(hl), a
	pop	hl
	ld	a, h
	ldhl	sp,	#5
	ld	(hl-), a
	ld	a, (hl+)
	ld	e, a
	ld	a, (hl-)
	dec	hl
	ld	d, a
	ld	a, (hl)
	ld	(de), a
;src/assets.c:883: dst[y * 2u + 1u] = hi;
	pop	de
	push	de
	ld	l, e
	ld	h, d
	inc	hl
	push	hl
	ld	a, l
	ldhl	sp,	#6
	ld	(hl), a
	pop	hl
	ld	a, h
	ldhl	sp,	#5
	ld	(hl-), a
	ld	a, (hl+)
	ld	e, a
	ld	d, (hl)
	ldhl	sp,	#8
	ld	a,	(hl+)
	ld	h, (hl)
	ld	l, a
	add	hl, de
	inc	sp
	inc	sp
	ld	e, l
	ld	d, h
	push	de
	ldhl	sp,	#2
	ld	a, (hl)
	ld	(de), a
;src/assets.c:872: for (y = 0u; y != 8u; ++y) {
	ldhl	sp,	#10
	inc	(hl)
	ld	a, (hl)
	sub	a, #0x08
	jp	NZ, 00105$
;src/assets.c:885: }
	add	sp, #12
	ret
;src/assets.c:887: void dt_init_assets(void) BANKED {
;	---------------------------------
; Function dt_init_assets
; ---------------------------------
	b_dt_init_assets	= 255
_dt_init_assets::
;src/assets.c:890: for (i = 0u; i != BG_TILE_COUNT; ++i) {
	ld	d, #0x00
00104$:
;src/assets.c:891: encode_tile(&bg_tiles[i * 16u], bg_patterns[i]);
	ld	l, d
	ld	h, #0x00
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	ld	bc, #_bg_patterns
	add	hl, bc
	ld	c, l
	ld	b, h
	ld	l, d
	ld	h, #0x00
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	ld	a, l
	add	a, #<(_bg_tiles)
	ld	e, a
	ld	a, h
	adc	a, #>(_bg_tiles)
	push	de
	ld	d, a
	call	_encode_tile
	pop	de
;src/assets.c:890: for (i = 0u; i != BG_TILE_COUNT; ++i) {
	inc	d
	ld	a, d
;src/assets.c:893: for (i = 0u; i != APARTMENT_DECOR_TILE_COUNT; ++i) {
	sub	a, #0x20
	jr	NZ, 00104$
	ld	e, a
00106$:
;src/assets.c:894: encode_tile(&apartment_decor_tiles[i * 16u], bg_patterns[apartment_decor_sources[i]]);
	ld	hl, #_apartment_decor_sources
	ld	d, #0x00
	add	hl, de
	ld	l, (hl)
	ld	h, #0x00
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	ld	bc, #_bg_patterns
	add	hl, bc
	ld	c, l
	ld	b, h
	ld	l, e
	ld	h, #0x00
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	ld	a, l
	add	a, #<(_apartment_decor_tiles)
	ld	d, a
	ld	a, h
	adc	a, #>(_apartment_decor_tiles)
	push	de
	ld	e, d
	ld	d, a
	call	_encode_tile
	pop	de
;src/assets.c:893: for (i = 0u; i != APARTMENT_DECOR_TILE_COUNT; ++i) {
	inc	e
	ld	a, e
;src/assets.c:896: for (i = 0u; i != SPRITE_TILE_COUNT; ++i) {
	sub	a, #0x05
	jr	NZ, 00106$
	ld	d, a
00108$:
;src/assets.c:897: encode_tile(&sprite_tiles[i * 16u], sprite_patterns[i]);
	ld	l, d
	ld	h, #0x00
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	ld	bc, #_sprite_patterns
	add	hl, bc
	ld	c, l
	ld	b, h
	ld	l, d
	ld	h, #0x00
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	ld	a, l
	add	a, #<(_sprite_tiles)
	ld	e, a
	ld	a, h
	adc	a, #>(_sprite_tiles)
	push	de
	ld	d, a
	call	_encode_tile
	pop	de
;src/assets.c:896: for (i = 0u; i != SPRITE_TILE_COUNT; ++i) {
	inc	d
	ld	a, d
	sub	a, #0x34
	jr	NZ, 00108$
;src/assets.c:899: }
	ret
	.area _CODE_255
	.area _INITIALIZER
	.area _CABS (ABS)
