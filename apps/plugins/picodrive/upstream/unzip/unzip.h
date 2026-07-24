#ifndef ROCKBOX_PICODRIVE_UNZIP_STUB_H
#define ROCKBOX_PICODRIVE_UNZIP_STUB_H

typedef struct zip_stub
{
    FILE *fp;
} ZIP;

struct zipent
{
    const char *name;
    unsigned int uncompressed_size;
    unsigned int compression_method;
};

ZIP *openzip(const char *path);
struct zipent *readzip(ZIP *zip);
int seekcompresszip(ZIP *zip, struct zipent *entry);
void closezip(ZIP *zip);

#endif
