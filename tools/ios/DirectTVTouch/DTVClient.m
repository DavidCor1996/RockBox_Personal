#import "DTVClient.h"
#import <Security/Security.h>

static NSString * const DTVBaseURLKey = @"DTVRockPodBaseURL";
static NSString * const DTVTokenDefaultsKey = @"DTVRockPodBearerToken";
static NSString * const DTVTokenService = @"com.rockpod.directtv.host";
static NSString * const RPStoreBaseURLKey = @"RPStoreBaseURL";
static NSString * const RPStoreTokenDefaultsKey = @"RPStoreBearerToken";
static NSString * const RPStoreTokenService = @"com.rockpod.store.token";
static NSString * const RPStorePreferencesDomain = @"com.rockpod.store";

@interface DTVDownload : NSObject <NSURLConnectionDataDelegate>
@property(nonatomic, strong) NSMutableURLRequest *request;
@property(nonatomic, copy) NSString *destination;
@property(nonatomic) unsigned long long expectedBytes;
@property(nonatomic) unsigned long long resumeOffset;
@property(nonatomic, strong) NSFileHandle *handle;
@property(nonatomic, strong) NSError *error;
@property(nonatomic) BOOL finished;
- (NSString *)run;
@end

@implementation DTVDownload

- (NSError *)errorWithCode:(NSInteger)code message:(NSString *)message {
    return [NSError errorWithDomain:@"DirectTVDownload" code:code
        userInfo:@{NSLocalizedDescriptionKey: message ?: @"Live TV transfer failed."}];
}

- (unsigned long long)size:(NSString *)path {
    return [[[[NSFileManager defaultManager] attributesOfItemAtPath:path error:nil]
        objectForKey:NSFileSize] unsignedLongLongValue];
}

- (NSString *)run {
    NSFileManager *manager = [NSFileManager defaultManager];
    if ([manager fileExistsAtPath:self.destination] &&
            (!self.expectedBytes || [self size:self.destination] == self.expectedBytes))
        return self.destination;
    NSString *partial = [self.destination stringByAppendingString:@".part"];
    self.resumeOffset = [self size:partial];
    if (self.expectedBytes && self.resumeOffset > self.expectedBytes) {
        [manager removeItemAtPath:partial error:nil]; self.resumeOffset = 0;
    }
    if (self.resumeOffset) [self.request setValue:
        [NSString stringWithFormat:@"bytes=%llu-", self.resumeOffset]
        forHTTPHeaderField:@"Range"];
    NSURLConnection *connection = [[NSURLConnection alloc]
        initWithRequest:self.request delegate:self startImmediately:NO];
    [connection scheduleInRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];
    [connection start];
    while (!self.finished) [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode
        beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.2]];
    [self.handle closeFile]; self.handle = nil;
    if (self.error) return nil;
    if (self.expectedBytes && [self size:partial] != self.expectedBytes) {
        self.error = [self errorWithCode:-2 message:@"The channel transfer is incomplete."];
        return nil;
    }
    [manager removeItemAtPath:self.destination error:nil];
    NSError *moveError = nil;
    if (![manager moveItemAtPath:partial toPath:self.destination error:&moveError]) {
        self.error = moveError; return nil;
    }
    return self.destination;
}

- (void)connection:(NSURLConnection *)connection didReceiveResponse:(NSURLResponse *)response {
    NSInteger status = [(NSHTTPURLResponse *)response statusCode];
    if (status == 416 && self.expectedBytes && self.resumeOffset == self.expectedBytes) {
        [connection cancel]; self.finished = YES; return;
    }
    if (status < 200 || status >= 300) {
        self.error = [self errorWithCode:status message:@"RockPod could not transfer this channel."];
        [connection cancel]; self.finished = YES; return;
    }
    NSString *partial = [self.destination stringByAppendingString:@".part"];
    BOOL append = status == 206 && self.resumeOffset > 0;
    if (!append) { [[NSFileManager defaultManager] removeItemAtPath:partial error:nil]; self.resumeOffset = 0; }
    if (![[NSFileManager defaultManager] fileExistsAtPath:partial])
        [[NSFileManager defaultManager] createFileAtPath:partial contents:nil attributes:nil];
    self.handle = [NSFileHandle fileHandleForWritingAtPath:partial];
    if (!self.handle) {
        self.error = [self errorWithCode:-3 message:@"DIRECTV could not save the channel file."];
        [connection cancel]; self.finished = YES; return;
    }
    if (append) [self.handle seekToEndOfFile]; else [self.handle truncateFileAtOffset:0];
}
- (void)connection:(NSURLConnection *)connection didReceiveData:(NSData *)data { (void)connection; [self.handle writeData:data]; }
- (void)connectionDidFinishLoading:(NSURLConnection *)connection { (void)connection; self.finished = YES; }
- (void)connection:(NSURLConnection *)connection didFailWithError:(NSError *)error { (void)connection; self.error = error; self.finished = YES; }
@end

@interface DTVClient ()
- (void)importRockPodStorePairing;
- (NSString *)keychainToken;
- (NSString *)keychainTokenForService:(NSString *)service;
- (void)saveKeychainToken:(NSString *)token;
@end

@implementation DTVClient { NSString *_token; }

+ (instancetype)sharedClient {
    static DTVClient *client; static dispatch_once_t once;
    dispatch_once(&once, ^{ client = [[self alloc] init]; }); return client;
}

- (id)init {
    if ((self = [super init])) {
        _baseURL = [[[NSUserDefaults standardUserDefaults] stringForKey:DTVBaseURLKey] copy];
        if (!_baseURL.length) _baseURL = @"http://127.0.0.1:8732";
        _token = [[self keychainToken] copy];
        if (!_token.length) _token = [[[NSUserDefaults standardUserDefaults]
            stringForKey:DTVTokenDefaultsKey] copy];
        if (!_token.length) [self importRockPodStorePairing];
    }
    return self;
}

- (void)importRockPodStorePairing {
    NSDictionary *preferences = [[NSUserDefaults standardUserDefaults]
        persistentDomainForName:RPStorePreferencesDomain];
    if (!preferences.count) {
        preferences = [NSDictionary dictionaryWithContentsOfFile:
            @"/var/mobile/Library/Preferences/com.rockpod.store.plist"];
    }
    NSString *token = [self keychainTokenForService:RPStoreTokenService];
    if (!token.length) token = preferences[RPStoreTokenDefaultsKey];
    NSString *baseURL = preferences[RPStoreBaseURLKey];
    if (!token.length) return;

    _token = [token copy];
    if (baseURL.length) _baseURL = [baseURL copy];
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    [defaults setObject:_token forKey:DTVTokenDefaultsKey];
    [defaults setObject:_baseURL forKey:DTVBaseURLKey];
    [defaults synchronize];
    [self saveKeychainToken:_token];
}

- (void)setBaseURL:(NSString *)baseURL {
    NSString *clean = [baseURL stringByTrimmingCharactersInSet:
        [NSCharacterSet whitespaceAndNewlineCharacterSet]];
    while ([clean hasSuffix:@"/"]) clean = [clean substringToIndex:clean.length - 1];
    _baseURL = [clean copy];
    [[NSUserDefaults standardUserDefaults] setObject:_baseURL forKey:DTVBaseURLKey];
    [[NSUserDefaults standardUserDefaults] synchronize];
}
- (BOOL)paired { return _token.length > 0; }

- (NSMutableURLRequest *)request:(NSString *)path method:(NSString *)method {
    NSURL *URL = [NSURL URLWithString:[self.baseURL stringByAppendingString:path]];
    NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:URL
        cachePolicy:NSURLRequestReloadIgnoringLocalCacheData timeoutInterval:120.0];
    request.HTTPMethod = method;
    [request setValue:@"application/json" forHTTPHeaderField:@"Accept"];
    if (_token.length) [request setValue:[@"Bearer " stringByAppendingString:_token]
        forHTTPHeaderField:@"Authorization"];
    return request;
}

- (void)perform:(NSMutableURLRequest *)request completion:(DTVCompletion)completion {
    [NSURLConnection sendAsynchronousRequest:request queue:[NSOperationQueue mainQueue]
        completionHandler:^(NSURLResponse *response, NSData *data, NSError *transportError) {
        if (transportError) { if (completion) completion(nil, transportError); return; }
        NSError *JSONError = nil;
        NSDictionary *envelope = data.length ? [NSJSONSerialization JSONObjectWithData:data options:0 error:&JSONError] : nil;
        NSInteger status = [(NSHTTPURLResponse *)response statusCode];
        if (JSONError || ![envelope isKindOfClass:[NSDictionary class]]) {
            if (completion) completion(nil, [NSError errorWithDomain:@"DirectTV" code:status
                userInfo:@{NSLocalizedDescriptionKey:@"RockPod returned invalid Live TV data."}]);
            return;
        }
        if (status < 200 || status >= 300 || ![envelope[@"ok"] boolValue]) {
            NSString *message = envelope[@"error"][@"message"] ?: @"The RockPod request failed.";
            if (completion) completion(nil, [NSError errorWithDomain:@"DirectTV" code:status
                userInfo:@{NSLocalizedDescriptionKey:message}]);
            return;
        }
        if (completion) completion(envelope[@"data"], nil);
    }];
}

- (void)getPath:(NSString *)path completion:(DTVCompletion)completion { [self perform:[self request:path method:@"GET"] completion:completion]; }
- (void)postPath:(NSString *)path body:(NSDictionary *)body completion:(DTVCompletion)completion {
    NSMutableURLRequest *request = [self request:path method:@"POST"];
    [request setValue:@"application/json" forHTTPHeaderField:@"Content-Type"];
    request.HTTPBody = [NSJSONSerialization dataWithJSONObject:body ?: @{} options:0 error:nil];
    [self perform:request completion:completion];
}

- (void)downloadPath:(NSString *)path toFile:(NSString *)file expectedBytes:(unsigned long long)bytes completion:(DTVCompletion)completion {
    NSMutableURLRequest *request = [self request:path method:@"GET"];
    /* Full-length channels can be hundreds of megabytes on 802.11g-era
       hardware. Keep a generous request window; Range still resumes genuine
       disconnects without discarding the partial file. */
    request.timeoutInterval = 1800.0;
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        DTVDownload *download = [[DTVDownload alloc] init]; download.request = request;
        download.destination = file; download.expectedBytes = bytes;
        NSString *result = [download run];
        dispatch_async(dispatch_get_main_queue(), ^{ if (completion) completion(result, download.error); });
    });
}

- (void)pairWithCode:(NSString *)code completion:(DTVCompletion)completion {
    [self postPath:@"/v1/pair" body:@{@"code":code ?: @"", @"name":@"DIRECTV on iPod touch 4G"}
        completion:^(id data, NSError *error) {
        if (!error && [data[@"token"] length]) {
            _token = [data[@"token"] copy];
            [[NSUserDefaults standardUserDefaults] setObject:_token forKey:DTVTokenDefaultsKey];
            [[NSUserDefaults standardUserDefaults] synchronize]; [self saveKeychainToken:_token];
        }
        if (completion) completion(data, error);
    }];
}

- (NSMutableDictionary *)keychainQuery {
    return [@{(__bridge id)kSecClass:(__bridge id)kSecClassGenericPassword,
        (__bridge id)kSecAttrService:DTVTokenService,
        (__bridge id)kSecAttrAccount:@"bearer"} mutableCopy];
}
- (NSString *)keychainTokenForService:(NSString *)service {
    NSMutableDictionary *query = [self keychainQuery];
    query[(__bridge id)kSecAttrService] = service;
    query[(__bridge id)kSecReturnData] = @YES;
    query[(__bridge id)kSecMatchLimit] = (__bridge id)kSecMatchLimitOne;
    CFTypeRef result = NULL;
    if (SecItemCopyMatching((__bridge CFDictionaryRef)query, &result) != errSecSuccess) return nil;
    return [[NSString alloc] initWithData:(__bridge_transfer NSData *)result encoding:NSUTF8StringEncoding];
}
- (NSString *)keychainToken {
    return [self keychainTokenForService:DTVTokenService];
}
- (void)saveKeychainToken:(NSString *)token {
    NSMutableDictionary *query = [self keychainQuery]; SecItemDelete((__bridge CFDictionaryRef)query);
    query[(__bridge id)kSecValueData] = [token dataUsingEncoding:NSUTF8StringEncoding];
    SecItemAdd((__bridge CFDictionaryRef)query, NULL);
}
@end
