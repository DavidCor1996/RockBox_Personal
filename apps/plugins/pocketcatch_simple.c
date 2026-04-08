/* PocketCatch MVP launcher shim (no dependencies) */
/* Intentionally avoid including plugin.h to keep patch portable in this env */
int plugin_start(const void* parameter)
{
    (void)parameter;
    return 0;
}
