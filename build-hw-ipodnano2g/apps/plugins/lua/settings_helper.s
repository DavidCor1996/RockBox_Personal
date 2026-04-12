	.cpu arm920t
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
	/* "struct user_settings", "crossfade" = #20, #4, "i", #0; */

@ 0 "" 2
@ 56 "<stdin>" 1
	/* "struct user_settings", "crossfade_fade_in_delay" = #24, #4, "i", #0; */

@ 0 "" 2
@ 57 "<stdin>" 1
	/* "struct user_settings", "crossfade_fade_out_delay" = #28, #4, "i", #0; */

@ 0 "" 2
@ 58 "<stdin>" 1
	/* "struct user_settings", "crossfade_fade_in_duration" = #32, #4, "i", #0; */

@ 0 "" 2
@ 59 "<stdin>" 1
	/* "struct user_settings", "crossfade_fade_out_duration" = #36, #4, "i", #0; */

@ 0 "" 2
@ 60 "<stdin>" 1
	/* "struct user_settings", "crossfade_fade_out_mixmode" = #40, #4, "i", #0; */

@ 0 "" 2
@ 61 "<stdin>" 1
	/* "struct user_settings", "replaygain_settings" = #44, #12, "s_replaygain_settings", #0; */

@ 0 "" 2
@ 62 "<stdin>" 1
	/* "struct user_settings", "crossfeed" = #56, #4, "i", #0; */

@ 0 "" 2
@ 63 "<stdin>" 1
	/* "struct user_settings", "crossfeed_direct_gain" = #60, #4, "u_i", #0; */

@ 0 "" 2
@ 64 "<stdin>" 1
	/* "struct user_settings", "crossfeed_cross_gain" = #64, #4, "u_i", #0; */

@ 0 "" 2
@ 65 "<stdin>" 1
	/* "struct user_settings", "crossfeed_hf_attenuation" = #68, #4, "u_i", #0; */

@ 0 "" 2
@ 66 "<stdin>" 1
	/* "struct user_settings", "crossfeed_hf_cutoff" = #72, #4, "u_i", #0; */

@ 0 "" 2
@ 67 "<stdin>" 1
	/* "struct user_settings", "eq_enabled" = #76, #1, "b", #0; */

@ 0 "" 2
@ 68 "<stdin>" 1
	/* "struct user_settings", "eq_precut" = #80, #4, "u_i", #0; */

@ 0 "" 2
@ 69 "<stdin>" 1
	/* "struct user_settings", "eq_band_settings" = #84, #120, "s_eq_band_setting_typeisarray_", #10; */

@ 0 "" 2
@ 70 "<stdin>" 1
	/* "struct user_settings", "beep" = #204, #4, "i", #0; */

@ 0 "" 2
@ 71 "<stdin>" 1
	/* "struct user_settings", "keyclick" = #208, #4, "i", #0; */

@ 0 "" 2
@ 72 "<stdin>" 1
	/* "struct user_settings", "keyclick_repeats" = #212, #4, "i", #0; */

@ 0 "" 2
@ 73 "<stdin>" 1
	/* "struct user_settings", "dithering_enabled" = #216, #1, "b", #0; */

@ 0 "" 2
@ 74 "<stdin>" 1
	/* "struct user_settings", "timestretch_enabled" = #217, #1, "b", #0; */

@ 0 "" 2
@ 75 "<stdin>" 1
	/* "struct user_settings", "fm_region" = #220, #4, "i", #0; */

@ 0 "" 2
@ 76 "<stdin>" 1
	/* "struct user_settings", "fm_force_mono" = #224, #1, "b", #0; */

@ 0 "" 2
@ 77 "<stdin>" 1
	/* "struct user_settings", "fmr_file" = #225, #33, "str", #0; */

@ 0 "" 2
@ 78 "<stdin>" 1
	/* "struct user_settings", "fms_file" = #258, #33, "str", #0; */

@ 0 "" 2
@ 79 "<stdin>" 1
	/* "struct user_settings", "sync_rds_time" = #291, #1, "b", #0; */

@ 0 "" 2
@ 80 "<stdin>" 1
	/* "struct user_settings", "pause_rewind" = #292, #4, "i", #0; */

@ 0 "" 2
@ 81 "<stdin>" 1
	/* "struct user_settings", "unplug_mode" = #296, #4, "i", #0; */

@ 0 "" 2
@ 82 "<stdin>" 1
	/* "struct user_settings", "unplug_autoresume" = #300, #1, "b", #0; */

@ 0 "" 2
@ 83 "<stdin>" 1
	/* "struct user_settings", "qs_items" = #304, #16, "ptr_const_struct_typeisarray_settings_list", #0; */

@ 0 "" 2
@ 84 "<stdin>" 1
	/* "struct user_settings", "timeformat" = #320, #4, "i", #0; */

@ 0 "" 2
@ 85 "<stdin>" 1
	/* "struct user_settings", "dirfilter" = #324, #4, "i", #0; */

@ 0 "" 2
@ 86 "<stdin>" 1
	/* "struct user_settings", "show_filename_ext" = #328, #4, "i", #0; */

@ 0 "" 2
@ 87 "<stdin>" 1
	/* "struct user_settings", "default_codepage" = #332, #4, "i", #0; */

@ 0 "" 2
@ 88 "<stdin>" 1
	/* "struct user_settings", "hold_lr_for_scroll_in_list" = #336, #1, "b", #0; */

@ 0 "" 2
@ 89 "<stdin>" 1
	/* "struct user_settings", "play_selected" = #337, #1, "b", #0; */

@ 0 "" 2
@ 90 "<stdin>" 1
	/* "struct user_settings", "single_mode" = #340, #4, "i", #0; */

@ 0 "" 2
@ 91 "<stdin>" 1
	/* "struct user_settings", "party_mode" = #344, #1, "b", #0; */

@ 0 "" 2
@ 92 "<stdin>" 1
	/* "struct user_settings", "cuesheet" = #345, #1, "b", #0; */

@ 0 "" 2
@ 93 "<stdin>" 1
	/* "struct user_settings", "car_adapter_mode" = #346, #1, "b", #0; */

@ 0 "" 2
@ 94 "<stdin>" 1
	/* "struct user_settings", "car_adapter_mode_delay" = #348, #4, "i", #0; */

@ 0 "" 2
@ 95 "<stdin>" 1
	/* "struct user_settings", "start_in_screen" = #352, #4, "i", #0; */

@ 0 "" 2
@ 96 "<stdin>" 1
	/* "struct user_settings", "wps_select_action" = #356, #4, "i", #0; */

@ 0 "" 2
@ 97 "<stdin>" 1
	/* "struct user_settings", "ff_rewind_min_step" = #360, #4, "i", #0; */

@ 0 "" 2
@ 98 "<stdin>" 1
	/* "struct user_settings", "ff_rewind_accel" = #364, #4, "i", #0; */

@ 0 "" 2
@ 99 "<stdin>" 1
	/* "struct user_settings", "peak_meter_release" = #368, #4, "i", #0; */

@ 0 "" 2
@ 100 "<stdin>" 1
	/* "struct user_settings", "peak_meter_hold" = #372, #4, "i", #0; */

@ 0 "" 2
@ 101 "<stdin>" 1
	/* "struct user_settings", "peak_meter_clip_hold" = #376, #4, "i", #0; */

@ 0 "" 2
@ 102 "<stdin>" 1
	/* "struct user_settings", "peak_meter_dbfs" = #380, #1, "b", #0; */

@ 0 "" 2
@ 103 "<stdin>" 1
	/* "struct user_settings", "peak_meter_min" = #384, #4, "i", #0; */

@ 0 "" 2
@ 104 "<stdin>" 1
	/* "struct user_settings", "peak_meter_max" = #388, #4, "i", #0; */

@ 0 "" 2
@ 105 "<stdin>" 1
	/* "struct user_settings", "wps_file" = #392, #33, "str", #0; */

@ 0 "" 2
@ 106 "<stdin>" 1
	/* "struct user_settings", "sbs_file" = #425, #33, "str", #0; */

@ 0 "" 2
@ 107 "<stdin>" 1
	/* "struct user_settings", "lang_file" = #458, #33, "str", #0; */

@ 0 "" 2
@ 108 "<stdin>" 1
	/* "struct user_settings", "playlist_catalog_dir" = #491, #81, "str", #0; */

@ 0 "" 2
@ 109 "<stdin>" 1
	/* "struct user_settings", "skip_length" = #572, #4, "i", #0; */

@ 0 "" 2
@ 110 "<stdin>" 1
	/* "struct user_settings", "max_files_in_dir" = #576, #4, "i", #0; */

@ 0 "" 2
@ 111 "<stdin>" 1
	/* "struct user_settings", "max_files_in_playlist" = #580, #4, "i", #0; */

@ 0 "" 2
@ 112 "<stdin>" 1
	/* "struct user_settings", "volume_type" = #584, #4, "i", #0; */

@ 0 "" 2
@ 113 "<stdin>" 1
	/* "struct user_settings", "battery_display" = #588, #4, "i", #0; */

@ 0 "" 2
@ 114 "<stdin>" 1
	/* "struct user_settings", "show_icons" = #592, #1, "b", #0; */

@ 0 "" 2
@ 115 "<stdin>" 1
	/* "struct user_settings", "statusbar" = #596, #4, "i", #0; */

@ 0 "" 2
@ 116 "<stdin>" 1
	/* "struct user_settings", "scrollbar" = #600, #4, "i", #0; */

@ 0 "" 2
@ 117 "<stdin>" 1
	/* "struct user_settings", "scrollbar_width" = #604, #4, "i", #0; */

@ 0 "" 2
@ 118 "<stdin>" 1
	/* "struct user_settings", "list_separator_height" = #608, #4, "i", #0; */

@ 0 "" 2
@ 119 "<stdin>" 1
	/* "struct user_settings", "list_separator_color" = #612, #4, "i", #0; */

@ 0 "" 2
@ 120 "<stdin>" 1
	/* "struct user_settings", "browse_current" = #616, #1, "b", #0; */

@ 0 "" 2
@ 121 "<stdin>" 1
	/* "struct user_settings", "scroll_paginated" = #617, #1, "b", #0; */

@ 0 "" 2
@ 122 "<stdin>" 1
	/* "struct user_settings", "list_wraparound" = #618, #1, "b", #0; */

@ 0 "" 2
@ 123 "<stdin>" 1
	/* "struct user_settings", "list_order" = #620, #4, "i", #0; */

@ 0 "" 2
@ 124 "<stdin>" 1
	/* "struct user_settings", "scroll_speed" = #624, #4, "i", #0; */

@ 0 "" 2
@ 125 "<stdin>" 1
	/* "struct user_settings", "bidir_limit" = #628, #4, "i", #0; */

@ 0 "" 2
@ 126 "<stdin>" 1
	/* "struct user_settings", "scroll_delay" = #632, #4, "i", #0; */

@ 0 "" 2
@ 127 "<stdin>" 1
	/* "struct user_settings", "scroll_step" = #636, #4, "i", #0; */

@ 0 "" 2
@ 128 "<stdin>" 1
	/* "struct user_settings", "autoloadbookmark" = #640, #4, "i", #0; */

@ 0 "" 2
@ 129 "<stdin>" 1
	/* "struct user_settings", "autocreatebookmark" = #644, #4, "i", #0; */

@ 0 "" 2
@ 130 "<stdin>" 1
	/* "struct user_settings", "autoupdatebookmark" = #648, #1, "b", #0; */

@ 0 "" 2
@ 131 "<stdin>" 1
	/* "struct user_settings", "usemrb" = #652, #4, "i", #0; */

@ 0 "" 2
@ 132 "<stdin>" 1
	/* "struct user_settings", "dircache" = #656, #1, "b", #0; */

@ 0 "" 2
@ 133 "<stdin>" 1
	/* "struct user_settings", "tagcache_ram" = #660, #4, "i", #0; */

@ 0 "" 2
@ 134 "<stdin>" 1
	/* "struct user_settings", "tagcache_autoupdate" = #664, #1, "b", #0; */

@ 0 "" 2
@ 135 "<stdin>" 1
	/* "struct user_settings", "autoresume_enable" = #665, #1, "b", #0; */

@ 0 "" 2
@ 136 "<stdin>" 1
	/* "struct user_settings", "autoresume_automatic" = #668, #4, "i", #0; */

@ 0 "" 2
@ 137 "<stdin>" 1
	/* "struct user_settings", "autoresume_paths" = #672, #161, "str", #0; */

@ 0 "" 2
@ 138 "<stdin>" 1
	/* "struct user_settings", "runtimedb" = #833, #1, "b", #0; */

@ 0 "" 2
@ 139 "<stdin>" 1
	/* "struct user_settings", "tagcache_scan_paths" = #834, #161, "str", #0; */

@ 0 "" 2
@ 140 "<stdin>" 1
	/* "struct user_settings", "tagcache_db_path" = #995, #81, "str", #0; */

@ 0 "" 2
@ 141 "<stdin>" 1
	/* "struct user_settings", "backdrop_file" = #1076, #81, "str", #0; */

@ 0 "" 2
@ 142 "<stdin>" 1
	/* "struct user_settings", "bg_color" = #1160, #4, "i", #0; */

@ 0 "" 2
@ 143 "<stdin>" 1
	/* "struct user_settings", "fg_color" = #1164, #4, "i", #0; */

@ 0 "" 2
@ 144 "<stdin>" 1
	/* "struct user_settings", "lss_color" = #1168, #4, "i", #0; */

@ 0 "" 2
@ 145 "<stdin>" 1
	/* "struct user_settings", "lse_color" = #1172, #4, "i", #0; */

@ 0 "" 2
@ 146 "<stdin>" 1
	/* "struct user_settings", "lst_color" = #1176, #4, "i", #0; */

@ 0 "" 2
@ 147 "<stdin>" 1
	/* "struct user_settings", "ipone_charge_wallpaper" = #1180, #4, "i", #0; */

@ 0 "" 2
@ 148 "<stdin>" 1
	/* "struct user_settings", "colors_file" = #1184, #33, "str", #0; */

@ 0 "" 2
@ 149 "<stdin>" 1
	/* "struct user_settings", "dynamic_colors" = #1217, #1, "b", #0; */

@ 0 "" 2
@ 150 "<stdin>" 1
	/* "struct user_settings", "browser_default" = #1220, #4, "i", #0; */

@ 0 "" 2
@ 151 "<stdin>" 1
	/* "struct user_settings", "repeat_mode" = #1224, #4, "i", #0; */

@ 0 "" 2
@ 152 "<stdin>" 1
	/* "struct user_settings", "next_folder" = #1228, #4, "i", #0; */

@ 0 "" 2
@ 153 "<stdin>" 1
	/* "struct user_settings", "constrain_next_folder" = #1232, #1, "b", #0; */

@ 0 "" 2
@ 154 "<stdin>" 1
	/* "struct user_settings", "recursive_dir_insert" = #1236, #4, "i", #0; */

@ 0 "" 2
@ 155 "<stdin>" 1
	/* "struct user_settings", "fade_on_stop" = #1240, #1, "b", #0; */

@ 0 "" 2
@ 156 "<stdin>" 1
	/* "struct user_settings", "playlist_shuffle" = #1241, #1, "b", #0; */

@ 0 "" 2
@ 157 "<stdin>" 1
	/* "struct user_settings", "warnon_erase_dynplaylist" = #1242, #1, "b", #0; */

@ 0 "" 2
@ 158 "<stdin>" 1
	/* "struct user_settings", "keep_current_track_on_replace_playlist" = #1243, #1, "b", #0; */

@ 0 "" 2
@ 159 "<stdin>" 1
	/* "struct user_settings", "show_shuffled_adding_options" = #1244, #1, "b", #0; */

@ 0 "" 2
@ 160 "<stdin>" 1
	/* "struct user_settings", "show_queue_options" = #1248, #4, "i", #0; */

@ 0 "" 2
@ 161 "<stdin>" 1
	/* "struct user_settings", "album_art" = #1252, #4, "i", #0; */

@ 0 "" 2
@ 162 "<stdin>" 1
	/* "struct user_settings", "rewind_across_tracks" = #1256, #1, "b", #0; */

@ 0 "" 2
@ 163 "<stdin>" 1
	/* "struct user_settings", "playlist_viewer_icons" = #1257, #1, "b", #0; */

@ 0 "" 2
@ 164 "<stdin>" 1
	/* "struct user_settings", "playlist_viewer_indices" = #1258, #1, "b", #0; */

@ 0 "" 2
@ 165 "<stdin>" 1
	/* "struct user_settings", "playlist_viewer_track_display" = #1260, #4, "i", #0; */

@ 0 "" 2
@ 166 "<stdin>" 1
	/* "struct user_settings", "talk_menu" = #1264, #1, "b", #0; */

@ 0 "" 2
@ 167 "<stdin>" 1
	/* "struct user_settings", "talk_dir" = #1268, #4, "i", #0; */

@ 0 "" 2
@ 168 "<stdin>" 1
	/* "struct user_settings", "talk_dir_clip" = #1272, #1, "b", #0; */

@ 0 "" 2
@ 169 "<stdin>" 1
	/* "struct user_settings", "talk_file" = #1276, #4, "i", #0; */

@ 0 "" 2
@ 170 "<stdin>" 1
	/* "struct user_settings", "talk_file_clip" = #1280, #1, "b", #0; */

@ 0 "" 2
@ 171 "<stdin>" 1
	/* "struct user_settings", "talk_filetype" = #1281, #1, "b", #0; */

@ 0 "" 2
@ 172 "<stdin>" 1
	/* "struct user_settings", "talk_battery_level" = #1282, #1, "b", #0; */

@ 0 "" 2
@ 173 "<stdin>" 1
	/* "struct user_settings", "talk_mixer_amp" = #1284, #4, "i", #0; */

@ 0 "" 2
@ 174 "<stdin>" 1
	/* "struct user_settings", "sort_case" = #1288, #1, "b", #0; */

@ 0 "" 2
@ 175 "<stdin>" 1
	/* "struct user_settings", "sort_dir" = #1292, #4, "i", #0; */

@ 0 "" 2
@ 176 "<stdin>" 1
	/* "struct user_settings", "sort_file" = #1296, #4, "i", #0; */

@ 0 "" 2
@ 177 "<stdin>" 1
	/* "struct user_settings", "sort_playlists" = #1300, #4, "i", #0; */

@ 0 "" 2
@ 178 "<stdin>" 1
	/* "struct user_settings", "interpret_numbers" = #1304, #4, "i", #0; */

@ 0 "" 2
@ 179 "<stdin>" 1
	/* "struct user_settings", "poweroff" = #1308, #4, "i", #0; */

@ 0 "" 2
@ 180 "<stdin>" 1
	/* "struct user_settings", "battery_capacity" = #1312, #4, "i", #0; */

@ 0 "" 2
@ 181 "<stdin>" 1
	/* "struct user_settings", "usb_charging" = #1316, #4, "i", #0; */

@ 0 "" 2
@ 182 "<stdin>" 1
	/* "struct user_settings", "cursor_style" = #1320, #4, "i", #0; */

@ 0 "" 2
@ 183 "<stdin>" 1
	/* "struct user_settings", "screen_scroll_step" = #1324, #4, "i", #0; */

@ 0 "" 2
@ 184 "<stdin>" 1
	/* "struct user_settings", "show_path_in_browser" = #1328, #4, "i", #0; */

@ 0 "" 2
@ 185 "<stdin>" 1
	/* "struct user_settings", "offset_out_of_view" = #1332, #1, "b", #0; */

@ 0 "" 2
@ 186 "<stdin>" 1
	/* "struct user_settings", "disable_mainmenu_scrolling" = #1333, #1, "b", #0; */

@ 0 "" 2
@ 187 "<stdin>" 1
	/* "struct user_settings", "icon_file" = #1334, #33, "str", #0; */

@ 0 "" 2
@ 188 "<stdin>" 1
	/* "struct user_settings", "viewers_icon_file" = #1367, #33, "str", #0; */

@ 0 "" 2
@ 189 "<stdin>" 1
	/* "struct user_settings", "font_file" = #1400, #33, "str", #0; */

@ 0 "" 2
@ 190 "<stdin>" 1
	/* "struct user_settings", "glyphs_to_cache" = #1436, #4, "i", #0; */

@ 0 "" 2
@ 191 "<stdin>" 1
	/* "struct user_settings", "kbd_file" = #1440, #33, "str", #0; */

@ 0 "" 2
@ 192 "<stdin>" 1
	/* "struct user_settings", "backlight_timeout" = #1476, #4, "i", #0; */

@ 0 "" 2
@ 193 "<stdin>" 1
	/* "struct user_settings", "caption_backlight" = #1480, #1, "b", #0; */

@ 0 "" 2
@ 194 "<stdin>" 1
	/* "struct user_settings", "bl_filter_first_keypress" = #1481, #1, "b", #0; */

@ 0 "" 2
@ 195 "<stdin>" 1
	/* "struct user_settings", "backlight_timeout_plugged" = #1484, #4, "i", #0; */

@ 0 "" 2
@ 196 "<stdin>" 1
	/* "struct user_settings", "bl_selective_actions" = #1488, #1, "b", #0; */

@ 0 "" 2
@ 197 "<stdin>" 1
	/* "struct user_settings", "bl_selective_actions_mask" = #1492, #4, "i", #0; */

@ 0 "" 2
@ 198 "<stdin>" 1
	/* "struct user_settings", "backlight_on_button_hold" = #1496, #4, "i", #0; */

@ 0 "" 2
@ 199 "<stdin>" 1
	/* "struct user_settings", "lcd_sleep_after_backlight_off" = #1500, #4, "i", #0; */

@ 0 "" 2
@ 200 "<stdin>" 1
	/* "struct user_settings", "brightness" = #1504, #4, "i", #0; */

@ 0 "" 2
@ 201 "<stdin>" 1
	/* "struct user_settings", "serial_bitrate" = #1508, #4, "i", #0; */

@ 0 "" 2
@ 202 "<stdin>" 1
	/* "struct user_settings", "accessory_supply" = #1512, #1, "b", #0; */

@ 0 "" 2
@ 203 "<stdin>" 1
	/* "struct user_settings", "lineout_active" = #1513, #1, "b", #0; */

@ 0 "" 2
@ 204 "<stdin>" 1
	/* "struct user_settings", "prevent_skip" = #1514, #1, "b", #0; */

@ 0 "" 2
@ 205 "<stdin>" 1
	/* "struct user_settings", "pitch_mode_semitone" = #1515, #1, "b", #0; */

@ 0 "" 2
@ 206 "<stdin>" 1
	/* "struct user_settings", "pitch_mode_timestretch" = #1516, #1, "b", #0; */

@ 0 "" 2
@ 207 "<stdin>" 1
	/* "struct user_settings", "usb_hid" = #1517, #1, "b", #0; */

@ 0 "" 2
@ 208 "<stdin>" 1
	/* "struct user_settings", "usb_keypad_mode" = #1520, #4, "i", #0; */

@ 0 "" 2
@ 209 "<stdin>" 1
	/* "struct user_settings", "usb_audio" = #1524, #4, "i", #0; */

@ 0 "" 2
@ 210 "<stdin>" 1
	/* "struct user_settings", "ui_vp_config" = #1528, #64, "str", #0; */

@ 0 "" 2
@ 211 "<stdin>" 1
	/* "struct user_settings", "compressor_settings" = #1592, #24, "s_compressor_settings", #0; */

@ 0 "" 2
@ 212 "<stdin>" 1
	/* "struct user_settings", "sleeptimer_duration" = #1616, #4, "i", #0; */

@ 0 "" 2
@ 213 "<stdin>" 1
	/* "struct user_settings", "sleeptimer_on_startup" = #1620, #1, "b", #0; */

@ 0 "" 2
@ 214 "<stdin>" 1
	/* "struct user_settings", "keypress_restarts_sleeptimer" = #1621, #1, "b", #0; */

@ 0 "" 2
@ 215 "<stdin>" 1
	/* "struct user_settings", "show_shutdown_message" = #1622, #1, "b", #0; */

@ 0 "" 2
@ 216 "<stdin>" 1
	/* "struct user_settings", "morse_input" = #1623, #1, "b", #0; */

@ 0 "" 2
@ 217 "<stdin>" 1
	/* "struct user_settings", "hotkey_wps" = #1624, #4, "i", #0; */

@ 0 "" 2
@ 218 "<stdin>" 1
	/* "struct user_settings", "hotkey_tree" = #1628, #4, "i", #0; */

@ 0 "" 2
@ 219 "<stdin>" 1
	/* "struct user_settings", "resume_rewind" = #1632, #4, "i", #0; */

@ 0 "" 2
@ 220 "<stdin>" 1
	/* "struct user_settings", "keyclick_hardware" = #1636, #1, "b", #0; */

@ 0 "" 2
@ 221 "<stdin>" 1
	/* "struct user_settings", "start_directory" = #1637, #81, "str", #0; */

@ 0 "" 2
@ 222 "<stdin>" 1
	/* "struct user_settings", "root_menu_customized" = #1718, #1, "b", #0; */

@ 0 "" 2
@ 223 "<stdin>" 1
	/* "struct user_settings", "shortcuts_replaces_qs" = #1719, #1, "b", #0; */

@ 0 "" 2
@ 224 "<stdin>" 1
	/* "struct user_settings", "play_frequency" = #1720, #4, "i", #0; */

@ 0 "" 2
@ 225 "<stdin>" 1
	/* "struct user_settings", "volume_limit" = #1724, #4, "i", #0; */

@ 0 "" 2
@ 226 "<stdin>" 1
	/* "struct user_settings", "volume_adjust_mode" = #1728, #4, "i", #0; */

@ 0 "" 2
@ 227 "<stdin>" 1
	/* "struct user_settings", "volume_adjust_norm_steps" = #1732, #4, "i", #0; */

@ 0 "" 2
@ 228 "<stdin>" 1
	/* "struct user_settings", "surround_enabled" = #1736, #4, "i", #0; */

@ 0 "" 2
@ 229 "<stdin>" 1
	/* "struct user_settings", "surround_balance" = #1740, #4, "i", #0; */

@ 0 "" 2
@ 230 "<stdin>" 1
	/* "struct user_settings", "surround_fx1" = #1744, #4, "i", #0; */

@ 0 "" 2
@ 231 "<stdin>" 1
	/* "struct user_settings", "surround_fx2" = #1748, #4, "i", #0; */

@ 0 "" 2
@ 232 "<stdin>" 1
	/* "struct user_settings", "surround_method2" = #1752, #1, "b", #0; */

@ 0 "" 2
@ 233 "<stdin>" 1
	/* "struct user_settings", "surround_mix" = #1756, #4, "i", #0; */

@ 0 "" 2
@ 234 "<stdin>" 1
	/* "struct user_settings", "pbe" = #1760, #4, "i", #0; */

@ 0 "" 2
@ 235 "<stdin>" 1
	/* "struct user_settings", "pbe_precut" = #1764, #4, "i", #0; */

@ 0 "" 2
@ 236 "<stdin>" 1
	/* "struct user_settings", "afr_enabled" = #1768, #4, "i", #0; */

@ 0 "" 2
@ 237 "<stdin>" 1
	/* "struct user_settings", "usb_mode" = #1772, #4, "i", #0; */

@ 0 "" 2
@ 238 "<stdin>" 1
	/* "struct user_settings", "clear_settings_on_hold" = #1776, #1, "b", #0; */

@ 0 "" 2
@ 239 "<stdin>" 1
	/* "struct user_settings", "playback_log" = #1777, #1, "b", #0; */

@ 0 "" 2
@ 241 "<stdin>" 1
	/* "struct replaygain_settings", "noclip" = #0, #1, "b", #0; */

@ 0 "" 2
@ 242 "<stdin>" 1
	/* "struct replaygain_settings", "type" = #4, #4, "i", #0; */

@ 0 "" 2
@ 243 "<stdin>" 1
	/* "struct replaygain_settings", "preamp" = #8, #4, "i", #0; */

@ 0 "" 2
@ 245 "<stdin>" 1
	/* "struct eq_band_setting", "cutoff" = #0, #4, "i", #0; */

@ 0 "" 2
@ 246 "<stdin>" 1
	/* "struct eq_band_setting", "q" = #4, #4, "i", #0; */

@ 0 "" 2
@ 247 "<stdin>" 1
	/* "struct eq_band_setting", "gain" = #8, #4, "i", #0; */

@ 0 "" 2
@ 249 "<stdin>" 1
	/* "struct compressor_settings", "threshold" = #0, #4, "i", #0; */

@ 0 "" 2
@ 250 "<stdin>" 1
	/* "struct compressor_settings", "makeup_gain" = #4, #4, "i", #0; */

@ 0 "" 2
@ 251 "<stdin>" 1
	/* "struct compressor_settings", "ratio" = #8, #4, "i", #0; */

@ 0 "" 2
@ 252 "<stdin>" 1
	/* "struct compressor_settings", "knee" = #12, #4, "i", #0; */

@ 0 "" 2
@ 253 "<stdin>" 1
	/* "struct compressor_settings", "release_time" = #16, #4, "i", #0; */

@ 0 "" 2
@ 254 "<stdin>" 1
	/* "struct compressor_settings", "attack_time" = #20, #4, "i", #0; */

@ 0 "" 2
@ 256 "<stdin>" 1
	/* "struct mp3entry", "path" = #0, #260, "str", #0; */

@ 0 "" 2
@ 257 "<stdin>" 1
	/* "struct mp3entry", "title" = #260, #4, "ptr_char", #0; */

@ 0 "" 2
@ 258 "<stdin>" 1
	/* "struct mp3entry", "artist" = #264, #4, "ptr_char", #0; */

@ 0 "" 2
@ 259 "<stdin>" 1
	/* "struct mp3entry", "album" = #268, #4, "ptr_char", #0; */

@ 0 "" 2
@ 260 "<stdin>" 1
	/* "struct mp3entry", "genre_string" = #272, #4, "ptr_char", #0; */

@ 0 "" 2
@ 261 "<stdin>" 1
	/* "struct mp3entry", "disc_string" = #276, #4, "ptr_char", #0; */

@ 0 "" 2
@ 262 "<stdin>" 1
	/* "struct mp3entry", "track_string" = #280, #4, "ptr_char", #0; */

@ 0 "" 2
@ 263 "<stdin>" 1
	/* "struct mp3entry", "year_string" = #284, #4, "ptr_char", #0; */

@ 0 "" 2
@ 264 "<stdin>" 1
	/* "struct mp3entry", "composer" = #288, #4, "ptr_char", #0; */

@ 0 "" 2
@ 265 "<stdin>" 1
	/* "struct mp3entry", "comment" = #292, #4, "ptr_char", #0; */

@ 0 "" 2
@ 266 "<stdin>" 1
	/* "struct mp3entry", "albumartist" = #296, #4, "ptr_char", #0; */

@ 0 "" 2
@ 267 "<stdin>" 1
	/* "struct mp3entry", "grouping" = #300, #4, "ptr_char", #0; */

@ 0 "" 2
@ 268 "<stdin>" 1
	/* "struct mp3entry", "discnum" = #304, #4, "i", #0; */

@ 0 "" 2
@ 269 "<stdin>" 1
	/* "struct mp3entry", "tracknum" = #308, #4, "i", #0; */

@ 0 "" 2
@ 270 "<stdin>" 1
	/* "struct mp3entry", "layer" = #312, #4, "i", #0; */

@ 0 "" 2
@ 271 "<stdin>" 1
	/* "struct mp3entry", "year" = #316, #4, "i", #0; */

@ 0 "" 2
@ 272 "<stdin>" 1
	/* "struct mp3entry", "id3version" = #320, #1, "u_c", #0; */

@ 0 "" 2
@ 273 "<stdin>" 1
	/* "struct mp3entry", "codectype" = #324, #4, "u_i", #0; */

@ 0 "" 2
@ 274 "<stdin>" 1
	/* "struct mp3entry", "bitrate" = #328, #4, "u_i", #0; */

@ 0 "" 2
@ 275 "<stdin>" 1
	/* "struct mp3entry", "frequency" = #332, #4, "u_l", #0; */

@ 0 "" 2
@ 276 "<stdin>" 1
	/* "struct mp3entry", "id3v2len" = #336, #4, "u_l", #0; */

@ 0 "" 2
@ 277 "<stdin>" 1
	/* "struct mp3entry", "id3v1len" = #340, #4, "u_l", #0; */

@ 0 "" 2
@ 278 "<stdin>" 1
	/* "struct mp3entry", "first_frame_offset" = #344, #4, "u_l", #0; */

@ 0 "" 2
@ 279 "<stdin>" 1
	/* "struct mp3entry", "filesize" = #348, #4, "u_l", #0; */

@ 0 "" 2
@ 280 "<stdin>" 1
	/* "struct mp3entry", "length" = #352, #4, "u_l", #0; */

@ 0 "" 2
@ 281 "<stdin>" 1
	/* "struct mp3entry", "elapsed" = #356, #4, "u_l", #0; */

@ 0 "" 2
@ 282 "<stdin>" 1
	/* "struct mp3entry", "lead_trim" = #360, #4, "i", #0; */

@ 0 "" 2
@ 283 "<stdin>" 1
	/* "struct mp3entry", "tail_trim" = #364, #4, "i", #0; */

@ 0 "" 2
@ 284 "<stdin>" 1
	/* "struct mp3entry", "samples" = #368, #8, "u_llong", #0; */

@ 0 "" 2
@ 285 "<stdin>" 1
	/* "struct mp3entry", "frame_count" = #376, #4, "u_l", #0; */

@ 0 "" 2
@ 286 "<stdin>" 1
	/* "struct mp3entry", "bytesperframe" = #380, #4, "u_l", #0; */

@ 0 "" 2
@ 287 "<stdin>" 1
	/* "struct mp3entry", "vbr" = #384, #1, "b", #0; */

@ 0 "" 2
@ 288 "<stdin>" 1
	/* "struct mp3entry", "has_toc" = #385, #1, "b", #0; */

@ 0 "" 2
@ 289 "<stdin>" 1
	/* "struct mp3entry", "toc" = #386, #100, "str", #0; */

@ 0 "" 2
@ 290 "<stdin>" 1
	/* "struct mp3entry", "needs_upsampling_correction" = #486, #1, "b", #0; */

@ 0 "" 2
@ 291 "<stdin>" 1
	/* "struct mp3entry", "id3v2buf" = #487, #1800, "str", #0; */

@ 0 "" 2
@ 292 "<stdin>" 1
	/* "struct mp3entry", "id3v1buf" = #2287, #368, "str", #0; */

@ 0 "" 2
@ 293 "<stdin>" 1
	/* "struct mp3entry", "offset" = #2656, #4, "u_l", #0; */

@ 0 "" 2
@ 294 "<stdin>" 1
	/* "struct mp3entry", "index" = #2660, #4, "i", #0; */

@ 0 "" 2
@ 295 "<stdin>" 1
	/* "struct mp3entry", "skip_resume_adjustments" = #2664, #1, "b", #0; */

@ 0 "" 2
@ 296 "<stdin>" 1
	/* "struct mp3entry", "autoresumable" = #2665, #1, "u_c", #0; */

@ 0 "" 2
@ 297 "<stdin>" 1
	/* "struct mp3entry", "tagcache_idx" = #2668, #4, "l", #0; */

@ 0 "" 2
@ 298 "<stdin>" 1
	/* "struct mp3entry", "rating" = #2672, #4, "i", #0; */

@ 0 "" 2
@ 299 "<stdin>" 1
	/* "struct mp3entry", "score" = #2676, #4, "i", #0; */

@ 0 "" 2
@ 300 "<stdin>" 1
	/* "struct mp3entry", "playcount" = #2680, #4, "l", #0; */

@ 0 "" 2
@ 301 "<stdin>" 1
	/* "struct mp3entry", "lastplayed" = #2684, #4, "l", #0; */

@ 0 "" 2
@ 302 "<stdin>" 1
	/* "struct mp3entry", "playtime" = #2688, #4, "l", #0; */

@ 0 "" 2
@ 303 "<stdin>" 1
	/* "struct mp3entry", "track_level" = #2692, #4, "l", #0; */

@ 0 "" 2
@ 304 "<stdin>" 1
	/* "struct mp3entry", "album_level" = #2696, #4, "l", #0; */

@ 0 "" 2
@ 305 "<stdin>" 1
	/* "struct mp3entry", "track_gain" = #2700, #4, "l", #0; */

@ 0 "" 2
@ 306 "<stdin>" 1
	/* "struct mp3entry", "album_gain" = #2704, #4, "l", #0; */

@ 0 "" 2
@ 307 "<stdin>" 1
	/* "struct mp3entry", "track_peak" = #2708, #4, "l", #0; */

@ 0 "" 2
@ 308 "<stdin>" 1
	/* "struct mp3entry", "album_peak" = #2712, #4, "l", #0; */

@ 0 "" 2
@ 309 "<stdin>" 1
	/* "struct mp3entry", "has_embedded_albumart" = #2716, #1, "b", #0; */

@ 0 "" 2
@ 310 "<stdin>" 1
	/* "struct mp3entry", "albumart" = #2720, #12, "s_mp3_albumart", #0; */

@ 0 "" 2
@ 311 "<stdin>" 1
	/* "struct mp3entry", "has_embedded_cuesheet" = #2732, #1, "b", #0; */

@ 0 "" 2
@ 312 "<stdin>" 1
	/* "struct mp3entry", "embedded_cuesheet" = #2736, #12, "s_embedded_cuesheet", #0; */

@ 0 "" 2
@ 313 "<stdin>" 1
	/* "struct mp3entry", "cuesheet" = #2748, #4, "ptr_s_cuesheet", #0; */

@ 0 "" 2
@ 314 "<stdin>" 1
	/* "struct mp3entry", "mb_track_id" = #2752, #4, "ptr_char", #0; */

@ 0 "" 2
@ 315 "<stdin>" 1
	/* "struct mp3entry", "is_asf_stream" = #2756, #1, "b", #0; */

@ 0 "" 2
@ 316 "<stdin>" 1
	/* "struct mp3entry", "has_video" = #2757, #1, "b", #0; */

@ 0 "" 2
@ 318 "<stdin>" 1
	/* "struct mp3_albumart", "type" = #0, #1, "e_mp3_aa_type", #0; */

@ 0 "" 2
@ 319 "<stdin>" 1
	/* "struct mp3_albumart", "size" = #4, #4, "i", #0; */

@ 0 "" 2
@ 320 "<stdin>" 1
	/* "struct mp3_albumart", "pos" = #8, #4, "off_t", #0; */

@ 0 "" 2
@ 322 "<stdin>" 1
	/* "struct embedded_cuesheet", "size" = #0, #4, "i", #0; */

@ 0 "" 2
@ 323 "<stdin>" 1
	/* "struct embedded_cuesheet", "pos" = #4, #4, "off_t", #0; */

@ 0 "" 2
@ 324 "<stdin>" 1
	/* "struct embedded_cuesheet", "encoding" = #8, #1, "e_character_encoding", #0; */

@ 0 "" 2
	.arm
	.syntax unified
	mov	r0, #0
	bx	lr
	.size	main, .-main
	.ident	"GCC: (GNU) 9.5.0"
