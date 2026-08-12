#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN
@interface RPVideoMetadataService : NSObject
- (void)lookupVideoAtURL:(NSURL *)URL
              completion:(void (^)(NSDictionary * _Nullable metadata,
                                   NSString * _Nullable errorMessage))completion;
@end
NS_ASSUME_NONNULL_END
