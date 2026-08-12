#import "RPIPodAudioStreamer.h"
#import "RPRelayService.h"

@interface RPIPodAudioStreamer () <AVAssetResourceLoaderDelegate>
@property(nonatomic, strong) RPRelayService *relay;
@property(nonatomic, copy) NSString *path;
@property(nonatomic, strong) AVURLAsset *asset;
@property(nonatomic) dispatch_queue_t loaderQueue;
@property(atomic) BOOL invalidated;
@end

@implementation RPIPodAudioStreamer

- (instancetype)initWithRelay:(RPRelayService *)relay path:(NSString *)path
{
    if ((self = [super init]))
    {
        self.relay = relay;
        self.path = path;
        self.loaderQueue = dispatch_queue_create("com.rockpod.link.ipod-stream",
                                                  DISPATCH_QUEUE_SERIAL);
    }
    return self;
}

- (NSString *)contentType
{
    NSString *extension = self.path.pathExtension.lowercaseString;
    if ([extension isEqualToString:@"mp3"]) return @"public.mp3";
    if ([extension isEqualToString:@"m4a"] || [extension isEqualToString:@"aac"])
        return @"public.mpeg-4-audio";
    if ([extension isEqualToString:@"wav"]) return @"com.microsoft.waveform-audio";
    if ([extension isEqualToString:@"aif"] || [extension isEqualToString:@"aiff"])
        return @"public.aiff-audio";
    if ([extension isEqualToString:@"flac"]) return @"org.xiph.flac";
    if ([extension isEqualToString:@"ogg"] || [extension isEqualToString:@"opus"])
        return @"org.xiph.ogg-audio";
    return @"public.audio";
}

- (AVPlayerItem *)playerItem
{
    NSString *extension = self.path.pathExtension.length ?
        self.path.pathExtension : @"audio";
    NSString *name = [[NSUUID UUID].UUIDString
        stringByAppendingPathExtension:extension];
    NSURL *URL = [NSURL URLWithString:[@"rockpod-stream://ipod/"
        stringByAppendingString:name]];
    self.asset = [AVURLAsset URLAssetWithURL:URL options:nil];
    [self.asset.resourceLoader setDelegate:self queue:self.loaderQueue];
    return [AVPlayerItem playerItemWithAsset:self.asset];
}

- (void)finishRequest:(AVAssetResourceLoadingRequest *)loadingRequest
                 error:(NSString *)message
{
    NSError *error = [NSError errorWithDomain:@"RockPodStream" code:1
        userInfo:@{NSLocalizedDescriptionKey:
            message.length ? message : @"The iPod stream stopped."}];
    [loadingRequest finishLoadingWithError:error];
}

- (void)pumpRequest:(AVAssetResourceLoadingRequest *)loadingRequest
{
    if (self.invalidated || loadingRequest.isCancelled)
        return;
    AVAssetResourceLoadingDataRequest *dataRequest = loadingRequest.dataRequest;
    if (!dataRequest)
    {
        [self finishRequest:loadingRequest error:@"Invalid audio range request"];
        return;
    }
    int64_t offset = dataRequest.currentOffset;
    if (!offset)
        offset = dataRequest.requestedOffset;
    int64_t requestedEnd = dataRequest.requestedOffset + dataRequest.requestedLength;
    if (!dataRequest.requestsAllDataToEndOfResource && offset >= requestedEnd)
    {
        [loadingRequest finishLoading];
        return;
    }
    NSUInteger amount = 64 * 1024;
    if (!dataRequest.requestsAllDataToEndOfResource)
        amount = MIN(amount, (NSUInteger)MAX((int64_t)0, requestedEnd - offset));
    if (!amount)
    {
        [loadingRequest finishLoading];
        return;
    }

    __weak typeof(self) weakSelf = self;
    dispatch_queue_t loaderQueue = self.loaderQueue;
    [self.relay requestMediaDataAtPath:self.path offset:(uint64_t)offset
        length:amount completion:^(NSData *data, uint64_t totalLength,
                                   NSString *errorMessage) {
        dispatch_async(loaderQueue, ^{
            typeof(self) self = weakSelf;
            if (!self || self.invalidated || loadingRequest.isCancelled)
                return;
            if (!data.length || !totalLength)
            {
                [self finishRequest:loadingRequest error:errorMessage];
                return;
            }
            AVAssetResourceLoadingContentInformationRequest *information =
                loadingRequest.contentInformationRequest;
            if (information)
            {
                information.contentType = self.contentType;
                information.contentLength = (int64_t)totalLength;
                information.byteRangeAccessSupported = YES;
            }
            [dataRequest respondWithData:data];
            int64_t next = dataRequest.currentOffset;
            if ((uint64_t)next >= totalLength ||
                (!dataRequest.requestsAllDataToEndOfResource &&
                 next >= requestedEnd))
                [loadingRequest finishLoading];
            else
                [self pumpRequest:loadingRequest];
        });
    }];
}

- (BOOL)resourceLoader:(AVAssetResourceLoader *)resourceLoader
    shouldWaitForLoadingOfRequestedResource:
        (AVAssetResourceLoadingRequest *)loadingRequest
{
    (void)resourceLoader;
    [self pumpRequest:loadingRequest];
    return YES;
}

- (void)resourceLoader:(AVAssetResourceLoader *)resourceLoader
    didCancelLoadingRequest:(AVAssetResourceLoadingRequest *)loadingRequest
{
    (void)resourceLoader;
    (void)loadingRequest;
}

- (void)invalidate
{
    self.invalidated = YES;
    [self.asset cancelLoading];
}

@end
