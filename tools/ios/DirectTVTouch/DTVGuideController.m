#import "DTVGuideController.h"
#import "DTVClient.h"
#import <AVFoundation/AVFoundation.h>
#import <QuartzCore/QuartzCore.h>

static UIColor *DTVColor(unsigned value) {
    return [UIColor colorWithRed:((value >> 16) & 255) / 255.0
        green:((value >> 8) & 255) / 255.0 blue:(value & 255) / 255.0 alpha:1.0];
}
static UIColor *DTVBannerTop(void) { return DTVColor(0xDCEEF9); }
static UIColor *DTVBannerBottom(void) { return DTVColor(0xB7D6E9); }
static UIColor *DTVStripText(void) { return DTVColor(0x10386B); }
static UIColor *DTVDescription(void) { return DTVColor(0x026FAF); }
static UIColor *DTVHeader(void) { return DTVColor(0x122549); }
static UIColor *DTVRow(void) { return DTVColor(0x094871); }
static UIColor *DTVGridLine(void) { return DTVColor(0x0A2A50); }
static UIColor *DTVSelection(void) { return DTVColor(0xFEC425); }
static UIColor *DTVSelectionText(void) { return DTVColor(0x10254A); }
static UIColor *DTVHint(void) { return DTVColor(0x0F5689); }
static NSString * const DTVSyncScope = @"featured";

@class DTVGuideView;

@interface DTVSettingsController : UIViewController
@property(nonatomic, copy) void (^pairedHandler)(void);
@property(nonatomic, copy) void (^dismissHandler)(void);
@property(nonatomic) BOOL canCancel;
@end

@interface DTVGuideView : UIView
@property(nonatomic, weak) DTVGuideController *owner;
@property(nonatomic, strong) NSDictionary *manifest;
@property(nonatomic, strong) AVPlayerLayer *playerLayer;
@property(nonatomic, copy) NSString *status;
@property(nonatomic) NSInteger selectedChannel;
@property(nonatomic) NSInteger selectedDelta;
@property(nonatomic) NSInteger windowDelta;
@property(nonatomic) BOOL fullScreen;
- (NSDictionary *)slotForChannelIndex:(NSInteger)index delta:(NSInteger)delta;
- (void)moveChannels:(NSInteger)amount;
- (void)moveTime:(NSInteger)amount;
@end

@interface DTVGuideController ()
@property(nonatomic, strong) DTVGuideView *guideView;
@property(nonatomic, strong) AVPlayer *player;
@property(nonatomic, copy) NSString *currentMediaID;
@property(nonatomic) NSInteger currentChannel;
@property(nonatomic, strong) NSDictionary *pendingManifest;
@property(nonatomic) BOOL settingsVisible;
@property(nonatomic) BOOL syncInProgress;
- (void)startSync;
- (void)watchSelectedChannel;
- (BOOL)isTransientNetworkError:(NSError *)error;
- (void)retryStartSync;
@end

static void DTVResolveDelta(NSInteger delta, NSInteger *day, NSInteger *seconds) {
    NSDate *now = [NSDate date];
    NSCalendar *calendar = [NSCalendar currentCalendar];
    NSDateComponents *parts = [calendar components:
        (NSHourCalendarUnit | NSMinuteCalendarUnit | NSSecondCalendarUnit | NSWeekdayCalendarUnit)
        fromDate:now];
    NSInteger total = parts.hour * 3600 + parts.minute * 60 + parts.second + delta;
    NSInteger shift = 0;
    while (total < 0) { total += 86400; shift--; }
    while (total >= 86400) { total -= 86400; shift++; }
    NSInteger sundayZero = parts.weekday - 1;
    *day = (sundayZero + shift) % 7;
    if (*day < 0) *day += 7;
    *seconds = total;
}

static NSInteger DTVSecondsNow(void) {
    NSDateComponents *parts = [[NSCalendar currentCalendar] components:
        (NSHourCalendarUnit | NSMinuteCalendarUnit | NSSecondCalendarUnit) fromDate:[NSDate date]];
    return parts.hour * 3600 + parts.minute * 60 + parts.second;
}

static NSString *DTVClock(NSInteger seconds) {
    seconds = (seconds % 86400 + 86400) % 86400;
    NSInteger hour = seconds / 3600, minute = (seconds / 60) % 60;
    NSString *suffix = hour >= 12 ? @"p" : @"a";
    NSInteger shown = hour % 12; if (!shown) shown = 12;
    return [NSString stringWithFormat:@"%ld:%02ld%@", (long)shown, (long)minute, suffix];
}

@implementation DTVGuideView

- (id)initWithFrame:(CGRect)frame {
    if ((self = [super initWithFrame:frame])) {
        self.backgroundColor = [UIColor blackColor];
        self.opaque = YES;
        _selectedChannel = 0; _selectedDelta = 0; _windowDelta = 0;
    }
    return self;
}

- (void)setManifest:(NSDictionary *)manifest {
    _manifest = manifest;
    if (self.selectedChannel >= (NSInteger)[manifest[@"channels"] count]) self.selectedChannel = 0;
    [self setNeedsDisplay];
}
- (void)setStatus:(NSString *)status { _status = [status copy]; [self setNeedsDisplay]; }
- (void)setFullScreen:(BOOL)fullScreen {
    _fullScreen = fullScreen;
    self.playerLayer.frame = fullScreen ? self.bounds : CGRectMake(357, 5, 123, 83);
    [self setNeedsDisplay];
}
- (void)setPlayerLayer:(AVPlayerLayer *)playerLayer {
    [_playerLayer removeFromSuperlayer]; _playerLayer = playerLayer;
    if (playerLayer) { [self.layer addSublayer:playerLayer]; self.fullScreen = self.fullScreen; }
}

- (NSDictionary *)slotForChannelIndex:(NSInteger)index delta:(NSInteger)delta {
    NSArray *channels = self.manifest[@"channels"];
    if (index < 0 || index >= (NSInteger)channels.count) return nil;
    NSInteger day, seconds; DTVResolveDelta(delta, &day, &seconds);
    NSInteger number = [channels[(NSUInteger)index][@"number"] integerValue];
    for (NSDictionary *slot in self.manifest[@"slots"]) {
        if ([slot[@"channel"] integerValue] != number || [slot[@"day"] integerValue] != day) continue;
        NSInteger start = [slot[@"start"] integerValue];
        NSInteger duration = [slot[@"duration"] integerValue];
        if (seconds >= start && seconds < start + duration) return slot;
    }
    return nil;
}

- (BOOL)sameBlock:(NSDictionary *)first other:(NSDictionary *)second {
    if (!first || !second) return NO;
    return [first[@"channel"] isEqual:second[@"channel"]] &&
        [first[@"day"] isEqual:second[@"day"]] &&
        [first[@"block_start"] isEqual:second[@"block_start"]];
}

- (void)text:(NSString *)text rect:(CGRect)rect font:(UIFont *)font color:(UIColor *)color {
    [color set];
    [text ?: @"" drawInRect:CGRectInset(rect, 3, 1) withFont:font
        lineBreakMode:NSLineBreakByTruncatingTail alignment:NSTextAlignmentLeft];
}

- (void)drawRect:(CGRect)rect {
    (void)rect;
    if (self.fullScreen) { [[UIColor blackColor] setFill]; UIRectFill(self.bounds); return; }
    CGContextRef context = UIGraphicsGetCurrentContext();
    CGRect bounds = self.bounds;
    [DTVBannerTop() setFill]; UIRectFill(CGRectMake(0, 0, bounds.size.width, 17));
    [DTVBannerBottom() setFill]; UIRectFill(CGRectMake(0, 17, bounds.size.width, 37));
    [DTVDescription() setFill]; UIRectFill(CGRectMake(0, 54, bounds.size.width, 48));
    [DTVHeader() setFill]; UIRectFill(CGRectMake(0, 102, bounds.size.width, 19));

    UIImage *brand = [UIImage imageNamed:@"directv.png"];
    if (brand) [brand drawInRect:CGRectMake(6, 2, 96, 33)];
    else [self text:@"DIRECTV" rect:CGRectMake(5, 3, 95, 26)
        font:[UIFont boldSystemFontOfSize:16] color:DTVStripText()];

    NSArray *channels = self.manifest[@"channels"] ?: @[];
    NSDictionary *selected = [self slotForChannelIndex:self.selectedChannel delta:self.selectedDelta];
    if (!selected && channels.count) selected = [self slotForChannelIndex:self.selectedChannel delta:0];
    NSString *title = selected[@"title"] ?: @"Live TV";
    [self text:title rect:CGRectMake(106, 3, 245, 28) font:[UIFont systemFontOfSize:14] color:DTVStripText()];
    [self text:@"guide" rect:CGRectMake(309, 3, 43, 25) font:[UIFont systemFontOfSize:12]
        color:DTVColor(0x7CA2C1)];

    NSInteger selectedDay, selectedSeconds;
    DTVResolveDelta(self.selectedDelta, &selectedDay, &selectedSeconds);
    static NSString *days[] = {@"Sun", @"Mon", @"Tue", @"Wed", @"Thu", @"Fri", @"Sat"};
    NSString *air = @"";
    if (selected) {
        NSInteger start = [selected[@"block_start"] integerValue];
        NSInteger end = start + [selected[@"block_duration"] integerValue];
        air = [NSString stringWithFormat:@"%@ - %@  %@", DTVClock(start), DTVClock(end), selected[@"rating"] ?: @"--"];
    }
    [self text:[NSString stringWithFormat:@"%@ %@", days[selectedDay], DTVClock(selectedSeconds)]
        rect:CGRectMake(3, 34, 100, 20) font:[UIFont systemFontOfSize:12] color:DTVStripText()];
    [self text:air rect:CGRectMake(106, 34, 244, 20) font:[UIFont systemFontOfSize:12] color:DTVStripText()];
    [self text:selected[@"description"] ?: @"Sync with RockPod to add local Live TV."
        rect:CGRectMake(4, 56, 348, 44) font:[UIFont systemFontOfSize:13] color:[UIColor whiteColor]];

    NSInteger baseDelta = self.windowDelta - (DTVSecondsNow() % 1800);
    NSInteger headerDay, headerSeconds; DTVResolveDelta(baseDelta, &headerDay, &headerSeconds);
    [self text:[NSString stringWithFormat:@"%@", days[headerDay]] rect:CGRectMake(0, 102, 102, 19)
        font:[UIFont boldSystemFontOfSize:12] color:[UIColor whiteColor]];
    for (NSInteger column = 0; column < 3; column++)
        [self text:DTVClock(headerSeconds + column * 1800)
            rect:CGRectMake(102 + column * 126, 102, 126, 19)
            font:[UIFont boldSystemFontOfSize:12] color:[UIColor whiteColor]];

    CGFloat rowHeight = 29.333;
    NSInteger top = MAX(0, MIN(self.selectedChannel - 2, MAX(0, (NSInteger)channels.count - 6)));
    for (NSInteger row = 0; row < 6; row++) {
        NSInteger channelIndex = top + row;
        CGFloat y = 121 + row * rowHeight;
        [DTVHeader() setFill]; UIRectFill(CGRectMake(0, y, 102, rowHeight));
        [DTVRow() setFill]; UIRectFill(CGRectMake(102, y, 378, rowHeight));
        [DTVGridLine() setStroke]; CGContextSetLineWidth(context, 1);
        CGContextMoveToPoint(context, 0, y + rowHeight); CGContextAddLineToPoint(context, 480, y + rowHeight); CGContextStrokePath(context);
        if (channelIndex >= (NSInteger)channels.count) continue;
        NSDictionary *channel = channels[(NSUInteger)channelIndex];
        NSString *call = [NSString stringWithFormat:@"%@ %@", channel[@"number"], channel[@"callsign"]];
        [self text:call rect:CGRectMake(1, y + 3, 100, rowHeight - 4)
            font:[UIFont systemFontOfSize:12] color:[UIColor whiteColor]];
        NSInteger column = 0;
        while (column < 3) {
            NSDictionary *slot = [self slotForChannelIndex:channelIndex delta:baseDelta + column * 1800];
            NSInteger span = 1;
            while (column + span < 3 && [self sameBlock:slot other:
                    [self slotForChannelIndex:channelIndex delta:baseDelta + (column + span) * 1800]]) span++;
            CGRect cell = CGRectMake(102 + column * 126, y, span * 126, rowHeight);
            BOOL chosen = channelIndex == self.selectedChannel && [self sameBlock:slot other:selected];
            if (chosen) { [DTVSelection() setFill]; UIRectFill(CGRectInset(cell, 1, 1)); }
            [self text:slot[@"title"] ?: @"No programming" rect:CGRectInset(cell, 2, 2)
                font:[UIFont systemFontOfSize:12] color:chosen ? DTVSelectionText() : [UIColor whiteColor]];
            CGContextSetStrokeColorWithColor(context, DTVGridLine().CGColor);
            CGContextStrokeRect(context, cell);
            column += span;
        }
    }

    [DTVHint() setFill]; UIRectFill(CGRectMake(0, 297, 480, 23));
    NSString *left = self.status.length ? self.status : @"All Channels";
    [self text:left rect:CGRectMake(2, 297, 185, 23) font:[UIFont systemFontOfSize:12] color:[UIColor whiteColor]];
    [DTVColor(0xC22A18) setFill]; UIRectFill(CGRectMake(189, 305, 7, 7));
    [self text:@"-12h" rect:CGRectMake(197, 298, 53, 22) font:[UIFont systemFontOfSize:12] color:[UIColor whiteColor]];
    [DTVColor(0x2EA15C) setFill]; UIRectFill(CGRectMake(253, 305, 7, 7));
    [self text:@"+12h" rect:CGRectMake(261, 298, 53, 22) font:[UIFont systemFontOfSize:12] color:[UIColor whiteColor]];
    [DTVColor(0xF9C63C) setFill]; UIRectFill(CGRectMake(370, 305, 7, 7));
    [self text:@"Sync" rect:CGRectMake(379, 298, 96, 22) font:[UIFont boldSystemFontOfSize:12] color:[UIColor whiteColor]];
}

- (void)touchesEnded:(NSSet *)touches withEvent:(UIEvent *)event {
    (void)event; CGPoint point = [[touches anyObject] locationInView:self];
    if (self.fullScreen) { self.fullScreen = NO; return; }
    if (point.y >= 297) {
        if (point.x >= 360) [self.owner startSync];
        else if (point.x >= 245) { self.windowDelta += 12 * 3600; self.selectedDelta = self.windowDelta; [self setNeedsDisplay]; }
        else if (point.x >= 180) { self.windowDelta -= 12 * 3600; self.selectedDelta = self.windowDelta; [self setNeedsDisplay]; }
        return;
    }
    if (point.x >= 352 && point.y < 102) { [self.owner watchSelectedChannel]; return; }
    if (point.y >= 121) {
        NSArray *channels = self.manifest[@"channels"];
        NSInteger top = MAX(0, MIN(self.selectedChannel - 2, MAX(0, (NSInteger)channels.count - 6)));
        NSInteger row = (NSInteger)((point.y - 121) / 29.333);
        NSInteger index = top + row;
        if (index >= 0 && index < (NSInteger)channels.count) {
            NSInteger baseDelta = self.windowDelta - (DTVSecondsNow() % 1800);
            NSInteger column = MAX(0, MIN(2, (NSInteger)((point.x - 102) / 126)));
            NSInteger delta = baseDelta + column * 1800;
            NSDictionary *old = [self slotForChannelIndex:self.selectedChannel delta:self.selectedDelta];
            NSDictionary *next = [self slotForChannelIndex:index delta:delta];
            BOOL secondTap = index == self.selectedChannel && [self sameBlock:old other:next];
            self.selectedChannel = index; self.selectedDelta = delta; [self setNeedsDisplay];
            if (secondTap) [self.owner watchSelectedChannel];
        }
    }
}

- (void)moveChannels:(NSInteger)amount {
    NSInteger count = [self.manifest[@"channels"] count]; if (!count) return;
    self.selectedChannel = (self.selectedChannel + amount + count) % count;
    self.selectedDelta = 0; [self setNeedsDisplay];
    if (self.fullScreen) [self.owner watchSelectedChannel];
}
- (void)moveTime:(NSInteger)amount {
    if (self.fullScreen) { [self moveChannels:amount]; return; }
    self.windowDelta += amount * 1800; self.selectedDelta = self.windowDelta; [self setNeedsDisplay];
}
@end

@implementation DTVSettingsController {
    UITextField *_host; UITextField *_code; UILabel *_status;
}
- (void)viewDidLoad {
    [super viewDidLoad]; self.view.backgroundColor = DTVDescription();
    UILabel *title = [[UILabel alloc] initWithFrame:CGRectMake(20, 15, 440, 35)];
    title.text = @"RockPod Live TV"; title.textColor = [UIColor whiteColor];
    title.font = [UIFont boldSystemFontOfSize:24]; [self.view addSubview:title];
    _host = [[UITextField alloc] initWithFrame:CGRectMake(20, 66, 440, 38)];
    _host.borderStyle = UITextBorderStyleRoundedRect; _host.keyboardType = UIKeyboardTypeURL;
    _host.autocapitalizationType = UITextAutocapitalizationTypeNone; _host.autocorrectionType = UITextAutocorrectionTypeNo;
    _host.placeholder = @"http://rockpod-host:8732"; _host.text = [DTVClient sharedClient].baseURL; [self.view addSubview:_host];
    _code = [[UITextField alloc] initWithFrame:CGRectMake(20, 122, 210, 38)];
    _code.borderStyle = UITextBorderStyleRoundedRect; _code.keyboardType = UIKeyboardTypeNumberPad;
    _code.placeholder = @"Six-digit pairing code"; [self.view addSubview:_code];
    UIButton *pair = [UIButton buttonWithType:UIButtonTypeRoundedRect]; pair.frame = CGRectMake(250, 122, 100, 38);
    [pair setTitle:@"Pair" forState:UIControlStateNormal]; [pair addTarget:self action:@selector(pair) forControlEvents:UIControlEventTouchUpInside]; [self.view addSubview:pair];
    if (self.canCancel) { UIButton *cancel = [UIButton buttonWithType:UIButtonTypeRoundedRect]; cancel.frame = CGRectMake(360, 122, 100, 38); [cancel setTitle:@"Cancel" forState:UIControlStateNormal]; [cancel addTarget:self action:@selector(cancel) forControlEvents:UIControlEventTouchUpInside]; [self.view addSubview:cancel]; }
    _status = [[UILabel alloc] initWithFrame:CGRectMake(20, 180, 440, 80)]; _status.numberOfLines = 0;
    _status.textColor = [UIColor whiteColor]; _status.text = @"Start the RockPod companion, then enter its one-time code."; [self.view addSubview:_status];
}
- (void)pair {
    [self.view endEditing:YES]; [DTVClient sharedClient].baseURL = _host.text; _status.text = @"Pairing…";
    [[DTVClient sharedClient] pairWithCode:_code.text completion:^(id data, NSError *error) {
        (void)data; if (error) { _status.text = error.localizedDescription; return; }
        if (self.pairedHandler) self.pairedHandler();
        if (self.dismissHandler) self.dismissHandler();
        [self dismissViewControllerAnimated:YES completion:nil];
    }];
}
- (void)cancel {
    if (self.dismissHandler) self.dismissHandler();
    [self dismissViewControllerAnimated:YES completion:nil];
}
- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)orientation { return UIInterfaceOrientationIsLandscape(orientation); }
- (UIInterfaceOrientationMask)supportedInterfaceOrientations { return UIInterfaceOrientationMaskLandscape; }
@end

@implementation DTVGuideController

- (void)viewDidLoad {
    [super viewDidLoad]; self.view.backgroundColor = [UIColor blackColor];
    self.guideView = [[DTVGuideView alloc] initWithFrame:self.view.bounds];
    self.guideView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    self.guideView.owner = self; [self.view addSubview:self.guideView];
    for (NSNumber *direction in @[@(UISwipeGestureRecognizerDirectionUp), @(UISwipeGestureRecognizerDirectionDown), @(UISwipeGestureRecognizerDirectionLeft), @(UISwipeGestureRecognizerDirectionRight)]) {
        UISwipeGestureRecognizer *swipe = [[UISwipeGestureRecognizer alloc] initWithTarget:self action:@selector(swiped:)];
        swipe.direction = [direction unsignedIntegerValue]; [self.view addGestureRecognizer:swipe];
    }
    [self loadManifest];
}

- (void)viewDidAppear:(BOOL)animated {
    [super viewDidAppear:animated];
    if (![DTVClient sharedClient].paired && !self.settingsVisible) {
        [self showSettings];
    } else if (!self.guideView.manifest && !self.syncInProgress) {
        [self startSync];
    }
}
- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)orientation { return UIInterfaceOrientationIsLandscape(orientation); }
- (UIInterfaceOrientationMask)supportedInterfaceOrientations { return UIInterfaceOrientationMaskLandscape; }
- (UIInterfaceOrientation)preferredInterfaceOrientationForPresentation { return UIInterfaceOrientationLandscapeRight; }

- (NSString *)liveRoot {
    NSArray *documentPaths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    NSString *documents = documentPaths.count ? documentPaths[0] : NSTemporaryDirectory();
    NSString *root = [documents stringByAppendingPathComponent:@"LiveTV"];
    [[NSFileManager defaultManager] createDirectoryAtPath:[root stringByAppendingPathComponent:@"media"] withIntermediateDirectories:YES attributes:nil error:nil];
    return root;
}
- (NSString *)manifestPath { return [[self liveRoot] stringByAppendingPathComponent:@"manifest.json"]; }
- (NSString *)mediaPath:(NSString *)mediaID { return [[[self liveRoot] stringByAppendingPathComponent:@"media"] stringByAppendingPathComponent:[mediaID stringByAppendingString:@".mp4"]]; }

- (void)loadManifest {
    NSData *data = [NSData dataWithContentsOfFile:[self manifestPath]];
    NSDictionary *manifest = data ? [NSJSONSerialization JSONObjectWithData:data options:0 error:nil] : nil;
    if ([manifest isKindOfClass:[NSDictionary class]] &&
            [manifest[@"scope"] isEqualToString:DTVSyncScope]) {
        self.guideView.manifest = manifest; self.currentChannel = 0; [self tuneChannel:0];
    } else self.guideView.status = @"Syncing channels…";
}

- (void)showSettings {
    self.settingsVisible = YES;
    DTVSettingsController *settings = [[DTVSettingsController alloc] init];
    settings.modalPresentationStyle = UIModalPresentationFullScreen;
    settings.canCancel = self.guideView.manifest != nil;
    __weak DTVGuideController *weakSelf = self;
    settings.pairedHandler = ^{ weakSelf.settingsVisible = NO; [weakSelf startSync]; };
    settings.dismissHandler = ^{ weakSelf.settingsVisible = NO; };
    [self presentViewController:settings animated:YES completion:nil];
}

- (void)startSync {
    if (![DTVClient sharedClient].paired) { [self showSettings]; return; }
    if (self.syncInProgress) return;
    self.syncInProgress = YES;
    self.guideView.status = @"Starting sync…";
    [[DTVClient sharedClient] postPath:@"/v1/livetv/syncs" body:@{@"scope":DTVSyncScope}
        completion:^(id data, NSError *error) {
        if (error) {
            if ([self isTransientNetworkError:error]) { [self retryStartSync]; return; }
            [self fail:error]; return;
        }
        [self pollJob:data[@"id"]];
    }];
}

- (BOOL)isTransientNetworkError:(NSError *)error {
    if ([error.domain isEqualToString:NSURLErrorDomain]) return YES;
    return error.code == NSURLErrorTimedOut || error.code == NSURLErrorCannotConnectToHost ||
        error.code == NSURLErrorNetworkConnectionLost || error.code == NSURLErrorNotConnectedToInternet;
}

- (void)retryStartSync {
    self.syncInProgress = NO;
    self.guideView.status = @"Waiting for computer…";
    [self performSelector:@selector(startSync) withObject:nil afterDelay:3.0];
}

- (void)pollJob:(NSString *)jobID {
    if (!jobID.length) { [self failMessage:@"RockPod did not create a Live TV sync job."]; return; }
    [[DTVClient sharedClient] getPath:[@"/v1/livetv/syncs/" stringByAppendingString:jobID]
        completion:^(id data, NSError *error) {
        if (error) {
            if (error.code == 404) {
                self.guideView.status = @"Resuming on computer…";
                [self retryStartSync];
                return;
            }
            if ([self isTransientNetworkError:error]) {
                self.guideView.status = @"Waiting for computer…";
                [self performSelector:@selector(pollJob:) withObject:jobID afterDelay:3.0];
                return;
            }
            [self fail:error]; return;
        }
        NSString *state = data[@"state"];
        self.guideView.status = [NSString stringWithFormat:@"%@ %@/%@", data[@"label"] ?: @"Preparing", data[@"done"] ?: @0, data[@"total"] ?: @0];
        if ([state isEqual:@"ready"]) { self.pendingManifest = data[@"manifest"]; [self downloadMediaAtIndex:0]; return; }
        if ([state isEqual:@"failed"]) { [self failMessage:data[@"error"] ?: @"Live TV preparation failed."]; return; }
        [self performSelector:@selector(pollJob:) withObject:jobID afterDelay:2.0];
    }];
}

- (void)downloadMediaAtIndex:(NSUInteger)index {
    NSArray *media = self.pendingManifest[@"media"] ?: @[];
    if (index >= media.count) { [self publishPendingManifest]; return; }
    NSDictionary *item = media[index]; NSString *target = [self mediaPath:item[@"id"]];
    unsigned long long expected = [item[@"bytes"] unsignedLongLongValue];
    unsigned long long existing = [[[[NSFileManager defaultManager] attributesOfItemAtPath:target error:nil] objectForKey:NSFileSize] unsignedLongLongValue];
    if (existing == expected && expected) { [self downloadMediaAtIndex:index + 1]; return; }
    self.guideView.status = [NSString stringWithFormat:@"Sync %lu/%lu", (unsigned long)(index + 1), (unsigned long)media.count];
    [[DTVClient sharedClient] downloadPath:item[@"download_url"] toFile:target expectedBytes:expected
        completion:^(id data, NSError *error) {
        (void)data;
        if (error) {
            if ([self isTransientNetworkError:error]) {
                self.guideView.status = @"Transfer paused — resuming…";
                dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 3 * NSEC_PER_SEC),
                    dispatch_get_main_queue(), ^{ [self downloadMediaAtIndex:index]; });
                return;
            }
            [self fail:error]; return;
        }
        [self downloadMediaAtIndex:index + 1];
    }];
}

- (void)publishPendingManifest {
    NSData *JSON = [NSJSONSerialization dataWithJSONObject:self.pendingManifest options:0 error:nil];
    NSString *temporary = [[self manifestPath] stringByAppendingString:@".tmp"];
    if (!JSON || ![JSON writeToFile:temporary atomically:YES]) { [self failMessage:@"DIRECTV could not save the new guide."]; return; }
    [[NSFileManager defaultManager] removeItemAtPath:[self manifestPath] error:nil];
    NSError *moveError = nil;
    if (![[NSFileManager defaultManager] moveItemAtPath:temporary toPath:[self manifestPath] error:&moveError]) { [self fail:moveError]; return; }
    self.guideView.manifest = self.pendingManifest; self.pendingManifest = nil;
    self.syncInProgress = NO;
    self.guideView.status = @"Sync complete"; self.currentChannel = 0; self.guideView.selectedChannel = 0; [self tuneChannel:0];
}

- (void)fail:(NSError *)error { [self failMessage:error.localizedDescription]; }
- (void)failMessage:(NSString *)message {
    self.syncInProgress = NO;
    self.guideView.status = @"Sync failed";
    [[[UIAlertView alloc] initWithTitle:@"RockPod Live TV" message:message delegate:nil cancelButtonTitle:@"OK" otherButtonTitles:nil] show];
}

- (void)swiped:(UISwipeGestureRecognizer *)gesture {
    if (gesture.direction == UISwipeGestureRecognizerDirectionUp) [self.guideView moveChannels:1];
    else if (gesture.direction == UISwipeGestureRecognizerDirectionDown) [self.guideView moveChannels:-1];
    else if (gesture.direction == UISwipeGestureRecognizerDirectionLeft) [self.guideView moveTime:1];
    else if (gesture.direction == UISwipeGestureRecognizerDirectionRight) [self.guideView moveTime:-1];
}

- (void)watchSelectedChannel {
    self.currentChannel = self.guideView.selectedChannel;
    if ([self tuneChannel:self.currentChannel]) self.guideView.fullScreen = YES;
}

- (BOOL)tuneChannel:(NSInteger)channelIndex {
    NSDictionary *slot = [self.guideView slotForChannelIndex:channelIndex delta:0];
    NSString *mediaID = slot[@"media_id"];
    NSString *path = mediaID.length ? [self mediaPath:mediaID] : nil;
    if (!path.length || ![[NSFileManager defaultManager] fileExistsAtPath:path]) return NO;
    [[NSNotificationCenter defaultCenter] removeObserver:self name:AVPlayerItemDidPlayToEndTimeNotification object:nil];
    self.player = [AVPlayer playerWithURL:[NSURL fileURLWithPath:path]];
    AVPlayerLayer *layer = [AVPlayerLayer playerLayerWithPlayer:self.player];
    layer.videoGravity = AVLayerVideoGravityResizeAspect; self.guideView.playerLayer = layer;
    NSInteger day, seconds; DTVResolveDelta(0, &day, &seconds); (void)day;
    NSInteger offset = MAX(0, seconds - [slot[@"start"] integerValue]);
    if (offset + 3 >= [slot[@"duration"] integerValue]) offset = 0;
    [self.player seekToTime:CMTimeMake(offset, 1) toleranceBefore:kCMTimeZero toleranceAfter:kCMTimeZero];
    [self.player play]; self.currentMediaID = mediaID; self.currentChannel = channelIndex;
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(channelEnded:)
        name:AVPlayerItemDidPlayToEndTimeNotification object:self.player.currentItem];
    return YES;
}

- (void)channelEnded:(NSNotification *)note { (void)note; [self tuneChannel:self.currentChannel]; }
- (void)catchUpToLiveClock {
    if (self.guideView.manifest && self.player) [self tuneChannel:self.currentChannel];
    [self.guideView setNeedsDisplay];
}
- (void)dealloc { [[NSNotificationCenter defaultCenter] removeObserver:self]; }
@end
