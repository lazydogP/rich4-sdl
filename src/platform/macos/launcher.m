#import <AppKit/AppKit.h>
#import "import.h"
#include <stdatomic.h>
#include <sys/file.h>
#include <unistd.h>

@interface Launcher : NSObject <NSApplicationDelegate> {
    atomic_bool cancelled;
}
@property NSWindow *window;
@property NSTextField *status;
@property NSProgressIndicator *progress;
@property NSButton *choose, *start, *stop, *font;
@property NSString *home, *logDirectory;
@property NSTask *game;
@property BOOL busy;
@end
@implementation Launcher
- (NSButton *)button:(NSString *)title action:(SEL)action x:(CGFloat)x {
    NSButton *button=[[NSButton alloc] initWithFrame:NSMakeRect(x,24,145,32)];
    button.title=title; button.target=self; button.action=action;
    button.bezelStyle=NSBezelStyleRounded; [button setButtonType:NSButtonTypeMomentaryPushIn];
    button.frame=NSMakeRect(x,24,145,32); [self.window.contentView addSubview:button]; return button;
}
- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    (void)notification;
    self.home=[NSHomeDirectory() stringByAppendingPathComponent:@"Library/Application Support/Rich4"];
    self.logDirectory=[NSHomeDirectory() stringByAppendingPathComponent:@"Library/Logs/Rich4"];
    NSFileManager *fm=NSFileManager.defaultManager;
    [fm createDirectoryAtPath:self.logDirectory withIntermediateDirectories:YES attributes:nil error:NULL];
    freopen([[self.logDirectory stringByAppendingPathComponent:@"launcher.log"] fileSystemRepresentation],"a",stderr);
    NSLog(@"Rich4 launcher started");
    self.window=[[NSWindow alloc] initWithContentRect:NSMakeRect(0,0,660,225) styleMask:NSWindowStyleMaskTitled|NSWindowStyleMaskClosable backing:NSBackingStoreBuffered defer:NO];
    self.window.title=@"大富翁 4 · Rich4"; [self.window center];
    self.status=[[NSTextField alloc] initWithFrame:NSMakeRect(24,125,612,72)];
    self.status.stringValue=@"首次运行请选择合法持有、已解压的游戏目录。";
    self.status.editable=NO; self.status.selectable=NO; self.status.bezeled=NO; self.status.drawsBackground=NO;
    self.status.font=[NSFont systemFontOfSize:NSFont.systemFontSize];
    NSTextFieldCell *statusCell=self.status.cell;
    statusCell.wraps=YES; statusCell.scrollable=NO; statusCell.lineBreakMode=NSLineBreakByWordWrapping;
    self.status.frame=NSMakeRect(24,125,612,72); [self.window.contentView addSubview:self.status];
    self.progress=[[NSProgressIndicator alloc] initWithFrame:NSMakeRect(24,87,612,20)];
    self.progress.indeterminate=NO; self.progress.maxValue=1; [self.window.contentView addSubview:self.progress];
    self.choose=[self button:@"选择原版游戏目录…" action:@selector(chooseDirectory:) x:24];
    self.font=[self button:@"选择中文字体…" action:@selector(chooseFont:) x:180];
    self.start=[self button:@"启动游戏" action:@selector(startGame:) x:336];
    self.stop=[self button:@"取消" action:@selector(cancel:) x:492]; self.stop.enabled=NO;
    NSMenu *menu=[NSMenu new]; NSMenuItem *root=[NSMenuItem new]; NSMenu *app=[NSMenu new];
    [app addItemWithTitle:@"重新导入…" action:@selector(chooseDirectory:) keyEquivalent:@"i"];
    [app addItemWithTitle:@"选择中文字体…" action:@selector(chooseFont:) keyEquivalent:@"f"];
    [app addItem:NSMenuItem.separatorItem]; [app addItemWithTitle:@"退出 Rich4" action:@selector(terminate:) keyEquivalent:@"q"];
    root.submenu=app; [menu addItem:root]; NSApp.mainMenu=menu;
    [self.window makeKeyAndOrderFront:nil]; [NSApp activateIgnoringOtherApps:YES];
    self.start.enabled=R4Current(self.home)!=nil;
    if(self.start.enabled) [self startGame:nil];
}
- (void)setWorking:(BOOL)working {
    self.busy=working; self.choose.enabled=!working; self.font.enabled=!working;
    self.start.enabled=!working && R4Current(self.home)!=nil; self.stop.enabled=working;
}
- (void)failure:(NSString *)message {
    NSLog(@"%@",message); self.status.stringValue=message;
    NSAlert *alert=[NSAlert new]; alert.messageText=@"导入未完成"; alert.informativeText=message;
    [alert addButtonWithTitle:@"重试"]; [alert addButtonWithTitle:@"返回"];
    if([alert runModal]==NSAlertFirstButtonReturn) [self chooseDirectory:nil];
}
- (void)cancel:(id)sender { (void)sender; atomic_store(&cancelled,true); self.status.stringValue=@"正在取消…"; }
- (void)chooseDirectory:(id)sender {
    (void)sender; if(self.busy||self.game.running) return;
    NSOpenPanel *panel=[NSOpenPanel openPanel]; panel.canChooseDirectories=YES; panel.canChooseFiles=NO;
    panel.message=@"选择包含 MKF 和 MIDI 文件的游戏目录，也可以选择包含多个游戏目录的文件夹。";
    if([panel runModal]!=NSModalResponseOK) return;
    atomic_store(&cancelled,false); [self setWorking:YES]; self.status.stringValue=@"正在识别游戏目录…";
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED,0),^{ @autoreleasepool {
        @try {
            NSArray *candidates=R4Scan(panel.URL.path,^BOOL{ return atomic_load(&self->cancelled); });
            dispatch_async(dispatch_get_main_queue(),^{
                [self setWorking:NO];
                if(!candidates.count) { [self failure:@"未找到 Data.mkf。请选择已解压的原版游戏目录。"]; return; }
                NSDictionary *candidate=candidates.firstObject;
                if(candidates.count>1) {
                    NSAlert *alert=[NSAlert new]; alert.messageText=@"选择要导入的游戏目录";
                    NSPopUpButton *list=[[NSPopUpButton alloc] initWithFrame:NSMakeRect(0,0,580,28)];
                    for(NSDictionary *entry in candidates) [list addItemWithTitle:entry[@"root"]];
                    alert.accessoryView=list; [alert addButtonWithTitle:@"导入"]; [alert addButtonWithTitle:@"取消"];
                    if([alert runModal]!=NSAlertFirstButtonReturn) return;
                    candidate=candidates[list.indexOfSelectedItem];
                }
                if([candidate[@"problems"] count]) { [self failure:[candidate[@"problems"] componentsJoinedByString:@"\n"]]; return; }
                [self importCandidate:candidate];
            });
        } @catch(NSException *e) { dispatch_async(dispatch_get_main_queue(),^{ [self setWorking:NO]; [self failure:e.reason]; }); }
    } });
}
- (void)importCandidate:(NSDictionary *)candidate {
    [self setWorking:YES]; atomic_store(&cancelled,false);
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED,0),^{ @autoreleasepool {
        @try {
            R4Import(candidate,self.home,[NSBundle.mainBundle.executablePath.stringByDeletingLastPathComponent stringByAppendingPathComponent:@"rich4-midi"],^BOOL{ return atomic_load(&self->cancelled); },^(NSString *message,double value){
                dispatch_async(dispatch_get_main_queue(),^{ self.status.stringValue=message; self.progress.doubleValue=value; });
            });
            dispatch_async(dispatch_get_main_queue(),^{ [self setWorking:NO]; self.status.stringValue=@"导入完成：已生成 MultiverseJourney 音乐库。"; [self startGame:nil]; });
        } @catch(NSException *e) { dispatch_async(dispatch_get_main_queue(),^{ [self setWorking:NO]; if(atomic_load(&self->cancelled)) self.status.stringValue=@"已取消，现有安装保持可用。"; else [self failure:e.reason]; }); }
    } });
}
- (void)chooseFont:(id)sender {
    (void)sender; if(self.busy||self.game.running) return;
    NSOpenPanel *panel=[NSOpenPanel openPanel]; panel.message=@"选择包含繁体中文字符的 TTF、TTC 或 OTF 字体。";
    if([panel runModal]!=NSModalResponseOK) return;
    NSDictionary *font=R4Font(self.home,panel.URL.path);
    if(!font) { self.status.stringValue=@"该字体未包含所需中文字符，请选择其他字体。"; return; }
    NSString *dir=[self.home stringByAppendingPathComponent:@"fonts"];
    NSFileManager *fm=NSFileManager.defaultManager; NSError *error=nil;
    if(![fm createDirectoryAtPath:dir withIntermediateDirectories:YES attributes:nil error:&error]) { self.status.stringValue=error.localizedDescription; return; }
    NSString *destination=[dir stringByAppendingPathComponent:[@"user-font." stringByAppendingString:panel.URL.path.pathExtension]];
    NSData *data=[NSData dataWithContentsOfURL:panel.URL options:0 error:&error];
    if(!data||![data writeToFile:destination options:NSDataWritingAtomic error:&error]) { self.status.stringValue=error.localizedDescription; return; }
    [[NSUserDefaults standardUserDefaults] setObject:destination forKey:@"fontPath"];
    self.status.stringValue=@"已保存字体，下一次启动游戏时使用。";
}
- (void)startGame:(id)sender {
    (void)sender; if(self.busy||self.game.running) return;
    NSDictionary *current=R4Current(self.home); if(!current) { self.status.stringValue=@"请先导入原版游戏资源。"; return; }
    NSDictionary *font=R4Font(self.home,[[NSUserDefaults standardUserDefaults] stringForKey:@"fontPath"]);
    if(!font) { self.status.stringValue=@"未找到可用中文字体，请选择字体文件。"; return; }
    NSString *input=[self.home stringByAppendingPathComponent:@"input.toml"];
    NSFileManager *fm=NSFileManager.defaultManager; NSError *error=nil;
    if(![fm fileExistsAtPath:input] && ![fm copyItemAtPath:[NSBundle.mainBundle.resourcePath stringByAppendingPathComponent:@"input.toml"] toPath:input error:&error]) { self.status.stringValue=error.localizedDescription; return; }
    NSTask *task=[NSTask new]; self.game=task;
    task.launchPath=[NSBundle.mainBundle.executablePath.stringByDeletingLastPathComponent stringByAppendingPathComponent:@"rich4-game"];
    task.currentDirectoryPath=self.home;
    task.arguments=@[@"--data-dir",[current[@"path"] stringByAppendingPathComponent:@"original"],@"--music-dir",[current[@"path"] stringByAppendingPathComponent:@"music"],@"--font",font[@"path"],@"--font-face-index",[font[@"index"] stringValue],@"--input-config",input];
    // AppKit launcher uses Retina; the child SDL window keeps its logical pixel size.
    NSMutableDictionary *env=[NSProcessInfo.processInfo.environment mutableCopy]; env[@"SDL12COMPAT_HIGHDPI"]=@"0";
    task.environment=env;
    NSString *log=[self.logDirectory stringByAppendingPathComponent:@"game.log"];
    if(![fm fileExistsAtPath:log]) [fm createFileAtPath:log contents:nil attributes:nil];
    NSFileHandle *handle=[NSFileHandle fileHandleForWritingAtPath:log]; [handle seekToEndOfFile];
    task.standardOutput=handle; task.standardError=handle;
    task.terminationHandler=^(NSTask *finished){ dispatch_async(dispatch_get_main_queue(),^{
        [handle closeFile]; [self.window makeKeyAndOrderFront:nil]; [NSApp activateIgnoringOtherApps:YES];
        self.choose.enabled=YES; self.font.enabled=YES; self.start.enabled=YES;
        self.status.stringValue=finished.terminationStatus==0?@"游戏已退出。可再次启动或重新导入。":[@"游戏退出异常，请查看日志：" stringByAppendingString:log];
    }); };
    if(!R4LaunchTask(task,&error)) { [handle closeFile]; self.status.stringValue=error.localizedDescription; return; }
    self.choose.enabled=NO; self.font.enabled=NO; self.start.enabled=NO; [self.window orderOut:nil];
}
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender {
    (void)sender;
    if(self.busy) { atomic_store(&cancelled,true); self.status.stringValue=@"正在取消，请稍后退出。"; return NSTerminateCancel; }
    if(self.game.running) { [self.game terminate]; }
    return NSTerminateNow;
}
- (BOOL)applicationShouldHandleReopen:(NSApplication *)sender hasVisibleWindows:(BOOL)visible {
    (void)sender; if(!visible) [self.window makeKeyAndOrderFront:nil]; return YES;
}
@end

int main(void) { @autoreleasepool {
    NSApplication *app=NSApplication.sharedApplication; [app setActivationPolicy:NSApplicationActivationPolicyRegular];
    Launcher *delegate=[Launcher new]; app.delegate=delegate; [app run]; return 0;
} }
