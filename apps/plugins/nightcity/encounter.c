/***************************************************************************
 * nightcity - encounter system
 ***************************************************************************/

#include "nightcity.h"

static void push_log(char log_lines[][NC_MAX_LINE_CHARS], const char *message)
{
    int i;
    for (i = 0; i < NC_MAX_LOG_LINES - 1; ++i)
        rb->strlcpy(log_lines[i], log_lines[i + 1], NC_MAX_LINE_CHARS);
    rb->strlcpy(log_lines[NC_MAX_LOG_LINES - 1], message, NC_MAX_LINE_CHARS);
}

static int rand_range(int min_value, int max_value)
{
    if (max_value <= min_value)
        return min_value;
    return min_value + (rb->rand() % (max_value - min_value + 1));
}

int nc_run_encounter(struct nc_game_state *state, const struct nc_enemy *enemy)
{
    static const char *actions[] =
    {
        "Attack",
        "Hack",
        "Defend",
        "Medkit",
        "Stim",
    };
    char log_lines[NC_MAX_LOG_LINES][NC_MAX_LINE_CHARS];
    int player_hp;
    int enemy_hp;
    int selection = 0;
    int stim_turns = 0;
    int line_height;
    bool defending = false;

    if (enemy == NULL)
        return NC_ENCOUNTER_LOSE;

    rb->strlcpy(log_lines[0], enemy->intro, NC_MAX_LINE_CHARS);
    log_lines[1][0] = '\0';
    log_lines[2][0] = '\0';
    rb->font_getstringsize("M", NULL, &line_height, FONT_UI);

    player_hp = state->health;
    enemy_hp = enemy->max_health;
    if ((state->flags & NC_FLAG_SAVED_ROOK) && enemy->id == 2)
        enemy_hp -= 4;

    while (1)
    {
        int i;
        int y = 88;
        char buf[64];

        nc_ui_frame("Encounter", enemy->name);
        rb->snprintf(buf, sizeof(buf), "HP %d/%d", player_hp, state->max_health);
        rb->lcd_putsxy(14, 52, buf);
        nc_ui_meter(76, 54, 98, player_hp, state->max_health, player_hp < state->max_health / 3);

        rb->snprintf(buf, sizeof(buf), "%s %d/%d", enemy->name, enemy_hp, enemy->max_health);
        rb->lcd_putsxy(14, 66, buf);
        nc_ui_meter(156, 68, 146, enemy_hp, enemy->max_health, false);

        for (i = 0; i < NC_MAX_LOG_LINES; ++i)
        {
            if (log_lines[i][0] != '\0')
                rb->lcd_putsxy(14, y + i * (line_height + 2), log_lines[i]);
        }

        nc_ui_box(12, LCD_HEIGHT - 104, LCD_WIDTH - 24, 78, false);
        for (i = 0; i < (int)ARRAYLEN(actions); ++i)
        {
            bool selected = (i == selection);
            int row_y = LCD_HEIGHT - 96 + i * (line_height + 4);
            if (selected)
                nc_ui_box(18, row_y - 2, LCD_WIDTH - 36, line_height + 6, true);
            rb->lcd_putsxy(26, row_y, actions[i]);
        }

        nc_ui_footer("Deck", "Act", "Menu");
        nc_ui_update();

        switch (nc_ui_input(TIMEOUT_BLOCK))
        {
            case PLA_LEFT:
                return NC_ENCOUNTER_PANEL;
            case PLA_UP:
            case PLA_CANCEL:
            case PLA_EXIT:
                return NC_ENCOUNTER_MENU;
            case PLA_SCROLL_BACK:
            case PLA_SCROLL_BACK_REPEAT:
                selection = (selection + ARRAYLEN(actions) - 1) % ARRAYLEN(actions);
                continue;
            case PLA_SCROLL_FWD:
            case PLA_SCROLL_FWD_REPEAT:
                selection = (selection + 1) % ARRAYLEN(actions);
                continue;
            case PLA_SELECT:
            case PLA_SELECT_REL:
                break;
            default:
                continue;
        }

        if (selection == 0)
        {
            int damage = rand_range(4, 7) + state->street_cred / 2;
            if (state->cyberware & NC_CYBER_COMBAT_RIG)
                damage += 3;
            if (stim_turns > 0)
                damage += 2;
            enemy_hp -= damage;
            rb->snprintf(buf, sizeof(buf), "You cut in for %d damage.", damage);
            push_log(log_lines, buf);
        }
        else if (selection == 1)
        {
            int chance = 48 + state->ghost_sync * 5 - enemy->firewall / 2;
            int roll = rand_range(0, 99);
            if (state->cyberware & NC_CYBER_GHOSTWALL)
                chance += 15;
            if (stim_turns > 0)
                chance += 5;

            if (roll < chance)
            {
                int damage = rand_range(6, 10) + state->ghost_sync / 2;
                enemy_hp -= damage;
                rb->snprintf(buf, sizeof(buf), "Sable spikes the frame for %d.", damage);
                push_log(log_lines, buf);
            }
            else
            {
                int backlash = rand_range(2, 4);
                player_hp -= backlash;
                state->humanity = MAX(0, state->humanity - 1);
                rb->snprintf(buf, sizeof(buf), "Hack rebounds. You take %d.", backlash);
                push_log(log_lines, buf);
            }
        }
        else if (selection == 2)
        {
            defending = true;
            push_log(log_lines, "You brace and tighten the angle.");
        }
        else if (selection == 3)
        {
            if (state->medkits <= 0)
            {
                push_log(log_lines, "No medkits left.");
                continue;
            }
            --state->medkits;
            player_hp = MIN(state->max_health, player_hp + 10);
            push_log(log_lines, "Medkit burned. You steady out.");
        }
        else
        {
            if (state->stims <= 0)
            {
                push_log(log_lines, "No stims left.");
                continue;
            }
            --state->stims;
            stim_turns = 3;
            state->ghost_sync = MIN(NC_STAT_MAX, state->ghost_sync + 1);
            push_log(log_lines, "Stim hit. Vision sharpens, pulse spikes.");
        }

        if (enemy_hp <= 0)
        {
            player_hp = MAX(1, player_hp);
            state->health = MIN(state->max_health, player_hp);
            state->credits = MIN(999, state->credits + enemy->payout);
            state->street_cred = MIN(NC_STAT_MAX, state->street_cred + enemy->street_cred_reward);
            state->corp_heat = MIN(NC_STAT_MAX, state->corp_heat + enemy->heat_delta);
            return NC_ENCOUNTER_WIN;
        }

        {
            int enemy_damage = rand_range(enemy->attack_min, enemy->attack_max) + state->corp_heat / 3;
            if (defending)
            {
                enemy_damage = MAX(1, enemy_damage - 3);
                if (state->cyberware & NC_CYBER_COMBAT_RIG)
                    enemy_damage = MAX(1, enemy_damage - 1);
            }
            player_hp -= enemy_damage;
            rb->snprintf(buf, sizeof(buf), "%s hits for %d.", enemy->name, enemy_damage);
            push_log(log_lines, buf);
        }

        defending = false;
        if (stim_turns > 0)
            --stim_turns;

        if (player_hp <= 0)
        {
            state->health = 1;
            return NC_ENCOUNTER_LOSE;
        }
    }
}
