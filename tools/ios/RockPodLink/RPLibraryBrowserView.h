#import <UIKit/UIKit.h>
@class RPMusicTrack, RPVideoItem;

typedef NS_ENUM(NSInteger, RPLibraryBrowserKind) {
    RPLibraryBrowserSongs, RPLibraryBrowserArtists,
    RPLibraryBrowserAlbums, RPLibraryBrowserVideos,
};

@interface RPLibraryBrowserView : UIView
@property(nonatomic, copy) void (^trackSelected)(RPMusicTrack *track);
@property(nonatomic, copy) void (^trackDownloadRequested)(RPMusicTrack *track);
@property(nonatomic, copy) void (^artworkRequested)(RPMusicTrack *track,
                                                     void (^completion)(UIImage *image));
- (instancetype)initWithKind:(RPLibraryBrowserKind)kind;
- (void)updateTracks:(NSArray<RPMusicTrack *> *)tracks videos:(NSArray<RPVideoItem *> *)videos;
@end
