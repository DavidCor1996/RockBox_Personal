#import <AVFoundation/AVFoundation.h>
@class RPRelayService;

@interface RPIPodAudioStreamer : NSObject
- (instancetype)initWithRelay:(RPRelayService *)relay
                          path:(NSString *)path;
- (AVPlayerItem *)playerItem;
- (void)invalidate;
@end
