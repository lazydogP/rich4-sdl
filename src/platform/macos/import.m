#import "import.h"
#import <CoreText/CoreText.h>
#import <CommonCrypto/CommonDigest.h>
#include <sys/statvfs.h>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>

BOOL R4LaunchTask(NSTask *task, NSError **error) {
    @try { [task launch]; return YES; }
    @catch(NSException *exception) {
        if(error) *error=[NSError errorWithDomain:NSCocoaErrorDomain code:NSExecutableLoadError
            userInfo:@{NSLocalizedDescriptionKey:exception.reason ?: @"无法启动程序"}];
        return NO;
    }
}
static void need(BOOL ok, NSString *message) {
    if (!ok) @throw [NSException exceptionWithName:@"Rich4Import" reason:message ?: @"文件操作失败" userInfo:nil];
}
NSArray<NSString *> *R4RequiredFiles(void) {
    NSMutableArray *files=[NSMutableArray arrayWithArray:@[@"Data.mkf",@"Effect.mkf",@"Help.mkf",@"Jump.mkf",@"Map.mkf",@"Panel.mkf",@"Speaking.mkf",@"Rich08.mid"]];
    for(int i=16;i<=22;i++) [files addObject:[NSString stringWithFormat:@"Rich%02d.mid",i]];
    for(int i=1;i<=13;i++) [files addObject:[NSString stringWithFormat:@"Midi%02d.mid",i]];
    [files addObjectsFromArray:@[@"Midi14-1.mid",@"Midi14-2.mid",@"Midi15.mid",@"Midi16.mid"]];
    return files;
}
NSString *R4Hash(NSString *path) {
    NSInputStream *stream=[NSInputStream inputStreamWithFileAtPath:path]; [stream open];
    CC_SHA256_CTX ctx; CC_SHA256_Init(&ctx); uint8_t buffer[65536], hash[CC_SHA256_DIGEST_LENGTH];
    NSInteger n;
    while((n=[stream read:buffer maxLength:sizeof(buffer)])>0) CC_SHA256_Update(&ctx,buffer,(CC_LONG)n);
    NSError *readError=stream.streamError;
    [stream close]; need(n==0,[NSString stringWithFormat:@"无法读取 %@：%@",path,readError]);
    CC_SHA256_Final(hash,&ctx); NSMutableString *result=[NSMutableString string];
    for(unsigned i=0;i<sizeof(hash);i++) [result appendFormat:@"%02x",hash[i]];
    return result;
}
NSArray<NSDictionary *> *R4Scan(NSString *root, BOOL (^cancel)(void)) {
    NSFileManager *fm=NSFileManager.defaultManager;
    NSMutableArray *queue=[NSMutableArray arrayWithObject:root], *candidates=[NSMutableArray array];
    for(NSUInteger i=0;i<queue.count;i++) {
        need(!cancel(),@"已取消"); NSString *dir=queue[i]; NSError *error=nil;
        NSArray *names=[fm contentsOfDirectoryAtPath:dir error:&error]; need(names!=nil,error.localizedDescription);
        NSMutableDictionary *files=[NSMutableDictionary dictionary]; NSMutableArray *conflicts=[NSMutableArray array];
        for(NSString *name in names) {
            NSString *path=[dir stringByAppendingPathComponent:name];
            NSDictionary *attributes=[fm attributesOfItemAtPath:path error:&error]; need(attributes!=nil,error.localizedDescription);
            if([attributes[NSFileType] isEqual:NSFileTypeSymbolicLink]) continue;
            NSString *key=name.lowercaseString;
            if(files[key]) [conflicts addObject:name];
            files[key]=path;
            if([attributes[NSFileType] isEqual:NSFileTypeDirectory] && ![name hasPrefix:@"."] && ![name.pathExtension isEqual:@"app"]) [queue addObject:path];
        }
        if(!files[@"data.mkf"]) continue;
        NSMutableArray *problems=[NSMutableArray array];
        if(conflicts.count) [problems addObject:[@"文件名大小写冲突：" stringByAppendingString:[conflicts componentsJoinedByString:@", "]]];
        NSMutableDictionary *required=[NSMutableDictionary dictionary];
        for(NSString *name in R4RequiredFiles()) {
            NSString *path=files[name.lowercaseString]; BOOL directory=NO;
            if(!path || ![fm fileExistsAtPath:path isDirectory:&directory] || directory) [problems addObject:[@"缺少 " stringByAppendingString:name]];
            else required[name]=path;
        }
        [candidates addObject:@{@"root":dir,@"files":required,@"problems":problems}];
    }
    return candidates;
}
NSDictionary *R4Current(NSString *home) {
    NSData *pointer=[NSData dataWithContentsOfFile:[home stringByAppendingPathComponent:@"current.json"]];
    NSDictionary *current=pointer?[NSJSONSerialization JSONObjectWithData:pointer options:0 error:NULL]:nil;
    if(![current isKindOfClass:NSDictionary.class]) return nil;
    NSString *generation=current[@"generation"];
    if(![generation isKindOfClass:NSString.class] || ![generation isEqual:generation.lastPathComponent] || ![generation hasPrefix:@"install-"]) return nil;
    NSString *path=[home stringByAppendingPathComponent:generation];
    NSData *data=[NSData dataWithContentsOfFile:[path stringByAppendingPathComponent:@"manifest.json"]];
    NSDictionary *manifest=data?[NSJSONSerialization JSONObjectWithData:data options:0 error:NULL]:nil;
    if(![manifest isKindOfClass:NSDictionary.class] || ![manifest[@"schema"] isEqual:@1] || ![manifest[@"files"] isKindOfClass:NSDictionary.class]) return nil;
    NSMutableSet *expected=[NSMutableSet set];
    for(NSString *name in R4RequiredFiles()) [expected addObject:[@"original" stringByAppendingPathComponent:name]];
    for(int i=2;i<=26;i++) {
        NSString *stem=i<10?[NSString stringWithFormat:@"music/MultiverseJourney/track%02d",i]:[NSString stringWithFormat:@"music/track%02d",i];
        for(NSString *extension in @[@"ogg",@"wav",@"mid"]) {
            NSString *name=[stem stringByAppendingPathExtension:extension];
            if(manifest[@"files"][name]) { [expected addObject:name]; break; }
        }
    }
    if(expected.count!=57) return nil;
    if(![expected isEqualToSet:[NSSet setWithArray:[manifest[@"files"] allKeys]]]) return nil;
    for(NSString *name in manifest[@"files"]) {
        if(![manifest[@"files"][name] isKindOfClass:NSString.class] || [manifest[@"files"][name] length]!=64) return nil;
        if(![NSFileManager.defaultManager fileExistsAtPath:[path stringByAppendingPathComponent:name]]) return nil;
    }
    return @{@"path":path,@"manifest":manifest};
}
NSDictionary *R4Import(NSDictionary *candidate, NSString *home, NSString *helper,
                      BOOL (^cancel)(void), void (^progress)(NSString *, double)) {
    need([candidate[@"files"] count]==R4RequiredFiles().count && [candidate[@"problems"] count]==0,@"目录不完整或存在文件名冲突");
    NSFileManager *fm=NSFileManager.defaultManager; NSError *error=nil;
    need([fm createDirectoryAtPath:home withIntermediateDirectories:YES attributes:nil error:&error],error.localizedDescription);
    // Offline conversion reserves WAV/cache space; direct MID import only
    // needs the source files plus a small duplicate-MIDI/filesystem reserve.
    uint64_t required=(helper.length?800ULL:16ULL)*1024*1024;
    for(NSString *path in [candidate[@"files"] allValues]) {
        NSDictionary *a=[fm attributesOfItemAtPath:path error:&error]; need(a!=nil,error.localizedDescription);
        required += [a[NSFileSize] unsignedLongLongValue];
    }
    struct statvfs space; need(statvfs(home.fileSystemRepresentation,&space)==0,@"无法检查磁盘空间");
    need((uint64_t)space.f_bavail*space.f_frsize >= required,[NSString stringWithFormat:@"磁盘空间不足，导入需要约 %llu MB 可用空间",(unsigned long long)((required+1024*1024-1)/(1024*1024))]);
    NSString *identifier=NSUUID.UUID.UUIDString;
    NSString *stage=[home stringByAppendingPathComponent:[@".import-" stringByAppendingString:identifier]];
    NSString *generation=[@"install-" stringByAppendingString:identifier];
    NSString *committed=[home stringByAppendingPathComponent:generation]; BOOL published=NO;
    int lock=open([[home stringByAppendingPathComponent:@"import.lock"] fileSystemRepresentation],O_CREAT|O_RDWR,0600);
    need(lock>=0,@"无法锁定导入目录");
    if(flock(lock,LOCK_EX|LOCK_NB)!=0) { close(lock); need(NO,@"另一个导入正在进行，请稍后重试"); }
    NSString *previous=R4Current(home)[@"path"];
    @try {
        need([fm createDirectoryAtPath:[stage stringByAppendingPathComponent:@"original"] withIntermediateDirectories:YES attributes:nil error:&error],error.localizedDescription);
        need([fm createDirectoryAtPath:[stage stringByAppendingPathComponent:@"music/MultiverseJourney"] withIntermediateDirectories:YES attributes:nil error:&error],error.localizedDescription);
        NSMutableDictionary *hashes=[NSMutableDictionary dictionary];
        NSUInteger index=0;
        for(NSString *name in R4RequiredFiles()) {
            need(!cancel(),@"已取消"); progress([@"复制 " stringByAppendingString:name],0.1*(double)index++/32);
            NSString *source=candidate[@"files"][name]; NSString *relative=[@"original" stringByAppendingPathComponent:name];
            NSString *hash=R4Hash(source), *target=[stage stringByAppendingPathComponent:relative];
            need([fm copyItemAtPath:source toPath:target error:&error],error.localizedDescription);
            need([hash isEqual:R4Hash(target)],[name stringByAppendingString:@" 在复制时发生变化，请重试"]); hashes[relative]=hash;
        }
        NSArray *midis=[R4RequiredFiles() subarrayWithRange:NSMakeRange(7,25)];
        for(NSUInteger i=0;i<midis.count;i++) {
            need(!cancel(),@"已取消"); NSString *name=midis[i];
            progress([(helper.length?@"转换 ":@"导入 ") stringByAppendingString:name],0.1+0.9*i/25);
            need(!cancel(),@"已取消");
            NSString *stem=i<8?[NSString stringWithFormat:@"music/MultiverseJourney/track%02lu",i+2]:[NSString stringWithFormat:@"music/track%02lu",i+2];
            NSString *relative=[stem stringByAppendingPathExtension:helper.length?@"wav":@"mid"];
            if(!helper.length) {
                NSString *source=[stage stringByAppendingPathComponent:[@"original" stringByAppendingPathComponent:name]];
                NSString *target=[stage stringByAppendingPathComponent:relative];
                need([fm copyItemAtPath:source toPath:target error:&error],error.localizedDescription);
                NSString *hash=R4Hash(target);
                need([hash isEqual:hashes[[@"original" stringByAppendingPathComponent:name]]],[@"复制失败：" stringByAppendingString:name]);
                hashes[relative]=hash;
                continue;
            }
            NSTask *task=[NSTask new]; NSPipe *pipe=[NSPipe pipe], *errors=[NSPipe pipe];
            task.launchPath=helper;
            task.arguments=@[[stage stringByAppendingPathComponent:[@"original" stringByAppendingPathComponent:name]],[stage stringByAppendingPathComponent:relative],@"--cache-dir",[home stringByAppendingPathComponent:@"midi-cache"]];
            task.standardOutput=pipe; task.standardError=errors;
            need(R4LaunchTask(task,&error),error.localizedDescription);
            // Drain output while running; never wait for a full pipe at process exit.
            NSMutableData *lines=[NSMutableData data];
            pipe.fileHandleForReading.readabilityHandler=^(NSFileHandle *handle) {
                NSData *data=handle.availableData; if(!data.length) { handle.readabilityHandler=nil; return; }
                @synchronized(lines) { [lines appendData:data]; }
            };
            while(task.running) {
                if(cancel()) [task terminate];
                @synchronized(lines) {
                    NSString *text=[[NSString alloc] initWithData:lines encoding:NSUTF8StringEncoding];
                    NSRange last=[text rangeOfString:@"\n" options:NSBackwardsSearch];
                    if(last.location!=NSNotFound) {
                        NSString *complete=[text substringToIndex:last.location];
                        for(NSString *line in [complete componentsSeparatedByString:@"\n"]) {
                            NSDictionary *event=[NSJSONSerialization JSONObjectWithData:[line dataUsingEncoding:NSUTF8StringEncoding] options:0 error:NULL];
                            if(event[@"progress"]) progress([@"转换 " stringByAppendingString:name],0.1+0.9*(i+[event[@"progress"] doubleValue])/25);
                        }
                        NSData *remaining=[[text substringFromIndex:last.location+1] dataUsingEncoding:NSUTF8StringEncoding]; [lines setData:remaining];
                    }
                }
                [NSThread sleepForTimeInterval:0.03];
            }
            [task waitUntilExit]; pipe.fileHandleForReading.readabilityHandler=nil;
            NSString *diagnostic=[[NSString alloc] initWithData:[errors.fileHandleForReading readDataToEndOfFile] encoding:NSUTF8StringEncoding];
            need(!cancel(),@"已取消");
            need(task.terminationStatus==0,[NSString stringWithFormat:@"%@ 转换失败：%@",name,diagnostic]);
            hashes[relative]=R4Hash([stage stringByAppendingPathComponent:relative]);
        }
        NSDictionary *manifest=@{@"schema":@1,@"music_banks":@[@"MultiverseJourney"],@"files":hashes};
        NSData *data=[NSJSONSerialization dataWithJSONObject:manifest options:NSJSONWritingPrettyPrinted error:&error];
        need([data writeToFile:[stage stringByAppendingPathComponent:@"manifest.json"] options:NSDataWritingAtomic error:&error],error.localizedDescription);
        need(!cancel(),@"已取消");
        need([fm moveItemAtPath:stage toPath:committed error:&error],error.localizedDescription);
        NSData *pointer=[NSJSONSerialization dataWithJSONObject:@{@"generation":generation} options:0 error:&error];
        need([pointer writeToFile:[home stringByAppendingPathComponent:@"current.json"] options:NSDataWritingAtomic error:&error],error.localizedDescription);
        published=YES; progress(@"MultiverseJourney 音乐库已就绪",1);
        // Old generations remain valid until the pointer commit. Saves/config live outside them.
        if(previous) [fm removeItemAtPath:previous error:NULL];
        return manifest;
    } @finally {
        [fm removeItemAtPath:stage error:NULL];
        if(!published) [fm removeItemAtPath:committed error:NULL];
        flock(lock,LOCK_UN); close(lock);
    }
}

static NSDictionary *fontAtURL(NSURL *url, NSString *postscript) {
    NSArray *descriptors=CFBridgingRelease(CTFontManagerCreateFontDescriptorsFromURL((__bridge CFURLRef)url));
    for(NSUInteger i=0;i<descriptors.count;i++) {
        CTFontDescriptorRef descriptor=(__bridge CTFontDescriptorRef)descriptors[i];
        NSString *name=CFBridgingRelease(CTFontDescriptorCopyAttribute(descriptor,kCTFontNameAttribute));
        if(postscript && ![name isEqual:postscript]) continue;
        CTFontRef font=CTFontCreateWithFontDescriptor(descriptor,16,NULL);
        UniChar text[]={0x5927,0x5bcc,0x7fc1,0x9078,0x9805}; CGGlyph glyphs[5];
        BOOL cjk=CTFontGetGlyphsForCharacters(font,text,glyphs,5); CFRelease(font);
        if(cjk) return @{@"path":url.path,@"index":@(i),@"postscript":name};
    }
    return nil;
}
NSDictionary *R4Font(NSString *home, NSString *selected) {
    if(selected) return fontAtURL([NSURL fileURLWithPath:selected],nil);
    NSString *directory=[home stringByAppendingPathComponent:@"fonts"];
    for(NSString *entry in [[NSFileManager.defaultManager contentsOfDirectoryAtPath:directory error:NULL] sortedArrayUsingSelector:@selector(compare:)]) {
        NSDictionary *font=fontAtURL([NSURL fileURLWithPath:[directory stringByAppendingPathComponent:entry]],nil); if(font) return font;
    }
    CTFontRef base=CTFontCreateWithName(CFSTR("Songti TC"),16,NULL);
    CTFontRef chosen=CTFontCreateForString(base,CFSTR("大富翁選項"),CFRangeMake(0,5));
    NSURL *url=CFBridgingRelease(CTFontCopyAttribute(chosen,kCTFontURLAttribute));
    NSString *name=CFBridgingRelease(CTFontCopyPostScriptName(chosen)); CFRelease(chosen); CFRelease(base);
    return url?fontAtURL(url,name):nil;
}
