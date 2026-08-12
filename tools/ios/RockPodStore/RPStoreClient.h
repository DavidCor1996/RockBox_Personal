#import <Foundation/Foundation.h>

typedef void (^RPStoreCompletion)(id data, NSError *error);

@interface RPStoreClient : NSObject
+ (instancetype)sharedClient;
@property(nonatomic, copy) NSString *baseURL;
@property(nonatomic, readonly) BOOL paired;
@property(nonatomic, readonly) NSString *libraryID;
- (void)pairWithCode:(NSString *)code completion:(RPStoreCompletion)completion;
- (void)getPath:(NSString *)path completion:(RPStoreCompletion)completion;
- (void)postPath:(NSString *)path body:(NSDictionary *)body idempotencyKey:(NSString *)key completion:(RPStoreCompletion)completion;
- (void)downloadPath:(NSString *)path toFile:(NSString *)file completion:(RPStoreCompletion)completion;
- (NSURL *)URLForArtworkPath:(NSString *)path;
@end
