#import "RPLibraryBrowserView.h"
#import "RPMusicService.h"

typedef NS_ENUM(NSInteger, RPLibraryLevel) {
    RPLibraryLevelRoot,
    RPLibraryLevelArtistAlbums,
    RPLibraryLevelAlbumTracks,
};

@interface RPLibraryBrowserView () <UITableViewDataSource, UITableViewDelegate,
                                     UISearchBarDelegate>
@property(nonatomic) RPLibraryBrowserKind kind;
@property(nonatomic) RPLibraryLevel level;
@property(nonatomic, strong) UITableView *table;
@property(nonatomic, strong) UISearchBar *searchBar;
@property(nonatomic, strong) NSArray<RPMusicTrack *> *tracks;
@property(nonatomic, strong) NSArray<RPVideoItem *> *videos;
@property(nonatomic, strong) NSArray *allRows;
@property(nonatomic, strong) NSArray *rows;
@property(nonatomic, strong) NSArray<RPMusicTrack *> *selectedArtistTracks;
@property(nonatomic, copy) NSString *selectedArtistName;
@property(nonatomic, copy) NSString *contextTitle;
@property(nonatomic, strong) UILabel *emptyLabel;
@property(nonatomic, strong) UILabel *titleLabel;
@property(nonatomic, strong) UIButton *backButton;
@property(nonatomic, strong) NSCache<NSString *, UIImage *> *artworkCache;
@end

@implementation RPLibraryBrowserView

- (instancetype)initWithKind:(RPLibraryBrowserKind)kind
{
    if ((self = [super init]))
    {
        self.kind = kind;
        self.level = RPLibraryLevelRoot;
        self.artworkCache = [[NSCache alloc] init];
        self.backgroundColor = [UIColor colorWithWhite:0.91 alpha:1];
        UIImage *statusTexture = [[UIImage imageNamed:@"Legacy/itunes7-status-texture.png"]
            resizableImageWithCapInsets:UIEdgeInsetsMake(3, 3, 3, 3)];
        self.titleLabel = [[UILabel alloc] init];
        self.titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
        self.titleLabel.backgroundColor = [UIColor colorWithPatternImage:statusTexture];
        self.titleLabel.font = [UIFont boldSystemFontOfSize:15];
        self.titleLabel.textColor = [UIColor colorWithWhite:0.2 alpha:1];
        [self addSubview:self.titleLabel];
        self.backButton = [UIButton buttonWithType:UIButtonTypeSystem];
        self.backButton.translatesAutoresizingMaskIntoConstraints = NO;
        self.backButton.titleLabel.font = [UIFont boldSystemFontOfSize:13];
        [self.backButton setTitle:@"‹ Back" forState:UIControlStateNormal];
        [self.backButton addTarget:self action:@selector(goBack)
                  forControlEvents:UIControlEventTouchUpInside];
        self.backButton.hidden = YES;
        [self addSubview:self.backButton];
        self.searchBar = [[UISearchBar alloc] init];
        self.searchBar.translatesAutoresizingMaskIntoConstraints = NO;
        self.searchBar.delegate = self;
        self.searchBar.placeholder = @"Search this iPod";
        self.searchBar.autocapitalizationType = UITextAutocapitalizationTypeNone;
        self.searchBar.autocorrectionType = UITextAutocorrectionTypeNo;
        self.searchBar.backgroundImage = statusTexture;
        [self addSubview:self.searchBar];
        self.table = [[UITableView alloc] initWithFrame:CGRectZero
                                                  style:UITableViewStylePlain];
        self.table.translatesAutoresizingMaskIntoConstraints = NO;
        self.table.dataSource = self;
        self.table.delegate = self;
        self.table.rowHeight = 58;
        self.table.backgroundColor = UIColor.whiteColor;
        self.table.separatorColor = [UIColor colorWithWhite:0.78 alpha:1];
        [self addSubview:self.table];
        self.emptyLabel = [[UILabel alloc] init];
        self.emptyLabel.translatesAutoresizingMaskIntoConstraints = NO;
        self.emptyLabel.textAlignment = NSTextAlignmentCenter;
        self.emptyLabel.numberOfLines = 0;
        self.emptyLabel.font = [UIFont systemFontOfSize:14];
        self.emptyLabel.textColor = UIColor.darkGrayColor;
        [self addSubview:self.emptyLabel];
        [NSLayoutConstraint activateConstraints:@[
            [self.titleLabel.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
            [self.titleLabel.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
            [self.titleLabel.topAnchor constraintEqualToAnchor:self.topAnchor],
            [self.titleLabel.heightAnchor constraintEqualToConstant:42],
            [self.backButton.leadingAnchor constraintEqualToAnchor:self.leadingAnchor constant:7],
            [self.backButton.centerYAnchor constraintEqualToAnchor:self.titleLabel.centerYAnchor],
            [self.backButton.widthAnchor constraintEqualToConstant:58],
            [self.searchBar.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
            [self.searchBar.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
            [self.searchBar.topAnchor constraintEqualToAnchor:self.titleLabel.bottomAnchor],
            [self.searchBar.heightAnchor constraintEqualToConstant:46],
            [self.table.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
            [self.table.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
            [self.table.topAnchor constraintEqualToAnchor:self.searchBar.bottomAnchor],
            [self.table.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
            [self.emptyLabel.centerXAnchor constraintEqualToAnchor:self.centerXAnchor],
            [self.emptyLabel.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
            [self.emptyLabel.widthAnchor constraintEqualToAnchor:self.widthAnchor multiplier:0.8],
        ]];
    }
    return self;
}

- (NSString *)artistForTrack:(RPMusicTrack *)track
{
    NSString *artist = track.albumArtist.length ? track.albumArtist : track.artist;
    return artist.length ? artist : @"Unknown Artist";
}

- (NSArray *)albumRowsForTracks:(NSArray<RPMusicTrack *> *)tracks
{
    NSMutableDictionary<NSString *, NSMutableArray<RPMusicTrack *> *> *groups =
        [NSMutableDictionary dictionary];
    for (RPMusicTrack *track in tracks)
    {
        NSString *album = track.album.length ? track.album : @"Unknown Album";
        NSString *key = [NSString stringWithFormat:@"%@\x1f%@",
            [self artistForTrack:track], album];
        if (!groups[key]) groups[key] = [NSMutableArray array];
        [groups[key] addObject:track];
    }
    NSMutableArray *rows = [NSMutableArray array];
    for (NSArray<RPMusicTrack *> *albumTracks in groups.allValues)
    {
        RPMusicTrack *sample = albumTracks.firstObject;
        [rows addObject:@{ @"type": @"album",
            @"name": sample.album.length ? sample.album : @"Unknown Album",
            @"detail": [self artistForTrack:sample], @"tracks": albumTracks,
            @"sample": sample }];
    }
    return [rows sortedArrayUsingComparator:^NSComparisonResult(NSDictionary *a,
                                                                 NSDictionary *b) {
        NSComparisonResult result = [a[@"name"] localizedCaseInsensitiveCompare:b[@"name"]];
        return result == NSOrderedSame ?
            [a[@"detail"] localizedCaseInsensitiveCompare:b[@"detail"]] : result;
    }];
}

- (NSArray<RPMusicTrack *> *)sortedTracks:(NSArray<RPMusicTrack *> *)tracks
{
    return [tracks sortedArrayUsingComparator:^NSComparisonResult(RPMusicTrack *a,
                                                                   RPMusicTrack *b) {
        if (a.discNumber != b.discNumber)
            return a.discNumber < b.discNumber ? NSOrderedAscending : NSOrderedDescending;
        if (a.trackNumber != b.trackNumber)
            return a.trackNumber < b.trackNumber ? NSOrderedAscending : NSOrderedDescending;
        return [a.title localizedCaseInsensitiveCompare:b.title];
    }];
}

- (void)showRoot
{
    self.level = RPLibraryLevelRoot;
    self.contextTitle = nil;
    self.selectedArtistName = nil;
    self.selectedArtistTracks = nil;
    if (self.kind == RPLibraryBrowserSongs)
        self.allRows = [self.tracks sortedArrayUsingComparator:
            ^NSComparisonResult(RPMusicTrack *a, RPMusicTrack *b) {
                return [a.title localizedCaseInsensitiveCompare:b.title];
            }];
    else if (self.kind == RPLibraryBrowserVideos)
        self.allRows = [self.videos sortedArrayUsingComparator:
            ^NSComparisonResult(RPVideoItem *a, RPVideoItem *b) {
                return [a.title localizedCaseInsensitiveCompare:b.title];
            }];
    else if (self.kind == RPLibraryBrowserAlbums)
        self.allRows = [self albumRowsForTracks:self.tracks];
    else
    {
        NSMutableDictionary<NSString *, NSMutableArray<RPMusicTrack *> *> *groups =
            [NSMutableDictionary dictionary];
        for (RPMusicTrack *track in self.tracks)
        {
            NSString *artist = [self artistForTrack:track];
            if (!groups[artist]) groups[artist] = [NSMutableArray array];
            [groups[artist] addObject:track];
        }
        NSMutableArray *artists = [NSMutableArray array];
        for (NSString *artist in groups)
            [artists addObject:@{ @"type": @"artist", @"name": artist,
                                  @"tracks": groups[artist] }];
        self.allRows = [artists sortedArrayUsingComparator:
            ^NSComparisonResult(NSDictionary *a, NSDictionary *b) {
                return [a[@"name"] localizedCaseInsensitiveCompare:b[@"name"]];
            }];
    }
    self.searchBar.text = @"";
    [self applySearchText:@""];
}

- (void)updateTracks:(NSArray<RPMusicTrack *> *)tracks
               videos:(NSArray<RPVideoItem *> *)videos
{
    self.tracks = tracks ?: @[];
    self.videos = videos ?: @[];
    [self showRoot];
}

- (NSString *)searchTextForRow:(id)row
{
    if ([row isKindOfClass:RPMusicTrack.class])
    {
        RPMusicTrack *track = row;
        return [NSString stringWithFormat:@"%@ %@ %@ %@", track.title ?: @"",
            track.artist ?: @"", track.albumArtist ?: @"", track.album ?: @""];
    }
    if ([row isKindOfClass:RPVideoItem.class])
    {
        RPVideoItem *video = row;
        return [NSString stringWithFormat:@"%@ %@ %@", video.title ?: @"",
            video.showTitle ?: @"", video.kind ?: @""];
    }
    NSDictionary *group = row;
    return [NSString stringWithFormat:@"%@ %@", group[@"name"] ?: @"",
            group[@"detail"] ?: @""];
}

- (void)applySearchText:(NSString *)text
{
    NSString *query = [text stringByTrimmingCharactersInSet:
        NSCharacterSet.whitespaceAndNewlineCharacterSet];
    if (!query.length)
        self.rows = self.allRows ?: @[];
    else
        self.rows = [self.allRows filteredArrayUsingPredicate:
            [NSPredicate predicateWithBlock:^BOOL(id row, NSDictionary *bindings) {
                (void)bindings;
                return [[self searchTextForRow:row] rangeOfString:query
                    options:NSCaseInsensitiveSearch | NSDiacriticInsensitiveSearch].location != NSNotFound;
            }]];
    self.emptyLabel.hidden = self.rows.count > 0;
    self.table.hidden = !self.rows.count;
    self.emptyLabel.text = self.allRows.count ? @"No items match this search." :
        @"No cached items yet. Connect the iPod to refresh its library.";
    static NSArray *titles;
    if (!titles) titles = @[@"Songs", @"Artists", @"Albums", @"Videos"];
    NSString *title = self.level == RPLibraryLevelRoot ? titles[self.kind] :
                                                         self.contextTitle;
    self.backButton.hidden = self.level == RPLibraryLevelRoot;
    self.titleLabel.text = [NSString stringWithFormat:@"%@%@   •   %lu",
        self.level == RPLibraryLevelRoot ? @"   " : @"        ", title ?: @"Library",
        (unsigned long)self.rows.count];
    [self.table reloadData];
}

- (void)searchBar:(UISearchBar *)searchBar textDidChange:(NSString *)searchText
{ (void)searchBar; [self applySearchText:searchText]; }
- (void)searchBarSearchButtonClicked:(UISearchBar *)searchBar
{ [searchBar resignFirstResponder]; }
- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{ (void)tableView; (void)section; return self.rows.count; }

- (void)loadArtworkForTrack:(RPMusicTrack *)track cell:(UITableViewCell *)cell
{
    cell.imageView.image = nil;
    if (!track.artworkPath.length || !self.artworkRequested)
        return;
    UIImage *cached = [self.artworkCache objectForKey:track.artworkPath];
    if (cached)
    {
        cell.imageView.image = cached;
        return;
    }
    __weak typeof(self) weakSelf = self;
    __weak UITableViewCell *weakCell = cell;
    self.artworkRequested(track, ^(UIImage *image) {
        if (!image) return;
        [weakSelf.artworkCache setObject:image forKey:track.artworkPath];
        dispatch_async(dispatch_get_main_queue(), ^{
            UITableViewCell *visibleCell = weakCell;
            if (visibleCell && [weakSelf.table indexPathForCell:visibleCell])
            {
                visibleCell.imageView.image = image;
                [visibleCell setNeedsLayout];
            }
        });
    });
}

- (UITableViewCell *)tableView:(UITableView *)tableView
         cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
    UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:@"media"];
    if (!cell) cell = [[UITableViewCell alloc]
        initWithStyle:UITableViewCellStyleSubtitle reuseIdentifier:@"media"];
    cell.textLabel.font = [UIFont boldSystemFontOfSize:14];
    cell.detailTextLabel.font = [UIFont systemFontOfSize:11];
    cell.imageView.image = nil;
    id row = self.rows[indexPath.row];
    if ([row isKindOfClass:RPMusicTrack.class])
    {
        RPMusicTrack *track = row;
        NSString *number = self.level == RPLibraryLevelAlbumTracks && track.trackNumber > 0 ?
            [NSString stringWithFormat:@"%ld. ", (long)track.trackNumber] : @"";
        cell.textLabel.text = [number stringByAppendingString:track.title ?: @"Unknown Song"];
        cell.detailTextLabel.text = self.level == RPLibraryLevelAlbumTracks ?
            track.artist : [NSString stringWithFormat:@"%@ — %@", track.artist, track.album];
        cell.accessoryType = UITableViewCellAccessoryDetailButton;
    }
    else if ([row isKindOfClass:RPVideoItem.class])
    {
        RPVideoItem *video = row;
        cell.textLabel.text = video.title;
        cell.detailTextLabel.text = video.showTitle.length ?
            [NSString stringWithFormat:@"%@ • S%02ldE%02ld", video.showTitle,
             (long)video.seasonNumber, (long)video.episodeNumber] :
            video.kind.capitalizedString;
        cell.accessoryType = UITableViewCellAccessoryNone;
    }
    else
    {
        NSDictionary *group = row;
        NSArray *groupTracks = group[@"tracks"];
        cell.textLabel.text = group[@"name"];
        if ([group[@"type"] isEqualToString:@"artist"])
        {
            NSSet *albums = [NSSet setWithArray:[groupTracks valueForKey:@"album"]];
            cell.detailTextLabel.text = [NSString stringWithFormat:@"%lu album%@ • %lu songs",
                (unsigned long)albums.count, albums.count == 1 ? @"" : @"s",
                (unsigned long)groupTracks.count];
        }
        else
        {
            cell.detailTextLabel.text = [NSString stringWithFormat:@"%@ • %lu song%@",
                group[@"detail"], (unsigned long)groupTracks.count,
                groupTracks.count == 1 ? @"" : @"s"];
            [self loadArtworkForTrack:group[@"sample"] cell:cell];
        }
        cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
    }
    return cell;
}

- (void)tableView:(UITableView *)tableView
    accessoryButtonTappedForRowWithIndexPath:(NSIndexPath *)indexPath
{
    id row = self.rows[indexPath.row];
    if ([row isKindOfClass:RPMusicTrack.class] && self.trackDownloadRequested)
        self.trackDownloadRequested(row);
    (void)tableView;
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
    [tableView deselectRowAtIndexPath:indexPath animated:YES];
    id row = self.rows[indexPath.row];
    if ([row isKindOfClass:RPMusicTrack.class])
    {
        if (self.trackSelected) self.trackSelected(row);
        return;
    }
    if (![row isKindOfClass:NSDictionary.class])
        return;
    NSDictionary *group = row;
    if ([group[@"type"] isEqualToString:@"artist"])
    {
        self.level = RPLibraryLevelArtistAlbums;
        self.selectedArtistName = group[@"name"];
        self.selectedArtistTracks = group[@"tracks"];
        self.contextTitle = self.selectedArtistName;
        self.allRows = [self albumRowsForTracks:self.selectedArtistTracks];
    }
    else
    {
        self.level = RPLibraryLevelAlbumTracks;
        self.contextTitle = group[@"name"];
        self.allRows = [self sortedTracks:group[@"tracks"]];
    }
    self.searchBar.text = @"";
    [self applySearchText:@""];
}

- (void)goBack
{
    if (self.level == RPLibraryLevelAlbumTracks &&
        self.kind == RPLibraryBrowserArtists && self.selectedArtistTracks.count)
    {
        self.level = RPLibraryLevelArtistAlbums;
        self.contextTitle = self.selectedArtistName;
        self.allRows = [self albumRowsForTracks:self.selectedArtistTracks];
        self.searchBar.text = @"";
        [self applySearchText:@""];
    }
    else
        [self showRoot];
}

@end
