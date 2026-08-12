#import "RPVideoMetadataService.h"
#import <AVFoundation/AVFoundation.h>

@implementation RPVideoMetadataService

- (void)JSON:(NSURL *)URL completion:(void (^)(id, NSString *))completion
{
    NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:URL
        cachePolicy:NSURLRequestReloadIgnoringLocalCacheData timeoutInterval:20];
    [request setValue:@"RockPodLink/3.2" forHTTPHeaderField:@"User-Agent"];
    [[NSURLSession.sharedSession dataTaskWithRequest:request completionHandler:
      ^(NSData *data, NSURLResponse *response, NSError *error) {
        NSHTTPURLResponse *http = (NSHTTPURLResponse *)response;
        id json = data.length ? [NSJSONSerialization JSONObjectWithData:data options:0 error:nil] : nil;
        NSString *message = error.localizedDescription;
        if (!message.length && (http.statusCode < 200 || http.statusCode >= 300))
            message = [NSString stringWithFormat:@"Metadata service returned HTTP %ld", (long)http.statusCode];
        dispatch_async(dispatch_get_main_queue(), ^{ completion(json, message); });
    }] resume];
}

- (NSString *)cleanTitle:(NSString *)value
{
    if (value.length > 37 && [value characterAtIndex:36] == '-' &&
        [[NSUUID alloc] initWithUUIDString:[value substringToIndex:36]])
        value = [value substringFromIndex:37];
    NSString *text = [value stringByReplacingOccurrencesOfString:@"_" withString:@" "];
    text = [text stringByReplacingOccurrencesOfString:@"." withString:@" "];
    text = [text stringByReplacingOccurrencesOfString:
        @"(?i)\\s+(?:480p|720p|1080p|2160p|x264|x265|h[ .]?264|hevc|web[ -]?dl|blu[ -]?ray|brrip|dvdrip)(?:\\s+.*)?$"
        withString:@"" options:NSRegularExpressionSearch range:NSMakeRange(0, text.length)];
    text = [text stringByReplacingOccurrencesOfString:@"[\\[\\]()]" withString:@" "
        options:NSRegularExpressionSearch range:NSMakeRange(0, text.length)];
    text = [text stringByReplacingOccurrencesOfString:@"\\s+" withString:@" "
        options:NSRegularExpressionSearch range:NSMakeRange(0, text.length)];
    return [text stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet];
}

- (void)lookupVideoAtURL:(NSURL *)URL completion:(void (^)(NSDictionary *, NSString *))completion
{
    BOOL scoped = [URL startAccessingSecurityScopedResource];
    AVURLAsset *asset = [AVURLAsset URLAssetWithURL:URL options:nil];
    NSArray *commonTitles = [AVMetadataItem metadataItemsFromArray:asset.commonMetadata
        withKey:AVMetadataCommonKeyTitle keySpace:AVMetadataKeySpaceCommon];
    AVMetadataItem *commonTitle = commonTitles.firstObject;
    NSString *embeddedTitle = [commonTitle.value isKindOfClass:NSString.class] ?
        (NSString *)commonTitle.value : @"";
    NSString *embeddedShow = @""; NSInteger embeddedSeason = 0, embeddedEpisode = 0;
    for (AVMetadataItem *item in [asset metadataForFormat:AVMetadataFormatiTunesMetadata])
    {
        NSString *identifier = item.identifier ?: @"";
        if ([identifier hasSuffix:@"/tvsh"] && [item.value isKindOfClass:NSString.class])
            embeddedShow = (NSString *)item.value;
        else if ([identifier hasSuffix:@"/tvsn"])
            embeddedSeason = [item.numberValue integerValue];
        else if ([identifier hasSuffix:@"/tves"])
            embeddedEpisode = [item.numberValue integerValue];
    }
    if (scoped) [URL stopAccessingSecurityScopedResource];
    NSString *stem = [self cleanTitle:embeddedTitle.length ? embeddedTitle :
                      URL.URLByDeletingPathExtension.lastPathComponent];
    if (embeddedShow.length && embeddedSeason >= 0 && embeddedEpisode > 0)
        stem = [NSString stringWithFormat:@"%@ S%02ldE%02ld %@", embeddedShow,
                (long)embeddedSeason, (long)embeddedEpisode, stem];
    NSRegularExpression *episodePattern = [NSRegularExpression regularExpressionWithPattern:
        @"(?i)^(.+?)[ ._-]+S(\\d{1,2})E(\\d{1,3})(?:[ ._-]+(.+))?$" options:0 error:nil];
    NSTextCheckingResult *match = [episodePattern firstMatchInString:stem options:0
        range:NSMakeRange(0, stem.length)];
    if (match)
    {
        NSString *show = [stem substringWithRange:[match rangeAtIndex:1]];
        NSInteger season = [[stem substringWithRange:[match rangeAtIndex:2]] integerValue];
        NSInteger episode = [[stem substringWithRange:[match rangeAtIndex:3]] integerValue];
        NSURLComponents *search = [NSURLComponents componentsWithString:@"https://api.tvmaze.com/singlesearch/shows"];
        search.queryItems = @[[NSURLQueryItem queryItemWithName:@"q" value:show]];
        [self JSON:search.URL completion:^(NSDictionary *showJSON, NSString *error) {
            if (!showJSON[@"id"]) { completion(nil, error ?: @"TV show could not be identified."); return; }
            NSURL *episodesURL = [NSURL URLWithString:[NSString stringWithFormat:
                @"https://api.tvmaze.com/shows/%@/episodes", showJSON[@"id"]]];
            [self JSON:episodesURL completion:^(NSArray *rows, NSString *episodeError) {
                NSDictionary *found = nil;
                for (NSDictionary *row in rows ?: @[])
                    if ([row[@"season"] integerValue] == season && [row[@"number"] integerValue] == episode)
                    { found = row; break; }
                NSDictionary *image = found[@"image"] ?: showJSON[@"image"] ?: @{};
                NSString *summary = found[@"summary"] ?: showJSON[@"summary"] ?: @"";
                summary = [summary stringByReplacingOccurrencesOfString:@"<[^>]+>" withString:@" "
                    options:NSRegularExpressionSearch range:NSMakeRange(0, summary.length)];
                if (!found) { completion(nil, episodeError ?: @"The show matched, but this episode was not found."); return; }
                completion(@{ @"kind": @"show", @"show": showJSON[@"name"] ?: show,
                    @"season": @(season), @"episode": @(episode), @"title": found[@"name"] ?: stem,
                    @"year": [found[@"airdate"] length] >= 4 ? [found[@"airdate"] substringToIndex:4] : @"",
                    @"genre": [showJSON[@"genres"] componentsJoinedByString:@", "] ?: @"",
                    @"summary": summary, @"artwork_url": image[@"original"] ?: image[@"medium"] ?: @"" }, nil);
            }];
        }];
        return;
    }
    NSURLComponents *parts = [NSURLComponents componentsWithString:@"https://itunes.apple.com/search"];
    parts.queryItems = @[[NSURLQueryItem queryItemWithName:@"term" value:stem],
        [NSURLQueryItem queryItemWithName:@"country" value:@"us"],
        [NSURLQueryItem queryItemWithName:@"media" value:@"all"],
        [NSURLQueryItem queryItemWithName:@"limit" value:@"25"]];
    [self JSON:parts.URL completion:^(NSDictionary *json, NSString *error) {
        NSDictionary *row = nil;
        for (NSDictionary *candidate in json[@"results"] ?: @[])
        {
            BOOL movie = [candidate[@"kind"] isEqualToString:@"feature-movie"] ||
                [candidate[@"trackViewUrl"] containsString:@"/movie/"];
            if (!movie) continue;
            row = candidate;
            if ([candidate[@"trackName"] caseInsensitiveCompare:stem] == NSOrderedSame) break;
        }
        if (!row) { completion(nil, error ?: @"Movie could not be identified."); return; }
        NSString *art = [row[@"artworkUrl100"] stringByReplacingOccurrencesOfString:@"100x100bb" withString:@"600x600bb"];
        NSString *date = row[@"releaseDate"] ?: @"";
        completion(@{ @"kind": @"movie", @"title": row[@"trackName"] ?: stem,
            @"show": @"", @"season": @0, @"episode": @0,
            @"year": date.length >= 4 ? [date substringToIndex:4] : @"",
            @"genre": row[@"primaryGenreName"] ?: @"",
            @"summary": row[@"longDescription"] ?: row[@"shortDescription"] ?: @"",
            @"artwork_url": art ?: @"" }, nil);
    }];
}

@end
