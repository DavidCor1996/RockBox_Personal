#import <Foundation/Foundation.h>

@interface RPDeviceMusic : NSObject
+ (instancetype)sharedController;
- (void)start;
- (void)syncLibrary;
@end
