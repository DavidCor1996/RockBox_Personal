#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

@interface RPGameBrowserViewController : UIViewController
- (instancetype)initWithURL:(NSURL *)url;
@property(nonatomic, copy) void (^downloadHandler)(NSURL *fileURL);
@end

NS_ASSUME_NONNULL_END
