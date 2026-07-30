	.file	"<stdin>"
	.text
.Ltext0:
	.file 0 "/home/david/Documents/RockBox_Personal-master/build-sim-video-5g" "<stdin>"
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
	/* "struct system_status", "last_frequency" = $36, $4, "i", $0; */

# 0 "" 2
	.loc 1 43 5 view .LVU13
# 43 "<stdin>" 1
	/* "struct system_status", "last_screen" = $40, $1, "c", $0; */

# 0 "" 2
	.loc 1 44 5 view .LVU14
# 44 "<stdin>" 1
	/* "struct system_status", "viewer_icon_count" = $44, $4, "i", $0; */

# 0 "" 2
	.loc 1 45 5 view .LVU15
# 45 "<stdin>" 1
	/* "struct system_status", "last_volume_change" = $48, $4, "i", $0; */

# 0 "" 2
	.loc 1 46 5 view .LVU16
# 46 "<stdin>" 1
	/* "struct system_status", "font_id" = $52, $4, "i_typeisarray_", $1; */

# 0 "" 2
	.loc 1 47 5 view .LVU17
# 47 "<stdin>" 1
	/* "struct system_status", "resume_modified" = $56, $1, "b", $0; */

# 0 "" 2
	.loc 1 48 1 view .LVU18
	.loc 1 49 5 view .LVU19
# 49 "<stdin>" 1
	/* "struct user_settings", "balance" = $0, $4, "i", $0; */

# 0 "" 2
	.loc 1 50 5 view .LVU20
# 50 "<stdin>" 1
	/* "struct user_settings", "bass" = $4, $4, "i", $0; */

# 0 "" 2
	.loc 1 51 5 view .LVU21
# 51 "<stdin>" 1
	/* "struct user_settings", "treble" = $8, $4, "i", $0; */

# 0 "" 2
	.loc 1 52 5 view .LVU22
# 52 "<stdin>" 1
	/* "struct user_settings", "channel_config" = $12, $4, "i", $0; */

# 0 "" 2
	.loc 1 53 5 view .LVU23
# 53 "<stdin>" 1
	/* "struct user_settings", "stereo_width" = $16, $4, "i", $0; */

# 0 "" 2
	.loc 1 54 5 view .LVU24
# 54 "<stdin>" 1
	/* "struct user_settings", "bass_cutoff" = $20, $4, "i", $0; */

# 0 "" 2
	.loc 1 55 5 view .LVU25
# 55 "<stdin>" 1
	/* "struct user_settings", "treble_cutoff" = $24, $4, "i", $0; */

# 0 "" 2
	.loc 1 56 5 view .LVU26
# 56 "<stdin>" 1
	/* "struct user_settings", "crossfade" = $28, $4, "i", $0; */

# 0 "" 2
	.loc 1 57 5 view .LVU27
# 57 "<stdin>" 1
	/* "struct user_settings", "crossfade_fade_in_delay" = $32, $4, "i", $0; */

# 0 "" 2
	.loc 1 58 5 view .LVU28
# 58 "<stdin>" 1
	/* "struct user_settings", "crossfade_fade_out_delay" = $36, $4, "i", $0; */

# 0 "" 2
	.loc 1 59 5 view .LVU29
# 59 "<stdin>" 1
	/* "struct user_settings", "crossfade_fade_in_duration" = $40, $4, "i", $0; */

# 0 "" 2
	.loc 1 60 5 view .LVU30
# 60 "<stdin>" 1
	/* "struct user_settings", "crossfade_fade_out_duration" = $44, $4, "i", $0; */

# 0 "" 2
	.loc 1 61 5 view .LVU31
# 61 "<stdin>" 1
	/* "struct user_settings", "crossfade_fade_out_mixmode" = $48, $4, "i", $0; */

# 0 "" 2
	.loc 1 62 5 view .LVU32
# 62 "<stdin>" 1
	/* "struct user_settings", "replaygain_settings" = $52, $12, "s_replaygain_settings", $0; */

# 0 "" 2
	.loc 1 63 5 view .LVU33
# 63 "<stdin>" 1
	/* "struct user_settings", "crossfeed" = $64, $4, "i", $0; */

# 0 "" 2
	.loc 1 64 5 view .LVU34
# 64 "<stdin>" 1
	/* "struct user_settings", "crossfeed_direct_gain" = $68, $4, "u_i", $0; */

# 0 "" 2
	.loc 1 65 5 view .LVU35
# 65 "<stdin>" 1
	/* "struct user_settings", "crossfeed_cross_gain" = $72, $4, "u_i", $0; */

# 0 "" 2
	.loc 1 66 5 view .LVU36
# 66 "<stdin>" 1
	/* "struct user_settings", "crossfeed_hf_attenuation" = $76, $4, "u_i", $0; */

# 0 "" 2
	.loc 1 67 5 view .LVU37
# 67 "<stdin>" 1
	/* "struct user_settings", "crossfeed_hf_cutoff" = $80, $4, "u_i", $0; */

# 0 "" 2
	.loc 1 68 5 view .LVU38
# 68 "<stdin>" 1
	/* "struct user_settings", "eq_enabled" = $84, $1, "b", $0; */

# 0 "" 2
	.loc 1 69 5 view .LVU39
# 69 "<stdin>" 1
	/* "struct user_settings", "eq_precut" = $88, $4, "u_i", $0; */

# 0 "" 2
	.loc 1 70 5 view .LVU40
# 70 "<stdin>" 1
	/* "struct user_settings", "eq_band_settings" = $92, $120, "s_eq_band_setting_typeisarray_", $10; */

# 0 "" 2
	.loc 1 71 5 view .LVU41
# 71 "<stdin>" 1
	/* "struct user_settings", "beep" = $212, $4, "i", $0; */

# 0 "" 2
	.loc 1 72 5 view .LVU42
# 72 "<stdin>" 1
	/* "struct user_settings", "keyclick" = $216, $4, "i", $0; */

# 0 "" 2
	.loc 1 73 5 view .LVU43
# 73 "<stdin>" 1
	/* "struct user_settings", "keyclick_repeats" = $220, $4, "i", $0; */

# 0 "" 2
	.loc 1 74 5 view .LVU44
# 74 "<stdin>" 1
	/* "struct user_settings", "dithering_enabled" = $224, $1, "b", $0; */

# 0 "" 2
	.loc 1 75 5 view .LVU45
# 75 "<stdin>" 1
	/* "struct user_settings", "timestretch_enabled" = $225, $1, "b", $0; */

# 0 "" 2
	.loc 1 76 5 view .LVU46
# 76 "<stdin>" 1
	/* "struct user_settings", "rec_format" = $228, $4, "i", $0; */

# 0 "" 2
	.loc 1 77 5 view .LVU47
# 77 "<stdin>" 1
	/* "struct user_settings", "rec_mono_mode" = $232, $4, "i", $0; */

# 0 "" 2
	.loc 1 78 5 view .LVU48
# 78 "<stdin>" 1
	/* "struct user_settings", "mp3_enc_config" = $240, $8, "s_mp3_enc_config", $0; */

# 0 "" 2
	.loc 1 79 5 view .LVU49
# 79 "<stdin>" 1
	/* "struct user_settings", "rec_source" = $248, $4, "i", $0; */

# 0 "" 2
	.loc 1 80 5 view .LVU50
# 80 "<stdin>" 1
	/* "struct user_settings", "rec_frequency" = $252, $4, "i", $0; */

# 0 "" 2
	.loc 1 81 5 view .LVU51
# 81 "<stdin>" 1
	/* "struct user_settings", "rec_channels" = $256, $4, "i", $0; */

# 0 "" 2
	.loc 1 82 5 view .LVU52
# 82 "<stdin>" 1
	/* "struct user_settings", "rec_mic_gain" = $260, $4, "i", $0; */

# 0 "" 2
	.loc 1 83 5 view .LVU53
# 83 "<stdin>" 1
	/* "struct user_settings", "rec_left_gain" = $264, $4, "i", $0; */

# 0 "" 2
	.loc 1 84 5 view .LVU54
# 84 "<stdin>" 1
	/* "struct user_settings", "rec_right_gain" = $268, $4, "i", $0; */

# 0 "" 2
	.loc 1 85 5 view .LVU55
# 85 "<stdin>" 1
	/* "struct user_settings", "peak_meter_clipcounter" = $272, $1, "b", $0; */

# 0 "" 2
	.loc 1 86 5 view .LVU56
# 86 "<stdin>" 1
	/* "struct user_settings", "rec_editable" = $273, $1, "b", $0; */

# 0 "" 2
	.loc 1 87 5 view .LVU57
# 87 "<stdin>" 1
	/* "struct user_settings", "rec_timesplit" = $276, $4, "i", $0; */

# 0 "" 2
	.loc 1 88 5 view .LVU58
# 88 "<stdin>" 1
	/* "struct user_settings", "rec_sizesplit" = $280, $4, "i", $0; */

# 0 "" 2
	.loc 1 89 5 view .LVU59
# 89 "<stdin>" 1
	/* "struct user_settings", "rec_split_type" = $284, $4, "i", $0; */

# 0 "" 2
	.loc 1 90 5 view .LVU60
# 90 "<stdin>" 1
	/* "struct user_settings", "rec_split_method" = $288, $4, "i", $0; */

# 0 "" 2
	.loc 1 91 5 view .LVU61
# 91 "<stdin>" 1
	/* "struct user_settings", "rec_prerecord_time" = $292, $4, "i", $0; */

# 0 "" 2
	.loc 1 92 5 view .LVU62
# 92 "<stdin>" 1
	/* "struct user_settings", "rec_directory" = $296, $81, "str", $0; */

# 0 "" 2
	.loc 1 93 5 view .LVU63
# 93 "<stdin>" 1
	/* "struct user_settings", "cliplight" = $380, $4, "i", $0; */

# 0 "" 2
	.loc 1 94 5 view .LVU64
# 94 "<stdin>" 1
	/* "struct user_settings", "rec_start_thres_db" = $384, $4, "i", $0; */

# 0 "" 2
	.loc 1 95 5 view .LVU65
# 95 "<stdin>" 1
	/* "struct user_settings", "rec_start_thres_linear" = $388, $4, "i", $0; */

# 0 "" 2
	.loc 1 96 5 view .LVU66
# 96 "<stdin>" 1
	/* "struct user_settings", "rec_start_duration" = $392, $4, "i", $0; */

# 0 "" 2
	.loc 1 97 5 view .LVU67
# 97 "<stdin>" 1
	/* "struct user_settings", "rec_stop_thres_db" = $396, $4, "i", $0; */

# 0 "" 2
	.loc 1 98 5 view .LVU68
# 98 "<stdin>" 1
	/* "struct user_settings", "rec_stop_thres_linear" = $400, $4, "i", $0; */

# 0 "" 2
	.loc 1 99 5 view .LVU69
# 99 "<stdin>" 1
	/* "struct user_settings", "rec_stop_postrec" = $404, $4, "i", $0; */

# 0 "" 2
	.loc 1 100 5 view .LVU70
# 100 "<stdin>" 1
	/* "struct user_settings", "rec_stop_gap" = $408, $4, "i", $0; */

# 0 "" 2
	.loc 1 101 5 view .LVU71
# 101 "<stdin>" 1
	/* "struct user_settings", "rec_trigger_mode" = $412, $4, "i", $0; */

# 0 "" 2
	.loc 1 102 5 view .LVU72
# 102 "<stdin>" 1
	/* "struct user_settings", "rec_trigger_type" = $416, $4, "i", $0; */

# 0 "" 2
	.loc 1 103 5 view .LVU73
# 103 "<stdin>" 1
	/* "struct user_settings", "fm_region" = $420, $4, "i", $0; */

# 0 "" 2
	.loc 1 104 5 view .LVU74
# 104 "<stdin>" 1
	/* "struct user_settings", "fm_force_mono" = $424, $1, "b", $0; */

# 0 "" 2
	.loc 1 105 5 view .LVU75
# 105 "<stdin>" 1
	/* "struct user_settings", "fmr_file" = $425, $33, "str", $0; */

# 0 "" 2
	.loc 1 106 5 view .LVU76
# 106 "<stdin>" 1
	/* "struct user_settings", "fms_file" = $458, $33, "str", $0; */

# 0 "" 2
	.loc 1 107 5 view .LVU77
# 107 "<stdin>" 1
	/* "struct user_settings", "sync_rds_time" = $491, $1, "b", $0; */

# 0 "" 2
	.loc 1 108 5 view .LVU78
# 108 "<stdin>" 1
	/* "struct user_settings", "pause_rewind" = $492, $4, "i", $0; */

# 0 "" 2
	.loc 1 109 5 view .LVU79
# 109 "<stdin>" 1
	/* "struct user_settings", "unplug_mode" = $496, $4, "i", $0; */

# 0 "" 2
	.loc 1 110 5 view .LVU80
# 110 "<stdin>" 1
	/* "struct user_settings", "unplug_autoresume" = $500, $1, "b", $0; */

# 0 "" 2
	.loc 1 111 5 view .LVU81
# 111 "<stdin>" 1
	/* "struct user_settings", "qs_items" = $504, $32, "ptr_const_struct_typeisarray_settings_list", $0; */

# 0 "" 2
	.loc 1 112 5 view .LVU82
# 112 "<stdin>" 1
	/* "struct user_settings", "timeformat" = $536, $4, "i", $0; */

# 0 "" 2
	.loc 1 113 5 view .LVU83
# 113 "<stdin>" 1
	/* "struct user_settings", "disk_spindown" = $540, $4, "i", $0; */

# 0 "" 2
	.loc 1 114 5 view .LVU84
# 114 "<stdin>" 1
	/* "struct user_settings", "buffer_margin" = $544, $4, "i", $0; */

# 0 "" 2
	.loc 1 115 5 view .LVU85
# 115 "<stdin>" 1
	/* "struct user_settings", "storage_mode" = $548, $4, "i", $0; */

# 0 "" 2
	.loc 1 116 5 view .LVU86
# 116 "<stdin>" 1
	/* "struct user_settings", "dirfilter" = $552, $4, "i", $0; */

# 0 "" 2
	.loc 1 117 5 view .LVU87
# 117 "<stdin>" 1
	/* "struct user_settings", "show_filename_ext" = $556, $4, "i", $0; */

# 0 "" 2
	.loc 1 118 5 view .LVU88
# 118 "<stdin>" 1
	/* "struct user_settings", "default_codepage" = $560, $4, "i", $0; */

# 0 "" 2
	.loc 1 119 5 view .LVU89
# 119 "<stdin>" 1
	/* "struct user_settings", "hold_lr_for_scroll_in_list" = $564, $1, "b", $0; */

# 0 "" 2
	.loc 1 120 5 view .LVU90
# 120 "<stdin>" 1
	/* "struct user_settings", "play_selected" = $565, $1, "b", $0; */

# 0 "" 2
	.loc 1 121 5 view .LVU91
# 121 "<stdin>" 1
	/* "struct user_settings", "single_mode" = $568, $4, "i", $0; */

# 0 "" 2
	.loc 1 122 5 view .LVU92
# 122 "<stdin>" 1
	/* "struct user_settings", "party_mode" = $572, $1, "b", $0; */

# 0 "" 2
	.loc 1 123 5 view .LVU93
# 123 "<stdin>" 1
	/* "struct user_settings", "cuesheet" = $573, $1, "b", $0; */

# 0 "" 2
	.loc 1 124 5 view .LVU94
# 124 "<stdin>" 1
	/* "struct user_settings", "car_adapter_mode" = $574, $1, "b", $0; */

# 0 "" 2
	.loc 1 125 5 view .LVU95
# 125 "<stdin>" 1
	/* "struct user_settings", "car_adapter_mode_delay" = $576, $4, "i", $0; */

# 0 "" 2
	.loc 1 126 5 view .LVU96
# 126 "<stdin>" 1
	/* "struct user_settings", "start_in_screen" = $580, $4, "i", $0; */

# 0 "" 2
	.loc 1 127 5 view .LVU97
# 127 "<stdin>" 1
	/* "struct user_settings", "wps_select_action" = $584, $4, "i", $0; */

# 0 "" 2
	.loc 1 128 5 view .LVU98
# 128 "<stdin>" 1
	/* "struct user_settings", "alarm_wake_up_screen" = $588, $4, "i", $0; */

# 0 "" 2
	.loc 1 129 5 view .LVU99
# 129 "<stdin>" 1
	/* "struct user_settings", "ff_rewind_min_step" = $592, $4, "i", $0; */

# 0 "" 2
	.loc 1 130 5 view .LVU100
# 130 "<stdin>" 1
	/* "struct user_settings", "ff_rewind_accel" = $596, $4, "i", $0; */

# 0 "" 2
	.loc 1 131 5 view .LVU101
# 131 "<stdin>" 1
	/* "struct user_settings", "peak_meter_release" = $600, $4, "i", $0; */

# 0 "" 2
	.loc 1 132 5 view .LVU102
# 132 "<stdin>" 1
	/* "struct user_settings", "peak_meter_hold" = $604, $4, "i", $0; */

# 0 "" 2
	.loc 1 133 5 view .LVU103
# 133 "<stdin>" 1
	/* "struct user_settings", "peak_meter_clip_hold" = $608, $4, "i", $0; */

# 0 "" 2
	.loc 1 134 5 view .LVU104
# 134 "<stdin>" 1
	/* "struct user_settings", "peak_meter_dbfs" = $612, $1, "b", $0; */

# 0 "" 2
	.loc 1 135 5 view .LVU105
# 135 "<stdin>" 1
	/* "struct user_settings", "peak_meter_min" = $616, $4, "i", $0; */

# 0 "" 2
	.loc 1 136 5 view .LVU106
# 136 "<stdin>" 1
	/* "struct user_settings", "peak_meter_max" = $620, $4, "i", $0; */

# 0 "" 2
	.loc 1 137 5 view .LVU107
# 137 "<stdin>" 1
	/* "struct user_settings", "wps_file" = $624, $33, "str", $0; */

# 0 "" 2
	.loc 1 138 5 view .LVU108
# 138 "<stdin>" 1
	/* "struct user_settings", "sbs_file" = $657, $33, "str", $0; */

# 0 "" 2
	.loc 1 139 5 view .LVU109
# 139 "<stdin>" 1
	/* "struct user_settings", "lang_file" = $690, $33, "str", $0; */

# 0 "" 2
	.loc 1 140 5 view .LVU110
# 140 "<stdin>" 1
	/* "struct user_settings", "playlist_catalog_dir" = $723, $81, "str", $0; */

# 0 "" 2
	.loc 1 141 5 view .LVU111
# 141 "<stdin>" 1
	/* "struct user_settings", "skip_length" = $804, $4, "i", $0; */

# 0 "" 2
	.loc 1 142 5 view .LVU112
# 142 "<stdin>" 1
	/* "struct user_settings", "max_files_in_dir" = $808, $4, "i", $0; */

# 0 "" 2
	.loc 1 143 5 view .LVU113
# 143 "<stdin>" 1
	/* "struct user_settings", "max_files_in_playlist" = $812, $4, "i", $0; */

# 0 "" 2
	.loc 1 144 5 view .LVU114
# 144 "<stdin>" 1
	/* "struct user_settings", "volume_type" = $816, $4, "i", $0; */

# 0 "" 2
	.loc 1 145 5 view .LVU115
# 145 "<stdin>" 1
	/* "struct user_settings", "battery_display" = $820, $4, "i", $0; */

# 0 "" 2
	.loc 1 146 5 view .LVU116
# 146 "<stdin>" 1
	/* "struct user_settings", "show_icons" = $824, $1, "b", $0; */

# 0 "" 2
	.loc 1 147 5 view .LVU117
# 147 "<stdin>" 1
	/* "struct user_settings", "statusbar" = $828, $4, "i", $0; */

# 0 "" 2
	.loc 1 148 5 view .LVU118
# 148 "<stdin>" 1
	/* "struct user_settings", "scrollbar" = $832, $4, "i", $0; */

# 0 "" 2
	.loc 1 149 5 view .LVU119
# 149 "<stdin>" 1
	/* "struct user_settings", "scrollbar_width" = $836, $4, "i", $0; */

# 0 "" 2
	.loc 1 150 5 view .LVU120
# 150 "<stdin>" 1
	/* "struct user_settings", "list_separator_height" = $840, $4, "i", $0; */

# 0 "" 2
	.loc 1 151 5 view .LVU121
# 151 "<stdin>" 1
	/* "struct user_settings", "list_separator_color" = $844, $4, "i", $0; */

# 0 "" 2
	.loc 1 152 5 view .LVU122
# 152 "<stdin>" 1
	/* "struct user_settings", "browse_current" = $848, $1, "b", $0; */

# 0 "" 2
	.loc 1 153 5 view .LVU123
# 153 "<stdin>" 1
	/* "struct user_settings", "scroll_paginated" = $849, $1, "b", $0; */

# 0 "" 2
	.loc 1 154 5 view .LVU124
# 154 "<stdin>" 1
	/* "struct user_settings", "list_wraparound" = $850, $1, "b", $0; */

# 0 "" 2
	.loc 1 155 5 view .LVU125
# 155 "<stdin>" 1
	/* "struct user_settings", "list_order" = $852, $4, "i", $0; */

# 0 "" 2
	.loc 1 156 5 view .LVU126
# 156 "<stdin>" 1
	/* "struct user_settings", "scroll_speed" = $856, $4, "i", $0; */

# 0 "" 2
	.loc 1 157 5 view .LVU127
# 157 "<stdin>" 1
	/* "struct user_settings", "bidir_limit" = $860, $4, "i", $0; */

# 0 "" 2
	.loc 1 158 5 view .LVU128
# 158 "<stdin>" 1
	/* "struct user_settings", "scroll_delay" = $864, $4, "i", $0; */

# 0 "" 2
	.loc 1 159 5 view .LVU129
# 159 "<stdin>" 1
	/* "struct user_settings", "scroll_step" = $868, $4, "i", $0; */

# 0 "" 2
	.loc 1 160 5 view .LVU130
# 160 "<stdin>" 1
	/* "struct user_settings", "autoloadbookmark" = $872, $4, "i", $0; */

# 0 "" 2
	.loc 1 161 5 view .LVU131
# 161 "<stdin>" 1
	/* "struct user_settings", "autocreatebookmark" = $876, $4, "i", $0; */

# 0 "" 2
	.loc 1 162 5 view .LVU132
# 162 "<stdin>" 1
	/* "struct user_settings", "autoupdatebookmark" = $880, $1, "b", $0; */

# 0 "" 2
	.loc 1 163 5 view .LVU133
# 163 "<stdin>" 1
	/* "struct user_settings", "usemrb" = $884, $4, "i", $0; */

# 0 "" 2
	.loc 1 164 5 view .LVU134
# 164 "<stdin>" 1
	/* "struct user_settings", "tagcache_ram" = $888, $4, "i", $0; */

# 0 "" 2
	.loc 1 165 5 view .LVU135
# 165 "<stdin>" 1
	/* "struct user_settings", "tagcache_autoupdate" = $892, $1, "b", $0; */

# 0 "" 2
	.loc 1 166 5 view .LVU136
# 166 "<stdin>" 1
	/* "struct user_settings", "autoresume_enable" = $893, $1, "b", $0; */

# 0 "" 2
	.loc 1 167 5 view .LVU137
# 167 "<stdin>" 1
	/* "struct user_settings", "autoresume_automatic" = $896, $4, "i", $0; */

# 0 "" 2
	.loc 1 168 5 view .LVU138
# 168 "<stdin>" 1
	/* "struct user_settings", "autoresume_paths" = $900, $161, "str", $0; */

# 0 "" 2
	.loc 1 169 5 view .LVU139
# 169 "<stdin>" 1
	/* "struct user_settings", "runtimedb" = $1061, $1, "b", $0; */

# 0 "" 2
	.loc 1 170 5 view .LVU140
# 170 "<stdin>" 1
	/* "struct user_settings", "tagcache_scan_paths" = $1062, $161, "str", $0; */

# 0 "" 2
	.loc 1 171 5 view .LVU141
# 171 "<stdin>" 1
	/* "struct user_settings", "tagcache_db_path" = $1223, $81, "str", $0; */

# 0 "" 2
	.loc 1 172 5 view .LVU142
# 172 "<stdin>" 1
	/* "struct user_settings", "backdrop_file" = $1304, $81, "str", $0; */

# 0 "" 2
	.loc 1 173 5 view .LVU143
# 173 "<stdin>" 1
	/* "struct user_settings", "bg_color" = $1388, $4, "i", $0; */

# 0 "" 2
	.loc 1 174 5 view .LVU144
# 174 "<stdin>" 1
	/* "struct user_settings", "fg_color" = $1392, $4, "i", $0; */

# 0 "" 2
	.loc 1 175 5 view .LVU145
# 175 "<stdin>" 1
	/* "struct user_settings", "lss_color" = $1396, $4, "i", $0; */

# 0 "" 2
	.loc 1 176 5 view .LVU146
# 176 "<stdin>" 1
	/* "struct user_settings", "lse_color" = $1400, $4, "i", $0; */

# 0 "" 2
	.loc 1 177 5 view .LVU147
# 177 "<stdin>" 1
	/* "struct user_settings", "lst_color" = $1404, $4, "i", $0; */

# 0 "" 2
	.loc 1 178 5 view .LVU148
# 178 "<stdin>" 1
	/* "struct user_settings", "colors_file" = $1408, $33, "str", $0; */

# 0 "" 2
	.loc 1 179 5 view .LVU149
# 179 "<stdin>" 1
	/* "struct user_settings", "dynamic_colors" = $1441, $1, "b", $0; */

# 0 "" 2
	.loc 1 180 5 view .LVU150
# 180 "<stdin>" 1
	/* "struct user_settings", "ipone_charge_wallpaper" = $1444, $4, "i", $0; */

# 0 "" 2
	.loc 1 181 5 view .LVU151
# 181 "<stdin>" 1
	/* "struct user_settings", "ipone_lock_wallpaper" = $1448, $4, "i", $0; */

# 0 "" 2
	.loc 1 182 5 view .LVU152
# 182 "<stdin>" 1
	/* "struct user_settings", "ipone_right_pane" = $1452, $4, "i", $0; */

# 0 "" 2
	.loc 1 183 5 view .LVU153
# 183 "<stdin>" 1
	/* "struct user_settings", "ui_engine" = $1456, $4, "i", $0; */

# 0 "" 2
	.loc 1 184 5 view .LVU154
# 184 "<stdin>" 1
	/* "struct user_settings", "ui_engine_accent" = $1460, $4, "i", $0; */

# 0 "" 2
	.loc 1 185 5 view .LVU155
# 185 "<stdin>" 1
	/* "struct user_settings", "ui_engine_density" = $1464, $4, "i", $0; */

# 0 "" 2
	.loc 1 186 5 view .LVU156
# 186 "<stdin>" 1
	/* "struct user_settings", "ui_engine_font_scale" = $1468, $4, "i", $0; */

# 0 "" 2
	.loc 1 187 5 view .LVU157
# 187 "<stdin>" 1
	/* "struct user_settings", "ui_engine_surface" = $1472, $4, "i", $0; */

# 0 "" 2
	.loc 1 188 5 view .LVU158
# 188 "<stdin>" 1
	/* "struct user_settings", "ui_engine_hold_effect" = $1476, $4, "i", $0; */

# 0 "" 2
	.loc 1 189 5 view .LVU159
# 189 "<stdin>" 1
	/* "struct user_settings", "ui_engine_dark_mode" = $1480, $1, "b", $0; */

# 0 "" 2
	.loc 1 190 5 view .LVU160
# 190 "<stdin>" 1
	/* "struct user_settings", "album_list_layout" = $1484, $4, "i", $0; */

# 0 "" 2
	.loc 1 191 5 view .LVU161
# 191 "<stdin>" 1
	/* "struct user_settings", "browser_default" = $1488, $4, "i", $0; */

# 0 "" 2
	.loc 1 192 5 view .LVU162
# 192 "<stdin>" 1
	/* "struct user_settings", "repeat_mode" = $1492, $4, "i", $0; */

# 0 "" 2
	.loc 1 193 5 view .LVU163
# 193 "<stdin>" 1
	/* "struct user_settings", "next_folder" = $1496, $4, "i", $0; */

# 0 "" 2
	.loc 1 194 5 view .LVU164
# 194 "<stdin>" 1
	/* "struct user_settings", "constrain_next_folder" = $1500, $1, "b", $0; */

# 0 "" 2
	.loc 1 195 5 view .LVU165
# 195 "<stdin>" 1
	/* "struct user_settings", "recursive_dir_insert" = $1504, $4, "i", $0; */

# 0 "" 2
	.loc 1 196 5 view .LVU166
# 196 "<stdin>" 1
	/* "struct user_settings", "fade_on_stop" = $1508, $1, "b", $0; */

# 0 "" 2
	.loc 1 197 5 view .LVU167
# 197 "<stdin>" 1
	/* "struct user_settings", "playlist_shuffle" = $1509, $1, "b", $0; */

# 0 "" 2
	.loc 1 198 5 view .LVU168
# 198 "<stdin>" 1
	/* "struct user_settings", "warnon_erase_dynplaylist" = $1510, $1, "b", $0; */

# 0 "" 2
	.loc 1 199 5 view .LVU169
# 199 "<stdin>" 1
	/* "struct user_settings", "keep_current_track_on_replace_playlist" = $1511, $1, "b", $0; */

# 0 "" 2
	.loc 1 200 5 view .LVU170
# 200 "<stdin>" 1
	/* "struct user_settings", "show_shuffled_adding_options" = $1512, $1, "b", $0; */

# 0 "" 2
	.loc 1 201 5 view .LVU171
# 201 "<stdin>" 1
	/* "struct user_settings", "show_queue_options" = $1516, $4, "i", $0; */

# 0 "" 2
	.loc 1 202 5 view .LVU172
# 202 "<stdin>" 1
	/* "struct user_settings", "album_art" = $1520, $4, "i", $0; */

# 0 "" 2
	.loc 1 203 5 view .LVU173
# 203 "<stdin>" 1
	/* "struct user_settings", "rewind_across_tracks" = $1524, $1, "b", $0; */

# 0 "" 2
	.loc 1 204 5 view .LVU174
# 204 "<stdin>" 1
	/* "struct user_settings", "playlist_viewer_icons" = $1525, $1, "b", $0; */

# 0 "" 2
	.loc 1 205 5 view .LVU175
# 205 "<stdin>" 1
	/* "struct user_settings", "playlist_viewer_indices" = $1526, $1, "b", $0; */

# 0 "" 2
	.loc 1 206 5 view .LVU176
# 206 "<stdin>" 1
	/* "struct user_settings", "playlist_viewer_track_display" = $1528, $4, "i", $0; */

# 0 "" 2
	.loc 1 207 5 view .LVU177
# 207 "<stdin>" 1
	/* "struct user_settings", "talk_menu" = $1532, $1, "b", $0; */

# 0 "" 2
	.loc 1 208 5 view .LVU178
# 208 "<stdin>" 1
	/* "struct user_settings", "talk_dir" = $1536, $4, "i", $0; */

# 0 "" 2
	.loc 1 209 5 view .LVU179
# 209 "<stdin>" 1
	/* "struct user_settings", "talk_dir_clip" = $1540, $1, "b", $0; */

# 0 "" 2
	.loc 1 210 5 view .LVU180
# 210 "<stdin>" 1
	/* "struct user_settings", "talk_file" = $1544, $4, "i", $0; */

# 0 "" 2
	.loc 1 211 5 view .LVU181
# 211 "<stdin>" 1
	/* "struct user_settings", "talk_file_clip" = $1548, $1, "b", $0; */

# 0 "" 2
	.loc 1 212 5 view .LVU182
# 212 "<stdin>" 1
	/* "struct user_settings", "talk_filetype" = $1549, $1, "b", $0; */

# 0 "" 2
	.loc 1 213 5 view .LVU183
# 213 "<stdin>" 1
	/* "struct user_settings", "talk_battery_level" = $1550, $1, "b", $0; */

# 0 "" 2
	.loc 1 214 5 view .LVU184
# 214 "<stdin>" 1
	/* "struct user_settings", "talk_mixer_amp" = $1552, $4, "i", $0; */

# 0 "" 2
	.loc 1 215 5 view .LVU185
# 215 "<stdin>" 1
	/* "struct user_settings", "sort_case" = $1556, $1, "b", $0; */

# 0 "" 2
	.loc 1 216 5 view .LVU186
# 216 "<stdin>" 1
	/* "struct user_settings", "sort_dir" = $1560, $4, "i", $0; */

# 0 "" 2
	.loc 1 217 5 view .LVU187
# 217 "<stdin>" 1
	/* "struct user_settings", "sort_file" = $1564, $4, "i", $0; */

# 0 "" 2
	.loc 1 218 5 view .LVU188
# 218 "<stdin>" 1
	/* "struct user_settings", "sort_playlists" = $1568, $4, "i", $0; */

# 0 "" 2
	.loc 1 219 5 view .LVU189
# 219 "<stdin>" 1
	/* "struct user_settings", "interpret_numbers" = $1572, $4, "i", $0; */

# 0 "" 2
	.loc 1 220 5 view .LVU190
# 220 "<stdin>" 1
	/* "struct user_settings", "poweroff" = $1576, $4, "i", $0; */

# 0 "" 2
	.loc 1 221 5 view .LVU191
# 221 "<stdin>" 1
	/* "struct user_settings", "battery_capacity" = $1580, $4, "i", $0; */

# 0 "" 2
	.loc 1 222 5 view .LVU192
# 222 "<stdin>" 1
	/* "struct user_settings", "usb_charging" = $1584, $4, "i", $0; */

# 0 "" 2
	.loc 1 223 5 view .LVU193
# 223 "<stdin>" 1
	/* "struct user_settings", "cursor_style" = $1588, $4, "i", $0; */

# 0 "" 2
	.loc 1 224 5 view .LVU194
# 224 "<stdin>" 1
	/* "struct user_settings", "screen_scroll_step" = $1592, $4, "i", $0; */

# 0 "" 2
	.loc 1 225 5 view .LVU195
# 225 "<stdin>" 1
	/* "struct user_settings", "show_path_in_browser" = $1596, $4, "i", $0; */

# 0 "" 2
	.loc 1 226 5 view .LVU196
# 226 "<stdin>" 1
	/* "struct user_settings", "offset_out_of_view" = $1600, $1, "b", $0; */

# 0 "" 2
	.loc 1 227 5 view .LVU197
# 227 "<stdin>" 1
	/* "struct user_settings", "disable_mainmenu_scrolling" = $1601, $1, "b", $0; */

# 0 "" 2
	.loc 1 228 5 view .LVU198
# 228 "<stdin>" 1
	/* "struct user_settings", "icon_file" = $1602, $33, "str", $0; */

# 0 "" 2
	.loc 1 229 5 view .LVU199
# 229 "<stdin>" 1
	/* "struct user_settings", "viewers_icon_file" = $1635, $33, "str", $0; */

# 0 "" 2
	.loc 1 230 5 view .LVU200
# 230 "<stdin>" 1
	/* "struct user_settings", "font_file" = $1668, $33, "str", $0; */

# 0 "" 2
	.loc 1 231 5 view .LVU201
# 231 "<stdin>" 1
	/* "struct user_settings", "glyphs_to_cache" = $1704, $4, "i", $0; */

# 0 "" 2
	.loc 1 232 5 view .LVU202
# 232 "<stdin>" 1
	/* "struct user_settings", "kbd_file" = $1708, $33, "str", $0; */

# 0 "" 2
	.loc 1 233 5 view .LVU203
# 233 "<stdin>" 1
	/* "struct user_settings", "backlight_timeout" = $1744, $4, "i", $0; */

# 0 "" 2
	.loc 1 234 5 view .LVU204
# 234 "<stdin>" 1
	/* "struct user_settings", "caption_backlight" = $1748, $1, "b", $0; */

# 0 "" 2
	.loc 1 235 5 view .LVU205
# 235 "<stdin>" 1
	/* "struct user_settings", "bl_filter_first_keypress" = $1749, $1, "b", $0; */

# 0 "" 2
	.loc 1 236 5 view .LVU206
# 236 "<stdin>" 1
	/* "struct user_settings", "backlight_timeout_plugged" = $1752, $4, "i", $0; */

# 0 "" 2
	.loc 1 237 5 view .LVU207
# 237 "<stdin>" 1
	/* "struct user_settings", "bl_selective_actions" = $1756, $1, "b", $0; */

# 0 "" 2
	.loc 1 238 5 view .LVU208
# 238 "<stdin>" 1
	/* "struct user_settings", "bl_selective_actions_mask" = $1760, $4, "i", $0; */

# 0 "" 2
	.loc 1 239 5 view .LVU209
# 239 "<stdin>" 1
	/* "struct user_settings", "backlight_on_button_hold" = $1764, $4, "i", $0; */

# 0 "" 2
	.loc 1 240 5 view .LVU210
# 240 "<stdin>" 1
	/* "struct user_settings", "lcd_sleep_after_backlight_off" = $1768, $4, "i", $0; */

# 0 "" 2
	.loc 1 241 5 view .LVU211
# 241 "<stdin>" 1
	/* "struct user_settings", "brightness" = $1772, $4, "i", $0; */

# 0 "" 2
	.loc 1 242 5 view .LVU212
# 242 "<stdin>" 1
	/* "struct user_settings", "accessory_supply" = $1776, $1, "b", $0; */

# 0 "" 2
	.loc 1 243 5 view .LVU213
# 243 "<stdin>" 1
	/* "struct user_settings", "lineout_active" = $1777, $1, "b", $0; */

# 0 "" 2
	.loc 1 244 5 view .LVU214
# 244 "<stdin>" 1
	/* "struct user_settings", "prevent_skip" = $1778, $1, "b", $0; */

# 0 "" 2
	.loc 1 245 5 view .LVU215
# 245 "<stdin>" 1
	/* "struct user_settings", "pitch_mode_semitone" = $1779, $1, "b", $0; */

# 0 "" 2
	.loc 1 246 5 view .LVU216
# 246 "<stdin>" 1
	/* "struct user_settings", "pitch_mode_timestretch" = $1780, $1, "b", $0; */

# 0 "" 2
	.loc 1 247 5 view .LVU217
# 247 "<stdin>" 1
	/* "struct user_settings", "ui_vp_config" = $1781, $64, "str", $0; */

# 0 "" 2
	.loc 1 248 5 view .LVU218
# 248 "<stdin>" 1
	/* "struct user_settings", "compressor_settings" = $1848, $24, "s_compressor_settings", $0; */

# 0 "" 2
	.loc 1 249 5 view .LVU219
# 249 "<stdin>" 1
	/* "struct user_settings", "sleeptimer_duration" = $1872, $4, "i", $0; */

# 0 "" 2
	.loc 1 250 5 view .LVU220
# 250 "<stdin>" 1
	/* "struct user_settings", "sleeptimer_on_startup" = $1876, $1, "b", $0; */

# 0 "" 2
	.loc 1 251 5 view .LVU221
# 251 "<stdin>" 1
	/* "struct user_settings", "keypress_restarts_sleeptimer" = $1877, $1, "b", $0; */

# 0 "" 2
	.loc 1 252 5 view .LVU222
# 252 "<stdin>" 1
	/* "struct user_settings", "show_shutdown_message" = $1878, $1, "b", $0; */

# 0 "" 2
	.loc 1 253 5 view .LVU223
# 253 "<stdin>" 1
	/* "struct user_settings", "morse_input" = $1879, $1, "b", $0; */

# 0 "" 2
	.loc 1 254 5 view .LVU224
# 254 "<stdin>" 1
	/* "struct user_settings", "hotkey_wps" = $1880, $4, "i", $0; */

# 0 "" 2
	.loc 1 255 5 view .LVU225
# 255 "<stdin>" 1
	/* "struct user_settings", "hotkey_tree" = $1884, $4, "i", $0; */

# 0 "" 2
	.loc 1 256 5 view .LVU226
# 256 "<stdin>" 1
	/* "struct user_settings", "resume_rewind" = $1888, $4, "i", $0; */

# 0 "" 2
	.loc 1 257 5 view .LVU227
# 257 "<stdin>" 1
	/* "struct user_settings", "keyclick_hardware" = $1892, $1, "b", $0; */

# 0 "" 2
	.loc 1 258 5 view .LVU228
# 258 "<stdin>" 1
	/* "struct user_settings", "haptics_enabled" = $1893, $1, "b", $0; */

# 0 "" 2
	.loc 1 259 5 view .LVU229
# 259 "<stdin>" 1
	/* "struct user_settings", "start_directory" = $1894, $81, "str", $0; */

# 0 "" 2
	.loc 1 260 5 view .LVU230
# 260 "<stdin>" 1
	/* "struct user_settings", "root_menu_customized" = $1975, $1, "b", $0; */

# 0 "" 2
	.loc 1 261 5 view .LVU231
# 261 "<stdin>" 1
	/* "struct user_settings", "shortcuts_replaces_qs" = $1976, $1, "b", $0; */

# 0 "" 2
	.loc 1 262 5 view .LVU232
# 262 "<stdin>" 1
	/* "struct user_settings", "play_frequency" = $1980, $4, "i", $0; */

# 0 "" 2
	.loc 1 263 5 view .LVU233
# 263 "<stdin>" 1
	/* "struct user_settings", "volume_limit" = $1984, $4, "i", $0; */

# 0 "" 2
	.loc 1 264 5 view .LVU234
# 264 "<stdin>" 1
	/* "struct user_settings", "volume_adjust_mode" = $1988, $4, "i", $0; */

# 0 "" 2
	.loc 1 265 5 view .LVU235
# 265 "<stdin>" 1
	/* "struct user_settings", "volume_adjust_norm_steps" = $1992, $4, "i", $0; */

# 0 "" 2
	.loc 1 266 5 view .LVU236
# 266 "<stdin>" 1
	/* "struct user_settings", "surround_enabled" = $1996, $4, "i", $0; */

# 0 "" 2
	.loc 1 267 5 view .LVU237
# 267 "<stdin>" 1
	/* "struct user_settings", "surround_balance" = $2000, $4, "i", $0; */

# 0 "" 2
	.loc 1 268 5 view .LVU238
# 268 "<stdin>" 1
	/* "struct user_settings", "surround_fx1" = $2004, $4, "i", $0; */

# 0 "" 2
	.loc 1 269 5 view .LVU239
# 269 "<stdin>" 1
	/* "struct user_settings", "surround_fx2" = $2008, $4, "i", $0; */

# 0 "" 2
	.loc 1 270 5 view .LVU240
# 270 "<stdin>" 1
	/* "struct user_settings", "surround_method2" = $2012, $1, "b", $0; */

# 0 "" 2
	.loc 1 271 5 view .LVU241
# 271 "<stdin>" 1
	/* "struct user_settings", "surround_mix" = $2016, $4, "i", $0; */

# 0 "" 2
	.loc 1 272 5 view .LVU242
# 272 "<stdin>" 1
	/* "struct user_settings", "pbe" = $2020, $4, "i", $0; */

# 0 "" 2
	.loc 1 273 5 view .LVU243
# 273 "<stdin>" 1
	/* "struct user_settings", "pbe_precut" = $2024, $4, "i", $0; */

# 0 "" 2
	.loc 1 274 5 view .LVU244
# 274 "<stdin>" 1
	/* "struct user_settings", "afr_enabled" = $2028, $4, "i", $0; */

# 0 "" 2
	.loc 1 275 5 view .LVU245
# 275 "<stdin>" 1
	/* "struct user_settings", "clear_settings_on_hold" = $2032, $1, "b", $0; */

# 0 "" 2
	.loc 1 276 5 view .LVU246
# 276 "<stdin>" 1
	/* "struct user_settings", "playback_log" = $2033, $1, "b", $0; */

# 0 "" 2
	.loc 1 277 5 view .LVU247
# 277 "<stdin>" 1
	/* "struct user_settings", "ui_engine_lock_settings" = $2034, $1, "b", $0; */

# 0 "" 2
	.loc 1 278 5 view .LVU248
# 278 "<stdin>" 1
	/* "struct user_settings", "ui_engine_video_appearance" = $2036, $4, "i", $0; */

# 0 "" 2
	.loc 1 279 5 view .LVU249
# 279 "<stdin>" 1
	/* "struct user_settings", "ui_engine_games_appearance" = $2040, $4, "i", $0; */

# 0 "" 2
	.loc 1 280 5 view .LVU250
# 280 "<stdin>" 1
	/* "struct user_settings", "ui_engine_extras_pane" = $2044, $4, "i", $0; */

# 0 "" 2
	.loc 1 281 1 view .LVU251
	.loc 1 282 5 view .LVU252
# 282 "<stdin>" 1
	/* "struct replaygain_settings", "noclip" = $0, $1, "b", $0; */

# 0 "" 2
	.loc 1 283 5 view .LVU253
# 283 "<stdin>" 1
	/* "struct replaygain_settings", "type" = $4, $4, "i", $0; */

# 0 "" 2
	.loc 1 284 5 view .LVU254
# 284 "<stdin>" 1
	/* "struct replaygain_settings", "preamp" = $8, $4, "i", $0; */

# 0 "" 2
	.loc 1 285 1 view .LVU255
	.loc 1 286 5 view .LVU256
# 286 "<stdin>" 1
	/* "struct eq_band_setting", "cutoff" = $0, $4, "i", $0; */

# 0 "" 2
	.loc 1 287 5 view .LVU257
# 287 "<stdin>" 1
	/* "struct eq_band_setting", "q" = $4, $4, "i", $0; */

# 0 "" 2
	.loc 1 288 5 view .LVU258
# 288 "<stdin>" 1
	/* "struct eq_band_setting", "gain" = $8, $4, "i", $0; */

# 0 "" 2
	.loc 1 289 1 view .LVU259
	.loc 1 290 5 view .LVU260
# 290 "<stdin>" 1
	/* "struct compressor_settings", "threshold" = $0, $4, "i", $0; */

# 0 "" 2
	.loc 1 291 5 view .LVU261
# 291 "<stdin>" 1
	/* "struct compressor_settings", "makeup_gain" = $4, $4, "i", $0; */

# 0 "" 2
	.loc 1 292 5 view .LVU262
# 292 "<stdin>" 1
	/* "struct compressor_settings", "ratio" = $8, $4, "i", $0; */

# 0 "" 2
	.loc 1 293 5 view .LVU263
# 293 "<stdin>" 1
	/* "struct compressor_settings", "knee" = $12, $4, "i", $0; */

# 0 "" 2
	.loc 1 294 5 view .LVU264
# 294 "<stdin>" 1
	/* "struct compressor_settings", "release_time" = $16, $4, "i", $0; */

# 0 "" 2
	.loc 1 295 5 view .LVU265
# 295 "<stdin>" 1
	/* "struct compressor_settings", "attack_time" = $20, $4, "i", $0; */

# 0 "" 2
	.loc 1 296 1 view .LVU266
	.loc 1 297 5 view .LVU267
# 297 "<stdin>" 1
	/* "struct mp3_enc_config", "bitrate" = $0, $8, "u_l", $0; */

# 0 "" 2
	.loc 1 298 1 view .LVU268
	.loc 1 299 5 view .LVU269
# 299 "<stdin>" 1
	/* "struct mp3entry", "path" = $0, $260, "str", $0; */

# 0 "" 2
	.loc 1 300 5 view .LVU270
# 300 "<stdin>" 1
	/* "struct mp3entry", "title" = $264, $8, "ptr_char", $0; */

# 0 "" 2
	.loc 1 301 5 view .LVU271
# 301 "<stdin>" 1
	/* "struct mp3entry", "artist" = $272, $8, "ptr_char", $0; */

# 0 "" 2
	.loc 1 302 5 view .LVU272
# 302 "<stdin>" 1
	/* "struct mp3entry", "album" = $280, $8, "ptr_char", $0; */

# 0 "" 2
	.loc 1 303 5 view .LVU273
# 303 "<stdin>" 1
	/* "struct mp3entry", "genre_string" = $288, $8, "ptr_char", $0; */

# 0 "" 2
	.loc 1 304 5 view .LVU274
# 304 "<stdin>" 1
	/* "struct mp3entry", "disc_string" = $296, $8, "ptr_char", $0; */

# 0 "" 2
	.loc 1 305 5 view .LVU275
# 305 "<stdin>" 1
	/* "struct mp3entry", "track_string" = $304, $8, "ptr_char", $0; */

# 0 "" 2
	.loc 1 306 5 view .LVU276
# 306 "<stdin>" 1
	/* "struct mp3entry", "year_string" = $312, $8, "ptr_char", $0; */

# 0 "" 2
	.loc 1 307 5 view .LVU277
# 307 "<stdin>" 1
	/* "struct mp3entry", "composer" = $320, $8, "ptr_char", $0; */

# 0 "" 2
	.loc 1 308 5 view .LVU278
# 308 "<stdin>" 1
	/* "struct mp3entry", "comment" = $328, $8, "ptr_char", $0; */

# 0 "" 2
	.loc 1 309 5 view .LVU279
# 309 "<stdin>" 1
	/* "struct mp3entry", "albumartist" = $336, $8, "ptr_char", $0; */

# 0 "" 2
	.loc 1 310 5 view .LVU280
# 310 "<stdin>" 1
	/* "struct mp3entry", "grouping" = $344, $8, "ptr_char", $0; */

# 0 "" 2
	.loc 1 311 5 view .LVU281
# 311 "<stdin>" 1
	/* "struct mp3entry", "discnum" = $352, $4, "i", $0; */

# 0 "" 2
	.loc 1 312 5 view .LVU282
# 312 "<stdin>" 1
	/* "struct mp3entry", "tracknum" = $356, $4, "i", $0; */

# 0 "" 2
	.loc 1 313 5 view .LVU283
# 313 "<stdin>" 1
	/* "struct mp3entry", "layer" = $360, $4, "i", $0; */

# 0 "" 2
	.loc 1 314 5 view .LVU284
# 314 "<stdin>" 1
	/* "struct mp3entry", "year" = $364, $4, "i", $0; */

# 0 "" 2
	.loc 1 315 5 view .LVU285
# 315 "<stdin>" 1
	/* "struct mp3entry", "id3version" = $368, $1, "u_c", $0; */

# 0 "" 2
	.loc 1 316 5 view .LVU286
# 316 "<stdin>" 1
	/* "struct mp3entry", "codectype" = $372, $4, "u_i", $0; */

# 0 "" 2
	.loc 1 317 5 view .LVU287
# 317 "<stdin>" 1
	/* "struct mp3entry", "bitrate" = $376, $4, "u_i", $0; */

# 0 "" 2
	.loc 1 318 5 view .LVU288
# 318 "<stdin>" 1
	/* "struct mp3entry", "frequency" = $384, $8, "u_l", $0; */

# 0 "" 2
	.loc 1 319 5 view .LVU289
# 319 "<stdin>" 1
	/* "struct mp3entry", "id3v2len" = $392, $8, "u_l", $0; */

# 0 "" 2
	.loc 1 320 5 view .LVU290
# 320 "<stdin>" 1
	/* "struct mp3entry", "id3v1len" = $400, $8, "u_l", $0; */

# 0 "" 2
	.loc 1 321 5 view .LVU291
# 321 "<stdin>" 1
	/* "struct mp3entry", "first_frame_offset" = $408, $8, "u_l", $0; */

# 0 "" 2
	.loc 1 322 5 view .LVU292
# 322 "<stdin>" 1
	/* "struct mp3entry", "sim_filesize" = $416, $8, "u_l", $0; */

# 0 "" 2
	.loc 1 323 5 view .LVU293
# 323 "<stdin>" 1
	/* "struct mp3entry", "length" = $424, $8, "u_l", $0; */

# 0 "" 2
	.loc 1 324 5 view .LVU294
# 324 "<stdin>" 1
	/* "struct mp3entry", "elapsed" = $432, $8, "u_l", $0; */

# 0 "" 2
	.loc 1 325 5 view .LVU295
# 325 "<stdin>" 1
	/* "struct mp3entry", "lead_trim" = $440, $4, "i", $0; */

# 0 "" 2
	.loc 1 326 5 view .LVU296
# 326 "<stdin>" 1
	/* "struct mp3entry", "tail_trim" = $444, $4, "i", $0; */

# 0 "" 2
	.loc 1 327 5 view .LVU297
# 327 "<stdin>" 1
	/* "struct mp3entry", "samples" = $448, $8, "u_i", $0; */

# 0 "" 2
	.loc 1 328 5 view .LVU298
# 328 "<stdin>" 1
	/* "struct mp3entry", "frame_count" = $456, $8, "u_l", $0; */

# 0 "" 2
	.loc 1 329 5 view .LVU299
# 329 "<stdin>" 1
	/* "struct mp3entry", "bytesperframe" = $464, $8, "u_l", $0; */

# 0 "" 2
	.loc 1 330 5 view .LVU300
# 330 "<stdin>" 1
	/* "struct mp3entry", "vbr" = $472, $1, "b", $0; */

# 0 "" 2
	.loc 1 331 5 view .LVU301
# 331 "<stdin>" 1
	/* "struct mp3entry", "has_toc" = $473, $1, "b", $0; */

# 0 "" 2
	.loc 1 332 5 view .LVU302
# 332 "<stdin>" 1
	/* "struct mp3entry", "toc" = $474, $100, "str", $0; */

# 0 "" 2
	.loc 1 333 5 view .LVU303
# 333 "<stdin>" 1
	/* "struct mp3entry", "needs_upsampling_correction" = $574, $1, "b", $0; */

# 0 "" 2
	.loc 1 334 5 view .LVU304
# 334 "<stdin>" 1
	/* "struct mp3entry", "id3v2buf" = $575, $1800, "str", $0; */

# 0 "" 2
	.loc 1 335 5 view .LVU305
# 335 "<stdin>" 1
	/* "struct mp3entry", "id3v1buf" = $2375, $368, "str", $0; */

# 0 "" 2
	.loc 1 336 5 view .LVU306
# 336 "<stdin>" 1
	/* "struct mp3entry", "offset" = $2744, $8, "u_l", $0; */

# 0 "" 2
	.loc 1 337 5 view .LVU307
# 337 "<stdin>" 1
	/* "struct mp3entry", "index" = $2752, $4, "i", $0; */

# 0 "" 2
	.loc 1 338 5 view .LVU308
# 338 "<stdin>" 1
	/* "struct mp3entry", "skip_resume_adjustments" = $2756, $1, "b", $0; */

# 0 "" 2
	.loc 1 339 5 view .LVU309
# 339 "<stdin>" 1
	/* "struct mp3entry", "autoresumable" = $2757, $1, "u_c", $0; */

# 0 "" 2
	.loc 1 340 5 view .LVU310
# 340 "<stdin>" 1
	/* "struct mp3entry", "tagcache_idx" = $2760, $8, "l", $0; */

# 0 "" 2
	.loc 1 341 5 view .LVU311
# 341 "<stdin>" 1
	/* "struct mp3entry", "rating" = $2768, $4, "i", $0; */

# 0 "" 2
	.loc 1 342 5 view .LVU312
# 342 "<stdin>" 1
	/* "struct mp3entry", "score" = $2772, $4, "i", $0; */

# 0 "" 2
	.loc 1 343 5 view .LVU313
# 343 "<stdin>" 1
	/* "struct mp3entry", "playcount" = $2776, $8, "l", $0; */

# 0 "" 2
	.loc 1 344 5 view .LVU314
# 344 "<stdin>" 1
	/* "struct mp3entry", "lastplayed" = $2784, $8, "l", $0; */

# 0 "" 2
	.loc 1 345 5 view .LVU315
# 345 "<stdin>" 1
	/* "struct mp3entry", "playtime" = $2792, $8, "l", $0; */

# 0 "" 2
	.loc 1 346 5 view .LVU316
# 346 "<stdin>" 1
	/* "struct mp3entry", "track_level" = $2800, $8, "l", $0; */

# 0 "" 2
	.loc 1 347 5 view .LVU317
# 347 "<stdin>" 1
	/* "struct mp3entry", "album_level" = $2808, $8, "l", $0; */

# 0 "" 2
	.loc 1 348 5 view .LVU318
# 348 "<stdin>" 1
	/* "struct mp3entry", "track_gain" = $2816, $8, "l", $0; */

# 0 "" 2
	.loc 1 349 5 view .LVU319
# 349 "<stdin>" 1
	/* "struct mp3entry", "album_gain" = $2824, $8, "l", $0; */

# 0 "" 2
	.loc 1 350 5 view .LVU320
# 350 "<stdin>" 1
	/* "struct mp3entry", "track_peak" = $2832, $8, "l", $0; */

# 0 "" 2
	.loc 1 351 5 view .LVU321
# 351 "<stdin>" 1
	/* "struct mp3entry", "album_peak" = $2840, $8, "l", $0; */

# 0 "" 2
	.loc 1 352 5 view .LVU322
# 352 "<stdin>" 1
	/* "struct mp3entry", "has_embedded_albumart" = $2848, $1, "b", $0; */

# 0 "" 2
	.loc 1 353 5 view .LVU323
# 353 "<stdin>" 1
	/* "struct mp3entry", "albumart" = $2856, $16, "s_mp3_albumart", $0; */

# 0 "" 2
	.loc 1 354 5 view .LVU324
# 354 "<stdin>" 1
	/* "struct mp3entry", "has_embedded_cuesheet" = $2872, $1, "b", $0; */

# 0 "" 2
	.loc 1 355 5 view .LVU325
# 355 "<stdin>" 1
	/* "struct mp3entry", "embedded_cuesheet" = $2880, $24, "s_embedded_cuesheet", $0; */

# 0 "" 2
	.loc 1 356 5 view .LVU326
# 356 "<stdin>" 1
	/* "struct mp3entry", "cuesheet" = $2904, $8, "ptr_s_cuesheet", $0; */

# 0 "" 2
	.loc 1 357 5 view .LVU327
# 357 "<stdin>" 1
	/* "struct mp3entry", "mb_track_id" = $2912, $8, "ptr_char", $0; */

# 0 "" 2
	.loc 1 358 5 view .LVU328
# 358 "<stdin>" 1
	/* "struct mp3entry", "is_asf_stream" = $2920, $1, "b", $0; */

# 0 "" 2
	.loc 1 359 5 view .LVU329
# 359 "<stdin>" 1
	/* "struct mp3entry", "has_video" = $2921, $1, "b", $0; */

# 0 "" 2
	.loc 1 360 1 view .LVU330
	.loc 1 361 5 view .LVU331
# 361 "<stdin>" 1
	/* "struct mp3_albumart", "type" = $0, $4, "e_mp3_aa_type", $0; */

# 0 "" 2
	.loc 1 362 5 view .LVU332
# 362 "<stdin>" 1
	/* "struct mp3_albumart", "size" = $4, $4, "i", $0; */

# 0 "" 2
	.loc 1 363 5 view .LVU333
# 363 "<stdin>" 1
	/* "struct mp3_albumart", "pos" = $8, $8, "off_t", $0; */

# 0 "" 2
	.loc 1 364 1 view .LVU334
	.loc 1 365 5 view .LVU335
# 365 "<stdin>" 1
	/* "struct embedded_cuesheet", "size" = $0, $4, "i", $0; */

# 0 "" 2
	.loc 1 366 5 view .LVU336
# 366 "<stdin>" 1
	/* "struct embedded_cuesheet", "pos" = $8, $8, "off_t", $0; */

# 0 "" 2
	.loc 1 367 5 view .LVU337
# 367 "<stdin>" 1
	/* "struct embedded_cuesheet", "encoding" = $16, $4, "e_character_encoding", $0; */

# 0 "" 2
	.loc 1 369 5 view .LVU338
	.loc 1 370 1 is_stmt 0 view .LVU339
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
	.file 6 "/usr/lib/gcc/x86_64-pc-linux-gnu/16/include/stddef.h"
	.file 7 "/home/david/Documents/RockBox_Personal-master/lib/rbcodec/metadata/metadata.h"
	.file 8 "/home/david/Documents/RockBox_Personal-master/firmware/export/enc_base.h"
	.file 9 "/home/david/Documents/RockBox_Personal-master/lib/rbcodec/dsp/compressor.h"
	.file 10 "/home/david/Documents/RockBox_Personal-master/lib/rbcodec/dsp/dsp_misc.h"
	.file 11 "/home/david/Documents/RockBox_Personal-master/lib/rbcodec/dsp/eq.h"
	.file 12 "/home/david/Documents/RockBox_Personal-master/apps/settings.h"
	.file 13 "/home/david/Documents/RockBox_Personal-master/apps/settings_list.h"
	.section	.debug_info,"",@progbits
.Ldebug_info0:
	.long	0x1a26
	.value	0x5
	.byte	0x1
	.byte	0x8
	.long	.Ldebug_abbrev0
	.uleb128 0x19
	.long	.LASF411
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
	.byte	0x3c
	.byte	0xc
	.value	0x194
	.byte	0x8
	.long	0x79c
	.uleb128 0x3
	.long	.LASF118
	.value	0x196
	.byte	0x9
	.long	0x71
	.byte	0
	.uleb128 0x3
	.long	.LASF119
	.value	0x197
	.byte	0x9
	.long	0x71
	.byte	0x4
	.uleb128 0x3
	.long	.LASF120
	.value	0x198
	.byte	0xe
	.long	0xd3
	.byte	0x8
	.uleb128 0x3
	.long	.LASF121
	.value	0x199
	.byte	0xe
	.long	0xd3
	.byte	0xc
	.uleb128 0x3
	.long	.LASF122
	.value	0x19a
	.byte	0xe
	.long	0xd3
	.byte	0x10
	.uleb128 0x3
	.long	.LASF123
	.value	0x19c
	.byte	0xd
	.long	0xc7
	.byte	0x14
	.uleb128 0x3
	.long	.LASF124
	.value	0x19d
	.byte	0xd
	.long	0xc7
	.byte	0x18
	.uleb128 0x3
	.long	.LASF125
	.value	0x19f
	.byte	0x9
	.long	0x71
	.byte	0x1c
	.uleb128 0x3
	.long	.LASF126
	.value	0x1a0
	.byte	0x9
	.long	0x71
	.byte	0x20
	.uleb128 0x3
	.long	.LASF127
	.value	0x1a5
	.byte	0x9
	.long	0x71
	.byte	0x24
	.uleb128 0x3
	.long	.LASF128
	.value	0x1a8
	.byte	0x11
	.long	0x4b
	.byte	0x28
	.uleb128 0x3
	.long	.LASF129
	.value	0x1a9
	.byte	0xa
	.long	0x71
	.byte	0x2c
	.uleb128 0x3
	.long	.LASF130
	.value	0x1aa
	.byte	0x9
	.long	0x71
	.byte	0x30
	.uleb128 0x3
	.long	.LASF131
	.value	0x1ab
	.byte	0x9
	.long	0x79c
	.byte	0x34
	.uleb128 0x3
	.long	.LASF132
	.value	0x1ad
	.byte	0xa
	.long	0x121
	.byte	0x38
	.byte	0
	.uleb128 0xb
	.long	0x71
	.long	0x7ac
	.uleb128 0xd
	.long	0x44
	.byte	0
	.byte	0
	.uleb128 0x21
	.long	.LASF133
	.value	0x800
	.byte	0xc
	.value	0x1b0
	.byte	0x8
	.long	0x1514
	.uleb128 0x3
	.long	.LASF134
	.value	0x1b3
	.byte	0x9
	.long	0x71
	.byte	0
	.uleb128 0x3
	.long	.LASF135
	.value	0x1b4
	.byte	0x9
	.long	0x71
	.byte	0x4
	.uleb128 0x3
	.long	.LASF136
	.value	0x1b5
	.byte	0x9
	.long	0x71
	.byte	0x8
	.uleb128 0x3
	.long	.LASF137
	.value	0x1b6
	.byte	0x9
	.long	0x71
	.byte	0xc
	.uleb128 0x3
	.long	.LASF138
	.value	0x1b7
	.byte	0x9
	.long	0x71
	.byte	0x10
	.uleb128 0x3
	.long	.LASF139
	.value	0x1ba
	.byte	0x9
	.long	0x71
	.byte	0x14
	.uleb128 0x3
	.long	.LASF140
	.value	0x1bd
	.byte	0x9
	.long	0x71
	.byte	0x18
	.uleb128 0x3
	.long	.LASF141
	.value	0x1c2
	.byte	0x9
	.long	0x71
	.byte	0x1c
	.uleb128 0x3
	.long	.LASF142
	.value	0x1c4
	.byte	0x9
	.long	0x71
	.byte	0x20
	.uleb128 0x3
	.long	.LASF143
	.value	0x1c5
	.byte	0x9
	.long	0x71
	.byte	0x24
	.uleb128 0x3
	.long	.LASF144
	.value	0x1c6
	.byte	0x9
	.long	0x71
	.byte	0x28
	.uleb128 0x3
	.long	.LASF145
	.value	0x1c7
	.byte	0x9
	.long	0x71
	.byte	0x2c
	.uleb128 0x3
	.long	.LASF146
	.value	0x1c8
	.byte	0x9
	.long	0x71
	.byte	0x30
	.uleb128 0x3
	.long	.LASF111
	.value	0x1cc
	.byte	0x20
	.long	0x664
	.byte	0x34
	.uleb128 0x3
	.long	.LASF147
	.value	0x1cf
	.byte	0x9
	.long	0x71
	.byte	0x40
	.uleb128 0x3
	.long	.LASF148
	.value	0x1d0
	.byte	0x12
	.long	0x3d
	.byte	0x44
	.uleb128 0x3
	.long	.LASF149
	.value	0x1d1
	.byte	0x12
	.long	0x3d
	.byte	0x48
	.uleb128 0x3
	.long	.LASF150
	.value	0x1d2
	.byte	0x12
	.long	0x3d
	.byte	0x4c
	.uleb128 0x3
	.long	.LASF151
	.value	0x1d3
	.byte	0x12
	.long	0x3d
	.byte	0x50
	.uleb128 0x3
	.long	.LASF152
	.value	0x1d6
	.byte	0xa
	.long	0x121
	.byte	0x54
	.uleb128 0x3
	.long	.LASF153
	.value	0x1d7
	.byte	0x12
	.long	0x3d
	.byte	0x58
	.uleb128 0x3
	.long	.LASF154
	.value	0x1d8
	.byte	0x1c
	.long	0x1514
	.byte	0x5c
	.uleb128 0x3
	.long	.LASF155
	.value	0x1db
	.byte	0xa
	.long	0x71
	.byte	0xd4
	.uleb128 0x3
	.long	.LASF156
	.value	0x1dc
	.byte	0xa
	.long	0x71
	.byte	0xd8
	.uleb128 0x3
	.long	.LASF157
	.value	0x1dd
	.byte	0xa
	.long	0x71
	.byte	0xdc
	.uleb128 0x3
	.long	.LASF158
	.value	0x1de
	.byte	0xa
	.long	0x121
	.byte	0xe0
	.uleb128 0x3
	.long	.LASF159
	.value	0x1e0
	.byte	0xa
	.long	0x121
	.byte	0xe1
	.uleb128 0x3
	.long	.LASF160
	.value	0x1e4
	.byte	0x9
	.long	0x71
	.byte	0xe4
	.uleb128 0x3
	.long	.LASF161
	.value	0x1e5
	.byte	0x9
	.long	0x71
	.byte	0xe8
	.uleb128 0x3
	.long	.LASF103
	.value	0x1e8
	.byte	0x1f
	.long	0x5ef
	.byte	0xf0
	.uleb128 0x3
	.long	.LASF162
	.value	0x1f1
	.byte	0x9
	.long	0x71
	.byte	0xf8
	.uleb128 0x3
	.long	.LASF163
	.value	0x1f2
	.byte	0x9
	.long	0x71
	.byte	0xfc
	.uleb128 0x1
	.long	.LASF164
	.byte	0xc
	.value	0x1f8
	.byte	0x9
	.long	0x71
	.value	0x100
	.uleb128 0x1
	.long	.LASF165
	.byte	0xc
	.value	0x1fa
	.byte	0x9
	.long	0x71
	.value	0x104
	.uleb128 0x1
	.long	.LASF166
	.byte	0xc
	.value	0x1fb
	.byte	0x9
	.long	0x71
	.value	0x108
	.uleb128 0x1
	.long	.LASF167
	.byte	0xc
	.value	0x1fc
	.byte	0x9
	.long	0x71
	.value	0x10c
	.uleb128 0x1
	.long	.LASF168
	.byte	0xc
	.value	0x1fd
	.byte	0xa
	.long	0x121
	.value	0x110
	.uleb128 0x1
	.long	.LASF169
	.byte	0xc
	.value	0x1fe
	.byte	0xa
	.long	0x121
	.value	0x111
	.uleb128 0x1
	.long	.LASF170
	.byte	0xc
	.value	0x201
	.byte	0x9
	.long	0x71
	.value	0x114
	.uleb128 0x1
	.long	.LASF171
	.byte	0xc
	.value	0x202
	.byte	0x9
	.long	0x71
	.value	0x118
	.uleb128 0x1
	.long	.LASF172
	.byte	0xc
	.value	0x207
	.byte	0x9
	.long	0x71
	.value	0x11c
	.uleb128 0x1
	.long	.LASF173
	.byte	0xc
	.value	0x208
	.byte	0x9
	.long	0x71
	.value	0x120
	.uleb128 0x1
	.long	.LASF174
	.byte	0xc
	.value	0x20a
	.byte	0x9
	.long	0x71
	.value	0x124
	.uleb128 0x1
	.long	.LASF175
	.byte	0xc
	.value	0x20b
	.byte	0xa
	.long	0x1524
	.value	0x128
	.uleb128 0x1
	.long	.LASF176
	.byte	0xc
	.value	0x20c
	.byte	0x9
	.long	0x71
	.value	0x17c
	.uleb128 0x1
	.long	.LASF177
	.byte	0xc
	.value	0x211
	.byte	0x9
	.long	0x71
	.value	0x180
	.uleb128 0x1
	.long	.LASF178
	.byte	0xc
	.value	0x212
	.byte	0x9
	.long	0x71
	.value	0x184
	.uleb128 0x1
	.long	.LASF179
	.byte	0xc
	.value	0x213
	.byte	0x9
	.long	0x71
	.value	0x188
	.uleb128 0x1
	.long	.LASF180
	.byte	0xc
	.value	0x214
	.byte	0x9
	.long	0x71
	.value	0x18c
	.uleb128 0x1
	.long	.LASF181
	.byte	0xc
	.value	0x215
	.byte	0x9
	.long	0x71
	.value	0x190
	.uleb128 0x1
	.long	.LASF182
	.byte	0xc
	.value	0x216
	.byte	0x9
	.long	0x71
	.value	0x194
	.uleb128 0x1
	.long	.LASF183
	.byte	0xc
	.value	0x217
	.byte	0x9
	.long	0x71
	.value	0x198
	.uleb128 0x1
	.long	.LASF184
	.byte	0xc
	.value	0x218
	.byte	0x9
	.long	0x71
	.value	0x19c
	.uleb128 0x1
	.long	.LASF185
	.byte	0xc
	.value	0x219
	.byte	0x9
	.long	0x71
	.value	0x1a0
	.uleb128 0x1
	.long	.LASF186
	.byte	0xc
	.value	0x234
	.byte	0x9
	.long	0x71
	.value	0x1a4
	.uleb128 0x1
	.long	.LASF187
	.byte	0xc
	.value	0x235
	.byte	0xa
	.long	0x121
	.value	0x1a8
	.uleb128 0x1
	.long	.LASF188
	.byte	0xc
	.value	0x236
	.byte	0x13
	.long	0x1534
	.value	0x1a9
	.uleb128 0x1
	.long	.LASF189
	.byte	0xc
	.value	0x237
	.byte	0x13
	.long	0x1534
	.value	0x1ca
	.uleb128 0x1
	.long	.LASF190
	.byte	0xc
	.value	0x23d
	.byte	0xa
	.long	0x121
	.value	0x1eb
	.uleb128 0x1
	.long	.LASF191
	.byte	0xc
	.value	0x24e
	.byte	0xa
	.long	0x71
	.value	0x1ec
	.uleb128 0x1
	.long	.LASF192
	.byte	0xc
	.value	0x250
	.byte	0xa
	.long	0x71
	.value	0x1f0
	.uleb128 0x1
	.long	.LASF193
	.byte	0xc
	.value	0x251
	.byte	0xa
	.long	0x121
	.value	0x1f4
	.uleb128 0x1
	.long	.LASF194
	.byte	0xc
	.value	0x255
	.byte	0x21
	.long	0x1544
	.value	0x1f8
	.uleb128 0x1
	.long	.LASF195
	.byte	0xc
	.value	0x258
	.byte	0x9
	.long	0x71
	.value	0x218
	.uleb128 0x1
	.long	.LASF196
	.byte	0xc
	.value	0x25b
	.byte	0x9
	.long	0x71
	.value	0x21c
	.uleb128 0x1
	.long	.LASF197
	.byte	0xc
	.value	0x25c
	.byte	0x9
	.long	0x71
	.value	0x220
	.uleb128 0x1
	.long	.LASF198
	.byte	0xc
	.value	0x25d
	.byte	0x9
	.long	0x71
	.value	0x224
	.uleb128 0x1
	.long	.LASF199
	.byte	0xc
	.value	0x260
	.byte	0x9
	.long	0x71
	.value	0x228
	.uleb128 0x1
	.long	.LASF200
	.byte	0xc
	.value	0x262
	.byte	0x9
	.long	0x71
	.value	0x22c
	.uleb128 0x1
	.long	.LASF201
	.byte	0xc
	.value	0x264
	.byte	0x9
	.long	0x71
	.value	0x230
	.uleb128 0x1
	.long	.LASF202
	.byte	0xc
	.value	0x265
	.byte	0xa
	.long	0x121
	.value	0x234
	.uleb128 0x1
	.long	.LASF203
	.byte	0xc
	.value	0x266
	.byte	0xa
	.long	0x121
	.value	0x235
	.uleb128 0x1
	.long	.LASF204
	.byte	0xc
	.value	0x267
	.byte	0x9
	.long	0x71
	.value	0x238
	.uleb128 0x1
	.long	.LASF205
	.byte	0xc
	.value	0x269
	.byte	0xa
	.long	0x121
	.value	0x23c
	.uleb128 0x1
	.long	.LASF82
	.byte	0xc
	.value	0x26a
	.byte	0xa
	.long	0x121
	.value	0x23d
	.uleb128 0x1
	.long	.LASF206
	.byte	0xc
	.value	0x26b
	.byte	0xa
	.long	0x121
	.value	0x23e
	.uleb128 0x1
	.long	.LASF207
	.byte	0xc
	.value	0x26c
	.byte	0x9
	.long	0x71
	.value	0x240
	.uleb128 0x1
	.long	.LASF208
	.byte	0xc
	.value	0x26d
	.byte	0x9
	.long	0x71
	.value	0x244
	.uleb128 0x1
	.long	.LASF209
	.byte	0xc
	.value	0x26e
	.byte	0x9
	.long	0x71
	.value	0x248
	.uleb128 0x1
	.long	.LASF210
	.byte	0xc
	.value	0x271
	.byte	0x9
	.long	0x71
	.value	0x24c
	.uleb128 0x1
	.long	.LASF211
	.byte	0xc
	.value	0x273
	.byte	0x9
	.long	0x71
	.value	0x250
	.uleb128 0x1
	.long	.LASF212
	.byte	0xc
	.value	0x274
	.byte	0x9
	.long	0x71
	.value	0x254
	.uleb128 0x1
	.long	.LASF213
	.byte	0xc
	.value	0x276
	.byte	0x9
	.long	0x71
	.value	0x258
	.uleb128 0x1
	.long	.LASF214
	.byte	0xc
	.value	0x277
	.byte	0x9
	.long	0x71
	.value	0x25c
	.uleb128 0x1
	.long	.LASF215
	.byte	0xc
	.value	0x278
	.byte	0x9
	.long	0x71
	.value	0x260
	.uleb128 0x1
	.long	.LASF216
	.byte	0xc
	.value	0x279
	.byte	0xa
	.long	0x121
	.value	0x264
	.uleb128 0x1
	.long	.LASF217
	.byte	0xc
	.value	0x27a
	.byte	0x9
	.long	0x71
	.value	0x268
	.uleb128 0x1
	.long	.LASF218
	.byte	0xc
	.value	0x27b
	.byte	0x9
	.long	0x71
	.value	0x26c
	.uleb128 0x1
	.long	.LASF219
	.byte	0xc
	.value	0x27d
	.byte	0x13
	.long	0x1534
	.value	0x270
	.uleb128 0x1
	.long	.LASF220
	.byte	0xc
	.value	0x27e
	.byte	0x13
	.long	0x1534
	.value	0x291
	.uleb128 0x1
	.long	.LASF221
	.byte	0xc
	.value	0x283
	.byte	0x13
	.long	0x1534
	.value	0x2b2
	.uleb128 0x1
	.long	.LASF222
	.byte	0xc
	.value	0x284
	.byte	0x13
	.long	0x15b2
	.value	0x2d3
	.uleb128 0x1
	.long	.LASF223
	.byte	0xc
	.value	0x285
	.byte	0x9
	.long	0x71
	.value	0x324
	.uleb128 0x1
	.long	.LASF224
	.byte	0xc
	.value	0x286
	.byte	0x9
	.long	0x71
	.value	0x328
	.uleb128 0x1
	.long	.LASF225
	.byte	0xc
	.value	0x287
	.byte	0x9
	.long	0x71
	.value	0x32c
	.uleb128 0x1
	.long	.LASF226
	.byte	0xc
	.value	0x288
	.byte	0x9
	.long	0x71
	.value	0x330
	.uleb128 0x1
	.long	.LASF227
	.byte	0xc
	.value	0x289
	.byte	0x9
	.long	0x71
	.value	0x334
	.uleb128 0x1
	.long	.LASF228
	.byte	0xc
	.value	0x28a
	.byte	0xa
	.long	0x121
	.value	0x338
	.uleb128 0x1
	.long	.LASF229
	.byte	0xc
	.value	0x28b
	.byte	0x9
	.long	0x71
	.value	0x33c
	.uleb128 0x1
	.long	.LASF230
	.byte	0xc
	.value	0x290
	.byte	0x9
	.long	0x71
	.value	0x340
	.uleb128 0x1
	.long	.LASF231
	.byte	0xc
	.value	0x291
	.byte	0x9
	.long	0x71
	.value	0x344
	.uleb128 0x1
	.long	.LASF232
	.byte	0xc
	.value	0x297
	.byte	0x9
	.long	0x71
	.value	0x348
	.uleb128 0x1
	.long	.LASF233
	.byte	0xc
	.value	0x298
	.byte	0x9
	.long	0x71
	.value	0x34c
	.uleb128 0x1
	.long	.LASF234
	.byte	0xc
	.value	0x29b
	.byte	0xa
	.long	0x121
	.value	0x350
	.uleb128 0x1
	.long	.LASF235
	.byte	0xc
	.value	0x29d
	.byte	0xa
	.long	0x121
	.value	0x351
	.uleb128 0x1
	.long	.LASF236
	.byte	0xc
	.value	0x29e
	.byte	0xa
	.long	0x121
	.value	0x352
	.uleb128 0x1
	.long	.LASF237
	.byte	0xc
	.value	0x29f
	.byte	0xa
	.long	0x71
	.value	0x354
	.uleb128 0x1
	.long	.LASF238
	.byte	0xc
	.value	0x2a0
	.byte	0xa
	.long	0x71
	.value	0x358
	.uleb128 0x1
	.long	.LASF239
	.byte	0xc
	.value	0x2a1
	.byte	0xa
	.long	0x71
	.value	0x35c
	.uleb128 0x1
	.long	.LASF240
	.byte	0xc
	.value	0x2a2
	.byte	0xa
	.long	0x71
	.value	0x360
	.uleb128 0x1
	.long	.LASF241
	.byte	0xc
	.value	0x2a3
	.byte	0xa
	.long	0x71
	.value	0x364
	.uleb128 0x1
	.long	.LASF242
	.byte	0xc
	.value	0x2a6
	.byte	0x9
	.long	0x71
	.value	0x368
	.uleb128 0x1
	.long	.LASF243
	.byte	0xc
	.value	0x2a7
	.byte	0x9
	.long	0x71
	.value	0x36c
	.uleb128 0x1
	.long	.LASF244
	.byte	0xc
	.value	0x2a8
	.byte	0xa
	.long	0x121
	.value	0x370
	.uleb128 0x1
	.long	.LASF245
	.byte	0xc
	.value	0x2a9
	.byte	0x9
	.long	0x71
	.value	0x374
	.uleb128 0x1
	.long	.LASF246
	.byte	0xc
	.value	0x2b1
	.byte	0x9
	.long	0x71
	.value	0x378
	.uleb128 0x1
	.long	.LASF247
	.byte	0xc
	.value	0x2b3
	.byte	0xa
	.long	0x121
	.value	0x37c
	.uleb128 0x1
	.long	.LASF248
	.byte	0xc
	.value	0x2b4
	.byte	0xa
	.long	0x121
	.value	0x37d
	.uleb128 0x1
	.long	.LASF249
	.byte	0xc
	.value	0x2b5
	.byte	0x9
	.long	0x71
	.value	0x380
	.uleb128 0x1
	.long	.LASF250
	.byte	0xc
	.value	0x2b7
	.byte	0x13
	.long	0x15c2
	.value	0x384
	.uleb128 0x1
	.long	.LASF251
	.byte	0xc
	.value	0x2b8
	.byte	0xa
	.long	0x121
	.value	0x425
	.uleb128 0x1
	.long	.LASF252
	.byte	0xc
	.value	0x2b9
	.byte	0x13
	.long	0x15c2
	.value	0x426
	.uleb128 0x1
	.long	.LASF253
	.byte	0xc
	.value	0x2ba
	.byte	0x13
	.long	0x15b2
	.value	0x4c7
	.uleb128 0x1
	.long	.LASF254
	.byte	0xc
	.value	0x2be
	.byte	0x13
	.long	0x15b2
	.value	0x518
	.uleb128 0x1
	.long	.LASF255
	.byte	0xc
	.value	0x2c2
	.byte	0x9
	.long	0x71
	.value	0x56c
	.uleb128 0x1
	.long	.LASF256
	.byte	0xc
	.value	0x2c3
	.byte	0x9
	.long	0x71
	.value	0x570
	.uleb128 0x1
	.long	.LASF257
	.byte	0xc
	.value	0x2c4
	.byte	0x9
	.long	0x71
	.value	0x574
	.uleb128 0x1
	.long	.LASF258
	.byte	0xc
	.value	0x2c5
	.byte	0x9
	.long	0x71
	.value	0x578
	.uleb128 0x1
	.long	.LASF259
	.byte	0xc
	.value	0x2c6
	.byte	0x9
	.long	0x71
	.value	0x57c
	.uleb128 0x1
	.long	.LASF260
	.byte	0xc
	.value	0x2c7
	.byte	0x13
	.long	0x1534
	.value	0x580
	.uleb128 0x1
	.long	.LASF261
	.byte	0xc
	.value	0x2c9
	.byte	0xa
	.long	0x121
	.value	0x5a1
	.uleb128 0x1
	.long	.LASF262
	.byte	0xc
	.value	0x2cd
	.byte	0x9
	.long	0x71
	.value	0x5a4
	.uleb128 0x1
	.long	.LASF263
	.byte	0xc
	.value	0x2ce
	.byte	0x9
	.long	0x71
	.value	0x5a8
	.uleb128 0x1
	.long	.LASF264
	.byte	0xc
	.value	0x2cf
	.byte	0x9
	.long	0x71
	.value	0x5ac
	.uleb128 0x1
	.long	.LASF265
	.byte	0xc
	.value	0x2d0
	.byte	0x9
	.long	0x71
	.value	0x5b0
	.uleb128 0x1
	.long	.LASF266
	.byte	0xc
	.value	0x2d1
	.byte	0x9
	.long	0x71
	.value	0x5b4
	.uleb128 0x1
	.long	.LASF267
	.byte	0xc
	.value	0x2d2
	.byte	0x9
	.long	0x71
	.value	0x5b8
	.uleb128 0x1
	.long	.LASF268
	.byte	0xc
	.value	0x2d3
	.byte	0x9
	.long	0x71
	.value	0x5bc
	.uleb128 0x1
	.long	.LASF269
	.byte	0xc
	.value	0x2d4
	.byte	0x9
	.long	0x71
	.value	0x5c0
	.uleb128 0x1
	.long	.LASF270
	.byte	0xc
	.value	0x2d5
	.byte	0x9
	.long	0x71
	.value	0x5c4
	.uleb128 0x1
	.long	.LASF271
	.byte	0xc
	.value	0x2d6
	.byte	0xa
	.long	0x121
	.value	0x5c8
	.uleb128 0x1
	.long	.LASF272
	.byte	0xc
	.value	0x2d7
	.byte	0x9
	.long	0x71
	.value	0x5cc
	.uleb128 0x1
	.long	.LASF273
	.byte	0xc
	.value	0x2d9
	.byte	0x9
	.long	0x71
	.value	0x5d0
	.uleb128 0x1
	.long	.LASF274
	.byte	0xc
	.value	0x2dc
	.byte	0xa
	.long	0x71
	.value	0x5d4
	.uleb128 0x1
	.long	.LASF275
	.byte	0xc
	.value	0x2dd
	.byte	0xa
	.long	0x71
	.value	0x5d8
	.uleb128 0x1
	.long	.LASF276
	.byte	0xc
	.value	0x2de
	.byte	0xa
	.long	0x121
	.value	0x5dc
	.uleb128 0x1
	.long	.LASF277
	.byte	0xc
	.value	0x2e0
	.byte	0xa
	.long	0x71
	.value	0x5e0
	.uleb128 0x1
	.long	.LASF278
	.byte	0xc
	.value	0x2e1
	.byte	0xa
	.long	0x121
	.value	0x5e4
	.uleb128 0x1
	.long	.LASF279
	.byte	0xc
	.value	0x2e2
	.byte	0xa
	.long	0x121
	.value	0x5e5
	.uleb128 0x1
	.long	.LASF280
	.byte	0xc
	.value	0x2e3
	.byte	0xa
	.long	0x121
	.value	0x5e6
	.uleb128 0x1
	.long	.LASF281
	.byte	0xc
	.value	0x2e4
	.byte	0xa
	.long	0x121
	.value	0x5e7
	.uleb128 0x1
	.long	.LASF282
	.byte	0xc
	.value	0x2e5
	.byte	0xa
	.long	0x121
	.value	0x5e8
	.uleb128 0x1
	.long	.LASF283
	.byte	0xc
	.value	0x2e6
	.byte	0x9
	.long	0x71
	.value	0x5ec
	.uleb128 0x1
	.long	.LASF284
	.byte	0xc
	.value	0x2e8
	.byte	0x9
	.long	0x71
	.value	0x5f0
	.uleb128 0x1
	.long	.LASF285
	.byte	0xc
	.value	0x2ea
	.byte	0xa
	.long	0x121
	.value	0x5f4
	.uleb128 0x1
	.long	.LASF286
	.byte	0xc
	.value	0x2ed
	.byte	0xa
	.long	0x121
	.value	0x5f5
	.uleb128 0x1
	.long	.LASF287
	.byte	0xc
	.value	0x2ee
	.byte	0xa
	.long	0x121
	.value	0x5f6
	.uleb128 0x1
	.long	.LASF288
	.byte	0xc
	.value	0x2ef
	.byte	0x9
	.long	0x71
	.value	0x5f8
	.uleb128 0x1
	.long	.LASF289
	.byte	0xc
	.value	0x2f2
	.byte	0xa
	.long	0x121
	.value	0x5fc
	.uleb128 0x1
	.long	.LASF290
	.byte	0xc
	.value	0x2f3
	.byte	0x9
	.long	0x71
	.value	0x600
	.uleb128 0x1
	.long	.LASF291
	.byte	0xc
	.value	0x2f4
	.byte	0xa
	.long	0x121
	.value	0x604
	.uleb128 0x1
	.long	.LASF292
	.byte	0xc
	.value	0x2f5
	.byte	0x9
	.long	0x71
	.value	0x608
	.uleb128 0x1
	.long	.LASF293
	.byte	0xc
	.value	0x2f6
	.byte	0xa
	.long	0x121
	.value	0x60c
	.uleb128 0x1
	.long	.LASF294
	.byte	0xc
	.value	0x2f7
	.byte	0xa
	.long	0x121
	.value	0x60d
	.uleb128 0x1
	.long	.LASF295
	.byte	0xc
	.value	0x2f8
	.byte	0xa
	.long	0x121
	.value	0x60e
	.uleb128 0x1
	.long	.LASF296
	.byte	0xc
	.value	0x2f9
	.byte	0xa
	.long	0x71
	.value	0x610
	.uleb128 0x1
	.long	.LASF297
	.byte	0xc
	.value	0x2fc
	.byte	0xa
	.long	0x121
	.value	0x614
	.uleb128 0x1
	.long	.LASF298
	.byte	0xc
	.value	0x2fd
	.byte	0x9
	.long	0x71
	.value	0x618
	.uleb128 0x1
	.long	.LASF299
	.byte	0xc
	.value	0x2fe
	.byte	0x9
	.long	0x71
	.value	0x61c
	.uleb128 0x1
	.long	.LASF300
	.byte	0xc
	.value	0x2ff
	.byte	0x9
	.long	0x71
	.value	0x620
	.uleb128 0x1
	.long	.LASF301
	.byte	0xc
	.value	0x300
	.byte	0x9
	.long	0x71
	.value	0x624
	.uleb128 0x1
	.long	.LASF302
	.byte	0xc
	.value	0x303
	.byte	0x9
	.long	0x71
	.value	0x628
	.uleb128 0x1
	.long	.LASF303
	.byte	0xc
	.value	0x305
	.byte	0x9
	.long	0x71
	.value	0x62c
	.uleb128 0x1
	.long	.LASF304
	.byte	0xc
	.value	0x30b
	.byte	0x9
	.long	0x71
	.value	0x630
	.uleb128 0x1
	.long	.LASF305
	.byte	0xc
	.value	0x318
	.byte	0xa
	.long	0x71
	.value	0x634
	.uleb128 0x1
	.long	.LASF306
	.byte	0xc
	.value	0x319
	.byte	0xa
	.long	0x71
	.value	0x638
	.uleb128 0x1
	.long	.LASF307
	.byte	0xc
	.value	0x31a
	.byte	0xa
	.long	0x71
	.value	0x63c
	.uleb128 0x1
	.long	.LASF308
	.byte	0xc
	.value	0x31b
	.byte	0xa
	.long	0x121
	.value	0x640
	.uleb128 0x1
	.long	.LASF309
	.byte	0xc
	.value	0x31c
	.byte	0xa
	.long	0x121
	.value	0x641
	.uleb128 0x1
	.long	.LASF310
	.byte	0xc
	.value	0x31d
	.byte	0x13
	.long	0x1534
	.value	0x642
	.uleb128 0x1
	.long	.LASF311
	.byte	0xc
	.value	0x31e
	.byte	0x13
	.long	0x1534
	.value	0x663
	.uleb128 0x1
	.long	.LASF312
	.byte	0xc
	.value	0x31f
	.byte	0x13
	.long	0x1534
	.value	0x684
	.uleb128 0x1
	.long	.LASF313
	.byte	0xc
	.value	0x320
	.byte	0x9
	.long	0x71
	.value	0x6a8
	.uleb128 0x1
	.long	.LASF314
	.byte	0xc
	.value	0x324
	.byte	0x13
	.long	0x1534
	.value	0x6ac
	.uleb128 0x1
	.long	.LASF315
	.byte	0xc
	.value	0x325
	.byte	0xa
	.long	0x71
	.value	0x6d0
	.uleb128 0x1
	.long	.LASF316
	.byte	0xc
	.value	0x327
	.byte	0xa
	.long	0x121
	.value	0x6d4
	.uleb128 0x1
	.long	.LASF317
	.byte	0xc
	.value	0x328
	.byte	0xa
	.long	0x121
	.value	0x6d5
	.uleb128 0x1
	.long	.LASF318
	.byte	0xc
	.value	0x32a
	.byte	0x9
	.long	0x71
	.value	0x6d8
	.uleb128 0x1
	.long	.LASF319
	.byte	0xc
	.value	0x331
	.byte	0xa
	.long	0x121
	.value	0x6dc
	.uleb128 0x1
	.long	.LASF320
	.byte	0xc
	.value	0x332
	.byte	0xa
	.long	0x71
	.value	0x6e0
	.uleb128 0x1
	.long	.LASF321
	.byte	0xc
	.value	0x333
	.byte	0x9
	.long	0x71
	.value	0x6e4
	.uleb128 0x1
	.long	.LASF322
	.byte	0xc
	.value	0x336
	.byte	0x9
	.long	0x71
	.value	0x6e8
	.uleb128 0x1
	.long	.LASF323
	.byte	0xc
	.value	0x344
	.byte	0x9
	.long	0x71
	.value	0x6ec
	.uleb128 0x1
	.long	.LASF324
	.byte	0xc
	.value	0x36c
	.byte	0xa
	.long	0x121
	.value	0x6f0
	.uleb128 0x1
	.long	.LASF325
	.byte	0xc
	.value	0x36f
	.byte	0xa
	.long	0x121
	.value	0x6f1
	.uleb128 0x1
	.long	.LASF326
	.byte	0xc
	.value	0x375
	.byte	0xa
	.long	0x121
	.value	0x6f2
	.uleb128 0x1
	.long	.LASF327
	.byte	0xc
	.value	0x37e
	.byte	0xa
	.long	0x121
	.value	0x6f3
	.uleb128 0x1
	.long	.LASF328
	.byte	0xc
	.value	0x37f
	.byte	0xa
	.long	0x121
	.value	0x6f4
	.uleb128 0x1
	.long	.LASF329
	.byte	0xc
	.value	0x392
	.byte	0x13
	.long	0x15d2
	.value	0x6f5
	.uleb128 0x1
	.long	.LASF104
	.byte	0xc
	.value	0x397
	.byte	0x20
	.long	0x609
	.value	0x738
	.uleb128 0x1
	.long	.LASF330
	.byte	0xc
	.value	0x399
	.byte	0x9
	.long	0x71
	.value	0x750
	.uleb128 0x1
	.long	.LASF331
	.byte	0xc
	.value	0x39a
	.byte	0xa
	.long	0x121
	.value	0x754
	.uleb128 0x1
	.long	.LASF332
	.byte	0xc
	.value	0x39b
	.byte	0xa
	.long	0x121
	.value	0x755
	.uleb128 0x1
	.long	.LASF333
	.byte	0xc
	.value	0x39d
	.byte	0xa
	.long	0x121
	.value	0x756
	.uleb128 0x1
	.long	.LASF334
	.byte	0xc
	.value	0x3a1
	.byte	0xa
	.long	0x121
	.value	0x757
	.uleb128 0x1
	.long	.LASF335
	.byte	0xc
	.value	0x3a7
	.byte	0x9
	.long	0x71
	.value	0x758
	.uleb128 0x1
	.long	.LASF336
	.byte	0xc
	.value	0x3a8
	.byte	0x9
	.long	0x71
	.value	0x75c
	.uleb128 0x1
	.long	.LASF337
	.byte	0xc
	.value	0x3ac
	.byte	0x9
	.long	0x71
	.value	0x760
	.uleb128 0x1
	.long	.LASF338
	.byte	0xc
	.value	0x3ca
	.byte	0xa
	.long	0x121
	.value	0x764
	.uleb128 0x1
	.long	.LASF339
	.byte	0xc
	.value	0x3cb
	.byte	0xa
	.long	0x121
	.value	0x765
	.uleb128 0x1
	.long	.LASF340
	.byte	0xc
	.value	0x3ce
	.byte	0xa
	.long	0x1524
	.value	0x766
	.uleb128 0x1
	.long	.LASF341
	.byte	0xc
	.value	0x3d0
	.byte	0xa
	.long	0x121
	.value	0x7b7
	.uleb128 0x1
	.long	.LASF342
	.byte	0xc
	.value	0x3d2
	.byte	0xa
	.long	0x121
	.value	0x7b8
	.uleb128 0x1
	.long	.LASF343
	.byte	0xc
	.value	0x3d6
	.byte	0x9
	.long	0x71
	.value	0x7bc
	.uleb128 0x1
	.long	.LASF344
	.byte	0xc
	.value	0x3d8
	.byte	0x9
	.long	0x71
	.value	0x7c0
	.uleb128 0x1
	.long	.LASF345
	.byte	0xc
	.value	0x3db
	.byte	0x9
	.long	0x71
	.value	0x7c4
	.uleb128 0x1
	.long	.LASF346
	.byte	0xc
	.value	0x3dc
	.byte	0x9
	.long	0x71
	.value	0x7c8
	.uleb128 0x1
	.long	.LASF347
	.byte	0xc
	.value	0x3df
	.byte	0x9
	.long	0x71
	.value	0x7cc
	.uleb128 0x1
	.long	.LASF348
	.byte	0xc
	.value	0x3e0
	.byte	0x9
	.long	0x71
	.value	0x7d0
	.uleb128 0x1
	.long	.LASF349
	.byte	0xc
	.value	0x3e1
	.byte	0x9
	.long	0x71
	.value	0x7d4
	.uleb128 0x1
	.long	.LASF350
	.byte	0xc
	.value	0x3e2
	.byte	0x9
	.long	0x71
	.value	0x7d8
	.uleb128 0x1
	.long	.LASF351
	.byte	0xc
	.value	0x3e3
	.byte	0xa
	.long	0x121
	.value	0x7dc
	.uleb128 0x1
	.long	.LASF352
	.byte	0xc
	.value	0x3e4
	.byte	0x9
	.long	0x71
	.value	0x7e0
	.uleb128 0x13
	.string	"pbe"
	.byte	0xc
	.value	0x3e6
	.byte	0x9
	.long	0x71
	.value	0x7e4
	.uleb128 0x1
	.long	.LASF353
	.byte	0xc
	.value	0x3e7
	.byte	0x9
	.long	0x71
	.value	0x7e8
	.uleb128 0x1
	.long	.LASF354
	.byte	0xc
	.value	0x3e9
	.byte	0x9
	.long	0x71
	.value	0x7ec
	.uleb128 0x1
	.long	.LASF355
	.byte	0xc
	.value	0x3f5
	.byte	0xa
	.long	0x121
	.value	0x7f0
	.uleb128 0x1
	.long	.LASF356
	.byte	0xc
	.value	0x3fa
	.byte	0xa
	.long	0x121
	.value	0x7f1
	.uleb128 0x1
	.long	.LASF357
	.byte	0xc
	.value	0x3ff
	.byte	0xa
	.long	0x121
	.value	0x7f2
	.uleb128 0x1
	.long	.LASF358
	.byte	0xc
	.value	0x400
	.byte	0x9
	.long	0x71
	.value	0x7f4
	.uleb128 0x1
	.long	.LASF359
	.byte	0xc
	.value	0x401
	.byte	0x9
	.long	0x71
	.value	0x7f8
	.uleb128 0x1
	.long	.LASF360
	.byte	0xc
	.value	0x402
	.byte	0x9
	.long	0x71
	.value	0x7fc
	.byte	0
	.uleb128 0xb
	.long	0x698
	.long	0x1524
	.uleb128 0xd
	.long	0x44
	.byte	0x9
	.byte	0
	.uleb128 0xb
	.long	0xaf
	.long	0x1534
	.uleb128 0xd
	.long	0x44
	.byte	0x50
	.byte	0
	.uleb128 0xb
	.long	0x2a
	.long	0x1544
	.uleb128 0xd
	.long	0x44
	.byte	0x20
	.byte	0
	.uleb128 0xb
	.long	0x1554
	.long	0x1554
	.uleb128 0xd
	.long	0x44
	.byte	0x3
	.byte	0
	.uleb128 0x4
	.long	0x15ad
	.uleb128 0x9
	.long	.LASF361
	.byte	0x30
	.byte	0xd
	.byte	0xa9
	.long	0x15ad
	.uleb128 0x2
	.long	.LASF362
	.byte	0xd
	.byte	0xaa
	.byte	0x1a
	.long	0xd3
	.byte	0
	.uleb128 0x2
	.long	.LASF363
	.byte	0xd
	.byte	0xab
	.byte	0x1a
	.long	0xa8
	.byte	0x8
	.uleb128 0x2
	.long	.LASF364
	.byte	0xd
	.byte	0xac
	.byte	0x1a
	.long	0x71
	.byte	0x10
	.uleb128 0x2
	.long	.LASF365
	.byte	0xd
	.byte	0xad
	.byte	0x1a
	.long	0x15ee
	.byte	0x18
	.uleb128 0x2
	.long	.LASF366
	.byte	0xd
	.byte	0xae
	.byte	0x1a
	.long	0x111
	.byte	0x20
	.uleb128 0x16
	.long	0x1915
	.byte	0x28
	.byte	0
	.uleb128 0xc
	.long	0x1559
	.uleb128 0xb
	.long	0x2a
	.long	0x15c2
	.uleb128 0xd
	.long	0x44
	.byte	0x50
	.byte	0
	.uleb128 0xb
	.long	0x2a
	.long	0x15d2
	.uleb128 0xd
	.long	0x44
	.byte	0xa0
	.byte	0
	.uleb128 0xb
	.long	0x2a
	.long	0x15e2
	.uleb128 0xd
	.long	0x44
	.byte	0x3f
	.byte	0
	.uleb128 0xa
	.long	.LASF367
	.byte	0xd
	.byte	0x1d
	.byte	0xf
	.long	0x5e5
	.uleb128 0x22
	.long	.LASF412
	.byte	0x8
	.byte	0xd
	.byte	0x1f
	.byte	0x7
	.long	0x1649
	.uleb128 0x6
	.long	.LASF368
	.byte	0x20
	.byte	0x9
	.long	0x71
	.uleb128 0x6
	.long	.LASF369
	.byte	0x21
	.byte	0x12
	.long	0x3d
	.uleb128 0x6
	.long	.LASF370
	.byte	0x22
	.byte	0xa
	.long	0x121
	.uleb128 0x6
	.long	.LASF371
	.byte	0x23
	.byte	0xb
	.long	0xaa
	.uleb128 0x6
	.long	.LASF372
	.byte	0x24
	.byte	0x14
	.long	0x15e
	.uleb128 0x6
	.long	.LASF373
	.byte	0x25
	.byte	0x12
	.long	0x15e2
	.uleb128 0x6
	.long	.LASF374
	.byte	0x26
	.byte	0xb
	.long	0xa8
	.byte	0
	.uleb128 0x9
	.long	.LASF375
	.byte	0x4
	.byte	0xd
	.byte	0x31
	.long	0x1663
	.uleb128 0x2
	.long	.LASF363
	.byte	0xd
	.byte	0x32
	.byte	0x9
	.long	0x71
	.byte	0
	.byte	0
	.uleb128 0xc
	.long	0x1649
	.uleb128 0x9
	.long	.LASF376
	.byte	0x18
	.byte	0xd
	.byte	0x37
	.long	0x16a9
	.uleb128 0x2
	.long	.LASF377
	.byte	0xd
	.byte	0x38
	.byte	0xc
	.long	0x16b9
	.byte	0
	.uleb128 0x2
	.long	.LASF378
	.byte	0xd
	.byte	0x39
	.byte	0x9
	.long	0x71
	.byte	0x8
	.uleb128 0x2
	.long	.LASF379
	.byte	0xd
	.byte	0x3a
	.byte	0x9
	.long	0x71
	.byte	0xc
	.uleb128 0x2
	.long	.LASF380
	.byte	0xd
	.byte	0x3b
	.byte	0x13
	.long	0x111
	.byte	0x10
	.byte	0
	.uleb128 0xc
	.long	0x1668
	.uleb128 0x11
	.long	0x16b9
	.uleb128 0x7
	.long	0x121
	.byte	0
	.uleb128 0x4
	.long	0x16ae
	.uleb128 0x9
	.long	.LASF381
	.byte	0x18
	.byte	0xd
	.byte	0x42
	.long	0x16f2
	.uleb128 0x2
	.long	.LASF382
	.byte	0xd
	.byte	0x43
	.byte	0x11
	.long	0x111
	.byte	0
	.uleb128 0x2
	.long	.LASF383
	.byte	0xd
	.byte	0x44
	.byte	0x11
	.long	0x111
	.byte	0x8
	.uleb128 0x2
	.long	.LASF384
	.byte	0xd
	.byte	0x45
	.byte	0x9
	.long	0x71
	.byte	0x10
	.byte	0
	.uleb128 0xc
	.long	0x16be
	.uleb128 0x9
	.long	.LASF385
	.byte	0x28
	.byte	0xd
	.byte	0x49
	.long	0x175f
	.uleb128 0x2
	.long	.LASF377
	.byte	0xd
	.byte	0x4a
	.byte	0xc
	.long	0x12f
	.byte	0
	.uleb128 0x2
	.long	.LASF386
	.byte	0xd
	.byte	0x4b
	.byte	0xd
	.long	0xbb
	.byte	0x8
	.uleb128 0x2
	.long	.LASF387
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
	.long	.LASF388
	.byte	0xd
	.byte	0x50
	.byte	0x13
	.long	0x1782
	.byte	0x18
	.uleb128 0x2
	.long	.LASF389
	.byte	0xd
	.byte	0x51
	.byte	0xf
	.long	0x179b
	.byte	0x20
	.byte	0
	.uleb128 0xc
	.long	0x16f7
	.uleb128 0x12
	.long	0x111
	.long	0x1782
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
	.long	0x1764
	.uleb128 0x12
	.long	0xc7
	.long	0x179b
	.uleb128 0x7
	.long	0x71
	.uleb128 0x7
	.long	0x71
	.byte	0
	.uleb128 0x4
	.long	0x1787
	.uleb128 0x17
	.byte	0x5b
	.long	0x17bd
	.uleb128 0x6
	.long	.LASF390
	.byte	0x5c
	.byte	0x1f
	.long	0x17bd
	.uleb128 0x6
	.long	.LASF391
	.byte	0x5d
	.byte	0x1e
	.long	0x116
	.byte	0
	.uleb128 0x4
	.long	0x5ea
	.uleb128 0x9
	.long	.LASF392
	.byte	0x20
	.byte	0xd
	.byte	0x55
	.long	0x17fc
	.uleb128 0x2
	.long	.LASF377
	.byte	0xd
	.byte	0x56
	.byte	0xc
	.long	0x12f
	.byte	0
	.uleb128 0x2
	.long	.LASF393
	.byte	0xd
	.byte	0x57
	.byte	0x9
	.long	0x71
	.byte	0x8
	.uleb128 0x2
	.long	.LASF380
	.byte	0xd
	.byte	0x58
	.byte	0x11
	.long	0x111
	.byte	0x10
	.uleb128 0x16
	.long	0x17a0
	.byte	0x18
	.byte	0
	.uleb128 0xc
	.long	0x17c2
	.uleb128 0x9
	.long	.LASF394
	.byte	0x30
	.byte	0xd
	.byte	0x68
	.long	0x1869
	.uleb128 0x2
	.long	.LASF377
	.byte	0xd
	.byte	0x69
	.byte	0xc
	.long	0x12f
	.byte	0
	.uleb128 0x2
	.long	.LASF388
	.byte	0xd
	.byte	0x6a
	.byte	0x13
	.long	0x1782
	.byte	0x8
	.uleb128 0x2
	.long	.LASF389
	.byte	0xd
	.byte	0x6b
	.byte	0xf
	.long	0x179b
	.byte	0x10
	.uleb128 0x2
	.long	.LASF386
	.byte	0xd
	.byte	0x6c
	.byte	0x9
	.long	0x71
	.byte	0x18
	.uleb128 0x2
	.long	.LASF393
	.byte	0xd
	.byte	0x6d
	.byte	0x9
	.long	0x71
	.byte	0x1c
	.uleb128 0x2
	.long	.LASF380
	.byte	0xd
	.byte	0x6e
	.byte	0x11
	.long	0x111
	.byte	0x20
	.uleb128 0x2
	.long	.LASF395
	.byte	0xd
	.byte	0x71
	.byte	0x11
	.long	0x116
	.byte	0x28
	.byte	0
	.uleb128 0xc
	.long	0x1801
	.uleb128 0x9
	.long	.LASF396
	.byte	0x20
	.byte	0xd
	.byte	0x7b
	.long	0x18af
	.uleb128 0x2
	.long	.LASF397
	.byte	0xd
	.byte	0x80
	.byte	0xc
	.long	0x18c4
	.byte	0
	.uleb128 0x2
	.long	.LASF398
	.byte	0xd
	.byte	0x86
	.byte	0xd
	.long	0x18e2
	.byte	0x8
	.uleb128 0x2
	.long	.LASF399
	.byte	0xd
	.byte	0x8c
	.byte	0xc
	.long	0x18fb
	.byte	0x10
	.uleb128 0x2
	.long	.LASF400
	.byte	0xd
	.byte	0x91
	.byte	0xc
	.long	0x1910
	.byte	0x18
	.byte	0
	.uleb128 0xc
	.long	0x186e
	.uleb128 0x11
	.long	0x18c4
	.uleb128 0x7
	.long	0xa8
	.uleb128 0x7
	.long	0xaa
	.byte	0
	.uleb128 0x4
	.long	0x18b4
	.uleb128 0x12
	.long	0xaa
	.long	0x18e2
	.uleb128 0x7
	.long	0xa8
	.uleb128 0x7
	.long	0xaa
	.uleb128 0x7
	.long	0x71
	.byte	0
	.uleb128 0x4
	.long	0x18c9
	.uleb128 0x12
	.long	0x121
	.long	0x18fb
	.uleb128 0x7
	.long	0xa8
	.uleb128 0x7
	.long	0xa8
	.byte	0
	.uleb128 0x4
	.long	0x18e7
	.uleb128 0x11
	.long	0x1910
	.uleb128 0x7
	.long	0xa8
	.uleb128 0x7
	.long	0xa8
	.byte	0
	.uleb128 0x4
	.long	0x1900
	.uleb128 0x17
	.byte	0xb0
	.long	0x197f
	.uleb128 0x6
	.long	.LASF401
	.byte	0xb1
	.byte	0x15
	.long	0x11b
	.uleb128 0x6
	.long	.LASF375
	.byte	0xb2
	.byte	0x25
	.long	0x197f
	.uleb128 0x6
	.long	.LASF376
	.byte	0xb3
	.byte	0x25
	.long	0x1984
	.uleb128 0x6
	.long	.LASF381
	.byte	0xb4
	.byte	0x28
	.long	0x1989
	.uleb128 0x6
	.long	.LASF385
	.byte	0xb5
	.byte	0x23
	.long	0x198e
	.uleb128 0x6
	.long	.LASF392
	.byte	0xb6
	.byte	0x26
	.long	0x1993
	.uleb128 0x6
	.long	.LASF394
	.byte	0xb7
	.byte	0x25
	.long	0x1998
	.uleb128 0x6
	.long	.LASF396
	.byte	0xb8
	.byte	0x26
	.long	0x199d
	.uleb128 0x6
	.long	.LASF380
	.byte	0xb9
	.byte	0x17
	.long	0x111
	.byte	0
	.uleb128 0x4
	.long	0x1663
	.uleb128 0x4
	.long	0x16a9
	.uleb128 0x4
	.long	0x16f2
	.uleb128 0x4
	.long	0x175f
	.uleb128 0x4
	.long	0x17fc
	.uleb128 0x4
	.long	0x1869
	.uleb128 0x4
	.long	0x18af
	.uleb128 0x23
	.long	.LASF413
	.byte	0x1
	.byte	0x19
	.byte	0x5
	.long	0x71
	.quad	.LFB96
	.quad	.LFE96-.LFB96
	.uleb128 0x1
	.byte	0x9c
	.uleb128 0x18
	.long	.LASF402
	.byte	0x20
	.long	0x6ca
	.uleb128 0x18
	.long	.LASF403
	.byte	0x30
	.long	0x7ac
	.uleb128 0xf
	.long	.LASF404
	.value	0x119
	.byte	0x1c
	.long	0x664
	.uleb128 0xf
	.long	.LASF405
	.value	0x11d
	.byte	0x18
	.long	0x698
	.uleb128 0xf
	.long	.LASF406
	.value	0x121
	.byte	0x1c
	.long	0x609
	.uleb128 0xf
	.long	.LASF407
	.value	0x128
	.byte	0x17
	.long	0x5ef
	.uleb128 0xf
	.long	.LASF408
	.value	0x12a
	.byte	0x11
	.long	0x163
	.uleb128 0xf
	.long	.LASF409
	.value	0x168
	.byte	0x15
	.long	0x510
	.uleb128 0xf
	.long	.LASF410
	.value	0x16c
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
.LASF280:
	.string	"warnon_erase_dynplaylist"
.LASF41:
	.string	"layer"
.LASF143:
	.string	"crossfade_fade_out_delay"
.LASF169:
	.string	"rec_editable"
.LASF43:
	.string	"id3version"
.LASF260:
	.string	"colors_file"
.LASF325:
	.string	"lineout_active"
.LASF123:
	.string	"resume_pitch"
.LASF364:
	.string	"lang_id"
.LASF146:
	.string	"crossfade_fade_out_mixmode"
.LASF305:
	.string	"cursor_style"
.LASF57:
	.string	"bytesperframe"
.LASF61:
	.string	"id3v1buf"
.LASF69:
	.string	"playcount"
.LASF370:
	.string	"bool_"
.LASF164:
	.string	"rec_channels"
.LASF275:
	.string	"next_folder"
.LASF323:
	.string	"brightness"
.LASF346:
	.string	"volume_adjust_norm_steps"
.LASF175:
	.string	"rec_directory"
.LASF207:
	.string	"car_adapter_mode_delay"
.LASF149:
	.string	"crossfeed_cross_gain"
.LASF184:
	.string	"rec_trigger_mode"
.LASF399:
	.string	"is_changed"
.LASF36:
	.string	"comment"
.LASF166:
	.string	"rec_left_gain"
.LASF107:
	.string	"ratio"
.LASF90:
	.string	"AA_FLAG_ID3_UNSYNC"
.LASF232:
	.string	"list_separator_height"
.LASF192:
	.string	"unplug_mode"
.LASF347:
	.string	"surround_enabled"
.LASF9:
	.string	"__int32_t"
.LASF160:
	.string	"rec_format"
.LASF237:
	.string	"list_order"
.LASF77:
	.string	"album_peak"
.LASF78:
	.string	"has_embedded_albumart"
.LASF183:
	.string	"rec_stop_gap"
.LASF340:
	.string	"start_directory"
.LASF222:
	.string	"playlist_catalog_dir"
.LASF282:
	.string	"show_shuffled_adding_options"
.LASF148:
	.string	"crossfeed_direct_gain"
.LASF83:
	.string	"mb_track_id"
.LASF182:
	.string	"rec_stop_postrec"
.LASF277:
	.string	"recursive_dir_insert"
.LASF25:
	.string	"__int128"
.LASF7:
	.string	"short int"
.LASF145:
	.string	"crossfade_fade_out_duration"
.LASF339:
	.string	"haptics_enabled"
.LASF46:
	.string	"frequency"
.LASF167:
	.string	"rec_right_gain"
.LASF216:
	.string	"peak_meter_dbfs"
.LASF332:
	.string	"keypress_restarts_sleeptimer"
.LASF310:
	.string	"icon_file"
.LASF344:
	.string	"volume_limit"
.LASF159:
	.string	"timestretch_enabled"
.LASF261:
	.string	"dynamic_colors"
.LASF298:
	.string	"sort_dir"
.LASF411:
	.string	"GNU C99 16.1.1 20260625 -mtune=generic -march=x86-64 -g -Os -std=gnu99 -funit-at-a-time -fno-delete-null-pointer-checks -fno-strict-overflow -fno-common -fno-builtin"
.LASF292:
	.string	"talk_file"
.LASF170:
	.string	"rec_timesplit"
.LASF304:
	.string	"usb_charging"
.LASF210:
	.string	"alarm_wake_up_screen"
.LASF401:
	.string	"RESERVED"
.LASF15:
	.string	"int16_t"
.LASF249:
	.string	"autoresume_automatic"
.LASF106:
	.string	"makeup_gain"
.LASF236:
	.string	"list_wraparound"
.LASF150:
	.string	"crossfeed_hf_attenuation"
.LASF172:
	.string	"rec_split_type"
.LASF126:
	.string	"topruntime"
.LASF263:
	.string	"ipone_lock_wallpaper"
.LASF124:
	.string	"resume_speed"
.LASF28:
	.string	"title"
.LASF363:
	.string	"setting"
.LASF17:
	.string	"uint32_t"
.LASF257:
	.string	"lss_color"
.LASF108:
	.string	"knee"
.LASF171:
	.string	"rec_sizesplit"
.LASF26:
	.string	"__int128 unsigned"
.LASF118:
	.string	"volume"
.LASF178:
	.string	"rec_start_thres_linear"
.LASF198:
	.string	"storage_mode"
.LASF320:
	.string	"bl_selective_actions_mask"
.LASF45:
	.string	"bitrate"
.LASF223:
	.string	"skip_length"
.LASF158:
	.string	"dithering_enabled"
.LASF393:
	.string	"count"
.LASF63:
	.string	"index"
.LASF21:
	.string	"long long unsigned int"
.LASF329:
	.string	"ui_vp_config"
.LASF372:
	.string	"ucharptr"
.LASF135:
	.string	"bass"
.LASF133:
	.string	"user_settings"
.LASF384:
	.string	"max_len"
.LASF100:
	.string	"CHAR_ENC_UTF_16_LE"
.LASF174:
	.string	"rec_prerecord_time"
.LASF32:
	.string	"disc_string"
.LASF359:
	.string	"ui_engine_games_appearance"
.LASF130:
	.string	"last_volume_change"
.LASF119:
	.string	"resume_index"
.LASF66:
	.string	"tagcache_idx"
.LASF141:
	.string	"crossfade"
.LASF162:
	.string	"rec_source"
.LASF50:
	.string	"sim_filesize"
.LASF195:
	.string	"timeformat"
.LASF103:
	.string	"mp3_enc_config"
.LASF336:
	.string	"hotkey_tree"
.LASF39:
	.string	"discnum"
.LASF297:
	.string	"sort_case"
.LASF385:
	.string	"int_setting"
.LASF380:
	.string	"cfg_vals"
.LASF262:
	.string	"ipone_charge_wallpaper"
.LASF92:
	.string	"mp3entry"
.LASF245:
	.string	"usemrb"
.LASF152:
	.string	"eq_enabled"
.LASF154:
	.string	"eq_band_settings"
.LASF226:
	.string	"volume_type"
.LASF112:
	.string	"noclip"
.LASF58:
	.string	"has_toc"
.LASF337:
	.string	"resume_rewind"
.LASF137:
	.string	"channel_config"
.LASF127:
	.string	"last_frequency"
.LASF252:
	.string	"tagcache_scan_paths"
.LASF308:
	.string	"offset_out_of_view"
.LASF360:
	.string	"ui_engine_extras_pane"
.LASF355:
	.string	"clear_settings_on_hold"
.LASF382:
	.string	"prefix"
.LASF201:
	.string	"default_codepage"
.LASF20:
	.string	"size_t"
.LASF33:
	.string	"track_string"
.LASF62:
	.string	"offset"
.LASF243:
	.string	"autocreatebookmark"
.LASF161:
	.string	"rec_mono_mode"
.LASF316:
	.string	"caption_backlight"
.LASF373:
	.string	"func"
.LASF291:
	.string	"talk_dir_clip"
.LASF23:
	.string	"_Bool"
.LASF125:
	.string	"runtime"
.LASF89:
	.string	"AA_TYPE_JPG"
.LASF134:
	.string	"balance"
.LASF352:
	.string	"surround_mix"
.LASF49:
	.string	"first_frame_offset"
.LASF53:
	.string	"lead_trim"
.LASF227:
	.string	"battery_display"
.LASF136:
	.string	"treble"
.LASF362:
	.string	"flags"
.LASF142:
	.string	"crossfade_fade_in_delay"
.LASF79:
	.string	"albumart"
.LASF59:
	.string	"needs_upsampling_correction"
.LASF27:
	.string	"path"
.LASF228:
	.string	"show_icons"
.LASF296:
	.string	"talk_mixer_amp"
.LASF398:
	.string	"write_to_cfg"
.LASF48:
	.string	"id3v1len"
.LASF345:
	.string	"volume_adjust_mode"
.LASF285:
	.string	"rewind_across_tracks"
.LASF322:
	.string	"lcd_sleep_after_backlight_off"
.LASF116:
	.string	"gain"
.LASF38:
	.string	"grouping"
.LASF396:
	.string	"custom_setting"
.LASF115:
	.string	"cutoff"
.LASF93:
	.string	"mp3_albumart"
.LASF317:
	.string	"bl_filter_first_keypress"
.LASF374:
	.string	"custom"
.LASF306:
	.string	"screen_scroll_step"
.LASF286:
	.string	"playlist_viewer_icons"
.LASF258:
	.string	"lse_color"
.LASF266:
	.string	"ui_engine_accent"
.LASF14:
	.string	"char"
.LASF153:
	.string	"eq_precut"
.LASF196:
	.string	"disk_spindown"
.LASF64:
	.string	"skip_resume_adjustments"
.LASF102:
	.string	"encoding"
.LASF81:
	.string	"embedded_cuesheet"
.LASF281:
	.string	"keep_current_track_on_replace_playlist"
.LASF254:
	.string	"backdrop_file"
.LASF410:
	.string	"section_10"
.LASF217:
	.string	"peak_meter_min"
.LASF12:
	.string	"__uint64_t"
.LASF4:
	.string	"unsigned int"
.LASF97:
	.string	"character_encoding"
.LASF307:
	.string	"show_path_in_browser"
.LASF234:
	.string	"browse_current"
.LASF205:
	.string	"party_mode"
.LASF302:
	.string	"poweroff"
.LASF278:
	.string	"fade_on_stop"
.LASF392:
	.string	"choice_setting"
.LASF248:
	.string	"autoresume_enable"
.LASF284:
	.string	"album_art"
.LASF358:
	.string	"ui_engine_video_appearance"
.LASF186:
	.string	"fm_region"
.LASF197:
	.string	"buffer_margin"
.LASF139:
	.string	"bass_cutoff"
.LASF121:
	.string	"resume_elapsed"
.LASF180:
	.string	"rec_stop_thres_db"
.LASF129:
	.string	"viewer_icon_count"
.LASF341:
	.string	"root_menu_customized"
.LASF86:
	.string	"AA_TYPE_UNKNOWN"
.LASF368:
	.string	"int_"
.LASF326:
	.string	"prevent_skip"
.LASF200:
	.string	"show_filename_ext"
.LASF187:
	.string	"fm_force_mono"
.LASF68:
	.string	"score"
.LASF350:
	.string	"surround_fx2"
.LASF55:
	.string	"samples"
.LASF40:
	.string	"tracknum"
.LASF233:
	.string	"list_separator_color"
.LASF22:
	.string	"long long int"
.LASF98:
	.string	"CHAR_ENC_ISO_8859_1"
.LASF412:
	.string	"storage_type"
.LASF151:
	.string	"crossfeed_hf_cutoff"
.LASF271:
	.string	"ui_engine_dark_mode"
.LASF191:
	.string	"pause_rewind"
.LASF402:
	.string	"section_1"
.LASF403:
	.string	"section_2"
.LASF404:
	.string	"section_3"
.LASF405:
	.string	"section_4"
.LASF406:
	.string	"section_5"
.LASF407:
	.string	"section_6"
.LASF408:
	.string	"section_8"
.LASF409:
	.string	"section_9"
.LASF394:
	.string	"table_setting"
.LASF44:
	.string	"codectype"
.LASF109:
	.string	"release_time"
.LASF189:
	.string	"fms_file"
.LASF8:
	.string	"__int16_t"
.LASF238:
	.string	"scroll_speed"
.LASF276:
	.string	"constrain_next_folder"
.LASF379:
	.string	"lang_no"
.LASF389:
	.string	"get_talk_id"
.LASF47:
	.string	"id3v2len"
.LASF82:
	.string	"cuesheet"
.LASF268:
	.string	"ui_engine_font_scale"
.LASF114:
	.string	"eq_band_setting"
.LASF231:
	.string	"scrollbar_width"
.LASF387:
	.string	"step"
.LASF54:
	.string	"tail_trim"
.LASF95:
	.string	"size"
.LASF256:
	.string	"fg_color"
.LASF397:
	.string	"load_from_cfg"
.LASF343:
	.string	"play_frequency"
.LASF220:
	.string	"sbs_file"
.LASF67:
	.string	"rating"
.LASF301:
	.string	"interpret_numbers"
.LASF388:
	.string	"formatter"
.LASF74:
	.string	"track_gain"
.LASF173:
	.string	"rec_split_method"
.LASF395:
	.string	"values"
.LASF144:
	.string	"crossfade_fade_in_duration"
.LASF113:
	.string	"preamp"
.LASF101:
	.string	"CHAR_ENC_UTF_16_BE"
.LASF85:
	.string	"has_video"
.LASF88:
	.string	"AA_TYPE_PNG"
.LASF354:
	.string	"afr_enabled"
.LASF386:
	.string	"unit"
.LASF357:
	.string	"ui_engine_lock_settings"
.LASF163:
	.string	"rec_frequency"
.LASF400:
	.string	"set_default"
.LASF250:
	.string	"autoresume_paths"
.LASF272:
	.string	"album_list_layout"
.LASF34:
	.string	"year_string"
.LASF60:
	.string	"id3v2buf"
.LASF391:
	.string	"talks"
.LASF31:
	.string	"genre_string"
.LASF361:
	.string	"settings_list"
.LASF35:
	.string	"composer"
.LASF206:
	.string	"car_adapter_mode"
.LASF221:
	.string	"lang_file"
.LASF138:
	.string	"stereo_width"
.LASF377:
	.string	"option_callback"
.LASF73:
	.string	"album_level"
.LASF327:
	.string	"pitch_mode_semitone"
.LASF204:
	.string	"single_mode"
.LASF199:
	.string	"dirfilter"
.LASF269:
	.string	"ui_engine_surface"
.LASF168:
	.string	"peak_meter_clipcounter"
.LASF76:
	.string	"track_peak"
.LASF287:
	.string	"playlist_viewer_indices"
.LASF383:
	.string	"suffix"
.LASF334:
	.string	"morse_input"
.LASF177:
	.string	"rec_start_thres_db"
.LASF117:
	.string	"system_status"
.LASF84:
	.string	"is_asf_stream"
.LASF338:
	.string	"keyclick_hardware"
.LASF132:
	.string	"resume_modified"
.LASF309:
	.string	"disable_mainmenu_scrolling"
.LASF290:
	.string	"talk_dir"
.LASF11:
	.string	"long int"
.LASF110:
	.string	"attack_time"
.LASF342:
	.string	"shortcuts_replaces_qs"
.LASF244:
	.string	"autoupdatebookmark"
.LASF51:
	.string	"length"
.LASF318:
	.string	"backlight_timeout_plugged"
.LASF300:
	.string	"sort_playlists"
.LASF295:
	.string	"talk_battery_level"
.LASF242:
	.string	"autoloadbookmark"
.LASF56:
	.string	"frame_count"
.LASF96:
	.string	"mp3_aa_type"
.LASF211:
	.string	"ff_rewind_min_step"
.LASF71:
	.string	"playtime"
.LASF366:
	.string	"cfg_name"
.LASF219:
	.string	"wps_file"
.LASF18:
	.string	"uint64_t"
.LASF299:
	.string	"sort_file"
.LASF122:
	.string	"resume_offset"
.LASF313:
	.string	"glyphs_to_cache"
.LASF312:
	.string	"font_file"
.LASF371:
	.string	"charptr"
.LASF176:
	.string	"cliplight"
.LASF246:
	.string	"tagcache_ram"
.LASF225:
	.string	"max_files_in_playlist"
.LASF289:
	.string	"talk_menu"
.LASF190:
	.string	"sync_rds_time"
.LASF274:
	.string	"repeat_mode"
.LASF224:
	.string	"max_files_in_dir"
.LASF251:
	.string	"runtimedb"
.LASF29:
	.string	"artist"
.LASF259:
	.string	"lst_color"
.LASF194:
	.string	"qs_items"
.LASF179:
	.string	"rec_start_duration"
.LASF213:
	.string	"peak_meter_release"
.LASF348:
	.string	"surround_balance"
.LASF330:
	.string	"sleeptimer_duration"
.LASF319:
	.string	"bl_selective_actions"
.LASF294:
	.string	"talk_filetype"
.LASF30:
	.string	"album"
.LASF230:
	.string	"scrollbar"
.LASF5:
	.string	"long unsigned int"
.LASF315:
	.string	"backlight_timeout"
.LASF208:
	.string	"start_in_screen"
.LASF212:
	.string	"ff_rewind_accel"
.LASF288:
	.string	"playlist_viewer_track_display"
.LASF255:
	.string	"bg_color"
.LASF328:
	.string	"pitch_mode_timestretch"
.LASF267:
	.string	"ui_engine_density"
.LASF42:
	.string	"year"
.LASF99:
	.string	"CHAR_ENC_UTF_8"
.LASF324:
	.string	"accessory_supply"
.LASF293:
	.string	"talk_file_clip"
.LASF120:
	.string	"resume_crc32"
.LASF239:
	.string	"bidir_limit"
.LASF349:
	.string	"surround_fx1"
.LASF94:
	.string	"type"
.LASF131:
	.string	"font_id"
.LASF2:
	.string	"unsigned char"
.LASF72:
	.string	"track_level"
.LASF10:
	.string	"__uint32_t"
.LASF104:
	.string	"compressor_settings"
.LASF181:
	.string	"rec_stop_thres_linear"
.LASF75:
	.string	"album_gain"
.LASF235:
	.string	"scroll_paginated"
.LASF265:
	.string	"ui_engine"
.LASF185:
	.string	"rec_trigger_type"
.LASF203:
	.string	"play_selected"
.LASF375:
	.string	"sound_setting"
.LASF37:
	.string	"albumartist"
.LASF356:
	.string	"playback_log"
.LASF264:
	.string	"ipone_right_pane"
.LASF311:
	.string	"viewers_icon_file"
.LASF188:
	.string	"fmr_file"
.LASF365:
	.string	"default_val"
.LASF105:
	.string	"threshold"
.LASF367:
	.string	"_isfunc_type"
.LASF376:
	.string	"bool_setting"
.LASF229:
	.string	"statusbar"
.LASF111:
	.string	"replaygain_settings"
.LASF13:
	.string	"__off_t"
.LASF147:
	.string	"crossfeed"
.LASF155:
	.string	"beep"
.LASF128:
	.string	"last_screen"
.LASF331:
	.string	"sleeptimer_on_startup"
.LASF279:
	.string	"playlist_shuffle"
.LASF6:
	.string	"signed char"
.LASF19:
	.string	"off_t"
.LASF3:
	.string	"short unsigned int"
.LASF335:
	.string	"hotkey_wps"
.LASF157:
	.string	"keyclick_repeats"
.LASF333:
	.string	"show_shutdown_message"
.LASF413:
	.string	"main"
.LASF253:
	.string	"tagcache_db_path"
.LASF218:
	.string	"peak_meter_max"
.LASF283:
	.string	"show_queue_options"
.LASF247:
	.string	"tagcache_autoupdate"
.LASF353:
	.string	"pbe_precut"
.LASF140:
	.string	"treble_cutoff"
.LASF65:
	.string	"autoresumable"
.LASF52:
	.string	"elapsed"
.LASF80:
	.string	"has_embedded_cuesheet"
.LASF24:
	.string	"double"
.LASF273:
	.string	"browser_default"
.LASF91:
	.string	"AA_FLAG_VORBIS_BASE64"
.LASF321:
	.string	"backlight_on_button_hold"
.LASF240:
	.string	"scroll_delay"
.LASF16:
	.string	"int32_t"
.LASF209:
	.string	"wps_select_action"
.LASF314:
	.string	"kbd_file"
.LASF70:
	.string	"lastplayed"
.LASF390:
	.string	"desc"
.LASF303:
	.string	"battery_capacity"
.LASF369:
	.string	"uint_"
.LASF193:
	.string	"unplug_autoresume"
.LASF87:
	.string	"AA_TYPE_BMP"
.LASF351:
	.string	"surround_method2"
.LASF214:
	.string	"peak_meter_hold"
.LASF202:
	.string	"hold_lr_for_scroll_in_list"
.LASF215:
	.string	"peak_meter_clip_hold"
.LASF156:
	.string	"keyclick"
.LASF378:
	.string	"lang_yes"
.LASF381:
	.string	"filename_setting"
.LASF270:
	.string	"ui_engine_hold_effect"
.LASF241:
	.string	"scroll_step"
.LASF165:
	.string	"rec_mic_gain"
	.section	.debug_line_str,"MS",@progbits,1
.LASF0:
	.string	"<stdin>"
.LASF1:
	.string	"/home/david/Documents/RockBox_Personal-master/build-sim-video-5g"
	.ident	"GCC: (GNU) 16.1.1 20260625"
	.section	.note.GNU-stack,"",@progbits
