/* Automatically generated from rocklib.c & plugin.h ($Revision$)
 *
 * See apps/plugins/lua/rocklib_aux.pl for the generator.
 */

#define lrocklib_c
#define LUA_LIB

#define _ROCKCONF_H_ /* We don't need strcmp() etc. wrappers */
#include "lua.h"
#include "lauxlib.h"
#include "plugin.h"

static int rock_action_userabort(lua_State *L)
{
	int timeout = (int) luaL_checkint(L, 1);
	bool result = rb->action_userabort(timeout);
	lua_pushboolean(L, result);
	return 1;
}

static int rock_adjust_volume(lua_State *L)
{
	int steps = (int) luaL_checkint(L, 1);
	rb->adjust_volume(steps);
	return 0;
}

static int rock_audio_set_input_source(lua_State *L)
{
	int source = (int) luaL_checkint(L, 1);
	unsigned flags = (unsigned) luaL_checkint(L, 2);
	rb->audio_set_input_source(source, flags);
	return 0;
}

static int rock_audio_set_output_source(lua_State *L)
{
	int monitor = (int) luaL_checkint(L, 1);
	rb->audio_set_output_source(monitor);
	return 0;
}

static int rock_audio_set_recording_gain(lua_State *L)
{
	int left = (int) luaL_checkint(L, 1);
	int right = (int) luaL_checkint(L, 2);
	int type = (int) luaL_checkint(L, 3);
	rb->audio_set_recording_gain(left, right, type);
	return 0;
}

static int rock_backlight_set_timeout(lua_State *L)
{
	int index = (int) luaL_checkint(L, 1);
	rb->backlight_set_timeout(index);
	return 0;
}

static int rock_backlight_set_timeout_plugged(lua_State *L)
{
	int index = (int) luaL_checkint(L, 1);
	rb->backlight_set_timeout_plugged(index);
	return 0;
}

static int rock_battery_current(lua_State *L)
{
	(void)L;
	int result = rb->battery_current();
	lua_pushinteger(L, result);
	return 1;
}

static int rock_battery_level(lua_State *L)
{
	(void)L;
	int result = rb->battery_level();
	lua_pushinteger(L, result);
	return 1;
}

static int rock_battery_level_safe(lua_State *L)
{
	(void)L;
	bool result = rb->battery_level_safe();
	lua_pushboolean(L, result);
	return 1;
}

static int rock_battery_time(lua_State *L)
{
	(void)L;
	int result = rb->battery_time();
	lua_pushinteger(L, result);
	return 1;
}

static int rock_battery_voltage(lua_State *L)
{
	(void)L;
	int result = rb->battery_voltage();
	lua_pushinteger(L, result);
	return 1;
}

static int rock_beep_play(lua_State *L)
{
	unsigned int frequency = (unsigned int) luaL_checkint(L, 1);
	unsigned int duration = (unsigned int) luaL_checkint(L, 2);
	unsigned int amplitude = (unsigned int) luaL_checkint(L, 3);
	rb->beep_play(frequency, duration, amplitude);
	return 0;
}

static int rock_button_clear_queue(lua_State *L)
{
	(void)L;
	rb->button_clear_queue();
	return 0;
}

static int rock_button_get(lua_State *L)
{
	bool block = luaL_checkboolean(L, 1);
	long result = rb->button_get(block);
	lua_pushinteger(L, result);
	return 1;
}

static int rock_button_get_sw_poweroff_state(lua_State *L)
{
	(void)L;
	bool result = rb->button_get_sw_poweroff_state();
	lua_pushboolean(L, result);
	return 1;
}

static int rock_button_get_w_tmo(lua_State *L)
{
	int ticks = (int) luaL_checkint(L, 1);
	long result = rb->button_get_w_tmo(ticks);
	lua_pushinteger(L, result);
	return 1;
}

static int rock_button_hold(lua_State *L)
{
	(void)L;
	bool result = rb->button_hold();
	lua_pushboolean(L, result);
	return 1;
}

static int rock_button_queue_count(lua_State *L)
{
	(void)L;
	int result = rb->button_queue_count();
	lua_pushinteger(L, result);
	return 1;
}

static int rock_button_set_sw_poweroff_state(lua_State *L)
{
	bool enable = luaL_checkboolean(L, 1);
	rb->button_set_sw_poweroff_state(enable);
	return 0;
}

static int rock_button_status(lua_State *L)
{
	(void)L;
	int result = rb->button_status();
	lua_pushinteger(L, result);
	return 1;
}

static int rock_charger_inserted(lua_State *L)
{
	(void)L;
	bool result = rb->charger_inserted();
	lua_pushboolean(L, result);
	return 1;
}

static int rock_charging_state(lua_State *L)
{
	(void)L;
	bool result = rb->charging_state();
	lua_pushboolean(L, result);
	return 1;
}

static int rock_default_event_handler(lua_State *L)
{
	long event = (long) luaL_checkint(L, 1);
	long result = rb->default_event_handler(event);
	lua_pushinteger(L, result);
	return 1;
}

static int rock_dir_exists(lua_State *L)
{
	const char * dirname = (const char *) luaL_checkstring(L, 1);
	bool result = rb->dir_exists(dirname);
	lua_pushboolean(L, result);
	return 1;
}

static int rock_dsp_dither_enable(lua_State *L)
{
	bool enable = luaL_checkboolean(L, 1);
	rb->dsp_dither_enable(enable);
	return 0;
}

static int rock_dsp_eq_enable(lua_State *L)
{
	bool enable = luaL_checkboolean(L, 1);
	rb->dsp_eq_enable(enable);
	return 0;
}

static int rock_dsp_set_crossfeed_type(lua_State *L)
{
	int type = (int) luaL_checkint(L, 1);
	rb->dsp_set_crossfeed_type(type);
	return 0;
}

static int rock_dsp_timestretch_available(lua_State *L)
{
	(void)L;
	bool result = rb->dsp_timestretch_available();
	lua_pushboolean(L, result);
	return 1;
}

static int rock_dsp_timestretch_enable(lua_State *L)
{
	bool enabled = luaL_checkboolean(L, 1);
	rb->dsp_timestretch_enable(enabled);
	return 0;
}

static int rock_dynamic_colors_resolve(lua_State *L)
{
	unsigned int original = (unsigned int) luaL_checkint(L, 1);
	unsigned int result = rb->dynamic_colors_resolve(original);
	lua_pushinteger(L, result);
	return 1;
}

static int rock_file_exists(lua_State *L)
{
	const char * path = (const char *) luaL_checkstring(L, 1);
	bool result = rb->file_exists(path);
	lua_pushboolean(L, result);
	return 1;
}

static int rock_filetype_get_attr(lua_State *L)
{
	const char* file = (const char*) luaL_checkstring(L, 1);
	int result = rb->filetype_get_attr(file);
	lua_pushinteger(L, result);
	return 1;
}

static int rock_filetype_get_plugin(lua_State *L)
{
	int attr = (int) luaL_checkint(L, 1);
	char * buffer = (char *) luaL_checkstring(L, 2);
	size_t buffer_len = (size_t) luaL_checkint(L, 3);
	char* result = rb->filetype_get_plugin(attr, buffer, buffer_len);
	lua_pushstring(L, result);
	return 1;
}

static int rock_fix_path_part(lua_State *L)
{
	char* path = (char*) luaL_checkstring(L, 1);
	int offset = (int) luaL_checkint(L, 2);
	int count = (int) luaL_checkint(L, 3);
	rb->fix_path_part(path, offset, count);
	return 0;
}

static int rock_font_load(lua_State *L)
{
	const char * path = (const char *) luaL_checkstring(L, 1);
	int result = rb->font_load(path);
	lua_pushinteger(L, result);
	return 1;
}

static int rock_font_unload(lua_State *L)
{
	int font_id = (int) luaL_checkint(L, 1);
	rb->font_unload(font_id);
	return 0;
}

static int rock_format_time_auto(lua_State *L)
{
	char * buffer = (char *) luaL_checkstring(L, 1);
	int buf_len = (int) luaL_checkint(L, 2);
	long value = (long) luaL_checkint(L, 3);
	int unit_idx = (int) luaL_checkint(L, 4);
	bool supress_unit = luaL_checkboolean(L, 5);
	const char* result = rb->format_time_auto(buffer, buf_len, value, unit_idx, supress_unit);
	lua_pushstring(L, result);
	return 1;
}

static int rock_get_action(lua_State *L)
{
	int context = (int) luaL_checkint(L, 1);
	int timeout = (int) luaL_checkint(L, 2);
	int result = rb->get_action(context, timeout);
	lua_pushinteger(L, result);
	return 1;
}

static int rock_get_codec_filename(lua_State *L)
{
	int cod_spec = (int) luaL_checkint(L, 1);
	const char * result = rb->get_codec_filename(cod_spec);
	lua_pushstring(L, result);
	return 1;
}

static int rock_get_codec_string(lua_State *L)
{
	int codectype = (int) luaL_checkint(L, 1);
	const char* result = rb->get_codec_string(codectype);
	lua_pushstring(L, result);
	return 1;
}

static int rock_get_codepage_name(lua_State *L)
{
	int cp = (int) luaL_checkint(L, 1);
	const char* result = rb->get_codepage_name(cp);
	lua_pushstring(L, result);
	return 1;
}

static int rock_get_sleep_timer(lua_State *L)
{
	(void)L;
	int result = rb->get_sleep_timer();
	lua_pushinteger(L, result);
	return 1;
}

static int rock_is_backlight_on(lua_State *L)
{
	bool ignore_always_off = luaL_checkboolean(L, 1);
	bool result = rb->is_backlight_on(ignore_always_off);
	lua_pushboolean(L, result);
	return 1;
}

static int rock_keyclick_click(lua_State *L)
{
	bool rawbutton = luaL_checkboolean(L, 1);
	int action = (int) luaL_checkint(L, 2);
	rb->keyclick_click(rawbutton, action);
	return 0;
}

static int rock_lang_is_rtl(lua_State *L)
{
	(void)L;
	int result = rb->lang_is_rtl();
	lua_pushinteger(L, result);
	return 1;
}

static int rock_lcd_get_drawmode(lua_State *L)
{
	(void)L;
	int result = rb->lcd_get_drawmode();
	lua_pushinteger(L, result);
	return 1;
}

static int rock_led(lua_State *L)
{
	bool on = luaL_checkboolean(L, 1);
	rb->led(on);
	return 0;
}

static int rock_pcm_close_recording(lua_State *L)
{
	(void)L;
	rb->pcm_close_recording();
	return 0;
}

static int rock_pcm_init_recording(lua_State *L)
{
	(void)L;
	rb->pcm_init_recording();
	return 0;
}

static int rock_pcm_stop_recording(lua_State *L)
{
	(void)L;
	rb->pcm_stop_recording();
	return 0;
}

static int rock_pcmbuf_fade(lua_State *L)
{
	bool fade = luaL_checkboolean(L, 1);
	bool in = luaL_checkboolean(L, 2);
	rb->pcmbuf_fade(fade, in);
	return 0;
}

static int rock_pcmbuf_set_low_latency(lua_State *L)
{
	bool state = luaL_checkboolean(L, 1);
	rb->pcmbuf_set_low_latency(state);
	return 0;
}

static int rock_playlist_get_display_index(lua_State *L)
{
	(void)L;
	int result = rb->playlist_get_display_index();
	lua_pushinteger(L, result);
	return 1;
}

static int rock_plugin_open(lua_State *L)
{
	const char * path = (const char *) luaL_checkstring(L, 1);
	const char * parameter = (const char *) luaL_checkstring(L, 2);
	int result = rb->plugin_open(path, parameter);
	lua_pushinteger(L, result);
	return 1;
}

static int rock_plugin_reserve_buffer(lua_State *L)
{
	size_t buffer_size = (size_t) luaL_checkint(L, 1);
	size_t result = rb->plugin_reserve_buffer(buffer_size);
	lua_pushinteger(L, result);
	return 1;
}

static int rock_reset_poweroff_timer(lua_State *L)
{
	(void)L;
	rb->reset_poweroff_timer();
	return 0;
}

static int rock_sb_set_persistent_title(lua_State *L)
{
	const char* title = (const char*) luaL_checkstring(L, 1);
	int icon = luaL_checkint(L, 2); /*(enum)*/
	int screen = luaL_checkint(L, 3); /*(enum)*/
	bool result = rb->sb_set_persistent_title(title, icon, screen);
	lua_pushboolean(L, result);
	return 1;
}

static int rock_sb_set_title_text(lua_State *L)
{
	const char* title = (const char*) luaL_checkstring(L, 1);
	int icon = luaL_checkint(L, 2); /*(enum)*/
	int screen = luaL_checkint(L, 3); /*(enum)*/
	bool result = rb->sb_set_title_text(title, icon, screen);
	lua_pushboolean(L, result);
	return 1;
}

static int rock_set_sleeptimer_duration(lua_State *L)
{
	int minutes = (int) luaL_checkint(L, 1);
	rb->set_sleeptimer_duration(minutes);
	return 0;
}

static int rock_skin_render_inhibit_flush(lua_State *L)
{
	bool inhibit = luaL_checkboolean(L, 1);
	rb->skin_render_inhibit_flush(inhibit);
	return 0;
}

static int rock_splash_progress_set_delay(lua_State *L)
{
	long delay_ticks = (long) luaL_checkint(L, 1);
	rb->splash_progress_set_delay(delay_ticks);
	return 0;
}

static int rock_storage_sleep(lua_State *L)
{
	(void)L;
	rb->storage_sleep();
	return 0;
}

static int rock_storage_spin(lua_State *L)
{
	(void)L;
	rb->storage_spin();
	return 0;
}

static int rock_storage_spindown(lua_State *L)
{
	int seconds = (int) luaL_checkint(L, 1);
	rb->storage_spindown(seconds);
	return 0;
}

static int rock_strchr(lua_State *L)
{
	const char * s = (const char *) luaL_checkstring(L, 1);
	int c = (int) luaL_checkint(L, 2);
	char * result = rb->strchr(s, c);
	lua_pushstring(L, result);
	return 1;
}

static int rock_strcmp(lua_State *L)
{
	const char * kyvtv = (const char *) luaL_checkstring(L, 1);
	const char * xwzvv = (const char *) luaL_checkstring(L, 2);
	int result = rb->strcmp(kyvtv, xwzvv);
	lua_pushinteger(L, result);
	return 1;
}

static int rock_strrchr(lua_State *L)
{
	const char * s = (const char *) luaL_checkstring(L, 1);
	int c = (int) luaL_checkint(L, 2);
	char * result = rb->strrchr(s, c);
	lua_pushstring(L, result);
	return 1;
}

static int rock_strstr(lua_State *L)
{
	const char * s1 = (const char *) luaL_checkstring(L, 1);
	const char * s2 = (const char *) luaL_checkstring(L, 2);
	char* result = rb->strstr(s1, s2);
	lua_pushstring(L, result);
	return 1;
}

static int rock_sys_poweroff(lua_State *L)
{
	(void)L;
	rb->sys_poweroff();
	return 0;
}

static int rock_sys_reboot(lua_State *L)
{
	(void)L;
	rb->sys_reboot();
	return 0;
}

static int rock_tagcache_commit_finalize(lua_State *L)
{
	(void)L;
	rb->tagcache_commit_finalize();
	return 0;
}

static int rock_tagcache_is_in_ram(lua_State *L)
{
	(void)L;
	bool result = rb->tagcache_is_in_ram();
	lua_pushboolean(L, result);
	return 1;
}

static int rock_talk_disable(lua_State *L)
{
	bool disable = luaL_checkboolean(L, 1);
	rb->talk_disable(disable);
	return 0;
}

static int rock_talk_force_enqueue_next(lua_State *L)
{
	(void)L;
	rb->talk_force_enqueue_next();
	return 0;
}

static int rock_talk_fullpath(lua_State *L)
{
	const char* path = (const char*) luaL_checkstring(L, 1);
	bool enqueue = luaL_checkboolean(L, 2);
	int result = rb->talk_fullpath(path, enqueue);
	lua_pushinteger(L, result);
	return 1;
}

static int rock_talk_value_decimal(lua_State *L)
{
	long n = (long) luaL_checkint(L, 1);
	int unit = (int) luaL_checkint(L, 2);
	int decimals = (int) luaL_checkint(L, 3);
	bool enqueue = luaL_checkboolean(L, 4);
	int result = rb->talk_value_decimal(n, unit, decimals, enqueue);
	lua_pushinteger(L, result);
	return 1;
}

static int rock_usb_inserted(lua_State *L)
{
	(void)L;
	bool result = rb->usb_inserted();
	lua_pushboolean(L, result);
	return 1;
}

static int rock_utf8length(lua_State *L)
{
	const unsigned char * utf8 = (const unsigned char *) luaL_checkstring(L, 1);
	unsigned long result = rb->utf8length(utf8);
	lua_pushinteger(L, result);
	return 1;
}

static int rock_utf8seek(lua_State *L)
{
	const unsigned char* utf8 = (const unsigned char*) luaL_checkstring(L, 1);
	int offset = (int) luaL_checkint(L, 2);
	int result = rb->utf8seek(utf8, offset);
	lua_pushinteger(L, result);
	return 1;
}

static int rock_warn_on_pl_erase(lua_State *L)
{
	(void)L;
	bool result = rb->warn_on_pl_erase();
	lua_pushboolean(L, result);
	return 1;
}

static int rock_yesno_pop(lua_State *L)
{
	const char* text = (const char*) luaL_checkstring(L, 1);
	bool result = rb->yesno_pop(text);
	lua_pushboolean(L, result);
	return 1;
}

static int rock_yesno_pop_confirm(lua_State *L)
{
	const char* text = (const char*) luaL_checkstring(L, 1);
	bool result = rb->yesno_pop_confirm(text);
	lua_pushboolean(L, result);
	return 1;
}

static int rock_yield(lua_State *L)
{
	(void)L;
	rb->yield();
	return 0;
}

const luaL_Reg rocklib_aux[] =
{
	{"action_userabort", rock_action_userabort},
	{"adjust_volume", rock_adjust_volume},
	{"audio_set_input_source", rock_audio_set_input_source},
	{"audio_set_output_source", rock_audio_set_output_source},
	{"audio_set_recording_gain", rock_audio_set_recording_gain},
	{"backlight_set_timeout", rock_backlight_set_timeout},
	{"backlight_set_timeout_plugged", rock_backlight_set_timeout_plugged},
	{"battery_current", rock_battery_current},
	{"battery_level", rock_battery_level},
	{"battery_level_safe", rock_battery_level_safe},
	{"battery_time", rock_battery_time},
	{"battery_voltage", rock_battery_voltage},
	{"beep_play", rock_beep_play},
	{"button_clear_queue", rock_button_clear_queue},
	{"button_get", rock_button_get},
	{"button_get_sw_poweroff_state", rock_button_get_sw_poweroff_state},
	{"button_get_w_tmo", rock_button_get_w_tmo},
	{"button_hold", rock_button_hold},
	{"button_queue_count", rock_button_queue_count},
	{"button_set_sw_poweroff_state", rock_button_set_sw_poweroff_state},
	{"button_status", rock_button_status},
	{"charger_inserted", rock_charger_inserted},
	{"charging_state", rock_charging_state},
	{"default_event_handler", rock_default_event_handler},
	{"dir_exists", rock_dir_exists},
	{"dsp_dither_enable", rock_dsp_dither_enable},
	{"dsp_eq_enable", rock_dsp_eq_enable},
	{"dsp_set_crossfeed_type", rock_dsp_set_crossfeed_type},
	{"dsp_timestretch_available", rock_dsp_timestretch_available},
	{"dsp_timestretch_enable", rock_dsp_timestretch_enable},
	{"dynamic_colors_resolve", rock_dynamic_colors_resolve},
	{"file_exists", rock_file_exists},
	{"filetype_get_attr", rock_filetype_get_attr},
	{"filetype_get_plugin", rock_filetype_get_plugin},
	{"fix_path_part", rock_fix_path_part},
	{"font_load", rock_font_load},
	{"font_unload", rock_font_unload},
	{"format_time_auto", rock_format_time_auto},
	{"get_action", rock_get_action},
	{"get_codec_filename", rock_get_codec_filename},
	{"get_codec_string", rock_get_codec_string},
	{"get_codepage_name", rock_get_codepage_name},
	{"get_sleep_timer", rock_get_sleep_timer},
	{"is_backlight_on", rock_is_backlight_on},
	{"keyclick_click", rock_keyclick_click},
	{"lang_is_rtl", rock_lang_is_rtl},
	{"lcd_get_drawmode", rock_lcd_get_drawmode},
	{"led", rock_led},
	{"pcm_close_recording", rock_pcm_close_recording},
	{"pcm_init_recording", rock_pcm_init_recording},
	{"pcm_stop_recording", rock_pcm_stop_recording},
	{"pcmbuf_fade", rock_pcmbuf_fade},
	{"pcmbuf_set_low_latency", rock_pcmbuf_set_low_latency},
	{"playlist_get_display_index", rock_playlist_get_display_index},
	{"plugin_open", rock_plugin_open},
	{"plugin_reserve_buffer", rock_plugin_reserve_buffer},
	{"reset_poweroff_timer", rock_reset_poweroff_timer},
	{"sb_set_persistent_title", rock_sb_set_persistent_title},
	{"sb_set_title_text", rock_sb_set_title_text},
	{"set_sleeptimer_duration", rock_set_sleeptimer_duration},
	{"skin_render_inhibit_flush", rock_skin_render_inhibit_flush},
	{"splash_progress_set_delay", rock_splash_progress_set_delay},
	{"storage_sleep", rock_storage_sleep},
	{"storage_spin", rock_storage_spin},
	{"storage_spindown", rock_storage_spindown},
	{"strchr", rock_strchr},
	{"strcmp", rock_strcmp},
	{"strrchr", rock_strrchr},
	{"strstr", rock_strstr},
	{"sys_poweroff", rock_sys_poweroff},
	{"sys_reboot", rock_sys_reboot},
	{"tagcache_commit_finalize", rock_tagcache_commit_finalize},
	{"tagcache_is_in_ram", rock_tagcache_is_in_ram},
	{"talk_disable", rock_talk_disable},
	{"talk_force_enqueue_next", rock_talk_force_enqueue_next},
	{"talk_fullpath", rock_talk_fullpath},
	{"talk_value_decimal", rock_talk_value_decimal},
	{"usb_inserted", rock_usb_inserted},
	{"utf8length", rock_utf8length},
	{"utf8seek", rock_utf8seek},
	{"warn_on_pl_erase", rock_warn_on_pl_erase},
	{"yesno_pop", rock_yesno_pop},
	{"yesno_pop_confirm", rock_yesno_pop_confirm},
	{"yield", rock_yield},
	{NULL, NULL}
};

