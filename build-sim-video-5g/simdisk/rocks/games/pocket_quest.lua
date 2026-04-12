require "actions"

local SAVE_FILE = rb.PLUGIN_GAMES_DATA_DIR .. "/pocket_quest.sav"
local LCD_W = rb.LCD_WIDTH
local LCD_H = rb.LCD_HEIGHT

if rb.lcd_setfont then
    rb.lcd_setfont(rb.FONT_SYSFIXED)
end

local FONT = rb.FONT_SYSFIXED
local _, CHAR_W, CHAR_H = rb.font_getstringsize("W", FONT)
if not CHAR_W or CHAR_W < 1 then
    FONT = rb.FONT_UI
    _, CHAR_W, CHAR_H = rb.font_getstringsize("W", FONT)
end
CHAR_W = math.max(CHAR_W or 6, 6)
CHAR_H = math.max(CHAR_H or 8, 8)

local function rgb(r, g, b)
    if rb.lcd_rgbpack then
        return rb.lcd_rgbpack(r, g, b)
    end
    if (r + g + b) > 382 then
        return rb.LCD_DEFAULT_FG
    end
    return rb.LCD_DEFAULT_BG
end

local COLORS = {
    bg = rgb(14, 18, 28),
    panel = rgb(28, 36, 52),
    panel_alt = rgb(22, 28, 42),
    line = rgb(82, 95, 118),
    text = rgb(252, 244, 230),
    dim = rgb(176, 188, 204),
    shadow = rgb(0, 0, 0),
    alert = rgb(255, 96, 102),
    gold = rgb(255, 219, 94),
    hp = rgb(110, 227, 124),
    hunger = rgb(255, 194, 78),
    energy = rgb(108, 200, 255),
    mood = rgb(255, 124, 182),
    hygiene = rgb(156, 232, 240),
    leaf = rgb(118, 224, 115),
    ember = rgb(255, 136, 82),
    tide = rgb(110, 178, 255),
    enemy = rgb(255, 120, 134),
    tab = rgb(44, 54, 78)
}

local seed = rb.current_tick()
if os and os.time then
    seed = seed + os.time()
end
math.randomseed(seed)
for _ = 1, 4 do
    math.random()
end

local act = rb.actions
local PAGE_NAMES = {"Habitat", "Stats", "Journal"}
local ACTIONS = {
    {label = "Feed", hint = "Restore fullness and bond."},
    {label = "Play", hint = "Lift mood and closeness."},
    {label = "Clean", hint = "Wash off grime and risk."},
    {label = "Rest", hint = "Recover energy and health."},
    {label = "Train", hint = "Gain XP for evolution."},
    {label = "Explore", hint = "Find loot or start a battle."},
    {label = "Market", hint = "Spend gold on supplies."},
    {label = "Heal", hint = "Use a med kit."}
}

local STARTERS = {
    {
        id = "leaf",
        egg = "Verdant Egg",
        names = {"Sprig", "Bramble", "Canopy Rex"},
        accent = COLORS.leaf
    },
    {
        id = "ember",
        egg = "Cinder Egg",
        names = {"Cindlet", "Ashfang", "Solarclaw"},
        accent = COLORS.ember
    },
    {
        id = "tide",
        egg = "Tidal Egg",
        names = {"Drizzle", "Wavehorn", "Starfin Sage"},
        accent = COLORS.tide
    }
}

local PET_SPRITES = {
    leaf = {
        {
            "....22......",
            "...2211.....",
            "..211111....",
            ".2113311....",
            ".11111111...",
            ".11111111...",
            ".11111111...",
            ".11111111...",
            "..111111....",
            "...1111.....",
            "....11......",
            "...2..2....."
        },
        {
            "....22......",
            "...22112....",
            "..2111111...",
            ".211133112..",
            ".111111111..",
            "21111111112.",
            ".111111111..",
            ".111111111..",
            "..1111111...",
            "...111111...",
            "..2.1..1.2..",
            ".2........2."
        },
        {
            "...2222.....",
            "..2211122...",
            ".211111112..",
            "21113311112.",
            "21111111112.",
            "111111111111",
            "111111111111",
            ".1111111111.",
            "..11111111..",
            ".2111111112.",
            "2..11..11..2",
            "...2....2..."
        }
    },
    ember = {
        {
            "...2..2.....",
            "..221122....",
            ".21111112...",
            ".11133111...",
            ".11111111...",
            ".11111111...",
            "..111111....",
            "...1111.....",
            "..111111....",
            ".2..11..2...",
            "....22......",
            "...2..2....."
        },
        {
            "..2....2....",
            ".221122112...",
            ".211111112..",
            "21113311112.",
            "11111111111.",
            ".111111111..",
            "..1111111...",
            "...11111....",
            "..1111111...",
            ".2.111111.2.",
            "...22..22...",
            "..2......2.."
        },
        {
            "..22..22....",
            ".221122112...",
            "21111111112.",
            "21113311112.",
            "111111111111",
            ".1111111111.",
            "..11111111..",
            "...111111...",
            "..11111111..",
            ".2111111112.",
            "2.22....22.2",
            "..2......2.."
        }
    },
    tide = {
        {
            ".....22.....",
            "...221122...",
            "..21111112..",
            ".2111331112.",
            ".1111111112.",
            ".1111111112.",
            "..11111112..",
            "...111112...",
            "..2111112...",
            ".2.11112....",
            "...22..2....",
            ".....22....."
        },
        {
            ".....22.....",
            "...221122...",
            "..21111112..",
            ".2111331112.",
            "21111111112.",
            "111111111112",
            ".1111111112.",
            "..11111112..",
            ".211111112..",
            "2.111111.2..",
            "...22..22...",
            ".....22....."
        },
        {
            "....2222....",
            "..2211122...",
            ".211111112..",
            "21113311112.",
            "21111111112.",
            "111111111111",
            "111111111111",
            ".1111111112.",
            "..11111112..",
            ".211111112..",
            "2.211..112.2",
            "...22..22..."
        }
    }
}

local ENEMY_SPRITES = {
    slime = {
        "............",
        "...22222....",
        "..2111112...",
        ".211111112..",
        ".111133111..",
        ".111111111..",
        ".111111111..",
        "..1111111...",
        "...11111....",
        "..2.....2...",
        "............",
        "............"
    },
    bat = {
        "2..2....2..2",
        "22.22..22.22",
        ".2111222112.",
        "..11111111..",
        ".1111331111.",
        ".1111111111.",
        "..11111111..",
        "...111111...",
        "..2.1..1.2..",
        "............",
        "............",
        "............"
    },
    beetle = {
        "....22......",
        "...2112.....",
        "..211112....",
        "..111111....",
        ".11133111...",
        ".11111111...",
        ".11111111...",
        "..211112....",
        "..2.11.2....",
        ".2..11..2...",
        "....22......",
        "...2..2....."
    }
}

local function clamp(value, min_value, max_value)
    if value < min_value then
        return min_value
    elseif value > max_value then
        return max_value
    end
    return value
end

local function wrap(index, count)
    if index < 1 then
        return count
    elseif index > count then
        return 1
    end
    return index
end

local function starter_from_id(id)
    for _, starter in ipairs(STARTERS) do
        if starter.id == id then
            return starter
        end
    end
    return STARTERS[1]
end

local function pet_title(state)
    local starter = starter_from_id(state.starter)
    return starter.names[clamp(state.stage, 1, 3)]
end

local function pet_color(state)
    return starter_from_id(state.starter).accent
end

local function max_xp(state)
    return 12 + ((state.level - 1) * 9)
end

local function stat_average(state)
    return math.floor((state.health + state.fullness + state.energy +
        state.mood + state.hygiene) / 5)
end

local function creature_mood_line(state)
    if state.sick == 1 then
        return "Needs care right away."
    elseif stat_average(state) >= 80 then
        return "Sparkling and adventure-ready."
    elseif state.energy < 22 then
        return "Yawning and drifting."
    elseif state.fullness < 24 then
        return "Rumbling for a snack."
    elseif state.hygiene < 25 then
        return "Could use a bath."
    elseif state.mood > 70 then
        return "Wants to show off."
    end
    return "Waiting for your next move."
end

local function rank_name(state)
    if state.stage == 1 then
        return "Hatchling"
    elseif state.stage == 2 then
        return "Guardian"
    end
    return "Legend"
end

local function add_log(state, message)
    table.insert(state.log, 1, message)
    while #state.log > 7 do
        table.remove(state.log)
    end
end

local function encode_value(value)
    return tostring(value)
end

local SAVE_KEYS = {
    "starter", "stage", "day", "turn_clock", "level", "xp", "bond", "gold",
    "food", "meds", "health", "fullness", "energy", "mood", "hygiene",
    "wins", "losses", "sick"
}

local function normalize_state(state)
    state.stage = clamp(tonumber(state.stage or 1), 1, 3)
    state.day = math.max(tonumber(state.day or 1), 1)
    state.turn_clock = clamp(tonumber(state.turn_clock or 0), 0, 3)
    state.level = math.max(tonumber(state.level or 1), 1)
    state.xp = clamp(tonumber(state.xp or 0), 0, max_xp(state))
    state.bond = clamp(tonumber(state.bond or 10), 0, 100)
    state.gold = math.max(tonumber(state.gold or 0), 0)
    state.food = math.max(tonumber(state.food or 0), 0)
    state.meds = math.max(tonumber(state.meds or 0), 0)
    state.health = clamp(tonumber(state.health or 75), 0, 100)
    state.fullness = clamp(tonumber(state.fullness or 70), 0, 100)
    state.energy = clamp(tonumber(state.energy or 72), 0, 100)
    state.mood = clamp(tonumber(state.mood or 65), 0, 100)
    state.hygiene = clamp(tonumber(state.hygiene or 60), 0, 100)
    state.wins = math.max(tonumber(state.wins or 0), 0)
    state.losses = math.max(tonumber(state.losses or 0), 0)
    state.sick = clamp(tonumber(state.sick or 0), 0, 1)
    state.page = clamp(tonumber(state.page or 1), 1, #PAGE_NAMES)
    state.selected_action = clamp(tonumber(state.selected_action or 1), 1, #ACTIONS)
    state.log = state.log or {}
end

local function save_state(state)
    local file = io.open(SAVE_FILE, "w")
    if not file then
        return false
    end

    for _, key in ipairs(SAVE_KEYS) do
        file:write(key, "=", encode_value(state[key]), "\n")
    end
    file:close()
    return true
end

local function load_state()
    local file = io.open(SAVE_FILE, "r")
    if not file then
        return nil
    end

    local state = {}
    for line in file:lines() do
        local key, value = line:match("^(%w+)=(.-)$")
        if key and value then
            local number = tonumber(value)
            state[key] = number or value
        end
    end
    file:close()

    if not state.starter then
        return nil
    end

    state.log = {
        "Save loaded for Day " .. tostring(state.day or 1) .. ".",
        pet_title(state) .. " wakes up in the habitat."
    }
    state.page = 1
    state.selected_action = 1
    normalize_state(state)
    return state
end

local function new_state(starter)
    local state = {
        starter = starter.id,
        stage = 1,
        day = 1,
        turn_clock = 0,
        level = 1,
        xp = 0,
        bond = 12,
        gold = 10,
        food = 4,
        meds = 1,
        health = 78,
        fullness = 72,
        energy = 74,
        mood = 66,
        hygiene = 62,
        wins = 0,
        losses = 0,
        sick = 0,
        page = 1,
        selected_action = 1,
        log = {
            starter.names[1] .. " hatched from the " .. starter.egg .. ".",
            "Your new partner is ready to grow."
        }
    }
    normalize_state(state)
    return state
end

local function choose_starter()
    local items = {}
    for _, starter in ipairs(STARTERS) do
        table.insert(items, starter.egg)
    end

    local choice = rb.do_menu("Choose Egg", items, nil, false)
    if choice == nil or choice < 0 then
        return nil
    end
    return new_state(STARTERS[choice + 1])
end

local function set_bg()
    if rb.lcd_set_background then
        rb.lcd_set_background(COLORS.bg)
    end
    if rb.lcd_set_foreground then
        rb.lcd_set_foreground(COLORS.text)
    end
end

local function clear_screen()
    set_bg()
    rb.lcd_clear_display()
end

local function text_size(message)
    local _, width, height = rb.font_getstringsize(message, FONT)
    return width or (#message * CHAR_W), height or CHAR_H
end

local function draw_text(x, y, message, color)
    rb.lcd_set_foreground(color or COLORS.text)
    rb.lcd_putsxy(x, y, message)
end

local function draw_center(y, message, color)
    local width = text_size(message)
    local x = math.max(4, math.floor((LCD_W - width) / 2))
    draw_text(x, y, message, color)
end

local function draw_panel(x, y, width, height, title)
    rb.lcd_set_foreground(COLORS.panel)
    rb.lcd_fillrect(x, y, width, height)
    rb.lcd_set_foreground(COLORS.line)
    rb.lcd_drawrect(x, y, width, height)
    if title then
        rb.lcd_set_foreground(COLORS.tab)
        rb.lcd_fillrect(x + 1, y + 1, width - 2, CHAR_H + 4)
        draw_text(x + 6, y + 3, title, COLORS.text)
        rb.lcd_set_foreground(COLORS.line)
        rb.lcd_hline(x + 1, x + width - 2, y + CHAR_H + 5)
    end
end

local function draw_bar(x, y, width, label, value, color)
    local fill = math.floor((clamp(value, 0, 100) * (width - 2)) / 100)
    draw_text(x, y, label .. " " .. tostring(value), COLORS.dim)
    rb.lcd_set_foreground(COLORS.panel_alt)
    rb.lcd_fillrect(x, y + CHAR_H, width, 8)
    rb.lcd_set_foreground(COLORS.line)
    rb.lcd_drawrect(x, y + CHAR_H, width, 8)
    rb.lcd_set_foreground(color)
    rb.lcd_fillrect(x + 1, y + CHAR_H + 1, fill, 6)
end

local function draw_sprite(sprite, x, y, scale, body_color, accent_color)
    local eye_color = COLORS.text
    for row = 1, #sprite do
        local line = sprite[row]
        for col = 1, #line do
            local code = line:sub(col, col)
            if code ~= "." then
                local color = body_color
                if code == "2" then
                    color = accent_color
                elseif code == "3" then
                    color = eye_color
                elseif code == "4" then
                    color = COLORS.shadow
                end
                rb.lcd_set_foreground(color)
                rb.lcd_fillrect(x + ((col - 1) * scale), y + ((row - 1) * scale), scale, scale)
            end
        end
    end
end

local function care_label(value)
    if value >= 85 then
        return "Peak"
    elseif value >= 65 then
        return "Strong"
    elseif value >= 40 then
        return "Steady"
    elseif value >= 20 then
        return "Shaky"
    end
    return "Critical"
end

local function maybe_evolve(state)
    local average = stat_average(state)
    local old_stage = state.stage

    if state.stage == 1 and state.level >= 3 and state.bond >= 24 and average >= 45 then
        state.stage = 2
        state.health = clamp(state.health + 14, 0, 100)
        state.energy = clamp(state.energy + 12, 0, 100)
    elseif state.stage == 2 and state.level >= 7 and state.bond >= 55 and
        average >= 60 and state.wins >= 3 then
        state.stage = 3
        state.health = clamp(state.health + 18, 0, 100)
        state.energy = clamp(state.energy + 14, 0, 100)
    end

    if state.stage ~= old_stage then
        add_log(state, pet_title(state) .. " evolved into a new form.")
        rb.splash(rb.HZ * 2, pet_title(state) .. " evolved!")
    end
end

local function gain_xp(state, amount)
    state.xp = state.xp + amount
    while state.xp >= max_xp(state) do
        state.xp = state.xp - max_xp(state)
        state.level = state.level + 1
        state.health = clamp(state.health + 8, 0, 100)
        state.energy = clamp(state.energy + 8, 0, 100)
        state.mood = clamp(state.mood + 5, 0, 100)
        add_log(state, pet_title(state) .. " reached level " .. tostring(state.level) .. ".")
    end
    maybe_evolve(state)
end

local function collapse_check(state)
    if state.health <= 0 then
        state.health = 36
        state.energy = math.max(state.energy, 30)
        state.fullness = math.max(state.fullness, 28)
        state.mood = math.max(state.mood - 8, 18)
        state.losses = state.losses + 1
        state.sick = 0
        add_log(state, pet_title(state) .. " collapsed and had to be revived.")
        rb.splash(rb.HZ, "Gentle care needed.")
    end
end

local function apply_turn(state, turns)
    for _ = 1, turns do
        state.turn_clock = state.turn_clock + 1
        state.fullness = state.fullness - 4
        state.hygiene = state.hygiene - 2
        state.energy = state.energy - 2
        state.mood = state.mood - 1

        if state.turn_clock >= 4 then
            state.turn_clock = 0
            state.day = state.day + 1
            add_log(state, "Day " .. tostring(state.day) .. " begins.")
        end

        if state.fullness < 25 then
            state.health = state.health - 3
        end
        if state.hygiene < 20 then
            state.health = state.health - 2
        end
        if state.energy < 16 then
            state.mood = state.mood - 2
        end
        if state.mood < 20 then
            state.health = state.health - 1
        end
    end

    state.sick = 0
    if state.hygiene < 18 or state.health < 24 then
        state.sick = 1
        state.health = state.health - 2
    end

    state.health = clamp(state.health, 0, 100)
    state.fullness = clamp(state.fullness, 0, 100)
    state.energy = clamp(state.energy, 0, 100)
    state.mood = clamp(state.mood, 0, 100)
    state.hygiene = clamp(state.hygiene, 0, 100)
    state.bond = clamp(state.bond, 0, 100)

    collapse_check(state)
    maybe_evolve(state)
    save_state(state)
end

local function feed_action(state)
    if state.food <= 0 then
        if state.gold >= 4 then
            state.gold = state.gold - 4
            state.food = 1
            add_log(state, "Bought a travel snack for 4g.")
        else
            add_log(state, "No food left in the habitat.")
            rb.splash(rb.HZ, "Pantry is empty.")
            return
        end
    end

    state.food = state.food - 1
    state.fullness = clamp(state.fullness + 26, 0, 100)
    state.mood = clamp(state.mood + 5, 0, 100)
    state.health = clamp(state.health + 4, 0, 100)
    state.bond = clamp(state.bond + 3, 0, 100)
    gain_xp(state, 2)
    add_log(state, "Shared a warm meal with " .. pet_title(state) .. ".")
    apply_turn(state, 1)
end

local function play_action(state)
    if state.energy < 12 then
        add_log(state, pet_title(state) .. " is too sleepy to play.")
        rb.splash(rb.HZ, "Too sleepy.")
        return
    end

    state.energy = clamp(state.energy - 8, 0, 100)
    state.fullness = clamp(state.fullness - 3, 0, 100)
    state.hygiene = clamp(state.hygiene - 3, 0, 100)
    state.mood = clamp(state.mood + 15, 0, 100)
    state.bond = clamp(state.bond + 6, 0, 100)
    gain_xp(state, 4)
    add_log(state, "You played until the room was full of sparks.")
    apply_turn(state, 1)
end

local function clean_action(state)
    state.hygiene = clamp(state.hygiene + 32, 0, 100)
    state.mood = clamp(state.mood + 2, 0, 100)
    state.health = clamp(state.health + 3, 0, 100)
    if state.sick == 1 and state.hygiene > 32 then
        state.sick = 0
    end
    gain_xp(state, 2)
    add_log(state, pet_title(state) .. " looks fresh and polished.")
    apply_turn(state, 1)
end

local function rest_action(state)
    state.energy = clamp(state.energy + 28, 0, 100)
    state.health = clamp(state.health + 8, 0, 100)
    state.mood = clamp(state.mood + 4, 0, 100)
    state.fullness = clamp(state.fullness - 2, 0, 100)
    add_log(state, pet_title(state) .. " curled up for a quick rest.")
    apply_turn(state, 1)
end

local function train_action(state)
    if state.energy < 18 or state.fullness < 18 then
        add_log(state, pet_title(state) .. " needs food or rest before training.")
        rb.splash(rb.HZ, "Needs food or rest.")
        return
    end

    state.energy = clamp(state.energy - 15, 0, 100)
    state.fullness = clamp(state.fullness - 8, 0, 100)
    state.hygiene = clamp(state.hygiene - 5, 0, 100)
    state.mood = clamp(state.mood - 2, 0, 100)
    gain_xp(state, 10)
    add_log(state, "Training paid off with new instincts.")
    apply_turn(state, 1)
end

local function market_action(state)
    local choice = rb.do_menu("Market", {
        "Snack pack x2 - 6g",
        "Med kit - 9g",
        "Leave"
    }, nil, false)

    if choice == 0 then
        if state.gold >= 6 then
            state.gold = state.gold - 6
            state.food = state.food + 2
            add_log(state, "Bought a snack pack.")
            apply_turn(state, 1)
        else
            add_log(state, "Not enough gold for snacks.")
            rb.splash(rb.HZ, "Need 6g.")
        end
    elseif choice == 1 then
        if state.gold >= 9 then
            state.gold = state.gold - 9
            state.meds = state.meds + 1
            add_log(state, "Bought a med kit.")
            apply_turn(state, 1)
        else
            add_log(state, "Not enough gold for a med kit.")
            rb.splash(rb.HZ, "Need 9g.")
        end
    end
end

local function heal_action(state)
    if state.meds <= 0 then
        add_log(state, "Your med pouch is empty.")
        rb.splash(rb.HZ, "No med kit.")
        return
    end

    state.meds = state.meds - 1
    state.health = clamp(state.health + 24, 0, 100)
    state.energy = clamp(state.energy + 5, 0, 100)
    state.mood = clamp(state.mood + 2, 0, 100)
    state.sick = 0
    gain_xp(state, 1)
    add_log(state, pet_title(state) .. " perked up after treatment.")
    apply_turn(state, 1)
end

local function make_enemy(state)
    local pools = {
        {
            {name = "Dust Slime", sprite = "slime"},
            {name = "Bandit Bat", sprite = "bat"},
            {name = "Gear Beetle", sprite = "beetle"}
        },
        {
            {name = "Shade Bat", sprite = "bat"},
            {name = "Cinder Beetle", sprite = "beetle"},
            {name = "Iron Slime", sprite = "slime"}
        },
        {
            {name = "Night Wyrm", sprite = "bat"},
            {name = "Titan Beetle", sprite = "beetle"},
            {name = "Void Slime", sprite = "slime"}
        }
    }
    local pool = pools[clamp(state.stage, 1, 3)]
    local pick = pool[math.random(#pool)]
    local hp = 18 + (state.level * 5) + (state.stage * 6) + math.random(0, 8)
    local power = 4 + state.level + (state.stage * 2) + math.random(0, 2)

    return {
        name = pick.name,
        sprite = pick.sprite,
        hp = hp,
        max_hp = hp,
        power = power
    }
end

local function draw_battle(state, enemy, hero_hp, hero_max_hp, selected, lines)
    clear_screen()
    draw_panel(8, 8, LCD_W - 16, LCD_H - 16, "Battle")

    draw_text(20, 24, pet_title(state) .. " Lv" .. tostring(state.level), pet_color(state))
    draw_text(LCD_W - (14 * CHAR_W), 24, enemy.name, COLORS.enemy)

    draw_bar(20, 40, 120, "HP", math.floor((hero_hp * 100) / hero_max_hp), COLORS.hp)
    draw_bar(LCD_W - 140, 40, 120, "FOE", math.floor((enemy.hp * 100) / enemy.max_hp), COLORS.alert)

    draw_sprite(PET_SPRITES[state.starter][state.stage], 28, 86, 4, pet_color(state), COLORS.text)
    draw_sprite(ENEMY_SPRITES[enemy.sprite], LCD_W - 90, 86, 4, COLORS.enemy, COLORS.gold)

    draw_text(20, 148, lines[1], COLORS.text)
    draw_text(20, 160, lines[2], COLORS.dim)

    local options = {"Strike", "Burst", "Snack", "Run"}
    for index, option in ipairs(options) do
        local x = 22 + (((index - 1) % 2) * 96)
        local y = 188 + (math.floor((index - 1) / 2) * (CHAR_H + 8))
        local color = COLORS.dim
        if index == selected then
            rb.lcd_set_foreground(COLORS.tab)
            rb.lcd_fillrect(x - 4, y - 2, 78, CHAR_H + 6)
            color = COLORS.text
        end
        draw_text(x, y, option, color)
    end

    draw_text(208, 192, "Up/Down choose", COLORS.dim)
    draw_text(208, 204, "Select act", COLORS.dim)
    draw_text(208, 216, "Cancel run", COLORS.dim)
    rb.lcd_update()
end

local function run_battle(state)
    local enemy = make_enemy(state)
    local hero_max_hp = 26 + (state.level * 8) + (state.stage * 7) + math.floor(state.health / 2)
    local hero_hp = hero_max_hp
    local selected = 1
    local lines = {
        enemy.name .. " blocks the path.",
        "Pick a move."
    }

    while true do
        draw_battle(state, enemy, hero_hp, hero_max_hp, selected, lines)
        local action = rb.get_plugin_action(-1)

        if action == act.PLA_UP or action == act.PLA_UP_REPEAT then
            selected = wrap(selected - 1, 4)
        elseif action == act.PLA_DOWN or action == act.PLA_DOWN_REPEAT then
            selected = wrap(selected + 1, 4)
        elseif action == act.PLA_CANCEL or action == act.PLA_EXIT then
            if math.random(100) <= 45 then
                state.mood = clamp(state.mood - 2, 0, 100)
                add_log(state, "You escaped from " .. enemy.name .. ".")
                apply_turn(state, 1)
                return
            end
            lines = {"Could not escape.", enemy.name .. " closes in."}
        elseif action == act.PLA_SELECT then
            local player_message = ""
            local enemy_message = ""
            local player_acted = true

            if selected == 1 then
                local damage = math.random(6, 10) + state.level + state.stage + math.floor(state.bond / 18)
                enemy.hp = enemy.hp - damage
                player_message = "Strike lands for " .. tostring(damage) .. "."
            elseif selected == 2 then
                if state.energy < 10 then
                    player_message = "Not enough energy for Burst."
                    enemy_message = "Catch your breath."
                    player_acted = false
                else
                    state.energy = clamp(state.energy - 10, 0, 100)
                    local damage = math.random(10, 16) + (state.level * 2) + (state.stage * 2)
                    enemy.hp = enemy.hp - damage
                    player_message = "Burst hits for " .. tostring(damage) .. "."
                end
            elseif selected == 3 then
                if state.food <= 0 then
                    player_message = "No snack left."
                    enemy_message = "Bag is empty."
                    player_acted = false
                else
                    state.food = state.food - 1
                    hero_hp = clamp(hero_hp + 16, 0, hero_max_hp)
                    state.mood = clamp(state.mood + 4, 0, 100)
                    player_message = "Snack restores spirit."
                end
            else
                if math.random(100) <= 58 then
                    state.mood = clamp(state.mood - 1, 0, 100)
                    add_log(state, "Slipped away from " .. enemy.name .. ".")
                    apply_turn(state, 1)
                    return
                end
                player_message = "Run failed."
            end

            if enemy.hp <= 0 then
                local gold_gain = 4 + state.stage + math.random(0, 4)
                local xp_gain = 6 + state.level + math.random(0, 5)
                state.gold = state.gold + gold_gain
                state.wins = state.wins + 1
                if math.random(100) <= 26 then
                    state.food = state.food + 1
                    enemy_message = "Found a ration on the trail."
                elseif math.random(100) <= 16 then
                    state.meds = state.meds + 1
                    enemy_message = "Recovered a spare med kit."
                else
                    enemy_message = "Victory feels good."
                end
                gain_xp(state, xp_gain)
                state.health = clamp(state.health + 2, 0, 100)
                add_log(state, "Defeated " .. enemy.name .. " for " .. tostring(gold_gain) .. "g.")
                lines = {player_message, enemy_message}
                draw_battle(state, enemy, hero_hp, hero_max_hp, selected, lines)
                rb.sleep(rb.HZ)
                apply_turn(state, 2)
                return
            end

            if player_acted then
                local damage = math.random(enemy.power, enemy.power + 4) - math.floor(state.stage / 2)
                damage = math.max(damage, 2)
                if state.mood >= 75 and math.random(100) <= 30 then
                    damage = math.max(damage - 3, 1)
                end
                hero_hp = hero_hp - damage
                state.health = clamp(state.health - math.max(1, math.floor(damage / 2)), 0, 100)
                enemy_message = enemy.name .. " hits for " .. tostring(damage) .. "."
                if hero_hp <= 0 or state.health <= 0 then
                    state.losses = state.losses + 1
                    state.health = math.max(18, state.health)
                    state.energy = clamp(state.energy - 12, 0, 100)
                    state.mood = clamp(state.mood - 9, 0, 100)
                    add_log(state, enemy.name .. " forced a retreat.")
                    lines = {player_message, enemy_message}
                    draw_battle(state, enemy, math.max(hero_hp, 1), hero_max_hp, selected, lines)
                    rb.sleep(rb.HZ)
                    apply_turn(state, 2)
                    return
                end
            end

            lines = {player_message, enemy_message}
        end

        rb.yield()
    end
end

local function explore_action(state)
    if state.energy < 16 or state.health < 18 then
        add_log(state, pet_title(state) .. " is too worn out to explore.")
        rb.splash(rb.HZ, "Needs rest first.")
        return
    end

    state.energy = clamp(state.energy - 8, 0, 100)
    state.fullness = clamp(state.fullness - 4, 0, 100)
    state.hygiene = clamp(state.hygiene - 3, 0, 100)

    local roll = math.random(100)
    if roll <= 56 then
        add_log(state, "An enemy lurks beyond the habitat.")
        run_battle(state)
    elseif roll <= 76 then
        local gold = math.random(4, 9)
        state.gold = state.gold + gold
        state.mood = clamp(state.mood + 3, 0, 100)
        gain_xp(state, 5)
        add_log(state, "Scouted a hidden cache worth " .. tostring(gold) .. "g.")
        apply_turn(state, 2)
    elseif roll <= 90 then
        state.food = state.food + 1
        state.mood = clamp(state.mood + 7, 0, 100)
        state.bond = clamp(state.bond + 4, 0, 100)
        gain_xp(state, 4)
        add_log(state, "Found a friendly traveler with spare food.")
        apply_turn(state, 2)
    else
        state.health = clamp(state.health - 10, 0, 100)
        state.mood = clamp(state.mood - 5, 0, 100)
        add_log(state, "A trap snapped shut on the trail.")
        apply_turn(state, 2)
    end
end

local function perform_action(state)
    local label = ACTIONS[state.selected_action].label
    if label == "Feed" then
        feed_action(state)
    elseif label == "Play" then
        play_action(state)
    elseif label == "Clean" then
        clean_action(state)
    elseif label == "Rest" then
        rest_action(state)
    elseif label == "Train" then
        train_action(state)
    elseif label == "Explore" then
        explore_action(state)
    elseif label == "Market" then
        market_action(state)
    elseif label == "Heal" then
        heal_action(state)
    end
end

local function draw_tabs(page)
    local x = 12
    for index, label in ipairs(PAGE_NAMES) do
        local width = (#label * CHAR_W) + 12
        if index == page then
            rb.lcd_set_foreground(COLORS.tab)
            rb.lcd_fillrect(x, 8, width, CHAR_H + 8)
        end
        draw_text(x + 6, 12, label, (index == page) and COLORS.text or COLORS.dim)
        x = x + width + 6
    end
end

local function draw_habitat_page(state)
    draw_panel(10, 30, 186, 176, pet_title(state))
    draw_panel(202, 30, 108, 176, "Actions")

    local sprite = PET_SPRITES[state.starter][state.stage]
    draw_sprite(sprite, 24, 54, 7, pet_color(state), COLORS.text)

    draw_text(110, 58, rank_name(state), COLORS.dim)
    draw_text(110, 74, "Bond " .. tostring(state.bond), COLORS.gold)
    draw_text(110, 90, creature_mood_line(state), COLORS.text)

    draw_bar(22, 126, 160, "HP", state.health, COLORS.hp)
    draw_bar(22, 148, 160, "Full", state.fullness, COLORS.hunger)
    draw_bar(22, 170, 160, "Mood", state.mood, COLORS.mood)

    local y = 48
    for index, item in ipairs(ACTIONS) do
        if index == state.selected_action then
            rb.lcd_set_foreground(COLORS.tab)
            rb.lcd_fillrect(208, y - 2, 96, CHAR_H + 6)
            draw_text(214, y, item.label, COLORS.text)
        else
            draw_text(214, y, item.label, COLORS.dim)
        end
        y = y + CHAR_H + 6
    end

    draw_text(214, 170, ACTIONS[state.selected_action].hint, COLORS.text)
end

local function draw_stats_page(state)
    draw_panel(10, 30, 300, 176, "Status")
    draw_text(22, 48, pet_title(state) .. " the " .. rank_name(state), pet_color(state))
    draw_text(22, 64, "Day " .. tostring(state.day) .. "  Level " .. tostring(state.level), COLORS.dim)
    draw_text(170, 64, "XP " .. tostring(state.xp) .. "/" .. tostring(max_xp(state)), COLORS.gold)

    draw_bar(22, 90, 126, "Energy", state.energy, COLORS.energy)
    draw_bar(162, 90, 126, "Hygiene", state.hygiene, COLORS.hygiene)
    draw_bar(22, 128, 126, "Mood", state.mood, COLORS.mood)
    draw_bar(162, 128, 126, "Care", stat_average(state), pet_color(state))

    draw_text(22, 168, "Gold: " .. tostring(state.gold) .. "g", COLORS.gold)
    draw_text(112, 168, "Food: " .. tostring(state.food), COLORS.text)
    draw_text(186, 168, "Meds: " .. tostring(state.meds), COLORS.text)
    draw_text(22, 184, "Wins: " .. tostring(state.wins), COLORS.text)
    draw_text(112, 184, "Losses: " .. tostring(state.losses), COLORS.text)
    draw_text(206, 184, "Care: " .. care_label(stat_average(state)), COLORS.dim)
end

local function draw_journal_page(state)
    draw_panel(10, 30, 300, 176, "Journal")
    local y = 48
    for index = 1, 7 do
        local line = state.log[index]
        if line then
            draw_text(22, y, tostring(index) .. ". " .. line, (index == 1) and COLORS.text or COLORS.dim)
            y = y + CHAR_H + 8
        end
    end
    if #state.log == 0 then
        draw_text(22, 48, "The habitat is quiet right now.", COLORS.dim)
    end
end

local function draw_footer()
    draw_text(14, LCD_H - 22, "Up/Down action", COLORS.dim)
    draw_text(112, LCD_H - 22, "Left/Right page", COLORS.dim)
    draw_text(214, LCD_H - 22, "Select do", COLORS.dim)
    draw_text(272, LCD_H - 22, "Menu", COLORS.dim)
end

local function draw_screen(state)
    clear_screen()
    draw_tabs(state.page)
    if state.page == 1 then
        draw_habitat_page(state)
    elseif state.page == 2 then
        draw_stats_page(state)
    else
        draw_journal_page(state)
    end

    if state.sick == 1 then
        draw_text(216, 12, "SICK", COLORS.alert)
    end

    draw_footer()
    rb.lcd_update()
end

local function show_help()
    clear_screen()
    draw_panel(10, 14, 300, 212, "Pocket Quest")
    draw_text(22, 36, "Raise a creature that can grow into a legend.", COLORS.text)
    draw_text(22, 56, "Feed, clean, train, and rest to keep stats up.", COLORS.dim)
    draw_text(22, 76, "Explore to find treasure or trigger battles.", COLORS.dim)
    draw_text(22, 96, "Good care plus wins unlock evolution stages.", COLORS.dim)
    draw_text(22, 124, "Controls:", COLORS.text)
    draw_text(34, 140, "Up/Down  move through actions", COLORS.dim)
    draw_text(34, 156, "Left/Right  switch page", COLORS.dim)
    draw_text(34, 172, "Select  perform action", COLORS.dim)
    draw_text(34, 188, "Cancel/Exit  pause menu", COLORS.dim)
    draw_text(22, 208, "Press any button to return.", COLORS.gold)
    rb.lcd_update()
    rb.button_clear_queue()
    rb.get_plugin_action(-1)
end

local function hatch_new_game()
    local state = choose_starter()
    if not state then
        return nil
    end
    save_state(state)
    rb.splash(rb.HZ, pet_title(state) .. " joined your party.")
    return state
end

local function pause_menu(state)
    local choice = rb.do_menu("Pocket Quest", {
        "Save game",
        "Help",
        "Hatch new egg",
        "Quit"
    }, nil, false)

    if choice == 0 then
        if save_state(state) then
            add_log(state, "Game saved at Day " .. tostring(state.day) .. ".")
            rb.splash(rb.HZ, "Saved.")
        else
            rb.splash(rb.HZ, "Save failed.")
        end
    elseif choice == 1 then
        show_help()
    elseif choice == 2 then
        local confirm = rb.do_menu("Replace Save?", {"No", "Yes"}, 0, false)
        if confirm == 1 then
            local fresh = hatch_new_game()
            if fresh then
                return fresh
            end
        end
    elseif choice == 3 then
        save_state(state)
        os.exit()
    end
    return state
end

local state = load_state()
if not state then
    state = hatch_new_game()
    if not state then
        return
    end
end

while true do
    draw_screen(state)
    local action = rb.get_plugin_action(-1)

    if action == act.PLA_UP or action == act.PLA_UP_REPEAT then
        state.selected_action = wrap(state.selected_action - 1, #ACTIONS)
    elseif action == act.PLA_DOWN or action == act.PLA_DOWN_REPEAT then
        state.selected_action = wrap(state.selected_action + 1, #ACTIONS)
    elseif action == act.PLA_LEFT or action == act.PLA_LEFT_REPEAT then
        state.page = wrap(state.page - 1, #PAGE_NAMES)
    elseif action == act.PLA_RIGHT or action == act.PLA_RIGHT_REPEAT then
        state.page = wrap(state.page + 1, #PAGE_NAMES)
    elseif action == act.PLA_SELECT then
        perform_action(state)
    elseif action == act.PLA_CANCEL or action == act.PLA_EXIT then
        local next_state = pause_menu(state)
        if next_state then
            state = next_state
        end
    elseif action == rb.SYS_USB_CONNECTED then
        save_state(state)
        return rb.PLUGIN_USB_CONNECTED
    end

    rb.yield()
end
