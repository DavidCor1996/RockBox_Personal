#import "RPRelayService.h"
#import <UIKit/UIKit.h>
#import <WebKit/WebKit.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#define RP_WEATHER_PORT 47700
#define RP_BROWSER_PORT 47702
#define RP_SYNC_PORT 47703
#define RP_CHUNK 1360
#define RP_SYNC_CHUNK 1360
#define RP_IP "10.77.0.2"
/* USB_INTERNET_SYNC_STATUS in apps/usb_internet.c */
#define RP_SYNC_STATUS 1
/* Bounds how much unsolicited status traffic may be absorbed inside one
 * chunk request before the retry budget is charged normally again. */
#define RP_SYNC_ABSORB_MAX 16

@interface RPTransfer : NSObject
@property(nonatomic, strong) NSURL *url;
@property(nonatomic, copy) NSString *destination;
@property(nonatomic, copy) void (^completion)(BOOL, NSString *);
@property(nonatomic) BOOL securityScoped;
@end

@implementation RPTransfer
@end

@interface RPFileRequest : NSObject
@property(nonatomic, copy) NSString *path;
@property(nonatomic, copy) void (^completion)(NSURL *, NSString *);
@end
@implementation RPFileRequest
@end

@interface RPStreamDataRequest : NSObject
@property(nonatomic, copy) NSString *path;
@property(nonatomic) uint64_t offset;
@property(nonatomic) NSUInteger length;
@property(nonatomic, copy) void (^completion)(NSData *, uint64_t, NSString *);
@end
@implementation RPStreamDataRequest
@end

static NSString *rp_weather_name(NSInteger code, NSString **icon)
{
    if (code == 0) { *icon = @"clear"; return @"Clear"; }
    if (code == 1 || code == 2)
    { *icon = @"partly_cloudy"; return @"Partly Cloudy"; }
    if (code == 3) { *icon = @"cloudy"; return @"Cloudy"; }
    if (code == 45 || code == 48) { *icon = @"fog"; return @"Fog"; }
    if ((code >= 51 && code <= 57))
    { *icon = @"drizzle"; return @"Drizzle"; }
    if ((code >= 61 && code <= 67) || (code >= 80 && code <= 82))
    { *icon = @"rain"; return @"Rain"; }
    if ((code >= 71 && code <= 77) || code == 85 || code == 86)
    { *icon = @"snow"; return @"Snow"; }
    if (code >= 95) { *icon = @"thunderstorm"; return @"Thunderstorm"; }
    *icon = @"cloudy";
    return @"Cloudy";
}

static NSString *rp_compass(double degrees)
{
    static NSArray<NSString *> *names;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        names = @[@"N", @"NE", @"E", @"SE", @"S", @"SW", @"W", @"NW"];
    });
    NSInteger index = ((NSInteger)((degrees + 22.5) / 45.0)) % 8;
    return names[index < 0 ? index + 8 : index];
}

static NSString *rp_tail_time(NSString *stamp)
{
    return stamp.length >= 5 ? [stamp substringFromIndex:stamp.length - 5] : stamp;
}

static void rp_put_be32(unsigned char *p, uint32_t value)
{
    uint32_t encoded = htonl(value);
    memcpy(p, &encoded, sizeof(encoded));
}

static uint32_t rp_get_be32(const unsigned char *p)
{
    uint32_t encoded;
    memcpy(&encoded, p, sizeof(encoded));
    return ntohl(encoded);
}

static NSData *rp_viewport_from_image(UIImage *image)
{
    return image ? UIImageJPEGRepresentation(image, 0.62) : nil;
}

@interface RPRelayService () <WKNavigationDelegate>
@property(nonatomic, strong) WKWebView *webView;
@property(nonatomic, strong) NSThread *thread;
@property(atomic) BOOL running;
@property(atomic) BOOL pageBusy;
@property(nonatomic) uint32_t pendingRequest;
@property(nonatomic, strong) NSData *readyPayload;
@property(nonatomic) uint32_t readyRequest;
@property(atomic) BOOL weatherBusy;
@property(atomic) BOOL navigationPending;
@property(nonatomic) uint32_t captureIssuedRequest;
@property(atomic, readwrite, getter=isConnected) BOOL connected;
@property(nonatomic, strong) NSMutableArray<RPTransfer *> *transfers;
@property(nonatomic, strong) NSMutableArray<RPFileRequest *> *fileRequests;
@property(nonatomic, strong) NSMutableArray<RPStreamDataRequest *> *streamRequests;
@property(atomic) BOOL sitekickRequested;
@property(atomic) BOOL libraryRequested;
@property(atomic) BOOL safeDisconnectRequested;
@property(nonatomic, copy) void (^safeDisconnectCompletion)(BOOL, NSString *);
@property(nonatomic) uint32_t syncSequence;
@property(atomic) NSTimeInterval lastDeviceResponse;
@property(atomic) int catalogRetries;
@end

@implementation RPRelayService

+ (instancetype)sharedService
{
    static RPRelayService *service;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        service = [[RPRelayService alloc] init];
        service.transfers = [NSMutableArray array];
        service.fileRequests = [NSMutableArray array];
        service.streamRequests = [NSMutableArray array];
    });
    return service;
}

- (void)requestMediaDataAtPath:(NSString *)path
                        offset:(uint64_t)offset
                        length:(NSUInteger)length
                    completion:(void (^)(NSData *, uint64_t, NSString *))completion
{
    if (![path hasPrefix:@"/"] || [path containsString:@".."] ||
        !length || offset > UINT32_MAX)
    {
        completion(nil, 0, @"Invalid iPod stream request");
        return;
    }
    RPStreamDataRequest *request = [[RPStreamDataRequest alloc] init];
    request.path = path;
    request.offset = offset;
    request.length = MIN(length, (NSUInteger)(256 * 1024));
    request.completion = completion;
    @synchronized(self) { [self.streamRequests addObject:request]; }
}

- (void)report:(NSString *)status connected:(BOOL)connected
{
    self.connected = connected;
    dispatch_async(dispatch_get_main_queue(), ^{
        if (self.statusHandler)
            self.statusHandler(status, connected);
    });
}

/* Any reply from the iPod proves the link is alive, whichever helper happens
 * to be holding the sync socket at the time.  Bulk sync, media streaming and
 * catalog reads all run as blocking branches above the relay loop's select,
 * so without this the liveness clock only advanced between transfers and a
 * photo sync longer than the watchdog window looked like a disconnect. */
- (void)noteDeviceResponse
{
    self.lastDeviceResponse = [NSDate timeIntervalSinceReferenceDate];
    if (!self.connected)
        [self report:@"iPod USB Link ready" connected:YES];
}

/* The sync port multiplexes acknowledgements, bulk data and the two-second
 * status reply.  Record liveness for every well-formed reply, and route a
 * status packet to the device parser rather than dropping it.  Returns YES
 * when the caller consumed status traffic, which must not be charged against
 * a transfer's retry budget. */
- (BOOL)absorbSyncPacket:(const unsigned char *)bytes length:(ssize_t)length
{
    if (length < 5 || memcmp(bytes, "RPS1", 4))
        return NO;
    [self noteDeviceResponse];
    if (bytes[4] != RP_SYNC_STATUS)
        return NO;
    [self parseDeviceStatus:bytes + 5 length:(NSUInteger)length - 5];
    return YES;
}

- (void)enqueueFileAtURL:(NSURL *)url
             destination:(NSString *)destination
              completion:(void (^)(BOOL, NSString *))completion
{
    if (!url.isFileURL || destination.length == 0)
    {
        if (completion)
            completion(NO, @"Invalid sync item");
        return;
    }
    RPTransfer *transfer = [[RPTransfer alloc] init];
    transfer.url = url;
    transfer.destination = destination;
    transfer.completion = completion;
    transfer.securityScoped = [url startAccessingSecurityScopedResource];
    @synchronized(self)
    {
        [self.transfers addObject:transfer];
    }
}

- (void)requestSitekickPreview
{
    self.sitekickRequested = YES;
}

- (void)requestLibraryCatalog
{
    self.libraryRequested = YES;
}

- (void)requestMediaFileAtPath:(NSString *)path
                    completion:(void (^)(NSURL *, NSString *))completion
{
    if (![path hasPrefix:@"/"] || [path containsString:@".."])
    {
        completion(nil, @"Invalid iPod media path");
        return;
    }
    RPFileRequest *request = [[RPFileRequest alloc] init];
    request.path = path; request.completion = completion;
    @synchronized(self) { [self.fileRequests addObject:request]; }
}

- (void)requestSafeDisconnect:(void (^)(BOOL, NSString *))completion
{
    self.safeDisconnectCompletion = completion;
    self.safeDisconnectRequested = YES;
}

- (void)start
{
    @synchronized(self)
    {
        if (self.running && self.thread.isExecuting)
            return;
        /* A bind error or an iOS network teardown can end the worker while
         * leaving the singleton alive.  A foreground/start request must be
         * able to create a fresh worker instead of remaining permanently in
         * the initial "Connect iPod" state. */
        self.running = YES;
        self.lastDeviceResponse = 0;
        self.catalogRetries = 0;
    }
    void (^createWebView)(void) = ^{
        WKWebViewConfiguration *configuration =
            [[WKWebViewConfiguration alloc] init];
        self.webView = [[WKWebView alloc]
            initWithFrame:CGRectMake(0, 0, 310, 170)
             configuration:configuration];
        self.webView.navigationDelegate = self;
        self.webView.customUserAgent =
            @"Mozilla/5.0 (iPhone; CPU iPhone OS 17_0 like Mac OS X) "
             @"AppleWebKit/605.1.15 Version/17.0 Mobile Safari/604.1";
        /* WKWebView snapshotting is unreliable when the view has never been
         * attached.  Keep the real renderer behind the opaque app UI so it
         * receives a normal layout/window lifecycle without becoming a fake
         * browser surface in the companion. */
        UIWindow *window = UIApplication.sharedApplication.windows.firstObject;
        if (window)
            [window insertSubview:self.webView atIndex:0];
    };
    if ([NSThread isMainThread])
        createWebView();
    else
        dispatch_sync(dispatch_get_main_queue(), createWebView);
    self.thread = [[NSThread alloc] initWithTarget:self
                                          selector:@selector(relayLoop)
                                            object:nil];
    self.thread.name = @"RockPod USB Relay";
    [self.thread start];
}

- (void)stop
{
    self.running = NO;
}

- (int)openSocketOnPort:(uint16_t)port
{
    int descriptor = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    int yes = 1;
    struct sockaddr_in local;
    if (descriptor < 0)
        return -1;
    setsockopt(descriptor, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    memset(&local, 0, sizeof(local));
    local.sin_family = AF_INET;
    local.sin_port = htons(port);
    local.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(descriptor, (struct sockaddr *)&local, sizeof(local)) < 0)
    {
        close(descriptor);
        return -1;
    }
    return descriptor;
}

- (void)sendProbe:(int)descriptor port:(uint16_t)port bytes:(const void *)bytes
            length:(size_t)length
{
    struct sockaddr_in destination;
    memset(&destination, 0, sizeof(destination));
    destination.sin_family = AF_INET;
    destination.sin_port = htons(port);
    inet_pton(AF_INET, RP_IP, &destination.sin_addr);
    sendto(descriptor, bytes, length, 0,
           (struct sockaddr *)&destination, sizeof(destination));
}

- (BOOL)sendConfirmed:(int)descriptor packet:(NSData *)packet
             request:(uint32_t)request expected:(uint32_t)expected
{
    unsigned char response[512];
    struct sockaddr_in destination;
    memset(&destination, 0, sizeof(destination));
    destination.sin_family = AF_INET;
    destination.sin_port = htons(RP_BROWSER_PORT);
    inet_pton(AF_INET, RP_IP, &destination.sin_addr);
    for (int attempt = 0; attempt < 8 && self.running; attempt++)
    {
        sendto(descriptor, packet.bytes, packet.length, 0,
               (struct sockaddr *)&destination, sizeof(destination));
        for (int poll = 0; poll < 4; poll++)
        {
            fd_set reads;
            struct timeval timeout = { 0, 250000 };
            FD_ZERO(&reads);
            FD_SET(descriptor, &reads);
            if (select(descriptor + 1, &reads, NULL, NULL, &timeout) <= 0)
                continue;
            ssize_t length = recv(descriptor, response, sizeof(response), 0);
            if (length == 13 && !memcmp(response, "RPB1\5", 5) &&
                rp_get_be32(response + 5) == request &&
                rp_get_be32(response + 9) == expected)
                return YES;
        }
    }
    return NO;
}

- (BOOL)sendWeatherConfirmed:(int)descriptor packet:(NSData *)packet
                     expected:(uint32_t)expected
{
    unsigned char response[512];
    struct sockaddr_in destination;
    memset(&destination, 0, sizeof(destination));
    destination.sin_family = AF_INET;
    destination.sin_port = htons(RP_WEATHER_PORT);
    inet_pton(AF_INET, RP_IP, &destination.sin_addr);
    for (int attempt = 0; attempt < 8 && self.running; attempt++)
    {
        sendto(descriptor, packet.bytes, packet.length, 0,
               (struct sockaddr *)&destination, sizeof(destination));
        for (int poll = 0; poll < 4; poll++)
        {
            fd_set reads;
            struct timeval timeout = { 0, 250000 };
            FD_ZERO(&reads);
            FD_SET(descriptor, &reads);
            if (select(descriptor + 1, &reads, NULL, NULL, &timeout) <= 0)
                continue;
            ssize_t length = recv(descriptor, response, sizeof(response), 0);
            if (length == 9 && !memcmp(response, "RPI1\5", 5) &&
                rp_get_be32(response + 5) == expected)
                return YES;
        }
    }
    return NO;
}

- (NSData *)weatherPayload
{
    NSString *urlString =
        @"https://api.open-meteo.com/v1/forecast?latitude=46.0878&longitude=-64.7782&current=temperature_2m%2Cweather_code%2Cwind_speed_10m%2Cwind_direction_10m%2Cis_day%2Cprecipitation&hourly=temperature_2m%2Cprecipitation_probability%2Cweather_code%2Cwind_speed_10m%2Cwind_direction_10m%2Cis_day&daily=weather_code%2Ctemperature_2m_max%2Ctemperature_2m_min%2Cprecipitation_probability_max%2Cwind_speed_10m_max%2Cwind_direction_10m_dominant%2Csunrise%2Csunset&timezone=auto&forecast_days=7";
    NSError *error = nil;
    NSData *data = [NSData dataWithContentsOfURL:[NSURL URLWithString:urlString]
                                        options:0 error:&error];
    if (!data || error)
        return nil;
    NSDictionary *root = [NSJSONSerialization JSONObjectWithData:data
                                                          options:0
                                                            error:&error];
    NSDictionary *hourly = root[@"hourly"];
    NSDictionary *daily = root[@"daily"];
    NSDictionary *current = root[@"current"];
    NSArray *hourTimes = hourly[@"time"];
    NSArray *hourCodes = hourly[@"weather_code"];
    NSArray *hourTemps = hourly[@"temperature_2m"];
    NSArray *hourPrecip = hourly[@"precipitation_probability"];
    NSArray *hourWind = hourly[@"wind_speed_10m"];
    NSArray *hourDirection = hourly[@"wind_direction_10m"];
    NSArray *hourDay = hourly[@"is_day"];
    NSArray *dates = daily[@"time"];
    if (error || ![hourTimes isKindOfClass:[NSArray class]] ||
        ![dates isKindOfClass:[NSArray class]])
        return nil;

    NSDateFormatter *formatter = [[NSDateFormatter alloc] init];
    formatter.locale = [NSLocale localeWithLocaleIdentifier:@"en_US_POSIX"];
    formatter.timeZone = [NSTimeZone timeZoneForSecondsFromGMT:0];
    formatter.dateFormat = @"yyyy-MM-dd'T'HH:mm'Z'";
    NSMutableString *output = [NSMutableString stringWithFormat:
        @"rockpod_weather_v1\tMoncton\t46.0878\t-64.7782\t%@\t%@\tOpen-Meteo\tmetric\n",
        root[@"timezone"] ?: @"UTC", [formatter stringFromDate:[NSDate date]]];

    if ([current isKindOfClass:[NSDictionary class]] && current[@"time"] &&
        current[@"temperature_2m"] && current[@"weather_code"])
    {
        NSString *icon;
        NSString *name = rp_weather_name([current[@"weather_code"] integerValue],
                                         &icon);
        [output appendFormat:@"current\t%@\t%@\t%@\t%.0f\t%.0f\t%.0f\t%@\t%ld\tOpen-Meteo Live\n",
            current[@"time"], icon, name,
            [current[@"temperature_2m"] doubleValue],
            [current[@"precipitation"] doubleValue],
            [current[@"wind_speed_10m"] doubleValue],
            rp_compass([current[@"wind_direction_10m"] doubleValue]),
            (long)[current[@"is_day"] integerValue]];
    }

    NSUInteger hours = MIN((NSUInteger)168, hourTimes.count);
    hours = MIN(hours, hourCodes.count);
    hours = MIN(hours, hourTemps.count);
    hours = MIN(hours, hourPrecip.count);
    hours = MIN(hours, hourWind.count);
    hours = MIN(hours, hourDirection.count);
    hours = MIN(hours, hourDay.count);
    for (NSUInteger index = 0; index < hours; index++)
    {
        NSString *icon;
        NSString *name = rp_weather_name([hourCodes[index] integerValue], &icon);
        [output appendFormat:@"hourly\t%@\t%@\t%@\t%.0f\t%.0f\t%.0f\t%@\t%ld\tOpen-Meteo\n",
            hourTimes[index], icon, name, [hourTemps[index] doubleValue],
            [hourPrecip[index] doubleValue], [hourWind[index] doubleValue],
            rp_compass([hourDirection[index] doubleValue]),
            (long)[hourDay[index] integerValue]];
    }

    NSArray *dayCodes = daily[@"weather_code"];
    NSArray *dayMax = daily[@"temperature_2m_max"];
    NSArray *dayMin = daily[@"temperature_2m_min"];
    NSArray *dayPrecip = daily[@"precipitation_probability_max"];
    NSArray *dayWind = daily[@"wind_speed_10m_max"];
    NSArray *dayDirection = daily[@"wind_direction_10m_dominant"];
    NSArray *sunrise = daily[@"sunrise"];
    NSArray *sunset = daily[@"sunset"];
    NSUInteger days = MIN((NSUInteger)7, dates.count);
    days = MIN(days, dayCodes.count);
    days = MIN(days, dayMax.count);
    days = MIN(days, dayMin.count);
    days = MIN(days, dayPrecip.count);
    days = MIN(days, dayWind.count);
    days = MIN(days, dayDirection.count);
    days = MIN(days, sunrise.count);
    days = MIN(days, sunset.count);
    for (NSUInteger index = 0; index < days; index++)
    {
        NSString *icon;
        NSString *name = rp_weather_name([dayCodes[index] integerValue], &icon);
        [output appendFormat:@"%@\t%@\t%@\t%.0f\t%.0f\t%.0f\t%.0f\t%@\t%@\t%@\tOpen-Meteo\n",
            dates[index], icon, name, [dayMin[index] doubleValue],
            [dayMax[index] doubleValue], [dayPrecip[index] doubleValue],
            [dayWind[index] doubleValue],
            rp_compass([dayDirection[index] doubleValue]),
            rp_tail_time(sunrise[index]), rp_tail_time(sunset[index])];
    }
    return [output dataUsingEncoding:NSUTF8StringEncoding];
}

- (BOOL)sendWeather:(NSData *)payload socket:(int)descriptor
{
    NSMutableData *packet = [NSMutableData dataWithLength:13];
    unsigned char *bytes = packet.mutableBytes;
    memcpy(bytes, "RPI1\1", 5);
    rp_put_be32(bytes + 5, (uint32_t)payload.length);
    rp_put_be32(bytes + 9, 0);
    if (![self sendWeatherConfirmed:descriptor packet:packet expected:0])
        return NO;
    for (NSUInteger offset = 0; offset < payload.length; offset += 480)
    {
        NSUInteger count = MIN((NSUInteger)480, payload.length - offset);
        packet = [NSMutableData dataWithLength:9 + count];
        bytes = packet.mutableBytes;
        memcpy(bytes, "RPI1\2", 5);
        rp_put_be32(bytes + 5, (uint32_t)offset);
        [payload getBytes:bytes + 9 range:NSMakeRange(offset, count)];
        if (![self sendWeatherConfirmed:descriptor packet:packet
                               expected:(uint32_t)(offset + count)])
            return NO;
    }
    packet = [NSMutableData dataWithBytes:"RPI1\3" length:5];
    return [self sendWeatherConfirmed:descriptor packet:packet
                             expected:0xffffffff];
}

- (BOOL)sendPage:(NSData *)payload request:(uint32_t)request
           socket:(int)descriptor
{
    NSMutableData *packet = [NSMutableData dataWithLength:17];
    unsigned char *bytes = packet.mutableBytes;
    memcpy(bytes, "RPB1\2", 5);
    rp_put_be32(bytes + 5, request);
    rp_put_be32(bytes + 9, (uint32_t)payload.length);
    rp_put_be32(bytes + 13, 0);
    if (![self sendConfirmed:descriptor packet:packet request:request expected:0])
        return NO;
    for (NSUInteger offset = 0; offset < payload.length; offset += RP_CHUNK)
    {
        NSUInteger count = MIN((NSUInteger)RP_CHUNK, payload.length - offset);
        packet = [NSMutableData dataWithLength:13 + count];
        bytes = packet.mutableBytes;
        memcpy(bytes, "RPB1\3", 5);
        rp_put_be32(bytes + 5, request);
        rp_put_be32(bytes + 9, (uint32_t)offset);
        [payload getBytes:bytes + 13 range:NSMakeRange(offset, count)];
        if (![self sendConfirmed:descriptor packet:packet request:request
                           expected:(uint32_t)(offset + count)])
            return NO;
    }
    packet = [NSMutableData dataWithLength:9];
    bytes = packet.mutableBytes;
    memcpy(bytes, "RPB1\4", 5);
    rp_put_be32(bytes + 5, request);
    return [self sendConfirmed:descriptor packet:packet request:request
                      expected:0xffffffff];
}

- (void)beginPage:(NSURL *)url request:(uint32_t)request
{
    if (!url || self.pageBusy)
        return;
    self.pageBusy = YES;
    self.pendingRequest = request;
    self.captureIssuedRequest = 0;
    [self report:@"Loading page from iPhone…" connected:YES];
    dispatch_async(dispatch_get_main_queue(), ^{
        [self.webView loadRequest:[NSURLRequest requestWithURL:url
            cachePolicy:NSURLRequestReloadIgnoringLocalCacheData
            timeoutInterval:25.0]];
    });
}

- (void)captureCurrentPage
{
    uint32_t request = self.pendingRequest;
    if (!request || self.captureIssuedRequest == request)
        return;
    self.captureIssuedRequest = request;
    WKSnapshotConfiguration *configuration =
        [[WKSnapshotConfiguration alloc] init];
    /* WKSnapshotConfiguration rects are page coordinates.  A fixed y=0 kept
     * returning Google's first viewport after the iPod had scrolled. */
    CGFloat offset = MAX(0, self.webView.scrollView.contentOffset.y);
    configuration.rect = CGRectMake(0, offset, 310, 170);
    configuration.snapshotWidth = @310;
    [self.webView takeSnapshotWithConfiguration:configuration
                              completionHandler:^(UIImage *image,
                                                  NSError *snapshotError) {
        if (!image || snapshotError)
        {
            self.pageBusy = NO;
            [self report:@"Could not render page" connected:YES];
            return;
        }
        NSData *bitmap = rp_viewport_from_image(image);
        NSString *html = @"<html><body><img src=\"/.rockbox/offlineweb/"
                          @"cache/live.bmp\" width=\"310\" height=\"170\">"
                          @"</body></html>";
        NSData *page = [html dataUsingEncoding:NSUTF8StringEncoding];
        NSMutableData *bundle = [NSMutableData dataWithLength:12];
        unsigned char *header = bundle.mutableBytes;
        memcpy(header, "RPWB", 4);
        rp_put_be32(header + 4, (uint32_t)page.length);
        rp_put_be32(header + 8, (uint32_t)bitmap.length);
        [bundle appendData:page];
        [bundle appendData:bitmap];
        @synchronized(self)
        {
            self.readyRequest = request;
            self.readyPayload = bundle;
        }
    }];
}

- (void)snapshotAfterInteraction
{
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW,
                                 (int64_t)(900 * NSEC_PER_MSEC)),
                   dispatch_get_main_queue(), ^{
        if (!self.navigationPending)
            [self captureCurrentPage];
    });
}

- (void)performCommand:(NSString *)command request:(uint32_t)request
{
    if (!command || self.pageBusy)
        return;
    self.pageBusy = YES;
    self.pendingRequest = request;
    self.captureIssuedRequest = 0;
    [self report:@"Live browser active" connected:YES];
    dispatch_async(dispatch_get_main_queue(), ^{
        self.navigationPending = NO;
        if ([command hasPrefix:@"scroll:"])
        {
            NSInteger delta = [[command substringFromIndex:7] integerValue];
            NSString *script = [NSString stringWithFormat:
                @"window.scrollBy(0,%ld);true", (long)delta];
            [self.webView evaluateJavaScript:script completionHandler:nil];
            [self snapshotAfterInteraction];
        }
        else if ([command hasPrefix:@"tap:"])
        {
            NSArray<NSString *> *parts = [command componentsSeparatedByString:@":"];
            NSInteger x = parts.count > 1 ? [parts[1] integerValue] : 155;
            NSInteger y = parts.count > 2 ? [parts[2] integerValue] : 85;
            NSString *script = [NSString stringWithFormat:
                @"(function(){var e=document.elementFromPoint(%ld,%ld);"
                 @"if(!e)return false;var a=e.closest?e.closest('a[href]'):null;"
                 @"if(a){location.href=a.href;return true;}e.focus();e.click();"
                 @"return false;})()", (long)x, (long)y];
            [self.webView evaluateJavaScript:script completionHandler:
                ^(id result, NSError *error) {
                    (void)result;
                    (void)error;
                    [self snapshotAfterInteraction];
                }];
        }
        else if ([command hasPrefix:@"text:"])
        {
            NSString *encoded = [command substringFromIndex:5];
            encoded = [encoded stringByReplacingOccurrencesOfString:@"+"
                                                           withString:@" "];
            NSString *value = encoded.stringByRemovingPercentEncoding ?: encoded;
            NSData *jsonData = [NSJSONSerialization dataWithJSONObject:@[value]
                                                                options:0 error:nil];
            NSString *json = [[NSString alloc] initWithData:jsonData
                                                   encoding:NSUTF8StringEncoding];
            NSString *quoted = json.length >= 2 ?
                [json substringWithRange:NSMakeRange(1, json.length - 2)] : @"\"\"";
            NSString *script = [NSString stringWithFormat:
                @"(function(){var e=document.activeElement;if(!e)return false;"
                 @"var v=%@;if('value' in e)e.value=v;else if(e.isContentEditable)"
                 @"e.innerText=v;else return false;e.dispatchEvent(new Event('input',"
                 @"{bubbles:true}));e.dispatchEvent(new Event('change',{bubbles:true}));"
                 @"return true;})()", quoted];
            [self.webView evaluateJavaScript:script completionHandler:nil];
            [self snapshotAfterInteraction];
        }
        else if ([command isEqualToString:@"back"])
        {
            if ([self.webView canGoBack])
                [self.webView goBack];
            else
                [self snapshotAfterInteraction];
        }
        else if ([command isEqualToString:@"forward"])
        {
            if ([self.webView canGoForward])
                [self.webView goForward];
            else
                [self snapshotAfterInteraction];
        }
        else if ([command isEqualToString:@"reload"])
            [self.webView reload];
        else
            [self snapshotAfterInteraction];
    });
}

- (uint32_t)nextSyncRequest
{
    self.syncSequence++;
    if (!self.syncSequence)
        self.syncSequence = 1;
    return self.syncSequence;
}

- (BOOL)sendSyncConfirmed:(int)descriptor packet:(NSData *)packet
                   request:(uint32_t)request expected:(uint32_t)expected
{
    unsigned char response[RP_SYNC_CHUNK + 64];
    struct sockaddr_in destination;
    memset(&destination, 0, sizeof(destination));
    destination.sin_family = AF_INET;
    destination.sin_port = htons(RP_SYNC_PORT);
    inet_pton(AF_INET, RP_IP, &destination.sin_addr);
    for (int attempt = 0; attempt < 8 && self.running; attempt++)
    {
        sendto(descriptor, packet.bytes, packet.length, 0,
               (struct sockaddr *)&destination, sizeof(destination));
        for (int poll = 0; poll < 8; poll++)
        {
            fd_set reads;
            struct timeval timeout = { 0, 250000 };
            FD_ZERO(&reads);
            FD_SET(descriptor, &reads);
            if (select(descriptor + 1, &reads, NULL, NULL, &timeout) <= 0)
                continue;
            ssize_t length = recv(descriptor, response, sizeof(response), 0);
            if ([self absorbSyncPacket:response length:length])
                continue;
            if (length == 14 && !memcmp(response, "RPS1\5", 5) &&
                rp_get_be32(response + 5) == request &&
                rp_get_be32(response + 9) == expected && response[13] == 0)
                return YES;
            if (length == 14 && !memcmp(response, "RPS1\5", 5) &&
                rp_get_be32(response + 5) == request && response[13] != 0)
                return NO;
        }
    }
    return NO;
}

- (void)finishTransfer:(RPTransfer *)transfer success:(BOOL)success
                message:(NSString *)message
{
    if (transfer.securityScoped)
        [transfer.url stopAccessingSecurityScopedResource];
    dispatch_async(dispatch_get_main_queue(), ^{
        if (transfer.completion)
            transfer.completion(success, message);
    });
}

- (BOOL)sendTransfer:(RPTransfer *)transfer socket:(int)descriptor
{
    NSError *error = nil;
    NSDictionary *attributes = [[NSFileManager defaultManager]
        attributesOfItemAtPath:transfer.url.path error:&error];
    unsigned long long size = [attributes[NSFileSize] unsignedLongLongValue];
    NSData *path = [transfer.destination dataUsingEncoding:NSUTF8StringEncoding];
    if (error || size == 0 || size > INT32_MAX || path.length == 0 ||
        path.length + 18 > 1400)
    {
        [self finishTransfer:transfer success:NO
                     message:@"File is empty, inaccessible, or over 2 GB"];
        return NO;
    }

    uint32_t request = [self nextSyncRequest];
    NSMutableData *packet = [NSMutableData dataWithLength:18 + path.length];
    unsigned char *bytes = packet.mutableBytes;
    memcpy(bytes, "RPS1\2", 5);
    rp_put_be32(bytes + 5, request);
    rp_put_be32(bytes + 9, (uint32_t)size);
    rp_put_be32(bytes + 13, 0);
    [path getBytes:bytes + 17 length:path.length];
    bytes[17 + path.length] = 0;
    if (![self sendSyncConfirmed:descriptor packet:packet request:request
                        expected:0])
    {
        [self finishTransfer:transfer success:NO
                     message:@"iPod rejected the destination"];
        return NO;
    }

    NSFileHandle *handle = [NSFileHandle fileHandleForReadingFromURL:transfer.url
                                                               error:&error];
    if (!handle || error)
    {
        [self finishTransfer:transfer success:NO message:@"Could not open file"];
        return NO;
    }
    uint32_t offset = 0;
    while (offset < size && self.running)
    {
        @autoreleasepool
        {
            NSData *chunk = [handle readDataOfLength:
                (NSUInteger)MIN((unsigned long long)RP_SYNC_CHUNK,
                                size - offset)];
            if (chunk.length == 0)
                break;
            packet = [NSMutableData dataWithLength:13 + chunk.length];
            bytes = packet.mutableBytes;
            memcpy(bytes, "RPS1\3", 5);
            rp_put_be32(bytes + 5, request);
            rp_put_be32(bytes + 9, offset);
            [chunk getBytes:bytes + 13 length:chunk.length];
            uint32_t next = offset + (uint32_t)chunk.length;
            if (![self sendSyncConfirmed:descriptor packet:packet
                                  request:request expected:next])
                break;
            offset = next;
            if (self.transferProgressHandler)
            {
                double progress = (double)offset / (double)size;
                NSString *name = transfer.url.lastPathComponent;
                dispatch_async(dispatch_get_main_queue(), ^{
                    self.transferProgressHandler(name, progress);
                });
            }
        }
    }
    [handle closeFile];
    if (offset != size)
    {
        [self finishTransfer:transfer success:NO
                     message:@"USB transfer was interrupted"];
        return NO;
    }

    packet = [NSMutableData dataWithLength:9];
    bytes = packet.mutableBytes;
    memcpy(bytes, "RPS1\4", 5);
    rp_put_be32(bytes + 5, request);
    BOOL success = [self sendSyncConfirmed:descriptor packet:packet
                                    request:request expected:0xffffffff];
    [self finishTransfer:transfer success:success
                 message:success ? [NSString stringWithFormat:@"Synced to %@",
                                     transfer.destination] : @"Commit failed"];
    return success;
}

- (NSData *)fetchSitekickOnSocket:(int)descriptor
{
    uint32_t request = [self nextSyncRequest];
    uint32_t offset = 0;
    uint32_t total = 0;
    NSMutableData *result = [NSMutableData data];
    unsigned char packet[13];
    unsigned char response[RP_SYNC_CHUNK + 64];
    struct sockaddr_in destination;
    memset(&destination, 0, sizeof(destination));
    destination.sin_family = AF_INET;
    destination.sin_port = htons(RP_SYNC_PORT);
    inet_pton(AF_INET, RP_IP, &destination.sin_addr);

    do
    {
    memcpy(packet, "RPS1", 4);
        packet[4] = 8;
        rp_put_be32(packet + 5, request);
        rp_put_be32(packet + 9, offset);
        BOOL received = NO;
        int absorbed = 0;
        for (int attempt = 0; attempt < 8 && !received; attempt++)
        {
            sendto(descriptor, packet, sizeof(packet), 0,
                   (struct sockaddr *)&destination, sizeof(destination));
            fd_set reads;
            struct timeval timeout = { 0, 500000 };
            FD_ZERO(&reads);
            FD_SET(descriptor, &reads);
            if (select(descriptor + 1, &reads, NULL, NULL, &timeout) <= 0)
                continue;
            ssize_t length = recv(descriptor, response, sizeof(response), 0);
            if ([self absorbSyncPacket:response length:length])
            {
                if (absorbed++ < RP_SYNC_ABSORB_MAX)
                    attempt--;
                continue;
            }
            if (length < 17 || memcmp(response, "RPS1", 4) ||
                response[4] != 7 || rp_get_be32(response + 5) != request ||
                rp_get_be32(response + 13) != offset)
                continue;
            total = rp_get_be32(response + 9);
            if (!total || total > 2 * 1024 * 1024 ||
                offset + length - 17 > total)
                return nil;
            [result appendBytes:response + 17 length:(NSUInteger)length - 17];
            offset += (uint32_t)length - 17;
            received = YES;
        }
        if (!received)
            return nil;
    } while (offset < total && self.running);
    return offset == total ? result : nil;
}

- (NSData *)fetchLibraryCatalogOnSocket:(int)descriptor
{
    uint32_t request = [self nextSyncRequest];
    uint32_t offset = 0;
    uint32_t total = 0;
    NSMutableData *result = [NSMutableData data];
    unsigned char packet[13];
    unsigned char response[RP_SYNC_CHUNK + 64];
    struct sockaddr_in destination;
    memset(&destination, 0, sizeof(destination));
    destination.sin_family = AF_INET;
    destination.sin_port = htons(RP_SYNC_PORT);
    inet_pton(AF_INET, RP_IP, &destination.sin_addr);

    do
    {
        memcpy(packet, "RPS1", 4);
        packet[4] = 11;
        rp_put_be32(packet + 5, request);
        rp_put_be32(packet + 9, offset);
        BOOL received = NO;
        int absorbed = 0;
        for (int attempt = 0; attempt < 8 && !received; attempt++)
        {
            sendto(descriptor, packet, sizeof(packet), 0,
                   (struct sockaddr *)&destination, sizeof(destination));
            fd_set reads;
            struct timeval timeout = { 0, 750000 };
            FD_ZERO(&reads);
            FD_SET(descriptor, &reads);
            if (select(descriptor + 1, &reads, NULL, NULL, &timeout) <= 0)
                continue;
            ssize_t length = recv(descriptor, response, sizeof(response), 0);
            if ([self absorbSyncPacket:response length:length])
            {
                if (absorbed++ < RP_SYNC_ABSORB_MAX)
                    attempt--;
                continue;
            }
            if (length < 17 || memcmp(response, "RPS1", 4) ||
                response[4] != 12 || rp_get_be32(response + 5) != request ||
                rp_get_be32(response + 13) != offset)
                continue;
            total = rp_get_be32(response + 9);
            if (!total || total > 16 * 1024 * 1024 ||
                offset + length - 17 > total)
                return nil;
            [result appendBytes:response + 17 length:(NSUInteger)length - 17];
            offset += (uint32_t)length - 17;
            received = YES;
        }
        if (!received)
            return nil;
    } while (offset < total && self.running);
    return offset == total ? result : nil;
}

- (NSURL *)fetchMediaFile:(RPFileRequest *)fileRequest socket:(int)descriptor
                    error:(NSString **)errorMessage
{
    uint32_t request = [self nextSyncRequest], offset = 0, total = 0;
    NSData *path = [fileRequest.path dataUsingEncoding:NSUTF8StringEncoding];
    NSMutableData *packet = [NSMutableData dataWithLength:14 + path.length];
    unsigned char *bytes = packet.mutableBytes;
    memcpy(bytes, "RPS1", 4); bytes[4] = 13;
    rp_put_be32(bytes + 5, request); [packet replaceBytesInRange:NSMakeRange(13, path.length)
        withBytes:path.bytes]; bytes = packet.mutableBytes; bytes[13 + path.length] = 0;
    NSString *suffix = fileRequest.path.pathExtension.length ? fileRequest.path.pathExtension : @"audio";
    NSURL *output = [NSFileManager.defaultManager.temporaryDirectory URLByAppendingPathComponent:
        [[NSString stringWithFormat:@"ipod-%@", NSUUID.UUID.UUIDString] stringByAppendingPathExtension:suffix]];
    [NSFileManager.defaultManager createFileAtPath:output.path contents:nil attributes:nil];
    NSFileHandle *handle = [NSFileHandle fileHandleForWritingAtPath:output.path];
    unsigned char response[RP_SYNC_CHUNK + 64];
    struct sockaddr_in destination = {0};
    destination.sin_family = AF_INET; destination.sin_port = htons(RP_SYNC_PORT);
    inet_pton(AF_INET, RP_IP, &destination.sin_addr);
    do {
        bytes = packet.mutableBytes; rp_put_be32(bytes + 9, offset);
        BOOL received = NO;
        int absorbed = 0;
        for (int attempt = 0; attempt < 8 && !received; attempt++)
        {
            sendto(descriptor, packet.bytes, packet.length, 0,
                   (struct sockaddr *)&destination, sizeof(destination));
            fd_set reads; struct timeval timeout = {0, 750000};
            FD_ZERO(&reads); FD_SET(descriptor, &reads);
            if (select(descriptor + 1, &reads, NULL, NULL, &timeout) <= 0) continue;
            ssize_t length = recv(descriptor, response, sizeof(response), 0);
            if ([self absorbSyncPacket:response length:length])
            {
                if (absorbed++ < RP_SYNC_ABSORB_MAX)
                    attempt--;
                continue;
            }
            if (length < 17 || memcmp(response, "RPS1", 4) || response[4] != 14 ||
                rp_get_be32(response + 5) != request || rp_get_be32(response + 13) != offset) continue;
            total = rp_get_be32(response + 9);
            if (!total || total > 1024u * 1024u * 1024u || offset + length - 17 > total) break;
            [handle writeData:[NSData dataWithBytes:response + 17 length:(NSUInteger)length - 17]];
            offset += (uint32_t)length - 17; received = YES;
            if (self.transferProgressHandler)
                dispatch_async(dispatch_get_main_queue(), ^{
                    self.transferProgressHandler(fileRequest.path.lastPathComponent,
                        total ? (double)offset / total : 0);
                });
        }
        if (!received) { *errorMessage = total ? @"iPod media transfer was interrupted" : @"iPod refused the media file"; break; }
    } while (offset < total && self.running);
    [handle closeFile];
    if (!total || offset != total)
    { [NSFileManager.defaultManager removeItemAtURL:output error:nil]; return nil; }
    return output;
}

- (NSData *)fetchMediaData:(RPStreamDataRequest *)streamRequest
                     socket:(int)descriptor
                totalLength:(uint64_t *)totalLength
                      error:(NSString **)errorMessage
{
    uint32_t request = [self nextSyncRequest];
    uint32_t offset = (uint32_t)streamRequest.offset;
    uint32_t start = offset;
    uint32_t total = 0;
    NSData *path = [streamRequest.path dataUsingEncoding:NSUTF8StringEncoding];
    NSMutableData *packet = [NSMutableData dataWithLength:14 + path.length];
    unsigned char *bytes = packet.mutableBytes;
    memcpy(bytes, "RPS1", 4);
    bytes[4] = 13;
    rp_put_be32(bytes + 5, request);
    [packet replaceBytesInRange:NSMakeRange(13, path.length)
                      withBytes:path.bytes];
    bytes = packet.mutableBytes;
    bytes[13 + path.length] = 0;
    NSMutableData *result = [NSMutableData dataWithCapacity:streamRequest.length];
    unsigned char response[RP_SYNC_CHUNK + 64];
    struct sockaddr_in destination = {0};
    destination.sin_family = AF_INET;
    destination.sin_port = htons(RP_SYNC_PORT);
    inet_pton(AF_INET, RP_IP, &destination.sin_addr);

    while (result.length < streamRequest.length && self.running)
    {
        bytes = packet.mutableBytes;
        rp_put_be32(bytes + 9, offset);
        BOOL received = NO;
        int absorbed = 0;
        for (int attempt = 0; attempt < 8 && !received; attempt++)
        {
            sendto(descriptor, packet.bytes, packet.length, 0,
                   (struct sockaddr *)&destination, sizeof(destination));
            fd_set reads;
            struct timeval timeout = {0, 750000};
            FD_ZERO(&reads);
            FD_SET(descriptor, &reads);
            if (select(descriptor + 1, &reads, NULL, NULL, &timeout) <= 0)
                continue;
            ssize_t length = recv(descriptor, response, sizeof(response), 0);
            if ([self absorbSyncPacket:response length:length])
            {
                if (absorbed++ < RP_SYNC_ABSORB_MAX)
                    attempt--;
                continue;
            }
            if (length < 17 || memcmp(response, "RPS1", 4) ||
                response[4] != 14 || rp_get_be32(response + 5) != request ||
                rp_get_be32(response + 13) != offset)
                continue;
            total = rp_get_be32(response + 9);
            NSUInteger available = (NSUInteger)length - 17;
            if (!total || total > 1024u * 1024u * 1024u ||
                offset > total || offset + available > total)
                break;
            NSUInteger wanted = MIN(available,
                streamRequest.length - result.length);
            [result appendBytes:response + 17 length:wanted];
            offset += (uint32_t)wanted;
            received = YES;
        }
        if (!received)
        {
            if (errorMessage)
                *errorMessage = total ? @"iPod stream was interrupted" :
                                       @"iPod refused the stream";
            return nil;
        }
        if (offset >= total)
            break;
    }
    if (totalLength)
        *totalLength = total;
    if (!result.length && start < total)
    {
        if (errorMessage) *errorMessage = @"iPod returned no stream data";
        return nil;
    }
    return result;
}

- (void)parseDeviceStatus:(const unsigned char *)bytes length:(NSUInteger)length
{
    NSString *text = [[NSString alloc] initWithBytes:bytes length:length
                                             encoding:NSUTF8StringEncoding];
    if (!text)
        return;
    NSMutableDictionary *info = [NSMutableDictionary dictionary];
    for (NSString *line in [text componentsSeparatedByString:@"\n"])
    {
        NSRange separator = [line rangeOfString:@"="];
        if (separator.location == NSNotFound)
            continue;
        NSString *key = [line substringToIndex:separator.location];
        NSString *value = [line substringFromIndex:separator.location + 1];
        if (key.length)
            info[key] = value ?: @"";
    }
    [self noteDeviceResponse];
    self.connected = YES;
    dispatch_async(dispatch_get_main_queue(), ^{
        if (self.deviceInfoHandler)
            self.deviceInfoHandler(info);
    });
}

- (void)performSafeDisconnectOnSocket:(int)descriptor
{
    uint32_t request = [self nextSyncRequest];
    unsigned char packet[9];
    unsigned char response[32];
    struct sockaddr_in destination;
    memcpy(packet, "RPS1", 4);
    packet[4] = 9;
    rp_put_be32(packet + 5, request);
    memset(&destination, 0, sizeof(destination));
    destination.sin_family = AF_INET;
    destination.sin_port = htons(RP_SYNC_PORT);
    inet_pton(AF_INET, RP_IP, &destination.sin_addr);
    BOOL success = NO;
    int absorbed = 0;
    for (int attempt = 0; attempt < 8 && !success; attempt++)
    {
        sendto(descriptor, packet, sizeof(packet), 0,
               (struct sockaddr *)&destination, sizeof(destination));
        fd_set reads;
        struct timeval timeout = { 0, 500000 };
        FD_ZERO(&reads);
        FD_SET(descriptor, &reads);
        if (select(descriptor + 1, &reads, NULL, NULL, &timeout) <= 0)
            continue;
        ssize_t length = recv(descriptor, response, sizeof(response), 0);
        if ([self absorbSyncPacket:response length:length])
        {
            if (absorbed++ < RP_SYNC_ABSORB_MAX)
                attempt--;
            continue;
        }
        success = length == 9 && !memcmp(response, "RPS1", 4) &&
                  response[4] == 10 && rp_get_be32(response + 5) == request;
    }
    void (^completion)(BOOL, NSString *) = self.safeDisconnectCompletion;
    self.safeDisconnectCompletion = nil;
    dispatch_async(dispatch_get_main_queue(), ^{
        if (completion)
            completion(success, success ? @"Safe to disconnect" :
                                         @"iPod did not confirm flush");
    });
}

- (void)relayLoop
{
    @autoreleasepool
    {
        int weather = [self openSocketOnPort:RP_WEATHER_PORT];
        int browser = [self openSocketOnPort:RP_BROWSER_PORT];
        int sync = [self openSocketOnPort:RP_SYNC_PORT];
        if (weather < 0 || browser < 0 || sync < 0)
        {
            [self report:@"Could not open USB relay" connected:NO];
            if (weather >= 0) close(weather);
            if (browser >= 0) close(browser);
            if (sync >= 0) close(sync);
            self.running = NO;
            return;
        }
        [self report:@"Connect iPod USB" connected:NO];
        NSTimeInterval lastProbe = 0;
        while (self.running)
        {
            @autoreleasepool
            {
                NSTimeInterval now = [NSDate timeIntervalSinceReferenceDate];
                if (now - lastProbe >= 2.0)
                {
                    [self sendProbe:weather port:RP_WEATHER_PORT
                               bytes:"RPI1\0" length:5];
                    [self sendProbe:browser port:RP_BROWSER_PORT
                               bytes:"RPB1\0" length:5];
                    [self sendProbe:sync port:RP_SYNC_PORT
                               bytes:"RPS1\0" length:5];
                    lastProbe = now;
                }
                if (self.connected && self.lastDeviceResponse > 0 &&
                    now - self.lastDeviceResponse > 8.0)
                    [self report:@"iPod link lost — reconnecting…" connected:NO];
                /* Playback has priority over bulk sync. AVPlayer requests
                 * bounded ranges, so each turn still yields back to status,
                 * browser and weather work between audio chunks. */
                RPStreamDataRequest *streamRequest = nil;
                @synchronized(self)
                {
                    if (self.streamRequests.count)
                    {
                        streamRequest = self.streamRequests.firstObject;
                        [self.streamRequests removeObjectAtIndex:0];
                    }
                }
                if (streamRequest)
                {
                    NSString *error = nil;
                    uint64_t totalLength = 0;
                    NSData *data = [self fetchMediaData:streamRequest
                        socket:sync totalLength:&totalLength error:&error];
                    dispatch_async(dispatch_get_main_queue(), ^{
                        streamRequest.completion(data, totalLength, error);
                    });
                    continue;
                }
                RPTransfer *transfer = nil;
                @synchronized(self)
                {
                    if (self.transfers.count)
                    {
                        transfer = self.transfers.firstObject;
                        [self.transfers removeObjectAtIndex:0];
                    }
                }
                if (transfer)
                {
                    [self report:@"Syncing media…" connected:YES];
                    [self sendTransfer:transfer socket:sync];
                    [self report:@"iPod USB Link ready" connected:YES];
                    continue;
                }
                RPFileRequest *fileRequest = nil;
                @synchronized(self)
                {
                    if (self.fileRequests.count)
                    { fileRequest = self.fileRequests.firstObject; [self.fileRequests removeObjectAtIndex:0]; }
                }
                if (fileRequest)
                {
                    [self report:@"Loading music from iPod…" connected:YES];
                    NSString *error = nil;
                    NSURL *URL = [self fetchMediaFile:fileRequest socket:sync error:&error];
                    dispatch_async(dispatch_get_main_queue(), ^{
                        fileRequest.completion(URL, error);
                    });
                    [self report:@"iPod USB Link ready" connected:YES];
                    continue;
                }
                if (self.sitekickRequested)
                {
                    self.sitekickRequested = NO;
                    NSData *bitmap = [self fetchSitekickOnSocket:sync];
                    if (bitmap)
                    {
                        dispatch_async(dispatch_get_main_queue(), ^{
                            if (self.sitekickHandler)
                                self.sitekickHandler(bitmap);
                        });
                    }
                    continue;
                }
                if (self.libraryRequested)
                {
                    self.libraryRequested = NO;
                    [self report:@"Reading iPod music library…" connected:YES];
                    NSData *catalog = [self fetchLibraryCatalogOnSocket:sync];
                    if (catalog)
                    {
                        dispatch_async(dispatch_get_main_queue(), ^{
                            if (self.libraryCatalogHandler)
                                self.libraryCatalogHandler(catalog);
                        });
                    }
                    [self report:catalog ? @"iPod music library updated" :
                                           @"Could not read iPod music library"
                         connected:YES];
                    if (catalog)
                        self.catalogRetries = 0;
                    else if (self.running)
                    {
                        /* Tagcache can still be starting when the first USB
                         * status packet arrives. Retry without requiring an
                         * app restart or a cable reconnect.  The delay backs
                         * off because the iPod rebuilds its whole catalog
                         * inline for each new request id, blocking its relay
                         * service point; a fixed ten-second retry kept a slow
                         * library in a continuous rebuild.  There is no retry
                         * limit, so a tagcache that takes minutes to come up
                         * still recovers on its own. */
                        int delay = 10 << MIN(self.catalogRetries, 3);
                        self.catalogRetries++;
                        dispatch_after(dispatch_time(DISPATCH_TIME_NOW,
                            (int64_t)delay * (int64_t)NSEC_PER_SEC),
                            dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), ^{
                            if (self.running && self.connected)
                                self.libraryRequested = YES;
                        });
                    }
                    continue;
                }
                if (self.safeDisconnectRequested)
                {
                    self.safeDisconnectRequested = NO;
                    [self performSafeDisconnectOnSocket:sync];
                    continue;
                }
                NSData *payload = nil;
                uint32_t payloadRequest = 0;
                @synchronized(self)
                {
                    if (self.readyPayload)
                    {
                        payload = self.readyPayload;
                        payloadRequest = self.readyRequest;
                        self.readyPayload = nil;
                    }
                }
                if (payload)
                {
                    BOOL sent = [self sendPage:payload request:payloadRequest
                                        socket:browser];
                    self.pageBusy = NO;
                    [self report:sent ? @"iPod Internet connected" :
                                      @"USB transfer interrupted"
                         connected:sent];
                    continue;
                }
                fd_set reads;
                struct timeval timeout = { 0, 250000 };
                FD_ZERO(&reads);
                FD_SET(weather, &reads);
                FD_SET(browser, &reads);
                FD_SET(sync, &reads);
                int maximum = MAX(MAX(weather, browser), sync);
                if (select(maximum + 1, &reads, NULL, NULL, &timeout) <= 0)
                    continue;
                unsigned char request[1024];
                int source = FD_ISSET(weather, &reads) ? weather :
                             FD_ISSET(browser, &reads) ? browser : sync;
                ssize_t length = recv(source, request, sizeof(request) - 1, 0);
                if (source == weather && length == 5 &&
                    !memcmp(request, "RPI1\0", 5))
                {
                    [self noteDeviceResponse];
                    [self report:@"iPod Internet connected" connected:YES];
                }
                else if (source == weather && length == 5 &&
                         !memcmp(request, "RPI1\4", 5) && !self.weatherBusy)
                {
                    self.weatherBusy = YES;
                    [self report:@"Updating iPod weather…" connected:YES];
                    NSData *forecast = [self weatherPayload];
                    BOOL sent = forecast && [self sendWeather:forecast
                                                       socket:weather];
                    [self report:sent ? @"Weather updated" :
                                      @"Weather update failed"
                         connected:YES];
                    self.weatherBusy = NO;
                }
                else if (source == browser && length > 9 &&
                         !memcmp(request, "RPB1\1", 5))
                {
                    request[length] = 0;
                    uint32_t identifier = rp_get_be32(request + 5);
                    NSString *value = [[NSString alloc]
                        initWithBytes:request + 9
                              length:(NSUInteger)length - 9
                            encoding:NSUTF8StringEncoding];
                    [self beginPage:[NSURL URLWithString:value]
                            request:identifier];
                }
                else if (source == browser && length > 9 &&
                         !memcmp(request, "RPB1", 4) && request[4] == 8)
                {
                    request[length] = 0;
                    uint32_t identifier = rp_get_be32(request + 5);
                    NSString *command = [[NSString alloc]
                        initWithBytes:request + 9
                              length:(NSUInteger)length - 9
                            encoding:NSUTF8StringEncoding];
                    [self performCommand:command request:identifier];
                }
                else if (source == sync && length > 5 &&
                         !memcmp(request, "RPS1", 4) && request[4] == 1)
                {
                    [self parseDeviceStatus:request + 5
                                     length:(NSUInteger)length - 5];
                    [self report:@"iPod USB Link ready" connected:YES];
                }
            }
        }
        close(weather);
        close(browser);
        close(sync);
        self.running = NO;
        if (self.connected)
            [self report:@"iPod link stopped — reconnecting on foreground"
                connected:NO];
    }
}

- (void)webView:(WKWebView *)webView
        didStartProvisionalNavigation:(WKNavigation *)navigation
{
    (void)webView;
    (void)navigation;
    self.navigationPending = YES;
}

- (void)webView:(WKWebView *)webView
        didFinishNavigation:(WKNavigation *)navigation
{
    (void)webView;
    (void)navigation;
    self.navigationPending = NO;
    /* Modern pages finish navigation before their first stable paint.  Give
     * WebKit a short layout/paint window so the iPod does not receive a white
     * or half-rendered Google viewport. */
    [self snapshotAfterInteraction];
}

- (void)webView:(WKWebView *)webView
        didFailNavigation:(WKNavigation *)navigation
        withError:(NSError *)error
{
    (void)webView;
    (void)navigation;
    self.navigationPending = NO;
    [self report:[NSString stringWithFormat:@"Page failed: %@",
                  error.localizedDescription] connected:YES];
    [self captureCurrentPage];
}

- (void)webView:(WKWebView *)webView
        didFailProvisionalNavigation:(WKNavigation *)navigation
        withError:(NSError *)error
{
    [self webView:webView didFailNavigation:navigation withError:error];
}

@end
