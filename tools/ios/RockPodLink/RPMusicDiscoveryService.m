#import "RPMusicDiscoveryService.h"
#import <CommonCrypto/CommonDigest.h>

@implementation RPMusicDiscoveryService

- (NSString *)escapedQuery:(NSString *)value
{
    return [value stringByReplacingOccurrencesOfString:@"\"" withString:@"\\\""];
}

- (void)fetch:(NSURL *)URL completion:(void (^)(NSDictionary *, NSString *))completion
{
    NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:URL
        cachePolicy:NSURLRequestReloadIgnoringLocalCacheData timeoutInterval:30];
    [request setValue:@"RockPodLink/3.1 (music discovery)" forHTTPHeaderField:@"User-Agent"];
    NSURLSessionDataTask *task = [NSURLSession.sharedSession dataTaskWithRequest:request
        completionHandler:^(NSData *data, NSURLResponse *response, NSError *error) {
        NSHTTPURLResponse *http = (NSHTTPURLResponse *)response;
        NSDictionary *json = data.length ? [NSJSONSerialization JSONObjectWithData:data
            options:0 error:nil] : nil;
        NSString *message = error.localizedDescription;
        if (!message.length && (http.statusCode < 200 || http.statusCode >= 300))
            message = [NSString stringWithFormat:@"MusicBrainz returned HTTP %ld", (long)http.statusCode];
        dispatch_async(dispatch_get_main_queue(), ^{ completion(json, message); });
    }];
    [task resume];
}

- (NSURL *)URLForPath:(NSString *)path query:(NSString *)query
{
    NSURLComponents *parts = [NSURLComponents componentsWithString:
        [@"https://musicbrainz.org/ws/2/" stringByAppendingString:path]];
    parts.queryItems = @[[NSURLQueryItem queryItemWithName:@"query" value:query],
        [NSURLQueryItem queryItemWithName:@"fmt" value:@"json"],
        [NSURLQueryItem queryItemWithName:@"limit" value:@"4"]];
    return parts.URL;
}

- (void)discoverForArtists:(NSArray<NSString *> *)artists
                completion:(void (^)(NSArray<NSDictionary *> *, NSString *))completion
{
    NSArray *limited = [artists subarrayWithRange:NSMakeRange(0, MIN((NSUInteger)5, artists.count))];
    if (!limited.count) { completion(@[], @"Listen to some artists before checking discovery."); return; }
    NSMutableArray *results = [NSMutableArray array];
    dispatch_group_t group = dispatch_group_create();
    __block NSString *lastError;
    NSDateFormatter *formatter = [[NSDateFormatter alloc] init]; formatter.dateFormat = @"yyyy-MM-dd";
    NSString *today = [formatter stringFromDate:NSDate.date];
    NSString *future = [formatter stringFromDate:[NSDate dateWithTimeIntervalSinceNow:31536000]];
    for (NSString *artist in limited)
    {
        NSString *escaped = [self escapedQuery:artist];
        dispatch_group_enter(group);
        [self fetch:[self URLForPath:@"release-group/" query:[NSString stringWithFormat:
            @"artist:\"%@\" AND firstreleasedate:[%@ TO *]", escaped, today]]
        completion:^(NSDictionary *json, NSString *error) {
            for (NSDictionary *row in json[@"release-groups"] ?: @[])
                [results addObject:@{ @"kind": @"release", @"id": row[@"id"] ?: @"",
                    @"artist": artist, @"title": row[@"title"] ?: @"New release",
                    @"date": row[@"first-release-date"] ?: @"Upcoming" }];
            if (error.length) lastError = error; dispatch_group_leave(group);
        }];
        dispatch_group_enter(group);
        [self fetch:[self URLForPath:@"event/" query:[NSString stringWithFormat:
            @"artist:\"%@\" AND date:[%@ TO %@]", escaped, today, future]]
        completion:^(NSDictionary *json, NSString *error) {
            for (NSDictionary *row in json[@"events"] ?: @[])
                [results addObject:@{ @"kind": @"concert", @"id": row[@"id"] ?: @"",
                    @"artist": artist, @"title": row[@"name"] ?: @"Live show",
                    @"date": row[@"life-span"][@"begin"] ?: @"Upcoming" }];
            if (error.length) lastError = error; dispatch_group_leave(group);
        }];
    }
    dispatch_group_notify(group, dispatch_get_main_queue(), ^{
        completion(results, results.count ? nil : lastError);
    });
}

- (uint32_t)stableID:(NSString *)value
{
    NSData *data = [value dataUsingEncoding:NSUTF8StringEncoding];
    unsigned char digest[CC_MD5_DIGEST_LENGTH];
    CC_MD5(data.bytes, (CC_LONG)data.length, digest);
    uint32_t value32; memcpy(&value32, digest, sizeof(value32)); return value32 ?: 1;
}

- (NSURL *)notificationManifestForResults:(NSArray<NSDictionary *> *)results error:(NSError **)error
{
    NSMutableString *text = [NSMutableString stringWithString:@"# rockpod-notification-inbox-v1\n"];
    for (NSDictionary *row in results)
    {
        NSString *kind = [row[@"kind"] isEqual:@"concert"] ? @"Concert Alert" : @"New Release";
        NSString *body = [NSString stringWithFormat:@"%@ — %@ (%@)", row[@"artist"] ?: @"Artist",
            row[@"title"] ?: @"Music", row[@"date"] ?: @"Upcoming"];
        body = [[body stringByReplacingOccurrencesOfString:@"\t" withString:@" "]
                     stringByReplacingOccurrencesOfString:@"\n" withString:@" "];
        [text appendFormat:@"%08x\t%@\t%@\n", [self stableID:row[@"id"] ?: body], kind, body];
    }
    if (!results.count) return nil;
    NSURL *URL = [NSFileManager.defaultManager.temporaryDirectory URLByAppendingPathComponent:
        [NSString stringWithFormat:@"music-notifications-%@.tsv", NSUUID.UUID.UUIDString]];
    return [text writeToURL:URL atomically:YES encoding:NSUTF8StringEncoding error:error] ? URL : nil;
}

@end
