#import "OLGameViewController.h"

#import <AudioToolbox/AudioToolbox.h>
#import <QuartzCore/QuartzCore.h>
#include <pthread.h>

#include "../../../apps/plugins/openlara/fixed/game.h"
#include "OLPlatform.h"

static_assert((unsigned)OL_KEY_UP == (unsigned)IK_UP &&
              (unsigned)OL_KEY_RIGHT == (unsigned)IK_RIGHT &&
              (unsigned)OL_KEY_DOWN == (unsigned)IK_DOWN &&
              (unsigned)OL_KEY_LEFT == (unsigned)IK_LEFT &&
              (unsigned)OL_KEY_A == (unsigned)IK_A &&
              (unsigned)OL_KEY_B == (unsigned)IK_B &&
              (unsigned)OL_KEY_C == (unsigned)IK_C &&
              (unsigned)OL_KEY_X == (unsigned)IK_X &&
              (unsigned)OL_KEY_Y == (unsigned)IK_Y &&
              (unsigned)OL_KEY_Z == (unsigned)IK_Z &&
              (unsigned)OL_KEY_START == (unsigned)IK_START &&
              (unsigned)OL_KEY_SELECT == (unsigned)IK_SELECT,
              "iOS input key values must match the fixed engine");

#define OL_LEVEL_CAPACITY (8u * 1024u * 1024u)
#define OL_AUDIO_BLOCKS 8

extern int8 soundBuffer[];
extern uint16 fb[];

const void *TRACKS_AD4;
const void *TITLE_SCR;
int32 fps;

static NSString *olDataRoot;
static NSData *olTracksData;
static NSData *olTitleData;
static uint8 *olScaledTitle;
static uint8 *olLevelBuffer;
static BOOL olLevelLoadFailed;
static uint32 olPalette[256];

static AudioQueueRef olAudioQueue;
static pthread_mutex_t olAudioLock = PTHREAD_MUTEX_INITIALIZER;
static int16 olAudioRing[OL_AUDIO_BLOCKS][SND_SAMPLES * 2];
static int olAudioRead;
static int olAudioWrite;
static int olAudioQueued;
static BOOL olAudioRunning;
static volatile unsigned long olAudioUnderruns;

static NSString *olPath(NSString *name) {
    return [olDataRoot stringByAppendingPathComponent:name];
}

static NSData *olReadAsset(NSString *name, NSString *extension) {
    NSString *filename = [name stringByAppendingPathExtension:extension];
    NSData *data = [NSData dataWithContentsOfFile:olPath(filename)];
    if (!data) {
        data = [NSData dataWithContentsOfFile:
                olPath([@"levels" stringByAppendingPathComponent:filename])];
    }
    return data;
}

int32 osGetSystemTimeMS() {
    return (int32)(CACurrentMediaTime() * 1000.0);
}

bool osSaveSettings() {
    NSData *data = [NSData dataWithBytes:&gSettings length:sizeof(gSettings)];
    return [data writeToFile:olPath(@"openlara.cfg") atomically:YES];
}

bool osLoadSettings() {
    NSData *data = [NSData dataWithContentsOfFile:olPath(@"openlara.cfg")];
    Settings loaded;
    if ([data length] != sizeof(loaded)) {
        if (!TRACKS_AD4) gSettings.audio_music = 0;
        return false;
    }
    [data getBytes:&loaded length:sizeof(loaded)];
    if (loaded.version != SETTINGS_VER) {
        if (!TRACKS_AD4) gSettings.audio_music = 0;
        return false;
    }
    gSettings = loaded;
    if (!TRACKS_AD4) gSettings.audio_music = 0;
    return true;
}

bool osCheckSave() {
    return [[NSFileManager defaultManager] fileExistsAtPath:olPath(@"savegame.dat")];
}

bool osSaveGame() {
    if (gSaveGame.dataSize > sizeof(gSaveData)) return false;
    NSMutableData *data = [NSMutableData dataWithBytes:&gSaveGame
                                                length:sizeof(gSaveGame)];
    [data appendBytes:gSaveData length:gSaveGame.dataSize];
    return [data writeToFile:olPath(@"savegame.dat") atomically:YES];
}

bool osLoadGame() {
    NSData *data = [NSData dataWithContentsOfFile:olPath(@"savegame.dat")];
    if ([data length] < sizeof(gSaveGame)) return false;
    [data getBytes:&gSaveGame range:NSMakeRange(0, sizeof(gSaveGame))];
    if (gSaveGame.version != SAVEGAME_VER ||
        gSaveGame.dataSize > sizeof(gSaveData) ||
        [data length] != sizeof(gSaveGame) + gSaveGame.dataSize) return false;
    [data getBytes:gSaveData
             range:NSMakeRange(sizeof(gSaveGame), gSaveGame.dataSize)];
    return true;
}

void osJoyVibrate(int32 index, int32 left, int32 right) {
    (void)index;
    if (gSettings.controls_vibration && X_MAX(left, right) > 0)
        AudioServicesPlaySystemSound(kSystemSoundID_Vibrate);
}

void osSetPalette(const uint16 *palette) {
    for (int i = 0; i < 256; ++i) {
        ol_palette_rgba(palette[i], (uint8 *)&olPalette[i]);
    }
}

const void *osLoadScreen(LevelID level) {
    (void)level;
    return TITLE_SCR;
}

const void *osLoadLevel(LevelID level) {
    if ((unsigned)level >= LVL_MAX || !gLevelInfo[level].data) {
        olLevelLoadFailed = YES;
        return NULL;
    }
    NSString *name = [NSString stringWithUTF8String:
                      (const char *)gLevelInfo[level].data];
    NSData *data = olReadAsset(name, @"PKD");
    NSUInteger size = [data length];
    if (size < 172 || size > OL_LEVEL_CAPACITY) {
        olLevelLoadFailed = YES;
        return NULL;
    }
    [data getBytes:olLevelBuffer length:size];
    const uint16 *counts = (const uint16 *)(olLevelBuffer + 4);
    if (!counts[0] || !counts[1] || counts[1] > MAX_ROOMS ||
        counts[8] > MAX_TEXTURES || counts[9] > MAX_SPRITES ||
        counts[10] > MAX_ITEMS || counts[11] > MAX_CAMERAS) {
        olLevelLoadFailed = YES;
        return NULL;
    }
    const uint32 *offsets = (const uint32 *)(olLevelBuffer + 32);
    for (int i = 0; i < 35; ++i) {
        if (offsets[i] >= size) {
            olLevelLoadFailed = YES;
            return NULL;
        }
    }
    if (counts[1] * 56u > size - offsets[3]) {
        olLevelLoadFailed = YES;
        return NULL;
    }
    gLevelID = level;
    return olLevelBuffer;
}

static void olAudioCallback(void *context, AudioQueueRef queue,
                            AudioQueueBufferRef buffer) {
    (void)context;
    const UInt32 bytes = SND_SAMPLES * 2 * sizeof(int16);
    pthread_mutex_lock(&olAudioLock);
    if (olAudioQueued > 0) {
        memcpy(buffer->mAudioData, olAudioRing[olAudioRead], bytes);
        olAudioRead = (olAudioRead + 1) % OL_AUDIO_BLOCKS;
        olAudioQueued--;
    } else {
        memset(buffer->mAudioData, 0, bytes);
        olAudioUnderruns++;
    }
    pthread_mutex_unlock(&olAudioLock);
    buffer->mAudioDataByteSize = bytes;
    if (olAudioRunning) AudioQueueEnqueueBuffer(queue, buffer, 0, NULL);
}

static BOOL olAudioStart(void) {
    AudioStreamBasicDescription format;
    memset(&format, 0, sizeof(format));
    format.mSampleRate = SND_OUTPUT_FREQ;
    format.mFormatID = kAudioFormatLinearPCM;
    format.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger |
                          kLinearPCMFormatFlagIsPacked;
    format.mBytesPerPacket = 4;
    format.mFramesPerPacket = 1;
    format.mBytesPerFrame = 4;
    format.mChannelsPerFrame = 2;
    format.mBitsPerChannel = 16;

    UInt32 category = kAudioSessionCategory_SoloAmbientSound;
    AudioSessionInitialize(NULL, NULL, NULL, NULL);
    AudioSessionSetProperty(kAudioSessionProperty_AudioCategory,
                            sizeof(category), &category);
    AudioSessionSetActive(true);
    if (AudioQueueNewOutput(&format, olAudioCallback, NULL, NULL, NULL, 0,
                            &olAudioQueue) != noErr) return NO;

    olAudioRead = olAudioWrite = olAudioQueued = 0;
    olAudioUnderruns = 0;
    olAudioRunning = YES;
    for (int i = 0; i < 4; ++i) {
        AudioQueueBufferRef buffer;
        if (AudioQueueAllocateBuffer(olAudioQueue,
                                     SND_SAMPLES * 4, &buffer) != noErr) {
            olAudioRunning = NO;
            AudioQueueDispose(olAudioQueue, true);
            olAudioQueue = NULL;
            return NO;
        }
        memset(buffer->mAudioData, 0, SND_SAMPLES * 4);
        buffer->mAudioDataByteSize = SND_SAMPLES * 4;
        AudioQueueEnqueueBuffer(olAudioQueue, buffer, 0, NULL);
    }
    if (AudioQueueStart(olAudioQueue, NULL) != noErr) {
        olAudioRunning = NO;
        AudioQueueDispose(olAudioQueue, true);
        olAudioQueue = NULL;
        return NO;
    }
    return YES;
}

static void olAudioSubmit(void) {
    int16 block[SND_SAMPLES * 2];
    sndFill(soundBuffer);
    for (int i = 0; i < SND_SAMPLES; ++i) {
        int16 sample = ((int)(uint8)soundBuffer[i] - 128) << 8;
        block[i * 2] = block[i * 2 + 1] = sample;
    }
    pthread_mutex_lock(&olAudioLock);
    if (olAudioQueued < OL_AUDIO_BLOCKS - 1) {
        memcpy(olAudioRing[olAudioWrite], block, sizeof(block));
        olAudioWrite = (olAudioWrite + 1) % OL_AUDIO_BLOCKS;
        olAudioQueued++;
    }
    pthread_mutex_unlock(&olAudioLock);
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

@interface OLScreenView : UIView
@property(nonatomic, readonly) uint32 *rgbaFrame;
@end

@implementation OLScreenView {
    CGDataProviderRef _provider;
    CGColorSpaceRef _colorSpace;
}

- (id)initWithFrame:(CGRect)frame {
    if ((self = [super initWithFrame:frame])) {
        _rgbaFrame = (uint32 *)calloc(FRAME_WIDTH * FRAME_HEIGHT,
                                      sizeof(uint32));
        if (!_rgbaFrame) return nil;
        _provider = CGDataProviderCreateWithData(
            NULL, _rgbaFrame, FRAME_WIDTH * FRAME_HEIGHT * 4, NULL);
        _colorSpace = CGColorSpaceCreateDeviceRGB();
        self.backgroundColor = [UIColor blackColor];
        self.opaque = YES;
        self.contentMode = UIViewContentModeRedraw;
    }
    return self;
}

- (void)dealloc {
    if (_provider) CGDataProviderRelease(_provider);
    if (_colorSpace) CGColorSpaceRelease(_colorSpace);
    if (_rgbaFrame) free(_rgbaFrame);
}

- (void)drawRect:(CGRect)rect {
    (void)rect;
    CGImageRef image = CGImageCreate(FRAME_WIDTH, FRAME_HEIGHT, 8, 32,
        FRAME_WIDTH * 4, _colorSpace,
        kCGBitmapByteOrder32Big | kCGImageAlphaPremultipliedLast,
        _provider, NULL, false, kCGRenderingIntentDefault);
    if (!image) return;

    CGContextRef context = UIGraphicsGetCurrentContext();
    CGFloat scale = MIN(self.bounds.size.width / FRAME_WIDTH,
                        self.bounds.size.height / FRAME_HEIGHT);
    CGFloat width = FRAME_WIDTH * scale;
    CGFloat height = FRAME_HEIGHT * scale;
    CGRect target = CGRectMake((self.bounds.size.width - width) * 0.5,
                               (self.bounds.size.height - height) * 0.5,
                               width, height);
    CGContextSaveGState(context);
    CGContextSetInterpolationQuality(context, kCGInterpolationNone);
    CGContextTranslateCTM(context, 0, self.bounds.size.height);
    CGContextScaleCTM(context, 1, -1);
    CGContextDrawImage(context, target, image);
    CGContextRestoreGState(context);
    CGImageRelease(image);
}

@end

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
    for (NSUInteger p = 0; p < [text length]; ++p) {
        int recognized = 0;
        self.hardwareKeys = ol_icade_update(
            self.hardwareKeys, [text characterAtIndex:p], &recognized);
        if (recognized) {
            self.alpha = 0.28;
            [self.delegate inputView:self
                         changedKeys:self.touchKeys | self.hardwareKeys];
            [self.delegate inputViewSawICade:self];
        }
    }
}

- (uint32)keysAtPoint:(CGPoint)point {
    return ol_touch_keys(point.x, point.y, self.bounds.size.width,
                         self.bounds.size.height);
}

- (void)updateTouches {
    uint32 newKeys = 0;
    for (UITouch *touch in self.activeTouches)
        newKeys |= [self keysAtPoint:[touch locationInView:self]];
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
    (void)touches; (void)event; [self updateTouches];
}
- (void)touchesEnded:(NSSet *)touches withEvent:(UIEvent *)event {
    (void)event;
    [self.activeTouches minusSet:touches];
    [self updateTouches];
}
- (void)touchesCancelled:(NSSet *)touches withEvent:(UIEvent *)event {
    [self touchesEnded:touches withEvent:event];
}

- (void)drawCircle:(CGContextRef)context center:(CGPoint)center
             label:(NSString *)label active:(BOOL)active {
    CGContextSetRGBFillColor(context, 0.05, 0.05, 0.06, active ? 0.80 : 0.48);
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
    CGFloat h = self.bounds.size.height;
    CGContextSetRGBFillColor(context, 0.05, 0.05, 0.06, 0.45);
    CGContextFillEllipseInRect(context, CGRectMake(20, h - 145, 126, 126));
    UIFont *arrowFont = [UIFont boldSystemFontOfSize:24];
    [[UIColor colorWithWhite:1 alpha:0.8] set];
    [@"▲" drawAtPoint:CGPointMake(73, h - 141) withFont:arrowFont];
    [@"▼" drawAtPoint:CGPointMake(73, h - 57) withFont:arrowFont];
    [@"◀" drawAtPoint:CGPointMake(24, h - 101) withFont:arrowFont];
    [@"▶" drawAtPoint:CGPointMake(117, h - 101) withFont:arrowFont];

    [self drawCircle:context center:CGPointMake(410, h - 68) label:@"ACT"
               active:(self.touchKeys & IK_A) != 0];
    [self drawCircle:context center:CGPointMake(352, h - 43) label:@"JUMP"
               active:(self.touchKeys & IK_B) != 0];
    [self drawCircle:context center:CGPointMake(442, h - 126) label:@"GUN"
               active:(self.touchKeys & IK_C) != 0];
    [self drawCircle:context center:CGPointMake(300, h - 101) label:@"WALK"
               active:(self.touchKeys & IK_X) != 0];
    [self drawCircle:context center:CGPointMake(361, h - 145) label:@"LOOK"
               active:(self.touchKeys & IK_Y) != 0];
    [self drawCircle:context center:CGPointMake(244, h - 40) label:@"ROLL"
               active:(self.touchKeys & IK_Z) != 0];

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
@property(nonatomic, strong) OLScreenView *screenView;
@property(nonatomic, strong) OLInputView *inputView;
@property(nonatomic, strong) UILabel *messageLabel;
@property(nonatomic, strong) UILabel *controllerLabel;
@property(nonatomic, strong) CADisplayLink *displayLink;
@property(nonatomic) uint32 inputKeys;
@property(nonatomic) BOOL engineRunning;
@property(nonatomic) BOOL appActive;
@property(nonatomic) BOOL icadeActive;
@property(nonatomic) NSUInteger runFrames;
@property(nonatomic) NSUInteger fpsWindowFrames;
@property(nonatomic) NSUInteger slowEngineFrames;
@property(nonatomic) NSTimeInterval runStarted;
@property(nonatomic) NSTimeInterval fpsWindowStarted;
@property(nonatomic) NSTimeInterval pausedAt;
@property(nonatomic) NSTimeInterval worstEngineSeconds;
@end

@implementation OLGameViewController

- (void)loadView {
    UIView *root = [[UIView alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
    root.backgroundColor = [UIColor blackColor];
    self.view = root;

    self.screenView = [[OLScreenView alloc] initWithFrame:root.bounds];
    self.screenView.autoresizingMask = UIViewAutoresizingFlexibleWidth |
                                       UIViewAutoresizingFlexibleHeight;
    [root addSubview:self.screenView];

    self.inputView = [[OLInputView alloc] initWithFrame:root.bounds];
    self.inputView.autoresizingMask = UIViewAutoresizingFlexibleWidth |
                                      UIViewAutoresizingFlexibleHeight;
    self.inputView.delegate = self;
    [root addSubview:self.inputView];

    self.messageLabel = [[UILabel alloc] initWithFrame:CGRectInset(root.bounds, 45, 65)];
    self.messageLabel.autoresizingMask = UIViewAutoresizingFlexibleWidth |
                                         UIViewAutoresizingFlexibleHeight;
    self.messageLabel.backgroundColor = [UIColor colorWithWhite:0 alpha:0.78];
    self.messageLabel.textColor = [UIColor whiteColor];
    self.messageLabel.textAlignment = NSTextAlignmentCenter;
    self.messageLabel.numberOfLines = 0;
    self.messageLabel.font = [UIFont boldSystemFontOfSize:16];
    self.messageLabel.layer.cornerRadius = 12;
    self.messageLabel.layer.masksToBounds = YES;
    [root addSubview:self.messageLabel];

    self.controllerLabel = [[UILabel alloc] initWithFrame:CGRectMake(8, 8, 180, 24)];
    self.controllerLabel.autoresizingMask = UIViewAutoresizingFlexibleRightMargin |
                                            UIViewAutoresizingFlexibleBottomMargin;
    self.controllerLabel.backgroundColor = [UIColor colorWithWhite:0 alpha:0.55];
    self.controllerLabel.textColor = [UIColor whiteColor];
    self.controllerLabel.font = [UIFont boldSystemFontOfSize:11];
    self.controllerLabel.textAlignment = NSTextAlignmentCenter;
    self.controllerLabel.text = @"TOUCH · iCADE READY";
    self.controllerLabel.layer.cornerRadius = 5;
    self.controllerLabel.layer.masksToBounds = YES;
    [root addSubview:self.controllerLabel];
}

- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)orientation {
    return UIInterfaceOrientationIsLandscape(orientation);
}
- (NSUInteger)supportedInterfaceOrientations {
    return UIInterfaceOrientationMaskLandscape;
}
- (BOOL)prefersStatusBarHidden { return YES; }

- (void)viewDidAppear:(BOOL)animated {
    [super viewDidAppear:animated];
    [self.inputView becomeFirstResponder];
    self.appActive = YES;
    [self startEngineIfPossible];
}

- (void)dealloc {
    [self stopEngine];
}

- (NSUInteger)dataScoreAtRoot:(NSString *)root {
    NSArray *required = @[@"TITLE.PKD", @"GYM.PKD", @"LEVEL1.PKD",
                          @"LEVEL2.PKD"];
    NSUInteger score = 0;

    for (NSString *name in required) {
        NSString *direct = [root stringByAppendingPathComponent:name];
        NSString *levels = [root stringByAppendingPathComponent:
                            [@"levels" stringByAppendingPathComponent:name]];
        if ([[NSFileManager defaultManager] fileExistsAtPath:direct] ||
            [[NSFileManager defaultManager] fileExistsAtPath:levels])
            score++;
    }
    return score;
}

- (void)prepareDataDirectory {
    NSArray *documents = NSSearchPathForDirectoriesInDomains(
        NSDocumentDirectory, NSUserDomainMask, YES);
    NSString *documentsRoot = [[documents objectAtIndex:0]
                               stringByAppendingPathComponent:@"OpenLara"];
    NSString *mediaRoot = @"/var/mobile/Media/OpenLara";
    NSUInteger documentsScore = [self dataScoreAtRoot:documentsRoot];
    NSUInteger mediaScore = [self dataScoreAtRoot:mediaRoot];

    olDataRoot = documentsScore > mediaScore ? documentsRoot : mediaRoot;
    if (![[NSFileManager defaultManager] createDirectoryAtPath:olDataRoot
                                   withIntermediateDirectories:YES
                                                    attributes:nil error:NULL]) {
        olDataRoot = documentsRoot;
        [[NSFileManager defaultManager] createDirectoryAtPath:olDataRoot
                                  withIntermediateDirectories:YES
                                                   attributes:nil error:NULL];
    }
}

- (void)loadOptionalAssets {
    olTracksData = [NSData dataWithContentsOfFile:olPath(@"TRACKS.AD4")];
    TRACKS_AD4 = NULL;
    if ([olTracksData length] >= 14 * sizeof(int32) * 2) {
        const int32 *info = (const int32 *)[olTracksData bytes];
        BOOL valid = YES;
        const int needed[] = {4, 5, 13};
        for (unsigned i = 0; i < sizeof(needed) / sizeof(needed[0]); ++i) {
            int offset = info[needed[i] * 2];
            int size = info[needed[i] * 2 + 1];
            if (offset < 0 || size < 0 ||
                (NSUInteger)offset > [olTracksData length] ||
                (NSUInteger)size > [olTracksData length] - offset)
                valid = NO;
        }
        if (valid) TRACKS_AD4 = [olTracksData bytes];
    }
    olTitleData = [NSData dataWithContentsOfFile:olPath(@"TITLE.SCR")];
    if ([olTitleData length] == FRAME_WIDTH * FRAME_HEIGHT) {
        TITLE_SCR = [olTitleData bytes];
    } else if ([olTitleData length] == 240 * 160) {
        if (!olScaledTitle) olScaledTitle = (uint8 *)malloc(FRAME_WIDTH * FRAME_HEIGHT);
        const uint8 *source = (const uint8 *)[olTitleData bytes];
        for (int y = 0; y < FRAME_HEIGHT; ++y)
            for (int x = 0; x < FRAME_WIDTH; ++x)
                olScaledTitle[y * FRAME_WIDTH + x] =
                    source[(y * 160 / FRAME_HEIGHT) * 240 +
                           x * 240 / FRAME_WIDTH];
        TITLE_SCR = olScaledTitle;
    }
}

- (void)startEngineIfPossible {
    if (self.engineRunning || !self.appActive) return;
    [self prepareDataDirectory];
    NSArray *required = @[@"TITLE.PKD", @"GYM.PKD", @"LEVEL1.PKD",
                          @"LEVEL2.PKD"];
    NSMutableArray *missing = [NSMutableArray array];
    for (NSString *name in required) {
        if (![[NSFileManager defaultManager] fileExistsAtPath:olPath(name)] &&
            ![[NSFileManager defaultManager] fileExistsAtPath:
              olPath([@"levels" stringByAppendingPathComponent:name])])
            [missing addObject:name];
    }
    if ([missing count]) {
        self.messageLabel.hidden = NO;
        self.messageLabel.text = [NSString stringWithFormat:
            @"OPENLARA DATA NEEDED\n\nMissing: %@\n\nCopy the converted files "
             "into %@ using iFunBox or SSH. A sandboxed/IPA install may use "
             "Documents/OpenLara through iTunes File Sharing. Optional: "
             "TITLE.SCR and TRACKS.AD4.\n\nReopen the app after copying.",
             [missing componentsJoinedByString:@", "], olDataRoot];
        return;
    }
    if (!olLevelBuffer) olLevelBuffer = (uint8 *)malloc(OL_LEVEL_CAPACITY);
    if (!olLevelBuffer) {
        self.messageLabel.text = @"OpenLara could not reserve its level memory.";
        self.messageLabel.hidden = NO;
        return;
    }
    [self loadOptionalAssets];
    olLevelLoadFailed = NO;
    for (int level = LVL_TR1_TITLE; level <= LVL_TR1_2; ++level) {
        if (!osLoadLevel((LevelID)level)) break;
    }
    if (olLevelLoadFailed) {
        self.messageLabel.text = @"One or more PKD files are invalid or incomplete.";
        self.messageLabel.hidden = NO;
        return;
    }
    gLevelID = LVL_TR1_TITLE;
    sndInit();
    gameInit();
    if (olLevelLoadFailed) {
        gameFree();
        self.messageLabel.text = @"TITLE.PKD is invalid or incomplete.";
        self.messageLabel.hidden = NO;
        return;
    }
    if (!olAudioStart())
        self.controllerLabel.text = @"AUDIO UNAVAILABLE · TOUCH READY";
    self.messageLabel.hidden = YES;
    self.engineRunning = YES;
    self.runFrames = self.fpsWindowFrames = self.slowEngineFrames = 0;
    self.worstEngineSeconds = 0;
    self.runStarted = self.fpsWindowStarted = CACurrentMediaTime();
    self.displayLink = [CADisplayLink displayLinkWithTarget:self
                                                   selector:@selector(gameTick:)];
    self.displayLink.frameInterval = 2;
    [self.displayLink addToRunLoop:[NSRunLoop mainRunLoop]
                           forMode:NSRunLoopCommonModes];
}

- (void)stopEngine {
    [self.displayLink invalidate];
    self.displayLink = nil;
    if (!self.engineRunning) return;
    [self writeDiagnostics:@"stop"];
    self.engineRunning = NO;
    keys = 0;
    sndStop();
    olAudioStop();
    gameFree();
}

- (void)pauseGame {
    self.appActive = NO;
    [self writeDiagnostics:@"background"];
    self.pausedAt = CACurrentMediaTime();
    self.displayLink.paused = YES;
    if (olAudioQueue) AudioQueuePause(olAudioQueue);
}

- (void)resumeGame {
    self.appActive = YES;
    [self.inputView becomeFirstResponder];
    if (!self.engineRunning) [self startEngineIfPossible];
    NSTimeInterval now = CACurrentMediaTime();
    if (self.runStarted > 0 && self.pausedAt > 0)
        self.runStarted += now - self.pausedAt;
    self.pausedAt = 0;
    self.fpsWindowFrames = 0;
    self.fpsWindowStarted = now;
    self.displayLink.paused = NO;
    if (olAudioQueue) AudioQueueStart(olAudioQueue, NULL);
}

- (void)gameTick:(CADisplayLink *)link {
    (void)link;
    if (!self.engineRunning || olLevelLoadFailed) return;
    NSTimeInterval tickStarted = CACurrentMediaTime();
    keys = self.inputKeys;
    gameUpdate(1);
    olAudioSubmit();
    gameRender();

    const uint8 *source = (const uint8 *)fb;
    for (int i = 0; i < FRAME_WIDTH * FRAME_HEIGHT; ++i)
        self.screenView.rgbaFrame[i] = olPalette[source[i]];
    [self.screenView setNeedsDisplay];

    NSTimeInterval now = CACurrentMediaTime();
    NSTimeInterval engineSeconds = now - tickStarted;
    self.runFrames++;
    self.fpsWindowFrames++;
    if (engineSeconds > 1.0 / 30.0) self.slowEngineFrames++;
    if (engineSeconds > self.worstEngineSeconds)
        self.worstEngineSeconds = engineSeconds;
    if (now - self.fpsWindowStarted >= 1.0) {
        double measuredFPS = self.fpsWindowFrames /
                             (now - self.fpsWindowStarted);
        self.controllerLabel.text = [NSString stringWithFormat:@"%@ · %.1f FPS",
            self.icadeActive ? @"iCADE" : @"TOUCH", measuredFPS];
        self.fpsWindowFrames = 0;
        self.fpsWindowStarted = now;
    }
}

- (void)writeDiagnostics:(NSString *)reason {
    if (!self.engineRunning || !olDataRoot || self.runStarted <= 0) return;
    NSTimeInterval seconds = CACurrentMediaTime() - self.runStarted;
    double measuredFPS = seconds > 0 ? self.runFrames / seconds : 0;
    NSString *report = [NSString stringWithFormat:
        @"reason=%@\nframes=%lu\nseconds=%.3f\nfps=%.2f\n"
         "slow_engine_frames=%lu\nworst_engine_ms=%.3f\n"
         "audio_underruns=%lu\ncontroller=%@\n",
        reason, (unsigned long)self.runFrames, seconds, measuredFPS,
        (unsigned long)self.slowEngineFrames,
        self.worstEngineSeconds * 1000.0, olAudioUnderruns,
        self.icadeActive ? @"icade" : @"touch"];
    [report writeToFile:olPath(@"last-run.log") atomically:YES
               encoding:NSUTF8StringEncoding error:NULL];
}

- (void)inputView:(OLInputView *)view changedKeys:(uint32)newKeys {
    (void)view;
    self.inputKeys = newKeys;
}

- (void)inputViewSawICade:(OLInputView *)view {
    (void)view;
    self.icadeActive = YES;
    self.controllerLabel.text = @"iCADE BLUETOOTH ACTIVE";
}

@end
