#include "dialtone.h"

save_data_t g_save;

bool dt_has_save(void) {
    return false;
}

bool dt_load_game(void) {
    return false;
}

void dt_save_game(void) {
    /* Save disabled temporarily while isolating Rockboy boot issues. */
}

void dt_erase_save(void) {
    /* Save disabled temporarily while isolating Rockboy boot issues. */
}
