/* Sandboxed Varvara file devices. */

#ifndef UXN_FILE_H
#define UXN_FILE_H

#include "plugin.h"
#include "uxn.h"

#define UXN_DATA_DIR ROCKBOX_DIR "/uxn/data"

void uxn_file_init(void);
void uxn_file_shutdown(void);
void uxn_file_deo(uint8_t addr);

#endif
