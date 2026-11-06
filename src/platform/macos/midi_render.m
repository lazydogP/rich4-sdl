/* Import-time renderer. MusicSequence owns SMF parsing and tempo conversion. */
#import <Foundation/Foundation.h>
#import <AudioToolbox/AudioToolbox.h>
#import <CommonCrypto/CommonDigest.h>
#include <signal.h>
#include <math.h>
#include <unistd.h>
#ifndef RICH4_CONVERTER_VERSION
#define RICH4_CONVERTER_VERSION "apple-midi-1"
#endif

static volatile sig_atomic_t cancelled;
static void cancel_render(int sig) { (void)sig; cancelled = 1; }
static void check(OSStatus status, NSString *operation) {
    if (status) @throw [NSException exceptionWithName:@"AppleAudioError"
        reason:[NSString stringWithFormat:@"%@ (OSStatus %d)", operation, (int)status] userInfo:nil];
}
static void emit(NSDictionary *value) {
    NSData *data = [NSJSONSerialization dataWithJSONObject:value options:0 error:NULL];
    fwrite(data.bytes, 1, data.length, stdout); fputc('\n', stdout); fflush(stdout);
}
static NSString *digest(NSData *data) {
    unsigned char hash[CC_SHA256_DIGEST_LENGTH];
    CC_SHA256(data.bytes, (CC_LONG)data.length, hash);
    NSMutableString *result = [NSMutableString string];
    for (unsigned i = 0; i < sizeof(hash); i++) [result appendFormat:@"%02x", hash[i]];
    return result;
}
static void require(BOOL ok, NSString *message) {
    if (!ok) @throw [NSException exceptionWithName:@"ImportError" reason:message userInfo:nil];
}

int main(int argc, const char **argv) { @autoreleasepool {
    if (argc < 3 || (argc != 3 && (argc != 5 || strcmp(argv[3], "--cache-dir")))) {
        fprintf(stderr, "usage: rich4-midi INPUT.mid OUTPUT.wav [--cache-dir DIR]\n"); return 2;
    }
    signal(SIGINT, cancel_render); signal(SIGTERM, cancel_render);
    NSString *source = @(argv[1]), *output = @(argv[2]);
    NSString *temporary = [output stringByAppendingFormat:@".%@.tmp", NSUUID.UUID.UUIDString];
    MusicSequence sequence = NULL; AudioUnit synth = NULL; ExtAudioFileRef file = NULL;
    int result = 1;
    @try {
        require(![[source stringByStandardizingPath] isEqual:[output stringByStandardizingPath]], @"Input and output must differ");
        NSFileManager *fm = NSFileManager.defaultManager;
        NSError *error = nil;
        NSData *midi = [NSData dataWithContentsOfFile:source options:0 error:&error];
        require(midi != nil, error.localizedDescription);
        NSMutableData *keyData = [midi mutableCopy];
        [keyData appendData:[@RICH4_CONVERTER_VERSION "|44100|stereo|pcm16|sequence-end" dataUsingEncoding:NSUTF8StringEncoding]];
        NSString *key = digest(keyData), *cache = nil, *cacheMeta = nil;
        if (argc == 5) {
            require([fm createDirectoryAtPath:@(argv[4]) withIntermediateDirectories:YES attributes:nil error:&error], error.localizedDescription);
            cache = [@(argv[4]) stringByAppendingPathComponent:[key stringByAppendingString:@".wav"]];
            cacheMeta = [cache stringByAppendingString:@".json"];
            NSDictionary *meta = [NSJSONSerialization JSONObjectWithData:[NSData dataWithContentsOfFile:cacheMeta] ?: [NSData data] options:0 error:NULL];
            NSData *cached = [NSData dataWithContentsOfFile:cache];
            if (cached && [meta[@"sha256"] isEqual:digest(cached)]) {
                require([fm copyItemAtPath:cache toPath:temporary error:&error], error.localizedDescription);
                require(!cancelled, @"Cancelled");
                require(rename(temporary.fileSystemRepresentation, output.fileSystemRepresentation) == 0, @"Cannot commit cached WAV");
                NSMutableDictionary *status = [meta mutableCopy]; status[@"cached"] = @YES; status[@"source"] = source;
                emit(status); return 0;
            }
        }
        check(NewMusicSequence(&sequence), @"NewMusicSequence");
        check(MusicSequenceFileLoad(sequence, (__bridge CFURLRef)[NSURL fileURLWithPath:source], 0, 0), @"MusicSequenceFileLoad");
        UInt32 count = 0; check(MusicSequenceGetTrackCount(sequence, &count), @"MusicSequenceGetTrackCount");
        NSMutableArray<NSDictionary *> *events = [NSMutableArray array];
        MusicTimeStamp endBeat = 0;
        for (UInt32 t = 0; t < count; t++) {
            MusicTrack track; check(MusicSequenceGetIndTrack(sequence,t,&track), @"MusicSequenceGetIndTrack");
            MusicTimeStamp length = 0; UInt32 size = sizeof(length);
            check(MusicTrackGetProperty(track,kSequenceTrackProperty_TrackLength,&length,&size), @"TrackLength");
            endBeat = MAX(endBeat,length);
            MusicEventIterator iterator; check(NewMusicEventIterator(track,&iterator), @"NewMusicEventIterator");
            @try {
                Boolean more = false; check(MusicEventIteratorHasCurrentEvent(iterator,&more), @"HasCurrentEvent");
                while (more) {
                    MusicTimeStamp beat; MusicEventType type; const void *data; UInt32 dataSize;
                    check(MusicEventIteratorGetEventInfo(iterator,&beat,&type,&data,&dataSize), @"GetEventInfo");
                    Float64 seconds; check(MusicSequenceGetSecondsForBeats(sequence,beat,&seconds), @"SecondsForBeats");
                    long long frame = llround(seconds * 44100);
                    if (type == kMusicEventType_MIDINoteMessage) {
                        const MIDINoteMessage *note = data;
                        uint8_t on[] = {0x90 | note->channel,note->note,note->velocity};
                        uint8_t off[] = {0x80 | note->channel,note->note,note->releaseVelocity};
                        [events addObject:@{@"frame":@(frame),@"data":[NSData dataWithBytes:on length:3]}];
                        check(MusicSequenceGetSecondsForBeats(sequence,beat+note->duration,&seconds), @"NoteEndSeconds");
                        [events addObject:@{@"frame":@(llround(seconds*44100)),@"data":[NSData dataWithBytes:off length:3]}];
                    } else if (type == kMusicEventType_MIDIChannelMessage) {
                        const MIDIChannelMessage *message = data;
                        uint8_t bytes[] = {message->status,message->data1,message->data2};
                        [events addObject:@{@"frame":@(frame),@"data":[NSData dataWithBytes:bytes length:3]}];
                    } else if (type == kMusicEventType_MIDIRawData) {
                        const MIDIRawData *raw = data;
                        [events addObject:@{@"frame":@(frame),@"sysex":@YES,@"data":[NSData dataWithBytes:raw->data length:raw->length]}];
                    }
                    check(MusicEventIteratorNextEvent(iterator), @"NextEvent");
                    check(MusicEventIteratorHasCurrentEvent(iterator,&more), @"HasCurrentEvent");
                }
            } @finally { DisposeMusicEventIterator(iterator); }
        }
        [events sortWithOptions:NSSortStable usingComparator:^NSComparisonResult(NSDictionary *a, NSDictionary *b) { return [a[@"frame"] compare:b[@"frame"]]; }];
        Float64 seconds; check(MusicSequenceGetSecondsForBeats(sequence,endBeat,&seconds), @"SequenceEndSeconds");
        long long total = llround(seconds * 44100);
        AudioComponentDescription description = {kAudioUnitType_MusicDevice,kAudioUnitSubType_DLSSynth,kAudioUnitManufacturer_Apple,0,0};
        AudioComponent component = AudioComponentFindNext(NULL,&description);
        require(component != NULL, @"Apple DLSMusicDevice unavailable");
        check(AudioComponentInstanceNew(component,&synth), @"AudioComponentInstanceNew");
        UInt32 offline = 1, maximum = 512;
        check(AudioUnitSetProperty(synth,kAudioUnitProperty_OfflineRender,kAudioUnitScope_Global,0,&offline,sizeof(offline)), @"OfflineRender");
        check(AudioUnitSetProperty(synth,kAudioUnitProperty_MaximumFramesPerSlice,kAudioUnitScope_Global,0,&maximum,sizeof(maximum)), @"MaximumFramesPerSlice");
        AudioStreamBasicDescription format = {44100,kAudioFormatLinearPCM,kAudioFormatFlagsNativeFloatPacked|kAudioFormatFlagIsNonInterleaved,4,1,4,2,32,0};
        check(AudioUnitSetProperty(synth,kAudioUnitProperty_StreamFormat,kAudioUnitScope_Output,0,&format,sizeof(format)), @"StreamFormat");
        check(AudioUnitInitialize(synth), @"AudioUnitInitialize");
        AudioStreamBasicDescription pcm = {44100,kAudioFormatLinearPCM,kAudioFormatFlagIsSignedInteger|kAudioFormatFlagIsPacked,4,1,4,2,16,0};
        check(ExtAudioFileCreateWithURL((__bridge CFURLRef)[NSURL fileURLWithPath:temporary],kAudioFileWAVEType,&pcm,NULL,kAudioFileFlags_EraseFile,&file), @"CreateWAV");
        check(ExtAudioFileSetProperty(file,kExtAudioFileProperty_ClientDataFormat,sizeof(format),&format), @"ClientDataFormat");
        float left[512], right[512];
        struct { UInt32 count; AudioBuffer buffers[2]; } storage = {2,{{1,sizeof(left),left},{1,sizeof(right),right}}};
        AudioBufferList *buffers = (AudioBufferList *)&storage;
        long long position = 0, nextReport = 0; NSUInteger index = 0; double peak = 0, squares = 0;
        while (position < total) {
            require(!cancelled, @"Cancelled");
            while (index < events.count && [events[index][@"frame"] longLongValue] <= position) {
                NSDictionary *event = events[index++]; NSData *bytes = event[@"data"];
                if (event[@"sysex"]) check(MusicDeviceSysEx(synth,bytes.bytes,(UInt32)bytes.length), @"MusicDeviceSysEx");
                else { const uint8_t *b = bytes.bytes; check(MusicDeviceMIDIEvent(synth,b[0],b[1],b[2],0), @"MusicDeviceMIDIEvent"); }
            }
            long long stop = MIN(total,position+512);
            if (index < events.count) stop = MIN(stop,[events[index][@"frame"] longLongValue]);
            UInt32 frames = (UInt32)(stop-position);
            for (int b=0;b<2;b++) buffers->mBuffers[b].mDataByteSize=frames*sizeof(float);
            AudioTimeStamp time = {0}; time.mSampleTime=position; time.mFlags=kAudioTimeStampSampleTimeValid;
            AudioUnitRenderActionFlags flags = kAudioOfflineUnitRenderAction_Render;
            check(AudioUnitRender(synth,&flags,&time,0,frames,buffers), @"AudioUnitRender");
            check(ExtAudioFileWrite(file,frames,buffers), @"WriteWAV");
            for (UInt32 f=0;f<frames;f++) { peak=MAX(peak,MAX(fabs(left[f]),fabs(right[f]))); squares+=(double)left[f]*left[f]+(double)right[f]*right[f]; }
            position=stop;
            if (position >= nextReport) { emit(@{@"progress":@(total ? (double)position/total : 1),@"source":source}); nextReport=position+44100; }
        }
        OSStatus closed = ExtAudioFileDispose(file); file=NULL; check(closed,@"CloseWAV");
        require(!cancelled, @"Cancelled");
        require(rename(temporary.fileSystemRepresentation,output.fileSystemRepresentation)==0,@"Cannot commit rendered WAV");
        NSMutableDictionary *meta = [@{@"source":source,@"frames":@(total),@"seconds":@(seconds),@"peak":@(peak),@"rms":@(total?sqrt(squares/(2*total)):0),@"cached":@NO,@"converter":@RICH4_CONVERTER_VERSION} mutableCopy];
        if (cache) {
            NSData *wav=[NSData dataWithContentsOfFile:output]; meta[@"sha256"]=digest(wav);
            require([wav writeToFile:cache options:NSDataWritingAtomic error:&error],error.localizedDescription);
            require([[NSJSONSerialization dataWithJSONObject:meta options:0 error:&error] writeToFile:cacheMeta options:NSDataWritingAtomic error:&error],error.localizedDescription);
        }
        emit(meta); result=0;
    } @catch (NSException *exception) {
        fprintf(stderr,"%s: %s\n",source.UTF8String,exception.reason.UTF8String);
        result=cancelled?130:1;
    } @finally {
        if(file) ExtAudioFileDispose(file);
        if(synth) { AudioUnitUninitialize(synth); AudioComponentInstanceDispose(synth); }
        if(sequence) DisposeMusicSequence(sequence);
        unlink(temporary.fileSystemRepresentation);
    }
    return result;
} }
