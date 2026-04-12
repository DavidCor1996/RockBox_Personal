	.cpu arm7tdmi
	.arch armv4t
	.fpu softvfp
	.eabi_attribute 20, 1
	.eabi_attribute 21, 1
	.eabi_attribute 23, 3
	.eabi_attribute 24, 1
	.eabi_attribute 25, 1
	.eabi_attribute 26, 1
	.eabi_attribute 30, 4
	.eabi_attribute 34, 0
	.eabi_attribute 18, 4
	.file	""
	.text
	.section	.text.startup,"ax",%progbits
	.align	2
	.global	main
	.syntax unified
	.arm
	.type	main, %function
main:
	@ Function supports interworking.
	@ args = 0, pretend = 0, frame = 0
	@ frame_needed = 0, uses_anonymous_args = 0
	@ link register save eliminated.
	.syntax unified
@ 30 "<stdin>" 1
	/* LUA_RB_SETTINGS_H_HELPER, , struct system_status, struct user_settings, struct replaygain_settings, struct eq_band_setting, struct compressor_settings, struct mp3_enc_config, , struct mp3entry, struct mp3_albumart, struct embedded_cuesheet; */
@ 0 "" 2
@ 33 "<stdin>" 1
	/* "struct system_status", "volume" = #0, #4, "i", #0; */

@ 0 "" 2
@ 34 "<stdin>" 1
	/* "struct system_status", "resume_index" = #4, #4, "i", #0; */

@ 0 "" 2
@ 35 "<stdin>" 1
	/* "struct system_status", "resume_crc32" = #8, #4, "u_l", #0; */

@ 0 "" 2
@ 36 "<stdin>" 1
	/* "struct system_status", "resume_elapsed" = #12, #4, "u_l", #0; */

@ 0 "" 2
@ 37 "<stdin>" 1
	/* "struct system_status", "resume_offset" = #16, #4, "u_l", #0; */

@ 0 "" 2
@ 38 "<stdin>" 1
	/* "struct system_status", "resume_pitch" = #20, #4, "l", #0; */

@ 0 "" 2
@ 39 "<stdin>" 1
	/* "struct system_status", "resume_speed" = #24, #4, "l", #0; */

@ 0 "" 2
@ 40 "<stdin>" 1
	/* "struct system_status", "runtime" = #28, #4, "i", #0; */

@ 0 "" 2
@ 41 "<stdin>" 1
	/* "struct system_status", "topruntime" = #32, #4, "i", #0; */

@ 0 "" 2
@ 42 "<stdin>" 1
	/* "struct system_status", "dircache_size" = #36, #4, "i", #0; */

@ 0 "" 2
@ 43 "<stdin>" 1
	/* "struct system_status", "last_frequency" = #40, #4, "i", #0; */

@ 0 "" 2
@ 44 "<stdin>" 1
	/* "struct system_status", "last_screen" = #44, #1, "c", #0; */

@ 0 "" 2
@ 45 "<stdin>" 1
	/* "struct system_status", "viewer_icon_count" = #48, #4, "i", #0; */

@ 0 "" 2
@ 46 "<stdin>" 1
	/* "struct system_status", "last_volume_change" = #52, #4, "i", #0; */

@ 0 "" 2
@ 47 "<stdin>" 1
	/* "struct system_status", "font_id" = #56, #4, "i_typeisarray_", #1; */

@ 0 "" 2
@ 48 "<stdin>" 1
	/* "struct system_status", "resume_modified" = #60, #1, "b", #0; */

@ 0 "" 2
@ 50 "<stdin>" 1
	/* "struct user_settings", "balance" = #0, #4, "i", #0; */

@ 0 "" 2
@ 51 "<stdin>" 1
	/* "struct user_settings", "bass" = #4, #4, "i", #0; */

@ 0 "" 2
@ 52 "<stdin>" 1
	/* "struct user_settings", "treble" = #8, #4, "i", #0; */

@ 0 "" 2
@ 53 "<stdin>" 1
	/* "struct user_settings", "channel_config" = #12, #4, "i", #0; */

@ 0 "" 2
@ 54 "<stdin>" 1
	/* "struct user_settings", "stereo_width" = #16, #4, "i", #0; */

@ 0 "" 2
@ 55 "<stdin>" 1
	/* "struct user_settings", "bass_cutoff" = #20, #4, "i", #0; */

@ 0 "" 2
@ 56 "<stdin>" 1
	/* "struct user_settings", "treble_cutoff" = #24, #4, "i", #0; */

@ 0 "" 2
@ 57 "<stdin>" 1
	/* "struct user_settings", "crossfade" = #28, #4, "i", #0; */

@ 0 "" 2
@ 58 "<stdin>" 1
	/* "struct user_settings", "crossfade_fade_in_delay" = #32, #4, "i", #0; */

@ 0 "" 2
@ 59 "<stdin>" 1
	/* "struct user_settings", "crossfade_fade_out_delay" = #36, #4, "i", #0; */

@ 0 "" 2
@ 60 "<stdin>" 1
	/* "struct user_settings", "crossfade_fade_in_duration" = #40, #4, "i", #0; */

@ 0 "" 2
@ 61 "<stdin>" 1
	/* "struct user_settings", "crossfade_fade_out_duration" = #44, #4, "i", #0; */

@ 0 "" 2
@ 62 "<stdin>" 1
	/* "struct user_settings", "crossfade_fade_out_mixmode" = #48, #4, "i", #0; */

@ 0 "" 2
@ 63 "<stdin>" 1
	/* "struct user_settings", "replaygain_settings" = #52, #12, "s_replaygain_settings", #0; */

@ 0 "" 2
@ 64 "<stdin>" 1
	/* "struct user_settings", "crossfeed" = #64, #4, "i", #0; */

@ 0 "" 2
@ 65 "<stdin>" 1
	/* "struct user_settings", "crossfeed_direct_gain" = #68, #4, "u_i", #0; */

@ 0 "" 2
@ 66 "<stdin>" 1
	/* "struct user_settings", "crossfeed_cross_gain" = #72, #4, "u_i", #0; */

@ 0 "" 2
@ 67 "<stdin>" 1
	/* "struct user_settings", "crossfeed_hf_attenuation" = #76, #4, "u_i", #0; */

@ 0 "" 2
@ 68 "<stdin>" 1
	/* "struct user_settings", "crossfeed_hf_cutoff" = #80, #4, "u_i", #0; */

@ 0 "" 2
@ 69 "<stdin>" 1
	/* "struct user_settings", "eq_enabled" = #84, #1, "b", #0; */

@ 0 "" 2
@ 70 "<stdin>" 1
	/* "struct user_settings", "eq_precut" = #88, #4, "u_i", #0; */

@ 0 "" 2
@ 71 "<stdin>" 1
	/* "struct user_settings", "eq_band_settings" = #92, #120, "s_eq_band_setting_typeisarray_", #10; */

@ 0 "" 2
@ 72 "<stdin>" 1
	/* "struct user_settings", "beep" = #212, #4, "i", #0; */

@ 0 "" 2
@ 73 "<stdin>" 1
	/* "struct user_settings", "keyclick" = #216, #4, "i", #0; */

@ 0 "" 2
@ 74 "<stdin>" 1
	/* "struct user_settings", "keyclick_repeats" = #220, #4, "i", #0; */

@ 0 "" 2
@ 75 "<stdin>" 1
	/* "struct user_settings", "dithering_enabled" = #224, #1, "b", #0; */

@ 0 "" 2
@ 76 "<stdin>" 1
	/* "struct user_settings", "timestretch_enabled" = #225, #1, "b", #0; */

@ 0 "" 2
@ 77 "<stdin>" 1
	/* "struct user_settings", "rec_format" = #228, #4, "i", #0; */

@ 0 "" 2
@ 78 "<stdin>" 1
	/* "struct user_settings", "rec_mono_mode" = #232, #4, "i", #0; */

@ 0 "" 2
@ 79 "<stdin>" 1
	/* "struct user_settings", "mp3_enc_config" = #236, #4, "s_mp3_enc_config", #0; */

@ 0 "" 2
@ 80 "<stdin>" 1
	/* "struct user_settings", "rec_source" = #240, #4, "i", #0; */

@ 0 "" 2
@ 81 "<stdin>" 1
	/* "struct user_settings", "rec_frequency" = #244, #4, "i", #0; */

@ 0 "" 2
@ 82 "<stdin>" 1
	/* "struct user_settings", "rec_channels" = #248, #4, "i", #0; */

@ 0 "" 2
@ 83 "<stdin>" 1
	/* "struct user_settings", "rec_mic_gain" = #252, #4, "i", #0; */

@ 0 "" 2
@ 84 "<stdin>" 1
	/* "struct user_settings", "rec_left_gain" = #256, #4, "i", #0; */

@ 0 "" 2
@ 85 "<stdin>" 1
	/* "struct user_settings", "rec_right_gain" = #260, #4, "i", #0; */

@ 0 "" 2
@ 86 "<stdin>" 1
	/* "struct user_settings", "peak_meter_clipcounter" = #264, #1, "b", #0; */

@ 0 "" 2
@ 87 "<stdin>" 1
	/* "struct user_settings", "rec_editable" = #265, #1, "b", #0; */

@ 0 "" 2
@ 88 "<stdin>" 1
	/* "struct user_settings", "rec_timesplit" = #268, #4, "i", #0; */

@ 0 "" 2
@ 89 "<stdin>" 1
	/* "struct user_settings", "rec_sizesplit" = #272, #4, "i", #0; */

@ 0 "" 2
@ 90 "<stdin>" 1
	/* "struct user_settings", "rec_split_type" = #276, #4, "i", #0; */

@ 0 "" 2
@ 91 "<stdin>" 1
	/* "struct user_settings", "rec_split_method" = #280, #4, "i", #0; */

@ 0 "" 2
@ 92 "<stdin>" 1
	/* "struct user_settings", "rec_prerecord_time" = #284, #4, "i", #0; */

@ 0 "" 2
@ 93 "<stdin>" 1
	/* "struct user_settings", "rec_directory" = #288, #81, "str", #0; */

@ 0 "" 2
@ 94 "<stdin>" 1
	/* "struct user_settings", "cliplight" = #372, #4, "i", #0; */

@ 0 "" 2
@ 95 "<stdin>" 1
	/* "struct user_settings", "rec_start_thres_db" = #376, #4, "i", #0; */

@ 0 "" 2
@ 96 "<stdin>" 1
	/* "struct user_settings", "rec_start_thres_linear" = #380, #4, "i", #0; */

@ 0 "" 2
@ 97 "<stdin>" 1
	/* "struct user_settings", "rec_start_duration" = #384, #4, "i", #0; */

@ 0 "" 2
@ 98 "<stdin>" 1
	/* "struct user_settings", "rec_stop_thres_db" = #388, #4, "i", #0; */

@ 0 "" 2
@ 99 "<stdin>" 1
	/* "struct user_settings", "rec_stop_thres_linear" = #392, #4, "i", #0; */

@ 0 "" 2
@ 100 "<stdin>" 1
	/* "struct user_settings", "rec_stop_postrec" = #396, #4, "i", #0; */

@ 0 "" 2
@ 101 "<stdin>" 1
	/* "struct user_settings", "rec_stop_gap" = #400, #4, "i", #0; */

@ 0 "" 2
@ 102 "<stdin>" 1
	/* "struct user_settings", "rec_trigger_mode" = #404, #4, "i", #0; */

@ 0 "" 2
@ 103 "<stdin>" 1
	/* "struct user_settings", "rec_trigger_type" = #408, #4, "i", #0; */

@ 0 "" 2
@ 104 "<stdin>" 1
	/* "struct user_settings", "fm_region" = #412, #4, "i", #0; */

@ 0 "" 2
@ 105 "<stdin>" 1
	/* "struct user_settings", "fm_force_mono" = #416, #1, "b", #0; */

@ 0 "" 2
@ 106 "<stdin>" 1
	/* "struct user_settings", "fmr_file" = #417, #33, "str", #0; */

@ 0 "" 2
@ 107 "<stdin>" 1
	/* "struct user_settings", "fms_file" = #450, #33, "str", #0; */

@ 0 "" 2
@ 108 "<stdin>" 1
	/* "struct user_settings", "sync_rds_time" = #483, #1, "b", #0; */

@ 0 "" 2
@ 109 "<stdin>" 1
	/* "struct user_settings", "pause_rewind" = #484, #4, "i", #0; */

@ 0 "" 2
@ 110 "<stdin>" 1
	/* "struct user_settings", "unplug_mode" = #488, #4, "i", #0; */

@ 0 "" 2
@ 111 "<stdin>" 1
	/* "struct user_settings", "unplug_autoresume" = #492, #1, "b", #0; */

@ 0 "" 2
@ 112 "<stdin>" 1
	/* "struct user_settings", "qs_items" = #496, #16, "ptr_const_struct_typeisarray_settings_list", #0; */

@ 0 "" 2
@ 113 "<stdin>" 1
	/* "struct user_settings", "timeformat" = #512, #4, "i", #0; */

@ 0 "" 2
@ 114 "<stdin>" 1
	/* "struct user_settings", "disk_spindown" = #516, #4, "i", #0; */

@ 0 "" 2
@ 115 "<stdin>" 1
	/* "struct user_settings", "buffer_margin" = #520, #4, "i", #0; */

@ 0 "" 2
@ 116 "<stdin>" 1
	/* "struct user_settings", "storage_mode" = #524, #4, "i", #0; */

@ 0 "" 2
@ 117 "<stdin>" 1
	/* "struct user_settings", "dirfilter" = #528, #4, "i", #0; */

@ 0 "" 2
@ 118 "<stdin>" 1
	/* "struct user_settings", "show_filename_ext" = #532, #4, "i", #0; */

@ 0 "" 2
@ 119 "<stdin>" 1
	/* "struct user_settings", "default_codepage" = #536, #4, "i", #0; */

@ 0 "" 2
@ 120 "<stdin>" 1
	/* "struct user_settings", "hold_lr_for_scroll_in_list" = #540, #1, "b", #0; */

@ 0 "" 2
@ 121 "<stdin>" 1
	/* "struct user_settings", "play_selected" = #541, #1, "b", #0; */

@ 0 "" 2
@ 122 "<stdin>" 1
	/* "struct user_settings", "single_mode" = #544, #4, "i", #0; */

@ 0 "" 2
@ 123 "<stdin>" 1
	/* "struct user_settings", "party_mode" = #548, #1, "b", #0; */

@ 0 "" 2
@ 124 "<stdin>" 1
	/* "struct user_settings", "cuesheet" = #549, #1, "b", #0; */

@ 0 "" 2
@ 125 "<stdin>" 1
	/* "struct user_settings", "car_adapter_mode" = #550, #1, "b", #0; */

@ 0 "" 2
@ 126 "<stdin>" 1
	/* "struct user_settings", "car_adapter_mode_delay" = #552, #4, "i", #0; */

@ 0 "" 2
@ 127 "<stdin>" 1
	/* "struct user_settings", "start_in_screen" = #556, #4, "i", #0; */

@ 0 "" 2
@ 128 "<stdin>" 1
	/* "struct user_settings", "wps_select_action" = #560, #4, "i", #0; */

@ 0 "" 2
@ 129 "<stdin>" 1
	/* "struct user_settings", "alarm_wake_up_screen" = #564, #4, "i", #0; */

@ 0 "" 2
@ 130 "<stdin>" 1
	/* "struct user_settings", "ff_rewind_min_step" = #568, #4, "i", #0; */

@ 0 "" 2
@ 131 "<stdin>" 1
	/* "struct user_settings", "ff_rewind_accel" = #572, #4, "i", #0; */

@ 0 "" 2
@ 132 "<stdin>" 1
	/* "struct user_settings", "peak_meter_release" = #576, #4, "i", #0; */

@ 0 "" 2
@ 133 "<stdin>" 1
	/* "struct user_settings", "peak_meter_hold" = #580, #4, "i", #0; */

@ 0 "" 2
@ 134 "<stdin>" 1
	/* "struct user_settings", "peak_meter_clip_hold" = #584, #4, "i", #0; */

@ 0 "" 2
@ 135 "<stdin>" 1
	/* "struct user_settings", "peak_meter_dbfs" = #588, #1, "b", #0; */

@ 0 "" 2
@ 136 "<stdin>" 1
	/* "struct user_settings", "peak_meter_min" = #592, #4, "i", #0; */

@ 0 "" 2
@ 137 "<stdin>" 1
	/* "struct user_settings", "peak_meter_max" = #596, #4, "i", #0; */

@ 0 "" 2
@ 138 "<stdin>" 1
	/* "struct user_settings", "wps_file" = #600, #33, "str", #0; */

@ 0 "" 2
@ 139 "<stdin>" 1
	/* "struct user_settings", "sbs_file" = #633, #33, "str", #0; */

@ 0 "" 2
@ 140 "<stdin>" 1
	/* "struct user_settings", "lang_file" = #666, #33, "str", #0; */

@ 0 "" 2
@ 141 "<stdin>" 1
	/* "struct user_settings", "playlist_catalog_dir" = #699, #81, "str", #0; */

@ 0 "" 2
@ 142 "<stdin>" 1
	/* "struct user_settings", "skip_length" = #780, #4, "i", #0; */

@ 0 "" 2
@ 143 "<stdin>" 1
	/* "struct user_settings", "max_files_in_dir" = #784, #4, "i", #0; */

@ 0 "" 2
@ 144 "<stdin>" 1
	/* "struct user_settings", "max_files_in_playlist" = #788, #4, "i", #0; */

@ 0 "" 2
@ 145 "<stdin>" 1
	/* "struct user_settings", "volume_type" = #792, #4, "i", #0; */

@ 0 "" 2
@ 146 "<stdin>" 1
	/* "struct user_settings", "battery_display" = #796, #4, "i", #0; */

@ 0 "" 2
@ 147 "<stdin>" 1
	/* "struct user_settings", "show_icons" = #800, #1, "b", #0; */

@ 0 "" 2
@ 148 "<stdin>" 1
	/* "struct user_settings", "statusbar" = #804, #4, "i", #0; */

@ 0 "" 2
@ 149 "<stdin>" 1
	/* "struct user_settings", "scrollbar" = #808, #4, "i", #0; */

@ 0 "" 2
@ 150 "<stdin>" 1
	/* "struct user_settings", "scrollbar_width" = #812, #4, "i", #0; */

@ 0 "" 2
@ 151 "<stdin>" 1
	/* "struct user_settings", "list_separator_height" = #816, #4, "i", #0; */

@ 0 "" 2
@ 152 "<stdin>" 1
	/* "struct user_settings", "list_separator_color" = #820, #4, "i", #0; */

@ 0 "" 2
@ 153 "<stdin>" 1
	/* "struct user_settings", "browse_current" = #824, #1, "b", #0; */

@ 0 "" 2
@ 154 "<stdin>" 1
	/* "struct user_settings", "scroll_paginated" = #825, #1, "b", #0; */

@ 0 "" 2
@ 155 "<stdin>" 1
	/* "struct user_settings", "list_wraparound" = #826, #1, "b", #0; */

@ 0 "" 2
@ 156 "<stdin>" 1
	/* "struct user_settings", "list_order" = #828, #4, "i", #0; */

@ 0 "" 2
@ 157 "<stdin>" 1
	/* "struct user_settings", "scroll_speed" = #832, #4, "i", #0; */

@ 0 "" 2
@ 158 "<stdin>" 1
	/* "struct user_settings", "bidir_limit" = #836, #4, "i", #0; */

@ 0 "" 2
@ 159 "<stdin>" 1
	/* "struct user_settings", "scroll_delay" = #840, #4, "i", #0; */

@ 0 "" 2
@ 160 "<stdin>" 1
	/* "struct user_settings", "scroll_step" = #844, #4, "i", #0; */

@ 0 "" 2
@ 161 "<stdin>" 1
	/* "struct user_settings", "autoloadbookmark" = #848, #4, "i", #0; */

@ 0 "" 2
@ 162 "<stdin>" 1
	/* "struct user_settings", "autocreatebookmark" = #852, #4, "i", #0; */

@ 0 "" 2
@ 163 "<stdin>" 1
	/* "struct user_settings", "autoupdatebookmark" = #856, #1, "b", #0; */

@ 0 "" 2
@ 164 "<stdin>" 1
	/* "struct user_settings", "usemrb" = #860, #4, "i", #0; */

@ 0 "" 2
@ 165 "<stdin>" 1
	/* "struct user_settings", "dircache" = #864, #1, "b", #0; */

@ 0 "" 2
@ 166 "<stdin>" 1
	/* "struct user_settings", "tagcache_ram" = #868, #4, "i", #0; */

@ 0 "" 2
@ 167 "<stdin>" 1
	/* "struct user_settings", "tagcache_autoupdate" = #872, #1, "b", #0; */

@ 0 "" 2
@ 168 "<stdin>" 1
	/* "struct user_settings", "autoresume_enable" = #873, #1, "b", #0; */

@ 0 "" 2
@ 169 "<stdin>" 1
	/* "struct user_settings", "autoresume_automatic" = #876, #4, "i", #0; */

@ 0 "" 2
@ 170 "<stdin>" 1
	/* "struct user_settings", "autoresume_paths" = #880, #161, "str", #0; */

@ 0 "" 2
@ 171 "<stdin>" 1
	/* "struct user_settings", "runtimedb" = #1041, #1, "b", #0; */

@ 0 "" 2
@ 172 "<stdin>" 1
	/* "struct user_settings", "tagcache_scan_paths" = #1042, #161, "str", #0; */

@ 0 "" 2
@ 173 "<stdin>" 1
	/* "struct user_settings", "tagcache_db_path" = #1203, #81, "str", #0; */

@ 0 "" 2
@ 174 "<stdin>" 1
	/* "struct user_settings", "backdrop_file" = #1284, #81, "str", #0; */

@ 0 "" 2
@ 175 "<stdin>" 1
	/* "struct user_settings", "bg_color" = #1368, #4, "i", #0; */

@ 0 "" 2
@ 176 "<stdin>" 1
	/* "struct user_settings", "fg_color" = #1372, #4, "i", #0; */

@ 0 "" 2
@ 177 "<stdin>" 1
	/* "struct user_settings", "lss_color" = #1376, #4, "i", #0; */

@ 0 "" 2
@ 178 "<stdin>" 1
	/* "struct user_settings", "lse_color" = #1380, #4, "i", #0; */

@ 0 "" 2
@ 179 "<stdin>" 1
	/* "struct user_settings", "lst_color" = #1384, #4, "i", #0; */

@ 0 "" 2
@ 180 "<stdin>" 1
	/* "struct user_settings", "ipone_charge_wallpaper" = #1388, #4, "i", #0; */

@ 0 "" 2
@ 181 "<stdin>" 1
	/* "struct user_settings", "colors_file" = #1392, #33, "str", #0; */

@ 0 "" 2
@ 182 "<stdin>" 1
	/* "struct user_settings", "dynamic_colors" = #1425, #1, "b", #0; */

@ 0 "" 2
@ 183 "<stdin>" 1
	/* "struct user_settings", "browser_default" = #1428, #4, "i", #0; */

@ 0 "" 2
@ 184 "<stdin>" 1
	/* "struct user_settings", "repeat_mode" = #1432, #4, "i", #0; */

@ 0 "" 2
@ 185 "<stdin>" 1
	/* "struct user_settings", "next_folder" = #1436, #4, "i", #0; */

@ 0 "" 2
@ 186 "<stdin>" 1
	/* "struct user_settings", "constrain_next_folder" = #1440, #1, "b", #0; */

@ 0 "" 2
@ 187 "<stdin>" 1
	/* "struct user_settings", "recursive_dir_insert" = #1444, #4, "i", #0; */

@ 0 "" 2
@ 188 "<stdin>" 1
	/* "struct user_settings", "fade_on_stop" = #1448, #1, "b", #0; */

@ 0 "" 2
@ 189 "<stdin>" 1
	/* "struct user_settings", "playlist_shuffle" = #1449, #1, "b", #0; */

@ 0 "" 2
@ 190 "<stdin>" 1
	/* "struct user_settings", "warnon_erase_dynplaylist" = #1450, #1, "b", #0; */

@ 0 "" 2
@ 191 "<stdin>" 1
	/* "struct user_settings", "keep_current_track_on_replace_playlist" = #1451, #1, "b", #0; */

@ 0 "" 2
@ 192 "<stdin>" 1
	/* "struct user_settings", "show_shuffled_adding_options" = #1452, #1, "b", #0; */

@ 0 "" 2
@ 193 "<stdin>" 1
	/* "struct user_settings", "show_queue_options" = #1456, #4, "i", #0; */

@ 0 "" 2
@ 194 "<stdin>" 1
	/* "struct user_settings", "album_art" = #1460, #4, "i", #0; */

@ 0 "" 2
@ 195 "<stdin>" 1
	/* "struct user_settings", "rewind_across_tracks" = #1464, #1, "b", #0; */

@ 0 "" 2
@ 196 "<stdin>" 1
	/* "struct user_settings", "playlist_viewer_icons" = #1465, #1, "b", #0; */

@ 0 "" 2
@ 197 "<stdin>" 1
	/* "struct user_settings", "playlist_viewer_indices" = #1466, #1, "b", #0; */

@ 0 "" 2
@ 198 "<stdin>" 1
	/* "struct user_settings", "playlist_viewer_track_display" = #1468, #4, "i", #0; */

@ 0 "" 2
@ 199 "<stdin>" 1
	/* "struct user_settings", "talk_menu" = #1472, #1, "b", #0; */

@ 0 "" 2
@ 200 "<stdin>" 1
	/* "struct user_settings", "talk_dir" = #1476, #4, "i", #0; */

@ 0 "" 2
@ 201 "<stdin>" 1
	/* "struct user_settings", "talk_dir_clip" = #1480, #1, "b", #0; */

@ 0 "" 2
@ 202 "<stdin>" 1
	/* "struct user_settings", "talk_file" = #1484, #4, "i", #0; */

@ 0 "" 2
@ 203 "<stdin>" 1
	/* "struct user_settings", "talk_file_clip" = #1488, #1, "b", #0; */

@ 0 "" 2
@ 204 "<stdin>" 1
	/* "struct user_settings", "talk_filetype" = #1489, #1, "b", #0; */

@ 0 "" 2
@ 205 "<stdin>" 1
	/* "struct user_settings", "talk_battery_level" = #1490, #1, "b", #0; */

@ 0 "" 2
@ 206 "<stdin>" 1
	/* "struct user_settings", "talk_mixer_amp" = #1492, #4, "i", #0; */

@ 0 "" 2
@ 207 "<stdin>" 1
	/* "struct user_settings", "sort_case" = #1496, #1, "b", #0; */

@ 0 "" 2
@ 208 "<stdin>" 1
	/* "struct user_settings", "sort_dir" = #1500, #4, "i", #0; */

@ 0 "" 2
@ 209 "<stdin>" 1
	/* "struct user_settings", "sort_file" = #1504, #4, "i", #0; */

@ 0 "" 2
@ 210 "<stdin>" 1
	/* "struct user_settings", "sort_playlists" = #1508, #4, "i", #0; */

@ 0 "" 2
@ 211 "<stdin>" 1
	/* "struct user_settings", "interpret_numbers" = #1512, #4, "i", #0; */

@ 0 "" 2
@ 212 "<stdin>" 1
	/* "struct user_settings", "poweroff" = #1516, #4, "i", #0; */

@ 0 "" 2
@ 213 "<stdin>" 1
	/* "struct user_settings", "battery_capacity" = #1520, #4, "i", #0; */

@ 0 "" 2
@ 214 "<stdin>" 1
	/* "struct user_settings", "usb_charging" = #1524, #4, "i", #0; */

@ 0 "" 2
@ 215 "<stdin>" 1
	/* "struct user_settings", "cursor_style" = #1528, #4, "i", #0; */

@ 0 "" 2
@ 216 "<stdin>" 1
	/* "struct user_settings", "screen_scroll_step" = #1532, #4, "i", #0; */

@ 0 "" 2
@ 217 "<stdin>" 1
	/* "struct user_settings", "show_path_in_browser" = #1536, #4, "i", #0; */

@ 0 "" 2
@ 218 "<stdin>" 1
	/* "struct user_settings", "offset_out_of_view" = #1540, #1, "b", #0; */

@ 0 "" 2
@ 219 "<stdin>" 1
	/* "struct user_settings", "disable_mainmenu_scrolling" = #1541, #1, "b", #0; */

@ 0 "" 2
@ 220 "<stdin>" 1
	/* "struct user_settings", "icon_file" = #1542, #33, "str", #0; */

@ 0 "" 2
@ 221 "<stdin>" 1
	/* "struct user_settings", "viewers_icon_file" = #1575, #33, "str", #0; */

@ 0 "" 2
@ 222 "<stdin>" 1
	/* "struct user_settings", "font_file" = #1608, #33, "str", #0; */

@ 0 "" 2
@ 223 "<stdin>" 1
	/* "struct user_settings", "glyphs_to_cache" = #1644, #4, "i", #0; */

@ 0 "" 2
@ 224 "<stdin>" 1
	/* "struct user_settings", "kbd_file" = #1648, #33, "str", #0; */

@ 0 "" 2
@ 225 "<stdin>" 1
	/* "struct user_settings", "backlight_timeout" = #1684, #4, "i", #0; */

@ 0 "" 2
@ 226 "<stdin>" 1
	/* "struct user_settings", "caption_backlight" = #1688, #1, "b", #0; */

@ 0 "" 2
@ 227 "<stdin>" 1
	/* "struct user_settings", "bl_filter_first_keypress" = #1689, #1, "b", #0; */

@ 0 "" 2
@ 228 "<stdin>" 1
	/* "struct user_settings", "backlight_timeout_plugged" = #1692, #4, "i", #0; */

@ 0 "" 2
@ 229 "<stdin>" 1
	/* "struct user_settings", "bl_selective_actions" = #1696, #1, "b", #0; */

@ 0 "" 2
@ 230 "<stdin>" 1
	/* "struct user_settings", "bl_selective_actions_mask" = #1700, #4, "i", #0; */

@ 0 "" 2
@ 231 "<stdin>" 1
	/* "struct user_settings", "backlight_on_button_hold" = #1704, #4, "i", #0; */

@ 0 "" 2
@ 232 "<stdin>" 1
	/* "struct user_settings", "lcd_sleep_after_backlight_off" = #1708, #4, "i", #0; */

@ 0 "" 2
@ 233 "<stdin>" 1
	/* "struct user_settings", "backlight_fade_in" = #1712, #4, "i", #0; */

@ 0 "" 2
@ 234 "<stdin>" 1
	/* "struct user_settings", "backlight_fade_out" = #1716, #4, "i", #0; */

@ 0 "" 2
@ 235 "<stdin>" 1
	/* "struct user_settings", "brightness" = #1720, #4, "i", #0; */

@ 0 "" 2
@ 236 "<stdin>" 1
	/* "struct user_settings", "serial_bitrate" = #1724, #4, "i", #0; */

@ 0 "" 2
@ 237 "<stdin>" 1
	/* "struct user_settings", "accessory_supply" = #1728, #1, "b", #0; */

@ 0 "" 2
@ 238 "<stdin>" 1
	/* "struct user_settings", "lineout_active" = #1729, #1, "b", #0; */

@ 0 "" 2
@ 239 "<stdin>" 1
	/* "struct user_settings", "prevent_skip" = #1730, #1, "b", #0; */

@ 0 "" 2
@ 240 "<stdin>" 1
	/* "struct user_settings", "pitch_mode_semitone" = #1731, #1, "b", #0; */

@ 0 "" 2
@ 241 "<stdin>" 1
	/* "struct user_settings", "pitch_mode_timestretch" = #1732, #1, "b", #0; */

@ 0 "" 2
@ 242 "<stdin>" 1
	/* "struct user_settings", "usb_hid" = #1733, #1, "b", #0; */

@ 0 "" 2
@ 243 "<stdin>" 1
	/* "struct user_settings", "usb_keypad_mode" = #1736, #4, "i", #0; */

@ 0 "" 2
@ 244 "<stdin>" 1
	/* "struct user_settings", "usb_audio" = #1740, #4, "i", #0; */

@ 0 "" 2
@ 245 "<stdin>" 1
	/* "struct user_settings", "ui_vp_config" = #1744, #64, "str", #0; */

@ 0 "" 2
@ 246 "<stdin>" 1
	/* "struct user_settings", "compressor_settings" = #1808, #24, "s_compressor_settings", #0; */

@ 0 "" 2
@ 247 "<stdin>" 1
	/* "struct user_settings", "sleeptimer_duration" = #1832, #4, "i", #0; */

@ 0 "" 2
@ 248 "<stdin>" 1
	/* "struct user_settings", "sleeptimer_on_startup" = #1836, #1, "b", #0; */

@ 0 "" 2
@ 249 "<stdin>" 1
	/* "struct user_settings", "keypress_restarts_sleeptimer" = #1837, #1, "b", #0; */

@ 0 "" 2
@ 250 "<stdin>" 1
	/* "struct user_settings", "show_shutdown_message" = #1838, #1, "b", #0; */

@ 0 "" 2
@ 251 "<stdin>" 1
	/* "struct user_settings", "morse_input" = #1839, #1, "b", #0; */

@ 0 "" 2
@ 252 "<stdin>" 1
	/* "struct user_settings", "hotkey_wps" = #1840, #4, "i", #0; */

@ 0 "" 2
@ 253 "<stdin>" 1
	/* "struct user_settings", "hotkey_tree" = #1844, #4, "i", #0; */

@ 0 "" 2
@ 254 "<stdin>" 1
	/* "struct user_settings", "resume_rewind" = #1848, #4, "i", #0; */

@ 0 "" 2
@ 255 "<stdin>" 1
	/* "struct user_settings", "keyclick_hardware" = #1852, #1, "b", #0; */

@ 0 "" 2
@ 256 "<stdin>" 1
	/* "struct user_settings", "start_directory" = #1853, #81, "str", #0; */

@ 0 "" 2
@ 257 "<stdin>" 1
	/* "struct user_settings", "root_menu_customized" = #1934, #1, "b", #0; */

@ 0 "" 2
@ 258 "<stdin>" 1
	/* "struct user_settings", "shortcuts_replaces_qs" = #1935, #1, "b", #0; */

@ 0 "" 2
@ 259 "<stdin>" 1
	/* "struct user_settings", "play_frequency" = #1936, #4, "i", #0; */

@ 0 "" 2
@ 260 "<stdin>" 1
	/* "struct user_settings", "volume_limit" = #1940, #4, "i", #0; */

@ 0 "" 2
@ 261 "<stdin>" 1
	/* "struct user_settings", "volume_adjust_mode" = #1944, #4, "i", #0; */

@ 0 "" 2
@ 262 "<stdin>" 1
	/* "struct user_settings", "volume_adjust_norm_steps" = #1948, #4, "i", #0; */

@ 0 "" 2
@ 263 "<stdin>" 1
	/* "struct user_settings", "surround_enabled" = #1952, #4, "i", #0; */

@ 0 "" 2
@ 264 "<stdin>" 1
	/* "struct user_settings", "surround_balance" = #1956, #4, "i", #0; */

@ 0 "" 2
@ 265 "<stdin>" 1
	/* "struct user_settings", "surround_fx1" = #1960, #4, "i", #0; */

@ 0 "" 2
@ 266 "<stdin>" 1
	/* "struct user_settings", "surround_fx2" = #1964, #4, "i", #0; */

@ 0 "" 2
@ 267 "<stdin>" 1
	/* "struct user_settings", "surround_method2" = #1968, #1, "b", #0; */

@ 0 "" 2
@ 268 "<stdin>" 1
	/* "struct user_settings", "surround_mix" = #1972, #4, "i", #0; */

@ 0 "" 2
@ 269 "<stdin>" 1
	/* "struct user_settings", "pbe" = #1976, #4, "i", #0; */

@ 0 "" 2
@ 270 "<stdin>" 1
	/* "struct user_settings", "pbe_precut" = #1980, #4, "i", #0; */

@ 0 "" 2
@ 271 "<stdin>" 1
	/* "struct user_settings", "afr_enabled" = #1984, #4, "i", #0; */

@ 0 "" 2
@ 272 "<stdin>" 1
	/* "struct user_settings", "usb_mode" = #1988, #4, "i", #0; */

@ 0 "" 2
@ 273 "<stdin>" 1
	/* "struct user_settings", "clear_settings_on_hold" = #1992, #1, "b", #0; */

@ 0 "" 2
@ 274 "<stdin>" 1
	/* "struct user_settings", "playback_log" = #1993, #1, "b", #0; */

@ 0 "" 2
@ 276 "<stdin>" 1
	/* "struct replaygain_settings", "noclip" = #0, #1, "b", #0; */

@ 0 "" 2
@ 277 "<stdin>" 1
	/* "struct replaygain_settings", "type" = #4, #4, "i", #0; */

@ 0 "" 2
@ 278 "<stdin>" 1
	/* "struct replaygain_settings", "preamp" = #8, #4, "i", #0; */

@ 0 "" 2
@ 280 "<stdin>" 1
	/* "struct eq_band_setting", "cutoff" = #0, #4, "i", #0; */

@ 0 "" 2
@ 281 "<stdin>" 1
	/* "struct eq_band_setting", "q" = #4, #4, "i", #0; */

@ 0 "" 2
@ 282 "<stdin>" 1
	/* "struct eq_band_setting", "gain" = #8, #4, "i", #0; */

@ 0 "" 2
@ 284 "<stdin>" 1
	/* "struct compressor_settings", "threshold" = #0, #4, "i", #0; */

@ 0 "" 2
@ 285 "<stdin>" 1
	/* "struct compressor_settings", "makeup_gain" = #4, #4, "i", #0; */

@ 0 "" 2
@ 286 "<stdin>" 1
	/* "struct compressor_settings", "ratio" = #8, #4, "i", #0; */

@ 0 "" 2
@ 287 "<stdin>" 1
	/* "struct compressor_settings", "knee" = #12, #4, "i", #0; */

@ 0 "" 2
@ 288 "<stdin>" 1
	/* "struct compressor_settings", "release_time" = #16, #4, "i", #0; */

@ 0 "" 2
@ 289 "<stdin>" 1
	/* "struct compressor_settings", "attack_time" = #20, #4, "i", #0; */

@ 0 "" 2
@ 291 "<stdin>" 1
	/* "struct mp3_enc_config", "bitrate" = #0, #4, "u_l", #0; */

@ 0 "" 2
@ 293 "<stdin>" 1
	/* "struct mp3entry", "path" = #0, #260, "str", #0; */

@ 0 "" 2
@ 294 "<stdin>" 1
	/* "struct mp3entry", "title" = #260, #4, "ptr_char", #0; */

@ 0 "" 2
@ 295 "<stdin>" 1
	/* "struct mp3entry", "artist" = #264, #4, "ptr_char", #0; */

@ 0 "" 2
@ 296 "<stdin>" 1
	/* "struct mp3entry", "album" = #268, #4, "ptr_char", #0; */

@ 0 "" 2
@ 297 "<stdin>" 1
	/* "struct mp3entry", "genre_string" = #272, #4, "ptr_char", #0; */

@ 0 "" 2
@ 298 "<stdin>" 1
	/* "struct mp3entry", "disc_string" = #276, #4, "ptr_char", #0; */

@ 0 "" 2
@ 299 "<stdin>" 1
	/* "struct mp3entry", "track_string" = #280, #4, "ptr_char", #0; */

@ 0 "" 2
@ 300 "<stdin>" 1
	/* "struct mp3entry", "year_string" = #284, #4, "ptr_char", #0; */

@ 0 "" 2
@ 301 "<stdin>" 1
	/* "struct mp3entry", "composer" = #288, #4, "ptr_char", #0; */

@ 0 "" 2
@ 302 "<stdin>" 1
	/* "struct mp3entry", "comment" = #292, #4, "ptr_char", #0; */

@ 0 "" 2
@ 303 "<stdin>" 1
	/* "struct mp3entry", "albumartist" = #296, #4, "ptr_char", #0; */

@ 0 "" 2
@ 304 "<stdin>" 1
	/* "struct mp3entry", "grouping" = #300, #4, "ptr_char", #0; */

@ 0 "" 2
@ 305 "<stdin>" 1
	/* "struct mp3entry", "discnum" = #304, #4, "i", #0; */

@ 0 "" 2
@ 306 "<stdin>" 1
	/* "struct mp3entry", "tracknum" = #308, #4, "i", #0; */

@ 0 "" 2
@ 307 "<stdin>" 1
	/* "struct mp3entry", "layer" = #312, #4, "i", #0; */

@ 0 "" 2
@ 308 "<stdin>" 1
	/* "struct mp3entry", "year" = #316, #4, "i", #0; */

@ 0 "" 2
@ 309 "<stdin>" 1
	/* "struct mp3entry", "id3version" = #320, #1, "u_c", #0; */

@ 0 "" 2
@ 310 "<stdin>" 1
	/* "struct mp3entry", "codectype" = #324, #4, "u_i", #0; */

@ 0 "" 2
@ 311 "<stdin>" 1
	/* "struct mp3entry", "bitrate" = #328, #4, "u_i", #0; */

@ 0 "" 2
@ 312 "<stdin>" 1
	/* "struct mp3entry", "frequency" = #332, #4, "u_l", #0; */

@ 0 "" 2
@ 313 "<stdin>" 1
	/* "struct mp3entry", "id3v2len" = #336, #4, "u_l", #0; */

@ 0 "" 2
@ 314 "<stdin>" 1
	/* "struct mp3entry", "id3v1len" = #340, #4, "u_l", #0; */

@ 0 "" 2
@ 315 "<stdin>" 1
	/* "struct mp3entry", "first_frame_offset" = #344, #4, "u_l", #0; */

@ 0 "" 2
@ 316 "<stdin>" 1
	/* "struct mp3entry", "filesize" = #348, #4, "u_l", #0; */

@ 0 "" 2
@ 317 "<stdin>" 1
	/* "struct mp3entry", "length" = #352, #4, "u_l", #0; */

@ 0 "" 2
@ 318 "<stdin>" 1
	/* "struct mp3entry", "elapsed" = #356, #4, "u_l", #0; */

@ 0 "" 2
@ 319 "<stdin>" 1
	/* "struct mp3entry", "lead_trim" = #360, #4, "i", #0; */

@ 0 "" 2
@ 320 "<stdin>" 1
	/* "struct mp3entry", "tail_trim" = #364, #4, "i", #0; */

@ 0 "" 2
@ 321 "<stdin>" 1
	/* "struct mp3entry", "samples" = #368, #8, "u_llong", #0; */

@ 0 "" 2
@ 322 "<stdin>" 1
	/* "struct mp3entry", "frame_count" = #376, #4, "u_l", #0; */

@ 0 "" 2
@ 323 "<stdin>" 1
	/* "struct mp3entry", "bytesperframe" = #380, #4, "u_l", #0; */

@ 0 "" 2
@ 324 "<stdin>" 1
	/* "struct mp3entry", "vbr" = #384, #1, "b", #0; */

@ 0 "" 2
@ 325 "<stdin>" 1
	/* "struct mp3entry", "has_toc" = #385, #1, "b", #0; */

@ 0 "" 2
@ 326 "<stdin>" 1
	/* "struct mp3entry", "toc" = #386, #100, "str", #0; */

@ 0 "" 2
@ 327 "<stdin>" 1
	/* "struct mp3entry", "needs_upsampling_correction" = #486, #1, "b", #0; */

@ 0 "" 2
@ 328 "<stdin>" 1
	/* "struct mp3entry", "id3v2buf" = #487, #1800, "str", #0; */

@ 0 "" 2
@ 329 "<stdin>" 1
	/* "struct mp3entry", "id3v1buf" = #2287, #368, "str", #0; */

@ 0 "" 2
@ 330 "<stdin>" 1
	/* "struct mp3entry", "offset" = #2656, #4, "u_l", #0; */

@ 0 "" 2
@ 331 "<stdin>" 1
	/* "struct mp3entry", "index" = #2660, #4, "i", #0; */

@ 0 "" 2
@ 332 "<stdin>" 1
	/* "struct mp3entry", "skip_resume_adjustments" = #2664, #1, "b", #0; */

@ 0 "" 2
@ 333 "<stdin>" 1
	/* "struct mp3entry", "autoresumable" = #2665, #1, "u_c", #0; */

@ 0 "" 2
@ 334 "<stdin>" 1
	/* "struct mp3entry", "tagcache_idx" = #2668, #4, "l", #0; */

@ 0 "" 2
@ 335 "<stdin>" 1
	/* "struct mp3entry", "rating" = #2672, #4, "i", #0; */

@ 0 "" 2
@ 336 "<stdin>" 1
	/* "struct mp3entry", "score" = #2676, #4, "i", #0; */

@ 0 "" 2
@ 337 "<stdin>" 1
	/* "struct mp3entry", "playcount" = #2680, #4, "l", #0; */

@ 0 "" 2
@ 338 "<stdin>" 1
	/* "struct mp3entry", "lastplayed" = #2684, #4, "l", #0; */

@ 0 "" 2
@ 339 "<stdin>" 1
	/* "struct mp3entry", "playtime" = #2688, #4, "l", #0; */

@ 0 "" 2
@ 340 "<stdin>" 1
	/* "struct mp3entry", "track_level" = #2692, #4, "l", #0; */

@ 0 "" 2
@ 341 "<stdin>" 1
	/* "struct mp3entry", "album_level" = #2696, #4, "l", #0; */

@ 0 "" 2
@ 342 "<stdin>" 1
	/* "struct mp3entry", "track_gain" = #2700, #4, "l", #0; */

@ 0 "" 2
@ 343 "<stdin>" 1
	/* "struct mp3entry", "album_gain" = #2704, #4, "l", #0; */

@ 0 "" 2
@ 344 "<stdin>" 1
	/* "struct mp3entry", "track_peak" = #2708, #4, "l", #0; */

@ 0 "" 2
@ 345 "<stdin>" 1
	/* "struct mp3entry", "album_peak" = #2712, #4, "l", #0; */

@ 0 "" 2
@ 346 "<stdin>" 1
	/* "struct mp3entry", "has_embedded_albumart" = #2716, #1, "b", #0; */

@ 0 "" 2
@ 347 "<stdin>" 1
	/* "struct mp3entry", "albumart" = #2720, #12, "s_mp3_albumart", #0; */

@ 0 "" 2
@ 348 "<stdin>" 1
	/* "struct mp3entry", "has_embedded_cuesheet" = #2732, #1, "b", #0; */

@ 0 "" 2
@ 349 "<stdin>" 1
	/* "struct mp3entry", "embedded_cuesheet" = #2736, #12, "s_embedded_cuesheet", #0; */

@ 0 "" 2
@ 350 "<stdin>" 1
	/* "struct mp3entry", "cuesheet" = #2748, #4, "ptr_s_cuesheet", #0; */

@ 0 "" 2
@ 351 "<stdin>" 1
	/* "struct mp3entry", "mb_track_id" = #2752, #4, "ptr_char", #0; */

@ 0 "" 2
@ 352 "<stdin>" 1
	/* "struct mp3entry", "is_asf_stream" = #2756, #1, "b", #0; */

@ 0 "" 2
@ 353 "<stdin>" 1
	/* "struct mp3entry", "has_video" = #2757, #1, "b", #0; */

@ 0 "" 2
@ 355 "<stdin>" 1
	/* "struct mp3_albumart", "type" = #0, #1, "e_mp3_aa_type", #0; */

@ 0 "" 2
@ 356 "<stdin>" 1
	/* "struct mp3_albumart", "size" = #4, #4, "i", #0; */

@ 0 "" 2
@ 357 "<stdin>" 1
	/* "struct mp3_albumart", "pos" = #8, #4, "off_t", #0; */

@ 0 "" 2
@ 359 "<stdin>" 1
	/* "struct embedded_cuesheet", "size" = #0, #4, "i", #0; */

@ 0 "" 2
@ 360 "<stdin>" 1
	/* "struct embedded_cuesheet", "pos" = #4, #4, "off_t", #0; */

@ 0 "" 2
@ 361 "<stdin>" 1
	/* "struct embedded_cuesheet", "encoding" = #8, #1, "e_character_encoding", #0; */

@ 0 "" 2
	.arm
	.syntax unified
	mov	r0, #0
	bx	lr
	.size	main, .-main
	.ident	"GCC: (GNU) 9.5.0"
