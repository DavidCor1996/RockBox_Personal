#import "RPMusicService.h"

static NSString *RPNormalMusicKey(NSString *value)
{
    NSString *folded = [value ?: @"" stringByFoldingWithOptions:
        NSDiacriticInsensitiveSearch | NSCaseInsensitiveSearch
                                                  locale:NSLocale.currentLocale];
    NSCharacterSet *allowed = NSCharacterSet.alphanumericCharacterSet;
    NSMutableString *result = [NSMutableString string];
    for (NSUInteger index = 0; index < folded.length; index++)
    {
        unichar character = [folded characterAtIndex:index];
        if ([allowed characterIsMember:character])
            [result appendFormat:@"%C", character];
    }
    return result;
}

@implementation RPMusicTrack

- (BOOL)audiobook
{
    NSString *kind = [NSString stringWithFormat:@"%@ %@", self.genre, self.path].lowercaseString;
    return [kind containsString:@"audiobook"] ||
           [kind containsString:@"spoken word"] ||
           [kind containsString:@"spoken-word"];
}

- (NSDictionary *)dictionaryRepresentation
{
    return @{ @"path": self.path ?: @"", @"title": self.title ?: @"",
        @"artist": self.artist ?: @"", @"album": self.album ?: @"",
        @"album_artist": self.albumArtist ?: @"", @"genre": self.genre ?: @"",
        @"artwork_path": self.artworkPath ?: @"",
        @"year": @(self.year), @"disc": @(self.discNumber),
        @"track": @(self.trackNumber), @"length_ms": @(self.lengthMS),
        @"bitrate_kbps": @(self.bitrateKbps), @"play_count": @(self.playCount),
        @"play_time_ms": @(self.playTimeMS), @"last_played": @(self.lastPlayed),
        @"rating": @(self.rating), @"last_elapsed_ms": @(self.lastElapsedMS),
        @"last_offset": @(self.lastOffset), @"mtime": @(self.modificationTime),
        @"audiobook": @(self.audiobook) };
}

@end

@implementation RPMusicAnalysis

- (NSDictionary *)dictionaryRepresentation
{
    return @{ @"recommendations": self.recommendations ?: @[],
        @"missing_album_tracks": self.missingAlbumTracks ?: @[],
        @"duplicate_groups": self.duplicateGroups ?: @[],
        @"top_tracks": self.topTracks ?: @[],
        @"top_artists": self.topArtists ?: @[],
        @"top_albums": self.topAlbums ?: @[],
        @"audiobook_positions": self.audiobookPositions ?: @[],
        @"total_tracks": @(self.totalTracks),
        @"total_play_time_ms": @(self.totalPlayTimeMS) };
}

@end

@implementation RPVideoItem
@end

@interface RPMusicService ()
@property(nonatomic, strong, readwrite) NSArray<RPMusicTrack *> *catalog;
@property(nonatomic, strong, readwrite) NSArray<RPVideoItem *> *videos;
@property(nonatomic, strong, readwrite) RPMusicAnalysis *analysis;
@end

@implementation RPMusicService

- (instancetype)init
{
    if ((self = [super init]))
    {
        self.catalog = @[];
        self.videos = @[];
        NSURL *cached = [[self applicationSupportDirectory]
            URLByAppendingPathComponent:@"ipod-library-v1.tsv"];
        NSData *data = [NSData dataWithContentsOfURL:cached];
        if (data.length)
            [self ingestLibraryCatalog:data error:nil];
    }
    return self;
}

- (NSURL *)applicationSupportDirectory
{
    NSURL *root = [NSFileManager.defaultManager URLsForDirectory:
        NSApplicationSupportDirectory inDomains:NSUserDomainMask].firstObject;
    NSURL *directory = [root URLByAppendingPathComponent:@"RockPodLink/Music"
                                             isDirectory:YES];
    [NSFileManager.defaultManager createDirectoryAtURL:directory
                           withIntermediateDirectories:YES attributes:nil error:nil];
    return directory;
}

- (BOOL)ingestLibraryCatalog:(NSData *)data error:(NSError **)error
{
    NSString *text = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
    if (![text hasPrefix:@"# rockpod-library-v1\n"])
    {
        if (error)
            *error = [NSError errorWithDomain:@"RockPodMusic" code:1 userInfo:
                @{NSLocalizedDescriptionKey: @"The iPod returned an unsupported music catalog."}];
        return NO;
    }
    NSMutableArray<RPMusicTrack *> *tracks = [NSMutableArray array];
    NSMutableArray<RPVideoItem *> *videos = [NSMutableArray array];
    NSArray<NSString *> *lines = [text componentsSeparatedByCharactersInSet:
        NSCharacterSet.newlineCharacterSet];
    for (NSString *line in lines)
    {
        if (!line.length || [line hasPrefix:@"#"] || [line hasPrefix:@"path\t"])
            continue;
        NSArray<NSString *> *fields = [line componentsSeparatedByString:@"\t"];
        if ([fields.firstObject isEqualToString:@"@video"] && fields.count >= 24)
        {
            RPVideoItem *video = [[RPVideoItem alloc] init];
            video.identifier = fields[1]; video.thumbPath = fields[2];
            video.title = fields[4]; video.kind = fields[5];
            video.devicePath = [@"/" stringByAppendingString:fields[7]];
            video.showTitle = fields[8]; video.seasonNumber = fields[9].integerValue;
            video.episodeNumber = fields[10].integerValue;
            video.durationSeconds = fields[11].integerValue;
            [videos addObject:video];
            continue;
        }
        if (fields.count < 18 || ![fields[0] hasPrefix:@"/"])
            continue;
        RPMusicTrack *track = [[RPMusicTrack alloc] init];
        track.path = fields[0]; track.title = fields[1]; track.artist = fields[2];
        track.album = fields[3]; track.albumArtist = fields[4]; track.genre = fields[5];
        track.year = fields[6].integerValue; track.discNumber = fields[7].integerValue;
        track.trackNumber = fields[8].integerValue; track.lengthMS = fields[9].integerValue;
        track.bitrateKbps = fields[10].integerValue; track.playCount = fields[11].integerValue;
        track.playTimeMS = fields[12].integerValue; track.lastPlayed = fields[13].doubleValue;
        track.rating = fields[14].integerValue; track.lastElapsedMS = fields[15].integerValue;
        track.lastOffset = fields[16].integerValue; track.modificationTime = fields[17].doubleValue;
        track.artworkPath = fields.count > 18 ? fields[18] : @"";
        [tracks addObject:track];
    }
    if (!tracks.count && !videos.count)
    {
        if (error)
            *error = [NSError errorWithDomain:@"RockPodMusic" code:2 userInfo:
                @{NSLocalizedDescriptionKey: @"No music or videos were found in the iPod catalog."}];
        return NO;
    }
    self.catalog = tracks;
    self.videos = videos;
    [data writeToURL:[[self applicationSupportDirectory]
        URLByAppendingPathComponent:@"ipod-library-v1.tsv"] atomically:YES];
    [self analyzeLibrary];
    return YES;
}

- (NSArray<NSDictionary *> *)rankedRows:(NSDictionary<NSString *, NSNumber *> *)scores
                                  limit:(NSUInteger)limit
{
    NSArray *keys = [scores keysSortedByValueUsingComparator:
        ^NSComparisonResult(NSNumber *left, NSNumber *right) {
            return [right compare:left];
        }];
    NSMutableArray *rows = [NSMutableArray array];
    for (NSString *key in [keys subarrayWithRange:NSMakeRange(0, MIN(limit, keys.count))])
        [rows addObject:@{ @"name": key, @"score": scores[key] }];
    return rows;
}

- (RPMusicAnalysis *)analyzeLibrary
{
    RPMusicAnalysis *result = [[RPMusicAnalysis alloc] init];
    NSMutableDictionary<NSString *, NSNumber *> *artistScores = [NSMutableDictionary dictionary];
    NSMutableDictionary<NSString *, NSNumber *> *albumScores = [NSMutableDictionary dictionary];
    NSMutableDictionary<NSString *, NSNumber *> *genreScores = [NSMutableDictionary dictionary];
    NSMutableDictionary<NSString *, NSMutableArray<RPMusicTrack *> *> *albums = [NSMutableDictionary dictionary];
    NSMutableDictionary<NSString *, NSMutableArray<RPMusicTrack *> *> *duplicateKeys = [NSMutableDictionary dictionary];
    NSMutableArray<NSDictionary *> *audiobooks = [NSMutableArray array];
    NSInteger totalPlayTime = 0;

    for (RPMusicTrack *track in self.catalog ?: @[])
    {
        NSInteger engagement = MAX(track.playTimeMS, track.playCount * MAX(track.lengthMS, 1));
        engagement += track.rating * 15 * 60 * 1000;
        totalPlayTime += track.playTimeMS;
        NSString *artist = track.albumArtist.length ? track.albumArtist : track.artist;
        if (artist.length)
            artistScores[artist] = @([artistScores[artist] longLongValue] + engagement);
        if (track.album.length)
        {
            NSString *display = artist.length ?
                [NSString stringWithFormat:@"%@ — %@", artist, track.album] : track.album;
            albumScores[display] = @([albumScores[display] longLongValue] + engagement);
            NSString *key = [NSString stringWithFormat:@"%@|%@",
                RPNormalMusicKey(artist), RPNormalMusicKey(track.album)];
            if (!albums[key]) albums[key] = [NSMutableArray array];
            [albums[key] addObject:track];
        }
        if (track.genre.length)
            genreScores[track.genre] = @([genreScores[track.genre] longLongValue] + engagement);
        NSString *duplicate = [NSString stringWithFormat:@"%@|%@",
            RPNormalMusicKey(track.artist), RPNormalMusicKey(track.title)];
        if (track.title.length && track.artist.length)
        {
            if (!duplicateKeys[duplicate]) duplicateKeys[duplicate] = [NSMutableArray array];
            [duplicateKeys[duplicate] addObject:track];
        }
        if (track.audiobook && track.lastElapsedMS > 0)
            [audiobooks addObject:@{ @"path": track.path, @"title": track.title,
                @"author": track.artist, @"elapsed_ms": @(track.lastElapsedMS),
                @"length_ms": @(track.lengthMS), @"offset": @(track.lastOffset),
                @"updated_at": @(track.lastPlayed) }];
    }

    NSMutableArray<NSDictionary *> *recommendations = [NSMutableArray array];
    for (RPMusicTrack *track in self.catalog ?: @[])
    {
        if (track.audiobook || !track.title.length || track.playCount > 2)
            continue;
        NSString *artist = track.albumArtist.length ? track.albumArtist : track.artist;
        double score = [artistScores[artist] doubleValue] * 0.68 +
                       [genreScores[track.genre] doubleValue] * 0.22 +
                       [albumScores[[NSString stringWithFormat:@"%@ — %@", artist, track.album]] doubleValue] * 0.10;
        score /= 1.0 + track.playCount * 2.0;
        [recommendations addObject:@{ @"track": track.dictionaryRepresentation,
            @"score": @(score), @"reason": track.playCount ?
                [NSString stringWithFormat:@"More from %@", artist] :
                [NSString stringWithFormat:@"Unplayed %@ track", track.genre.length ? track.genre : @"library"] }];
    }
    [recommendations sortUsingComparator:^NSComparisonResult(NSDictionary *left, NSDictionary *right) {
        return [right[@"score"] compare:left[@"score"]];
    }];
    if (recommendations.count > 30)
        [recommendations removeObjectsInRange:NSMakeRange(30, recommendations.count - 30)];

    NSMutableArray<NSDictionary *> *missing = [NSMutableArray array];
    for (NSArray<RPMusicTrack *> *albumTracks in albums.allValues)
    {
        if (albumTracks.count < 2)
            continue;
        NSMutableIndexSet *present = [NSMutableIndexSet indexSet];
        NSInteger maximum = 0;
        for (RPMusicTrack *track in albumTracks)
            if (track.trackNumber > 0 && track.trackNumber < 100)
            { [present addIndex:(NSUInteger)track.trackNumber]; maximum = MAX(maximum, track.trackNumber); }
        NSMutableArray *numbers = [NSMutableArray array];
        for (NSInteger number = 1; number <= maximum; number++)
            if (![present containsIndex:(NSUInteger)number]) [numbers addObject:@(number)];
        if (numbers.count && numbers.count <= 8)
        {
            RPMusicTrack *sample = albumTracks.firstObject;
            [missing addObject:@{ @"artist": sample.albumArtist.length ? sample.albumArtist : sample.artist,
                @"album": sample.album, @"missing_track_numbers": numbers,
                @"present_tracks": @(albumTracks.count) }];
        }
    }

    NSMutableArray<NSDictionary *> *duplicates = [NSMutableArray array];
    for (NSArray<RPMusicTrack *> *group in duplicateKeys.allValues)
    {
        if (group.count < 2)
            continue;
        NSMutableArray *items = [NSMutableArray array];
        for (RPMusicTrack *track in group) [items addObject:track.dictionaryRepresentation];
        [duplicates addObject:@{ @"artist": group.firstObject.artist,
            @"title": group.firstObject.title, @"tracks": items,
            @"reclaimable_bytes_estimate": @0 }];
    }

    NSArray *rankedTracks = [self.catalog sortedArrayUsingComparator:
        ^NSComparisonResult(RPMusicTrack *left, RPMusicTrack *right) {
            NSInteger leftScore = left.playTimeMS + left.playCount * left.lengthMS;
            NSInteger rightScore = right.playTimeMS + right.playCount * right.lengthMS;
            return rightScore > leftScore ? NSOrderedDescending :
                   rightScore < leftScore ? NSOrderedAscending : NSOrderedSame;
        }];
    NSMutableArray *topTracks = [NSMutableArray array];
    for (RPMusicTrack *track in [rankedTracks subarrayWithRange:
         NSMakeRange(0, MIN((NSUInteger)25, rankedTracks.count))])
        [topTracks addObject:track.dictionaryRepresentation];

    result.recommendations = recommendations;
    result.missingAlbumTracks = missing;
    result.duplicateGroups = duplicates;
    result.topTracks = topTracks;
    result.topArtists = [self rankedRows:artistScores limit:25];
    result.topAlbums = [self rankedRows:albumScores limit:25];
    result.audiobookPositions = audiobooks;
    result.totalTracks = self.catalog.count;
    result.totalPlayTimeMS = totalPlayTime;
    self.analysis = result;
    NSData *json = [NSJSONSerialization dataWithJSONObject:result.dictionaryRepresentation
                                                    options:NSJSONWritingPrettyPrinted error:nil];
    [json writeToURL:[[self applicationSupportDirectory]
        URLByAppendingPathComponent:@"analysis-v1.json"] atomically:YES];
    NSDateComponents *parts = [NSCalendar.currentCalendar components:NSCalendarUnitYear
                                                             fromDate:NSDate.date];
    NSURL *replayDirectory = [[self applicationSupportDirectory]
        URLByAppendingPathComponent:@"Replay" isDirectory:YES];
    [NSFileManager.defaultManager createDirectoryAtURL:replayDirectory
                           withIntermediateDirectories:YES attributes:nil error:nil];
    NSMutableDictionary *snapshot = result.dictionaryRepresentation.mutableCopy;
    snapshot[@"year"] = @(parts.year);
    snapshot[@"generated_at"] = @([NSDate date].timeIntervalSince1970);
    NSData *snapshotJSON = [NSJSONSerialization dataWithJSONObject:snapshot
        options:NSJSONWritingPrettyPrinted error:nil];
    [snapshotJSON writeToURL:[replayDirectory URLByAppendingPathComponent:
        [NSString stringWithFormat:@"%ld.json", (long)parts.year]] atomically:YES];
    return result;
}

- (NSInteger)recommendedBitrateForFreeBytes:(unsigned long long)freeBytes
                         incomingSourceBytes:(unsigned long long)sourceBytes
                           preferredQuality:(NSInteger)preferredKbps
{
    NSInteger preferred = MAX(64, MIN(320, preferredKbps ?: 192));
    if (!sourceBytes || freeBytes > sourceBytes * 2 + 1024ull * 1024ull * 1024ull)
        return preferred;
    if (freeBytes > sourceBytes + 512ull * 1024ull * 1024ull)
        return MIN(preferred, 192);
    if (freeBytes > sourceBytes / 2 + 256ull * 1024ull * 1024ull)
        return MIN(preferred, 128);
    return MIN(preferred, 96);
}

- (NSURL *)audiobookPositionManifestForRemotePositions:
    (NSArray<NSDictionary *> *)remotePositions error:(NSError **)error
{
    NSMutableDictionary<NSString *, RPMusicTrack *> *local = [NSMutableDictionary dictionary];
    for (RPMusicTrack *track in self.catalog)
        if (track.audiobook)
            local[track.path] = track;
    NSMutableString *manifest = [NSMutableString stringWithString:
        @"# rockpod-audiobook-positions-v1\n"];
    NSInteger updates = 0;
    for (NSDictionary *position in remotePositions)
    {
        NSString *path = [position[@"path"] isKindOfClass:NSString.class] ?
                         position[@"path"] : @"";
        RPMusicTrack *track = local[path];
        NSInteger elapsed = [position[@"elapsed_ms"] integerValue];
        NSInteger offset = MAX(0, [position[@"offset"] integerValue]);
        NSTimeInterval updated = [position[@"updated_at"] doubleValue];
        if (!track || elapsed < 0 || updated <= track.lastPlayed ||
            labs(elapsed - track.lastElapsedMS) <= 15000)
            continue;
        if (track.lengthMS > 0 && elapsed >= track.lengthMS - 30000)
        {
            elapsed = 0;
            offset = 0;
        }
        [manifest appendFormat:@"%@\t%ld\t%ld\t%.0f\n", path,
            (long)elapsed, (long)offset, updated];
        updates++;
    }
    if (!updates)
        return nil;
    NSURL *file = [NSFileManager.defaultManager.temporaryDirectory
        URLByAppendingPathComponent:[NSString stringWithFormat:
            @"audiobook-positions-%@.tsv", NSUUID.UUID.UUIDString]];
    if (![manifest writeToURL:file atomically:YES encoding:NSUTF8StringEncoding
                        error:error])
        return nil;
    return file;
}

- (NSURL *)duplicateCleanupManifest:(NSError **)error
{
    NSMutableString *text = [NSMutableString stringWithString:@"# rockpod-cleanup-v1\n"];
    NSInteger count = 0;
    for (NSDictionary *group in self.analysis.duplicateGroups)
    {
        NSArray *tracks = [group[@"tracks"] sortedArrayUsingComparator:^NSComparisonResult(NSDictionary *a, NSDictionary *b) {
            NSInteger left = [a[@"rating"] integerValue] * 100000 + [a[@"play_count"] integerValue] * 1000 + [a[@"bitrate_kbps"] integerValue];
            NSInteger right = [b[@"rating"] integerValue] * 100000 + [b[@"play_count"] integerValue] * 1000 + [b[@"bitrate_kbps"] integerValue];
            return right > left ? NSOrderedDescending : right < left ? NSOrderedAscending : NSOrderedSame;
        }];
        for (NSUInteger index = 1; index < tracks.count; index++)
        {
            NSString *path = tracks[index][@"path"];
            if ([path hasPrefix:@"/Music/RockPodLink/"] && ![path containsString:@".."])
            { [text appendFormat:@"%@\n", path]; count++; }
        }
    }
    if (!count) return nil;
    NSURL *URL = [NSFileManager.defaultManager.temporaryDirectory URLByAppendingPathComponent:
        [NSString stringWithFormat:@"duplicate-cleanup-%@.txt", NSUUID.UUID.UUIDString]];
    return [text writeToURL:URL atomically:YES encoding:NSUTF8StringEncoding error:error] ? URL : nil;
}

@end
