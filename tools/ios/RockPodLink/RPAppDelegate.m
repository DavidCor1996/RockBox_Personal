#import "RPAppDelegate.h"
#import "RPRootViewController.h"
#import "RPRelayService.h"
#import <AVFoundation/AVFoundation.h>

@interface RPAppDelegate ()
@property(nonatomic) UIBackgroundTaskIdentifier linkBackgroundTask;
@property(nonatomic, strong) AVAudioPlayer *linkKeepalivePlayer;
@end

@implementation RPAppDelegate

- (BOOL)application:(UIApplication *)application
        didFinishLaunchingWithOptions:(NSDictionary *)launchOptions
{
    (void)application;
    (void)launchOptions;
    self.window = [[UIWindow alloc] initWithFrame:[UIScreen mainScreen].bounds];
    self.linkBackgroundTask = UIBackgroundTaskInvalid;
    self.window.rootViewController = [[UINavigationController alloc]
        initWithRootViewController:[[RPRootViewController alloc] init]];
    [self.window makeKeyAndVisible];
    [UIApplication sharedApplication].idleTimerDisabled = YES;
    [[RPRelayService sharedService] start];
    AVAudioSession *session = [AVAudioSession sharedInstance];
    [session setCategory:AVAudioSessionCategoryPlayback
             withOptions:AVAudioSessionCategoryOptionMixWithOthers error:nil];
    [session setActive:YES error:nil];
    NSURL *keepalive = [[NSBundle mainBundle] URLForResource:@"link-keepalive"
                                              withExtension:@"wav"
                                               subdirectory:@"Legacy"];
    self.linkKeepalivePlayer = [[AVAudioPlayer alloc] initWithContentsOfURL:keepalive
                                                                      error:nil];
    self.linkKeepalivePlayer.numberOfLoops = -1;
    self.linkKeepalivePlayer.volume = 0.001f;
    [self.linkKeepalivePlayer play];
    [[NSNotificationCenter defaultCenter]
        addObserver:self selector:@selector(applicationEnteredBackground)
              name:UIApplicationDidEnterBackgroundNotification object:nil];
    [[NSNotificationCenter defaultCenter]
        addObserver:self selector:@selector(applicationEnteredForeground)
              name:UIApplicationWillEnterForegroundNotification object:nil];
    return YES;
}

- (void)applicationEnteredBackground
{
    if (self.linkBackgroundTask != UIBackgroundTaskInvalid)
        return;
    self.linkBackgroundTask = [[UIApplication sharedApplication]
        beginBackgroundTaskWithName:@"RockPod USB Link"
        expirationHandler:^{
            if (self.linkBackgroundTask != UIBackgroundTaskInvalid)
            {
                [[UIApplication sharedApplication]
                    endBackgroundTask:self.linkBackgroundTask];
                self.linkBackgroundTask = UIBackgroundTaskInvalid;
            }
        }];
}

- (void)applicationEnteredForeground
{
    if (self.linkBackgroundTask != UIBackgroundTaskInvalid)
    {
        [[UIApplication sharedApplication] endBackgroundTask:self.linkBackgroundTask];
        self.linkBackgroundTask = UIBackgroundTaskInvalid;
    }
    [[RPRelayService sharedService] start];
}

@end
