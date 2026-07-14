unsigned char gSoundDataADSR[] = {
#if UINTPTR_MAX > UINT32_MAX
#include "sound/sound_data.ctl.inc.c"
#else
#include "sound32/sound_data.ctl.inc.c"
#endif
};

unsigned char gSoundDataRaw[] = {
#if UINTPTR_MAX > UINT32_MAX
#include "sound/sound_data.tbl.inc.c"
#else
#include "sound32/sound_data.tbl.inc.c"
#endif
};

unsigned char gMusicData[] = {
#if UINTPTR_MAX > UINT32_MAX
#include "sound/sequences.bin.inc.c"
#else
#include "sound32/sequences.bin.inc.c"
#endif
};

unsigned char gBankSetsData[] = {
#if UINTPTR_MAX > UINT32_MAX
#include "sound/bank_sets.inc.c"
#else
#include "sound32/bank_sets.inc.c"
#endif
};
