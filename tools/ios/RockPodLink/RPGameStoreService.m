#import "RPGameStoreService.h"
#import <CommonCrypto/CommonDigest.h>

@implementation RPGameStoreItem
@end

@implementation RPGameInstall
@end

@implementation RPGameStoreService

+ (NSSet<NSString *> *)supportedExtensions
{
    return [NSSet setWithArray:@[@"gb", @"gbc", @"nes", @"sms", @"gg",
                                  @"sg", @"mgw", @"gw", @"gwz", @"sfc",
                                  @"smc", @"md", @"gen", @"bin", @"smd"]];
}

+ (BOOL)isSupportedFilename:(NSString *)filename
{
    return [[self supportedExtensions] containsObject:filename.pathExtension.lowercaseString];
}

+ (NSString *)string:(id)value
{
    if ([value isKindOfClass:NSString.class])
        return [(NSString *)value stringByTrimmingCharactersInSet:
                NSCharacterSet.whitespaceAndNewlineCharacterSet];
    if ([value respondsToSelector:@selector(stringValue)])
        return [value stringValue];
    return @"";
}

+ (NSString *)absoluteURLString:(NSString *)value base:(NSURL *)base
{
    NSURL *url = [NSURL URLWithString:value relativeToURL:base];
    return url.absoluteURL.absoluteString ?: @"";
}

+ (NSString *)plainTitle:(NSString *)value fallback:(NSString *)fallback
{
    NSString *title = value ?: @"";
    NSRegularExpression *tags = [NSRegularExpression
        regularExpressionWithPattern:@"<[^>]+>" options:0 error:nil];
    title = [tags stringByReplacingMatchesInString:title options:0
        range:NSMakeRange(0, title.length) withTemplate:@" "];
    NSDictionary *entities = @{@"&amp;": @"&", @"&quot;": @"\"",
        @"&#39;": @"'", @"&lt;": @"<", @"&gt;": @">",
        @"&nbsp;": @" "};
    for (NSString *entity in entities)
        title = [title stringByReplacingOccurrencesOfString:entity
                                                withString:entities[entity]];
    title = [title stringByTrimmingCharactersInSet:
        NSCharacterSet.whitespaceAndNewlineCharacterSet];
    while ([title containsString:@"  "])
        title = [title stringByReplacingOccurrencesOfString:@"  " withString:@" "];
    return title.length ? title : fallback;
}

+ (void)addPageItemURL:(NSString *)value title:(NSString *)title
                   base:(NSURL *)base rows:(NSMutableArray *)rows
                   seen:(NSMutableSet *)seen
{
    NSString *absolute = [self absoluteURLString:value base:base];
    NSURL *url = [NSURL URLWithString:absolute];
    NSString *filename = url.lastPathComponent.stringByRemovingPercentEncoding ?:
                         url.lastPathComponent;
    if (![self isSupportedFilename:filename] || [seen containsObject:absolute])
        return;
    [seen addObject:absolute];
    [rows addObject:@{@"title": [self plainTitle:title
        fallback:filename.stringByDeletingPathExtension],
        @"filename": filename, @"download_url": absolute}];
}

+ (RPGameStoreItem *)itemFromDictionary:(NSDictionary *)row base:(NSURL *)base
{
    if (![row isKindOfClass:NSDictionary.class])
        return nil;
    RPGameStoreItem *item = [[RPGameStoreItem alloc] init];
    item.identifier = [self string:row[@"id"]];
    item.title = [self string:row[@"title"]];
    item.platform = [self string:row[@"platform"]];
    item.downloadURL = [self absoluteURLString:
        [self string:row[@"download_url"] ?: row[@"downloadUrl"]] base:base];
    item.coverURL = [self absoluteURLString:
        [self string:row[@"cover_url"] ?: row[@"image"]] base:base];
    item.filename = [self string:row[@"filename"]];
    if (!item.filename.length)
        item.filename = [NSURL URLWithString:item.downloadURL].lastPathComponent;
    if (!item.title.length)
        item.title = item.filename.stringByDeletingPathExtension;
    item.year = [self string:row[@"year"]];
    item.genre = [self string:row[@"genre"]];
    item.publisher = [self string:row[@"publisher"]];
    item.developer = [self string:row[@"developer"]];
    item.gameDescription = [self string:row[@"description"]];
    item.sha256 = [self string:row[@"sha256"]].lowercaseString;
    item.licenseName = [self string:row[@"license"]];
    if (!item.downloadURL.length || ![self isSupportedFilename:item.filename])
        return nil;
    return item;
}

- (void)loadStoreURL:(NSURL *)url
          completion:(void (^)(NSArray<RPGameStoreItem *> *, NSString *))completion
{
    if (!url || ![@[@"http", @"https"] containsObject:url.scheme.lowercaseString])
    {
        completion(@[], @"Enter a valid HTTP or HTTPS Game Store URL.");
        return;
    }
    NSString *directFilename = url.lastPathComponent.stringByRemovingPercentEncoding ?:
                               url.lastPathComponent;
    if ([self.class isSupportedFilename:directFilename])
    {
        RPGameStoreItem *direct = [self.class itemFromDictionary:@{
            @"title": directFilename.stringByDeletingPathExtension,
            @"filename": directFilename,
            @"download_url": url.absoluteString} base:url];
        completion(direct ? @[direct] : @[], direct ? nil :
            @"That direct game URL is not supported.");
        return;
    }
    NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:url];
    [request setValue:@"Mozilla/5.0 (iPhone; RockPod Link) AppleWebKit/605.1.15"
   forHTTPHeaderField:@"User-Agent"];
    [request setValue:@"application/json, text/html;q=0.9, */*;q=0.8"
   forHTTPHeaderField:@"Accept"];
    NSURLSessionDataTask *task = [NSURLSession.sharedSession dataTaskWithRequest:request
        completionHandler:^(NSData *data, NSURLResponse *response, NSError *error) {
        if (!data.length || error)
        {
            dispatch_async(dispatch_get_main_queue(), ^{
                completion(@[], error.localizedDescription ?: @"The game store could not be loaded.");
            });
            return;
        }
        NSMutableArray *rows = [NSMutableArray array];
        NSMutableSet *seen = [NSMutableSet set];
        NSURL *baseURL = response.URL ?: url;
        id json = [NSJSONSerialization JSONObjectWithData:data options:0 error:nil];
        if ([json isKindOfClass:NSArray.class])
            [rows addObjectsFromArray:json];
        else if ([json isKindOfClass:NSDictionary.class])
        {
            id games = json[@"games"] ?: json[@"items"];
            if ([games isKindOfClass:NSArray.class])
                [rows addObjectsFromArray:games];
        }
        if (!rows.count)
        {
            NSString *html = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
            NSRegularExpression *script = [NSRegularExpression regularExpressionWithPattern:
                @"<script[^>]+type=[\\\"']application/ld\\+json[\\\"'][^>]*>(.*?)</script>"
                options:NSRegularExpressionCaseInsensitive | NSRegularExpressionDotMatchesLineSeparators
                error:nil];
            for (NSTextCheckingResult *match in [script matchesInString:html ?: @"" options:0
                                                                  range:NSMakeRange(0, html.length)])
            {
                NSString *payload = [html substringWithRange:[match rangeAtIndex:1]];
                NSData *payloadData = [payload dataUsingEncoding:NSUTF8StringEncoding];
                id object = [NSJSONSerialization JSONObjectWithData:payloadData options:0 error:nil];
                if ([object isKindOfClass:NSDictionary.class])
                {
                    NSDictionary *source = object;
                    NSDictionary *offer = [source[@"offers"] isKindOfClass:NSDictionary.class] ? source[@"offers"] : @{};
                    NSString *download = [self.class string:source[@"downloadUrl"] ?: offer[@"url"]];
                    if (download.length)
                    {
                        [rows addObject:@{@"title": [self.class string:source[@"name"]],
                                          @"description": [self.class string:source[@"description"]],
                                          @"image": [self.class string:source[@"image"]],
                                          @"download_url": download}];
                    }
                }
            }

            /* RockPod's store field accepts an ordinary page URL. Mirror that
             * behavior by discovering direct supported downloads from normal
             * anchor tags instead of requiring a custom manifest. */
            NSRegularExpression *anchors = [NSRegularExpression
                regularExpressionWithPattern:
                    @"<a\\b[^>]*\\bhref\\s*=\\s*([\\\"'])(.*?)\\1[^>]*>(.*?)</a>"
                options:NSRegularExpressionCaseInsensitive |
                        NSRegularExpressionDotMatchesLineSeparators error:nil];
            for (NSTextCheckingResult *match in [anchors matchesInString:html ?: @""
                options:0 range:NSMakeRange(0, html.length)])
            {
                NSString *href = [html substringWithRange:[match rangeAtIndex:2]];
                NSString *title = [html substringWithRange:[match rangeAtIndex:3]];
                [self.class addPageItemURL:href title:title base:baseURL
                                      rows:rows seen:seen];
            }
        }
        NSMutableArray *items = [NSMutableArray array];
        NSMutableSet *itemURLs = [NSMutableSet set];
        for (NSDictionary *row in rows)
        {
            RPGameStoreItem *item = [self.class itemFromDictionary:row base:baseURL];
            if (item && ![itemURLs containsObject:item.downloadURL])
            {
                [itemURLs addObject:item.downloadURL];
                [items addObject:item];
            }
        }
        dispatch_async(dispatch_get_main_queue(), ^{
            completion(items, items.count ? nil : @"The URL loaded, but no supported game downloads were found on that page.");
        });
    }];
    [task resume];
}

+ (NSString *)safeFilename:(NSString *)filename
{
    NSString *name = filename.lastPathComponent;
    NSCharacterSet *bad = [NSCharacterSet characterSetWithCharactersInString:@"<>:\"/\\|?*"];
    name = [[name componentsSeparatedByCharactersInSet:bad] componentsJoinedByString:@"_"];
    return name.length > 120 ? [name substringToIndex:120] : name;
}

+ (NSString *)destinationForFilename:(NSString *)filename
{
    NSString *ext = filename.pathExtension.lowercaseString;
    if ([@[@"sfc", @"smc"] containsObject:ext])
        return [@"/.rockbox/roms/snes" stringByAppendingPathComponent:filename];
    if ([@[@"sms", @"gg", @"sg"] containsObject:ext])
        return [@"/.rockbox/games/smsgg/roms" stringByAppendingPathComponent:filename];
    if ([@[@"mgw", @"gw", @"gwz"] containsObject:ext])
        return [@"/.rockbox/games/gwatch/roms" stringByAppendingPathComponent:filename];
    if ([@[@"md", @"gen", @"bin", @"smd"] containsObject:ext])
        return [@"/.rockbox/games/genesis/roms" stringByAppendingPathComponent:filename];
    return [@"/gameboy" stringByAppendingPathComponent:filename];
}

+ (NSString *)pluginForFilename:(NSString *)filename
{
    NSString *ext = filename.pathExtension.lowercaseString;
    if ([@[@"sfc", @"smc"] containsObject:ext]) return @"/.rockbox/rocks/games/snes_lite.rock";
    if ([@[@"sms", @"gg", @"sg"] containsObject:ext]) return @"/.rockbox/rocks/games/smsplus.rock";
    if ([@[@"mgw", @"gw", @"gwz"] containsObject:ext]) return @"/.rockbox/rocks/games/gwatch.rock";
    if ([@[@"md", @"gen", @"bin", @"smd"] containsObject:ext]) return @"/.rockbox/rocks/games/picodrive.rock";
    return @"";
}

+ (NSData *)coverBMP:(UIImage *)image
{
    if (!image.CGImage) return nil;
    const size_t width = 140, height = 124;
    const size_t stride = (width * 3 + 3) & ~3;
    unsigned char *rgb = calloc(height, width * 4);
    if (!rgb) return nil;
    CGColorSpaceRef color = CGColorSpaceCreateDeviceRGB();
    CGContextRef context = CGBitmapContextCreate(rgb, width, height, 8, width * 4,
        color, kCGImageAlphaPremultipliedLast);
    CGColorSpaceRelease(color);
    if (!context) { free(rgb); return nil; }
    CGContextSetRGBFillColor(context, 0, 0, 0, 1);
    CGContextFillRect(context, CGRectMake(0, 0, width, height));
    CGFloat scale = MIN((CGFloat)width / image.size.width, (CGFloat)height / image.size.height);
    CGSize fit = CGSizeMake(image.size.width * scale, image.size.height * scale);
    CGContextDrawImage(context, CGRectMake((width-fit.width)/2, (height-fit.height)/2,
                                            fit.width, fit.height), image.CGImage);
    CGContextRelease(context);
    NSMutableData *bmp = [NSMutableData dataWithLength:54 + stride * height];
    unsigned char *out = bmp.mutableBytes;
    out[0] = 'B'; out[1] = 'M';
#define RP_LE16(P,V) do { (P)[0]=(V)&255; (P)[1]=((V)>>8)&255; } while (0)
#define RP_LE32(P,V) do { RP_LE16((P),(V)); RP_LE16((P)+2,(V)>>16); } while (0)
    RP_LE32(out+2, (uint32_t)bmp.length); RP_LE32(out+10, 54); RP_LE32(out+14, 40);
    RP_LE32(out+18, width); RP_LE32(out+22, height); RP_LE16(out+26, 1);
    RP_LE16(out+28, 24); RP_LE32(out+34, stride * height);
    for (size_t y=0; y<height; y++) for (size_t x=0; x<width; x++) {
        unsigned char *src = rgb + ((height-1-y)*width+x)*4;
        unsigned char *dst = out + 54 + y*stride + x*3;
        dst[0]=src[2]; dst[1]=src[1]; dst[2]=src[0];
    }
    free(rgb);
    return bmp;
}

+ (NSDictionary *)JSONAtURL:(NSURL *)url
{
    NSData *data = url ? [NSData dataWithContentsOfURL:url] : nil;
    id value = data.length ? [NSJSONSerialization JSONObjectWithData:data options:0 error:nil] : nil;
    return [value isKindOfClass:NSDictionary.class] ? value : nil;
}

+ (NSURL *)wikidataURLWithItems:(NSArray<NSURLQueryItem *> *)items
{
    NSURLComponents *components = [NSURLComponents componentsWithString:@"https://www.wikidata.org/w/api.php"];
    components.queryItems = items;
    return components.URL;
}

+ (void)enrichMetadata:(RPGameStoreItem *)item
{
    if (!item.title.length || (item.year.length && item.genre.length &&
        item.publisher.length && item.developer.length && item.gameDescription.length))
        return;
    NSDictionary *search = [self JSONAtURL:[self wikidataURLWithItems:@[
        [NSURLQueryItem queryItemWithName:@"action" value:@"wbsearchentities"],
        [NSURLQueryItem queryItemWithName:@"format" value:@"json"],
        [NSURLQueryItem queryItemWithName:@"language" value:@"en"],
        [NSURLQueryItem queryItemWithName:@"limit" value:@"1"],
        [NSURLQueryItem queryItemWithName:@"search" value:item.title]]]];
    NSArray *matches = [search[@"search"] isKindOfClass:NSArray.class] ? search[@"search"] : @[];
    NSString *entityID = matches.count ? [self string:matches[0][@"id"]] : @"";
    if (!entityID.length) return;
    NSDictionary *payload = [self JSONAtURL:[self wikidataURLWithItems:@[
        [NSURLQueryItem queryItemWithName:@"action" value:@"wbgetentities"],
        [NSURLQueryItem queryItemWithName:@"format" value:@"json"],
        [NSURLQueryItem queryItemWithName:@"languages" value:@"en"],
        [NSURLQueryItem queryItemWithName:@"props" value:@"claims|descriptions"],
        [NSURLQueryItem queryItemWithName:@"ids" value:entityID]]]];
    NSDictionary *entity = payload[@"entities"][entityID];
    if (!item.gameDescription.length)
        item.gameDescription = [self string:entity[@"descriptions"][@"en"][@"value"]];
    if (!item.year.length)
    {
        NSArray *dates = entity[@"claims"][@"P577"];
        NSString *time = dates.count ? [self string:dates[0][@"mainsnak"][@"datavalue"][@"value"][@"time"]] : @"";
        NSRegularExpression *year = [NSRegularExpression regularExpressionWithPattern:@"[12][0-9]{3}" options:0 error:nil];
        NSTextCheckingResult *match = [year firstMatchInString:time options:0 range:NSMakeRange(0, time.length)];
        if (match) item.year = [time substringWithRange:match.range];
    }
    NSMutableArray *referenceIDs = [NSMutableArray array];
    NSMutableDictionary *referenceForProperty = [NSMutableDictionary dictionary];
    for (NSString *property in @[@"P136", @"P123", @"P178"])
    {
        NSArray *claims = entity[@"claims"][property];
        NSString *reference = claims.count ? [self string:claims[0][@"mainsnak"][@"datavalue"][@"value"][@"id"]] : @"";
        if (reference.length) {
            [referenceIDs addObject:reference];
            referenceForProperty[property] = reference;
        }
    }
    if (!referenceIDs.count) return;
    NSDictionary *labels = [self JSONAtURL:[self wikidataURLWithItems:@[
        [NSURLQueryItem queryItemWithName:@"action" value:@"wbgetentities"],
        [NSURLQueryItem queryItemWithName:@"format" value:@"json"],
        [NSURLQueryItem queryItemWithName:@"languages" value:@"en"],
        [NSURLQueryItem queryItemWithName:@"props" value:@"labels"],
        [NSURLQueryItem queryItemWithName:@"ids" value:[referenceIDs componentsJoinedByString:@"|"]]]]];
    NSString *(^labelFor)(NSString *) = ^NSString *(NSString *property) {
        NSString *reference = referenceForProperty[property];
        if (!reference.length) return @"";
        return [self string:labels[@"entities"][reference][@"labels"][@"en"][@"value"]];
    };
    if (!item.genre.length) item.genre = labelFor(@"P136");
    if (!item.publisher.length) item.publisher = labelFor(@"P123");
    if (!item.developer.length) item.developer = labelFor(@"P178");
}

+ (NSData *)fallbackCoverForItem:(RPGameStoreItem *)item
{
    NSDictionary *roots = @{
        @"gb": @"Nintendo%20-%20Game%20Boy", @"gbc": @"Nintendo%20-%20Game%20Boy%20Color",
        @"nes": @"Nintendo%20-%20Nintendo%20Entertainment%20System", @"gg": @"Sega%20-%20Game%20Gear",
        @"sms": @"Sega%20-%20Master%20System%20-%20Mark%20III", @"sg": @"Sega%20-%20SG-1000",
        @"sfc": @"Nintendo%20-%20Super%20Nintendo%20Entertainment%20System", @"smc": @"Nintendo%20-%20Super%20Nintendo%20Entertainment%20System",
        @"md": @"Sega%20-%20Mega%20Drive%20-%20Genesis", @"gen": @"Sega%20-%20Mega%20Drive%20-%20Genesis",
        @"bin": @"Sega%20-%20Mega%20Drive%20-%20Genesis", @"smd": @"Sega%20-%20Mega%20Drive%20-%20Genesis"};
    NSString *root = roots[item.filename.pathExtension.lowercaseString];
    if (!root.length) return nil;
    NSString *name = [item.title stringByAddingPercentEncodingWithAllowedCharacters:NSCharacterSet.URLPathAllowedCharacterSet];
    NSString *url = [NSString stringWithFormat:@"https://thumbnails.libretro.com/%@/Named_Boxarts/%@.png", root, name];
    return [NSData dataWithContentsOfURL:[NSURL URLWithString:url]];
}

+ (NSString *)cleanField:(NSString *)value
{
    return [[[value ?: @"" stringByReplacingOccurrencesOfString:@"\t" withString:@" "]
             stringByReplacingOccurrencesOfString:@"\r" withString:@" "]
            stringByReplacingOccurrencesOfString:@"\n" withString:@" "];
}

- (void)prepareROMData:(NSData *)rom
                  item:(RPGameStoreItem *)item
              filename:(NSString *)suggestedFilename
            completion:(void (^)(RPGameInstall *, NSString *))completion
{
        if (!rom.length || rom.length > 32 * 1024 * 1024) {
            dispatch_async(dispatch_get_main_queue(), ^{ completion(nil, @"The game is empty or larger than 32 MB."); });
            return;
        }
        NSString *filename = [self.class safeFilename:item.filename.length ? item.filename : suggestedFilename];
        if (![self.class isSupportedFilename:filename]) {
            dispatch_async(dispatch_get_main_queue(), ^{ completion(nil, @"This file is not a supported RockPod game format."); }); return;
        }
        if (item.sha256.length) {
            unsigned char digest[CC_SHA256_DIGEST_LENGTH];
            CC_SHA256(rom.bytes, (CC_LONG)rom.length, digest);
            NSMutableString *actual = [NSMutableString string];
            for (NSUInteger i=0; i<sizeof(digest); i++) [actual appendFormat:@"%02x", digest[i]];
            if (![actual isEqualToString:item.sha256]) {
                dispatch_async(dispatch_get_main_queue(), ^{ completion(nil, @"SHA-256 verification failed; the game was not synced."); }); return;
            }
        }
        NSURL *temp = NSFileManager.defaultManager.temporaryDirectory;
        NSURL *romFile = [temp URLByAppendingPathComponent:[NSString stringWithFormat:@"game-%@-%@", NSUUID.UUID.UUIDString, filename]];
        if (![rom writeToURL:romFile atomically:YES]) {
            dispatch_async(dispatch_get_main_queue(), ^{ completion(nil, @"The downloaded game could not be staged."); }); return;
        }
        NSMutableArray *files = [NSMutableArray arrayWithObject:romFile];
        NSString *romDestination = [self.class destinationForFilename:filename];
        NSMutableArray *destinations = [NSMutableArray arrayWithObject:romDestination];
        NSString *coverDestination = @"";
        [self.class enrichMetadata:item];
        NSData *coverData = item.coverURL.length ? [NSData dataWithContentsOfURL:[NSURL URLWithString:item.coverURL]] : nil;
        if (!coverData.length)
            coverData = [self.class fallbackCoverForItem:item];
        UIImage *cover = coverData.length ? [UIImage imageWithData:coverData] : nil;
        NSData *bmp = cover ? [self.class coverBMP:cover] : nil;
        if (bmp.length) {
            NSURL *coverFile = [temp URLByAppendingPathComponent:[NSString stringWithFormat:@"cover-%@.bmp", NSUUID.UUID.UUIDString]];
            [bmp writeToURL:coverFile atomically:YES];
            NSString *stem = filename.stringByDeletingPathExtension;
            if ([@[@"md",@"gen",@"bin",@"smd"] containsObject:filename.pathExtension.lowercaseString])
                coverDestination = [@"/.rockbox/games/library/covers/genesis" stringByAppendingPathComponent:[stem stringByAppendingPathExtension:@"bmp"]];
            else
                coverDestination = [[romDestination stringByDeletingLastPathComponent] stringByAppendingPathComponent:[stem stringByAppendingPathExtension:@"bmp"]];
            [files addObject:coverFile]; [destinations addObject:coverDestination];
        }
        NSString *plugin = [self.class pluginForFilename:filename];
        NSString *romColumn = plugin.length ? plugin : romDestination;
        NSString *parameter = plugin.length ? romDestination : @"";
        NSArray *fields = @[item.title ?: filename.stringByDeletingPathExtension, romColumn,
            coverDestination, @"0", @"", item.year ?: @"", item.genre ?: @"",
            item.publisher ?: @"", item.developer ?: @"", item.gameDescription ?: @"", parameter];
        NSMutableArray *clean = [NSMutableArray array];
        for (NSString *field in fields) [clean addObject:[self.class cleanField:field]];
        NSData *row = [[[clean componentsJoinedByString:@"\t"] stringByAppendingString:@"\n"] dataUsingEncoding:NSUTF8StringEncoding];
        NSURL *rowFile = [temp URLByAppendingPathComponent:[NSString stringWithFormat:@"game-row-%@.tsv", NSUUID.UUID.UUIDString]];
        [row writeToURL:rowFile atomically:YES];
        [files addObject:rowFile]; [destinations addObject:@"/.rockbox/rockpod/phone/game-row.tsv"];
        RPGameInstall *install = [[RPGameInstall alloc] init];
        install.files = files; install.destinations = destinations;
        dispatch_async(dispatch_get_main_queue(), ^{ completion(install, nil); });
}

- (void)prepareItem:(RPGameStoreItem *)item
         completion:(void (^)(RPGameInstall *, NSString *))completion
{
    NSURL *romURL = [NSURL URLWithString:item.downloadURL];
    if (!romURL) { completion(nil, @"The game download URL is invalid."); return; }
    NSURLSessionDataTask *task = [NSURLSession.sharedSession dataTaskWithURL:romURL
        completionHandler:^(NSData *rom, NSURLResponse *response, NSError *error) {
        if (!rom.length || error) {
            dispatch_async(dispatch_get_main_queue(), ^{ completion(nil, error.localizedDescription ?: @"Game download failed."); });
            return;
        }
        [self prepareROMData:rom item:item filename:response.suggestedFilename
                  completion:completion];
    }];
    [task resume];
}

- (void)prepareLocalGameAtURL:(NSURL *)url
                   completion:(void (^)(RPGameInstall *, NSString *))completion
{
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        BOOL scoped = [url startAccessingSecurityScopedResource];
        NSError *error = nil;
        NSData *rom = [NSData dataWithContentsOfURL:url options:NSDataReadingMappedIfSafe
                                             error:&error];
        if (scoped)
            [url stopAccessingSecurityScopedResource];
        if (!rom.length || error)
        {
            dispatch_async(dispatch_get_main_queue(), ^{
                completion(nil, error.localizedDescription ?: @"The selected game could not be read.");
            });
            return;
        }
        RPGameStoreItem *item = [[RPGameStoreItem alloc] init];
        item.filename = url.lastPathComponent;
        item.title = item.filename.stringByDeletingPathExtension;
        [self prepareROMData:rom item:item filename:item.filename completion:completion];
    });
}

@end
