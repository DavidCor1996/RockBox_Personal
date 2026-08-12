#import "OLAppDelegate.h"
#import "OLGameViewController.h"

@implementation OLAppDelegate

- (BOOL)application:(UIApplication *)application
        didFinishLaunchingWithOptions:(NSDictionary *)options {
    (void)application;
    (void)options;
    self.window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
    self.window.rootViewController = [[OLGameViewController alloc] init];
    [self.window makeKeyAndVisible];
    return YES;
}

- (void)applicationWillResignActive:(UIApplication *)application {
    (void)application;
    [(OLGameViewController *)self.window.rootViewController pauseGame];
}

- (void)applicationDidBecomeActive:(UIApplication *)application {
    (void)application;
    [(OLGameViewController *)self.window.rootViewController resumeGame];
}

@end
