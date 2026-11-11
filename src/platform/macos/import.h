#import <Foundation/Foundation.h>
BOOL R4LaunchTask(NSTask *task, NSError **error);

NSArray<NSString *> *R4RequiredFiles(void);
NSString *R4Hash(NSString *path);
NSArray<NSDictionary *> *R4Scan(NSString *root, BOOL (^cancel)(void));
NSDictionary *R4Current(NSString *home);
/* A nil helper imports MID directly; an explicit helper renders WAV offline. */
NSDictionary *R4Import(NSDictionary *candidate, NSString *home, NSString *helper,
                      BOOL (^cancel)(void), void (^progress)(NSString *, double));
NSDictionary *R4Font(NSString *home, NSString *selected);
