/***************************************************************************
 * nightcity - Rockbox plugin entry point
 ***************************************************************************/

#include "nightcity.h"
#include "nc_audio.h"

enum plugin_status plugin_start(const void *parameter)
{
    enum plugin_status status;

    (void)parameter;
    nc_audio_init();
    status = nc_engine_run();
    nc_audio_stop();
    return status;
}
