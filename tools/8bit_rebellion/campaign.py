"""Offline adaptation of the six missions described by 1.4.5 localization.

Objective/dialogue text is imported from the original package. Positions,
combat rules and stage transitions here are reconstructed for the click wheel,
not recovered executable logic. Kind: talk, collect, fight, boss, password.
"""

HUBS = ['centralpark', 'casino', 'park', 'downtown', 'soho', 'westside', 'beach']
NAMES = {
    'centralpark': 'City Center', 'casino': 'Casino Row', 'park': 'The Park',
    'downtown': 'Downtown', 'soho': 'SoHo', 'westside': 'The West Side',
    'beach': 'The Beach', 'casinorow': 'Casino Le Chaz',
    'casinobar': 'The Purple Pixxel', 'casinotattoo': 'Club Tattoo',
    'casinoconstruction': 'Construction Site', 'cpcoffeeshop': 'Pixxel Kafe',
    'alifeshop': 'Artificial Life', 'lpunderground': 'LP Underground',
    'hospital': 'Hospital', 'mall': 'Mega Pixxel Mall',
    'parkdelson': 'Delson Park', 'parkcoffee': 'The Juice Boxx',
    'parkpixelgift': 'The Korp Shop', 'downtownjoyroom': "Joe Hahn's Joyroom",
    'downtownsuru': 'SURU', 'downtownpixxelbar': 'The Pixxel Bar',
    'sohophoenixpub': 'The Wobbly Phoenix Pub', 'sohocoffee': 'SoHo Kafe',
    'sohopixxelbar': 'Pixxel Pub', 'sohopoolhall': 'Side Pocket Pub',
    'artgallery': 'Shinoda Art Gallery', 'westsidecoffee': 'West Side Kafe',
    'boss1arena': 'West Side Garage', 'preboss2arena': 'Secret Hideout',
    'boss2arena': 'Pixie Arena', 'beachcoffee': 'Beach Pixxel Kafe',
    'beachbourdon': 'Bourdon Beach', 'beachhotdog': 'Pixxel Dogz & Snaxx',
}
TRACKS = ['one_step_closer_8_bit', 'faint_8_bit', 'in_the_end_8_bit',
          'new_divide_8_bit', 'qwerty_8_bit', 'hands_held_high_8_bit',
          'crawling_8_bit', 'no_more_sorrow_8_bit', 'Blackbirds_Mix',
          'bgm_apartment', 'bgm_mall', 'boss1_bg', 'boss2_bg']
SPRITES = ['Punker', 'jess', 'Chester-Bennington', 'Brad-Delson', 'Joe-Hahn',
           'Dave-Farrell', 'Mike-Shinoda', 'Rob-Bourdon', 'group01_03',
           'group04_05v2', 'group03_03v2', 'group01_07', 'group03_01v2',
           'flore', 'group01_05', 'group06_01', 'group08_01']

# scene, kind, count, actor, reward track bit (0 means none), objective key,
# dialogue key, short target label. '*' distributes collectibles over hubs.
STEPS = [
 ('lpunderground',0,1,1,0,'M1_OBJECTIVE_FIND_JESS','M1_Q0_CONV_JESS_0','Jess'),
 ('casinorow',0,1,2,0,'M1_OBJECTIVE_FIND_CHESTER','M1_Q1_NPC_5','Chester'),
 ('*',1,20,2,0,'M1_OBJECTIVE_DEFACE_POSTER','','Wanted poster'),
 ('casinorow',0,1,2,0,'M1_OBJECTIVE_RETURN_TO_CHESTER','M1_Q1_NPC_11','Chester'),
 ('downtown',0,1,9,0,'M1_OBJECTIVE_FIND_KITTY','M1_Q2_KITTY_6','Kitty'),
 ('downtown',2,6,16,0,'M1_OBJECTIVE_KILL_BUGS','M1_Q2_KITTY_8','Bug'),
 ('casino',0,1,10,0,'M1_OBJECTIVE_FIND_CONSTRUCTION','M1_Q3_BEAUBEAU_1','Beau Beau'),
 ('casino',2,3,8,0,'M1_OBJECTIVE_FIND_TRACK','M1_Q3_PLAYER_3','Agent'),
 ('casinoconstruction',2,3,8,0,'M1_OBJECTIVE_FIND_TRACK','M1_Q5_PLAYER_3','Guard'),
 ('casinorow',0,1,2,1,'M1_OBJECTIVE_RETURN_TRACK_TO_CHESTER','M1_Q5_CHESTER_3','Chester'),
 ('parkdelson',0,1,3,0,'M2_OBJECTIVE_FIND_BRAD','M2_Q1_BRAD_1B','Brad'),
 ('park',0,1,10,0,'M2_OBJECTIVE_FIND_JUICER','M2_Q2_PIXXELJUICER_2','Pixxel Juicer'),
 ('parkcoffee',2,6,16,0,'M2_OBJECTIVE_KILL_RATS','','Rat'),
 ('park',0,1,10,0,'M2_OBJECTIVE_RETURN_JUICE','M2_Q1_BRAD_2B','Pixxel Juicer'),
 ('park',2,4,14,0,'M2_OBJECTIVE_FIND_TRACK','','Officer'),
 ('parkdelson',0,1,3,2,'M2_OBJECTIVE_RETURN_TO_BRAD','M3_OBJECTIVE_FIND_JOE','Brad'),
 ('downtownsuru',0,1,4,0,'M3_OBJECTIVE_FIND_JOE','M3_Q1_JOE_3','Joe'),
 ('downtownjoyroom',0,1,10,0,'M3_OBJECTIVE_FIND_JIMMY','M3_Q2_PLAYER_2','Light switch'),
 ('downtownpixxelbar',2,3,11,0,'M3_OBJECTIVE_FIND_JIMMY_BAR','M3_Q2_JIMMY_2','Bouncer'),
 ('downtownsuru',0,1,4,4,'M3_OBJECTIVE_RETURN_TO_JOE','M4_OBJECTIVE_FIND_PHOENIX','Joe'),
 ('sohophoenixpub',0,1,5,0,'M4_OBJECTIVE_FIND_PHOENIX','M4_Q1_PHOENIX_4','Phoenix'),
 ('sohopoolhall',0,1,11,0,'M4_OBJECTIVE_TO_POOL_HALL','M4_Q2_PLAYER_4','Bouncer'),
 ('parkcoffee',1,1,16,0,'M4_OBJECTIVE_TO_POOL_HALL','M4_Q3_PLAYER_1','Catch a rat'),
 ('sohopoolhall',2,4,8,0,'M4_OBJECTIVE_SEARCH_TRACK','M4_Q3_BOUNCER_1','Agent'),
 ('sohophoenixpub',0,1,5,8,'M4_OBJECTIVE_RETURN_TRACK','M5_OBJECTIVE_Q1','Phoenix'),
 ('artgallery',0,1,6,0,'M5_OBJECTIVE_Q1','M5_Q1_MIKE_3','Mike'),
 ('*',1,20,8,0,'M5_OBJECTIVE_COVER_ADS','M5_Q3_PAPARAZZI_3','PixxelKorp ad'),
 ('*',1,6,12,0,'M5_OBJECTIVE_SNAP_COOKIE','M5_Q3_PLAYER_SNAP_6','Photograph Cookie'),
 ('park',0,1,12,0,'M5_OBJECTIVE_SNAP_RETURN_TO_PAPARAZZI','M5_Q4_PAPARAZZI_2','Ms. Paparazzi'),
 ('boss1arena',3,1,15,0,'M5_OBJECTIVE_FIGHT_BOSS','M5_Q4_PLAYER_2','Robotic Agent'),
 ('artgallery',0,1,6,16,'M5_OBJECTIVE_RETURN_TO_MIKE','M5_Q4_MIKE_2','Mike'),
 ('beachbourdon',0,1,13,0,'M6_OBJECTIVE_Q1','M6_Q1_JESS_2B','Fiore'),
 ('beachcoffee',0,1,8,0,'M6_OBJECTIVE_Q2','M6_Q2_SECRET_CONVERSATION_1','Listen to agents'),
 ('beachhotdog',4,1,8,0,'M6_OBJECTIVE_Q3','M6_Q3_LOCK_2','Secret door'),
 ('boss2arena',3,1,15,32,'M6_OBJECTIVE_Q4','M6_Q4_PLAYER_1','Pixie'),
]


def hub(stem):
    if stem.startswith(('boss', 'preboss', 'artgallery', 'westside')):
        return 5
    for i, key in enumerate(HUBS[1:], 1):
        if stem.startswith(key):
            return i
    return 0
