#import <Foundation/Foundation.h>
#import <CoreVideo/CoreVideo.h>

NS_ASSUME_NONNULL_BEGIN

@interface RPMPEGEncoder : NSObject

@property(nonatomic, readonly) NSInteger audioFrameSamples;

- (nullable instancetype)initWithURL:(NSURL *)url
                                error:(NSString * _Nullable * _Nullable)error;
- (BOOL)appendVideoPixelBuffer:(CVPixelBufferRef)pixelBuffer
                     frameIndex:(int64_t)frameIndex
                          error:(NSString * _Nullable * _Nullable)error;
- (BOOL)appendAudioPCMData:(NSData *)pcm
                sampleIndex:(int64_t)sampleIndex
                      error:(NSString * _Nullable * _Nullable)error;
- (BOOL)finishWithError:(NSString * _Nullable * _Nullable)error;
- (void)cancel;

@end

NS_ASSUME_NONNULL_END
