#import <Foundation/Foundation.h>

NSArray<NSString *> *R4RequiredFiles(void);
NSString *R4Hash(NSString *path);
NSArray<NSDictionary *> *R4Scan(NSString *root, BOOL (^cancel)(void));
NSDictionary *R4Current(NSString *home);
NSDictionary *R4Import(NSDictionary *candidate, NSString *home, NSString *helper,
                      BOOL (^cancel)(void), void (^progress)(NSString *, double));
NSDictionary *R4Font(NSString *home, NSString *selected);
