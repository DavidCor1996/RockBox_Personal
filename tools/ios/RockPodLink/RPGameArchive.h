#import <Foundation/Foundation.h>

@interface RPGameArchive : NSObject
+ (NSURL *)extractSupportedGameFromArchive:(NSURL *)archive
                                     error:(NSError **)error;
@end
