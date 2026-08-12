#import "RPVideoConverter.h"
#import "RPMPEGEncoder.h"
#import <AVFoundation/AVFoundation.h>
#import <UIKit/UIKit.h>

static const NSInteger RPVideoWidth = 320;
static const NSInteger RPVideoHeight = 240;
static const NSInteger RPVideoFPS = 20;
static const NSInteger RPAudioRate = 44100;

@implementation RPVideoConversion
@end

static NSString *RPSafeCell(NSString *value)
{
    if (!value.length)
        return @"";
    NSCharacterSet *bad = [NSCharacterSet characterSetWithCharactersInString:@"\t\r\n"];
    NSString *safe = [[value componentsSeparatedByCharactersInSet:bad]
                      componentsJoinedByString:@" "];
    return [safe substringToIndex:MIN(safe.length, 180)];
}

static NSString *RPSafeCellLimit(NSString *value, NSUInteger limit)
{
    NSString *safe = RPSafeCell(value);
    return [safe substringToIndex:MIN(safe.length, limit)];
}

static NSString *RPSafeFilename(NSString *value)
{
    NSCharacterSet *bad = [NSCharacterSet characterSetWithCharactersInString:@"<>:\"/\\|?*\t\r\n"];
    NSString *safe = [[value componentsSeparatedByCharactersInSet:bad]
                      componentsJoinedByString:@"_"];
    return safe.length ? [safe substringToIndex:MIN(safe.length, 100)] : @"Phone Video";
}

static void RPPut16(uint8_t *p, uint16_t value)
{
    p[0] = value & 255; p[1] = value >> 8;
}

static void RPPut32(uint8_t *p, uint32_t value)
{
    p[0] = value & 255; p[1] = value >> 8; p[2] = value >> 16; p[3] = value >> 24;
}

static NSData *RPBitmapData(UIImage *source, NSInteger width, NSInteger height)
{
    size_t row = (size_t)((width * 3 + 3) & ~3);
    NSMutableData *pixels = [NSMutableData dataWithLength:row * height];
    CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
    NSMutableData *rgba = [NSMutableData dataWithLength:width * height * 4];
    CGContextRef context = CGBitmapContextCreate(rgba.mutableBytes, width, height, 8,
        width * 4, space, kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
    CGColorSpaceRelease(space);
    if (!context)
        return nil;
    CGContextSetRGBFillColor(context, 0, 0, 0, 1);
    CGContextFillRect(context, CGRectMake(0, 0, width, height));
    CGSize size = source.size;
    CGFloat scale = MIN((CGFloat)width / MAX(size.width, 1),
                        (CGFloat)height / MAX(size.height, 1));
    CGRect target = CGRectMake((width - size.width * scale) / 2,
                               (height - size.height * scale) / 2,
                               size.width * scale, size.height * scale);
    if (source.CGImage)
        CGContextDrawImage(context, target, source.CGImage);
    CGContextRelease(context);
    uint8_t *rgb = rgba.mutableBytes;
    uint8_t *bgr = pixels.mutableBytes;
    for (NSInteger y = 0; y < height; y++)
    {
        uint8_t *destination = bgr + (height - 1 - y) * row;
        for (NSInteger x = 0; x < width; x++)
        {
            uint8_t *pixel = rgb + (y * width + x) * 4;
            destination[x * 3] = pixel[2];
            destination[x * 3 + 1] = pixel[1];
            destination[x * 3 + 2] = pixel[0];
        }
    }
    NSMutableData *bmp = [NSMutableData dataWithLength:54];
    uint8_t *header = bmp.mutableBytes;
    header[0] = 'B'; header[1] = 'M';
    RPPut32(header + 2, (uint32_t)(54 + pixels.length));
    RPPut32(header + 10, 54); RPPut32(header + 14, 40);
    RPPut32(header + 18, (uint32_t)width); RPPut32(header + 22, (uint32_t)height);
    RPPut16(header + 26, 1); RPPut16(header + 28, 24);
    RPPut32(header + 34, (uint32_t)pixels.length);
    [bmp appendData:pixels];
    return bmp;
}

@implementation RPVideoConverter

- (void)convertURL:(NSURL *)url
        completion:(void (^)(RPVideoConversion *, NSString *))completion
{
    [self convertURL:url metadata:@{} completion:completion];
}

- (void)convertURL:(NSURL *)url metadata:(NSDictionary *)metadata
        completion:(void (^)(RPVideoConversion *, NSString *))completion
{
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        @autoreleasepool {
            BOOL securityScoped = [url startAccessingSecurityScopedResource];
            NSString *error = nil;
            RPVideoConversion *result = [self convertSynchronously:url metadata:metadata error:&error];
            if (securityScoped)
                [url stopAccessingSecurityScopedResource];
            dispatch_async(dispatch_get_main_queue(), ^{ completion(result, error); });
        }
    });
}

- (RPVideoConversion *)convertSynchronously:(NSURL *)url metadata:(NSDictionary *)metadata
                                      error:(NSString **)error
{
    AVURLAsset *asset = [AVURLAsset URLAssetWithURL:url options:nil];
    AVAssetTrack *track = [[asset tracksWithMediaType:AVMediaTypeVideo] firstObject];
    if (!track)
    {
        *error = @"The selected file has no readable video track.";
        return nil;
    }
    NSString *fallbackTitle = url.URLByDeletingPathExtension.lastPathComponent;
    if (fallbackTitle.length > 37 && [fallbackTitle characterAtIndex:36] == '-' &&
        [[NSUUID alloc] initWithUUIDString:[fallbackTitle substringToIndex:36]])
        fallbackTitle = [fallbackTitle substringFromIndex:37];
    NSArray<AVMetadataItem *> *titleItems = [AVMetadataItem metadataItemsFromArray:
        asset.commonMetadata withKey:AVMetadataCommonKeyTitle keySpace:AVMetadataKeySpaceCommon];
    NSString *metadataTitle = [titleItems.firstObject.value isKindOfClass:NSString.class] ?
                              (NSString *)titleItems.firstObject.value : nil;
    NSString *matchedTitle = [metadata[@"title"] isKindOfClass:NSString.class] ? metadata[@"title"] : @"";
    NSString *title = RPSafeFilename(matchedTitle.length ? matchedTitle :
                                     (metadataTitle.length ? metadataTitle : fallbackTitle));
    NSString *identifier = [NSUUID.UUID.UUIDString lowercaseString];
    NSString *folderName = [NSString stringWithFormat:@"%@-%@", title,
                            [identifier substringToIndex:8]];
    NSURL *root = [NSFileManager.defaultManager.temporaryDirectory
                   URLByAppendingPathComponent:folderName isDirectory:YES];
    [NSFileManager.defaultManager createDirectoryAtURL:root
                           withIntermediateDirectories:YES attributes:nil error:nil];

    NSError *readerError = nil;
    AVAssetReader *reader = [[AVAssetReader alloc] initWithAsset:asset error:&readerError];
    NSDictionary *videoSettings = @{(id)kCVPixelBufferPixelFormatTypeKey:
                                     @(kCVPixelFormatType_32BGRA)};
    AVAssetReaderVideoCompositionOutput *output =
        [[AVAssetReaderVideoCompositionOutput alloc] initWithVideoTracks:@[track]
                                                           videoSettings:videoSettings];
    AVMutableVideoComposition *composition = [AVMutableVideoComposition videoComposition];
    composition.renderSize = CGSizeMake(RPVideoWidth, RPVideoHeight);
    composition.frameDuration = CMTimeMake(1, RPVideoFPS);
    AVMutableVideoCompositionInstruction *instruction =
        [AVMutableVideoCompositionInstruction videoCompositionInstruction];
    instruction.timeRange = CMTimeRangeMake(kCMTimeZero, asset.duration);
    AVMutableVideoCompositionLayerInstruction *layer =
        [AVMutableVideoCompositionLayerInstruction videoCompositionLayerInstructionWithAssetTrack:track];
    CGRect natural = CGRectApplyAffineTransform(CGRectMake(0, 0,
        track.naturalSize.width, track.naturalSize.height), track.preferredTransform);
    CGFloat width = fabs(natural.size.width), height = fabs(natural.size.height);
    CGFloat scale = MIN((CGFloat)RPVideoWidth / MAX(width, 1),
                        (CGFloat)RPVideoHeight / MAX(height, 1));
    CGAffineTransform transform = track.preferredTransform;
    transform = CGAffineTransformConcat(transform,
        CGAffineTransformMakeTranslation(-natural.origin.x, -natural.origin.y));
    transform = CGAffineTransformConcat(transform, CGAffineTransformMakeScale(scale, scale));
    transform = CGAffineTransformConcat(transform, CGAffineTransformMakeTranslation(
        (RPVideoWidth - width * scale) / 2, (RPVideoHeight - height * scale) / 2));
    [layer setTransform:transform atTime:kCMTimeZero];
    instruction.layerInstructions = @[layer];
    composition.instructions = @[instruction];
    output.videoComposition = composition;
    [reader addOutput:output];

    AVAssetTrack *audioTrack = [[asset tracksWithMediaType:AVMediaTypeAudio] firstObject];
    NSDictionary *audioSettings = @{AVFormatIDKey: @(kAudioFormatLinearPCM),
        AVSampleRateKey: @(RPAudioRate), AVNumberOfChannelsKey: @2,
        AVLinearPCMBitDepthKey: @16, AVLinearPCMIsFloatKey: @NO,
        AVLinearPCMIsBigEndianKey: @NO, AVLinearPCMIsNonInterleaved: @NO};
    AVAssetReaderTrackOutput *audioOutput = audioTrack ?
        [[AVAssetReaderTrackOutput alloc] initWithTrack:audioTrack
                                         outputSettings:audioSettings] : nil;
    if (audioOutput && [reader canAddOutput:audioOutput])
        [reader addOutput:audioOutput];
    else
        audioOutput = nil;
    if (![reader startReading])
    {
        *error = reader.error.localizedDescription ?: @"Video conversion could not start.";
        return nil;
    }

    NSURL *marker = [root URLByAppendingPathComponent:
                     [title stringByAppendingPathExtension:@"mpg"]];
    NSString *mpegError = nil;
    RPMPEGEncoder *encoder = [[RPMPEGEncoder alloc] initWithURL:marker error:&mpegError];
    if (!encoder)
    {
        *error = mpegError ?: @"The MPEG encoder could not start.";
        [NSFileManager.defaultManager removeItemAtURL:root error:nil];
        return nil;
    }

    CMSampleBufferRef videoSample = [output copyNextSampleBuffer];
    NSMutableData *audioPending = [NSMutableData data];
    NSInteger audioFrameSamples = encoder.audioFrameSamples;
    NSUInteger audioFrameBytes = (NSUInteger)audioFrameSamples * 2 * sizeof(int16_t);
    BOOL audioDone = (audioOutput == nil);
    int64_t frames = 0;
    int64_t audioSamples = 0;
    BOOL encodeOK = YES;
    while (videoSample && encodeOK)
    {
        @autoreleasepool {
            double videoTime = (double)frames / RPVideoFPS;
            double audioTime = (double)audioSamples / RPAudioRate;
            if (audioTime <= videoTime)
            {
                while (audioPending.length < audioFrameBytes && !audioDone)
                {
                    CMSampleBufferRef audioSample = [audioOutput copyNextSampleBuffer];
                    if (!audioSample)
                    {
                        audioDone = YES;
                        break;
                    }
                    CMBlockBufferRef block = CMSampleBufferGetDataBuffer(audioSample);
                    size_t length = block ? CMBlockBufferGetDataLength(block) : 0;
                    if (length)
                    {
                        NSUInteger offset = audioPending.length;
                        audioPending.length += length;
                        CMBlockBufferCopyDataBytes(block, 0, length,
                                                   (uint8_t *)audioPending.mutableBytes + offset);
                    }
                    CFRelease(audioSample);
                }
                if (audioPending.length < audioFrameBytes)
                    [audioPending increaseLengthBy:audioFrameBytes - audioPending.length];
                NSData *frameData = [audioPending subdataWithRange:NSMakeRange(0, audioFrameBytes)];
                [audioPending replaceBytesInRange:NSMakeRange(0, audioFrameBytes)
                                        withBytes:NULL length:0];
                encodeOK = [encoder appendAudioPCMData:frameData
                                           sampleIndex:audioSamples error:&mpegError];
                audioSamples += audioFrameSamples;
            }
            else
            {
                encodeOK = [encoder appendVideoPixelBuffer:
                    CMSampleBufferGetImageBuffer(videoSample)
                                               frameIndex:frames error:&mpegError];
                CFRelease(videoSample);
                videoSample = NULL;
                frames++;
                if (encodeOK)
                    videoSample = [output copyNextSampleBuffer];
            }
        }
    }
    if (videoSample)
        CFRelease(videoSample);

    int64_t targetAudioSamples = (frames * RPAudioRate + RPVideoFPS - 1) / RPVideoFPS;
    while (encodeOK && audioSamples < targetAudioSamples)
    {
        while (audioPending.length < audioFrameBytes && !audioDone)
        {
            CMSampleBufferRef audioSample = [audioOutput copyNextSampleBuffer];
            if (!audioSample)
            {
                audioDone = YES;
                break;
            }
            CMBlockBufferRef block = CMSampleBufferGetDataBuffer(audioSample);
            size_t length = block ? CMBlockBufferGetDataLength(block) : 0;
            if (length)
            {
                NSUInteger offset = audioPending.length;
                audioPending.length += length;
                CMBlockBufferCopyDataBytes(block, 0, length,
                                           (uint8_t *)audioPending.mutableBytes + offset);
            }
            CFRelease(audioSample);
        }
        if (audioPending.length < audioFrameBytes)
            [audioPending increaseLengthBy:audioFrameBytes - audioPending.length];
        NSData *frameData = [audioPending subdataWithRange:NSMakeRange(0, audioFrameBytes)];
        [audioPending replaceBytesInRange:NSMakeRange(0, audioFrameBytes)
                                withBytes:NULL length:0];
        encodeOK = [encoder appendAudioPCMData:frameData sampleIndex:audioSamples
                                         error:&mpegError];
        audioSamples += audioFrameSamples;
    }
    if (encodeOK && frames > 0)
        encodeOK = [encoder finishWithError:&mpegError];
    if (!encodeOK || !frames)
    {
        [encoder cancel];
        *error = mpegError ?: (reader.error.localizedDescription ?:
                 @"No video frames could be converted to MPEG.");
        [NSFileManager.defaultManager removeItemAtURL:root error:nil];
        return nil;
    }

    AVAssetImageGenerator *generator = [[AVAssetImageGenerator alloc] initWithAsset:asset];
    generator.appliesPreferredTrackTransform = YES;
    CMTime posterTime = CMTimeMakeWithSeconds(MIN(3.0, MAX(0.0, CMTimeGetSeconds(asset.duration) / 8.0)), 600);
    CGImageRef imageRef = [generator copyCGImageAtTime:posterTime actualTime:NULL error:nil];
    UIImage *poster = imageRef ? [UIImage imageWithCGImage:imageRef] : nil;
    if (imageRef) CGImageRelease(imageRef);
    NSString *artworkURL = [metadata[@"artwork_url"] isKindOfClass:NSString.class] ? metadata[@"artwork_url"] : @"";
    NSData *artworkData = artworkURL.length ? [NSData dataWithContentsOfURL:[NSURL URLWithString:artworkURL]] : nil;
    if (!artworkData.length)
    {
        NSArray *embeddedItems = [AVMetadataItem metadataItemsFromArray:asset.commonMetadata
            withKey:AVMetadataCommonKeyArtwork keySpace:AVMetadataKeySpaceCommon];
        AVMetadataItem *embeddedItem = embeddedItems.firstObject;
        id value = embeddedItem.value;
        if ([value isKindOfClass:NSData.class]) artworkData = value;
        else if ([value isKindOfClass:NSDictionary.class])
            artworkData = value[@"data"] ?: value[@"value"];
    }
    if (artworkData.length && [UIImage imageWithData:artworkData])
        poster = [UIImage imageWithData:artworkData];
    NSURL *thumb = [root URLByAppendingPathComponent:@"poster.bmp"];
    [RPBitmapData(poster ?: [UIImage new], 174, 130) writeToURL:thumb atomically:YES];

    NSString *kind = [metadata[@"kind"] isKindOfClass:NSString.class] ? metadata[@"kind"] : @"movie";
    NSString *rawShow = [metadata[@"show"] isKindOfClass:NSString.class] ? metadata[@"show"] : @"";
    NSString *show = rawShow.length ? RPSafeFilename(rawShow) : @"";
    NSInteger season = [metadata[@"season"] integerValue];
    NSInteger episode = [metadata[@"episode"] integerValue];
    NSString *category = [kind isEqualToString:@"show"] ? @"TV Shows" :
        ([kind isEqualToString:@"home_video"] ? @"Home Videos" : @"Movies");
    NSString *base = [NSString stringWithFormat:@"/Videos/%@", category];
    if ([kind isEqualToString:@"show"] && show.length)
        base = [base stringByAppendingPathComponent:[show stringByAppendingPathComponent:
            season ? [NSString stringWithFormat:@"Season %02ld", (long)season] : @"Specials"]];
    NSString *episodeLabel = ([kind isEqualToString:@"show"] && episode > 0) ?
        [NSString stringWithFormat:@"S%02ldE%02ld - %@", (long)season, (long)episode, title] : title;
    NSString *deviceFolder = [base stringByAppendingPathComponent:
        [NSString stringWithFormat:@"%@-%@", RPSafeFilename(episodeLabel), [identifier substringToIndex:8]]];
    NSString *thumbName = [NSString stringWithFormat:@"%@.bmp", identifier];
    NSString *relativeMarker = [[deviceFolder substringFromIndex:1]
                                stringByAppendingPathComponent:marker.lastPathComponent];
    NSInteger duration = (NSInteger)llround(CMTimeGetSeconds(asset.duration));
    NSArray<AVMetadataItem *> *descriptionItems = [AVMetadataItem metadataItemsFromArray:
        asset.commonMetadata withKey:AVMetadataCommonKeyDescription keySpace:AVMetadataKeySpaceCommon];
    NSString *summary = [metadata[@"summary"] isKindOfClass:NSString.class] && [metadata[@"summary"] length] ?
                        RPSafeCell(metadata[@"summary"]) :
                        ([descriptionItems.firstObject.value isKindOfClass:NSString.class] ?
                        RPSafeCell((NSString *)descriptionItems.firstObject.value) :
                        @"Synced from iPhone");
    NSString *shortSummary = RPSafeCellLimit(summary, 72);
    NSString *longSummary = RPSafeCellLimit(summary, 140);
    NSArray<AVMetadataItem *> *typeItems = [AVMetadataItem metadataItemsFromArray:
        asset.commonMetadata withKey:AVMetadataCommonKeyType keySpace:AVMetadataKeySpaceCommon];
    NSString *genre = [metadata[@"genre"] isKindOfClass:NSString.class] && [metadata[@"genre"] length] ?
                      RPSafeCell(metadata[@"genre"]) :
                      ([typeItems.firstObject.value isKindOfClass:NSString.class] ?
                       RPSafeCell((NSString *)typeItems.firstObject.value) : @"");
    NSString *groupKey = [kind isEqualToString:@"show"] ?
        [NSString stringWithFormat:@"show:%@", show.lowercaseString] :
        ([kind isEqualToString:@"home_video"] ? @"home_video" :
         [NSString stringWithFormat:@"movie:%@", title.lowercaseString]);
    NSArray *columns = @[identifier,
        [@"thumbs" stringByAppendingPathComponent:thumbName],
        [@"previews" stringByAppendingPathComponent:thumbName],
        RPSafeCellLimit(title, 100), kind, RPSafeCellLimit(groupKey, 120), relativeMarker,
        RPSafeCellLimit(show, 100), [NSString stringWithFormat:@"%ld", (long)season],
        [NSString stringWithFormat:@"%ld", (long)episode], [NSString stringWithFormat:@"%ld", (long)MAX(duration, 1)],
        @"0", [NSString stringWithFormat:@"%@", metadata[@"year"] ?: @""],
        RPSafeCellLimit(genre, 80), @"", shortSummary,
        longSummary, @"", @"", @"", @"", @"", @""];
    NSURL *manifest = [root URLByAppendingPathComponent:@"video-row.tsv"];
    NSString *row = [columns componentsJoinedByString:@"\t"];
    NSError *manifestError = nil;
    if ([row lengthOfBytesUsingEncoding:NSUTF8StringEncoding] >= 1023 ||
        ![row writeToURL:manifest atomically:YES
                 encoding:NSUTF8StringEncoding error:&manifestError])
    {
        *error = manifestError.localizedDescription ?:
            @"The Netflix/Videos catalog row is too large.";
        [NSFileManager.defaultManager removeItemAtURL:root error:nil];
        return nil;
    }

    NSMutableArray<NSURL *> *files = [NSMutableArray arrayWithObject:marker];
    NSMutableArray<NSString *> *destinations = [NSMutableArray arrayWithObject:
        [deviceFolder stringByAppendingPathComponent:marker.lastPathComponent]];
    [files addObject:thumb];
    [destinations addObject:[@"/.rockbox/videolist/thumbs" stringByAppendingPathComponent:thumbName]];
    [files addObject:thumb];
    [destinations addObject:[@"/.rockbox/videolist/previews" stringByAppendingPathComponent:thumbName]];
    [files addObject:manifest];
    [destinations addObject:@"/.rockbox/rockpod/phone/video-row.tsv"];
    RPVideoConversion *conversion = [[RPVideoConversion alloc] init];
    conversion.title = title; conversion.identifier = identifier;
    conversion.files = files; conversion.destinations = destinations;
    return conversion;
}

@end
