	.file	"<stdin>"
	.text
.Ltext0:
	.file 0 "/home/david/Documents/RockBox_Personal-master/build-sim" "<stdin>"
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
	/* "struct user_settings", "crossfade" = $20, $4, "i", $0; */

# 0 "" 2
	.loc 1 54 5 view .LVU24
# 54 "<stdin>" 1
	/* "struct user_settings", "crossfade_fade_in_delay" = $24, $4, "i", $0; */

# 0 "" 2
	.loc 1 55 5 view .LVU25
# 55 "<stdin>" 1
	/* "struct user_settings", "crossfade_fade_out_delay" = $28, $4, "i", $0; */

# 0 "" 2
	.loc 1 56 5 view .LVU26
# 56 "<stdin>" 1
	/* "struct user_settings", "crossfade_fade_in_duration" = $32, $4, "i", $0; */

# 0 "" 2
	.loc 1 57 5 view .LVU27
# 57 "<stdin>" 1
	/* "struct user_settings", "crossfade_fade_out_duration" = $36, $4, "i", $0; */

# 0 "" 2
	.loc 1 58 5 view .LVU28
# 58 "<stdin>" 1
	/* "struct user_settings", "crossfade_fade_out_mixmode" = $40, $4, "i", $0; */

# 0 "" 2
	.loc 1 59 5 view .LVU29
# 59 "<stdin>" 1
	/* "struct user_settings", "replaygain_settings" = $44, $12, "s_replaygain_settings", $0; */

# 0 "" 2
	.loc 1 60 5 view .LVU30
# 60 "<stdin>" 1
	/* "struct user_settings", "crossfeed" = $56, $4, "i", $0; */

# 0 "" 2
	.loc 1 61 5 view .LVU31
# 61 "<stdin>" 1
	/* "struct user_settings", "crossfeed_direct_gain" = $60, $4, "u_i", $0; */

# 0 "" 2
	.loc 1 62 5 view .LVU32
# 62 "<stdin>" 1
	/* "struct user_settings", "crossfeed_cross_gain" = $64, $4, "u_i", $0; */

# 0 "" 2
	.loc 1 63 5 view .LVU33
# 63 "<stdin>" 1
	/* "struct user_settings", "crossfeed_hf_attenuation" = $68, $4, "u_i", $0; */

# 0 "" 2
	.loc 1 64 5 view .LVU34
# 64 "<stdin>" 1
	/* "struct user_settings", "crossfeed_hf_cutoff" = $72, $4, "u_i", $0; */

# 0 "" 2
	.loc 1 65 5 view .LVU35
# 65 "<stdin>" 1
	/* "struct user_settings", "eq_enabled" = $76, $1, "b", $0; */

# 0 "" 2
	.loc 1 66 5 view .LVU36
# 66 "<stdin>" 1
	/* "struct user_settings", "eq_precut" = $80, $4, "u_i", $0; */

# 0 "" 2
	.loc 1 67 5 view .LVU37
# 67 "<stdin>" 1
	/* "struct user_settings", "eq_band_settings" = $84, $120, "s_eq_band_setting_typeisarray_", $10; */

# 0 "" 2
	.loc 1 68 5 view .LVU38
# 68 "<stdin>" 1
	/* "struct user_settings", "beep" = $204, $4, "i", $0; */

# 0 "" 2
	.loc 1 69 5 view .LVU39
# 69 "<stdin>" 1
	/* "struct user_settings", "keyclick" = $208, $4, "i", $0; */

# 0 "" 2
	.loc 1 70 5 view .LVU40
# 70 "<stdin>" 1
	/* "struct user_settings", "keyclick_repeats" = $212, $4, "i", $0; */

# 0 "" 2
	.loc 1 71 5 view .LVU41
# 71 "<stdin>" 1
	/* "struct user_settings", "dithering_enabled" = $216, $1, "b", $0; */

# 0 "" 2
	.loc 1 72 5 view .LVU42
# 72 "<stdin>" 1
	/* "struct user_settings", "timestretch_enabled" = $217, $1, "b", $0; */

# 0 "" 2
	.loc 1 73 5 view .LVU43
# 73 "<stdin>" 1
	/* "struct user_settings", "pause_rewind" = $220, $4, "i", $0; */

# 0 "" 2
	.loc 1 74 5 view .LVU44
# 74 "<stdin>" 1
	/* "struct user_settings", "unplug_mode" = $224, $4, "i", $0; */

# 0 "" 2
	.loc 1 75 5 view .LVU45
# 75 "<stdin>" 1
	/* "struct user_settings", "unplug_autoresume" = $228, $1, "b", $0; */

# 0 "" 2
	.loc 1 76 5 view .LVU46
# 76 "<stdin>" 1
	/* "struct user_settings", "qs_items" = $232, $32, "ptr_const_struct_typeisarray_settings_list", $0; */

# 0 "" 2
	.loc 1 77 5 view .LVU47
# 77 "<stdin>" 1
	/* "struct user_settings", "timeformat" = $264, $4, "i", $0; */

# 0 "" 2
	.loc 1 78 5 view .LVU48
# 78 "<stdin>" 1
	/* "struct user_settings", "disk_spindown" = $268, $4, "i", $0; */

# 0 "" 2
	.loc 1 79 5 view .LVU49
# 79 "<stdin>" 1
	/* "struct user_settings", "buffer_margin" = $272, $4, "i", $0; */

# 0 "" 2
	.loc 1 80 5 view .LVU50
# 80 "<stdin>" 1
	/* "struct user_settings", "storage_mode" = $276, $4, "i", $0; */

# 0 "" 2
	.loc 1 81 5 view .LVU51
# 81 "<stdin>" 1
	/* "struct user_settings", "dirfilter" = $280, $4, "i", $0; */

# 0 "" 2
	.loc 1 82 5 view .LVU52
# 82 "<stdin>" 1
	/* "struct user_settings", "show_filename_ext" = $284, $4, "i", $0; */

# 0 "" 2
	.loc 1 83 5 view .LVU53
# 83 "<stdin>" 1
	/* "struct user_settings", "default_codepage" = $288, $4, "i", $0; */

# 0 "" 2
	.loc 1 84 5 view .LVU54
# 84 "<stdin>" 1
	/* "struct user_settings", "hold_lr_for_scroll_in_list" = $292, $1, "b", $0; */

# 0 "" 2
	.loc 1 85 5 view .LVU55
# 85 "<stdin>" 1
	/* "struct user_settings", "play_selected" = $293, $1, "b", $0; */

# 0 "" 2
	.loc 1 86 5 view .LVU56
# 86 "<stdin>" 1
	/* "struct user_settings", "single_mode" = $296, $4, "i", $0; */

# 0 "" 2
	.loc 1 87 5 view .LVU57
# 87 "<stdin>" 1
	/* "struct user_settings", "party_mode" = $300, $1, "b", $0; */

# 0 "" 2
	.loc 1 88 5 view .LVU58
# 88 "<stdin>" 1
	/* "struct user_settings", "cuesheet" = $301, $1, "b", $0; */

# 0 "" 2
	.loc 1 89 5 view .LVU59
# 89 "<stdin>" 1
	/* "struct user_settings", "car_adapter_mode" = $302, $1, "b", $0; */

# 0 "" 2
	.loc 1 90 5 view .LVU60
# 90 "<stdin>" 1
	/* "struct user_settings", "car_adapter_mode_delay" = $304, $4, "i", $0; */

# 0 "" 2
	.loc 1 91 5 view .LVU61
# 91 "<stdin>" 1
	/* "struct user_settings", "start_in_screen" = $308, $4, "i", $0; */

# 0 "" 2
	.loc 1 92 5 view .LVU62
# 92 "<stdin>" 1
	/* "struct user_settings", "wps_select_action" = $312, $4, "i", $0; */

# 0 "" 2
	.loc 1 93 5 view .LVU63
# 93 "<stdin>" 1
	/* "struct user_settings", "ff_rewind_min_step" = $316, $4, "i", $0; */

# 0 "" 2
	.loc 1 94 5 view .LVU64
# 94 "<stdin>" 1
	/* "struct user_settings", "ff_rewind_accel" = $320, $4, "i", $0; */

# 0 "" 2
	.loc 1 95 5 view .LVU65
# 95 "<stdin>" 1
	/* "struct user_settings", "peak_meter_release" = $324, $4, "i", $0; */

# 0 "" 2
	.loc 1 96 5 view .LVU66
# 96 "<stdin>" 1
	/* "struct user_settings", "peak_meter_hold" = $328, $4, "i", $0; */

# 0 "" 2
	.loc 1 97 5 view .LVU67
# 97 "<stdin>" 1
	/* "struct user_settings", "peak_meter_clip_hold" = $332, $4, "i", $0; */

# 0 "" 2
	.loc 1 98 5 view .LVU68
# 98 "<stdin>" 1
	/* "struct user_settings", "peak_meter_dbfs" = $336, $1, "b", $0; */

# 0 "" 2
	.loc 1 99 5 view .LVU69
# 99 "<stdin>" 1
	/* "struct user_settings", "peak_meter_min" = $340, $4, "i", $0; */

# 0 "" 2
	.loc 1 100 5 view .LVU70
# 100 "<stdin>" 1
	/* "struct user_settings", "peak_meter_max" = $344, $4, "i", $0; */

# 0 "" 2
	.loc 1 101 5 view .LVU71
# 101 "<stdin>" 1
	/* "struct user_settings", "wps_file" = $348, $33, "str", $0; */

# 0 "" 2
	.loc 1 102 5 view .LVU72
# 102 "<stdin>" 1
	/* "struct user_settings", "sbs_file" = $381, $33, "str", $0; */

# 0 "" 2
	.loc 1 103 5 view .LVU73
# 103 "<stdin>" 1
	/* "struct user_settings", "lang_file" = $414, $33, "str", $0; */

# 0 "" 2
	.loc 1 104 5 view .LVU74
# 104 "<stdin>" 1
	/* "struct user_settings", "playlist_catalog_dir" = $447, $81, "str", $0; */

# 0 "" 2
	.loc 1 105 5 view .LVU75
# 105 "<stdin>" 1
	/* "struct user_settings", "skip_length" = $528, $4, "i", $0; */

# 0 "" 2
	.loc 1 106 5 view .LVU76
# 106 "<stdin>" 1
	/* "struct user_settings", "max_files_in_dir" = $532, $4, "i", $0; */

# 0 "" 2
	.loc 1 107 5 view .LVU77
# 107 "<stdin>" 1
	/* "struct user_settings", "max_files_in_playlist" = $536, $4, "i", $0; */

# 0 "" 2
	.loc 1 108 5 view .LVU78
# 108 "<stdin>" 1
	/* "struct user_settings", "volume_type" = $540, $4, "i", $0; */

# 0 "" 2
	.loc 1 109 5 view .LVU79
# 109 "<stdin>" 1
	/* "struct user_settings", "battery_display" = $544, $4, "i", $0; */

# 0 "" 2
	.loc 1 110 5 view .LVU80
# 110 "<stdin>" 1
	/* "struct user_settings", "show_icons" = $548, $1, "b", $0; */

# 0 "" 2
	.loc 1 111 5 view .LVU81
# 111 "<stdin>" 1
	/* "struct user_settings", "statusbar" = $552, $4, "i", $0; */

# 0 "" 2
	.loc 1 112 5 view .LVU82
# 112 "<stdin>" 1
	/* "struct user_settings", "scrollbar" = $556, $4, "i", $0; */

# 0 "" 2
	.loc 1 113 5 view .LVU83
# 113 "<stdin>" 1
	/* "struct user_settings", "scrollbar_width" = $560, $4, "i", $0; */

# 0 "" 2
	.loc 1 114 5 view .LVU84
# 114 "<stdin>" 1
	/* "struct user_settings", "list_separator_height" = $564, $4, "i", $0; */

# 0 "" 2
	.loc 1 115 5 view .LVU85
# 115 "<stdin>" 1
	/* "struct user_settings", "list_separator_color" = $568, $4, "i", $0; */

# 0 "" 2
	.loc 1 116 5 view .LVU86
# 116 "<stdin>" 1
	/* "struct user_settings", "browse_current" = $572, $1, "b", $0; */

# 0 "" 2
	.loc 1 117 5 view .LVU87
# 117 "<stdin>" 1
	/* "struct user_settings", "scroll_paginated" = $573, $1, "b", $0; */

# 0 "" 2
	.loc 1 118 5 view .LVU88
# 118 "<stdin>" 1
	/* "struct user_settings", "list_wraparound" = $574, $1, "b", $0; */

# 0 "" 2
	.loc 1 119 5 view .LVU89
# 119 "<stdin>" 1
	/* "struct user_settings", "list_order" = $576, $4, "i", $0; */

# 0 "" 2
	.loc 1 120 5 view .LVU90
# 120 "<stdin>" 1
	/* "struct user_settings", "scroll_speed" = $580, $4, "i", $0; */

# 0 "" 2
	.loc 1 121 5 view .LVU91
# 121 "<stdin>" 1
	/* "struct user_settings", "bidir_limit" = $584, $4, "i", $0; */

# 0 "" 2
	.loc 1 122 5 view .LVU92
# 122 "<stdin>" 1
	/* "struct user_settings", "scroll_delay" = $588, $4, "i", $0; */

# 0 "" 2
	.loc 1 123 5 view .LVU93
# 123 "<stdin>" 1
	/* "struct user_settings", "scroll_step" = $592, $4, "i", $0; */

# 0 "" 2
	.loc 1 124 5 view .LVU94
# 124 "<stdin>" 1
	/* "struct user_settings", "autoloadbookmark" = $596, $4, "i", $0; */

# 0 "" 2
	.loc 1 125 5 view .LVU95
# 125 "<stdin>" 1
	/* "struct user_settings", "autocreatebookmark" = $600, $4, "i", $0; */

# 0 "" 2
	.loc 1 126 5 view .LVU96
# 126 "<stdin>" 1
	/* "struct user_settings", "autoupdatebookmark" = $604, $1, "b", $0; */

# 0 "" 2
	.loc 1 127 5 view .LVU97
# 127 "<stdin>" 1
	/* "struct user_settings", "usemrb" = $608, $4, "i", $0; */

# 0 "" 2
	.loc 1 128 5 view .LVU98
# 128 "<stdin>" 1
	/* "struct user_settings", "tagcache_ram" = $612, $4, "i", $0; */

# 0 "" 2
	.loc 1 129 5 view .LVU99
# 129 "<stdin>" 1
	/* "struct user_settings", "tagcache_autoupdate" = $616, $1, "b", $0; */

# 0 "" 2
	.loc 1 130 5 view .LVU100
# 130 "<stdin>" 1
	/* "struct user_settings", "autoresume_enable" = $617, $1, "b", $0; */

# 0 "" 2
	.loc 1 131 5 view .LVU101
# 131 "<stdin>" 1
	/* "struct user_settings", "autoresume_automatic" = $620, $4, "i", $0; */

# 0 "" 2
	.loc 1 132 5 view .LVU102
# 132 "<stdin>" 1
	/* "struct user_settings", "autoresume_paths" = $624, $161, "str", $0; */

# 0 "" 2
	.loc 1 133 5 view .LVU103
# 133 "<stdin>" 1
	/* "struct user_settings", "runtimedb" = $785, $1, "b", $0; */

# 0 "" 2
	.loc 1 134 5 view .LVU104
# 134 "<stdin>" 1
	/* "struct user_settings", "tagcache_scan_paths" = $786, $161, "str", $0; */

# 0 "" 2
	.loc 1 135 5 view .LVU105
# 135 "<stdin>" 1
	/* "struct user_settings", "tagcache_db_path" = $947, $81, "str", $0; */

# 0 "" 2
	.loc 1 136 5 view .LVU106
# 136 "<stdin>" 1
	/* "struct user_settings", "ipone_charge_wallpaper" = $1028, $4, "i", $0; */

# 0 "" 2
	.loc 1 137 5 view .LVU107
# 137 "<stdin>" 1
	/* "struct user_settings", "backdrop_file" = $1032, $81, "str", $0; */

# 0 "" 2
	.loc 1 138 5 view .LVU108
# 138 "<stdin>" 1
	/* "struct user_settings", "browser_default" = $1116, $4, "i", $0; */

# 0 "" 2
	.loc 1 139 5 view .LVU109
# 139 "<stdin>" 1
	/* "struct user_settings", "repeat_mode" = $1120, $4, "i", $0; */

# 0 "" 2
	.loc 1 140 5 view .LVU110
# 140 "<stdin>" 1
	/* "struct user_settings", "next_folder" = $1124, $4, "i", $0; */

# 0 "" 2
	.loc 1 141 5 view .LVU111
# 141 "<stdin>" 1
	/* "struct user_settings", "constrain_next_folder" = $1128, $1, "b", $0; */

# 0 "" 2
	.loc 1 142 5 view .LVU112
# 142 "<stdin>" 1
	/* "struct user_settings", "recursive_dir_insert" = $1132, $4, "i", $0; */

# 0 "" 2
	.loc 1 143 5 view .LVU113
# 143 "<stdin>" 1
	/* "struct user_settings", "fade_on_stop" = $1136, $1, "b", $0; */

# 0 "" 2
	.loc 1 144 5 view .LVU114
# 144 "<stdin>" 1
	/* "struct user_settings", "playlist_shuffle" = $1137, $1, "b", $0; */

# 0 "" 2
	.loc 1 145 5 view .LVU115
# 145 "<stdin>" 1
	/* "struct user_settings", "warnon_erase_dynplaylist" = $1138, $1, "b", $0; */

# 0 "" 2
	.loc 1 146 5 view .LVU116
# 146 "<stdin>" 1
	/* "struct user_settings", "keep_current_track_on_replace_playlist" = $1139, $1, "b", $0; */

# 0 "" 2
	.loc 1 147 5 view .LVU117
# 147 "<stdin>" 1
	/* "struct user_settings", "show_shuffled_adding_options" = $1140, $1, "b", $0; */

# 0 "" 2
	.loc 1 148 5 view .LVU118
# 148 "<stdin>" 1
	/* "struct user_settings", "show_queue_options" = $1144, $4, "i", $0; */

# 0 "" 2
	.loc 1 149 5 view .LVU119
# 149 "<stdin>" 1
	/* "struct user_settings", "album_art" = $1148, $4, "i", $0; */

# 0 "" 2
	.loc 1 150 5 view .LVU120
# 150 "<stdin>" 1
	/* "struct user_settings", "rewind_across_tracks" = $1152, $1, "b", $0; */

# 0 "" 2
	.loc 1 151 5 view .LVU121
# 151 "<stdin>" 1
	/* "struct user_settings", "playlist_viewer_icons" = $1153, $1, "b", $0; */

# 0 "" 2
	.loc 1 152 5 view .LVU122
# 152 "<stdin>" 1
	/* "struct user_settings", "playlist_viewer_indices" = $1154, $1, "b", $0; */

# 0 "" 2
	.loc 1 153 5 view .LVU123
# 153 "<stdin>" 1
	/* "struct user_settings", "playlist_viewer_track_display" = $1156, $4, "i", $0; */

# 0 "" 2
	.loc 1 154 5 view .LVU124
# 154 "<stdin>" 1
	/* "struct user_settings", "talk_menu" = $1160, $1, "b", $0; */

# 0 "" 2
	.loc 1 155 5 view .LVU125
# 155 "<stdin>" 1
	/* "struct user_settings", "talk_dir" = $1164, $4, "i", $0; */

# 0 "" 2
	.loc 1 156 5 view .LVU126
# 156 "<stdin>" 1
	/* "struct user_settings", "talk_dir_clip" = $1168, $1, "b", $0; */

# 0 "" 2
	.loc 1 157 5 view .LVU127
# 157 "<stdin>" 1
	/* "struct user_settings", "talk_file" = $1172, $4, "i", $0; */

# 0 "" 2
	.loc 1 158 5 view .LVU128
# 158 "<stdin>" 1
	/* "struct user_settings", "talk_file_clip" = $1176, $1, "b", $0; */

# 0 "" 2
	.loc 1 159 5 view .LVU129
# 159 "<stdin>" 1
	/* "struct user_settings", "talk_filetype" = $1177, $1, "b", $0; */

# 0 "" 2
	.loc 1 160 5 view .LVU130
# 160 "<stdin>" 1
	/* "struct user_settings", "talk_battery_level" = $1178, $1, "b", $0; */

# 0 "" 2
	.loc 1 161 5 view .LVU131
# 161 "<stdin>" 1
	/* "struct user_settings", "talk_mixer_amp" = $1180, $4, "i", $0; */

# 0 "" 2
	.loc 1 162 5 view .LVU132
# 162 "<stdin>" 1
	/* "struct user_settings", "sort_case" = $1184, $1, "b", $0; */

# 0 "" 2
	.loc 1 163 5 view .LVU133
# 163 "<stdin>" 1
	/* "struct user_settings", "sort_dir" = $1188, $4, "i", $0; */

# 0 "" 2
	.loc 1 164 5 view .LVU134
# 164 "<stdin>" 1
	/* "struct user_settings", "sort_file" = $1192, $4, "i", $0; */

# 0 "" 2
	.loc 1 165 5 view .LVU135
# 165 "<stdin>" 1
	/* "struct user_settings", "sort_playlists" = $1196, $4, "i", $0; */

# 0 "" 2
	.loc 1 166 5 view .LVU136
# 166 "<stdin>" 1
	/* "struct user_settings", "interpret_numbers" = $1200, $4, "i", $0; */

# 0 "" 2
	.loc 1 167 5 view .LVU137
# 167 "<stdin>" 1
	/* "struct user_settings", "poweroff" = $1204, $4, "i", $0; */

# 0 "" 2
	.loc 1 168 5 view .LVU138
# 168 "<stdin>" 1
	/* "struct user_settings", "battery_capacity" = $1208, $4, "i", $0; */

# 0 "" 2
	.loc 1 169 5 view .LVU139
# 169 "<stdin>" 1
	/* "struct user_settings", "contrast" = $1212, $4, "i", $0; */

# 0 "" 2
	.loc 1 170 5 view .LVU140
# 170 "<stdin>" 1
	/* "struct user_settings", "invert" = $1216, $1, "b", $0; */

# 0 "" 2
	.loc 1 171 5 view .LVU141
# 171 "<stdin>" 1
	/* "struct user_settings", "flip_display" = $1217, $1, "b", $0; */

# 0 "" 2
	.loc 1 172 5 view .LVU142
# 172 "<stdin>" 1
	/* "struct user_settings", "cursor_style" = $1220, $4, "i", $0; */

# 0 "" 2
	.loc 1 173 5 view .LVU143
# 173 "<stdin>" 1
	/* "struct user_settings", "screen_scroll_step" = $1224, $4, "i", $0; */

# 0 "" 2
	.loc 1 174 5 view .LVU144
# 174 "<stdin>" 1
	/* "struct user_settings", "show_path_in_browser" = $1228, $4, "i", $0; */

# 0 "" 2
	.loc 1 175 5 view .LVU145
# 175 "<stdin>" 1
	/* "struct user_settings", "offset_out_of_view" = $1232, $1, "b", $0; */

# 0 "" 2
	.loc 1 176 5 view .LVU146
# 176 "<stdin>" 1
	/* "struct user_settings", "disable_mainmenu_scrolling" = $1233, $1, "b", $0; */

# 0 "" 2
	.loc 1 177 5 view .LVU147
# 177 "<stdin>" 1
	/* "struct user_settings", "icon_file" = $1234, $33, "str", $0; */

# 0 "" 2
	.loc 1 178 5 view .LVU148
# 178 "<stdin>" 1
	/* "struct user_settings", "viewers_icon_file" = $1267, $33, "str", $0; */

# 0 "" 2
	.loc 1 179 5 view .LVU149
# 179 "<stdin>" 1
	/* "struct user_settings", "font_file" = $1300, $33, "str", $0; */

# 0 "" 2
	.loc 1 180 5 view .LVU150
# 180 "<stdin>" 1
	/* "struct user_settings", "glyphs_to_cache" = $1336, $4, "i", $0; */

# 0 "" 2
	.loc 1 181 5 view .LVU151
# 181 "<stdin>" 1
	/* "struct user_settings", "kbd_file" = $1340, $33, "str", $0; */

# 0 "" 2
	.loc 1 182 5 view .LVU152
# 182 "<stdin>" 1
	/* "struct user_settings", "backlight_timeout" = $1376, $4, "i", $0; */

# 0 "" 2
	.loc 1 183 5 view .LVU153
# 183 "<stdin>" 1
	/* "struct user_settings", "caption_backlight" = $1380, $1, "b", $0; */

# 0 "" 2
	.loc 1 184 5 view .LVU154
# 184 "<stdin>" 1
	/* "struct user_settings", "bl_filter_first_keypress" = $1381, $1, "b", $0; */

# 0 "" 2
	.loc 1 185 5 view .LVU155
# 185 "<stdin>" 1
	/* "struct user_settings", "backlight_timeout_plugged" = $1384, $4, "i", $0; */

# 0 "" 2
	.loc 1 186 5 view .LVU156
# 186 "<stdin>" 1
	/* "struct user_settings", "bl_selective_actions" = $1388, $1, "b", $0; */

# 0 "" 2
	.loc 1 187 5 view .LVU157
# 187 "<stdin>" 1
	/* "struct user_settings", "bl_selective_actions_mask" = $1392, $4, "i", $0; */

# 0 "" 2
	.loc 1 188 5 view .LVU158
# 188 "<stdin>" 1
	/* "struct user_settings", "backlight_on_button_hold" = $1396, $4, "i", $0; */

# 0 "" 2
	.loc 1 189 5 view .LVU159
# 189 "<stdin>" 1
	/* "struct user_settings", "accessory_supply" = $1400, $1, "b", $0; */

# 0 "" 2
	.loc 1 190 5 view .LVU160
# 190 "<stdin>" 1
	/* "struct user_settings", "prevent_skip" = $1401, $1, "b", $0; */

# 0 "" 2
	.loc 1 191 5 view .LVU161
# 191 "<stdin>" 1
	/* "struct user_settings", "pitch_mode_semitone" = $1402, $1, "b", $0; */

# 0 "" 2
	.loc 1 192 5 view .LVU162
# 192 "<stdin>" 1
	/* "struct user_settings", "pitch_mode_timestretch" = $1403, $1, "b", $0; */

# 0 "" 2
	.loc 1 193 5 view .LVU163
# 193 "<stdin>" 1
	/* "struct user_settings", "ui_vp_config" = $1404, $64, "str", $0; */

# 0 "" 2
	.loc 1 194 5 view .LVU164
# 194 "<stdin>" 1
	/* "struct user_settings", "compressor_settings" = $1468, $24, "s_compressor_settings", $0; */

# 0 "" 2
	.loc 1 195 5 view .LVU165
# 195 "<stdin>" 1
	/* "struct user_settings", "sleeptimer_duration" = $1492, $4, "i", $0; */

# 0 "" 2
	.loc 1 196 5 view .LVU166
# 196 "<stdin>" 1
	/* "struct user_settings", "sleeptimer_on_startup" = $1496, $1, "b", $0; */

# 0 "" 2
	.loc 1 197 5 view .LVU167
# 197 "<stdin>" 1
	/* "struct user_settings", "keypress_restarts_sleeptimer" = $1497, $1, "b", $0; */

# 0 "" 2
	.loc 1 198 5 view .LVU168
# 198 "<stdin>" 1
	/* "struct user_settings", "show_shutdown_message" = $1498, $1, "b", $0; */

# 0 "" 2
	.loc 1 199 5 view .LVU169
# 199 "<stdin>" 1
	/* "struct user_settings", "morse_input" = $1499, $1, "b", $0; */

# 0 "" 2
	.loc 1 200 5 view .LVU170
# 200 "<stdin>" 1
	/* "struct user_settings", "hotkey_wps" = $1500, $4, "i", $0; */

# 0 "" 2
	.loc 1 201 5 view .LVU171
# 201 "<stdin>" 1
	/* "struct user_settings", "hotkey_tree" = $1504, $4, "i", $0; */

# 0 "" 2
	.loc 1 202 5 view .LVU172
# 202 "<stdin>" 1
	/* "struct user_settings", "resume_rewind" = $1508, $4, "i", $0; */

# 0 "" 2
	.loc 1 203 5 view .LVU173
# 203 "<stdin>" 1
	/* "struct user_settings", "start_directory" = $1512, $81, "str", $0; */

# 0 "" 2
	.loc 1 204 5 view .LVU174
# 204 "<stdin>" 1
	/* "struct user_settings", "root_menu_customized" = $1593, $1, "b", $0; */

# 0 "" 2
	.loc 1 205 5 view .LVU175
# 205 "<stdin>" 1
	/* "struct user_settings", "shortcuts_replaces_qs" = $1594, $1, "b", $0; */

# 0 "" 2
	.loc 1 206 5 view .LVU176
# 206 "<stdin>" 1
	/* "struct user_settings", "play_frequency" = $1596, $4, "i", $0; */

# 0 "" 2
	.loc 1 207 5 view .LVU177
# 207 "<stdin>" 1
	/* "struct user_settings", "volume_limit" = $1600, $4, "i", $0; */

# 0 "" 2
	.loc 1 208 5 view .LVU178
# 208 "<stdin>" 1
	/* "struct user_settings", "volume_adjust_mode" = $1604, $4, "i", $0; */

# 0 "" 2
	.loc 1 209 5 view .LVU179
# 209 "<stdin>" 1
	/* "struct user_settings", "volume_adjust_norm_steps" = $1608, $4, "i", $0; */

# 0 "" 2
	.loc 1 210 5 view .LVU180
# 210 "<stdin>" 1
	/* "struct user_settings", "surround_enabled" = $1612, $4, "i", $0; */

# 0 "" 2
	.loc 1 211 5 view .LVU181
# 211 "<stdin>" 1
	/* "struct user_settings", "surround_balance" = $1616, $4, "i", $0; */

# 0 "" 2
	.loc 1 212 5 view .LVU182
# 212 "<stdin>" 1
	/* "struct user_settings", "surround_fx1" = $1620, $4, "i", $0; */

# 0 "" 2
	.loc 1 213 5 view .LVU183
# 213 "<stdin>" 1
	/* "struct user_settings", "surround_fx2" = $1624, $4, "i", $0; */

# 0 "" 2
	.loc 1 214 5 view .LVU184
# 214 "<stdin>" 1
	/* "struct user_settings", "surround_method2" = $1628, $1, "b", $0; */

# 0 "" 2
	.loc 1 215 5 view .LVU185
# 215 "<stdin>" 1
	/* "struct user_settings", "surround_mix" = $1632, $4, "i", $0; */

# 0 "" 2
	.loc 1 216 5 view .LVU186
# 216 "<stdin>" 1
	/* "struct user_settings", "pbe" = $1636, $4, "i", $0; */

# 0 "" 2
	.loc 1 217 5 view .LVU187
# 217 "<stdin>" 1
	/* "struct user_settings", "pbe_precut" = $1640, $4, "i", $0; */

# 0 "" 2
	.loc 1 218 5 view .LVU188
# 218 "<stdin>" 1
	/* "struct user_settings", "afr_enabled" = $1644, $4, "i", $0; */

# 0 "" 2
	.loc 1 219 5 view .LVU189
# 219 "<stdin>" 1
	/* "struct user_settings", "playback_log" = $1648, $1, "b", $0; */

# 0 "" 2
	.loc 1 220 1 view .LVU190
	.loc 1 221 5 view .LVU191
# 221 "<stdin>" 1
	/* "struct replaygain_settings", "noclip" = $0, $1, "b", $0; */

# 0 "" 2
	.loc 1 222 5 view .LVU192
# 222 "<stdin>" 1
	/* "struct replaygain_settings", "type" = $4, $4, "i", $0; */

# 0 "" 2
	.loc 1 223 5 view .LVU193
# 223 "<stdin>" 1
	/* "struct replaygain_settings", "preamp" = $8, $4, "i", $0; */

# 0 "" 2
	.loc 1 224 1 view .LVU194
	.loc 1 225 5 view .LVU195
# 225 "<stdin>" 1
	/* "struct eq_band_setting", "cutoff" = $0, $4, "i", $0; */

# 0 "" 2
	.loc 1 226 5 view .LVU196
# 226 "<stdin>" 1
	/* "struct eq_band_setting", "q" = $4, $4, "i", $0; */

# 0 "" 2
	.loc 1 227 5 view .LVU197
# 227 "<stdin>" 1
	/* "struct eq_band_setting", "gain" = $8, $4, "i", $0; */

# 0 "" 2
	.loc 1 228 1 view .LVU198
	.loc 1 229 5 view .LVU199
# 229 "<stdin>" 1
	/* "struct compressor_settings", "threshold" = $0, $4, "i", $0; */

# 0 "" 2
	.loc 1 230 5 view .LVU200
# 230 "<stdin>" 1
	/* "struct compressor_settings", "makeup_gain" = $4, $4, "i", $0; */

# 0 "" 2
	.loc 1 231 5 view .LVU201
# 231 "<stdin>" 1
	/* "struct compressor_settings", "ratio" = $8, $4, "i", $0; */

# 0 "" 2
	.loc 1 232 5 view .LVU202
# 232 "<stdin>" 1
	/* "struct compressor_settings", "knee" = $12, $4, "i", $0; */

# 0 "" 2
	.loc 1 233 5 view .LVU203
# 233 "<stdin>" 1
	/* "struct compressor_settings", "release_time" = $16, $4, "i", $0; */

# 0 "" 2
	.loc 1 234 5 view .LVU204
# 234 "<stdin>" 1
	/* "struct compressor_settings", "attack_time" = $20, $4, "i", $0; */

# 0 "" 2
	.loc 1 235 1 view .LVU205
	.loc 1 236 5 view .LVU206
# 236 "<stdin>" 1
	/* "struct mp3entry", "path" = $0, $260, "str", $0; */

# 0 "" 2
	.loc 1 237 5 view .LVU207
# 237 "<stdin>" 1
	/* "struct mp3entry", "title" = $264, $8, "ptr_char", $0; */

# 0 "" 2
	.loc 1 238 5 view .LVU208
# 238 "<stdin>" 1
	/* "struct mp3entry", "artist" = $272, $8, "ptr_char", $0; */

# 0 "" 2
	.loc 1 239 5 view .LVU209
# 239 "<stdin>" 1
	/* "struct mp3entry", "album" = $280, $8, "ptr_char", $0; */

# 0 "" 2
	.loc 1 240 5 view .LVU210
# 240 "<stdin>" 1
	/* "struct mp3entry", "genre_string" = $288, $8, "ptr_char", $0; */

# 0 "" 2
	.loc 1 241 5 view .LVU211
# 241 "<stdin>" 1
	/* "struct mp3entry", "disc_string" = $296, $8, "ptr_char", $0; */

# 0 "" 2
	.loc 1 242 5 view .LVU212
# 242 "<stdin>" 1
	/* "struct mp3entry", "track_string" = $304, $8, "ptr_char", $0; */

# 0 "" 2
	.loc 1 243 5 view .LVU213
# 243 "<stdin>" 1
	/* "struct mp3entry", "year_string" = $312, $8, "ptr_char", $0; */

# 0 "" 2
	.loc 1 244 5 view .LVU214
# 244 "<stdin>" 1
	/* "struct mp3entry", "composer" = $320, $8, "ptr_char", $0; */

# 0 "" 2
	.loc 1 245 5 view .LVU215
# 245 "<stdin>" 1
	/* "struct mp3entry", "comment" = $328, $8, "ptr_char", $0; */

# 0 "" 2
	.loc 1 246 5 view .LVU216
# 246 "<stdin>" 1
	/* "struct mp3entry", "albumartist" = $336, $8, "ptr_char", $0; */

# 0 "" 2
	.loc 1 247 5 view .LVU217
# 247 "<stdin>" 1
	/* "struct mp3entry", "grouping" = $344, $8, "ptr_char", $0; */

# 0 "" 2
	.loc 1 248 5 view .LVU218
# 248 "<stdin>" 1
	/* "struct mp3entry", "discnum" = $352, $4, "i", $0; */

# 0 "" 2
	.loc 1 249 5 view .LVU219
# 249 "<stdin>" 1
	/* "struct mp3entry", "tracknum" = $356, $4, "i", $0; */

# 0 "" 2
	.loc 1 250 5 view .LVU220
# 250 "<stdin>" 1
	/* "struct mp3entry", "layer" = $360, $4, "i", $0; */

# 0 "" 2
	.loc 1 251 5 view .LVU221
# 251 "<stdin>" 1
	/* "struct mp3entry", "year" = $364, $4, "i", $0; */

# 0 "" 2
	.loc 1 252 5 view .LVU222
# 252 "<stdin>" 1
	/* "struct mp3entry", "id3version" = $368, $1, "u_c", $0; */

# 0 "" 2
	.loc 1 253 5 view .LVU223
# 253 "<stdin>" 1
	/* "struct mp3entry", "codectype" = $372, $4, "u_i", $0; */

# 0 "" 2
	.loc 1 254 5 view .LVU224
# 254 "<stdin>" 1
	/* "struct mp3entry", "bitrate" = $376, $4, "u_i", $0; */

# 0 "" 2
	.loc 1 255 5 view .LVU225
# 255 "<stdin>" 1
	/* "struct mp3entry", "frequency" = $384, $8, "u_l", $0; */

# 0 "" 2
	.loc 1 256 5 view .LVU226
# 256 "<stdin>" 1
	/* "struct mp3entry", "id3v2len" = $392, $8, "u_l", $0; */

# 0 "" 2
	.loc 1 257 5 view .LVU227
# 257 "<stdin>" 1
	/* "struct mp3entry", "id3v1len" = $400, $8, "u_l", $0; */

# 0 "" 2
	.loc 1 258 5 view .LVU228
# 258 "<stdin>" 1
	/* "struct mp3entry", "first_frame_offset" = $408, $8, "u_l", $0; */

# 0 "" 2
	.loc 1 259 5 view .LVU229
# 259 "<stdin>" 1
	/* "struct mp3entry", "sim_filesize" = $416, $8, "u_l", $0; */

# 0 "" 2
	.loc 1 260 5 view .LVU230
# 260 "<stdin>" 1
	/* "struct mp3entry", "length" = $424, $8, "u_l", $0; */

# 0 "" 2
	.loc 1 261 5 view .LVU231
# 261 "<stdin>" 1
	/* "struct mp3entry", "elapsed" = $432, $8, "u_l", $0; */

# 0 "" 2
	.loc 1 262 5 view .LVU232
# 262 "<stdin>" 1
	/* "struct mp3entry", "lead_trim" = $440, $4, "i", $0; */

# 0 "" 2
	.loc 1 263 5 view .LVU233
# 263 "<stdin>" 1
	/* "struct mp3entry", "tail_trim" = $444, $4, "i", $0; */

# 0 "" 2
	.loc 1 264 5 view .LVU234
# 264 "<stdin>" 1
	/* "struct mp3entry", "samples" = $448, $8, "u_i", $0; */

# 0 "" 2
	.loc 1 265 5 view .LVU235
# 265 "<stdin>" 1
	/* "struct mp3entry", "frame_count" = $456, $8, "u_l", $0; */

# 0 "" 2
	.loc 1 266 5 view .LVU236
# 266 "<stdin>" 1
	/* "struct mp3entry", "bytesperframe" = $464, $8, "u_l", $0; */

# 0 "" 2
	.loc 1 267 5 view .LVU237
# 267 "<stdin>" 1
	/* "struct mp3entry", "vbr" = $472, $1, "b", $0; */

# 0 "" 2
	.loc 1 268 5 view .LVU238
# 268 "<stdin>" 1
	/* "struct mp3entry", "has_toc" = $473, $1, "b", $0; */

# 0 "" 2
	.loc 1 269 5 view .LVU239
# 269 "<stdin>" 1
	/* "struct mp3entry", "toc" = $474, $100, "str", $0; */

# 0 "" 2
	.loc 1 270 5 view .LVU240
# 270 "<stdin>" 1
	/* "struct mp3entry", "needs_upsampling_correction" = $574, $1, "b", $0; */

# 0 "" 2
	.loc 1 271 5 view .LVU241
# 271 "<stdin>" 1
	/* "struct mp3entry", "id3v2buf" = $575, $1800, "str", $0; */

# 0 "" 2
	.loc 1 272 5 view .LVU242
# 272 "<stdin>" 1
	/* "struct mp3entry", "id3v1buf" = $2375, $368, "str", $0; */

# 0 "" 2
	.loc 1 273 5 view .LVU243
# 273 "<stdin>" 1
	/* "struct mp3entry", "offset" = $2744, $8, "u_l", $0; */

# 0 "" 2
	.loc 1 274 5 view .LVU244
# 274 "<stdin>" 1
	/* "struct mp3entry", "index" = $2752, $4, "i", $0; */

# 0 "" 2
	.loc 1 275 5 view .LVU245
# 275 "<stdin>" 1
	/* "struct mp3entry", "skip_resume_adjustments" = $2756, $1, "b", $0; */

# 0 "" 2
	.loc 1 276 5 view .LVU246
# 276 "<stdin>" 1
	/* "struct mp3entry", "autoresumable" = $2757, $1, "u_c", $0; */

# 0 "" 2
	.loc 1 277 5 view .LVU247
# 277 "<stdin>" 1
	/* "struct mp3entry", "tagcache_idx" = $2760, $8, "l", $0; */

# 0 "" 2
	.loc 1 278 5 view .LVU248
# 278 "<stdin>" 1
	/* "struct mp3entry", "rating" = $2768, $4, "i", $0; */

# 0 "" 2
	.loc 1 279 5 view .LVU249
# 279 "<stdin>" 1
	/* "struct mp3entry", "score" = $2772, $4, "i", $0; */

# 0 "" 2
	.loc 1 280 5 view .LVU250
# 280 "<stdin>" 1
	/* "struct mp3entry", "playcount" = $2776, $8, "l", $0; */

# 0 "" 2
	.loc 1 281 5 view .LVU251
# 281 "<stdin>" 1
	/* "struct mp3entry", "lastplayed" = $2784, $8, "l", $0; */

# 0 "" 2
	.loc 1 282 5 view .LVU252
# 282 "<stdin>" 1
	/* "struct mp3entry", "playtime" = $2792, $8, "l", $0; */

# 0 "" 2
	.loc 1 283 5 view .LVU253
# 283 "<stdin>" 1
	/* "struct mp3entry", "track_level" = $2800, $8, "l", $0; */

# 0 "" 2
	.loc 1 284 5 view .LVU254
# 284 "<stdin>" 1
	/* "struct mp3entry", "album_level" = $2808, $8, "l", $0; */

# 0 "" 2
	.loc 1 285 5 view .LVU255
# 285 "<stdin>" 1
	/* "struct mp3entry", "track_gain" = $2816, $8, "l", $0; */

# 0 "" 2
	.loc 1 286 5 view .LVU256
# 286 "<stdin>" 1
	/* "struct mp3entry", "album_gain" = $2824, $8, "l", $0; */

# 0 "" 2
	.loc 1 287 5 view .LVU257
# 287 "<stdin>" 1
	/* "struct mp3entry", "track_peak" = $2832, $8, "l", $0; */

# 0 "" 2
	.loc 1 288 5 view .LVU258
# 288 "<stdin>" 1
	/* "struct mp3entry", "album_peak" = $2840, $8, "l", $0; */

# 0 "" 2
	.loc 1 289 5 view .LVU259
# 289 "<stdin>" 1
	/* "struct mp3entry", "has_embedded_albumart" = $2848, $1, "b", $0; */

# 0 "" 2
	.loc 1 290 5 view .LVU260
# 290 "<stdin>" 1
	/* "struct mp3entry", "albumart" = $2856, $16, "s_mp3_albumart", $0; */

# 0 "" 2
	.loc 1 291 5 view .LVU261
# 291 "<stdin>" 1
	/* "struct mp3entry", "has_embedded_cuesheet" = $2872, $1, "b", $0; */

# 0 "" 2
	.loc 1 292 5 view .LVU262
# 292 "<stdin>" 1
	/* "struct mp3entry", "embedded_cuesheet" = $2880, $24, "s_embedded_cuesheet", $0; */

# 0 "" 2
	.loc 1 293 5 view .LVU263
# 293 "<stdin>" 1
	/* "struct mp3entry", "cuesheet" = $2904, $8, "ptr_s_cuesheet", $0; */

# 0 "" 2
	.loc 1 294 5 view .LVU264
# 294 "<stdin>" 1
	/* "struct mp3entry", "mb_track_id" = $2912, $8, "ptr_char", $0; */

# 0 "" 2
	.loc 1 295 5 view .LVU265
# 295 "<stdin>" 1
	/* "struct mp3entry", "is_asf_stream" = $2920, $1, "b", $0; */

# 0 "" 2
	.loc 1 296 5 view .LVU266
# 296 "<stdin>" 1
	/* "struct mp3entry", "has_video" = $2921, $1, "b", $0; */

# 0 "" 2
	.loc 1 297 1 view .LVU267
	.loc 1 298 5 view .LVU268
# 298 "<stdin>" 1
	/* "struct mp3_albumart", "type" = $0, $4, "e_mp3_aa_type", $0; */

# 0 "" 2
	.loc 1 299 5 view .LVU269
# 299 "<stdin>" 1
	/* "struct mp3_albumart", "size" = $4, $4, "i", $0; */

# 0 "" 2
	.loc 1 300 5 view .LVU270
# 300 "<stdin>" 1
	/* "struct mp3_albumart", "pos" = $8, $8, "off_t", $0; */

# 0 "" 2
	.loc 1 301 1 view .LVU271
	.loc 1 302 5 view .LVU272
# 302 "<stdin>" 1
	/* "struct embedded_cuesheet", "size" = $0, $4, "i", $0; */

# 0 "" 2
	.loc 1 303 5 view .LVU273
# 303 "<stdin>" 1
	/* "struct embedded_cuesheet", "pos" = $8, $8, "off_t", $0; */

# 0 "" 2
	.loc 1 304 5 view .LVU274
# 304 "<stdin>" 1
	/* "struct embedded_cuesheet", "encoding" = $16, $4, "e_character_encoding", $0; */

# 0 "" 2
	.loc 1 306 5 view .LVU275
	.loc 1 307 1 is_stmt 0 view .LVU276
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
	.file 6 "/usr/lib/gcc/x86_64-pc-linux-gnu/15.2.1/include/stddef.h"
	.file 7 "/home/david/Documents/RockBox_Personal-master/lib/rbcodec/metadata/metadata.h"
	.file 8 "/home/david/Documents/RockBox_Personal-master/lib/rbcodec/dsp/compressor.h"
	.file 9 "/home/david/Documents/RockBox_Personal-master/lib/rbcodec/dsp/dsp_misc.h"
	.file 10 "/home/david/Documents/RockBox_Personal-master/lib/rbcodec/dsp/eq.h"
	.file 11 "/home/david/Documents/RockBox_Personal-master/apps/settings.h"
	.file 12 "/home/david/Documents/RockBox_Personal-master/apps/settings_list.h"
	.section	.debug_info,"",@progbits
.Ldebug_info0:
	.long	0x1673
	.value	0x5
	.byte	0x1
	.byte	0x8
	.long	.Ldebug_abbrev0
	.uleb128 0x19
	.long	.LASF349
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
	.byte	0x18
	.byte	0x8
	.byte	0x18
	.long	0x64a
	.uleb128 0x2
	.long	.LASF104
	.byte	0x8
	.byte	0x1a
	.byte	0x9
	.long	0x71
	.byte	0
	.uleb128 0x2
	.long	.LASF105
	.byte	0x8
	.byte	0x1b
	.byte	0x9
	.long	0x71
	.byte	0x4
	.uleb128 0x2
	.long	.LASF106
	.byte	0x8
	.byte	0x1c
	.byte	0x9
	.long	0x71
	.byte	0x8
	.uleb128 0x2
	.long	.LASF107
	.byte	0x8
	.byte	0x1d
	.byte	0x9
	.long	0x71
	.byte	0xc
	.uleb128 0x2
	.long	.LASF108
	.byte	0x8
	.byte	0x1e
	.byte	0x9
	.long	0x71
	.byte	0x10
	.uleb128 0x2
	.long	.LASF109
	.byte	0x8
	.byte	0x1f
	.byte	0x9
	.long	0x71
	.byte	0x14
	.byte	0
	.uleb128 0x9
	.long	.LASF110
	.byte	0xc
	.byte	0x9
	.byte	0x25
	.long	0x67e
	.uleb128 0x2
	.long	.LASF111
	.byte	0x9
	.byte	0x27
	.byte	0xa
	.long	0x121
	.byte	0
	.uleb128 0x2
	.long	.LASF94
	.byte	0x9
	.byte	0x28
	.byte	0x9
	.long	0x71
	.byte	0x4
	.uleb128 0x2
	.long	.LASF112
	.byte	0x9
	.byte	0x2a
	.byte	0x9
	.long	0x71
	.byte	0x8
	.byte	0
	.uleb128 0x9
	.long	.LASF113
	.byte	0xc
	.byte	0xa
	.byte	0x1b
	.long	0x6b0
	.uleb128 0x2
	.long	.LASF114
	.byte	0xa
	.byte	0x1d
	.byte	0x9
	.long	0x71
	.byte	0
	.uleb128 0x10
	.string	"q"
	.byte	0xa
	.byte	0x1e
	.byte	0x9
	.long	0x71
	.byte	0x4
	.uleb128 0x2
	.long	.LASF115
	.byte	0xa
	.byte	0x1f
	.byte	0x9
	.long	0x71
	.byte	0x8
	.byte	0
	.uleb128 0x20
	.long	.LASF116
	.byte	0x38
	.byte	0xb
	.value	0x156
	.byte	0x8
	.long	0x775
	.uleb128 0x3
	.long	.LASF117
	.value	0x158
	.byte	0x9
	.long	0x71
	.byte	0
	.uleb128 0x3
	.long	.LASF118
	.value	0x159
	.byte	0x9
	.long	0x71
	.byte	0x4
	.uleb128 0x3
	.long	.LASF119
	.value	0x15a
	.byte	0xe
	.long	0xd3
	.byte	0x8
	.uleb128 0x3
	.long	.LASF120
	.value	0x15b
	.byte	0xe
	.long	0xd3
	.byte	0xc
	.uleb128 0x3
	.long	.LASF121
	.value	0x15c
	.byte	0xe
	.long	0xd3
	.byte	0x10
	.uleb128 0x3
	.long	.LASF122
	.value	0x15e
	.byte	0xd
	.long	0xc7
	.byte	0x14
	.uleb128 0x3
	.long	.LASF123
	.value	0x15f
	.byte	0xd
	.long	0xc7
	.byte	0x18
	.uleb128 0x3
	.long	.LASF124
	.value	0x161
	.byte	0x9
	.long	0x71
	.byte	0x1c
	.uleb128 0x3
	.long	.LASF125
	.value	0x162
	.byte	0x9
	.long	0x71
	.byte	0x20
	.uleb128 0x3
	.long	.LASF126
	.value	0x16a
	.byte	0x11
	.long	0x4b
	.byte	0x24
	.uleb128 0x3
	.long	.LASF127
	.value	0x16b
	.byte	0xa
	.long	0x71
	.byte	0x28
	.uleb128 0x3
	.long	.LASF128
	.value	0x16c
	.byte	0x9
	.long	0x71
	.byte	0x2c
	.uleb128 0x3
	.long	.LASF129
	.value	0x16d
	.byte	0x9
	.long	0x775
	.byte	0x30
	.uleb128 0x3
	.long	.LASF130
	.value	0x16f
	.byte	0xa
	.long	0x121
	.byte	0x34
	.byte	0
	.uleb128 0xb
	.long	0x71
	.long	0x785
	.uleb128 0xd
	.long	0x44
	.byte	0
	.byte	0
	.uleb128 0x21
	.long	.LASF131
	.value	0x678
	.byte	0xb
	.value	0x172
	.byte	0x8
	.long	0x116f
	.uleb128 0x3
	.long	.LASF132
	.value	0x175
	.byte	0x9
	.long	0x71
	.byte	0
	.uleb128 0x3
	.long	.LASF133
	.value	0x176
	.byte	0x9
	.long	0x71
	.byte	0x4
	.uleb128 0x3
	.long	.LASF134
	.value	0x177
	.byte	0x9
	.long	0x71
	.byte	0x8
	.uleb128 0x3
	.long	.LASF135
	.value	0x178
	.byte	0x9
	.long	0x71
	.byte	0xc
	.uleb128 0x3
	.long	.LASF136
	.value	0x179
	.byte	0x9
	.long	0x71
	.byte	0x10
	.uleb128 0x3
	.long	.LASF137
	.value	0x184
	.byte	0x9
	.long	0x71
	.byte	0x14
	.uleb128 0x3
	.long	.LASF138
	.value	0x186
	.byte	0x9
	.long	0x71
	.byte	0x18
	.uleb128 0x3
	.long	.LASF139
	.value	0x187
	.byte	0x9
	.long	0x71
	.byte	0x1c
	.uleb128 0x3
	.long	.LASF140
	.value	0x188
	.byte	0x9
	.long	0x71
	.byte	0x20
	.uleb128 0x3
	.long	.LASF141
	.value	0x189
	.byte	0x9
	.long	0x71
	.byte	0x24
	.uleb128 0x3
	.long	.LASF142
	.value	0x18a
	.byte	0x9
	.long	0x71
	.byte	0x28
	.uleb128 0x3
	.long	.LASF110
	.value	0x18e
	.byte	0x20
	.long	0x64a
	.byte	0x2c
	.uleb128 0x3
	.long	.LASF143
	.value	0x191
	.byte	0x9
	.long	0x71
	.byte	0x38
	.uleb128 0x3
	.long	.LASF144
	.value	0x192
	.byte	0x12
	.long	0x3d
	.byte	0x3c
	.uleb128 0x3
	.long	.LASF145
	.value	0x193
	.byte	0x12
	.long	0x3d
	.byte	0x40
	.uleb128 0x3
	.long	.LASF146
	.value	0x194
	.byte	0x12
	.long	0x3d
	.byte	0x44
	.uleb128 0x3
	.long	.LASF147
	.value	0x195
	.byte	0x12
	.long	0x3d
	.byte	0x48
	.uleb128 0x3
	.long	.LASF148
	.value	0x198
	.byte	0xa
	.long	0x121
	.byte	0x4c
	.uleb128 0x3
	.long	.LASF149
	.value	0x199
	.byte	0x12
	.long	0x3d
	.byte	0x50
	.uleb128 0x3
	.long	.LASF150
	.value	0x19a
	.byte	0x1c
	.long	0x116f
	.byte	0x54
	.uleb128 0x3
	.long	.LASF151
	.value	0x19d
	.byte	0xa
	.long	0x71
	.byte	0xcc
	.uleb128 0x3
	.long	.LASF152
	.value	0x19e
	.byte	0xa
	.long	0x71
	.byte	0xd0
	.uleb128 0x3
	.long	.LASF153
	.value	0x19f
	.byte	0xa
	.long	0x71
	.byte	0xd4
	.uleb128 0x3
	.long	.LASF154
	.value	0x1a0
	.byte	0xa
	.long	0x121
	.byte	0xd8
	.uleb128 0x3
	.long	.LASF155
	.value	0x1a2
	.byte	0xa
	.long	0x121
	.byte	0xd9
	.uleb128 0x3
	.long	.LASF156
	.value	0x210
	.byte	0xa
	.long	0x71
	.byte	0xdc
	.uleb128 0x3
	.long	.LASF157
	.value	0x212
	.byte	0xa
	.long	0x71
	.byte	0xe0
	.uleb128 0x3
	.long	.LASF158
	.value	0x213
	.byte	0xa
	.long	0x121
	.byte	0xe4
	.uleb128 0x3
	.long	.LASF159
	.value	0x217
	.byte	0x21
	.long	0x117f
	.byte	0xe8
	.uleb128 0x1
	.long	.LASF160
	.byte	0xb
	.value	0x21a
	.byte	0x9
	.long	0x71
	.value	0x108
	.uleb128 0x1
	.long	.LASF161
	.byte	0xb
	.value	0x21d
	.byte	0x9
	.long	0x71
	.value	0x10c
	.uleb128 0x1
	.long	.LASF162
	.byte	0xb
	.value	0x21e
	.byte	0x9
	.long	0x71
	.value	0x110
	.uleb128 0x1
	.long	.LASF163
	.byte	0xb
	.value	0x21f
	.byte	0x9
	.long	0x71
	.value	0x114
	.uleb128 0x1
	.long	.LASF164
	.byte	0xb
	.value	0x222
	.byte	0x9
	.long	0x71
	.value	0x118
	.uleb128 0x1
	.long	.LASF165
	.byte	0xb
	.value	0x224
	.byte	0x9
	.long	0x71
	.value	0x11c
	.uleb128 0x1
	.long	.LASF166
	.byte	0xb
	.value	0x226
	.byte	0x9
	.long	0x71
	.value	0x120
	.uleb128 0x1
	.long	.LASF167
	.byte	0xb
	.value	0x227
	.byte	0xa
	.long	0x121
	.value	0x124
	.uleb128 0x1
	.long	.LASF168
	.byte	0xb
	.value	0x228
	.byte	0xa
	.long	0x121
	.value	0x125
	.uleb128 0x1
	.long	.LASF169
	.byte	0xb
	.value	0x229
	.byte	0x9
	.long	0x71
	.value	0x128
	.uleb128 0x1
	.long	.LASF170
	.byte	0xb
	.value	0x22b
	.byte	0xa
	.long	0x121
	.value	0x12c
	.uleb128 0x1
	.long	.LASF82
	.byte	0xb
	.value	0x22c
	.byte	0xa
	.long	0x121
	.value	0x12d
	.uleb128 0x1
	.long	.LASF171
	.byte	0xb
	.value	0x22d
	.byte	0xa
	.long	0x121
	.value	0x12e
	.uleb128 0x1
	.long	.LASF172
	.byte	0xb
	.value	0x22e
	.byte	0x9
	.long	0x71
	.value	0x130
	.uleb128 0x1
	.long	.LASF173
	.byte	0xb
	.value	0x22f
	.byte	0x9
	.long	0x71
	.value	0x134
	.uleb128 0x1
	.long	.LASF174
	.byte	0xb
	.value	0x230
	.byte	0x9
	.long	0x71
	.value	0x138
	.uleb128 0x1
	.long	.LASF175
	.byte	0xb
	.value	0x235
	.byte	0x9
	.long	0x71
	.value	0x13c
	.uleb128 0x1
	.long	.LASF176
	.byte	0xb
	.value	0x236
	.byte	0x9
	.long	0x71
	.value	0x140
	.uleb128 0x1
	.long	.LASF177
	.byte	0xb
	.value	0x238
	.byte	0x9
	.long	0x71
	.value	0x144
	.uleb128 0x1
	.long	.LASF178
	.byte	0xb
	.value	0x239
	.byte	0x9
	.long	0x71
	.value	0x148
	.uleb128 0x1
	.long	.LASF179
	.byte	0xb
	.value	0x23a
	.byte	0x9
	.long	0x71
	.value	0x14c
	.uleb128 0x1
	.long	.LASF180
	.byte	0xb
	.value	0x23b
	.byte	0xa
	.long	0x121
	.value	0x150
	.uleb128 0x1
	.long	.LASF181
	.byte	0xb
	.value	0x23c
	.byte	0x9
	.long	0x71
	.value	0x154
	.uleb128 0x1
	.long	.LASF182
	.byte	0xb
	.value	0x23d
	.byte	0x9
	.long	0x71
	.value	0x158
	.uleb128 0x1
	.long	.LASF183
	.byte	0xb
	.value	0x23f
	.byte	0x13
	.long	0x11ed
	.value	0x15c
	.uleb128 0x1
	.long	.LASF184
	.byte	0xb
	.value	0x240
	.byte	0x13
	.long	0x11ed
	.value	0x17d
	.uleb128 0x1
	.long	.LASF185
	.byte	0xb
	.value	0x245
	.byte	0x13
	.long	0x11ed
	.value	0x19e
	.uleb128 0x1
	.long	.LASF186
	.byte	0xb
	.value	0x246
	.byte	0x13
	.long	0x11fd
	.value	0x1bf
	.uleb128 0x1
	.long	.LASF187
	.byte	0xb
	.value	0x247
	.byte	0x9
	.long	0x71
	.value	0x210
	.uleb128 0x1
	.long	.LASF188
	.byte	0xb
	.value	0x248
	.byte	0x9
	.long	0x71
	.value	0x214
	.uleb128 0x1
	.long	.LASF189
	.byte	0xb
	.value	0x249
	.byte	0x9
	.long	0x71
	.value	0x218
	.uleb128 0x1
	.long	.LASF190
	.byte	0xb
	.value	0x24a
	.byte	0x9
	.long	0x71
	.value	0x21c
	.uleb128 0x1
	.long	.LASF191
	.byte	0xb
	.value	0x24b
	.byte	0x9
	.long	0x71
	.value	0x220
	.uleb128 0x1
	.long	.LASF192
	.byte	0xb
	.value	0x24c
	.byte	0xa
	.long	0x121
	.value	0x224
	.uleb128 0x1
	.long	.LASF193
	.byte	0xb
	.value	0x24d
	.byte	0x9
	.long	0x71
	.value	0x228
	.uleb128 0x1
	.long	.LASF194
	.byte	0xb
	.value	0x252
	.byte	0x9
	.long	0x71
	.value	0x22c
	.uleb128 0x1
	.long	.LASF195
	.byte	0xb
	.value	0x253
	.byte	0x9
	.long	0x71
	.value	0x230
	.uleb128 0x1
	.long	.LASF196
	.byte	0xb
	.value	0x259
	.byte	0x9
	.long	0x71
	.value	0x234
	.uleb128 0x1
	.long	.LASF197
	.byte	0xb
	.value	0x25a
	.byte	0x9
	.long	0x71
	.value	0x238
	.uleb128 0x1
	.long	.LASF198
	.byte	0xb
	.value	0x25d
	.byte	0xa
	.long	0x121
	.value	0x23c
	.uleb128 0x1
	.long	.LASF199
	.byte	0xb
	.value	0x25f
	.byte	0xa
	.long	0x121
	.value	0x23d
	.uleb128 0x1
	.long	.LASF200
	.byte	0xb
	.value	0x260
	.byte	0xa
	.long	0x121
	.value	0x23e
	.uleb128 0x1
	.long	.LASF201
	.byte	0xb
	.value	0x261
	.byte	0xa
	.long	0x71
	.value	0x240
	.uleb128 0x1
	.long	.LASF202
	.byte	0xb
	.value	0x262
	.byte	0xa
	.long	0x71
	.value	0x244
	.uleb128 0x1
	.long	.LASF203
	.byte	0xb
	.value	0x263
	.byte	0xa
	.long	0x71
	.value	0x248
	.uleb128 0x1
	.long	.LASF204
	.byte	0xb
	.value	0x264
	.byte	0xa
	.long	0x71
	.value	0x24c
	.uleb128 0x1
	.long	.LASF205
	.byte	0xb
	.value	0x265
	.byte	0xa
	.long	0x71
	.value	0x250
	.uleb128 0x1
	.long	.LASF206
	.byte	0xb
	.value	0x268
	.byte	0x9
	.long	0x71
	.value	0x254
	.uleb128 0x1
	.long	.LASF207
	.byte	0xb
	.value	0x269
	.byte	0x9
	.long	0x71
	.value	0x258
	.uleb128 0x1
	.long	.LASF208
	.byte	0xb
	.value	0x26a
	.byte	0xa
	.long	0x121
	.value	0x25c
	.uleb128 0x1
	.long	.LASF209
	.byte	0xb
	.value	0x26b
	.byte	0x9
	.long	0x71
	.value	0x260
	.uleb128 0x1
	.long	.LASF210
	.byte	0xb
	.value	0x273
	.byte	0x9
	.long	0x71
	.value	0x264
	.uleb128 0x1
	.long	.LASF211
	.byte	0xb
	.value	0x275
	.byte	0xa
	.long	0x121
	.value	0x268
	.uleb128 0x1
	.long	.LASF212
	.byte	0xb
	.value	0x276
	.byte	0xa
	.long	0x121
	.value	0x269
	.uleb128 0x1
	.long	.LASF213
	.byte	0xb
	.value	0x277
	.byte	0x9
	.long	0x71
	.value	0x26c
	.uleb128 0x1
	.long	.LASF214
	.byte	0xb
	.value	0x279
	.byte	0x13
	.long	0x120d
	.value	0x270
	.uleb128 0x1
	.long	.LASF215
	.byte	0xb
	.value	0x27a
	.byte	0xa
	.long	0x121
	.value	0x311
	.uleb128 0x1
	.long	.LASF216
	.byte	0xb
	.value	0x27b
	.byte	0x13
	.long	0x120d
	.value	0x312
	.uleb128 0x1
	.long	.LASF217
	.byte	0xb
	.value	0x27c
	.byte	0x13
	.long	0x11fd
	.value	0x3b3
	.uleb128 0x1
	.long	.LASF218
	.byte	0xb
	.value	0x27f
	.byte	0x9
	.long	0x71
	.value	0x404
	.uleb128 0x1
	.long	.LASF219
	.byte	0xb
	.value	0x282
	.byte	0x13
	.long	0x11fd
	.value	0x408
	.uleb128 0x1
	.long	.LASF220
	.byte	0xb
	.value	0x291
	.byte	0x9
	.long	0x71
	.value	0x45c
	.uleb128 0x1
	.long	.LASF221
	.byte	0xb
	.value	0x294
	.byte	0xa
	.long	0x71
	.value	0x460
	.uleb128 0x1
	.long	.LASF222
	.byte	0xb
	.value	0x295
	.byte	0xa
	.long	0x71
	.value	0x464
	.uleb128 0x1
	.long	.LASF223
	.byte	0xb
	.value	0x296
	.byte	0xa
	.long	0x121
	.value	0x468
	.uleb128 0x1
	.long	.LASF224
	.byte	0xb
	.value	0x298
	.byte	0xa
	.long	0x71
	.value	0x46c
	.uleb128 0x1
	.long	.LASF225
	.byte	0xb
	.value	0x299
	.byte	0xa
	.long	0x121
	.value	0x470
	.uleb128 0x1
	.long	.LASF226
	.byte	0xb
	.value	0x29a
	.byte	0xa
	.long	0x121
	.value	0x471
	.uleb128 0x1
	.long	.LASF227
	.byte	0xb
	.value	0x29b
	.byte	0xa
	.long	0x121
	.value	0x472
	.uleb128 0x1
	.long	.LASF228
	.byte	0xb
	.value	0x29c
	.byte	0xa
	.long	0x121
	.value	0x473
	.uleb128 0x1
	.long	.LASF229
	.byte	0xb
	.value	0x29d
	.byte	0xa
	.long	0x121
	.value	0x474
	.uleb128 0x1
	.long	.LASF230
	.byte	0xb
	.value	0x29e
	.byte	0x9
	.long	0x71
	.value	0x478
	.uleb128 0x1
	.long	.LASF231
	.byte	0xb
	.value	0x2a0
	.byte	0x9
	.long	0x71
	.value	0x47c
	.uleb128 0x1
	.long	.LASF232
	.byte	0xb
	.value	0x2a2
	.byte	0xa
	.long	0x121
	.value	0x480
	.uleb128 0x1
	.long	.LASF233
	.byte	0xb
	.value	0x2a5
	.byte	0xa
	.long	0x121
	.value	0x481
	.uleb128 0x1
	.long	.LASF234
	.byte	0xb
	.value	0x2a6
	.byte	0xa
	.long	0x121
	.value	0x482
	.uleb128 0x1
	.long	.LASF235
	.byte	0xb
	.value	0x2a7
	.byte	0x9
	.long	0x71
	.value	0x484
	.uleb128 0x1
	.long	.LASF236
	.byte	0xb
	.value	0x2aa
	.byte	0xa
	.long	0x121
	.value	0x488
	.uleb128 0x1
	.long	.LASF237
	.byte	0xb
	.value	0x2ab
	.byte	0x9
	.long	0x71
	.value	0x48c
	.uleb128 0x1
	.long	.LASF238
	.byte	0xb
	.value	0x2ac
	.byte	0xa
	.long	0x121
	.value	0x490
	.uleb128 0x1
	.long	.LASF239
	.byte	0xb
	.value	0x2ad
	.byte	0x9
	.long	0x71
	.value	0x494
	.uleb128 0x1
	.long	.LASF240
	.byte	0xb
	.value	0x2ae
	.byte	0xa
	.long	0x121
	.value	0x498
	.uleb128 0x1
	.long	.LASF241
	.byte	0xb
	.value	0x2af
	.byte	0xa
	.long	0x121
	.value	0x499
	.uleb128 0x1
	.long	.LASF242
	.byte	0xb
	.value	0x2b0
	.byte	0xa
	.long	0x121
	.value	0x49a
	.uleb128 0x1
	.long	.LASF243
	.byte	0xb
	.value	0x2b1
	.byte	0xa
	.long	0x71
	.value	0x49c
	.uleb128 0x1
	.long	.LASF244
	.byte	0xb
	.value	0x2b4
	.byte	0xa
	.long	0x121
	.value	0x4a0
	.uleb128 0x1
	.long	.LASF245
	.byte	0xb
	.value	0x2b5
	.byte	0x9
	.long	0x71
	.value	0x4a4
	.uleb128 0x1
	.long	.LASF246
	.byte	0xb
	.value	0x2b6
	.byte	0x9
	.long	0x71
	.value	0x4a8
	.uleb128 0x1
	.long	.LASF247
	.byte	0xb
	.value	0x2b7
	.byte	0x9
	.long	0x71
	.value	0x4ac
	.uleb128 0x1
	.long	.LASF248
	.byte	0xb
	.value	0x2b8
	.byte	0x9
	.long	0x71
	.value	0x4b0
	.uleb128 0x1
	.long	.LASF249
	.byte	0xb
	.value	0x2bb
	.byte	0x9
	.long	0x71
	.value	0x4b4
	.uleb128 0x1
	.long	.LASF250
	.byte	0xb
	.value	0x2bd
	.byte	0x9
	.long	0x71
	.value	0x4b8
	.uleb128 0x1
	.long	.LASF251
	.byte	0xb
	.value	0x2c7
	.byte	0x9
	.long	0x71
	.value	0x4bc
	.uleb128 0x1
	.long	.LASF252
	.byte	0xb
	.value	0x2cb
	.byte	0xa
	.long	0x121
	.value	0x4c0
	.uleb128 0x1
	.long	.LASF253
	.byte	0xb
	.value	0x2ce
	.byte	0xa
	.long	0x121
	.value	0x4c1
	.uleb128 0x1
	.long	.LASF254
	.byte	0xb
	.value	0x2d0
	.byte	0xa
	.long	0x71
	.value	0x4c4
	.uleb128 0x1
	.long	.LASF255
	.byte	0xb
	.value	0x2d1
	.byte	0xa
	.long	0x71
	.value	0x4c8
	.uleb128 0x1
	.long	.LASF256
	.byte	0xb
	.value	0x2d2
	.byte	0xa
	.long	0x71
	.value	0x4cc
	.uleb128 0x1
	.long	.LASF257
	.byte	0xb
	.value	0x2d3
	.byte	0xa
	.long	0x121
	.value	0x4d0
	.uleb128 0x1
	.long	.LASF258
	.byte	0xb
	.value	0x2d4
	.byte	0xa
	.long	0x121
	.value	0x4d1
	.uleb128 0x1
	.long	.LASF259
	.byte	0xb
	.value	0x2d5
	.byte	0x13
	.long	0x11ed
	.value	0x4d2
	.uleb128 0x1
	.long	.LASF260
	.byte	0xb
	.value	0x2d6
	.byte	0x13
	.long	0x11ed
	.value	0x4f3
	.uleb128 0x1
	.long	.LASF261
	.byte	0xb
	.value	0x2d7
	.byte	0x13
	.long	0x11ed
	.value	0x514
	.uleb128 0x1
	.long	.LASF262
	.byte	0xb
	.value	0x2d8
	.byte	0x9
	.long	0x71
	.value	0x538
	.uleb128 0x1
	.long	.LASF263
	.byte	0xb
	.value	0x2dc
	.byte	0x13
	.long	0x11ed
	.value	0x53c
	.uleb128 0x1
	.long	.LASF264
	.byte	0xb
	.value	0x2dd
	.byte	0xa
	.long	0x71
	.value	0x560
	.uleb128 0x1
	.long	.LASF265
	.byte	0xb
	.value	0x2df
	.byte	0xa
	.long	0x121
	.value	0x564
	.uleb128 0x1
	.long	.LASF266
	.byte	0xb
	.value	0x2e0
	.byte	0xa
	.long	0x121
	.value	0x565
	.uleb128 0x1
	.long	.LASF267
	.byte	0xb
	.value	0x2e2
	.byte	0x9
	.long	0x71
	.value	0x568
	.uleb128 0x1
	.long	.LASF268
	.byte	0xb
	.value	0x2e9
	.byte	0xa
	.long	0x121
	.value	0x56c
	.uleb128 0x1
	.long	.LASF269
	.byte	0xb
	.value	0x2ea
	.byte	0xa
	.long	0x71
	.value	0x570
	.uleb128 0x1
	.long	.LASF270
	.byte	0xb
	.value	0x2eb
	.byte	0x9
	.long	0x71
	.value	0x574
	.uleb128 0x1
	.long	.LASF271
	.byte	0xb
	.value	0x324
	.byte	0xa
	.long	0x121
	.value	0x578
	.uleb128 0x1
	.long	.LASF272
	.byte	0xb
	.value	0x32d
	.byte	0xa
	.long	0x121
	.value	0x579
	.uleb128 0x1
	.long	.LASF273
	.byte	0xb
	.value	0x336
	.byte	0xa
	.long	0x121
	.value	0x57a
	.uleb128 0x1
	.long	.LASF274
	.byte	0xb
	.value	0x337
	.byte	0xa
	.long	0x121
	.value	0x57b
	.uleb128 0x1
	.long	.LASF275
	.byte	0xb
	.value	0x34a
	.byte	0x13
	.long	0x121d
	.value	0x57c
	.uleb128 0x1
	.long	.LASF103
	.byte	0xb
	.value	0x34f
	.byte	0x20
	.long	0x5ef
	.value	0x5bc
	.uleb128 0x1
	.long	.LASF276
	.byte	0xb
	.value	0x351
	.byte	0x9
	.long	0x71
	.value	0x5d4
	.uleb128 0x1
	.long	.LASF277
	.byte	0xb
	.value	0x352
	.byte	0xa
	.long	0x121
	.value	0x5d8
	.uleb128 0x1
	.long	.LASF278
	.byte	0xb
	.value	0x353
	.byte	0xa
	.long	0x121
	.value	0x5d9
	.uleb128 0x1
	.long	.LASF279
	.byte	0xb
	.value	0x355
	.byte	0xa
	.long	0x121
	.value	0x5da
	.uleb128 0x1
	.long	.LASF280
	.byte	0xb
	.value	0x359
	.byte	0xa
	.long	0x121
	.value	0x5db
	.uleb128 0x1
	.long	.LASF281
	.byte	0xb
	.value	0x35f
	.byte	0x9
	.long	0x71
	.value	0x5dc
	.uleb128 0x1
	.long	.LASF282
	.byte	0xb
	.value	0x360
	.byte	0x9
	.long	0x71
	.value	0x5e0
	.uleb128 0x1
	.long	.LASF283
	.byte	0xb
	.value	0x364
	.byte	0x9
	.long	0x71
	.value	0x5e4
	.uleb128 0x1
	.long	.LASF284
	.byte	0xb
	.value	0x385
	.byte	0xa
	.long	0x122d
	.value	0x5e8
	.uleb128 0x1
	.long	.LASF285
	.byte	0xb
	.value	0x387
	.byte	0xa
	.long	0x121
	.value	0x639
	.uleb128 0x1
	.long	.LASF286
	.byte	0xb
	.value	0x389
	.byte	0xa
	.long	0x121
	.value	0x63a
	.uleb128 0x1
	.long	.LASF287
	.byte	0xb
	.value	0x38d
	.byte	0x9
	.long	0x71
	.value	0x63c
	.uleb128 0x1
	.long	.LASF288
	.byte	0xb
	.value	0x38f
	.byte	0x9
	.long	0x71
	.value	0x640
	.uleb128 0x1
	.long	.LASF289
	.byte	0xb
	.value	0x392
	.byte	0x9
	.long	0x71
	.value	0x644
	.uleb128 0x1
	.long	.LASF290
	.byte	0xb
	.value	0x393
	.byte	0x9
	.long	0x71
	.value	0x648
	.uleb128 0x1
	.long	.LASF291
	.byte	0xb
	.value	0x396
	.byte	0x9
	.long	0x71
	.value	0x64c
	.uleb128 0x1
	.long	.LASF292
	.byte	0xb
	.value	0x397
	.byte	0x9
	.long	0x71
	.value	0x650
	.uleb128 0x1
	.long	.LASF293
	.byte	0xb
	.value	0x398
	.byte	0x9
	.long	0x71
	.value	0x654
	.uleb128 0x1
	.long	.LASF294
	.byte	0xb
	.value	0x399
	.byte	0x9
	.long	0x71
	.value	0x658
	.uleb128 0x1
	.long	.LASF295
	.byte	0xb
	.value	0x39a
	.byte	0xa
	.long	0x121
	.value	0x65c
	.uleb128 0x1
	.long	.LASF296
	.byte	0xb
	.value	0x39b
	.byte	0x9
	.long	0x71
	.value	0x660
	.uleb128 0x13
	.string	"pbe"
	.byte	0xb
	.value	0x39d
	.byte	0x9
	.long	0x71
	.value	0x664
	.uleb128 0x1
	.long	.LASF297
	.byte	0xb
	.value	0x39e
	.byte	0x9
	.long	0x71
	.value	0x668
	.uleb128 0x1
	.long	.LASF298
	.byte	0xb
	.value	0x3a0
	.byte	0x9
	.long	0x71
	.value	0x66c
	.uleb128 0x1
	.long	.LASF299
	.byte	0xb
	.value	0x3b1
	.byte	0xa
	.long	0x121
	.value	0x670
	.byte	0
	.uleb128 0xb
	.long	0x67e
	.long	0x117f
	.uleb128 0xd
	.long	0x44
	.byte	0x9
	.byte	0
	.uleb128 0xb
	.long	0x118f
	.long	0x118f
	.uleb128 0xd
	.long	0x44
	.byte	0x3
	.byte	0
	.uleb128 0x4
	.long	0x11e8
	.uleb128 0x9
	.long	.LASF300
	.byte	0x30
	.byte	0xc
	.byte	0xa9
	.long	0x11e8
	.uleb128 0x2
	.long	.LASF301
	.byte	0xc
	.byte	0xaa
	.byte	0x1a
	.long	0xd3
	.byte	0
	.uleb128 0x2
	.long	.LASF302
	.byte	0xc
	.byte	0xab
	.byte	0x1a
	.long	0xa8
	.byte	0x8
	.uleb128 0x2
	.long	.LASF303
	.byte	0xc
	.byte	0xac
	.byte	0x1a
	.long	0x71
	.byte	0x10
	.uleb128 0x2
	.long	.LASF304
	.byte	0xc
	.byte	0xad
	.byte	0x1a
	.long	0x1249
	.byte	0x18
	.uleb128 0x2
	.long	.LASF305
	.byte	0xc
	.byte	0xae
	.byte	0x1a
	.long	0x111
	.byte	0x20
	.uleb128 0x16
	.long	0x1570
	.byte	0x28
	.byte	0
	.uleb128 0xc
	.long	0x1194
	.uleb128 0xb
	.long	0x2a
	.long	0x11fd
	.uleb128 0xd
	.long	0x44
	.byte	0x20
	.byte	0
	.uleb128 0xb
	.long	0x2a
	.long	0x120d
	.uleb128 0xd
	.long	0x44
	.byte	0x50
	.byte	0
	.uleb128 0xb
	.long	0x2a
	.long	0x121d
	.uleb128 0xd
	.long	0x44
	.byte	0xa0
	.byte	0
	.uleb128 0xb
	.long	0x2a
	.long	0x122d
	.uleb128 0xd
	.long	0x44
	.byte	0x3f
	.byte	0
	.uleb128 0xb
	.long	0xaf
	.long	0x123d
	.uleb128 0xd
	.long	0x44
	.byte	0x50
	.byte	0
	.uleb128 0xa
	.long	.LASF306
	.byte	0xc
	.byte	0x1d
	.byte	0xf
	.long	0x5e5
	.uleb128 0x22
	.long	.LASF350
	.byte	0x8
	.byte	0xc
	.byte	0x1f
	.byte	0x7
	.long	0x12a4
	.uleb128 0x6
	.long	.LASF307
	.byte	0x20
	.byte	0x9
	.long	0x71
	.uleb128 0x6
	.long	.LASF308
	.byte	0x21
	.byte	0x12
	.long	0x3d
	.uleb128 0x6
	.long	.LASF309
	.byte	0x22
	.byte	0xa
	.long	0x121
	.uleb128 0x6
	.long	.LASF310
	.byte	0x23
	.byte	0xb
	.long	0xaa
	.uleb128 0x6
	.long	.LASF311
	.byte	0x24
	.byte	0x14
	.long	0x15e
	.uleb128 0x6
	.long	.LASF312
	.byte	0x25
	.byte	0x12
	.long	0x123d
	.uleb128 0x6
	.long	.LASF313
	.byte	0x26
	.byte	0xb
	.long	0xa8
	.byte	0
	.uleb128 0x9
	.long	.LASF314
	.byte	0x4
	.byte	0xc
	.byte	0x31
	.long	0x12be
	.uleb128 0x2
	.long	.LASF302
	.byte	0xc
	.byte	0x32
	.byte	0x9
	.long	0x71
	.byte	0
	.byte	0
	.uleb128 0xc
	.long	0x12a4
	.uleb128 0x9
	.long	.LASF315
	.byte	0x18
	.byte	0xc
	.byte	0x37
	.long	0x1304
	.uleb128 0x2
	.long	.LASF316
	.byte	0xc
	.byte	0x38
	.byte	0xc
	.long	0x1314
	.byte	0
	.uleb128 0x2
	.long	.LASF317
	.byte	0xc
	.byte	0x39
	.byte	0x9
	.long	0x71
	.byte	0x8
	.uleb128 0x2
	.long	.LASF318
	.byte	0xc
	.byte	0x3a
	.byte	0x9
	.long	0x71
	.byte	0xc
	.uleb128 0x2
	.long	.LASF319
	.byte	0xc
	.byte	0x3b
	.byte	0x13
	.long	0x111
	.byte	0x10
	.byte	0
	.uleb128 0xc
	.long	0x12c3
	.uleb128 0x11
	.long	0x1314
	.uleb128 0x7
	.long	0x121
	.byte	0
	.uleb128 0x4
	.long	0x1309
	.uleb128 0x9
	.long	.LASF320
	.byte	0x18
	.byte	0xc
	.byte	0x42
	.long	0x134d
	.uleb128 0x2
	.long	.LASF321
	.byte	0xc
	.byte	0x43
	.byte	0x11
	.long	0x111
	.byte	0
	.uleb128 0x2
	.long	.LASF322
	.byte	0xc
	.byte	0x44
	.byte	0x11
	.long	0x111
	.byte	0x8
	.uleb128 0x2
	.long	.LASF323
	.byte	0xc
	.byte	0x45
	.byte	0x9
	.long	0x71
	.byte	0x10
	.byte	0
	.uleb128 0xc
	.long	0x1319
	.uleb128 0x9
	.long	.LASF324
	.byte	0x28
	.byte	0xc
	.byte	0x49
	.long	0x13ba
	.uleb128 0x2
	.long	.LASF316
	.byte	0xc
	.byte	0x4a
	.byte	0xc
	.long	0x12f
	.byte	0
	.uleb128 0x2
	.long	.LASF325
	.byte	0xc
	.byte	0x4b
	.byte	0xd
	.long	0xbb
	.byte	0x8
	.uleb128 0x2
	.long	.LASF326
	.byte	0xc
	.byte	0x4c
	.byte	0xd
	.long	0xbb
	.byte	0xa
	.uleb128 0x10
	.string	"min"
	.byte	0xc
	.byte	0x4d
	.byte	0x9
	.long	0x71
	.byte	0xc
	.uleb128 0x10
	.string	"max"
	.byte	0xc
	.byte	0x4e
	.byte	0x9
	.long	0x71
	.byte	0x10
	.uleb128 0x2
	.long	.LASF327
	.byte	0xc
	.byte	0x50
	.byte	0x13
	.long	0x13dd
	.byte	0x18
	.uleb128 0x2
	.long	.LASF328
	.byte	0xc
	.byte	0x51
	.byte	0xf
	.long	0x13f6
	.byte	0x20
	.byte	0
	.uleb128 0xc
	.long	0x1352
	.uleb128 0x12
	.long	0x111
	.long	0x13dd
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
	.long	0x13bf
	.uleb128 0x12
	.long	0xc7
	.long	0x13f6
	.uleb128 0x7
	.long	0x71
	.uleb128 0x7
	.long	0x71
	.byte	0
	.uleb128 0x4
	.long	0x13e2
	.uleb128 0x17
	.byte	0x5b
	.long	0x1418
	.uleb128 0x6
	.long	.LASF329
	.byte	0x5c
	.byte	0x1f
	.long	0x1418
	.uleb128 0x6
	.long	.LASF330
	.byte	0x5d
	.byte	0x1e
	.long	0x116
	.byte	0
	.uleb128 0x4
	.long	0x5ea
	.uleb128 0x9
	.long	.LASF331
	.byte	0x20
	.byte	0xc
	.byte	0x55
	.long	0x1457
	.uleb128 0x2
	.long	.LASF316
	.byte	0xc
	.byte	0x56
	.byte	0xc
	.long	0x12f
	.byte	0
	.uleb128 0x2
	.long	.LASF332
	.byte	0xc
	.byte	0x57
	.byte	0x9
	.long	0x71
	.byte	0x8
	.uleb128 0x2
	.long	.LASF319
	.byte	0xc
	.byte	0x58
	.byte	0x11
	.long	0x111
	.byte	0x10
	.uleb128 0x16
	.long	0x13fb
	.byte	0x18
	.byte	0
	.uleb128 0xc
	.long	0x141d
	.uleb128 0x9
	.long	.LASF333
	.byte	0x30
	.byte	0xc
	.byte	0x68
	.long	0x14c4
	.uleb128 0x2
	.long	.LASF316
	.byte	0xc
	.byte	0x69
	.byte	0xc
	.long	0x12f
	.byte	0
	.uleb128 0x2
	.long	.LASF327
	.byte	0xc
	.byte	0x6a
	.byte	0x13
	.long	0x13dd
	.byte	0x8
	.uleb128 0x2
	.long	.LASF328
	.byte	0xc
	.byte	0x6b
	.byte	0xf
	.long	0x13f6
	.byte	0x10
	.uleb128 0x2
	.long	.LASF325
	.byte	0xc
	.byte	0x6c
	.byte	0x9
	.long	0x71
	.byte	0x18
	.uleb128 0x2
	.long	.LASF332
	.byte	0xc
	.byte	0x6d
	.byte	0x9
	.long	0x71
	.byte	0x1c
	.uleb128 0x2
	.long	.LASF319
	.byte	0xc
	.byte	0x6e
	.byte	0x11
	.long	0x111
	.byte	0x20
	.uleb128 0x2
	.long	.LASF334
	.byte	0xc
	.byte	0x71
	.byte	0x11
	.long	0x116
	.byte	0x28
	.byte	0
	.uleb128 0xc
	.long	0x145c
	.uleb128 0x9
	.long	.LASF335
	.byte	0x20
	.byte	0xc
	.byte	0x7b
	.long	0x150a
	.uleb128 0x2
	.long	.LASF336
	.byte	0xc
	.byte	0x80
	.byte	0xc
	.long	0x151f
	.byte	0
	.uleb128 0x2
	.long	.LASF337
	.byte	0xc
	.byte	0x86
	.byte	0xd
	.long	0x153d
	.byte	0x8
	.uleb128 0x2
	.long	.LASF338
	.byte	0xc
	.byte	0x8c
	.byte	0xc
	.long	0x1556
	.byte	0x10
	.uleb128 0x2
	.long	.LASF339
	.byte	0xc
	.byte	0x91
	.byte	0xc
	.long	0x156b
	.byte	0x18
	.byte	0
	.uleb128 0xc
	.long	0x14c9
	.uleb128 0x11
	.long	0x151f
	.uleb128 0x7
	.long	0xa8
	.uleb128 0x7
	.long	0xaa
	.byte	0
	.uleb128 0x4
	.long	0x150f
	.uleb128 0x12
	.long	0xaa
	.long	0x153d
	.uleb128 0x7
	.long	0xa8
	.uleb128 0x7
	.long	0xaa
	.uleb128 0x7
	.long	0x71
	.byte	0
	.uleb128 0x4
	.long	0x1524
	.uleb128 0x12
	.long	0x121
	.long	0x1556
	.uleb128 0x7
	.long	0xa8
	.uleb128 0x7
	.long	0xa8
	.byte	0
	.uleb128 0x4
	.long	0x1542
	.uleb128 0x11
	.long	0x156b
	.uleb128 0x7
	.long	0xa8
	.uleb128 0x7
	.long	0xa8
	.byte	0
	.uleb128 0x4
	.long	0x155b
	.uleb128 0x17
	.byte	0xb0
	.long	0x15da
	.uleb128 0x6
	.long	.LASF340
	.byte	0xb1
	.byte	0x15
	.long	0x11b
	.uleb128 0x6
	.long	.LASF314
	.byte	0xb2
	.byte	0x25
	.long	0x15da
	.uleb128 0x6
	.long	.LASF315
	.byte	0xb3
	.byte	0x25
	.long	0x15df
	.uleb128 0x6
	.long	.LASF320
	.byte	0xb4
	.byte	0x28
	.long	0x15e4
	.uleb128 0x6
	.long	.LASF324
	.byte	0xb5
	.byte	0x23
	.long	0x15e9
	.uleb128 0x6
	.long	.LASF331
	.byte	0xb6
	.byte	0x26
	.long	0x15ee
	.uleb128 0x6
	.long	.LASF333
	.byte	0xb7
	.byte	0x25
	.long	0x15f3
	.uleb128 0x6
	.long	.LASF335
	.byte	0xb8
	.byte	0x26
	.long	0x15f8
	.uleb128 0x6
	.long	.LASF319
	.byte	0xb9
	.byte	0x17
	.long	0x111
	.byte	0
	.uleb128 0x4
	.long	0x12be
	.uleb128 0x4
	.long	0x1304
	.uleb128 0x4
	.long	0x134d
	.uleb128 0x4
	.long	0x13ba
	.uleb128 0x4
	.long	0x1457
	.uleb128 0x4
	.long	0x14c4
	.uleb128 0x4
	.long	0x150a
	.uleb128 0x23
	.long	.LASF351
	.byte	0x1
	.byte	0x19
	.byte	0x5
	.long	0x71
	.quad	.LFB96
	.quad	.LFE96-.LFB96
	.uleb128 0x1
	.byte	0x9c
	.uleb128 0xf
	.long	.LASF341
	.byte	0x20
	.byte	0x16
	.long	0x6b0
	.uleb128 0xf
	.long	.LASF342
	.byte	0x2f
	.byte	0x16
	.long	0x785
	.uleb128 0xf
	.long	.LASF343
	.byte	0xdc
	.byte	0x1c
	.long	0x64a
	.uleb128 0xf
	.long	.LASF344
	.byte	0xe0
	.byte	0x18
	.long	0x67e
	.uleb128 0xf
	.long	.LASF345
	.byte	0xe4
	.byte	0x1c
	.long	0x5ef
	.uleb128 0xf
	.long	.LASF346
	.byte	0xeb
	.byte	0x11
	.long	0x163
	.uleb128 0x18
	.long	.LASF347
	.value	0x129
	.byte	0x15
	.long	0x510
	.uleb128 0x18
	.long	.LASF348
	.value	0x12d
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
	.sleb128 11
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
	.sleb128 12
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
	.uleb128 0xb
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
	.sleb128 12
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
	.uleb128 0x5
	.uleb128 0x39
	.uleb128 0xb
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
.LASF227:
	.string	"warnon_erase_dynplaylist"
.LASF41:
	.string	"layer"
.LASF139:
	.string	"crossfade_fade_out_delay"
.LASF43:
	.string	"id3version"
.LASF253:
	.string	"flip_display"
.LASF122:
	.string	"resume_pitch"
.LASF303:
	.string	"lang_id"
.LASF142:
	.string	"crossfade_fade_out_mixmode"
.LASF254:
	.string	"cursor_style"
.LASF57:
	.string	"bytesperframe"
.LASF61:
	.string	"id3v1buf"
.LASF69:
	.string	"playcount"
.LASF309:
	.string	"bool_"
.LASF177:
	.string	"peak_meter_release"
.LASF222:
	.string	"next_folder"
.LASF290:
	.string	"volume_adjust_norm_steps"
.LASF301:
	.string	"flags"
.LASF172:
	.string	"car_adapter_mode_delay"
.LASF145:
	.string	"crossfeed_cross_gain"
.LASF338:
	.string	"is_changed"
.LASF36:
	.string	"comment"
.LASF150:
	.string	"eq_band_settings"
.LASF106:
	.string	"ratio"
.LASF90:
	.string	"AA_FLAG_ID3_UNSYNC"
.LASF196:
	.string	"list_separator_height"
.LASF157:
	.string	"unplug_mode"
.LASF291:
	.string	"surround_enabled"
.LASF9:
	.string	"__int32_t"
.LASF201:
	.string	"list_order"
.LASF77:
	.string	"album_peak"
.LASF78:
	.string	"has_embedded_albumart"
.LASF284:
	.string	"start_directory"
.LASF186:
	.string	"playlist_catalog_dir"
.LASF229:
	.string	"show_shuffled_adding_options"
.LASF144:
	.string	"crossfeed_direct_gain"
.LASF83:
	.string	"mb_track_id"
.LASF316:
	.string	"option_callback"
.LASF224:
	.string	"recursive_dir_insert"
.LASF25:
	.string	"__int128"
.LASF7:
	.string	"short int"
.LASF141:
	.string	"crossfade_fade_out_duration"
.LASF46:
	.string	"frequency"
.LASF180:
	.string	"peak_meter_dbfs"
.LASF278:
	.string	"keypress_restarts_sleeptimer"
.LASF259:
	.string	"icon_file"
.LASF288:
	.string	"volume_limit"
.LASF155:
	.string	"timestretch_enabled"
.LASF245:
	.string	"sort_dir"
.LASF239:
	.string	"talk_file"
.LASF340:
	.string	"RESERVED"
.LASF15:
	.string	"int16_t"
.LASF213:
	.string	"autoresume_automatic"
.LASF105:
	.string	"makeup_gain"
.LASF200:
	.string	"list_wraparound"
.LASF125:
	.string	"topruntime"
.LASF166:
	.string	"default_codepage"
.LASF123:
	.string	"resume_speed"
.LASF28:
	.string	"title"
.LASF302:
	.string	"setting"
.LASF17:
	.string	"uint32_t"
.LASF107:
	.string	"knee"
.LASF31:
	.string	"genre_string"
.LASF26:
	.string	"__int128 unsigned"
.LASF117:
	.string	"volume"
.LASF163:
	.string	"storage_mode"
.LASF269:
	.string	"bl_selective_actions_mask"
.LASF45:
	.string	"bitrate"
.LASF187:
	.string	"skip_length"
.LASF154:
	.string	"dithering_enabled"
.LASF332:
	.string	"count"
.LASF63:
	.string	"index"
.LASF21:
	.string	"long long unsigned int"
.LASF275:
	.string	"ui_vp_config"
.LASF311:
	.string	"ucharptr"
.LASF133:
	.string	"bass"
.LASF131:
	.string	"user_settings"
.LASF323:
	.string	"max_len"
.LASF100:
	.string	"CHAR_ENC_UTF_16_LE"
.LASF32:
	.string	"disc_string"
.LASF128:
	.string	"last_volume_change"
.LASF118:
	.string	"resume_index"
.LASF66:
	.string	"tagcache_idx"
.LASF137:
	.string	"crossfade"
.LASF50:
	.string	"sim_filesize"
.LASF160:
	.string	"timeformat"
.LASF282:
	.string	"hotkey_tree"
.LASF39:
	.string	"discnum"
.LASF244:
	.string	"sort_case"
.LASF324:
	.string	"int_setting"
.LASF319:
	.string	"cfg_vals"
.LASF218:
	.string	"ipone_charge_wallpaper"
.LASF92:
	.string	"mp3entry"
.LASF209:
	.string	"usemrb"
.LASF148:
	.string	"eq_enabled"
.LASF190:
	.string	"volume_type"
.LASF111:
	.string	"noclip"
.LASF58:
	.string	"has_toc"
.LASF283:
	.string	"resume_rewind"
.LASF135:
	.string	"channel_config"
.LASF216:
	.string	"tagcache_scan_paths"
.LASF257:
	.string	"offset_out_of_view"
.LASF321:
	.string	"prefix"
.LASF20:
	.string	"size_t"
.LASF33:
	.string	"track_string"
.LASF62:
	.string	"offset"
.LASF207:
	.string	"autocreatebookmark"
.LASF265:
	.string	"caption_backlight"
.LASF312:
	.string	"func"
.LASF238:
	.string	"talk_dir_clip"
.LASF23:
	.string	"_Bool"
.LASF251:
	.string	"contrast"
.LASF124:
	.string	"runtime"
.LASF89:
	.string	"AA_TYPE_JPG"
.LASF132:
	.string	"balance"
.LASF296:
	.string	"surround_mix"
.LASF49:
	.string	"first_frame_offset"
.LASF53:
	.string	"lead_trim"
.LASF191:
	.string	"battery_display"
.LASF134:
	.string	"treble"
.LASF79:
	.string	"albumart"
.LASF59:
	.string	"needs_upsampling_correction"
.LASF27:
	.string	"path"
.LASF192:
	.string	"show_icons"
.LASF243:
	.string	"talk_mixer_amp"
.LASF337:
	.string	"write_to_cfg"
.LASF48:
	.string	"id3v1len"
.LASF289:
	.string	"volume_adjust_mode"
.LASF232:
	.string	"rewind_across_tracks"
.LASF115:
	.string	"gain"
.LASF38:
	.string	"grouping"
.LASF335:
	.string	"custom_setting"
.LASF114:
	.string	"cutoff"
.LASF93:
	.string	"mp3_albumart"
.LASF266:
	.string	"bl_filter_first_keypress"
.LASF313:
	.string	"custom"
.LASF255:
	.string	"screen_scroll_step"
.LASF233:
	.string	"playlist_viewer_icons"
.LASF287:
	.string	"play_frequency"
.LASF14:
	.string	"char"
.LASF149:
	.string	"eq_precut"
.LASF161:
	.string	"disk_spindown"
.LASF64:
	.string	"skip_resume_adjustments"
.LASF102:
	.string	"encoding"
.LASF81:
	.string	"embedded_cuesheet"
.LASF219:
	.string	"backdrop_file"
.LASF348:
	.string	"section_10"
.LASF181:
	.string	"peak_meter_min"
.LASF12:
	.string	"__uint64_t"
.LASF4:
	.string	"unsigned int"
.LASF252:
	.string	"invert"
.LASF256:
	.string	"show_path_in_browser"
.LASF198:
	.string	"browse_current"
.LASF170:
	.string	"party_mode"
.LASF249:
	.string	"poweroff"
.LASF225:
	.string	"fade_on_stop"
.LASF331:
	.string	"choice_setting"
.LASF212:
	.string	"autoresume_enable"
.LASF231:
	.string	"album_art"
.LASF162:
	.string	"buffer_margin"
.LASF120:
	.string	"resume_elapsed"
.LASF127:
	.string	"viewer_icon_count"
.LASF285:
	.string	"root_menu_customized"
.LASF86:
	.string	"AA_TYPE_UNKNOWN"
.LASF307:
	.string	"int_"
.LASF272:
	.string	"prevent_skip"
.LASF165:
	.string	"show_filename_ext"
.LASF68:
	.string	"score"
.LASF294:
	.string	"surround_fx2"
.LASF55:
	.string	"samples"
.LASF40:
	.string	"tracknum"
.LASF197:
	.string	"list_separator_color"
.LASF22:
	.string	"long long int"
.LASF98:
	.string	"CHAR_ENC_ISO_8859_1"
.LASF350:
	.string	"storage_type"
.LASF147:
	.string	"crossfeed_hf_cutoff"
.LASF138:
	.string	"crossfade_fade_in_delay"
.LASF156:
	.string	"pause_rewind"
.LASF341:
	.string	"section_1"
.LASF342:
	.string	"section_2"
.LASF343:
	.string	"section_3"
.LASF344:
	.string	"section_4"
.LASF345:
	.string	"section_5"
.LASF346:
	.string	"section_8"
.LASF347:
	.string	"section_9"
.LASF333:
	.string	"table_setting"
.LASF44:
	.string	"codectype"
.LASF108:
	.string	"release_time"
.LASF8:
	.string	"__int16_t"
.LASF202:
	.string	"scroll_speed"
.LASF223:
	.string	"constrain_next_folder"
.LASF318:
	.string	"lang_no"
.LASF328:
	.string	"get_talk_id"
.LASF47:
	.string	"id3v2len"
.LASF82:
	.string	"cuesheet"
.LASF113:
	.string	"eq_band_setting"
.LASF195:
	.string	"scrollbar_width"
.LASF326:
	.string	"step"
.LASF54:
	.string	"tail_trim"
.LASF95:
	.string	"size"
.LASF349:
	.string	"GNU C99 15.2.1 20260209 -mtune=generic -march=x86-64 -g -Os -std=gnu99 -funit-at-a-time -fno-delete-null-pointer-checks -fno-strict-overflow -fno-common -fno-builtin"
.LASF336:
	.string	"load_from_cfg"
.LASF140:
	.string	"crossfade_fade_in_duration"
.LASF184:
	.string	"sbs_file"
.LASF67:
	.string	"rating"
.LASF248:
	.string	"interpret_numbers"
.LASF327:
	.string	"formatter"
.LASF74:
	.string	"track_gain"
.LASF334:
	.string	"values"
.LASF97:
	.string	"character_encoding"
.LASF112:
	.string	"preamp"
.LASF101:
	.string	"CHAR_ENC_UTF_16_BE"
.LASF85:
	.string	"has_video"
.LASF88:
	.string	"AA_TYPE_PNG"
.LASF298:
	.string	"afr_enabled"
.LASF325:
	.string	"unit"
.LASF339:
	.string	"set_default"
.LASF214:
	.string	"autoresume_paths"
.LASF228:
	.string	"keep_current_track_on_replace_playlist"
.LASF146:
	.string	"crossfeed_hf_attenuation"
.LASF60:
	.string	"id3v2buf"
.LASF330:
	.string	"talks"
.LASF300:
	.string	"settings_list"
.LASF35:
	.string	"composer"
.LASF171:
	.string	"car_adapter_mode"
.LASF185:
	.string	"lang_file"
.LASF136:
	.string	"stereo_width"
.LASF73:
	.string	"album_level"
.LASF273:
	.string	"pitch_mode_semitone"
.LASF169:
	.string	"single_mode"
.LASF164:
	.string	"dirfilter"
.LASF76:
	.string	"track_peak"
.LASF234:
	.string	"playlist_viewer_indices"
.LASF322:
	.string	"suffix"
.LASF280:
	.string	"morse_input"
.LASF116:
	.string	"system_status"
.LASF84:
	.string	"is_asf_stream"
.LASF130:
	.string	"resume_modified"
.LASF258:
	.string	"disable_mainmenu_scrolling"
.LASF237:
	.string	"talk_dir"
.LASF11:
	.string	"long int"
.LASF109:
	.string	"attack_time"
.LASF286:
	.string	"shortcuts_replaces_qs"
.LASF208:
	.string	"autoupdatebookmark"
.LASF51:
	.string	"length"
.LASF267:
	.string	"backlight_timeout_plugged"
.LASF247:
	.string	"sort_playlists"
.LASF242:
	.string	"talk_battery_level"
.LASF206:
	.string	"autoloadbookmark"
.LASF56:
	.string	"frame_count"
.LASF96:
	.string	"mp3_aa_type"
.LASF175:
	.string	"ff_rewind_min_step"
.LASF71:
	.string	"playtime"
.LASF305:
	.string	"cfg_name"
.LASF183:
	.string	"wps_file"
.LASF18:
	.string	"uint64_t"
.LASF246:
	.string	"sort_file"
.LASF121:
	.string	"resume_offset"
.LASF262:
	.string	"glyphs_to_cache"
.LASF261:
	.string	"font_file"
.LASF310:
	.string	"charptr"
.LASF210:
	.string	"tagcache_ram"
.LASF189:
	.string	"max_files_in_playlist"
.LASF236:
	.string	"talk_menu"
.LASF221:
	.string	"repeat_mode"
.LASF188:
	.string	"max_files_in_dir"
.LASF215:
	.string	"runtimedb"
.LASF29:
	.string	"artist"
.LASF159:
	.string	"qs_items"
.LASF292:
	.string	"surround_balance"
.LASF276:
	.string	"sleeptimer_duration"
.LASF268:
	.string	"bl_selective_actions"
.LASF241:
	.string	"talk_filetype"
.LASF30:
	.string	"album"
.LASF194:
	.string	"scrollbar"
.LASF5:
	.string	"long unsigned int"
.LASF264:
	.string	"backlight_timeout"
.LASF173:
	.string	"start_in_screen"
.LASF176:
	.string	"ff_rewind_accel"
.LASF235:
	.string	"playlist_viewer_track_display"
.LASF274:
	.string	"pitch_mode_timestretch"
.LASF42:
	.string	"year"
.LASF99:
	.string	"CHAR_ENC_UTF_8"
.LASF271:
	.string	"accessory_supply"
.LASF240:
	.string	"talk_file_clip"
.LASF119:
	.string	"resume_crc32"
.LASF203:
	.string	"bidir_limit"
.LASF293:
	.string	"surround_fx1"
.LASF94:
	.string	"type"
.LASF129:
	.string	"font_id"
.LASF2:
	.string	"unsigned char"
.LASF72:
	.string	"track_level"
.LASF10:
	.string	"__uint32_t"
.LASF103:
	.string	"compressor_settings"
.LASF75:
	.string	"album_gain"
.LASF199:
	.string	"scroll_paginated"
.LASF168:
	.string	"play_selected"
.LASF314:
	.string	"sound_setting"
.LASF37:
	.string	"albumartist"
.LASF299:
	.string	"playback_log"
.LASF260:
	.string	"viewers_icon_file"
.LASF34:
	.string	"year_string"
.LASF304:
	.string	"default_val"
.LASF104:
	.string	"threshold"
.LASF306:
	.string	"_isfunc_type"
.LASF315:
	.string	"bool_setting"
.LASF193:
	.string	"statusbar"
.LASF110:
	.string	"replaygain_settings"
.LASF13:
	.string	"__off_t"
.LASF143:
	.string	"crossfeed"
.LASF151:
	.string	"beep"
.LASF126:
	.string	"last_screen"
.LASF277:
	.string	"sleeptimer_on_startup"
.LASF226:
	.string	"playlist_shuffle"
.LASF6:
	.string	"signed char"
.LASF19:
	.string	"off_t"
.LASF3:
	.string	"short unsigned int"
.LASF281:
	.string	"hotkey_wps"
.LASF153:
	.string	"keyclick_repeats"
.LASF279:
	.string	"show_shutdown_message"
.LASF351:
	.string	"main"
.LASF217:
	.string	"tagcache_db_path"
.LASF182:
	.string	"peak_meter_max"
.LASF230:
	.string	"show_queue_options"
.LASF211:
	.string	"tagcache_autoupdate"
.LASF297:
	.string	"pbe_precut"
.LASF65:
	.string	"autoresumable"
.LASF52:
	.string	"elapsed"
.LASF80:
	.string	"has_embedded_cuesheet"
.LASF24:
	.string	"double"
.LASF220:
	.string	"browser_default"
.LASF91:
	.string	"AA_FLAG_VORBIS_BASE64"
.LASF270:
	.string	"backlight_on_button_hold"
.LASF204:
	.string	"scroll_delay"
.LASF16:
	.string	"int32_t"
.LASF174:
	.string	"wps_select_action"
.LASF263:
	.string	"kbd_file"
.LASF70:
	.string	"lastplayed"
.LASF329:
	.string	"desc"
.LASF250:
	.string	"battery_capacity"
.LASF308:
	.string	"uint_"
.LASF158:
	.string	"unplug_autoresume"
.LASF87:
	.string	"AA_TYPE_BMP"
.LASF295:
	.string	"surround_method2"
.LASF178:
	.string	"peak_meter_hold"
.LASF167:
	.string	"hold_lr_for_scroll_in_list"
.LASF179:
	.string	"peak_meter_clip_hold"
.LASF152:
	.string	"keyclick"
.LASF317:
	.string	"lang_yes"
.LASF320:
	.string	"filename_setting"
.LASF205:
	.string	"scroll_step"
	.section	.debug_line_str,"MS",@progbits,1
.LASF0:
	.string	"<stdin>"
.LASF1:
	.string	"/home/david/Documents/RockBox_Personal-master/build-sim"
	.ident	"GCC: (GNU) 15.2.1 20260209"
	.section	.note.GNU-stack,"",@progbits
