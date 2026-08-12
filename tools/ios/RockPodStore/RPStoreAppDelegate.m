#import "RPStoreAppDelegate.h"
#import "RPStoreViewController.h"
#import "RPDeviceMusic.h"

@implementation RPStoreAppDelegate

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)options {
    (void)application; (void)options;
    self.window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];

    RPStoreViewController *store = [[RPStoreViewController alloc] init];
    UINavigationController *storeNav = [[UINavigationController alloc] initWithRootViewController:store];
    storeNav.tabBarItem = [[UITabBarItem alloc] initWithTitle:@"Store"
        image:[UIImage imageNamed:@"TabStore.png"] tag:0];

    RPDownloadsViewController *downloads = [[RPDownloadsViewController alloc] init];
    UINavigationController *downloadsNav = [[UINavigationController alloc] initWithRootViewController:downloads];
    downloadsNav.tabBarItem = [[UITabBarItem alloc]
        initWithTabBarSystemItem:UITabBarSystemItemDownloads tag:1];
    downloadsNav.tabBarItem.title = @"Downloads";

    RPSettingsViewController *settings = [[RPSettingsViewController alloc] init];
    UINavigationController *settingsNav = [[UINavigationController alloc] initWithRootViewController:settings];
    settingsNav.tabBarItem = [[UITabBarItem alloc] initWithTitle:@"Settings"
        image:[UIImage imageNamed:@"TabSettings.png"] tag:2];

    UITabBarController *tabs = [[UITabBarController alloc] init];
    tabs.viewControllers = @[storeNav, downloadsNav, settingsNav];
    self.window.rootViewController = tabs;
    [self.window makeKeyAndVisible];
    [[RPDeviceMusic sharedController] start];
    return YES;
}

- (void)applicationDidBecomeActive:(UIApplication *)application {
    (void)application;
    [[RPDeviceMusic sharedController] start];
}
@end
