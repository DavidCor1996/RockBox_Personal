#import "DTVAppDelegate.h"
#import "DTVGuideController.h"

@implementation DTVAppDelegate

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)options {
    (void)application; (void)options;
    self.window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
    DTVGuideController *guide = [[DTVGuideController alloc] init];
    self.window.rootViewController = guide;
    [self.window makeKeyAndVisible];
    return YES;
}

- (void)applicationDidBecomeActive:(UIApplication *)application {
    (void)application;
    DTVGuideController *guide = (DTVGuideController *)self.window.rootViewController;
    if ([guide isKindOfClass:[DTVGuideController class]]) [guide catchUpToLiveClock];
}
@end
