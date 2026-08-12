#import <Foundation/Foundation.h>

typedef NS_ENUM(NSInteger, RPMediaServerType) {
    RPMediaServerTypePlex,
    RPMediaServerTypeJellyfin,
    RPMediaServerTypeSubsonic,
    RPMediaServerTypePersonal,
};

@interface RPMediaServerAccount : NSObject
@property(nonatomic, copy) NSString *identifier;
@property(nonatomic) RPMediaServerType type;
@property(nonatomic, strong) NSURL *baseURL;
@property(nonatomic, copy) NSString *username;
@property(nonatomic, copy) NSString *userID;
@property(nonatomic, copy) NSString *secret;
@property(nonatomic, copy) NSString *displayName;
@end

@interface RPMediaPlaylist : NSObject
@property(nonatomic, copy) NSString *identifier;
@property(nonatomic, copy) NSString *name;
@property(nonatomic) NSInteger trackCount;
@property(nonatomic, strong) NSURL *itemsURL;
@end

@interface RPMediaItem : NSObject
@property(nonatomic, copy) NSString *identifier;
@property(nonatomic, copy) NSString *title;
@property(nonatomic, copy) NSString *artist;
@property(nonatomic, copy) NSString *album;
@property(nonatomic, copy) NSString *suffix;
@property(nonatomic) NSInteger durationMS;
@property(nonatomic) unsigned long long size;
@property(nonatomic) BOOL downloadPermitted;
@property(nonatomic, strong) NSURL *mediaURL;
@property(nonatomic, strong) NSDictionary<NSString *, NSString *> *headers;
@end

@interface RPMediaServerService : NSObject
- (BOOL)saveAccount:(RPMediaServerAccount *)account error:(NSError **)error;
- (NSArray<RPMediaServerAccount *> *)savedAccounts;
- (void)testAccount:(RPMediaServerAccount *)account
          completion:(void (^)(BOOL success, NSString *message))completion;
- (void)fetchPlaylistsForAccount:(RPMediaServerAccount *)account
                      completion:(void (^)(NSArray<RPMediaPlaylist *> *playlists,
                                           NSString *errorMessage))completion;
- (void)fetchItemsForPlaylist:(RPMediaPlaylist *)playlist
                      account:(RPMediaServerAccount *)account
                   completion:(void (^)(NSArray<RPMediaItem *> *items,
                                        NSString *errorMessage))completion;
- (void)downloadItem:(RPMediaItem *)item
           completion:(void (^)(NSURL *fileURL, NSString *errorMessage))completion;
- (void)syncAudiobookPositionsForAccount:(RPMediaServerAccount *)account
                           localPositions:(NSArray<NSDictionary *> *)localPositions
                              completion:(void (^)(NSArray<NSDictionary *> *remotePositions,
                                                   NSString *errorMessage))completion;
@end
