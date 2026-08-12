#import "RPAudioTranscoder.h"
#import <AVFoundation/AVFoundation.h>
#import <AudioToolbox/AudioToolbox.h>

@implementation RPAudioTranscodeResult
@end

@implementation RPAudioTranscoder

- (void)finish:(RPAudioTranscodeResult *)result error:(NSString *)error
     completion:(void (^)(RPAudioTranscodeResult *, NSString *))completion
{
    dispatch_async(dispatch_get_main_queue(), ^{ completion(result, error); });
}

- (BOOL)isRockboxCompatibleExtension:(NSString *)extension
{
    return [@[@"mp3", @"m4a", @"aac", @"flac", @"ogg", @"opus", @"wav",
              @"aiff", @"aif", @"wma", @"wv"] containsObject:extension.lowercaseString];
}

- (void)prepareAudioAtURL:(NSURL *)source
             targetKbps:(NSInteger)targetKbps
          keepOriginal:(BOOL)keepOriginal
             completion:(void (^)(RPAudioTranscodeResult *, NSString *))completion
{
    if (!source.isFileURL)
    {
        [self finish:nil error:@"The cached track is not a local file." completion:completion];
        return;
    }
    AVURLAsset *asset = [AVURLAsset URLAssetWithURL:source options:nil];
    AVAssetTrack *track = [asset tracksWithMediaType:AVMediaTypeAudio].firstObject;
    if (!track)
    {
        [self finish:nil error:@"The download contains no readable audio track." completion:completion];
        return;
    }
    NSInteger sourceKbps = track.estimatedDataRate > 0 ?
        (NSInteger)llround(track.estimatedDataRate / 1000.0) : 0;
    if (keepOriginal && [self isRockboxCompatibleExtension:source.pathExtension] &&
        (!sourceKbps || sourceKbps <= targetKbps + 24))
    {
        RPAudioTranscodeResult *result = [[RPAudioTranscodeResult alloc] init];
        result.fileURL = source;
        result.bitrateKbps = sourceKbps;
        result.copiedOriginal = YES;
        [self finish:result error:nil completion:completion];
        return;
    }

    NSURL *output = [NSFileManager.defaultManager.temporaryDirectory
        URLByAppendingPathComponent:[NSString stringWithFormat:@"rockpod-audio-%@.m4a",
                                     NSUUID.UUID.UUIDString]];
    NSError *error = nil;
    AVAssetReader *reader = [[AVAssetReader alloc] initWithAsset:asset error:&error];
    AVAssetWriter *writer = [[AVAssetWriter alloc] initWithURL:output
                                                      fileType:AVFileTypeAppleM4A error:&error];
    if (!reader || !writer || error)
    {
        [self finish:nil error:error.localizedDescription ?: @"Could not create the audio converter."
           completion:completion];
        return;
    }
    NSDictionary *pcm = @{ AVFormatIDKey: @(kAudioFormatLinearPCM),
        AVLinearPCMIsFloatKey: @NO, AVLinearPCMBitDepthKey: @16,
        AVLinearPCMIsNonInterleaved: @NO };
    AVAssetReaderTrackOutput *readerOutput = [[AVAssetReaderTrackOutput alloc]
        initWithTrack:track outputSettings:pcm];
    Float64 detectedSampleRate = 44100;
    CMAudioFormatDescriptionRef format = (__bridge CMAudioFormatDescriptionRef)track.formatDescriptions.firstObject;
    const AudioStreamBasicDescription *stream = format ?
        CMAudioFormatDescriptionGetStreamBasicDescription(format) : NULL;
    if (stream && stream->mSampleRate > 0) detectedSampleRate = stream->mSampleRate;
    NSInteger sampleRate = MIN(48000, MAX(22050, (NSInteger)llround(detectedSampleRate)));
    AudioChannelLayout layout;
    memset(&layout, 0, sizeof(layout));
    layout.mChannelLayoutTag = kAudioChannelLayoutTag_Stereo;
    NSDictionary *aac = @{ AVFormatIDKey: @(kAudioFormatMPEG4AAC),
        AVSampleRateKey: @(sampleRate), AVNumberOfChannelsKey: @2,
        AVEncoderBitRateKey: @(MAX(64, MIN(320, targetKbps)) * 1000),
        AVChannelLayoutKey: [NSData dataWithBytes:&layout length:sizeof(layout)] };
    AVAssetWriterInput *writerInput = [[AVAssetWriterInput alloc]
        initWithMediaType:AVMediaTypeAudio outputSettings:aac];
    writerInput.expectsMediaDataInRealTime = NO;
    if (![reader canAddOutput:readerOutput] || ![writer canAddInput:writerInput])
    {
        [self finish:nil error:@"This audio format cannot be converted on this iPhone."
           completion:completion];
        return;
    }
    [reader addOutput:readerOutput]; [writer addInput:writerInput];
    writer.metadata = asset.commonMetadata;
    if (![writer startWriting] || ![reader startReading])
    {
        [self finish:nil error:writer.error.localizedDescription ?: reader.error.localizedDescription
           completion:completion];
        return;
    }
    [writer startSessionAtSourceTime:kCMTimeZero];
    dispatch_queue_t queue = dispatch_queue_create("com.rockpod.audio.transcode", DISPATCH_QUEUE_SERIAL);
    __block BOOL finished = NO;
    [writerInput requestMediaDataWhenReadyOnQueue:queue usingBlock:^{
        if (finished) return;
        while (writerInput.readyForMoreMediaData)
        {
            CMSampleBufferRef sample = [readerOutput copyNextSampleBuffer];
            if (sample)
            {
                BOOL appended = [writerInput appendSampleBuffer:sample];
                CFRelease(sample);
                if (!appended)
                {
                    finished = YES;
                    [reader cancelReading]; [writerInput markAsFinished];
                    [writer cancelWriting];
                    [self finish:nil error:writer.error.localizedDescription ?: @"Audio conversion failed."
                       completion:completion];
                    return;
                }
            }
            else
            {
                finished = YES;
                [writerInput markAsFinished];
                [writer finishWritingWithCompletionHandler:^{
                    AVURLAsset *check = [AVURLAsset URLAssetWithURL:output options:nil];
                    if (writer.status != AVAssetWriterStatusCompleted ||
                        ![check tracksWithMediaType:AVMediaTypeAudio].count)
                    {
                        [self finish:nil error:writer.error.localizedDescription ?: @"Converted audio verification failed."
                           completion:completion];
                        return;
                    }
                    RPAudioTranscodeResult *result = [[RPAudioTranscodeResult alloc] init];
                    result.fileURL = output; result.bitrateKbps = targetKbps;
                    [self finish:result error:nil completion:completion];
                }];
                return;
            }
        }
    }];
}

@end
