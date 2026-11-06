/* Native integration entry points; never included in the application bundle. */
#import <Foundation/Foundation.h>
#import "import.h"

int main(int argc,const char **argv) { @autoreleasepool {
    @try {
        id result=nil;
        if(argc==3 && !strcmp(argv[1],"scan")) result=R4Scan(@(argv[2]),^BOOL{return NO;});
        else if(argc==3 && !strcmp(argv[1],"current")) result=R4Current(@(argv[2])) ?: NSNull.null;
        else if(argc==3 && !strcmp(argv[1],"font")) result=R4Font(@(argv[2]),nil) ?: NSNull.null;
        else if(argc==5 && (!strcmp(argv[1],"import") || !strcmp(argv[1],"import-cancel"))) {
            NSArray *candidates=R4Scan(@(argv[2]),^BOOL{return NO;});
            if(candidates.count!=1) @throw [NSException exceptionWithName:@"Probe" reason:@"Expected one candidate" userInfo:nil];
            __block BOOL cancelled=NO; BOOL cancelRequested=!strcmp(argv[1],"import-cancel");
            result=R4Import(candidates[0],@(argv[3]),@(argv[4]),^BOOL{return cancelled;},^(NSString *message,double progress){(void)message; if(cancelRequested && progress>=0.1) cancelled=YES;});
        } else return 2;
        NSData *data=[NSJSONSerialization dataWithJSONObject:result options:NSJSONWritingFragmentsAllowed error:NULL];
        fwrite(data.bytes,1,data.length,stdout); puts(""); return 0;
    } @catch(NSException *e) { fprintf(stderr,"%s\n",e.reason.UTF8String); return 1; }
} }
