#import "OLGameViewController.h"

#import <AudioToolbox/AudioToolbox.h>
#import <QuartzCore/QuartzCore.h>
#include <mach/mach_time.h>

#include "UpstreamFull/game.h"
#include "OLPlatform.h"

#define OL_AUDIO_BUFFER_BYTES 8192

static NSString *olDataRoot;
static AudioQueueRef olAudioQueue;
static volatile BOOL olAudioRunning;
static volatile unsigned long olAudioUnderruns;
static volatile NSTimeInterval olAudioLastCallback;
static BOOL olICadeConnected;

static NSString *olPath(NSString *name) {
    return [olDataRoot stringByAppendingPathComponent:name];
}

static BOOL olValidTR1Level(NSString *path) {
    FILE *stream = fopen([path fileSystemRepresentation], "rb");
    uint8 magic[4];
    BOOL valid = stream && fread(magic, 1, sizeof(magic), stream) == sizeof(magic) &&
        magic[0] == 0x20 && magic[1] == 0 && magic[2] == 0 && magic[3] == 0;
    if (stream) fclose(stream);
    return valid;
}

bool osJoyReady(int index) {
    return index == 0 && olICadeConnected;
}

void osJoyVibrate(int index, float left, float right) {
    (void)index;
    if (left > 0.01f || right > 0.01f)
        AudioServicesPlaySystemSound(kSystemSoundID_Vibrate);
}

int osGetTimeMS() {
    return (int)(CACurrentMediaTime() * 1000.0);
}

static void olAudioCallback(void *context, AudioQueueRef queue,
                            AudioQueueBufferRef buffer) {
    (void)context;
    if (!olAudioRunning) return;
    NSTimeInterval now = CACurrentMediaTime();
    NSTimeInterval expected = buffer->mAudioDataBytesCapacity /
                              (44100.0 * sizeof(Sound::Frame));
    if (olAudioLastCallback > 0 && now - olAudioLastCallback > expected * 1.75)
        olAudioUnderruns++;
    olAudioLastCallback = now;
    UInt32 count = buffer->mAudioDataBytesCapacity / sizeof(Sound::Frame);
    if (Game::level) {
        Sound::fill((Sound::Frame *)buffer->mAudioData, count);
    } else {
        memset(buffer->mAudioData, 0, buffer->mAudioDataBytesCapacity);
    }
    buffer->mAudioDataByteSize = count * sizeof(Sound::Frame);
    if (olAudioRunning) AudioQueueEnqueueBuffer(queue, buffer, 0, NULL);
}

static BOOL olAudioStart(void) {
    AudioStreamBasicDescription format;
    memset(&format, 0, sizeof(format));
    format.mSampleRate = 44100;
    format.mFormatID = kAudioFormatLinearPCM;
    format.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger |
                          kLinearPCMFormatFlagIsPacked;
    format.mBytesPerPacket = sizeof(Sound::Frame);
    format.mFramesPerPacket = 1;
    format.mBytesPerFrame = sizeof(Sound::Frame);
    format.mChannelsPerFrame = 2;
    format.mBitsPerChannel = 16;

    UInt32 category = kAudioSessionCategory_SoloAmbientSound;
    AudioSessionInitialize(NULL, NULL, NULL, NULL);
    AudioSessionSetProperty(kAudioSessionProperty_AudioCategory,
                            sizeof(category), &category);
    AudioSessionSetActive(true);
    if (AudioQueueNewOutput(&format, olAudioCallback, NULL, NULL, NULL, 0,
                            &olAudioQueue) != noErr)
        return NO;

    olAudioUnderruns = 0;
    olAudioLastCallback = 0;
    olAudioRunning = YES;
    for (int index = 0; index < 3; ++index) {
        AudioQueueBufferRef buffer;
        if (AudioQueueAllocateBuffer(olAudioQueue, OL_AUDIO_BUFFER_BYTES,
                                     &buffer) != noErr) {
            olAudioRunning = NO;
            AudioQueueDispose(olAudioQueue, true);
            olAudioQueue = NULL;
            AudioSessionSetActive(false);
            return NO;
        }
        olAudioCallback(NULL, olAudioQueue, buffer);
    }
    if (AudioQueueStart(olAudioQueue, NULL) != noErr) {
        olAudioRunning = NO;
        AudioQueueDispose(olAudioQueue, true);
        olAudioQueue = NULL;
        AudioSessionSetActive(false);
        return NO;
    }
    return YES;
}

static void olAudioStop(void) {
    olAudioRunning = NO;
    if (olAudioQueue) {
        AudioQueueStop(olAudioQueue, true);
        AudioQueueDispose(olAudioQueue, true);
        olAudioQueue = NULL;
    }
    AudioSessionSetActive(false);
}

@class OLInputView;

@protocol OLInputViewDelegate <NSObject>
- (void)inputView:(OLInputView *)view changedKeys:(uint32)newKeys;
- (void)inputViewSawICade:(OLInputView *)view;
@end

@interface OLInputView : UIView <UIKeyInput>
@property(nonatomic, weak) id<OLInputViewDelegate> delegate;
@property(nonatomic) uint32 touchKeys;
@property(nonatomic) uint32 hardwareKeys;
@property(nonatomic, strong) NSMutableSet *activeTouches;
@property(nonatomic, strong) UIView *quietKeyboard;
@property(nonatomic) NSTimeInterval lastHardwareEvent;
- (void)resetInput;
- (void)expireHardwareInputAtTime:(NSTimeInterval)now;
@end

@implementation OLInputView

- (id)initWithFrame:(CGRect)frame {
    if ((self = [super initWithFrame:frame])) {
        self.backgroundColor = [UIColor clearColor];
        self.multipleTouchEnabled = YES;
        self.activeTouches = [NSMutableSet set];
        self.quietKeyboard = [[UIView alloc] initWithFrame:CGRectZero];
    }
    return self;
}

- (BOOL)canBecomeFirstResponder { return YES; }
- (UIView *)inputView { return self.quietKeyboard; }
- (BOOL)hasText { return NO; }
- (void)deleteBackward {}

- (void)insertText:(NSString *)text {
    for (NSUInteger index = 0; index < [text length]; ++index) {
        int recognized = 0;
        self.hardwareKeys = ol_icade_update(
            self.hardwareKeys, [text characterAtIndex:index], &recognized);
        if (recognized) {
            self.lastHardwareEvent = CACurrentMediaTime();
            self.alpha = 0.28;
            [self.delegate inputView:self
                         changedKeys:self.touchKeys | self.hardwareKeys];
            [self.delegate inputViewSawICade:self];
        }
    }
}

- (void)updateTouches {
    uint32 newKeys = 0;
    for (UITouch *touch in self.activeTouches) {
        CGPoint point = [touch locationInView:self];
        newKeys |= ol_touch_keys(point.x, point.y, self.bounds.size.width,
                                 self.bounds.size.height);
    }
    self.touchKeys = newKeys;
    [self.delegate inputView:self changedKeys:newKeys | self.hardwareKeys];
    [self setNeedsDisplay];
}

- (void)touchesBegan:(NSSet *)touches withEvent:(UIEvent *)event {
    (void)event;
    self.alpha = 1.0;
    [self.activeTouches unionSet:touches];
    [self updateTouches];
}
- (void)touchesMoved:(NSSet *)touches withEvent:(UIEvent *)event {
    (void)touches;
    (void)event;
    [self updateTouches];
}
- (void)touchesEnded:(NSSet *)touches withEvent:(UIEvent *)event {
    (void)event;
    [self.activeTouches minusSet:touches];
    [self updateTouches];
}
- (void)touchesCancelled:(NSSet *)touches withEvent:(UIEvent *)event {
    [self touchesEnded:touches withEvent:event];
}

- (void)resetInput {
    [self.activeTouches removeAllObjects];
    self.touchKeys = self.hardwareKeys = 0;
    [self.delegate inputView:self changedKeys:0];
    [self setNeedsDisplay];
}

- (void)expireHardwareInputAtTime:(NSTimeInterval)now {
    if (self.hardwareKeys && self.lastHardwareEvent > 0 &&
        now - self.lastHardwareEvent > 2.0) {
        self.hardwareKeys = 0;
        [self.delegate inputView:self changedKeys:self.touchKeys];
    }
}

- (void)drawCircle:(CGContextRef)context center:(CGPoint)center
             label:(NSString *)label active:(BOOL)active {
    CGContextSetRGBFillColor(context, 0.05, 0.05, 0.06,
                             active ? 0.80 : 0.48);
    CGContextFillEllipseInRect(context,
        CGRectMake(center.x - 27, center.y - 27, 54, 54));
    UIFont *font = [UIFont boldSystemFontOfSize:13];
    CGSize size = [label sizeWithFont:font];
    [[UIColor whiteColor] set];
    [label drawAtPoint:CGPointMake(center.x - size.width / 2,
                                   center.y - size.height / 2)
              withFont:font];
}

- (void)drawRect:(CGRect)rect {
    (void)rect;
    CGContextRef context = UIGraphicsGetCurrentContext();
    CGFloat height = self.bounds.size.height;
    CGContextSetRGBFillColor(context, 0.05, 0.05, 0.06, 0.45);
    CGContextFillEllipseInRect(context, CGRectMake(20, height - 145, 126, 126));
    UIFont *arrowFont = [UIFont boldSystemFontOfSize:24];
    [[UIColor colorWithWhite:1 alpha:0.8] set];
    [@"▲" drawAtPoint:CGPointMake(73, height - 141) withFont:arrowFont];
    [@"▼" drawAtPoint:CGPointMake(73, height - 57) withFont:arrowFont];
    [@"◀" drawAtPoint:CGPointMake(24, height - 101) withFont:arrowFont];
    [@"▶" drawAtPoint:CGPointMake(117, height - 101) withFont:arrowFont];

    [self drawCircle:context center:CGPointMake(410, height - 68) label:@"ACT"
               active:(self.touchKeys & OL_KEY_A) != 0];
    [self drawCircle:context center:CGPointMake(352, height - 43) label:@"JUMP"
               active:(self.touchKeys & OL_KEY_B) != 0];
    [self drawCircle:context center:CGPointMake(442, height - 126) label:@"GUN"
               active:(self.touchKeys & OL_KEY_C) != 0];
    [self drawCircle:context center:CGPointMake(300, height - 101) label:@"WALK"
               active:(self.touchKeys & OL_KEY_X) != 0];
    [self drawCircle:context center:CGPointMake(361, height - 145) label:@"LOOK"
               active:(self.touchKeys & OL_KEY_Y) != 0];
    [self drawCircle:context center:CGPointMake(244, height - 40) label:@"ROLL"
               active:(self.touchKeys & OL_KEY_Z) != 0];

    [[UIColor colorWithRed:0.05 green:0.05 blue:0.06 alpha:0.55] setFill];
    [[UIBezierPath bezierPathWithRoundedRect:
        CGRectMake(self.bounds.size.width - 63, 8, 55, 34)
                                  cornerRadius:8] fill];
    [[UIColor whiteColor] set];
    [@"INV" drawAtPoint:CGPointMake(self.bounds.size.width - 48, 17)
              withFont:[UIFont boldSystemFontOfSize:12]];
}

@end

@interface OLGameViewController () <OLInputViewDelegate>
@property(nonatomic, strong) EAGLContext *context;
@property(nonatomic, strong) OLInputView *controlsView;
@property(nonatomic, strong) UILabel *messageLabel;
@property(nonatomic, strong) UILabel *controllerLabel;
@property(nonatomic) BOOL engineRunning;
@property(nonatomic) BOOL engineInitialized;
@property(nonatomic) BOOL appActive;
@property(nonatomic) BOOL icadeActive;
@property(nonatomic) NSUInteger runFrames;
@property(nonatomic) NSUInteger fpsWindowFrames;
@property(nonatomic) NSTimeInterval runStarted;
@property(nonatomic) NSTimeInterval fpsWindowStarted;
@property(nonatomic) NSTimeInterval pausedAt;
@end

@implementation OLGameViewController

- (void)loadView {
    self.context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
    GLKView *view = [[GLKView alloc] initWithFrame:[[UIScreen mainScreen] bounds]
                                           context:self.context];
    view.drawableDepthFormat = GLKViewDrawableDepthFormat16;
    view.drawableColorFormat = GLKViewDrawableColorFormatRGBA8888;
    view.contentScaleFactor = 1.0;
    self.view = view;
    self.preferredFramesPerSecond = 30;

    self.controlsView = [[OLInputView alloc] initWithFrame:view.bounds];
    self.controlsView.autoresizingMask = UIViewAutoresizingFlexibleWidth |
                                         UIViewAutoresizingFlexibleHeight;
    self.controlsView.delegate = self;
    [view addSubview:self.controlsView];

    self.messageLabel = [[UILabel alloc] initWithFrame:CGRectInset(view.bounds, 45, 65)];
    self.messageLabel.autoresizingMask = UIViewAutoresizingFlexibleWidth |
                                         UIViewAutoresizingFlexibleHeight;
    self.messageLabel.backgroundColor = [UIColor colorWithWhite:0 alpha:0.82];
    self.messageLabel.textColor = [UIColor whiteColor];
    self.messageLabel.textAlignment = NSTextAlignmentCenter;
    self.messageLabel.numberOfLines = 0;
    self.messageLabel.font = [UIFont boldSystemFontOfSize:15];
    self.messageLabel.layer.cornerRadius = 12;
    self.messageLabel.layer.masksToBounds = YES;
    [view addSubview:self.messageLabel];

    self.controllerLabel = [[UILabel alloc] initWithFrame:CGRectMake(8, 8, 190, 24)];
    self.controllerLabel.backgroundColor = [UIColor colorWithWhite:0 alpha:0.6];
    self.controllerLabel.textColor = [UIColor whiteColor];
    self.controllerLabel.font = [UIFont boldSystemFontOfSize:11];
    self.controllerLabel.textAlignment = NSTextAlignmentCenter;
    self.controllerLabel.text = @"TOUCH · iCADE READY";
    self.controllerLabel.layer.cornerRadius = 5;
    self.controllerLabel.layer.masksToBounds = YES;
    [view addSubview:self.controllerLabel];
}

- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)orientation {
    return UIInterfaceOrientationIsLandscape(orientation);
}
- (NSUInteger)supportedInterfaceOrientations {
    return UIInterfaceOrientationMaskLandscape;
}
- (BOOL)prefersStatusBarHidden { return YES; }

- (NSArray *)requiredTR1Files {
    static NSArray *files;
    if (!files) files = @[@"TITLE.PHD", @"GYM.PHD", @"LEVEL1.PHD",
        @"LEVEL2.PHD", @"LEVEL3A.PHD", @"LEVEL3B.PHD", @"CUT1.PHD",
        @"LEVEL4.PHD", @"LEVEL5.PHD", @"LEVEL6.PHD", @"LEVEL7A.PHD",
        @"LEVEL7B.PHD", @"CUT2.PHD", @"LEVEL8A.PHD", @"LEVEL8B.PHD",
        @"LEVEL8C.PHD", @"LEVEL10A.PHD", @"CUT3.PHD", @"LEVEL10B.PHD",
        @"CUT4.PHD", @"LEVEL10C.PHD", @"EGYPT.PHD", @"CAT.PHD",
        @"END.PHD", @"END2.PHD"];
    return files;
}

- (NSUInteger)dataScoreAtRoot:(NSString *)root {
    NSUInteger score = 0;
    for (NSString *name in [self requiredTR1Files]) {
        NSString *path = [[root stringByAppendingPathComponent:@"DATA"]
                          stringByAppendingPathComponent:name];
        if ([[NSFileManager defaultManager] fileExistsAtPath:path]) score++;
    }
    return score;
}

- (void)prepareDataDirectory {
    NSArray *documents = NSSearchPathForDirectoriesInDomains(
        NSDocumentDirectory, NSUserDomainMask, YES);
    NSString *documentsRoot = [[documents objectAtIndex:0]
                               stringByAppendingPathComponent:@"OpenLara"];
    NSString *mediaRoot = @"/var/mobile/Media/OpenLara";
    olDataRoot = [self dataScoreAtRoot:documentsRoot] >
                 [self dataScoreAtRoot:mediaRoot] ? documentsRoot : mediaRoot;
    NSFileManager *manager = [NSFileManager defaultManager];
    if (![manager createDirectoryAtPath:olDataRoot
            withIntermediateDirectories:YES attributes:nil error:NULL]) {
        olDataRoot = documentsRoot;
        [manager createDirectoryAtPath:olDataRoot
            withIntermediateDirectories:YES attributes:nil error:NULL];
    }
    [manager createDirectoryAtPath:olPath(@"cache")
       withIntermediateDirectories:YES attributes:nil error:NULL];
    [manager createDirectoryAtPath:olPath(@"saves")
       withIntermediateDirectories:YES attributes:nil error:NULL];
}

- (NSArray *)missingTR1Files {
    NSMutableArray *missing = [NSMutableArray array];
    for (NSString *name in [self requiredTR1Files]) {
        NSString *path = olPath([@"DATA" stringByAppendingPathComponent:name]);
        if (!olValidTR1Level(path)) [missing addObject:name];
    }
    return missing;
}

- (void)viewDidAppear:(BOOL)animated {
    [super viewDidAppear:animated];
    self.appActive = YES;
    [self.controlsView becomeFirstResponder];
    [self startEngineIfPossible];
}

- (void)startEngineIfPossible {
    if (self.engineRunning || !self.appActive || !self.context) return;
    [self prepareDataDirectory];
    NSArray *missing = [self missingTR1Files];
    if ([missing count]) {
        self.paused = YES;
        NSArray *preview = [missing subarrayWithRange:
            NSMakeRange(0, MIN((NSUInteger)6, [missing count]))];
        NSString *more = [missing count] > 6 ?
            [NSString stringWithFormat:@" (+%lu more)",
             (unsigned long)[missing count] - 6] : @"";
        self.messageLabel.hidden = NO;
        self.messageLabel.text = [NSString stringWithFormat:
            @"OPENLARA TR1 DATA NEEDED\n\nMissing: %@%@\n\nCopy the original "
             "Tomb Raider I PC DATA directory to %@/DATA. Optional soundtrack "
             "files may go in %@/audio.\n\nReopen the app after copying.",
             [preview componentsJoinedByString:@", "], more, olDataRoot,
             olDataRoot];
        return;
    }

    [EAGLContext setCurrentContext:self.context];
    GLKView *view = (GLKView *)self.view;
    [view bindDrawable];
    Core::width = view.drawableWidth ? view.drawableWidth : view.bounds.size.width;
    Core::height = view.drawableHeight ? view.drawableHeight : view.bounds.size.height;
    snprintf(contentDir, sizeof(contentDir), "%s/", [olDataRoot fileSystemRepresentation]);
    snprintf(cacheDir, sizeof(cacheDir), "%s/cache/", [olDataRoot fileSystemRepresentation]);
    snprintf(saveDir, sizeof(saveDir), "%s/saves/", [olDataRoot fileSystemRepresentation]);

    Game::init();
    self.engineInitialized = YES;
    if (!Game::level || Core::isQuit) {
        self.messageLabel.hidden = NO;
        self.messageLabel.text = @"OpenLara could not initialize the TR1 data set.";
        Game::deinit();
        self.engineInitialized = NO;
        self.paused = YES;
        return;
    }
    Input::reset();
    self.engineRunning = YES;
    self.messageLabel.hidden = YES;
    self.runFrames = self.fpsWindowFrames = 0;
    self.runStarted = self.fpsWindowStarted = CACurrentMediaTime();
    if (!olAudioStart())
        self.controllerLabel.text = @"AUDIO UNAVAILABLE · TOUCH READY";
}

- (void)stopEngine {
    if (!self.engineInitialized) return;
    [self writeDiagnostics:@"stop"];
    self.engineRunning = NO;
    self.paused = YES;
    [self.controlsView resetInput];
    olAudioStop();
    [EAGLContext setCurrentContext:self.context];
    Game::deinit();
    self.engineInitialized = NO;
}

- (void)dealloc {
    [self stopEngine];
    if ([EAGLContext currentContext] == self.context)
        [EAGLContext setCurrentContext:nil];
}

- (void)pauseGame {
    self.appActive = NO;
    self.pausedAt = CACurrentMediaTime();
    self.paused = YES;
    [self.controlsView resetInput];
    self.icadeActive = olICadeConnected = NO;
    self.controllerLabel.text = @"TOUCH · iCADE READY";
    olAudioLastCallback = 0;
    if (self.engineInitialized) [self stopEngine];
    [EAGLContext setCurrentContext:self.context];
    GLKView *view = (GLKView *)self.view;
    [view bindDrawable];
    glFinish();
    [view deleteDrawable];
    [EAGLContext setCurrentContext:nil];
    self.view = nil;
    self.controlsView = nil;
    self.messageLabel = nil;
    self.controllerLabel = nil;
    self.context = nil;
}

- (void)resumeGame {
    self.appActive = YES;
    if (!self.context) [self loadView];
    [EAGLContext setCurrentContext:self.context];
    [self.controlsView becomeFirstResponder];
    if (!self.engineRunning) [self startEngineIfPossible];
    NSTimeInterval now = CACurrentMediaTime();
    if (self.runStarted > 0 && self.pausedAt > 0)
        self.runStarted += now - self.pausedAt;
    self.pausedAt = 0;
    self.fpsWindowFrames = 0;
    self.fpsWindowStarted = now;
    self.paused = !self.engineRunning;
    if (self.engineRunning && olAudioQueue) AudioQueueStart(olAudioQueue, NULL);
}

- (void)glkView:(GLKView *)view drawInRect:(CGRect)rect {
    (void)rect;
    [view bindDrawable];
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, (GLint *)&GAPI::defaultFBO);
    if (!self.engineRunning || !Game::level) {
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        return;
    }
    Core::width = view.drawableWidth;
    Core::height = view.drawableHeight;
    [self.controlsView expireHardwareInputAtTime:CACurrentMediaTime()];
    Game::update();
    Game::render();

    self.runFrames++;
    self.fpsWindowFrames++;
    NSTimeInterval now = CACurrentMediaTime();
    if (now - self.fpsWindowStarted >= 1.0) {
        double measured = self.fpsWindowFrames / (now - self.fpsWindowStarted);
        self.controllerLabel.text = [NSString stringWithFormat:@"%@ · %.1f FPS",
            self.icadeActive ? @"iCADE" : @"TOUCH", measured];
        self.fpsWindowFrames = 0;
        self.fpsWindowStarted = now;
    }
}

- (void)didReceiveMemoryWarning {
    [super didReceiveMemoryWarning];
    [self writeDiagnostics:@"memory-warning"];
}

- (void)writeDiagnostics:(NSString *)reason {
    if (!self.engineRunning || !olDataRoot || self.runStarted <= 0) return;
    NSTimeInterval seconds = CACurrentMediaTime() - self.runStarted;
    double measured = seconds > 0 ? self.runFrames / seconds : 0;
    NSString *report = [NSString stringWithFormat:
        @"engine=full\nreason=%@\nframes=%lu\nseconds=%.3f\nfps=%.2f\n"
         "audio_underruns=%lu\ncontroller=%@\nrender=%lux%lu\n",
        reason, (unsigned long)self.runFrames, seconds, measured,
        olAudioUnderruns, self.icadeActive ? @"icade" : @"touch",
        (unsigned long)((GLKView *)self.view).drawableWidth,
        (unsigned long)((GLKView *)self.view).drawableHeight];
    [report writeToFile:olPath(@"last-run.log") atomically:YES
               encoding:NSUTF8StringEncoding error:NULL];
}

- (void)inputView:(OLInputView *)view changedKeys:(uint32)newKeys {
    (void)view;
    if (!self.engineInitialized) return;
    Input::setJoyDown(0, jkLeft,   (newKeys & OL_KEY_LEFT) != 0);
    Input::setJoyDown(0, jkRight,  (newKeys & OL_KEY_RIGHT) != 0);
    Input::setJoyDown(0, jkUp,     (newKeys & OL_KEY_UP) != 0);
    Input::setJoyDown(0, jkDown,   (newKeys & OL_KEY_DOWN) != 0);
    Input::setJoyDown(0, jkA,      (newKeys & OL_KEY_A) != 0);
    Input::setJoyDown(0, jkX,      (newKeys & OL_KEY_B) != 0);
    Input::setJoyDown(0, jkY,      (newKeys & OL_KEY_C) != 0);
    Input::setJoyDown(0, jkRB,     (newKeys & OL_KEY_X) != 0);
    Input::setJoyDown(0, jkLB,     (newKeys & OL_KEY_Y) != 0);
    Input::setJoyDown(0, jkB,      (newKeys & OL_KEY_Z) != 0);
    Input::setJoyDown(0, jkSelect, (newKeys & OL_KEY_SELECT) != 0);
    Input::setJoyDown(0, jkStart,  (newKeys & OL_KEY_START) != 0);
}

- (void)inputViewSawICade:(OLInputView *)view {
    (void)view;
    self.icadeActive = olICadeConnected = YES;
    self.controllerLabel.text = @"iCADE BLUETOOTH ACTIVE";
}

@end
