#import <Foundation/Foundation.h>

typedef void (^DTVCompletion)(id data, NSError *error);

@interface DTVClient : NSObject
+ (instancetype)sharedClient;
@property(nonatomic, copy) NSString *baseURL;
@property(nonatomic, readonly) BOOL paired;
- (void)pairWithCode:(NSString *)code completion:(DTVCompletion)completion;
- (void)getPath:(NSString *)path completion:(DTVCompletion)completion;
- (void)postPath:(NSString *)path body:(NSDictionary *)body completion:(DTVCompletion)completion;
- (void)downloadPath:(NSString *)path toFile:(NSString *)file
       expectedBytes:(unsigned long long)expectedBytes completion:(DTVCompletion)completion;
@end
