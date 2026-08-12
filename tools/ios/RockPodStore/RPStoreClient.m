#import "RPStoreClient.h"
#import <Security/Security.h>

static NSString * const RPBaseURLKey = @"RPStoreBaseURL";
static NSString * const RPTokenDefaultsKey = @"RPStoreBearerToken";
static NSString * const RPTokenService = @"com.rockpod.store.token";
static NSString * const RPLibraryIDKey = @"RPStoreMusicLibraryID";

@implementation RPStoreClient {
    NSString *_token;
    NSString *_libraryID;
}

+ (instancetype)sharedClient {
    static RPStoreClient *client;
    static dispatch_once_t once;
    dispatch_once(&once, ^{ client = [[self alloc] init]; });
    return client;
}

- (id)init {
    if ((self = [super init])) {
        _baseURL = [[[NSUserDefaults standardUserDefaults] stringForKey:RPBaseURLKey] copy];
        if (!_baseURL.length) _baseURL = @"http://127.0.0.1:8732";
        _token = [[self keychainToken] copy];
        if (!_token.length) _token = [[[NSUserDefaults standardUserDefaults] stringForKey:RPTokenDefaultsKey] copy];
        _libraryID = [[[NSUserDefaults standardUserDefaults] stringForKey:RPLibraryIDKey] copy];
        if (!_libraryID.length) {
            CFUUIDRef UUID = CFUUIDCreate(NULL);
            _libraryID = [(__bridge_transfer NSString *)CFUUIDCreateString(NULL, UUID) copy];
            CFRelease(UUID);
            [[NSUserDefaults standardUserDefaults] setObject:_libraryID forKey:RPLibraryIDKey];
            [[NSUserDefaults standardUserDefaults] synchronize];
        }
    }
    return self;
}

- (void)setBaseURL:(NSString *)baseURL {
    NSString *clean = [baseURL stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
    while ([clean hasSuffix:@"/"]) clean = [clean substringToIndex:clean.length - 1];
    _baseURL = [clean copy];
    [[NSUserDefaults standardUserDefaults] setObject:_baseURL forKey:RPBaseURLKey];
    [[NSUserDefaults standardUserDefaults] synchronize];
}

- (BOOL)paired { return _token.length > 0; }
- (NSString *)libraryID { return _libraryID; }

- (NSMutableURLRequest *)requestForPath:(NSString *)path method:(NSString *)method {
    NSURL *URL = [NSURL URLWithString:[self.baseURL stringByAppendingString:path]];
    NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:URL cachePolicy:NSURLRequestReloadIgnoringLocalCacheData timeoutInterval:120.0];
    request.HTTPMethod = method;
    [request setValue:@"application/json" forHTTPHeaderField:@"Accept"];
    if (_token.length) [request setValue:[@"Bearer " stringByAppendingString:_token] forHTTPHeaderField:@"Authorization"];
    [request setValue:self.libraryID forHTTPHeaderField:@"X-RockPod-Library-ID"];
    return request;
}

- (void)downloadPath:(NSString *)path toFile:(NSString *)file completion:(RPStoreCompletion)completion {
    NSMutableURLRequest *request = [self requestForPath:path method:@"GET"];
    request.timeoutInterval = 300.0;
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        NSURLResponse *response = nil;
        NSError *error = nil;
        NSData *data = [NSURLConnection sendSynchronousRequest:request returningResponse:&response error:&error];
        NSInteger status = [(NSHTTPURLResponse *)response statusCode];
        if (!error && (status < 200 || status >= 300)) {
            error = [NSError errorWithDomain:@"RockPodStore" code:status userInfo:@{
                NSLocalizedDescriptionKey: @"The RockPod host could not transfer this song."
            }];
        }
        if (!error && ![data writeToFile:file options:NSDataWritingAtomic error:&error]) data = nil;
        dispatch_async(dispatch_get_main_queue(), ^{
            if (completion) completion(data ? file : nil, error);
        });
    });
}

- (void)perform:(NSMutableURLRequest *)request completion:(RPStoreCompletion)completion {
    [NSURLConnection sendAsynchronousRequest:request queue:[NSOperationQueue mainQueue] completionHandler:^(NSURLResponse *response, NSData *data, NSError *transportError) {
        if (transportError) { if (completion) completion(nil, transportError); return; }
        NSError *JSONError = nil;
        NSDictionary *envelope = data.length ? [NSJSONSerialization JSONObjectWithData:data options:0 error:&JSONError] : nil;
        NSInteger status = [(NSHTTPURLResponse *)response statusCode];
        if (JSONError || ![envelope isKindOfClass:[NSDictionary class]]) {
            NSError *error = [NSError errorWithDomain:@"RockPodStore" code:status userInfo:@{NSLocalizedDescriptionKey: @"The RockPod host returned invalid data."}];
            if (completion) completion(nil, error); return;
        }
        if (status < 200 || status >= 300 || ![envelope[@"ok"] boolValue]) {
            NSString *message = envelope[@"error"][@"message"] ?: @"The RockPod request failed.";
            NSError *error = [NSError errorWithDomain:@"RockPodStore" code:status userInfo:@{NSLocalizedDescriptionKey: message}];
            if (completion) completion(nil, error); return;
        }
        if (completion) completion(envelope[@"data"], nil);
    }];
}

- (void)getPath:(NSString *)path completion:(RPStoreCompletion)completion {
    [self perform:[self requestForPath:path method:@"GET"] completion:completion];
}

- (void)postPath:(NSString *)path body:(NSDictionary *)body idempotencyKey:(NSString *)key completion:(RPStoreCompletion)completion {
    NSMutableURLRequest *request = [self requestForPath:path method:@"POST"];
    [request setValue:@"application/json" forHTTPHeaderField:@"Content-Type"];
    if (key.length) [request setValue:key forHTTPHeaderField:@"Idempotency-Key"];
    request.HTTPBody = [NSJSONSerialization dataWithJSONObject:body ?: @{} options:0 error:nil];
    [self perform:request completion:completion];
}

- (void)pairWithCode:(NSString *)code completion:(RPStoreCompletion)completion {
    [self postPath:@"/v1/pair" body:@{ @"code": code ?: @"", @"name": @"iPod touch" } idempotencyKey:nil completion:^(id data, NSError *error) {
        if (!error && [data[@"token"] length]) {
            _token = [data[@"token"] copy];
            [[NSUserDefaults standardUserDefaults] setObject:_token forKey:RPTokenDefaultsKey];
            [[NSUserDefaults standardUserDefaults] synchronize];
            [self saveKeychainToken:_token];
        }
        if (completion) completion(data, error);
    }];
}

- (NSURL *)URLForArtworkPath:(NSString *)path {
    if (![path length]) return nil;
    if ([path hasPrefix:@"http://"] || [path hasPrefix:@"https://"]) return [NSURL URLWithString:path];
    return [NSURL URLWithString:[self.baseURL stringByAppendingString:path]];
}

- (NSMutableDictionary *)keychainQuery {
    return [@{(__bridge id)kSecClass: (__bridge id)kSecClassGenericPassword,
              (__bridge id)kSecAttrService: RPTokenService,
              (__bridge id)kSecAttrAccount: @"bearer"} mutableCopy];
}

- (NSString *)keychainToken {
    NSMutableDictionary *query = [self keychainQuery];
    query[(__bridge id)kSecReturnData] = @YES;
    query[(__bridge id)kSecMatchLimit] = (__bridge id)kSecMatchLimitOne;
    CFTypeRef result = NULL;
    if (SecItemCopyMatching((__bridge CFDictionaryRef)query, &result) != errSecSuccess) return nil;
    NSData *data = (__bridge_transfer NSData *)result;
    return [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
}

- (void)saveKeychainToken:(NSString *)token {
    NSMutableDictionary *query = [self keychainQuery];
    SecItemDelete((__bridge CFDictionaryRef)query);
    query[(__bridge id)kSecValueData] = [token dataUsingEncoding:NSUTF8StringEncoding];
    SecItemAdd((__bridge CFDictionaryRef)query, NULL);
}
@end
