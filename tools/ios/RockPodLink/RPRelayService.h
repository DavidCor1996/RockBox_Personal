#import <Foundation/Foundation.h>

@interface RPRelayService : NSObject
@property(nonatomic, copy) void (^statusHandler)(NSString *status,
                                                 BOOL connected);
@property(nonatomic, copy) void (^deviceInfoHandler)(NSDictionary *info);
@property(nonatomic, copy) void (^sitekickHandler)(NSData *bitmapData);
@property(nonatomic, copy) void (^libraryCatalogHandler)(NSData *catalogData);
@property(nonatomic, copy) void (^transferProgressHandler)(NSString *name,
                                                            double progress);
@property(atomic, readonly, getter=isConnected) BOOL connected;
+ (instancetype)sharedService;
- (void)start;
- (void)stop;
- (void)enqueueFileAtURL:(NSURL *)url
             destination:(NSString *)destination
              completion:(void (^)(BOOL success, NSString *message))completion;
- (void)requestSitekickPreview;
- (void)requestLibraryCatalog;
- (void)requestMediaFileAtPath:(NSString *)path
                    completion:(void (^)(NSURL *fileURL,
                                         NSString *errorMessage))completion;
- (void)requestMediaDataAtPath:(NSString *)path
                        offset:(uint64_t)offset
                        length:(NSUInteger)length
                    completion:(void (^)(NSData *data, uint64_t totalLength,
                                         NSString *errorMessage))completion;
- (void)requestSafeDisconnect:(void (^)(BOOL success,
                                         NSString *message))completion;
@end
