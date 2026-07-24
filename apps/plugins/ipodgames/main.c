#include "ipodgames.h"

#ifdef SIMULATOR
#include <stdlib.h>
#endif

#define IG_VISIBLE_ROWS 7
#define IG_SIM_LOG IG_ROOT_DIR "/ipodgames-sim.log"

static void ig_draw_header(const char *title)
{
    rb->lcd_set_background(LCD_BLACK);
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_clear_display();
    rb->lcd_fillrect(0, 0, LCD_WIDTH, 24);
    rb->lcd_set_background(LCD_WHITE);
    rb->lcd_set_foreground(LCD_BLACK);
    rb->lcd_putsxy(8, 5, title);
    rb->lcd_set_background(LCD_BLACK);
    rb->lcd_set_foreground(LCD_WHITE);
}

static const char *ig_state_label(const struct ig_game *game)
{
    if (!rb->strcmp(game->executable_state, "decrypted-eapp"))
        return "READY";
    if (!rb->strcmp(game->executable_state, "encrypted"))
        return "LOCKED";
    return "UNKNOWN";
}

static void ig_draw_catalog(const struct ig_catalog *catalog, int selected)
{
    int start = 0;
    int row;

    ig_draw_header("iPod Games - Prototype");
    if (catalog->count == 0)
    {
        rb->lcd_putsxy(10, 54, "No imported games found");
        rb->lcd_putsxy(10, 78, IG_GAMES_DIR);
        rb->lcd_putsxy(10, 202, "MENU: exit");
        rb->lcd_update();
        return;
    }

    if (selected >= IG_VISIBLE_ROWS)
        start = selected - IG_VISIBLE_ROWS + 1;

    for (row = 0; row < IG_VISIBLE_ROWS && start + row < catalog->count;
         ++row)
    {
        const struct ig_game *game = &catalog->games[start + row];
        int y = 31 + row * 23;

        if (start + row == selected)
        {
            rb->lcd_set_foreground(LCD_BLACK);
            rb->lcd_set_background(LCD_WHITE);
            rb->lcd_fillrect(4, y - 3, LCD_WIDTH - 8, 21);
        }
        rb->lcd_putsxyf(10, y, "%s", game->name);
        rb->lcd_putsxyf(245, y, "%s", ig_state_label(game));
        if (start + row == selected)
        {
            rb->lcd_set_foreground(LCD_WHITE);
            rb->lcd_set_background(LCD_BLACK);
        }
    }

    rb->lcd_hline(6, LCD_WIDTH - 7, 199);
    rb->lcd_putsxyf(8, 207, "%d game%s  SELECT: details", catalog->count,
                    catalog->count == 1 ? "" : "s");
    rb->lcd_update();
}

static void ig_show_details(const struct ig_game *game)
{
    struct ig_eapp_probe probe;
    bool done = false;

    ig_runtime_probe(game, &probe);
    while (!done)
    {
        int button;

        ig_draw_header(game->name);
        rb->lcd_putsxyf(8, 34, "GUID: %s", game->guid);
        rb->lcd_putsxyf(8, 54, "Version: %s", game->version);
        rb->lcd_putsxyf(8, 74, "Platform: %d  Build: %lu",
                        game->platform_id, game->build_id);
        rb->lcd_putsxyf(8, 94, "Executable: %s", ig_state_label(game));
        rb->lcd_putsxyf(8, 114, "Size: %lu bytes", game->executable_size);
        rb->lcd_putsxyf(8, 138, "%s", ig_probe_result_name(probe.result));
        if (probe.result == IG_PROBE_OK)
        {
            rb->lcd_putsxyf(8, 158, "Load base: 0x%08lx",
                            probe.inferred_load_base);
            rb->lcd_putsxyf(8, 178, "%u frameworks, %lu imports",
                            probe.framework_count, probe.total_imports);
#ifdef SIMULATOR
            rb->lcd_putsxy(8, 198, "ARM execution requires hardware");
#else
            rb->lcd_putsxy(8, 198, "Execution gate not enabled yet");
#endif
        }
        else if (probe.result == IG_PROBE_NO_EXECUTABLE)
        {
            rb->lcd_putsxy(8, 158, "Import a decrypted matching eApp");
        }
        rb->lcd_putsxy(8, 211, "MENU: back");
        rb->lcd_update();

        button = rb->button_get(true);
        if ((button & ~(BUTTON_REPEAT | BUTTON_REL)) == BUTTON_MENU)
            done = true;
        else if (button == SYS_USB_CONNECTED)
            done = true;
        else
            rb->default_event_handler(button);
    }
}

static enum plugin_status ig_run(void)
{
    struct ig_catalog catalog;
    int selected = 0;
    enum plugin_status status = PLUGIN_OK;
    bool running = true;

    ig_catalog_scan(&catalog);
    while (running)
    {
        int button;

        ig_draw_catalog(&catalog, selected);
        button = rb->button_get(true);
        switch (button & ~(BUTTON_REPEAT | BUTTON_REL))
        {
            case BUTTON_SCROLL_FWD:
            case BUTTON_RIGHT:
                if (selected + 1 < catalog.count)
                    ++selected;
                break;
            case BUTTON_SCROLL_BACK:
            case BUTTON_LEFT:
                if (selected > 0)
                    --selected;
                break;
            case BUTTON_SELECT:
                if (catalog.count > 0 && !(button & BUTTON_REL))
                    ig_show_details(&catalog.games[selected]);
                break;
            case BUTTON_MENU:
                running = false;
                break;
            default:
                if (button == SYS_USB_CONNECTED)
                {
                    status = PLUGIN_USB_CONNECTED;
                    running = false;
                }
                else
                    rb->default_event_handler(button);
                break;
        }
    }
    return status;
}

#ifdef SIMULATOR
static enum plugin_status ig_run_simulator_gate(const char *parameter)
{
    struct ig_game game;
    struct ig_eapp_probe probe;
    struct ig_vm_report vm_report;
    unsigned int frames = 120;
    const char *frames_value = getenv("IPODGAMES_TEST_FRAMES");
    bool save_exit_path =
        getenv("IPODGAMES_TEST_SAVE_EXIT_PATH") != NULL;
    bool generic_gate = getenv("IPODGAMES_TEST_GENERIC") != NULL;
    bool passed;
    int fd;

    if (frames_value)
    {
        int requested = atoi(frames_value);

        if (requested > 0 && requested <= 3600)
            frames = requested;
    }

    if (!parameter || !ig_game_load(parameter, &game))
        return PLUGIN_ERROR;

    ig_runtime_probe(&game, &probe);
    rb->memset(&vm_report, 0, sizeof(vm_report));
    if (probe.result == IG_PROBE_OK)
        ig_vm_lifecycle_test(&game, frames, &vm_report);
    else
        vm_report.result = IG_VM_UNSUPPORTED_IMAGE;
    passed = probe.result == IG_PROBE_OK &&
             vm_report.result == IG_VM_OK &&
             ((vm_report.completed_frames == frames && !save_exit_path) ||
              (save_exit_path && vm_report.save_exit_requested &&
               vm_report.completed_frames < frames)) &&
             vm_report.draw_calls > 0 &&
             vm_report.presented_frames > 0 &&
             vm_report.nonblack_pixels > 0 &&
             (generic_gate ||
              (vm_report.save_probe_pass &&
               vm_report.save_writes >= 2 &&
               vm_report.save_bytes >= 92)) &&
             vm_report.save_failures == 0;
    fd = rb->open(IG_SIM_LOG, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return PLUGIN_ERROR;

    rb->fdprintf(fd, "IPODGAMES-SIM/1\n");
    rb->fdprintf(fd, "name=%s\n", game.name);
    rb->fdprintf(fd, "guid=%s\n", game.guid);
    rb->fdprintf(fd, "platform_id=%d\n", game.platform_id);
    rb->fdprintf(fd, "build_id=%lu\n", game.build_id);
    rb->fdprintf(fd, "executable_state=%s\n", game.executable_state);
    rb->fdprintf(fd, "probe=%s\n", ig_probe_result_name(probe.result));
    rb->fdprintf(fd, "file_size=%lu\n", probe.file_size);
    rb->fdprintf(fd, "load_base=0x%08lx\n", probe.inferred_load_base);
    rb->fdprintf(fd, "header_word_14=0x%08lx\n", probe.header_word_14);
    rb->fdprintf(fd, "header_word_18=0x%08lx\n", probe.header_word_18);
    rb->fdprintf(fd, "header_word_24=0x%08lx\n", probe.header_word_24);
    rb->fdprintf(fd, "frameworks=%u\n", probe.framework_count);
    rb->fdprintf(fd, "imports=%lu\n", probe.total_imports);
    rb->fdprintf(fd, "vm=%s\n", ig_vm_result_name(vm_report.result));
    rb->fdprintf(fd, "vm_generic_gate=%u\n", generic_gate ? 1 : 0);
    rb->fdprintf(fd, "vm_memory=%lu\n",
                 (unsigned long)vm_report.available_memory);
    rb->fdprintf(fd, "vm_frames=%u\n", vm_report.completed_frames);
    rb->fdprintf(fd, "vm_instructions=%lu\n", vm_report.instructions);
    rb->fdprintf(fd, "vm_framework_calls=%lu\n",
                 vm_report.framework_calls);
    rb->fdprintf(fd, "vm_imports=%u\n", vm_report.import_count);
    rb->fdprintf(fd, "vm_fault=%d\n", vm_report.fault);
    rb->fdprintf(fd, "vm_fault_address=0x%08lx\n",
                 vm_report.fault_address);
    rb->fdprintf(fd, "vm_fault_instruction=0x%08lx\n",
                 vm_report.fault_instruction);
    rb->fdprintf(fd, "vm_last_framework=%u:%u\n",
                 vm_report.last_framework_id,
                 vm_report.last_framework_ordinal);
    rb->fdprintf(fd, "vm_last_framework_lr=0x%08lx\n",
                 vm_report.last_framework_lr);
    rb->fdprintf(fd, "vm_last_framework_r0=0x%08lx\n",
                 vm_report.last_framework_r0);
    rb->fdprintf(fd, "vm_draw_calls=%lu\n", vm_report.draw_calls);
    rb->fdprintf(fd, "vm_presented_frames=%lu\n",
                 vm_report.presented_frames);
    for (unsigned int draw = 0;
         draw < vm_report.draw_history_count; ++draw)
    {
        rb->fdprintf(fd,
                     "vm_draw_%u=%u:%d,%d,%d,%d:%d,%d,%d,%d\n",
                     draw, vm_report.draw_texture_ids[draw],
                     vm_report.draw_rects[draw][0],
                     vm_report.draw_rects[draw][1],
                     vm_report.draw_rects[draw][2],
                     vm_report.draw_rects[draw][3],
                     vm_report.draw_sources[draw][0],
                     vm_report.draw_sources[draw][1],
                     vm_report.draw_sources[draw][2],
                     vm_report.draw_sources[draw][3]);
    }
    for (unsigned int ordinal = 0; ordinal < IG_GLES_ORDINALS;
         ++ordinal)
    {
        if (!vm_report.gles_ordinal_counts[ordinal])
            continue;
        rb->fdprintf(fd,
                     "vm_gles_ordinal_%u=%u:0x%08x,0x%08x,"
                     "0x%08x,0x%08x\n",
                     ordinal, vm_report.gles_ordinal_counts[ordinal],
                     vm_report.gles_last_args[ordinal][0],
                     vm_report.gles_last_args[ordinal][1],
                     vm_report.gles_last_args[ordinal][2],
                     vm_report.gles_last_args[ordinal][3]);
    }
    for (unsigned int mode = 0; mode < IG_GLES_MODES; ++mode)
    {
        if (!vm_report.gles_mode_draws[mode])
            continue;
        rb->fdprintf(fd,
                     "vm_gles_mode_%u=%u:%lu,%lu,%lu:%u,%u,%u,%u\n",
                     mode, vm_report.gles_mode_draws[mode],
                     vm_report.gles_mode_alpha[mode][0],
                     vm_report.gles_mode_alpha[mode][1],
                     vm_report.gles_mode_alpha[mode][2],
                     vm_report.gles_uniform_color[mode][0],
                     vm_report.gles_uniform_color[mode][1],
                     vm_report.gles_uniform_color[mode][2],
                     vm_report.gles_uniform_color[mode][3]);
    }
    for (unsigned int pair = 0;
         pair < vm_report.gles_texture_pair_count; ++pair)
        rb->fdprintf(fd, "vm_gles_pair_%u=%u:%u,%u:%u\n",
                     pair, vm_report.gles_texture_pairs[pair][0],
                     vm_report.gles_texture_pairs[pair][1],
                     vm_report.gles_texture_pairs[pair][2],
                     vm_report.gles_texture_pairs[pair][3]);
    rb->fdprintf(fd,
                 "vm_texture_subimages=%u:%lu:%u:"
                 "%u,%u,%u,%u,0x%04x,0x%04x,0x%08x\n",
                 vm_report.texture_subimages,
                 vm_report.texture_subimage_bytes,
                 vm_report.texture_subimage_failures,
                 vm_report.texture_subimage_last[0],
                 vm_report.texture_subimage_last[1],
                 vm_report.texture_subimage_last[2],
                 vm_report.texture_subimage_last[3],
                 vm_report.texture_subimage_last[4],
                 vm_report.texture_subimage_last[5],
                 vm_report.texture_subimage_last[6]);
    for (unsigned int texture = 0;
         texture < vm_report.texture_count; ++texture)
    {
        rb->fdprintf(fd,
                     "vm_texture_%u=%u:%s:%ux%u:0x%04x,0x%04x:"
                     "%lu:%lu,%lu,%lu\n",
                     texture, vm_report.texture_ids[texture],
                     vm_report.texture_names[texture],
                     vm_report.texture_widths[texture],
                     vm_report.texture_heights[texture],
                     vm_report.texture_formats[texture],
                     vm_report.texture_types[texture],
                     vm_report.texture_draws[texture],
                     vm_report.texture_alpha[texture][0],
                     vm_report.texture_alpha[texture][1],
                     vm_report.texture_alpha[texture][2]);
    }
    rb->fdprintf(fd, "vm_framebuffer_checksum=0x%08lx\n",
                 vm_report.framebuffer_checksum);
    rb->fdprintf(fd, "vm_nonblack_pixels=%lu\n",
                 vm_report.nonblack_pixels);
    rb->fdprintf(fd, "vm_async_reads=%u\n", vm_report.async_reads);
    rb->fdprintf(fd, "vm_async_opens=%u\n", vm_report.async_opens);
    rb->fdprintf(fd, "vm_async_ordinals=%u,%u,%u,%u\n",
                 vm_report.async_ordinal_counts[0],
                 vm_report.async_ordinal_counts[1],
                 vm_report.async_ordinal_counts[2],
                 vm_report.async_ordinal_counts[3]);
    for (unsigned int ordinal = 0; ordinal < 17; ++ordinal)
    {
        if (!vm_report.async_ordinal_counts[ordinal])
            continue;
        rb->fdprintf(fd,
                     "vm_async_ordinal_%u=%u:0x%08x,0x%08x,"
                     "0x%08x,0x%08x\n",
                     ordinal, vm_report.async_ordinal_counts[ordinal],
                     vm_report.async_last_args[ordinal][0],
                     vm_report.async_last_args[ordinal][1],
                     vm_report.async_last_args[ordinal][2],
                     vm_report.async_last_args[ordinal][3]);
    }
    for (unsigned int ordinal = 0;
         ordinal < IG_METADATA_ORDINALS; ++ordinal)
    {
        if (!vm_report.metadata_ordinal_counts[ordinal])
            continue;
        rb->fdprintf(fd,
                     "vm_metadata_ordinal_%u=%u:0x%08x,0x%08x,"
                     "0x%08x,0x%08x\n",
                     ordinal, vm_report.metadata_ordinal_counts[ordinal],
                     vm_report.metadata_last_args[ordinal][0],
                     vm_report.metadata_last_args[ordinal][1],
                     vm_report.metadata_last_args[ordinal][2],
                     vm_report.metadata_last_args[ordinal][3]);
    }
    rb->fdprintf(fd, "vm_async_save_ordinals=%u,%u,%u\n",
                 vm_report.async_ordinal_counts[12],
                 vm_report.async_ordinal_counts[14],
                 vm_report.async_ordinal_counts[16]);
    rb->fdprintf(fd, "vm_save_writes=%u:%lu:%u:%u\n",
                 vm_report.save_writes, vm_report.save_bytes,
                 vm_report.save_failures, vm_report.save_probe_pass);
    rb->fdprintf(fd, "vm_save_exit=%u\n",
                 vm_report.save_exit_requested);
    rb->fdprintf(fd, "vm_last_async_file=%s\n",
                 vm_report.last_async_file);
    rb->fdprintf(fd, "vm_last_async_destination=0x%08lx\n",
                 vm_report.last_async_destination);
    rb->fdprintf(fd, "vm_last_async_capacity=%lu\n",
                 vm_report.last_async_capacity);
    rb->fdprintf(fd, "vm_last_async_bytes=%lu\n",
                 vm_report.last_async_bytes);
    rb->fdprintf(fd, "vm_last_async_context=0x%08x\n",
                 vm_report.last_async_context);
    rb->fdprintf(fd, "vm_last_async_callback=0x%08x\n",
                 vm_report.last_async_callback);
    rb->fdprintf(fd, "vm_last_async_inner=0x%08x:0x%08x\n",
                 vm_report.last_async_inner_callback,
                 vm_report.last_async_inner_context);
    rb->fdprintf(fd, "vm_last_async_manager=0x%08x:0x%08x\n",
                 vm_report.last_async_manager_callback,
                 vm_report.last_async_manager_context);
    rb->fdprintf(fd, "vm_last_async_busy=%u\n",
                 vm_report.last_async_busy);
    rb->fdprintf(fd, "vm_last_async_completion=0x%08x\n",
                 vm_report.last_async_completion);
    rb->fdprintf(fd, "vm_audio_resource_callbacks=%u\n",
                 vm_report.audio_resource_callbacks);
    rb->fdprintf(fd, "vm_audio_trace=0x%02x\n", vm_report.audio_trace);
    rb->fdprintf(fd, "vm_audio_data_reads=%u\n",
                 vm_report.audio_data_reads);
    rb->fdprintf(fd, "vm_audio_data_bytes=%lu\n",
                 vm_report.audio_data_bytes);
    rb->fdprintf(fd, "vm_audio_pending=0x%08x:0x%08x:%u\n",
                 vm_report.audio_pending_callback,
                 vm_report.audio_pending_context,
                 vm_report.audio_loader_busy);
    rb->fdprintf(fd, "vm_audio_playback=%u:%lu\n",
                 vm_report.audio_play_starts,
                 vm_report.audio_pcm_frames);
    rb->fdprintf(fd, "vm_audio_mixer=%u:%u\n",
                 vm_report.audio_max_voices,
                 vm_report.audio_multivoice_probe_pass);
    for (unsigned int ordinal = 0; ordinal < IG_AUDIO_ORDINALS;
         ++ordinal)
    {
        if (!vm_report.audio_ordinal_counts[ordinal])
            continue;
        rb->fdprintf(fd,
                     "vm_audio_ordinal_%u=%u:0x%08x,0x%08x,"
                     "0x%08x,0x%08x\n",
                     ordinal, vm_report.audio_ordinal_counts[ordinal],
                     vm_report.audio_last_args[ordinal][0],
                     vm_report.audio_last_args[ordinal][1],
                     vm_report.audio_last_args[ordinal][2],
                     vm_report.audio_last_args[ordinal][3]);
    }
    rb->fdprintf(fd, "vm_game_state=%u\n", vm_report.game_state);
    rb->fdprintf(fd, "vm_wait_flag=%u\n", vm_report.wait_flag);
    rb->fdprintf(fd, "vm_input_polls=%u\n", vm_report.input_polls);
    rb->fdprintf(fd, "vm_synthetic_input_frame=%u\n",
                 vm_report.synthetic_input_frame);
    rb->fdprintf(fd, "vm_game_object=0x%08x\n", vm_report.game_object);
    rb->fdprintf(fd, "vm_game_flags=0x%08x\n", vm_report.game_flags);
    rb->fdprintf(fd, "vm_game_countdown=%u\n",
                 vm_report.game_countdown);
    rb->fdprintf(fd, "vm_game_input_active=0x%08x\n",
                 vm_report.game_input_active);
    rb->fdprintf(fd, "vm_game_input_pressed=0x%08x\n",
                 vm_report.game_input_pressed);
    rb->fdprintf(fd, "vm_game_input_released=0x%08x\n",
                 vm_report.game_input_released);
    rb->fdprintf(fd, "vm_frontend_updates=%u\n",
                 vm_report.frontend_updates);
    rb->fdprintf(fd, "vm_frontend_complete=%u\n",
                 vm_report.frontend_complete);
    rb->fdprintf(fd, "vm_frontend_blocked=%u\n",
                 vm_report.frontend_blocked);
    rb->fdprintf(fd, "vm_frontend_mode=%u\n", vm_report.frontend_mode);
    rb->fdprintf(fd, "vm_component_input=0x%08x@0x%08x\n",
                 vm_report.component_input,
                 vm_report.component_input_method);
    rb->fdprintf(fd, "vm_component_loader=0x%08x@0x%08x\n",
                 vm_report.component_loader,
                 vm_report.component_loader_method);
    rb->fdprintf(fd, "vm_loader_audio=%u:%u@0x%08x[%u,%u]\n",
                 vm_report.loader_audio_type,
                 vm_report.loader_audio_index,
                 vm_report.loader_audio_slot,
                 vm_report.loader_audio_pending,
                 vm_report.loader_audio_status);
    rb->fdprintf(fd, "vm_component_scene=0x%08x@0x%08x\n",
                 vm_report.component_scene,
                 vm_report.component_scene_method);
    rb->fdprintf(fd, "vm_component_map=0x%08x\n",
                 vm_report.component_map);
    rb->fdprintf(fd, "vm_game_machine_stage=%u/%u/%u/%u\n",
                 vm_report.game_machine_stage,
                 vm_report.game_machine_first_split,
                 vm_report.game_machine_second_split,
                 vm_report.game_machine_stage_end);
    rb->fdprintf(fd, "vm_game_frame_delta=%u\n",
                 vm_report.game_frame_delta);
    rb->fdprintf(fd, "vm_component_state=%u->%u\n",
                 vm_report.component_state,
                 vm_report.component_next_state);
    rb->fdprintf(fd, "vm_component_flags=0x%08x\n",
                 vm_report.component_flags);
    rb->fdprintf(fd, "vm_component_timer=%u\n",
                 vm_report.component_timer);
    rb->fdprintf(fd, "vm_component_toggle=%u\n",
                 vm_report.component_toggle);
    rb->fdprintf(fd, "vm_component_interaction=%u\n",
                 vm_report.component_interaction);
    rb->fdprintf(fd, "vm_component_selection=%u/%u\n",
                 vm_report.component_selection,
                 vm_report.component_selection_count);
    rb->fdprintf(fd, "vm_component_name=%u@%u[%u,%u,%u]\n",
                 vm_report.component_name_length,
                 vm_report.component_name_cursor,
                 vm_report.component_name_backspace,
                 vm_report.component_name_confirm,
                 vm_report.component_name_base);
    rb->fdprintf(fd, "vm_input_handler_calls=%u\n",
                 vm_report.input_handler_calls);
    rb->fdprintf(fd,
                 "vm_input_handler_types=%u,%u,%u,%u,%u,%u,%u,%u\n",
                 vm_report.input_handler_type_counts[0],
                 vm_report.input_handler_type_counts[1],
                 vm_report.input_handler_type_counts[2],
                 vm_report.input_handler_type_counts[3],
                 vm_report.input_handler_type_counts[4],
                 vm_report.input_handler_type_counts[5],
                 vm_report.input_handler_type_counts[6],
                 vm_report.input_handler_type_counts[7]);
    rb->fdprintf(fd, "vm_input_handler_last=0x%08x,%u,%u,%u,%d\n",
                 vm_report.input_handler_last[0],
                 vm_report.input_handler_last[1],
                 vm_report.input_handler_last[2],
                 vm_report.input_handler_last[3],
                 (int)vm_report.input_handler_last[4]);
    rb->fdprintf(fd, "vm_input_wheel_handler=%d:%d:0x%08x:%u:%d\n",
                 vm_report.input_wheel_handler_total,
                 vm_report.input_wheel_handler_last,
                 vm_report.input_wheel_handler_object,
                 vm_report.input_wheel_handler_selection,
                 (int)vm_report.input_wheel_handler_accumulator);
    rb->fdprintf(fd, "vm_input_select_calls=%u\n",
                 vm_report.input_select_calls);
    rb->fdprintf(fd, "vm_input_transition_requests=%u\n",
                 vm_report.input_transition_requests);
    rb->fdprintf(fd, "vm_wheel_mapping=%u:%u:%u\n",
                 vm_report.wheel_mapping_pass,
                 vm_report.wheel_sweep_frames,
                 vm_report.wheel_sweep_unique_frames);
    rb->fdprintf(fd, "vm_armemu_signed_byte=%u\n",
                 vm_report.armemu_signed_byte_pass);
    rb->fdprintf(fd, "vm_system=%u:%u:%u:%u:%u\n",
                 vm_report.system_settings_reads,
                 vm_report.system_clock_reads,
                 vm_report.system_battery_reads,
                 vm_report.system_brightness_gets,
                 vm_report.system_brightness_sets);
    rb->fdprintf(fd, "vm_recent_frameworks=");
    {
        unsigned int recent;

        for (recent = 0; recent < vm_report.recent_framework_count; ++recent)
        {
            unsigned int value = vm_report.recent_framework[recent];

            rb->fdprintf(fd, "%s%u:%u", recent ? "," : "",
                         value >> 16, value & 0xffff);
        }
    }
    rb->fdprintf(fd, "\n");
    rb->fdprintf(fd,
                 "vm_fatal_signal=%#010x,%#010x,%#010x,%#010x:%#010x\n",
                 vm_report.fatal_signal[0], vm_report.fatal_signal[1],
                 vm_report.fatal_signal[2], vm_report.fatal_signal[3],
                 vm_report.fatal_signal[4]);
    rb->fdprintf(fd, "vm_recent_guest=");
    for (unsigned int recent = 0;
         recent < vm_report.recent_guest_count; ++recent)
    {
        rb->fdprintf(fd, "%s%08x:%08x:%08x:%08x",
                     recent ? "," : "",
                     vm_report.recent_guest_pc[recent],
                     vm_report.recent_guest_r4[recent],
                     vm_report.recent_guest_r5[recent],
                     vm_report.recent_guest_sp[recent]);
    }
    rb->fdprintf(fd, "\n");
    rb->fdprintf(fd, "status=%s\n", passed ? "pass" : "fail");
    rb->close(fd);
    /* The log is the gate result; exiting normally keeps headless CI prompt. */
    return PLUGIN_OK;
}
#endif

enum plugin_status plugin_start(const void *parameter)
{
    enum plugin_status status;
    struct ig_game game;

    rb->lcd_setfont(FONT_UI);
    rb->lcd_set_backdrop(NULL);
    rb->lcd_set_drawmode(DRMODE_SOLID);
#ifdef HAVE_WHEEL_POSITION
    rb->wheel_send_events(true);
#endif
#ifdef SIMULATOR
    if (getenv("IPODGAMES_TEST"))
        return ig_run_simulator_gate((const char *)parameter);
#endif
    if (parameter && ig_game_load((const char *)parameter, &game))
    {
        enum ig_vm_result result = ig_vm_run_game(&game);

        status = result == IG_VM_OK ? PLUGIN_OK : PLUGIN_ERROR;
        if (result != IG_VM_OK)
            rb->splashf(HZ * 3, "iPod game: %s",
                        ig_vm_result_name(result));
    }
    else
        status = ig_run();
#ifdef HAVE_WHEEL_POSITION
    rb->wheel_send_events(true);
#endif
    rb->lcd_set_background(LCD_BLACK);
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_clear_display();
    rb->lcd_update();
    return status;
}
