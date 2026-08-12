#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface RPMusicTrack : NSObject
@property(nonatomic, copy) NSString *path;
@property(nonatomic, copy) NSString *title;
@property(nonatomic, copy) NSString *artist;
@property(nonatomic, copy) NSString *album;
@property(nonatomic, copy) NSString *albumArtist;
@property(nonatomic, copy) NSString *genre;
@property(nonatomic, copy) NSString *artworkPath;
@property(nonatomic) NSInteger year;
@property(nonatomic) NSInteger discNumber;
@property(nonatomic) NSInteger trackNumber;
@property(nonatomic) NSInteger lengthMS;
@property(nonatomic) NSInteger bitrateKbps;
@property(nonatomic) NSInteger playCount;
@property(nonatomic) NSInteger playTimeMS;
@property(nonatomic) NSTimeInterval lastPlayed;
@property(nonatomic) NSInteger rating;
@property(nonatomic) NSInteger lastElapsedMS;
@property(nonatomic) NSInteger lastOffset;
@property(nonatomic) NSTimeInterval modificationTime;
@property(nonatomic, readonly) BOOL audiobook;
- (NSDictionary *)dictionaryRepresentation;
@end

@interface RPMusicAnalysis : NSObject
@property(nonatomic, strong) NSArray<NSDictionary *> *recommendations;
@property(nonatomic, strong) NSArray<NSDictionary *> *missingAlbumTracks;
@property(nonatomic, strong) NSArray<NSDictionary *> *duplicateGroups;
@property(nonatomic, strong) NSArray<NSDictionary *> *topTracks;
@property(nonatomic, strong) NSArray<NSDictionary *> *topArtists;
@property(nonatomic, strong) NSArray<NSDictionary *> *topAlbums;
@property(nonatomic, strong) NSArray<NSDictionary *> *audiobookPositions;
@property(nonatomic) NSInteger totalTracks;
@property(nonatomic) NSInteger totalPlayTimeMS;
- (NSDictionary *)dictionaryRepresentation;
@end

@interface RPVideoItem : NSObject
@property(nonatomic, copy) NSString *identifier;
@property(nonatomic, copy) NSString *title;
@property(nonatomic, copy) NSString *kind;
@property(nonatomic, copy) NSString *devicePath;
@property(nonatomic, copy) NSString *showTitle;
@property(nonatomic) NSInteger seasonNumber;
@property(nonatomic) NSInteger episodeNumber;
@property(nonatomic) NSInteger durationSeconds;
@property(nonatomic, copy) NSString *thumbPath;
@end

@interface RPMusicService : NSObject
@property(nonatomic, strong, readonly) NSArray<RPMusicTrack *> *catalog;
@property(nonatomic, strong, readonly) NSArray<RPVideoItem *> *videos;
@property(nonatomic, strong, readonly) RPMusicAnalysis *analysis;
- (BOOL)ingestLibraryCatalog:(NSData *)data error:(NSError **)error;
- (RPMusicAnalysis *)analyzeLibrary;
- (NSInteger)recommendedBitrateForFreeBytes:(unsigned long long)freeBytes
                         incomingSourceBytes:(unsigned long long)sourceBytes
                           preferredQuality:(NSInteger)preferredKbps;
- (nullable NSURL *)audiobookPositionManifestForRemotePositions:
    (NSArray<NSDictionary *> *)remotePositions error:(NSError **)error;
- (nullable NSURL *)duplicateCleanupManifest:(NSError **)error;
@end

NS_ASSUME_NONNULL_END
