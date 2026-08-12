#import "RPDeviceMusic.h"
#import "RPStoreClient.h"
#import <MediaPlayer/MediaPlayer.h>
#import <dlfcn.h>

@interface RPDeviceMusic ()
@property(nonatomic, strong) NSTimer *timer;
@property(nonatomic, copy) NSString *activeJobID;
@property(nonatomic, strong) NSDictionary *activeJob;
@property(nonatomic, strong) NSArray *activePaths;
@property(nonatomic) NSUInteger completedFiles;
@property(nonatomic) BOOL importFailed;
@end

@implementation RPDeviceMusic

+ (instancetype)sharedController {
    static RPDeviceMusic *controller;
    static dispatch_once_t once;
    dispatch_once(&once, ^{ controller = [[self alloc] init]; });
    return controller;
}

- (void)start {
    [self syncLibrary];
    [self poll];
    [self.timer invalidate];
    self.timer = [NSTimer scheduledTimerWithTimeInterval:3.0 target:self selector:@selector(poll) userInfo:nil repeats:YES];
}

- (void)syncLibrary {
    if (![RPStoreClient sharedClient].paired) return;
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        NSMutableArray *tracks = [NSMutableArray array];
        for (MPMediaItem *item in [[MPMediaQuery songsQuery] items]) {
            NSNumber *persistentID = [item valueForProperty:MPMediaItemPropertyPersistentID];
            [tracks addObject:@{
                @"persistent_id": [persistentID stringValue] ?: @"",
                @"title": [item valueForProperty:MPMediaItemPropertyTitle] ?: @"",
                @"artist": [item valueForProperty:MPMediaItemPropertyArtist] ?: @"",
                @"album": [item valueForProperty:MPMediaItemPropertyAlbumTitle] ?: @"",
                @"album_artist": [item valueForProperty:MPMediaItemPropertyAlbumArtist] ?: @"",
                @"track_number": [item valueForProperty:MPMediaItemPropertyAlbumTrackNumber] ?: @0,
                @"track_count": [item valueForProperty:MPMediaItemPropertyAlbumTrackCount] ?: @0,
                @"disc_number": [item valueForProperty:MPMediaItemPropertyDiscNumber] ?: @0,
                @"disc_count": [item valueForProperty:MPMediaItemPropertyDiscCount] ?: @0,
            }];
        }
        dispatch_async(dispatch_get_main_queue(), ^{
            [[RPStoreClient sharedClient] postPath:@"/v1/device/library" body:@{ @"tracks": tracks }
                                    idempotencyKey:nil completion:nil];
        });
    });
}

- (void)poll {
    if (self.activeJobID.length || ![RPStoreClient sharedClient].paired) return;
    [[RPStoreClient sharedClient] getPath:@"/v1/imports" completion:^(id data, NSError *error) {
        if (error || self.activeJobID.length || ![data isKindOfClass:[NSArray class]]) return;
        for (NSDictionary *job in data) {
            NSString *state = job[@"state"];
            if (([state isEqual:@"ready"] || [state isEqual:@"transferring"])
                    && [job[@"files"] count] > 0) {
                [self beginDeviceImport:job];
                break;
            }
        }
    }];
}

- (void)beginDeviceImport:(NSDictionary *)job {
    self.activeJobID = job[@"id"];
    self.activeJob = job;
    self.completedFiles = 0;
    self.importFailed = NO;
    NSArray *files = job[@"files"];
    NSString *folder = [@"/var/mobile/Media/RockPodStore/Incoming" stringByAppendingPathComponent:self.activeJobID];
    NSError *directoryError = nil;
    if (![[NSFileManager defaultManager] createDirectoryAtPath:folder withIntermediateDirectories:YES attributes:nil error:&directoryError]) {
        [self failActiveJob:directoryError.localizedDescription];
        return;
    }
    [[RPStoreClient sharedClient] postPath:[NSString stringWithFormat:@"/v1/imports/%@/state", self.activeJobID]
        body:@{ @"state": @"transferring" } idempotencyKey:nil completion:nil];
    NSMutableArray *paths = [NSMutableArray arrayWithCapacity:files.count];
    [self downloadFiles:files index:0 folder:folder paths:paths];
}

- (void)downloadFiles:(NSArray *)files index:(NSUInteger)index folder:(NSString *)folder paths:(NSMutableArray *)paths {
    if (index >= files.count) {
        self.activePaths = [paths copy];
        [self submitToGremlin];
        return;
    }
    NSDictionary *file = files[index];
    NSString *name = [[file[@"name"] lastPathComponent] length] ? [file[@"name"] lastPathComponent] : @"song.mp3";
    NSString *destination = [folder stringByAppendingPathComponent:[NSString stringWithFormat:@"%03lu-%@", (unsigned long)index, name]];
    [[RPStoreClient sharedClient] downloadPath:file[@"download_url"] toFile:destination completion:^(id data, NSError *error) {
        if (error) { [self failActiveJob:error.localizedDescription]; return; }
        [paths addObject:data];
        [self downloadFiles:files index:index + 1 folder:folder paths:paths];
    }];
}

- (void)submitToGremlin {
    void *handle = dlopen("/Library/Frameworks/Gremlin.framework/Gremlin", RTLD_LAZY);
    Class gremlin = NSClassFromString(@"Gremlin");
    if (!handle || !gremlin) {
        [self failActiveJob:@"Gremlin is not installed. Reinstall RockPod Store and its Music importer dependencies."];
        return;
    }
    SEL haveSelector = NSSelectorFromString(@"haveGremlin");
    SEL registerSelector = NSSelectorFromString(@"registerNotifications:");
    SEL importSelector = NSSelectorFromString(@"importFileWithInfo:");
    if (![gremlin respondsToSelector:haveSelector] || ![gremlin respondsToSelector:registerSelector]
            || ![gremlin respondsToSelector:importSelector]) {
        [self failActiveJob:@"The installed Gremlin Music importer is incompatible."];
        return;
    }
    BOOL (*HaveGremlin)(id, SEL) = (void *)[gremlin methodForSelector:haveSelector];
    BOOL (*Register)(id, SEL, id) = (void *)[gremlin methodForSelector:registerSelector];
    BOOL (*ImportFile)(id, SEL, id) = (void *)[gremlin methodForSelector:importSelector];
    if (!HaveGremlin(gremlin, haveSelector)) {
        [self failActiveJob:@"The Gremlin Music import service is not running."];
        return;
    }
    if (!Register(gremlin, registerSelector, self)) {
        [self failActiveJob:@"RockPod Store could not subscribe to Music import results."];
        return;
    }
    NSArray *files = self.activeJob[@"files"];
    for (NSUInteger index = 0; index < self.activePaths.count; index++) {
        NSDictionary *file = index < files.count ? files[index] : @{};
        NSString *title = [file[@"title"] length] ? file[@"title"]
            : [[file[@"name"] lastPathComponent] stringByDeletingPathExtension];
        NSString *artist = [file[@"artist"] length] ? file[@"artist"] : self.activeJob[@"artist"];
        NSString *album = [file[@"album"] length] ? file[@"album"] : self.activeJob[@"title"];
        NSUInteger trackNumber = [file[@"track_number"] unsignedIntegerValue] ?: index + 1;
        NSUInteger trackCount = [file[@"track_count"] unsignedIntegerValue] ?: self.activePaths.count;
        NSUInteger discNumber = [file[@"disc_number"] unsignedIntegerValue] ?: 1;
        NSUInteger discCount = [file[@"disc_count"] unsignedIntegerValue] ?: 1;

        NSMutableDictionary *metadata = [NSMutableDictionary dictionaryWithDictionary:@{
            @"title": title ?: @"",
            @"artist": artist ?: @"",
            @"albumName": album ?: @"",
            @"trackNumber": @(trackNumber),
            @"trackCount": @(trackCount),
            @"discNumber": @(discNumber),
            @"discCount": @(discCount),
            @"type": @"song",
        }];
        NSDictionary *optionalKeys = @{
            @"albumArtist": @"album_artist", @"genre": @"genre",
            @"composer": @"composer", @"comment": @"comment", @"year": @"year",
            @"compilation": @"compilation",
        };
        for (NSString *destinationKey in optionalKeys) {
            id value = file[optionalKeys[destinationKey]];
            if ([value isKindOfClass:[NSString class]] && [value length]) metadata[destinationKey] = value;
            if ([value isKindOfClass:[NSNumber class]] && [value integerValue]) metadata[destinationKey] = value;
        }

        NSDictionary *task = @{
            @"uuid": [[NSUUID UUID] UUIDString],
            @"path": self.activePaths[index],
            @"client": @"com.rockpod.store",
            @"apiVersion": @2,
            @"mediaKind": @"song",
            @"destination": @"iTunes",
            @"metadata": metadata,
        };
        /* Gremlin reports the authoritative result asynchronously. Some 3.x
           builds return NO even after accepting a task. */
        ImportFile(gremlin, importSelector, task);
    }
}

- (void)gremlinImportWasSuccessful:(NSDictionary *)info {
    (void)info;
    dispatch_async(dispatch_get_main_queue(), ^{
        if (!self.activeJobID.length || self.importFailed) return;
        self.completedFiles++;
        if (self.completedFiles < self.activePaths.count) return;
        NSString *jobID = self.activeJobID;
        [[RPStoreClient sharedClient] postPath:[NSString stringWithFormat:@"/v1/imports/%@/state", jobID]
            body:@{ @"state": @"completed" } idempotencyKey:nil completion:^(id data, NSError *error) {
                (void)data; (void)error;
                self.activeJobID = nil;
                self.activeJob = nil;
                self.activePaths = nil;
                [self syncLibrary];
            }];
    });
}

- (void)gremlinImport:(NSDictionary *)info didFailWithError:(NSError *)error {
    (void)info;
    dispatch_async(dispatch_get_main_queue(), ^{ [self failActiveJob:error.localizedDescription]; });
}

- (void)failActiveJob:(NSString *)message {
    if (!self.activeJobID.length || self.importFailed) return;
    self.importFailed = YES;
    NSString *jobID = self.activeJobID;
    [[RPStoreClient sharedClient] postPath:[NSString stringWithFormat:@"/v1/imports/%@/state", jobID]
        body:@{ @"state": @"device_failed", @"error": message ?: @"Music import failed." }
        idempotencyKey:nil completion:^(id data, NSError *error) {
            (void)data; (void)error;
            self.activeJobID = nil;
            self.activeJob = nil;
            self.activePaths = nil;
            self.importFailed = NO;
        }];
}

@end
