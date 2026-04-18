/***************************************************************************
 * nightcity - story data and content
 ***************************************************************************/

#include "nightcity.h"

enum
{
    NC_NODE_STREET_INTRO = 100,
    NC_NODE_CORPO_INTRO = 110,
    NC_NODE_NOMAD_INTRO = 120,
    NC_NODE_CREW_BAR = 130,
    NC_NODE_AFTERHOURS = 135,
    NC_NODE_PREP = 140,
    NC_NODE_APPROACH = 150,
    NC_NODE_VAULT = 160,
    NC_NODE_BETRAYAL = 170,
    NC_NODE_ESCAPE = 180,
    NC_NODE_SAFEHOUSE = 190,
    NC_NODE_SAFEHOUSE_BROKEN = 195,
    NC_NODE_SHADE = 200,
    NC_NODE_CROSSROADS = 220,
    NC_NODE_CLINIC = 230,
    NC_NODE_BROADCAST = 240,
    NC_NODE_CORP_MEET = 250,
    NC_NODE_CONVOY = 260,
    NC_NODE_FINAL_PREP = 270,
    NC_NODE_FINAL_ENCOUNTER = 280,
    NC_NODE_END_REBEL = 300,
    NC_NODE_END_CORP = 310,
    NC_NODE_END_GHOST = 320,
    NC_NODE_END_COST = 330,
};

enum
{
    NC_ENEMY_SENTINEL = 1,
    NC_ENEMY_AEGIS = 2,
};

#define COND_ANY { NC_ANY_MIN, NC_ANY_MIN, NC_ANY_MIN, NC_ANY_MIN, NC_ANY_MIN, NC_ANY_MAX, NC_LIFEPATH_NONE, 0, 0, 0 }
#define EFFECT_NONE { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }

static const struct nc_choice street_intro_choices[] =
{
    {
        "Call in old alley favors.",
        NC_NODE_CREW_BAR,
        COND_ANY,
        { 1, 1, 0, 0, 10, 0, 0, 0, 0, 0, 0, 0, 0 }
    },
    {
        "Keep the blades hidden and the heat low.",
        NC_NODE_CREW_BAR,
        COND_ANY,
        { 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
    },
};

static const struct nc_choice corpo_intro_choices[] =
{
    {
        "Keep the badge cipher in your sleeve.",
        NC_NODE_CREW_BAR,
        COND_ANY,
        { 0, 1, -1, 0, 20, 0, 0, 0, 0, 0, NC_FLAG_CORP_CONTACT, 0, 0 }
    },
    {
        "Burn the badge and vanish from the payroll.",
        NC_NODE_CREW_BAR,
        COND_ANY,
        { 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, NC_FLAG_CORP_CONTACT, 0 }
    },
};

static const struct nc_choice nomad_intro_choices[] =
{
    {
        "Keep the convoy transponder warm.",
        NC_NODE_CREW_BAR,
        COND_ANY,
        { 0, 0, 1, 0, 0, 0, 0, 0, 0, 2, NC_FLAG_NOMAD_ROUTE, 0, 0 }
    },
    {
        "Fence the spare parts for quick cash.",
        NC_NODE_CREW_BAR,
        COND_ANY,
        { 0, 0, 0, 0, 25, 0, 0, 0, 0, -1, 0, 0, 0 }
    },
};

static const struct nc_choice crew_bar_choices[] =
{
    {
        "Streetkid: blanket the docks with gutter lookouts.",
        NC_NODE_AFTERHOURS,
        { NC_ANY_MIN, NC_ANY_MIN, NC_ANY_MIN, NC_ANY_MIN, NC_ANY_MIN, NC_ANY_MAX,
          NC_LIFEPATH_STREETKID, 0, 0, 0 },
        { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, NC_FLAG_DOCK_COVER, 0, 0 }
    },
    {
        "Corpo: ask Mira for badge architecture and audit paths.",
        NC_NODE_AFTERHOURS,
        { NC_ANY_MIN, NC_ANY_MIN, NC_ANY_MIN, NC_ANY_MIN, NC_ANY_MIN, NC_ANY_MAX,
          NC_LIFEPATH_CORPO, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NC_FLAG_BADGE_SCHEMA, 0, 0 }
    },
    {
        "Nomad: map the freight coolant line under the skyport.",
        NC_NODE_AFTERHOURS,
        { NC_ANY_MIN, NC_ANY_MIN, NC_ANY_MIN, NC_ANY_MIN, NC_ANY_MIN, NC_ANY_MAX,
          NC_LIFEPATH_NOMAD, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, NC_FLAG_TUNNEL_MAP, 0, 0 }
    },
    {
        "Take the advance and keep prep simple.",
        NC_NODE_AFTERHOURS,
        COND_ANY,
        { 0, 0, 0, 0, 20, 0, 0, 0, 0, 0, 0, 0, 0 }
    },
};

static const struct nc_choice afterhours_choices[] =
{
    {
        "Woman: lean into Nyra Venn's pirate booth and flirt through the static.",
        NC_NODE_PREP,
        { NC_ANY_MIN, NC_ANY_MIN, NC_ANY_MIN, NC_ANY_MIN, NC_ANY_MIN, NC_ANY_MAX,
          NC_LIFEPATH_NONE, NC_FLAG_GENDER_FEMME, 0, 0 },
        { 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, NC_FLAG_SPARK_NYRA, 0, 0 }
    },
    {
        "Trade clinic stories with Mira while the room cools off.",
        NC_NODE_PREP,
        COND_ANY,
        { 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, NC_FLAG_SPARK_MIRA, 0, 0 }
    },
    {
        "Take the balcony with Rook and talk roads, exits, and fear.",
        NC_NODE_PREP,
        COND_ANY,
        { 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, NC_FLAG_SPARK_ROOK, 0, 0 }
    },
    {
        "Skip the chemistry. Go straight to loadout.",
        NC_NODE_PREP,
        COND_ANY,
        EFFECT_NONE
    },
};

static const struct nc_choice prep_choices[] =
{
    {
        "Install a reflex loop combat rig.",
        NC_NODE_APPROACH,
        { NC_ANY_MIN, NC_ANY_MIN, NC_ANY_MIN, 20, NC_ANY_MIN, NC_ANY_MAX,
          NC_LIFEPATH_NONE, 0, 0, 0 },
        { 0, 0, 0, 0, -20, 0, 2, 0, 0, 0, 0, 0, NC_CYBER_COMBAT_RIG }
    },
    {
        "Slot a ghostwall filter against hostile intrusions.",
        NC_NODE_APPROACH,
        { NC_ANY_MIN, NC_ANY_MIN, NC_ANY_MIN, 20, NC_ANY_MIN, NC_ANY_MAX,
          NC_LIFEPATH_NONE, 0, 0, 0 },
        { 0, 0, 0, 0, -20, 0, 0, 0, 0, 0, 0, 0, NC_CYBER_GHOSTWALL }
    },
    {
        "Buy a velvet spoof to fake clean credentials.",
        NC_NODE_APPROACH,
        { NC_ANY_MIN, NC_ANY_MIN, NC_ANY_MIN, 15, NC_ANY_MIN, NC_ANY_MAX,
          NC_LIFEPATH_NONE, 0, 0, 0 },
        { 0, 0, 0, 0, -15, 0, 0, 0, 0, 0, 0, 0, NC_CYBER_SOCIAL }
    },
    {
        "Stock medkits instead of chrome.",
        NC_NODE_APPROACH,
        { NC_ANY_MIN, NC_ANY_MIN, NC_ANY_MIN, 10, NC_ANY_MIN, NC_ANY_MAX,
          NC_LIFEPATH_NONE, 0, 0, 0 },
        { 0, 0, 0, 0, -10, 0, 0, 1, 0, 0, 0, 0, 0 }
    },
    {
        "Keep the cash and trust your nerve.",
        NC_NODE_APPROACH,
        COND_ANY,
        EFFECT_NONE
    },
};

static const struct nc_choice approach_choices[] =
{
    {
        "Bluff the dock gate with forged credentials.",
        NC_NODE_VAULT,
        { NC_ANY_MIN, NC_ANY_MIN, NC_ANY_MIN, NC_ANY_MIN, NC_ANY_MIN, NC_ANY_MAX,
          NC_LIFEPATH_NONE, 0, 0, NC_CYBER_SOCIAL },
        { 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
    },
    {
        "Streetkid route: walk the shadows behind your spotters.",
        NC_NODE_VAULT,
        { 2, NC_ANY_MIN, NC_ANY_MIN, NC_ANY_MIN, NC_ANY_MIN, NC_ANY_MAX,
          NC_LIFEPATH_NONE, NC_FLAG_DOCK_COVER, 0, 0 },
        { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
    },
    {
        "Nomad route: crawl the coolant trench under the pad.",
        NC_NODE_VAULT,
        { NC_ANY_MIN, NC_ANY_MIN, NC_ANY_MIN, NC_ANY_MIN, 1, NC_ANY_MAX,
          NC_LIFEPATH_NONE, NC_FLAG_TUNNEL_MAP, 0, 0 },
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0 }
    },
    {
        "Kick in through the cargo lift and eat the alarms.",
        NC_NODE_VAULT,
        COND_ANY,
        { 1, 2, -1, 0, 0, -2, 0, 0, 0, 0, 0, 0, 0 }
    },
};

static const struct nc_choice vault_choices[] =
{
    {
        "Steal the Blackglass Ledger first.",
        NC_NODE_BETRAYAL,
        COND_ANY,
        { 1, 0, 0, 0, 20, 0, 0, 0, 0, 0, NC_FLAG_LEDGER, 0, 0 }
    },
    {
        "Jack the Sable Echo shard first.",
        NC_NODE_BETRAYAL,
        COND_ANY,
        { 0, 0, -1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
    },
    {
        "Mirror both in one violent pass.",
        NC_NODE_BETRAYAL,
        { NC_ANY_MIN, NC_ANY_MIN, NC_ANY_MIN, NC_ANY_MIN, NC_ANY_MIN, 3,
          NC_LIFEPATH_NONE, 0, 0, NC_CYBER_GHOSTWALL },
        { 1, 1, -1, 1, 20, 0, 0, 0, 0, 0, NC_FLAG_LEDGER, 0, 0 }
    },
};

static const struct nc_choice betrayal_choices[] =
{
    {
        "Drag Rook through the shutter fire.",
        NC_NODE_ESCAPE,
        COND_ANY,
        { 0, 0, 1, 0, 0, -2, 0, 0, 0, 0, NC_FLAG_SAVED_ROOK, 0, 0 }
    },
    {
        "Sprint for the service stairs with the ledger.",
        NC_NODE_ESCAPE,
        COND_ANY,
        { 1, 1, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0 }
    },
    {
        "Let the voice in the shard route your heartbeat.",
        NC_NODE_ESCAPE,
        COND_ANY,
        { 0, 0, -1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
    },
};

static const struct nc_choice shade_choices[] =
{
    {
        "Hear Sable out. Maybe the ghost knows the board.",
        NC_NODE_CROSSROADS,
        COND_ANY,
        { 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
    },
    {
        "Ask Mira for blockers and keep the ghost behind glass.",
        NC_NODE_CROSSROADS,
        COND_ANY,
        { 0, 0, -1, -1, 0, 2, 0, 0, 0, 0, NC_FLAG_BLOCKERS, 0, 0 }
    },
    {
        "Ping the old corp channel and bait a private meeting.",
        NC_NODE_CROSSROADS,
        { NC_ANY_MIN, NC_ANY_MIN, NC_ANY_MIN, NC_ANY_MIN, NC_ANY_MIN, NC_ANY_MAX,
          NC_LIFEPATH_NONE, NC_FLAG_CORP_CONTACT, 0, 0 },
        { 0, 1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
    },
};

static const struct nc_choice crossroads_choices[] =
{
    {
        "Raid the stitch clinic for stabilizers.",
        NC_NODE_CLINIC,
        COND_ANY,
        EFFECT_NONE
    },
    {
        "Broadcast the ledger across the rainstack.",
        NC_NODE_BROADCAST,
        { 2, NC_ANY_MIN, NC_ANY_MIN, NC_ANY_MIN, NC_ANY_MIN, NC_ANY_MAX,
          NC_LIFEPATH_NONE, NC_FLAG_LEDGER, 0, 0 },
        EFFECT_NONE
    },
    {
        "Meet Kade Rhyne and hear the corporate offer.",
        NC_NODE_CORP_MEET,
        { NC_ANY_MIN, NC_ANY_MIN, NC_ANY_MIN, NC_ANY_MIN, NC_ANY_MIN, NC_ANY_MAX,
          NC_LIFEPATH_NONE, NC_FLAG_CORP_CONTACT, 0, 0 },
        EFFECT_NONE
    },
    {
        "Prime a convoy lane and keep one road out of the city.",
        NC_NODE_CONVOY,
        { NC_ANY_MIN, NC_ANY_MIN, NC_ANY_MIN, NC_ANY_MIN, 1, NC_ANY_MAX,
          NC_LIFEPATH_NONE, NC_FLAG_NOMAD_ROUTE, 0, 0 },
        EFFECT_NONE
    },
};

static const struct nc_choice clinic_choices[] =
{
    {
        "Pay the ripper, take the clean stabilizer, keep your soul.",
        NC_NODE_FINAL_PREP,
        { NC_ANY_MIN, NC_ANY_MIN, NC_ANY_MIN, 20, NC_ANY_MIN, NC_ANY_MAX,
          NC_LIFEPATH_NONE, 0, 0, 0 },
        { 0, 0, 1, -1, -20, 6, 0, 0, 0, 0, 0, 0, NC_CYBER_GHOSTWALL }
    },
    {
        "Strong-arm the clinic and rip the hardware out yourself.",
        NC_NODE_FINAL_PREP,
        COND_ANY,
        { 1, 1, -2, -1, 0, 4, 0, 0, 0, 1, 0, 0, NC_CYBER_COMBAT_RIG }
    },
    {
        "Let Rook talk the room calm while Mira works.",
        NC_NODE_FINAL_PREP,
        { NC_ANY_MIN, NC_ANY_MIN, NC_ANY_MIN, NC_ANY_MIN, NC_ANY_MIN, NC_ANY_MAX,
          NC_LIFEPATH_NONE, NC_FLAG_SAVED_ROOK, 0, 0 },
        { 0, 0, 1, 0, 0, 5, 0, 1, 0, 0, 0, 0, 0 }
    },
};

static const struct nc_choice broadcast_choices[] =
{
    {
        "Dump the full ledger and let the city choke on truth.",
        NC_NODE_FINAL_PREP,
        COND_ANY,
        { 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, NC_FLAG_REBEL_PLAN, 0, 0 }
    },
    {
        "Cut the names that would get street clinics burned.",
        NC_NODE_FINAL_PREP,
        COND_ANY,
        { 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, NC_FLAG_REBEL_PLAN, 0, 0 }
    },
    {
        "Let Sable encrypt the leak and seed it like a rumor plague.",
        NC_NODE_FINAL_PREP,
        COND_ANY,
        { 1, 1, -1, 1, 0, 0, 0, 0, 0, 0, NC_FLAG_REBEL_PLAN, 0, 0 }
    },
    {
        "Woman: stay on the rooftop with Nyra after the burst and make it real.",
        NC_NODE_FINAL_PREP,
        { NC_ANY_MIN, NC_ANY_MIN, NC_ANY_MIN, NC_ANY_MIN, NC_ANY_MIN, NC_ANY_MAX,
          NC_LIFEPATH_NONE, NC_FLAG_SPARK_NYRA | NC_FLAG_GENDER_FEMME, 0, 0 },
        { 1, 1, 1, 0, 0, 1, 0, 0, 0, 0, NC_FLAG_REBEL_PLAN | NC_FLAG_ROMANCE_NYRA, 0, 0 }
    },
};

static const struct nc_choice corp_meet_choices[] =
{
    {
        "Take Kade's provisional cure and dirty truce.",
        NC_NODE_FINAL_PREP,
        COND_ANY,
        { 0, -1, -1, -1, 60, 5, 0, 0, 0, 0, NC_FLAG_CORP_DEAL, 0, 0 }
    },
    {
        "Smile, stall, and steal his master key instead.",
        NC_NODE_FINAL_PREP,
        COND_ANY,
        { 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, NC_FLAG_MASTER_KEY, 0, NC_CYBER_SOCIAL }
    },
    {
        "Refuse the offer and walk out with your head intact.",
        NC_NODE_FINAL_PREP,
        COND_ANY,
        { 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, NC_FLAG_CORP_DEAL, 0 }
    },
};

static const struct nc_choice convoy_choices[] =
{
    {
        "Reserve seats for Rook and Mira and a hard road out.",
        NC_NODE_FINAL_PREP,
        COND_ANY,
        { 0, 0, 1, 0, 0, 2, 0, 0, 0, -1, NC_FLAG_ESCAPE_ROUTE, 0, 0 }
    },
    {
        "Trade your spare parts for blackout charges.",
        NC_NODE_FINAL_PREP,
        COND_ANY,
        { 1, 1, 0, 0, 30, 0, 0, 0, 0, -1, NC_FLAG_BLAST_PACK | NC_FLAG_ESCAPE_ROUTE, 0, 0 }
    },
    {
        "Wire the convoy around the worst checkpoints and keep moving.",
        NC_NODE_FINAL_PREP,
        COND_ANY,
        { 0, -1, 0, 0, 0, 3, 0, 0, 1, -1, NC_FLAG_ESCAPE_ROUTE, 0, 0 }
    },
};

static const struct nc_choice final_prep_choices[] =
{
    {
        "Light the relay and rip the city out of Solace's hands.",
        NC_NODE_FINAL_ENCOUNTER,
        { 3, 3, NC_ANY_MIN, NC_ANY_MIN, NC_ANY_MIN, NC_ANY_MAX,
          NC_LIFEPATH_NONE, NC_FLAG_REBEL_PLAN, 0, 0 },
        { 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, NC_FLAG_END_REBEL, 0, 0 }
    },
    {
        "Take the bargain and walk the shard into the boardroom.",
        NC_NODE_FINAL_ENCOUNTER,
        { NC_ANY_MIN, NC_ANY_MIN, NC_ANY_MIN, NC_ANY_MIN, NC_ANY_MIN, 4,
          NC_LIFEPATH_NONE, NC_FLAG_CORP_DEAL, 0, 0 },
        { 0, -1, -1, 0, 0, 0, 0, 0, 0, 0, NC_FLAG_END_CORP, 0, 0 }
    },
    {
        "Carry Sable into the core and burn with the signal.",
        NC_NODE_FINAL_ENCOUNTER,
        { NC_ANY_MIN, NC_ANY_MIN, 4, NC_ANY_MIN, NC_ANY_MIN, NC_ANY_MAX,
          NC_LIFEPATH_NONE, 0, 0, 0 },
        { 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, NC_FLAG_END_GHOST, 0, 0 }
    },
    {
        "Cut a road out, knock the locks loose, and survive the fallout.",
        NC_NODE_FINAL_ENCOUNTER,
        { NC_ANY_MIN, NC_ANY_MIN, NC_ANY_MIN, NC_ANY_MIN, NC_ANY_MIN, NC_ANY_MAX,
          NC_LIFEPATH_NONE, NC_FLAG_ESCAPE_ROUTE, 0, 0 },
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NC_FLAG_END_ESCAPE, 0, 0 }
    },
};

static const struct nc_enemy enemies[] =
{
    {
        NC_ENEMY_SENTINEL,
        "Solace Sentinel",
        "A vault sentinel drone peels out of the smoke, floodlights cutting the rain.",
        18, 3, 6, 45, 15, 1, 0
    },
    {
        NC_ENEMY_AEGIS,
        "Aegis Frame",
        "The relay core unfolds a corporate war chassis around you, all chrome prayers and riot fire.",
        28, 4, 8, 55, 30, 2, 0
    },
};

static const struct nc_node nodes[] =
{
    {
        NC_NODE_STREET_INTRO, NC_NODE_SCENE,
        "Prologue // Streetkid", "Vesper",
        "The rain in Glass Alley tastes like battery acid and old favors. "
        "You grew up running courier blades for the gutter crews, fast enough to dodge both sirens and debt collectors.\n\n"
        "Tonight fixer Juno Vale offers a clean job at Lattice Row Skyport. "
        "Clean, in this city, usually means someone richer gets to decide who bleeds.",
        street_intro_choices, ARRAYLEN(street_intro_choices), -1, -1, 0,
        { 1, 0, 1, 0, 10, 0, 0, 0, 0, 0, 0, 0, 0 }
    },
    {
        NC_NODE_CORPO_INTRO, NC_NODE_SCENE,
        "Prologue // Corpo", "Vesper",
        "Three nights ago you still had a window office, a biometric key, and a chair that cost more than most alley clinics. "
        "Now Helix Meridian wants its ghosted auditor erased from every ledger but the living ones.\n\n"
        "Juno's job is small, deniable, and close enough to revenge to feel useful.",
        corpo_intro_choices, ARRAYLEN(corpo_intro_choices), -1, -1, 0,
        { 0, 1, 0, 0, 40, 0, 0, 0, 0, 0, 0, 0, 0 }
    },
    {
        NC_NODE_NOMAD_INTRO, NC_NODE_SCENE,
        "Prologue // Nomad", "Vesper",
        "You rolled into Vanta Harbor with road dust in your teeth and a cargo bike held together by prayer, solder, and bad music. "
        "The city calls everyone from outside a drifter until it needs them to do something dangerous.\n\n"
        "Juno Vale found you before the customs drones did. That usually means the night's already chosen.",
        nomad_intro_choices, ARRAYLEN(nomad_intro_choices), -1, -1, 0,
        { 0, 0, 1, 0, 0, 4, 2, 0, 0, 1, 0, 0, 0 }
    },
    {
        NC_NODE_CREW_BAR, NC_NODE_SCENE,
        "Act I // Static and Brass", "Juno Vale",
        "Under the neon bleed of the Coil Room, Juno sketches the play across a sticky tabletop. "
        "Rook handles hardware and exits. Dr. Mira keeps everyone breathing. Your target is a Solace Dynamics vault tucked inside Lattice Row Skyport.\n\n"
        "\"We grab a ledger, maybe something hotter,\" Juno says. \"In and out before dawn notices.\"",
        crew_bar_choices, ARRAYLEN(crew_bar_choices), -1, -1, 0, EFFECT_NONE
    },
    {
        NC_NODE_AFTERHOURS, NC_NODE_SCENE,
        "Act I // Afterhours", "Nyra Venn",
        "Past the briefing tables, the Coil Room opens into a balcony packed with hacked speakers, wet chrome jackets, and too much voltage for the wiring. "
        "Nyra Venn is crouched over a pirate-radio deck with one side of her head shaved, magenta fiber threaded through the rest, combat boots up on the rail like the city owes her a better skyline.\n\n"
        "\"Juno says you bite back,\" Nyra says without looking up. \"Good. I hate dead-eyed mercs.\"",
        afterhours_choices, ARRAYLEN(afterhours_choices), -1, -1, 0, EFFECT_NONE
    },
    {
        NC_NODE_PREP, NC_NODE_SCENE,
        "Act I // Loadout", "Dr. Mira Sorn",
        "Mira unfolds a foam case of bargain chrome and field meds. "
        "\"Pick one edge,\" she says. \"Clickwheel body, not corpo tank body. Too much hardware and you lose the run before it starts.\"",
        prep_choices, ARRAYLEN(prep_choices), -1, -1, 0, EFFECT_NONE
    },
    {
        NC_NODE_APPROACH, NC_NODE_SCENE,
        "Act II // Lattice Row", "Rook",
        "The skyport hangs over black water and humming ad towers. Cargo skiffs drift beneath you while Solace floodlights sweep the docks in slow metallic arcs.\n\n"
        "\"Last chance to be smart,\" Rook mutters as the vault tower glows through the rain.",
        approach_choices, ARRAYLEN(approach_choices), -1, -1, 0, EFFECT_NONE
    },
    {
        NC_NODE_VAULT, NC_NODE_SCENE,
        "Act II // Vault Glass", "Vesper",
        "You crack the Blackglass chamber and find two prizes at once: "
        "the ledger tying Solace to ghosted clinic deaths, and a palm-sized neurolattice shard pulsing like it already knows your pulse.\n\n"
        "Sirens bloom across the tower. Whatever you take first decides what chases you out.",
        vault_choices, ARRAYLEN(vault_choices), -1, -1, 0, EFFECT_NONE
    },
    {
        NC_NODE_BETRAYAL, NC_NODE_SCENE,
        "Act II // Split Signal", "Unknown Voice",
        "Juno's extraction channel dies mid-sentence. Security shutters hammer down. "
        "Then the shard punches cold light behind your eyes and a stranger laughs inside your skull.\n\n"
        "\"Name's Sable,\" the ghost says. \"And your fixer sold both of us.\"",
        betrayal_choices, ARRAYLEN(betrayal_choices), -1, -1, 0, EFFECT_NONE
    },
    {
        NC_NODE_ESCAPE, NC_NODE_ENCOUNTER,
        "Act II // Hot Exit", "Solace Sentinel",
        "Smoke. Rain. Lights. Metal closing fast.",
        NULL, 0, NC_NODE_SAFEHOUSE, NC_NODE_SAFEHOUSE_BROKEN, NC_ENEMY_SENTINEL,
        EFFECT_NONE
    },
    {
        NC_NODE_SAFEHOUSE, NC_NODE_SCENE,
        "Act III // Backroom Surgery", "Dr. Mira Sorn",
        "Mira seals the clinic door with a welding torch and jacks a cable into the wound behind your ear. "
        "\"The shard carries a dissident neural imprint,\" she says. \"Sable Echo. Solace wanted to weaponize it. Now it's nesting in you.\"",
        NULL, 0, NC_NODE_SHADE, -1, 0, EFFECT_NONE
    },
    {
        NC_NODE_SAFEHOUSE_BROKEN, NC_NODE_SCENE,
        "Act III // Backroom Surgery", "Dr. Mira Sorn",
        "You crawl into the safehouse half-burned and tasting copper. Mira cuts your jacket off with a knife and doesn't waste time on sympathy.\n\n"
        "\"You lost blood, nerve shielding, and whatever illusion you had about a clean score,\" she says. \"The shard is still in there. So are you, for now.\"",
        NULL, 0, NC_NODE_SHADE, -1, 0,
        { 0, 1, -1, 0, 0, -4, 0, 0, 0, 0, 0, 0, 0 }
    },
    {
        NC_NODE_SHADE, NC_NODE_SCENE,
        "Act III // Ghost in the Machine", "Sable Echo",
        "Sable isn't a screaming virus. It talks like someone who remembers losing a revolution one compromise at a time. "
        "Solace built obedience firmware around its code. The relay in Crown Meridian can still push that signal citywide.\n\n"
        "\"We can cut the spine out of this place,\" Sable whispers. \"If you decide what you're willing to become first.\"",
        shade_choices, ARRAYLEN(shade_choices), -1, -1, 0, EFFECT_NONE
    },
    {
        NC_NODE_CROSSROADS, NC_NODE_SCENE,
        "Act III // Choose Your Angle", "Vesper",
        "Every route out of this mess points somewhere ugly: a clinic for stabilizers, a broadcast stack for the ledger, a corporate back room, or a convoy lane leading into the dark salt roads.\n\n"
        "Night City doesn't do pure choices. It does prices.",
        crossroads_choices, ARRAYLEN(crossroads_choices), -1, -1, 0, EFFECT_NONE
    },
    {
        NC_NODE_CLINIC, NC_NODE_SCENE,
        "Act III // Stitch Clinic", "Dr. Mira Sorn",
        "The stitch clinic smells like antiseptic, ozone, and panic. Ripper lights buzz above a half-shuttered operating chair while scavengers pound the outer gate.\n\n"
        "Mira spots a stabilizer cradle that could keep Sable from shredding your nerves for a few more days.",
        clinic_choices, ARRAYLEN(clinic_choices), -1, -1, 0, EFFECT_NONE
    },
    {
        NC_NODE_BROADCAST, NC_NODE_SCENE,
        "Act III // Rainstack Broadcast", "Nyra Venn",
        "You climb a lattice of rusted service ladders above the flood market and find Nyra already there, jacket covered in anarch patches, pirate deck wired into a jury-rigged antenna crown. "
        "Rain beads off the magenta edge-lighting she stitched into her collar while the whole district glows below like a circuit board with a knife in it.\n\n"
        "\"Truth is just ammo unless you aim it,\" Nyra says, passing you one earcup. Sable laughs somewhere behind your eyes like static approving static.",
        broadcast_choices, ARRAYLEN(broadcast_choices), -1, -1, 0, EFFECT_NONE
    },
    {
        NC_NODE_CORP_MEET, NC_NODE_SCENE,
        "Act III // Private Table", "Kade Rhyne",
        "Kade Rhyne waits in a quiet bar above the magrail, all polished shoes and expensive patience. "
        "\"Solace overreached,\" he says. \"Help me cage the asset and I can make your medical problem disappear.\"",
        corp_meet_choices, ARRAYLEN(corp_meet_choices), -1, -1, 0, EFFECT_NONE
    },
    {
        NC_NODE_CONVOY, NC_NODE_SCENE,
        "Act III // Salt Lane", "Rook",
        "The convoy lane smells like diesel and wet tarps. Smugglers trade rumors faster than fuel, and everyone knows the city only lets people leave when the city thinks they stop mattering.\n\n"
        "Rook leans against a truck door. \"If we're running,\" he says, \"we run with a plan, not panic.\"",
        convoy_choices, ARRAYLEN(convoy_choices), -1, -1, 0, EFFECT_NONE
    },
    {
        NC_NODE_FINAL_PREP, NC_NODE_SCENE,
        "Finale // Crown Meridian Relay", "Sable Echo",
        "The relay tower in Crown Meridian hums under stormlight, wrapped in Solace gunmetal and old civic propaganda. "
        "Inside waits the switch that can free the city, sell it, fuse with you, or simply buy you enough time to live.\n\n"
        "Pick the ending you can survive hearing about later.",
        final_prep_choices, ARRAYLEN(final_prep_choices), -1, -1, 0, EFFECT_NONE
    },
    {
        NC_NODE_FINAL_ENCOUNTER, NC_NODE_ENCOUNTER,
        "Finale // Relay Core", "Aegis Frame",
        "The last machine between you and the signal.",
        NULL, 0, NC_DYNAMIC_ENDING, NC_NODE_END_COST, NC_ENEMY_AEGIS,
        EFFECT_NONE
    },
    {
        NC_NODE_END_REBEL, NC_NODE_ENDING,
        "Ending // Neon Rebellion", "Vesper",
        "You crack the relay wide open and dump Solace's obedience lattice into the public rain. "
        "Clinic deaths, bribe chains, firmware tests, all of it spills across billboard glass and gutter projectors before the corp can mute the feed.\n\n"
        "By dawn the city is furious, louder than clean markets can tolerate, and alive in a way it hasn't been in years. "
        "You don't get a statue. You get a thousand copied masks, a price on your head, and the brief electric certainty that the megacity finally had to look at itself.",
        NULL, 0, -1, -1, 0, EFFECT_NONE
    },
    {
        NC_NODE_END_CORP, NC_NODE_ENDING,
        "Ending // Boardroom Truce", "Kade Rhyne",
        "Kade keeps his word exactly the way executives do: selectively. "
        "You walk Sable into a sealed boardroom, trade chaos for treatment, and watch one shark gut another with your evidence as the blade.\n\n"
        "The cure works enough to keep your hands steady. The city stays sick. "
        "Your new contract has better lighting, quieter enemies, and the permanent understanding that compromise is just surrender with stock options.",
        NULL, 0, -1, -1, 0, EFFECT_NONE
    },
    {
        NC_NODE_END_GHOST, NC_NODE_ENDING,
        "Ending // Ghostfire", "Sable Echo",
        "You carry Sable into the relay heart and let your pulse become the ignition. "
        "For one white-hot minute the two of you are not host and hitchhiker but a single impossible signal tearing corporate ownership out by the roots.\n\n"
        "The tower goes dark. So do you. "
        "Afterward people swear the dead zones whisper with your laugh whenever the rain hits bare wire. Maybe that's superstition. Maybe that's the only immortality Night City ever deserved.",
        NULL, 0, -1, -1, 0, EFFECT_NONE
    },
    {
        NC_NODE_END_COST, NC_NODE_ENDING,
        "Ending // Breathing Price", "Vesper",
        "You get out alive. That's not nothing. Maybe you torch enough hardware to slow Solace down. Maybe you flee with the convoy while the tower burns behind you. Maybe you wake up later with Sable still humming under the scar.\n\n"
        "The city keeps turning, cruel and luminous. "
        "You keep moving too, carrying whatever part of the night you couldn't quite cut free. Survival wins no purity points here. It just keeps the story from ending yet.",
        NULL, 0, -1, -1, 0, EFFECT_NONE
    },
};

const char *nc_lifepath_name(enum nc_lifepath lifepath)
{
    switch (lifepath)
    {
        case NC_LIFEPATH_STREETKID:
            return "Streetkid";
        case NC_LIFEPATH_CORPO:
            return "Corpo";
        case NC_LIFEPATH_NOMAD:
            return "Nomad";
        default:
            return "Unknown";
    }
}

const char *nc_gender_name(enum nc_gender gender)
{
    switch (gender)
    {
        case NC_GENDER_MASC:
            return "Man";
        case NC_GENDER_FEMME:
            return "Woman";
        case NC_GENDER_NONBINARY:
            return "Nonbinary";
        default:
            return "Unset";
    }
}

const char *nc_cyberware_name(unsigned item)
{
    switch (item)
    {
        case NC_CYBER_COMBAT_RIG:
            return "Reflex Loop";
        case NC_CYBER_GHOSTWALL:
            return "Ghostwall Filter";
        case NC_CYBER_SOCIAL:
            return "Velvet Spoof";
        default:
            return "";
    }
}

const char *nc_profile_name(enum nc_profile profile)
{
    switch (profile)
    {
        case NC_PROFILE_RAZOR:
            return "Razor";
        case NC_PROFILE_VELVET:
            return "Velvet";
        case NC_PROFILE_DRIFT:
            return "Drift";
        default:
            return "";
    }
}

void nc_story_start_run(struct nc_game_state *state, enum nc_lifepath lifepath)
{
    rb->memset(state, 0, sizeof(*state));
    state->lifepath = lifepath;
    state->gender = NC_GENDER_NONE;
    state->profile = NC_PROFILE_NONE;
    state->street_cred = 1;
    state->corp_heat = 1;
    state->humanity = 5;
    state->ghost_sync = 0;
    state->credits = 40;
    state->health = 24;
    state->max_health = 24;
    state->medkits = 1;
    state->stims = 1;
    state->scrap = 1;
    state->active_save = true;

    switch (lifepath)
    {
        case NC_LIFEPATH_STREETKID:
            state->current_node_id = NC_NODE_STREET_INTRO;
            break;
        case NC_LIFEPATH_CORPO:
            state->current_node_id = NC_NODE_CORPO_INTRO;
            break;
        case NC_LIFEPATH_NOMAD:
            state->current_node_id = NC_NODE_NOMAD_INTRO;
            break;
        default:
            state->current_node_id = NC_NODE_STREET_INTRO;
            break;
    }
}

const struct nc_node *nc_story_get_node(int id)
{
    int i;

    for (i = 0; i < (int)ARRAYLEN(nodes); ++i)
        if (nodes[i].id == id)
            return &nodes[i];

    return NULL;
}

const struct nc_enemy *nc_story_get_enemy(int id)
{
    int i;

    for (i = 0; i < (int)ARRAYLEN(enemies); ++i)
        if (enemies[i].id == id)
            return &enemies[i];

    return NULL;
}

int nc_story_choose_ending(const struct nc_game_state *state)
{
    if (state->flags & NC_FLAG_END_GHOST)
        return NC_NODE_END_GHOST;

    if (state->flags & NC_FLAG_END_CORP)
    {
        if (state->corp_heat <= 4 &&
            ((state->flags & NC_FLAG_CORP_DEAL) || state->lifepath == NC_LIFEPATH_CORPO))
            return NC_NODE_END_CORP;
        return NC_NODE_END_COST;
    }

    if (state->flags & NC_FLAG_END_ESCAPE)
        return NC_NODE_END_COST;

    if ((state->flags & NC_FLAG_END_REBEL) ||
        (state->flags & NC_FLAG_REBEL_PLAN) ||
        (state->street_cred >= 4 && state->humanity >= 4))
    {
        if (state->humanity >= 3)
            return NC_NODE_END_REBEL;
    }

    return NC_NODE_END_COST;
}
