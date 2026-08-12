#import "RPSimulatorHost.h"
#import <UIKit/UIKit.h>
#include <SDL.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdatomic.h>

extern int rockbox_sim_main(int argc, char *argv[]);
extern void sdl_sys_quit(void);

/* SDL's iOS objects are built with Clang's availability runtime checks.  The
 * Linux-hosted Apple toolchain does not ship compiler-rt's tiny helper, so the
 * companion supplies its equivalent from NSProcessInfo. */
int __isOSVersionAtLeast(int major, int minor, int patch)
{
    NSOperatingSystemVersion requested = { major, minor, patch };
    return [NSProcessInfo.processInfo isOperatingSystemAtLeastVersion:requested];
}

/* force_load also brings the voice Speex allocator adapter into the complete
 * core archive. It is not used by the app core, but must remain linkable. */
void *codec_malloc(size_t size) { return malloc(size); }
void *codec_realloc(void *pointer, size_t size) { return realloc(pointer, size); }
void codec_free(void *pointer) { free(pointer); }

/* Panel geometry, mirrored from sim-ui-defines.h (UI_WIDTH/UI_HEIGHT) and
 * uisimulator/buttonmap/ipod.c.  The published frame is the whole iPod body,
 * not just the 320x240 LCD, so the view must carry the body's aspect or the
 * artwork is letterboxed into a sliver of black. */
#define RP_PANEL_WIDTH 350.0
#define RP_PANEL_HEIGHT 591.0
#define RP_WHEEL_CX 175.0
#define RP_WHEEL_CY 432.0
#define RP_WHEEL_INNER 45.0
#define RP_WHEEL_OUTER 118.0
/* Wheel travel that counts as one detent, and the rotation past which a touch
 * is a scrub rather than a tap. */
#define RP_WHEEL_DETENT_RADIANS 0.20
#define RP_WHEEL_TAP_LIMIT_RADIANS 0.12

@interface RPSimulatorHost ()
@property(atomic, readwrite, getter=isRunning) BOOL running;
@property(atomic) BOOL preparing;
@property(atomic) BOOL stopping;
@property(nonatomic, strong) UIView *simulatorView;
@property(nonatomic, strong) UIImageView *simulatorImage;
@property(nonatomic, strong) UILabel *simulatorStatus;
@property(nonatomic, strong) UIImage *pendingFrameImage;
@property(nonatomic, strong) NSURL *previewURL;
@property(atomic) BOOL receivedFrame;
/* A black panel has several possible causes that look identical on screen:
 * the core never started, it started but never published a frame, or frames
 * arrive and the view is misplaced.  Record enough to tell them apart without
 * a device log. */
@property(atomic) NSUInteger framesPublished;
@property(atomic) int lastFrameWidth;
@property(atomic) int lastFrameHeight;
@property(atomic) BOOL coreExited;
@property(atomic) int lastFrameInkPercent;
@property(atomic) int panelInkPercent;
@property(atomic) NSUInteger publishCalls;
@property(atomic) NSUInteger convertFails;
@property(nonatomic, strong) NSTimer *filePreviewTimer;
@property(nonatomic, strong) NSURL *coreLogURL;
- (void)dismissSimulator;
@end

/* Touch surface laid over the rendered iPod.  It converts touches into the
 * same SDL events the desktop simulator already understands: a tap becomes a
 * left click, which xy2button() maps to Menu/Select/Play/prev/next using the
 * button table, and a circular drag becomes mouse-wheel events, which
 * scrollwheel_event() turns into BUTTON_SCROLL_FWD/BACK.  Keeping the region
 * table as the single source of truth means the artwork and the hit testing
 * cannot drift apart here. */
@interface RPClickWheelView : UIView
@property(nonatomic) CGPoint startPanelPoint;
@property(nonatomic) CGFloat lastAngle;
@property(nonatomic) CGFloat travel;
@property(nonatomic) CGFloat detentCarry;
@property(nonatomic) BOOL onWheel;
@end

@implementation RPClickWheelView

/* The image is drawn aspect-fit, but the view carries the panel aspect, so
 * the mapping is a straight scale. */
- (CGPoint)panelPointForTouch:(UITouch *)touch
{
    CGPoint p = [touch locationInView:self];
    CGFloat w = self.bounds.size.width, h = self.bounds.size.height;
    if (w <= 0 || h <= 0)
        return CGPointZero;
    return CGPointMake(p.x / w * RP_PANEL_WIDTH, p.y / h * RP_PANEL_HEIGHT);
}

- (void)pushMouse:(Uint32)type at:(CGPoint)panel
{
    SDL_Event event;
    SDL_zero(event);
    event.type = type;
    event.button.button = SDL_BUTTON_LEFT;
    event.button.state = type == SDL_MOUSEBUTTONDOWN ? SDL_PRESSED : SDL_RELEASED;
    event.button.clicks = 1;
    event.button.x = (Sint32)lround(panel.x);
    event.button.y = (Sint32)lround(panel.y);
    SDL_PushEvent(&event);
}

- (void)pushWheel:(int)direction
{
    SDL_Event event;
    SDL_zero(event);
    event.type = SDL_MOUSEWHEEL;
    event.wheel.x = 0;
    event.wheel.y = direction;
    SDL_PushEvent(&event);
}

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    (void)event;
    CGPoint panel = [self panelPointForTouch:touches.anyObject];
    CGFloat dx = panel.x - RP_WHEEL_CX, dy = panel.y - RP_WHEEL_CY;
    CGFloat radius = hypot(dx, dy);

    self.startPanelPoint = panel;
    self.travel = 0;
    self.detentCarry = 0;
    self.lastAngle = atan2(dy, dx);
    self.onWheel = radius > RP_WHEEL_INNER && radius <= RP_WHEEL_OUTER;
}

- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    (void)event;
    if (!self.onWheel)
        return;
    CGPoint panel = [self panelPointForTouch:touches.anyObject];
    CGFloat angle = atan2(panel.y - RP_WHEEL_CY, panel.x - RP_WHEEL_CX);
    CGFloat delta = angle - self.lastAngle;

    /* Keep the shortest way round so crossing the -pi/+pi seam does not read
     * as a full turn in the wrong direction. */
    while (delta > M_PI) delta -= 2 * M_PI;
    while (delta < -M_PI) delta += 2 * M_PI;
    self.lastAngle = angle;
    self.travel += fabs(delta);
    self.detentCarry += delta;

    while (fabs(self.detentCarry) >= RP_WHEEL_DETENT_RADIANS)
    {
        BOOL clockwise = self.detentCarry > 0;
        self.detentCarry -= clockwise ? RP_WHEEL_DETENT_RADIANS :
                                        -RP_WHEEL_DETENT_RADIANS;
        /* scrollwheel_event(): y < 0 is SCROLL_FWD, which is clockwise on a
         * real click wheel. */
        [self pushWheel:clockwise ? -1 : 1];
    }
}

- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    (void)touches;
    (void)event;
    if (self.onWheel && self.travel >= RP_WHEEL_TAP_LIMIT_RADIANS)
        return;                     /* a scrub, already delivered as detents */

    CGPoint panel = self.startPanelPoint;
    [self pushMouse:SDL_MOUSEBUTTONDOWN at:panel];
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 80 * NSEC_PER_MSEC),
                   dispatch_get_main_queue(), ^{
        [self pushMouse:SDL_MOUSEBUTTONUP at:panel];
    });
}

- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    (void)touches;
    (void)event;
    [self pushMouse:SDL_MOUSEBUTTONUP at:self.startPanelPoint];
}

@end

/* Read by the embedded core's frame capture; see window-sdl.c.  Raised only
 * after the in-memory path has demonstrably produced nothing. */
volatile int rockpod_ios_file_preview_wanted = 0;

/* Shared by the in-memory publish path and the on-disk fallback so both
 * produce a frame the same way, without going through an image decoder. */
static UIImage *rp_image_from_argb(NSData *frame, int width, int height,
                                   int pitch)
{
    if (!frame || width <= 0 || height <= 0 || pitch < width * 4)
        return nil;
    if (frame.length < (NSUInteger)pitch * (NSUInteger)height)
        return nil;
    CGDataProviderRef provider = CGDataProviderCreateWithCFData(
        (__bridge CFDataRef)frame);
    CGColorSpaceRef color = CGColorSpaceCreateDeviceRGB();
    CGImageRef image = CGImageCreate(width, height, 8, 32, pitch, color,
        kCGBitmapByteOrder32Little | kCGImageAlphaNoneSkipFirst, provider,
        NULL, false, kCGRenderingIntentDefault);
    UIImage *uiImage = image ? [UIImage imageWithCGImage:image] : nil;
    if (image) CGImageRelease(image);
    CGColorSpaceRelease(color);
    CGDataProviderRelease(provider);
    return uiImage;
}

void rockpod_ios_publish_frame(const void *pixels, int width, int height,
                               int pitch)
{
    RPSimulatorHost.sharedHost.publishCalls++;
    if (!pixels || width <= 0 || height <= 0 || pitch < width * 4)
        return;

    /* This runs on Rockbox's render thread, a plain pthread with no
     * autorelease pool.  Every frame allocates an ~800 KB autoreleased NSData
     * that nothing ever drains, so memory climbs until allocation fails and
     * conversion starts returning nil -- "produced frames, none could be
     * converted".  Give the thread its own pool per frame. */
    @autoreleasepool
    {
    /* Drop frames instead of queueing them.  The render thread is faster than
     * the main queue can consume, and an unbounded backlog of frame-sized
     * images is the other way this runs the app out of memory. */
    /* Coalescing disabled while diagnosing: a stuck flag would silently
     * suppress every frame and look identical to a conversion failure. */

    NSData *frame = [NSData dataWithBytes:pixels length:(NSUInteger)pitch * height];
    UIImage *uiImage = rp_image_from_argb(frame, width, height, pitch);
    /* Sample the LCD window of the panel so the counter can report whether
     * the frame has any content, not merely that one arrived. */
    {
        const unsigned char *px = pixels;
        long lit = 0, seen = 0;
        for (int y = 12; y < 252 && y < height; y += 4)
            for (int x = 14; x < 334 && x < width; x += 4)
            {
                const unsigned char *p = px + (long)y * pitch + (long)x * 4;
                seen++;
                if (p[0] | p[1] | p[2]) lit++;
            }
        RPSimulatorHost.sharedHost.lastFrameInkPercent =
            seen ? (int)(lit * 100 / seen) : -1;
        /* Also sample the whole panel.  If this is 0 too, the iPod body is
         * not rendering either and the problem is the whole framebuffer, not
         * the LCD window. */
        long plit = 0, pseen = 0;
        for (int y = 0; y < height; y += 8)
            for (int x = 0; x < width; x += 8)
            {
                const unsigned char *p = px + (long)y * pitch + (long)x * 4;
                pseen++;
                if (p[0] | p[1] | p[2]) plit++;
            }
        RPSimulatorHost.sharedHost.panelInkPercent =
            pseen ? (int)(plit * 100 / pseen) : -1;
    }
    RPSimulatorHost.sharedHost.lastFrameWidth = width;
    RPSimulatorHost.sharedHost.lastFrameHeight = height;
    if (!uiImage)
    {
        RPSimulatorHost.sharedHost.convertFails++;
        return;
    }
    RPSimulatorHost.sharedHost.framesPublished++;
    dispatch_async(dispatch_get_main_queue(), ^{
        RPSimulatorHost *host = RPSimulatorHost.sharedHost;
        if (!host.isRunning)
            return;
        if (host.simulatorImage)
        {
            host.receivedFrame = YES;
            host.simulatorImage.image = uiImage;
            /* Keep a live counter visible for the first half minute.  "Black
         * panel" has two very different causes -- no frames at all, or frames
         * arriving that are themselves black -- and only this tells them
         * apart on a device with no console. */
        if (host.framesPublished > 3 && host.framesPublished < 300)
        {
            host.simulatorStatus.hidden = NO;
            host.simulatorStatus.numberOfLines = 0;
            host.simulatorStatus.text = [NSString stringWithFormat:
                @"calls %lu ok %lu %dx%d lcd %d%% panel %d%%",
                (unsigned long)host.publishCalls,
                (unsigned long)host.framesPublished,
                host.lastFrameWidth, host.lastFrameHeight,
                host.lastFrameInkPercent, host.panelInkPercent];
        }
        else
            host.simulatorStatus.hidden = YES;
        }
        else
        {
            host.receivedFrame = YES;
            host.pendingFrameImage = uiImage;
        }
    });
    }
}

void rockpod_ios_simulator_failed(const char *message)
{
    NSString *detail = message ? [NSString stringWithUTF8String:message] :
                                 @"Rockbox simulator startup failed.";
    dispatch_async(dispatch_get_main_queue(), ^{
        RPSimulatorHost *host = RPSimulatorHost.sharedHost;
        host.running = NO;
        host.preparing = NO;
        host.stopping = NO;
        host.simulatorStatus.hidden = NO;
        host.simulatorStatus.text = detail;
        host.simulatorStatus.textColor = [UIColor colorWithRed:0.95 green:0.35
                                                          blue:0.30 alpha:1.0];
    });
}

void rockpod_ios_simulator_did_exit(void)
{
    dispatch_async(dispatch_get_main_queue(), ^{
        RPSimulatorHost *host = RPSimulatorHost.sharedHost;
        host.coreExited = YES;
        host.running = NO;
        host.preparing = NO;
        host.stopping = NO;
        [host dismissSimulator];
    });
}

@implementation RPSimulatorHost

+ (instancetype)sharedHost
{
    static RPSimulatorHost *host;
    static dispatch_once_t once;
    dispatch_once(&once, ^{ host = [[RPSimulatorHost alloc] init]; });
    return host;
}

- (NSURL *)simulatorRoot
{
    NSURL *documents = [NSFileManager.defaultManager URLsForDirectory:
        NSDocumentDirectory inDomains:NSUserDomainMask].firstObject;
    return [documents URLByAppendingPathComponent:@"RockPodSimulator"
                                       isDirectory:YES];
}

- (BOOL)prepareInstall:(NSString **)message
{
    NSFileManager *manager = NSFileManager.defaultManager;
    NSURL *root = self.simulatorRoot;
    NSURL *source = [NSBundle.mainBundle URLForResource:@"SimulatorInstall"
                                          withExtension:nil];
    NSURL *sourceRockbox = [source URLByAppendingPathComponent:@".rockbox"];
    NSURL *rockbox = [root URLByAppendingPathComponent:@".rockbox"];
    NSURL *sourceMarker = [sourceRockbox URLByAppendingPathComponent:
        @"rockpod-companion-install.id"];
    NSURL *marker = [rockbox URLByAppendingPathComponent:
        @"rockpod-companion-install.id"];
    NSError *error = nil;
    NSString *sourceID = [NSString stringWithContentsOfURL:sourceMarker
        encoding:NSUTF8StringEncoding error:&error];
    NSString *installedID = [NSString stringWithContentsOfURL:marker
        encoding:NSUTF8StringEncoding error:nil];

    if (!source || !sourceID.length)
    {
        if (message) *message = @"The versioned Rockbox install is missing from this build.";
        return NO;
    }
    if (![sourceID isEqualToString:installedID])
    {
        [manager createDirectoryAtURL:root withIntermediateDirectories:YES
                            attributes:nil error:&error];
        NSURL *staging = [root URLByAppendingPathComponent:@".rockbox.new"];
        NSURL *backup = [root URLByAppendingPathComponent:@".rockbox.old"];
        [manager removeItemAtURL:staging error:nil];
        [manager removeItemAtURL:backup error:nil];
        if (![manager copyItemAtURL:sourceRockbox toURL:staging error:&error])
        {
            if (message) *message = error.localizedDescription ?: @"Rockbox refresh copy failed";
            return NO;
        }
        BOOL hadInstall = [manager fileExistsAtPath:rockbox.path];
        if (hadInstall && ![manager moveItemAtURL:rockbox toURL:backup error:&error])
        {
            [manager removeItemAtURL:staging error:nil];
            if (message) *message = error.localizedDescription ?: @"Could not replace old Rockbox install";
            return NO;
        }
        if (![manager moveItemAtURL:staging toURL:rockbox error:&error])
        {
            if (hadInstall)
                [manager moveItemAtURL:backup toURL:rockbox error:nil];
            if (message) *message = error.localizedDescription ?: @"Could not activate refreshed Rockbox install";
            return NO;
        }
        [manager removeItemAtURL:backup error:nil];
    }
    for (NSString *folder in @[@"Music", @"Photos", @"Videos", @"Playlists"])
        [manager createDirectoryAtURL:[root URLByAppendingPathComponent:folder]
          withIntermediateDirectories:YES attributes:nil error:nil];

    NSURL *manifest = [root URLByAppendingPathComponent:
        @".rockbox/rockpod-dynamic-code.tsv"];
    NSString *links = [NSString stringWithContentsOfURL:manifest
        encoding:NSUTF8StringEncoding error:&error];
    if (!links)
    {
        if (message) *message = @"The signed Rockbox plugin manifest is missing.";
        return NO;
    }
    for (NSString *line in [links componentsSeparatedByCharactersInSet:
             NSCharacterSet.newlineCharacterSet])
    {
        if (!line.length)
            continue;
        NSArray<NSString *> *fields = [line componentsSeparatedByString:@"\t"];
        if (fields.count != 2)
            continue;
        NSURL *link = [root URLByAppendingPathComponent:fields[0]];
        NSURL *signedCode = [NSBundle.mainBundle.bundleURL
            URLByAppendingPathComponent:fields[1]];
        if (![manager fileExistsAtPath:signedCode.path])
        {
            if (message) *message = [NSString stringWithFormat:
                @"Signed Rockbox component is missing: %@", fields[1]];
            return NO;
        }
        [manager createDirectoryAtURL:[link URLByDeletingLastPathComponent]
          withIntermediateDirectories:YES attributes:nil error:nil];
        [manager removeItemAtURL:link error:nil];
        if (![manager createSymbolicLinkAtURL:link
                           withDestinationURL:signedCode error:&error])
        {
            if (message) *message = error.localizedDescription ?:
                @"Could not link a signed Rockbox component.";
            return NO;
        }
    }
    return YES;
}

- (UIButton *)simulatorButton:(NSString *)title key:(SDL_Keycode)key
{
    UIButton *button = [UIButton buttonWithType:UIButtonTypeSystem];
    [button setTitle:title forState:UIControlStateNormal];
    [button setTitleColor:UIColor.whiteColor forState:UIControlStateNormal];
    button.titleLabel.font = [UIFont boldSystemFontOfSize:14];
    button.backgroundColor = [UIColor colorWithWhite:0.16 alpha:1.0];
    button.layer.cornerRadius = 8;
    button.tag = (NSInteger)key;
    [button.heightAnchor constraintEqualToConstant:42].active = YES;
    [button addTarget:self action:@selector(simulatorKeyPressed:)
      forControlEvents:UIControlEventTouchUpInside];
    return button;
}

- (void)simulatorKeyPressed:(UIButton *)sender
{
    SDL_Event event;
    SDL_zero(event);
    event.type = SDL_KEYDOWN;
    event.key.keysym.sym = (SDL_Keycode)sender.tag;
    SDL_PushEvent(&event);
    SDL_Keycode key = (SDL_Keycode)sender.tag;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 90 * NSEC_PER_MSEC),
                   dispatch_get_main_queue(), ^{
        SDL_Event release;
        SDL_zero(release);
        release.type = SDL_KEYUP;
        release.key.keysym.sym = key;
        SDL_PushEvent(&release);
    });
}

- (void)presentSimulator
{
    UIWindow *window = nil;
    for (UIWindow *candidate in UIApplication.sharedApplication.windows)
        if (candidate.isKeyWindow) { window = candidate; break; }
    if (!window)
        window = UIApplication.sharedApplication.windows.lastObject;
    if (!window || self.simulatorView)
        return;

    UIView *surface = [[UIView alloc] init];
    surface.translatesAutoresizingMaskIntoConstraints = NO;
    surface.backgroundColor = [UIColor colorWithWhite:0.035 alpha:1.0];
    [window addSubview:surface];
    [NSLayoutConstraint activateConstraints:@[
        [surface.leadingAnchor constraintEqualToAnchor:window.leadingAnchor],
        [surface.trailingAnchor constraintEqualToAnchor:window.trailingAnchor],
        [surface.topAnchor constraintEqualToAnchor:window.topAnchor],
        [surface.bottomAnchor constraintEqualToAnchor:window.bottomAnchor],
    ]];

    UIButton *done = [UIButton buttonWithType:UIButtonTypeSystem];
    done.translatesAutoresizingMaskIntoConstraints = NO;
    [done setTitle:@"Done" forState:UIControlStateNormal];
    [done setTitleColor:UIColor.whiteColor forState:UIControlStateNormal];
    done.titleLabel.font = [UIFont boldSystemFontOfSize:16];
    [done addTarget:self action:@selector(stop) forControlEvents:UIControlEventTouchUpInside];
    [surface addSubview:done];

    UILabel *title = [[UILabel alloc] init];
    title.translatesAutoresizingMaskIntoConstraints = NO;
    title.text = @"iPod Rockbox Simulator";
    title.textColor = UIColor.whiteColor;
    title.font = [UIFont boldSystemFontOfSize:17];
    title.textAlignment = NSTextAlignmentCenter;
    [surface addSubview:title];

    UIImageView *screen = [[UIImageView alloc] init];
    screen.translatesAutoresizingMaskIntoConstraints = NO;
    screen.backgroundColor = UIColor.blackColor;
    screen.contentMode = UIViewContentModeScaleAspectFit;
    /* Show the iPod body straight away, using the same artwork the core
     * composites its LCD onto.  Until the first frame arrives the panel would
     * otherwise be an empty black rectangle, which is indistinguishable from a
     * failure; live frames replace this within a few hundred milliseconds. */
    NSString *panelPath = [NSBundle.mainBundle pathForResource:@"UI256"
                                                        ofType:@"bmp"];
    if (panelPath)
        screen.image = [UIImage imageWithContentsOfFile:panelPath];
    screen.layer.borderWidth = 1;
    screen.layer.borderColor = [UIColor colorWithWhite:0.28 alpha:1].CGColor;
    [surface addSubview:screen];
    self.simulatorImage = screen;
    if (self.pendingFrameImage)
    {
        self.simulatorImage.image = self.pendingFrameImage;
        self.pendingFrameImage = nil;
        self.receivedFrame = YES;
    }

    UILabel *status = [[UILabel alloc] init];
    status.translatesAutoresizingMaskIntoConstraints = NO;
    status.text = @"Starting the real Rockbox framebuffer…";
    status.textColor = [UIColor colorWithWhite:0.72 alpha:1];
    status.font = [UIFont systemFontOfSize:13];
    status.textAlignment = NSTextAlignmentCenter;
    [surface addSubview:status];
    self.simulatorStatus = status;
    if (self.receivedFrame)
        self.simulatorStatus.hidden = YES;

    RPClickWheelView *wheel = [[RPClickWheelView alloc] init];
    wheel.translatesAutoresizingMaskIntoConstraints = NO;
    wheel.backgroundColor = UIColor.clearColor;
    [surface addSubview:wheel];

    UILayoutGuide *safe = surface.safeAreaLayoutGuide;
    /* The panel keeps its true 350x591 aspect and is fitted to whichever of
     * width or height runs out first, so the whole iPod is on screen and the
     * touch surface lines up with the artwork exactly. */
    NSLayoutConstraint *fitWidth =
        [screen.widthAnchor constraintEqualToAnchor:safe.widthAnchor
                                         constant:-32];
    fitWidth.priority = UILayoutPriorityDefaultHigh;
    [NSLayoutConstraint activateConstraints:@[
        [done.leadingAnchor constraintEqualToAnchor:safe.leadingAnchor constant:16],
        [done.topAnchor constraintEqualToAnchor:safe.topAnchor constant:4],
        [done.widthAnchor constraintEqualToConstant:52],
        [done.heightAnchor constraintEqualToConstant:38],
        [title.centerXAnchor constraintEqualToAnchor:safe.centerXAnchor],
        [title.centerYAnchor constraintEqualToAnchor:done.centerYAnchor],
        [screen.centerXAnchor constraintEqualToAnchor:safe.centerXAnchor],
        [screen.topAnchor constraintEqualToAnchor:done.bottomAnchor constant:8],
        [screen.heightAnchor constraintEqualToAnchor:screen.widthAnchor
            multiplier:RP_PANEL_HEIGHT / RP_PANEL_WIDTH],
        fitWidth,
        [screen.widthAnchor constraintLessThanOrEqualToAnchor:safe.widthAnchor
            constant:-32],
        [screen.bottomAnchor constraintLessThanOrEqualToAnchor:safe.bottomAnchor
            constant:-12],
        [status.leadingAnchor constraintEqualToAnchor:screen.leadingAnchor],
        [status.trailingAnchor constraintEqualToAnchor:screen.trailingAnchor],
        [status.centerYAnchor constraintEqualToAnchor:screen.centerYAnchor],
        [wheel.leadingAnchor constraintEqualToAnchor:screen.leadingAnchor],
        [wheel.trailingAnchor constraintEqualToAnchor:screen.trailingAnchor],
        [wheel.topAnchor constraintEqualToAnchor:screen.topAnchor],
        [wheel.bottomAnchor constraintEqualToAnchor:screen.bottomAnchor],
    ]];
    self.simulatorView = surface;
}

- (void)startFilePreviewFallback
{
    if (self.filePreviewTimer || !self.previewURL)
        return;
    __weak typeof(self) weakSelf = self;
    self.filePreviewTimer = [NSTimer scheduledTimerWithTimeInterval:0.4
        repeats:YES block:^(NSTimer *timer) {
        typeof(self) host = weakSelf;
        if (!host || !host.isRunning)
        {
            [timer invalidate];
            return;
        }
        /* width, height, pitch as three little-endian words, then raw
         * ARGB8888 -- see the writer in window-sdl.c. */
        NSData *raw = [NSData dataWithContentsOfURL:host.previewURL];
        if (raw.length < 12)
            return;
        uint32_t head[3];
        [raw getBytes:head length:sizeof(head)];
        int width = (int)CFSwapInt32LittleToHost(head[0]);
        int height = (int)CFSwapInt32LittleToHost(head[1]);
        int pitch = (int)CFSwapInt32LittleToHost(head[2]);
        if (width <= 0 || height <= 0 || pitch < width * 4 ||
            raw.length < 12 + (NSUInteger)pitch * (NSUInteger)height)
            return;
        NSData *pixels = [raw subdataWithRange:NSMakeRange(12,
            (NSUInteger)pitch * (NSUInteger)height)];
        UIImage *image = rp_image_from_argb(pixels, width, height, pitch);
        if (!image)
            return;
        host.simulatorImage.image = image;
        host.simulatorStatus.hidden = YES;
    }];
}

- (void)stopFilePreviewFallback
{
    [self.filePreviewTimer invalidate];
    self.filePreviewTimer = nil;
    rockpod_ios_file_preview_wanted = 0;
}

- (void)dismissSimulator
{
    [self stopFilePreviewFallback];
    [self.simulatorView removeFromSuperview];
    self.simulatorView = nil;
    self.simulatorImage = nil;
    self.simulatorStatus = nil;
    self.pendingFrameImage = nil;
}

- (void)startWithCompletion:(void (^)(BOOL, NSString *))completion
{
    if (self.running)
    {
        if (completion) completion(YES, @"Rockbox simulator is already running.");
        return;
    }
    if (self.preparing)
    {
        if (completion)
            completion(YES, @"The complete Rockbox install is still being prepared…");
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 250 * NSEC_PER_MSEC),
                       dispatch_get_main_queue(), ^{
            [self startWithCompletion:completion];
        });
        return;
    }
    self.preparing = YES;
    if (completion) completion(YES, @"Preparing the complete Rockbox install…");
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        @autoreleasepool
        {
            NSString *message = nil;
            if (![self prepareInstall:&message])
            {
                self.preparing = NO;
                dispatch_async(dispatch_get_main_queue(), ^{
                    if (completion) completion(NO, message ?: @"Could not prepare simulator.");
                });
                return;
            }
            self.preparing = NO;
            self.running = YES;
            self.stopping = NO;
            self.receivedFrame = NO;
            self.framesPublished = 0;
            self.lastFrameWidth = 0;
            self.lastFrameHeight = 0;
            self.coreExited = NO;
            self.pendingFrameImage = nil;
            NSString *root = self.simulatorRoot.path;
            NSString *bundle = NSBundle.mainBundle.resourcePath;
            self.previewURL = [self.simulatorRoot URLByAppendingPathComponent:@"rockpod-live-preview.bmp"];
            [NSFileManager.defaultManager removeItemAtURL:self.previewURL error:nil];
            rockpod_ios_file_preview_wanted = 0;
            dispatch_async(dispatch_get_main_queue(), ^{
                if (completion) completion(YES, @"Starting the complete Rockbox install…");
                [self presentSimulator];
                /* If the in-memory frame path has produced nothing after a
                 * few seconds, fall back to the core's bitmap file so the
                 * panel still shows live output instead of staying blank. */
                dispatch_after(dispatch_time(DISPATCH_TIME_NOW,
                    3 * NSEC_PER_SEC), dispatch_get_main_queue(), ^{
                    /* One frame is the boot background from
                     * sdl_window_setup(); it does not mean Rockbox started.
                     * Only sustained output counts as working. */
                    if (!self.running || self.framesPublished > 3)
                        return;
                    rockpod_ios_file_preview_wanted = 1;
                    [self startFilePreviewFallback];
                });
                dispatch_after(dispatch_time(DISPATCH_TIME_NOW,
                    8 * NSEC_PER_SEC), dispatch_get_main_queue(), ^{
                    /* One frame is the boot background from
                     * sdl_window_setup(); it does not mean Rockbox started.
                     * Only sustained output counts as working. */
                    if (!self.running || self.framesPublished > 3)
                        return;
                    /* Say which stage stalled rather than "still starting",
                     * which is the same message whatever went wrong. */
                    self.simulatorStatus.hidden = NO;
                    if (self.coreExited)
                        self.simulatorStatus.text =
                            @"Rockbox exited during start-up. The staged "
                             "install under Documents/RockPodSimulator is "
                             "probably incomplete.";
                    else
                    {
                        /* Show what the core actually reported rather than a
                         * generic message; this is the only place its stdout
                         * is visible on a device. */
                        NSString *log = [NSString stringWithContentsOfURL:
                            self.coreLogURL encoding:NSUTF8StringEncoding
                            error:nil];
                        NSArray<NSString *> *lines = [log
                            componentsSeparatedByCharactersInSet:
                                NSCharacterSet.newlineCharacterSet];
                        NSMutableArray *tail = [NSMutableArray array];
                        for (NSString *l in lines.reverseObjectEnumerator)
                        {
                            if (!l.length) continue;
                            [tail insertObject:l atIndex:0];
                            if (tail.count >= 6) break;
                        }
                        self.simulatorStatus.numberOfLines = 0;
                        self.simulatorStatus.text = tail.count ?
                            [tail componentsJoinedByString:@"\n"] :
                            @"Rockbox is running but has not drawn a frame, "
                             "and reported nothing.";
                    }
                });
            });
            /* Must be the offscreen driver.  Letting SDL pick its default on
             * iOS selects UIKit, whose SDL_CreateWindow() builds a real
             * UIWindow; UIKit asserts that happens on the main thread and
             * aborts, because the core runs on a background dispatch queue.
             * The device crash log showed exactly that:
             *   -[UIWindow _initWithFrame:debugName:windowScene:]
             *   -> -[UIWindowScene _windowUpdatedVisibility:] -> abort
             * The companion never wants an SDL window anyway -- it renders
             * captured frames into its own UIImageView. */
            setenv("SDL_VIDEODRIVER", "offscreen", 1);
            SDL_SetHint(SDL_HINT_VIDEODRIVER, "offscreen");
            setenv("SDL_RENDER_DRIVER", "software", 1);
            setenv("ROCKPOD_SIM_HIDDEN", "1", 1);
            setenv("ROCKPOD_SIM_PREVIEW_INTERVAL_MS", "100", 1);
            setenv("ROCKPOD_SIM_PREVIEW_BMP",
                   self.previewURL.path.fileSystemRepresentation, 1);
            /* SDL_VideoInit() disables the screensaver unless this hint is
             * set (SDL_video.c: "if (!SDL_GetHintBoolean(
             * SDL_HINT_VIDEO_ALLOW_SCREENSAVER, SDL_FALSE))").  On iOS that
             * reaches -[UIApplication setIdleTimerDisabled:], which asserts it
             * is on the main run loop queue.  The core runs on a background
             * dispatch queue, so BoardServices raised EXC_BREAKPOINT inside
             * SDL_VideoInit and the simulator died before drawing a frame.
             * Leaving the screensaver alone keeps UIKit out of video init;
             * the app already manages idle timing for its own UI. */
            setenv("SDL_VIDEO_ALLOW_SCREENSAVER", "1", 1);
            SDL_SetHint(SDL_HINT_VIDEO_ALLOW_SCREENSAVER, "1");

            /* RockPod Link owns UIApplicationMain. Tell SDL its normal iOS
             * main shim was intentionally bypassed before any subsystem is
             * initialized by the embedded Rockbox core. */
            SDL_SetMainReady();
            /* Everything the core reports -- SDL errors, panicf(), the
             * "compiled with SDL" banner, missing-file warnings -- goes to
             * stdout/stderr.  On iOS those are discarded, not routed to the
             * system log, so the core has been running blind.  Point them at
             * a file in Documents (file sharing is enabled, so it can also be
             * pulled over USB) and show the tail on screen if no frame
             * arrives. */
            NSURL *logURL = [self.simulatorRoot
                URLByAppendingPathComponent:@"rockbox-sim.log"];
            [NSFileManager.defaultManager removeItemAtURL:logURL error:nil];
            self.coreLogURL = logURL;
            freopen(logURL.path.fileSystemRepresentation, "w", stdout);
            freopen(logURL.path.fileSystemRepresentation, "w", stderr);
            setvbuf(stdout, NULL, _IOLBF, 0);
            setvbuf(stderr, NULL, _IOLBF, 0);

            chdir(bundle.fileSystemRepresentation);
            char executable[] = "rockboxui";
            char rootOption[] = "--root";
            char *rootPath = strdup(root.fileSystemRepresentation);
            char *arguments[] = { executable, rootOption, rootPath, NULL };
            rockbox_sim_main(3, arguments);
            self.coreExited = YES;
            free(rootPath);
        }
        self.running = NO;
        self.stopping = NO;
        dispatch_async(dispatch_get_main_queue(), ^{
            [self dismissSimulator];
        });
    });
}

- (void)prepareForMediaWithCompletion:(void (^)(BOOL, NSString *))completion
{
    if (self.running)
    {
        if (completion) completion(YES, @"Simulator media root is ready.");
        return;
    }
    if (self.preparing)
    {
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 250 * NSEC_PER_MSEC),
                       dispatch_get_main_queue(), ^{
            [self prepareForMediaWithCompletion:completion];
        });
        return;
    }
    self.preparing = YES;
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        @autoreleasepool
        {
            NSString *message = nil;
            BOOL success = [self prepareInstall:&message];
            self.preparing = NO;
            dispatch_async(dispatch_get_main_queue(), ^{
                if (completion)
                    completion(success, success ?
                        @"Simulator media root is ready." :
                        (message ?: @"Could not prepare simulator media root."));
            });
        }
    });
}

- (void)stop
{
    if (!self.running)
    {
        [self dismissSimulator];
        return;
    }
    if (self.stopping)
        return;
    self.stopping = YES;
    self.simulatorStatus.hidden = NO;
    self.simulatorStatus.text = @"Closing Rockbox…";
    sdl_sys_quit();
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 2 * NSEC_PER_SEC),
                   dispatch_get_main_queue(), ^{
        if (!self.running || !self.stopping)
            return;
        self.simulatorStatus.text = @"Rockbox is still closing…";
        sdl_sys_quit();
        SDL_Event forced;
        SDL_zero(forced);
        forced.type = SDL_USEREVENT;
        SDL_PushEvent(&forced);
    });
}

@end
