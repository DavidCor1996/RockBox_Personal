/* blast_cli.c -- thin CLI wrapper around Mark Adler's public-domain-style
 * blast.c (PKWare Data Compression Library "explode" decompressor), zlib
 * licensed. Diablo's original MPQ archive stores most files compressed
 * with exactly this algorithm (the MPQ_FILE_IMPLODE block flag), so
 * diablo_prepare_assets.py shells out to this verified, unmodified
 * decompressor rather than reimplementing its bit-exact Huffman tables.
 *
 * Usage: blast_cli <infile> <outfile>
 * Reads the entire input file as one PKWare-imploded stream and writes
 * the decompressed bytes to outfile. Exit code mirrors blast()'s return
 * value (0 = success).
 */
#include <stdio.h>
#include <stdlib.h>
#include "blast.h"

struct mem_in {
    unsigned char *data;
    unsigned len;
    unsigned pos;
};

static unsigned in_fn(void *how, unsigned char **buf)
{
    struct mem_in *m = (struct mem_in *)how;
    *buf = m->data + m->pos;
    unsigned remaining = m->len - m->pos;
    m->pos = m->len;
    return remaining;
}

static int out_fn(void *how, unsigned char *buf, unsigned len)
{
    FILE *f = (FILE *)how;
    return fwrite(buf, 1, len, f) != len;
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "usage: %s <infile> <outfile>\n", argv[0]);
        return 3;
    }

    FILE *fin = fopen(argv[1], "rb");
    if (!fin) { perror("fopen input"); return 3; }
    fseek(fin, 0, SEEK_END);
    long sz = ftell(fin);
    fseek(fin, 0, SEEK_SET);
    unsigned char *buf = malloc(sz > 0 ? (size_t)sz : 1);
    if (sz > 0 && fread(buf, 1, (size_t)sz, fin) != (size_t)sz) {
        fprintf(stderr, "short read\n");
        return 3;
    }
    fclose(fin);

    FILE *fout = fopen(argv[2], "wb");
    if (!fout) { perror("fopen output"); return 3; }

    struct mem_in m = { buf, (unsigned)sz, 0 };
    int ret = blast(in_fn, &m, out_fn, fout, NULL, NULL);

    fclose(fout);
    free(buf);

    if (ret != 0)
        fprintf(stderr, "blast() returned %d\n", ret);
    return ret;
}
