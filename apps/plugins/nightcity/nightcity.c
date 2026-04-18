/***************************************************************************
 * nightcity - Rockbox plugin entry point
 ***************************************************************************/

#include "nightcity.h"

enum plugin_status plugin_start(const void *parameter)
{
    (void)parameter;
    return nc_engine_run();
}
