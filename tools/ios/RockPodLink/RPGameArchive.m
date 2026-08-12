#import "RPGameArchive.h"
#import "RPGameStoreService.h"

/* Public-domain miniz is already vendored by Rockbox. Compile one private
 * copy into the companion so ordinary ROM-site ZIP downloads never need an
 * external framework. */
#include "../../../apps/plugins/xrick/3rd_party/miniz/miniz.c"

@implementation RPGameArchive

+ (NSURL *)extractSupportedGameFromArchive:(NSURL *)archive
                                     error:(NSError **)error
{
    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));
    if (!mz_zip_reader_init_file(&zip, archive.path.fileSystemRepresentation, 0))
    {
        if (error) *error = [NSError errorWithDomain:@"RockPodGameArchive" code:1
            userInfo:@{NSLocalizedDescriptionKey: @"The downloaded ZIP could not be opened."}];
        return nil;
    }
    NSURL *result = nil;
    for (mz_uint index = 0; index < mz_zip_reader_get_num_files(&zip); index++)
    {
        mz_zip_archive_file_stat stat;
        if (!mz_zip_reader_file_stat(&zip, index, &stat) ||
            mz_zip_reader_is_file_a_directory(&zip, index) ||
            mz_zip_reader_is_file_encrypted(&zip, index))
            continue;
        NSString *name = [NSString stringWithUTF8String:stat.m_filename].lastPathComponent;
        if (![RPGameStoreService isSupportedFilename:name] ||
            stat.m_uncomp_size == 0 || stat.m_uncomp_size > 32 * 1024 * 1024)
            continue;
        size_t size = 0;
        void *bytes = mz_zip_reader_extract_to_heap(&zip, index, &size, 0);
        if (!bytes || size != stat.m_uncomp_size)
        {
            if (bytes) mz_free(bytes);
            continue;
        }
        result = [NSFileManager.defaultManager.temporaryDirectory
            URLByAppendingPathComponent:[NSString stringWithFormat:@"%@-%@",
                                         NSUUID.UUID.UUIDString, name]];
        NSData *data = [NSData dataWithBytes:bytes length:size];
        mz_free(bytes);
        if (![data writeToURL:result atomically:YES]) result = nil;
        break;
    }
    mz_zip_reader_end(&zip);
    if (!result && error) *error = [NSError errorWithDomain:@"RockPodGameArchive" code:2
        userInfo:@{NSLocalizedDescriptionKey: @"The ZIP contains no supported game file."}];
    return result;
}

@end
