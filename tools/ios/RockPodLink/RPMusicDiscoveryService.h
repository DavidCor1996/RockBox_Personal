#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface RPMusicDiscoveryService : NSObject
- (void)discoverForArtists:(NSArray<NSString *> *)artists
                completion:(void (^)(NSArray<NSDictionary *> *results,
                                     NSString * _Nullable errorMessage))completion;
- (nullable NSURL *)notificationManifestForResults:(NSArray<NSDictionary *> *)results
                                              error:(NSError **)error;
@end

NS_ASSUME_NONNULL_END
