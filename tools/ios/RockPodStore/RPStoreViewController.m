#import "RPStoreViewController.h"
#import "RPStoreClient.h"
#import <QuartzCore/QuartzCore.h>

static UIColor *RPBlue(void) { return [UIColor colorWithRed:0.04 green:0.47 blue:0.86 alpha:1.0]; }
static UIColor *RPDark(void) { return [UIColor colorWithRed:0.08 green:0.09 blue:0.11 alpha:1.0]; }

@interface RPAlbumViewController : UITableViewController
- (id)initWithAlbum:(NSDictionary *)album;
@end

@interface RPStoreViewController () <UITableViewDataSource, UITableViewDelegate, UISearchBarDelegate>
@property(nonatomic, strong) UITableView *tableView;
@property(nonatomic, strong) UISearchBar *searchBar;
@property(nonatomic, strong) UIScrollView *tabs;
@property(nonatomic, strong) NSArray *albums;
@property(nonatomic, strong) UIActivityIndicatorView *spinner;
@property(nonatomic, copy) NSString *selectedTab;
@end

@implementation RPStoreViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    self.title = @"RockPod Store";
    self.view.backgroundColor = RPDark();
    self.selectedTab = @"featured";
    self.searchBar = [[UISearchBar alloc] initWithFrame:CGRectMake(0, 0, self.view.bounds.size.width, 44)];
    self.searchBar.autoresizingMask = UIViewAutoresizingFlexibleWidth;
    self.searchBar.placeholder = @"Artists, albums, and songs";
    self.searchBar.delegate = self;
    [self.view addSubview:self.searchBar];

    self.tabs = [[UIScrollView alloc] initWithFrame:CGRectMake(0, 44, self.view.bounds.size.width, 38)];
    self.tabs.autoresizingMask = UIViewAutoresizingFlexibleWidth;
    self.tabs.showsHorizontalScrollIndicator = NO;
    NSArray *keys = @[@"featured", @"new_releases", @"top_albums", @"just_added", @"alternative", @"rock", @"hip_hop"];
    NSArray *titles = @[@"Featured", @"New", @"Top", @"Just Added", @"Alternative", @"Rock", @"Hip-Hop"];
    CGFloat x = 6;
    for (NSUInteger i = 0; i < keys.count; i++) {
        UIButton *button = [UIButton buttonWithType:UIButtonTypeCustom];
        button.frame = CGRectMake(x, 3, 82, 31); button.tag = (NSInteger)i;
        [button setTitle:titles[i] forState:UIControlStateNormal];
        [button setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
        button.titleLabel.font = [UIFont boldSystemFontOfSize:12];
        button.backgroundColor = i == 0 ? RPBlue() : [UIColor colorWithWhite:0.2 alpha:1];
        button.layer.cornerRadius = 5;
        [button addTarget:self action:@selector(tabTapped:) forControlEvents:UIControlEventTouchUpInside];
        [self.tabs addSubview:button]; x += 88;
    }
    self.tabs.contentSize = CGSizeMake(x, 38); self.tabs.accessibilityValue = [keys componentsJoinedByString:@","];
    [self.view addSubview:self.tabs];

    self.tableView = [[UITableView alloc] initWithFrame:CGRectMake(0, 82, self.view.bounds.size.width, self.view.bounds.size.height - 82) style:UITableViewStylePlain];
    self.tableView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    self.tableView.dataSource = self; self.tableView.delegate = self; self.tableView.rowHeight = 68;
    [self.view addSubview:self.tableView];
    self.spinner = [[UIActivityIndicatorView alloc] initWithActivityIndicatorStyle:UIActivityIndicatorViewStyleGray];
    self.spinner.center = self.view.center;
    self.spinner.autoresizingMask = UIViewAutoresizingFlexibleLeftMargin | UIViewAutoresizingFlexibleRightMargin | UIViewAutoresizingFlexibleTopMargin | UIViewAutoresizingFlexibleBottomMargin;
    [self.view addSubview:self.spinner];
    [self loadHome];
}

- (void)tabTapped:(UIButton *)sender {
    NSArray *keys = @[@"featured", @"new_releases", @"top_albums", @"just_added", @"alternative", @"rock", @"hip_hop"];
    self.selectedTab = keys[(NSUInteger)sender.tag];
    for (UIButton *button in self.tabs.subviews) if ([button isKindOfClass:[UIButton class]]) button.backgroundColor = button == sender ? RPBlue() : [UIColor colorWithWhite:0.2 alpha:1];
    [self loadHome];
}

- (void)loadHome {
    [self requestPath:[NSString stringWithFormat:@"/v1/store/home?tab=%@&limit=30", self.selectedTab]];
}

- (void)requestPath:(NSString *)path {
    [self.spinner startAnimating];
    [[RPStoreClient sharedClient] getPath:path completion:^(id data, NSError *error) {
        [self.spinner stopAnimating];
        if (error) { [self showError:error]; return; }
        self.albums = [data isKindOfClass:[NSArray class]] ? data : @[];
        [self.tableView reloadData];
    }];
}

- (void)searchBarSearchButtonClicked:(UISearchBar *)searchBar {
    [searchBar resignFirstResponder];
    NSString *query = [searchBar.text stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding];
    [self requestPath:[NSString stringWithFormat:@"/v1/store/search?source=tidal&limit=30&q=%@", query ?: @""]];
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section { (void)tableView; (void)section; return (NSInteger)self.albums.count; }

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath {
    UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:@"Album"];
    if (!cell) cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleSubtitle reuseIdentifier:@"Album"];
    NSDictionary *album = self.albums[(NSUInteger)indexPath.row];
    cell.textLabel.text = album[@"title"] ?: @"Untitled";
    NSString *artist = album[@"artist"] ?: @"";
    cell.detailTextLabel.text = [album[@"owned"] boolValue] ? [artist stringByAppendingString:@"  • Owned"] : artist;
    cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
    cell.imageView.image = [UIImage imageNamed:@"AlbumPlaceholder.png"];
    NSString *art = album[@"artwork_url"] ?: album[@"cover_url"];
    NSURL *URL = [[RPStoreClient sharedClient] URLForArtworkPath:art];
    if (URL) [NSURLConnection sendAsynchronousRequest:[NSURLRequest requestWithURL:URL] queue:[NSOperationQueue mainQueue] completionHandler:^(NSURLResponse *response, NSData *data, NSError *error) {
        (void)response; if (!error && data.length && [tableView.indexPathsForVisibleRows containsObject:indexPath]) { cell.imageView.image = [UIImage imageWithData:data]; [cell setNeedsLayout]; }
    }];
    return cell;
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath {
    [tableView deselectRowAtIndexPath:indexPath animated:YES];
    [self.navigationController pushViewController:[[RPAlbumViewController alloc] initWithAlbum:self.albums[(NSUInteger)indexPath.row]] animated:YES];
}

- (void)showError:(NSError *)error {
    UIAlertView *alert = [[UIAlertView alloc] initWithTitle:@"RockPod Store" message:error.localizedDescription delegate:nil cancelButtonTitle:@"OK" otherButtonTitles:nil];
    [alert show];
}
@end

@interface RPAlbumViewController ()
@property(nonatomic, strong) NSDictionary *album;
@property(nonatomic, strong) NSArray *tracks;
@end


@implementation RPAlbumViewController
- (id)initWithAlbum:(NSDictionary *)album { if ((self = [super initWithStyle:UITableViewStyleGrouped])) _album = album; return self; }
- (void)viewDidLoad {
    [super viewDidLoad]; self.title = self.album[@"title"] ?: @"Album";
    self.tracks = self.album[@"track_items"] ?: @[];
    self.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc] initWithTitle:[self.album[@"owned"] boolValue] ? @"Owned" : @"Buy Album" style:UIBarButtonItemStyleDone target:self action:@selector(buyAlbum)];
    NSString *source = self.album[@"source"] ?: @"tidal"; NSString *albumID = self.album[@"id"] ?: @"";
    [[RPStoreClient sharedClient] getPath:[NSString stringWithFormat:@"/v1/store/albums/%@/%@", source, albumID] completion:^(id data, NSError *error) {
        if (!error && [data isKindOfClass:[NSDictionary class]]) { self.album = data; self.tracks = data[@"track_items"] ?: @[]; [self.tableView reloadData]; }
    }];
}
- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView { (void)tableView; return 2; }
- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section { (void)tableView; return section == 0 ? 1 : (NSInteger)self.tracks.count; }
- (NSString *)tableView:(UITableView *)tableView titleForHeaderInSection:(NSInteger)section { (void)tableView; return section == 0 ? @"Album" : @"Tracks"; }
- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath {
    UITableViewCell *cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleSubtitle reuseIdentifier:nil];
    if (indexPath.section == 0) { cell.textLabel.text = self.album[@"title"]; cell.detailTextLabel.text = self.album[@"artist"]; }
    else { NSDictionary *track = self.tracks[(NSUInteger)indexPath.row]; cell.textLabel.text = [NSString stringWithFormat:@"%@. %@", track[@"track_number"] ?: @(indexPath.row + 1), track[@"title"] ?: @"Track"]; cell.detailTextLabel.text = [track[@"owned"] boolValue] ? @"Owned" : track[@"artist"]; cell.accessoryType = [track[@"owned"] boolValue] ? UITableViewCellAccessoryCheckmark : UITableViewCellAccessoryNone; }
    return cell;
}
- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath { [tableView deselectRowAtIndexPath:indexPath animated:YES]; if (indexPath.section == 1 && ![self.tracks[(NSUInteger)indexPath.row][@"owned"] boolValue]) [self buyItem:self.tracks[(NSUInteger)indexPath.row]]; }
- (void)buyAlbum { if (![self.album[@"owned"] boolValue]) [self buyItem:self.album]; }
- (void)buyItem:(NSDictionary *)item {
    NSString *key = [NSString stringWithFormat:@"%@-%@-%@", item[@"source"] ?: @"tidal", item[@"media_type"] ?: @"album", item[@"id"] ?: @""];
    [[RPStoreClient sharedClient] postPath:@"/v1/imports" body:@{ @"item": item, @"format": @"mp3" } idempotencyKey:key completion:^(id data, NSError *error) {
        (void)data;
        NSString *message = error ? error.localizedDescription : @"Downloading now. It will be added to the iPod Music app automatically.";
        [[[UIAlertView alloc] initWithTitle:error ? @"Could Not Buy" : @"Queued" message:message delegate:nil cancelButtonTitle:@"OK" otherButtonTitles:nil] show];
    }];
}
@end

@interface RPDownloadsViewController ()
@property(nonatomic, strong) NSArray *jobs;
@property(nonatomic, strong) NSTimer *timer;
@end

@implementation RPDownloadsViewController
- (void)viewDidLoad { [super viewDidLoad]; self.title = @"Downloads"; self.refreshControl = [[UIRefreshControl alloc] init]; [self.refreshControl addTarget:self action:@selector(reloadJobs) forControlEvents:UIControlEventValueChanged]; }
- (void)viewWillAppear:(BOOL)animated { [super viewWillAppear:animated]; [self reloadJobs]; self.timer = [NSTimer scheduledTimerWithTimeInterval:3 target:self selector:@selector(reloadJobs) userInfo:nil repeats:YES]; }
- (void)viewWillDisappear:(BOOL)animated { [super viewWillDisappear:animated]; [self.timer invalidate]; self.timer = nil; }
- (void)reloadJobs { [[RPStoreClient sharedClient] getPath:@"/v1/imports" completion:^(id data, NSError *error) { [self.refreshControl endRefreshing]; if (!error) { self.jobs = data; [self.tableView reloadData]; } }]; }
- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section { (void)tableView; (void)section; return (NSInteger)self.jobs.count; }
- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath { UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:@"Job"]; if (!cell) cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleSubtitle reuseIdentifier:@"Job"]; NSDictionary *job = self.jobs[(NSUInteger)indexPath.row]; cell.textLabel.text = job[@"title"]; NSString *state = job[@"state"]; cell.detailTextLabel.text = ([state isEqual:@"failed"] || [state isEqual:@"device_failed"]) ? job[@"error"] : ([state isEqual:@"completed"] ? @"Added to Music" : [state capitalizedString]); return cell; }
@end

@interface RPSettingsViewController () <UITextFieldDelegate>
@property(nonatomic, strong) UITextField *hostField;
@property(nonatomic, strong) UITextField *codeField;
@property(nonatomic, strong) UILabel *statusLabel;
@end

@implementation RPSettingsViewController
- (void)viewDidLoad {
    [super viewDidLoad]; self.title = @"Settings"; self.view.backgroundColor = [UIColor groupTableViewBackgroundColor];
    UILabel *host = [[UILabel alloc] initWithFrame:CGRectMake(20, 20, 280, 22)]; host.text = @"RockPod host URL"; [self.view addSubview:host];
    self.hostField = [[UITextField alloc] initWithFrame:CGRectMake(20, 46, 280, 36)]; self.hostField.borderStyle = UITextBorderStyleRoundedRect; self.hostField.autocapitalizationType = UITextAutocapitalizationTypeNone; self.hostField.autocorrectionType = UITextAutocorrectionTypeNo; self.hostField.keyboardType = UIKeyboardTypeURL; self.hostField.text = [RPStoreClient sharedClient].baseURL; [self.view addSubview:self.hostField];
    UILabel *pair = [[UILabel alloc] initWithFrame:CGRectMake(20, 100, 280, 22)]; pair.text = @"Six-digit pairing code"; [self.view addSubview:pair];
    self.codeField = [[UITextField alloc] initWithFrame:CGRectMake(20, 126, 180, 36)]; self.codeField.borderStyle = UITextBorderStyleRoundedRect; self.codeField.keyboardType = UIKeyboardTypeNumberPad; self.codeField.placeholder = @"000000"; [self.view addSubview:self.codeField];
    UIButton *button = [UIButton buttonWithType:UIButtonTypeRoundedRect]; button.frame = CGRectMake(210, 126, 90, 36); [button setTitle:@"Pair" forState:UIControlStateNormal]; [button addTarget:self action:@selector(pair) forControlEvents:UIControlEventTouchUpInside]; [self.view addSubview:button];
    self.statusLabel = [[UILabel alloc] initWithFrame:CGRectMake(20, 180, 280, 80)]; self.statusLabel.numberOfLines = 0; self.statusLabel.text = [RPStoreClient sharedClient].paired ? @"Paired. This app uses the same TIDAL account and library as RockPod." : @"Not paired. Start rockpod_store_host.py on the RockPod computer first."; [self.view addSubview:self.statusLabel];
}
- (void)pair { [self.view endEditing:YES]; [RPStoreClient sharedClient].baseURL = self.hostField.text; self.statusLabel.text = @"Pairing…"; [[RPStoreClient sharedClient] pairWithCode:self.codeField.text completion:^(id data, NSError *error) { (void)data; self.statusLabel.text = error ? error.localizedDescription : @"Paired. Store and Downloads are ready."; }]; }
@end
