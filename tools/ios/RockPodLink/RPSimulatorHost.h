#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface RPSimulatorHost : NSObject

@property(atomic, readonly, getter=isRunning) BOOL running;

+ (instancetype)sharedHost;
- (void)prepareForMediaWithCompletion:(void (^)(BOOL success,
                                                 NSString *message))completion;
- (void)startWithCompletion:(void (^)(BOOL success, NSString *message))completion;
- (void)stop;

@end

NS_ASSUME_NONNULL_END
