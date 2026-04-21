;--------------------------------------------------------
; File Created by SDCC : free open source ISO C Compiler
; Version 4.5.1 #15267 (Linux)
;--------------------------------------------------------
	.module world_data
	
;--------------------------------------------------------
; Public variables in this module
;--------------------------------------------------------
	.globl _hotspots
	.globl _doors
	.globl _tasks
	.globl _npcs
	.globl _room_maps
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
	.area _CODE
	.area _CODE
_room_maps:
	.dw __str_0
	.dw __str_1
	.dw __str_2
	.dw __str_3
	.dw __str_4
	.dw __str_5
	.dw __str_6
	.dw __str_7
	.dw __str_8
	.dw __str_9
	.dw __str_10
	.dw __str_11
	.dw __str_12
	.dw __str_13
	.dw __str_14
	.dw __str_15
	.dw __str_16
	.dw __str_0
	.dw __str_0
	.dw __str_17
	.dw __str_18
	.dw __str_19
	.dw __str_18
	.dw __str_20
	.dw __str_18
	.dw __str_21
	.dw __str_22
	.dw __str_23
	.dw __str_18
	.dw __str_18
	.dw __str_18
	.dw __str_18
	.dw __str_24
	.dw __str_18
	.dw __str_18
	.dw __str_0
	.dw __str_0
	.dw __str_25
	.dw __str_26
	.dw __str_27
	.dw __str_26
	.dw __str_28
	.dw __str_26
	.dw __str_29
	.dw __str_30
	.dw __str_26
	.dw __str_26
	.dw __str_31
	.dw __str_26
	.dw __str_26
	.dw __str_32
	.dw __str_26
	.dw __str_25
	.dw __str_0
	.dw __str_0
	.dw __str_33
	.dw __str_34
	.dw __str_35
	.dw __str_34
	.dw __str_36
	.dw __str_34
	.dw __str_37
	.dw __str_38
	.dw __str_34
	.dw __str_34
	.dw __str_39
	.dw __str_34
	.dw __str_34
	.dw __str_40
	.dw __str_34
	.dw __str_33
	.dw __str_0
	.dw __str_0
	.dw __str_33
	.dw __str_34
	.dw __str_41
	.dw __str_34
	.dw __str_42
	.dw __str_34
	.dw __str_43
	.dw __str_44
	.dw __str_34
	.dw __str_34
	.dw __str_34
	.dw __str_34
	.dw __str_34
	.dw __str_40
	.dw __str_34
	.dw __str_33
	.dw __str_0
	.dw __str_0
	.dw __str_45
	.dw __str_46
	.dw __str_47
	.dw __str_48
	.dw __str_47
	.dw __str_49
	.dw __str_50
	.dw __str_47
	.dw __str_51
	.dw __str_47
	.dw __str_47
	.dw __str_47
	.dw __str_47
	.dw __str_52
	.dw __str_47
	.dw __str_45
	.dw __str_0
	.dw __str_0
	.dw __str_53
	.dw __str_18
	.dw __str_54
	.dw __str_55
	.dw __str_56
	.dw __str_57
	.dw __str_18
	.dw __str_58
	.dw __str_58
	.dw __str_18
	.dw __str_59
	.dw __str_60
	.dw __str_59
	.dw __str_18
	.dw __str_61
	.dw __str_18
	.dw __str_0
_npcs:
	.db #0x02	; 2
	.db #0x09	; 9
	.db #0x07	; 7
	.db #0x19	; 25
	.dw __str_62
	.db #0x03	; 3
	.db #0x09	; 9
	.db #0x07	; 7
	.db #0x1a	; 26
	.dw __str_63
	.db #0x04	; 4
	.db #0x09	; 9
	.db #0x07	; 7
	.db #0x1b	; 27
	.dw __str_64
	.db #0x05	; 5
	.db #0x0a	; 10
	.db #0x06	; 6
	.db #0x1c	; 28
	.dw __str_65
	.db #0x00	; 0
	.db #0x0e	; 14
	.db #0x0d	; 13
	.db #0x1d	; 29
	.dw __str_66
_tasks:
	.db #0x03	; 3
	.db #0x01	; 1
	.db #0x04	; 4
	.db #0x0c	; 12
	.dw __str_67
	.dw __str_68
	.dw __str_69
	.db #0x00	; 0
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x0a	; 10
	.dw __str_70
	.dw __str_71
	.dw __str_72
	.db #0x01	; 1
	.db #0x04	; 4
	.db #0x01	; 1
	.db #0x0e	; 14
	.dw __str_73
	.dw __str_74
	.dw __str_75
_doors:
	.db #0x00	; 0
	.db #0x03	; 3
	.db #0x01	; 1
	.db #0x02	; 2
	.db #0x09	; 9
	.db #0x0e	; 14
	.db #0x00	; 0
	.db #0x08	; 8
	.db #0x01	; 1
	.db #0x03	; 3
	.db #0x09	; 9
	.db #0x0e	; 14
	.db #0x00	; 0
	.db #0x0d	; 13
	.db #0x01	; 1
	.db #0x05	; 5
	.db #0x0a	; 10
	.db #0x0e	; 14
	.db #0x00	; 0
	.db #0x05	; 5
	.db #0x08	; 8
	.db #0x01	; 1
	.db #0x10	; 16
	.db #0x0e	; 14
	.db #0x00	; 0
	.db #0x10	; 16
	.db #0x08	; 8
	.db #0x04	; 4
	.db #0x09	; 9
	.db #0x0e	; 14
	.db #0x00	; 0
	.db #0x09	; 9
	.db #0x0f	; 15
	.db #0x06	; 6
	.db #0x09	; 9
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x09	; 9
	.db #0x0e	; 14
	.db #0x00	; 0
	.db #0x03	; 3
	.db #0x02	; 2
	.db #0x03	; 3
	.db #0x09	; 9
	.db #0x0e	; 14
	.db #0x00	; 0
	.db #0x08	; 8
	.db #0x02	; 2
	.db #0x05	; 5
	.db #0x0a	; 10
	.db #0x0e	; 14
	.db #0x00	; 0
	.db #0x0d	; 13
	.db #0x02	; 2
	.db #0x01	; 1
	.db #0x10	; 16
	.db #0x0e	; 14
	.db #0x00	; 0
	.db #0x05	; 5
	.db #0x09	; 9
	.db #0x04	; 4
	.db #0x09	; 9
	.db #0x0e	; 14
	.db #0x00	; 0
	.db #0x10	; 16
	.db #0x09	; 9
	.db #0x06	; 6
	.db #0x09	; 9
	.db #0x01	; 1
	.db #0x00	; 0
	.db #0x09	; 9
	.db #0x0e	; 14
_hotspots:
	.db #0x01	; 1
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x05	; 5
	.db #0x05	; 5
	.db #0x02	; 2
	.db #0x01	; 1
	.db #0x0a	; 10
	.db #0x05	; 5
	.db #0x02	; 2
	.db #0x01	; 1
	.db #0x0f	; 15
	.db #0x05	; 5
	.db #0x02	; 2
	.db #0x01	; 1
	.db #0x03	; 3
	.db #0x07	; 7
	.db #0x03	; 3
	.db #0x06	; 6
	.db #0x04	; 4
	.db #0x05	; 5
	.db #0x04	; 4
__str_0:
	.ascii "####################"
	.db 0x00
__str_1:
	.ascii "#wwd##wwd##wwd##waa#"
	.db 0x00
__str_2:
	.ascii "#..##..##..##..#uvv#"
	.db 0x00
__str_3:
	.ascii "#..##..##..##..#.,,#"
	.db 0x00
__str_4:
	.ascii "#..p....l....p..u..#"
	.db 0x00
__str_5:
	.ascii "#..,....=....,.....#"
	.db 0x00
__str_6:
	.ascii "#==================#"
	.db 0x00
__str_7:
	.ascii "#..u....=....u..k..#"
	.db 0x00
__str_8:
	.ascii "#....d....l.....d..#"
	.db 0x00
__str_9:
	.ascii "#...###....|..###..#"
	.db 0x00
__str_10:
	.ascii "#...###...|...###..#"
	.db 0x00
__str_11:
	.ascii "#......u......u....#"
	.db 0x00
__str_12:
	.ascii "#..p.......p.......#"
	.db 0x00
__str_13:
	.ascii "#....k.......q.....#"
	.db 0x00
__str_14:
	.ascii "#.............i....#"
	.db 0x00
__str_15:
	.ascii "#....u....x....u...#"
	.db 0x00
__str_16:
	.ascii "#......,......,....#"
	.db 0x00
__str_17:
	.ascii "#....h.....m.......#"
	.db 0x00
__str_18:
	.ascii "#..................#"
	.db 0x00
__str_19:
	.ascii "#..b.........f.....#"
	.db 0x00
__str_20:
	.ascii "#....1....2....3...#"
	.db 0x00
__str_21:
	.ascii "#..o.....s.....t...#"
	.db 0x00
__str_22:
	.ascii "#......k...........#"
	.db 0x00
__str_23:
	.ascii "#.............x....#"
	.db 0x00
__str_24:
	.ascii "#...............d..#"
	.db 0x00
__str_25:
	.ascii "#rrrrrrrrrrrrrrrrrr#"
	.db 0x00
__str_26:
	.ascii "#r................r#"
	.db 0x00
__str_27:
	.ascii "#r..R......R......r#"
	.db 0x00
__str_28:
	.ascii "#r.cccccccccccccc.r#"
	.db 0x00
__str_29:
	.ascii "#r...o.........l..r#"
	.db 0x00
__str_30:
	.ascii "#r......R....R....r#"
	.db 0x00
__str_31:
	.ascii "#r........k.......r#"
	.db 0x00
__str_32:
	.ascii "#r........d.......r#"
	.db 0x00
__str_33:
	.ascii "#wwwwwwwwwwwwwwwwww#"
	.db 0x00
__str_34:
	.ascii "#w................w#"
	.db 0x00
__str_35:
	.ascii "#w..m......m......w#"
	.db 0x00
__str_36:
	.ascii "#w.cccccccccccccc.w#"
	.db 0x00
__str_37:
	.ascii "#w...x.........k..w#"
	.db 0x00
__str_38:
	.ascii "#w..m.......m.....w#"
	.db 0x00
__str_39:
	.ascii "#w........h.......w#"
	.db 0x00
__str_40:
	.ascii "#w........d.......w#"
	.db 0x00
__str_41:
	.ascii "#w.....l..........w#"
	.db 0x00
__str_42:
	.ascii "#w.nnnnnnnnnnnnnn.w#"
	.db 0x00
__str_43:
	.ascii "#w....s.....s.....w#"
	.db 0x00
__str_44:
	.ascii "#w......t.........w#"
	.db 0x00
__str_45:
	.ascii "#gggggggggggggggggg#"
	.db 0x00
__str_46:
	.ascii "#g.....p.....p....g#"
	.db 0x00
__str_47:
	.ascii "#g................g#"
	.db 0x00
__str_48:
	.ascii "#g....a...........g#"
	.db 0x00
__str_49:
	.ascii "#g.........x......g#"
	.db 0x00
__str_50:
	.ascii "#g...p........p...g#"
	.db 0x00
__str_51:
	.ascii "#g.....G....G.....g#"
	.db 0x00
__str_52:
	.ascii "#g.........d......g#"
	.db 0x00
__str_53:
	.ascii "#........d.........#"
	.db 0x00
__str_54:
	.ascii "#..pppp......lll...#"
	.db 0x00
__str_55:
	.ascii "#..pwwp......lll...#"
	.db 0x00
__str_56:
	.ascii "#..pddp......lll...#"
	.db 0x00
__str_57:
	.ascii "#..pppp............#"
	.db 0x00
__str_58:
	.ascii "#........==........#"
	.db 0x00
__str_59:
	.ascii "#............ttt...#"
	.db 0x00
__str_60:
	.ascii "#............tst...#"
	.db 0x00
__str_61:
	.ascii "#........q.........#"
	.db 0x00
__str_62:
	.ascii "Mina"
	.db 0x00
__str_63:
	.ascii "Rook"
	.db 0x00
__str_64:
	.ascii "Soma"
	.db 0x00
__str_65:
	.ascii "Iona"
	.db 0x00
__str_66:
	.ascii "Pix"
	.db 0x00
__str_67:
	.ascii "SOFTPHONES"
	.db 0x00
__str_68:
	.ascii "Bring Iona softphones from Rook."
	.db 0x00
__str_69:
	.ascii "Iona trades you a rooftop bootleg and some credits."
	.db 0x00
__str_70:
	.ascii "BROTH RUN"
	.db 0x00
__str_71:
	.ascii "Bring Mina sealed broth from Soma."
	.db 0x00
__str_72:
	.ascii "Mina hands over a vending-pop tape and some change."
	.db 0x00
__str_73:
	.ascii "DISC SWAP"
	.db 0x00
__str_74:
	.ascii "Bring Rook a blank mini-disc from Mina."
	.db 0x00
__str_75:
	.ascii "Rook grins and gives you Rain Loop with a few credits."
	.db 0x00
	.area _INITIALIZER
	.area _CABS (ABS)
