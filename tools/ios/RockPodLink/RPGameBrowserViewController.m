#import "RPGameBrowserViewController.h"
#import "RPGameStoreService.h"
#import <WebKit/WebKit.h>

@interface RPGameBrowserViewController () <WKNavigationDelegate, UITextFieldDelegate>
@property(nonatomic, strong) NSURL *initialURL;
@property(nonatomic, strong) WKWebView *webView;
@property(nonatomic, strong) UITextField *addressField;
@property(nonatomic, strong) UIProgressView *progress;
@property(nonatomic, strong) UILabel *statusLabel;
@property(nonatomic, strong) UIButton *backButton;
@property(nonatomic, strong) UIButton *forwardButton;
@end

@implementation RPGameBrowserViewController

- (instancetype)initWithURL:(NSURL *)url
{
    if ((self = [super init]))
        _initialURL = url;
    return self;
}

- (UIButton *)toolbarButton:(NSString *)title action:(SEL)action
{
    UIButton *button = [UIButton buttonWithType:UIButtonTypeCustom];
    [button setTitle:title forState:UIControlStateNormal];
    [button setTitleColor:[UIColor colorWithWhite:0.16 alpha:1]
                 forState:UIControlStateNormal];
    [button setTitleColor:[UIColor colorWithWhite:0.58 alpha:1]
                 forState:UIControlStateDisabled];
    [button setTitleShadowColor:UIColor.whiteColor forState:UIControlStateNormal];
    button.titleLabel.shadowOffset = CGSizeMake(0, 1);
    button.titleLabel.font = [UIFont boldSystemFontOfSize:15];
    [button addTarget:self action:action forControlEvents:UIControlEventTouchUpInside];
    return button;
}

- (void)viewDidLoad
{
    [super viewDidLoad];
    self.view.backgroundColor = UIColor.whiteColor;
    WKWebViewConfiguration *configuration = [[WKWebViewConfiguration alloc] init];
    configuration.websiteDataStore = WKWebsiteDataStore.defaultDataStore;
    self.webView = [[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration];
    self.webView.translatesAutoresizingMaskIntoConstraints = NO;
    self.webView.navigationDelegate = self;
    self.webView.allowsBackForwardNavigationGestures = YES;
    [self.webView addObserver:self forKeyPath:@"estimatedProgress" options:0 context:nil];
    [self.webView addObserver:self forKeyPath:@"URL" options:0 context:nil];

    UIView *top = [[UIView alloc] init];
    top.translatesAutoresizingMaskIntoConstraints = NO;
    UIImage *toolbarTexture = [[UIImage imageNamed:@"Legacy/itunes7-toolbar-texture.png"]
        resizableImageWithCapInsets:UIEdgeInsetsMake(4, 4, 4, 4)];
    top.backgroundColor = [UIColor colorWithPatternImage:toolbarTexture];
    self.addressField = [[UITextField alloc] init];
    self.addressField.translatesAutoresizingMaskIntoConstraints = NO;
    self.addressField.backgroundColor = UIColor.whiteColor;
    self.addressField.layer.cornerRadius = 4;
    self.addressField.layer.borderWidth = 1;
    self.addressField.layer.borderColor = [UIColor colorWithWhite:0.72 alpha:1].CGColor;
    self.addressField.font = [UIFont systemFontOfSize:13];
    self.addressField.keyboardType = UIKeyboardTypeURL;
    self.addressField.autocapitalizationType = UITextAutocapitalizationTypeNone;
    self.addressField.autocorrectionType = UITextAutocorrectionTypeNo;
    self.addressField.returnKeyType = UIReturnKeyGo;
    self.addressField.delegate = self;
    self.addressField.leftView = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 8, 1)];
    self.addressField.leftViewMode = UITextFieldViewModeAlways;
    UIButton *done = [self toolbarButton:@"Done" action:@selector(closeBrowser)];
    done.translatesAutoresizingMaskIntoConstraints = NO;
    [top addSubview:done]; [top addSubview:self.addressField];

    self.progress = [[UIProgressView alloc] initWithProgressViewStyle:UIProgressViewStyleBar];
    self.progress.translatesAutoresizingMaskIntoConstraints = NO;
    self.progress.progressTintColor = [UIColor colorWithRed:0.05 green:0.38 blue:0.76 alpha:1];
    [top addSubview:self.progress];

    UIView *bottom = [[UIView alloc] init];
    bottom.translatesAutoresizingMaskIntoConstraints = NO;
    UIImage *statusTexture = [[UIImage imageNamed:@"Legacy/itunes7-status-texture.png"]
        resizableImageWithCapInsets:UIEdgeInsetsMake(3, 3, 3, 3)];
    bottom.backgroundColor = [UIColor colorWithPatternImage:statusTexture];
    self.backButton = [self toolbarButton:@"‹" action:@selector(goBack)];
    self.forwardButton = [self toolbarButton:@"›" action:@selector(goForward)];
    UIButton *reload = [self toolbarButton:@"↻" action:@selector(reload)];
    UIStackView *buttons = [[UIStackView alloc] initWithArrangedSubviews:@[
        self.backButton, self.forwardButton, reload]];
    buttons.translatesAutoresizingMaskIntoConstraints = NO;
    buttons.axis = UILayoutConstraintAxisHorizontal;
    buttons.distribution = UIStackViewDistributionFillEqually;
    [bottom addSubview:buttons];
    self.statusLabel = [[UILabel alloc] init];
    self.statusLabel.translatesAutoresizingMaskIntoConstraints = NO;
    self.statusLabel.font = [UIFont systemFontOfSize:11];
    self.statusLabel.textColor = [UIColor colorWithWhite:0.3 alpha:1];
    self.statusLabel.textAlignment = NSTextAlignmentCenter;
    self.statusLabel.text = @"Tap a supported game download to sync it";
    [bottom addSubview:self.statusLabel];

    [self.view addSubview:top]; [self.view addSubview:self.webView]; [self.view addSubview:bottom];
    UILayoutGuide *safe = self.view.safeAreaLayoutGuide;
    [NSLayoutConstraint activateConstraints:@[
        [top.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
        [top.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],
        [top.topAnchor constraintEqualToAnchor:self.view.topAnchor],
        [top.bottomAnchor constraintEqualToAnchor:safe.topAnchor constant:58],
        [done.leadingAnchor constraintEqualToAnchor:safe.leadingAnchor constant:8],
        [done.bottomAnchor constraintEqualToAnchor:top.bottomAnchor constant:-8],
        [done.widthAnchor constraintEqualToConstant:52],
        [done.heightAnchor constraintEqualToConstant:42],
        [self.addressField.leadingAnchor constraintEqualToAnchor:done.trailingAnchor constant:5],
        [self.addressField.trailingAnchor constraintEqualToAnchor:safe.trailingAnchor constant:-10],
        [self.addressField.centerYAnchor constraintEqualToAnchor:done.centerYAnchor],
        [self.addressField.heightAnchor constraintEqualToConstant:38],
        [self.progress.leadingAnchor constraintEqualToAnchor:top.leadingAnchor],
        [self.progress.trailingAnchor constraintEqualToAnchor:top.trailingAnchor],
        [self.progress.bottomAnchor constraintEqualToAnchor:top.bottomAnchor],
        [bottom.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
        [bottom.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],
        [bottom.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],
        [bottom.topAnchor constraintEqualToAnchor:safe.bottomAnchor constant:-64],
        [buttons.leadingAnchor constraintEqualToAnchor:safe.leadingAnchor constant:8],
        [buttons.trailingAnchor constraintEqualToAnchor:safe.trailingAnchor constant:-8],
        [buttons.topAnchor constraintEqualToAnchor:bottom.topAnchor constant:2],
        [buttons.heightAnchor constraintEqualToConstant:40],
        [self.statusLabel.leadingAnchor constraintEqualToAnchor:safe.leadingAnchor constant:8],
        [self.statusLabel.trailingAnchor constraintEqualToAnchor:safe.trailingAnchor constant:-8],
        [self.statusLabel.topAnchor constraintEqualToAnchor:buttons.bottomAnchor],
        [self.webView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
        [self.webView.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],
        [self.webView.topAnchor constraintEqualToAnchor:top.bottomAnchor],
        [self.webView.bottomAnchor constraintEqualToAnchor:bottom.topAnchor],
    ]];
    [self loadURL:self.initialURL];
}

- (void)dealloc
{
    [self.webView removeObserver:self forKeyPath:@"estimatedProgress"];
    [self.webView removeObserver:self forKeyPath:@"URL"];
}

- (void)loadURL:(NSURL *)url
{
    if (!url) return;
    self.addressField.text = url.absoluteString;
    [self.webView loadRequest:[NSURLRequest requestWithURL:url
        cachePolicy:NSURLRequestUseProtocolCachePolicy timeoutInterval:45]];
}

- (BOOL)textFieldShouldReturn:(UITextField *)textField
{
    NSString *value = [textField.text stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet];
    if (![value containsString:@"://"]) value = [@"https://" stringByAppendingString:value];
    NSURL *url = [NSURL URLWithString:value];
    if (url.host.length && [@[@"http", @"https"] containsObject:url.scheme.lowercaseString])
        [self loadURL:url];
    [textField resignFirstResponder];
    return YES;
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object
                        change:(NSDictionary *)change context:(void *)context
{
    (void)object; (void)change; (void)context;
    if ([keyPath isEqualToString:@"estimatedProgress"])
    {
        self.progress.progress = self.webView.estimatedProgress;
        self.progress.hidden = self.webView.estimatedProgress >= 1.0;
    }
    else if ([keyPath isEqualToString:@"URL"])
    {
        self.addressField.text = self.webView.URL.absoluteString;
        self.backButton.enabled = self.webView.canGoBack;
        self.forwardButton.enabled = self.webView.canGoForward;
    }
}

- (void)closeBrowser { [self dismissViewControllerAnimated:YES completion:nil]; }
- (void)goBack { if (self.webView.canGoBack) [self.webView goBack]; }
- (void)goForward { if (self.webView.canGoForward) [self.webView goForward]; }
- (void)reload { [self.webView reload]; }

- (void)beginDownload:(NSURL *)url suggestedFilename:(NSString *)suggested
{
    self.statusLabel.text = [NSString stringWithFormat:@"Downloading %@…", suggested ?: url.lastPathComponent];
    [self.webView.configuration.websiteDataStore.httpCookieStore getAllCookies:
        ^(NSArray<NSHTTPCookie *> *cookies) {
        NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:url
            cachePolicy:NSURLRequestReloadIgnoringLocalCacheData timeoutInterval:120];
        NSDictionary *headers = [NSHTTPCookie requestHeaderFieldsWithCookies:cookies];
        for (NSString *field in headers) [request setValue:headers[field] forHTTPHeaderField:field];
        [request setValue:self.webView.customUserAgent forHTTPHeaderField:@"User-Agent"];
        NSURLSessionDownloadTask *task = [NSURLSession.sharedSession downloadTaskWithRequest:request
            completionHandler:^(NSURL *temporary, NSURLResponse *response, NSError *error) {
            NSString *filename = response.suggestedFilename.length ? response.suggestedFilename : suggested;
            filename = filename.lastPathComponent.length ? filename.lastPathComponent : @"game-download";
            NSURL *output = [NSFileManager.defaultManager.temporaryDirectory URLByAppendingPathComponent:
                [NSString stringWithFormat:@"%@-%@", NSUUID.UUID.UUIDString, filename]];
            NSError *moveError = nil;
            if (temporary && !error)
                [NSFileManager.defaultManager moveItemAtURL:temporary toURL:output error:&moveError];
            dispatch_async(dispatch_get_main_queue(), ^{
                if (error || moveError)
                    self.statusLabel.text = error.localizedDescription ?: moveError.localizedDescription;
                else
                {
                    self.statusLabel.text = [NSString stringWithFormat:@"Downloaded %@ — preparing for iPod…", filename];
                    if (self.downloadHandler) self.downloadHandler(output);
                }
            });
        }];
        [task resume];
    }];
}

- (void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)action
 decisionHandler:(void (^)(WKNavigationActionPolicy))decisionHandler
{
    (void)webView;
    NSURL *url = action.request.URL;
    if ([RPGameStoreService isSupportedFilename:url.lastPathComponent] ||
        [url.pathExtension.lowercaseString isEqualToString:@"zip"])
    {
        [self beginDownload:url suggestedFilename:url.lastPathComponent];
        decisionHandler(WKNavigationActionPolicyCancel);
        return;
    }
    decisionHandler(WKNavigationActionPolicyAllow);
}

- (void)webView:(WKWebView *)webView decidePolicyForNavigationResponse:(WKNavigationResponse *)response
 decisionHandler:(void (^)(WKNavigationResponsePolicy))decisionHandler
{
    (void)webView;
    NSString *name = response.response.suggestedFilename;
    NSString *disposition = [(NSHTTPURLResponse *)response.response allHeaderFields][@"Content-Disposition"];
    BOOL attachment = [disposition.lowercaseString containsString:@"attachment"];
    if ([RPGameStoreService isSupportedFilename:name] ||
        [name.pathExtension.lowercaseString isEqualToString:@"zip"] || attachment)
    {
        [self beginDownload:response.response.URL suggestedFilename:name];
        decisionHandler(WKNavigationResponsePolicyCancel);
        return;
    }
    decisionHandler(WKNavigationResponsePolicyAllow);
}

- (void)webView:(WKWebView *)webView didFailProvisionalNavigation:(WKNavigation *)navigation
       withError:(NSError *)error
{
    (void)webView; (void)navigation;
    self.statusLabel.text = error.localizedDescription;
}

@end
