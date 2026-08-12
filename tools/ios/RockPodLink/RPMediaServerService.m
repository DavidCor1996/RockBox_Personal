#import "RPMediaServerService.h"
#import <CommonCrypto/CommonDigest.h>
#import <Security/Security.h>

@implementation RPMediaServerAccount
@end
@implementation RPMediaPlaylist
@end
@implementation RPMediaItem
@end

@implementation RPMediaServerService

static NSString * const RPAccountsDefaultsKey = @"RPMediaServerAccountsV1";
static NSString * const RPKeychainService = @"com.rockpod.link.media";

- (NSDictionary *)keychainQuery:(NSString *)identifier
{
    return @{ (__bridge id)kSecClass: (__bridge id)kSecClassGenericPassword,
        (__bridge id)kSecAttrService: RPKeychainService,
        (__bridge id)kSecAttrAccount: identifier ?: @"" };
}

- (BOOL)saveSecret:(NSString *)secret identifier:(NSString *)identifier
             error:(NSError **)error
{
    NSDictionary *query = [self keychainQuery:identifier];
    SecItemDelete((__bridge CFDictionaryRef)query);
    NSMutableDictionary *insert = query.mutableCopy;
    insert[(__bridge id)kSecValueData] = [secret dataUsingEncoding:NSUTF8StringEncoding];
    insert[(__bridge id)kSecAttrAccessible] = (__bridge id)kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly;
    OSStatus status = SecItemAdd((__bridge CFDictionaryRef)insert, NULL);
    if (status != errSecSuccess && error)
        *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:status userInfo:nil];
    return status == errSecSuccess;
}

- (NSString *)secretForIdentifier:(NSString *)identifier
{
    NSMutableDictionary *query = [self keychainQuery:identifier].mutableCopy;
    query[(__bridge id)kSecReturnData] = @YES;
    query[(__bridge id)kSecMatchLimit] = (__bridge id)kSecMatchLimitOne;
    CFTypeRef result = NULL;
    if (SecItemCopyMatching((__bridge CFDictionaryRef)query, &result) != errSecSuccess)
        return @"";
    NSData *data = CFBridgingRelease(result);
    return [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding] ?: @"";
}

- (BOOL)saveAccount:(RPMediaServerAccount *)account error:(NSError **)error
{
    if (!account.identifier.length)
        account.identifier = NSUUID.UUID.UUIDString;
    if (!account.baseURL.host.length || ![@[@"http", @"https"] containsObject:account.baseURL.scheme.lowercaseString])
    {
        if (error) *error = [NSError errorWithDomain:@"RockPodMedia" code:1 userInfo:
            @{NSLocalizedDescriptionKey: @"Enter a valid HTTP or HTTPS server URL."}];
        return NO;
    }
    if (![self saveSecret:account.secret ?: @"" identifier:account.identifier error:error])
        return NO;
    NSMutableArray *rows = [[NSUserDefaults.standardUserDefaults arrayForKey:RPAccountsDefaultsKey] mutableCopy] ?: [NSMutableArray array];
    NSIndexSet *old = [rows indexesOfObjectsPassingTest:^BOOL(NSDictionary *row, NSUInteger index, BOOL *stop) {
        (void)index; (void)stop; return [row[@"id"] isEqualToString:account.identifier];
    }];
    [rows removeObjectsAtIndexes:old];
    [rows addObject:@{ @"id": account.identifier, @"type": @(account.type),
        @"url": account.baseURL.absoluteString, @"username": account.username ?: @"",
        @"user_id": account.userID ?: @"", @"name": account.displayName ?: @"" }];
    [NSUserDefaults.standardUserDefaults setObject:rows forKey:RPAccountsDefaultsKey];
    return YES;
}

- (NSArray<RPMediaServerAccount *> *)savedAccounts
{
    NSMutableArray *accounts = [NSMutableArray array];
    for (NSDictionary *row in [NSUserDefaults.standardUserDefaults arrayForKey:RPAccountsDefaultsKey] ?: @[])
    {
        RPMediaServerAccount *account = [[RPMediaServerAccount alloc] init];
        account.identifier = row[@"id"] ?: @"";
        account.type = [row[@"type"] integerValue];
        account.baseURL = [NSURL URLWithString:row[@"url"] ?: @""];
        account.username = row[@"username"] ?: @"";
        account.userID = row[@"user_id"] ?: @"";
        account.displayName = row[@"name"] ?: @"";
        account.secret = [self secretForIdentifier:account.identifier];
        if (account.baseURL) [accounts addObject:account];
    }
    return accounts;
}

- (NSString *)md5:(NSString *)value
{
    NSData *data = [value dataUsingEncoding:NSUTF8StringEncoding];
    unsigned char digest[CC_MD5_DIGEST_LENGTH];
    CC_MD5(data.bytes, (CC_LONG)data.length, digest);
    NSMutableString *hex = [NSMutableString stringWithCapacity:CC_MD5_DIGEST_LENGTH * 2];
    for (NSUInteger i = 0; i < CC_MD5_DIGEST_LENGTH; i++) [hex appendFormat:@"%02x", digest[i]];
    return hex;
}

- (NSURL *)URLForAccount:(RPMediaServerAccount *)account path:(NSString *)path
                    query:(NSArray<NSURLQueryItem *> *)extra
{
    NSURL *url = [NSURL URLWithString:path relativeToURL:account.baseURL].absoluteURL;
    NSURLComponents *components = [NSURLComponents componentsWithURL:url resolvingAgainstBaseURL:NO];
    NSMutableArray *items = [NSMutableArray arrayWithArray:extra ?: @[]];
    if (account.type == RPMediaServerTypeSubsonic)
    {
        NSString *salt = [NSUUID.UUID.UUIDString stringByReplacingOccurrencesOfString:@"-" withString:@""];
        salt = [salt substringToIndex:12];
        [items addObjectsFromArray:@[
            [NSURLQueryItem queryItemWithName:@"u" value:account.username],
            [NSURLQueryItem queryItemWithName:@"t" value:[self md5:[account.secret stringByAppendingString:salt]]],
            [NSURLQueryItem queryItemWithName:@"s" value:salt],
            [NSURLQueryItem queryItemWithName:@"v" value:@"1.16.1"],
            [NSURLQueryItem queryItemWithName:@"c" value:@"RockPodLink"],
            [NSURLQueryItem queryItemWithName:@"f" value:@"json"] ]];
    }
    else if (account.type == RPMediaServerTypePlex && account.secret.length)
        [items addObject:[NSURLQueryItem queryItemWithName:@"X-Plex-Token" value:account.secret]];
    components.queryItems = items.count ? items : nil;
    return components.URL;
}

- (NSMutableURLRequest *)requestForAccount:(RPMediaServerAccount *)account
                                       URL:(NSURL *)url
{
    NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:url
        cachePolicy:NSURLRequestReloadIgnoringLocalCacheData timeoutInterval:30];
    [request setValue:@"application/json" forHTTPHeaderField:@"Accept"];
    [request setValue:@"RockPodLink/3.1" forHTTPHeaderField:@"User-Agent"];
    if (account.type == RPMediaServerTypeJellyfin && account.secret.length)
    {
        [request setValue:account.secret forHTTPHeaderField:@"X-Emby-Token"];
        [request setValue:[NSString stringWithFormat:
            @"MediaBrowser Client=\"RockPod Link\", Device=\"iPhone\", DeviceId=\"%@\", Version=\"3.1\"",
            NSUUID.UUID.UUIDString] forHTTPHeaderField:@"X-Emby-Authorization"];
    }
    else if (account.type == RPMediaServerTypePersonal && account.secret.length)
        [request setValue:[@"Bearer " stringByAppendingString:account.secret]
       forHTTPHeaderField:@"Authorization"];
    return request;
}

- (void)JSONForAccount:(RPMediaServerAccount *)account URL:(NSURL *)url
             completion:(void (^)(NSDictionary *, NSString *))completion
{
    NSURLSessionDataTask *task = [NSURLSession.sharedSession dataTaskWithRequest:
        [self requestForAccount:account URL:url]
        completionHandler:^(NSData *data, NSURLResponse *response, NSError *error) {
        NSHTTPURLResponse *http = (NSHTTPURLResponse *)response;
        id json = data.length ? [NSJSONSerialization JSONObjectWithData:data options:0 error:nil] : nil;
        NSString *message = error.localizedDescription;
        if (!message.length && (http.statusCode < 200 || http.statusCode >= 300))
            message = [NSString stringWithFormat:@"Server returned HTTP %ld", (long)http.statusCode];
        if (!message.length && ![json isKindOfClass:NSDictionary.class])
            message = @"Server did not return valid JSON.";
        dispatch_async(dispatch_get_main_queue(), ^{ completion(json, message); });
    }];
    [task resume];
}

- (NSURL *)endpointForAccount:(RPMediaServerAccount *)account purpose:(NSString *)purpose
{
    if ([purpose isEqualToString:@"ping"])
    {
        if (account.type == RPMediaServerTypeSubsonic) return [self URLForAccount:account path:@"rest/ping.view" query:nil];
        if (account.type == RPMediaServerTypeJellyfin) return [self URLForAccount:account path:@"System/Info" query:nil];
        if (account.type == RPMediaServerTypePlex) return [self URLForAccount:account path:@"identity" query:nil];
        return [self URLForAccount:account path:@"rockpod/v1/status.json" query:nil];
    }
    if (account.type == RPMediaServerTypeSubsonic) return [self URLForAccount:account path:@"rest/getPlaylists.view" query:nil];
    if (account.type == RPMediaServerTypeJellyfin) return [self URLForAccount:account path:
        [NSString stringWithFormat:@"Users/%@/Items", account.userID]
        query:@[[NSURLQueryItem queryItemWithName:@"IncludeItemTypes" value:@"Playlist"],
                [NSURLQueryItem queryItemWithName:@"Recursive" value:@"true"]]];
    if (account.type == RPMediaServerTypePlex) return [self URLForAccount:account path:@"playlists"
        query:@[[NSURLQueryItem queryItemWithName:@"playlistType" value:@"audio"]]];
    return [self URLForAccount:account path:@"rockpod/v1/playlists.json" query:nil];
}

- (void)testAccount:(RPMediaServerAccount *)account
          completion:(void (^)(BOOL, NSString *))completion
{
    [self JSONForAccount:account URL:[self endpointForAccount:account purpose:@"ping"]
        completion:^(NSDictionary *json, NSString *error) {
        completion(json != nil && !error.length, error ?: @"Server connection succeeded.");
    }];
}

- (NSArray *)arrayAtPath:(NSArray<NSString *> *)path object:(NSDictionary *)object
{
    id value = object;
    for (NSString *key in path)
        value = [value isKindOfClass:NSDictionary.class] ? value[key] : nil;
    if ([value isKindOfClass:NSDictionary.class]) return @[value];
    return [value isKindOfClass:NSArray.class] ? value : @[];
}

- (void)fetchPlaylistsForAccount:(RPMediaServerAccount *)account
                      completion:(void (^)(NSArray<RPMediaPlaylist *> *, NSString *))completion
{
    [self JSONForAccount:account URL:[self endpointForAccount:account purpose:@"playlists"]
        completion:^(NSDictionary *json, NSString *error) {
        if (!json) { completion(@[], error); return; }
        NSArray *rows;
        if (account.type == RPMediaServerTypeSubsonic)
            rows = [self arrayAtPath:@[@"subsonic-response", @"playlists", @"playlist"] object:json];
        else if (account.type == RPMediaServerTypeJellyfin)
            rows = [self arrayAtPath:@[@"Items"] object:json];
        else if (account.type == RPMediaServerTypePlex)
            rows = [self arrayAtPath:@[@"MediaContainer", @"Metadata"] object:json];
        else rows = [self arrayAtPath:@[@"playlists"] object:json];
        NSMutableArray *playlists = [NSMutableArray array];
        for (NSDictionary *row in rows)
        {
            RPMediaPlaylist *playlist = [[RPMediaPlaylist alloc] init];
            playlist.identifier = [NSString stringWithFormat:@"%@", row[@"id"] ?: row[@"Id"] ?: row[@"ratingKey"] ?: @""];
            playlist.name = row[@"name"] ?: row[@"Name"] ?: row[@"title"] ?: @"Playlist";
            playlist.trackCount = [row[@"songCount"] ?: row[@"ChildCount"] ?: row[@"leafCount"] integerValue];
            NSString *items = row[@"items_url"];
            if (items.length) playlist.itemsURL = [NSURL URLWithString:items relativeToURL:account.baseURL].absoluteURL;
            if (playlist.identifier.length) [playlists addObject:playlist];
        }
        completion(playlists, error);
    }];
}

- (NSURL *)itemsURLForPlaylist:(RPMediaPlaylist *)playlist account:(RPMediaServerAccount *)account
{
    if (playlist.itemsURL) return playlist.itemsURL;
    if (account.type == RPMediaServerTypeSubsonic) return [self URLForAccount:account path:@"rest/getPlaylist.view"
        query:@[[NSURLQueryItem queryItemWithName:@"id" value:playlist.identifier]]];
    if (account.type == RPMediaServerTypeJellyfin) return [self URLForAccount:account path:
        [NSString stringWithFormat:@"Playlists/%@/Items", playlist.identifier]
        query:@[[NSURLQueryItem queryItemWithName:@"UserId" value:account.userID]]];
    if (account.type == RPMediaServerTypePlex) return [self URLForAccount:account path:
        [NSString stringWithFormat:@"playlists/%@/items", playlist.identifier] query:nil];
    return [self URLForAccount:account path:[NSString stringWithFormat:
        @"rockpod/v1/playlists/%@.json", playlist.identifier] query:nil];
}

- (void)fetchItemsForPlaylist:(RPMediaPlaylist *)playlist
                      account:(RPMediaServerAccount *)account
                   completion:(void (^)(NSArray<RPMediaItem *> *, NSString *))completion
{
    [self JSONForAccount:account URL:[self itemsURLForPlaylist:playlist account:account]
        completion:^(NSDictionary *json, NSString *error) {
        if (!json) { completion(@[], error); return; }
        NSArray *rows;
        if (account.type == RPMediaServerTypeSubsonic)
            rows = [self arrayAtPath:@[@"subsonic-response", @"playlist", @"entry"] object:json];
        else if (account.type == RPMediaServerTypeJellyfin)
            rows = [self arrayAtPath:@[@"Items"] object:json];
        else if (account.type == RPMediaServerTypePlex)
            rows = [self arrayAtPath:@[@"MediaContainer", @"Metadata"] object:json];
        else rows = [self arrayAtPath:@[@"tracks"] object:json];
        NSMutableArray *items = [NSMutableArray array];
        for (NSDictionary *row in rows)
        {
            RPMediaItem *item = [[RPMediaItem alloc] init];
            item.identifier = [NSString stringWithFormat:@"%@", row[@"id"] ?: row[@"Id"] ?: row[@"ratingKey"] ?: @""];
            item.title = row[@"title"] ?: row[@"Name"] ?: @"Track";
            item.artist = row[@"artist"] ?: row[@"AlbumArtist"] ?: row[@"grandparentTitle"] ?: @"";
            item.album = row[@"album"] ?: row[@"Album"] ?: row[@"parentTitle"] ?: @"";
            item.durationMS = [row[@"duration"] integerValue];
            if (account.type == RPMediaServerTypeJellyfin) item.durationMS = [row[@"RunTimeTicks"] longLongValue] / 10000;
            item.suffix = row[@"suffix"] ?: row[@"Container"] ?: @"m4a";
            item.size = [row[@"size"] unsignedLongLongValue];
            item.downloadPermitted = account.type != RPMediaServerTypePersonal || [row[@"download_permitted"] boolValue];
            NSArray *media = [row[@"Media"] isKindOfClass:NSArray.class] ? row[@"Media"] : @[];
            NSArray *parts = media.count && [media[0][@"Part"] isKindOfClass:NSArray.class] ? media[0][@"Part"] : @[];
            NSString *direct = row[@"download_url"] ?: row[@"media_url"] ?: (parts.count ? parts[0][@"key"] : nil);
            if (account.type == RPMediaServerTypeSubsonic)
                item.mediaURL = [self URLForAccount:account path:@"rest/download.view"
                    query:@[[NSURLQueryItem queryItemWithName:@"id" value:item.identifier]]];
            else if (account.type == RPMediaServerTypeJellyfin)
            {
                NSMutableArray *query = [NSMutableArray arrayWithObject:
                    [NSURLQueryItem queryItemWithName:@"static" value:@"true"]];
                if (account.secret.length)
                    [query addObject:[NSURLQueryItem queryItemWithName:@"api_key" value:account.secret]];
                item.mediaURL = [self URLForAccount:account path:[NSString stringWithFormat:@"Audio/%@/stream", item.identifier]
                    query:query];
            }
            else if (direct.length)
            {
                NSURL *directURL = [NSURL URLWithString:direct relativeToURL:account.baseURL].absoluteURL;
                item.mediaURL = account.type == RPMediaServerTypePlex ?
                    [self URLForAccount:account path:directURL.absoluteString query:nil] : directURL;
            }
            if (account.type == RPMediaServerTypeJellyfin)
                item.headers = @{ @"X-Emby-Token": account.secret ?: @"" };
            else if (account.type == RPMediaServerTypePersonal && account.secret.length)
                item.headers = @{ @"Authorization": [@"Bearer " stringByAppendingString:account.secret] };
            else item.headers = @{};
            if (item.identifier.length && item.mediaURL) [items addObject:item];
        }
        completion(items, error);
    }];
}

- (void)downloadItem:(RPMediaItem *)item
           completion:(void (^)(NSURL *, NSString *))completion
{
    if (!item.downloadPermitted || !item.mediaURL)
    {
        completion(nil, @"This server did not permit an offline download.");
        return;
    }
    NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:item.mediaURL
        cachePolicy:NSURLRequestReloadIgnoringLocalCacheData timeoutInterval:120];
    for (NSString *header in item.headers) [request setValue:item.headers[header] forHTTPHeaderField:header];
    NSURLSessionDownloadTask *task = [NSURLSession.sharedSession downloadTaskWithRequest:request
        completionHandler:^(NSURL *temporary, NSURLResponse *response, NSError *error) {
        (void)response;
        NSString *message = error.localizedDescription;
        NSURL *output = nil;
        if (temporary && !message.length)
        {
            NSString *name = [NSString stringWithFormat:@"%@.%@", NSUUID.UUID.UUIDString,
                              item.suffix.length ? item.suffix : @"m4a"];
            output = [NSFileManager.defaultManager.temporaryDirectory URLByAppendingPathComponent:name];
            [NSFileManager.defaultManager removeItemAtURL:output error:nil];
            NSError *moveError = nil;
            if (![NSFileManager.defaultManager moveItemAtURL:temporary toURL:output error:&moveError])
            { output = nil; message = moveError.localizedDescription; }
        }
        dispatch_async(dispatch_get_main_queue(), ^{ completion(output, message); });
    }];
    [task resume];
}

- (void)syncAudiobookPositionsForAccount:(RPMediaServerAccount *)account
                           localPositions:(NSArray<NSDictionary *> *)localPositions
                              completion:(void (^)(NSArray<NSDictionary *> *, NSString *))completion
{
    if (account.type != RPMediaServerTypePersonal)
    {
        completion(@[], @"Two-way audiobook positions currently require a personal RockPod server feed.");
        return;
    }
    NSURL *URL = [self URLForAccount:account path:@"rockpod/v1/audiobook-positions.json" query:nil];
    NSMutableURLRequest *request = [self requestForAccount:account URL:URL];
    request.HTTPMethod = @"POST";
    [request setValue:@"application/json" forHTTPHeaderField:@"Content-Type"];
    request.HTTPBody = [NSJSONSerialization dataWithJSONObject:@{ @"positions": localPositions ?: @[] }
                                                       options:0 error:nil];
    NSURLSessionDataTask *task = [NSURLSession.sharedSession dataTaskWithRequest:request
        completionHandler:^(NSData *data, NSURLResponse *response, NSError *error) {
        NSHTTPURLResponse *http = (NSHTTPURLResponse *)response;
        NSDictionary *json = data.length ? [NSJSONSerialization JSONObjectWithData:data options:0 error:nil] : nil;
        NSArray *positions = [json[@"positions"] isKindOfClass:NSArray.class] ? json[@"positions"] : @[];
        NSString *message = error.localizedDescription;
        if (!message.length && (http.statusCode < 200 || http.statusCode >= 300))
            message = [NSString stringWithFormat:@"Position sync returned HTTP %ld", (long)http.statusCode];
        dispatch_async(dispatch_get_main_queue(), ^{ completion(positions, message); });
    }];
    [task resume];
}

@end
