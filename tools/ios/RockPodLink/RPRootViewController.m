#import "RPRootViewController.h"
#import "RPRelayService.h"
#import "RPIPodAudioStreamer.h"
#import "RPVideoConverter.h"
#import "RPVideoMetadataService.h"
#import "RPGameStoreService.h"
#import "RPGameArchive.h"
#import "RPGameBrowserViewController.h"
#import "RPMusicService.h"
#import "RPMusicDiscoveryService.h"
#import "RPMediaServerService.h"
#import "RPAudioTranscoder.h"
#import "RPLibraryBrowserView.h"
#import "RPSimulatorHost.h"
#import <AVFoundation/AVFoundation.h>
#import <CoreLocation/CoreLocation.h>
#import <MediaPlayer/MediaPlayer.h>
#import <PhotosUI/PhotosUI.h>

typedef NS_ENUM(NSInteger, RPPickerPurpose) {
    RPPickerPurposeNone,
    RPPickerPurposeSyncPhotos,
    RPPickerPurposeSyncVideos,
    RPPickerPurposeSimulatorPhoto,
    RPPickerPurposeSimulatorVideo,
    RPPickerPurposeSimulatorMusic,
    RPPickerPurposeGameFiles,
};

@interface RPRootViewController ()
    <UIDocumentPickerDelegate, MPMediaPickerControllerDelegate,
     PHPickerViewControllerDelegate, UIImagePickerControllerDelegate,
     UINavigationControllerDelegate, CLLocationManagerDelegate>
@property(nonatomic, strong) RPRelayService *relay;
@property(nonatomic, strong) RPVideoConverter *videoConverter;
@property(nonatomic, strong) RPVideoMetadataService *videoMetadata;
@property(nonatomic, strong) UILabel *displayLabel;
@property(nonatomic, strong) UILabel *footerLabel;
@property(nonatomic, strong) UILabel *footerVersionLabel;
@property(nonatomic, strong) UIScrollView *sourceScroll;
@property(nonatomic, strong) UIStackView *sourceTabs;
@property(nonatomic, strong) NSArray<UIButton *> *sourceButtons;
@property(nonatomic, strong) NSArray<UIView *> *pages;
@property(nonatomic, strong) UIView *pageHost;
@property(nonatomic, strong) UILabel *deviceNameLabel;
@property(nonatomic, strong) UILabel *modeLabel;
@property(nonatomic, strong) UILabel *firmwareLabel;
@property(nonatomic, strong) UILabel *batteryLabel;
@property(nonatomic, strong) UILabel *capacityLabel;
@property(nonatomic, strong) UILabel *nowPlayingLabel;
@property(nonatomic, strong) UIView *usedStorage;
@property(nonatomic, strong) NSLayoutConstraint *usedStorageWidth;
@property(nonatomic, strong) UILabel *storageLegend;
@property(nonatomic, strong) UIProgressView *syncProgress;
@property(nonatomic, strong) UILabel *syncStatus;
@property(nonatomic, strong) UIImageView *sitekickImage;
@property(nonatomic, strong) UILabel *sitekickStatus;
@property(nonatomic, strong) UILabel *internetStatus;
@property(nonatomic, strong) RPMusicService *musicService;
@property(nonatomic, strong) RPMusicDiscoveryService *musicDiscovery;
@property(nonatomic, strong) NSArray<NSDictionary *> *musicDiscoveryResults;
@property(nonatomic, strong) UILabel *musicCatalogStatus;
@property(nonatomic, strong) UILabel *musicDiscoveryStatus;
@property(nonatomic, strong) UILabel *musicHealthStatus;
@property(nonatomic, strong) UILabel *musicReplayStatus;
@property(nonatomic) BOOL musicCatalogRequested;
@property(nonatomic, strong) RPMediaServerService *mediaServers;
@property(nonatomic, strong) RPAudioTranscoder *audioTranscoder;
@property(nonatomic, strong) RPMediaServerAccount *selectedMediaAccount;
@property(nonatomic, strong) NSArray<RPMediaPlaylist *> *remotePlaylists;
@property(nonatomic, strong) UIStackView *remotePlaylistStack;
@property(nonatomic, strong) UISegmentedControl *mediaServerTypeControl;
@property(nonatomic, strong) UISegmentedControl *musicQualityControl;
@property(nonatomic, strong) UITextField *mediaServerURLField;
@property(nonatomic, strong) UITextField *mediaServerUsernameField;
@property(nonatomic, strong) UITextField *mediaServerUserIDField;
@property(nonatomic, strong) UITextField *mediaServerSecretField;
@property(nonatomic, strong) UILabel *mediaServerStatus;
@property(nonatomic, strong) AVPlayer *remotePlayer;
@property(nonatomic, strong) RPIPodAudioStreamer *ipodAudioStreamer;
@property(nonatomic, strong) RPLibraryBrowserView *songsBrowser;
@property(nonatomic, strong) RPLibraryBrowserView *artistsBrowser;
@property(nonatomic, strong) RPLibraryBrowserView *albumsBrowser;
@property(nonatomic, strong) RPLibraryBrowserView *videosBrowser;
@property(nonatomic, strong) UIView *sourceDrawer;
@property(nonatomic, strong) UIControl *sourceScrim;
@property(nonatomic, strong) NSLayoutConstraint *sourceDrawerLeading;
@property(nonatomic) BOOL sourceDrawerOpen;
@property(nonatomic, strong) UIButton *playPauseButton;
@property(nonatomic, strong) NSArray<UIButton *> *drawerButtons;
@property(nonatomic, strong) UIButton *safeDisconnectButton;
@property(nonatomic, copy) NSString *phoneNowPlayingTitle;
@property(nonatomic) unsigned long long deviceFreeBytes;
@property(nonatomic, strong) RPGameStoreService *gameStore;
@property(nonatomic, strong) NSArray<RPGameStoreItem *> *gameItems;
@property(nonatomic, strong) UIStackView *gameListStack;
@property(nonatomic, strong) UILabel *gameStatus;
@property(nonatomic, strong) UITextField *gameDownloadURLField;
@property(nonatomic) RPPickerPurpose pickerPurpose;
@property(nonatomic, strong) CLLocationManager *locationManager;
@property(nonatomic) BOOL relayConnected;
@property(nonatomic, copy) NSString *lastKnownMode;
/* Mirror the iPod's own playback onto the phone. */
@property(nonatomic) BOOL mirrorIPodPlayback;
@property(nonatomic, copy) NSString *mirroredTrackPath;
@property(nonatomic) NSTimeInterval lastKnownModeTime;
@property(nonatomic) BOOL locationRequestInFlight;
@property(nonatomic) BOOL locationQueuedForConnection;
@property(nonatomic, strong) CLLocation *bestLocationFix;
@property(nonatomic, strong) NSTimer *locationFixTimer;
@property(atomic) NSInteger pendingSimulatorImports;
@end

@implementation RPRootViewController

static UIColor *RPBlue(void)
{
    return [UIColor colorWithRed:0.08 green:0.29 blue:0.48 alpha:1.0];
}

static UIColor *RPPanel(void)
{
    return [UIColor colorWithRed:0.88 green:0.91 blue:0.95 alpha:1.0];
}

static void RPPutLE16(unsigned char *p, uint16_t value)
{
    p[0] = value & 0xff;
    p[1] = value >> 8;
}

static void RPPutLE32(unsigned char *p, uint32_t value)
{
    p[0] = value & 0xff;
    p[1] = (value >> 8) & 0xff;
    p[2] = (value >> 16) & 0xff;
    p[3] = value >> 24;
}

static uint32_t RPGetBE32(const unsigned char *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}

static UIImage *RPSitekickPreview(NSData *data)
{
    if (data.length < 12 || memcmp(data.bytes, "RPSK", 4))
        return [UIImage imageWithData:data];
    const unsigned char *bytes = data.bytes;
    uint32_t paneLength = RPGetBE32(bytes + 4);
    uint32_t characterLength = RPGetBE32(bytes + 8);
    if (!paneLength || !characterLength ||
        12ull + paneLength + characterLength != data.length)
        return nil;
    UIImage *pane = [UIImage imageWithData:[data subdataWithRange:
        NSMakeRange(12, paneLength)]];
    UIImage *character = [UIImage imageWithData:[data subdataWithRange:
        NSMakeRange(12 + paneLength, characterLength)]];
    if (!pane.CGImage || !character.CGImage)
        return nil;

    size_t width = CGImageGetWidth(character.CGImage);
    size_t height = CGImageGetHeight(character.CGImage);
    unsigned char *pixels = calloc(width * height, 4);
    CGColorSpaceRef color = CGColorSpaceCreateDeviceRGB();
    CGContextRef context = CGBitmapContextCreate(pixels, width, height, 8,
        width * 4, color, kCGBitmapByteOrder32Big | kCGImageAlphaPremultipliedLast);
    CGColorSpaceRelease(color);
    if (!context) { free(pixels); return nil; }
    CGContextDrawImage(context, CGRectMake(0, 0, width, height), character.CGImage);
    for (size_t i = 0; i < width * height; i++)
    {
        unsigned char *pixel = pixels + i * 4;
        if (pixel[0] > 235 && pixel[1] < 30 && pixel[2] > 235)
            pixel[3] = 0;
    }
    CGImageRef keyed = CGBitmapContextCreateImage(context);
    CGContextRelease(context);
    free(pixels);
    UIImage *keyedImage = [UIImage imageWithCGImage:keyed];
    UIGraphicsBeginImageContextWithOptions(CGSizeMake(174, 240), YES, 1.0);
    [pane drawInRect:CGRectMake(0, 0, 174, 240)];
    [keyedImage drawInRect:CGRectMake(10, 40, 154, 139)];
    UIImage *composite = UIGraphicsGetImageFromCurrentImageContext();
    UIGraphicsEndImageContext();
    CGImageRelease(keyed);
    return composite;
}

/* Rockbox Photos discovers /Photos/.photo_thumbs/<name>.bmp sidecars. */
/* Scale a photo down before it is sent to the player.
 *
 * Rockbox decodes a JPEG into a native 16bpp bitmap, and on this target the
 * imageviewer only gets PLUGIN_BUFFER_SIZE (3 MiB) unless playback is stopped.
 * A full-resolution iPhone photo needs width*height*2 bytes -- 1920x1080 is
 * 4 MiB, 1824x2217 is 7.7 MiB -- so it cannot be opened at all, which is why
 * images stopped working right after the first photo sync.  An 800 px long
 * edge decodes in about 1 MiB and is already far beyond the 320x240 LCD. */
#define RP_PHOTO_MAX_EDGE 800.0

static UIImage *RPPhotoForDevice(UIImage *source)
{
    if (!source)
        return source;
    CGFloat w = source.size.width * source.scale;
    CGFloat h = source.size.height * source.scale;
    CGFloat longest = MAX(w, h);
    if (longest <= RP_PHOTO_MAX_EDGE || longest <= 0)
        return source;

    CGFloat scale = RP_PHOTO_MAX_EDGE / longest;
    CGSize target = CGSizeMake(round(w * scale), round(h * scale));
    UIGraphicsImageRendererFormat *format =
        [UIGraphicsImageRendererFormat defaultFormat];
    format.scale = 1;
    format.opaque = YES;
    UIGraphicsImageRenderer *renderer =
        [[UIGraphicsImageRenderer alloc] initWithSize:target format:format];
    return [renderer imageWithActions:
        ^(UIGraphicsImageRendererContext *context) {
        (void)context;
        [source drawInRect:CGRectMake(0, 0, target.width, target.height)];
    }];
}

static NSData *RPPhotoThumbnailBMP(UIImage *source)
{
    /* Match RockPod's working Photos contract: preserve aspect ratio and fit
     * inside the iPod 6G 80x60 thumbnail slot. */
    const CGFloat maxWidth = 80.0, maxHeight = 60.0;
    CGFloat scale = MIN(maxWidth / MAX(source.size.width, 1.0),
                        maxHeight / MAX(source.size.height, 1.0));
    const size_t width = MAX(1, (size_t)floor(source.size.width * scale));
    const size_t height = MAX(1, (size_t)floor(source.size.height * scale));
    const size_t rgbaStride = width * 4;
    const size_t bmpStride = (width * 3 + 3) & ~3;
    unsigned char *rgba = calloc(height, rgbaStride);
    if (!rgba)
        return nil;

    UIGraphicsBeginImageContextWithOptions(CGSizeMake(width, height), YES, 1.0);
    [[UIColor whiteColor] setFill];
    UIRectFill(CGRectMake(0, 0, width, height));
    [source drawInRect:CGRectMake(0, 0, width, height)];
    UIImage *normalized = UIGraphicsGetImageFromCurrentImageContext();
    UIGraphicsEndImageContext();

    CGColorSpaceRef color = CGColorSpaceCreateDeviceRGB();
    CGContextRef context = CGBitmapContextCreate(rgba, width, height, 8,
        rgbaStride, color,
        kCGBitmapByteOrder32Big | kCGImageAlphaPremultipliedLast);
    CGColorSpaceRelease(color);
    if (!context || !normalized.CGImage)
    {
        if (context)
            CGContextRelease(context);
        free(rgba);
        return nil;
    }
    CGContextDrawImage(context, CGRectMake(0, 0, width, height),
                       normalized.CGImage);
    CGContextRelease(context);

    NSMutableData *bmp = [NSMutableData dataWithLength:54 + bmpStride * height];
    unsigned char *bytes = bmp.mutableBytes;
    bytes[0] = 'B'; bytes[1] = 'M';
    RPPutLE32(bytes + 2, (uint32_t)bmp.length);
    RPPutLE32(bytes + 10, 54);
    RPPutLE32(bytes + 14, 40);
    RPPutLE32(bytes + 18, (uint32_t)width);
    RPPutLE32(bytes + 22, (uint32_t)height);
    RPPutLE16(bytes + 26, 1);
    RPPutLE16(bytes + 28, 24);
    RPPutLE32(bytes + 34, (uint32_t)(bmpStride * height));
    for (size_t y = 0; y < height; y++)
    {
        const unsigned char *src = rgba + (height - 1 - y) * rgbaStride;
        unsigned char *dst = bytes + 54 + y * bmpStride;
        for (size_t x = 0; x < width; x++)
        {
            dst[x * 3] = src[x * 4 + 2];
            dst[x * 3 + 1] = src[x * 4 + 1];
            dst[x * 3 + 2] = src[x * 4];
        }
    }
    free(rgba);
    return bmp;
}

- (NSString *)appVersionText
{
    NSDictionary *info = NSBundle.mainBundle.infoDictionary;
    NSString *version = info[@"CFBundleShortVersionString"];
    NSString *build = info[@"CFBundleVersion"];
    if (!version.length)
        version = @"?";
    if (build.length && ![build isEqualToString:version])
        return [NSString stringWithFormat:@"v%@ (%@)", version, build];
    return [NSString stringWithFormat:@"v%@", version];
}

- (void)configurePhonePlaybackAudioSession
{
    NSError *sessionError = nil;
    AVAudioSessionCategoryOptions options =
        AVAudioSessionCategoryOptionMixWithOthers |
        AVAudioSessionCategoryOptionDefaultToSpeaker |
        AVAudioSessionCategoryOptionAllowBluetoothA2DP;
    [AVAudioSession.sharedInstance setCategory:AVAudioSessionCategoryPlayback
                                   withOptions:options
                                         error:&sessionError];
    if (!sessionError)
        [AVAudioSession.sharedInstance setMode:AVAudioSessionModeDefault
                                         error:&sessionError];
    [AVAudioSession.sharedInstance setActive:YES error:&sessionError];
}

- (void)clearPhonePlaybackObservers
{
    NSNotificationCenter *center = NSNotificationCenter.defaultCenter;
    [center removeObserver:self
                      name:AVPlayerItemDidPlayToEndTimeNotification
                    object:nil];
    [center removeObserver:self
                      name:AVPlayerItemFailedToPlayToEndTimeNotification
                    object:nil];
}

- (void)audioSessionInterrupted:(NSNotification *)notification
{
    NSNumber *typeValue = notification.userInfo[AVAudioSessionInterruptionTypeKey];
    if (typeValue.integerValue == AVAudioSessionInterruptionTypeBegan)
    {
        self.playPauseButton.accessibilityValue = @"Paused";
        return;
    }
    NSNumber *optionsValue =
        notification.userInfo[AVAudioSessionInterruptionOptionKey];
    if (!(optionsValue.unsignedIntegerValue &
          AVAudioSessionInterruptionOptionShouldResume))
        return;
    if (!self.remotePlayer.currentItem)
        return;
    [self configurePhonePlaybackAudioSession];
    [self.remotePlayer play];
    self.playPauseButton.accessibilityValue = @"Playing";
}

- (UILabel *)label:(NSString *)text size:(CGFloat)size bold:(BOOL)bold
{
    UILabel *label = [[UILabel alloc] init];
    label.text = text;
    label.font = bold ? [UIFont boldSystemFontOfSize:size] :
                        [UIFont systemFontOfSize:size];
    label.textColor = [UIColor colorWithWhite:0.12 alpha:1.0];
    label.numberOfLines = 0;
    label.translatesAutoresizingMaskIntoConstraints = NO;
    return label;
}

- (UIButton *)button:(NSString *)title action:(SEL)action
{
    UIButton *button = [UIButton buttonWithType:UIButtonTypeCustom];
    [button setTitle:title forState:UIControlStateNormal];
    [button setTitleColor:[UIColor colorWithWhite:0.12 alpha:1.0]
                 forState:UIControlStateNormal];
    button.titleLabel.font = [UIFont boldSystemFontOfSize:13.0];
    UIImage *buttonTexture = [[UIImage imageNamed:@"Legacy/itunes7-status-texture.png"]
        resizableImageWithCapInsets:UIEdgeInsetsMake(4, 4, 4, 4)];
    [button setBackgroundImage:buttonTexture forState:UIControlStateNormal];
    button.layer.borderColor = [UIColor colorWithWhite:0.43 alpha:1.0].CGColor;
    button.layer.borderWidth = 1.0;
    button.layer.cornerRadius = 2.0;
    button.contentEdgeInsets = UIEdgeInsetsMake(11, 12, 11, 12);
    [button setTitleShadowColor:UIColor.whiteColor forState:UIControlStateNormal];
    button.titleLabel.shadowOffset = CGSizeMake(0, 1);
    [button setTitleColor:[UIColor colorWithWhite:0.55 alpha:1.0]
                 forState:UIControlStateDisabled];
    [button.heightAnchor constraintGreaterThanOrEqualToConstant:44].active = YES;
    [button addTarget:self action:action forControlEvents:UIControlEventTouchUpInside];
    return button;
}

- (UIView *)panelWithTitle:(NSString *)title content:(UIView *)content
{
    UIView *panel = [[UIView alloc] init];
    panel.backgroundColor = [UIColor whiteColor];
    panel.layer.borderColor = [UIColor colorWithWhite:0.67 alpha:1.0].CGColor;
    panel.layer.borderWidth = 1.0;
    panel.layer.cornerRadius = 0;
    UIImageView *headingBar = [[UIImageView alloc] initWithImage:
        [[UIImage imageNamed:@"Legacy/itunes7-status-texture.png"]
         resizableImageWithCapInsets:UIEdgeInsetsMake(3, 3, 3, 3)]];
    headingBar.translatesAutoresizingMaskIntoConstraints = NO;
    [panel addSubview:headingBar];
    UILabel *heading = [self label:title size:13 bold:YES];
    heading.textColor = [UIColor colorWithWhite:0.32 alpha:1.0];
    [panel addSubview:heading];
    content.translatesAutoresizingMaskIntoConstraints = NO;
    [panel addSubview:content];
    [NSLayoutConstraint activateConstraints:@[
        [headingBar.leadingAnchor constraintEqualToAnchor:panel.leadingAnchor],
        [headingBar.trailingAnchor constraintEqualToAnchor:panel.trailingAnchor],
        [headingBar.topAnchor constraintEqualToAnchor:panel.topAnchor],
        [headingBar.heightAnchor constraintEqualToConstant:34],
        [heading.leadingAnchor constraintEqualToAnchor:panel.leadingAnchor constant:14],
        [heading.trailingAnchor constraintEqualToAnchor:panel.trailingAnchor constant:-14],
        [heading.centerYAnchor constraintEqualToAnchor:headingBar.centerYAnchor],
        [content.leadingAnchor constraintEqualToAnchor:panel.leadingAnchor constant:14],
        [content.trailingAnchor constraintEqualToAnchor:panel.trailingAnchor constant:-14],
        [content.topAnchor constraintEqualToAnchor:headingBar.bottomAnchor constant:12],
        [content.bottomAnchor constraintEqualToAnchor:panel.bottomAnchor constant:-14],
    ]];
    return panel;
}

- (UIScrollView *)pageForStack:(UIStackView *)stack
{
    UIScrollView *scroll = [[UIScrollView alloc] init];
    scroll.backgroundColor = RPPanel();
    stack.axis = UILayoutConstraintAxisVertical;
    stack.spacing = 8;
    stack.layoutMargins = UIEdgeInsetsMake(10, 10, 18, 10);
    stack.layoutMarginsRelativeArrangement = YES;
    stack.translatesAutoresizingMaskIntoConstraints = NO;
    [scroll addSubview:stack];
    [NSLayoutConstraint activateConstraints:@[
        [stack.leadingAnchor constraintEqualToAnchor:scroll.contentLayoutGuide.leadingAnchor],
        [stack.trailingAnchor constraintEqualToAnchor:scroll.contentLayoutGuide.trailingAnchor],
        [stack.topAnchor constraintEqualToAnchor:scroll.contentLayoutGuide.topAnchor],
        [stack.bottomAnchor constraintEqualToAnchor:scroll.contentLayoutGuide.bottomAnchor],
        [stack.widthAnchor constraintEqualToAnchor:scroll.frameLayoutGuide.widthAnchor],
    ]];
    return scroll;
}

- (UIView *)summaryPage
{
    UIStackView *stack = [[UIStackView alloc] init];
    UIStackView *identity = [[UIStackView alloc] init];
    identity.axis = UILayoutConstraintAxisHorizontal;
    identity.spacing = 14;
    identity.alignment = UIStackViewAlignmentCenter;
    UIImageView *device = [[UIImageView alloc] initWithImage:
        [UIImage imageNamed:@"Legacy/ipod-plugged-2007.png"]];
    device.contentMode = UIViewContentModeScaleAspectFit;
    [device.widthAnchor constraintEqualToConstant:86].active = YES;
    [device.heightAnchor constraintEqualToConstant:132].active = YES;
    UIStackView *names = [[UIStackView alloc] init];
    names.axis = UILayoutConstraintAxisVertical;
    names.spacing = 3;
    self.deviceNameLabel = [self label:@"Apple iPod" size:20 bold:YES];
    self.modeLabel = [self label:@"Disconnected or Storage Mode" size:13 bold:YES];
    self.modeLabel.textColor = [UIColor colorWithRed:0.48 green:0.21 blue:0.07 alpha:1.0];
    [names addArrangedSubview:self.deviceNameLabel];
    [names addArrangedSubview:self.modeLabel];
    [identity addArrangedSubview:device];
    [identity addArrangedSubview:names];
    [stack addArrangedSubview:[self panelWithTitle:@"DEVICE" content:identity]];

    UIStackView *details = [[UIStackView alloc] init];
    details.axis = UILayoutConstraintAxisVertical;
    details.spacing = 7;
    self.firmwareLabel = [self label:@"Software: —" size:14 bold:NO];
    self.batteryLabel = [self label:@"Battery: —" size:14 bold:NO];
    self.capacityLabel = [self label:@"Capacity: —" size:14 bold:NO];
    self.nowPlayingLabel = [self label:@"Now Playing: Nothing" size:14 bold:NO];
    [details addArrangedSubview:self.firmwareLabel];
    [details addArrangedSubview:self.batteryLabel];
    [details addArrangedSubview:self.capacityLabel];
    [details addArrangedSubview:self.nowPlayingLabel];
    [stack addArrangedSubview:[self panelWithTitle:@"SUMMARY" content:details]];

    UIStackView *storage = [[UIStackView alloc] init];
    storage.axis = UILayoutConstraintAxisVertical;
    storage.spacing = 8;
    UIView *bar = [[UIView alloc] init];
    bar.backgroundColor = [UIColor colorWithWhite:0.84 alpha:1.0];
    bar.layer.borderColor = [UIColor colorWithWhite:0.46 alpha:1.0].CGColor;
    bar.layer.borderWidth = 1.0;
    bar.layer.cornerRadius = 0;
    [bar.heightAnchor constraintEqualToConstant:20].active = YES;
    self.usedStorage = [[UIView alloc] init];
    self.usedStorage.backgroundColor = [UIColor colorWithRed:0.15 green:0.55 blue:0.78 alpha:1.0];
    self.usedStorage.layer.cornerRadius = 0;
    self.usedStorage.translatesAutoresizingMaskIntoConstraints = NO;
    [bar addSubview:self.usedStorage];
    self.usedStorageWidth = [self.usedStorage.widthAnchor
        constraintEqualToAnchor:bar.widthAnchor multiplier:0.001 constant:-4];
    [NSLayoutConstraint activateConstraints:@[
        [self.usedStorage.leadingAnchor constraintEqualToAnchor:bar.leadingAnchor constant:2],
        [self.usedStorage.topAnchor constraintEqualToAnchor:bar.topAnchor constant:2],
        [self.usedStorage.bottomAnchor constraintEqualToAnchor:bar.bottomAnchor constant:-2],
        self.usedStorageWidth,
    ]];
    self.storageLegend = [self label:@"Connect in iPhone mode to read storage" size:12 bold:NO];
    [storage addArrangedSubview:bar];
    [storage addArrangedSubview:self.storageLegend];
    [stack addArrangedSubview:[self panelWithTitle:@"CAPACITY" content:storage]];

    UIStackView *quickActions = [[UIStackView alloc] init];
    quickActions.axis = UILayoutConstraintAxisHorizontal;
    quickActions.distribution = UIStackViewDistributionFillEqually;
    quickActions.spacing = 8;
    [quickActions addArrangedSubview:[self button:@"Retry Connection" action:@selector(retryConnection)]];
    [quickActions addArrangedSubview:[self button:@"Refresh Library" action:@selector(refreshMusicCatalog)]];
    [stack addArrangedSubview:quickActions];
    self.safeDisconnectButton = [self button:@"Safely Disconnect iPod" action:@selector(safeDisconnect)];
    self.safeDisconnectButton.enabled = NO;
    [stack addArrangedSubview:self.safeDisconnectButton];
    return [self pageForStack:stack];
}

- (RPLibraryBrowserView *)libraryPage:(RPLibraryBrowserKind)kind
{
    RPLibraryBrowserView *view = [[RPLibraryBrowserView alloc] initWithKind:kind];
    if (kind != RPLibraryBrowserVideos)
    {
        __weak typeof(self) weakSelf = self;
        view.trackSelected = ^(RPMusicTrack *track) { [weakSelf playIPodTrack:track]; };
        view.trackDownloadRequested = ^(RPMusicTrack *track) {
            [weakSelf downloadIPodTrack:track];
        };
        view.artworkRequested = ^(RPMusicTrack *track,
                                   void (^completion)(UIImage *image)) {
            [weakSelf loadArtworkForIPodTrack:track completion:completion];
        };
    }
    return view;
}

- (UIView *)syncPage
{
    UIStackView *stack = [[UIStackView alloc] init];
    UILabel *intro = [self label:@"Sync local iPhone media over the USB link. Photos go directly to /Photos with Rockbox-ready thumbnails; music and videos stay organized in RockPodLink folders." size:14 bold:NO];
    [stack addArrangedSubview:[self panelWithTitle:@"SYNC FROM THIS iPHONE" content:intro]];
    [stack addArrangedSubview:[self button:@"Add Photos…" action:@selector(addPhotos)]];
    [stack addArrangedSubview:[self button:@"Add Videos…" action:@selector(addVideos)]];
    [stack addArrangedSubview:[self button:@"Add Music Library Tracks…" action:@selector(addMusic)]];
    [stack addArrangedSubview:[self button:@"Add Audio or Video Files…" action:@selector(addDocuments)]];
    UIStackView *progress = [[UIStackView alloc] init];
    progress.axis = UILayoutConstraintAxisVertical;
    progress.spacing = 8;
    self.syncProgress = [[UIProgressView alloc] initWithProgressViewStyle:UIProgressViewStyleDefault];
    self.syncProgress.progressTintColor = RPBlue();
    self.syncStatus = [self label:@"No transfers queued" size:13 bold:NO];
    [progress addArrangedSubview:self.syncProgress];
    [progress addArrangedSubview:self.syncStatus];
    [stack addArrangedSubview:[self panelWithTitle:@"SYNC STATUS" content:progress]];
    return [self pageForStack:stack];
}

- (UIView *)sitekickPage
{
    UIStackView *stack = [[UIStackView alloc] init];
    self.sitekickImage = [[UIImageView alloc] init];
    self.sitekickImage.backgroundColor = [UIColor colorWithRed:0.03 green:0.05 blue:0.12 alpha:1.0];
    self.sitekickImage.contentMode = UIViewContentModeScaleAspectFit;
    self.sitekickImage.layer.cornerRadius = 0;
    self.sitekickImage.clipsToBounds = YES;
    [self.sitekickImage.heightAnchor constraintEqualToConstant:270].active = YES;
    [stack addArrangedSubview:self.sitekickImage];
    self.sitekickStatus = [self label:@"Connect in iPhone mode, then refresh to see the exact current Sitekick." size:13 bold:NO];
    [stack addArrangedSubview:self.sitekickStatus];
    [stack addArrangedSubview:[self button:@"Refresh Current Sitekick" action:@selector(refreshSitekick)]];
    return [self pageForStack:stack];
}

- (UIView *)musicPage
{
    UIStackView *stack = [[UIStackView alloc] init];
    UILabel *intro = [self label:@"RockPod Link reads the iPod’s real tagcache catalog and listening history over USB. Analysis stays on this iPhone; playback and the active iPod playlist are never interrupted." size:13 bold:NO];
    [stack addArrangedSubview:[self panelWithTitle:@"MUSIC INTELLIGENCE" content:intro]];
    self.musicCatalogStatus = [self label:@"Connect the iPod to analyze its music library." size:13 bold:YES];
    [stack addArrangedSubview:[self panelWithTitle:@"IPOD LIBRARY" content:self.musicCatalogStatus]];
    [stack addArrangedSubview:[self button:@"Refresh Music Library" action:@selector(refreshMusicCatalog)]];
    self.musicDiscoveryStatus = [self label:@"Recommendations appear after the first catalog sync." size:13 bold:NO];
    [stack addArrangedSubview:[self panelWithTitle:@"DISCOVER SIMILAR MUSIC" content:self.musicDiscoveryStatus]];
    [stack addArrangedSubview:[self button:@"Check New Releases & Concerts" action:@selector(checkMusicDiscovery)]];
    self.musicHealthStatus = [self label:@"Missing-track and duplicate reports appear here." size:13 bold:NO];
    [stack addArrangedSubview:[self panelWithTitle:@"LIBRARY HEALTH" content:self.musicHealthStatus]];
    [stack addArrangedSubview:[self button:@"Review Library Health" action:@selector(reviewMusicHealth)]];
    self.musicReplayStatus = [self label:@"RockPod Replay is calculated from iPod play counts and listening time." size:13 bold:NO];
    [stack addArrangedSubview:[self panelWithTitle:@"ROCKPOD REPLAY" content:self.musicReplayStatus]];
    [stack addArrangedSubview:[self button:@"Export This Year’s Replay" action:@selector(exportMusicReplay)]];

    self.mediaServers = [[RPMediaServerService alloc] init];
    self.musicDiscovery = [[RPMusicDiscoveryService alloc] init];
    self.audioTranscoder = [[RPAudioTranscoder alloc] init];
    UIStackView *server = [[UIStackView alloc] init];
    server.axis = UILayoutConstraintAxisVertical;
    server.spacing = 7;
    self.mediaServerTypeControl = [[UISegmentedControl alloc]
        initWithItems:@[@"Plex", @"Jellyfin", @"Subsonic", @"Personal"]];
    self.mediaServerTypeControl.selectedSegmentIndex = 2;
    [server addArrangedSubview:self.mediaServerTypeControl];
    self.mediaServerURLField = [self musicTextField:@"Server URL" secure:NO];
    self.mediaServerUsernameField = [self musicTextField:@"Username" secure:NO];
    self.mediaServerUserIDField = [self musicTextField:@"User ID (Jellyfin only)" secure:NO];
    self.mediaServerSecretField = [self musicTextField:@"Password or access token" secure:YES];
    [server addArrangedSubview:self.mediaServerURLField];
    [server addArrangedSubview:self.mediaServerUsernameField];
    [server addArrangedSubview:self.mediaServerUserIDField];
    [server addArrangedSubview:self.mediaServerSecretField];
    [stack addArrangedSubview:[self panelWithTitle:@"PLEX • JELLYFIN • NAVIDROME • SUBSONIC • PERSONAL SERVER"
                                              content:server]];
    [stack addArrangedSubview:[self button:@"Save, Test & Load Playlists"
                                    action:@selector(saveAndLoadMediaServer)]];
    self.musicQualityControl = [[UISegmentedControl alloc]
        initWithItems:@[@"96", @"128", @"192", @"256", @"Original"]];
    NSNumber *savedQuality = [NSUserDefaults.standardUserDefaults objectForKey:@"RPMusicQualityIndex"];
    NSInteger qualityIndex = savedQuality ? savedQuality.integerValue : 2;
    self.musicQualityControl.selectedSegmentIndex = MIN(4, MAX(0, qualityIndex));
    [self.musicQualityControl addTarget:self action:@selector(musicQualityChanged:)
                      forControlEvents:UIControlEventValueChanged];
    [stack addArrangedSubview:[self panelWithTitle:@"OFFLINE QUALITY (KBPS)"
                                              content:self.musicQualityControl]];
    self.mediaServerStatus = [self label:@"Add a server to stream, cache, and sync its playlists." size:12 bold:NO];
    [stack addArrangedSubview:self.mediaServerStatus];
    self.remotePlaylistStack = [[UIStackView alloc] init];
    self.remotePlaylistStack.axis = UILayoutConstraintAxisVertical;
    self.remotePlaylistStack.spacing = 4;
    [stack addArrangedSubview:self.remotePlaylistStack];
    [stack addArrangedSubview:[self button:@"Sync Audiobook Positions"
                                    action:@selector(syncAudiobookPositions)]];
    RPMediaServerAccount *saved = self.mediaServers.savedAccounts.firstObject;
    if (saved) [self populateMediaAccount:saved];
    return [self pageForStack:stack];
}

- (UIView *)gamesPage
{
    UIStackView *stack = [[UIStackView alloc] init];
    UILabel *intro = [self label:@"Choose an existing game file, or enter any website and browse it normally. Tap a game download on the site and RockPod Link will download it, unpack ZIPs, identify the emulator, fetch metadata and cover art, and sync it to the iPod." size:13 bold:NO];
    [stack addArrangedSubview:[self panelWithTitle:@"ADD GAMES" content:intro]];
    [stack addArrangedSubview:[self button:@"Choose Downloaded Game Files…" action:@selector(chooseGameFiles)]];
    self.gameDownloadURLField = [[UITextField alloc] init];
    self.gameDownloadURLField.translatesAutoresizingMaskIntoConstraints = NO;
    self.gameDownloadURLField.borderStyle = UITextBorderStyleNone;
    self.gameDownloadURLField.backgroundColor = UIColor.whiteColor;
    self.gameDownloadURLField.layer.borderWidth = 1.0;
    self.gameDownloadURLField.layer.borderColor = [UIColor colorWithWhite:0.48 alpha:1].CGColor;
    self.gameDownloadURLField.font = [UIFont systemFontOfSize:13];
    self.gameDownloadURLField.placeholder = @"https://any-game-website.example";
    self.gameDownloadURLField.keyboardType = UIKeyboardTypeURL;
    self.gameDownloadURLField.autocapitalizationType = UITextAutocapitalizationTypeNone;
    self.gameDownloadURLField.autocorrectionType = UITextAutocorrectionTypeNo;
    self.gameDownloadURLField.text = [NSUserDefaults.standardUserDefaults
        stringForKey:@"RPGameWebsiteURL"] ?: @"";
    self.gameDownloadURLField.leftView = [[UIView alloc] initWithFrame:CGRectMake(0,0,7,1)];
    self.gameDownloadURLField.leftViewMode = UITextFieldViewModeAlways;
    [self.gameDownloadURLField.heightAnchor constraintEqualToConstant:34].active = YES;
    [stack addArrangedSubview:[self panelWithTitle:@"GAME WEBSITE" content:self.gameDownloadURLField]];
    [stack addArrangedSubview:[self button:@"Open Game Website" action:@selector(openGameWebsite)]];
    self.gameStatus = [self label:@"Supported: Game Boy, GBC, NES, SMS, Game Gear, Game & Watch, SNES and Genesis." size:12 bold:NO];
    [stack addArrangedSubview:self.gameStatus];
    self.gameListStack = [[UIStackView alloc] init];
    self.gameListStack.axis = UILayoutConstraintAxisVertical;
    self.gameListStack.spacing = 1;
    [stack addArrangedSubview:self.gameListStack];
    return [self pageForStack:stack];
}

- (UIView *)settingsPage
{
    UIStackView *stack = [[UIStackView alloc] init];
    UILabel *note = [self label:@"RockPod Link automatically reconnects to the iPod, keeps the USB internet relay alive, and stages media without replacing the Rockbox database." size:13 bold:NO];
    [stack addArrangedSubview:[self panelWithTitle:@"CONNECTION" content:note]];
    UILabel *games = [self label:@"Games no longer require a store website. Add a downloaded file or a direct game URL from the Games tab." size:13 bold:NO];
    [stack addArrangedSubview:[self panelWithTitle:@"GAME SYNC" content:games]];
    return [self pageForStack:stack];
}

- (UIView *)simulatorPage
{
    UIStackView *stack = [[UIStackView alloc] init];
    UILabel *title = [self label:@"Complete iPod 6G Rockbox" size:22 bold:YES];
    UILabel *note = [self label:@"This launches the actual current Rockbox simulator core, iPodJS engine, themes, settings, database, codecs and plugins from a writable local install. It is not a screenshot or a UIKit recreation." size:14 bold:NO];
    [stack addArrangedSubview:[self panelWithTitle:@"ROCKBOX SIMULATOR" content:title]];
    [stack addArrangedSubview:note];
    [stack addArrangedSubview:[self button:@"Open Full Rockbox Simulator"
                                    action:@selector(launchSimulator)]];
    UILabel *media = [self label:@"The simulator opens immediately without copying your iPhone library. Use Add Local for individual items that Rockbox should access as ordinary files; they never overwrite the connected iPod." size:13 bold:NO];
    [stack addArrangedSubview:media];
    [stack addArrangedSubview:[self button:@"Add Local Photo…" action:@selector(previewPhoto)]];
    [stack addArrangedSubview:[self button:@"Add Local Video…" action:@selector(previewVideo)]];
    [stack addArrangedSubview:[self button:@"Add Local Music…" action:@selector(previewMusic)]];
    return [self pageForStack:stack];
}

- (void)launchSimulator
{
    if (self.pendingSimulatorImports > 0)
    {
        self.footerLabel.text = @"Finishing selected simulator media…";
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 250 * NSEC_PER_MSEC),
                       dispatch_get_main_queue(), ^{
            [self launchSimulator];
        });
        return;
    }
    self.footerLabel.text = @"Preparing complete Rockbox simulator…";
    [[RPSimulatorHost sharedHost] startWithCompletion:^(BOOL success,
                                                         NSString *message) {
        self.footerLabel.text = message;
        if (!success)
            self.displayLabel.text = @"Simulator could not start";
    }];
}

- (UIView *)internetPage
{
    UIStackView *stack = [[UIStackView alloc] init];
    UIImageView *controls = [[UIImageView alloc] initWithImage:
        [UIImage imageNamed:@"Legacy/itunes7-playback-controls.png"]];
    controls.contentMode = UIViewContentModeScaleAspectFit;
    [controls.heightAnchor constraintEqualToConstant:64].active = YES;
    [stack addArrangedSubview:controls];
    self.internetStatus = [self label:@"Internet relay is waiting for iPhone USB Link mode." size:15 bold:YES];
    [stack addArrangedSubview:[self panelWithTitle:@"INTERNET & BACKGROUND LINK" content:self.internetStatus]];
    UILabel *detail = [self label:@"The companion keeps weather, live Safari rendering, device status and sync on one persistent USB service. iOS may briefly suspend ordinary network work; RockPod Link requests background execution and resumes automatically." size:14 bold:NO];
    [stack addArrangedSubview:detail];
    return [self pageForStack:stack];
}

- (void)viewDidLoad
{
    [super viewDidLoad];
    self.title = @"RockPod";
    self.view.backgroundColor = RPPanel();
    self.navigationController.navigationBarHidden = YES;

    UIImage *toolbar = [[UIImage imageNamed:@"Legacy/itunes7-toolbar-texture.png"]
        resizableImageWithCapInsets:UIEdgeInsetsMake(4, 4, 4, 4)];
    UIImageView *header = [[UIImageView alloc] initWithImage:toolbar];
    header.userInteractionEnabled = YES;
    header.translatesAutoresizingMaskIntoConstraints = NO;
    UIImageView *controls = [[UIImageView alloc] initWithImage:
        [UIImage imageNamed:@"Legacy/itunes7-playback-controls.png"]];
    controls.contentMode = UIViewContentModeScaleAspectFit;
    controls.translatesAutoresizingMaskIntoConstraints = NO;
    [header addSubview:controls];
    UIButton *menuButton = [UIButton buttonWithType:UIButtonTypeCustom];
    menuButton.translatesAutoresizingMaskIntoConstraints = NO;
    [menuButton setTitle:@"Sources" forState:UIControlStateNormal];
    [menuButton setTitleColor:[UIColor colorWithWhite:0.16 alpha:1.0]
                     forState:UIControlStateNormal];
    [menuButton setTitleShadowColor:UIColor.whiteColor forState:UIControlStateNormal];
    menuButton.titleLabel.shadowOffset = CGSizeMake(0, 1);
    menuButton.titleLabel.font = [UIFont boldSystemFontOfSize:12];
    menuButton.accessibilityHint = @"Opens the iTunes-style source list";
    [menuButton addTarget:self action:@selector(toggleSourceDrawer) forControlEvents:UIControlEventTouchUpInside];
    [header addSubview:menuButton];
    /* The source artwork already contains the real 2007 iTunes play button.
     * Keep only a transparent hit target over it; a UIButtonTypeSystem title
     * was painting the unwanted blue play glyph on top of the asset. */
    self.playPauseButton = [UIButton buttonWithType:UIButtonTypeCustom];
    self.playPauseButton.translatesAutoresizingMaskIntoConstraints = NO;
    self.playPauseButton.backgroundColor = UIColor.clearColor;
    self.playPauseButton.accessibilityLabel = @"Play or pause iPhone playback";
    [self.playPauseButton addTarget:self action:@selector(togglePhonePlayback) forControlEvents:UIControlEventTouchUpInside];
    [header addSubview:self.playPauseButton];
    self.displayLabel = [self label:@"RockPod Link — Starting…" size:14 bold:YES];
    self.displayLabel.textAlignment = NSTextAlignmentCenter;
    self.displayLabel.backgroundColor = [UIColor colorWithRed:0.94 green:0.96 blue:0.82 alpha:1.0];
    self.displayLabel.layer.cornerRadius = 2.0;
    self.displayLabel.layer.borderColor = [UIColor colorWithWhite:0.46 alpha:1.0].CGColor;
    self.displayLabel.layer.borderWidth = 1.0;
    self.displayLabel.clipsToBounds = YES;
    [header addSubview:self.displayLabel];

    self.sourceScroll = [[UIScrollView alloc] init];
    self.sourceScroll.translatesAutoresizingMaskIntoConstraints = NO;
    self.sourceScroll.showsHorizontalScrollIndicator = YES;
    self.sourceScroll.alwaysBounceHorizontal = YES;
    self.sourceScroll.backgroundColor = [UIColor colorWithWhite:0.82 alpha:1.0];
    self.sourceScroll.directionalLockEnabled = YES;
    self.sourceTabs = [[UIStackView alloc] init];
    self.sourceTabs.translatesAutoresizingMaskIntoConstraints = NO;
    self.sourceTabs.axis = UILayoutConstraintAxisHorizontal;
    self.sourceTabs.spacing = 2;
    NSArray<NSString *> *tabNames = @[@"Summary", @"Sync", @"Music", @"Games",
                                      @"Settings", @"Sitekick", @"Simulator", @"Internet"];
    NSMutableArray *tabButtons = [NSMutableArray array];
    [tabNames enumerateObjectsUsingBlock:^(NSString *name, NSUInteger index, BOOL *stop) {
        (void)stop;
        UIButton *tab = [UIButton buttonWithType:UIButtonTypeSystem];
        tab.tag = (NSInteger)index;
        tab.titleLabel.font = [UIFont boldSystemFontOfSize:12];
        [tab setTitle:name forState:UIControlStateNormal];
        [tab addTarget:self action:@selector(sourceTabPressed:)
              forControlEvents:UIControlEventTouchUpInside];
        [tab.widthAnchor constraintEqualToConstant:index == 6 ? 86 : 76].active = YES;
        [self.sourceTabs addArrangedSubview:tab];
        [tabButtons addObject:tab];
    }];
    self.sourceButtons = tabButtons;
    [self.sourceScroll addSubview:self.sourceTabs];
    self.pageHost = [[UIView alloc] init];
    self.pageHost.translatesAutoresizingMaskIntoConstraints = NO;

    UIImage *statusTexture = [[UIImage imageNamed:@"Legacy/itunes7-status-texture.png"]
        resizableImageWithCapInsets:UIEdgeInsetsMake(3, 3, 3, 3)];
    UIImageView *footer = [[UIImageView alloc] initWithImage:statusTexture];
    footer.userInteractionEnabled = YES;
    footer.translatesAutoresizingMaskIntoConstraints = NO;
    self.footerLabel = [self label:@"Waiting for iPod" size:11 bold:NO];
    self.footerLabel.textAlignment = NSTextAlignmentLeft;
    [footer addSubview:self.footerLabel];
    self.footerVersionLabel = [self label:[self appVersionText] size:11 bold:YES];
    self.footerVersionLabel.textAlignment = NSTextAlignmentRight;
    self.footerVersionLabel.textColor = [UIColor colorWithWhite:0.35 alpha:1.0];
    [footer addSubview:self.footerVersionLabel];

    [self.view addSubview:header];
    [self.view addSubview:self.sourceScroll];
    [self.view addSubview:self.pageHost];
    [self.view addSubview:footer];
    UILayoutGuide *safe = self.view.safeAreaLayoutGuide;
    [NSLayoutConstraint activateConstraints:@[
        [header.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
        [header.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],
        [header.topAnchor constraintEqualToAnchor:self.view.topAnchor],
        [header.bottomAnchor constraintEqualToAnchor:safe.topAnchor constant:72],
        [menuButton.leadingAnchor constraintEqualToAnchor:header.leadingAnchor constant:8],
        [menuButton.bottomAnchor constraintEqualToAnchor:header.bottomAnchor constant:-9],
        [menuButton.widthAnchor constraintEqualToConstant:58],
        [menuButton.heightAnchor constraintEqualToConstant:36],
        [controls.leadingAnchor constraintEqualToAnchor:menuButton.trailingAnchor constant:2],
        [controls.bottomAnchor constraintEqualToAnchor:header.bottomAnchor constant:-6],
        [controls.widthAnchor constraintEqualToConstant:98],
        [controls.heightAnchor constraintEqualToConstant:46],
        [self.playPauseButton.centerXAnchor constraintEqualToAnchor:controls.centerXAnchor],
        [self.playPauseButton.centerYAnchor constraintEqualToAnchor:controls.centerYAnchor],
        [self.playPauseButton.widthAnchor constraintEqualToConstant:36],
        [self.playPauseButton.heightAnchor constraintEqualToConstant:36],
        [self.displayLabel.leadingAnchor constraintEqualToAnchor:controls.trailingAnchor constant:6],
        [self.displayLabel.trailingAnchor constraintEqualToAnchor:header.trailingAnchor constant:-10],
        [self.displayLabel.bottomAnchor constraintEqualToAnchor:header.bottomAnchor constant:-10],
        [self.displayLabel.heightAnchor constraintEqualToConstant:36],
        [self.sourceScroll.leadingAnchor constraintEqualToAnchor:safe.leadingAnchor],
        [self.sourceScroll.trailingAnchor constraintEqualToAnchor:safe.trailingAnchor],
        [self.sourceScroll.topAnchor constraintEqualToAnchor:header.bottomAnchor],
        [self.sourceScroll.heightAnchor constraintEqualToConstant:0],
        [self.sourceTabs.leadingAnchor constraintEqualToAnchor:self.sourceScroll.contentLayoutGuide.leadingAnchor constant:6],
        [self.sourceTabs.trailingAnchor constraintEqualToAnchor:self.sourceScroll.contentLayoutGuide.trailingAnchor constant:-6],
        [self.sourceTabs.topAnchor constraintEqualToAnchor:self.sourceScroll.contentLayoutGuide.topAnchor constant:4],
        [self.sourceTabs.bottomAnchor constraintEqualToAnchor:self.sourceScroll.contentLayoutGuide.bottomAnchor constant:-4],
        [self.sourceTabs.heightAnchor constraintEqualToConstant:28],
        [self.pageHost.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
        [self.pageHost.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],
        [self.pageHost.topAnchor constraintEqualToAnchor:self.sourceScroll.bottomAnchor],
        [self.pageHost.bottomAnchor constraintEqualToAnchor:footer.topAnchor],
        [footer.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
        [footer.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],
        [footer.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],
        [footer.heightAnchor constraintEqualToConstant:28],
        [self.footerLabel.leadingAnchor constraintEqualToAnchor:footer.leadingAnchor
                                                       constant:8],
        [self.footerLabel.trailingAnchor constraintLessThanOrEqualToAnchor:
            self.footerVersionLabel.leadingAnchor constant:-8],
        [self.footerLabel.centerYAnchor constraintEqualToAnchor:footer.centerYAnchor],
        [self.footerVersionLabel.trailingAnchor constraintEqualToAnchor:
            footer.trailingAnchor constant:-8],
        [self.footerVersionLabel.centerYAnchor constraintEqualToAnchor:
            footer.centerYAnchor],
    ]];

    self.songsBrowser = [self libraryPage:RPLibraryBrowserSongs];
    self.artistsBrowser = [self libraryPage:RPLibraryBrowserArtists];
    self.albumsBrowser = [self libraryPage:RPLibraryBrowserAlbums];
    self.videosBrowser = [self libraryPage:RPLibraryBrowserVideos];
    self.musicService = [[RPMusicService alloc] init];
    self.pages = @[[self summaryPage], self.songsBrowser, self.artistsBrowser,
                   self.albumsBrowser, self.videosBrowser, [self syncPage],
                   [self musicPage], [self gamesPage], [self settingsPage],
                   [self sitekickPage], [self simulatorPage], [self internetPage]];
    [self updateLibraryBrowsers];
    if (self.musicService.catalog.count || self.musicService.videos.count)
    {
        [self updateMusicAnalysis];
        self.musicCatalogStatus.text = [NSString stringWithFormat:
            @"Cached iPod library • %lu songs • %lu videos — refreshing when connected",
            (unsigned long)self.musicService.catalog.count,
            (unsigned long)self.musicService.videos.count];
    }
    [self showPage:0];
    [self installSourceDrawerBelowHeader:header aboveFooter:footer];
    self.locationManager = [[CLLocationManager alloc] init];
    self.locationManager.delegate = self;
    self.locationManager.desiredAccuracy = kCLLocationAccuracyBest;
    self.locationManager.distanceFilter = kCLDistanceFilterNone;
    self.relay = [RPRelayService sharedService];
    self.mirrorIPodPlayback = YES;
    [self configurePhonePlaybackAudioSession];
    [NSNotificationCenter.defaultCenter addObserver:self
                                           selector:@selector(audioSessionInterrupted:)
                                               name:AVAudioSessionInterruptionNotification
                                             object:nil];
    [self attachRelayHandlers];
    [self.relay start];
}

- (void)dealloc
{
    [self clearPhonePlaybackObservers];
    [NSNotificationCenter.defaultCenter removeObserver:self
                                                  name:AVAudioSessionInterruptionNotification
                                                object:nil];
}

- (void)requestPreciseLocationForConnectedIPod
{
    if (!self.relayConnected || self.locationQueuedForConnection ||
        self.locationRequestInFlight)
        return;

    CLAuthorizationStatus authorization =
        [CLLocationManager authorizationStatus];
    self.locationRequestInFlight = YES;
    if (authorization == kCLAuthorizationStatusNotDetermined)
    {
        [self.locationManager requestWhenInUseAuthorization];
        return;
    }
    if (authorization != kCLAuthorizationStatusAuthorizedWhenInUse &&
        authorization != kCLAuthorizationStatusAuthorizedAlways)
    {
        self.locationRequestInFlight = NO;
        self.syncStatus.text = @"Enable Location for RockPod Link to update iPod Maps.";
        return;
    }
    self.bestLocationFix = nil;
    [self.locationManager startUpdatingLocation];
    [self.locationFixTimer invalidate];
    self.locationFixTimer = [NSTimer scheduledTimerWithTimeInterval:12.0
        target:self selector:@selector(finishLocationFix) userInfo:nil repeats:NO];
}

- (void)locationManagerDidChangeAuthorization:(CLLocationManager *)manager
{
    CLAuthorizationStatus authorization =
        [CLLocationManager authorizationStatus];
    if (authorization == kCLAuthorizationStatusAuthorizedWhenInUse ||
        authorization == kCLAuthorizationStatusAuthorizedAlways)
    {
        self.locationRequestInFlight = NO;
        [self requestPreciseLocationForConnectedIPod];
    }
    else if (authorization != kCLAuthorizationStatusNotDetermined)
    {
        self.locationRequestInFlight = NO;
        self.syncStatus.text = @"Location permission is off; iPod Maps kept its previous position.";
    }
    (void)manager;
}

- (void)locationManager:(CLLocationManager *)manager
    didChangeAuthorizationStatus:(CLAuthorizationStatus)status
{
    /* iOS 12/13 authorization callback. iOS 14+ calls the method above. */
    if (@available(iOS 14.0, *))
        return;
    if (status == kCLAuthorizationStatusAuthorizedWhenInUse ||
        status == kCLAuthorizationStatusAuthorizedAlways)
    {
        self.locationRequestInFlight = NO;
        [self requestPreciseLocationForConnectedIPod];
    }
    else if (status != kCLAuthorizationStatusNotDetermined)
    {
        self.locationRequestInFlight = NO;
        self.syncStatus.text = @"Location permission is off; iPod Maps kept its previous position.";
    }
    (void)manager;
}

- (void)locationManager:(CLLocationManager *)manager
    didUpdateLocations:(NSArray<CLLocation *> *)locations
{
    NSDate *now = [NSDate date];
    for (CLLocation *candidate in locations)
    {
        if (candidate.horizontalAccuracy < 0 ||
            fabs([candidate.timestamp timeIntervalSinceDate:now]) > 60.0)
            continue;
        if (!self.bestLocationFix ||
            candidate.horizontalAccuracy < self.bestLocationFix.horizontalAccuracy)
            self.bestLocationFix = candidate;
    }
    /* A sub-15 m fix is already finer than the map marker and tile pixels. */
    if (self.bestLocationFix.horizontalAccuracy <= 15.0)
        [self finishLocationFix];
    (void)manager;
}

- (void)finishLocationFix
{
    if (!self.locationRequestInFlight)
        return;
    [self.locationFixTimer invalidate];
    self.locationFixTimer = nil;
    [self.locationManager stopUpdatingLocation];
    self.locationRequestInFlight = NO;
    CLLocation *location = self.bestLocationFix;
    self.bestLocationFix = nil;
    if (!location)
    {
        self.syncStatus.text = @"iPhone could not get a fresh location; iPod Maps kept its previous position.";
        return;
    }

    int latitudeE6 = (int)llround(location.coordinate.latitude * 1000000.0);
    int longitudeE6 = (int)llround(location.coordinate.longitude * 1000000.0);
    long long timestamp = (long long)llround(location.timestamp.timeIntervalSince1970);
    double accuracy = MAX(location.horizontalAccuracy, 0.0);
    NSString *mapData = [NSString stringWithFormat:
        @"# rockpod-map-location-v1\nlocation\t%d\t%d\tiPhone Location (±%.0f m)\n"
         @"meta\t%lld\t%.1f\n",
        latitudeE6, longitudeE6, accuracy, timestamp, accuracy];
    NSURL *file = [NSFileManager.defaultManager.temporaryDirectory
        URLByAppendingPathComponent:[NSString stringWithFormat:
            @"rockpod-location-%@.tsv", NSUUID.UUID.UUIDString]];
    NSError *error = nil;
    if (![mapData writeToURL:file atomically:YES encoding:NSUTF8StringEncoding
                       error:&error])
    {
        self.syncStatus.text = error.localizedDescription ?: @"Could not stage iPhone location.";
        return;
    }

    self.locationQueuedForConnection = YES;
    self.syncStatus.text = [NSString stringWithFormat:
        @"Sending precise iPhone location to Maps (±%.0f m)…", accuracy];
    __weak typeof(self) weakSelf = self;
    [self.relay enqueueFileAtURL:file destination:@"/.rockbox/maps/location.v1.tsv"
        completion:^(BOOL success, NSString *message) {
            [NSFileManager.defaultManager removeItemAtURL:file error:nil];
            if (success)
                weakSelf.syncStatus.text = [NSString stringWithFormat:
                    @"iPod Maps updated from iPhone location (±%.0f m).", accuracy];
            else
            {
                weakSelf.locationQueuedForConnection = NO;
                weakSelf.syncStatus.text = message ?: @"iPhone location could not be synced.";
            }
        }];
}

- (void)locationManager:(CLLocationManager *)manager
    didFailWithError:(NSError *)error
{
    [self.locationFixTimer invalidate];
    self.locationFixTimer = nil;
    [manager stopUpdatingLocation];
    self.locationRequestInFlight = NO;
    self.syncStatus.text = [NSString stringWithFormat:
        @"Location unavailable; iPod Maps kept its previous position (%@).",
        error.localizedDescription ?: @"unknown error"];
    (void)manager;
}

- (void)attachRelayHandlers
{
    __weak typeof(self) weakSelf = self;
    self.relay.statusHandler = ^(NSString *status, BOOL connected) {
        BOOL wasConnected = weakSelf.relayConnected;
        weakSelf.relayConnected = connected;
        if (!connected)
        {
            [weakSelf.locationFixTimer invalidate];
            weakSelf.locationFixTimer = nil;
            [weakSelf.locationManager stopUpdatingLocation];
            weakSelf.bestLocationFix = nil;
            weakSelf.locationQueuedForConnection = NO;
            weakSelf.locationRequestInFlight = NO;
        }
        else if (!wasConnected)
            [weakSelf requestPreciseLocationForConnectedIPod];
        weakSelf.displayLabel.text = weakSelf.phoneNowPlayingTitle.length ?
            weakSelf.phoneNowPlayingTitle : status;
        /* Never assert Storage Mode from a briefly dropped link.  The iPod
         * reports its real USB mode in the status packet, so an interrupted
         * iPhone USB Link session is a reconnect, not a mode change.  Once
         * the iPod has been silent long enough that it really could have been
         * switched or unplugged, fall back to the original wording. */
        NSTimeInterval quiet = [NSDate timeIntervalSinceReferenceDate] -
                               weakSelf.lastKnownModeTime;
        NSString *idleText = (weakSelf.lastKnownMode.length && quiet < 60.0) ?
            [NSString stringWithFormat:@"%@ — reconnecting…",
                weakSelf.lastKnownMode] : @"Disconnected or Storage Mode";
        weakSelf.footerLabel.text = connected ? @"iPod connected — USB network" :
                                                idleText;
        weakSelf.safeDisconnectButton.enabled = connected;
        weakSelf.internetStatus.text = connected ?
            @"Internet, weather and live Safari relay are active in the background service." :
            @"Internet relay is waiting for iPhone USB Link mode.";
        if (!connected)
        {
            weakSelf.modeLabel.text = idleText;
            weakSelf.musicCatalogRequested = NO;
        }
        else if (!weakSelf.musicCatalogRequested)
        {
            weakSelf.musicCatalogRequested = YES;
            [weakSelf.relay requestLibraryCatalog];
        }
    };
    self.relay.deviceInfoHandler = ^(NSDictionary *info) {
        [weakSelf applyDeviceInfo:info];
    };
    self.relay.sitekickHandler = ^(NSData *data) {
        UIImage *image = RPSitekickPreview(data);
        weakSelf.sitekickImage.image = image;
        weakSelf.sitekickStatus.text = image ? @"Live Sitekick preview from iPod" :
                                               @"Sitekick preview could not be decoded";
    };
    self.relay.libraryCatalogHandler = ^(NSData *data) {
        if (!weakSelf.musicService)
            weakSelf.musicService = [[RPMusicService alloc] init];
        NSError *error = nil;
        if ([weakSelf.musicService ingestLibraryCatalog:data error:&error])
        {
            [weakSelf updateMusicAnalysis];
            [weakSelf updateLibraryBrowsers];
        }
        else
            weakSelf.musicCatalogStatus.text = error.localizedDescription ?: @"Music catalog could not be read.";
    };
    self.relay.transferProgressHandler = ^(NSString *name, double progress) {
        weakSelf.syncProgress.progress = progress;
        weakSelf.syncStatus.text = [NSString stringWithFormat:@"%@ — %.0f%%",
                                    name, progress * 100.0];
    };
}

- (void)installSourceDrawerBelowHeader:(UIView *)header aboveFooter:(UIView *)footer
{
    self.sourceScrim = [[UIControl alloc] init];
    self.sourceScrim.translatesAutoresizingMaskIntoConstraints = NO;
    self.sourceScrim.backgroundColor = [UIColor colorWithWhite:0 alpha:0.32];
    self.sourceScrim.alpha = 0;
    self.sourceScrim.hidden = YES;
    [self.sourceScrim addTarget:self action:@selector(closeSourceDrawer)
               forControlEvents:UIControlEventTouchUpInside];
    [self.view addSubview:self.sourceScrim];
    self.sourceDrawer = [[UIView alloc] init];
    self.sourceDrawer.translatesAutoresizingMaskIntoConstraints = NO;
    self.sourceDrawer.backgroundColor = [UIColor colorWithRed:0.84 green:0.87 blue:0.91 alpha:1];
    self.sourceDrawer.layer.shadowColor = UIColor.blackColor.CGColor;
    self.sourceDrawer.layer.shadowOpacity = 0.35; self.sourceDrawer.layer.shadowRadius = 8;
    self.sourceDrawer.layer.shadowOffset = CGSizeMake(3, 0);
    UIScrollView *drawerScroll = [[UIScrollView alloc] init];
    drawerScroll.translatesAutoresizingMaskIntoConstraints = NO;
    drawerScroll.alwaysBounceVertical = YES;
    drawerScroll.showsVerticalScrollIndicator = YES;
    UIStackView *list = [[UIStackView alloc] init];
    list.translatesAutoresizingMaskIntoConstraints = NO; list.axis = UILayoutConstraintAxisVertical;
    list.spacing = 1;
    NSArray *names = @[@"iPod", @"Songs", @"Artists", @"Albums", @"Videos",
        @"Sync", @"Music & Media", @"Games", @"Settings", @"Sitekick",
        @"Simulator", @"Internet"];
    NSMutableArray *buttons = [NSMutableArray array];
    NSDictionary<NSNumber *, NSString *> *groupNames = @{
        @0: @"LIBRARY", @5: @"TRANSFER", @8: @"DEVICE & SERVICES"
    };
    [names enumerateObjectsUsingBlock:^(NSString *name, NSUInteger index, BOOL *stop) {
        (void)stop;
        NSString *groupName = groupNames[@(index)];
        if (groupName)
        {
            UILabel *group = [self label:groupName size:10 bold:YES];
            group.textColor = [UIColor colorWithWhite:0.36 alpha:1.0];
            group.text = [@"   " stringByAppendingString:groupName];
            [group.heightAnchor constraintEqualToConstant:index ? 31 : 27].active = YES;
            [list addArrangedSubview:group];
        }
        UIButton *button = [UIButton buttonWithType:UIButtonTypeCustom];
        button.tag = (NSInteger)index; button.contentHorizontalAlignment = UIControlContentHorizontalAlignmentLeft;
        button.titleLabel.font = index == 0 ? [UIFont boldSystemFontOfSize:16] : [UIFont boldSystemFontOfSize:14];
        [button setTitle:[@"   " stringByAppendingString:name] forState:UIControlStateNormal];
        [button setTitleColor:[UIColor colorWithRed:0.08 green:0.25 blue:0.43 alpha:1] forState:UIControlStateNormal];
        [button setTitleColor:UIColor.whiteColor forState:UIControlStateSelected];
        button.backgroundColor = index == 0 ? RPBlue() : UIColor.clearColor;
        button.selected = index == 0;
        [button.heightAnchor constraintEqualToConstant:44].active = YES;
        [button addTarget:self action:@selector(drawerSourcePressed:) forControlEvents:UIControlEventTouchUpInside];
        [list addArrangedSubview:button];
        [buttons addObject:button];
    }];
    self.drawerButtons = buttons;
    [drawerScroll addSubview:list];
    [self.sourceDrawer addSubview:drawerScroll]; [self.view addSubview:self.sourceDrawer];
    self.sourceDrawerLeading = [self.sourceDrawer.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor constant:-276];
    [NSLayoutConstraint activateConstraints:@[
        [self.sourceScrim.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
        [self.sourceScrim.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],
        [self.sourceScrim.topAnchor constraintEqualToAnchor:header.bottomAnchor],
        [self.sourceScrim.bottomAnchor constraintEqualToAnchor:footer.topAnchor],
        self.sourceDrawerLeading,
        [self.sourceDrawer.widthAnchor constraintEqualToConstant:276],
        [self.sourceDrawer.topAnchor constraintEqualToAnchor:header.bottomAnchor],
        [self.sourceDrawer.bottomAnchor constraintEqualToAnchor:footer.topAnchor],
        [drawerScroll.leadingAnchor constraintEqualToAnchor:self.sourceDrawer.leadingAnchor],
        [drawerScroll.trailingAnchor constraintEqualToAnchor:self.sourceDrawer.trailingAnchor],
        [drawerScroll.topAnchor constraintEqualToAnchor:self.sourceDrawer.topAnchor],
        [drawerScroll.bottomAnchor constraintEqualToAnchor:self.sourceDrawer.bottomAnchor],
        [list.leadingAnchor constraintEqualToAnchor:drawerScroll.contentLayoutGuide.leadingAnchor],
        [list.trailingAnchor constraintEqualToAnchor:drawerScroll.contentLayoutGuide.trailingAnchor],
        [list.topAnchor constraintEqualToAnchor:drawerScroll.contentLayoutGuide.topAnchor constant:6],
        [list.bottomAnchor constraintEqualToAnchor:drawerScroll.contentLayoutGuide.bottomAnchor constant:-12],
        [list.widthAnchor constraintEqualToAnchor:drawerScroll.frameLayoutGuide.widthAnchor]]];
    UISwipeGestureRecognizer *open = [[UISwipeGestureRecognizer alloc] initWithTarget:self action:@selector(openSourceDrawer)];
    open.direction = UISwipeGestureRecognizerDirectionRight; [self.view addGestureRecognizer:open];
    UISwipeGestureRecognizer *close = [[UISwipeGestureRecognizer alloc] initWithTarget:self action:@selector(closeSourceDrawer)];
    close.direction = UISwipeGestureRecognizerDirectionLeft; [self.sourceDrawer addGestureRecognizer:close];
}

- (void)setSourceDrawerOpen:(BOOL)open
{
    _sourceDrawerOpen = open;
    if (open) self.sourceScrim.hidden = NO;
    self.sourceDrawerLeading.constant = open ? 0 : -276;
    [UIView animateWithDuration:0.28 delay:0 options:UIViewAnimationOptionCurveEaseInOut
        animations:^{
            self.sourceScrim.alpha = open ? 1 : 0;
            [self.view layoutIfNeeded];
        } completion:^(BOOL finished) {
            (void)finished;
            if (!open) self.sourceScrim.hidden = YES;
        }];
}
- (void)toggleSourceDrawer { [self setSourceDrawerOpen:!self.sourceDrawerOpen]; }
- (void)openSourceDrawer { [self setSourceDrawerOpen:YES]; }
- (void)closeSourceDrawer { [self setSourceDrawerOpen:NO]; }
- (void)drawerSourcePressed:(UIButton *)sender
{
    [self.drawerButtons enumerateObjectsUsingBlock:^(UIButton *button, NSUInteger index, BOOL *stop) {
        (void)stop;
        button.selected = index == (NSUInteger)sender.tag;
        button.backgroundColor = button.selected ? RPBlue() : UIColor.clearColor;
    }];
    [self showPage:sender.tag]; [self setSourceDrawerOpen:NO];
}

- (void)updateLibraryBrowsers
{
    NSArray *tracks = self.musicService.catalog ?: @[];
    NSArray *videos = self.musicService.videos ?: @[];
    [self.songsBrowser updateTracks:tracks videos:videos];
    [self.artistsBrowser updateTracks:tracks videos:videos];
    [self.albumsBrowser updateTracks:tracks videos:videos];
    [self.videosBrowser updateTracks:tracks videos:videos];
}

/* AVAssetResourceLoader streaming makes AVFoundation trust the UTI we declare,
 * and it will not decode FLAC or AIFF that way -- it takes the bytes and plays
 * silence, which is why a track title appeared with no sound.  Given a real
 * file on disk AVFoundation sniffs the container itself and plays both.  So
 * these formats are fetched whole and played locally; MP3/AAC still stream. */
static BOOL rp_format_needs_local_file(NSString *path)
{
    NSString *ext = path.pathExtension.lowercaseString;
    return [ext isEqualToString:@"flac"] || [ext isEqualToString:@"aiff"] ||
           [ext isEqualToString:@"aif"]  || [ext isEqualToString:@"wav"] ||
           [ext isEqualToString:@"ogg"]  || [ext isEqualToString:@"opus"] ||
           [ext isEqualToString:@"ape"]  || [ext isEqualToString:@"wv"];
}

- (void)playIPodTrackFromLocalCopy:(RPMusicTrack *)track
{
    self.displayLabel.text = [NSString stringWithFormat:
        @"Fetching %@ from iPod…", track.title];
    __weak typeof(self) weakSelf = self;
    [self.relay requestMediaFileAtPath:track.path
        completion:^(NSURL *fileURL, NSString *errorMessage) {
        typeof(self) self = weakSelf;
        if (!self) return;
        if (!fileURL)
        {
            self.displayLabel.text = errorMessage ?: @"iPod would not send this track";
            return;
        }
        [self clearPhonePlaybackObservers];
        [self.remotePlayer pause];
        [self configurePhonePlaybackAudioSession];
        self.remotePlayer = [AVPlayer playerWithURL:fileURL];
        self.phoneNowPlayingTitle = [NSString stringWithFormat:@"%@ — %@",
                                     track.title, track.artist];
        [self.remotePlayer play];
        self.displayLabel.text = self.phoneNowPlayingTitle;
        self.playPauseButton.accessibilityValue = @"Playing";
    }];
}

- (void)playIPodTrack:(RPMusicTrack *)track
{
    if (!self.relay.isConnected) { self.displayLabel.text = @"Connect iPod to play this song"; return; }
    if (rp_format_needs_local_file(track.path))
    {
        [self playIPodTrackFromLocalCopy:track];
        return;
    }
    [self clearPhonePlaybackObservers];
    [self.remotePlayer pause];
    [self.ipodAudioStreamer invalidate];
    [self configurePhonePlaybackAudioSession];
    self.ipodAudioStreamer = [[RPIPodAudioStreamer alloc]
        initWithRelay:self.relay path:track.path];
    AVPlayerItem *item = [self.ipodAudioStreamer playerItem];
    self.remotePlayer = [AVPlayer playerWithPlayerItem:item];
    self.remotePlayer.automaticallyWaitsToMinimizeStalling = YES;
    self.phoneNowPlayingTitle = [NSString stringWithFormat:@"%@ — %@",
                                 track.title, track.artist];
    self.displayLabel.text = [NSString stringWithFormat:@"Streaming %@ from iPod…",
                              track.title];
    self.playPauseButton.accessibilityValue = @"Buffering";
    [[NSNotificationCenter defaultCenter] addObserver:self
        selector:@selector(phonePlaybackEnded:)
        name:AVPlayerItemDidPlayToEndTimeNotification object:item];
    [[NSNotificationCenter defaultCenter] addObserver:self
        selector:@selector(phonePlaybackFailed:)
        name:AVPlayerItemFailedToPlayToEndTimeNotification object:item];
    [self.remotePlayer play];
    self.displayLabel.text = self.phoneNowPlayingTitle;
    self.playPauseButton.accessibilityValue = @"Playing";
}

- (void)loadArtworkForIPodTrack:(RPMusicTrack *)track
                     completion:(void (^)(UIImage *image))completion
{
    if (!track.artworkPath.length || !self.relay.isConnected)
    {
        completion(nil);
        return;
    }
    [self.relay requestMediaDataAtPath:track.artworkPath offset:0
        length:1024 * 1024 completion:^(NSData *data, uint64_t totalLength,
                                       NSString *errorMessage) {
        (void)totalLength;
        (void)errorMessage;
        completion(data.length ? [UIImage imageWithData:data] : nil);
    }];
}

- (void)downloadIPodTrack:(RPMusicTrack *)track
{
    if (!self.relay.isConnected)
    {
        self.displayLabel.text = @"Connect iPod to download this song";
        return;
    }
    self.displayLabel.text = [NSString stringWithFormat:@"Downloading %@…", track.title];
    [self.relay requestMediaFileAtPath:track.path
        completion:^(NSURL *fileURL, NSString *errorMessage) {
        if (!fileURL)
        {
            self.displayLabel.text = errorMessage ?: @"Song download failed";
            return;
        }
        self.displayLabel.text = [NSString stringWithFormat:@"Downloaded %@", track.title];
        UIActivityViewController *share = [[UIActivityViewController alloc]
            initWithActivityItems:@[fileURL] applicationActivities:nil];
        if (share.popoverPresentationController)
            share.popoverPresentationController.sourceView = self.view;
        [self presentViewController:share animated:YES completion:nil];
    }];
}

- (void)phonePlaybackEnded:(NSNotification *)notification
{
    (void)notification;
    [self clearPhonePlaybackObservers];
    self.phoneNowPlayingTitle = nil;
    [self.ipodAudioStreamer invalidate];
    self.ipodAudioStreamer = nil;
    self.playPauseButton.accessibilityValue = @"Stopped";
    self.displayLabel.text = @"iPod USB Link ready";
}

- (void)phonePlaybackFailed:(NSNotification *)notification
{
    NSError *error = notification.userInfo[AVPlayerItemFailedToPlayToEndTimeErrorKey];
    [self clearPhonePlaybackObservers];
    self.phoneNowPlayingTitle = nil;
    [self.ipodAudioStreamer invalidate];
    self.ipodAudioStreamer = nil;
    self.playPauseButton.accessibilityValue = @"Stopped";
    self.displayLabel.text = error.localizedDescription ?: @"iPod stream could not play";
}

- (void)togglePhonePlayback
{
    if (!self.remotePlayer.currentItem) return;
    if (self.remotePlayer.rate > 0)
    {
        [self.remotePlayer pause];
        self.playPauseButton.accessibilityValue = @"Paused";
    }
    else
    {
        [self.remotePlayer play];
        self.playPauseButton.accessibilityValue = @"Playing";
    }
}

- (void)refreshMusicCatalog
{
    if (!self.relay.isConnected)
    {
        self.musicCatalogStatus.text = @"Connect the iPod in iPhone USB Link mode first.";
        return;
    }
    self.musicCatalogStatus.text = @"Reading the iPod tagcache catalog…";
    [self.relay requestLibraryCatalog];
}

- (void)updateMusicAnalysis
{
    RPMusicAnalysis *analysis = self.musicService.analysis;
    self.musicCatalogStatus.text = [NSString stringWithFormat:
        @"%ld tracks analyzed • %.1f hours of listening history",
        (long)analysis.totalTracks, analysis.totalPlayTimeMS / 3600000.0];
    NSDictionary *recommendation = analysis.recommendations.firstObject;
    NSDictionary *track = recommendation[@"track"];
    self.musicDiscoveryStatus.text = recommendation ? [NSString stringWithFormat:
        @"Try “%@” by %@\n%@", track[@"title"], track[@"artist"], recommendation[@"reason"]] :
        @"Listen a little longer to build recommendations.";
    self.musicHealthStatus.text = [NSString stringWithFormat:
        @"%lu albums with track-number gaps • %lu possible duplicate groups",
        (unsigned long)analysis.missingAlbumTracks.count,
        (unsigned long)analysis.duplicateGroups.count];
    NSDictionary *artist = analysis.topArtists.firstObject;
    NSDictionary *album = analysis.topAlbums.firstObject;
    self.musicReplayStatus.text = artist ? [NSString stringWithFormat:
        @"Top artist: %@\nTop album: %@", artist[@"name"], album[@"name"] ?: @"—"] :
        @"No play history is available yet.";
}

- (void)reviewMusicHealth
{
    RPMusicAnalysis *analysis = self.musicService.analysis;
    NSMutableString *message = [NSMutableString string];
    for (NSDictionary *gap in [analysis.missingAlbumTracks subarrayWithRange:
         NSMakeRange(0, MIN((NSUInteger)4, analysis.missingAlbumTracks.count))])
        [message appendFormat:@"%@ — %@: missing %@\n", gap[@"artist"], gap[@"album"],
                              gap[@"missing_track_numbers"]];
    for (NSDictionary *duplicate in [analysis.duplicateGroups subarrayWithRange:
         NSMakeRange(0, MIN((NSUInteger)4, analysis.duplicateGroups.count))])
        [message appendFormat:@"Duplicate: %@ — %@ (%lu copies)\n", duplicate[@"artist"],
            duplicate[@"title"], (unsigned long)[duplicate[@"tracks"] count]];
    if (!message.length) [message appendString:@"No track-number gaps or likely duplicates were found."];
    UIAlertController *alert = [UIAlertController alertControllerWithTitle:@"Library Health"
        message:message preferredStyle:UIAlertControllerStyleAlert];
    [alert addAction:[UIAlertAction actionWithTitle:@"Done" style:UIAlertActionStyleCancel handler:nil]];
    if (analysis.duplicateGroups.count && self.relay.isConnected)
        [alert addAction:[UIAlertAction actionWithTitle:@"Clean Synced Copies…"
            style:UIAlertActionStyleDestructive handler:^(UIAlertAction *action) {
            (void)action; [self confirmDuplicateCleanup];
        }]];
    [self presentViewController:alert animated:YES completion:nil];
}

- (void)confirmDuplicateCleanup
{
    NSError *error = nil;
    NSURL *manifest = [self.musicService duplicateCleanupManifest:&error];
    if (!manifest) { self.musicHealthStatus.text = error.localizedDescription ?: @"No safe RockPodLink duplicates can be removed."; return; }
    UIAlertController *confirm = [UIAlertController alertControllerWithTitle:@"Remove Duplicate Synced Copies?"
        message:@"Only duplicate files inside Music/RockPodLink are eligible. Your original music folders are never touched. Refresh the library after cleanup."
        preferredStyle:UIAlertControllerStyleAlert];
    [confirm addAction:[UIAlertAction actionWithTitle:@"Cancel" style:UIAlertActionStyleCancel handler:nil]];
    [confirm addAction:[UIAlertAction actionWithTitle:@"Remove Copies" style:UIAlertActionStyleDestructive
        handler:^(UIAlertAction *action) { (void)action;
        [self.relay enqueueFileAtURL:manifest destination:@"/.rockbox/rockpod/phone/cleanup-v1.txt"
            completion:^(BOOL success, NSString *message) {
            self.musicHealthStatus.text = success ? @"Cleanup applied. Refreshing the catalog…" : message;
            if (success) [self refreshMusicCatalog];
        }];
    }]];
    [self presentViewController:confirm animated:YES completion:nil];
}

- (void)checkMusicDiscovery
{
    NSArray *artists = [self.musicService.analysis.topArtists valueForKey:@"name"] ?: @[];
    self.musicDiscoveryStatus.text = @"Checking MusicBrainz for releases and live shows…";
    [self.musicDiscovery discoverForArtists:artists completion:^(NSArray<NSDictionary *> *results,
                                                                  NSString *errorMessage) {
        self.musicDiscoveryResults = results;
        if (!results.count) { self.musicDiscoveryStatus.text = errorMessage ?: @"No upcoming results found."; return; }
        NSDictionary *first = results.firstObject;
        self.musicDiscoveryStatus.text = [NSString stringWithFormat:
            @"%lu upcoming result%@ • %@ — %@ (%@)", (unsigned long)results.count,
            results.count == 1 ? @"" : @"s", first[@"artist"], first[@"title"], first[@"date"]];
        NSError *error = nil;
        NSURL *manifest = [self.musicDiscovery notificationManifestForResults:results error:&error];
        if (manifest && self.relay.isConnected)
            [self queueURL:manifest destination:@"/.rockbox/rockpod/phone/notification-inbox-v1.tsv"];
    }];
}

- (void)exportMusicReplay
{
    if (!self.musicService.analysis) return;
    NSMutableDictionary *report = self.musicService.analysis.dictionaryRepresentation.mutableCopy;
    report[@"exported_at"] = @([NSDate date].timeIntervalSince1970);
    NSData *data = [NSJSONSerialization dataWithJSONObject:report options:NSJSONWritingPrettyPrinted error:nil];
    NSURL *URL = [NSFileManager.defaultManager.temporaryDirectory URLByAppendingPathComponent:
                  [NSString stringWithFormat:@"RockPod-Replay-%ld.json", (long)[NSCalendar.currentCalendar
                   component:NSCalendarUnitYear fromDate:NSDate.date]]];
    [data writeToURL:URL atomically:YES];
    UIActivityViewController *share = [[UIActivityViewController alloc] initWithActivityItems:@[URL]
        applicationActivities:nil];
    if (share.popoverPresentationController) share.popoverPresentationController.sourceView = self.view;
    [self presentViewController:share animated:YES completion:nil];
}

- (UITextField *)musicTextField:(NSString *)placeholder secure:(BOOL)secure
{
    UITextField *field = [[UITextField alloc] init];
    field.borderStyle = UITextBorderStyleRoundedRect;
    field.backgroundColor = UIColor.whiteColor;
    field.font = [UIFont systemFontOfSize:13];
    field.placeholder = placeholder;
    field.secureTextEntry = secure;
    field.autocapitalizationType = UITextAutocapitalizationTypeNone;
    field.autocorrectionType = UITextAutocorrectionTypeNo;
    [field.heightAnchor constraintEqualToConstant:36].active = YES;
    return field;
}

- (void)populateMediaAccount:(RPMediaServerAccount *)account
{
    self.selectedMediaAccount = account;
    self.mediaServerTypeControl.selectedSegmentIndex = account.type;
    self.mediaServerURLField.text = account.baseURL.absoluteString;
    self.mediaServerUsernameField.text = account.username;
    self.mediaServerUserIDField.text = account.userID;
    self.mediaServerSecretField.text = account.secret;
}

- (void)musicQualityChanged:(UISegmentedControl *)control
{
    [NSUserDefaults.standardUserDefaults setInteger:control.selectedSegmentIndex
                                             forKey:@"RPMusicQualityIndex"];
}

- (NSInteger)preferredMusicBitrate
{
    static const NSInteger rates[] = { 96, 128, 192, 256, 320 };
    NSInteger index = MIN(4, MAX(0, self.musicQualityControl.selectedSegmentIndex));
    return rates[index];
}

- (void)saveAndLoadMediaServer
{
    NSString *value = [self.mediaServerURLField.text stringByTrimmingCharactersInSet:
                       NSCharacterSet.whitespaceAndNewlineCharacterSet];
    if (value.length && ![value containsString:@"://"])
        value = [@"https://" stringByAppendingString:value];
    if (value.length && ![value hasSuffix:@"/"])
        value = [value stringByAppendingString:@"/"];
    RPMediaServerAccount *account = self.selectedMediaAccount ?: [[RPMediaServerAccount alloc] init];
    account.type = self.mediaServerTypeControl.selectedSegmentIndex;
    account.baseURL = [NSURL URLWithString:value];
    account.username = self.mediaServerUsernameField.text ?: @"";
    account.userID = self.mediaServerUserIDField.text ?: @"";
    account.secret = self.mediaServerSecretField.text ?: @"";
    static NSArray *names;
    if (!names) names = @[@"Plex", @"Jellyfin", @"Navidrome / Subsonic", @"Personal Server"];
    account.displayName = names[(NSUInteger)account.type];
    NSError *error = nil;
    if (![self.mediaServers saveAccount:account error:&error])
    {
        self.mediaServerStatus.text = error.localizedDescription;
        return;
    }
    self.selectedMediaAccount = account;
    self.mediaServerStatus.text = @"Testing server connection…";
    [self.mediaServers testAccount:account completion:^(BOOL success, NSString *message) {
        if (!success)
        {
            self.mediaServerStatus.text = message;
            return;
        }
        self.mediaServerStatus.text = @"Connected. Loading playlists…";
        [self loadRemotePlaylists];
    }];
}

- (void)loadRemotePlaylists
{
    if (!self.selectedMediaAccount) return;
    [self.mediaServers fetchPlaylistsForAccount:self.selectedMediaAccount
        completion:^(NSArray<RPMediaPlaylist *> *playlists, NSString *errorMessage) {
        self.remotePlaylists = playlists;
        for (UIView *view in self.remotePlaylistStack.arrangedSubviews)
        {
            [self.remotePlaylistStack removeArrangedSubview:view];
            [view removeFromSuperview];
        }
        self.mediaServerStatus.text = errorMessage ?: [NSString stringWithFormat:
            @"%lu playlists available. Choose one to stream or sync.",
            (unsigned long)playlists.count];
        [playlists enumerateObjectsUsingBlock:^(RPMediaPlaylist *playlist,
                                                NSUInteger index, BOOL *stop) {
            (void)stop;
            UIButton *row = [self button:[NSString stringWithFormat:@"%@  (%ld)",
                playlist.name, (long)playlist.trackCount]
                                   action:@selector(remotePlaylistPressed:)];
            row.tag = (NSInteger)index;
            row.contentHorizontalAlignment = UIControlContentHorizontalAlignmentLeft;
            [self.remotePlaylistStack addArrangedSubview:row];
        }];
    }];
}

- (void)syncAudiobookPositions
{
    if (!self.selectedMediaAccount || !self.musicService.analysis)
    {
        self.mediaServerStatus.text = @"Load a server and refresh the iPod music library first.";
        return;
    }
    self.mediaServerStatus.text = @"Merging audiobook positions…";
    [self.mediaServers syncAudiobookPositionsForAccount:self.selectedMediaAccount
        localPositions:self.musicService.analysis.audiobookPositions
        completion:^(NSArray<NSDictionary *> *positions, NSString *errorMessage) {
        if (errorMessage.length) { self.mediaServerStatus.text = errorMessage; return; }
        NSError *error = nil;
        NSURL *manifest = [self.musicService audiobookPositionManifestForRemotePositions:positions error:&error];
        if (!manifest)
        {
            self.mediaServerStatus.text = error.localizedDescription ?: @"Audiobook positions are already current.";
            return;
        }
        [self.relay enqueueFileAtURL:manifest
            destination:@"/.rockbox/rockpod/phone/audiobook-positions-v1.tsv"
            completion:^(BOOL success, NSString *message) {
            self.mediaServerStatus.text = success ? @"Audiobook positions synchronized both ways." : message;
        }];
    }];
}

- (void)remotePlaylistPressed:(UIButton *)sender
{
    if (sender.tag < 0 || sender.tag >= (NSInteger)self.remotePlaylists.count)
        return;
    RPMediaPlaylist *playlist = self.remotePlaylists[(NSUInteger)sender.tag];
    UIAlertController *menu = [UIAlertController alertControllerWithTitle:playlist.name
        message:@"Stream from the server or cache an offline-compatible copy on the iPod."
        preferredStyle:UIAlertControllerStyleActionSheet];
    [menu addAction:[UIAlertAction actionWithTitle:@"Stream & Cache First Track"
        style:UIAlertActionStyleDefault handler:^(UIAlertAction *action) {
        (void)action; [self streamAndCacheFirstTrack:playlist];
    }]];
    [menu addAction:[UIAlertAction actionWithTitle:@"Cache & Sync Entire Playlist"
        style:UIAlertActionStyleDefault handler:^(UIAlertAction *action) {
        (void)action; [self syncRemotePlaylist:playlist];
    }]];
    [menu addAction:[UIAlertAction actionWithTitle:@"Cancel" style:UIAlertActionStyleCancel handler:nil]];
    if (menu.popoverPresentationController)
        menu.popoverPresentationController.sourceView = sender;
    [self presentViewController:menu animated:YES completion:nil];
}

- (NSString *)safeMusicComponent:(NSString *)value fallback:(NSString *)fallback
{
    NSString *safe = [self safeName:value fallback:fallback];
    safe = [safe stringByReplacingOccurrencesOfString:@":" withString:@"-"];
    return safe.length ? safe : fallback;
}

- (NSURL *)cacheRemoteItem:(RPMediaItem *)item result:(RPAudioTranscodeResult *)result
                    account:(RPMediaServerAccount *)account
{
    NSURL *support = [NSFileManager.defaultManager URLsForDirectory:
        NSApplicationSupportDirectory inDomains:NSUserDomainMask].firstObject;
    NSURL *directory = [[support URLByAppendingPathComponent:@"RockPodLink/MusicCache"]
        URLByAppendingPathComponent:[self safeMusicComponent:account.displayName fallback:@"Server"]];
    [NSFileManager.defaultManager createDirectoryAtURL:directory
                           withIntermediateDirectories:YES attributes:nil error:nil];
    NSString *extension = result.fileURL.pathExtension.length ? result.fileURL.pathExtension : @"m4a";
    NSURL *cached = [directory URLByAppendingPathComponent:
        [[self safeMusicComponent:item.identifier fallback:NSUUID.UUID.UUIDString]
         stringByAppendingPathExtension:extension]];
    [NSFileManager.defaultManager removeItemAtURL:cached error:nil];
    if (![NSFileManager.defaultManager copyItemAtURL:result.fileURL toURL:cached error:nil])
        return result.fileURL;
    NSDictionary *metadata = @{ @"provider": account.displayName ?: @"",
        @"remote_id": item.identifier ?: @"", @"title": item.title ?: @"",
        @"artist": item.artist ?: @"", @"album": item.album ?: @"",
        @"download_permitted": @(item.downloadPermitted),
        @"selected_bitrate_kbps": @(result.bitrateKbps),
        @"cached_at": @([NSDate date].timeIntervalSince1970) };
    NSData *json = [NSJSONSerialization dataWithJSONObject:metadata
                                                    options:NSJSONWritingPrettyPrinted error:nil];
    [json writeToURL:[cached URLByAppendingPathExtension:@"json"] atomically:YES];
    return cached;
}

- (NSString *)destinationForRemoteItem:(RPMediaItem *)item
                                account:(RPMediaServerAccount *)account
                              extension:(NSString *)extension
{
    NSString *server = [self safeMusicComponent:account.displayName fallback:@"Server"];
    NSString *artist = [self safeMusicComponent:item.artist fallback:@"Unknown Artist"];
    NSString *album = [self safeMusicComponent:item.album fallback:@"Unknown Album"];
    NSString *title = [self safeMusicComponent:item.title fallback:@"Track"];
    NSString *name = [title stringByAppendingPathExtension:extension.length ? extension : @"m4a"];
    return [NSString stringWithFormat:@"/Music/RockPodLink/%@/%@/%@/%@",
            server, artist, album, name];
}

- (void)prepareRemoteItem:(RPMediaItem *)item account:(RPMediaServerAccount *)account
                completion:(void (^)(NSURL *cached, NSString *destination,
                                     NSString *errorMessage))completion
{
    if (!self.musicService) self.musicService = [[RPMusicService alloc] init];
    [self.mediaServers downloadItem:item completion:^(NSURL *fileURL, NSString *downloadError) {
        if (!fileURL)
        {
            completion(nil, nil, downloadError); return;
        }
        NSInteger target = [self.musicService recommendedBitrateForFreeBytes:self.deviceFreeBytes
            incomingSourceBytes:item.size preferredQuality:self.preferredMusicBitrate];
        BOOL keepOriginal = self.musicQualityControl.selectedSegmentIndex == 4;
        [self.audioTranscoder prepareAudioAtURL:fileURL targetKbps:target
            keepOriginal:keepOriginal completion:^(RPAudioTranscodeResult *result,
                                                    NSString *transcodeError) {
            if (!result)
            {
                completion(nil, nil, transcodeError); return;
            }
            NSURL *cached = [self cacheRemoteItem:item result:result account:account];
            NSString *destination = [self destinationForRemoteItem:item account:account
                extension:cached.pathExtension];
            completion(cached, destination, nil);
        }];
    }];
}

- (void)streamAndCacheFirstTrack:(RPMediaPlaylist *)playlist
{
    self.mediaServerStatus.text = @"Loading playlist for streaming…";
    [self.mediaServers fetchItemsForPlaylist:playlist account:self.selectedMediaAccount
        completion:^(NSArray<RPMediaItem *> *items, NSString *errorMessage) {
        RPMediaItem *item = items.firstObject;
        if (!item)
        {
            self.mediaServerStatus.text = errorMessage ?: @"That playlist has no playable tracks.";
            return;
        }
        self.remotePlayer = [AVPlayer playerWithURL:item.mediaURL];
        [self.remotePlayer play];
        self.mediaServerStatus.text = [NSString stringWithFormat:@"Streaming %@ — caching permitted copy…", item.title];
        [self prepareRemoteItem:item account:self.selectedMediaAccount
            completion:^(NSURL *cached, NSString *destination, NSString *cacheError) {
            (void)destination;
            self.mediaServerStatus.text = cached ? [NSString stringWithFormat:
                @"Streaming %@ • offline cache ready", item.title] : cacheError;
        }];
    }];
}

- (void)syncRemotePlaylist:(RPMediaPlaylist *)playlist
{
    if (!self.relay.isConnected)
    {
        self.mediaServerStatus.text = @"Connect the iPod in iPhone USB Link mode before syncing.";
        return;
    }
    self.mediaServerStatus.text = [NSString stringWithFormat:@"Loading %@…", playlist.name];
    [self.mediaServers fetchItemsForPlaylist:playlist account:self.selectedMediaAccount
        completion:^(NSArray<RPMediaItem *> *items, NSString *errorMessage) {
        if (!items.count)
        {
            self.mediaServerStatus.text = errorMessage ?: @"That playlist contains no downloadable tracks.";
            return;
        }
        [self syncRemoteItems:items index:0 paths:[NSMutableArray array]
                      account:self.selectedMediaAccount playlist:playlist];
    }];
}

- (void)syncRemoteItems:(NSArray<RPMediaItem *> *)items index:(NSUInteger)index
                   paths:(NSMutableArray<NSString *> *)paths
                 account:(RPMediaServerAccount *)account
                playlist:(RPMediaPlaylist *)playlist
{
    if (index >= items.count)
    {
        NSMutableString *m3u = [NSMutableString stringWithString:@"#EXTM3U\n"];
        for (NSString *path in paths) [m3u appendFormat:@"%@\n", path];
        NSURL *file = [NSFileManager.defaultManager.temporaryDirectory
            URLByAppendingPathComponent:[NSString stringWithFormat:@"playlist-%@.m3u8",
                                         NSUUID.UUID.UUIDString]];
        [m3u writeToURL:file atomically:YES encoding:NSUTF8StringEncoding error:nil];
        NSString *name = [[self safeMusicComponent:playlist.name fallback:@"Server Playlist"]
                          stringByAppendingPathExtension:@"m3u8"];
        [self queueURL:file destination:[@"/Playlists/RockPodLink" stringByAppendingPathComponent:name]];
        self.mediaServerStatus.text = [NSString stringWithFormat:
            @"%@ queued: %lu tracks plus offline playlist", playlist.name,
            (unsigned long)paths.count];
        return;
    }
    RPMediaItem *item = items[index];
    self.mediaServerStatus.text = [NSString stringWithFormat:@"Preparing %lu of %lu — %@",
        (unsigned long)index + 1, (unsigned long)items.count, item.title];
    [self prepareRemoteItem:item account:account
        completion:^(NSURL *cached, NSString *destination, NSString *errorMessage) {
        if (cached && destination)
        {
            [paths addObject:destination];
            [self queueURL:cached destination:destination];
        }
        else if (errorMessage.length)
            self.mediaServerStatus.text = [NSString stringWithFormat:@"Skipping %@ — %@",
                                           item.title, errorMessage];
        [self syncRemoteItems:items index:index + 1 paths:paths
                      account:account playlist:playlist];
    }];
}

- (void)applyDeviceInfo:(NSDictionary *)info
{
    self.deviceNameLabel.text = info[@"name"] ?: @"RockPod iPod";
    self.lastKnownMode = info[@"mode"];
    self.lastKnownModeTime = [NSDate timeIntervalSinceReferenceDate];
    self.modeLabel.text = info[@"mode"] ?: @"iPhone USB Link";
    self.modeLabel.textColor = [UIColor colorWithRed:0.08 green:0.42 blue:0.15 alpha:1.0];
    self.firmwareLabel.text = [NSString stringWithFormat:@"Software: %@",
                               info[@"firmware"] ?: @"—"];
    self.batteryLabel.text = [NSString stringWithFormat:@"Battery: %@%%",
                              info[@"battery"] ?: @"—"];
    unsigned long long disk = [info[@"disk_bytes"] longLongValue];
    unsigned long long capacity = [info[@"volume_bytes"] longLongValue];
    if (!capacity)
        capacity = [info[@"capacity_bytes"] longLongValue];
    if (!disk)
        disk = capacity;
    unsigned long long free = [info[@"free_bytes"] longLongValue];
    if (free > capacity)
        free = capacity;
    self.deviceFreeBytes = free;
    unsigned long long usedBytes = capacity - free;
    double used = capacity ? (double)usedBytes / capacity : 0;
    const double gib = 1024.0 * 1024.0 * 1024.0;
    if (disk && capacity)
    {
        /* Match iTunes/Rockbox's binary capacity display. Dividing by one
         * billion made a 512 GB replacement drive appear roughly 35 GB too
         * large compared with the iPod and desktop storage tools. */
        self.capacityLabel.text = [NSString stringWithFormat:
            @"Capacity: %.1f GB", disk / gib];
        self.storageLegend.text = [NSString stringWithFormat:
            @"%.1f GB used     %.1f GB free     %.1f GB usable",
            usedBytes / gib, free / gib, capacity / gib];
    }
    else
    {
        self.capacityLabel.text = @"Capacity: Unavailable";
        self.storageLegend.text = @"Waiting for a valid iPod storage report";
    }
    self.usedStorageWidth.active = NO;
    self.usedStorageWidth = [self.usedStorage.widthAnchor
        constraintEqualToAnchor:self.usedStorage.superview.widthAnchor
                      multiplier:MAX(0.001, MIN(1, used)) constant:-4];
    self.usedStorageWidth.active = YES;
    NSString *track = info[@"track"] ?: @"";
    NSString *artist = info[@"artist"] ?: @"";
    self.nowPlayingLabel.text = track.length ?
        [NSString stringWithFormat:@"Now Playing: %@ — %@", track, artist] :
        @"Now Playing: Nothing";
    [self followIPodPlayback:info];
}

/* Play on the phone whatever the iPod is playing.
 *
 * The status now carries track_path, so the same media request used by the
 * library browser can pull the file the player is actually on.  Only a change
 * of path starts a new stream, otherwise every status packet would restart
 * playback two seconds in. */
- (void)followIPodPlayback:(NSDictionary *)info
{
    if (!self.mirrorIPodPlayback)
        return;
    NSString *path = info[@"track_path"] ?: @"";
    BOOL playing = [info[@"playing"] intValue] != 0;

    if (!playing || !path.length)
    {
        if (self.mirroredTrackPath.length)
        {
            self.mirroredTrackPath = nil;
            [self.remotePlayer pause];
            [self.ipodAudioStreamer invalidate];
            self.ipodAudioStreamer = nil;
            self.phoneNowPlayingTitle = nil;
            self.displayLabel.text = @"iPod stopped";
        }
        return;
    }
    if ([path isEqualToString:self.mirroredTrackPath])
        return;

    self.mirroredTrackPath = path;
    RPMusicTrack *mirrored = [[RPMusicTrack alloc] init];
    mirrored.path = path;
    NSString *mirroredTitle = info[@"track"];
    mirrored.title = mirroredTitle.length ? mirroredTitle :
                     path.lastPathComponent;
    mirrored.artist = info[@"artist"] ?: @"";
    [self playIPodTrack:mirrored];
}

- (void)selectSourceIndex:(NSInteger)index
{
    if (index < 0 || index >= (NSInteger)self.sourceButtons.count)
        return;
    [self.sourceButtons enumerateObjectsUsingBlock:^(UIButton *button,
                                                      NSUInteger buttonIndex,
                                                      BOOL *stop) {
        (void)stop;
        BOOL selected = buttonIndex == (NSUInteger)index;
        button.backgroundColor = selected ? RPBlue() : UIColor.clearColor;
        [button setTitleColor:selected ? UIColor.whiteColor : RPBlue()
                     forState:UIControlStateNormal];
        button.layer.cornerRadius = 3;
    }];
    [self showPage:index];
    UIButton *button = self.sourceButtons[(NSUInteger)index];
    CGRect visible = [self.sourceScroll convertRect:button.bounds fromView:button];
    [self.sourceScroll scrollRectToVisible:CGRectInset(visible, -12, 0) animated:YES];
}

- (void)sourceTabPressed:(UIButton *)sender
{
    [self selectSourceIndex:sender.tag];
}

- (void)chooseGameFiles
{
    self.pickerPurpose = RPPickerPurposeGameFiles;
    UIDocumentPickerViewController *picker = [[UIDocumentPickerViewController alloc]
        initWithDocumentTypes:@[@"public.data"] inMode:UIDocumentPickerModeImport];
    picker.delegate = self;
    picker.allowsMultipleSelection = YES;
    [self presentViewController:picker animated:YES completion:nil];
}

- (void)openGameWebsite
{
    NSString *value = [self.gameDownloadURLField.text stringByTrimmingCharactersInSet:
                       NSCharacterSet.whitespaceAndNewlineCharacterSet];
    if (value.length && ![value containsString:@"://"])
        value = [@"https://" stringByAppendingString:value];
    NSURL *url = value.length ? [NSURL URLWithString:value] : nil;
    if (!url.host.length ||
        ![@[@"http", @"https"] containsObject:url.scheme.lowercaseString])
    {
        self.gameStatus.text = @"Enter any valid HTTP or HTTPS website.";
        return;
    }
    [self.gameDownloadURLField resignFirstResponder];
    self.gameDownloadURLField.text = value;
    [NSUserDefaults.standardUserDefaults setObject:value forKey:@"RPGameWebsiteURL"];
    self.gameStatus.text = @"Game website open — tap a download on the page.";
    RPGameBrowserViewController *browser = [[RPGameBrowserViewController alloc] initWithURL:url];
    __weak typeof(self) weakSelf = self;
    __weak RPGameBrowserViewController *weakBrowser = browser;
    browser.downloadHandler = ^(NSURL *fileURL) {
        [weakBrowser dismissViewControllerAnimated:YES completion:^{
            [weakSelf prepareAndSyncLocalGameURL:fileURL];
        }];
    };
    browser.modalPresentationStyle = UIModalPresentationFullScreen;
    [self presentViewController:browser animated:YES completion:nil];
}

- (void)prepareAndSyncLocalGameURL:(NSURL *)url
{
    if (!self.relay.isConnected)
    {
        self.gameStatus.text = @"Game downloaded. Connect the iPod in iPhone USB Link mode, then select it from Files.";
        return;
    }
    self.gameStatus.text = [NSString stringWithFormat:@"Preparing %@…", url.lastPathComponent];
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        NSError *archiveError = nil;
        NSURL *gameURL = [url.pathExtension.lowercaseString isEqualToString:@"zip"] ?
            [RPGameArchive extractSupportedGameFromArchive:url error:&archiveError] : url;
        dispatch_async(dispatch_get_main_queue(), ^{
            if (!gameURL)
            {
                self.gameStatus.text = archiveError.localizedDescription ?: @"The downloaded game could not be unpacked.";
                return;
            }
            if (![RPGameStoreService isSupportedFilename:gameURL.lastPathComponent])
            {
                self.gameStatus.text = [NSString stringWithFormat:@"%@ is not a supported game file.", gameURL.lastPathComponent];
                return;
            }
            if (!self.gameStore) self.gameStore = [[RPGameStoreService alloc] init];
            [self.gameStore prepareLocalGameAtURL:gameURL
                completion:^(RPGameInstall *install, NSString *errorMessage) {
                if (!install)
                {
                    self.gameStatus.text = errorMessage ?: @"Game preparation failed.";
                    return;
                }
                self.gameStatus.text = [NSString stringWithFormat:@"Syncing %@ with metadata and cover art…", gameURL.lastPathComponent];
                for (NSUInteger i = 0; i < install.files.count; i++)
                    [self queueURL:install.files[i] destination:install.destinations[i]];
            }];
        });
    });
}

- (void)prepareAndSyncGameItem:(RPGameStoreItem *)item
{
    if (!self.relay.isConnected)
    {
        self.gameStatus.text = @"Connect the iPod in iPhone USB Link mode before syncing.";
        return;
    }
    self.gameStatus.text = [NSString stringWithFormat:@"Downloading %@ and optimizing artwork…", item.title];
    if (!self.gameStore) self.gameStore = [[RPGameStoreService alloc] init];
    [self.gameStore prepareItem:item completion:^(RPGameInstall *install, NSString *errorMessage) {
        if (!install)
        {
            self.gameStatus.text = errorMessage ?: @"Game preparation failed.";
            return;
        }
        self.gameStatus.text = [NSString stringWithFormat:@"Syncing %@ metadata, cover and game…", item.title];
        for (NSUInteger i = 0; i < install.files.count; i++)
            [self queueURL:install.files[i] destination:install.destinations[i]];
    }];
}

- (void)installGame:(UIButton *)sender
{
    if (sender.tag < 0 || sender.tag >= (NSInteger)self.gameItems.count)
        return;
    [self prepareAndSyncGameItem:self.gameItems[(NSUInteger)sender.tag]];
}

- (void)showPage:(NSInteger)index
{
    if (index < 0 || index >= (NSInteger)self.pages.count)
        return;
    for (UIView *view in self.pageHost.subviews)
        [view removeFromSuperview];
    UIView *page = self.pages[index];
    page.translatesAutoresizingMaskIntoConstraints = NO;
    [self.pageHost addSubview:page];
    [NSLayoutConstraint activateConstraints:@[
        [page.leadingAnchor constraintEqualToAnchor:self.pageHost.leadingAnchor],
        [page.trailingAnchor constraintEqualToAnchor:self.pageHost.trailingAnchor],
        [page.topAnchor constraintEqualToAnchor:self.pageHost.topAnchor],
        [page.bottomAnchor constraintEqualToAnchor:self.pageHost.bottomAnchor],
    ]];
    page.alpha = 0;
    [UIView animateWithDuration:0.16 animations:^{ page.alpha = 1; }];
}

- (NSString *)safeName:(NSString *)name fallback:(NSString *)fallback
{
    NSString *value = name.length ? name : fallback;
    NSCharacterSet *bad = [NSCharacterSet characterSetWithCharactersInString:@"<>:\"/\\|?*"];
    value = [[value componentsSeparatedByCharactersInSet:bad] componentsJoinedByString:@"_"];
    return value.length > 120 ? [value substringToIndex:120] : value;
}

- (void)queueURL:(NSURL *)url folder:(NSString *)folder
{
    NSString *name = [self safeName:url.lastPathComponent fallback:@"Phone Media"];
    NSString *destination = [folder stringByAppendingPathComponent:name];
    [self queueURL:url destination:destination];
}

- (void)queueURL:(NSURL *)url destination:(NSString *)destination
{
    NSString *name = destination.lastPathComponent;
    self.syncStatus.text = [NSString stringWithFormat:@"Queued %@", name];
    [self.relay enqueueFileAtURL:url destination:destination
                     completion:^(BOOL success, NSString *message) {
        self.syncProgress.progress = success ? 1.0 : 0.0;
        self.syncStatus.text = [NSString stringWithFormat:@"%@ — %@", name, message];
    }];
}

- (NSURL *)simulatorMediaURLForFolder:(NSString *)folder name:(NSString *)name
{
    NSURL *documents = [NSFileManager.defaultManager URLsForDirectory:
        NSDocumentDirectory inDomains:NSUserDomainMask].firstObject;
    NSURL *directory = [[[documents URLByAppendingPathComponent:@"RockPodSimulator"]
        URLByAppendingPathComponent:folder] URLByStandardizingPath];
    [NSFileManager.defaultManager createDirectoryAtURL:directory
        withIntermediateDirectories:YES attributes:nil error:nil];
    return [directory URLByAppendingPathComponent:
        [self safeName:name fallback:@"Phone Media"]];
}

- (void)copyURLToSimulator:(NSURL *)url folder:(NSString *)folder
{
    NSURL *destination = [self simulatorMediaURLForFolder:folder
                                                     name:url.lastPathComponent];
    [NSFileManager.defaultManager removeItemAtURL:destination error:nil];
    NSError *error = nil;
    if ([NSFileManager.defaultManager copyItemAtURL:url toURL:destination
                                              error:&error])
        self.syncStatus.text = [NSString stringWithFormat:
            @"Added %@ to simulator /%@", destination.lastPathComponent, folder];
    else
        self.syncStatus.text = error.localizedDescription ?: @"Simulator import failed";
}

- (BOOL)copyURL:(NSURL *)url toSimulatorDestination:(NSString *)destination
          error:(NSError **)error
{
    NSString *relative = [destination hasPrefix:@"/"] ?
        [destination substringFromIndex:1] : destination;
    NSURL *documents = [NSFileManager.defaultManager URLsForDirectory:
        NSDocumentDirectory inDomains:NSUserDomainMask].firstObject;
    NSURL *root = [documents URLByAppendingPathComponent:@"RockPodSimulator"
                                             isDirectory:YES];
    NSURL *target = [root URLByAppendingPathComponent:relative];
    [NSFileManager.defaultManager createDirectoryAtURL:
        target.URLByDeletingLastPathComponent withIntermediateDirectories:YES
        attributes:nil error:error];
    [NSFileManager.defaultManager removeItemAtURL:target error:nil];
    return [NSFileManager.defaultManager copyItemAtURL:url toURL:target
                                                 error:error];
}

- (BOOL)registerSimulatorVideoRowAtURL:(NSURL *)rowURL error:(NSError **)error
{
    NSString *row = [NSString stringWithContentsOfURL:rowURL
        encoding:NSUTF8StringEncoding error:error];
    row = [row stringByTrimmingCharactersInSet:
        NSCharacterSet.newlineCharacterSet];
    NSArray<NSString *> *fields = [row componentsSeparatedByString:@"\t"];
    NSString *identifier = fields.firstObject;
    if (!row.length || !identifier.length || fields.count < 7)
        return NO;

    NSString *mediaRelative = [fields[6] hasPrefix:@"/"] ?
        [fields[6] substringFromIndex:1] : fields[6];
    NSURL *documents = [NSFileManager.defaultManager URLsForDirectory:
        NSDocumentDirectory inDomains:NSUserDomainMask].firstObject;
    NSURL *media = [[documents URLByAppendingPathComponent:@"RockPodSimulator"
                                              isDirectory:YES]
        URLByAppendingPathComponent:mediaRelative];
    if (![NSFileManager.defaultManager fileExistsAtPath:media.path])
    {
        if (error)
            *error = [NSError errorWithDomain:@"RockPodSimulator" code:2
                userInfo:@{NSLocalizedDescriptionKey:
                    @"Converted video is missing from the simulator media root."}];
        return NO;
    }

    NSURL *index = [self simulatorMediaURLForFolder:@".rockbox/videolist"
                                               name:@"index.tsv"];
    NSString *existing = [NSString stringWithContentsOfURL:index
        encoding:NSUTF8StringEncoding error:nil] ?: @"";
    NSString *needle = [NSString stringWithFormat:@"\n%@\t", identifier];
    if ([existing hasPrefix:[identifier stringByAppendingString:@"\t"]] ||
        [existing containsString:needle])
        return YES;
    if (!existing.length)
        existing = @"# rockpod videolist v6\n"
            @"video_id\tthumb\tpreview\ttitle\tkind\tgroup_key\tdevice_path\t"
            @"show\tseason\tepisode\tduration\tlocked\tyear\tgenre\trating\t"
            @"plot_short\tplot_long\tcontent_rating\tnetflix_poster\t"
            @"netflix_detail\tshow_art_id\tseason_art_id\tshow_plot\n";
    else if (![existing hasSuffix:@"\n"])
        existing = [existing stringByAppendingString:@"\n"];
    existing = [existing stringByAppendingFormat:@"%@\n", row];
    return [existing writeToURL:index atomically:YES
                       encoding:NSUTF8StringEncoding error:error];
}

- (void)installVideoConversionInSimulator:(RPVideoConversion *)conversion
{
    if (!conversion.files.count ||
        conversion.files.count != conversion.destinations.count)
    {
        self.syncProgress.progress = 0;
        self.syncStatus.text = @"Simulator video bundle is incomplete.";
        self.pendingSimulatorImports = MAX(0,
            self.pendingSimulatorImports - 1);
        return;
    }
    [[RPSimulatorHost sharedHost] prepareForMediaWithCompletion:
        ^(BOOL ready, NSString *message) {
        if (!ready)
        {
            self.syncProgress.progress = 0;
            self.syncStatus.text = message;
            self.pendingSimulatorImports = MAX(0,
                self.pendingSimulatorImports - 1);
            return;
        }
        dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
            NSError *error = nil;
            BOOL success = YES;
            NSUInteger catalogIndex = conversion.files.count - 1;
            for (NSUInteger index = 0; index < catalogIndex; index++)
            {
                if (![self copyURL:conversion.files[index]
                    toSimulatorDestination:conversion.destinations[index]
                    error:&error])
                {
                    success = NO;
                    break;
                }
            }
            if (success)
                success = [self registerSimulatorVideoRowAtURL:
                    conversion.files[catalogIndex] error:&error];
            dispatch_async(dispatch_get_main_queue(), ^{
                self.syncProgress.progress = success ? 1.0 : 0.0;
                self.syncStatus.text = success ? [NSString stringWithFormat:
                    @"%@ is ready in simulator Netflix/Videos.", conversion.title] :
                    (error.localizedDescription ?: @"Simulator video install failed.");
                self.pendingSimulatorImports = MAX(0,
                    self.pendingSimulatorImports - 1);
            });
        });
    }];
}

- (void)convertAndInstallSimulatorVideoURL:(NSURL *)url
{
    self.pendingSimulatorImports++;
    if (!self.videoConverter)
        self.videoConverter = [[RPVideoConverter alloc] init];
    self.syncProgress.progress = 0.05;
    self.syncStatus.text = @"Encoding simulator video as Rockbox MPEG…";
    NSDictionary *metadata = @{
        @"kind": @"home_video",
        @"title": url.URLByDeletingPathExtension.lastPathComponent ?: @"Phone Video"
    };
    [self.videoConverter convertURL:url metadata:metadata
        completion:^(RPVideoConversion *conversion, NSString *errorMessage) {
        if (!conversion)
        {
            self.syncProgress.progress = 0;
            self.syncStatus.text = errorMessage ?: @"Simulator video conversion failed.";
            self.pendingSimulatorImports = MAX(0,
                self.pendingSimulatorImports - 1);
            return;
        }
        self.syncProgress.progress = 0.75;
        [self installVideoConversionInSimulator:conversion];
    }];
}

- (void)convertAndQueueVideoURL:(NSURL *)url
{
    if (!self.videoConverter)
        self.videoConverter = [[RPVideoConverter alloc] init];
    if (!self.videoMetadata)
        self.videoMetadata = [[RPVideoMetadataService alloc] init];
    self.syncStatus.text = @"Identifying title, show, season and episode…";
    [self.videoMetadata lookupVideoAtURL:url completion:^(NSDictionary *metadata,
                                                          NSString *lookupError) {
        NSString *title = metadata[@"title"] ?: url.URLByDeletingPathExtension.lastPathComponent;
        NSString *detail = [metadata[@"kind"] isEqualToString:@"show"] ?
            [NSString stringWithFormat:@"%@ • Season %@ • Episode %@\n%@",
             metadata[@"show"], metadata[@"season"], metadata[@"episode"], title] :
            [NSString stringWithFormat:@"Movie • %@%@", title,
             metadata[@"year"] ? [NSString stringWithFormat:@" (%@)", metadata[@"year"]] : @""];
        UIAlertController *confirm = [UIAlertController alertControllerWithTitle:
            metadata ? @"Video Match Found" : @"No Metadata Match"
            message:metadata ? detail : (lookupError ?: @"Sync this as an unmatched home video?")
            preferredStyle:UIAlertControllerStyleAlert];
        if (metadata)
            [confirm addAction:[UIAlertAction actionWithTitle:@"Use Match & Sync"
                style:UIAlertActionStyleDefault handler:^(UIAlertAction *action) {
                (void)action; [self convertAndQueueVideoURL:url metadata:metadata];
            }]];
        [confirm addAction:[UIAlertAction actionWithTitle:@"Sync as Home Video"
            style:UIAlertActionStyleDefault handler:^(UIAlertAction *action) {
            (void)action; [self convertAndQueueVideoURL:url metadata:@{
                @"kind": @"home_video", @"title": url.URLByDeletingPathExtension.lastPathComponent }];
        }]];
        [confirm addAction:[UIAlertAction actionWithTitle:@"Cancel" style:UIAlertActionStyleCancel handler:nil]];
        [self presentViewController:confirm animated:YES completion:nil];
    }];
}

- (void)convertAndQueueVideoURL:(NSURL *)url metadata:(NSDictionary *)metadata
{
    if (!self.videoConverter) self.videoConverter = [[RPVideoConverter alloc] init];
    self.syncProgress.progress = 0.05;
    self.syncStatus.text = @"Encoding Rockbox MPEG-2 video… Keep RockPod Link open.";
    [self.videoConverter convertURL:url metadata:metadata completion:^(RPVideoConversion *result,
                                                     NSString *errorMessage) {
        if (!result)
        {
            self.syncProgress.progress = 0;
            self.syncStatus.text = errorMessage ?: @"Video conversion failed.";
            return;
        }
        self.syncProgress.progress = 0.25;
        self.syncStatus.text = [NSString stringWithFormat:
            @"%@ encoded as MPEG. Syncing video, artwork and catalog…",
            result.title];
        [self queueVideoConversion:result index:0];
    }];
}

- (void)queueVideoConversion:(RPVideoConversion *)conversion
                        index:(NSUInteger)index
{
    if (index >= conversion.files.count ||
        index >= conversion.destinations.count)
    {
        self.syncProgress.progress = 1.0;
        self.syncStatus.text = [NSString stringWithFormat:
            @"%@ is synced and registered in Netflix/Videos.", conversion.title];
        return;
    }

    NSURL *file = conversion.files[index];
    NSString *destination = conversion.destinations[index];
    self.syncProgress.progress = 0.25 +
        0.7 * ((double)index / (double)MAX(conversion.files.count, 1));
    [self.relay enqueueFileAtURL:file destination:destination
        completion:^(BOOL success, NSString *message) {
        if (!success)
        {
            self.syncProgress.progress = 0;
            self.syncStatus.text = [NSString stringWithFormat:
                @"%@ sync stopped at %@: %@", conversion.title,
                destination.lastPathComponent, message ?: @"transfer failed"];
            return;
        }
        [self queueVideoConversion:conversion index:index + 1];
    }];
}

- (void)exportMusicItem:(MPMediaItem *)item
{
    NSString *title = [self safeName:
        [item valueForProperty:MPMediaItemPropertyTitle] fallback:@"Track"];
    NSString *artist = [self safeName:
        [item valueForProperty:MPMediaItemPropertyArtist] fallback:@"Unknown Artist"];
    NSString *album = [self safeName:
        [item valueForProperty:MPMediaItemPropertyAlbumTitle] fallback:@"Unknown Album"];
    NSURL *assetURL = [item valueForProperty:MPMediaItemPropertyAssetURL];
    if (!assetURL)
    {
        self.syncStatus.text = @"That Apple Music item is protected, cloud-only, or not downloaded.";
        return;
    }
    AVURLAsset *asset = [AVURLAsset URLAssetWithURL:assetURL options:nil];
    AVAssetExportSession *exporter = [[AVAssetExportSession alloc]
        initWithAsset:asset presetName:AVAssetExportPresetAppleM4A];
    if (!exporter)
    {
        self.syncStatus.text = @"This music item cannot be exported.";
        return;
    }
    NSURL *output = [NSFileManager.defaultManager.temporaryDirectory
        URLByAppendingPathComponent:[NSString stringWithFormat:@"%@.m4a",
                                      NSUUID.UUID.UUIDString]];
    exporter.outputURL = output;
    exporter.outputFileType = AVFileTypeAppleM4A;
    exporter.metadata = asset.commonMetadata;
    [exporter exportAsynchronouslyWithCompletionHandler:^{
        dispatch_async(dispatch_get_main_queue(), ^{
            if (exporter.status != AVAssetExportSessionStatusCompleted)
            {
                self.syncStatus.text = @"Apple Music protected/cloud tracks cannot be copied; use an owned download or Files.";
                return;
            }
            NSString *folder = [NSString stringWithFormat:
                @"/Music/RockPodLink/%@/%@", artist, album];
            [self queueURL:output destination:[folder stringByAppendingPathComponent:
                [title stringByAppendingPathExtension:@"m4a"]]];
            MPMediaItemArtwork *art = [item valueForProperty:MPMediaItemPropertyArtwork];
            UIImage *image = [art imageWithSize:CGSizeMake(600, 600)];
            NSData *jpeg = image ? UIImageJPEGRepresentation(image, 0.9) : nil;
            if (jpeg)
            {
                NSURL *cover = [NSFileManager.defaultManager.temporaryDirectory
                    URLByAppendingPathComponent:[NSString stringWithFormat:
                        @"cover-%@.jpg", NSUUID.UUID.UUIDString]];
                if ([jpeg writeToURL:cover atomically:YES])
                    [self queueURL:cover destination:
                        [folder stringByAppendingPathComponent:@"cover.jpg"]];
            }
        });
    }];
}

- (void)presentPhotoPickerFor:(RPPickerPurpose)purpose videos:(BOOL)videos
{
    self.pickerPurpose = purpose;
    if (NSClassFromString(@"PHPickerViewController") != nil)
    {
        PHPickerConfiguration *configuration = [[PHPickerConfiguration alloc] init];
        configuration.filter = videos ? PHPickerFilter.videosFilter :
                                        PHPickerFilter.imagesFilter;
        configuration.selectionLimit = (purpose == RPPickerPurposeSyncPhotos ||
                                        purpose == RPPickerPurposeSyncVideos) ? 0 : 1;
        PHPickerViewController *picker = [[PHPickerViewController alloc]
            initWithConfiguration:configuration];
        picker.delegate = self;
        [self presentViewController:picker animated:YES completion:nil];
    }
    else
    {
        UIImagePickerController *picker = [[UIImagePickerController alloc] init];
        picker.delegate = self;
        picker.mediaTypes = @[videos ? @"public.movie" : @"public.image"];
        [self presentViewController:picker animated:YES completion:nil];
    }
}

- (void)addPhotos { [self presentPhotoPickerFor:RPPickerPurposeSyncPhotos videos:NO]; }
- (void)addVideos { [self presentPhotoPickerFor:RPPickerPurposeSyncVideos videos:YES]; }
- (void)previewPhoto { [self presentPhotoPickerFor:RPPickerPurposeSimulatorPhoto videos:NO]; }
- (void)previewVideo { [self presentPhotoPickerFor:RPPickerPurposeSimulatorVideo videos:YES]; }

- (void)addMusic
{
    self.pickerPurpose = RPPickerPurposeNone;
    MPMediaPickerController *picker = [[MPMediaPickerController alloc]
        initWithMediaTypes:MPMediaTypeMusic];
    picker.delegate = self;
    picker.allowsPickingMultipleItems = YES;
    [self presentViewController:picker animated:YES completion:nil];
}

- (void)previewMusic
{
    self.pickerPurpose = RPPickerPurposeSimulatorMusic;
    MPMediaPickerController *picker = [[MPMediaPickerController alloc]
        initWithMediaTypes:MPMediaTypeMusic];
    picker.delegate = self;
    picker.allowsPickingMultipleItems = YES;
    [self presentViewController:picker animated:YES completion:nil];
}

- (void)addDocuments
{
    UIDocumentPickerViewController *picker = [[UIDocumentPickerViewController alloc]
        initWithDocumentTypes:@[@"public.audio", @"public.movie", @"public.video"]
                       inMode:UIDocumentPickerModeOpen];
    picker.delegate = self;
    picker.allowsMultipleSelection = YES;
    [self presentViewController:picker animated:YES completion:nil];
}

- (void)refreshSitekick
{
    self.sitekickStatus.text = @"Loading current Sitekick from iPod…";
    [self.relay requestSitekickPreview];
}

- (void)retryConnection
{
    self.displayLabel.text = @"Restarting iPod USB Link…";
    self.footerLabel.text = @"Looking for iPod in iPhone USB Link mode";
    self.musicCatalogRequested = NO;
    [self.relay stop];
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.6 * NSEC_PER_SEC)),
                   dispatch_get_main_queue(), ^{
        [self.relay start];
    });
}

- (void)safeDisconnect
{
    self.footerLabel.text = @"Flushing iPod storage…";
    [self.relay requestSafeDisconnect:^(BOOL success, NSString *message) {
        self.footerLabel.text = message;
        self.displayLabel.text = success ? @"iPod sync is complete. OK to disconnect." :
                                           @"Safe disconnect was not confirmed";
    }];
}

- (void)picker:(PHPickerViewController *)picker
 didFinishPicking:(NSArray<PHPickerResult *> *)results API_AVAILABLE(ios(14))
{
    [picker dismissViewControllerAnimated:YES completion:nil];
    RPPickerPurpose purpose = self.pickerPurpose;
    for (PHPickerResult *result in results)
    {
        NSItemProvider *provider = result.itemProvider;
        if ((purpose == RPPickerPurposeSyncPhotos ||
             purpose == RPPickerPurposeSimulatorPhoto) &&
            [provider canLoadObjectOfClass:UIImage.class])
        {
            [provider loadObjectOfClass:UIImage.class completionHandler:
                ^(id<NSItemProviderReading> object, NSError *error) {
                UIImage *image = (UIImage *)object;
                if (!image || error)
                    return;
                dispatch_async(dispatch_get_main_queue(), ^{
                    if (purpose == RPPickerPurposeSimulatorPhoto)
                    {
                        NSString *name = [NSString stringWithFormat:@"IMG-%@.jpg",
                            NSUUID.UUID.UUIDString];
                        NSURL *local = [self simulatorMediaURLForFolder:@"Photos"
                                                                  name:name];
                        [UIImageJPEGRepresentation(RPPhotoForDevice(image), 0.9)
                            writeToURL:local atomically:YES];
                        NSData *thumb = RPPhotoThumbnailBMP(image);
                        NSURL *thumbURL = [self simulatorMediaURLForFolder:
                            @"Photos/.photo_thumbs"
                            name:[name stringByAppendingString:@".bmp"]];
                        [thumb writeToURL:thumbURL atomically:YES];
                        self.syncStatus.text = @"Photo added to local Rockbox simulator";
                        return;
                    }
                    NSData *jpeg = UIImageJPEGRepresentation(
                        RPPhotoForDevice(image), 0.88);
                    NSString *name = [NSString stringWithFormat:@"IMG-%@.jpg",
                        NSUUID.UUID.UUIDString];
                    NSURL *url = [[NSFileManager.defaultManager temporaryDirectory]
                        URLByAppendingPathComponent:name];
                    if ([jpeg writeToURL:url atomically:YES])
                    {
                        NSData *thumbnail = RPPhotoThumbnailBMP(image);
                        NSURL *thumbURL = [[NSFileManager.defaultManager temporaryDirectory]
                            URLByAppendingPathComponent:[name stringByAppendingString:@".bmp"]];
                        if ([thumbnail writeToURL:thumbURL atomically:YES])
                            [self queueURL:thumbURL destination:
                                [@"/Photos/.photo_thumbs" stringByAppendingPathComponent:
                                    [name stringByAppendingString:@".bmp"]]];
                        /* Publish the sidecar first. If Photos is already
                         * open, the new JPEG can never be discovered during
                         * the brief window before its thumbnail exists. */
                        [self queueURL:url destination:
                            [NSString stringWithFormat:@"/Photos/%@", name]];
                    }
                });
            }];
        }
        else
        {
            NSString *type = @"public.movie";
            [provider loadFileRepresentationForTypeIdentifier:type
                completionHandler:^(NSURL *url, NSError *error) {
                if (!url || error)
                    return;
                NSString *name = [self safeName:url.lastPathComponent
                                       fallback:@"Phone Video.mov"];
                NSURL *copy = [[NSFileManager.defaultManager temporaryDirectory]
                    URLByAppendingPathComponent:
                        [NSString stringWithFormat:@"%@-%@", NSUUID.UUID.UUIDString,
                                                   name]];
                [NSFileManager.defaultManager copyItemAtURL:url toURL:copy error:nil];
                dispatch_async(dispatch_get_main_queue(), ^{
                    if (purpose == RPPickerPurposeSimulatorVideo)
                    {
                        [self convertAndInstallSimulatorVideoURL:copy];
                    }
                    else
                        [self convertAndQueueVideoURL:copy];
                });
            }];
        }
    }
}

- (void)documentPicker:(UIDocumentPickerViewController *)controller
 didPickDocumentsAtURLs:(NSArray<NSURL *> *)urls
{
    (void)controller;
    if (self.pickerPurpose == RPPickerPurposeGameFiles)
    {
        if (!self.relay.isConnected)
        {
            self.gameStatus.text = @"Connect the iPod in iPhone USB Link mode before syncing.";
            return;
        }
        if (!self.gameStore)
            self.gameStore = [[RPGameStoreService alloc] init];
        self.gameStatus.text = [NSString stringWithFormat:@"Preparing %lu selected game file%@…",
            (unsigned long)urls.count, urls.count == 1 ? @"" : @"s"];
        for (NSURL *url in urls)
        {
            if (![RPGameStoreService isSupportedFilename:url.lastPathComponent] &&
                ![url.pathExtension.lowercaseString isEqualToString:@"zip"])
            {
                self.gameStatus.text = [NSString stringWithFormat:@"%@ is not a supported game file.",
                                        url.lastPathComponent];
                continue;
            }
            [self prepareAndSyncLocalGameURL:url];
        }
        self.pickerPurpose = RPPickerPurposeNone;
        return;
    }
    for (NSURL *url in urls)
    {
        NSString *ext = url.pathExtension.lowercaseString;
        BOOL video = [@[@"mov", @"mp4", @"m4v", @"mpg", @"mpeg"]
                      containsObject:ext];
        if (video)
            [self convertAndQueueVideoURL:url];
        else
            [self queueURL:url folder:@"/Music/RockPodLink"];
    }
}

- (void)mediaPicker:(MPMediaPickerController *)mediaPicker
  didPickMediaItems:(MPMediaItemCollection *)mediaItemCollection
{
    [mediaPicker dismissViewControllerAnimated:YES completion:nil];
    for (MPMediaItem *item in mediaItemCollection.items)
    {
        NSString *title = [item valueForProperty:MPMediaItemPropertyTitle] ?: @"Music";
        if (self.pickerPurpose == RPPickerPurposeSimulatorMusic)
        {
            NSURL *assetURL = [item valueForProperty:MPMediaItemPropertyAssetURL];
            if (assetURL)
            {
                NSString *artist = [self safeName:
                    [item valueForProperty:MPMediaItemPropertyArtist]
                                       fallback:@"Unknown Artist"];
                NSString *album = [self safeName:
                    [item valueForProperty:MPMediaItemPropertyAlbumTitle]
                                       fallback:@"Unknown Album"];
                unsigned long long persistentID = [[item valueForProperty:
                    MPMediaItemPropertyPersistentID] unsignedLongLongValue];
                AVURLAsset *asset = [AVURLAsset URLAssetWithURL:assetURL options:nil];
                AVAssetExportSession *exporter = [[AVAssetExportSession alloc]
                    initWithAsset:asset presetName:AVAssetExportPresetAppleM4A];
                if (!exporter)
                {
                    self.syncStatus.text = @"That music item cannot be exported for Rockbox.";
                    continue;
                }
                NSString *name = [NSString stringWithFormat:@"%016llx - %@.m4a",
                    persistentID, [self safeName:title fallback:@"Track"]];
                NSURL *destination = [self simulatorMediaURLForFolder:
                    [@"Music" stringByAppendingPathComponent:
                        [artist stringByAppendingPathComponent:album]] name:name];
                if ([NSFileManager.defaultManager fileExistsAtPath:destination.path])
                {
                    self.syncStatus.text = [NSString stringWithFormat:
                        @"%@ is already in the simulator.", title];
                    continue;
                }
                NSURL *staging = [NSFileManager.defaultManager.temporaryDirectory
                    URLByAppendingPathComponent:[NSString stringWithFormat:
                        @"sim-music-%@.m4a", NSUUID.UUID.UUIDString]];
                exporter.outputURL = staging;
                exporter.outputFileType = AVFileTypeAppleM4A;
                exporter.metadata = asset.commonMetadata;
                self.pendingSimulatorImports++;
                [exporter exportAsynchronouslyWithCompletionHandler:^{
                    dispatch_async(dispatch_get_main_queue(), ^{
                        BOOL installed = NO;
                        if (exporter.status == AVAssetExportSessionStatusCompleted)
                        {
                            NSError *moveError = nil;
                            installed = [NSFileManager.defaultManager
                                moveItemAtURL:staging toURL:destination
                                error:&moveError];
                            if (!installed)
                                self.syncStatus.text = moveError.localizedDescription ?:
                                    @"Music export could not be installed.";
                        }
                        if (!installed)
                            [NSFileManager.defaultManager removeItemAtURL:staging
                                                                    error:nil];
                        if (installed)
                            self.syncStatus.text =
                                @"Music added to local Rockbox simulator";
                        else if (exporter.status !=
                                 AVAssetExportSessionStatusCompleted)
                            self.syncStatus.text =
                                @"That music item could not be added locally";
                        self.pendingSimulatorImports = MAX(0,
                            self.pendingSimulatorImports - 1);
                    });
                }];
            }
            else
                self.syncStatus.text = @"That music item is protected or cloud-only";
        }
        else
            [self exportMusicItem:item];
    }
}

- (void)mediaPickerDidCancel:(MPMediaPickerController *)mediaPicker
{
    [mediaPicker dismissViewControllerAnimated:YES completion:nil];
}

- (void)imagePickerController:(UIImagePickerController *)picker
 didFinishPickingMediaWithInfo:(NSDictionary<UIImagePickerControllerInfoKey,id> *)info
{
    [picker dismissViewControllerAnimated:YES completion:nil];
    UIImage *image = info[UIImagePickerControllerOriginalImage];
    NSURL *url = info[UIImagePickerControllerMediaURL];
    if (image)
    {
        if (self.pickerPurpose == RPPickerPurposeSimulatorPhoto)
        {
            NSURL *file = [self simulatorMediaURLForFolder:@"Photos"
                name:[NSString stringWithFormat:@"IMG-%@.jpg",
                    NSUUID.UUID.UUIDString]];
            [UIImageJPEGRepresentation(RPPhotoForDevice(image), 0.9) writeToURL:file atomically:YES];
            NSData *thumb = RPPhotoThumbnailBMP(image);
            NSURL *thumbURL = [self simulatorMediaURLForFolder:
                @"Photos/.photo_thumbs"
                name:[file.lastPathComponent stringByAppendingString:@".bmp"]];
            [thumb writeToURL:thumbURL atomically:YES];
        }
        else
        {
            NSURL *file = [NSFileManager.defaultManager.temporaryDirectory
                URLByAppendingPathComponent:[NSString stringWithFormat:@"IMG-%@.jpg",
                                              NSUUID.UUID.UUIDString]];
            [UIImageJPEGRepresentation(image, 0.88) writeToURL:file atomically:YES];
            NSString *name = file.lastPathComponent;
            NSData *thumbnail = RPPhotoThumbnailBMP(image);
            NSURL *thumbURL = [NSFileManager.defaultManager.temporaryDirectory
                URLByAppendingPathComponent:[name stringByAppendingString:@".bmp"]];
            if ([thumbnail writeToURL:thumbURL atomically:YES])
                [self queueURL:thumbURL destination:
                    [@"/Photos/.photo_thumbs" stringByAppendingPathComponent:
                        [name stringByAppendingString:@".bmp"]]];
            [self queueURL:file destination:
                [NSString stringWithFormat:@"/Photos/%@", name]];
        }
    }
    else if (url)
    {
        if (self.pickerPurpose == RPPickerPurposeSimulatorVideo)
            [self convertAndInstallSimulatorVideoURL:url];
        else
            [self convertAndQueueVideoURL:url];
    }
}

@end
