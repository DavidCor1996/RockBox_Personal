#include "cps1.h"

#define MINIZ_NO_STDIO
#define MINIZ_NO_TIME
#define MINIZ_NO_ARCHIVE_APIS
#define MINIZ_NO_ARCHIVE_WRITING_APIS
#define MINIZ_NO_ZLIB_APIS
#define MINIZ_NO_MALLOC
#define MINIZ_NO_SPRINTF
#define MINIZ_USE_UNALIGNED_LOADS_AND_STORES 0
#include "upstream/miniz.c"

int cps1_inflate_raw(void *output, size_t output_size,
                     const void *input, size_t input_size)
{
    size_t result = tinfl_decompress_mem_to_mem(
        output, output_size, input, input_size,
        TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);

    return result == output_size ? 0 : -1;
}
