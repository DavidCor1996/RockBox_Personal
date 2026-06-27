	.file	"<stdin>"
	.text
.Ltext0:
	.file 0 "/home/david/Documents/RockBox_Personal-master/build-sim-ipod6g" "<stdin>"
	.section	.text.startup,"ax",@progbits
	.globl	main
	.type	main, @function
main:
.LFB96:
	.file 1 "<stdin>"
	.loc 1 26 1 view -0
	.cfi_startproc
	.loc 1 30 1 view .LVU1
#APP
# 30 "<stdin>" 1
	/* LUA_RB_SETTINGS_H_HELPER, , struct system_status, struct user_settings, struct replaygain_settings, struct eq_band_setting, struct compressor_settings, struct mp3_enc_config, , struct mp3entry, struct mp3_albumart, struct embedded_cuesheet; */
# 0 "" 2
	.loc 1 32 1 view .LVU2
	.loc 1 33 5 view .LVU3
# 33 "<stdin>" 1
	/* "struct system_status", "volume" = $0, $4, "i", $0; */

# 0 "" 2
	.loc 1 34 5 view .LVU4
# 34 "<stdin>" 1
	/* "struct system_status", "resume_index" = $4, $4, "i", $0; */

# 0 "" 2
	.loc 1 35 5 view .LVU5
# 35 "<stdin>" 1
	/* "struct system_status", "resume_crc32" = $8, $4, "u_i", $0; */

# 0 "" 2
	.loc 1 36 5 view .LVU6
# 36 "<stdin>" 1
	/* "struct system_status", "resume_elapsed" = $12, $4, "u_i", $0; */

# 0 "" 2
	.loc 1 37 5 view .LVU7
# 37 "<stdin>" 1
	/* "struct system_status", "resume_offset" = $16, $4, "u_i", $0; */

# 0 "" 2
	.loc 1 38 5 view .LVU8
# 38 "<stdin>" 1
	/* "struct system_status", "resume_pitch" = $20, $4, "i", $0; */

# 0 "" 2
	.loc 1 39 5 view .LVU9
# 39 "<stdin>" 1
	/* "struct system_status", "resume_speed" = $24, $4, "i", $0; */

# 0 "" 2
	.loc 1 40 5 view .LVU10
# 40 "<stdin>" 1
	/* "struct system_status", "runtime" = $28, $4, "i", $0; */

# 0 "" 2
	.loc 1 41 5 view .LVU11
# 41 "<stdin>" 1
	/* "struct system_status", "topruntime" = $32, $4, "i", $0; */

# 0 "" 2
	.loc 1 42 5 view .LVU12
# 42 "<stdin>" 1
	/* "struct system_status", "last_screen" = $36, $1, "c", $0; */

# 0 "" 2
	.loc 1 43 5 view .LVU13
# 43 "<stdin>" 1
	/* "struct system_status", "viewer_icon_count" = $40, $4, "i", $0; */

# 0 "" 2
	.loc 1 44 5 view .LVU14
# 44 "<stdin>" 1
	/* "struct system_status", "last_volume_change" = $44, $4, "i", $0; */

# 0 "" 2
	.loc 1 45 5 view .LVU15
# 45 "<stdin>" 1
	/* "struct system_status", "font_id" = $48, $4, "i_typeisarray_", $1; */

# 0 "" 2
	.loc 1 46 5 view .LVU16
# 46 "<stdin>" 1
	/* "struct system_status", "resume_modified" = $52, $1, "b", $0; */

# 0 "" 2
	.loc 1 47 1 view .LVU17
	.loc 1 48 5 view .LVU18
# 48 "<stdin>" 1
	/* "struct user_settings", "balance" = $0, $4, "i", $0; */

# 0 "" 2
	.loc 1 49 5 view .LVU19
# 49 "<stdin>" 1
	/* "struct user_settings", "bass" = $4, $4, "i", $0; */

# 0 "" 2
	.loc 1 50 5 view .LVU20
# 50 "<stdin>" 1
	/* "struct user_settings", "treble" = $8, $4, "i", $0; */

# 0 "" 2
	.loc 1 51 5 view .LVU21
# 51 "<stdin>" 1
	/* "struct user_settings", "channel_config" = $12, $4, "i", $0; */

# 0 "" 2
	.loc 1 52 5 view .LVU22
# 52 "<stdin>" 1
	/* "struct user_settings", "stereo_width" = $16, $4, "i", $0; */

# 0 "" 2
	.loc 1 53 5 view .LVU23
# 53 "<stdin>" 1
	/* "struct user_settings", "bass_cutoff" = $20, $4, "i", $0; */

# 0 "" 2
	.loc 1 54 5 view .LVU24
# 54 "<stdin>" 1
	/* "struct user_settings", "treble_cutoff" = $24, $4, "i", $0; */

# 0 "" 2
	.loc 1 55 5 view .LVU25
# 55 "<stdin>" 1
	/* "struct user_settings", "crossfade" = $28, $4, "i", $0; */

# 0 "" 2
	.loc 1 56 5 view .LVU26
# 56 "<stdin>" 1
	/* "struct user_settings", "crossfade_fade_in_delay" = $32, $4, "i", $0; */

# 0 "" 2
	.loc 1 57 5 view .LVU27
# 57 "<stdin>" 1
	/* "struct user_settings", "crossfade_fade_out_delay" = $36, $4, "i", $0; */

# 0 "" 2
	.loc 1 58 5 view .LVU28
# 58 "<stdin>" 1
	/* "struct user_settings", "crossfade_fade_in_duration" = $40, $4, "i", $0; */

# 0 "" 2
	.loc 1 59 5 view .LVU29
# 59 "<stdin>" 1
	/* "struct user_settings", "crossfade_fade_out_duration" = $44, $4, "i", $0; */

# 0 "" 2
	.loc 1 60 5 view .LVU30
# 60 "<stdin>" 1
	/* "struct user_settings", "crossfade_fade_out_mixmode" = $48, $4, "i", $0; */

# 0 "" 2
	.loc 1 61 5 view .LVU31
# 61 "<stdin>" 1
	/* "struct user_settings", "replaygain_settings" = $52, $12, "s_replaygain_settings", $0; */

# 0 "" 2
	.loc 1 62 5 view .LVU32
# 62 "<stdin>" 1
	/* "struct user_settings", "crossfeed" = $64, $4, "i", $0; */

# 0 "" 2
	.loc 1 63 5 view .LVU33
# 63 "<stdin>" 1
	/* "struct user_settings", "crossfeed_direct_gain" = $68, $4, "u_i", $0; */

# 0 "" 2
	.loc 1 64 5 view .LVU34
# 64 "<stdin>" 1
	/* "struct user_settings", "crossfeed_cross_gain" = $72, $4, "u_i", $0; */

# 0 "" 2
	.loc 1 65 5 view .LVU35
# 65 "<stdin>" 1
	/* "struct user_settings", "crossfeed_hf_attenuation" = $76, $4, "u_i", $0; */

# 0 "" 2
	.loc 1 66 5 view .LVU36
# 66 "<stdin>" 1
	/* "struct user_settings", "crossfeed_hf_cutoff" = $80, $4, "u_i", $0; */

# 0 "" 2
	.loc 1 67 5 view .LVU37
# 67 "<stdin>" 1
	/* "struct user_settings", "eq_enabled" = $84, $1, "b", $0; */

# 0 "" 2
	.loc 1 68 5 view .LVU38
# 68 "<stdin>" 1
	/* "struct user_settings", "eq_precut" = $88, $4, "u_i", $0; */

# 0 "" 2
	.loc 1 69 5 view .LVU39
# 69 "<stdin>" 1
	/* "struct user_settings", "eq_band_settings" = $92, $120, "s_eq_band_setting_typeisarray_", $10; */

# 0 "" 2
	.loc 1 70 5 view .LVU40
# 70 "<stdin>" 1
	/* "struct user_settings", "beep" = $212, $4, "i", $0; */

# 0 "" 2
	.loc 1 71 5 view .LVU41
# 71 "<stdin>" 1
	/* "struct user_settings", "keyclick" = $216, $4, "i", $0; */

# 0 "" 2
	.loc 1 72 5 view .LVU42
# 72 "<stdin>" 1
	/* "struct user_settings", "keyclick_repeats" = $220, $4, "i", $0; */

# 0 "" 2
	.loc 1 73 5 view .LVU43
# 73 "<stdin>" 1
	/* "struct user_settings", "dithering_enabled" = $224, $1, "b", $0; */

# 0 "" 2
	.loc 1 74 5 view .LVU44
# 74 "<stdin>" 1
	/* "struct user_settings", "timestretch_enabled" = $225, $1, "b", $0; */

# 0 "" 2
	.loc 1 75 5 view .LVU45
# 75 "<stdin>" 1
	/* "struct user_settings", "rec_format" = $228, $4, "i", $0; */

# 0 "" 2
	.loc 1 76 5 view .LVU46
# 76 "<stdin>" 1
	/* "struct user_settings", "rec_mono_mode" = $232, $4, "i", $0; */

# 0 "" 2
	.loc 1 77 5 view .LVU47
# 77 "<stdin>" 1
	/* "struct user_settings", "mp3_enc_config" = $240, $8, "s_mp3_enc_config", $0; */

# 0 "" 2
	.loc 1 78 5 view .LVU48
# 78 "<stdin>" 1
	/* "struct user_settings", "rec_source" = $248, $4, "i", $0; */

# 0 "" 2
	.loc 1 79 5 view .LVU49
# 79 "<stdin>" 1
	/* "struct user_settings", "rec_frequency" = $252, $4, "i", $0; */

# 0 "" 2
	.loc 1 80 5 view .LVU50
# 80 "<stdin>" 1
	/* "struct user_settings", "rec_channels" = $256, $4, "i", $0; */

# 0 "" 2
	.loc 1 81 5 view .LVU51
# 81 "<stdin>" 1
	/* "struct user_settings", "rec_mic_gain" = $260, $4, "i", $0; */

# 0 "" 2
	.loc 1 82 5 view .LVU52
# 82 "<stdin>" 1
	/* "struct user_settings", "rec_left_gain" = $264, $4, "i", $0; */

# 0 "" 2
	.loc 1 83 5 view .LVU53
# 83 "<stdin>" 1
	/* "struct user_settings", "rec_right_gain" = $268, $4, "i", $0; */

# 0 "" 2
	.loc 1 84 5 view .LVU54
# 84 "<stdin>" 1
	/* "struct user_settings", "peak_meter_clipcounter" = $272, $1, "b", $0; */

# 0 "" 2
	.loc 1 85 5 view .LVU55
# 85 "<stdin>" 1
	/* "struct user_settings", "rec_editable" = $273, $1, "b", $0; */

# 0 "" 2
	.loc 1 86 5 view .LVU56
# 86 "<stdin>" 1
	/* "struct user_settings", "rec_timesplit" = $276, $4, "i", $0; */

# 0 "" 2
	.loc 1 87 5 view .LVU57
# 87 "<stdin>" 1
	/* "struct user_settings", "rec_sizesplit" = $280, $4, "i", $0; */

# 0 "" 2
	.loc 1 88 5 view .LVU58
# 88 "<stdin>" 1
	/* "struct user_settings", "rec_split_type" = $284, $4, "i", $0; */

# 0 "" 2
	.loc 1 89 5 view .LVU59
# 89 "<stdin>" 1
	/* "struct user_settings", "rec_split_method" = $288, $4, "i", $0; */

# 0 "" 2
	.loc 1 90 5 view .LVU60
# 90 "<stdin>" 1
	/* "struct user_settings", "rec_prerecord_time" = $292, $4, "i", $0; */

# 0 "" 2
	.loc 1 91 5 view .LVU61
# 91 "<stdin>" 1
	/* "struct user_settings", "rec_directory" = $296, $81, "str", $0; */

# 0 "" 2
	.loc 1 92 5 view .LVU62
# 92 "<stdin>" 1
	/* "struct user_settings", "cliplight" = $380, $4, "i", $0; */

# 0 "" 2
	.loc 1 93 5 view .LVU63
# 93 "<stdin>" 1
	/* "struct user_settings", "rec_start_thres_db" = $384, $4, "i", $0; */

# 0 "" 2
	.loc 1 94 5 view .LVU64
# 94 "<stdin>" 1
	/* "struct user_settings", "rec_start_thres_linear" = $388, $4, "i", $0; */

# 0 "" 2
	.loc 1 95 5 view .LVU65
# 95 "<stdin>" 1
	/* "struct user_settings", "rec_start_duration" = $392, $4, "i", $0; */

# 0 "" 2
	.loc 1 96 5 view .LVU66
# 96 "<stdin>" 1
	/* "struct user_settings", "rec_stop_thres_db" = $396, $4, "i", $0; */

# 0 "" 2
	.loc 1 97 5 view .LVU67
# 97 "<stdin>" 1
	/* "struct user_settings", "rec_stop_thres_linear" = $400, $4, "i", $0; */

# 0 "" 2
	.loc 1 98 5 view .LVU68
# 98 "<stdin>" 1
	/* "struct user_settings", "rec_stop_postrec" = $404, $4, "i", $0; */

# 0 "" 2
	.loc 1 99 5 view .LVU69
# 99 "<stdin>" 1
	/* "struct user_settings", "rec_stop_gap" = $408, $4, "i", $0; */

# 0 "" 2
	.loc 1 100 5 view .LVU70
# 100 "<stdin>" 1
	/* "struct user_settings", "rec_trigger_mode" = $412, $4, "i", $0; */

# 0 "" 2
	.loc 1 101 5 view .LVU71
# 101 "<stdin>" 1
	/* "struct user_settings", "rec_trigger_type" = $416, $4, "i", $0; */

# 0 "" 2
	.loc 1 102 5 view .LVU72
# 102 "<stdin>" 1
	/* "struct user_settings", "pause_rewind" = $420, $4, "i", $0; */

# 0 "" 2
	.loc 1 103 5 view .LVU73
# 103 "<stdin>" 1
	/* "struct user_settings", "unplug_mode" = $424, $4, "i", $0; */

# 0 "" 2
	.loc 1 104 5 view .LVU74
# 104 "<stdin>" 1
	/* "struct user_settings", "unplug_autoresume" = $428, $1, "b", $0; */

# 0 "" 2
	.loc 1 105 5 view .LVU75
# 105 "<stdin>" 1
	/* "struct user_settings", "qs_items" = $432, $32, "ptr_const_struct_typeisarray_settings_list", $0; */

# 0 "" 2
	.loc 1 106 5 view .LVU76
# 106 "<stdin>" 1
	/* "struct user_settings", "timeformat" = $464, $4, "i", $0; */

# 0 "" 2
	.loc 1 107 5 view .LVU77
# 107 "<stdin>" 1
	/* "struct user_settings", "disk_spindown" = $468, $4, "i", $0; */

# 0 "" 2
	.loc 1 108 5 view .LVU78
# 108 "<stdin>" 1
	/* "struct user_settings", "buffer_margin" = $472, $4, "i", $0; */

# 0 "" 2
	.loc 1 109 5 view .LVU79
# 109 "<stdin>" 1
	/* "struct user_settings", "storage_mode" = $476, $4, "i", $0; */

# 0 "" 2
	.loc 1 110 5 view .LVU80
# 110 "<stdin>" 1
	/* "struct user_settings", "dirfilter" = $480, $4, "i", $0; */

# 0 "" 2
	.loc 1 111 5 view .LVU81
# 111 "<stdin>" 1
	/* "struct user_settings", "show_filename_ext" = $484, $4, "i", $0; */

# 0 "" 2
	.loc 1 112 5 view .LVU82
# 112 "<stdin>" 1
	/* "struct user_settings", "default_codepage" = $488, $4, "i", $0; */

# 0 "" 2
	.loc 1 113 5 view .LVU83
# 113 "<stdin>" 1
	/* "struct user_settings", "hold_lr_for_scroll_in_list" = $492, $1, "b", $0; */

# 0 "" 2
	.loc 1 114 5 view .LVU84
# 114 "<stdin>" 1
	/* "struct user_settings", "play_selected" = $493, $1, "b", $0; */

# 0 "" 2
	.loc 1 115 5 view .LVU85
# 115 "<stdin>" 1
	/* "struct user_settings", "single_mode" = $496, $4, "i", $0; */

# 0 "" 2
	.loc 1 116 5 view .LVU86
# 116 "<stdin>" 1
	/* "struct user_settings", "party_mode" = $500, $1, "b", $0; */

# 0 "" 2
	.loc 1 117 5 view .LVU87
# 117 "<stdin>" 1
	/* "struct user_settings", "cuesheet" = $501, $1, "b", $0; */

# 0 "" 2
	.loc 1 118 5 view .LVU88
# 118 "<stdin>" 1
	/* "struct user_settings", "car_adapter_mode" = $502, $1, "b", $0; */

# 0 "" 2
	.loc 1 119 5 view .LVU89
# 119 "<stdin>" 1
	/* "struct user_settings", "car_adapter_mode_delay" = $504, $4, "i", $0; */

# 0 "" 2
	.loc 1 120 5 view .LVU90
# 120 "<stdin>" 1
	/* "struct user_settings", "start_in_screen" = $508, $4, "i", $0; */

# 0 "" 2
	.loc 1 121 5 view .LVU91
# 121 "<stdin>" 1
	/* "struct user_settings", "wps_select_action" = $512, $4, "i", $0; */

# 0 "" 2
	.loc 1 122 5 view .LVU92
# 122 "<stdin>" 1
	/* "struct user_settings", "ff_rewind_min_step" = $516, $4, "i", $0; */

# 0 "" 2
	.loc 1 123 5 view .LVU93
# 123 "<stdin>" 1
	/* "struct user_settings", "ff_rewind_accel" = $520, $4, "i", $0; */

# 0 "" 2
	.loc 1 124 5 view .LVU94
# 124 "<stdin>" 1
	/* "struct user_settings", "peak_meter_release" = $524, $4, "i", $0; */

# 0 "" 2
	.loc 1 125 5 view .LVU95
# 125 "<stdin>" 1
	/* "struct user_settings", "peak_meter_hold" = $528, $4, "i", $0; */

# 0 "" 2
	.loc 1 126 5 view .LVU96
# 126 "<stdin>" 1
	/* "struct user_settings", "peak_meter_clip_hold" = $532, $4, "i", $0; */

# 0 "" 2
	.loc 1 127 5 view .LVU97
# 127 "<stdin>" 1
	/* "struct user_settings", "peak_meter_dbfs" = $536, $1, "b", $0; */

# 0 "" 2
	.loc 1 128 5 view .LVU98
# 128 "<stdin>" 1
	/* "struct user_settings", "peak_meter_min" = $540, $4, "i", $0; */

# 0 "" 2
	.loc 1 129 5 view .LVU99
# 129 "<stdin>" 1
	/* "struct user_settings", "peak_meter_max" = $544, $4, "i", $0; */

# 0 "" 2
	.loc 1 130 5 view .LVU100
# 130 "<stdin>" 1
	/* "struct user_settings", "wps_file" = $548, $33, "str", $0; */

# 0 "" 2
	.loc 1 131 5 view .LVU101
# 131 "<stdin>" 1
	/* "struct user_settings", "sbs_file" = $581, $33, "str", $0; */

# 0 "" 2
	.loc 1 132 5 view .LVU102
# 132 "<stdin>" 1
	/* "struct user_settings", "lang_file" = $614, $33, "str", $0; */

# 0 "" 2
	.loc 1 133 5 view .LVU103
# 133 "<stdin>" 1
	/* "struct user_settings", "playlist_catalog_dir" = $647, $81, "str", $0; */

# 0 "" 2
	.loc 1 134 5 view .LVU104
# 134 "<stdin>" 1
	/* "struct user_settings", "skip_length" = $728, $4, "i", $0; */

# 0 "" 2
	.loc 1 135 5 view .LVU105
# 135 "<stdin>" 1
	/* "struct user_settings", "max_files_in_dir" = $732, $4, "i", $0; */

# 0 "" 2
	.loc 1 136 5 view .LVU106
# 136 "<stdin>" 1
	/* "struct user_settings", "max_files_in_playlist" = $736, $4, "i", $0; */

# 0 "" 2
	.loc 1 137 5 view .LVU107
# 137 "<stdin>" 1
	/* "struct user_settings", "volume_type" = $740, $4, "i", $0; */

# 0 "" 2
	.loc 1 138 5 view .LVU108
# 138 "<stdin>" 1
	/* "struct user_settings", "battery_display" = $744, $4, "i", $0; */

# 0 "" 2
	.loc 1 139 5 view .LVU109
# 139 "<stdin>" 1
	/* "struct user_settings", "show_icons" = $748, $1, "b", $0; */

# 0 "" 2
	.loc 1 140 5 view .LVU110
# 140 "<stdin>" 1
	/* "struct user_settings", "statusbar" = $752, $4, "i", $0; */

# 0 "" 2
	.loc 1 141 5 view .LVU111
# 141 "<stdin>" 1
	/* "struct user_settings", "scrollbar" = $756, $4, "i", $0; */

# 0 "" 2
	.loc 1 142 5 view .LVU112
# 142 "<stdin>" 1
	/* "struct user_settings", "scrollbar_width" = $760, $4, "i", $0; */

# 0 "" 2
	.loc 1 143 5 view .LVU113
# 143 "<stdin>" 1
	/* "struct user_settings", "list_separator_height" = $764, $4, "i", $0; */

# 0 "" 2
	.loc 1 144 5 view .LVU114
# 144 "<stdin>" 1
	/* "struct user_settings", "list_separator_color" = $768, $4, "i", $0; */

# 0 "" 2
	.loc 1 145 5 view .LVU115
# 145 "<stdin>" 1
	/* "struct user_settings", "browse_current" = $772, $1, "b", $0; */

# 0 "" 2
	.loc 1 146 5 view .LVU116
# 146 "<stdin>" 1
	/* "struct user_settings", "scroll_paginated" = $773, $1, "b", $0; */

# 0 "" 2
	.loc 1 147 5 view .LVU117
# 147 "<stdin>" 1
	/* "struct user_settings", "list_wraparound" = $774, $1, "b", $0; */

# 0 "" 2
	.loc 1 148 5 view .LVU118
# 148 "<stdin>" 1
	/* "struct user_settings", "list_order" = $776, $4, "i", $0; */

# 0 "" 2
	.loc 1 149 5 view .LVU119
# 149 "<stdin>" 1
	/* "struct user_settings", "scroll_speed" = $780, $4, "i", $0; */

# 0 "" 2
	.loc 1 150 5 view .LVU120
# 150 "<stdin>" 1
	/* "struct user_settings", "bidir_limit" = $784, $4, "i", $0; */

# 0 "" 2
	.loc 1 151 5 view .LVU121
# 151 "<stdin>" 1
	/* "struct user_settings", "scroll_delay" = $788, $4, "i", $0; */

# 0 "" 2
	.loc 1 152 5 view .LVU122
# 152 "<stdin>" 1
	/* "struct user_settings", "scroll_step" = $792, $4, "i", $0; */

# 0 "" 2
	.loc 1 153 5 view .LVU123
# 153 "<stdin>" 1
	/* "struct user_settings", "autoloadbookmark" = $796, $4, "i", $0; */

# 0 "" 2
	.loc 1 154 5 view .LVU124
# 154 "<stdin>" 1
	/* "struct user_settings", "autocreatebookmark" = $800, $4, "i", $0; */

# 0 "" 2
	.loc 1 155 5 view .LVU125
# 155 "<stdin>" 1
	/* "struct user_settings", "autoupdatebookmark" = $804, $1, "b", $0; */

# 0 "" 2
	.loc 1 156 5 view .LVU126
# 156 "<stdin>" 1
	/* "struct user_settings", "usemrb" = $808, $4, "i", $0; */

# 0 "" 2
	.loc 1 157 5 view .LVU127
# 157 "<stdin>" 1
	/* "struct user_settings", "tagcache_ram" = $812, $4, "i", $0; */

# 0 "" 2
	.loc 1 158 5 view .LVU128
# 158 "<stdin>" 1
	/* "struct user_settings", "tagcache_autoupdate" = $816, $1, "b", $0; */

# 0 "" 2
	.loc 1 159 5 view .LVU129
# 159 "<stdin>" 1
	/* "struct user_settings", "autoresume_enable" = $817, $1, "b", $0; */

# 0 "" 2
	.loc 1 160 5 view .LVU130
# 160 "<stdin>" 1
	/* "struct user_settings", "autoresume_automatic" = $820, $4, "i", $0; */

# 0 "" 2
	.loc 1 161 5 view .LVU131
# 161 "<stdin>" 1
	/* "struct user_settings", "autoresume_paths" = $824, $161, "str", $0; */

# 0 "" 2
	.loc 1 162 5 view .LVU132
# 162 "<stdin>" 1
	/* "struct user_settings", "runtimedb" = $985, $1, "b", $0; */

# 0 "" 2
	.loc 1 163 5 view .LVU133
# 163 "<stdin>" 1
	/* "struct user_settings", "tagcache_scan_paths" = $986, $161, "str", $0; */

# 0 "" 2
	.loc 1 164 5 view .LVU134
# 164 "<stdin>" 1
	/* "struct user_settings", "tagcache_db_path" = $1147, $81, "str", $0; */

# 0 "" 2
	.loc 1 165 5 view .LVU135
# 165 "<stdin>" 1
	/* "struct user_settings", "backdrop_file" = $1228, $81, "str", $0; */

# 0 "" 2
	.loc 1 166 5 view .LVU136
# 166 "<stdin>" 1
	/* "struct user_settings", "bg_color" = $1312, $4, "i", $0; */

# 0 "" 2
	.loc 1 167 5 view .LVU137
# 167 "<stdin>" 1
	/* "struct user_settings", "fg_color" = $1316, $4, "i", $0; */

# 0 "" 2
	.loc 1 168 5 view .LVU138
# 168 "<stdin>" 1
	/* "struct user_settings", "lss_color" = $1320, $4, "i", $0; */

# 0 "" 2
	.loc 1 169 5 view .LVU139
# 169 "<stdin>" 1
	/* "struct user_settings", "lse_color" = $1324, $4, "i", $0; */

# 0 "" 2
	.loc 1 170 5 view .LVU140
# 170 "<stdin>" 1
	/* "struct user_settings", "lst_color" = $1328, $4, "i", $0; */

# 0 "" 2
	.loc 1 171 5 view .LVU141
# 171 "<stdin>" 1
	/* "struct user_settings", "colors_file" = $1332, $33, "str", $0; */

# 0 "" 2
	.loc 1 172 5 view .LVU142
# 172 "<stdin>" 1
	/* "struct user_settings", "dynamic_colors" = $1365, $1, "b", $0; */

# 0 "" 2
	.loc 1 173 5 view .LVU143
# 173 "<stdin>" 1
	/* "struct user_settings", "ipone_charge_wallpaper" = $1368, $4, "i", $0; */

# 0 "" 2
	.loc 1 174 5 view .LVU144
# 174 "<stdin>" 1
	/* "struct user_settings", "ipone_lock_wallpaper" = $1372, $4, "i", $0; */

# 0 "" 2
	.loc 1 175 5 view .LVU145
# 175 "<stdin>" 1
	/* "struct user_settings", "ipone_right_pane" = $1376, $4, "i", $0; */

# 0 "" 2
	.loc 1 176 5 view .LVU146
# 176 "<stdin>" 1
	/* "struct user_settings", "album_list_layout" = $1380, $4, "i", $0; */

# 0 "" 2
	.loc 1 177 5 view .LVU147
# 177 "<stdin>" 1
	/* "struct user_settings", "browser_default" = $1384, $4, "i", $0; */

# 0 "" 2
	.loc 1 178 5 view .LVU148
# 178 "<stdin>" 1
	/* "struct user_settings", "repeat_mode" = $1388, $4, "i", $0; */

# 0 "" 2
	.loc 1 179 5 view .LVU149
# 179 "<stdin>" 1
	/* "struct user_settings", "next_folder" = $1392, $4, "i", $0; */

# 0 "" 2
	.loc 1 180 5 view .LVU150
# 180 "<stdin>" 1
	/* "struct user_settings", "constrain_next_folder" = $1396, $1, "b", $0; */

# 0 "" 2
	.loc 1 181 5 view .LVU151
# 181 "<stdin>" 1
	/* "struct user_settings", "recursive_dir_insert" = $1400, $4, "i", $0; */

# 0 "" 2
	.loc 1 182 5 view .LVU152
# 182 "<stdin>" 1
	/* "struct user_settings", "fade_on_stop" = $1404, $1, "b", $0; */

# 0 "" 2
	.loc 1 183 5 view .LVU153
# 183 "<stdin>" 1
	/* "struct user_settings", "playlist_shuffle" = $1405, $1, "b", $0; */

# 0 "" 2
	.loc 1 184 5 view .LVU154
# 184 "<stdin>" 1
	/* "struct user_settings", "warnon_erase_dynplaylist" = $1406, $1, "b", $0; */

# 0 "" 2
	.loc 1 185 5 view .LVU155
# 185 "<stdin>" 1
	/* "struct user_settings", "keep_current_track_on_replace_playlist" = $1407, $1, "b", $0; */

# 0 "" 2
	.loc 1 186 5 view .LVU156
# 186 "<stdin>" 1
	/* "struct user_settings", "show_shuffled_adding_options" = $1408, $1, "b", $0; */

# 0 "" 2
	.loc 1 187 5 view .LVU157
# 187 "<stdin>" 1
	/* "struct user_settings", "show_queue_options" = $1412, $4, "i", $0; */

# 0 "" 2
	.loc 1 188 5 view .LVU158
# 188 "<stdin>" 1
	/* "struct user_settings", "album_art" = $1416, $4, "i", $0; */

# 0 "" 2
	.loc 1 189 5 view .LVU159
# 189 "<stdin>" 1
	/* "struct user_settings", "rewind_across_tracks" = $1420, $1, "b", $0; */

# 0 "" 2
	.loc 1 190 5 view .LVU160
# 190 "<stdin>" 1
	/* "struct user_settings", "playlist_viewer_icons" = $1421, $1, "b", $0; */

# 0 "" 2
	.loc 1 191 5 view .LVU161
# 191 "<stdin>" 1
	/* "struct user_settings", "playlist_viewer_indices" = $1422, $1, "b", $0; */

# 0 "" 2
	.loc 1 192 5 view .LVU162
# 192 "<stdin>" 1
	/* "struct user_settings", "playlist_viewer_track_display" = $1424, $4, "i", $0; */

# 0 "" 2
	.loc 1 193 5 view .LVU163
# 193 "<stdin>" 1
	/* "struct user_settings", "talk_menu" = $1428, $1, "b", $0; */

# 0 "" 2
	.loc 1 194 5 view .LVU164
# 194 "<stdin>" 1
	/* "struct user_settings", "talk_dir" = $1432, $4, "i", $0; */

# 0 "" 2
	.loc 1 195 5 view .LVU165
# 195 "<stdin>" 1
	/* "struct user_settings", "talk_dir_clip" = $1436, $1, "b", $0; */

# 0 "" 2
	.loc 1 196 5 view .LVU166
# 196 "<stdin>" 1
	/* "struct user_settings", "talk_file" = $1440, $4, "i", $0; */

# 0 "" 2
	.loc 1 197 5 view .LVU167
# 197 "<stdin>" 1
	/* "struct user_settings", "talk_file_clip" = $1444, $1, "b", $0; */

# 0 "" 2
	.loc 1 198 5 view .LVU168
# 198 "<stdin>" 1
	/* "struct user_settings", "talk_filetype" = $1445, $1, "b", $0; */

# 0 "" 2
	.loc 1 199 5 view .LVU169
# 199 "<stdin>" 1
	/* "struct user_settings", "talk_battery_level" = $1446, $1, "b", $0; */

# 0 "" 2
	.loc 1 200 5 view .LVU170
# 200 "<stdin>" 1
	/* "struct user_settings", "talk_mixer_amp" = $1448, $4, "i", $0; */

# 0 "" 2
	.loc 1 201 5 view .LVU171
# 201 "<stdin>" 1
	/* "struct user_settings", "sort_case" = $1452, $1, "b", $0; */

# 0 "" 2
	.loc 1 202 5 view .LVU172
# 202 "<stdin>" 1
	/* "struct user_settings", "sort_dir" = $1456, $4, "i", $0; */

# 0 "" 2
	.loc 1 203 5 view .LVU173
# 203 "<stdin>" 1
	/* "struct user_settings", "sort_file" = $1460, $4, "i", $0; */

# 0 "" 2
	.loc 1 204 5 view .LVU174
# 204 "<stdin>" 1
	/* "struct user_settings", "sort_playlists" = $1464, $4, "i", $0; */

# 0 "" 2
	.loc 1 205 5 view .LVU175
# 205 "<stdin>" 1
	/* "struct user_settings", "interpret_numbers" = $1468, $4, "i", $0; */

# 0 "" 2
	.loc 1 206 5 view .LVU176
# 206 "<stdin>" 1
	/* "struct user_settings", "poweroff" = $1472, $4, "i", $0; */

# 0 "" 2
	.loc 1 207 5 view .LVU177
# 207 "<stdin>" 1
	/* "struct user_settings", "battery_capacity" = $1476, $4, "i", $0; */

# 0 "" 2
	.loc 1 208 5 view .LVU178
# 208 "<stdin>" 1
	/* "struct user_settings", "usb_charging" = $1480, $4, "i", $0; */

# 0 "" 2
	.loc 1 209 5 view .LVU179
# 209 "<stdin>" 1
	/* "struct user_settings", "cursor_style" = $1484, $4, "i", $0; */

# 0 "" 2
	.loc 1 210 5 view .LVU180
# 210 "<stdin>" 1
	/* "struct user_settings", "screen_scroll_step" = $1488, $4, "i", $0; */

# 0 "" 2
	.loc 1 211 5 view .LVU181
# 211 "<stdin>" 1
	/* "struct user_settings", "show_path_in_browser" = $1492, $4, "i", $0; */

# 0 "" 2
	.loc 1 212 5 view .LVU182
# 212 "<stdin>" 1
	/* "struct user_settings", "offset_out_of_view" = $1496, $1, "b", $0; */

# 0 "" 2
	.loc 1 213 5 view .LVU183
# 213 "<stdin>" 1
	/* "struct user_settings", "disable_mainmenu_scrolling" = $1497, $1, "b", $0; */

# 0 "" 2
	.loc 1 214 5 view .LVU184
# 214 "<stdin>" 1
	/* "struct user_settings", "icon_file" = $1498, $33, "str", $0; */

# 0 "" 2
	.loc 1 215 5 view .LVU185
# 215 "<stdin>" 1
	/* "struct user_settings", "viewers_icon_file" = $1531, $33, "str", $0; */

# 0 "" 2
	.loc 1 216 5 view .LVU186
# 216 "<stdin>" 1
	/* "struct user_settings", "font_file" = $1564, $33, "str", $0; */

# 0 "" 2
	.loc 1 217 5 view .LVU187
# 217 "<stdin>" 1
	/* "struct user_settings", "glyphs_to_cache" = $1600, $4, "i", $0; */

# 0 "" 2
	.loc 1 218 5 view .LVU188
# 218 "<stdin>" 1
	/* "struct user_settings", "kbd_file" = $1604, $33, "str", $0; */

# 0 "" 2
	.loc 1 219 5 view .LVU189
# 219 "<stdin>" 1
	/* "struct user_settings", "backlight_timeout" = $1640, $4, "i", $0; */

# 0 "" 2
	.loc 1 220 5 view .LVU190
# 220 "<stdin>" 1
	/* "struct user_settings", "caption_backlight" = $1644, $1, "b", $0; */

# 0 "" 2
	.loc 1 221 5 view .LVU191
# 221 "<stdin>" 1
	/* "struct user_settings", "bl_filter_first_keypress" = $1645, $1, "b", $0; */

# 0 "" 2
	.loc 1 222 5 view .LVU192
# 222 "<stdin>" 1
	/* "struct user_settings", "backlight_timeout_plugged" = $1648, $4, "i", $0; */

# 0 "" 2
	.loc 1 223 5 view .LVU193
# 223 "<stdin>" 1
	/* "struct user_settings", "bl_selective_actions" = $1652, $1, "b", $0; */

# 0 "" 2
	.loc 1 224 5 view .LVU194
# 224 "<stdin>" 1
	/* "struct user_settings", "bl_selective_actions_mask" = $1656, $4, "i", $0; */

# 0 "" 2
	.loc 1 225 5 view .LVU195
# 225 "<stdin>" 1
	/* "struct user_settings", "backlight_on_button_hold" = $1660, $4, "i", $0; */

# 0 "" 2
	.loc 1 226 5 view .LVU196
# 226 "<stdin>" 1
	/* "struct user_settings", "lcd_sleep_after_backlight_off" = $1664, $4, "i", $0; */

# 0 "" 2
	.loc 1 227 5 view .LVU197
# 227 "<stdin>" 1
	/* "struct user_settings", "brightness" = $1668, $4, "i", $0; */

# 0 "" 2
	.loc 1 228 5 view .LVU198
# 228 "<stdin>" 1
	/* "struct user_settings", "accessory_supply" = $1672, $1, "b", $0; */

# 0 "" 2
	.loc 1 229 5 view .LVU199
# 229 "<stdin>" 1
	/* "struct user_settings", "lineout_active" = $1673, $1, "b", $0; */

# 0 "" 2
	.loc 1 230 5 view .LVU200
# 230 "<stdin>" 1
	/* "struct user_settings", "prevent_skip" = $1674, $1, "b", $0; */

# 0 "" 2
	.loc 1 231 5 view .LVU201
# 231 "<stdin>" 1
	/* "struct user_settings", "pitch_mode_semitone" = $1675, $1, "b", $0; */

# 0 "" 2
	.loc 1 232 5 view .LVU202
# 232 "<stdin>" 1
	/* "struct user_settings", "pitch_mode_timestretch" = $1676, $1, "b", $0; */

# 0 "" 2
	.loc 1 233 5 view .LVU203
# 233 "<stdin>" 1
	/* "struct user_settings", "ui_vp_config" = $1677, $64, "str", $0; */

# 0 "" 2
	.loc 1 234 5 view .LVU204
# 234 "<stdin>" 1
	/* "struct user_settings", "compressor_settings" = $1744, $24, "s_compressor_settings", $0; */

# 0 "" 2
	.loc 1 235 5 view .LVU205
# 235 "<stdin>" 1
	/* "struct user_settings", "sleeptimer_duration" = $1768, $4, "i", $0; */

# 0 "" 2
	.loc 1 236 5 view .LVU206
# 236 "<stdin>" 1
	/* "struct user_settings", "sleeptimer_on_startup" = $1772, $1, "b", $0; */

# 0 "" 2
	.loc 1 237 5 view .LVU207
# 237 "<stdin>" 1
	/* "struct user_settings", "keypress_restarts_sleeptimer" = $1773, $1, "b", $0; */

# 0 "" 2
	.loc 1 238 5 view .LVU208
# 238 "<stdin>" 1
	/* "struct user_settings", "show_shutdown_message" = $1774, $1, "b", $0; */

# 0 "" 2
	.loc 1 239 5 view .LVU209
# 239 "<stdin>" 1
	/* "struct user_settings", "morse_input" = $1775, $1, "b", $0; */

# 0 "" 2
	.loc 1 240 5 view .LVU210
# 240 "<stdin>" 1
	/* "struct user_settings", "hotkey_wps" = $1776, $4, "i", $0; */

# 0 "" 2
	.loc 1 241 5 view .LVU211
# 241 "<stdin>" 1
	/* "struct user_settings", "hotkey_tree" = $1780, $4, "i", $0; */

# 0 "" 2
	.loc 1 242 5 view .LVU212
# 242 "<stdin>" 1
	/* "struct user_settings", "resume_rewind" = $1784, $4, "i", $0; */

# 0 "" 2
	.loc 1 243 5 view .LVU213
# 243 "<stdin>" 1
	/* "struct user_settings", "keyclick_hardware" = $1788, $1, "b", $0; */

# 0 "" 2
	.loc 1 244 5 view .LVU214
# 244 "<stdin>" 1
	/* "struct user_settings", "start_directory" = $1789, $81, "str", $0; */

# 0 "" 2
	.loc 1 245 5 view .LVU215
# 245 "<stdin>" 1
	/* "struct user_settings", "root_menu_customized" = $1870, $1, "b", $0; */

# 0 "" 2
	.loc 1 246 5 view .LVU216
# 246 "<stdin>" 1
	/* "struct user_settings", "shortcuts_replaces_qs" = $1871, $1, "b", $0; */

# 0 "" 2
	.loc 1 247 5 view .LVU217
# 247 "<stdin>" 1
	/* "struct user_settings", "play_frequency" = $1872, $4, "i", $0; */

# 0 "" 2
	.loc 1 248 5 view .LVU218
# 248 "<stdin>" 1
	/* "struct user_settings", "volume_limit" = $1876, $4, "i", $0; */

# 0 "" 2
	.loc 1 249 5 view .LVU219
# 249 "<stdin>" 1
	/* "struct user_settings", "volume_adjust_mode" = $1880, $4, "i", $0; */

# 0 "" 2
	.loc 1 250 5 view .LVU220
# 250 "<stdin>" 1
	/* "struct user_settings", "volume_adjust_norm_steps" = $1884, $4, "i", $0; */

# 0 "" 2
	.loc 1 251 5 view .LVU221
# 251 "<stdin>" 1
	/* "struct user_settings", "surround_enabled" = $1888, $4, "i", $0; */

# 0 "" 2
	.loc 1 252 5 view .LVU222
# 252 "<stdin>" 1
	/* "struct user_settings", "surround_balance" = $1892, $4, "i", $0; */

# 0 "" 2
	.loc 1 253 5 view .LVU223
# 253 "<stdin>" 1
	/* "struct user_settings", "surround_fx1" = $1896, $4, "i", $0; */

# 0 "" 2
	.loc 1 254 5 view .LVU224
# 254 "<stdin>" 1
	/* "struct user_settings", "surround_fx2" = $1900, $4, "i", $0; */

# 0 "" 2
	.loc 1 255 5 view .LVU225
# 255 "<stdin>" 1
	/* "struct user_settings", "surround_method2" = $1904, $1, "b", $0; */

# 0 "" 2
	.loc 1 256 5 view .LVU226
# 256 "<stdin>" 1
	/* "struct user_settings", "surround_mix" = $1908, $4, "i", $0; */

# 0 "" 2
	.loc 1 257 5 view .LVU227
# 257 "<stdin>" 1
	/* "struct user_settings", "pbe" = $1912, $4, "i", $0; */

# 0 "" 2
	.loc 1 258 5 view .LVU228
# 258 "<stdin>" 1
	/* "struct user_settings", "pbe_precut" = $1916, $4, "i", $0; */

# 0 "" 2
	.loc 1 259 5 view .LVU229
# 259 "<stdin>" 1
	/* "struct user_settings", "afr_enabled" = $1920, $4, "i", $0; */

# 0 "" 2
	.loc 1 260 5 view .LVU230
# 260 "<stdin>" 1
	/* "struct user_settings", "clear_settings_on_hold" = $1924, $1, "b", $0; */

# 0 "" 2
	.loc 1 261 5 view .LVU231
# 261 "<stdin>" 1
	/* "struct user_settings", "playback_log" = $1925, $1, "b", $0; */

# 0 "" 2
	.loc 1 262 1 view .LVU232
	.loc 1 263 5 view .LVU233
# 263 "<stdin>" 1
	/* "struct replaygain_settings", "noclip" = $0, $1, "b", $0; */

# 0 "" 2
	.loc 1 264 5 view .LVU234
# 264 "<stdin>" 1
	/* "struct replaygain_settings", "type" = $4, $4, "i", $0; */

# 0 "" 2
	.loc 1 265 5 view .LVU235
# 265 "<stdin>" 1
	/* "struct replaygain_settings", "preamp" = $8, $4, "i", $0; */

# 0 "" 2
	.loc 1 266 1 view .LVU236
	.loc 1 267 5 view .LVU237
# 267 "<stdin>" 1
	/* "struct eq_band_setting", "cutoff" = $0, $4, "i", $0; */

# 0 "" 2
	.loc 1 268 5 view .LVU238
# 268 "<stdin>" 1
	/* "struct eq_band_setting", "q" = $4, $4, "i", $0; */

# 0 "" 2
	.loc 1 269 5 view .LVU239
# 269 "<stdin>" 1
	/* "struct eq_band_setting", "gain" = $8, $4, "i", $0; */

# 0 "" 2
	.loc 1 270 1 view .LVU240
	.loc 1 271 5 view .LVU241
# 271 "<stdin>" 1
	/* "struct compressor_settings", "threshold" = $0, $4, "i", $0; */

# 0 "" 2
	.loc 1 272 5 view .LVU242
# 272 "<stdin>" 1
	/* "struct compressor_settings", "makeup_gain" = $4, $4, "i", $0; */

# 0 "" 2
	.loc 1 273 5 view .LVU243
# 273 "<stdin>" 1
	/* "struct compressor_settings", "ratio" = $8, $4, "i", $0; */

# 0 "" 2
	.loc 1 274 5 view .LVU244
# 274 "<stdin>" 1
	/* "struct compressor_settings", "knee" = $12, $4, "i", $0; */

# 0 "" 2
	.loc 1 275 5 view .LVU245
# 275 "<stdin>" 1
	/* "struct compressor_settings", "release_time" = $16, $4, "i", $0; */

# 0 "" 2
	.loc 1 276 5 view .LVU246
# 276 "<stdin>" 1
	/* "struct compressor_settings", "attack_time" = $20, $4, "i", $0; */

# 0 "" 2
	.loc 1 277 1 view .LVU247
	.loc 1 278 5 view .LVU248
# 278 "<stdin>" 1
	/* "struct mp3_enc_config", "bitrate" = $0, $8, "u_l", $0; */

# 0 "" 2
	.loc 1 279 1 view .LVU249
	.loc 1 280 5 view .LVU250
# 280 "<stdin>" 1
	/* "struct mp3entry", "path" = $0, $260, "str", $0; */

# 0 "" 2
	.loc 1 281 5 view .LVU251
# 281 "<stdin>" 1
	/* "struct mp3entry", "title" = $264, $8, "ptr_char", $0; */

# 0 "" 2
	.loc 1 282 5 view .LVU252
# 282 "<stdin>" 1
	/* "struct mp3entry", "artist" = $272, $8, "ptr_char", $0; */

# 0 "" 2
	.loc 1 283 5 view .LVU253
# 283 "<stdin>" 1
	/* "struct mp3entry", "album" = $280, $8, "ptr_char", $0; */

# 0 "" 2
	.loc 1 284 5 view .LVU254
# 284 "<stdin>" 1
	/* "struct mp3entry", "genre_string" = $288, $8, "ptr_char", $0; */

# 0 "" 2
	.loc 1 285 5 view .LVU255
# 285 "<stdin>" 1
	/* "struct mp3entry", "disc_string" = $296, $8, "ptr_char", $0; */

# 0 "" 2
	.loc 1 286 5 view .LVU256
# 286 "<stdin>" 1
	/* "struct mp3entry", "track_string" = $304, $8, "ptr_char", $0; */

# 0 "" 2
	.loc 1 287 5 view .LVU257
# 287 "<stdin>" 1
	/* "struct mp3entry", "year_string" = $312, $8, "ptr_char", $0; */

# 0 "" 2
	.loc 1 288 5 view .LVU258
# 288 "<stdin>" 1
	/* "struct mp3entry", "composer" = $320, $8, "ptr_char", $0; */

# 0 "" 2
	.loc 1 289 5 view .LVU259
# 289 "<stdin>" 1
	/* "struct mp3entry", "comment" = $328, $8, "ptr_char", $0; */

# 0 "" 2
	.loc 1 290 5 view .LVU260
# 290 "<stdin>" 1
	/* "struct mp3entry", "albumartist" = $336, $8, "ptr_char", $0; */

# 0 "" 2
	.loc 1 291 5 view .LVU261
# 291 "<stdin>" 1
	/* "struct mp3entry", "grouping" = $344, $8, "ptr_char", $0; */

# 0 "" 2
	.loc 1 292 5 view .LVU262
# 292 "<stdin>" 1
	/* "struct mp3entry", "discnum" = $352, $4, "i", $0; */

# 0 "" 2
	.loc 1 293 5 view .LVU263
# 293 "<stdin>" 1
	/* "struct mp3entry", "tracknum" = $356, $4, "i", $0; */

# 0 "" 2
	.loc 1 294 5 view .LVU264
# 294 "<stdin>" 1
	/* "struct mp3entry", "layer" = $360, $4, "i", $0; */

# 0 "" 2
	.loc 1 295 5 view .LVU265
# 295 "<stdin>" 1
	/* "struct mp3entry", "year" = $364, $4, "i", $0; */

# 0 "" 2
	.loc 1 296 5 view .LVU266
# 296 "<stdin>" 1
	/* "struct mp3entry", "id3version" = $368, $1, "u_c", $0; */

# 0 "" 2
	.loc 1 297 5 view .LVU267
# 297 "<stdin>" 1
	/* "struct mp3entry", "codectype" = $372, $4, "u_i", $0; */

# 0 "" 2
	.loc 1 298 5 view .LVU268
# 298 "<stdin>" 1
	/* "struct mp3entry", "bitrate" = $376, $4, "u_i", $0; */

# 0 "" 2
	.loc 1 299 5 view .LVU269
# 299 "<stdin>" 1
	/* "struct mp3entry", "frequency" = $384, $8, "u_l", $0; */

# 0 "" 2
	.loc 1 300 5 view .LVU270
# 300 "<stdin>" 1
	/* "struct mp3entry", "id3v2len" = $392, $8, "u_l", $0; */

# 0 "" 2
	.loc 1 301 5 view .LVU271
# 301 "<stdin>" 1
	/* "struct mp3entry", "id3v1len" = $400, $8, "u_l", $0; */

# 0 "" 2
	.loc 1 302 5 view .LVU272
# 302 "<stdin>" 1
	/* "struct mp3entry", "first_frame_offset" = $408, $8, "u_l", $0; */

# 0 "" 2
	.loc 1 303 5 view .LVU273
# 303 "<stdin>" 1
	/* "struct mp3entry", "sim_filesize" = $416, $8, "u_l", $0; */

# 0 "" 2
	.loc 1 304 5 view .LVU274
# 304 "<stdin>" 1
	/* "struct mp3entry", "length" = $424, $8, "u_l", $0; */

# 0 "" 2
	.loc 1 305 5 view .LVU275
# 305 "<stdin>" 1
	/* "struct mp3entry", "elapsed" = $432, $8, "u_l", $0; */

# 0 "" 2
	.loc 1 306 5 view .LVU276
# 306 "<stdin>" 1
	/* "struct mp3entry", "lead_trim" = $440, $4, "i", $0; */

# 0 "" 2
	.loc 1 307 5 view .LVU277
# 307 "<stdin>" 1
	/* "struct mp3entry", "tail_trim" = $444, $4, "i", $0; */

# 0 "" 2
	.loc 1 308 5 view .LVU278
# 308 "<stdin>" 1
	/* "struct mp3entry", "samples" = $448, $8, "u_i", $0; */

# 0 "" 2
	.loc 1 309 5 view .LVU279
# 309 "<stdin>" 1
	/* "struct mp3entry", "frame_count" = $456, $8, "u_l", $0; */

# 0 "" 2
	.loc 1 310 5 view .LVU280
# 310 "<stdin>" 1
	/* "struct mp3entry", "bytesperframe" = $464, $8, "u_l", $0; */

# 0 "" 2
	.loc 1 311 5 view .LVU281
# 311 "<stdin>" 1
	/* "struct mp3entry", "vbr" = $472, $1, "b", $0; */

# 0 "" 2
	.loc 1 312 5 view .LVU282
# 312 "<stdin>" 1
	/* "struct mp3entry", "has_toc" = $473, $1, "b", $0; */

# 0 "" 2
	.loc 1 313 5 view .LVU283
# 313 "<stdin>" 1
	/* "struct mp3entry", "toc" = $474, $100, "str", $0; */

# 0 "" 2
	.loc 1 314 5 view .LVU284
# 314 "<stdin>" 1
	/* "struct mp3entry", "needs_upsampling_correction" = $574, $1, "b", $0; */

# 0 "" 2
	.loc 1 315 5 view .LVU285
# 315 "<stdin>" 1
	/* "struct mp3entry", "id3v2buf" = $575, $1800, "str", $0; */

# 0 "" 2
	.loc 1 316 5 view .LVU286
# 316 "<stdin>" 1
	/* "struct mp3entry", "id3v1buf" = $2375, $368, "str", $0; */

# 0 "" 2
	.loc 1 317 5 view .LVU287
# 317 "<stdin>" 1
	/* "struct mp3entry", "offset" = $2744, $8, "u_l", $0; */

# 0 "" 2
	.loc 1 318 5 view .LVU288
# 318 "<stdin>" 1
	/* "struct mp3entry", "index" = $2752, $4, "i", $0; */

# 0 "" 2
	.loc 1 319 5 view .LVU289
# 319 "<stdin>" 1
	/* "struct mp3entry", "skip_resume_adjustments" = $2756, $1, "b", $0; */

# 0 "" 2
	.loc 1 320 5 view .LVU290
# 320 "<stdin>" 1
	/* "struct mp3entry", "autoresumable" = $2757, $1, "u_c", $0; */

# 0 "" 2
	.loc 1 321 5 view .LVU291
# 321 "<stdin>" 1
	/* "struct mp3entry", "tagcache_idx" = $2760, $8, "l", $0; */

# 0 "" 2
	.loc 1 322 5 view .LVU292
# 322 "<stdin>" 1
	/* "struct mp3entry", "rating" = $2768, $4, "i", $0; */

# 0 "" 2
	.loc 1 323 5 view .LVU293
# 323 "<stdin>" 1
	/* "struct mp3entry", "score" = $2772, $4, "i", $0; */

# 0 "" 2
	.loc 1 324 5 view .LVU294
# 324 "<stdin>" 1
	/* "struct mp3entry", "playcount" = $2776, $8, "l", $0; */

# 0 "" 2
	.loc 1 325 5 view .LVU295
# 325 "<stdin>" 1
	/* "struct mp3entry", "lastplayed" = $2784, $8, "l", $0; */

# 0 "" 2
	.loc 1 326 5 view .LVU296
# 326 "<stdin>" 1
	/* "struct mp3entry", "playtime" = $2792, $8, "l", $0; */

# 0 "" 2
	.loc 1 327 5 view .LVU297
# 327 "<stdin>" 1
	/* "struct mp3entry", "track_level" = $2800, $8, "l", $0; */

# 0 "" 2
	.loc 1 328 5 view .LVU298
# 328 "<stdin>" 1
	/* "struct mp3entry", "album_level" = $2808, $8, "l", $0; */

# 0 "" 2
	.loc 1 329 5 view .LVU299
# 329 "<stdin>" 1
	/* "struct mp3entry", "track_gain" = $2816, $8, "l", $0; */

# 0 "" 2
	.loc 1 330 5 view .LVU300
# 330 "<stdin>" 1
	/* "struct mp3entry", "album_gain" = $2824, $8, "l", $0; */

# 0 "" 2
	.loc 1 331 5 view .LVU301
# 331 "<stdin>" 1
	/* "struct mp3entry", "track_peak" = $2832, $8, "l", $0; */

# 0 "" 2
	.loc 1 332 5 view .LVU302
# 332 "<stdin>" 1
	/* "struct mp3entry", "album_peak" = $2840, $8, "l", $0; */

# 0 "" 2
	.loc 1 333 5 view .LVU303
# 333 "<stdin>" 1
	/* "struct mp3entry", "has_embedded_albumart" = $2848, $1, "b", $0; */

# 0 "" 2
	.loc 1 334 5 view .LVU304
# 334 "<stdin>" 1
	/* "struct mp3entry", "albumart" = $2856, $16, "s_mp3_albumart", $0; */

# 0 "" 2
	.loc 1 335 5 view .LVU305
# 335 "<stdin>" 1
	/* "struct mp3entry", "has_embedded_cuesheet" = $2872, $1, "b", $0; */

# 0 "" 2
	.loc 1 336 5 view .LVU306
# 336 "<stdin>" 1
	/* "struct mp3entry", "embedded_cuesheet" = $2880, $24, "s_embedded_cuesheet", $0; */

# 0 "" 2
	.loc 1 337 5 view .LVU307
# 337 "<stdin>" 1
	/* "struct mp3entry", "cuesheet" = $2904, $8, "ptr_s_cuesheet", $0; */

# 0 "" 2
	.loc 1 338 5 view .LVU308
# 338 "<stdin>" 1
	/* "struct mp3entry", "mb_track_id" = $2912, $8, "ptr_char", $0; */

# 0 "" 2
	.loc 1 339 5 view .LVU309
# 339 "<stdin>" 1
	/* "struct mp3entry", "is_asf_stream" = $2920, $1, "b", $0; */

# 0 "" 2
	.loc 1 340 5 view .LVU310
# 340 "<stdin>" 1
	/* "struct mp3entry", "has_video" = $2921, $1, "b", $0; */

# 0 "" 2
	.loc 1 341 1 view .LVU311
	.loc 1 342 5 view .LVU312
# 342 "<stdin>" 1
	/* "struct mp3_albumart", "type" = $0, $4, "e_mp3_aa_type", $0; */

# 0 "" 2
	.loc 1 343 5 view .LVU313
# 343 "<stdin>" 1
	/* "struct mp3_albumart", "size" = $4, $4, "i", $0; */

# 0 "" 2
	.loc 1 344 5 view .LVU314
# 344 "<stdin>" 1
	/* "struct mp3_albumart", "pos" = $8, $8, "off_t", $0; */

# 0 "" 2
	.loc 1 345 1 view .LVU315
	.loc 1 346 5 view .LVU316
# 346 "<stdin>" 1
	/* "struct embedded_cuesheet", "size" = $0, $4, "i", $0; */

# 0 "" 2
	.loc 1 347 5 view .LVU317
# 347 "<stdin>" 1
	/* "struct embedded_cuesheet", "pos" = $8, $8, "off_t", $0; */

# 0 "" 2
	.loc 1 348 5 view .LVU318
# 348 "<stdin>" 1
	/* "struct embedded_cuesheet", "encoding" = $16, $4, "e_character_encoding", $0; */

# 0 "" 2
	.loc 1 350 5 view .LVU319
	.loc 1 351 1 is_stmt 0 view .LVU320
#NO_APP
	xorl	%eax, %eax
	ret
	.cfi_endproc
.LFE96:
	.size	main, .-main
	.text
.Letext0:
	.file 2 "/usr/include/bits/types.h"
	.file 3 "/usr/include/bits/stdint-intn.h"
	.file 4 "/usr/include/bits/stdint-uintn.h"
	.file 5 "/usr/include/sys/types.h"
	.file 6 "/usr/lib/gcc/x86_64-pc-linux-gnu/16.1.1/include/stddef.h"
	.file 7 "/home/david/Documents/RockBox_Personal-master/lib/rbcodec/metadata/metadata.h"
	.file 8 "/home/david/Documents/RockBox_Personal-master/firmware/export/enc_base.h"
	.file 9 "/home/david/Documents/RockBox_Personal-master/lib/rbcodec/dsp/compressor.h"
	.file 10 "/home/david/Documents/RockBox_Personal-master/lib/rbcodec/dsp/dsp_misc.h"
	.file 11 "/home/david/Documents/RockBox_Personal-master/lib/rbcodec/dsp/eq.h"
	.file 12 "/home/david/Documents/RockBox_Personal-master/apps/settings.h"
	.file 13 "/home/david/Documents/RockBox_Personal-master/apps/settings_list.h"
	.section	.debug_info,"",@progbits
.Ldebug_info0:
	.long	0x190b
	.value	0x5
	.byte	0x1
	.byte	0x8
	.long	.Ldebug_abbrev0
	.uleb128 0x19
	.long	.LASF392
	.byte	0xc
	.long	.LASF0
	.long	.LASF1
	.long	.LLRL0
	.quad	0
	.long	.Ldebug_line0
	.uleb128 0x8
	.byte	0x1
	.byte	0x8
	.long	.LASF2
	.uleb128 0xc
	.long	0x2a
	.uleb128 0x8
	.byte	0x2
	.byte	0x7
	.long	.LASF3
	.uleb128 0x8
	.byte	0x4
	.byte	0x7
	.long	.LASF4
	.uleb128 0x8
	.byte	0x8
	.byte	0x7
	.long	.LASF5
	.uleb128 0x8
	.byte	0x1
	.byte	0x6
	.long	.LASF6
	.uleb128 0xa
	.long	.LASF8
	.byte	0x2
	.byte	0x27
	.byte	0x1a
	.long	0x5e
	.uleb128 0x8
	.byte	0x2
	.byte	0x5
	.long	.LASF7
	.uleb128 0xa
	.long	.LASF9
	.byte	0x2
	.byte	0x29
	.byte	0x14
	.long	0x71
	.uleb128 0x1a
	.byte	0x4
	.byte	0x5
	.string	"int"
	.uleb128 0xc
	.long	0x71
	.uleb128 0xa
	.long	.LASF10
	.byte	0x2
	.byte	0x2a
	.byte	0x16
	.long	0x3d
	.uleb128 0x8
	.byte	0x8
	.byte	0x5
	.long	.LASF11
	.uleb128 0xa
	.long	.LASF12
	.byte	0x2
	.byte	0x2d
	.byte	0x1b
	.long	0x44
	.uleb128 0xa
	.long	.LASF13
	.byte	0x2
	.byte	0x98
	.byte	0x19
	.long	0x89
	.uleb128 0x1b
	.byte	0x8
	.uleb128 0x4
	.long	0xaf
	.uleb128 0x8
	.byte	0x1
	.byte	0x6
	.long	.LASF14
	.uleb128 0xc
	.long	0xaf
	.uleb128 0xa
	.long	.LASF15
	.byte	0x3
	.byte	0x19
	.byte	0x13
	.long	0x52
	.uleb128 0xa
	.long	.LASF16
	.byte	0x3
	.byte	0x1a
	.byte	0x13
	.long	0x65
	.uleb128 0xa
	.long	.LASF17
	.byte	0x4
	.byte	0x1a
	.byte	0x14
	.long	0x7d
	.uleb128 0xa
	.long	.LASF18
	.byte	0x4
	.byte	0x1b
	.byte	0x14
	.long	0x90
	.uleb128 0xa
	.long	.LASF19
	.byte	0x5
	.byte	0x55
	.byte	0x11
	.long	0x9c
	.uleb128 0xa
	.long	.LASF20
	.byte	0x6
	.byte	0xe5
	.byte	0x17
	.long	0x44
	.uleb128 0x8
	.byte	0x8
	.byte	0x7
	.long	.LASF21
	.uleb128 0x8
	.byte	0x8
	.byte	0x5
	.long	.LASF22
	.uleb128 0x4
	.long	0xb6
	.uleb128 0x4
	.long	0x78
	.uleb128 0x4
	.long	0x120
	.uleb128 0x1c
	.uleb128 0x8
	.byte	0x1
	.byte	0x2
	.long	.LASF23
	.uleb128 0x8
	.byte	0x8
	.byte	0x4
	.long	.LASF24
	.uleb128 0x4
	.long	0x134
	.uleb128 0x11
	.long	0x13f
	.uleb128 0x7
	.long	0x71
	.byte	0
	.uleb128 0x8
	.byte	0x10
	.byte	0x5
	.long	.LASF25
	.uleb128 0x8
	.byte	0x10
	.byte	0x7
	.long	.LASF26
	.uleb128 0xb
	.long	0xaf
	.long	0x15e
	.uleb128 0x14
	.long	0x44
	.value	0x103
	.byte	0
	.uleb128 0x4
	.long	0x2a
	.uleb128 0x1d
	.long	.LASF92
	.value	0xb70
	.byte	0x7
	.byte	0xeb
	.byte	0x8
	.long	0x4dd
	.uleb128 0x2
	.long	.LASF27
	.byte	0x7
	.byte	0xec
	.byte	0xa
	.long	0x14d
	.byte	0
	.uleb128 0x5
	.long	.LASF28
	.byte	0xed
	.byte	0xb
	.long	0xaa
	.value	0x108
	.uleb128 0x5
	.long	.LASF29
	.byte	0xee
	.byte	0xb
	.long	0xaa
	.value	0x110
	.uleb128 0x5
	.long	.LASF30
	.byte	0xef
	.byte	0xb
	.long	0xaa
	.value	0x118
	.uleb128 0x5
	.long	.LASF31
	.byte	0xf0
	.byte	0xb
	.long	0xaa
	.value	0x120
	.uleb128 0x5
	.long	.LASF32
	.byte	0xf1
	.byte	0xb
	.long	0xaa
	.value	0x128
	.uleb128 0x5
	.long	.LASF33
	.byte	0xf2
	.byte	0xb
	.long	0xaa
	.value	0x130
	.uleb128 0x5
	.long	.LASF34
	.byte	0xf3
	.byte	0xb
	.long	0xaa
	.value	0x138
	.uleb128 0x5
	.long	.LASF35
	.byte	0xf4
	.byte	0xb
	.long	0xaa
	.value	0x140
	.uleb128 0x5
	.long	.LASF36
	.byte	0xf5
	.byte	0xb
	.long	0xaa
	.value	0x148
	.uleb128 0x5
	.long	.LASF37
	.byte	0xf6
	.byte	0xb
	.long	0xaa
	.value	0x150
	.uleb128 0x5
	.long	.LASF38
	.byte	0xf7
	.byte	0xb
	.long	0xaa
	.value	0x158
	.uleb128 0x5
	.long	.LASF39
	.byte	0xf8
	.byte	0x9
	.long	0x71
	.value	0x160
	.uleb128 0x5
	.long	.LASF40
	.byte	0xf9
	.byte	0x9
	.long	0x71
	.value	0x164
	.uleb128 0x5
	.long	.LASF41
	.byte	0xfa
	.byte	0x9
	.long	0x71
	.value	0x168
	.uleb128 0x5
	.long	.LASF42
	.byte	0xfb
	.byte	0x9
	.long	0x71
	.value	0x16c
	.uleb128 0x5
	.long	.LASF43
	.byte	0xfc
	.byte	0x13
	.long	0x2a
	.value	0x170
	.uleb128 0x5
	.long	.LASF44
	.byte	0xfd
	.byte	0x12
	.long	0x3d
	.value	0x174
	.uleb128 0x5
	.long	.LASF45
	.byte	0xfe
	.byte	0x12
	.long	0x3d
	.value	0x178
	.uleb128 0x5
	.long	.LASF46
	.byte	0xff
	.byte	0x13
	.long	0x44
	.value	0x180
	.uleb128 0x1
	.long	.LASF47
	.byte	0x7
	.value	0x100
	.byte	0x13
	.long	0x44
	.value	0x188
	.uleb128 0x1
	.long	.LASF48
	.byte	0x7
	.value	0x101
	.byte	0x13
	.long	0x44
	.value	0x190
	.uleb128 0x1
	.long	.LASF49
	.byte	0x7
	.value	0x102
	.byte	0x13
	.long	0x44
	.value	0x198
	.uleb128 0x1
	.long	.LASF50
	.byte	0x7
	.value	0x105
	.byte	0x13
	.long	0x44
	.value	0x1a0
	.uleb128 0x1
	.long	.LASF51
	.byte	0x7
	.value	0x106
	.byte	0x13
	.long	0x44
	.value	0x1a8
	.uleb128 0x1
	.long	.LASF52
	.byte	0x7
	.value	0x107
	.byte	0x13
	.long	0x44
	.value	0x1b0
	.uleb128 0x1
	.long	.LASF53
	.byte	0x7
	.value	0x109
	.byte	0x9
	.long	0x71
	.value	0x1b8
	.uleb128 0x1
	.long	.LASF54
	.byte	0x7
	.value	0x10a
	.byte	0x9
	.long	0x71
	.value	0x1bc
	.uleb128 0x1
	.long	.LASF55
	.byte	0x7
	.value	0x10d
	.byte	0xe
	.long	0xdf
	.value	0x1c0
	.uleb128 0x1
	.long	.LASF56
	.byte	0x7
	.value	0x110
	.byte	0x13
	.long	0x44
	.value	0x1c8
	.uleb128 0x1
	.long	.LASF57
	.byte	0x7
	.value	0x113
	.byte	0x13
	.long	0x44
	.value	0x1d0
	.uleb128 0x13
	.string	"vbr"
	.byte	0x7
	.value	0x116
	.byte	0xa
	.long	0x121
	.value	0x1d8
	.uleb128 0x1
	.long	.LASF58
	.byte	0x7
	.value	0x117
	.byte	0xa
	.long	0x121
	.value	0x1d9
	.uleb128 0x13
	.string	"toc"
	.byte	0x7
	.value	0x118
	.byte	0x13
	.long	0x59f
	.value	0x1da
	.uleb128 0x1
	.long	.LASF59
	.byte	0x7
	.value	0x11b
	.byte	0xa
	.long	0x121
	.value	0x23e
	.uleb128 0x1
	.long	.LASF60
	.byte	0x7
	.value	0x11e
	.byte	0xa
	.long	0x5af
	.value	0x23f
	.uleb128 0x1
	.long	.LASF61
	.byte	0x7
	.value	0x11f
	.byte	0xa
	.long	0x5c0
	.value	0x947
	.uleb128 0x1
	.long	.LASF62
	.byte	0x7
	.value	0x122
	.byte	0x13
	.long	0x44
	.value	0xab8
	.uleb128 0x1
	.long	.LASF63
	.byte	0x7
	.value	0x123
	.byte	0x9
	.long	0x71
	.value	0xac0
	.uleb128 0x1
	.long	.LASF64
	.byte	0x7
	.value	0x124
	.byte	0xa
	.long	0x121
	.value	0xac4
	.uleb128 0x1
	.long	.LASF65
	.byte	0x7
	.value	0x127
	.byte	0x13
	.long	0x2a
	.value	0xac5
	.uleb128 0x1
	.long	.LASF66
	.byte	0x7
	.value	0x12a
	.byte	0xa
	.long	0x89
	.value	0xac8
	.uleb128 0x1
	.long	.LASF67
	.byte	0x7
	.value	0x12b
	.byte	0x9
	.long	0x71
	.value	0xad0
	.uleb128 0x1
	.long	.LASF68
	.byte	0x7
	.value	0x12c
	.byte	0x9
	.long	0x71
	.value	0xad4
	.uleb128 0x1
	.long	.LASF69
	.byte	0x7
	.value	0x12d
	.byte	0xa
	.long	0x89
	.value	0xad8
	.uleb128 0x1
	.long	.LASF70
	.byte	0x7
	.value	0x12e
	.byte	0xa
	.long	0x89
	.value	0xae0
	.uleb128 0x1
	.long	.LASF71
	.byte	0x7
	.value	0x12f
	.byte	0xa
	.long	0x89
	.value	0xae8
	.uleb128 0x1
	.long	.LASF72
	.byte	0x7
	.value	0x133
	.byte	0xa
	.long	0x89
	.value	0xaf0
	.uleb128 0x1
	.long	.LASF73
	.byte	0x7
	.value	0x134
	.byte	0xa
	.long	0x89
	.value	0xaf8
	.uleb128 0x1
	.long	.LASF74
	.byte	0x7
	.value	0x135
	.byte	0xa
	.long	0x89
	.value	0xb00
	.uleb128 0x1
	.long	.LASF75
	.byte	0x7
	.value	0x136
	.byte	0xa
	.long	0x89
	.value	0xb08
	.uleb128 0x1
	.long	.LASF76
	.byte	0x7
	.value	0x137
	.byte	0xa
	.long	0x89
	.value	0xb10
	.uleb128 0x1
	.long	.LASF77
	.byte	0x7
	.value	0x138
	.byte	0xa
	.long	0x89
	.value	0xb18
	.uleb128 0x1
	.long	.LASF78
	.byte	0x7
	.value	0x13b
	.byte	0xa
	.long	0x121
	.value	0xb20
	.uleb128 0x1
	.long	.LASF79
	.byte	0x7
	.value	0x13c
	.byte	0x19
	.long	0x510
	.value	0xb28
	.uleb128 0x1
	.long	.LASF80
	.byte	0x7
	.value	0x140
	.byte	0xa
	.long	0x121
	.value	0xb38
	.uleb128 0x1
	.long	.LASF81
	.byte	0x7
	.value	0x141
	.byte	0x1e
	.long	0x56b
	.value	0xb40
	.uleb128 0x1
	.long	.LASF82
	.byte	0x7
	.value	0x142
	.byte	0x16
	.long	0x5db
	.value	0xb58
	.uleb128 0x1
	.long	.LASF83
	.byte	0x7
	.value	0x145
	.byte	0xb
	.long	0xaa
	.value	0xb60
	.uleb128 0x1
	.long	.LASF84
	.byte	0x7
	.value	0x148
	.byte	0xa
	.long	0x121
	.value	0xb68
	.uleb128 0x1
	.long	.LASF85
	.byte	0x7
	.value	0x14b
	.byte	0xa
	.long	0x121
	.value	0xb69
	.byte	0
	.uleb128 0x15
	.long	.LASF96
	.long	0x3d
	.byte	0xcc
	.long	0x510
	.uleb128 0xe
	.long	.LASF86
	.byte	0
	.uleb128 0xe
	.long	.LASF87
	.byte	0x1
	.uleb128 0xe
	.long	.LASF88
	.byte	0x2
	.uleb128 0xe
	.long	.LASF89
	.byte	0x3
	.uleb128 0xe
	.long	.LASF90
	.byte	0x10
	.uleb128 0xe
	.long	.LASF91
	.byte	0x20
	.byte	0
	.uleb128 0x9
	.long	.LASF93
	.byte	0x10
	.byte	0x7
	.byte	0xd6
	.long	0x544
	.uleb128 0x2
	.long	.LASF94
	.byte	0x7
	.byte	0xd7
	.byte	0x16
	.long	0x4dd
	.byte	0
	.uleb128 0x2
	.long	.LASF95
	.byte	0x7
	.byte	0xd8
	.byte	0x9
	.long	0x71
	.byte	0x4
	.uleb128 0x10
	.string	"pos"
	.byte	0x7
	.byte	0xd9
	.byte	0xb
	.long	0xeb
	.byte	0x8
	.byte	0
	.uleb128 0x15
	.long	.LASF97
	.long	0x3d
	.byte	0xdd
	.long	0x56b
	.uleb128 0xe
	.long	.LASF98
	.byte	0x1
	.uleb128 0xe
	.long	.LASF99
	.byte	0x2
	.uleb128 0xe
	.long	.LASF100
	.byte	0x3
	.uleb128 0xe
	.long	.LASF101
	.byte	0x4
	.byte	0
	.uleb128 0x9
	.long	.LASF81
	.byte	0x18
	.byte	0x7
	.byte	0xe5
	.long	0x59f
	.uleb128 0x2
	.long	.LASF95
	.byte	0x7
	.byte	0xe6
	.byte	0x9
	.long	0x71
	.byte	0
	.uleb128 0x10
	.string	"pos"
	.byte	0x7
	.byte	0xe7
	.byte	0xb
	.long	0xeb
	.byte	0x8
	.uleb128 0x2
	.long	.LASF102
	.byte	0x7
	.byte	0xe8
	.byte	0x1d
	.long	0x544
	.byte	0x10
	.byte	0
	.uleb128 0xb
	.long	0x2a
	.long	0x5af
	.uleb128 0xd
	.long	0x44
	.byte	0x63
	.byte	0
	.uleb128 0xb
	.long	0xaf
	.long	0x5c0
	.uleb128 0x14
	.long	0x44
	.value	0x707
	.byte	0
	.uleb128 0xb
	.long	0xaf
	.long	0x5d6
	.uleb128 0xd
	.long	0x44
	.byte	0x3
	.uleb128 0xd
	.long	0x44
	.byte	0x5b
	.byte	0
	.uleb128 0x1e
	.long	.LASF82
	.uleb128 0x4
	.long	0x5d6
	.uleb128 0x1f
	.long	0x71
	.uleb128 0x4
	.long	0x5e0
	.uleb128 0x4
	.long	0x31
	.uleb128 0x9
	.long	.LASF103
	.byte	0x8
	.byte	0x8
	.byte	0x6f
	.long	0x609
	.uleb128 0x2
	.long	.LASF45
	.byte	0x8
	.byte	0x71
	.byte	0x13
	.long	0x44
	.byte	0
	.byte	0
	.uleb128 0x9
	.long	.LASF104
	.byte	0x18
	.byte	0x9
	.byte	0x18
	.long	0x664
	.uleb128 0x2
	.long	.LASF105
	.byte	0x9
	.byte	0x1a
	.byte	0x9
	.long	0x71
	.byte	0
	.uleb128 0x2
	.long	.LASF106
	.byte	0x9
	.byte	0x1b
	.byte	0x9
	.long	0x71
	.byte	0x4
	.uleb128 0x2
	.long	.LASF107
	.byte	0x9
	.byte	0x1c
	.byte	0x9
	.long	0x71
	.byte	0x8
	.uleb128 0x2
	.long	.LASF108
	.byte	0x9
	.byte	0x1d
	.byte	0x9
	.long	0x71
	.byte	0xc
	.uleb128 0x2
	.long	.LASF109
	.byte	0x9
	.byte	0x1e
	.byte	0x9
	.long	0x71
	.byte	0x10
	.uleb128 0x2
	.long	.LASF110
	.byte	0x9
	.byte	0x1f
	.byte	0x9
	.long	0x71
	.byte	0x14
	.byte	0
	.uleb128 0x9
	.long	.LASF111
	.byte	0xc
	.byte	0xa
	.byte	0x25
	.long	0x698
	.uleb128 0x2
	.long	.LASF112
	.byte	0xa
	.byte	0x27
	.byte	0xa
	.long	0x121
	.byte	0
	.uleb128 0x2
	.long	.LASF94
	.byte	0xa
	.byte	0x28
	.byte	0x9
	.long	0x71
	.byte	0x4
	.uleb128 0x2
	.long	.LASF113
	.byte	0xa
	.byte	0x2a
	.byte	0x9
	.long	0x71
	.byte	0x8
	.byte	0
	.uleb128 0x9
	.long	.LASF114
	.byte	0xc
	.byte	0xb
	.byte	0x1b
	.long	0x6ca
	.uleb128 0x2
	.long	.LASF115
	.byte	0xb
	.byte	0x1d
	.byte	0x9
	.long	0x71
	.byte	0
	.uleb128 0x10
	.string	"q"
	.byte	0xb
	.byte	0x1e
	.byte	0x9
	.long	0x71
	.byte	0x4
	.uleb128 0x2
	.long	.LASF116
	.byte	0xb
	.byte	0x1f
	.byte	0x9
	.long	0x71
	.byte	0x8
	.byte	0
	.uleb128 0x20
	.long	.LASF117
	.byte	0x38
	.byte	0xc
	.value	0x15c
	.byte	0x8
	.long	0x78f
	.uleb128 0x3
	.long	.LASF118
	.value	0x15e
	.byte	0x9
	.long	0x71
	.byte	0
	.uleb128 0x3
	.long	.LASF119
	.value	0x15f
	.byte	0x9
	.long	0x71
	.byte	0x4
	.uleb128 0x3
	.long	.LASF120
	.value	0x160
	.byte	0xe
	.long	0xd3
	.byte	0x8
	.uleb128 0x3
	.long	.LASF121
	.value	0x161
	.byte	0xe
	.long	0xd3
	.byte	0xc
	.uleb128 0x3
	.long	.LASF122
	.value	0x162
	.byte	0xe
	.long	0xd3
	.byte	0x10
	.uleb128 0x3
	.long	.LASF123
	.value	0x164
	.byte	0xd
	.long	0xc7
	.byte	0x14
	.uleb128 0x3
	.long	.LASF124
	.value	0x165
	.byte	0xd
	.long	0xc7
	.byte	0x18
	.uleb128 0x3
	.long	.LASF125
	.value	0x167
	.byte	0x9
	.long	0x71
	.byte	0x1c
	.uleb128 0x3
	.long	.LASF126
	.value	0x168
	.byte	0x9
	.long	0x71
	.byte	0x20
	.uleb128 0x3
	.long	.LASF127
	.value	0x170
	.byte	0x11
	.long	0x4b
	.byte	0x24
	.uleb128 0x3
	.long	.LASF128
	.value	0x171
	.byte	0xa
	.long	0x71
	.byte	0x28
	.uleb128 0x3
	.long	.LASF129
	.value	0x172
	.byte	0x9
	.long	0x71
	.byte	0x2c
	.uleb128 0x3
	.long	.LASF130
	.value	0x173
	.byte	0x9
	.long	0x78f
	.byte	0x30
	.uleb128 0x3
	.long	.LASF131
	.value	0x175
	.byte	0xa
	.long	0x121
	.byte	0x34
	.byte	0
	.uleb128 0xb
	.long	0x71
	.long	0x79f
	.uleb128 0xd
	.long	0x44
	.byte	0
	.byte	0
	.uleb128 0x21
	.long	.LASF132
	.value	0x788
	.byte	0xc
	.value	0x178
	.byte	0x8
	.long	0x13f9
	.uleb128 0x3
	.long	.LASF133
	.value	0x17b
	.byte	0x9
	.long	0x71
	.byte	0
	.uleb128 0x3
	.long	.LASF134
	.value	0x17c
	.byte	0x9
	.long	0x71
	.byte	0x4
	.uleb128 0x3
	.long	.LASF135
	.value	0x17d
	.byte	0x9
	.long	0x71
	.byte	0x8
	.uleb128 0x3
	.long	.LASF136
	.value	0x17e
	.byte	0x9
	.long	0x71
	.byte	0xc
	.uleb128 0x3
	.long	.LASF137
	.value	0x17f
	.byte	0x9
	.long	0x71
	.byte	0x10
	.uleb128 0x3
	.long	.LASF138
	.value	0x182
	.byte	0x9
	.long	0x71
	.byte	0x14
	.uleb128 0x3
	.long	.LASF139
	.value	0x185
	.byte	0x9
	.long	0x71
	.byte	0x18
	.uleb128 0x3
	.long	.LASF140
	.value	0x18a
	.byte	0x9
	.long	0x71
	.byte	0x1c
	.uleb128 0x3
	.long	.LASF141
	.value	0x18c
	.byte	0x9
	.long	0x71
	.byte	0x20
	.uleb128 0x3
	.long	.LASF142
	.value	0x18d
	.byte	0x9
	.long	0x71
	.byte	0x24
	.uleb128 0x3
	.long	.LASF143
	.value	0x18e
	.byte	0x9
	.long	0x71
	.byte	0x28
	.uleb128 0x3
	.long	.LASF144
	.value	0x18f
	.byte	0x9
	.long	0x71
	.byte	0x2c
	.uleb128 0x3
	.long	.LASF145
	.value	0x190
	.byte	0x9
	.long	0x71
	.byte	0x30
	.uleb128 0x3
	.long	.LASF111
	.value	0x194
	.byte	0x20
	.long	0x664
	.byte	0x34
	.uleb128 0x3
	.long	.LASF146
	.value	0x197
	.byte	0x9
	.long	0x71
	.byte	0x40
	.uleb128 0x3
	.long	.LASF147
	.value	0x198
	.byte	0x12
	.long	0x3d
	.byte	0x44
	.uleb128 0x3
	.long	.LASF148
	.value	0x199
	.byte	0x12
	.long	0x3d
	.byte	0x48
	.uleb128 0x3
	.long	.LASF149
	.value	0x19a
	.byte	0x12
	.long	0x3d
	.byte	0x4c
	.uleb128 0x3
	.long	.LASF150
	.value	0x19b
	.byte	0x12
	.long	0x3d
	.byte	0x50
	.uleb128 0x3
	.long	.LASF151
	.value	0x19e
	.byte	0xa
	.long	0x121
	.byte	0x54
	.uleb128 0x3
	.long	.LASF152
	.value	0x19f
	.byte	0x12
	.long	0x3d
	.byte	0x58
	.uleb128 0x3
	.long	.LASF153
	.value	0x1a0
	.byte	0x1c
	.long	0x13f9
	.byte	0x5c
	.uleb128 0x3
	.long	.LASF154
	.value	0x1a3
	.byte	0xa
	.long	0x71
	.byte	0xd4
	.uleb128 0x3
	.long	.LASF155
	.value	0x1a4
	.byte	0xa
	.long	0x71
	.byte	0xd8
	.uleb128 0x3
	.long	.LASF156
	.value	0x1a5
	.byte	0xa
	.long	0x71
	.byte	0xdc
	.uleb128 0x3
	.long	.LASF157
	.value	0x1a6
	.byte	0xa
	.long	0x121
	.byte	0xe0
	.uleb128 0x3
	.long	.LASF158
	.value	0x1a8
	.byte	0xa
	.long	0x121
	.byte	0xe1
	.uleb128 0x3
	.long	.LASF159
	.value	0x1ac
	.byte	0x9
	.long	0x71
	.byte	0xe4
	.uleb128 0x3
	.long	.LASF160
	.value	0x1ad
	.byte	0x9
	.long	0x71
	.byte	0xe8
	.uleb128 0x3
	.long	.LASF103
	.value	0x1b0
	.byte	0x1f
	.long	0x5ef
	.byte	0xf0
	.uleb128 0x3
	.long	.LASF161
	.value	0x1b9
	.byte	0x9
	.long	0x71
	.byte	0xf8
	.uleb128 0x3
	.long	.LASF162
	.value	0x1ba
	.byte	0x9
	.long	0x71
	.byte	0xfc
	.uleb128 0x1
	.long	.LASF163
	.byte	0xc
	.value	0x1c0
	.byte	0x9
	.long	0x71
	.value	0x100
	.uleb128 0x1
	.long	.LASF164
	.byte	0xc
	.value	0x1c2
	.byte	0x9
	.long	0x71
	.value	0x104
	.uleb128 0x1
	.long	.LASF165
	.byte	0xc
	.value	0x1c3
	.byte	0x9
	.long	0x71
	.value	0x108
	.uleb128 0x1
	.long	.LASF166
	.byte	0xc
	.value	0x1c4
	.byte	0x9
	.long	0x71
	.value	0x10c
	.uleb128 0x1
	.long	.LASF167
	.byte	0xc
	.value	0x1c5
	.byte	0xa
	.long	0x121
	.value	0x110
	.uleb128 0x1
	.long	.LASF168
	.byte	0xc
	.value	0x1c6
	.byte	0xa
	.long	0x121
	.value	0x111
	.uleb128 0x1
	.long	.LASF169
	.byte	0xc
	.value	0x1c9
	.byte	0x9
	.long	0x71
	.value	0x114
	.uleb128 0x1
	.long	.LASF170
	.byte	0xc
	.value	0x1ca
	.byte	0x9
	.long	0x71
	.value	0x118
	.uleb128 0x1
	.long	.LASF171
	.byte	0xc
	.value	0x1cf
	.byte	0x9
	.long	0x71
	.value	0x11c
	.uleb128 0x1
	.long	.LASF172
	.byte	0xc
	.value	0x1d0
	.byte	0x9
	.long	0x71
	.value	0x120
	.uleb128 0x1
	.long	.LASF173
	.byte	0xc
	.value	0x1d2
	.byte	0x9
	.long	0x71
	.value	0x124
	.uleb128 0x1
	.long	.LASF174
	.byte	0xc
	.value	0x1d3
	.byte	0xa
	.long	0x1409
	.value	0x128
	.uleb128 0x1
	.long	.LASF175
	.byte	0xc
	.value	0x1d4
	.byte	0x9
	.long	0x71
	.value	0x17c
	.uleb128 0x1
	.long	.LASF176
	.byte	0xc
	.value	0x1d9
	.byte	0x9
	.long	0x71
	.value	0x180
	.uleb128 0x1
	.long	.LASF177
	.byte	0xc
	.value	0x1da
	.byte	0x9
	.long	0x71
	.value	0x184
	.uleb128 0x1
	.long	.LASF178
	.byte	0xc
	.value	0x1db
	.byte	0x9
	.long	0x71
	.value	0x188
	.uleb128 0x1
	.long	.LASF179
	.byte	0xc
	.value	0x1dc
	.byte	0x9
	.long	0x71
	.value	0x18c
	.uleb128 0x1
	.long	.LASF180
	.byte	0xc
	.value	0x1dd
	.byte	0x9
	.long	0x71
	.value	0x190
	.uleb128 0x1
	.long	.LASF181
	.byte	0xc
	.value	0x1de
	.byte	0x9
	.long	0x71
	.value	0x194
	.uleb128 0x1
	.long	.LASF182
	.byte	0xc
	.value	0x1df
	.byte	0x9
	.long	0x71
	.value	0x198
	.uleb128 0x1
	.long	.LASF183
	.byte	0xc
	.value	0x1e0
	.byte	0x9
	.long	0x71
	.value	0x19c
	.uleb128 0x1
	.long	.LASF184
	.byte	0xc
	.value	0x1e1
	.byte	0x9
	.long	0x71
	.value	0x1a0
	.uleb128 0x1
	.long	.LASF185
	.byte	0xc
	.value	0x216
	.byte	0xa
	.long	0x71
	.value	0x1a4
	.uleb128 0x1
	.long	.LASF186
	.byte	0xc
	.value	0x218
	.byte	0xa
	.long	0x71
	.value	0x1a8
	.uleb128 0x1
	.long	.LASF187
	.byte	0xc
	.value	0x219
	.byte	0xa
	.long	0x121
	.value	0x1ac
	.uleb128 0x1
	.long	.LASF188
	.byte	0xc
	.value	0x21d
	.byte	0x21
	.long	0x1419
	.value	0x1b0
	.uleb128 0x1
	.long	.LASF189
	.byte	0xc
	.value	0x220
	.byte	0x9
	.long	0x71
	.value	0x1d0
	.uleb128 0x1
	.long	.LASF190
	.byte	0xc
	.value	0x223
	.byte	0x9
	.long	0x71
	.value	0x1d4
	.uleb128 0x1
	.long	.LASF191
	.byte	0xc
	.value	0x224
	.byte	0x9
	.long	0x71
	.value	0x1d8
	.uleb128 0x1
	.long	.LASF192
	.byte	0xc
	.value	0x225
	.byte	0x9
	.long	0x71
	.value	0x1dc
	.uleb128 0x1
	.long	.LASF193
	.byte	0xc
	.value	0x228
	.byte	0x9
	.long	0x71
	.value	0x1e0
	.uleb128 0x1
	.long	.LASF194
	.byte	0xc
	.value	0x22a
	.byte	0x9
	.long	0x71
	.value	0x1e4
	.uleb128 0x1
	.long	.LASF195
	.byte	0xc
	.value	0x22c
	.byte	0x9
	.long	0x71
	.value	0x1e8
	.uleb128 0x1
	.long	.LASF196
	.byte	0xc
	.value	0x22d
	.byte	0xa
	.long	0x121
	.value	0x1ec
	.uleb128 0x1
	.long	.LASF197
	.byte	0xc
	.value	0x22e
	.byte	0xa
	.long	0x121
	.value	0x1ed
	.uleb128 0x1
	.long	.LASF198
	.byte	0xc
	.value	0x22f
	.byte	0x9
	.long	0x71
	.value	0x1f0
	.uleb128 0x1
	.long	.LASF199
	.byte	0xc
	.value	0x231
	.byte	0xa
	.long	0x121
	.value	0x1f4
	.uleb128 0x1
	.long	.LASF82
	.byte	0xc
	.value	0x232
	.byte	0xa
	.long	0x121
	.value	0x1f5
	.uleb128 0x1
	.long	.LASF200
	.byte	0xc
	.value	0x233
	.byte	0xa
	.long	0x121
	.value	0x1f6
	.uleb128 0x1
	.long	.LASF201
	.byte	0xc
	.value	0x234
	.byte	0x9
	.long	0x71
	.value	0x1f8
	.uleb128 0x1
	.long	.LASF202
	.byte	0xc
	.value	0x235
	.byte	0x9
	.long	0x71
	.value	0x1fc
	.uleb128 0x1
	.long	.LASF203
	.byte	0xc
	.value	0x236
	.byte	0x9
	.long	0x71
	.value	0x200
	.uleb128 0x1
	.long	.LASF204
	.byte	0xc
	.value	0x23b
	.byte	0x9
	.long	0x71
	.value	0x204
	.uleb128 0x1
	.long	.LASF205
	.byte	0xc
	.value	0x23c
	.byte	0x9
	.long	0x71
	.value	0x208
	.uleb128 0x1
	.long	.LASF206
	.byte	0xc
	.value	0x23e
	.byte	0x9
	.long	0x71
	.value	0x20c
	.uleb128 0x1
	.long	.LASF207
	.byte	0xc
	.value	0x23f
	.byte	0x9
	.long	0x71
	.value	0x210
	.uleb128 0x1
	.long	.LASF208
	.byte	0xc
	.value	0x240
	.byte	0x9
	.long	0x71
	.value	0x214
	.uleb128 0x1
	.long	.LASF209
	.byte	0xc
	.value	0x241
	.byte	0xa
	.long	0x121
	.value	0x218
	.uleb128 0x1
	.long	.LASF210
	.byte	0xc
	.value	0x242
	.byte	0x9
	.long	0x71
	.value	0x21c
	.uleb128 0x1
	.long	.LASF211
	.byte	0xc
	.value	0x243
	.byte	0x9
	.long	0x71
	.value	0x220
	.uleb128 0x1
	.long	.LASF212
	.byte	0xc
	.value	0x245
	.byte	0x13
	.long	0x1487
	.value	0x224
	.uleb128 0x1
	.long	.LASF213
	.byte	0xc
	.value	0x246
	.byte	0x13
	.long	0x1487
	.value	0x245
	.uleb128 0x1
	.long	.LASF214
	.byte	0xc
	.value	0x24b
	.byte	0x13
	.long	0x1487
	.value	0x266
	.uleb128 0x1
	.long	.LASF215
	.byte	0xc
	.value	0x24c
	.byte	0x13
	.long	0x1497
	.value	0x287
	.uleb128 0x1
	.long	.LASF216
	.byte	0xc
	.value	0x24d
	.byte	0x9
	.long	0x71
	.value	0x2d8
	.uleb128 0x1
	.long	.LASF217
	.byte	0xc
	.value	0x24e
	.byte	0x9
	.long	0x71
	.value	0x2dc
	.uleb128 0x1
	.long	.LASF218
	.byte	0xc
	.value	0x24f
	.byte	0x9
	.long	0x71
	.value	0x2e0
	.uleb128 0x1
	.long	.LASF219
	.byte	0xc
	.value	0x250
	.byte	0x9
	.long	0x71
	.value	0x2e4
	.uleb128 0x1
	.long	.LASF220
	.byte	0xc
	.value	0x251
	.byte	0x9
	.long	0x71
	.value	0x2e8
	.uleb128 0x1
	.long	.LASF221
	.byte	0xc
	.value	0x252
	.byte	0xa
	.long	0x121
	.value	0x2ec
	.uleb128 0x1
	.long	.LASF222
	.byte	0xc
	.value	0x253
	.byte	0x9
	.long	0x71
	.value	0x2f0
	.uleb128 0x1
	.long	.LASF223
	.byte	0xc
	.value	0x258
	.byte	0x9
	.long	0x71
	.value	0x2f4
	.uleb128 0x1
	.long	.LASF224
	.byte	0xc
	.value	0x259
	.byte	0x9
	.long	0x71
	.value	0x2f8
	.uleb128 0x1
	.long	.LASF225
	.byte	0xc
	.value	0x25f
	.byte	0x9
	.long	0x71
	.value	0x2fc
	.uleb128 0x1
	.long	.LASF226
	.byte	0xc
	.value	0x260
	.byte	0x9
	.long	0x71
	.value	0x300
	.uleb128 0x1
	.long	.LASF227
	.byte	0xc
	.value	0x263
	.byte	0xa
	.long	0x121
	.value	0x304
	.uleb128 0x1
	.long	.LASF228
	.byte	0xc
	.value	0x265
	.byte	0xa
	.long	0x121
	.value	0x305
	.uleb128 0x1
	.long	.LASF229
	.byte	0xc
	.value	0x266
	.byte	0xa
	.long	0x121
	.value	0x306
	.uleb128 0x1
	.long	.LASF230
	.byte	0xc
	.value	0x267
	.byte	0xa
	.long	0x71
	.value	0x308
	.uleb128 0x1
	.long	.LASF231
	.byte	0xc
	.value	0x268
	.byte	0xa
	.long	0x71
	.value	0x30c
	.uleb128 0x1
	.long	.LASF232
	.byte	0xc
	.value	0x269
	.byte	0xa
	.long	0x71
	.value	0x310
	.uleb128 0x1
	.long	.LASF233
	.byte	0xc
	.value	0x26a
	.byte	0xa
	.long	0x71
	.value	0x314
	.uleb128 0x1
	.long	.LASF234
	.byte	0xc
	.value	0x26b
	.byte	0xa
	.long	0x71
	.value	0x318
	.uleb128 0x1
	.long	.LASF235
	.byte	0xc
	.value	0x26e
	.byte	0x9
	.long	0x71
	.value	0x31c
	.uleb128 0x1
	.long	.LASF236
	.byte	0xc
	.value	0x26f
	.byte	0x9
	.long	0x71
	.value	0x320
	.uleb128 0x1
	.long	.LASF237
	.byte	0xc
	.value	0x270
	.byte	0xa
	.long	0x121
	.value	0x324
	.uleb128 0x1
	.long	.LASF238
	.byte	0xc
	.value	0x271
	.byte	0x9
	.long	0x71
	.value	0x328
	.uleb128 0x1
	.long	.LASF239
	.byte	0xc
	.value	0x279
	.byte	0x9
	.long	0x71
	.value	0x32c
	.uleb128 0x1
	.long	.LASF240
	.byte	0xc
	.value	0x27b
	.byte	0xa
	.long	0x121
	.value	0x330
	.uleb128 0x1
	.long	.LASF241
	.byte	0xc
	.value	0x27c
	.byte	0xa
	.long	0x121
	.value	0x331
	.uleb128 0x1
	.long	.LASF242
	.byte	0xc
	.value	0x27d
	.byte	0x9
	.long	0x71
	.value	0x334
	.uleb128 0x1
	.long	.LASF243
	.byte	0xc
	.value	0x27f
	.byte	0x13
	.long	0x14a7
	.value	0x338
	.uleb128 0x1
	.long	.LASF244
	.byte	0xc
	.value	0x280
	.byte	0xa
	.long	0x121
	.value	0x3d9
	.uleb128 0x1
	.long	.LASF245
	.byte	0xc
	.value	0x281
	.byte	0x13
	.long	0x14a7
	.value	0x3da
	.uleb128 0x1
	.long	.LASF246
	.byte	0xc
	.value	0x282
	.byte	0x13
	.long	0x1497
	.value	0x47b
	.uleb128 0x1
	.long	.LASF247
	.byte	0xc
	.value	0x286
	.byte	0x13
	.long	0x1497
	.value	0x4cc
	.uleb128 0x1
	.long	.LASF248
	.byte	0xc
	.value	0x28a
	.byte	0x9
	.long	0x71
	.value	0x520
	.uleb128 0x1
	.long	.LASF249
	.byte	0xc
	.value	0x28b
	.byte	0x9
	.long	0x71
	.value	0x524
	.uleb128 0x1
	.long	.LASF250
	.byte	0xc
	.value	0x28c
	.byte	0x9
	.long	0x71
	.value	0x528
	.uleb128 0x1
	.long	.LASF251
	.byte	0xc
	.value	0x28d
	.byte	0x9
	.long	0x71
	.value	0x52c
	.uleb128 0x1
	.long	.LASF252
	.byte	0xc
	.value	0x28e
	.byte	0x9
	.long	0x71
	.value	0x530
	.uleb128 0x1
	.long	.LASF253
	.byte	0xc
	.value	0x28f
	.byte	0x13
	.long	0x1487
	.value	0x534
	.uleb128 0x1
	.long	.LASF254
	.byte	0xc
	.value	0x291
	.byte	0xa
	.long	0x121
	.value	0x555
	.uleb128 0x1
	.long	.LASF255
	.byte	0xc
	.value	0x295
	.byte	0x9
	.long	0x71
	.value	0x558
	.uleb128 0x1
	.long	.LASF256
	.byte	0xc
	.value	0x296
	.byte	0x9
	.long	0x71
	.value	0x55c
	.uleb128 0x1
	.long	.LASF257
	.byte	0xc
	.value	0x297
	.byte	0x9
	.long	0x71
	.value	0x560
	.uleb128 0x1
	.long	.LASF258
	.byte	0xc
	.value	0x298
	.byte	0x9
	.long	0x71
	.value	0x564
	.uleb128 0x1
	.long	.LASF259
	.byte	0xc
	.value	0x29a
	.byte	0x9
	.long	0x71
	.value	0x568
	.uleb128 0x1
	.long	.LASF260
	.byte	0xc
	.value	0x29d
	.byte	0xa
	.long	0x71
	.value	0x56c
	.uleb128 0x1
	.long	.LASF261
	.byte	0xc
	.value	0x29e
	.byte	0xa
	.long	0x71
	.value	0x570
	.uleb128 0x1
	.long	.LASF262
	.byte	0xc
	.value	0x29f
	.byte	0xa
	.long	0x121
	.value	0x574
	.uleb128 0x1
	.long	.LASF263
	.byte	0xc
	.value	0x2a1
	.byte	0xa
	.long	0x71
	.value	0x578
	.uleb128 0x1
	.long	.LASF264
	.byte	0xc
	.value	0x2a2
	.byte	0xa
	.long	0x121
	.value	0x57c
	.uleb128 0x1
	.long	.LASF265
	.byte	0xc
	.value	0x2a3
	.byte	0xa
	.long	0x121
	.value	0x57d
	.uleb128 0x1
	.long	.LASF266
	.byte	0xc
	.value	0x2a4
	.byte	0xa
	.long	0x121
	.value	0x57e
	.uleb128 0x1
	.long	.LASF267
	.byte	0xc
	.value	0x2a5
	.byte	0xa
	.long	0x121
	.value	0x57f
	.uleb128 0x1
	.long	.LASF268
	.byte	0xc
	.value	0x2a6
	.byte	0xa
	.long	0x121
	.value	0x580
	.uleb128 0x1
	.long	.LASF269
	.byte	0xc
	.value	0x2a7
	.byte	0x9
	.long	0x71
	.value	0x584
	.uleb128 0x1
	.long	.LASF270
	.byte	0xc
	.value	0x2a9
	.byte	0x9
	.long	0x71
	.value	0x588
	.uleb128 0x1
	.long	.LASF271
	.byte	0xc
	.value	0x2ab
	.byte	0xa
	.long	0x121
	.value	0x58c
	.uleb128 0x1
	.long	.LASF272
	.byte	0xc
	.value	0x2ae
	.byte	0xa
	.long	0x121
	.value	0x58d
	.uleb128 0x1
	.long	.LASF273
	.byte	0xc
	.value	0x2af
	.byte	0xa
	.long	0x121
	.value	0x58e
	.uleb128 0x1
	.long	.LASF274
	.byte	0xc
	.value	0x2b0
	.byte	0x9
	.long	0x71
	.value	0x590
	.uleb128 0x1
	.long	.LASF275
	.byte	0xc
	.value	0x2b3
	.byte	0xa
	.long	0x121
	.value	0x594
	.uleb128 0x1
	.long	.LASF276
	.byte	0xc
	.value	0x2b4
	.byte	0x9
	.long	0x71
	.value	0x598
	.uleb128 0x1
	.long	.LASF277
	.byte	0xc
	.value	0x2b5
	.byte	0xa
	.long	0x121
	.value	0x59c
	.uleb128 0x1
	.long	.LASF278
	.byte	0xc
	.value	0x2b6
	.byte	0x9
	.long	0x71
	.value	0x5a0
	.uleb128 0x1
	.long	.LASF279
	.byte	0xc
	.value	0x2b7
	.byte	0xa
	.long	0x121
	.value	0x5a4
	.uleb128 0x1
	.long	.LASF280
	.byte	0xc
	.value	0x2b8
	.byte	0xa
	.long	0x121
	.value	0x5a5
	.uleb128 0x1
	.long	.LASF281
	.byte	0xc
	.value	0x2b9
	.byte	0xa
	.long	0x121
	.value	0x5a6
	.uleb128 0x1
	.long	.LASF282
	.byte	0xc
	.value	0x2ba
	.byte	0xa
	.long	0x71
	.value	0x5a8
	.uleb128 0x1
	.long	.LASF283
	.byte	0xc
	.value	0x2bd
	.byte	0xa
	.long	0x121
	.value	0x5ac
	.uleb128 0x1
	.long	.LASF284
	.byte	0xc
	.value	0x2be
	.byte	0x9
	.long	0x71
	.value	0x5b0
	.uleb128 0x1
	.long	.LASF285
	.byte	0xc
	.value	0x2bf
	.byte	0x9
	.long	0x71
	.value	0x5b4
	.uleb128 0x1
	.long	.LASF286
	.byte	0xc
	.value	0x2c0
	.byte	0x9
	.long	0x71
	.value	0x5b8
	.uleb128 0x1
	.long	.LASF287
	.byte	0xc
	.value	0x2c1
	.byte	0x9
	.long	0x71
	.value	0x5bc
	.uleb128 0x1
	.long	.LASF288
	.byte	0xc
	.value	0x2c4
	.byte	0x9
	.long	0x71
	.value	0x5c0
	.uleb128 0x1
	.long	.LASF289
	.byte	0xc
	.value	0x2c6
	.byte	0x9
	.long	0x71
	.value	0x5c4
	.uleb128 0x1
	.long	.LASF290
	.byte	0xc
	.value	0x2cc
	.byte	0x9
	.long	0x71
	.value	0x5c8
	.uleb128 0x1
	.long	.LASF291
	.byte	0xc
	.value	0x2d9
	.byte	0xa
	.long	0x71
	.value	0x5cc
	.uleb128 0x1
	.long	.LASF292
	.byte	0xc
	.value	0x2da
	.byte	0xa
	.long	0x71
	.value	0x5d0
	.uleb128 0x1
	.long	.LASF293
	.byte	0xc
	.value	0x2db
	.byte	0xa
	.long	0x71
	.value	0x5d4
	.uleb128 0x1
	.long	.LASF294
	.byte	0xc
	.value	0x2dc
	.byte	0xa
	.long	0x121
	.value	0x5d8
	.uleb128 0x1
	.long	.LASF295
	.byte	0xc
	.value	0x2dd
	.byte	0xa
	.long	0x121
	.value	0x5d9
	.uleb128 0x1
	.long	.LASF296
	.byte	0xc
	.value	0x2de
	.byte	0x13
	.long	0x1487
	.value	0x5da
	.uleb128 0x1
	.long	.LASF297
	.byte	0xc
	.value	0x2df
	.byte	0x13
	.long	0x1487
	.value	0x5fb
	.uleb128 0x1
	.long	.LASF298
	.byte	0xc
	.value	0x2e0
	.byte	0x13
	.long	0x1487
	.value	0x61c
	.uleb128 0x1
	.long	.LASF299
	.byte	0xc
	.value	0x2e1
	.byte	0x9
	.long	0x71
	.value	0x640
	.uleb128 0x1
	.long	.LASF300
	.byte	0xc
	.value	0x2e5
	.byte	0x13
	.long	0x1487
	.value	0x644
	.uleb128 0x1
	.long	.LASF301
	.byte	0xc
	.value	0x2e6
	.byte	0xa
	.long	0x71
	.value	0x668
	.uleb128 0x1
	.long	.LASF302
	.byte	0xc
	.value	0x2e8
	.byte	0xa
	.long	0x121
	.value	0x66c
	.uleb128 0x1
	.long	.LASF303
	.byte	0xc
	.value	0x2e9
	.byte	0xa
	.long	0x121
	.value	0x66d
	.uleb128 0x1
	.long	.LASF304
	.byte	0xc
	.value	0x2eb
	.byte	0x9
	.long	0x71
	.value	0x670
	.uleb128 0x1
	.long	.LASF305
	.byte	0xc
	.value	0x2f2
	.byte	0xa
	.long	0x121
	.value	0x674
	.uleb128 0x1
	.long	.LASF306
	.byte	0xc
	.value	0x2f3
	.byte	0xa
	.long	0x71
	.value	0x678
	.uleb128 0x1
	.long	.LASF307
	.byte	0xc
	.value	0x2f4
	.byte	0x9
	.long	0x71
	.value	0x67c
	.uleb128 0x1
	.long	.LASF308
	.byte	0xc
	.value	0x2f7
	.byte	0x9
	.long	0x71
	.value	0x680
	.uleb128 0x1
	.long	.LASF309
	.byte	0xc
	.value	0x305
	.byte	0x9
	.long	0x71
	.value	0x684
	.uleb128 0x1
	.long	.LASF310
	.byte	0xc
	.value	0x32d
	.byte	0xa
	.long	0x121
	.value	0x688
	.uleb128 0x1
	.long	.LASF311
	.byte	0xc
	.value	0x330
	.byte	0xa
	.long	0x121
	.value	0x689
	.uleb128 0x1
	.long	.LASF312
	.byte	0xc
	.value	0x336
	.byte	0xa
	.long	0x121
	.value	0x68a
	.uleb128 0x1
	.long	.LASF313
	.byte	0xc
	.value	0x33f
	.byte	0xa
	.long	0x121
	.value	0x68b
	.uleb128 0x1
	.long	.LASF314
	.byte	0xc
	.value	0x340
	.byte	0xa
	.long	0x121
	.value	0x68c
	.uleb128 0x1
	.long	.LASF315
	.byte	0xc
	.value	0x353
	.byte	0x13
	.long	0x14b7
	.value	0x68d
	.uleb128 0x1
	.long	.LASF104
	.byte	0xc
	.value	0x358
	.byte	0x20
	.long	0x609
	.value	0x6d0
	.uleb128 0x1
	.long	.LASF316
	.byte	0xc
	.value	0x35a
	.byte	0x9
	.long	0x71
	.value	0x6e8
	.uleb128 0x1
	.long	.LASF317
	.byte	0xc
	.value	0x35b
	.byte	0xa
	.long	0x121
	.value	0x6ec
	.uleb128 0x1
	.long	.LASF318
	.byte	0xc
	.value	0x35c
	.byte	0xa
	.long	0x121
	.value	0x6ed
	.uleb128 0x1
	.long	.LASF319
	.byte	0xc
	.value	0x35e
	.byte	0xa
	.long	0x121
	.value	0x6ee
	.uleb128 0x1
	.long	.LASF320
	.byte	0xc
	.value	0x362
	.byte	0xa
	.long	0x121
	.value	0x6ef
	.uleb128 0x1
	.long	.LASF321
	.byte	0xc
	.value	0x368
	.byte	0x9
	.long	0x71
	.value	0x6f0
	.uleb128 0x1
	.long	.LASF322
	.byte	0xc
	.value	0x369
	.byte	0x9
	.long	0x71
	.value	0x6f4
	.uleb128 0x1
	.long	.LASF323
	.byte	0xc
	.value	0x36d
	.byte	0x9
	.long	0x71
	.value	0x6f8
	.uleb128 0x1
	.long	.LASF324
	.byte	0xc
	.value	0x38b
	.byte	0xa
	.long	0x121
	.value	0x6fc
	.uleb128 0x1
	.long	.LASF325
	.byte	0xc
	.value	0x38e
	.byte	0xa
	.long	0x1409
	.value	0x6fd
	.uleb128 0x1
	.long	.LASF326
	.byte	0xc
	.value	0x390
	.byte	0xa
	.long	0x121
	.value	0x74e
	.uleb128 0x1
	.long	.LASF327
	.byte	0xc
	.value	0x392
	.byte	0xa
	.long	0x121
	.value	0x74f
	.uleb128 0x1
	.long	.LASF328
	.byte	0xc
	.value	0x396
	.byte	0x9
	.long	0x71
	.value	0x750
	.uleb128 0x1
	.long	.LASF329
	.byte	0xc
	.value	0x398
	.byte	0x9
	.long	0x71
	.value	0x754
	.uleb128 0x1
	.long	.LASF330
	.byte	0xc
	.value	0x39b
	.byte	0x9
	.long	0x71
	.value	0x758
	.uleb128 0x1
	.long	.LASF331
	.byte	0xc
	.value	0x39c
	.byte	0x9
	.long	0x71
	.value	0x75c
	.uleb128 0x1
	.long	.LASF332
	.byte	0xc
	.value	0x39f
	.byte	0x9
	.long	0x71
	.value	0x760
	.uleb128 0x1
	.long	.LASF333
	.byte	0xc
	.value	0x3a0
	.byte	0x9
	.long	0x71
	.value	0x764
	.uleb128 0x1
	.long	.LASF334
	.byte	0xc
	.value	0x3a1
	.byte	0x9
	.long	0x71
	.value	0x768
	.uleb128 0x1
	.long	.LASF335
	.byte	0xc
	.value	0x3a2
	.byte	0x9
	.long	0x71
	.value	0x76c
	.uleb128 0x1
	.long	.LASF336
	.byte	0xc
	.value	0x3a3
	.byte	0xa
	.long	0x121
	.value	0x770
	.uleb128 0x1
	.long	.LASF337
	.byte	0xc
	.value	0x3a4
	.byte	0x9
	.long	0x71
	.value	0x774
	.uleb128 0x13
	.string	"pbe"
	.byte	0xc
	.value	0x3a6
	.byte	0x9
	.long	0x71
	.value	0x778
	.uleb128 0x1
	.long	.LASF338
	.byte	0xc
	.value	0x3a7
	.byte	0x9
	.long	0x71
	.value	0x77c
	.uleb128 0x1
	.long	.LASF339
	.byte	0xc
	.value	0x3a9
	.byte	0x9
	.long	0x71
	.value	0x780
	.uleb128 0x1
	.long	.LASF340
	.byte	0xc
	.value	0x3b5
	.byte	0xa
	.long	0x121
	.value	0x784
	.uleb128 0x1
	.long	.LASF341
	.byte	0xc
	.value	0x3ba
	.byte	0xa
	.long	0x121
	.value	0x785
	.byte	0
	.uleb128 0xb
	.long	0x698
	.long	0x1409
	.uleb128 0xd
	.long	0x44
	.byte	0x9
	.byte	0
	.uleb128 0xb
	.long	0xaf
	.long	0x1419
	.uleb128 0xd
	.long	0x44
	.byte	0x50
	.byte	0
	.uleb128 0xb
	.long	0x1429
	.long	0x1429
	.uleb128 0xd
	.long	0x44
	.byte	0x3
	.byte	0
	.uleb128 0x4
	.long	0x1482
	.uleb128 0x9
	.long	.LASF342
	.byte	0x30
	.byte	0xd
	.byte	0xa9
	.long	0x1482
	.uleb128 0x2
	.long	.LASF343
	.byte	0xd
	.byte	0xaa
	.byte	0x1a
	.long	0xd3
	.byte	0
	.uleb128 0x2
	.long	.LASF344
	.byte	0xd
	.byte	0xab
	.byte	0x1a
	.long	0xa8
	.byte	0x8
	.uleb128 0x2
	.long	.LASF345
	.byte	0xd
	.byte	0xac
	.byte	0x1a
	.long	0x71
	.byte	0x10
	.uleb128 0x2
	.long	.LASF346
	.byte	0xd
	.byte	0xad
	.byte	0x1a
	.long	0x14d3
	.byte	0x18
	.uleb128 0x2
	.long	.LASF347
	.byte	0xd
	.byte	0xae
	.byte	0x1a
	.long	0x111
	.byte	0x20
	.uleb128 0x16
	.long	0x17fa
	.byte	0x28
	.byte	0
	.uleb128 0xc
	.long	0x142e
	.uleb128 0xb
	.long	0x2a
	.long	0x1497
	.uleb128 0xd
	.long	0x44
	.byte	0x20
	.byte	0
	.uleb128 0xb
	.long	0x2a
	.long	0x14a7
	.uleb128 0xd
	.long	0x44
	.byte	0x50
	.byte	0
	.uleb128 0xb
	.long	0x2a
	.long	0x14b7
	.uleb128 0xd
	.long	0x44
	.byte	0xa0
	.byte	0
	.uleb128 0xb
	.long	0x2a
	.long	0x14c7
	.uleb128 0xd
	.long	0x44
	.byte	0x3f
	.byte	0
	.uleb128 0xa
	.long	.LASF348
	.byte	0xd
	.byte	0x1d
	.byte	0xf
	.long	0x5e5
	.uleb128 0x22
	.long	.LASF393
	.byte	0x8
	.byte	0xd
	.byte	0x1f
	.byte	0x7
	.long	0x152e
	.uleb128 0x6
	.long	.LASF349
	.byte	0x20
	.byte	0x9
	.long	0x71
	.uleb128 0x6
	.long	.LASF350
	.byte	0x21
	.byte	0x12
	.long	0x3d
	.uleb128 0x6
	.long	.LASF351
	.byte	0x22
	.byte	0xa
	.long	0x121
	.uleb128 0x6
	.long	.LASF352
	.byte	0x23
	.byte	0xb
	.long	0xaa
	.uleb128 0x6
	.long	.LASF353
	.byte	0x24
	.byte	0x14
	.long	0x15e
	.uleb128 0x6
	.long	.LASF354
	.byte	0x25
	.byte	0x12
	.long	0x14c7
	.uleb128 0x6
	.long	.LASF355
	.byte	0x26
	.byte	0xb
	.long	0xa8
	.byte	0
	.uleb128 0x9
	.long	.LASF356
	.byte	0x4
	.byte	0xd
	.byte	0x31
	.long	0x1548
	.uleb128 0x2
	.long	.LASF344
	.byte	0xd
	.byte	0x32
	.byte	0x9
	.long	0x71
	.byte	0
	.byte	0
	.uleb128 0xc
	.long	0x152e
	.uleb128 0x9
	.long	.LASF357
	.byte	0x18
	.byte	0xd
	.byte	0x37
	.long	0x158e
	.uleb128 0x2
	.long	.LASF358
	.byte	0xd
	.byte	0x38
	.byte	0xc
	.long	0x159e
	.byte	0
	.uleb128 0x2
	.long	.LASF359
	.byte	0xd
	.byte	0x39
	.byte	0x9
	.long	0x71
	.byte	0x8
	.uleb128 0x2
	.long	.LASF360
	.byte	0xd
	.byte	0x3a
	.byte	0x9
	.long	0x71
	.byte	0xc
	.uleb128 0x2
	.long	.LASF361
	.byte	0xd
	.byte	0x3b
	.byte	0x13
	.long	0x111
	.byte	0x10
	.byte	0
	.uleb128 0xc
	.long	0x154d
	.uleb128 0x11
	.long	0x159e
	.uleb128 0x7
	.long	0x121
	.byte	0
	.uleb128 0x4
	.long	0x1593
	.uleb128 0x9
	.long	.LASF362
	.byte	0x18
	.byte	0xd
	.byte	0x42
	.long	0x15d7
	.uleb128 0x2
	.long	.LASF363
	.byte	0xd
	.byte	0x43
	.byte	0x11
	.long	0x111
	.byte	0
	.uleb128 0x2
	.long	.LASF364
	.byte	0xd
	.byte	0x44
	.byte	0x11
	.long	0x111
	.byte	0x8
	.uleb128 0x2
	.long	.LASF365
	.byte	0xd
	.byte	0x45
	.byte	0x9
	.long	0x71
	.byte	0x10
	.byte	0
	.uleb128 0xc
	.long	0x15a3
	.uleb128 0x9
	.long	.LASF366
	.byte	0x28
	.byte	0xd
	.byte	0x49
	.long	0x1644
	.uleb128 0x2
	.long	.LASF358
	.byte	0xd
	.byte	0x4a
	.byte	0xc
	.long	0x12f
	.byte	0
	.uleb128 0x2
	.long	.LASF367
	.byte	0xd
	.byte	0x4b
	.byte	0xd
	.long	0xbb
	.byte	0x8
	.uleb128 0x2
	.long	.LASF368
	.byte	0xd
	.byte	0x4c
	.byte	0xd
	.long	0xbb
	.byte	0xa
	.uleb128 0x10
	.string	"min"
	.byte	0xd
	.byte	0x4d
	.byte	0x9
	.long	0x71
	.byte	0xc
	.uleb128 0x10
	.string	"max"
	.byte	0xd
	.byte	0x4e
	.byte	0x9
	.long	0x71
	.byte	0x10
	.uleb128 0x2
	.long	.LASF369
	.byte	0xd
	.byte	0x50
	.byte	0x13
	.long	0x1667
	.byte	0x18
	.uleb128 0x2
	.long	.LASF370
	.byte	0xd
	.byte	0x51
	.byte	0xf
	.long	0x1680
	.byte	0x20
	.byte	0
	.uleb128 0xc
	.long	0x15dc
	.uleb128 0x12
	.long	0x111
	.long	0x1667
	.uleb128 0x7
	.long	0xaa
	.uleb128 0x7
	.long	0xf7
	.uleb128 0x7
	.long	0x71
	.uleb128 0x7
	.long	0x111
	.byte	0
	.uleb128 0x4
	.long	0x1649
	.uleb128 0x12
	.long	0xc7
	.long	0x1680
	.uleb128 0x7
	.long	0x71
	.uleb128 0x7
	.long	0x71
	.byte	0
	.uleb128 0x4
	.long	0x166c
	.uleb128 0x17
	.byte	0x5b
	.long	0x16a2
	.uleb128 0x6
	.long	.LASF371
	.byte	0x5c
	.byte	0x1f
	.long	0x16a2
	.uleb128 0x6
	.long	.LASF372
	.byte	0x5d
	.byte	0x1e
	.long	0x116
	.byte	0
	.uleb128 0x4
	.long	0x5ea
	.uleb128 0x9
	.long	.LASF373
	.byte	0x20
	.byte	0xd
	.byte	0x55
	.long	0x16e1
	.uleb128 0x2
	.long	.LASF358
	.byte	0xd
	.byte	0x56
	.byte	0xc
	.long	0x12f
	.byte	0
	.uleb128 0x2
	.long	.LASF374
	.byte	0xd
	.byte	0x57
	.byte	0x9
	.long	0x71
	.byte	0x8
	.uleb128 0x2
	.long	.LASF361
	.byte	0xd
	.byte	0x58
	.byte	0x11
	.long	0x111
	.byte	0x10
	.uleb128 0x16
	.long	0x1685
	.byte	0x18
	.byte	0
	.uleb128 0xc
	.long	0x16a7
	.uleb128 0x9
	.long	.LASF375
	.byte	0x30
	.byte	0xd
	.byte	0x68
	.long	0x174e
	.uleb128 0x2
	.long	.LASF358
	.byte	0xd
	.byte	0x69
	.byte	0xc
	.long	0x12f
	.byte	0
	.uleb128 0x2
	.long	.LASF369
	.byte	0xd
	.byte	0x6a
	.byte	0x13
	.long	0x1667
	.byte	0x8
	.uleb128 0x2
	.long	.LASF370
	.byte	0xd
	.byte	0x6b
	.byte	0xf
	.long	0x1680
	.byte	0x10
	.uleb128 0x2
	.long	.LASF367
	.byte	0xd
	.byte	0x6c
	.byte	0x9
	.long	0x71
	.byte	0x18
	.uleb128 0x2
	.long	.LASF374
	.byte	0xd
	.byte	0x6d
	.byte	0x9
	.long	0x71
	.byte	0x1c
	.uleb128 0x2
	.long	.LASF361
	.byte	0xd
	.byte	0x6e
	.byte	0x11
	.long	0x111
	.byte	0x20
	.uleb128 0x2
	.long	.LASF376
	.byte	0xd
	.byte	0x71
	.byte	0x11
	.long	0x116
	.byte	0x28
	.byte	0
	.uleb128 0xc
	.long	0x16e6
	.uleb128 0x9
	.long	.LASF377
	.byte	0x20
	.byte	0xd
	.byte	0x7b
	.long	0x1794
	.uleb128 0x2
	.long	.LASF378
	.byte	0xd
	.byte	0x80
	.byte	0xc
	.long	0x17a9
	.byte	0
	.uleb128 0x2
	.long	.LASF379
	.byte	0xd
	.byte	0x86
	.byte	0xd
	.long	0x17c7
	.byte	0x8
	.uleb128 0x2
	.long	.LASF380
	.byte	0xd
	.byte	0x8c
	.byte	0xc
	.long	0x17e0
	.byte	0x10
	.uleb128 0x2
	.long	.LASF381
	.byte	0xd
	.byte	0x91
	.byte	0xc
	.long	0x17f5
	.byte	0x18
	.byte	0
	.uleb128 0xc
	.long	0x1753
	.uleb128 0x11
	.long	0x17a9
	.uleb128 0x7
	.long	0xa8
	.uleb128 0x7
	.long	0xaa
	.byte	0
	.uleb128 0x4
	.long	0x1799
	.uleb128 0x12
	.long	0xaa
	.long	0x17c7
	.uleb128 0x7
	.long	0xa8
	.uleb128 0x7
	.long	0xaa
	.uleb128 0x7
	.long	0x71
	.byte	0
	.uleb128 0x4
	.long	0x17ae
	.uleb128 0x12
	.long	0x121
	.long	0x17e0
	.uleb128 0x7
	.long	0xa8
	.uleb128 0x7
	.long	0xa8
	.byte	0
	.uleb128 0x4
	.long	0x17cc
	.uleb128 0x11
	.long	0x17f5
	.uleb128 0x7
	.long	0xa8
	.uleb128 0x7
	.long	0xa8
	.byte	0
	.uleb128 0x4
	.long	0x17e5
	.uleb128 0x17
	.byte	0xb0
	.long	0x1864
	.uleb128 0x6
	.long	.LASF382
	.byte	0xb1
	.byte	0x15
	.long	0x11b
	.uleb128 0x6
	.long	.LASF356
	.byte	0xb2
	.byte	0x25
	.long	0x1864
	.uleb128 0x6
	.long	.LASF357
	.byte	0xb3
	.byte	0x25
	.long	0x1869
	.uleb128 0x6
	.long	.LASF362
	.byte	0xb4
	.byte	0x28
	.long	0x186e
	.uleb128 0x6
	.long	.LASF366
	.byte	0xb5
	.byte	0x23
	.long	0x1873
	.uleb128 0x6
	.long	.LASF373
	.byte	0xb6
	.byte	0x26
	.long	0x1878
	.uleb128 0x6
	.long	.LASF375
	.byte	0xb7
	.byte	0x25
	.long	0x187d
	.uleb128 0x6
	.long	.LASF377
	.byte	0xb8
	.byte	0x26
	.long	0x1882
	.uleb128 0x6
	.long	.LASF361
	.byte	0xb9
	.byte	0x17
	.long	0x111
	.byte	0
	.uleb128 0x4
	.long	0x1548
	.uleb128 0x4
	.long	0x158e
	.uleb128 0x4
	.long	0x15d7
	.uleb128 0x4
	.long	0x1644
	.uleb128 0x4
	.long	0x16e1
	.uleb128 0x4
	.long	0x174e
	.uleb128 0x4
	.long	0x1794
	.uleb128 0x23
	.long	.LASF394
	.byte	0x1
	.byte	0x19
	.byte	0x5
	.long	0x71
	.quad	.LFB96
	.quad	.LFE96-.LFB96
	.uleb128 0x1
	.byte	0x9c
	.uleb128 0x18
	.long	.LASF383
	.byte	0x20
	.long	0x6ca
	.uleb128 0x18
	.long	.LASF384
	.byte	0x2f
	.long	0x79f
	.uleb128 0xf
	.long	.LASF385
	.value	0x106
	.byte	0x1c
	.long	0x664
	.uleb128 0xf
	.long	.LASF386
	.value	0x10a
	.byte	0x18
	.long	0x698
	.uleb128 0xf
	.long	.LASF387
	.value	0x10e
	.byte	0x1c
	.long	0x609
	.uleb128 0xf
	.long	.LASF388
	.value	0x115
	.byte	0x17
	.long	0x5ef
	.uleb128 0xf
	.long	.LASF389
	.value	0x117
	.byte	0x11
	.long	0x163
	.uleb128 0xf
	.long	.LASF390
	.value	0x155
	.byte	0x15
	.long	0x510
	.uleb128 0xf
	.long	.LASF391
	.value	0x159
	.byte	0x1a
	.long	0x56b
	.byte	0
	.byte	0
	.section	.debug_abbrev,"",@progbits
.Ldebug_abbrev0:
	.uleb128 0x1
	.uleb128 0xd
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0x5
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x38
	.uleb128 0x5
	.byte	0
	.byte	0
	.uleb128 0x2
	.uleb128 0xd
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x38
	.uleb128 0xb
	.byte	0
	.byte	0
	.uleb128 0x3
	.uleb128 0xd
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0x21
	.sleb128 12
	.uleb128 0x3b
	.uleb128 0x5
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x38
	.uleb128 0xb
	.byte	0
	.byte	0
	.uleb128 0x4
	.uleb128 0xf
	.byte	0
	.uleb128 0xb
	.uleb128 0x21
	.sleb128 8
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x5
	.uleb128 0xd
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0x21
	.sleb128 7
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x38
	.uleb128 0x5
	.byte	0
	.byte	0
	.uleb128 0x6
	.uleb128 0xd
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0x21
	.sleb128 13
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x7
	.uleb128 0x5
	.byte	0
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x8
	.uleb128 0x24
	.byte	0
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x3e
	.uleb128 0xb
	.uleb128 0x3
	.uleb128 0xe
	.byte	0
	.byte	0
	.uleb128 0x9
	.uleb128 0x13
	.byte	0x1
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x39
	.uleb128 0x21
	.sleb128 8
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0xa
	.uleb128 0x16
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0xb
	.uleb128 0x1
	.byte	0x1
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0xc
	.uleb128 0x26
	.byte	0
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0xd
	.uleb128 0x21
	.byte	0
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x2f
	.uleb128 0xb
	.byte	0
	.byte	0
	.uleb128 0xe
	.uleb128 0x28
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x1c
	.uleb128 0xb
	.byte	0
	.byte	0
	.uleb128 0xf
	.uleb128 0x34
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0x21
	.sleb128 1
	.uleb128 0x3b
	.uleb128 0x5
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x10
	.uleb128 0xd
	.byte	0
	.uleb128 0x3
	.uleb128 0x8
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x38
	.uleb128 0xb
	.byte	0
	.byte	0
	.uleb128 0x11
	.uleb128 0x15
	.byte	0x1
	.uleb128 0x27
	.uleb128 0x19
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x12
	.uleb128 0x15
	.byte	0x1
	.uleb128 0x27
	.uleb128 0x19
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x13
	.uleb128 0xd
	.byte	0
	.uleb128 0x3
	.uleb128 0x8
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0x5
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x38
	.uleb128 0x5
	.byte	0
	.byte	0
	.uleb128 0x14
	.uleb128 0x21
	.byte	0
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x2f
	.uleb128 0x5
	.byte	0
	.byte	0
	.uleb128 0x15
	.uleb128 0x4
	.byte	0x1
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3e
	.uleb128 0x21
	.sleb128 7
	.uleb128 0xb
	.uleb128 0x21
	.sleb128 4
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x3a
	.uleb128 0x21
	.sleb128 7
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x39
	.uleb128 0x21
	.sleb128 6
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x16
	.uleb128 0xd
	.byte	0
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x38
	.uleb128 0xb
	.byte	0
	.byte	0
	.uleb128 0x17
	.uleb128 0x17
	.byte	0x1
	.uleb128 0xb
	.uleb128 0x21
	.sleb128 8
	.uleb128 0x3a
	.uleb128 0x21
	.sleb128 13
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x39
	.uleb128 0x21
	.sleb128 5
	.uleb128 0x89
	.uleb128 0x19
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x18
	.uleb128 0x34
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0x21
	.sleb128 1
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x39
	.uleb128 0x21
	.sleb128 22
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x19
	.uleb128 0x11
	.byte	0x1
	.uleb128 0x25
	.uleb128 0xe
	.uleb128 0x13
	.uleb128 0xb
	.uleb128 0x3
	.uleb128 0x1f
	.uleb128 0x1b
	.uleb128 0x1f
	.uleb128 0x55
	.uleb128 0x17
	.uleb128 0x11
	.uleb128 0x1
	.uleb128 0x10
	.uleb128 0x17
	.byte	0
	.byte	0
	.uleb128 0x1a
	.uleb128 0x24
	.byte	0
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x3e
	.uleb128 0xb
	.uleb128 0x3
	.uleb128 0x8
	.byte	0
	.byte	0
	.uleb128 0x1b
	.uleb128 0xf
	.byte	0
	.uleb128 0xb
	.uleb128 0xb
	.byte	0
	.byte	0
	.uleb128 0x1c
	.uleb128 0x26
	.byte	0
	.byte	0
	.byte	0
	.uleb128 0x1d
	.uleb128 0x13
	.byte	0x1
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0xb
	.uleb128 0x5
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x1e
	.uleb128 0x13
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3c
	.uleb128 0x19
	.byte	0
	.byte	0
	.uleb128 0x1f
	.uleb128 0x15
	.byte	0
	.uleb128 0x27
	.uleb128 0x19
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x20
	.uleb128 0x13
	.byte	0x1
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0x5
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x21
	.uleb128 0x13
	.byte	0x1
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0xb
	.uleb128 0x5
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0x5
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x22
	.uleb128 0x17
	.byte	0x1
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x23
	.uleb128 0x2e
	.byte	0x1
	.uleb128 0x3f
	.uleb128 0x19
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x27
	.uleb128 0x19
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x11
	.uleb128 0x1
	.uleb128 0x12
	.uleb128 0x7
	.uleb128 0x40
	.uleb128 0x18
	.uleb128 0x7a
	.uleb128 0x19
	.byte	0
	.byte	0
	.byte	0
	.section	.debug_aranges,"",@progbits
	.long	0x2c
	.value	0x2
	.long	.Ldebug_info0
	.byte	0x8
	.byte	0
	.value	0
	.value	0
	.quad	.LFB96
	.quad	.LFE96-.LFB96
	.quad	0
	.quad	0
	.section	.debug_rnglists,"",@progbits
.Ldebug_ranges0:
	.long	.Ldebug_ranges3-.Ldebug_ranges2
.Ldebug_ranges2:
	.value	0x5
	.byte	0x8
	.byte	0
	.long	0
.LLRL0:
	.byte	0x7
	.quad	.LFB96
	.uleb128 .LFE96-.LFB96
	.byte	0
.Ldebug_ranges3:
	.section	.debug_line,"",@progbits
.Ldebug_line0:
	.section	.debug_str,"MS",@progbits,1
.LASF266:
	.string	"warnon_erase_dynplaylist"
.LASF41:
	.string	"layer"
.LASF142:
	.string	"crossfade_fade_out_delay"
.LASF168:
	.string	"rec_editable"
.LASF43:
	.string	"id3version"
.LASF253:
	.string	"colors_file"
.LASF311:
	.string	"lineout_active"
.LASF123:
	.string	"resume_pitch"
.LASF345:
	.string	"lang_id"
.LASF145:
	.string	"crossfade_fade_out_mixmode"
.LASF291:
	.string	"cursor_style"
.LASF57:
	.string	"bytesperframe"
.LASF61:
	.string	"id3v1buf"
.LASF69:
	.string	"playcount"
.LASF351:
	.string	"bool_"
.LASF163:
	.string	"rec_channels"
.LASF261:
	.string	"next_folder"
.LASF309:
	.string	"brightness"
.LASF331:
	.string	"volume_adjust_norm_steps"
.LASF174:
	.string	"rec_directory"
.LASF201:
	.string	"car_adapter_mode_delay"
.LASF148:
	.string	"crossfeed_cross_gain"
.LASF183:
	.string	"rec_trigger_mode"
.LASF392:
	.string	"GNU C99 16.1.1 20260430 -mtune=generic -march=x86-64 -g -Os -std=gnu99 -funit-at-a-time -fno-delete-null-pointer-checks -fno-strict-overflow -fno-common -fno-builtin"
.LASF380:
	.string	"is_changed"
.LASF36:
	.string	"comment"
.LASF165:
	.string	"rec_left_gain"
.LASF107:
	.string	"ratio"
.LASF90:
	.string	"AA_FLAG_ID3_UNSYNC"
.LASF225:
	.string	"list_separator_height"
.LASF186:
	.string	"unplug_mode"
.LASF332:
	.string	"surround_enabled"
.LASF9:
	.string	"__int32_t"
.LASF159:
	.string	"rec_format"
.LASF230:
	.string	"list_order"
.LASF77:
	.string	"album_peak"
.LASF78:
	.string	"has_embedded_albumart"
.LASF182:
	.string	"rec_stop_gap"
.LASF325:
	.string	"start_directory"
.LASF215:
	.string	"playlist_catalog_dir"
.LASF268:
	.string	"show_shuffled_adding_options"
.LASF147:
	.string	"crossfeed_direct_gain"
.LASF83:
	.string	"mb_track_id"
.LASF181:
	.string	"rec_stop_postrec"
.LASF263:
	.string	"recursive_dir_insert"
.LASF25:
	.string	"__int128"
.LASF7:
	.string	"short int"
.LASF144:
	.string	"crossfade_fade_out_duration"
.LASF46:
	.string	"frequency"
.LASF166:
	.string	"rec_right_gain"
.LASF209:
	.string	"peak_meter_dbfs"
.LASF318:
	.string	"keypress_restarts_sleeptimer"
.LASF296:
	.string	"icon_file"
.LASF329:
	.string	"volume_limit"
.LASF158:
	.string	"timestretch_enabled"
.LASF284:
	.string	"sort_dir"
.LASF278:
	.string	"talk_file"
.LASF169:
	.string	"rec_timesplit"
.LASF290:
	.string	"usb_charging"
.LASF254:
	.string	"dynamic_colors"
.LASF382:
	.string	"RESERVED"
.LASF15:
	.string	"int16_t"
.LASF242:
	.string	"autoresume_automatic"
.LASF106:
	.string	"makeup_gain"
.LASF229:
	.string	"list_wraparound"
.LASF171:
	.string	"rec_split_type"
.LASF126:
	.string	"topruntime"
.LASF256:
	.string	"ipone_lock_wallpaper"
.LASF124:
	.string	"resume_speed"
.LASF28:
	.string	"title"
.LASF344:
	.string	"setting"
.LASF17:
	.string	"uint32_t"
.LASF250:
	.string	"lss_color"
.LASF108:
	.string	"knee"
.LASF170:
	.string	"rec_sizesplit"
.LASF26:
	.string	"__int128 unsigned"
.LASF118:
	.string	"volume"
.LASF177:
	.string	"rec_start_thres_linear"
.LASF192:
	.string	"storage_mode"
.LASF306:
	.string	"bl_selective_actions_mask"
.LASF45:
	.string	"bitrate"
.LASF216:
	.string	"skip_length"
.LASF157:
	.string	"dithering_enabled"
.LASF374:
	.string	"count"
.LASF63:
	.string	"index"
.LASF21:
	.string	"long long unsigned int"
.LASF315:
	.string	"ui_vp_config"
.LASF353:
	.string	"ucharptr"
.LASF134:
	.string	"bass"
.LASF132:
	.string	"user_settings"
.LASF365:
	.string	"max_len"
.LASF100:
	.string	"CHAR_ENC_UTF_16_LE"
.LASF173:
	.string	"rec_prerecord_time"
.LASF32:
	.string	"disc_string"
.LASF129:
	.string	"last_volume_change"
.LASF119:
	.string	"resume_index"
.LASF66:
	.string	"tagcache_idx"
.LASF140:
	.string	"crossfade"
.LASF161:
	.string	"rec_source"
.LASF50:
	.string	"sim_filesize"
.LASF189:
	.string	"timeformat"
.LASF103:
	.string	"mp3_enc_config"
.LASF322:
	.string	"hotkey_tree"
.LASF39:
	.string	"discnum"
.LASF283:
	.string	"sort_case"
.LASF366:
	.string	"int_setting"
.LASF361:
	.string	"cfg_vals"
.LASF255:
	.string	"ipone_charge_wallpaper"
.LASF92:
	.string	"mp3entry"
.LASF238:
	.string	"usemrb"
.LASF151:
	.string	"eq_enabled"
.LASF153:
	.string	"eq_band_settings"
.LASF219:
	.string	"volume_type"
.LASF112:
	.string	"noclip"
.LASF58:
	.string	"has_toc"
.LASF323:
	.string	"resume_rewind"
.LASF136:
	.string	"channel_config"
.LASF245:
	.string	"tagcache_scan_paths"
.LASF294:
	.string	"offset_out_of_view"
.LASF340:
	.string	"clear_settings_on_hold"
.LASF363:
	.string	"prefix"
.LASF195:
	.string	"default_codepage"
.LASF20:
	.string	"size_t"
.LASF33:
	.string	"track_string"
.LASF62:
	.string	"offset"
.LASF236:
	.string	"autocreatebookmark"
.LASF160:
	.string	"rec_mono_mode"
.LASF302:
	.string	"caption_backlight"
.LASF354:
	.string	"func"
.LASF277:
	.string	"talk_dir_clip"
.LASF23:
	.string	"_Bool"
.LASF125:
	.string	"runtime"
.LASF89:
	.string	"AA_TYPE_JPG"
.LASF133:
	.string	"balance"
.LASF337:
	.string	"surround_mix"
.LASF49:
	.string	"first_frame_offset"
.LASF53:
	.string	"lead_trim"
.LASF220:
	.string	"battery_display"
.LASF135:
	.string	"treble"
.LASF343:
	.string	"flags"
.LASF79:
	.string	"albumart"
.LASF59:
	.string	"needs_upsampling_correction"
.LASF27:
	.string	"path"
.LASF221:
	.string	"show_icons"
.LASF282:
	.string	"talk_mixer_amp"
.LASF379:
	.string	"write_to_cfg"
.LASF48:
	.string	"id3v1len"
.LASF330:
	.string	"volume_adjust_mode"
.LASF271:
	.string	"rewind_across_tracks"
.LASF308:
	.string	"lcd_sleep_after_backlight_off"
.LASF116:
	.string	"gain"
.LASF38:
	.string	"grouping"
.LASF377:
	.string	"custom_setting"
.LASF115:
	.string	"cutoff"
.LASF93:
	.string	"mp3_albumart"
.LASF303:
	.string	"bl_filter_first_keypress"
.LASF355:
	.string	"custom"
.LASF292:
	.string	"screen_scroll_step"
.LASF272:
	.string	"playlist_viewer_icons"
.LASF251:
	.string	"lse_color"
.LASF14:
	.string	"char"
.LASF152:
	.string	"eq_precut"
.LASF190:
	.string	"disk_spindown"
.LASF64:
	.string	"skip_resume_adjustments"
.LASF102:
	.string	"encoding"
.LASF81:
	.string	"embedded_cuesheet"
.LASF267:
	.string	"keep_current_track_on_replace_playlist"
.LASF247:
	.string	"backdrop_file"
.LASF391:
	.string	"section_10"
.LASF210:
	.string	"peak_meter_min"
.LASF12:
	.string	"__uint64_t"
.LASF4:
	.string	"unsigned int"
.LASF97:
	.string	"character_encoding"
.LASF293:
	.string	"show_path_in_browser"
.LASF227:
	.string	"browse_current"
.LASF199:
	.string	"party_mode"
.LASF288:
	.string	"poweroff"
.LASF264:
	.string	"fade_on_stop"
.LASF373:
	.string	"choice_setting"
.LASF241:
	.string	"autoresume_enable"
.LASF270:
	.string	"album_art"
.LASF191:
	.string	"buffer_margin"
.LASF138:
	.string	"bass_cutoff"
.LASF121:
	.string	"resume_elapsed"
.LASF179:
	.string	"rec_stop_thres_db"
.LASF128:
	.string	"viewer_icon_count"
.LASF326:
	.string	"root_menu_customized"
.LASF86:
	.string	"AA_TYPE_UNKNOWN"
.LASF349:
	.string	"int_"
.LASF312:
	.string	"prevent_skip"
.LASF194:
	.string	"show_filename_ext"
.LASF328:
	.string	"play_frequency"
.LASF68:
	.string	"score"
.LASF335:
	.string	"surround_fx2"
.LASF55:
	.string	"samples"
.LASF40:
	.string	"tracknum"
.LASF226:
	.string	"list_separator_color"
.LASF22:
	.string	"long long int"
.LASF98:
	.string	"CHAR_ENC_ISO_8859_1"
.LASF393:
	.string	"storage_type"
.LASF150:
	.string	"crossfeed_hf_cutoff"
.LASF141:
	.string	"crossfade_fade_in_delay"
.LASF185:
	.string	"pause_rewind"
.LASF383:
	.string	"section_1"
.LASF384:
	.string	"section_2"
.LASF385:
	.string	"section_3"
.LASF386:
	.string	"section_4"
.LASF387:
	.string	"section_5"
.LASF388:
	.string	"section_6"
.LASF389:
	.string	"section_8"
.LASF390:
	.string	"section_9"
.LASF375:
	.string	"table_setting"
.LASF44:
	.string	"codectype"
.LASF109:
	.string	"release_time"
.LASF8:
	.string	"__int16_t"
.LASF231:
	.string	"scroll_speed"
.LASF262:
	.string	"constrain_next_folder"
.LASF360:
	.string	"lang_no"
.LASF370:
	.string	"get_talk_id"
.LASF47:
	.string	"id3v2len"
.LASF82:
	.string	"cuesheet"
.LASF114:
	.string	"eq_band_setting"
.LASF224:
	.string	"scrollbar_width"
.LASF368:
	.string	"step"
.LASF54:
	.string	"tail_trim"
.LASF95:
	.string	"size"
.LASF249:
	.string	"fg_color"
.LASF378:
	.string	"load_from_cfg"
.LASF213:
	.string	"sbs_file"
.LASF67:
	.string	"rating"
.LASF287:
	.string	"interpret_numbers"
.LASF369:
	.string	"formatter"
.LASF74:
	.string	"track_gain"
.LASF172:
	.string	"rec_split_method"
.LASF376:
	.string	"values"
.LASF143:
	.string	"crossfade_fade_in_duration"
.LASF113:
	.string	"preamp"
.LASF101:
	.string	"CHAR_ENC_UTF_16_BE"
.LASF85:
	.string	"has_video"
.LASF88:
	.string	"AA_TYPE_PNG"
.LASF339:
	.string	"afr_enabled"
.LASF367:
	.string	"unit"
.LASF162:
	.string	"rec_frequency"
.LASF381:
	.string	"set_default"
.LASF243:
	.string	"autoresume_paths"
.LASF258:
	.string	"album_list_layout"
.LASF149:
	.string	"crossfeed_hf_attenuation"
.LASF60:
	.string	"id3v2buf"
.LASF372:
	.string	"talks"
.LASF31:
	.string	"genre_string"
.LASF342:
	.string	"settings_list"
.LASF35:
	.string	"composer"
.LASF200:
	.string	"car_adapter_mode"
.LASF214:
	.string	"lang_file"
.LASF137:
	.string	"stereo_width"
.LASF358:
	.string	"option_callback"
.LASF73:
	.string	"album_level"
.LASF313:
	.string	"pitch_mode_semitone"
.LASF198:
	.string	"single_mode"
.LASF193:
	.string	"dirfilter"
.LASF167:
	.string	"peak_meter_clipcounter"
.LASF76:
	.string	"track_peak"
.LASF273:
	.string	"playlist_viewer_indices"
.LASF364:
	.string	"suffix"
.LASF320:
	.string	"morse_input"
.LASF176:
	.string	"rec_start_thres_db"
.LASF117:
	.string	"system_status"
.LASF84:
	.string	"is_asf_stream"
.LASF324:
	.string	"keyclick_hardware"
.LASF131:
	.string	"resume_modified"
.LASF295:
	.string	"disable_mainmenu_scrolling"
.LASF276:
	.string	"talk_dir"
.LASF11:
	.string	"long int"
.LASF110:
	.string	"attack_time"
.LASF327:
	.string	"shortcuts_replaces_qs"
.LASF237:
	.string	"autoupdatebookmark"
.LASF51:
	.string	"length"
.LASF304:
	.string	"backlight_timeout_plugged"
.LASF286:
	.string	"sort_playlists"
.LASF281:
	.string	"talk_battery_level"
.LASF235:
	.string	"autoloadbookmark"
.LASF56:
	.string	"frame_count"
.LASF96:
	.string	"mp3_aa_type"
.LASF204:
	.string	"ff_rewind_min_step"
.LASF71:
	.string	"playtime"
.LASF347:
	.string	"cfg_name"
.LASF212:
	.string	"wps_file"
.LASF18:
	.string	"uint64_t"
.LASF285:
	.string	"sort_file"
.LASF122:
	.string	"resume_offset"
.LASF299:
	.string	"glyphs_to_cache"
.LASF298:
	.string	"font_file"
.LASF352:
	.string	"charptr"
.LASF175:
	.string	"cliplight"
.LASF239:
	.string	"tagcache_ram"
.LASF218:
	.string	"max_files_in_playlist"
.LASF275:
	.string	"talk_menu"
.LASF260:
	.string	"repeat_mode"
.LASF217:
	.string	"max_files_in_dir"
.LASF244:
	.string	"runtimedb"
.LASF29:
	.string	"artist"
.LASF252:
	.string	"lst_color"
.LASF188:
	.string	"qs_items"
.LASF178:
	.string	"rec_start_duration"
.LASF206:
	.string	"peak_meter_release"
.LASF333:
	.string	"surround_balance"
.LASF316:
	.string	"sleeptimer_duration"
.LASF305:
	.string	"bl_selective_actions"
.LASF280:
	.string	"talk_filetype"
.LASF30:
	.string	"album"
.LASF223:
	.string	"scrollbar"
.LASF5:
	.string	"long unsigned int"
.LASF301:
	.string	"backlight_timeout"
.LASF202:
	.string	"start_in_screen"
.LASF205:
	.string	"ff_rewind_accel"
.LASF274:
	.string	"playlist_viewer_track_display"
.LASF248:
	.string	"bg_color"
.LASF314:
	.string	"pitch_mode_timestretch"
.LASF42:
	.string	"year"
.LASF99:
	.string	"CHAR_ENC_UTF_8"
.LASF310:
	.string	"accessory_supply"
.LASF279:
	.string	"talk_file_clip"
.LASF120:
	.string	"resume_crc32"
.LASF232:
	.string	"bidir_limit"
.LASF334:
	.string	"surround_fx1"
.LASF94:
	.string	"type"
.LASF130:
	.string	"font_id"
.LASF2:
	.string	"unsigned char"
.LASF72:
	.string	"track_level"
.LASF10:
	.string	"__uint32_t"
.LASF104:
	.string	"compressor_settings"
.LASF180:
	.string	"rec_stop_thres_linear"
.LASF75:
	.string	"album_gain"
.LASF228:
	.string	"scroll_paginated"
.LASF184:
	.string	"rec_trigger_type"
.LASF197:
	.string	"play_selected"
.LASF356:
	.string	"sound_setting"
.LASF37:
	.string	"albumartist"
.LASF341:
	.string	"playback_log"
.LASF257:
	.string	"ipone_right_pane"
.LASF297:
	.string	"viewers_icon_file"
.LASF34:
	.string	"year_string"
.LASF346:
	.string	"default_val"
.LASF105:
	.string	"threshold"
.LASF348:
	.string	"_isfunc_type"
.LASF357:
	.string	"bool_setting"
.LASF222:
	.string	"statusbar"
.LASF111:
	.string	"replaygain_settings"
.LASF13:
	.string	"__off_t"
.LASF146:
	.string	"crossfeed"
.LASF154:
	.string	"beep"
.LASF127:
	.string	"last_screen"
.LASF317:
	.string	"sleeptimer_on_startup"
.LASF265:
	.string	"playlist_shuffle"
.LASF6:
	.string	"signed char"
.LASF19:
	.string	"off_t"
.LASF3:
	.string	"short unsigned int"
.LASF321:
	.string	"hotkey_wps"
.LASF156:
	.string	"keyclick_repeats"
.LASF319:
	.string	"show_shutdown_message"
.LASF394:
	.string	"main"
.LASF246:
	.string	"tagcache_db_path"
.LASF211:
	.string	"peak_meter_max"
.LASF269:
	.string	"show_queue_options"
.LASF240:
	.string	"tagcache_autoupdate"
.LASF338:
	.string	"pbe_precut"
.LASF139:
	.string	"treble_cutoff"
.LASF65:
	.string	"autoresumable"
.LASF52:
	.string	"elapsed"
.LASF80:
	.string	"has_embedded_cuesheet"
.LASF24:
	.string	"double"
.LASF259:
	.string	"browser_default"
.LASF91:
	.string	"AA_FLAG_VORBIS_BASE64"
.LASF307:
	.string	"backlight_on_button_hold"
.LASF233:
	.string	"scroll_delay"
.LASF16:
	.string	"int32_t"
.LASF203:
	.string	"wps_select_action"
.LASF300:
	.string	"kbd_file"
.LASF70:
	.string	"lastplayed"
.LASF371:
	.string	"desc"
.LASF289:
	.string	"battery_capacity"
.LASF350:
	.string	"uint_"
.LASF187:
	.string	"unplug_autoresume"
.LASF87:
	.string	"AA_TYPE_BMP"
.LASF336:
	.string	"surround_method2"
.LASF207:
	.string	"peak_meter_hold"
.LASF196:
	.string	"hold_lr_for_scroll_in_list"
.LASF208:
	.string	"peak_meter_clip_hold"
.LASF155:
	.string	"keyclick"
.LASF359:
	.string	"lang_yes"
.LASF362:
	.string	"filename_setting"
.LASF234:
	.string	"scroll_step"
.LASF164:
	.string	"rec_mic_gain"
	.section	.debug_line_str,"MS",@progbits,1
.LASF0:
	.string	"<stdin>"
.LASF1:
	.string	"/home/david/Documents/RockBox_Personal-master/build-sim-ipod6g"
	.ident	"GCC: (GNU) 16.1.1 20260430"
	.section	.note.GNU-stack,"",@progbits
