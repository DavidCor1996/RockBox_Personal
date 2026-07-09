#include "mudclient.h"

#if defined(ROCKBOX) && defined(SIMULATOR)
#include "ui/menu.h"
#include "ui/ui-tabs.h"
extern char *getenv(const char *name);
#endif

#ifdef EMSCRIPTEN
/* clang doesn't know what triple equals is, understandably */
/* clang-format off */
EM_JS(int, can_resize, (), {
    return window._mudclientCanResize &&
               document.activeElement !== window._mudclientKeyboard &&
               document.activeElement !== window._mudclientPassword;
});

EM_JS(int, get_window_width, (), { return window.innerWidth; });
EM_JS(int, get_window_height, (), { return window.innerHeight; });

EM_JS(void, browser_trigger_keyboard,
      (char *text, int is_password, int x, int y, int width, int height,
       int font, int is_centred, int is_scaled), {
          const keyboard = is_password ? window._mudclientPassword :
                                         window._mudclientKeyboard;

          keyboard.value = UTF8ToString(text);

          if (is_centred) {
              keyboard.style.height = `${height}px`;
              keyboard.style.textAlign = 'center';
          } else {
              keyboard.style.height = null;
              keyboard.style.textAlign = 'left';
          }

          keyboard.style.left = `${x}px`;
          keyboard.style.top = `${y}px`;

          keyboard.style.width = `${width}px`;

          if (is_scaled) {
              keyboard.style.transform = 'scale(2)';
          } else {
              keyboard.style.transform = 'none';
          }

          const fonts = {
              1: 'mudclient-font-bold-12',
              4: 'mudclient-font-bold-14',
              5: 'mudclient-font-bold-16'
          };

          keyboard.classList.remove(...Object.values(fonts));

          const fontClass = fonts[font];

          if (fontClass) {
              keyboard.classList.add(fontClass);
          }

          keyboard.style.display = 'block';

          keyboard.focus();
      });

EM_JS(int, browser_is_touch, (), { return window._mudclientIsTouch; });
/* clang-format on */

int last_canvas_check = 0;

mudclient *global_mud = NULL;
#endif

int mudclient_finger_1_x = 0;
int mudclient_finger_1_y = 0;
int mudclient_finger_1_down = 0;

int mudclient_finger_2_x = 0;
int mudclient_finger_2_y = 0;
int mudclient_finger_2_down = 0;

int mudclient_full_width = 0;
int mudclient_full_height = 0;

const char *font_files[] = {"h11p.jf", "h12b.jf", "h12p.jf", "h13b.jf",
                            "h14b.jf", "h16b.jf", "h20b.jf", "h24b.jf"};

/* only the first of models with animations are stored in the cache */
const char *animated_models[] = {
    "torcha2",      "torcha3",    "torcha4",    "skulltorcha2", "skulltorcha3",
    "skulltorcha4", "firea2",     "firea3",     "fireplacea2",  "fireplacea3",
    "firespell2",   "firespell3", "lightning2", "lightning3",   "clawspell2",
    "clawspell3",   "clawspell4", "clawspell5", "spellcharge2", "spellcharge3"};

#if !defined(RENDER_GL) && !defined(RENDER_3DS_GL)
/*
 * animations that experienced a loss of fine detail in January 2002 with the
 * "Compression" update
 *
 * camel - eyes lose distinctiveness.
 * bat - most noticable. mouth is nearly gone entirely.
 * bear - loses some shading that gives it more of a "fur" texture.
 * human heads - eyes lose detail.
 * human tops - belt buckles lose detail or become flesh (ew).
 */
static const char *anims_older_is_better[] = {
    "camel",  "bat",           "battleaxe", "bear",  "fbody1",
    "fhead1", "fplatemailtop", "head1",     "head2", "head3",
    "head4",  "platemailtop",  "staff",     "body1", NULL};
#endif

char login_screen_status[255] = {0};

void mudclient_new(mudclient *mud) {
    memset(mud, 0, sizeof(mudclient));

    mud->target_fps = 20;
    mud->loading_step = 1;
    mud->loading_progess_text = "Loading";
    mud->game_width = MUD_WIDTH;
    mud->game_height = MUD_HEIGHT;
    mud->camera_angle = 1;
    mud->camera_rotation = 128;
    mud->camera_rotation_x_increment = 2;
    mud->camera_rotation_y_increment = 2;
    mud->last_plane_index = -1;

    mud->menu_items_size = 32;
    mud->menu_items = calloc(mud->menu_items_size, sizeof(struct MenuEntry));
    mud->menu_indices = calloc(mud->menu_items_size, sizeof(int));

    mud->options = malloc(sizeof(Options));

    options_new(mud->options);
    options_load(mud->options);

    mud->camera_zoom = mud->options->zoom_camera ? ZOOM_OUTDOORS : ZOOM_INDOORS;

    for (int i = 0; i < MESSAGE_HISTORY_LENGTH; i++) {
        memset(mud->message_history[i], '\0', 255);
    }

    mud->selected_spell = -1;
    mud->selected_item_name = "";
    mud->selected_item_inventory_index = -1;
    mud->quest_complete = calloc(quests_length, sizeof(int8_t));

#ifdef _3DS
    mud->_3ds_sound_position = -1;
#endif

    mud->appearance_body_type = 1;
    mud->appearance_hair_colour = 2;
    mud->appearance_top_colour = 8;
    mud->appearance_bottom_colour = 14;
    mud->appearance_head_gender = 1;

    mud->sleep_word_delay = 1;
    mud->offline_profile = RSC_OFFLINE_PROFILE_DAVID;

    /* set by the server to 192 on p2p servers */
    mud->bank_items_max = 48;

    mud->bank_selected_item_slot = -1;
    mud->bank_selected_item = -2;

    mud->sprite_media = 2000;
    mud->sprite_util = mud->sprite_media + 100;
    mud->sprite_item = mud->sprite_util + 50;
    mud->sprite_logo = mud->sprite_item + 1000;
    mud->sprite_projectile = mud->sprite_logo + 10;
    // TODO this is also used for sleep word
    mud->sprite_texture = mud->sprite_projectile + 50;
    mud->sprite_texture_world = mud->sprite_texture + 10;
}

void mudclient_resize(mudclient *mud) {
#ifdef ROCKBOX
    (void)mud;
    return;
#endif
#if !defined(WII) && !defined(_3DS) && !defined(ROCKBOX)
    SDL_FreeSurface(mud->screen);
    SDL_FreeSurface(mud->pixel_surface);

    int surface_width = mud->game_width;
    int surface_height = mud->game_height;

#ifdef SDL12
    mud->screen = SDL_GetVideoSurface();
#else
    mud->screen = SDL_GetWindowSurface(mud->window);

#ifdef RENDER_SW
    if (mudclient_is_ui_scaled(mud)) {
        surface_width /= 2;
        surface_height /= 2;
    }
#endif
#endif

    mud->pixel_surface =
        SDL_CreateRGBSurface(0, surface_width, surface_height, 32, 0xff0000,
                             0x00ff00, 0x0000ff, 0);

    if (mud->surface != NULL) {
#ifdef RENDER_SW
        mud->surface->pixels = mud->pixel_surface->pixels;
#endif

#ifdef RENDER_GL
        free(mud->surface->pixels);

        mud->surface->pixels =
            calloc(mud->game_width * mud->game_height, sizeof(int32_t));
#endif

        panel_destroy(mud->panel_login_welcome);
        free(mud->panel_login_welcome);

        panel_destroy(mud->panel_login_new_user);
        free(mud->panel_login_new_user);

        panel_destroy(mud->panel_login_worldlist);
        free(mud->panel_login_worldlist);

        panel_destroy(mud->panel_login_existing_user);
        free(mud->panel_login_existing_user);

        worldlist_new(mud);

        mudclient_create_login_panels(mud);

        panel_destroy(mud->panel_appearance);
        free(mud->panel_appearance);

        mudclient_create_appearance_panel(mud);

        mud->scene->raster = mud->surface->pixels;

        int is_compact = mud->surface->width < MUD_VANILLA_WIDTH ||
                         mud->surface->height < MUD_VANILLA_HEIGHT;

        int is_touch = mudclient_is_touch(mud);

        int full_offset_x = mud->surface->width - MUD_WIDTH;
        int full_offset_y = mud->surface->height - MUD_HEIGHT;
        int half_offset_x = (mud->surface->width / 2) - (MUD_WIDTH / 2);
        int half_offset_y = (mud->surface->height / 2) - (MUD_HEIGHT / 2);

        int dynamic_offset_x =
            (mud->surface->width / 2) -
            (is_compact ? MUD_MIN_WIDTH : MUD_VANILLA_WIDTH) / 2;

        int dynamic_offset_y =
            (mud->surface->height / 2) -
            (is_compact ? MUD_MIN_HEIGHT : MUD_VANILLA_HEIGHT) / 2;

        if (mud->panel_login_welcome != NULL) {
            mud->panel_login_welcome->offset_x = dynamic_offset_x;
            mud->panel_login_welcome->offset_y = dynamic_offset_y;
        }

        if (mud->panel_login_new_user != NULL) {
            mud->panel_login_new_user->offset_x = dynamic_offset_x;
            mud->panel_login_new_user->offset_y = dynamic_offset_y;
        }

        if (mud->panel_login_existing_user != NULL) {
            mud->panel_login_existing_user->offset_x = dynamic_offset_x;
            mud->panel_login_existing_user->offset_y = dynamic_offset_y;
        }

        if (mud->panel_login_worldlist != NULL) {
            mud->panel_login_worldlist->offset_x = dynamic_offset_x;
            mud->panel_login_worldlist->offset_y = dynamic_offset_y;
        }

        if (mud->panel_appearance != NULL) {
            mud->panel_appearance->offset_x = dynamic_offset_x;
            mud->panel_appearance->offset_y = dynamic_offset_y;
        }

        if (mud->panel_message_tabs != NULL && !is_touch) {
            mud->panel_message_tabs->offset_y = full_offset_y;
        }

        if (mud->panel_quests != NULL) {
            mud->panel_quests->offset_x = full_offset_x;

            if (is_touch) {
                mud->panel_quests->offset_y = full_offset_y;
            }
        }

        if (mud->panel_magic != NULL) {
            mud->panel_magic->offset_x = full_offset_x;

            if (is_touch) {
                mud->panel_magic->offset_y = full_offset_y;
            }
        }

        if (mud->panel_social_list != NULL) {
            mud->panel_social_list->offset_x = full_offset_x;

            if (is_touch) {
                mud->panel_social_list->offset_y = full_offset_y;
            }
        }

        if (mud->panel_game_options != NULL) {
            mud->panel_game_options->offset_x = half_offset_x;
            mud->panel_game_options->offset_y = half_offset_y;
        }

        if (mud->panel_control_options != NULL) {
            mud->panel_control_options->offset_x = half_offset_x;
            mud->panel_control_options->offset_y = half_offset_y;
        }

        if (mud->panel_ui_options != NULL) {
            mud->panel_ui_options->offset_x = half_offset_x;
            mud->panel_ui_options->offset_y = half_offset_y;
        }

        if (mud->panel_bank_options != NULL) {
            mud->panel_bank_options->offset_x = half_offset_x;
            mud->panel_bank_options->offset_y = half_offset_y;
        }
    }

#ifdef RENDER_GL
    glViewport(0, 0, mud->game_width, mud->game_height);
#endif
#endif
}

void mudclient_start_application_common(struct mudclient *mud) {
#ifdef RENDER_GL

#ifdef GLAD
#if defined(SDL_OPENGL) || defined(SDL_WINDOW_OPENGL)
    if (gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress) == 0) {
        mud_error("Error loading GL library through GLAD/SDL\n");
        exit(1);
    }
#else
    if (gladLoadGL() == 0) {
        mud_error("Error loading GL library through GLAD\n");
        exit(1);
    }
#endif
    printf("INFO: Loaded OpenGL version %d.%d\n", GLVersion.major,
           GLVersion.minor);
#elif !defined(ANDROID)
    glewExperimental = GL_TRUE;

    GLenum glew_error = glewInit();

    if (glew_error != GLEW_OK) {
        mud_error("GLEW error: %s\n", glewGetErrorString(glew_error));
        exit(1);
    }
#endif

    glViewport(0, 0, mud->game_width, mud->game_height);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

    /* when two vertices have the same depth, the last one gets drawn rather
     * than the first one. used for entity quads */
    glDepthFunc(GL_LEQUAL);

    /* transparent textures */
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glEnable(GL_SCISSOR_TEST);

    glDisable(GL_MULTISAMPLE);
#endif /* RENDER_GL */

    mudclient_resize(mud);

    mud->surface = malloc(sizeof(Surface));

    surface_new(mud->surface, mud->game_width, mud->game_height, SPRITE_LIMIT,
                mud);

    surface_set_bounds(mud->surface, 0, 0, mud->game_width, mud->game_height);

    mud_log("Started application\n");

#ifdef _3DS
    mudclient_3ds_draw_top_background(mud);
#endif

#ifdef ANDROID
    SDL_SetWindowFullscreen(mud->window, SDL_WINDOW_FULLSCREEN);
#endif

#ifdef ROCKBOX
    if (mud->username[0] == '\0') {
        strcpy(mud->username, RSC_DEFAULT_NAME);
    }
    if (mud->options->username[0] == '\0') {
        strcpy(mud->options->username, RSC_DEFAULT_NAME);
    }
#endif
    mudclient_run(mud);
}

void mudclient_handle_key_press(mudclient *mud, int key_code) {
    if (mud->show_additional_options) {
        Panel *panel = mudclient_get_active_option_panel(mud);
        panel_key_press(panel, key_code);
        return;
    }

    if (!mud->logged_in) {
        if (mud->login_screen == LOGIN_STAGE_WELCOME &&
            mud->panel_login_welcome) {
            panel_key_press(mud->panel_login_welcome, key_code);
        }

        if (mud->login_screen == LOGIN_STAGE_NEW && mud->panel_login_new_user) {
            panel_key_press(mud->panel_login_new_user, key_code);
        }

        if (mud->login_screen == LOGIN_STAGE_EXISTING &&
            mud->panel_login_existing_user) {
            panel_key_press(mud->panel_login_existing_user, key_code);
        }

        /*if (mud->login_screen == 3 && mud->panel_recover_user) {
            panel_key_press(mud->panel_recover_user, key_code);
        }*/
    } else {
        if (mud->show_appearance_change && mud->panel_appearance) {
            panel_key_press(mud->panel_appearance, key_code);
            return;
        }

        if (mud->show_change_password_step == PASSWORD_STEP_NONE &&
            mud->show_dialog_social_input == 0 &&
            mud->show_dialog_offer_x == 0 &&
            !(mud->bank_search_focus && mud->show_dialog_bank) &&
            /*mud->show_dialog_report_abuse_step == 0 &&*/
            !mud->is_sleeping && mud->panel_message_tabs) {
            int is_option_number = mud->options->option_numbers &&
                                   mud->show_option_menu && key_code >= '1' &&
                                   key_code <= '5';

            if (!is_option_number) {
                panel_key_press(mud->panel_message_tabs, key_code);
            }
        }

        if (mud->show_change_password_step == PASSWORD_STEP_MISMATCH ||
            mud->show_change_password_step == PASSWORD_STEP_FINISHED) {
            mud->show_change_password_step = PASSWORD_STEP_NONE;
        }
    }
}

void mudclient_key_pressed(mudclient *mud, int code, int char_code) {
    if (char_code == -1) {
        if (code == K_LEFT) {
            mud->key_left = 1;
        } else if (code == K_RIGHT) {
            mud->key_right = 1;
        } else if (code == K_UP) {
            mud->key_up = 1;
        } else if (code == K_DOWN) {
            mud->key_down = 1;
        } else if (code == K_PAGE_UP) {
            mud->key_page_up = 1;
        } else if (code == K_PAGE_DOWN) {
            mud->key_page_down = 1;
        } else if (code == K_HOME) {
            mud->key_home = 1;
        } else if (code == K_F1) {
            mud->options->interlace = !mud->options->interlace;

            /*for (int i = 0; i < mud->panel_game_options->control_count; i++) {
                if ((int *)mud->ui_options[i] == &mud->options->interlace) {
                    panel_toggle_checkbox(mud->panel_ui_options, i,
                                          mud->options->interlace);
                    break;
                }
            }*/
        } else if (mud->options->escape_clear && code == K_ESCAPE) {
            memset(mud->input_text_current, '\0', INPUT_TEXT_LENGTH + 1);
            memset(mud->input_pm_current, '\0', INPUT_PM_LENGTH + 1);
            memset(mud->input_digits_current, '\0', INPUT_DIGITS_LENGTH + 1);
        }
    } else {
        if (code == K_TAB) {
            mud->key_tab = 1;
        } else if (mud->show_option_menu && mud->options->option_numbers) {
            if (code == K_1) {
                mud->key_1 = 1;
            } else if (code == K_2) {
                mud->key_2 = 1;
            } else if (code == K_3) {
                mud->key_3 = 1;
            } else if (code == K_4) {
                mud->key_4 = 1;
            } else if (code == K_5) {
                mud->key_5 = 1;
            }
        }

        mudclient_handle_key_press(mud, char_code);
    }

    int found_text = 0;

    for (int i = 0; i < CHAR_SET_LENGTH; i++) {
        if (CHAR_SET[i] == char_code) {
            found_text = 1;
            break;
        }
    }

    int should_append_pm = (mud->show_dialog_bank ? mud->bank_search_focus : 1);

    if (found_text) {
        if (!mud->show_dialog_offer_x) {
            size_t current_length = strlen(mud->input_text_current);

            if (current_length < INPUT_TEXT_LENGTH) {
                mud->input_text_current[current_length] = char_code;
                mud->input_text_current[current_length + 1] = '\0';
            }

            size_t pm_length = strlen(mud->input_pm_current);

            if (pm_length < INPUT_PM_LENGTH && should_append_pm) {
                mud->input_pm_current[pm_length] = char_code;
                mud->input_pm_current[pm_length + 1] = '\0';
            }
        } else if ((IS_DIGIT_SEPARATOR(char_code) ||
                    IS_DIGIT_SUFFIX(char_code) ||
                    isdigit((unsigned char)char_code))) {
            size_t digits_length = strlen(mud->input_digits_current);

            if (digits_length < INPUT_DIGITS_LENGTH) {
                int add_digit_char = 1;

                if (digits_length > 0) {
                    int last_digit_char =
                        mud->input_digits_current[digits_length - 1];

                    /* only one suffix */
                    if (IS_DIGIT_SUFFIX(last_digit_char)) {
                        add_digit_char = 0;
                    } else {
                        /* don't allow consecutive decimals or separators */
                        add_digit_char = !(IS_DIGIT_SEPARATOR(char_code) &&
                                           IS_DIGIT_SEPARATOR(last_digit_char));
                    }
                } else {
                    /* don't allow separators or suffixes as first characters */
                    add_digit_char = !IS_DIGIT_SUFFIX(char_code) &&
                                     !IS_DIGIT_SEPARATOR(char_code);
                }

                if (add_digit_char) {
                    mud->input_digits_current[digits_length] = char_code;
                    mud->input_digits_current[digits_length + 1] = '\0';
                }
            }
        }
    }

    if (code == K_ENTER) {
        strcpy(mud->input_text_final, mud->input_text_current);

        if (should_append_pm) {
            strcpy(mud->input_pm_final, mud->input_pm_current);
        }

        if (mud->options->offer_x) {
            char filtered_digits[INPUT_DIGITS_LENGTH + 1] = {0};
            int filtered_length = 0;
            char digits_suffix = '\0';
            int has_decimal = 0;
            size_t digits_length = strlen(mud->input_digits_current);

            for (size_t i = 0; i < digits_length; i++) {
                char digit_char = mud->input_digits_current[i];

                if (isdigit((unsigned char)digit_char)) {
                    filtered_digits[filtered_length++] = digit_char;
                } else if (tolower((unsigned char)digit_char) == 'k' ||
                           tolower((unsigned char)digit_char) == 'm') {
                    digits_suffix = digit_char;
                } else if (!has_decimal && digit_char == '.') {
                    filtered_digits[filtered_length++] = digit_char;
                    has_decimal = 1;
                }
            }

            int scale = 1;

            if (digits_suffix == 'k') {
                scale = 1000;
            } else if (digits_suffix == 'm') {
                scale = 1000000;
            }

            mud->input_digits_final =
                (int)(atof(filtered_digits) * (float)scale);

            memset(mud->input_digits_current, '\0', INPUT_DIGITS_LENGTH + 1);
        }
    } else if (code == K_BACKSPACE) {
        size_t current_length = strlen(mud->input_text_current);

        if (current_length > 0) {
            mud->input_text_current[current_length - 1] = '\0';
        }

        size_t pm_length = strlen(mud->input_pm_current);

        if (pm_length > 0 && should_append_pm) {
            mud->input_pm_current[pm_length - 1] = '\0';
        }

        if (mud->options->offer_x) {
            size_t digits_length = strlen(mud->input_digits_current);

            if (digits_length > 0) {
                mud->input_digits_current[digits_length - 1] = '\0';
            }
        }
    }
}

void mudclient_key_released(mudclient *mud, int code) {
    if (code == K_LEFT) {
        mud->key_left = 0;
    } else if (code == K_RIGHT) {
        mud->key_right = 0;
    } else if (code == K_UP) {
        mud->key_up = 0;
    } else if (code == K_DOWN) {
        mud->key_down = 0;
    } else if (code == K_PAGE_UP) {
        mud->key_page_up = 0;
    } else if (code == K_PAGE_DOWN) {
        mud->key_page_down = 0;
    } else if (code == K_HOME) {
        mud->key_home = 0;
    } else if (code == K_TAB) {
        mud->key_tab = 0;
    } else if (code == K_1) {
        mud->key_1 = 0;
    } else if (code == K_2) {
        mud->key_2 = 0;
    } else if (code == K_3) {
        mud->key_3 = 0;
    } else if (code == K_4) {
        mud->key_4 = 0;
    } else if (code == K_5) {
        mud->key_5 = 0;
    }
}

void mudclient_mouse_moved(mudclient *mud, int x, int y) {
    mud->mouse_x = x;
    mud->mouse_y = y;

#ifdef RENDER_GL
    mud->gl_mouse_x = x;
    mud->gl_mouse_y = y;
#endif

    if (mudclient_is_ui_scaled(mud)) {
        mud->mouse_x /= 2;
        mud->mouse_y /= 2;
    }

    mud->mouse_action_timeout = 0;
}

void mudclient_mouse_released(mudclient *mud, int x, int y, int button) {
    mud->mouse_x = x;
    mud->mouse_y = y;

#ifdef RENDER_GL
    mud->gl_mouse_x = x;
    mud->gl_mouse_y = y;
#endif

    if (mudclient_is_ui_scaled(mud)) {
        mud->mouse_x /= 2;
        mud->mouse_y /= 2;
    }

    mud->mouse_button_down = 0;

    if (button == 2) {
        mud->middle_button_down = 0;

        int tick_delta = get_ticks() - mud->last_mouse_sample_ticks;

        if (tick_delta <= 0) {
            return;
        }

        int x_delta = mud->mouse_x - mud->last_mouse_sample_x;

        mud->camera_momentum = 2 * ((float)x_delta / (float)tick_delta);
    }
}

void mudclient_handle_mouse_history(mudclient *mud, int x, int y) {
    mud->mouse_click_x_history[mud->mouse_click_count] = x;
    mud->mouse_click_y_history[mud->mouse_click_count] = y;

    mud->mouse_click_count =
        (mud->mouse_click_count + 1) & (MOUSE_HISTORY_LENGTH - 1);

    for (int i = 10; i < 4000; i++) {
        int i1 = (mud->mouse_click_count - i) & (MOUSE_HISTORY_LENGTH - 1);

        if (mud->mouse_click_x_history[i1] == x &&
            mud->mouse_click_y_history[i1] == y) {
            int flag = 0;

            for (int j = 1; j < i; j++) {
                int k1 =
                    (mud->mouse_click_count - j) & (MOUSE_HISTORY_LENGTH - 1);

                int l1 = (i1 - j) & (MOUSE_HISTORY_LENGTH - 1);

                if (mud->mouse_click_x_history[l1] != x ||
                    mud->mouse_click_y_history[l1] != y) {
                    flag = 1;
                }

                if (mud->mouse_click_x_history[k1] !=
                        mud->mouse_click_x_history[l1] ||
                    mud->mouse_click_y_history[k1] !=
                        mud->mouse_click_y_history[l1]) {
                    break;
                }

                if (j == i - 1 && flag && mud->combat_timeout == 0 &&
                    mud->logout_timeout == 0) {
                    mudclient_send_logout(mud);
                    return;
                }
            }
        }
    }
}

void mudclient_mouse_pressed(mudclient *mud, int x, int y, int button) {
    mud->mouse_x = x;
    mud->mouse_y = y;

#ifdef RENDER_GL
    mud->gl_mouse_x = x;
    mud->gl_mouse_y = y;
#endif

    if (mudclient_is_ui_scaled(mud)) {
        mud->mouse_x /= 2;
        mud->mouse_y /= 2;
    }

    /*
     * in SDL12 mouse wheel scrolling is treated as digital button press,
     * while in SDL2 it is handled as a different type of event entirely.
     */
    if (button == 4 || button == 5) {
        if (mud->options->mouse_wheel) {
            if (button == 4) {
                mud->mouse_scroll_delta--;
            } else {
                mud->mouse_scroll_delta++;
            }
            return;
        } else {
            /* treat it as a right click when scrolling is disabled */
            button = 3;
        }
    }

    if (mud->options->middle_click_camera != 0 && button == 2) {
        mud->middle_button_down = 1;
        mud->origin_rotation = mud->camera_rotation;
        mud->origin_mouse_x = mud->mouse_x;

        mud->last_mouse_sample_ticks = get_ticks();
        mud->last_mouse_sample_x = mud->mouse_x;
        mud->camera_momentum = 0;
        return;
    }

    mud->mouse_button_down = button == 3 ? 2 : 1;
    mud->last_mouse_button_down = mud->mouse_button_down;
    mud->mouse_action_timeout = 0;

    mudclient_handle_mouse_history(mud, x, y);
}

void mudclient_set_target_fps(mudclient *mud, int fps) {
    mud->target_fps = 1000 / fps;
}

void mudclient_reset_timings(mudclient *mud) {
    for (int i = 0; i < 10; i++) {
        mud->timings[i] = 0;
    }
}

void mudclient_start(mudclient *mud) {
    if (mud->stop_timeout >= 0) {
        mud->stop_timeout = 0;
    }
}

void mudclient_stop(mudclient *mud) {
    if (mud->stop_timeout >= 0) {
        mud->stop_timeout = 4000 / mud->target_fps;
    }
}

void mudclient_draw_loading_progress(mudclient *mud, int percent, char *text) {
    surface_black_screen(mud->surface);

    /* hide the previously drawn textures */
    surface_draw_box(mud->surface, 0, 0, 128, 128, BLACK);

    if (!mud->options->lowmem) {
        /* jagex logo */
        int logo_sprite_id = SPRITE_LIMIT - 1;

        if (mud->surface->sprite_width[logo_sprite_id]) {
            int offset_x = 2;

            int logo_x = (mud->game_width / 2) -
                         (mud->surface->sprite_width[logo_sprite_id] / 2) -
                         offset_x;

            int logo_y = (mud->game_height / 2) -
                         (mud->surface->sprite_height[logo_sprite_id] / 2) - 46;

            surface_draw_sprite(mud->surface, logo_x, logo_y, logo_sprite_id);
        }
    }

    /* loading bar */
    int bar_x = (mud->game_width / 2.0f) - (LOADING_WIDTH / 2.0f);
    int bar_y = (mud->game_height / 2) + 2;
    int width = (int)((percent / (float)100) * LOADING_WIDTH);

    surface_draw_border(mud->surface, bar_x - 2, bar_y - 2, LOADING_WIDTH + 4,
                        LOADING_HEIGHT + 4, GREY_84);

    surface_draw_box(mud->surface, bar_x, bar_y, LOADING_WIDTH, LOADING_HEIGHT,
                     BLACK);

    surface_draw_box(mud->surface, bar_x, bar_y, width, LOADING_HEIGHT,
                     GREY_84);

    int copyright_x = (mud->surface->width / 2) - 1;
    int copyright_y = (mud->surface->height / 2) + 16;

    if (game_fonts[2] != NULL) {
        surface_draw_string_centre(mud->surface, text, copyright_x, copyright_y,
                                   FONT_REGULAR_12, GREY_C6);
    }

    /* footer */
    if (game_fonts[3] != NULL) {
        copyright_y += 20;

        surface_draw_string_centre(
            mud->surface, "Created by JAGeX - visit www.jagex.com", copyright_x,
            copyright_y, FONT_BOLD_13, GREY_C6);

        copyright_x += 7;
        copyright_y += 16;

        char *copyright_date = "2001-2002 Andrew Gower and Jagex Ltd";

        int copyright_icon_x =
            copyright_x - (surface_text_width(copyright_date, 3) / 2) - 8;

        surface_draw_circle(mud->surface, copyright_icon_x + 2, copyright_y - 5,
                            5, GREY_C6, 255, 0);

        surface_draw_circle(mud->surface, copyright_icon_x + 2, copyright_y - 5,
                            4, BLACK, 255, 0);

        surface_draw_string(mud->surface, "c", copyright_icon_x,
                            copyright_y - 2, FONT_REGULAR_11, GREY_C6);

        surface_draw_string_centre(mud->surface, copyright_date, copyright_x,
                                   copyright_y, FONT_BOLD_13, GREY_C6);
    }

#ifdef RENDER_GL
    if (mud->gl_last_swap == 0 || get_ticks() - mud->gl_last_swap >= 16) {
        mudclient_poll_events(mud);
        surface_draw(mud->surface);
#ifdef SDL12
        SDL_GL_SwapBuffers();
#else
        SDL_GL_SwapWindow(mud->gl_window);
#endif
        mud->gl_last_swap = get_ticks();
    } else {
        surface_gl_reset_context(mud->surface);
    }
#elif defined(RENDER_3DS_GL)
    mudclient_3ds_gl_frame_start(mud, 1);
    surface_draw(mud->surface);
    mudclient_3ds_gl_frame_end();
#else
    surface_draw(mud->surface);
#endif
}

int8_t *mudclient_read_data_file(mudclient *mud, char *file, char *description,
                                 int percent) {
    char loading_text[35] = {0}; /* max description is 19 */

    sprintf(loading_text, "Loading %s - 0%%", description);
    mudclient_draw_loading_progress(mud, percent, loading_text);

    int8_t header[6];
#ifdef WII
    const int8_t *file_data = NULL;

    if (strstr(file, "jagex.jag") != NULL) {
        file_data = (int8_t *)jagex_jag;
    } else if (strstr(file, "config") != NULL) {
        file_data = (int8_t *)config85_jag;
    } else if (strstr(file, "media") != NULL) {
        file_data = (int8_t *)media58_jag;
    } else if (strstr(file, "entity") != NULL && strstr(file, ".mem") == NULL) {
        file_data = (int8_t *)entity24_jag;
    } else if (strstr(file, "entity") != NULL && strstr(file, ".mem") != NULL) {
        file_data = (int8_t *)entity24_mem;
    } else if (strstr(file, "textures") != NULL) {
        file_data = (int8_t *)textures17_jag;
    } else if (strstr(file, "maps") != NULL && strstr(file, ".mem") == NULL) {
        file_data = (int8_t *)maps63_jag;
    } else if (strstr(file, "maps") != NULL && strstr(file, ".mem") != NULL) {
        file_data = (int8_t *)maps63_mem;
    } else if (strstr(file, "land") != NULL && strstr(file, ".mem") == NULL) {
        file_data = (int8_t *)land63_jag;
    } else if (strstr(file, "land") != NULL && strstr(file, ".mem") != NULL) {
        file_data = (int8_t *)land63_mem;
    } else if (strstr(file, "models") != NULL) {
        file_data = (int8_t *)models36_jag;
    } else if (strstr(file, "sounds") != NULL) {
        file_data = (int8_t *)sounds1_mem;
    }

    if (file_data == NULL) {
        mud_error("Unable to read file: %s\n", file);
        exit(1);
    }

    memcpy(header, file_data, sizeof(header));
#else

#ifdef ROCKBOX
    char prefixed_file[PATH_MAX];
    snprintf(prefixed_file, sizeof(prefixed_file), "%s/ready/%s",
             RSC_DATA_DIR, file);
    mud_log("Loading %s\n", prefixed_file);
    int archive_stream = open(prefixed_file, O_RDONLY);

    if (archive_stream < 0) {
        snprintf(prefixed_file, sizeof(prefixed_file), "%s/%s", RSC_DATA_DIR,
                 file);
        mud_log("Loading %s\n", prefixed_file);
        archive_stream = open(prefixed_file, O_RDONLY);
    }
#elif defined(ANDROID)
    char *prefixed_file = file;
    SDL_RWops *archive_stream = SDL_RWFromFile(prefixed_file, "rb");
#elif defined(_3DS) || defined(__SWITCH__)
    char prefixed_file[PATH_MAX];
    snprintf(prefixed_file, sizeof(prefixed_file), "romfs:/%s", file);
#else
    char prefixed_file[PATH_MAX];
    snprintf(prefixed_file, sizeof(prefixed_file), "./cache/%s", file);
#endif

#if !defined(ANDROID) && !defined(ROCKBOX)
    printf("INFO: Loading %s\n", prefixed_file);
    FILE *archive_stream = fopen(prefixed_file, "rb");
#endif

    /* attempt to read cache from the current working directory first */
#ifdef ROCKBOX
    if (archive_stream < 0) {
        mud_error("Unable to read file: %s\n", prefixed_file);
        return NULL;
    }
#else
    if (archive_stream == NULL) {
        /* cwd failed, now try the xdg home directory... */
        const char *xdg_home = getenv("XDG_DATA_HOME");

        if (xdg_home == NULL) {
            const char *home = getenv("HOME");
            if (home == NULL) {
                home = "";
            }
            snprintf(prefixed_file, sizeof(prefixed_file),
                     "%s/.local/share/rsc-c/%s", home, file);
        } else {
            snprintf(prefixed_file, sizeof(prefixed_file), "%s/rsc-c/%s",
                     xdg_home, file);
        }

        printf("INFO: Loading %s\n", prefixed_file);
        archive_stream = fopen(prefixed_file, "rb");

        /* XDG failed, now try the global prefix... */
        if (archive_stream == NULL) {
            snprintf(prefixed_file, sizeof(prefixed_file), "%s/%s", MUD_DATADIR,
                     file);

            printf("INFO: Loading %s\n", prefixed_file);
            archive_stream = fopen(prefixed_file, "rb");
        }
    }

    if (archive_stream == NULL) {
        mud_error("Unable to read file: %s\n", prefixed_file);
        exit(1);
    }
#endif

#ifdef ROCKBOX
    if (read(archive_stream, header, sizeof(header)) != (int)sizeof(header)) {
        close(archive_stream);
        mud_error("Unable to read file: %s\n", prefixed_file);
        return NULL;
    }
#elif defined(ANDROID)
    SDL_RWread(archive_stream, header, sizeof(header), 1);
#else
    fread(header, sizeof(header), 1, archive_stream);
#endif
#endif

    int archive_size = ((header[0] & 0xff) << 16) + ((header[1] & 0xff) << 8) +
                       (header[2] & 0xff);

    int archive_size_compressed = ((header[3] & 0xff) << 16) +
                                  ((header[4] & 0xff) << 8) +
                                  (header[5] & 0xff);

    sprintf(loading_text, "Loading %s - 5%%", description);
    mudclient_draw_loading_progress(mud, percent, loading_text);

#ifdef WII
    int8_t *archive_data = file_data + 6;
#else
    int bytes_read = 0;
    int8_t *archive_data = malloc(archive_size_compressed);
    if (archive_data == NULL) {
#ifdef ROCKBOX
        close(archive_stream);
#elif defined(ANDROID)
        SDL_RWclose(archive_stream);
#else
        fclose(archive_stream);
#endif
        mud_error("Unable to allocate %s\n", description);
        return NULL;
    }

    while (bytes_read < archive_size_compressed) {
        int length = archive_size_compressed - bytes_read;

#ifdef ROCKBOX
        int got = read(archive_stream, archive_data + bytes_read, length);
        if (got <= 0) {
            break;
        }
        bytes_read += got;
#elif defined(ANDROID)
        SDL_RWread(archive_stream, archive_data + bytes_read, length, 1);
        bytes_read += length;
#else
        fread(archive_data + bytes_read, length, 1, archive_stream);
        bytes_read += length;
#endif

        sprintf(loading_text, "Loading %s - %d%%", description,
                5 + (bytes_read * 95) / archive_size_compressed);

        mudclient_draw_loading_progress(mud, percent, loading_text);
    }

#ifdef ROCKBOX
    close(archive_stream);
#elif defined(ANDROID)
    SDL_RWclose(archive_stream);
#else
    fclose(archive_stream);
#endif
#endif

    if (bytes_read != archive_size_compressed) {
        free(archive_data);
        mud_error("Unable to read file: %s\n", prefixed_file);
        return NULL;
    }

    sprintf(loading_text, "Unpacking %s", description);
    mudclient_draw_loading_progress(mud, percent, loading_text);

    if (archive_size_compressed != archive_size) {
#ifdef ROCKBOX
        free(archive_data);
        mud_error("Cached archive missing: %s\n", file);
        return NULL;
#else
        int8_t *decompressed = malloc(archive_size);
        if (decompressed == NULL) {
#ifndef WII
            free(archive_data);
#endif
            mud_error("Unable to allocate %s\n", description);
            return NULL;
        }
        bzip_decompress(decompressed, archive_data, archive_size_compressed, 0);

#ifndef WII
        free(archive_data);
#endif

#ifdef ROCKBOX
        rsc_register_archive(decompressed, archive_size, file);
#endif
        return decompressed;
#endif
    }

#ifdef ROCKBOX
    rsc_register_archive(archive_data, archive_size, file);
#endif
    return archive_data;
}

void mudclient_load_jagex(mudclient *mud) {
    int8_t *jagex_jag =
        mudclient_read_data_file(mud, "jagex.jag", "Jagex library", 0);

    if (jagex_jag != NULL) {
#ifdef RENDER_SW
        if (!mud->options->lowmem) {
            size_t len = 0;
            int8_t *logo_tga = load_data("logo.tga", 0, jagex_jag, &len);

            surface_parse_sprite_tga(mud->surface, SPRITE_LIMIT - 1, logo_tga,
                                     len, 0, 0);

            free(logo_tga);
        }
#endif
        for (size_t i = 0; i < FONT_FILES_LENGTH; i++) {
            int8_t *font = load_data(font_files[i], 0, jagex_jag, NULL);
            if (font == NULL) {
                break;
            }
            create_font(font, i);
        }

#ifndef WII
        free(jagex_jag);
#endif
    }
#if defined(RENDER_GL) || defined(RENDER_3DS_GL)
    int logo_sprite_id = SPRITE_LIMIT - 1;

    mud->surface->sprite_width[logo_sprite_id] = 281;
    mud->surface->sprite_height[logo_sprite_id] = 85;
#endif
}

void mudclient_load_game_config(mudclient *mud) {
    char jag[16];

    snprintf(jag, sizeof(jag), "config%d.jag", mud->options->version_config);

    int8_t *config_jag = mudclient_read_data_file(
        mud, jag, "Configuration", 10);

    if (config_jag == NULL) {
        mud->error_loading_data = 1;
        return;
    }

    game_data_load_data(config_jag, mud->options->members,
                        mud->options->version_config);
    free(config_jag);

    /*int8_t *filter_jag = mudclient_read_data_file(
        mud, "filter" VERSION_STR(VERSION_FILTER) ".jag", "Chat system", 15);

    if (filter_jag == NULL) {
        mud->error_loading_data = 1;
        return;
    }

    free(filter_jag);*/

    if (mud->options->members && mud->options->rename_herblaw_items) {
        modify_unidentified_herbs();
        modify_unfinished_potions();
    }
    if (mud->options->rename_herblaw_items) {
        modify_potion_dosage();
    }
}

static void mudclient_load_media_dat(mudclient *mud, void *media_jag) {
    int8_t *index_dat = load_data("index.dat", 0, media_jag, NULL);

    if (mud->options->version_media < 59) {
        surface_parse_sprite(mud->surface, mud->sprite_media,
                             load_data("inv1.dat", 0, media_jag, NULL),
                             index_dat, 1);
    }

    surface_parse_sprite(mud->surface, mud->sprite_media + 1,
                         load_data("inv2.dat", 0, media_jag, NULL), index_dat,
                         6);

    surface_parse_sprite(mud->surface, mud->sprite_media + 9,
                         load_data("bubble.dat", 0, media_jag, NULL), index_dat,
                         1);

    if (!mud->options->lowmem) {
        surface_parse_sprite(mud->surface, mud->sprite_media + 10,
                             load_data("runescape.dat", 0, media_jag, NULL),
                             index_dat, 1);
    }

    surface_parse_sprite(mud->surface, mud->sprite_media + 11,
                         load_data("splat.dat", 0, media_jag, NULL), index_dat,
                         3);

    surface_parse_sprite(mud->surface, mud->sprite_media + 14,
                         load_data("icon.dat", 0, media_jag, NULL), index_dat,
                         8);

    if (!mud->options->lowmem) {
        surface_parse_sprite(mud->surface, mud->sprite_media + 22,
                             load_data("hbar.dat", 0, media_jag, NULL),
                             index_dat, 1);
    }

    surface_parse_sprite(mud->surface, mud->sprite_media + 23,
                         load_data("hbar2.dat", 0, media_jag, NULL), index_dat,
                         1);

    surface_parse_sprite(mud->surface, mud->sprite_media + 24,
                         load_data("compass.dat", 0, media_jag, NULL),
                         index_dat, 1);

    surface_parse_sprite(mud->surface, mud->sprite_media + 25,
                         load_data("buttons.dat", 0, media_jag, NULL),
                         index_dat, 2);

    if (mud->options->version_media >= 59) {
        surface_parse_sprite(mud->surface, mud->sprite_media + 27,
                             load_data("labels.dat", 0, media_jag, NULL),
                             index_dat, 6);

        surface_parse_sprite(mud->surface, mud->sprite_media + 33,
                             load_data("inv3.dat", 0, media_jag, NULL),
                             index_dat, 6);

        surface_parse_sprite(mud->surface, mud->sprite_media + 39,
                             load_data("message.dat", 0, media_jag, NULL),
                             index_dat, 1);

        surface_parse_sprite(mud->surface, mud->sprite_media + 40,
                             load_data("keyboard.dat", 0, media_jag, NULL),
                             index_dat, 1);
    }

    surface_parse_sprite(mud->surface, mud->sprite_util,
                         load_data("scrollbar.dat", 0, media_jag, NULL),
                         index_dat, 2);

    surface_parse_sprite(mud->surface, mud->sprite_util + 2,
                         load_data("corners.dat", 0, media_jag, NULL),
                         index_dat, 4);

    surface_parse_sprite(mud->surface, mud->sprite_util + 6,
                         load_data("arrows.dat", 0, media_jag, NULL), index_dat,
                         2);

    surface_parse_sprite(mud->surface, mud->sprite_projectile,
                         load_data("projectile.dat", 0, media_jag, NULL),
                         index_dat, game_data.projectile_sprite);

    int sprite_count = game_data.item_sprite_count;

    for (int i = 1; sprite_count > 0; i++) {
        char file_name[20] = {0};
        sprintf(file_name, "objects%d.dat", i);

        int current_sprite_count = sprite_count;
        sprite_count -= 30;

        if (current_sprite_count > 30) {
            current_sprite_count = 30;
        }

        surface_parse_sprite(mud->surface, mud->sprite_item + (i - 1) * 30,
                             load_data(file_name, 0, media_jag, NULL),
                             index_dat, current_sprite_count);
    }

    free(index_dat);
}

static void mudclient_load_media_tga(mudclient *mud, void *media_jag) {
    void *data;
    size_t len;

    data = load_data("inv1.tga", 0, media_jag, &len);
    assert(data != NULL);
    surface_parse_sprite_tga(mud->surface, mud->sprite_media,
                         data, len, 1, 1);

    data = load_data("inv2.tga", 0, media_jag, &len);
    assert(data != NULL);
    surface_parse_sprite_tga(mud->surface, mud->sprite_media + 1,
                         data, len, 1, 6);

    data = load_data("bubble.tga", 0, media_jag, &len);
    assert(data != NULL);
    surface_parse_sprite_tga(mud->surface, mud->sprite_media + 9,
                         data, len, 1, 1);

    if (!mud->options->lowmem) {
        data = load_data("runescape.tga", 0, media_jag, &len);
        assert(data != NULL);
        surface_parse_sprite_tga(mud->surface, mud->sprite_media + 10,
                             data, len, 1, 1);
    }

    data = load_data("splat.tga", 0, media_jag, &len);
    assert(data != NULL);
    surface_parse_sprite_tga(mud->surface, mud->sprite_media + 11,
                             data, len, 3, 1);

    data = load_data("icon.tga", 0, media_jag, &len);
    assert(data != NULL);
    surface_parse_sprite_tga(mud->surface, mud->sprite_media + 14,
                             data, len, 4, 2);

    if (!mud->options->lowmem) {
        data = load_data("hbar.tga", 0, media_jag, &len);
        assert(data != NULL);
        surface_parse_sprite_tga(mud->surface, mud->sprite_media + 22,
                                 data, len, 1, 1);
    }

    data = load_data("hbar2.tga", 0, media_jag, &len);
    assert(data != NULL);
    surface_parse_sprite_tga(mud->surface, mud->sprite_media + 23,
                             data, len, 1, 1);

    data = load_data("compass.tga", 0, media_jag, &len);
    assert(data != NULL);
    surface_parse_sprite_tga(mud->surface, mud->sprite_media + 24,
                             data, len, 1, 1);

    data = load_data("buttons.tga", 0, media_jag, &len);
    assert(data != NULL);
    surface_parse_sprite_tga(mud->surface, mud->sprite_media + 25,
                             data, len, 1, 2);

    data = load_data("scrollbar.tga", 0, media_jag, &len);
    assert(data != NULL);
    surface_parse_sprite_tga(mud->surface, mud->sprite_util,
                             data, len, 2, 1);

    data = load_data("corners.tga", 0, media_jag, &len);
    assert(data != NULL);
    surface_parse_sprite_tga(mud->surface, mud->sprite_util + 2,
                             data, len, 4, 1);

    data = load_data("arrows.tga", 0, media_jag, &len);
    assert(data != NULL);
    surface_parse_sprite_tga(mud->surface, mud->sprite_util + 6,
                         data, len, 2, 1);

    data = load_data("projectile.tga", 0, media_jag, &len);
    assert(data != NULL);
    surface_parse_sprite_tga(mud->surface, mud->sprite_projectile,
                             data, len, 3, 1);

    int sprite_count = game_data.item_sprite_count;

    for (int i = 1; sprite_count > 0; i++) {
        char file_name[32];

        snprintf(file_name, sizeof(file_name), "objects%d.tga", i);

        data = load_data(file_name, 0, media_jag, &len);
        if (data == NULL) {
            break;
        }

        int current_sprite_count = sprite_count;
        sprite_count -= 30;

        if (current_sprite_count > 30) {
            current_sprite_count = 30;
        }


        surface_parse_sprite_tga(mud->surface, mud->sprite_item + (i - 1) * 30,
                             data, len, 10, i < 7 ? 3 : 1);
    }
}

void mudclient_load_media(mudclient *mud) {
#if defined(RENDER_GL) || defined(RENDER_SW) || defined(RENDER_3DS_GL)
    char jag[16];

    snprintf(jag, sizeof(jag), "media%d.jag", mud->options->version_media);

    int8_t *media_jag = mudclient_read_data_file(
        mud, jag, "2d graphics", 20);

    if (media_jag == NULL) {
        mud->error_loading_data = 1;
        return;
    }

    if (!MEDIA_IS_TGA(mud->options->version_media)) {
        mudclient_load_media_dat(mud, media_jag);
    } else {
        mudclient_load_media_tga(mud, media_jag);
    }

#ifdef RENDER_SW
    /* this is probably for an optimization, but it is necessary for the action
     * bubble scaling */
    if (mud->options->version_media >= 59) {
        for (int i = 0; i < 6; i++) {
            surface_load_sprite(mud->surface, mud->sprite_media + 33 + i);
        }

        surface_load_sprite(mud->surface, mud->sprite_media + 39);
    } else {
        surface_load_sprite(mud->surface, mud->sprite_media);
    }

    surface_load_sprite(mud->surface, mud->sprite_media + 9);

    for (int i = 11; i <= 26; i++) {
        surface_load_sprite(mud->surface, mud->sprite_media + i);
    }

    for (int i = 0; i < game_data.projectile_sprite; i++) {
        surface_load_sprite(mud->surface, mud->sprite_projectile + i);
    }

    for (int i = 0; i < game_data.item_sprite_count; i++) {
        surface_load_sprite(mud->surface, mud->sprite_item + i);
    }
#endif

#ifndef WII
    free(media_jag);
#endif
#endif
}

void mudclient_load_entities(mudclient *mud) {
#if defined(RENDER_GL) || defined(RENDER_SW) || defined(RENDER_3DS_GL)
    char jag[16];
    snprintf(jag, sizeof(jag), "entity%d.jag", mud->options->version_entity);

    int8_t *entity_jag = mudclient_read_data_file(
        mud, jag, "people and monsters", 30);

    int8_t *entity_jag_legacy = NULL;

#if !defined(RENDER_GL) && !defined(RENDER_3DS_GL)
    if (mud->options->tga_sprites) {
        entity_jag_legacy = mudclient_read_data_file(mud, "entity8.jag",
                                                     "people and monsters", 37);
    }
    if (ENTITY_IS_TGA(mud->options->version_entity)) {
        entity_jag_legacy = entity_jag;
    }
#endif

    if (entity_jag == NULL) {
        mud->error_loading_data = 1;
        return;
    }

    int8_t *index_dat = load_data("index.dat", 0, entity_jag, NULL);
    int8_t *entity_jag_mem = NULL;
    int8_t *index_dat_mem = NULL;

    if (mud->options->members && !ENTITY_IS_TGA(mud->options->version_entity)) {
        snprintf(jag, sizeof(jag), "entity%d.mem",
            mud->options->version_entity);

        entity_jag_mem = mudclient_read_data_file(
            mud, jag, "member graphics", 45);

        if (entity_jag_mem == NULL) {
            mud->error_loading_data = 1;
            return;
        }

        index_dat_mem = load_data("index.dat", 0, entity_jag_mem, NULL);
    }

    int frame_count = 0;
    int animation_index = 0;

    int i = 0;

    for (;;) {
    label0:;
        if (i >= game_data.animation_count) {
            break;
        }
        char *animation_name = game_data.animations[i].name;
        for (int j = 0; j < i; j++) {
            if (strcmp(game_data.animations[j].name, animation_name) != 0) {
                continue;
            }

            game_data.animations[i].file_id = game_data.animations[j].file_id;
            i++;
            goto label0;
        }

        bool older_is_better = false;
        const char *extension = "dat";
        int8_t *archive_file = entity_jag;

#if !defined(RENDER_GL) && !defined(RENDER_3DS_GL)
        if (ENTITY_IS_TGA(mud->options->version_entity)) {
            older_is_better = true;
            extension = "tga";
        } else if (mud->options->tga_sprites) {
            const char **older_names = anims_older_is_better;
            while (*older_names != NULL) {
                if (strcmp(animation_name, *older_names) == 0) {
                    older_is_better = true;
                    extension = "tga";
                    archive_file = entity_jag_legacy;
                    break;
                }

                older_names++;
            }
        }
#endif

        char file_name[255] = {0};
        sprintf(file_name, "%s.%s", animation_name, extension);

        size_t len = 0;

        int8_t *animation_dat = load_data(file_name, 0, archive_file, &len);
        int8_t *animation_index_dat = index_dat;

        if (animation_dat == NULL && mud->options->members) {
            animation_dat = load_data(file_name, 0, entity_jag_mem, &len);
            animation_index_dat = index_dat_mem;
        }

        if (animation_dat != NULL) {
            if (older_is_better) {
                surface_parse_sprite_tga(mud->surface, animation_index,
                                         animation_dat, len, 15, 1);
            } else {
                surface_parse_sprite(mud->surface, animation_index,
                                     animation_dat, animation_index_dat, 15);
            }

            frame_count += 15;

            if (game_data.animations[i].has_a) {
                if (older_is_better && strcmp(animation_name, "camel") == 0) {
                    /* camel attack anim was a much later addition */
                    older_is_better = false;
                    extension = "dat";
                    archive_file = entity_jag;
                }

                sprintf(file_name, "%sa.%s", animation_name, extension);

                int8_t *a_dat = load_data(file_name, 0, archive_file, &len);
                int8_t *a_index_dat = index_dat;

                if (a_dat == NULL && mud->options->members) {
                    a_dat = load_data(file_name, 0, entity_jag_mem, &len);
                    a_index_dat = index_dat_mem;
                }

                if (a_dat == NULL) {
                    goto fallthrough;
                }

                if (older_is_better) {
                    surface_parse_sprite_tga(mud->surface, animation_index + 15,
                                             a_dat, len, 3, 1);
                } else {
                    surface_parse_sprite(mud->surface, animation_index + 15,
                                         a_dat, a_index_dat, 3);
                }

                frame_count += 3;
            }

            if (game_data.animations[i].has_f) {
                sprintf(file_name, "%sf.%s", animation_name, extension);

                int8_t *f_dat = load_data(file_name, 0, archive_file, &len);
                int8_t *f_index_dat = index_dat;

                if (f_dat == NULL && mud->options->members) {
                    f_dat = load_data(file_name, 0, entity_jag_mem, &len);
                    f_index_dat = index_dat_mem;
                }

                if (older_is_better) {
                    surface_parse_sprite_tga(mud->surface, animation_index + 18,
                                             f_dat, len, 9, 1);
                } else {
                    surface_parse_sprite(mud->surface, animation_index + 18,
                                         f_dat, f_index_dat, 9);
                }

                frame_count += 9;
            }

fallthrough:
            /* TODO why? */
            if (game_data.animations[i].gender != 0) {
                for (int j = animation_index; j < animation_index + 27; j++) {
                    surface_load_sprite(mud->surface, j);
                }
            }
        }

        game_data.animations[i].file_id = animation_index;
        animation_index += 27;

        i++;
    }

    mud_log("Loaded: %d frames of animation\n", frame_count);

#ifndef WII
    free(entity_jag);
    if (entity_jag_legacy != entity_jag) {
        free(entity_jag_legacy);
    }
    free(entity_jag_mem);
#endif

    free(index_dat);
    free(index_dat_mem);
#endif
}

void mudclient_load_textures(mudclient *mud) {
#ifdef RENDER_SW
    char jag[16];

    snprintf(jag, sizeof(jag), "textures%d.jag",
        mud->options->version_textures);

    int8_t *textures_jag = mudclient_read_data_file(mud, jag, "Textures", 50);

    if (textures_jag == NULL) {
        mud->error_loading_data = 1;
        return;
    }

    int8_t *index_dat = load_data("index.dat", 0, textures_jag, NULL);

    scene_allocate_textures(mud->scene, game_data.texture_count, 7, 11);

    char file_name[255] = {0};

    Surface *surface = mud->surface;

    for (int i = 0; i < game_data.texture_count; i++) {
#ifdef USE_TOONSCAPE
        if (toonscape_avoid_load(i)) {
            continue;
        }
#endif
        sprintf(file_name, "%s.dat", game_data.textures[i].name);

        int8_t *texture_dat = load_data(file_name, 0, textures_jag, NULL);
        if (texture_dat == NULL) {
            continue;
        }

        surface_parse_sprite(surface, mud->sprite_texture, texture_dat,
                             index_dat, 1);

        surface_draw_box(surface, 0, 0, 128, 128, MAGENTA);
        surface_draw_sprite(surface, 0, 0, mud->sprite_texture);

#ifndef USE_LOCOLOUR
        free(surface->sprite_palette[mud->sprite_texture]);
        surface->sprite_palette[mud->sprite_texture] = NULL;
#endif

        free(surface->sprite_colours[mud->sprite_texture]);
        surface->sprite_colours[mud->sprite_texture] = NULL;

        int texture_size = surface->sprite_width_full[mud->sprite_texture];
        char *name_sub = game_data.textures[i].subtype_name;

        if (name_sub) {
            int sub_length = strlen(name_sub);

            if (sub_length > 0 && sub_length <= 250) {
                sprintf(file_name, "%s.dat", name_sub);

                int8_t *texture_sub_dat =
                    load_data(file_name, 0, textures_jag, NULL);

                surface_parse_sprite(surface, mud->sprite_texture,
                                     texture_sub_dat, index_dat, 1);

                surface_draw_sprite(surface, 0, 0, mud->sprite_texture);

#ifndef USE_LOCOLOUR
                free(surface->sprite_palette[mud->sprite_texture]);
                surface->sprite_palette[mud->sprite_texture] = NULL;
#endif

                free(surface->sprite_colours[mud->sprite_texture]);
                surface->sprite_colours[mud->sprite_texture] = NULL;
            }
        }

        surface_screen_raster_to_sprite(surface, mud->sprite_texture_world + i,
                                        0, 0, texture_size, texture_size);

        for (int j = 0; j < texture_size * texture_size; j++) {
            if (surface->surface_pixels[mud->sprite_texture_world + i][j] ==
                GREEN) {
                surface->surface_pixels[mud->sprite_texture_world + i][j] =
                    MAGENTA;
            }
        }

        surface_screen_raster_to_palette_sprite(surface,
                                                mud->sprite_texture_world + i);

        scene_define_texture(
            mud->scene, i,
            surface->sprite_colours[mud->sprite_texture_world + i],
            surface->sprite_palette[mud->sprite_texture_world + i],
            (texture_size / 64) - 1);

        free(surface->surface_pixels[mud->sprite_texture_world + i]);
        surface->surface_pixels[mud->sprite_texture_world + i] = NULL;
    }

    free(index_dat);

#ifndef WII
    free(textures_jag);
#endif
#else
    (void)mud;
#endif
}

void mudclient_load_models(mudclient *mud) {
    if (!mud->options->lowmem) {
        for (int i = 0; i < ANIMATED_MODELS_LENGTH; i++) {
            game_data_get_model_index(mud_strdup(animated_models[i]));
        }
    }

    char models_filename[16];

    snprintf(models_filename, sizeof(models_filename),
        "models%d.jag", mud->options->version_models);

    int8_t *models_jag =
        mudclient_read_data_file(mud, models_filename, "3d models", 60);

    if (models_jag == NULL) {
        mud->error_loading_data = 1;
        return;
    }

    for (int i = 0; i < game_data.model_count; i++) {
        char *model_name = game_data.model_name[i];

        char file_name[strlen(model_name) + 5];
        sprintf(file_name, "%s.ob3", model_name);

        uint32_t offset = get_data_file_offset(file_name, models_jag);
        uint32_t len = get_data_file_length(file_name, models_jag);

        GameModel *game_model = malloc(sizeof(GameModel));

        if (offset != 0) {
            game_model_new_ob3(game_model, models_jag + offset, len);
        } else {
            mud_error("missing model \"%s.ob3\" from %s\n", model_name,
                      models_filename);

            game_model_new_alloc(game_model, 1, 1);
        }

        mud->game_models[i] = game_model;

        if (strcmp(model_name, "giantcrystal") == 0) {
            mud->game_models[i]->transparent = 1;
        }
    }

    if (mud->options->ground_item_models) {
#if defined(RENDER_GL) || defined(RENDER_3DS_GL)
        mud->item_models = calloc(game_data.item_count, sizeof(GameModel *));

        for (int i = 0; i < game_data.item_count; i++) {
            int sprite_id = game_data.items[i].sprite;

            char file_name[21] = {0};
            sprintf(file_name, "item-%d.ob3", sprite_id);

            uint32_t offset = get_data_file_offset(file_name, models_jag);
            uint32_t len = get_data_file_length(file_name, models_jag);

            if (offset == 0) {
                continue;
            }

            GameModel *game_model = malloc(sizeof(GameModel));
            game_model_new_ob3(game_model, models_jag + offset, len);

            int mask_colour = game_data.items[i].mask;

            if (mask_colour != 0) {
                game_model_mask_faces(game_model, game_model->face_fill_back,
                                      mask_colour);

                game_model_mask_faces(game_model, game_model->face_fill_front,
                                      mask_colour);
            }

            mud->item_models[i] = game_model;
        }
#else
        int max_sprite_id = 0;

        for (int i = 0; i < game_data.item_count; i++) {
            int sprite_id = game_data.items[i].sprite;

            if (sprite_id > max_sprite_id) {
                max_sprite_id = sprite_id;
            }
        }

        mud->item_models = calloc(max_sprite_id, sizeof(GameModel *));

        for (int i = 0; i < max_sprite_id; i++) {
            char file_name[21] = {0};
            sprintf(file_name, "item-%d.ob3", i);

            uint32_t offset = get_data_file_offset(file_name, models_jag);
            uint32_t len = get_data_file_length(file_name, models_jag);

            if (offset == 0) {
                continue;
            }

            GameModel *game_model = malloc(sizeof(GameModel));
            game_model_new_ob3(game_model, models_jag + offset, len);

            mud->item_models[i] = game_model;
        }
#endif
    }

    free(models_jag);

#ifdef RENDER_GL
    int models_length = game_data.model_count - 1;
    int item_models_length = game_data.item_count;

    if (mud->options->ground_item_models) {
        models_length += item_models_length;
    }

    GameModel *models_buffer[models_length];

    for (int i = 0; i < game_data.model_count - 1; i++) {
        models_buffer[i] = mud->game_models[i];
    }

    if (mud->options->ground_item_models) {
        for (int i = 0; i < item_models_length; i++) {
            models_buffer[game_data.model_count - 1 + i] = mud->item_models[i];
        }
    }

    game_model_gl_buffer_models(&mud->scene->gl_game_model_buffers,
                                &mud->scene->gl_game_model_buffer_length,
                                models_buffer, models_length);
#endif
}

void mudclient_load_maps(mudclient *mud) {
    char jag[16];

    snprintf(jag, sizeof(jag), "maps%d.jag", mud->options->version_maps);
    mud->world->map_pack = mudclient_read_data_file(
        mud, jag, "map", 70);

    if (mud->options->members) {
        snprintf(jag, sizeof(jag), "maps%d.mem", mud->options->version_maps);
        mud->world->member_map_pack = mudclient_read_data_file(
            mud, jag, "members map", 75);
    }

    if (HAS_SEPARATE_LAND(mud->options->version_maps)) {
        snprintf(jag, sizeof(jag), "land%d.jag", mud->options->version_maps);
        mud->world->landscape_pack = mudclient_read_data_file(
            mud, jag, "landscape", 80);

        if (mud->options->members) {
            snprintf(jag, sizeof(jag), "land%d.mem",
                mud->options->version_maps);
            mud->world->member_landscape_pack = mudclient_read_data_file(
                mud, jag, "members landscape", 85);
        }
    }
}

void mudclient_load_sounds(mudclient *mud) {
    char jag[16];

    snprintf(jag, sizeof(jag), "sounds%d.mem", mud->options->version_sounds);

    mud->sound_data = mudclient_read_data_file(mud, jag, "Sound effects", 90);
}

#ifdef ROCKBOX
struct OfflineNpcSpawn {
    int server_index;
    int global_x;
    int global_y;
    int npc_id;
    int direction;
};

struct OfflineItemSpawn {
    int id;
    int global_x;
    int global_y;
};

enum {
    RSC_MAX_OFFLINE_NPC_SPAWNS = 768,
    RSC_MAX_OFFLINE_ITEM_SPAWNS = 384,
    RSC_SPAWN_FILE_BUFFER_SIZE = 65536
};

static const struct OfflineNpcSpawn offline_fallback_npc_spawns[] = {
    {0, 2524, 2337, 1, DIR_SOUTH},  /* Bob */
    {1, 2537, 2319, 5, DIR_SOUTH},  /* Hans */
    {2, 2535, 2329, 7, DIR_SOUTH},  /* cook */
    {3, 2534, 2311, 55, DIR_SOUTH}, /* shopkeeper */
    {4, 2535, 2308, 83, DIR_SOUTH}, /* shop assistant */
    {5, 2513, 2335, 9, DIR_SOUTH},  /* priest */
    {6, 2521, 2279, 63, DIR_SOUTH}, /* farmer */
    {7, 2559, 2286, 77, DIR_SOUTH}, /* Fred the farmer */
    {8, 2518, 2271, 3, DIR_SOUTH},  /* chicken */
    {9, 2516, 2297, 62, DIR_SOUTH}, /* goblin */
};

static const struct OfflineItemSpawn offline_fallback_item_spawns[] = {
    {11, 2540, 2316},  /* Bronze Arrows */
    {13, 2530, 2335},  /* Knife */
    {19, 2519, 2271},  /* Egg */
    {21, 2521, 2275},  /* Bucket */
    {28, 2519, 2299},  /* Iron dagger */
    {33, 2517, 2337},  /* Air-Rune */
    {35, 2538, 2336},  /* Mind-Rune */
    {135, 2534, 2328}, /* pot */
    {140, 2533, 2329}, /* jug */
};

static struct OfflineNpcSpawn
    offline_loaded_npc_spawns[RSC_MAX_OFFLINE_NPC_SPAWNS];
static struct OfflineItemSpawn
    offline_loaded_item_spawns[RSC_MAX_OFFLINE_ITEM_SPAWNS];
static int offline_loaded_npc_spawn_count;
static int offline_loaded_item_spawn_count;
static int offline_spawn_data_loaded;
static int offline_spawn_data_warned;
static int offline_combat_cooldown;
static int offline_combat_engaged;
static int offline_combat_phase;
static int offline_respawn_timer;
static int offline_respawn_server_index = -1;
static int offline_combat_server_index = -1;
static int offline_ground_item_defer_updates;
static int offline_walk_x[PATH_STEPS_MAX];
static int offline_walk_y[PATH_STEPS_MAX];
static int offline_walk_len;
static int offline_walk_pos;

static int mudclient_openrsc_x_to_global(int raw_x) {
    return raw_x + RSC_OPENRSC_X_OFFSET;
}

static int mudclient_openrsc_y_to_global(int raw_y) {
    return raw_y + RSC_OPENRSC_Y_OFFSET;
}

static char *mudclient_find_char(char *text, char wanted) {
    while (text != NULL && *text != '\0') {
        if (*text == wanted) {
            return text;
        }
        text++;
    }

    return NULL;
}

static int mudclient_parse_spawn_int(const char *text, int *value) {
    int sign = 1;
    int parsed = 0;
    int result = 0;

    if (text == NULL || value == NULL) {
        return 0;
    }

    while (*text == ' ' || *text == '\t') {
        text++;
    }

    if (*text == '-') {
        sign = -1;
        text++;
    }

    while (*text >= '0' && *text <= '9') {
        result = result * 10 + (*text - '0');
        parsed = 1;
        text++;
    }

    if (!parsed) {
        return 0;
    }

    *value = result * sign;
    return 1;
}

static char *mudclient_next_tsv_field(char **cursor) {
    char *field;
    char *end;

    if (cursor == NULL || *cursor == NULL) {
        return NULL;
    }

    field = *cursor;
    end = field;
    while (*end != '\0' && *end != '\t') {
        end++;
    }

    if (*end == '\t') {
        *end = '\0';
        *cursor = end + 1;
    } else {
        *cursor = end;
    }

    return field;
}

static int mudclient_read_spawn_file(const char *filename, char *buffer,
                                     int buffer_size) {
    char path[256];
    int fd;
    int total = 0;
    const char *roots[] = {
        RSC_DATA_DIR,
        ROCKBOX_DIR "/ipodjs/runescape_classic",
    };

    if (filename == NULL || buffer == NULL || buffer_size <= 1) {
        return 0;
    }

    for (int root = 0; root < 2; root++) {
        snprintf(path, sizeof(path), "%s/%s", roots[root], filename);
        fd = open(path, O_RDONLY);
        if (fd < 0) {
            continue;
        }

        while (total < buffer_size - 1) {
            int chunk = read(fd, buffer + total, buffer_size - 1 - total);
            if (chunk <= 0) {
                break;
            }
            total += chunk;
        }

        close(fd);
        buffer[total] = '\0';
        return total > 0;
    }

    buffer[0] = '\0';
    return 0;
}

static int mudclient_spawn_line_is_comment(char *line) {
    if (line == NULL) {
        return 1;
    }

    while (*line == ' ' || *line == '\t' || *line == '\r') {
        line++;
    }

    return *line == '\0' || *line == '\n' || *line == '#';
}

static void mudclient_load_offline_npc_spawns(void) {
    static char buffer[RSC_SPAWN_FILE_BUFFER_SIZE];
    char *line = buffer;
    int count = 0;

    if (!mudclient_read_spawn_file("npc_spawns.tsv", buffer,
                                   sizeof(buffer))) {
        return;
    }

    while (line != NULL && *line != '\0') {
        char *next = mudclient_find_char(line, '\n');
        char *cursor;
        char *field;
        int id;
        int raw_x;
        int raw_y;
        int facing;

        if (next != NULL) {
            *next = '\0';
            next++;
        }

        if (mudclient_spawn_line_is_comment(line)) {
            line = next;
            continue;
        }

        cursor = line;
        field = mudclient_next_tsv_field(&cursor);
        if (!mudclient_parse_spawn_int(field, &id)) {
            line = next;
            continue;
        }

        mudclient_next_tsv_field(&cursor);
        field = mudclient_next_tsv_field(&cursor);
        if (!mudclient_parse_spawn_int(field, &raw_x)) {
            line = next;
            continue;
        }

        field = mudclient_next_tsv_field(&cursor);
        if (!mudclient_parse_spawn_int(field, &raw_y)) {
            line = next;
            continue;
        }

        field = mudclient_next_tsv_field(&cursor);
        if (!mudclient_parse_spawn_int(field, &facing)) {
            facing = DIR_SOUTH;
        }

        if (count < RSC_MAX_OFFLINE_NPC_SPAWNS && id >= 0 &&
            id < game_data.npc_count) {
            offline_loaded_npc_spawns[count].server_index = count;
            offline_loaded_npc_spawns[count].global_x =
                mudclient_openrsc_x_to_global(raw_x);
            offline_loaded_npc_spawns[count].global_y =
                mudclient_openrsc_y_to_global(raw_y);
            offline_loaded_npc_spawns[count].npc_id = id;
            offline_loaded_npc_spawns[count].direction = facing & 7;
            count++;
        }

        line = next;
    }

    offline_loaded_npc_spawn_count = count;
}

static void mudclient_load_offline_item_spawns(void) {
    static char buffer[RSC_SPAWN_FILE_BUFFER_SIZE];
    char *line = buffer;
    int count = 0;

    if (!mudclient_read_spawn_file("item_spawns.tsv", buffer,
                                   sizeof(buffer))) {
        return;
    }

    while (line != NULL && *line != '\0') {
        char *next = mudclient_find_char(line, '\n');
        char *cursor;
        char *field;
        int id;
        int raw_x;
        int raw_y;

        if (next != NULL) {
            *next = '\0';
            next++;
        }

        if (mudclient_spawn_line_is_comment(line)) {
            line = next;
            continue;
        }

        cursor = line;
        field = mudclient_next_tsv_field(&cursor);
        if (!mudclient_parse_spawn_int(field, &id)) {
            line = next;
            continue;
        }

        mudclient_next_tsv_field(&cursor);
        field = mudclient_next_tsv_field(&cursor);
        if (!mudclient_parse_spawn_int(field, &raw_x)) {
            line = next;
            continue;
        }

        field = mudclient_next_tsv_field(&cursor);
        if (!mudclient_parse_spawn_int(field, &raw_y)) {
            line = next;
            continue;
        }

        if (count < RSC_MAX_OFFLINE_ITEM_SPAWNS && id >= 0 &&
            id < game_data.item_count) {
            offline_loaded_item_spawns[count].id = id;
            offline_loaded_item_spawns[count].global_x =
                mudclient_openrsc_x_to_global(raw_x);
            offline_loaded_item_spawns[count].global_y =
                mudclient_openrsc_y_to_global(raw_y);
            count++;
        }

        line = next;
    }

    offline_loaded_item_spawn_count = count;
}

static void mudclient_load_offline_spawn_data(void) {
    if (offline_spawn_data_loaded) {
        return;
    }

    offline_loaded_npc_spawn_count = 0;
    offline_loaded_item_spawn_count = 0;
    mudclient_load_offline_npc_spawns();
    mudclient_load_offline_item_spawns();
    offline_spawn_data_loaded = 1;
}

enum {
    RSC_SAVE_VERSION = 1,
    RSC_OFFLINE_QUEST_SAVE_MAX = 64,
    RSC_QUEST_COOKS_ASSISTANT = 1,
    RSC_QUEST_RESTLESS_GHOST = 4,
    RSC_QUEST_SHEEP_SHEARER = 11,
    RSC_ITEM_BONES = 20,
    RSC_ITEM_KNIFE = 13,
    RSC_ITEM_BUCKET = 21,
    RSC_ITEM_SHORTBOW = 189,
    RSC_ITEM_SWORDFISH = 370,
    RSC_ITEM_LOBSTER = 373,
    RSC_ITEM_BIG_BONES = 413,
    RSC_ITEM_BAT_BONES = 604,
    RSC_ITEM_DRAGON_BONES = 814,
    RSC_ITEM_RUNE_LONGSWORD = 75,
    RSC_ITEM_RUNE_TWO_HANDED_SWORD = 81,
    RSC_ITEM_RUNE_BATTLE_AXE = 93,
    RSC_ITEM_RUNE_SCIMITAR = 398,
    RSC_ITEM_RUNE_PLATE_BODY = 401,
    RSC_ITEM_RUNE_PLATE_LEGS = 402,
    RSC_ITEM_RUNE_KITE_SHIELD = 404,
    RSC_ITEM_DRAGON_SWORD = 593,
    RSC_ITEM_DRAGON_AXE = 594,
    RSC_ITEM_CHARGED_DRAGONSTONE_AMULET = 597,
    RSC_ITEM_MAGIC_LONGBOW = 656,
    RSC_ITEM_RUNE_ARROWS = 646,
    RSC_ITEM_STEEL_GAUNTLETS = 698,
    RSC_ITEM_ENCHANTED_FIRE_BATTLESTAFF = 682,
    RSC_ITEM_ENCHANTED_WATER_BATTLESTAFF = 683,
    RSC_ITEM_ENCHANTED_AIR_BATTLESTAFF = 684,
    RSC_ITEM_ENCHANTED_EARTH_BATTLESTAFF = 685,
    RSC_ITEM_DRAGON_MEDIUM_HELMET = 795,
    RSC_ITEM_BOOTS = 966,
    RSC_ITEM_RUNE_THROWING_DART = 1070,
    RSC_ITEM_RUNE_THROWING_KNIFE = 1080,
    RSC_ITEM_RUNE_SPEAR = 1092,
    RSC_ITEM_ZAMORAK_CAPE = 1213,
    RSC_ITEM_SARADOMIN_CAPE = 1214,
    RSC_ITEM_GUTHIX_CAPE = 1215,
    RSC_ITEM_DRAGON_SQUARE_SHIELD = 1278,
    RSC_ITEM_CAPE_OF_LEGENDS = 1288
};

struct OfflineSave {
    int profile;
    int global_x;
    int global_y;
    int has_position;
    int appearance_hair;
    int appearance_top;
    int appearance_bottom;
    int appearance_skin;
    int combat_style;
    int player_quest_points;
    int quest_complete[RSC_OFFLINE_QUEST_SAVE_MAX];
    int bank_items_max;
    int bank_item_count;
    int bank_items[BANK_ITEMS_MAX];
    int bank_items_count[BANK_ITEMS_MAX];
    int inventory_items_count;
    int inventory_item_id[INVENTORY_ITEMS_MAX];
    int inventory_item_stack_count[INVENTORY_ITEMS_MAX];
    int inventory_equipped[INVENTORY_ITEMS_MAX];
    int player_skill_current[PLAYER_SKILL_COUNT];
    int player_skill_base[PLAYER_SKILL_COUNT];
    int player_experience[PLAYER_SKILL_COUNT];
};

static int mudclient_step_delta(int value) {
    if (value > 0) {
        return 1;
    }
    if (value < 0) {
        return -1;
    }
    return 0;
}

static void mudclient_clear_offline_walk(void) {
    offline_walk_len = 0;
    offline_walk_pos = 0;
}

static void mudclient_reset_character_waypoints(GameCharacter *player) {
    if (player == NULL) {
        return;
    }

    player->moving_step = 0;
    player->waypoint_current = 0;
    player->waypoints_x[0] = player->current_x;
    player->waypoints_y[0] = player->current_y;
}

static int mudclient_append_offline_walk_tile(int x, int y) {
    if (offline_walk_len >= PATH_STEPS_MAX) {
        return 0;
    }

    offline_walk_x[offline_walk_len] = x;
    offline_walk_y[offline_walk_len] = y;
    offline_walk_len++;
    return 1;
}

static int mudclient_append_offline_walk_segment(int *from_x, int *from_y,
                                                int to_x, int to_y) {
    while (*from_x != to_x || *from_y != to_y) {
        *from_x += mudclient_step_delta(to_x - *from_x);
        *from_y += mudclient_step_delta(to_y - *from_y);

        if (!mudclient_append_offline_walk_tile(*from_x, *from_y)) {
            return 0;
        }
    }

    return 1;
}

static void mudclient_feed_offline_walk(mudclient *mud) {
    GameCharacter *player = mud->local_player;

    if (player == NULL) {
        mudclient_clear_offline_walk();
        return;
    }

    while (offline_walk_pos < offline_walk_len) {
        int waypoint = (player->waypoint_current + 1) % WAYPOINT_COUNT;

        if ((waypoint + 1) % WAYPOINT_COUNT == player->moving_step) {
            break;
        }

        player->waypoints_x[waypoint] =
            offline_walk_x[offline_walk_pos] * MAGIC_LOC + 64;
        player->waypoints_y[waypoint] =
            offline_walk_y[offline_walk_pos] * MAGIC_LOC + 64;
        player->waypoint_current = waypoint;
        offline_walk_pos++;
    }

    if (offline_walk_pos >= offline_walk_len &&
        player->moving_step ==
            (player->waypoint_current + 1) % WAYPOINT_COUNT) {
        mudclient_clear_offline_walk();
    }
}

static int mudclient_start_offline_walk(mudclient *mud, int start_x,
                                        int start_y, int route_steps) {
    int from_x = start_x;
    int from_y = start_y;

    mudclient_clear_offline_walk();
    mudclient_reset_character_waypoints(mud->local_player);

    for (int i = route_steps - 1; i >= 0; i--) {
        if (!mudclient_append_offline_walk_segment(
                &from_x, &from_y, mud->walk_path_x[i], mud->walk_path_y[i])) {
            mudclient_clear_offline_walk();
            return 0;
        }
    }

    mudclient_feed_offline_walk(mud);
    return 1;
}

static void mudclient_run_offline_player(mudclient *mud) {
    if (mud == NULL || mud->local_player == NULL ||
        mud->local_player->current_animation == 8 ||
        mud->local_player->current_animation == 9) {
        return;
    }

    for (int i = 0; i < 3; i++) {
        int old_x = mud->local_player->current_x;
        int old_y = mud->local_player->current_y;

        mudclient_feed_offline_walk(mud);
        game_character_move(mud->local_player);

        if (mud->local_player->current_x == old_x &&
            mud->local_player->current_y == old_y) {
            break;
        }
    }
}

static void mudclient_offline_save_path(mudclient *mud, char *path,
                                        size_t path_size) {
    char file[32];

    snprintf(file, sizeof(file), "offline_%d.sav", mud->offline_profile);
    get_config_path(file, path);
    (void)path_size;
}

static int mudclient_write_save_line(int fd, const char *line) {
    size_t length = strlen(line);
    return write(fd, line, length) == (ssize_t)length;
}

static int mudclient_offline_find_inventory_slot(mudclient *mud, int id);
static void mudclient_set_offline_base_appearance(mudclient *mud,
                                                  GameCharacter *player);

static void mudclient_add_offline_inventory_item(mudclient *mud, int id,
                                                int count, int equipped) {
    int slot = mud->inventory_items_count;

    if (slot >= INVENTORY_ITEMS_MAX || id < 0 || id >= game_data.item_count) {
        return;
    }

    if (count < 1) {
        count = 1;
    }

    mud->inventory_item_id[slot] = id;
    mud->inventory_item_stack_count[slot] = count;
    mud->inventory_equipped[slot] = equipped;
    mud->inventory_items_count++;
}

static int mudclient_offline_text_contains(const char *text,
                                           const char *needle) {
    if (text == NULL || needle == NULL) {
        return 0;
    }

    for (int i = 0; text[i] != '\0'; i++) {
        int j = 0;

        while (needle[j] != '\0' &&
               tolower((unsigned char)text[i + j]) ==
                   tolower((unsigned char)needle[j])) {
            j++;
        }

        if (needle[j] == '\0') {
            return 1;
        }
    }

    return 0;
}

static int mudclient_find_offline_item(const char *name) {
    for (int i = 0; i < game_data.item_count; i++) {
        char *item_name = game_data.items[i].name;

        if (item_name != NULL && strcasecmp(item_name, name) == 0) {
            return i;
        }
    }

    return -1;
}

static int mudclient_find_offline_item_containing(const char *name) {
    for (int i = 0; i < game_data.item_count; i++) {
        char *item_name = game_data.items[i].name;

        if (mudclient_offline_text_contains(item_name, name)) {
            return i;
        }
    }

    return -1;
}

static int mudclient_offline_has_item_name(mudclient *mud, const char *name,
                                           int amount) {
    int id = mudclient_find_offline_item(name);

    if (id < 0) {
        id = mudclient_find_offline_item_containing(name);
    }

    return id >= 0 && mudclient_get_inventory_count(mud, id) >= amount;
}

static int mudclient_offline_remove_item_name(mudclient *mud,
                                              const char *name, int amount) {
    int id = mudclient_find_offline_item(name);

    if (id < 0) {
        id = mudclient_find_offline_item_containing(name);
    }

    if (id < 0) {
        return 0;
    }

    return mudclient_offline_remove_inventory_item(mud, id, amount) == amount;
}

static void mudclient_ensure_offline_inventory_item(mudclient *mud,
                                                   const char *name,
                                                   int count, int equipped) {
    int id = mudclient_find_offline_item(name);
    int slot;

    if (id < 0 || mudclient_has_inventory_item(mud, id, 1)) {
        return;
    }

    mudclient_add_offline_inventory_item(mud, id, count, equipped);

    if (!equipped) {
        return;
    }

    slot = mudclient_offline_find_inventory_slot(mud, id);
    if (slot >= 0) {
        mud->inventory_equipped[slot] = 1;
    }
}

static void mudclient_compact_offline_inventory(mudclient *mud, int slot) {
    if (slot < 0 || slot >= mud->inventory_items_count) {
        return;
    }

    mud->inventory_items_count--;
    for (int i = slot; i < mud->inventory_items_count; i++) {
        mud->inventory_item_id[i] = mud->inventory_item_id[i + 1];
        mud->inventory_item_stack_count[i] =
            mud->inventory_item_stack_count[i + 1];
        mud->inventory_equipped[i] = mud->inventory_equipped[i + 1];
    }

    mud->inventory_item_id[mud->inventory_items_count] = 0;
    mud->inventory_item_stack_count[mud->inventory_items_count] = 0;
    mud->inventory_equipped[mud->inventory_items_count] = 0;
}

static int mudclient_offline_find_inventory_slot(mudclient *mud, int id) {
    for (int i = 0; i < mud->inventory_items_count; i++) {
        if (mud->inventory_item_id[i] == id) {
            return i;
        }
    }

    return -1;
}

int mudclient_offline_add_inventory_item(mudclient *mud, int id, int amount) {
    int added = 0;

    if (mud == NULL || id < 0 || id >= game_data.item_count || amount <= 0) {
        return 0;
    }

    if (game_data.items[id].stackable == 0) {
        int slot = mudclient_offline_find_inventory_slot(mud, id);

        if (slot < 0) {
            if (mud->inventory_items_count >= INVENTORY_ITEMS_MAX) {
                return 0;
            }
            slot = mud->inventory_items_count++;
            mud->inventory_item_id[slot] = id;
            mud->inventory_item_stack_count[slot] = 0;
            mud->inventory_equipped[slot] = 0;
        }

        int room = INT_MAX - mud->inventory_item_stack_count[slot];
        if (amount > room) {
            amount = room;
        }
        mud->inventory_item_stack_count[slot] += amount;
        return amount;
    }

    while (added < amount && mud->inventory_items_count < INVENTORY_ITEMS_MAX) {
        int slot = mud->inventory_items_count++;
        mud->inventory_item_id[slot] = id;
        mud->inventory_item_stack_count[slot] = 1;
        mud->inventory_equipped[slot] = 0;
        added++;
    }

    return added;
}

int mudclient_offline_remove_inventory_item(mudclient *mud, int id,
                                            int amount) {
    int removed = 0;

    if (mud == NULL || id < 0 || id >= game_data.item_count || amount <= 0) {
        return 0;
    }

    if (game_data.items[id].stackable == 0) {
        int slot = mudclient_offline_find_inventory_slot(mud, id);

        if (slot < 0) {
            return 0;
        }

        removed = mud->inventory_item_stack_count[slot];
        if (removed > amount) {
            removed = amount;
        }

        mud->inventory_item_stack_count[slot] -= removed;
        if (mud->inventory_item_stack_count[slot] <= 0) {
            mudclient_compact_offline_inventory(mud, slot);
        }
        return removed;
    }

    for (int i = 0; i < mud->inventory_items_count && removed < amount;) {
        if (mud->inventory_item_id[i] == id) {
            mudclient_compact_offline_inventory(mud, i);
            removed++;
        } else {
            i++;
        }
    }

    return removed;
}

static void mudclient_offline_add_ground_item(mudclient *mud, int id, int x,
                                              int y) {
    if (mud == NULL || id < 0 || id >= game_data.item_count ||
        mud->ground_item_count >= GROUND_ITEMS_MAX) {
        return;
    }

    mud->ground_items[mud->ground_item_count].x = x;
    mud->ground_items[mud->ground_item_count].y = y;
    mud->ground_items[mud->ground_item_count].id = id;
    mud->ground_items[mud->ground_item_count].z = 0;

    for (int i = 0; i < mud->object_count; i++) {
        if (mud->objects[i].x == x && mud->objects[i].y == y) {
            mud->ground_items[mud->ground_item_count].z =
                game_data.objects[mud->objects[i].id].elevation;
            break;
        }
    }

    mud->ground_item_count++;
    if (!offline_ground_item_defer_updates) {
        mudclient_update_ground_item_models(mud);
    }
}

static void mudclient_offline_add_ground_item_id_once(mudclient *mud, int id,
                                                      int x, int y) {
    if (mud == NULL || id < 0 || id >= game_data.item_count) {
        return;
    }

    x -= mud->region_x;
    y -= mud->region_y;
    if (x < 0 || x >= REGION_WIDTH || y < 0 || y >= REGION_HEIGHT) {
        return;
    }

    for (int i = 0; i < mud->ground_item_count; i++) {
        if (mud->ground_items[i].x == x && mud->ground_items[i].y == y &&
            mud->ground_items[i].id == id) {
            return;
        }
    }

    mudclient_offline_add_ground_item(mud, id, x, y);
}

static void mudclient_seed_offline_quest_ground_items(mudclient *mud) {
    const struct OfflineItemSpawn *spawns = offline_loaded_item_spawns;
    int count;

    if (mud == NULL) {
        return;
    }

    mudclient_load_offline_spawn_data();
    count = offline_loaded_item_spawn_count;
    if (count <= 0) {
        spawns = offline_fallback_item_spawns;
        count = sizeof(offline_fallback_item_spawns) /
                sizeof(offline_fallback_item_spawns[0]);

        if (!offline_spawn_data_warned) {
            mudclient_show_message(
                mud,
                "@cya@Missing offline item spawn data; using fallback.",
                MESSAGE_TYPE_GAME);
            offline_spawn_data_warned = 1;
        }
    }

    mud->ground_item_count = 0;
    offline_ground_item_defer_updates = 1;
    for (int i = 0; i < count; i++) {
        mudclient_offline_add_ground_item_id_once(mud, spawns[i].id,
                                                  spawns[i].global_x,
                                                  spawns[i].global_y);
    }
    offline_ground_item_defer_updates = 0;
    mudclient_update_ground_item_models(mud);
}

static void mudclient_offline_remove_ground_item(mudclient *mud, int slot) {
    if (mud == NULL || slot < 0 || slot >= mud->ground_item_count) {
        return;
    }

    mud->ground_item_count--;
    for (int i = slot; i < mud->ground_item_count; i++) {
        mud->ground_items[i].x = mud->ground_items[i + 1].x;
        mud->ground_items[i].y = mud->ground_items[i + 1].y;
        mud->ground_items[i].id = mud->ground_items[i + 1].id;
        mud->ground_items[i].z = mud->ground_items[i + 1].z;
        mud->ground_items[i].already_in_menu =
            mud->ground_items[i + 1].already_in_menu;
    }
    mudclient_update_ground_item_models(mud);
}

int mudclient_offline_take_ground_item(mudclient *mud, int x, int y,
                                       int item_id) {
    if (mud == NULL) {
        return 0;
    }

    for (int i = 0; i < mud->ground_item_count; i++) {
        if (mud->ground_items[i].x != x || mud->ground_items[i].y != y ||
            mud->ground_items[i].id != item_id) {
            continue;
        }

        if (mudclient_offline_add_inventory_item(mud, item_id, 1) <= 0) {
            mudclient_show_message(mud, "@cya@Your inventory is full.",
                                   MESSAGE_TYPE_GAME);
            return 0;
        }

        mudclient_offline_remove_ground_item(mud, i);
        mudclient_show_message(mud, game_data.items[item_id].name,
                               MESSAGE_TYPE_GAME);
        return 1;
    }

    return 0;
}

void mudclient_offline_drop_inventory_slot(mudclient *mud, int slot) {
    if (mud == NULL || mud->local_player == NULL || slot < 0 ||
        slot >= mud->inventory_items_count) {
        return;
    }

    int id = mud->inventory_item_id[slot];
    int x = mud->local_player->current_x / MAGIC_LOC;
    int y = mud->local_player->current_y / MAGIC_LOC;

    if (mudclient_offline_remove_inventory_item(mud, id, 1) <= 0) {
        return;
    }

    mudclient_offline_add_ground_item(mud, id, x, y);
    mudclient_show_message(mud, "@cya@You drop the item.", MESSAGE_TYPE_GAME);
}

static int mudclient_offline_experience_to_level(int experience) {
    int total_exp = 0;

    for (int i = 0; i < 99; i++) {
        int level = i + 1;
        int exp = level + 300 * pow(2, (float)level / 7);
        total_exp += exp;

        if (experience < (total_exp & 0xffffffc)) {
            return level;
        }
    }

    return 99;
}

static const char *mudclient_offline_skill_name(int skill) {
    static const char *names[PLAYER_SKILL_COUNT] = {
        "Attack",   "Defense",  "Strength",    "Hits",      "Ranged",
        "Prayer",   "Magic",    "Cooking",     "Woodcutting","Fletching",
        "Fishing",  "Firemaking","Crafting",   "Smithing",  "Mining",
        "Herblaw",  "Agility",  "Thieving"};

    if (skill < 0 || skill >= PLAYER_SKILL_COUNT) {
        return "skill";
    }

    return names[skill];
}

static void mudclient_update_offline_combat_level(mudclient *mud) {
    GameCharacter *player = mud->local_player;

    if (player == NULL) {
        return;
    }

    player->level = (mud->player_skill_base[SKILL_ATTACK] +
                     mud->player_skill_base[SKILL_DEFENSE] +
                     mud->player_skill_base[SKILL_STRENGTH] +
                     mud->player_skill_base[SKILL_HITS] + 27) /
                    4;
    player->current_hits = mud->player_skill_current[SKILL_HITS];
    player->max_hits = mud->player_skill_base[SKILL_HITS];
}

static void mudclient_award_offline_xp(mudclient *mud, int skill, int xp) {
    int old_level;
    int new_level;
    int was_full;
    char message[96];

    if (mud == NULL || skill < 0 || skill >= PLAYER_SKILL_COUNT || xp <= 0) {
        return;
    }

    old_level = mud->player_skill_base[skill];
    was_full = mud->player_skill_current[skill] >= old_level;

    if (mud->player_experience[skill] < 800000000 - xp) {
        mud->player_experience[skill] += xp;
    } else {
        mud->player_experience[skill] = 800000000;
    }

    new_level = mudclient_offline_experience_to_level(
        mud->player_experience[skill]);
    if (new_level > mud->player_skill_base[skill]) {
        mud->player_skill_base[skill] = new_level;
        if (was_full || skill == SKILL_HITS) {
            mud->player_skill_current[skill] = new_level;
        }

        snprintf(message, sizeof(message), "@gre@Your %s level has increased!",
                 mudclient_offline_skill_name(skill));
        mudclient_show_message(mud, message, MESSAGE_TYPE_GAME);
    }

    if (skill == SKILL_ATTACK || skill == SKILL_DEFENSE ||
        skill == SKILL_STRENGTH || skill == SKILL_HITS) {
        mudclient_update_offline_combat_level(mud);
    }

    rsc_haptic_skill();
    mudclient_drop_experience(mud, skill, xp);
}

static int mudclient_offline_bone_xp(int item_id) {
    switch (item_id) {
    case RSC_ITEM_BIG_BONES:
        return 60;
    case RSC_ITEM_BAT_BONES:
        return 22;
    case RSC_ITEM_DRAGON_BONES:
        return 288;
    case RSC_ITEM_BONES:
    default:
        return 18;
    }
}

static int mudclient_offline_inventory_has_named_tool(mudclient *mud,
                                                      const char *needle) {
    for (int i = 0; i < mud->inventory_items_count; i++) {
        int id = mud->inventory_item_id[i];

        if (id >= 0 && id < game_data.item_count &&
            mudclient_offline_text_contains(game_data.items[id].name,
                                            needle)) {
            return 1;
        }
    }

    return 0;
}

static int mudclient_offline_object_text_contains(int object_id,
                                                  const char *needle) {
    if (object_id < 0 || object_id >= game_data.object_count) {
        return 0;
    }

    return mudclient_offline_text_contains(game_data.objects[object_id].name,
                                           needle) ||
           mudclient_offline_text_contains(game_data.objects[object_id].command1,
                                           needle) ||
           mudclient_offline_text_contains(game_data.objects[object_id].command2,
                                           needle);
}

static int mudclient_find_offline_object_text(const char *needle) {
    for (int i = 0; i < game_data.object_count; i++) {
        if (mudclient_offline_object_text_contains(i, needle)) {
            return i;
        }
    }

    return -1;
}

static int mudclient_offline_add_named_item(mudclient *mud, const char *name,
                                            int amount) {
    int id = mudclient_find_offline_item(name);

    if (id < 0) {
        id = mudclient_find_offline_item_containing(name);
    }
    if (id < 0) {
        return 0;
    }

    return mudclient_offline_add_inventory_item(mud, id, amount);
}

static int mudclient_offline_consume_first_named(mudclient *mud,
                                                 const char **names,
                                                 int *item_id) {
    for (int i = 0; names[i] != NULL; i++) {
        int id = mudclient_find_offline_item(names[i]);

        if (id < 0) {
            id = mudclient_find_offline_item_containing(names[i]);
        }
        if (id >= 0 && mudclient_get_inventory_count(mud, id) > 0 &&
            mudclient_offline_remove_inventory_item(mud, id, 1) > 0) {
            if (item_id != NULL) {
                *item_id = id;
            }
            return 1;
        }
    }

    return 0;
}

static int mudclient_offline_log_item_for_object(int object_id,
                                                 const char **item_name,
                                                 int *xp) {
    char *name;

    if (object_id < 0 || object_id >= game_data.object_count) {
        return 0;
    }

    name = game_data.objects[object_id].name;
    if (!mudclient_offline_object_text_contains(object_id, "tree")) {
        return 0;
    }

    if (mudclient_offline_text_contains(name, "magic")) {
        *item_name = "Magic Logs";
        *xp = 1000;
    } else if (mudclient_offline_text_contains(name, "yew")) {
        *item_name = "Yew Logs";
        *xp = 700;
    } else if (mudclient_offline_text_contains(name, "maple")) {
        *item_name = "Maple Logs";
        *xp = 400;
    } else if (mudclient_offline_text_contains(name, "willow")) {
        *item_name = "Willow Logs";
        *xp = 270;
    } else if (mudclient_offline_text_contains(name, "oak")) {
        *item_name = "Oak Logs";
        *xp = 150;
    } else {
        *item_name = "Logs";
        *xp = 100;
    }

    return 1;
}

static int mudclient_offline_ore_for_object(int object_id,
                                            const char **item_name, int *xp) {
    if (object_id < 0 || object_id >= game_data.object_count ||
        !mudclient_offline_object_text_contains(object_id, "rock")) {
        return 0;
    }

    switch (object_id % 5) {
    case 0:
        *item_name = "iron ore";
        *xp = 140;
        break;
    case 1:
        *item_name = "tin ore";
        *xp = 70;
        break;
    default:
        *item_name = "copper ore";
        *xp = 70;
        break;
    }

    return 1;
}

static int mudclient_offline_cook_first_raw_food(mudclient *mud) {
    static const struct {
        const char *raw;
        const char *cooked;
        int xp;
    } foods[] = {
        {"Raw Shrimp", "Shrimp", 120},
        {"Raw Anchovies", "Anchovies", 120},
        {"Raw Sardine", "Sardine", 160},
        {"Raw Trout", "Trout", 200},
        {"Raw Salmon", "Salmon", 280},
        {"Raw Lobster", "Lobster", 480},
        {"Raw Swordfish", "Swordfish", 560},
        {"raw chicken", "cookedmeat", 120},
        {"raw beef", "cookedmeat", 120},
        {"raw bear meat", "cookedmeat", 120},
        {"raw rat meat", "cookedmeat", 120},
        {NULL, NULL, 0},
    };

    for (int i = 0; foods[i].raw != NULL; i++) {
        int raw_id = mudclient_find_offline_item(foods[i].raw);
        int cooked_id = mudclient_find_offline_item(foods[i].cooked);

        if (raw_id < 0 || cooked_id < 0 ||
            mudclient_get_inventory_count(mud, raw_id) <= 0) {
            continue;
        }

        if (mudclient_offline_remove_inventory_item(mud, raw_id, 1) <= 0) {
            return 1;
        }
        if (mudclient_offline_add_inventory_item(mud, cooked_id, 1) <= 0) {
            mudclient_offline_add_inventory_item(mud, raw_id, 1);
            mudclient_show_message(mud, "@cya@Your inventory is full.",
                                   MESSAGE_TYPE_GAME);
            return 1;
        }

        mudclient_award_offline_xp(mud, SKILL_COOKING, foods[i].xp);
        mudclient_show_message(mud, "@cya@You successfully cook the food.",
                               MESSAGE_TYPE_GAME);
        return 1;
    }

    mudclient_show_message(mud, "@cya@You have nothing raw to cook.",
                           MESSAGE_TYPE_GAME);
    return 1;
}

static int mudclient_offline_smelt_bar(mudclient *mud) {
    int copper = mudclient_find_offline_item("copper ore");
    int tin = mudclient_find_offline_item("tin ore");
    int iron = mudclient_find_offline_item("iron ore");
    int bronze_bar = mudclient_find_offline_item("bronze bar");
    int iron_bar = mudclient_find_offline_item("iron bar");

    if (copper >= 0 && tin >= 0 && bronze_bar >= 0 &&
        mudclient_get_inventory_count(mud, copper) > 0 &&
        mudclient_get_inventory_count(mud, tin) > 0) {
        mudclient_offline_remove_inventory_item(mud, copper, 1);
        mudclient_offline_remove_inventory_item(mud, tin, 1);
        mudclient_offline_add_inventory_item(mud, bronze_bar, 1);
        mudclient_award_offline_xp(mud, SKILL_SMITHING, 25);
        mudclient_show_message(mud, "@cya@You smelt a bronze bar.",
                               MESSAGE_TYPE_GAME);
        return 1;
    }

    if (iron >= 0 && iron_bar >= 0 && mudclient_get_inventory_count(mud, iron) > 0) {
        mudclient_offline_remove_inventory_item(mud, iron, 1);
        mudclient_offline_add_inventory_item(mud, iron_bar, 1);
        mudclient_award_offline_xp(mud, SKILL_SMITHING, 50);
        mudclient_show_message(mud, "@cya@You smelt an iron bar.",
                               MESSAGE_TYPE_GAME);
        return 1;
    }

    mudclient_show_message(mud, "@cya@You need ore to smelt.",
                           MESSAGE_TYPE_GAME);
    return 1;
}

static int mudclient_offline_smith_item(mudclient *mud) {
    int bronze_bar = mudclient_find_offline_item("bronze bar");
    int iron_bar = mudclient_find_offline_item("iron bar");
    int bronze_sword = mudclient_find_offline_item("Bronze Short Sword");
    int iron_mace = IRON_MACE_ID;

    if (bronze_bar >= 0 && bronze_sword >= 0 &&
        mudclient_get_inventory_count(mud, bronze_bar) > 0) {
        mudclient_offline_remove_inventory_item(mud, bronze_bar, 1);
        mudclient_offline_add_inventory_item(mud, bronze_sword, 1);
        mudclient_award_offline_xp(mud, SKILL_SMITHING, 50);
        mudclient_show_message(mud, "@cya@You smith a bronze sword.",
                               MESSAGE_TYPE_GAME);
        return 1;
    }

    if (iron_bar >= 0 && mudclient_get_inventory_count(mud, iron_bar) > 0) {
        mudclient_offline_remove_inventory_item(mud, iron_bar, 1);
        mudclient_offline_add_inventory_item(mud, iron_mace, 1);
        mudclient_award_offline_xp(mud, SKILL_SMITHING, 100);
        mudclient_show_message(mud, "@cya@You smith an iron mace.",
                               MESSAGE_TYPE_GAME);
        return 1;
    }

    mudclient_show_message(mud, "@cya@You need a bar to smith.",
                           MESSAGE_TYPE_GAME);
    return 1;
}

static int mudclient_offline_fish(mudclient *mud) {
    if (mudclient_offline_inventory_has_named_tool(mud, "net")) {
        if (mudclient_offline_add_named_item(mud, "Raw Shrimp", 1) <= 0) {
            rsc_haptic_error();
            mudclient_show_message(mud, "@cya@Your inventory is full.",
                                   MESSAGE_TYPE_GAME);
            return 1;
        }
        mudclient_award_offline_xp(mud, SKILL_FISHING, 40);
        mudclient_show_message(mud, "@cya@You catch some shrimp.",
                               MESSAGE_TYPE_GAME);
        return 1;
    }

    if (mudclient_offline_inventory_has_named_tool(mud, "fishing rod") &&
        mudclient_offline_has_item_name(mud, "Feather", 1)) {
        mudclient_offline_remove_item_name(mud, "Feather", 1);
        if (mudclient_offline_add_named_item(mud, "Raw Trout", 1) <= 0) {
            mudclient_offline_add_named_item(mud, "Feather", 1);
            rsc_haptic_error();
            mudclient_show_message(mud, "@cya@Your inventory is full.",
                                   MESSAGE_TYPE_GAME);
            return 1;
        }
        mudclient_award_offline_xp(mud, SKILL_FISHING, 200);
        mudclient_show_message(mud, "@cya@You catch a fish.",
                               MESSAGE_TYPE_GAME);
        return 1;
    }

    rsc_haptic_error();
    mudclient_show_message(mud, "@cya@You need a net or rod to fish.",
                           MESSAGE_TYPE_GAME);
    return 1;
}

static int mudclient_offline_eat_inventory_item(mudclient *mud, int slot) {
    int item_id;
    int heal;

    if (mud == NULL || slot < 0 || slot >= mud->inventory_items_count ||
        mud->local_player == NULL) {
        return 0;
    }

    item_id = mud->inventory_item_id[slot];
    if (item_id < 0 || item_id >= game_data.item_count ||
        !mudclient_offline_text_contains(game_data.items[item_id].command,
                                         "eat")) {
        return 0;
    }

    heal = 2;
    if (mudclient_offline_text_contains(game_data.items[item_id].name,
                                        "lobster")) {
        heal = 12;
    } else if (mudclient_offline_text_contains(game_data.items[item_id].name,
                                               "swordfish")) {
        heal = 14;
    } else if (mudclient_offline_text_contains(game_data.items[item_id].name,
                                               "trout")) {
        heal = 7;
    } else if (mudclient_offline_text_contains(game_data.items[item_id].name,
                                               "salmon")) {
        heal = 9;
    } else if (mudclient_offline_text_contains(game_data.items[item_id].name,
                                               "shrimp")) {
        heal = 3;
    } else if (mudclient_offline_text_contains(game_data.items[item_id].name,
                                               "meat")) {
        heal = 3;
    }

    if (mudclient_offline_remove_inventory_item(mud, item_id, 1) <= 0) {
        return 1;
    }

    mud->player_skill_current[SKILL_HITS] += heal;
    if (mud->player_skill_current[SKILL_HITS] >
        mud->player_skill_base[SKILL_HITS]) {
        mud->player_skill_current[SKILL_HITS] =
            mud->player_skill_base[SKILL_HITS];
    }
    mud->local_player->current_hits = mud->player_skill_current[SKILL_HITS];
    mudclient_show_message(mud, "@cya@You eat the food.", MESSAGE_TYPE_GAME);
    return 1;
}

void mudclient_offline_bury_inventory_item(mudclient *mud, int slot) {
    if (mud == NULL || slot < 0 || slot >= mud->inventory_items_count) {
        return;
    }

    int item_id = mud->inventory_item_id[slot];
    if (item_id < 0 || item_id >= game_data.item_count ||
        strcmp(game_data.items[item_id].command, "Bury") != 0) {
        return;
    }

    int xp = mudclient_offline_bone_xp(item_id);

    if (mudclient_offline_remove_inventory_item(mud, item_id, 1) <= 0) {
        return;
    }

    mudclient_award_offline_xp(mud, SKILL_PRAYER, xp);
    mudclient_show_message(mud, "@cya@You bury the bones.",
                           MESSAGE_TYPE_GAME);
}

int mudclient_offline_inventory_command(mudclient *mud, int slot) {
    if (mudclient_offline_eat_inventory_item(mud, slot)) {
        return 1;
    }

    mudclient_offline_bury_inventory_item(mud, slot);
    return 1;
}

int mudclient_offline_use_inventory_items(mudclient *mud, int source_slot,
                                          int target_slot) {
    int source_id;
    int target_id;
    int logs_id;
    int tinderbox_id;

    if (mud == NULL || mud->local_player == NULL || source_slot < 0 ||
        source_slot >= mud->inventory_items_count || target_slot < 0 ||
        target_slot >= mud->inventory_items_count) {
        return 0;
    }

    source_id = mud->inventory_item_id[source_slot];
    target_id = mud->inventory_item_id[target_slot];
    tinderbox_id = mudclient_find_offline_item("tinderbox");

    if (source_id < 0 || source_id >= game_data.item_count || target_id < 0 ||
        target_id >= game_data.item_count || tinderbox_id < 0) {
        return 0;
    }

    logs_id = mudclient_offline_text_contains(game_data.items[source_id].name,
                                              "logs")
                  ? source_id
                  : target_id;

    if ((source_id == tinderbox_id || target_id == tinderbox_id) &&
        mudclient_offline_text_contains(game_data.items[logs_id].name,
                                        "logs")) {
        if (mudclient_offline_remove_inventory_item(mud, logs_id, 1) <= 0) {
            return 1;
        }

        mudclient_award_offline_xp(mud, SKILL_FIREMAKING, 160);
        mudclient_show_message(mud, "@cya@The logs catch fire.",
                               MESSAGE_TYPE_GAME);
        return 1;
    }

    return 0;
}

int mudclient_offline_handle_object_command(mudclient *mud, int object_id) {
    const char *item_name = NULL;
    int xp = 0;

    if (mud == NULL || object_id < 0 || object_id >= game_data.object_count) {
        return 0;
    }

    if (mudclient_offline_log_item_for_object(object_id, &item_name, &xp)) {
        if (!mudclient_offline_inventory_has_named_tool(mud, "axe")) {
            rsc_haptic_error();
            mudclient_show_message(mud, "@cya@You need an axe to chop this.",
                                   MESSAGE_TYPE_GAME);
            return 1;
        }
        if (mudclient_offline_add_named_item(mud, item_name, 1) <= 0) {
            rsc_haptic_error();
            mudclient_show_message(mud, "@cya@Your inventory is full.",
                                   MESSAGE_TYPE_GAME);
            return 1;
        }
        mudclient_award_offline_xp(mud, SKILL_WOODCUT, xp);
        mudclient_show_message(mud, "@cya@You get some logs.",
                               MESSAGE_TYPE_GAME);
        return 1;
    }

    if (mudclient_offline_ore_for_object(object_id, &item_name, &xp)) {
        if (!mudclient_offline_inventory_has_named_tool(mud, "pickaxe")) {
            rsc_haptic_error();
            mudclient_show_message(mud,
                                   "@cya@You need a pickaxe to mine this.",
                                   MESSAGE_TYPE_GAME);
            return 1;
        }
        if (mudclient_offline_add_named_item(mud, item_name, 1) <= 0) {
            rsc_haptic_error();
            mudclient_show_message(mud, "@cya@Your inventory is full.",
                                   MESSAGE_TYPE_GAME);
            return 1;
        }
        mudclient_award_offline_xp(mud, SKILL_MINING, xp);
        mudclient_show_message(mud, "@cya@You mine some ore.",
                               MESSAGE_TYPE_GAME);
        return 1;
    }

    if (mudclient_offline_object_text_contains(object_id, "range") ||
        mudclient_offline_object_text_contains(object_id, "fire")) {
        return mudclient_offline_cook_first_raw_food(mud);
    }

    if (mudclient_offline_object_text_contains(object_id, "furnace")) {
        return mudclient_offline_smelt_bar(mud);
    }

    if (mudclient_offline_object_text_contains(object_id, "anvil")) {
        return mudclient_offline_smith_item(mud);
    }

    if (mudclient_offline_object_text_contains(object_id, "water") ||
        mudclient_offline_object_text_contains(object_id, "fish") ||
        mudclient_offline_object_text_contains(object_id, "net") ||
        mudclient_offline_object_text_contains(object_id, "lure") ||
        mudclient_offline_object_text_contains(object_id, "bait")) {
        return mudclient_offline_fish(mud);
    }

    if (mudclient_offline_object_text_contains(object_id, "altar")) {
        mud->player_skill_current[SKILL_PRAYER] =
            mud->player_skill_base[SKILL_PRAYER];
        rsc_haptic_action();
        mudclient_show_message(mud, "@cya@You recharge your Prayer points.",
                               MESSAGE_TYPE_GAME);
        return 1;
    }

    if (mudclient_offline_object_text_contains(object_id, "wheat")) {
        mudclient_offline_add_named_item(mud, "Flour", 1);
        rsc_haptic_action();
        mudclient_show_message(mud, "@cya@You pick some wheat.",
                               MESSAGE_TYPE_GAME);
        return 1;
    }

    if (mudclient_offline_object_text_contains(object_id, "potato")) {
        mudclient_offline_add_named_item(mud, "Potato", 1);
        rsc_haptic_action();
        mudclient_show_message(mud, "@cya@You pick a potato.",
                               MESSAGE_TYPE_GAME);
        return 1;
    }

    return 0;
}

void mudclient_offline_pickpocket(mudclient *mud, GameCharacter *npc) {
    int xp = 32;
    int coins = 3 + (rand() % 5);

    if (mud == NULL || npc == NULL || npc->npc_id < 0 ||
        npc->npc_id >= game_data.npc_count ||
        !mudclient_offline_text_contains(game_data.npcs[npc->npc_id].command,
                                         "pickpocket")) {
        return;
    }

    if (mudclient_offline_add_inventory_item(mud, COINS_ID, coins) <= 0) {
        mudclient_show_message(mud, "@cya@Your inventory is full.",
                               MESSAGE_TYPE_GAME);
        return;
    }

    mudclient_award_offline_xp(mud, SKILL_THIEVING, xp);
    mudclient_show_message(mud, "@cya@You pick the NPC's pocket.",
                           MESSAGE_TYPE_GAME);
}

static int mudclient_offline_is_quest_complete(mudclient *mud, int quest) {
    return mud != NULL && mud->quest_complete != NULL && quest >= 0 &&
           quest < quests_length && mud->quest_complete[quest] != 0;
}

static void mudclient_complete_offline_quest(mudclient *mud, int quest,
                                             const char *name) {
    char message[96];

    if (mud == NULL || mud->quest_complete == NULL || quest < 0 ||
        quest >= quests_length || mud->quest_complete[quest]) {
        return;
    }

    mud->quest_complete[quest] = 1;
    mud->player_quest_points++;
    snprintf(message, sizeof(message), "@gre@Quest complete: %s", name);
    mudclient_show_message(mud, message, MESSAGE_TYPE_GAME);
}

static int mudclient_offline_handle_cook_quest(mudclient *mud) {
    int has_milk;
    int has_flour;

    if (mudclient_offline_is_quest_complete(mud, RSC_QUEST_COOKS_ASSISTANT)) {
        mudclient_show_message(mud, "@cya@The cook thanks you again.",
                               MESSAGE_TYPE_GAME);
        return 1;
    }

    has_milk = mudclient_offline_has_item_name(mud, "bucket of milk", 1) ||
               mudclient_offline_has_item_name(mud, "milk", 1);
    has_flour = mudclient_offline_has_item_name(mud, "pot of flour", 1) ||
                mudclient_offline_has_item_name(mud, "flour", 1);

    if (!mudclient_offline_has_item_name(mud, "egg", 1) || !has_milk ||
        !has_flour) {
        mudclient_show_message(
            mud, "@cya@The cook needs an egg, milk, and flour.",
            MESSAGE_TYPE_GAME);
        return 1;
    }

    mudclient_offline_remove_item_name(mud, "egg", 1);
    if (!mudclient_offline_remove_item_name(mud, "bucket of milk", 1)) {
        mudclient_offline_remove_item_name(mud, "milk", 1);
    }
    if (!mudclient_offline_remove_item_name(mud, "pot of flour", 1)) {
        mudclient_offline_remove_item_name(mud, "flour", 1);
    }

    mudclient_complete_offline_quest(mud, RSC_QUEST_COOKS_ASSISTANT,
                                     "Cook's assistant");
    mudclient_award_offline_xp(mud, SKILL_COOKING, 1200);
    return 1;
}

static int mudclient_offline_handle_sheep_quest(mudclient *mud) {
    if (mudclient_offline_is_quest_complete(mud, RSC_QUEST_SHEEP_SHEARER)) {
        mudclient_show_message(mud, "@cya@Fred has all the wool he needs.",
                               MESSAGE_TYPE_GAME);
        return 1;
    }

    if (!mudclient_offline_has_item_name(mud, "ball of wool", 20)) {
        mudclient_show_message(mud,
                               "@cya@Fred needs 20 balls of wool.",
                               MESSAGE_TYPE_GAME);
        return 1;
    }

    mudclient_offline_remove_item_name(mud, "ball of wool", 20);
    mudclient_complete_offline_quest(mud, RSC_QUEST_SHEEP_SHEARER,
                                     "Sheep shearer");
    mudclient_award_offline_xp(mud, SKILL_CRAFTING, 600);
    return 1;
}

static int mudclient_offline_handle_father_aereck(mudclient *mud) {
    if (mudclient_offline_is_quest_complete(mud, RSC_QUEST_RESTLESS_GHOST)) {
        mudclient_show_message(mud, "@cya@Father Aereck blesses you.",
                               MESSAGE_TYPE_GAME);
        return 1;
    }

    mudclient_ensure_offline_inventory_item(mud, "Ghostspeak amulet", 1, 0);
    mudclient_show_message(
        mud, "@cya@Father Aereck asks you to help the restless ghost.",
        MESSAGE_TYPE_GAME);
    return 1;
}

static int mudclient_offline_handle_ghost_quest(mudclient *mud) {
    if (mudclient_offline_is_quest_complete(mud, RSC_QUEST_RESTLESS_GHOST)) {
        mudclient_show_message(mud, "@cya@The ghost is at peace.",
                               MESSAGE_TYPE_GAME);
        return 1;
    }

    if (!mudclient_offline_has_item_name(mud, "ghost", 1) ||
        !mudclient_offline_has_item_name(mud, "skull", 1)) {
        mudclient_show_message(mud,
                               "@cya@The ghost needs its skull returned.",
                               MESSAGE_TYPE_GAME);
        return 1;
    }

    mudclient_offline_remove_item_name(mud, "skull", 1);
    mudclient_complete_offline_quest(mud, RSC_QUEST_RESTLESS_GHOST,
                                     "The restless ghost");
    mudclient_award_offline_xp(mud, SKILL_PRAYER, 500);
    return 1;
}

int mudclient_offline_handle_npc_talk(mudclient *mud, GameCharacter *npc) {
    char *name;

    if (mud == NULL || npc == NULL || npc->npc_id < 0 ||
        npc->npc_id >= game_data.npc_count) {
        return 0;
    }

    name = game_data.npcs[npc->npc_id].name;
    if (mudclient_offline_text_contains(name, "cook")) {
        return mudclient_offline_handle_cook_quest(mud);
    }
    if (mudclient_offline_text_contains(name, "fred")) {
        return mudclient_offline_handle_sheep_quest(mud);
    }
    if (mudclient_offline_text_contains(name, "aereck")) {
        return mudclient_offline_handle_father_aereck(mud);
    }
    if (mudclient_offline_text_contains(name, "ghost")) {
        return mudclient_offline_handle_ghost_quest(mud);
    }

    return 0;
}

int mudclient_offline_handle_npc_command(mudclient *mud, GameCharacter *npc) {
    char *name;

    if (mud == NULL || npc == NULL || npc->npc_id < 0 ||
        npc->npc_id >= game_data.npc_count) {
        return 0;
    }

    name = game_data.npcs[npc->npc_id].name;
    if (mudclient_offline_text_contains(name, "fred") ||
        mudclient_offline_text_contains(name, "ghost")) {
        return mudclient_offline_handle_npc_talk(mud, npc);
    }

    return 0;
}

static int mudclient_offline_bank_find_item(mudclient *mud, int item_id) {
    for (int i = 0; i < mud->new_bank_item_count; i++) {
        if (mud->new_bank_items[i] == item_id) {
            return i;
        }
    }

    return -1;
}

static int mudclient_offline_bank_add_item(mudclient *mud, int item_id,
                                           int amount) {
    int slot;

    if (mud == NULL || item_id < 0 || item_id >= game_data.item_count ||
        amount <= 0) {
        return 0;
    }

    slot = mudclient_offline_bank_find_item(mud, item_id);
    if (slot < 0) {
        if (mud->new_bank_item_count >= mud->bank_items_max ||
            mud->new_bank_item_count >= BANK_ITEMS_MAX) {
            return 0;
        }
        slot = mud->new_bank_item_count++;
        mud->new_bank_items[slot] = item_id;
        mud->new_bank_items_count[slot] = 0;
    }

    if (amount > INT_MAX - mud->new_bank_items_count[slot]) {
        amount = INT_MAX - mud->new_bank_items_count[slot];
    }

    mud->new_bank_items_count[slot] += amount;
    return amount;
}

static int mudclient_offline_bank_remove_item(mudclient *mud, int item_id,
                                              int amount) {
    int slot;
    int removed;

    if (mud == NULL || amount <= 0) {
        return 0;
    }

    slot = mudclient_offline_bank_find_item(mud, item_id);
    if (slot < 0) {
        return 0;
    }

    removed = mud->new_bank_items_count[slot];
    if (removed > amount) {
        removed = amount;
    }

    mud->new_bank_items_count[slot] -= removed;
    if (mud->new_bank_items_count[slot] <= 0) {
        mud->new_bank_item_count--;
        for (int i = slot; i < mud->new_bank_item_count; i++) {
            mud->new_bank_items[i] = mud->new_bank_items[i + 1];
            mud->new_bank_items_count[i] = mud->new_bank_items_count[i + 1];
        }
        mud->new_bank_items[mud->new_bank_item_count] = 0;
        mud->new_bank_items_count[mud->new_bank_item_count] = 0;
    }

    return removed;
}

void mudclient_offline_open_bank(mudclient *mud) {
    if (mud == NULL) {
        return;
    }

    if (mud->bank_items_max <= 0 || mud->bank_items_max > BANK_ITEMS_MAX) {
        mud->bank_items_max = 48;
    }

    mudclient_update_bank_items(mud);
    mud->bank_selected_item = -1;
    mud->bank_selected_item_slot = -1;
    mud->bank_active_page = 0;
    mud->bank_scroll_row = 0;
    mud->show_dialog_shop = 0;
    mud->show_dialog_bank = 1;
}

int mudclient_offline_bank_transaction(mudclient *mud, int item_id,
                                       int amount, int is_withdraw) {
    int moved = 0;

    if (mud == NULL || item_id < 0 || item_id >= game_data.item_count ||
        amount <= 0) {
        return 0;
    }

    if (is_withdraw) {
        int bank_slot = mudclient_offline_bank_find_item(mud, item_id);
        int bank_count =
            bank_slot >= 0 ? mud->new_bank_items_count[bank_slot] : 0;

        if (amount > bank_count) {
            amount = bank_count;
        }

        moved = mudclient_offline_add_inventory_item(mud, item_id, amount);
        if (moved > 0) {
            mudclient_offline_bank_remove_item(mud, item_id, moved);
        }
    } else {
        int inventory_count = mudclient_get_inventory_count(mud, item_id);

        if (amount > inventory_count) {
            amount = inventory_count;
        }

        moved = mudclient_offline_remove_inventory_item(mud, item_id, amount);
        if (moved > 0 &&
            mudclient_offline_bank_add_item(mud, item_id, moved) < moved) {
            mudclient_offline_add_inventory_item(mud, item_id, moved);
            moved = 0;
        }
    }

    mudclient_update_bank_items(mud);
    return moved;
}

void mudclient_offline_open_shop(mudclient *mud) {
    static const int stock[] = {
        IRON_MACE_ID, RSC_ITEM_KNIFE, RSC_ITEM_BUCKET, RSC_ITEM_SHORTBOW,
        AIR_RUNE_ID, WATER_RUNE_ID, EARTH_RUNE_ID, FIRE_RUNE_ID,
        RSC_ITEM_LOBSTER, RSC_ITEM_SWORDFISH,
    };
    static const char *named_stock[] = {
        "tinderbox",       "bronze Axe",  "Iron Axe",
        "Bronze Pickaxe",  "Iron Pickaxe","Net",
        "Fishing Rod",    "Fly Fishing Rod",
        "Feather",        "Raw Shrimp",  "raw chicken",
        "raw beef",       "copper ore",  "tin ore",
        "iron ore",       "bronze bar",  "iron bar",
        NULL,
    };

    if (mud == NULL) {
        return;
    }

    for (int i = 0; i < SHOP_ITEMS_MAX; i++) {
        mud->shop_items[i] = -1;
        mud->shop_items_count[i] = 0;
        mud->shop_items_price[i] = 0;
    }

    int max_stock = (int)(sizeof(stock) / sizeof(stock[0]));
    for (int i = 0; i < max_stock && i < SHOP_ITEMS_MAX; i++) {
        mud->shop_items[i] = stock[i];
        mud->shop_items_count[i] = 10;
        mud->shop_items_price[i] = 0;
    }
    for (int i = 0; named_stock[i] != NULL && max_stock < SHOP_ITEMS_MAX; i++) {
        int item_id = mudclient_find_offline_item(named_stock[i]);

        if (item_id < 0) {
            item_id = mudclient_find_offline_item_containing(named_stock[i]);
        }
        if (item_id < 0) {
            continue;
        }

        mud->shop_items[max_stock] = item_id;
        mud->shop_items_count[max_stock] = 25;
        mud->shop_items_price[max_stock] = 0;
        max_stock++;
    }

    mud->shop_buy_price_mod = 100;
    mud->shop_sell_price_mod = 80;
    mud->shop_selected_item_index = -1;
    mud->shop_selected_item_type = -1;
    mud->show_dialog_bank = 0;
    mud->show_dialog_shop = 1;
}

static int mudclient_offline_shop_find_slot(mudclient *mud, int item_id) {
    for (int i = 0; i < SHOP_ITEMS_MAX; i++) {
        if (mud->shop_items[i] == item_id) {
            return i;
        }
    }

    return -1;
}

int mudclient_offline_shop_buy(mudclient *mud, int item_id, int item_price) {
    int slot;

    if (mud == NULL || item_id < 0 || item_id >= game_data.item_count ||
        item_price < 0) {
        return 0;
    }

    slot = mudclient_offline_shop_find_slot(mud, item_id);
    if (slot < 0 || mud->shop_items_count[slot] <= 0) {
        return 0;
    }

    if (mudclient_get_inventory_count(mud, COINS_ID) < item_price) {
        mudclient_show_message(mud, "@cya@You do not have enough coins.",
                               MESSAGE_TYPE_GAME);
        return 0;
    }

    if (mudclient_offline_add_inventory_item(mud, item_id, 1) <= 0) {
        mudclient_show_message(mud, "@cya@Your inventory is full.",
                               MESSAGE_TYPE_GAME);
        return 0;
    }

    mudclient_offline_remove_inventory_item(mud, COINS_ID, item_price);
    mud->shop_items_count[slot]--;
    return 1;
}

int mudclient_offline_shop_sell(mudclient *mud, int item_id, int item_price) {
    int slot;

    if (mud == NULL || item_id < 0 || item_id >= game_data.item_count ||
        item_price < 0) {
        return 0;
    }

    if (mudclient_offline_remove_inventory_item(mud, item_id, 1) <= 0) {
        return 0;
    }

    mudclient_offline_add_inventory_item(mud, COINS_ID, item_price);

    slot = mudclient_offline_shop_find_slot(mud, item_id);
    if (slot >= 0 && mud->shop_items_count[slot] < INT_MAX) {
        mud->shop_items_count[slot]++;
    }

    return 1;
}

static void mudclient_seed_david_inventory(mudclient *mud) {
    mud->inventory_items_count = 0;

    mudclient_add_offline_inventory_item(mud, COINS_ID, 2147483647, 0);
    mudclient_add_offline_inventory_item(mud, RSC_ITEM_DRAGON_SWORD, 1, 1);
    mudclient_add_offline_inventory_item(mud, RSC_ITEM_DRAGON_SQUARE_SHIELD,
                                        1, 1);
    mudclient_add_offline_inventory_item(mud, RSC_ITEM_DRAGON_MEDIUM_HELMET,
                                        1, 1);
    mudclient_add_offline_inventory_item(mud, RSC_ITEM_RUNE_PLATE_BODY, 1, 1);
    mudclient_add_offline_inventory_item(mud, RSC_ITEM_RUNE_PLATE_LEGS, 1, 1);
    mudclient_add_offline_inventory_item(mud,
                                        RSC_ITEM_CHARGED_DRAGONSTONE_AMULET,
                                        1, 1);
    mudclient_add_offline_inventory_item(mud, RSC_ITEM_STEEL_GAUNTLETS, 1, 1);
    mudclient_add_offline_inventory_item(mud, RSC_ITEM_BOOTS, 1, 1);
    mudclient_add_offline_inventory_item(mud, RSC_ITEM_CAPE_OF_LEGENDS, 1, 1);
    mudclient_add_offline_inventory_item(mud, RSC_ITEM_ZAMORAK_CAPE, 1, 0);
    mudclient_add_offline_inventory_item(mud, RSC_ITEM_SARADOMIN_CAPE, 1, 0);
    mudclient_add_offline_inventory_item(mud, RSC_ITEM_GUTHIX_CAPE, 1, 0);
    mudclient_add_offline_inventory_item(mud, RSC_ITEM_DRAGON_AXE, 1, 0);
    mudclient_add_offline_inventory_item(mud, RSC_ITEM_RUNE_TWO_HANDED_SWORD,
                                        1, 0);
    mudclient_add_offline_inventory_item(mud, RSC_ITEM_RUNE_SCIMITAR, 1, 0);
    mudclient_add_offline_inventory_item(mud, RSC_ITEM_RUNE_LONGSWORD, 1, 0);
    mudclient_add_offline_inventory_item(mud, RSC_ITEM_RUNE_BATTLE_AXE, 1, 0);
    mudclient_add_offline_inventory_item(mud, RSC_ITEM_RUNE_SPEAR, 1, 0);
    mudclient_add_offline_inventory_item(mud, RSC_ITEM_RUNE_KITE_SHIELD, 1, 0);
    mudclient_add_offline_inventory_item(mud,
                                        RSC_ITEM_ENCHANTED_FIRE_BATTLESTAFF,
                                        1, 0);
    mudclient_add_offline_inventory_item(mud,
                                        RSC_ITEM_ENCHANTED_WATER_BATTLESTAFF,
                                        1, 0);
    mudclient_add_offline_inventory_item(mud,
                                        RSC_ITEM_ENCHANTED_AIR_BATTLESTAFF,
                                        1, 0);
    mudclient_add_offline_inventory_item(mud,
                                        RSC_ITEM_ENCHANTED_EARTH_BATTLESTAFF,
                                        1, 0);
    mudclient_add_offline_inventory_item(mud, RSC_ITEM_MAGIC_LONGBOW, 1, 0);
    mudclient_add_offline_inventory_item(mud, RSC_ITEM_RUNE_ARROWS, 10000, 0);
    mudclient_add_offline_inventory_item(mud, RSC_ITEM_RUNE_THROWING_DART,
                                        500, 0);
    mudclient_add_offline_inventory_item(mud, RSC_ITEM_RUNE_THROWING_KNIFE,
                                        500, 0);
    mudclient_ensure_offline_inventory_item(mud, "Bronze Pickaxe", 1, 0);
    mudclient_ensure_offline_inventory_item(mud, "Small fishing net", 1, 0);
    mudclient_ensure_offline_inventory_item(mud, "Fishing Rod", 1, 0);
    mudclient_ensure_offline_inventory_item(mud, "Feather", 25, 0);
    mudclient_ensure_offline_inventory_item(mud, "tinderbox", 1, 0);
}

static void mudclient_set_above_chaos_appearance(mudclient *mud,
                                                 GameCharacter *player) {
    mud->appearance_head_type = 3;
    mud->appearance_body_type = 4;
    mud->appearance_hair_colour = 2;
    mud->appearance_top_colour = 8;
    mud->appearance_bottom_colour = 14;
    mud->appearance_skin_colour = 0;
    mud->appearance_head_gender = 2;

    if (player != NULL) {
        player->hair_colour = mud->appearance_hair_colour;
        player->top_colour = mud->appearance_top_colour;
        player->bottom_colour = mud->appearance_bottom_colour;
        player->skin_colour = mud->appearance_skin_colour;
        mudclient_set_offline_base_appearance(mud, player);
    }
}

static void mudclient_seed_above_chaos_inventory(mudclient *mud) {
    mud->inventory_items_count = 0;

    mudclient_ensure_offline_inventory_item(mud, "Bronze Short Sword", 1, 1);
    mudclient_ensure_offline_inventory_item(mud, "Bronze Square Shield", 1, 1);
    mudclient_ensure_offline_inventory_item(mud, "Wooden Shield", 1, 0);
    mudclient_ensure_offline_inventory_item(mud, "bread", 1, 0);
    mudclient_ensure_offline_inventory_item(mud, "cookedmeat", 1, 0);
    mudclient_ensure_offline_inventory_item(mud, "tinderbox", 1, 0);
    mudclient_ensure_offline_inventory_item(mud, "bronze Axe", 1, 0);
    mudclient_ensure_offline_inventory_item(mud, "Bronze Pickaxe", 1, 0);
    mudclient_ensure_offline_inventory_item(mud, "Small fishing net", 1, 0);
    mudclient_ensure_offline_inventory_item(mud, "Fishing Rod", 1, 0);
    mudclient_ensure_offline_inventory_item(mud, "Feather", 25, 0);
}

static void mudclient_ensure_above_chaos_starter_items(mudclient *mud) {
    mudclient_ensure_offline_inventory_item(mud, "Bronze Short Sword", 1, 1);
    mudclient_ensure_offline_inventory_item(mud, "Bronze Square Shield", 1, 1);
    mudclient_ensure_offline_inventory_item(mud, "Wooden Shield", 1, 0);
    mudclient_ensure_offline_inventory_item(mud, "bread", 1, 0);
    mudclient_ensure_offline_inventory_item(mud, "cookedmeat", 1, 0);
    mudclient_ensure_offline_inventory_item(mud, "tinderbox", 1, 0);
    mudclient_ensure_offline_inventory_item(mud, "bronze Axe", 1, 0);
    mudclient_ensure_offline_inventory_item(mud, "Bronze Pickaxe", 1, 0);
    mudclient_ensure_offline_inventory_item(mud, "Small fishing net", 1, 0);
    mudclient_ensure_offline_inventory_item(mud, "Fishing Rod", 1, 0);
    mudclient_ensure_offline_inventory_item(mud, "Feather", 25, 0);
}

static void mudclient_init_offline_save(mudclient *mud,
                                        struct OfflineSave *save) {
    memset(save, 0, sizeof(*save));
    save->profile = mud->offline_profile;
    save->global_x = RSC_LUMBRIDGE_CASTLE_X;
    save->global_y = RSC_LUMBRIDGE_CASTLE_Y;
    save->appearance_hair = mud->appearance_hair_colour;
    save->appearance_top = mud->appearance_top_colour;
    save->appearance_bottom = mud->appearance_bottom_colour;
    save->appearance_skin = mud->appearance_skin_colour;
    save->combat_style = mud->combat_style;
    save->player_quest_points = mud->player_quest_points;
    save->bank_items_max = mud->bank_items_max;
    save->bank_item_count = mud->new_bank_item_count;
    save->inventory_items_count = mud->inventory_items_count;

    if (mud->quest_complete != NULL) {
        int quest_count = quests_length;

        if (quest_count > RSC_OFFLINE_QUEST_SAVE_MAX) {
            quest_count = RSC_OFFLINE_QUEST_SAVE_MAX;
        }

        for (int i = 0; i < quest_count; i++) {
            save->quest_complete[i] = mud->quest_complete[i] ? 1 : 0;
        }
    }

    for (int i = 0; i < BANK_ITEMS_MAX; i++) {
        save->bank_items[i] = mud->new_bank_items[i];
        save->bank_items_count[i] = mud->new_bank_items_count[i];
    }

    for (int i = 0; i < INVENTORY_ITEMS_MAX; i++) {
        save->inventory_item_id[i] = mud->inventory_item_id[i];
        save->inventory_item_stack_count[i] =
            mud->inventory_item_stack_count[i];
        save->inventory_equipped[i] = mud->inventory_equipped[i];
    }

    for (int i = 0; i < PLAYER_SKILL_COUNT; i++) {
        save->player_skill_current[i] = mud->player_skill_current[i];
        save->player_skill_base[i] = mud->player_skill_base[i];
        save->player_experience[i] = mud->player_experience[i];
    }
}

static int mudclient_offline_parse_key(char *line, const char *key,
                                       char **cursor) {
    size_t length = strlen(key);

    if (strncmp(line, key, length) != 0) {
        return 0;
    }

    if (line[length] != '\0' && line[length] != ' ' && line[length] != '\t' &&
        line[length] != '\r' && line[length] != '\n') {
        return 0;
    }

    *cursor = line + length;
    return 1;
}

static int mudclient_offline_parse_int(char **cursor, int *value) {
    char *p = *cursor;
    int sign = 1;
    int digits = 0;
    long parsed = 0;

    while (*p == ' ' || *p == '\t') {
        p++;
    }

    if (*p == '-') {
        sign = -1;
        p++;
    }

    while (*p >= '0' && *p <= '9') {
        parsed = parsed * 10 + (*p - '0');
        digits++;
        p++;
    }

    if (digits <= 0) {
        return 0;
    }

    *value = (int)(parsed * sign);
    *cursor = p;
    return 1;
}

static int mudclient_parse_offline_save_line(struct OfflineSave *save,
                                             char *line) {
    int a;
    int b;
    int c;
    int d;
    int index;
    char *cursor;

    if (mudclient_offline_parse_key(line, "rsc_offline_save", &cursor)) {
        return mudclient_offline_parse_int(&cursor, &a) &&
               a == RSC_SAVE_VERSION;
    }
    if (mudclient_offline_parse_key(line, "profile", &cursor)) {
        if (!mudclient_offline_parse_int(&cursor, &a)) {
            return 0;
        }
        save->profile = a;
        return 1;
    }
    if (mudclient_offline_parse_key(line, "pos", &cursor)) {
        if (!mudclient_offline_parse_int(&cursor, &a) ||
            !mudclient_offline_parse_int(&cursor, &b)) {
            return 0;
        }
        save->global_x = a;
        save->global_y = b;
        save->has_position = 1;
        return 1;
    }
    if (mudclient_offline_parse_key(line, "appearance", &cursor)) {
        if (!mudclient_offline_parse_int(&cursor, &a) ||
            !mudclient_offline_parse_int(&cursor, &b) ||
            !mudclient_offline_parse_int(&cursor, &c) ||
            !mudclient_offline_parse_int(&cursor, &d)) {
            return 0;
        }
        save->appearance_hair = a;
        save->appearance_top = b;
        save->appearance_bottom = c;
        save->appearance_skin = d;
        return 1;
    }
    if (mudclient_offline_parse_key(line, "combat_style", &cursor)) {
        if (!mudclient_offline_parse_int(&cursor, &a)) {
            return 0;
        }
        save->combat_style = a;
        return 1;
    }
    if (mudclient_offline_parse_key(line, "quest_points", &cursor)) {
        if (!mudclient_offline_parse_int(&cursor, &a)) {
            return 0;
        }
        save->player_quest_points = a;
        return 1;
    }
    if (mudclient_offline_parse_key(line, "quest", &cursor)) {
        if (!mudclient_offline_parse_int(&cursor, &index) ||
            !mudclient_offline_parse_int(&cursor, &a)) {
            return 0;
        }
        if (index >= 0 && index < RSC_OFFLINE_QUEST_SAVE_MAX) {
            save->quest_complete[index] = a ? 1 : 0;
            return 1;
        }
        return 0;
    }
    if (mudclient_offline_parse_key(line, "bank_max", &cursor)) {
        if (!mudclient_offline_parse_int(&cursor, &a)) {
            return 0;
        }
        save->bank_items_max = a;
        return 1;
    }
    if (mudclient_offline_parse_key(line, "bank_count", &cursor)) {
        if (!mudclient_offline_parse_int(&cursor, &a)) {
            return 0;
        }
        if (a >= 0 && a <= BANK_ITEMS_MAX) {
            save->bank_item_count = a;
            return 1;
        }
        return 0;
    }
    if (mudclient_offline_parse_key(line, "bank_item", &cursor)) {
        if (!mudclient_offline_parse_int(&cursor, &index) ||
            !mudclient_offline_parse_int(&cursor, &a) ||
            !mudclient_offline_parse_int(&cursor, &b)) {
            return 0;
        }
        if (index >= 0 && index < BANK_ITEMS_MAX) {
            save->bank_items[index] = a;
            save->bank_items_count[index] = b;
            return 1;
        }
        return 0;
    }
    if (mudclient_offline_parse_key(line, "inv_count", &cursor)) {
        if (!mudclient_offline_parse_int(&cursor, &a)) {
            return 0;
        }
        if (a >= 0 && a <= INVENTORY_ITEMS_MAX) {
            save->inventory_items_count = a;
            return 1;
        }
        return 0;
    }
    if (mudclient_offline_parse_key(line, "item", &cursor)) {
        if (!mudclient_offline_parse_int(&cursor, &index) ||
            !mudclient_offline_parse_int(&cursor, &a) ||
            !mudclient_offline_parse_int(&cursor, &b) ||
            !mudclient_offline_parse_int(&cursor, &c)) {
            return 0;
        }
        if (index >= 0 && index < INVENTORY_ITEMS_MAX) {
            save->inventory_item_id[index] = a;
            save->inventory_item_stack_count[index] = b;
            save->inventory_equipped[index] = c;
            return 1;
        }
        return 0;
    }
    if (mudclient_offline_parse_key(line, "skill", &cursor)) {
        if (!mudclient_offline_parse_int(&cursor, &index) ||
            !mudclient_offline_parse_int(&cursor, &a) ||
            !mudclient_offline_parse_int(&cursor, &b) ||
            !mudclient_offline_parse_int(&cursor, &c)) {
            return 0;
        }
        if (index >= 0 && index < PLAYER_SKILL_COUNT) {
            save->player_skill_current[index] = a;
            save->player_skill_base[index] = b;
            save->player_experience[index] = c;
            return 1;
        }
        return 0;
    }

    return 1;
}

static int mudclient_read_offline_save(mudclient *mud,
                                       struct OfflineSave *save) {
    char path[PATH_MAX];
    int fd;
    char *buffer;
    ssize_t bytes;
    int ok = 1;
    int saw_version = 0;

    mudclient_init_offline_save(mud, save);
    mudclient_offline_save_path(mud, path, sizeof(path));

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        return 0;
    }

    buffer = malloc(8192);
    if (buffer == NULL) {
        close(fd);
        return 0;
    }

    bytes = read(fd, buffer, 8191);
    close(fd);

    if (bytes <= 0) {
        free(buffer);
        return 0;
    }

    buffer[bytes] = '\0';

    for (char *line = strtok(buffer, "\n"); line != NULL;
         line = strtok(NULL, "\n")) {
        if (strncmp(line, "rsc_offline_save", 16) == 0) {
            saw_version = 1;
        }
        if (!mudclient_parse_offline_save_line(save, line)) {
            ok = 0;
            break;
        }
    }

    free(buffer);

    if (!ok || !saw_version ||
        save->profile != (int)mud->offline_profile) {
        return 0;
    }

    return 1;
}

static void mudclient_apply_offline_save(mudclient *mud, GameCharacter *player,
                                         struct OfflineSave *save) {
    int local_x;
    int local_y;
    int global_x = RSC_LUMBRIDGE_CASTLE_X;
    int global_y = RSC_LUMBRIDGE_CASTLE_Y;

    if (!save->has_position) {
        return;
    }

    (void)save;

    if (mudclient_load_next_region(mud, global_x, global_y)) {
        mudclient_clear_offline_walk();
    }

    mud->local_region_x = global_x - mud->region_x;
    mud->local_region_y = global_y - mud->region_y;

    local_x = mud->local_region_x * MAGIC_LOC + 64;
    local_y = mud->local_region_y * MAGIC_LOC + 64;

    player->current_x = local_x;
    player->current_y = local_y;
    player->waypoints_x[0] = local_x;
    player->waypoints_y[0] = local_y;
    player->moving_step = 0;
    player->waypoint_current = 0;
    if (mud->local_player != NULL) {
        mud->camera_auto_rotate_player_x = mud->local_player->current_x;
        mud->camera_auto_rotate_player_y = mud->local_player->current_y;
    } else {
        mud->camera_auto_rotate_player_x = local_x;
        mud->camera_auto_rotate_player_y = local_y;
    }
}

static void mudclient_load_offline_save(mudclient *mud, GameCharacter *player) {
    struct OfflineSave save;

    if (!mudclient_read_offline_save(mud, &save)) {
        return;
    }

    mud->appearance_hair_colour = save.appearance_hair;
    mud->appearance_top_colour = save.appearance_top;
    mud->appearance_bottom_colour = save.appearance_bottom;
    mud->appearance_skin_colour = save.appearance_skin;
    mud->combat_style = save.combat_style;
    mud->player_quest_points = save.player_quest_points;
    if (mud->quest_complete != NULL) {
        int quest_count = quests_length;

        if (quest_count > RSC_OFFLINE_QUEST_SAVE_MAX) {
            quest_count = RSC_OFFLINE_QUEST_SAVE_MAX;
        }

        for (int i = 0; i < quest_count; i++) {
            mud->quest_complete[i] = save.quest_complete[i] ? 1 : 0;
        }
    }
    mud->bank_items_max = save.bank_items_max;
    mud->new_bank_item_count = save.bank_item_count;
    if (mud->new_bank_item_count < 0 ||
        mud->new_bank_item_count > BANK_ITEMS_MAX) {
        mud->new_bank_item_count = 0;
    }

    for (int i = 0; i < mud->new_bank_item_count; i++) {
        int id = save.bank_items[i];
        if (id < 0 || id >= game_data.item_count ||
            save.bank_items_count[i] <= 0) {
            id = COINS_ID;
            save.bank_items_count[i] = 1;
        }
        mud->new_bank_items[i] = id;
        mud->new_bank_items_count[i] = save.bank_items_count[i];
    }

    for (int i = mud->new_bank_item_count; i < BANK_ITEMS_MAX; i++) {
        mud->new_bank_items[i] = 0;
        mud->new_bank_items_count[i] = 0;
    }

    player->hair_colour = mud->appearance_hair_colour;
    player->top_colour = mud->appearance_top_colour;
    player->bottom_colour = mud->appearance_bottom_colour;
    player->skin_colour = mud->appearance_skin_colour;

    mud->inventory_items_count = save.inventory_items_count;
    for (int i = 0; i < mud->inventory_items_count; i++) {
        int id = save.inventory_item_id[i];
        if (id < 0 || id >= game_data.item_count) {
            id = COINS_ID;
        }
        mud->inventory_item_id[i] = id;
        mud->inventory_item_stack_count[i] =
            save.inventory_item_stack_count[i] > 0
                ? save.inventory_item_stack_count[i]
                : 1;
        mud->inventory_equipped[i] = save.inventory_equipped[i] ? 1 : 0;
    }

    for (int i = mud->inventory_items_count; i < INVENTORY_ITEMS_MAX; i++) {
        mud->inventory_item_id[i] = 0;
        mud->inventory_item_stack_count[i] = 0;
        mud->inventory_equipped[i] = 0;
    }

    mudclient_update_bank_items(mud);

    for (int i = 0; i < PLAYER_SKILL_COUNT; i++) {
        mud->player_skill_current[i] = save.player_skill_current[i];
        mud->player_skill_base[i] = save.player_skill_base[i];
        mud->player_experience[i] = save.player_experience[i];
    }

    player->level = (mud->player_skill_base[SKILL_ATTACK] +
                     mud->player_skill_base[SKILL_DEFENSE] +
                     mud->player_skill_base[SKILL_STRENGTH] +
                     mud->player_skill_base[SKILL_HITS] + 27) /
                    4;
    player->current_hits = mud->player_skill_current[SKILL_HITS];
    player->max_hits = mud->player_skill_base[SKILL_HITS];

    if (mud->offline_profile == RSC_OFFLINE_PROFILE_ABOVE_CHAOS) {
        mudclient_set_above_chaos_appearance(mud, player);
        mudclient_ensure_above_chaos_starter_items(mud);
    }

    mudclient_refresh_offline_equipment(mud);
    mudclient_apply_offline_save(mud, player, &save);
    mudclient_refresh_offline_equipment(mud);
}

void mudclient_save_offline_game(mudclient *mud) {
    char path[PATH_MAX];
    char line[160];
    int fd;
    int global_x;
    int global_y;

    if (mud == NULL || mud->local_player == NULL || !mud->logged_in) {
        return;
    }

    global_x = mud->region_x + (mud->local_player->current_x / MAGIC_LOC);
    global_y = mud->region_y + (mud->local_player->current_y / MAGIC_LOC);

    mudclient_offline_save_path(mud, path, sizeof(path));
    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC);
    if (fd < 0) {
        return;
    }

    snprintf(line, sizeof(line), "rsc_offline_save %d\n", RSC_SAVE_VERSION);
    if (!mudclient_write_save_line(fd, line)) {
        close(fd);
        return;
    }

    snprintf(line, sizeof(line), "profile %d\n", mud->offline_profile);
    mudclient_write_save_line(fd, line);
    snprintf(line, sizeof(line), "pos %d %d\n", global_x, global_y);
    mudclient_write_save_line(fd, line);
    snprintf(line, sizeof(line), "appearance %d %d %d %d\n",
             mud->appearance_hair_colour, mud->appearance_top_colour,
             mud->appearance_bottom_colour, mud->appearance_skin_colour);
    mudclient_write_save_line(fd, line);
    snprintf(line, sizeof(line), "combat_style %d\n", mud->combat_style);
    mudclient_write_save_line(fd, line);
    snprintf(line, sizeof(line), "quest_points %d\n",
             mud->player_quest_points);
    mudclient_write_save_line(fd, line);
    if (mud->quest_complete != NULL) {
        int quest_count = quests_length;

        if (quest_count > RSC_OFFLINE_QUEST_SAVE_MAX) {
            quest_count = RSC_OFFLINE_QUEST_SAVE_MAX;
        }

        for (int i = 0; i < quest_count; i++) {
            snprintf(line, sizeof(line), "quest %d %d\n", i,
                     mud->quest_complete[i] ? 1 : 0);
            mudclient_write_save_line(fd, line);
        }
    }
    snprintf(line, sizeof(line), "bank_max %d\n", mud->bank_items_max);
    mudclient_write_save_line(fd, line);
    snprintf(line, sizeof(line), "bank_count %d\n", mud->new_bank_item_count);
    mudclient_write_save_line(fd, line);
    for (int i = 0; i < mud->new_bank_item_count; i++) {
        snprintf(line, sizeof(line), "bank_item %d %d %d\n", i,
                 mud->new_bank_items[i], mud->new_bank_items_count[i]);
        mudclient_write_save_line(fd, line);
    }

    snprintf(line, sizeof(line), "inv_count %d\n",
             mud->inventory_items_count);
    mudclient_write_save_line(fd, line);
    for (int i = 0; i < mud->inventory_items_count; i++) {
        snprintf(line, sizeof(line), "item %d %d %d %d\n", i,
                 mud->inventory_item_id[i],
                 mud->inventory_item_stack_count[i],
                 mud->inventory_equipped[i]);
        mudclient_write_save_line(fd, line);
    }

    for (int i = 0; i < PLAYER_SKILL_COUNT; i++) {
        snprintf(line, sizeof(line), "skill %d %d %d %d\n", i,
                 mud->player_skill_current[i], mud->player_skill_base[i],
                 mud->player_experience[i]);
        mudclient_write_save_line(fd, line);
    }

    close(fd);
}

static void mudclient_set_offline_npc_stats(GameCharacter *npc) {
    int hits = 1;

    if (npc == NULL || npc->npc_id < 0 || npc->npc_id >= game_data.npc_count) {
        return;
    }

    hits = game_data.npcs[npc->npc_id].hits;
    if (hits <= 0) {
        hits = 1;
    }

    npc->max_hits = hits;
    npc->current_hits = hits;
}

static const struct OfflineNpcSpawn *
mudclient_get_offline_npc_spawn(int server_index) {
    const struct OfflineNpcSpawn *spawns = offline_loaded_npc_spawns;
    int count = offline_loaded_npc_spawn_count;

    if (count <= 0) {
        spawns = offline_fallback_npc_spawns;
        count = sizeof(offline_fallback_npc_spawns) /
                sizeof(offline_fallback_npc_spawns[0]);
    }

    for (int i = 0; i < count; i++) {
        if (spawns[i].server_index == server_index) {
            return &spawns[i];
        }
    }

    return NULL;
}

static GameCharacter *mudclient_spawn_offline_npc(mudclient *mud,
                                                  int server_index) {
    const struct OfflineNpcSpawn *spawn =
        mudclient_get_offline_npc_spawn(server_index);

    if (spawn == NULL) {
        return NULL;
    }

    int local_x = spawn->global_x - mud->region_x;
    int local_y = spawn->global_y - mud->region_y;

    if (local_x < 0 || local_x >= REGION_WIDTH ||
        local_y < 0 || local_y >= REGION_HEIGHT) {
        return NULL;
    }

    GameCharacter *npc = mudclient_add_npc(mud, spawn->server_index,
                                           local_x * MAGIC_LOC + 64,
                                           local_y * MAGIC_LOC + 64,
                                           spawn->direction, spawn->npc_id);

    mudclient_set_offline_npc_stats(npc);

    if (npc != NULL && mud->known_npc_count < NPCS_MAX) {
        mud->known_npcs[mud->known_npc_count++] = npc;
    }

    return npc;
}

static void mudclient_remove_offline_npc(mudclient *mud, GameCharacter *npc) {
    if (npc == NULL) {
        return;
    }

    for (int i = 0; i < mud->npc_count; i++) {
        if (mud->npcs[i] == npc) {
            mud->npc_count--;
            for (int j = i; j < mud->npc_count; j++) {
                mud->npcs[j] = mud->npcs[j + 1];
            }
            mud->npcs[mud->npc_count] = NULL;
            break;
        }
    }

    for (int i = 0; i < mud->known_npc_count; i++) {
        if (mud->known_npcs[i] == npc) {
            mud->known_npc_count--;
            for (int j = i; j < mud->known_npc_count; j++) {
                mud->known_npcs[j] = mud->known_npcs[j + 1];
            }
            mud->known_npcs[mud->known_npc_count] = NULL;
            break;
        }
    }

    if (npc->server_index < NPCS_SERVER_MAX &&
        mud->npcs_server[npc->server_index] == npc) {
        mud->npcs_server[npc->server_index] = NULL;
    }

    if (mud->combat_target == npc) {
        mud->combat_target = NULL;
        offline_combat_engaged = 0;
        offline_combat_server_index = -1;
    }

    free(npc);
}

void mudclient_offline_start_combat(mudclient *mud, GameCharacter *npc) {
    if (mud == NULL || npc == NULL || npc->npc_id < 0 ||
        npc->npc_id >= game_data.npc_count ||
        game_data.npcs[npc->npc_id].attackable <= 0) {
        return;
    }

    mud->combat_target = npc;
    offline_combat_engaged = 1;
    offline_combat_server_index = npc->server_index;
    offline_combat_cooldown = 0;
}

static int mudclient_offline_character_distance(GameCharacter *a,
                                                GameCharacter *b) {
    int dx = a->current_x - b->current_x;
    int dy = a->current_y - b->current_y;

    if (dx < 0) {
        dx = -dx;
    }
    if (dy < 0) {
        dy = -dy;
    }

    return dx > dy ? dx : dy;
}

static int mudclient_offline_player_attack_skill(mudclient *mud) {
    switch (mud->combat_style) {
    case 1:
        return SKILL_DEFENSE;
    case 2:
        return SKILL_STRENGTH;
    default:
        return SKILL_ATTACK;
    }
}

static void mudclient_award_offline_combat_xp(mudclient *mud, int damage) {
    int skill = mudclient_offline_player_attack_skill(mud);
    int xp = damage * 16;

    if (damage <= 0) {
        return;
    }

    mudclient_award_offline_xp(mud, skill, xp);
    mudclient_award_offline_xp(mud, SKILL_HITS, xp);
}

static void mudclient_drop_offline_loot(mudclient *mud, GameCharacter *npc) {
    if (mud == NULL || npc == NULL || npc->npc_id < 0 ||
        npc->npc_id >= game_data.npc_count ||
        game_data.npcs[npc->npc_id].attackable <= 0) {
        return;
    }

    int x = (npc->current_x - 64) / MAGIC_LOC;
    int y = (npc->current_y - 64) / MAGIC_LOC;
    int coin_drops = 1 + (game_data.npcs[npc->npc_id].hits / 3);

    if (coin_drops > 8) {
        coin_drops = 8;
    }

    mudclient_offline_add_ground_item(mud, RSC_ITEM_BONES, x, y);
    for (int i = 0; i < coin_drops; i++) {
        mudclient_offline_add_ground_item(mud, COINS_ID, x, y);
    }
}

static void mudclient_tick_offline_combat(mudclient *mud) {
    GameCharacter *player = mud->local_player;
    GameCharacter *npc = mud->combat_target;

    if (offline_respawn_timer > 0 && --offline_respawn_timer == 0 &&
        offline_respawn_server_index >= 0) {
        mudclient_spawn_offline_npc(mud, offline_respawn_server_index);
        offline_respawn_server_index = -1;
    }

    if (!offline_combat_engaged || player == NULL || npc == NULL ||
        npc->server_index != offline_combat_server_index || npc->npc_id < 0 ||
        npc->current_hits <= 0 || npc->max_hits <= 0) {
        if (npc == NULL) {
            offline_combat_engaged = 0;
            offline_combat_server_index = -1;
        }
        return;
    }

    if (mudclient_offline_character_distance(player, npc) > MAGIC_LOC + 80) {
        int x = (npc->current_x - 64) / MAGIC_LOC;
        int y = (npc->current_y - 64) / MAGIC_LOC;

        if (offline_walk_len == 0) {
            mudclient_walk_to_action_source(mud, mud->local_region_x,
                                            mud->local_region_y, x, y, 1);
        }
        return;
    }

    npc->current_x = player->current_x;
    npc->current_y = player->current_y;
    npc->waypoints_x[npc->waypoint_current] = npc->current_x;
    npc->waypoints_y[npc->waypoint_current] = npc->current_y;
    npc->moving_step = npc->waypoint_current;

    player->next_animation = offline_combat_phase ? 8 : 9;
    npc->next_animation = offline_combat_phase ? 9 : 8;
    player->current_animation = player->next_animation;
    npc->current_animation = npc->next_animation;
    mud->combat_timeout = 500;

    if (offline_combat_cooldown > 0) {
        offline_combat_cooldown--;
        return;
    }

    int max_hit = 1 + mud->player_skill_base[SKILL_STRENGTH] / 12;
    int damage = 1 + (rand() % max_hit);

    if (damage > npc->current_hits) {
        damage = npc->current_hits;
    }

    npc->damage_taken = damage;
    npc->current_hits -= damage;
    npc->combat_timer = 35;
    player->damage_taken = 0;
    player->current_hits = mud->player_skill_current[SKILL_HITS];
    player->max_hits = mud->player_skill_base[SKILL_HITS];
    player->combat_timer = 35;

    mudclient_award_offline_combat_xp(mud, damage);

    if (npc->current_hits <= 0) {
        int server_index = npc->server_index;
        char defeated[96];

        snprintf(defeated, sizeof(defeated), "@red@You have defeated the %s.",
                 game_data.npcs[npc->npc_id].name);
        mudclient_show_message(mud, defeated, MESSAGE_TYPE_GAME);
        mudclient_drop_offline_loot(mud, npc);
        mudclient_remove_offline_npc(mud, npc);

        player->current_animation = DIR_SOUTH;
        player->next_animation = DIR_SOUTH;
        offline_respawn_server_index = server_index;
        offline_respawn_timer = 120;
        offline_combat_cooldown = 0;
        offline_combat_engaged = 0;
        offline_combat_server_index = -1;
        return;
    }

    offline_combat_phase = !offline_combat_phase;
    offline_combat_cooldown = 22;
}

static void mudclient_set_offline_base_appearance(mudclient *mud,
                                                  GameCharacter *player) {
    player->animations[ANIMATION_INDEX_HEAD] = mud->appearance_head_type + 1;
    player->animations[ANIMATION_INDEX_BODY] = mud->appearance_body_type + 1;
    player->animations[ANIMATION_INDEX_LEGS] = 3;
    player->animations[ANIMATION_INDEX_LEFT_HAND] = 0;
    player->animations[ANIMATION_INDEX_RIGHT_HAND] = 0;
    player->animations[ANIMATION_INDEX_HEAD_OVERLAY] = 0;
    player->animations[ANIMATION_INDEX_BODY_OVERLAY] = 0;
    player->animations[ANIMATION_INDEX_LEGS_OVERLAY] = 0;
    player->animations[ANIMATION_INDEX_8] = 0;
    player->animations[ANIMATION_INDEX_BOOTS] = 12;
    player->animations[ANIMATION_INDEX_NECK] = 0;
    player->animations[ANIMATION_INDEX_CAPE] = 0;
    player->skull_visible = 0;
}

static int mudclient_offline_equipment_layer(int item_id) {
    char *name;

    if (item_id < 0 || item_id >= game_data.item_count) {
        return ANIMATION_INDEX_RIGHT_HAND;
    }

    name = game_data.items[item_id].name;
    if (mudclient_offline_text_contains(name, "shield")) {
        return ANIMATION_INDEX_LEFT_HAND;
    }
    if (mudclient_offline_text_contains(name, "cape")) {
        return ANIMATION_INDEX_CAPE;
    }
    if (mudclient_offline_text_contains(name, "amulet")) {
        return ANIMATION_INDEX_NECK;
    }
    if (mudclient_offline_text_contains(name, "helmet") ||
        mudclient_offline_text_contains(name, "helm") ||
        mudclient_offline_text_contains(name, "hat")) {
        return ANIMATION_INDEX_HEAD_OVERLAY;
    }
    if (mudclient_offline_text_contains(name, "chainmail")) {
        return ANIMATION_INDEX_BODY_OVERLAY;
    }
    if (mudclient_offline_text_contains(name, "legs") ||
        mudclient_offline_text_contains(name, "skirt") ||
        mudclient_offline_text_contains(name, "robe bottom")) {
        return ANIMATION_INDEX_LEGS_OVERLAY;
    }
    if (mudclient_offline_text_contains(name, "plate") ||
        mudclient_offline_text_contains(name, "mail") ||
        mudclient_offline_text_contains(name, "robe top")) {
        return ANIMATION_INDEX_BODY;
    }
    if (mudclient_offline_text_contains(name, "boots")) {
        return ANIMATION_INDEX_BOOTS;
    }
    if (mudclient_offline_text_contains(name, "gauntlets") ||
        mudclient_offline_text_contains(name, "gloves")) {
        return ANIMATION_INDEX_8;
    }

    return ANIMATION_INDEX_RIGHT_HAND;
}

static const char *mudclient_offline_equipment_animation_name(int item_id,
                                                             int layer) {
    char *name;

    if (item_id < 0 || item_id >= game_data.item_count) {
        return NULL;
    }

    name = game_data.items[item_id].name;
    switch (layer) {
    case ANIMATION_INDEX_LEFT_HAND:
        return "squareshield";
    case ANIMATION_INDEX_RIGHT_HAND:
        if (mudclient_offline_text_contains(name, "battle axe") ||
            mudclient_offline_text_contains(name, "battleaxe") ||
            mudclient_offline_text_contains(name, " axe")) {
            return "battleaxe";
        }
        if (mudclient_offline_text_contains(name, "mace")) {
            return "mace";
        }
        if (mudclient_offline_text_contains(name, "spear")) {
            return "spear";
        }
        if (mudclient_offline_text_contains(name, "longbow") ||
            mudclient_offline_text_contains(name, "shortbow")) {
            return "longbow";
        }
        if (mudclient_offline_text_contains(name, "crossbow")) {
            return "crossbow";
        }
        if (mudclient_offline_text_contains(name, "staff")) {
            if (mudclient_offline_text_contains(name, "iban")) {
                return "ibanstaff";
            }
            if (mudclient_offline_text_contains(name, "saradomin")) {
                return "saradominstaff";
            }
            return "staff";
        }
        return "sword";
    case ANIMATION_INDEX_HEAD_OVERLAY:
        if (mudclient_offline_text_contains(name, "partyhat")) {
            return "partyhat";
        }
        if (mudclient_offline_text_contains(name, "wizard")) {
            return "wizardshat";
        }
        if (mudclient_offline_text_contains(name, "chef")) {
            return "chefshat";
        }
        if (mudclient_offline_text_contains(name, "santa")) {
            return "santahat";
        }
        if (mudclient_offline_text_contains(name, "medium")) {
            return "mediumhelm";
        }
        return "fullhelm";
    case ANIMATION_INDEX_BODY:
        if (mudclient_offline_text_contains(name, "robe")) {
            return "wizardsrobe";
        }
        if (mudclient_offline_text_contains(name, "apron")) {
            return "apron";
        }
        if (mudclient_offline_text_contains(name, "leather")) {
            return "leatherarmour";
        }
        return "platemailtop";
    case ANIMATION_INDEX_BODY_OVERLAY:
        return "chainmail";
    case ANIMATION_INDEX_LEGS_OVERLAY:
        if (mudclient_offline_text_contains(name, "skirt") ||
            mudclient_offline_text_contains(name, "robe")) {
            return "skirt";
        }
        return "platemaillegs";
    case ANIMATION_INDEX_BOOTS:
        return "boots";
    case ANIMATION_INDEX_8:
        return "leathergloves";
    case ANIMATION_INDEX_NECK:
        return "necklace";
    case ANIMATION_INDEX_CAPE:
        return "cape";
    default:
        return NULL;
    }
}

static int mudclient_find_offline_animation(const char *animation_name,
                                            int colour) {
    int fallback = 0;

    if (animation_name == NULL) {
        return 0;
    }

    for (int i = 0; i < game_data.animation_count; i++) {
        if (strcmp(game_data.animations[i].name, animation_name) != 0) {
            continue;
        }

        if (fallback == 0) {
            fallback = i + 1;
        }

        if ((int)game_data.animations[i].colour == colour) {
            return i + 1;
        }
    }

    return fallback;
}

static int mudclient_offline_equipment_colour(int item_id, int layer) {
    char *name = game_data.items[item_id].name;
    int colour = (int)game_data.items[item_id].mask;

    if (layer == ANIMATION_INDEX_NECK &&
        mudclient_offline_text_contains(name, "dragonstone")) {
        return 16763980;
    }

    if (layer == ANIMATION_INDEX_8 &&
        mudclient_offline_text_contains(name, "steel")) {
        return 11202303;
    }

    return colour;
}

static int mudclient_offline_equipment_animation(int item_id, int layer) {
    const char *animation_name =
        mudclient_offline_equipment_animation_name(item_id, layer);
    int animation = mudclient_find_offline_animation(
        animation_name, mudclient_offline_equipment_colour(item_id, layer));

    if (animation <= 0 && layer == ANIMATION_INDEX_RIGHT_HAND) {
        animation = mudclient_find_offline_animation("sword", 0);
    }

    return animation;
}

static int mudclient_offline_equipment_conflicts(int item_id, int other_id) {
    int item_wearable;
    int other_wearable;
    int item_layer;
    int other_layer;

    if (item_id < 0 || item_id >= game_data.item_count || other_id < 0 ||
        other_id >= game_data.item_count) {
        return 0;
    }

    item_wearable = game_data.items[item_id].wearable;
    other_wearable = game_data.items[other_id].wearable;
    item_layer = mudclient_offline_equipment_layer(item_id);
    other_layer = mudclient_offline_equipment_layer(other_id);

    if (item_layer == other_layer) {
        return 1;
    }

    return (((item_wearable & 24) == 24) && (other_wearable & 24)) ||
           (((other_wearable & 24) == 24) && (item_wearable & 24));
}

void mudclient_offline_wear_inventory_slot(mudclient *mud, int slot) {
    int item_id;

    if (mud == NULL || slot < 0 || slot >= mud->inventory_items_count) {
        return;
    }

    item_id = mud->inventory_item_id[slot];
    if (item_id < 0 || item_id >= game_data.item_count ||
        game_data.items[item_id].wearable <= 0) {
        return;
    }

    for (int i = 0; i < mud->inventory_items_count; i++) {
        int other_id;

        if (i == slot || !mud->inventory_equipped[i]) {
            continue;
        }

        other_id = mud->inventory_item_id[i];
        if (mudclient_offline_equipment_conflicts(item_id, other_id)) {
            mud->inventory_equipped[i] = 0;
        }
    }

    mud->inventory_equipped[slot] = 1;
    mudclient_refresh_offline_equipment(mud);
}

void mudclient_refresh_offline_equipment(mudclient *mud) {
    GameCharacter *player;

    if (mud == NULL || mud->local_player == NULL) {
        return;
    }

    player = mud->local_player;
    mudclient_set_offline_base_appearance(mud, player);

    for (int i = 0; i < mud->inventory_items_count; i++) {
        int item_id;
        int animation;
        int layer;

        if (!mud->inventory_equipped[i]) {
            continue;
        }

        item_id = mud->inventory_item_id[i];
        if (item_id < 0 || item_id >= game_data.item_count) {
            mud->inventory_equipped[i] = 0;
            continue;
        }

        if (game_data.items[item_id].wearable <= 0) {
            mud->inventory_equipped[i] = 0;
            continue;
        }

        layer = mudclient_offline_equipment_layer(item_id);
        animation = mudclient_offline_equipment_animation(item_id, layer);
        if (animation <= 0) {
            continue;
        }
        player->animations[layer] = animation;
    }
}

static void mudclient_apply_offline_account(mudclient *mud,
                                           GameCharacter *player) {
    mudclient_set_offline_base_appearance(mud, player);

    for (int i = 0; i < PLAYER_STAT_EQUIPMENT_COUNT; i++) {
        mud->player_stat_equipment[i] = 0;
    }

    mud->new_bank_item_count = 0;
    mud->bank_item_count = 0;
    for (int i = 0; i < BANK_ITEMS_MAX; i++) {
        mud->new_bank_items[i] = 0;
        mud->new_bank_items_count[i] = 0;
        mud->bank_items[i] = 0;
        mud->bank_items_count[i] = 0;
    }

    if (mud->offline_profile == RSC_OFFLINE_PROFILE_DAVID) {
        mud->moderator_level = 1;
        mudclient_seed_david_inventory(mud);
        mud->selected_item_inventory_index = -1;
        mud->player_quest_points = 999;
        if (mud->quest_complete != NULL) {
            for (int i = 0; i < quests_length; i++) {
                mud->quest_complete[i] = 1;
            }
        }
        mud->bank_items_max = 192;

        for (int i = 0; i < PLAYER_SKILL_COUNT; i++) {
            mud->player_skill_current[i] = 99;
            mud->player_skill_base[i] = 99;
            mud->player_experience[i] = 800000000;
        }
        player->level = 126;
        player->current_hits = 99;
        player->max_hits = 99;
        return;
    }

    if (mud->offline_profile == RSC_OFFLINE_PROFILE_ABOVE_CHAOS) {
        mudclient_set_above_chaos_appearance(mud, player);
    }

    mud->moderator_level = 0;
    mud->inventory_items_count = 0;
    mud->selected_item_inventory_index = -1;
    mud->player_quest_points = 0;
    if (mud->quest_complete != NULL) {
        for (int i = 0; i < quests_length; i++) {
            mud->quest_complete[i] = 0;
        }
    }
    mud->bank_items_max = 48;

    for (int i = 0; i < PLAYER_SKILL_COUNT; i++) {
        mud->player_skill_current[i] = (i == SKILL_HITS) ? 10 : 1;
        mud->player_skill_base[i] = (i == SKILL_HITS) ? 10 : 1;
        mud->player_experience[i] = 0;
    }

    player->level = 3;
    player->current_hits = 10;
    player->max_hits = 10;

    if (mud->offline_profile == RSC_OFFLINE_PROFILE_ABOVE_CHAOS) {
        mudclient_seed_above_chaos_inventory(mud);
        mudclient_refresh_offline_equipment(mud);
    }

    for (int i = 0; i < PLAYER_STAT_EQUIPMENT_COUNT; i++) {
        mud->player_stat_equipment[i] = 0;
    }
}

static void mudclient_clear_offline_npcs(mudclient *mud) {
    for (int i = 0; i < NPCS_SERVER_MAX; i++) {
        if (mud->npcs_server[i] != NULL) {
            free(mud->npcs_server[i]);
            mud->npcs_server[i] = NULL;
        }
    }

    mud->npc_count = 0;
    mud->known_npc_count = 0;
    mud->combat_target = NULL;
    offline_combat_engaged = 0;
    offline_combat_server_index = -1;

    for (int i = 0; i < NPCS_MAX; i++) {
        mud->npcs[i] = NULL;
        mud->known_npcs[i] = NULL;
    }
}

static void mudclient_refresh_offline_npcs(mudclient *mud) {
    const struct OfflineNpcSpawn *spawns = offline_loaded_npc_spawns;
    int count;

    mudclient_load_offline_spawn_data();
    mudclient_clear_offline_npcs(mud);

    count = offline_loaded_npc_spawn_count;
    if (count <= 0) {
        spawns = offline_fallback_npc_spawns;
        count = sizeof(offline_fallback_npc_spawns) /
                sizeof(offline_fallback_npc_spawns[0]);

        if (!offline_spawn_data_warned) {
            mudclient_show_message(
                mud, "@cya@Missing offline NPC spawn data; using fallback.",
                MESSAGE_TYPE_GAME);
            offline_spawn_data_warned = 1;
        }
    }

    for (int i = 0; i < count; i++) {
        mudclient_spawn_offline_npc(mud, spawns[i].server_index);
    }
}

static void mudclient_sync_offline_region(mudclient *mud) {
    if (mud->local_player == NULL || mud->loading_area) {
        return;
    }

    int global_x = mud->region_x + (mud->local_player->current_x / MAGIC_LOC);
    int global_y = mud->region_y + (mud->local_player->current_y / MAGIC_LOC);

    if (mudclient_load_next_region(mud, global_x, global_y)) {
        mud->local_region_x = mud->local_player->current_x / MAGIC_LOC;
        mud->local_region_y = mud->local_player->current_y / MAGIC_LOC;
        mud->camera_auto_rotate_player_x = mud->local_player->current_x;
        mud->camera_auto_rotate_player_y = mud->local_player->current_y;
        mudclient_clear_offline_walk();
        mudclient_refresh_offline_npcs(mud);
        mudclient_seed_offline_quest_ground_items(mud);
    }
}

#ifdef SIMULATOR
static void mudclient_sim_test_lumbridge_skills(mudclient *mud) {
    int tree_id = mudclient_find_offline_object_text("tree");
    int fish_id = mudclient_find_offline_object_text("fish");
    int rock_id = mudclient_find_offline_object_text("rock");
    int wood_xp = mud->player_experience[SKILL_WOODCUT];
    int fish_xp = mud->player_experience[SKILL_FISHING];
    int mine_xp = mud->player_experience[SKILL_MINING];
    int ok;

    if (fish_id < 0) {
        fish_id = mudclient_find_offline_object_text("water");
    }
    if (fish_id < 0) {
        fish_id = mudclient_find_offline_object_text("net");
    }

    mudclient_seed_above_chaos_inventory(mud);
    mudclient_offline_handle_object_command(mud, tree_id);
    mudclient_offline_handle_object_command(mud, fish_id);
    mudclient_offline_handle_object_command(mud, rock_id);

    ok = tree_id >= 0 && fish_id >= 0 && rock_id >= 0 &&
         mud->player_experience[SKILL_WOODCUT] > wood_xp &&
         mud->player_experience[SKILL_FISHING] > fish_xp &&
         mud->player_experience[SKILL_MINING] > mine_xp;

    fprintf(stderr, "RSC_SIM_TEST_LUMBRIDGE_SKILLS_%s "
            "tree=%d fish=%d rock=%d wc=%d fishing=%d mining=%d\n",
            ok ? "PASS" : "FAIL", tree_id, fish_id, rock_id,
            mud->player_experience[SKILL_WOODCUT] - wood_xp,
            mud->player_experience[SKILL_FISHING] - fish_xp,
            mud->player_experience[SKILL_MINING] - mine_xp);

    mud->stop_timeout = -1;
}
#endif

static void mudclient_start_offline_game(mudclient *mud) {
    int global_x = RSC_LUMBRIDGE_CASTLE_X;
    int global_y = RSC_LUMBRIDGE_CASTLE_Y;

    mud->logged_in = 1;
    mud->login_screen = 0;
    mud->plane_index = 0;
    mud->plane_width = 0;
    mud->plane_height = 0;
    mud->local_player_server_index = 0;
    mud->options->show_roofs = 0;
    mud->options->show_hover_tooltip = 1;
    mud->camera_zoom = 1100;

    /*
     * Spawn uses RSC global coordinates. load_next_region chooses the 96x96
     * local map around that point; entities subtract mud->region_x/y before
     * converting to scene pixels with MAGIC_LOC.
     */
    mudclient_load_next_region(mud, global_x, global_y);

    mud->local_region_x = global_x - mud->region_x;
    mud->local_region_y = global_y - mud->region_y;

    int local_x = mud->local_region_x * MAGIC_LOC + 64;
    int local_y = mud->local_region_y * MAGIC_LOC + 64;

    char offline_name[USERNAME_LENGTH + 1];
    if (mud->offline_profile == RSC_OFFLINE_PROFILE_ABOVE_CHAOS) {
        strcpy(offline_name, "AboveChaos");
    } else {
        strcpy(offline_name, RSC_DEFAULT_NAME);
    }

    GameCharacter *player = mudclient_add_player(mud, 0, local_x, local_y,
                                                 DIR_SOUTH);
    if (player != NULL) {
        mud->local_player = player;
        strcpy(player->name, offline_name);
        player->encoded_username = encode_username(offline_name);
        player->hair_colour = mud->appearance_hair_colour;
        player->top_colour = mud->appearance_top_colour;
        player->bottom_colour = mud->appearance_bottom_colour;
        player->skin_colour = mud->appearance_skin_colour;
        if (mud->username[0] == '\0') {
            strcpy(mud->username, offline_name);
        }
        if (mud->options->username[0] == '\0') {
            strcpy(mud->options->username, offline_name);
        }
        mudclient_apply_offline_account(mud, player);
        mudclient_load_offline_save(mud, player);
        mudclient_refresh_offline_equipment(mud);
    }

    mud->camera_auto_rotate_player_x = local_x;
    mud->camera_auto_rotate_player_y = local_y;
    mud->camera_rotation_x = 0;
    mud->camera_rotation_y = 0;
    mud->world->player_alive = 1;
    mudclient_clear_offline_walk();
    offline_combat_cooldown = 0;
    offline_combat_phase = 0;
    offline_respawn_timer = 0;
    offline_respawn_server_index = -1;
    mudclient_refresh_offline_npcs(mud);
    mudclient_seed_offline_quest_ground_items(mud);

    mud->mouse_x = mud->game_width / 2;
    mud->mouse_y = (mud->game_height - 48) / 2;

    mud_log("offline start name=%s global=%d,%d region=%d,%d local=%d,%d "
            "camera=%d,%d scene_models=%d npcs=%d items=%d mod=%d coins=%d "
            "skill=%d/%d",
            mud->username, global_x, global_y, mud->region_x, mud->region_y,
            mud->local_region_x, mud->local_region_y,
            mud->camera_auto_rotate_player_x, mud->camera_auto_rotate_player_y,
            mud->scene->model_count, mud->npc_count, mud->ground_item_count,
            mud->moderator_level, mud->inventory_item_stack_count[0],
            mud->player_skill_current[SKILL_ATTACK],
            mud->player_skill_base[SKILL_ATTACK]);

#ifdef SIMULATOR
    if (getenv("RSC_SIM_TEST_LUMBRIDGE_WORLD") != NULL) {
        int player_global_x = -1;
        int player_global_y = -1;
        int ok = 0;

        if (mud->local_player != NULL) {
            player_global_x = mud->region_x +
                              (mud->local_player->current_x / MAGIC_LOC);
            player_global_y = mud->region_y +
                              (mud->local_player->current_y / MAGIC_LOC);
        }

        ok = player_global_x == RSC_LUMBRIDGE_CASTLE_X &&
             player_global_y == RSC_LUMBRIDGE_CASTLE_Y &&
             mud->npc_count >= 20 && mud->ground_item_count >= 16;

        fprintf(stderr, "RSC_SIM_TEST_LUMBRIDGE_WORLD_%s "
                "spawn=%d,%d local=%d,%d region=%d,%d npcs=%d items=%d\n",
                ok ? "PASS" : "FAIL", player_global_x, player_global_y,
                mud->local_region_x, mud->local_region_y, mud->region_x,
                mud->region_y, mud->npc_count, mud->ground_item_count);

        mud->stop_timeout = -1;
    }

    if (getenv("RSC_SIM_TEST_LUMBRIDGE_SKILLS") != NULL) {
        mudclient_sim_test_lumbridge_skills(mud);
    }

    if (getenv("RSC_SIM_TEST_INVENTORY_USE") != NULL) {
        mud->menu_items[0].type = MENU_INVENTORY_USE;
        mud->menu_items[0].index = 0;
        mud->show_ui_tab = INVENTORY_TAB;
        mud->selected_item_inventory_index = -1;

        mudclient_menu_item_click(mud, 0);

        if (mud->show_ui_tab == INVENTORY_TAB &&
            mud->selected_item_inventory_index == 0 &&
            mud->selected_item_name ==
                game_data.items[mud->inventory_item_id[0]].name) {
            fprintf(stderr, "RSC_SIM_TEST_INVENTORY_USE_PASS\n");
        } else {
            fprintf(stderr, "RSC_SIM_TEST_INVENTORY_USE_FAIL tab=%d "
                    "selected=%d\n", mud->show_ui_tab,
                    mud->selected_item_inventory_index);
        }

        mud->stop_timeout = -1;
    }

    if (getenv("RSC_SIM_TEST_OFFLINE_COMBAT") != NULL) {
        GameCharacter *chicken = mud->npcs_server[3];
        mud->combat_target = chicken;

        if (chicken != NULL) {
            chicken->current_x = mud->local_player->current_x;
            chicken->current_y = mud->local_player->current_y;
        }

        for (int i = 0; i < 260 && mud->combat_target != NULL; i++) {
            mudclient_tick_offline_combat(mud);
        }

        if (mud->combat_target == NULL && mud->npcs_server[3] == NULL &&
            offline_respawn_timer > 0) {
            fprintf(stderr, "RSC_SIM_TEST_OFFLINE_COMBAT_PASS\n");
        } else {
            fprintf(stderr, "RSC_SIM_TEST_OFFLINE_COMBAT_FAIL target=%p "
                    "npc=%p respawn=%d\n", (void *)mud->combat_target,
                    (void *)mud->npcs_server[3], offline_respawn_timer);
        }

        mud->stop_timeout = -1;
    }
#endif
}
#endif

void mudclient_reset_game(mudclient *mud) {
#ifndef REVISION_177
    mud->system_update = 0;
#endif

    mud->combat_style = 0;
    mud->logout_timeout = 0;
    mud->login_screen = 0;
    mud->logged_in = 1;

    memset(mud->input_pm_current, '\0', INPUT_PM_LENGTH + 1);
    memset(mud->input_pm_final, '\0', INPUT_PM_LENGTH + 1);

    surface_black_screen(mud->surface);

#ifdef RENDER_3DS_GL
    mudclient_3ds_gl_frame_start(mud, 1);
    surface_draw(mud->surface);
    mudclient_3ds_gl_frame_end();
#else
    surface_draw(mud->surface);
#endif

    for (int i = 0; i < mud->object_count; i++) {
        scene_remove_model(mud->scene, mud->objects[i].model);

        world_remove_object(mud->world, mud->objects[i].x, mud->objects[i].y,
                            mud->objects[i].id);

#ifdef RENDER_SW
        game_model_destroy(mud->objects[i].model);
#endif
        free(mud->objects[i].model);
        mud->objects[i].model = NULL;
    }

    for (int i = 0; i < mud->wall_object_count; i++) {
        scene_remove_model(mud->scene, mud->wall_objects[i].model);

        world_remove_wall_object(
            mud->world, mud->wall_objects[i].x, mud->wall_objects[i].y,
            mud->wall_objects[i].direction, mud->wall_objects[i].id);

        game_model_destroy(mud->wall_objects[i].model);
        free(mud->wall_objects[i].model);
        mud->wall_objects[i].model = NULL;
    }

    mud->object_count = 0;
    mud->wall_object_count = 0;
    mud->ground_item_count = 0;
    mud->player_count = 0;

    GameCharacter *freed_characters[NPCS_SERVER_MAX] = {0};
    int freed_count = 0;

    for (int i = 0; i < PLAYERS_SERVER_MAX; i++) {
        GameCharacter *player = mud->player_server[i];

        if (player != NULL) {
            freed_characters[freed_count++] = player;
            free(player);
            mud->player_server[i] = NULL;
        }
    }

    for (int i = 0; i < PLAYERS_MAX; i++) {
    label0:;
        GameCharacter *player = mud->players[i];

        if (player) {
            for (int j = 0; j < NPCS_SERVER_MAX; j++) {
                if (freed_characters[j] == player) {
                    mud->players[i] = NULL;
                    i++;
                    goto label0;
                }
            }
        }

        free(player);
        mud->players[i] = NULL;
    }

    mud->combat_target = NULL;
    mud->local_player = malloc(sizeof(GameCharacter));
    game_character_new(mud->local_player);

    memset(freed_characters, 0, sizeof(GameCharacter *) * NPCS_SERVER_MAX);
    freed_count = 0;

    mud->npc_count = 0;

    for (int i = 0; i < NPCS_SERVER_MAX; i++) {
        GameCharacter *npc = mud->npcs_server[i];

        if (npc != NULL) {
            freed_characters[freed_count++] = npc;
            free(npc);
            mud->npcs_server[i] = NULL;
        }
    }

    for (int i = 0; i < NPCS_MAX; i++) {
    label1:;
        GameCharacter *npc = mud->npcs[i];

        if (npc != NULL) {
            for (int j = 0; j < freed_count; j++) {
                if (freed_characters[j] == npc) {
                    mud->npcs[i] = NULL;
                    i++;
                    goto label1;
                }
            }
        }

        free(npc);
        mud->npcs[i] = NULL;
    }

    for (int i = 0; i < PRAYER_COUNT; i++) {
        mud->prayer_on[i] = 0;
    }

    mud->mouse_button_click = 0;
    mud->last_mouse_button_down = 0;
    mud->mouse_button_down = 0;
    mud->show_dialog_shop = 0;
    mud->show_dialog_bank = 0;
    mud->is_sleeping = 0;
    mud->friend_list_count = 0;
}

void mudclient_login(mudclient *mud, char *username, char *password,
                     int reconnecting) {
    if (mud->world_full_timeout > 0) {
        mudclient_show_login_screen_status(mud, "Please wait...",
                                           "Connecting to server");

        delay_ticks(2000);

        mudclient_show_login_screen_status(
            mud, "Sorry! the server is currently full.",
            "Please try again later");

        return;
    }

    if (strlen(username) == 0 || strlen(password) == 0) {
        mudclient_show_login_screen_status(mud,
                                           "You must enter both a username",
                                           "and a password - Please try again");
        return;
    }

    if (mud->username != username) {
        strcpy(mud->username, username);
    }

    if (mud->password != password) {
        strcpy(mud->password, password);
    }

    char formatted_username[USERNAME_LENGTH + 1] = {0};
    format_auth_string(username, USERNAME_LENGTH, formatted_username);

    char formatted_password[PASSWORD_LENGTH + 1] = {0};
    format_auth_string(password, PASSWORD_LENGTH, formatted_password);

    if (reconnecting) {
#ifdef RENDER_3DS_GL
        mudclient_3ds_gl_frame_start(mud, 0);
#endif

        mudclient_draw_lost_connection(mud);
        surface_draw(mud->surface);

#ifdef RENDER_GL
#ifdef SDL12
        SDL_GL_SwapBuffers();
#else
        SDL_GL_SwapWindow(mud->gl_window);
#endif
#elif defined(RENDER_3DS_GL)
        mudclient_3ds_gl_frame_end();
#endif
    } else {
        mudclient_show_login_screen_status(mud, "Please wait...",
                                           "Connecting to server");
    }

    free(mud->packet_stream);
    mud->packet_stream = malloc(sizeof(PacketStream));
    packet_stream_new(mud->packet_stream, mud);

    if (mud->packet_stream->closed) {
        goto login_fail;
    }

#ifdef REVISION_177
    int session_id = packet_stream_get_int(mud->packet_stream);
    mud->session_id = session_id;
#else
    packet_stream_new_packet(mud->packet_stream, CLIENT_SESSION);

    int64_t encoded_username = encode_username(formatted_username);

    packet_stream_put_byte(mud->packet_stream,
                           (int)((encoded_username >> 16) & 31));

    if (packet_stream_flush_packet(mud->packet_stream) < 0) {
        goto login_fail;
    }

    int64_t session_id = packet_stream_get_long(mud->packet_stream);
    mud->session_id = session_id;
#endif

    if (mud->session_id == 0) {
        mudclient_show_login_screen_status(mud, "Login server offline.",
                                           "Please try again in a few mins");
        return;
    }

#ifdef REVISION_177
    mud_log("Session id: %d\n", session_id);

    packet_stream_new_packet(mud->packet_stream,
                             reconnecting ? CLIENT_RECONNECT : CLIENT_LOGIN);

    packet_stream_put_short(mud->packet_stream, VERSION);

    /* limit30 */
    packet_stream_put_short(mud->packet_stream, 0);

    packet_stream_put_long(mud->packet_stream,
                           encode_username(formatted_username));

    packet_stream_put_password(mud->packet_stream, session_id,
                               formatted_password);

    /* uid/randomDat */
    packet_stream_put_int(mud->packet_stream, 0);

    if (packet_stream_flush_packet(mud->packet_stream) < 0) {
        goto login_fail;
    }

    packet_stream_get_byte(mud->packet_stream);

    int response = packet_stream_get_byte(mud->packet_stream);
#else
#ifdef _3DS
    mud_log("Verb: Session id: %lld\n", session_id); /* ? */
#else
    mud_log("Verb: Session id: %ld\n", session_id);
#endif

    uint32_t keys[4] = {0};
    keys[0] = (int)(((float)rand() / (float)RAND_MAX) * (float)99999999);
    keys[1] = (int)(((float)rand() / (float)RAND_MAX) * (float)99999999);
    keys[2] = (int32_t)(session_id >> 32);
    keys[3] = (int32_t)(session_id);

    packet_stream_new_packet(mud->packet_stream, CLIENT_LOGIN);
    packet_stream_put_byte(mud->packet_stream, reconnecting);
    packet_stream_put_short(mud->packet_stream, VERSION);
    packet_stream_put_byte(mud->packet_stream, 0); /* limit30 */

    packet_stream_put_login_block(mud->packet_stream, formatted_username,
                                  formatted_password, keys, 0);

    if (packet_stream_flush_packet(mud->packet_stream) < 0) {
        goto login_fail;
    }

    int response = packet_stream_get_byte(mud->packet_stream);
#endif

    mud_log("Login response: %d\n", response);

    if (response == 0 || response == 1 || response == 25) {
        mud->moderator_level = response == 25;
        mud->auto_login_attempts = 0;

        strcpy(mud->options->username,
               mud->options->remember_username ? username : "");

        strcpy(mud->options->password,
               mud->options->remember_password ? password : "");

        if (mud->options->remember_username ||
            mud->options->remember_password) {
            options_save(mud->options);
        }

        mudclient_reset_game(mud);
        return;
    }

    /*if (response == 1) {
        mud->auto_login_attempts = 0;
        return;
    }*/

    if (reconnecting) {
        mudclient_reset_login_screen(mud);
        return;
    }

    // TODO enums
    switch (response) {
    case -1:
        mudclient_show_login_screen_status(mud, "Error unable to login.",
                                           "Server timed out");
        return;
    case 3:
        mudclient_show_login_screen_status(
            mud, "Invalid username or password.",
            "Try again, or create a new account");
        return;
    case 4:
        mudclient_show_login_screen_status(
            mud, "That username is already logged in.",
            "Wait 60 seconds then retry");
        return;
    case 5:
        mudclient_show_login_screen_status(mud, "The client has been updated.",
                                           "Please reload this page");
        return;
    case 6:
        mudclient_show_login_screen_status(
            mud, "You may only use 1 character at once.",
            "Your ip-address is already in use");
        return;
    case 7:
        mudclient_show_login_screen_status(mud, "Login attempts exceeded!",
                                           "Please try again in 5 minutes");
        return;
    case 8:
        mudclient_show_login_screen_status(mud, "Error unable to login.",
                                           "Server rejected session");
        return;
    case 9:
        mudclient_show_login_screen_status(mud, "Error unable to login.",
                                           "Loginserver rejected session");
        return;
    case 10:
        mudclient_show_login_screen_status(mud,
                                           "That username is already in use.",
                                           "Wait 60 seconds then retry");
        return;
    case 11:
        mudclient_show_login_screen_status(
            mud, "Account temporarily disabled.",
            "Check your message inbox for details");
        return;
    case 12:
        mudclient_show_login_screen_status(
            mud, "Account permanently disabled.",
            "Check your message inbox for details");
        return;
    case 14:
        mudclient_show_login_screen_status(
            mud, "Sorry! This world is currently full.",
            "Please try a different world");

        mud->world_full_timeout = 1500;
        return;
    case 15:
        mudclient_show_login_screen_status(mud, "You need a members account",
                                           "to login to this world");
        return;
    case 16:
        mudclient_show_login_screen_status(
            mud, "Error - no reply from loginserver.", "Please try again");
        return;
    case 17:
        mudclient_show_login_screen_status(mud,
                                           "Error - failed to decode profile.",
                                           "Contact customer support");
        return;
    case 18:
        mudclient_show_login_screen_status(
            mud, "Account suspected stolen.",
            "Press \"recover a locked account\" on front page.");
        return;
    case 20:
        mudclient_show_login_screen_status(mud, "Error - loginserver mismatch",
                                           "Please try a different world");
        return;
    case 21:
        mudclient_show_login_screen_status(mud, "Unable to login.",
                                           "That is not an RS-Classic account");
        return;
    case 22:
        mudclient_show_login_screen_status(
            mud, "Password suspected stolen.",
            "Press \"change your password\" on front page.");
        return;
    default:
        mudclient_show_login_screen_status(mud, "Error unable to login.",
                                           "Unrecognised response code");
        return;
    }

login_fail:
    if (mud->auto_login_attempts > 0) {
        int delay = 0;

        while (delay < 5000) {
            mudclient_poll_events(mud);
            delay += 16;
            delay_ticks(16);
        }

        mud->auto_login_attempts--;
        mudclient_login(mud, username, password, reconnecting);
        return;
    }

    if (reconnecting) {
        mudclient_reset_login_screen(mud);
        mud->login_screen = LOGIN_STAGE_EXISTING;
    }

    mudclient_show_login_screen_status(
        mud, "Sorry! Unable to connect.",
        "Check internet settings or try another world");
}

void mudclient_registration_login(mudclient *mud) {
    char *username =
        panel_get_text(mud->panel_login_new_user, mud->control_register_user);

    char *password = panel_get_text(mud->panel_login_new_user,
                                    mud->control_register_password);

    mud->login_screen = 2;

    panel_update_text(mud->panel_login_existing_user, mud->control_login_status,
                      "Please enter your username and password");

    panel_update_text(mud->panel_login_existing_user,
                      mud->control_login_username, username);

    panel_update_text(mud->panel_login_existing_user,
                      mud->control_login_password, password);

    mudclient_draw_login_screens(mud);
    mudclient_reset_timings(mud);
    mudclient_login(mud, username, password, 0);
}

void mudclient_register(mudclient *mud, char *username, char *password) {
    if (mud->world_full_timeout > 0) {
        mudclient_show_login_screen_status(mud, "Please wait...",
                                           "Connecting to server");

        delay_ticks(2000);

        mudclient_show_login_screen_status(
            mud, "Sorry! The server is currently full.",
            "Please try again later");

        return;
    }

    char formatted_username[USERNAME_LENGTH + 1] = {0};
    format_auth_string(username, USERNAME_LENGTH, formatted_username);

    char formatted_password[PASSWORD_LENGTH + 1] = {0};
    format_auth_string(password, PASSWORD_LENGTH, formatted_password);

    mudclient_show_login_screen_status(mud, "Please wait...",
                                       "Connecting to server");

    free(mud->packet_stream);
    mud->packet_stream = malloc(sizeof(PacketStream));
    packet_stream_new(mud->packet_stream, mud);

    if (mud->packet_stream->closed) {
        goto register_fail;
    }

#ifdef REVISION_177
    int session_id = packet_stream_get_int(mud->packet_stream);
    mud->session_id = session_id;
#else
    packet_stream_new_packet(mud->packet_stream, CLIENT_SESSION);

    int64_t encoded_username = encode_username(formatted_username);

    packet_stream_put_byte(mud->packet_stream,
                           (int)((encoded_username >> 16) & 31));

    if (packet_stream_flush_packet(mud->packet_stream) < 0) {
        goto register_fail;
    }

    int64_t session_id = packet_stream_get_long(mud->packet_stream);
    mud->session_id = session_id;
#endif

    if (mud->session_id == 0) {
        mudclient_show_login_screen_status(mud, "Login server offline.",
                                           "Please try again in a few mins");
        return;
    }

#ifdef REVISION_177
    mud_log("Session id: %d\n", session_id);

    packet_stream_new_packet(mud->packet_stream, CLIENT_REGISTER);
    packet_stream_put_short(mud->packet_stream, VERSION);

    packet_stream_put_long(mud->packet_stream,
                           encode_username(formatted_username));

    /* refer id */
    packet_stream_put_short(mud->packet_stream, 0);

    packet_stream_put_password(mud->packet_stream, session_id,
                               formatted_password);

    /* uid/randomDat */
    packet_stream_put_int(mud->packet_stream, 0);

    if (packet_stream_flush_packet(mud->packet_stream) < 0) {
        goto register_fail;
    }

    packet_stream_get_byte(mud->packet_stream);
#else
    mud_log("Verb: Session id: %ld\n", session_id);

    uint32_t keys[4] = {0};
    keys[0] = (int)(((float)rand() / (float)RAND_MAX) * (float)99999999);
    keys[1] = (int)(((float)rand() / (float)RAND_MAX) * (float)99999999);
    keys[2] = (int32_t)(session_id >> 32);
    keys[3] = (int32_t)(session_id);

    packet_stream_new_packet(mud->packet_stream, CLIENT_REGISTER);
    packet_stream_put_byte(mud->packet_stream, 0);
    packet_stream_put_short(mud->packet_stream, VERSION);
    packet_stream_put_byte(mud->packet_stream, 0); /* limit30 */

    packet_stream_put_login_block(mud->packet_stream, formatted_username,
                                  formatted_password, keys, 0);

    if (packet_stream_flush_packet(mud->packet_stream) < 0) {
        goto register_fail;
    }
#endif

    int response = packet_stream_get_byte(mud->packet_stream);
    mud_log("Newplayer response: %d\n", response);

    switch (response) {
    case 2:
        mudclient_registration_login(mud);
        return;
    case 13:
    case 3:
        mudclient_show_login_screen_status(mud, "Username already taken.",
                                           "Please choose another username");
        return;
    case 4:
        mudclient_show_login_screen_status(mud,
                                           "That username is already in use.",
                                           "Wait 60 seconds then retry");
        return;
    case 5:
        mudclient_show_login_screen_status(mud, "The client has been updated.",
                                           "Please reload this page");
        return;
    case 6:
        mudclient_show_login_screen_status(
            mud, "You may only use 1 character at once.",
            "Your ip-address is already in use");
        return;
    case 7:
        mudclient_show_login_screen_status(mud, "Login attempts exceeded!",
                                           "Please try again in 5 minutes");
        return;
    case 11:
        mudclient_show_login_screen_status(
            mud, "Account has been temporarily disabled",
            "for cheating or abuse");
        return;
    case 12:
        mudclient_show_login_screen_status(
            mud, "Account has been permanently disabled",
            "for cheating or abuse");
        /* ^ this would be "Check your message inbox for details." */
        return;
    case 14:
        mudclient_show_login_screen_status(
            mud, "Sorry! The server is currently full.",
            "Please try again later");

        mud->world_full_timeout = 1500;
        return;
    case 15:
        mudclient_show_login_screen_status(mud, "You need a members account",
                                           "to login to this server");
        return;
    case 16:
        mudclient_show_login_screen_status(mud,
                                           "Please login to a members server",
                                           "to access member-only features");
        return;
    default:
        mudclient_show_login_screen_status(mud,
                                           "Error unable to create username.",
                                           "Unrecognised response code");
        return;
    }

register_fail:
    mudclient_show_login_screen_status(
        mud, "Sorry! Unable to connect.",
        "Check internet settings or try another world");
}

void mudclient_change_password(mudclient *mud, char *old_password,
                               char *new_password) {
    char formatted_old_password[PASSWORD_LENGTH + 1] = {0};
    format_auth_string(old_password, 20, formatted_old_password);

    char formatted_new_password[PASSWORD_LENGTH + 1] = {0};
    format_auth_string(new_password, 20, formatted_new_password);

    char passwords[(PASSWORD_LENGTH * 2) + 1] = {0};
    sprintf(passwords, "%s%s", formatted_old_password, formatted_new_password);

    packet_stream_new_packet(mud->packet_stream, CLIENT_CHANGE_PASSWORD);

#ifdef REVISION_177
    packet_stream_put_password(mud->packet_stream, mud->session_id, passwords);
#endif

    packet_stream_flush_packet(mud->packet_stream);
}

#if defined(RENDER_GL) || defined(RENDER_3DS_GL)
void mudclient_update_fov(mudclient *mud) {
    if (mud->options->field_of_view) {
        mud->scene->gl_fov = glm_rad(mud->options->field_of_view / 10.0f);

        int view_distance =
            round((-254.452344 * pow(mud->scene->gl_fov, 3)) +
                  (1142.234460 * pow(mud->scene->gl_fov, 2)) -
                  (1901.194134 * mud->scene->gl_fov) + 1318.230265);

        mud->scene->view_distance =
            round((float)mud->scene->gl_height *
                  ((float)view_distance / (float)(346 - 12)));
    } else {
        float scaled_scene_height =
            (float)(mud->scene->gl_height - 1) / 1000.0f;

        /* no idea, i just used cubic regression */
        mud->scene->gl_fov = (-0.1608132078 * powf(scaled_scene_height, 3)) -
                             (0.3012063997 * powf(scaled_scene_height, 2)) +
                             (2.0149949882 * scaled_scene_height) -
                             0.0030409762;

        mud->scene->view_distance = 512;
    }
}
#endif

void mudclient_start_game(mudclient *mud) {
    mudclient_load_game_config(mud);

    if (mud->error_loading_data) {
        return;
    }

#ifdef ROCKBOX
    mudclient_set_target_fps(mud, 20);
#else
    mudclient_set_target_fps(mud, 50);
#endif

    panel_base_sprite_start = mud->sprite_util;

    int x = MUD_WIDTH - 199;
    int y = UI_BUTTON_SIZE + 1;

    int is_touch = mudclient_is_touch(mud);

    mud->panel_quests = malloc(sizeof(Panel));
    panel_new(mud->panel_quests, mud->surface, 5);

    if (is_touch) {
        x = UI_TABS_TOUCH_X - STATS_WIDTH - 1;

        y = (UI_TABS_TOUCH_Y + UI_TABS_TOUCH_HEIGHT) - STATS_COMPACT_HEIGHT -
            STATS_TAB_HEIGHT - 5;
    }

    mud->control_list_quest = panel_add_text_list_interactive(
        mud->panel_quests, x, y + STATS_TAB_HEIGHT, STATS_WIDTH,
        STATS_HEIGHT - STATS_TAB_HEIGHT, FONT_BOLD_12, 500, 1);

    mud->panel_magic = malloc(sizeof(Panel));
    panel_new(mud->panel_magic, mud->surface, 5);

    if (is_touch) {
        x = UI_TABS_TOUCH_X - MAGIC_WIDTH - 1;
        y = UI_TABS_TOUCH_Y + 10;
    }

    mud->control_list_magic = panel_add_text_list_interactive(
        mud->panel_magic, x, y + MAGIC_TAB_HEIGHT - (is_touch ? 11 : 0),
        MAGIC_WIDTH, 90 + (is_touch ? 16 : 0), FONT_BOLD_12, 500, 1);

    mud->panel_social_list = malloc(sizeof(Panel));
    panel_new(mud->panel_social_list, mud->surface, 5);

    mud->control_list_social = panel_add_text_list_interactive(
        mud->panel_social_list, x,
        y + SOCIAL_TAB_HEIGHT + 16 - (is_touch ? 11 : 0), 196,
        126 + (is_touch ? 16 : 0), FONT_BOLD_12, 500, 1);

    mudclient_load_media(mud);

    if (mud->error_loading_data) {
        return;
    }

    mudclient_load_entities(mud);

    if (mud->error_loading_data) {
        return;
    }

    mud->scene = malloc(sizeof(Scene));
#ifdef ROCKBOX
    scene_new(mud->scene, mud->surface, 7500, 7500, 1000);
#else
    if (mud->options->lowmem) {
        scene_new(mud->scene, mud->surface, 7500, 7500, 1000);
    } else {
        scene_new(mud->scene, mud->surface, 15000, 15000, 1000);
    }
#endif

#ifdef RENDER_3DS_GL
    scene_set_bounds(mud->scene, mud->game_width, mud->game_height);
#else
    scene_set_bounds(mud->scene, mud->game_width, mud->game_height - 12);
#endif

    mud->scene->clip_far_3d = 2400;
    mud->scene->clip_far_2d = 2400;
    mud->scene->fog_z_distance = 2300;

#if defined(RENDER_GL) || defined(RENDER_3DS_GL)
    mudclient_update_fov(mud);
#endif

    mud->world = malloc(sizeof(World));
    world_new(mud->world, mud->scene, mud->surface, mud->options->version_maps);

    /* used for storing minimap sprite */
    mud->world->base_media_sprite = mud->sprite_media;

    mud->world->thick_walls = mud->options->thick_walls;

    mudclient_load_textures(mud);

    if (mud->error_loading_data) {
        return;
    }

    mudclient_load_models(mud);

    if (mud->error_loading_data) {
        return;
    }

    mudclient_load_maps(mud);

    if (mud->error_loading_data) {
        return;
    }

#if !defined(ROCKBOX)
    if (mud->options->members && !mud->options->lowmem) {
        mudclient_load_sounds(mud);
    }
#endif

    if (mud->error_loading_data) {
        return;
    }

    mudclient_draw_loading_progress(mud, 100, "Starting game...");
    mudclient_create_message_tabs_panel(mud);
#if !defined(ROCKBOX)
    mudclient_create_login_panels(mud);
    mudclient_create_appearance_panel(mud);
#endif
    mudclient_create_options_panel(mud);
#if !defined(ROCKBOX)
    mudclient_reset_login_screen(mud);

    worldlist_new(mud);

    if (!mud->options->lowmem) {
        mudclient_render_login_scene_sprites(mud);
    }
#endif

#ifdef ROCKBOX
    mudclient_start_offline_game(mud);
#endif

    free(surface_texture_pixels);
    surface_texture_pixels = NULL;
}

GameModel *mudclient_create_wall_object(mudclient *mud, int x, int y,
                                        int direction, int id, int count) {
    int x1 = x;
    int y1 = y;
    int x2 = x;
    int y2 = y;

    int front_texture = game_data.wall_objects[id].texture_front;
    int back_texture = game_data.wall_objects[id].texture_back;
    int height = game_data.wall_objects[id].height;

    GameModel *game_model = malloc(sizeof(GameModel));
    game_model_new_alloc(game_model, 4, 1);

    if (direction == 0) {
        x2 = x + 1;
    } else if (direction == 1) {
        y2 = y + 1;
    } else if (direction == 2) {
        x1 = x + 1;
        y2 = y + 1;
    } else if (direction == 3) {
        x2 = x + 1;
        y2 = y + 1;
    }

    x1 *= MAGIC_LOC;
    y1 *= MAGIC_LOC;
    x2 *= MAGIC_LOC;
    y2 *= MAGIC_LOC;

    uint16_t *vertices = malloc(4 * sizeof(uint16_t));

    vertices[0] = game_model_vertex_at(
        game_model, x1, -world_get_elevation(mud->world, x1, y1), y1);

    vertices[1] = game_model_vertex_at(
        game_model, x1, -world_get_elevation(mud->world, x1, y1) - height, y1);

    vertices[2] = game_model_vertex_at(
        game_model, x2, -world_get_elevation(mud->world, x2, y2) - height, y2);

    vertices[3] = game_model_vertex_at(
        game_model, x2, -world_get_elevation(mud->world, x2, y2), y2);

    game_model_create_face(game_model, 4, vertices, front_texture,
                           back_texture);

    game_model_set_light(game_model, 0, 60, 24, -50, -10, -50);

    if (x >= 0 && y >= 0 && x < 96 && y < 96) {
        scene_add_model(mud->scene, game_model);
    }

    game_model->key = count + 10000;

    return game_model;
}

int mudclient_load_next_region(mudclient *mud, int lx, int ly) {
    if (mud->death_screen_timeout != 0) {
        mud->world->player_alive = 0;
        return 0;
    }

    mud->loading_area = 0;

    lx += mud->plane_width;
    ly += mud->plane_height;

    if (mud->last_plane_index == mud->plane_index && lx > mud->local_lower_x &&
        lx < mud->local_upper_x && ly > mud->local_lower_y &&
        ly < mud->local_upper_y) {
        mud->world->player_alive = 1;
        return 0;
    }

    surface_draw_string_centre(
        mud->surface, "Loading... Please wait", mud->surface->width / 2,
        mud->surface->height / 2 + 19, FONT_BOLD_12, WHITE);

    mudclient_draw_chat_message_tabs(mud);

#ifdef RENDER_3DS_GL
    mudclient_3ds_gl_frame_start(mud, 0);
#endif

    surface_draw(mud->surface);

#ifdef RENDER_GL
#ifdef SDL12
    SDL_GL_SwapBuffers();
#else
    SDL_GL_SwapWindow(mud->gl_window);
#endif
#elif defined(RENDER_3DS_GL)
    mudclient_3ds_gl_frame_end();
#endif

    int ax = mud->region_x;
    int ay = mud->region_y;
    int section_x = (lx + (REGION_SIZE / 2)) / REGION_SIZE;
    int section_y = (ly + (REGION_SIZE / 2)) / REGION_SIZE;

    mud->last_plane_index = mud->plane_index;
    mud->region_x = section_x * REGION_SIZE - REGION_SIZE;
    mud->region_y = section_y * REGION_SIZE - REGION_SIZE;
    mud->local_lower_x = section_x * REGION_SIZE - 32;
    mud->local_lower_y = section_y * REGION_SIZE - 32;
    mud->local_upper_x = section_x * REGION_SIZE + 32;
    mud->local_upper_y = section_y * REGION_SIZE + 32;

    world_load_section(mud->world, lx, ly, mud->last_plane_index);

#ifdef ROCKBOX
    world_add_models(mud->world, mud->game_models);
#endif

    mud->region_x -= mud->plane_width;
    mud->region_y -= mud->plane_height;

    int offset_x = mud->region_x - ax;
    int offset_y = mud->region_y - ay;

    for (int i = 0; i < mud->object_count; i++) {
        mud->objects[i].x -= offset_x;
        mud->objects[i].y -= offset_y;

        int object_x = mud->objects[i].x;
        int object_y = mud->objects[i].y;
        int object_id = mud->objects[i].id;

        GameModel *game_model = mud->objects[i].model;

        int object_direction = mud->objects[i].direction;
        int object_width = 0;
        int object_height = 0;

        if (object_direction == DIR_NORTH || object_direction == DIR_SOUTH) {
            object_width = game_data.objects[object_id].width;
            object_height = game_data.objects[object_id].height;
        } else {
            object_height = game_data.objects[object_id].width;
            object_width = game_data.objects[object_id].height;
        }

        int base_x = ((object_x + object_x + object_width) * MAGIC_LOC) / 2;
        int base_y = ((object_y + object_y + object_height) * MAGIC_LOC) / 2;

        if (object_x >= 0 && object_y >= 0 && object_x < 96 && object_y < 96) {
            scene_add_model(mud->scene, game_model);

            game_model_place(game_model, base_x,
                             -world_get_elevation(mud->world, base_x, base_y),
                             base_y);

            world_register_object(mud->world, object_x, object_y, object_id);

            if (object_id == WINDMILL_SAILS_ID) {
                game_model_translate(game_model, 0, -480, 0);
            }
        }
    }

#if defined(RENDER_GL) || defined(RENDER_3DS_GL)
    world_gl_buffer_world_models(mud->world);
#endif

    for (int i = 0; i < mud->wall_object_count; i++) {
        mud->wall_objects[i].x -= offset_x;
        mud->wall_objects[i].y -= offset_y;

        int wall_object_x = mud->wall_objects[i].x;
        int wall_object_y = mud->wall_objects[i].y;
        int wall_object_id = mud->wall_objects[i].id;
        int wall_object_dir = mud->wall_objects[i].direction;

        world_register_wall_object(mud->world, wall_object_x, wall_object_y,
                                   wall_object_dir, wall_object_id);

        game_model_destroy(mud->wall_objects[i].model);
        free(mud->wall_objects[i].model);

        GameModel *wall_object_model =
            mudclient_create_wall_object(mud, wall_object_x, wall_object_y,
                                         wall_object_dir, wall_object_id, i);

        mud->wall_objects[i].model = wall_object_model;
    }

#if defined(RENDER_GL) || defined(RENDER_3DS_GL)
    mudclient_gl_update_wall_models(mud);
#endif

    for (int i = 0; i < mud->ground_item_count; i++) {
        mud->ground_items[i].x -= offset_x;
        mud->ground_items[i].y -= offset_y;
    }

    mudclient_update_ground_item_models(mud);

    for (int i = 0; i < mud->player_count; i++) {
        GameCharacter *player = mud->players[i];

        player->current_x -= offset_x * MAGIC_LOC;
        player->current_y -= offset_y * MAGIC_LOC;

        for (int j = 0; j <= player->waypoint_current; j++) {
            player->waypoints_x[j] -= offset_x * MAGIC_LOC;
            player->waypoints_y[j] -= offset_y * MAGIC_LOC;
        }
    }

    for (int i = 0; i < mud->npc_count; i++) {
        GameCharacter *npc = mud->npcs[i];

        npc->current_x -= offset_x * MAGIC_LOC;
        npc->current_y -= offset_y * MAGIC_LOC;

        for (int j = 0; j <= npc->waypoint_current; j++) {
            npc->waypoints_x[j] -= offset_x * MAGIC_LOC;
            npc->waypoints_y[j] -= offset_y * MAGIC_LOC;
        }
    }

    mud->world->player_alive = 1;

    return 1;
}

GameCharacter *mudclient_add_character(mudclient *mud,
                                       GameCharacter **character_server,
                                       GameCharacter **known_characters,
                                       int known_character_count,
                                       int server_index, int x, int y,
                                       int animation, int npc_id) {
    if (character_server[server_index] == NULL) {
        if (npc_id == -1 && server_index == mud->local_player_server_index) {
            /* unlikely but just in case */
            for (int i = 0; i < PLAYERS_SERVER_MAX; i++) {
                if (mud->player_server[i] == mud->local_player) {
                    mud->player_server[i] = NULL;
                    break;
                }
            }

            for (int i = 0; i < PLAYERS_MAX; i++) {
                if (mud->players[i] == mud->local_player) {
                    mud->players[i] = NULL;
                    break;
                }
            }

            free(mud->local_player);
            mud->local_player = NULL;
        }

        GameCharacter *character = malloc(sizeof(GameCharacter));

        if (character == NULL) {
            return NULL;
        }

        game_character_new(character);

        character_server[server_index] = character;
        character_server[server_index]->server_index = server_index;
    }

    GameCharacter *character = character_server[server_index];
    int exists = 0;

    for (int i = 0; i < known_character_count; i++) {
        if (known_characters[i]->server_index != server_index) {
            continue;
        }

        exists = 1;
        break;
    }

    if (npc_id > -1) {
        character->npc_id = npc_id;
    }

    if (exists) {
        character->next_animation = animation;
        int waypoint_index = character->waypoint_current;

        if (x != character->waypoints_x[waypoint_index] ||
            y != character->waypoints_y[waypoint_index]) {
            waypoint_index = (waypoint_index + 1) % 10;
            character->waypoint_current = waypoint_index;
            character->waypoints_x[waypoint_index] = x;
            character->waypoints_y[waypoint_index] = y;
        }
    } else {
        character->server_index = server_index;
        character->moving_step = 0;
        character->waypoint_current = 0;
        character->current_x = x;
        character->current_y = y;
        character->waypoints_x[0] = x;
        character->waypoints_y[0] = y;
        character->current_animation = animation;
        character->next_animation = animation;
        character->step_count = 0;
    }

    return character;
}

GameCharacter *mudclient_add_player(mudclient *mud, int server_index, int x,
                                    int y, int animation) {
    if (server_index >= PLAYERS_SERVER_MAX ||
        mud->player_count >= PLAYERS_MAX) {
        return NULL;
    }

    GameCharacter *player = mudclient_add_character(
        mud, mud->player_server, mud->known_players, mud->known_player_count,
        server_index, x, y, animation, -1);

    if (player == NULL) {
        return NULL;
    }

    mud->players[mud->player_count++] = player;

    return player;
}

GameCharacter *mudclient_add_npc(mudclient *mud, int server_index, int x, int y,
                                 int animation, int npc_id) {
    if (server_index >= NPCS_SERVER_MAX || mud->npc_count >= NPCS_MAX) {
        return NULL;
    }

#ifdef RENDER_SW
    if (mud->options->diversify_npcs) {
        npc_id = diversify_npc(npc_id, server_index, x, y);
    }
#endif

    GameCharacter *npc = mudclient_add_character(
        mud, mud->npcs_server, mud->known_npcs, mud->known_npc_count,
        server_index, x, y, animation, npc_id);

    if (npc == NULL) {
        return NULL;
    }

    mud->npcs[mud->npc_count++] = npc;

    return npc;
}

void mudclient_update_bank_items(mudclient *mud) {
    mud->bank_item_count = mud->new_bank_item_count;

    for (int i = 0; i < mud->new_bank_item_count; i++) {
        mud->bank_items[i] = mud->new_bank_items[i];
        mud->bank_items_count[i] = mud->new_bank_items_count[i];
    }

    for (int i = 0; i < mud->inventory_items_count; i++) {
        if (mud->bank_item_count >= mud->bank_items_max) {
            break;
        }

        int inventory_id = mud->inventory_item_id[i];
        int has_item_in_bank = 0;

        for (int j = 0; j < mud->bank_item_count; j++) {
            if (mud->bank_items[j] == inventory_id) {
                has_item_in_bank = 1;
                break;
            }
        }

        if (!has_item_in_bank) {
            mud->bank_items[mud->bank_item_count] = inventory_id;
            mud->bank_items_count[mud->bank_item_count] = 0;
            mud->bank_item_count++;
        }
    }
}

void mudclient_close_connection(mudclient *mud) {
    if (mud->packet_stream != NULL) {
        packet_stream_new_packet(mud->packet_stream, CLIENT_CLOSE_CONNECTION);
        packet_stream_flush_packet(mud->packet_stream);
    }

    memset(mud->username, '\0', USERNAME_LENGTH + 1);
    memset(mud->password, '\0', PASSWORD_LENGTH + 1);

    mudclient_reset_login_screen(mud);
}

void mudclient_lost_connection(mudclient *mud) {
#ifndef REVISION_177
    mud->system_update = 0;
#endif

    if (mud->logout_timeout != 0) {
        mudclient_reset_login_screen(mud);
    } else {
        mud->auto_login_attempts = 10;
        mudclient_login(mud, mud->username, mud->password, 1);
    }
}

int mudclient_is_valid_camera_angle(mudclient *mud, int angle) {
    int x = mud->local_player->current_x / 128;
    int y = mud->local_player->current_y / 128;

    for (int i = 2; i >= 1; i--) {
        if (angle == 1 &&
            ((mud->world->object_adjacency[x][y - i] & 128) == 128 ||
             (mud->world->object_adjacency[x - i][y] & 128) == 128 ||
             (mud->world->object_adjacency[x - i][y - i] & 128) == 128)) {
            return 0;
        }

        if (angle == 3 &&
            ((mud->world->object_adjacency[x][y + i] & 128) == 128 ||
             (mud->world->object_adjacency[x - i][y] & 128) == 128 ||
             (mud->world->object_adjacency[x - i][y + i] & 128) == 128)) {
            return 0;
        }

        if (angle == 5 &&
            ((mud->world->object_adjacency[x][y + i] & 128) == 128 ||
             (mud->world->object_adjacency[x + i][y] & 128) == 128 ||
             (mud->world->object_adjacency[x + i][y + i] & 128) == 128)) {
            return 0;
        }

        if (angle == 7 &&
            ((mud->world->object_adjacency[x][y - i] & 128) == 128 ||
             (mud->world->object_adjacency[x + i][y] & 128) == 128 ||
             (mud->world->object_adjacency[x + i][y - i] & 128) == 128)) {
            return 0;
        }

        if (angle == 0 &&
            (mud->world->object_adjacency[x][y - i] & 128) == 128) {
            return 0;
        }

        if (angle == 2 &&
            (mud->world->object_adjacency[x - i][y] & 128) == 128) {
            return 0;
        }

        if (angle == 4 &&
            (mud->world->object_adjacency[x][y + i] & 128) == 128) {
            return 0;
        }

        if (angle == 6 &&
            (mud->world->object_adjacency[x + i][y] & 128) == 128) {
            return 0;
        }
    }

    return 1;
}

void mudclient_auto_rotate_camera(mudclient *mud) {
    if ((mud->camera_angle & 1) == 1 &&
        mudclient_is_valid_camera_angle(mud, mud->camera_angle)) {
        return;
    }

    if ((mud->camera_angle & 1) == 0 &&
        mudclient_is_valid_camera_angle(mud, mud->camera_angle)) {
        if (mudclient_is_valid_camera_angle(mud, (mud->camera_angle + 1) & 7)) {
            mud->camera_angle = (mud->camera_angle + 1) & 7;
            return;
        }

        if (mudclient_is_valid_camera_angle(mud, (mud->camera_angle + 7) & 7)) {
            mud->camera_angle = (mud->camera_angle + 7) & 7;
        }

        return;
    }

    int angles[] = {1, -1, 2, -2, 3, -3, 4};

    for (int i = 0; i < 7; i++) {
        int angle = (mud->camera_angle + angles[i] + 8) & 7;

        if (!mudclient_is_valid_camera_angle(mud, angle)) {
            continue;
        }

        mud->camera_angle = angle;
        break;
    }

    if ((mud->camera_angle & 1) == 0 &&
        mudclient_is_valid_camera_angle(mud, mud->camera_angle)) {
        if (mudclient_is_valid_camera_angle(mud, (mud->camera_angle + 1) & 7)) {
            mud->camera_angle = (mud->camera_angle + 1) & 7;
            return;
        }

        if (mudclient_is_valid_camera_angle(mud, (mud->camera_angle + 7) & 7)) {
            mud->camera_angle = (mud->camera_angle + 7) & 7;
        }
    }
}

void mudclient_handle_camera_zoom(mudclient *mud) {
    if (mud->key_up) {
        mud->camera_zoom -= 16;
    } else if (mud->key_down) {
        mud->camera_zoom += 16;
    } else if (mud->key_page_up) {
        mud->camera_zoom = ZOOM_MIN;
    } else if (mud->key_page_down) {
        mud->camera_zoom = ZOOM_MAX;
    } else if (mud->key_home) {
        mud->camera_zoom = ZOOM_OUTDOORS;
    }

    int is_touch = mudclient_is_touch(mud);

    int exclude_max_x = MUD_VANILLA_WIDTH;

    int exclude_min_y = is_touch ? 0 : mud->surface->height - 80;
    int exclude_max_y = is_touch ? 100 : mud->surface->height;

    if (mud->mouse_scroll_delta != 0 &&
        (mud->show_ui_tab == 0 || mud->show_ui_tab == MAP_TAB) &&
        !(mud->message_tab_selected != MESSAGE_TAB_ALL &&
          mud->mouse_y > exclude_min_y && mud->mouse_y <= exclude_max_y &&
          mud->mouse_x <= exclude_max_x) &&
        !mud->show_dialog_bank) {
        mud->camera_zoom += mud->mouse_scroll_delta * 24;
    }

    if (mud->camera_zoom > ZOOM_MAX) {
        mud->camera_zoom = ZOOM_MAX;
    } else if (mud->camera_zoom < ZOOM_MIN) {
        mud->camera_zoom = ZOOM_MIN;
    }
}

void mudclient_handle_game_input(mudclient *mud) {
#ifndef REVISION_177
    if (mud->system_update > 1) {
        mud->system_update--;
    }
#endif

    if (mud->show_dialog_confirm) {
        mudclient_handle_confirm_input(mud);
    } else if (mud->show_additional_options) {
        mudclient_handle_additional_options_input(mud);
    }

    if (mud->options->tab_respond && mud->key_tab &&
        mud->private_message_target != 0) {
        int is_online = 0;

        for (int i = 0; i < mud->friend_list_count; i++) {
            if (mud->friend_list[i] == mud->private_message_target &&
                mud->friend_list_online[i] > 0) {
                is_online = 1;
                break;
            }
        }

        if (is_online) {
            mud->show_dialog_social_input = SOCIAL_MESSAGE_FRIEND;

            memset(mud->input_pm_current, '\0', INPUT_PM_LENGTH + 1);
            memset(mud->input_pm_final, '\0', INPUT_PM_LENGTH + 1);
        }

        mud->key_tab = 0;
    }

    if (mud->options->middle_click_camera != 0 && mud->middle_button_down) {
        int ticks = get_ticks();

        if (ticks - mud->last_mouse_sample_ticks >= 250) {
            mud->last_mouse_sample_ticks = ticks;
            mud->last_mouse_sample_x = mud->mouse_x;
        }
    }

    mudclient_packet_tick(mud);

    if (mud->logout_timeout > 0) {
        mud->logout_timeout--;
    }

    if (mud->options->idle_logout && mud->mouse_action_timeout > 4500 &&
        mud->combat_timeout == 0 && mud->logout_timeout == 0) {
        mud->mouse_action_timeout -= 500;
        mudclient_send_logout(mud);
        return;
    }

    if (mud->local_player->current_animation == 8 ||
        mud->local_player->current_animation == 9) {
        mud->combat_timeout = 500;
    }

    if (mud->combat_timeout > 0) {
        mud->combat_timeout--;
    }

    if (mud->show_appearance_change) {
        mudclient_handle_appearance_panel_input(mud);
        return;
    }

    for (int i = 0; i < mud->player_count; i++) {
        game_character_move(mud->players[i]);
    }

#ifdef ROCKBOX
    mudclient_sync_offline_region(mud);
    mudclient_feed_offline_walk(mud);
    mudclient_run_offline_player(mud);
    mudclient_tick_offline_combat(mud);
#endif

    if (mud->death_screen_timeout > 0) {
        mud->death_screen_timeout--;

        if (mud->death_screen_timeout == 0) {
            mudclient_show_message(mud,
                                   "You have been granted another life. Be "
                                   "more careful this time!",
                                   MESSAGE_TYPE_GAME);

            mudclient_show_message(
                mud, "You retain your skills. Your objects land where you died",
                MESSAGE_TYPE_GAME);
        }
    }

    for (int i = 0; i < mud->npc_count; i++) {
        game_character_move(mud->npcs[i]);
    }

    if (mud->show_ui_tab != MAP_TAB) {
        if (an_int_346 > 0) {
            mud->sleep_word_delay_timer++;
        }

        if (an_int_347 > 0) {
            mud->sleep_word_delay_timer = 0;
        }

        an_int_346 = 0;
        an_int_347 = 0;
    }

    for (int i = 0; i < mud->player_count; i++) {
        GameCharacter *player = mud->players[i];

        if (player->projectile_range > 0) {
            player->projectile_range--;
        }
    }

#ifdef ROCKBOX
    if (mud->camera_auto_rotate_player_x - mud->local_player->current_x <
            -MAGIC_LOC * 12 ||
        mud->camera_auto_rotate_player_x - mud->local_player->current_x >
            MAGIC_LOC * 12 ||
        mud->camera_auto_rotate_player_y - mud->local_player->current_y <
            -MAGIC_LOC * 12 ||
        mud->camera_auto_rotate_player_y - mud->local_player->current_y >
            MAGIC_LOC * 12) {
#else
    if (mud->camera_auto_rotate_player_x - mud->local_player->current_x <
            -500 ||
        mud->camera_auto_rotate_player_x - mud->local_player->current_x > 500 ||
        mud->camera_auto_rotate_player_y - mud->local_player->current_y <
            -500 ||
        mud->camera_auto_rotate_player_y - mud->local_player->current_y > 500) {
#endif
        mud->camera_auto_rotate_player_x = mud->local_player->current_x;
        mud->camera_auto_rotate_player_y = mud->local_player->current_y;
    }

    if (mud->camera_auto_rotate_player_x != mud->local_player->current_x) {
        mud->camera_auto_rotate_player_x +=
            (mud->local_player->current_x - mud->camera_auto_rotate_player_x) /
            (16 + ((mud->camera_zoom - 500) / 15));
    }

    if (mud->camera_auto_rotate_player_y != mud->local_player->current_y) {
        mud->camera_auto_rotate_player_y +=
            (mud->local_player->current_y - mud->camera_auto_rotate_player_y) /
            (16 + ((mud->camera_zoom - 500) / 15));
    }

    if (mud->settings_camera_auto) {
        int k1 = mud->camera_angle * 32;
        int j3 = k1 - mud->camera_rotation;
        int direction = 1;

        if (j3 != 0) {
            mud->camera_auto_counter++;

            if (j3 > 128) {
                direction = -1;
                j3 = 256 - j3;
            } else if (j3 > 0)
                direction = 1;
            else if (j3 < -128) {
                direction = 1;
                j3 = 256 + j3;
            } else if (j3 < 0) {
                direction = -1;
                j3 = -j3;
            }

            mud->camera_rotation +=
                ((mud->camera_auto_counter * j3 + 255) / 256) * direction;

            mud->camera_rotation &= 0xff;
        } else {
            mud->camera_auto_counter = 0;
        }
    } else if (mud->camera_momentum != 0) {
        int sign = mud->camera_momentum > 0 ? 1 : -1;

        mud->camera_rotation += abs(mud->camera_momentum) * sign;
        mud->camera_momentum -= 1 * sign;
    }

    if (mud->sleep_word_delay_timer > 20) {
        mud->sleep_word_delay = 0;
        mud->sleep_word_delay_timer = 0;
    }

    if (mud->is_sleeping) {
        mudclient_handle_sleep_input(mud);
        return;
    }

    mudclient_handle_message_tabs_input(mud);

    if (mud->death_screen_timeout != 0) {
        mud->last_mouse_button_down = 0;
    }

    if (mud->show_dialog_trade || mud->show_dialog_duel ||
        (mud->show_dialog_shop && mud->options->hold_to_buy)) {

        if (mud->mouse_button_down != 0) {
            mud->mouse_button_down_time++;
        } else {
            mud->mouse_button_down_time = 0;
        }

        if (mud->mouse_button_down_time > 600) {
            mud->mouse_item_count_increment += 5000;
        } else if (mud->mouse_button_down_time > 450) {
            mud->mouse_item_count_increment += 500;
        } else if (mud->mouse_button_down_time > 300) {
            mud->mouse_item_count_increment += 50;
        } else if (mud->mouse_button_down_time > 150) {
            mud->mouse_item_count_increment += 5;
        } else if (mud->mouse_button_down_time > 50) {
            mud->mouse_item_count_increment++;
        } else if (mud->mouse_button_down_time > 20 &&
                   (mud->mouse_button_down_time & 5) == 0) {
            mud->mouse_item_count_increment++;
        }
    } else {
        mud->mouse_button_down_time = 0;
        mud->mouse_item_count_increment = 0;
    }

    if (mud->last_mouse_button_down == 1) {
        mud->mouse_button_click = 1;
    } else if (mud->last_mouse_button_down == 2) {
        mud->mouse_button_click = 2;
    }

#ifdef RENDER_GL
    scene_set_mouse_location(mud->scene, mud->gl_mouse_x, mud->gl_mouse_y);
#else
    scene_set_mouse_location(mud->scene, mud->mouse_x, mud->mouse_y);
#endif

    mud->last_mouse_button_down = 0;

    if (mud->settings_camera_auto) {
        if (mud->camera_auto_counter == 0) {
            if (mud->key_left) {
                mud->camera_angle = (mud->camera_angle + 1) & 7;
                mud->key_left = 0;

                if (!mud->fog_of_war) {
                    if ((mud->camera_angle & 1) == 0) {
                        mud->camera_angle = (mud->camera_angle + 1) & 7;
                    }

                    for (int i = 0; i < 8; i++) {
                        if (mudclient_is_valid_camera_angle(
                                mud, mud->camera_angle)) {
                            break;
                        }

                        mud->camera_angle = (mud->camera_angle + 1) & 7;
                    }
                }
            } else if (mud->key_right) {
                mud->camera_angle = (mud->camera_angle + 7) & 7;
                mud->key_right = 0;

                if (!mud->fog_of_war) {
                    if ((mud->camera_angle & 1) == 0) {
                        mud->camera_angle = (mud->camera_angle + 7) & 7;
                    }

                    for (int i = 0; i < 8; i++) {
                        if (mudclient_is_valid_camera_angle(
                                mud, mud->camera_angle)) {
                            break;
                        }

                        mud->camera_angle = (mud->camera_angle + 7) & 7;
                    }
                }
            }
        }
    } else if (mud->key_left) {
        mud->camera_rotation = (mud->camera_rotation + 2) & 0xff;
    } else if (mud->key_right) {
        mud->camera_rotation = (mud->camera_rotation - 2) & 0xff;
    }

    if (!mud->settings_camera_auto && mud->options->middle_click_camera != 0 &&
        mud->middle_button_down) {
        float scale = mud->options->middle_click_camera / 100.0f;

        mud->camera_rotation =
            (mud->origin_rotation +
             (int)((mud->mouse_x - mud->origin_mouse_x) * scale)) &
            0xff;
    }

    if (mud->options->zoom_camera) {
        mudclient_handle_camera_zoom(mud);
    } else {
        if (mud->fog_of_war && mud->camera_zoom > ZOOM_INDOORS) {
            mud->camera_zoom -= 4;
        } else if (!mud->fog_of_war && mud->camera_zoom < ZOOM_OUTDOORS) {
            mud->camera_zoom += 4;
        }
    }

    if (mud->mouse_click_x_step > 0) {
        mud->mouse_click_x_step--;
    } else if (mud->mouse_click_x_step < 0) {
        mud->mouse_click_x_step++;
    }

#ifdef RENDER_SW
    scene_scroll_texture(mud->scene, FOUNTAIN_ID);
#endif

    mud->object_animation_count++;

    if (mud->object_animation_count > 5) {
        mud->object_animation_count = 0;
        mud->object_animation_cycle = (mud->object_animation_cycle + 1) % 3;
        mud->torch_animation_cycle = (mud->torch_animation_cycle + 1) % 4;
        mud->claw_animation_cycle = (mud->claw_animation_cycle + 1) % 5;
    }

    for (int i = 0; i < mud->object_count; i++) {
        int x = mud->objects[i].x;
        int y = mud->objects[i].y;

        if (x >= 0 && y >= 0 && x < 96 && y < 96 &&
            mud->objects[i].id == WINDMILL_SAILS_ID) {
            game_model_rotate(mud->objects[i].model, 1, 0, 0);
        }
    }

    for (int i = 0; i < mud->magic_bubble_count; i++) {
        mud->magic_bubbles[i].time++;

        if (mud->magic_bubbles[i].time > 50) {
            mud->magic_bubble_count--;

            for (int j = i; j < mud->magic_bubble_count; j++) {
                mud->magic_bubbles[j] = mud->magic_bubbles[j + 1];
            }
        }
    }
}

void mudclient_handle_inputs(mudclient *mud) {
    if (mud->error_loading_data) {
        return;
    }

    mud->login_timer++;

    if (mud->logged_in == 0) {
        mud->mouse_action_timeout = 0;
        mudclient_handle_login_screen_input(mud);
    } else if (mud->logged_in == 1) {
        mud->mouse_action_timeout++;

#if defined(RENDER_GL) || defined(RENDER_3DS_GL)
        if (mud->gl_is_walking &&
            mud->scene->gl_terrain_pick_step == GL_PICK_STEP_FINISHED) {
            mud->gl_is_walking = 0;
            mud->scene->gl_terrain_pick_step = GL_PICK_STEP_NONE;

#ifdef EMSCRIPTEN
            int x = mud->world->local_x[mud->scene->gl_pick_face_tag];
            int y = mud->world->local_y[mud->scene->gl_pick_face_tag];
#else
            int x = mud->scene->gl_terrain_pick_x;
            int y = mud->scene->gl_terrain_pick_y;
#endif

            mudclient_walk_to_action_source(mud, mud->local_region_x,
                                            mud->local_region_y, x, y, 0);

            if (mud->mouse_click_x_step == -24) {
                mud->mouse_click_x_step = 24;
            }
        }
#endif

        mudclient_handle_game_input(mud);
    }

    mud->last_mouse_button_down = 0;
    mud->camera_rotation_time++;

    if (mud->camera_rotation_time > 500) {
        mud->camera_rotation_time = 0;

        if (mud->options->anti_macro) {
            int roll = (int)(((float)rand() / (float)RAND_MAX) * 4.0f);

            if ((roll & 1) == 1) {
                mud->camera_rotation_x += mud->camera_rotation_x_increment;
            }

            if ((roll & 2) == 2) {
                mud->camera_rotation_y += mud->camera_rotation_y_increment;
            }
        }
    }

    if (mud->camera_rotation_x < -50) {
        mud->camera_rotation_x_increment = 2;
    } else if (mud->camera_rotation_x > 50) {
        mud->camera_rotation_x_increment = -2;
    }

    if (mud->camera_rotation_y < -50) {
        mud->camera_rotation_y_increment = 2;
    } else if (mud->camera_rotation_y > 50) {
        mud->camera_rotation_y_increment = -2;
    }

    mudclient_decrement_message_flash(mud);
}

void mudclient_update_object_animation(mudclient *mud, int object_index,
                                       char *model_name) {
    int object_x = mud->objects[object_index].x;
    int object_y = mud->objects[object_index].y;

    int within_distance = 0;

    if (mud->options->distant_animation) {
        within_distance = 1;
    } else {
        int distance_x = object_x - (mud->local_player->current_x / 128);
        int distance_y = object_y - (mud->local_player->current_y / 128);

        within_distance = distance_x > -OBJECT_ANIMATION_DISTANCE &&
                          distance_x < OBJECT_ANIMATION_DISTANCE &&
                          distance_y > -OBJECT_ANIMATION_DISTANCE &&
                          distance_y < OBJECT_ANIMATION_DISTANCE;
    }

    if (object_x >= 0 && object_y >= 0 && object_x < 96 && object_y < 96 &&
        within_distance) {
        scene_remove_model(mud->scene, mud->objects[object_index].model);

        int model_index = game_data_get_model_index(model_name);
        GameModel *game_model = game_model_copy(mud->game_models[model_index]);

        scene_add_model(mud->scene, game_model);

        game_model_set_light(game_model, 1, 48, 48, -50, -10, -50);
        game_model_copy_position(game_model, mud->objects[object_index].model);

        game_model->key = object_index;

#ifdef RENDER_SW
        game_model_destroy(mud->objects[object_index].model);
#endif
        free(mud->objects[object_index].model);

        mud->objects[object_index].model = game_model;
    }
}

void mudclient_draw_character_message(mudclient *mud, GameCharacter *character,
                                      int x, int y, int width) {
    if (character->message_timeout <= 0) {
        return;
    }
    if (mud->received_messages_count >= RECEIVED_MESSAGE_MAX) {
        return;
    }

    int text_width = surface_text_width(character->message, 1);

    mud->received_message_mid_point[mud->received_messages_count] =
        text_width / 2;

    if (mud->received_message_mid_point[mud->received_messages_count] > 150) {
        mud->received_message_mid_point[mud->received_messages_count] = 150;
    }

    mud->received_message_height[mud->received_messages_count] =
        (text_width / 300) * surface_text_height(1);

    mud->received_message_x[mud->received_messages_count] = x + (width / 2);
    mud->received_message_y[mud->received_messages_count] = y;
    mud->received_messages[mud->received_messages_count++] = character->message;
}

void mudclient_draw_character_damage(mudclient *mud, GameCharacter *character,
                                     int x, int y, int ty, int width,
                                     int height, int is_npc, float depth) {
    if (character->current_animation != 8 &&
        character->current_animation != 9 && character->combat_timer == 0) {
        return;
    }

    if (character->combat_timer > 0) {
        int offset_x = x;

        if (character->current_animation == 8) {
            offset_x -= (20 * ty) / 100;
        } else if (character->current_animation == 9) {
            offset_x += (20 * ty) / 100;
        }

        int missing = (character->current_hits * 30) / character->max_hits;

        if (mud->health_bar_count < HEALTH_BAR_MAX) {
            mud->health_bars[mud->health_bar_count].x = offset_x + (width / 2);
            mud->health_bars[mud->health_bar_count].y = y;
            mud->health_bars[mud->health_bar_count++].missing = missing;
        }
    }

    if (character->combat_timer > 150) {
        int offset_x = x;

        if (character->current_animation == 8) {
            offset_x -= (10 * ty) / 100;
        } else if (character->current_animation == 9) {
            offset_x += (10 * ty) / 100;
        }

        surface_draw_sprite_depth(mud->surface, (offset_x + (width / 2)) - 12,
                                  (y + (height / 2)) - 12,
                                  mud->sprite_media + 11 + (is_npc ? 1 : 0),
                                  depth, depth);

        char damage_string[12] = {0};
        sprintf(damage_string, "%d", character->damage_taken);

        surface_draw_string_centre_depth(
            mud->surface, damage_string, (offset_x + (width / 2)) - 1,
            y + (height / 2) + 5, FONT_BOLD_13, WHITE, depth);
    }
}

// TODO make sure it's a human
int mudclient_should_chop_head(mudclient *mud, GameCharacter *character,
                               ANIMATION_INDEX animation_index) {
#if defined(RENDER_GL) || defined(RENDER_3DS_GL)
    int roof_id = world_get_wall_roof(mud->world, character->current_x / 128,
                                      character->current_y / 128);

    return (mud->options->show_roofs && roof_id > 0 &&
            /* check if he's smol */
            (character->npc_id > -1
                 ? game_data.npcs[character->npc_id].height >= 200
                 : 1) &&
            (animation_index == ANIMATION_INDEX_HEAD ||
             animation_index == ANIMATION_INDEX_HEAD_OVERLAY) &&
            !world_is_under_roof(mud->world, mud->local_player->current_x,
                                 mud->local_player->current_y) &&
            world_is_under_roof(mud->world, character->current_x,
                                character->current_y));
#else
    (void)mud;
    (void)character;
    (void)animation_index;

    return 0;
#endif
}

void mudclient_draw_player(mudclient *mud, int x, int y, int width, int height,
                           int id, int skew_x, int ty, float depth_top,
                           float depth_bottom) {
    GameCharacter *player = mud->players[id];

    if (player->bottom_colour == 255) {
        return;
    }

    int animation_order =
        (player->current_animation + (mud->camera_rotation + 16) / 32) & 7;

    int flip = 0;
    int i2 = animation_order;

    if (i2 == 5) {
        i2 = 3;
        flip = 1;
    } else if (i2 == 6) {
        i2 = 2;
        flip = 1;
    } else if (i2 == 7) {
        i2 = 1;
        flip = 1;
    }

    int j2 = i2 * 3 + character_walk_model[(player->step_count / 6) % 4];

    if (player->current_animation == 8) {
        i2 = 5;
        animation_order = 2;
        flip = 0;
        x -= (5 * ty) / 100;
        j2 = i2 * 3 + character_combat_model_array1[(mud->login_timer / 5) % 8];
    } else if (player->current_animation == 9) {
        i2 = 5;
        animation_order = 2;
        flip = 1;
        x += (5 * ty) / 100;
        j2 = i2 * 3 + character_combat_model_array2[(mud->login_timer / 6) % 8];
    }

#if defined(RENDER_GL) || defined(RENDER_3DS_GL)
    depth_top = (depth_bottom + depth_top) / 2.0f;
    depth_bottom = depth_top;
#endif

    for (int i = 0; i < ANIMATION_COUNT; i++) {
        ANIMATION_INDEX animation_index =
            character_animation_array[animation_order][i];

        int animation_id = player->animations[animation_index] - 1;

        if (animation_id < 0) {
            continue;
        }

        if (mudclient_should_chop_head(mud, player, animation_index)) {
            continue;
        }

        int offset_x = 0;
        int offset_y = 0;
        int j5 = j2;

        if (flip && i2 >= 1 && i2 <= 3) {
            if (game_data.animations[animation_id].has_f == 1) {
                j5 += 15;
            } else if (animation_index == ANIMATION_INDEX_RIGHT_HAND &&
                       i2 == 1) {
                offset_x = -22;
                offset_y = -3;

                j5 = i2 * 3 +
                     character_walk_model[(2 + (player->step_count / 6)) % 4];
            } else if (animation_index == ANIMATION_INDEX_RIGHT_HAND &&
                       i2 == 2) {
                offset_x = 0;
                offset_y = -8;

                j5 = i2 * 3 +
                     character_walk_model[(2 + (player->step_count / 6)) % 4];
            } else if (animation_index == ANIMATION_INDEX_RIGHT_HAND &&
                       i2 == 3) {
                offset_x = 26;
                offset_y = -5;

                j5 = i2 * 3 +
                     character_walk_model[(2 + (player->step_count / 6)) % 4];
            } else if (animation_index == ANIMATION_INDEX_LEFT_HAND &&
                       i2 == 1) {
                offset_x = 22;
                offset_y = 3;

                j5 = i2 * 3 +
                     character_walk_model[(2 + (player->step_count / 6)) % 4];
            } else if (animation_index == ANIMATION_INDEX_LEFT_HAND &&
                       i2 == 2) {
                offset_x = 0;
                offset_y = 8;

                j5 = i2 * 3 +
                     character_walk_model[(2 + (player->step_count / 6)) % 4];
            } else if (animation_index == ANIMATION_INDEX_LEFT_HAND &&
                       i2 == 3) {
                offset_x = -26;
                offset_y = 5;

                j5 = i2 * 3 +
                     character_walk_model[(2 + (player->step_count / 6)) % 4];
            }
        }

        if (i2 != 5 || game_data.animations[animation_id].has_a == 1) {
            int sprite_id = j5 + game_data.animations[animation_id].file_id;

#ifdef RENDER_SW
            if (mud->surface->surface_pixels[sprite_id] == NULL &&
                mud->surface->sprite_colours[sprite_id] == NULL) {
                /* sprite file was not loaded, probably on f2p version */
                continue;
            }
#endif

            offset_x =
                (offset_x * width) / mud->surface->sprite_width_full[sprite_id];

            offset_y = (offset_y * height) /
                       mud->surface->sprite_height_full[sprite_id];

            int clip_width =
                (width * mud->surface->sprite_width_full[sprite_id]) /
                mud->surface->sprite_width_full
                    [game_data.animations[animation_id].file_id];

            offset_x -= (clip_width - width) / 2;

            int animation_colour = game_data.animations[animation_id].colour;

            if (animation_colour == 1) {
                animation_colour = player_hair_colours[player->hair_colour];
            } else if (animation_colour == 2) {
                animation_colour =
                    player_top_bottom_colours[player->top_colour];
            } else if (animation_colour == 3) {
                animation_colour =
                    player_top_bottom_colours[player->bottom_colour];
            }

            int skin_colour = player_skin_colours[player->skin_colour];

            surface_draw_sprite_transform_mask_depth(
                mud->surface, x + offset_x, y + offset_y, clip_width, height,
                sprite_id, animation_colour, skin_colour, skew_x, flip,
                depth_top, depth_bottom);
        }
    }

    mudclient_draw_character_message(mud, player, x, y, width);

    if (player->bubble_timeout > 0 &&
        mud->action_bubble_count < ACTION_BUBBLE_MAX) {
        mud->action_bubbles[mud->action_bubble_count].x = x + (width / 2);
        mud->action_bubbles[mud->action_bubble_count].y = y;
        mud->action_bubbles[mud->action_bubble_count].scale = ty;

        mud->action_bubbles[mud->action_bubble_count++].item =
            player->bubble_item;
    }

    float damage_depth = 0.0f;

#if defined(RENDER_GL) || defined(RENDER_3DS_GL)
    damage_depth = depth_top;
#endif

    mudclient_draw_character_damage(mud, player, x, y, ty, width, height, 0,
                                    damage_depth);

    if (player->skull_visible && player->bubble_timeout == 0) {
        int k3 = skew_x + x + (width / 2);

        if (player->current_animation == 8) {
            k3 -= (20 * ty) / 100;
        } else if (player->current_animation == 9) {
            k3 += (20 * ty) / 100;
        }

        int width = (16 * ty) / 100;
        int height = (16 * ty) / 100;

        surface_draw_sprite_scale(mud->surface, k3 - (width / 2),
                                  y - (height / 2) - ((10 * ty) / 100), width,
                                  height, mud->sprite_media + 13, damage_depth);
    }
}

void mudclient_draw_npc(mudclient *mud, int x, int y, int width, int height,
                        int id, int skew_x, int ty, float depth_top,
                        float depth_bottom) {
    GameCharacter *npc = mud->npcs[id];

    int animation_order =
        (npc->current_animation + (mud->camera_rotation + 16) / 32) & 7;

    int flip = 0;
    int i2 = animation_order;

    if (i2 == 5) {
        i2 = 3;
        flip = 1;
    } else if (i2 == 6) {
        i2 = 2;
        flip = 1;
    } else if (i2 == 7) {
        i2 = 1;
        flip = 1;
    }

    int j2 =
        i2 * 3 + character_walk_model[(npc->step_count /
                                       game_data.npcs[npc->npc_id].walk_speed) %
                                      4];

    if (npc->current_animation == 8) {
        i2 = 5;
        animation_order = 2;
        flip = 0;
        x -= (game_data.npcs[npc->npc_id].combat_width * ty) / 100;
        j2 = i2 * 3 +
             character_combat_model_array1[((mud->login_timer /
                                                 (game_data.npcs[npc->npc_id]
                                                      .combat_speed) -
                                             1)) %
                                           8];
    } else if (npc->current_animation == 9) {
        i2 = 5;
        animation_order = 2;
        flip = 1;
        x += (game_data.npcs[npc->npc_id].combat_width * ty) / 100;

        j2 =
            i2 * 3 +
            character_combat_model_array2
                [(mud->login_timer / game_data.npcs[npc->npc_id].combat_speed) %
                 8];
    }

#if defined(RENDER_GL) || defined(RENDER_3DS_GL)
    depth_top = (depth_bottom + depth_top) / 2.0f;
    depth_bottom = depth_top;
#endif

    for (int i = 0; i < ANIMATION_COUNT; i++) {
        int animation_index = character_animation_array[animation_order][i];
        int animation_id = game_data.npcs[npc->npc_id].sprites[animation_index];

        if (animation_id < 0) {
            continue;
        }

        if (mudclient_should_chop_head(mud, npc, animation_index)) {
            continue;
        }

        int offset_x = 0;
        int offset_y = 0;
        int k4 = j2;

        if (flip && i2 >= 1 && i2 <= 3 &&
            game_data.animations[animation_id].has_f == 1) {
            k4 += 15;
        }

        if (i2 != 5 || game_data.animations[animation_id].has_a == 1) {
            int sprite_id = k4 + game_data.animations[animation_id].file_id;

#ifdef RENDER_SW
            if (mud->surface->surface_pixels[sprite_id] == NULL &&
                mud->surface->sprite_colours[sprite_id] == NULL) {
                /* sprite file was not loaded, probably on f2p version */
                continue;
            }
#endif

            offset_x =
                (offset_x * width) / mud->surface->sprite_width_full[sprite_id];

            offset_y = (offset_y * height) /
                       mud->surface->sprite_height_full[sprite_id];

            int clip_width =
                (width * mud->surface->sprite_width_full[sprite_id]) /
                mud->surface->sprite_width_full
                    [game_data.animations[animation_id].file_id];

            offset_x -= (clip_width - width) / 2;

            int animation_colour = game_data.animations[animation_id].colour;

            int skin_colour = 0;

            if (animation_colour == 1) {
                animation_colour = game_data.npcs[npc->npc_id].hair_colour;
                skin_colour = game_data.npcs[npc->npc_id].skin_colour;
            } else if (animation_colour == 2) {
                animation_colour = game_data.npcs[npc->npc_id].top_colour;
                skin_colour = game_data.npcs[npc->npc_id].skin_colour;
            } else if (animation_colour == 3) {
                animation_colour = game_data.npcs[npc->npc_id].bottom_colour;
                skin_colour = game_data.npcs[npc->npc_id].skin_colour;
            }

            surface_draw_sprite_transform_mask_depth(
                mud->surface, x + offset_x, y + offset_y, clip_width, height,
                sprite_id, animation_colour, skin_colour, skew_x, flip,
                depth_top, depth_bottom);
        }
    }

    mudclient_draw_character_message(mud, npc, x, y, width);

    float damage_depth = 0.0f;

#if defined(RENDER_GL) || defined(RENDER_3DS_GL)
    damage_depth = depth_top;
#endif

    mudclient_draw_character_damage(mud, npc, x, y, ty, width, height, 1,
                                    damage_depth);
}

void mudclient_draw_blue_bar(mudclient *mud) {
    int bars = 1;

    if (mud->surface->width > HBAR_WIDTH) {
        bars += mud->surface->width / HBAR_WIDTH;
    }

    for (int i = 0; i < bars; i++) {
        surface_draw_sprite(mud->surface, i * HBAR_WIDTH,
                            mud->surface->height - 16 +
                                (mud->surface->height < 268 ? 4 : 0),
                            mud->sprite_media + 22);
    }
}

int mudclient_is_in_combat(mudclient *mud) {
    return mud->local_player->current_animation == 8 ||
           mud->local_player->current_animation == 9;
}

GameCharacter *mudclient_get_opponent(mudclient *mud) {
    if (!mudclient_is_in_combat(mud)) {
        if (mud->combat_target != NULL) {
            if (mud->combat_target->max_hits <= 0) {
                return NULL;
            }

            /*
             * if there is a target, check that they are still in view
             */
            if (mud->combat_target->npc_id != -1) {
                for (int i = 0; i < mud->known_npc_count; i++) {
                    if (mud->known_npcs[i] == mud->combat_target) {
                        return mud->combat_target;
                    }
                }
            } else {
                for (int i = 0; i < mud->known_player_count; i++) {
                    if (mud->known_players[i] == mud->combat_target) {
                        return mud->combat_target;
                    }
                }
            }
        }

        return NULL;
    }

    int desired_animation = mud->local_player->current_animation == 8 ? 9 : 8;

    for (int i = 0; i < mud->known_npc_count; i++) {
        GameCharacter *npc = mud->known_npcs[i];

        if (npc->current_x == mud->local_player->current_x &&
            npc->current_y == mud->local_player->current_y &&
            npc->current_animation == desired_animation) {
            return npc;
        }
    }

    for (int i = 0; i < mud->known_player_count; i++) {
        GameCharacter *player = mud->known_players[i];

        if (player->current_x == mud->local_player->current_x &&
            player->current_y == mud->local_player->current_y &&
            player->current_animation == desired_animation) {
            return player;
        }
    }

    return NULL;
}

void mudclient_draw_ui(mudclient *mud) {
    mudclient_draw_ui_tabs(mud);

    int no_menus = !mud->show_option_menu && !mud->show_right_click_menu;

    if (no_menus) {
        mud->menu_items_count = 0;
    }

    if (mud->options->experience_drops) {
        mudclient_draw_experience_drops(mud);
    }

    if (mud->options->status_bars && !mudclient_is_touch(mud)) {
        mudclient_draw_status_bars(mud);
    }

    if (mud->show_additional_options) {
        mudclient_draw_additional_options(mud);

        if (mud->show_dialog_confirm) {
            mudclient_draw_confirm(mud);
        }
    } else if (mud->show_dialog_confirm) {
        mudclient_draw_confirm(mud);
    } else if (mud->logout_timeout != 0) {
        mudclient_draw_logout(mud);
    } else if (mud->show_dialog_welcome) {
        mudclient_draw_welcome(mud);
    } else if (mud->show_dialog_server_message) {
        mudclient_draw_server_message(mud);
    } else if (mud->show_wilderness_warning == 1) {
        mudclient_draw_wilderness_warning(mud);
    } else if (mud->show_dialog_bank && mud->combat_timeout == 0) {
        mudclient_draw_bank(mud);

        if (mud->options->bank_menus) {
            if (mud->show_right_click_menu) {
                mudclient_draw_right_click_menu(mud);
            } else {
                mudclient_create_top_mouse_menu(mud);
            }
        }
    } else if (mud->show_dialog_shop && mud->combat_timeout == 0) {
        mudclient_draw_shop(mud);
    } else if (mud->show_dialog_trade_confirm) {
        mudclient_draw_trade_confirm(mud);
    } else if (mud->show_dialog_trade) {
        mudclient_draw_trade(mud);

        if (mud->options->transaction_menus) {
            if (mud->show_right_click_menu) {
                mudclient_draw_right_click_menu(mud);
            } else {
                mudclient_create_top_mouse_menu(mud);
            }
        }
    } else if (mud->show_dialog_duel_confirm) {
        mudclient_draw_duel_confirm(mud);
    } else if (mud->show_dialog_duel) {
        mudclient_draw_duel(mud);

        if (mud->options->transaction_menus) {
            if (mud->show_right_click_menu) {
                mudclient_draw_right_click_menu(mud);
            } else {
                mudclient_create_top_mouse_menu(mud);
            }
        }
    } else if (mud->show_change_password_step != 0) {
        mudclient_draw_change_password(mud);
    } else if (mud->show_dialog_social_input != 0) {
        mudclient_draw_social_input(mud);
    } else {
        if (mud->show_option_menu) {
            mudclient_draw_option_menu(mud);
        }

        mudclient_set_active_ui_tab(mud, no_menus);

        if (mudclient_is_in_combat(mud) || mud->options->combat_style_always) {
            mudclient_draw_combat_style(mud);
        }

        if (mud->show_ui_tab == 0 && no_menus) {
            mudclient_create_right_click_menu(mud);
        }

        mudclient_draw_active_ui_tab(mud, no_menus);

        if (!mud->show_option_menu) {
            if (mud->show_right_click_menu) {
                mudclient_draw_right_click_menu(mud);
            } else {
                mudclient_create_top_mouse_menu(mud);

#if defined(RENDER_GL) || defined(RENDER_3DS_GL)
                if (!mud->gl_is_walking) {
                    mud->scene->gl_terrain_pick_step = GL_PICK_STEP_NONE;
                }
#endif
            }
        }

        mudclient_draw_hover_tooltip(mud);
    }

    mud->mouse_button_click = 0;
}

int mudclient_compare_text(const void *v1, const void *v2) {
    struct OverworldText *ot1 = (struct OverworldText *)v1;
    struct OverworldText *ot2 = (struct OverworldText *)v2;
    return strcmp(ot1->text, ot2->text);
}

void mudclient_draw_overhead(mudclient *mud) {
    for (int i = 0; i < mud->received_messages_count; i++) {
        int text_height = surface_text_height(1);
        int x = mud->received_message_x[i];
        int y = mud->received_message_y[i];
        int message_mid = mud->received_message_mid_point[i];
        int message_height = mud->received_message_height[i];
        int flag = 1;

        while (flag) {
            flag = 0;

            for (int j = 0; j < i; j++) {
                if (y + message_height >
                        mud->received_message_y[j] - text_height &&
                    y - text_height < mud->received_message_y[j] +
                                          mud->received_message_height[j] &&
                    x - message_mid < mud->received_message_x[j] +
                                          mud->received_message_mid_point[j] &&
                    x + message_mid > mud->received_message_x[j] -
                                          mud->received_message_mid_point[j] &&
                    mud->received_message_y[j] - text_height - message_height <
                        y) {
                    y = mud->received_message_y[j] - text_height -
                        message_height;

                    flag = 1;
                }
            }
        }

        mud->received_message_y[i] = y;

#ifdef RENDER_GL
        if (mudclient_is_ui_scaled(mud)) {
            x /= 2;
            y /= 2;
        }
#endif

        surface_draw_paragraph(mud->surface, mud->received_messages[i], x, y, 1,
                               YELLOW, 300);
    }

    for (int i = 0; i < mud->action_bubble_count; i++) {
        int x = mud->action_bubbles[i].x;
        int y = mud->action_bubbles[i].y;
        int scale = mud->action_bubbles[i].scale;

#ifdef RENDER_GL
        if (mudclient_is_ui_scaled(mud)) {
            x /= 2;
            y /= 2;
            scale /= 2;
        }
#endif

        int id = mud->action_bubbles[i].item;
        int scale_x = (39 * scale) / 100;
        int scale_y = (27 * scale) / 100;

        surface_draw_sprite_scale_alpha(mud->surface, x - (scale_x / 2),
                                        y - scale_y, scale_x, scale_y,
                                        mud->sprite_media + 9, 85);

        int scale_x_clip = (36 * scale) / 100;
        int scale_y_clip = (24 * scale) / 100;

        int final_x = x - (scale_x_clip / 2);
        int final_y = (y - scale_y + (scale_y / 2)) - (scale_y_clip / 2);

        surface_draw_sprite_transform_mask(
            mud->surface, final_x, final_y, scale_x_clip, scale_y_clip,
            game_data.items[id].sprite + mud->sprite_item,
            game_data.items[id].mask, 0, 0, 0);
    }

    /* prevent strobing from random sort order */
    qsort(mud->overworld_text, mud->overworld_text_count,
          sizeof(struct OverworldText), mudclient_compare_text);

    /* check and fix overlapping text */
    for (int i = 0; i < mud->overworld_text_count; i++) {
        int x = mud->overworld_text[i].x;
        int y = mud->overworld_text[i].y;
        int width =
            surface_text_width(mud->overworld_text[i].text, FONT_REGULAR_11);
        int height = surface_text_height(FONT_REGULAR_11);
        for (int j = 0; j < mud->overworld_text_count; j++) {
            int x2 = mud->overworld_text[j].x;
            int y2 = mud->overworld_text[j].y;
            if ((x + width + 2) < x2 || (x - width - 2) > x2) {
                continue;
            }
            if ((y + height + 2) < y2 || (y - height - 2) > y2) {
                continue;
            }
            mud->overworld_text[i].y += (height + 1);
        }
    }

    for (int i = 0; i < mud->overworld_text_count; i++) {
        int32_t colour = (int32_t)mud->overworld_text[i].colour;
        int x = mud->overworld_text[i].x;
        int y = mud->overworld_text[i].y;

        surface_draw_string_centre(mud->surface, mud->overworld_text[i].text, x,
                                   y, FONT_REGULAR_11, colour);
    }

    for (int i = 0; i < mud->health_bar_count; i++) {
        int x = mud->health_bars[i].x;
        int y = mud->health_bars[i].y;
        int missing = mud->health_bars[i].missing;

#ifdef RENDER_GL
        if (mudclient_is_ui_scaled(mud)) {
            x /= 2;
            y /= 2;
        }
#endif

        surface_draw_box_alpha(mud->surface, x - 15, y - 3, missing, 5, GREEN,
                               192);

        surface_draw_box_alpha(mud->surface, (x - 15) + missing, y - 3,
                               30 - missing, 5, RED, 192);
    }
}

void mudclient_animate_objects(mudclient *mud) {
    char name[23] = {0};

    if (mud->object_animation_cycle != mud->last_object_animation_cycle) {
        mud->last_object_animation_cycle = mud->object_animation_cycle;

        for (int i = 0; i < mud->object_count; i++) {
            if (mud->objects[i].id == FIRE_ID) {
                sprintf(name, "firea%d", (mud->object_animation_cycle + 1));
                mudclient_update_object_animation(mud, i, name);
            } else if (mud->objects[i].id == FIREPLACE_ID) {
                sprintf(name, "fireplacea%d",
                        (mud->object_animation_cycle + 1));
                mudclient_update_object_animation(mud, i, name);
            } else if (mud->objects[i].id == LIGHTNING_ID) {
                sprintf(name, "lightning%d", (mud->object_animation_cycle + 1));
                mudclient_update_object_animation(mud, i, name);
            } else if (mud->objects[i].id == FIRE_SPELL_ID) {
                sprintf(name, "firespell%d", (mud->object_animation_cycle + 1));
                mudclient_update_object_animation(mud, i, name);
            } else if (mud->objects[i].id == SPELL_CHARGE_ID) {
                sprintf(name, "spellcharge%d",
                        (mud->object_animation_cycle + 1));

                mudclient_update_object_animation(mud, i, name);
            }
        }
    }

    if (mud->torch_animation_cycle != mud->last_torch_animation_cycle) {
        mud->last_torch_animation_cycle = mud->torch_animation_cycle;

        for (int i = 0; i < mud->object_count; i++) {
            if (mud->objects[i].id == TORCH_ID) {
                sprintf(name, "torcha%d", mud->torch_animation_cycle + 1);
                mudclient_update_object_animation(mud, i, name);
            } else if (mud->objects[i].id == SKULL_TORCH_ID) {
                sprintf(name, "skulltorcha%d", mud->torch_animation_cycle + 1);
                mudclient_update_object_animation(mud, i, name);
            }
        }
    }

    if (mud->claw_animation_cycle != mud->last_claw_animation_cycle) {
        mud->last_claw_animation_cycle = mud->claw_animation_cycle;

        for (int i = 0; i < mud->object_count; i++) {
            if (mud->objects[i].id == CLAW_SPELL_ID) {
                sprintf(name, "clawspell%d", mud->claw_animation_cycle + 1);
                mudclient_update_object_animation(mud, i, name);
            }
        }
    }
}

// TODO prepare entity sprites
void mudclient_draw_entity_sprites(mudclient *mud) {
    scene_reduce_sprites(mud->scene, mud->scene_sprite_count);

    mud->scene_sprite_count = 0;

    for (int i = 0; i < mud->player_count; i++) {
        GameCharacter *player = mud->players[i];

        if (player->bottom_colour == 255) {
            continue;
        }

        int x = player->current_x;
        int y = player->current_y;
        int elevation = -world_get_elevation(mud->world, x, y);

        int sprite_id = scene_add_sprite(mud->scene, 5000 + i, x, elevation, y,
                                         145, 220, i + PLAYER_FACE_TAG);

        mud->scene_sprite_count++;

        if (player == mud->local_player) {
            scene_set_local_player(mud->scene, sprite_id);
        }

        if (player->current_animation == 8) {
            scene_set_sprite_translate_x(mud->scene, sprite_id, -30);
        } else if (player->current_animation == 9) {
            scene_set_sprite_translate_x(mud->scene, sprite_id, 30);
        }
    }

    for (int i = 0; i < mud->player_count; i++) {
        GameCharacter *player = mud->players[i];

        if (player->projectile_range > 0) {
            GameCharacter *character = NULL;

            if (player->attacking_npc_server_index != -1) {
                character =
                    mud->npcs_server[player->attacking_npc_server_index];
            } else if (player->attacking_player_server_index != -1) {
                character =
                    mud->player_server[player->attacking_player_server_index];
            }

            if (character != NULL) {
                int sx = player->current_x;
                int sy = player->current_y;
                int selev = -world_get_elevation(mud->world, sx, sy) - 110;
                int dx = character->current_x;
                int dy = character->current_y;

                /*
                 * Original game incorrectly uses the height of unicorns
                 * for players here, match it.
                 */
                int target_height =
                    player->attacking_npc_server_index != -1
                        ? game_data.npcs[character->npc_id].height
                        : game_data.npcs[0].height;

                int delev = -world_get_elevation(mud->world, dx, dy) -
                            (target_height / 2);

                int rx =
                    (sx * player->projectile_range +
                     dx * (PROJECTILE_RANGE_MAX - player->projectile_range)) /
                    PROJECTILE_RANGE_MAX;

                int rz = (selev * player->projectile_range +
                          delev * (PROJECTILE_RANGE_MAX -
                                   player->projectile_range)) /
                         PROJECTILE_RANGE_MAX;

                int ry =
                    (sy * player->projectile_range +
                     dy * (PROJECTILE_RANGE_MAX - player->projectile_range)) /
                    PROJECTILE_RANGE_MAX;

                scene_add_sprite(mud->scene,
                                 mud->sprite_projectile +
                                     player->incoming_projectile_sprite,
                                 rx, rz, ry, 32, 32, 0);

                mud->scene_sprite_count++;
            }
        }
    }

    for (int i = 0; i < mud->npc_count; i++) {
        GameCharacter *npc = mud->npcs[i];

        int x = npc->current_x;
        int y = npc->current_y;
        int elevation = -world_get_elevation(mud->world, x, y);

        int sprite_id = scene_add_sprite(mud->scene, 20000 + i, x, elevation, y,
                                         game_data.npcs[npc->npc_id].width,
                                         game_data.npcs[npc->npc_id].height,
                                         i + NPC_FACE_TAG);

        mud->scene_sprite_count++;

        if (npc->current_animation == 8) {
            scene_set_sprite_translate_x(mud->scene, sprite_id, -30);
        } else if (npc->current_animation == 9) {
            scene_set_sprite_translate_x(mud->scene, sprite_id, 30);
        }
    }

    for (int i = 0; i < mud->ground_item_count; i++) {
        int x = mud->ground_items[i].x * MAGIC_LOC + 64;
        int y = mud->ground_items[i].y * MAGIC_LOC + 64;
        int id = mud->ground_items[i].id;
        int elevation =
            -world_get_elevation(mud->world, x, y) - mud->ground_items[i].z;

        scene_add_sprite(mud->scene, 40000 + id, x, elevation, y, 96, 64,
                         i + GROUND_ITEM_FACE_TAG);

        mud->scene_sprite_count++;
    }

    for (int i = 0; i < mud->magic_bubble_count; i++) {
        int x = mud->magic_bubbles[i].x * MAGIC_LOC + 64;
        int y = mud->magic_bubbles[i].y * MAGIC_LOC + 64;
        int type = mud->magic_bubbles[i].type;
        int height = type == 0 ? 256 : 64;

        scene_add_sprite(mud->scene, 50000 + i, x,
                         -world_get_elevation(mud->world, x, y), y, 128, height,
                         i + 50000);

        mud->scene_sprite_count++;
    }
}

void mudclient_draw_game(mudclient *mud) {
#ifdef RENDER_3DS_GL
    mudclient_3ds_gl_frame_start(mud, 1);
#endif

    if (mud->death_screen_timeout != 0) {
        surface_fade_to_black(mud->surface);

        surface_draw_string_centre(
            mud->surface, "Oh dear! You are dead...", mud->surface->width / 2,
            (mud->surface->height - 12) / 2, FONT_BOLD_24, RED);

        mudclient_draw_chat_message_tabs(mud);

        surface_draw(mud->surface);
        return;
    }

    if (mud->show_appearance_change) {
        mudclient_draw_appearance_panel(mud);
        return;
    }

    if (mud->is_sleeping) {
        mudclient_draw_sleep(mud);
        return;
    }

    if (!mud->world->player_alive) {
        return;
    }

    for (int i = 0; i < TERRAIN_COUNT; i++) {
        // TODO this is really slow!
        scene_remove_model(mud->scene,
                           mud->world->roof_models[mud->last_plane_index][i]);

        if (mud->last_plane_index == 0) {
            scene_remove_model(mud->scene, mud->world->wall_models[1][i]);
            scene_remove_model(mud->scene, mud->world->roof_models[1][i]);
            scene_remove_model(mud->scene, mud->world->wall_models[2][i]);
            scene_remove_model(mud->scene, mud->world->roof_models[2][i]);
        }

        if (mud->options->show_roofs) {
            mud->fog_of_war = 1;

            if (mud->last_plane_index == 0 &&
                !world_is_under_roof(mud->world, mud->local_player->current_x,
                                     mud->local_player->current_y)) {
                scene_add_model(
                    mud->scene,
                    mud->world->roof_models[mud->last_plane_index][i]);

                scene_add_model(mud->scene, mud->world->wall_models[1][i]);
                scene_add_model(mud->scene, mud->world->roof_models[1][i]);
                scene_add_model(mud->scene, mud->world->wall_models[2][i]);
                scene_add_model(mud->scene, mud->world->roof_models[2][i]);

                mud->fog_of_war = 0;
            }
        }
    }

    if (!mud->options->lowmem) {
        mudclient_animate_objects(mud);
    }

    mudclient_draw_entity_sprites(mud);

    mud->surface->interlace = 0;

    surface_black_screen(mud->surface);

    mud->surface->interlace = mud->options->interlace;

    /* flickering lights in dungeons */
    if (mud->last_plane_index == 3 && mud->options->flicker) {
        int ambience = 40 + ((float)rand() / (float)RAND_MAX) * 3;
        int diffuse = 40 + ((float)rand() / (float)RAND_MAX) * 7;

        scene_set_light(mud->scene, ambience, diffuse, -50, -10, -50);
    }

    mud->action_bubble_count = 0;
    mud->received_messages_count = 0;
    mud->health_bar_count = 0;
    mud->overworld_text_count = 0;

    if (mud->settings_camera_auto && !mud->fog_of_war) {
        mudclient_auto_rotate_camera(mud);
    }

    if (mud->options->zoom_camera) {
        int clip_far =
            (int)((2400.0f / ZOOM_OUTDOORS) * (float)mud->camera_zoom);

        mud->scene->clip_far_3d = clip_far;
        mud->scene->clip_far_2d = clip_far;
        mud->scene->fog_z_distance = clip_far - 100;
    } else {
        mud->scene->clip_far_3d = 2400;
        mud->scene->clip_far_2d = 2400;
        mud->scene->fog_z_distance = 2300;
    }

    if (mud->options->interlace) {
        mud->scene->clip_far_3d -= 200;
        mud->scene->clip_far_2d -= 200;
        mud->scene->fog_z_distance -= 200;
    }

    /* TODO this should probably be tied with FOV instead */
#ifdef RENDER_SW
    /*
     * Keep the fog roughly "feeling the same" as the vanilla
     * 512x346 client when resized beyond that.
     */
    if (mud->game_height > MUD_VANILLA_HEIGHT) {
        int clip_far = mud->scene->clip_far_3d /
                       (MUD_VANILLA_HEIGHT / (float)mud->game_height);

        mud->scene->clip_far_3d = clip_far;
        mud->scene->clip_far_2d = clip_far;
        mud->scene->fog_z_distance = clip_far - 100;
    }
#endif

    if (!mud->options->fog_of_war) {
        mud->scene->clip_far_3d = 20000;
        mud->scene->clip_far_2d = 20000;
        mud->scene->fog_z_distance = 20000;
    }

    int camera_x = mud->camera_auto_rotate_player_x + mud->camera_rotation_x;
    int camera_z = mud->camera_auto_rotate_player_y + mud->camera_rotation_y;

    int offset_y = 0;

    int is_touch = mudclient_is_touch(mud);

    /* centres the camera for the smaller FOV */
    /* TODO could be an option */
    if (is_touch) {
        offset_y = 100;
    } else if (MUD_IS_COMPACT) {
        offset_y = 75;
    }

    scene_set_camera(
        mud->scene, camera_x,
        -world_get_elevation(mud->world, camera_x, camera_z) - offset_y,
        camera_z, 912, (mud->camera_rotation * 4), 0, (mud->camera_zoom * 2));

    surface_black_screen(mud->surface);

#if defined(RENDER_GL) && !defined(EMSCRIPTEN)
    /*if (mud->options->anti_alias) {
        glEnable(GL_MULTISAMPLE);
    } else {
        glDisable(GL_MULTISAMPLE);
    }*/
#endif

    scene_render(mud->scene);

#if defined(RENDER_GL) || defined(RENDER_3DS_GL)
    surface_gl_draw(mud->surface, GL_DEPTH_DISABLED);
#endif

    mudclient_draw_overhead(mud);

    /* draw the animated X sprite when clicking */
    if (mud->mouse_click_x_step > 0) {
        surface_draw_sprite(
            mud->surface, mud->mouse_click_x_x - 8, mud->mouse_click_x_y - 8,
            mud->sprite_media + 14 + ((24 - mud->mouse_click_x_step) / 6));
    } else if (mud->mouse_click_x_step < 0) {
        surface_draw_sprite(
            mud->surface, mud->mouse_click_x_x - 8, mud->mouse_click_x_y - 8,
            mud->sprite_media + 18 + ((24 + mud->mouse_click_x_step) / 6));
    }

    if (mud->options->display_fps) {
        int offset_x = mud->is_in_wilderness ? 70 : 0;

        char fps[17] = {0};
        sprintf(fps, "Fps: %d", mud->fps);

        surface_draw_string(mud->surface, fps,
                            is_touch ? 9 + offset_x
                                     : mud->surface->width - 62 - offset_x,
                            mud->surface->height - 22, FONT_BOLD_12, YELLOW);
    }

#ifndef REVISION_177
    if (mud->system_update != 0) {
        int seconds = mud->system_update / 50;
        int minutes = seconds / 60;

        seconds %= 60;

        char formatted_update[41] = {0};

        sprintf(formatted_update, "System update in: %d:%02d", minutes,
                seconds);

        surface_draw_string_centre(mud->surface, formatted_update, 256,
                                   mud->game_height - 19, FONT_BOLD_12, YELLOW);
    }
#endif

    if (!mud->loading_area) {
        int wilderness_depth = mudclient_get_wilderness_depth(mud);

        mud->is_in_wilderness = wilderness_depth > 0;

        if (mud->is_in_wilderness) {
            int x = is_touch ? 29 : mud->surface->width - 59;

            surface_draw_sprite(mud->surface, x, mud->surface->height - 68,
                                mud->sprite_media + 13);

            surface_draw_string_centre(mud->surface, "Wilderness", x + 12,
                                       mud->surface->height - 32, FONT_BOLD_12,
                                       YELLOW);

            int wilderness_level = 1 + (wilderness_depth / 6);

            char formatted_level[19] = {0};
            sprintf(formatted_level, "Level: %d", wilderness_level);

            surface_draw_string_centre(mud->surface, formatted_level, x + 12,
                                       mud->surface->height - 19, FONT_BOLD_12,
                                       YELLOW);

            if (mud->show_wilderness_warning == 0) {
                mud->show_wilderness_warning = 2;
            }
        }

        if (mud->options->wilderness_warning &&
            mud->show_wilderness_warning == 0 && wilderness_depth > -10 &&
            wilderness_depth <= 0) {
            mud->show_wilderness_warning = 1;
        }
    }

    mudclient_draw_chat_message_tabs_panel(mud);
    mudclient_draw_ui(mud);

    mud->surface->draw_string_shadow = 0;
    mudclient_draw_chat_message_tabs(mud);

    if (mud->options->status_bars && mudclient_is_touch(mud)) {
        mud->surface->draw_string_shadow = 1;
        mudclient_draw_status_bars(mud);
    }

#ifdef RENDER_GL
    scene_gl_render_transparent_models(mud->scene);
#elif defined(RENDER_3DS_GL)
    scene_3ds_gl_render_transparent_models(mud->scene);
#endif

#if defined(RENDER_GL) || defined(RENDER_3DS_GL)
    surface_gl_draw(mud->surface, GL_DEPTH_ENABLED);
    surface_gl_reset_context(mud->surface);
#else
    surface_draw(mud->surface);
#endif

#if defined(_3DS) && defined(RENDER_SW)
    gfxFlushBuffers();
    gfxSwapBuffers();
#endif
}

void mudclient_draw(mudclient *mud) {
#ifdef EMSCRIPTEN
    if (get_ticks() - last_canvas_check > 1000) {
        if (can_resize() && (get_window_width() != mud->game_width ||
                             get_window_height() != mud->game_height)) {
            mudclient_on_resize(mud);
        }

        last_canvas_check = get_ticks();
    }
#endif

    if (mud->error_loading_data) {
        /* TODO draw error */
        // mud_log("ERROR LOADING DATA\n");
        return;
    }

#ifdef WII
    draw_background(mud->framebuffer, 0);
#endif

#ifdef RENDER_GL
    glClear(GL_DEPTH_BUFFER_BIT);
#endif

    if (mud->logged_in == 0) {
        mud->surface->draw_string_shadow = 0;
        mudclient_draw_login_screens(mud);
    } else if (mud->logged_in == 1) {
        mud->surface->draw_string_shadow = 1;
        mudclient_draw_game(mud);
#ifdef RENDER_GL
#ifdef SDL12
        SDL_GL_SwapBuffers();
#else
        SDL_GL_SwapWindow(mud->gl_window);
#endif
#elif defined(RENDER_3DS_GL)
        mudclient_3ds_gl_frame_end();
#endif
    }
}

#ifdef SDL12
void mudclient_sdl1_on_resize(mudclient *mud, int width, int height) {
    int new_width = width;
    int new_height = height;
#ifdef RENDER_SW
    if ((SDL_SetVideoMode(width, height, 32, SDL_HWSURFACE | SDL_RESIZABLE)) ==
        NULL) {
        return;
    }
#else
    if ((SDL_SetVideoMode(width, height, 32, SDL_OPENGL | SDL_RESIZABLE)) ==
        NULL) {
        return;
    }
#endif
    mud->game_width = new_width;
    mud->game_height = new_height;

    if (mud->surface != NULL) {
        if (mudclient_is_ui_scaled(mud)) {
            mud->surface->width = new_width / 2;
            mud->surface->height = new_height / 2;
        } else {
            mud->surface->width = new_width;
            mud->surface->height = new_height;
        }

        surface_reset_bounds(mud->surface);
    }

    if (mud->scene != NULL) {
#ifdef RENDER_SW
        free(mud->scene->scanlines);
#endif

        // TODO change 12 to bar height - 1
        scene_set_bounds(mud->scene, new_width, new_height - 12);

#ifdef RENDER_GL
        mudclient_update_fov(mud);
#endif
    }

    mudclient_resize(mud);
}
#endif

void mudclient_on_resize(mudclient *mud) {
    int new_width = MUD_WIDTH;
    int new_height = MUD_HEIGHT;

#if !defined(_3DS) && !defined(WII) && !defined(SDL12) && !defined(ROCKBOX)
#ifdef RENDER_GL
    SDL_Window *window = mud->gl_window;
#else
    SDL_Window *window = mud->window;
#endif

#ifdef EMSCRIPTEN
    new_width = get_window_width();
    new_height = get_window_height();
    SDL_SetWindowSize(window, new_width, new_height);
#endif

    SDL_GetWindowSize(window, &new_width, &new_height);
#endif

#ifdef ANDROID
    mudclient_full_width = new_width;
    mudclient_full_height = new_height;

    if (new_width > new_height) {
        new_width =
            roundf(360 * (mudclient_full_width / (float)mudclient_full_height));

        new_height = 360;
    } else {
        new_width = 360;

        new_height =
            roundf(360 * (mudclient_full_height / (float)mudclient_full_width));
    }
#endif

    mud->game_width = new_width;
    mud->game_height = new_height;

    if (mud->surface != NULL) {
        if (mudclient_is_ui_scaled(mud)) {
            mud->surface->width = new_width / 2;
            mud->surface->height = new_height / 2;
        } else {
            mud->surface->width = new_width;
            mud->surface->height = new_height;
        }

        surface_reset_bounds(mud->surface);
    }

    if (mud->scene != NULL) {
#ifdef RENDER_SW
        free(mud->scene->scanlines);

        if (mudclient_is_ui_scaled(mud)) {
            new_width /= 2;
            new_height /= 2;
        }
#endif

        // TODO change 12 to bar height - 1
        scene_set_bounds(mud->scene, new_width, new_height - 12);

#ifdef RENDER_GL
        mudclient_update_fov(mud);
#endif
    }

    mudclient_resize(mud);
}


int mudclient_is_touch(mudclient *mud) {
    (void)(mud);

#ifdef ANDROID
    return 1; // TODO maybe still make this toggleable
#elif defined(EMSCRIPTEN)
    return browser_is_touch();
#else
    return 0;
#endif
}

// TODO open_keyboard
void mudclient_trigger_keyboard(mudclient *mud, char *text, int is_password,
                                int x, int y, int width, int height, int font,
                                int is_centred) {
    (void)mud;
    (void)text;
    (void)is_password;
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    (void)font;
    (void)is_centred;
#ifdef ANDROID
    SDL_StartTextInput();
#elif defined(EMSCRIPTEN)
    int is_scaled = mudclient_is_ui_scaled(mud);

    if (is_scaled) {
        x *= 2;
        y *= 2;
    }

    browser_trigger_keyboard(text, is_password, x, y, width, height, font,
                             is_centred, is_scaled);
#endif
}

void mudclient_run(mudclient *mud) {
#ifdef WII
    draw_background(mud->framebuffers[0], 1);
    draw_background(mud->framebuffers[1], 1);

    mud->active_framebuffer ^= 1;
    mud->framebuffer = mud->framebuffers[mud->active_framebuffer];
#endif

    if (mud->loading_step == 1) {
        mud->loading_step = 2;
        mudclient_load_jagex(mud);
        mudclient_start_game(mud);
        mud->loading_step = 0;

#ifdef EMSCRIPTEN
        mudclient_on_resize(mud);
#endif
    }

    int timing_index = 0;
    int j = 256;
    int delay = 1;
    int i1 = 0;

    for (int i = 0; i < 10; i++) {
        mud->timings[i] = get_ticks();
    }

    while (mud->stop_timeout >= 0) {
        if (mud->stop_timeout > 0) {
            mud->stop_timeout--;

            if (mud->stop_timeout == 0) {
                mudclient_close_connection(mud);
                return;
            }
        }

        int k1 = j;
        int last_delay = delay;

        j = 300;
        delay = 1;

        int time = get_ticks();

        if (mud->timings[timing_index] == 0) {
            j = k1;
            delay = last_delay;
        } else if (time > mud->timings[timing_index]) {
            j = (float)(2560 * mud->target_fps) /
                (float)(time - mud->timings[timing_index]);
        }

        if (j < 25) {
            j = 25;
        }

        if (j > 256) {
            j = 256;
            delay = mud->target_fps - (time - mud->timings[timing_index]) / 10;

            // TODO minimum delay
            if (delay < 10) {
                delay = 10;
            }
        }

        delay_ticks(delay);

        mud->timings[timing_index] = time;
        timing_index = (timing_index + 1) % 10;

        if (delay > 1) {
            for (int i = 0; i < 10; i++) {
                if (mud->timings[i] != 0) {
                    mud->timings[i] += delay;
                }
            }
        }

        int k2 = 0;

        while (i1 < 256) {
            mudclient_poll_events(mud);
            mudclient_handle_inputs(mud);

            i1 += j;

            // TODO magic #
            if (++k2 > 1000) {
                i1 = 0;
                break;
            }
        }

        i1 &= 255;

#ifdef _3DS
        if (!mud->keyboard_open) {
            mudclient_draw(mud);
        }

        mudclient_3ds_flush_audio(mud);

        if (!aptMainLoop()) {
            return;
        }
#else
        mudclient_draw(mud);
#endif

        mud->fps = (j * 1000) / (mud->target_fps * 256);

        mud->mouse_scroll_delta = 0;
    }
}

void mudclient_draw_magic_bubble(mudclient *mud, int x, int y, int width,
                                 int height, int id, float depth) {
    int type = mud->magic_bubbles[id].type;
    int time = mud->magic_bubbles[id].time;

    if (type == 0) {
        /* blue bubble used for teleports */
        int colour = BLUE + time * 5 * 256;

        surface_draw_circle(mud->surface, x + (width / 2), y + (height / 2),
                            20 + time * 2, colour, 255 - time * 5, depth);
    } else if (type == 1) {
        /* red bubble used for telegrab */
        int colour = RED + time * 5 * 256;

        surface_draw_circle(mud->surface, x + (width / 2), y + (height / 2),
                            10 + time, colour, 255 - time * 5, depth);
    }
}

void mudclient_draw_ground_item(mudclient *mud, int x, int y, int width,
                                int height, int id, float depth_top,
                                float depth_bottom) {
    int32_t highlight_colour = highlight_item(id);

    if (highlight_colour != 0 && mud->options->ground_item_text &&
        mud->overworld_text_count < OVERWORLD_TEXT_MAX) {
        struct OverworldText text = {0};

        text.text = game_data.items[id].name;
        text.colour = highlight_colour;
        text.x = x + (width / 2);
        text.y = y - (height / 2);

        mud->overworld_text[mud->overworld_text_count++] = text;
    }

    if (!mud->options->ground_item_models) {
        int picture = game_data.items[id].sprite + mud->sprite_item;
        int mask = game_data.items[id].mask;

        surface_draw_sprite_transform_mask_depth(mud->surface, x, y, width,
                                                 height, picture, mask, 0, 0, 0,
                                                 depth_top, depth_bottom);
    }
}

int mudclient_is_item_equipped(mudclient *mud, int id) {
    for (int i = 0; i < mud->inventory_items_count; i++) {
        if (mud->inventory_item_id[i] == id && mud->inventory_equipped[i]) {
            return 1;
        }
    }

    return 0;
}

int mudclient_get_inventory_count(mudclient *mud, int id) {
    int count = 0;

    for (int i = 0; i < mud->inventory_items_count; i++) {
        if (mud->inventory_item_id[i] == id) {
            if (game_data.items[id].stackable == 1) {
                count++;
            } else {
                count += mud->inventory_item_stack_count[i];
            }
        }
    }

    return count;
}

int mudclient_has_inventory_item(mudclient *mud, int id, int minimum) {
    if (id == FIRE_RUNE_ID &&
        (mudclient_is_item_equipped(mud, FIRE_STAFF_ID) ||
         mudclient_is_item_equipped(mud, FIRE_BATTLESTAFF_ID) ||
         mudclient_is_item_equipped(mud, ENCHANTED_FIRE_BATTLESTAFF_ID))) {
        return 1;
    }

    if (id == WATER_RUNE_ID &&
        (mudclient_is_item_equipped(mud, WATER_STAFF_ID) ||
         mudclient_is_item_equipped(mud, WATER_BATTLESTAFF_ID) ||
         mudclient_is_item_equipped(mud, ENCHANTED_WATER_BATTLESTAFF_ID))) {
        return 1;
    }

    if (id == AIR_RUNE_ID &&
        (mudclient_is_item_equipped(mud, AIR_STAFF_ID) ||
         mudclient_is_item_equipped(mud, AIR_BATTLESTAFF_ID) ||
         mudclient_is_item_equipped(mud, ENCHANTED_AIR_BATTLESTAFF_ID))) {
        return 1;
    }

    if (id == EARTH_RUNE_ID &&
        (mudclient_is_item_equipped(mud, EARTH_STAFF_ID) ||
         mudclient_is_item_equipped(mud, EARTH_BATTLESTAFF_ID) ||
         mudclient_is_item_equipped(mud, ENCHANTED_EARTH_BATTLESTAFF_ID))) {
        return 1;
    }

    return mudclient_get_inventory_count(mud, id) >= minimum;
}

void mudclient_send_logout(mudclient *mud) {
#ifdef ROCKBOX
    (void)mud;
    return;
#endif
    if (mud->logged_in == 0) {
        return;
    }

    if (mud->combat_timeout > 450) {
        mudclient_show_message(mud, "@cya@You can't logout during combat!",
                               MESSAGE_TYPE_GAME);

        return;
    }

    if (mud->combat_timeout > 0) {
        mudclient_show_message(
            mud, "@cya@You can't logout for 10 seconds after combat",
            MESSAGE_TYPE_GAME);

        return;
    }

    packet_stream_new_packet(mud->packet_stream, CLIENT_LOGOUT);
    packet_stream_send_packet(mud->packet_stream);

    mud->logout_timeout = 1000;
}

void mudclient_play_sound(mudclient *mud, char *name) {
    if (!mud->options->members || mud->settings_sound_disabled ||
        mud->options->lowmem) {
        return;
    }

#ifdef _3DS
    if (mud->_3ds_sound_position != -1) {
        return;
    }
#endif

    char file_name[strlen(name) + 5];
    sprintf(file_name, "%s.pcm", name);

    uint32_t offset = get_data_file_offset(file_name, mud->sound_data);

    if (offset == 0) {
        return;
    }

    uint32_t length = get_data_file_length(file_name, mud->sound_data);

    memset(mud->pcm_out, 0, PCM_LENGTH * sizeof(uint16_t));

    ulaw_to_linear(length, (uint8_t *)mud->sound_data + offset, mud->pcm_out);

#ifdef WII
    // ASND_StopVoice(0);

    ASND_SetVoice(0, VOICE_MONO_16BIT_BE, SAMPLE_RATE, 0, mud->pcm_out,
                  length * 2, 127, 127, NULL);
#elif defined(_3DS)
    mud->_3ds_sound_position = 0;
    mud->_3ds_sound_length = length * 2;
#elif defined(SDL_VERSION_ATLEAST)
#if SDL_VERSION_ATLEAST(2, 0, 4)
    SDL_PauseAudio(0);
    SDL_ClearQueuedAudio(1);
    SDL_QueueAudio(1, mud->pcm_out, length * 2);
#endif
#endif
}

int mudclient_walk_to(mudclient *mud, int start_x, int start_y, int x1, int y1,
                      int x2, int y2, int check_objects, int walk_to_action,
                      int first_step) {
#ifdef ROCKBOX
    if (mud->local_player != NULL) {
        start_x = mud->local_player->current_x / MAGIC_LOC;
        start_y = mud->local_player->current_y / MAGIC_LOC;
    }

    if (start_x < 0 || start_x >= REGION_WIDTH ||
        start_y < 0 || start_y >= REGION_HEIGHT ||
        x1 < 0 || x1 >= REGION_WIDTH || x2 < 0 || x2 >= REGION_WIDTH ||
        y1 < 0 || y1 >= REGION_HEIGHT || y2 < 0 || y2 >= REGION_HEIGHT) {
        return 0;
    }
#endif

    int steps = world_route(mud->world, start_x, start_y, x1, y1, x2, y2,
                            mud->walk_path_x, mud->walk_path_y, check_objects);

    if (first_step) {
        if (steps == -1) {
            if (walk_to_action) {
                steps = 1;
                mud->walk_path_x[0] = x1;
                mud->walk_path_y[0] = y1;
            } else {
                return 0;
            }
        }
    } else {
        if (steps == -1) {
            return 0;
        }
    }

#ifdef ROCKBOX
    if (mud->local_player != NULL) {
        if (!mudclient_start_offline_walk(mud, start_x, start_y, steps)) {
            return 0;
        }

        mud->mouse_click_x_step = -24;
        mud->mouse_click_x_x = mud->mouse_x;
        mud->mouse_click_x_y = mud->mouse_y;

        return 1;
    }
#endif

    steps--;
    start_x = mud->walk_path_x[steps];
    start_y = mud->walk_path_y[steps];
    steps--;

    packet_stream_new_packet(mud->packet_stream,
                             walk_to_action ? CLIENT_WALK_ACTION : CLIENT_WALK);

    packet_stream_put_short(mud->packet_stream, start_x + mud->region_x);
    packet_stream_put_short(mud->packet_stream, start_y + mud->region_y);

    if (walk_to_action && steps == -1 && (start_x + mud->region_x) % 5 == 0) {
        steps = 0;
    }

    for (int i = steps; i >= 0 && i > steps - 25; i--) {
        packet_stream_put_byte(mud->packet_stream,
                               mud->walk_path_x[i] - start_x);

        packet_stream_put_byte(mud->packet_stream,
                               mud->walk_path_y[i] - start_y);
    }

    packet_stream_send_packet(mud->packet_stream);

    mud->mouse_click_x_step = -24;
    mud->mouse_click_x_x = mud->mouse_x;
    mud->mouse_click_x_y = mud->mouse_y;

    return 1;
}

void mudclient_walk_to_action_source(mudclient *mud, int start_x, int start_y,
                                     int dest_x, int dest_y, int action) {
    mudclient_walk_to(mud, start_x, start_y, dest_x, dest_y, dest_x, dest_y, 0,
                      action, 1);
}

void mudclient_walk_to_ground_item(mudclient *mud, int start_x, int start_y,
                                   int dest_x, int dest_y, int walk_to_action) {
    if (mudclient_walk_to(mud, start_x, start_y, dest_x, dest_y, dest_x, dest_y,
                          0, walk_to_action, 0)) {
        return;
    }

    mudclient_walk_to(mud, start_x, start_y, dest_x, dest_y, dest_x, dest_y, 1,
                      walk_to_action, 1);
}

void mudclient_walk_to_wall_object(mudclient *mud, int dest_x, int dest_y,
                                   int direction) {
    if (direction == 0) {
        mudclient_walk_to(mud, mud->local_region_x, mud->local_region_y, dest_x,
                          dest_y - 1, dest_x, dest_y, 0, 1, 1);
    } else if (direction == 1) {
        mudclient_walk_to(mud, mud->local_region_x, mud->local_region_y,
                          dest_x - 1, dest_y, dest_x, dest_y, 0, 1, 1);
    } else {
        mudclient_walk_to(mud, mud->local_region_x, mud->local_region_y, dest_x,
                          dest_y, dest_x, dest_y, 1, 1, 1);
    }
}

void mudclient_walk_to_object(mudclient *mud, int x, int y, int direction,
                              int id) {
    int width = 0;
    int height = 0;

    if (direction == DIR_NORTH || direction == DIR_SOUTH) {
        width = game_data.objects[id].width;
        height = game_data.objects[id].height;
    } else {
        height = game_data.objects[id].width;
        width = game_data.objects[id].height;
    }

    if (game_data.objects[id].type == 2 || game_data.objects[id].type == 3) {
        if (direction == DIR_NORTH) {
            x--;
            width++;
        } else if (direction == DIR_WEST) {
            height++;
        } else if (direction == DIR_SOUTH) {
            width++;
        } else if (direction == DIR_EAST) {
            y--;
            height++;
        }

        mudclient_walk_to(mud, mud->local_region_x, mud->local_region_y, x, y,
                          (x + width) - 1, (y + height) - 1, 0, 1, 1);
    } else {
        mudclient_walk_to(mud, mud->local_region_x, mud->local_region_y, x, y,
                          (x + width) - 1, (y + height) - 1, 1, 1, 1);
    }
}

int mudclient_is_ui_scaled(mudclient *mud) {
#if defined(RENDER_GL) || defined(SDL2)
    return mud->options->ui_scale && mud->game_width >= (MUD_WIDTH * 2) &&
           mud->game_height >= (MUD_HEIGHT * 2);
#else
    (void)mud;

    return 0;
#endif
}

void mudclient_format_number_commas(mudclient *mud, int number, char *dest) {
    if (mud->options->number_commas) {
        format_number_commas(number, dest);
    } else {
        sprintf(dest, "%d", number);
    }
}

void mudclient_format_item_amount(mudclient *mud, int item_amount, char *dest) {
    if (mud->options->condense_item_amounts) {
        format_amount_suffix(item_amount, 1, 0, mud->options->number_commas,
                             dest);
    } else {
        mudclient_format_number_commas(mud, item_amount, dest);
    }
}

int mudclient_get_wilderness_depth(mudclient *mud) {
    int wilderness_depth =
        2203 - (mud->local_region_y + mud->plane_height + mud->region_y);

    if (mud->local_region_x + mud->plane_width + mud->region_x >= 2640) {
        wilderness_depth = -50;
    }

    return wilderness_depth;
}

void mudclient_draw_item(mudclient *mud, int x, int y, int slot_width,
                         int slot_height, int item_id) {
    int certificate_item_id = -1;

    if (mud->options->certificate_items) {
        certificate_item_id = get_certificate_item_id(item_id);
    }

    int offset_x = 0;

    if (certificate_item_id != -1) {
        offset_x = -2;
    }

    surface_draw_item(mud->surface, x + offset_x, y, slot_width, slot_height,
                      item_id);

    if (certificate_item_id != -1) {
        int og_width = ITEM_GRID_SLOT_WIDTH - 1;
        int og_height = ITEM_GRID_SLOT_HEIGHT - 2;

        surface_draw_sprite_transform_mask(
            mud->surface, x + 4 + og_width * 0.125f, y + 2 + og_height * 0.125f,
            og_width * 0.75f, og_height * 0.75f,
            mud->surface->mud->sprite_item +
                game_data.items[certificate_item_id].sprite,
            game_data.items[certificate_item_id].mask, 0, 0, 0);
    }
}
#ifdef WIN9X
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine,
                   int nCmdShow) {
    int argc;
    char **argv;

    argv = (char**)CommandLineToArgvW(GetCommandLineW(), &argc);
#else
int main(int argc, char **argv) {
#endif
#ifdef _3DS
    osSetSpeedupEnable(true);
#endif
    srand(0);

    init_utility_global();
    init_surface_global();
    init_world_global();
    /*init_packet_stream_global();*/
    init_stats_tab_global();

    mudclient *mud = malloc(sizeof(mudclient));
    mudclient_new(mud);

#ifdef EMSCRIPTEN
    global_mud = mud;
#endif

    if (argc > 1 && strlen(argv[1]) > 0) {
        mud->options->members = strcmp(argv[1], "members") == 0;
    }

    if (argc > 2) {
        strcpy(mud->server, argv[2]);
    }

    if (argc > 3) {
        mud->port = atoi(argv[3]);
    }

#ifdef REVISION_177
    /* BEGIN INAUTHENTIC COMMAND LINE ARGUMENTS */
    if (argc > 4) {
        strcpy(mud->rsa_exponent, argv[4]);
    }

    if (argc > 5) {
        strcpy(mud->rsa_modulus, argv[5]);
    }
    /* END INAUTHENTIC COMMAND LINE ARGUMENTS */
#endif

    mudclient_start_application(mud, "Runescape by Andrew Gower");
    mudclient_start_application_common(mud);

#ifdef RENDER_3DS_GL
    shaderProgramFree(&mud->surface->_3ds_gl_flat_shader);
    DVLB_Free(mud->surface->_3ds_gl_flat_shader_dvlb);

    C3D_Fini();
#endif

#ifdef _3DS
    linearFree(audio_buffer);
    ndspExit();

    gfxExit();
#endif

    return 0;
}

#ifdef EMSCRIPTEN
void browser_mouse_moved(int x, int y) {
    mudclient_mouse_moved(global_mud, x, y);
}

void browser_key_pressed(int code, int char_code) {
    mudclient_key_pressed(global_mud, code, char_code);
}
#endif
