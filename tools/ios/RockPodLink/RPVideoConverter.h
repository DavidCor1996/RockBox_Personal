#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface RPVideoConversion : NSObject
@property(nonatomic, copy) NSString *title;
@property(nonatomic, copy) NSString *identifier;
@property(nonatomic, strong) NSArray<NSURL *> *files;
@property(nonatomic, strong) NSArray<NSString *> *destinations;
@end

@interface RPVideoConverter : NSObject
- (void)convertURL:(NSURL *)url
        completion:(void (^)(RPVideoConversion *_Nullable result,
                             NSString *_Nullable errorMessage))completion;
- (void)convertURL:(NSURL *)url metadata:(NSDictionary *)metadata
        completion:(void (^)(RPVideoConversion *_Nullable result,
                             NSString *_Nullable errorMessage))completion;
@end

NS_ASSUME_NONNULL_END
