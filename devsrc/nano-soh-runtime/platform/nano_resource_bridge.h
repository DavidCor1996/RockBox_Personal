#ifndef NANO_RESOURCE_BRIDGE_H
#define NANO_RESOURCE_BRIDGE_H
#include "nano_assets.h"
struct PlayState;

/* One synchronous game thread. The caller owns this manager and must fence
 * consumers before clearing or unbinding it. Only the original US 1.0 resource
 * pack is currently admitted; no alternate/mod resource registry is installed.
 */
int nano_resources_bind(struct nano_assets *);
struct nano_asset *nano_resource_named(const char *, uint32_t);
struct nano_asset *nano_resource_by_data(const void *, uint32_t);
struct nano_asset *nano_resource_crc(uint64_t, uint32_t);
void nano_scene_execute(struct PlayState *, struct nano_asset *);
int nano_resource_dma_read(uintptr_t, void *, size_t);

#endif
