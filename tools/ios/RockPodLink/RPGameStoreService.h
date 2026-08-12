#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

@interface RPGameStoreItem : NSObject
@property(nonatomic, copy) NSString *identifier;
@property(nonatomic, copy) NSString *title;
@property(nonatomic, copy) NSString *platform;
@property(nonatomic, copy) NSString *downloadURL;
@property(nonatomic, copy) NSString *coverURL;
@property(nonatomic, copy) NSString *filename;
@property(nonatomic, copy) NSString *year;
@property(nonatomic, copy) NSString *genre;
@property(nonatomic, copy) NSString *publisher;
@property(nonatomic, copy) NSString *developer;
@property(nonatomic, copy) NSString *gameDescription;
@property(nonatomic, copy) NSString *sha256;
@property(nonatomic, copy) NSString *licenseName;
@end

@interface RPGameInstall : NSObject
@property(nonatomic, strong) NSArray<NSURL *> *files;
@property(nonatomic, strong) NSArray<NSString *> *destinations;
@end

@interface RPGameStoreService : NSObject
+ (BOOL)isSupportedFilename:(NSString *)filename;
- (void)loadStoreURL:(NSURL *)url
          completion:(void (^)(NSArray<RPGameStoreItem *> *items,
                               NSString *errorMessage))completion;
- (void)prepareItem:(RPGameStoreItem *)item
         completion:(void (^)(RPGameInstall *install,
                              NSString *errorMessage))completion;
- (void)prepareLocalGameAtURL:(NSURL *)url
                   completion:(void (^)(RPGameInstall *install,
                                        NSString *errorMessage))completion;
@end
