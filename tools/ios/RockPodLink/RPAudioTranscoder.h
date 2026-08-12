#import <Foundation/Foundation.h>

@interface RPAudioTranscodeResult : NSObject
@property(nonatomic, strong) NSURL *fileURL;
@property(nonatomic) NSInteger bitrateKbps;
@property(nonatomic) BOOL copiedOriginal;
@end

@interface RPAudioTranscoder : NSObject
- (void)prepareAudioAtURL:(NSURL *)source
             targetKbps:(NSInteger)targetKbps
          keepOriginal:(BOOL)keepOriginal
             completion:(void (^)(RPAudioTranscodeResult *result,
                                  NSString *errorMessage))completion;
@end
