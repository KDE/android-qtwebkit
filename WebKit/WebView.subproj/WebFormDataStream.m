/*
    WebFormDataStream.m
    originally written by Becky Willrich, additional code by Darin Adler
    Copyright 2005 Apple Computer, Inc.
*/

#import "WebFormDataStream.h"

#import <sys/types.h>
#import <sys/stat.h>

#import <CoreFoundation/CFStreamAbstract.h>

#import "WebAssertions.h"
#import "WebNSObjectExtras.h"

#if !BUILDING_ON_PANTHER

static void formEventCallback(CFReadStreamRef stream, CFStreamEventType type, void *context);

typedef struct {
    CFMutableSetRef scheduledRunLoopPairs;
    CFMutableArrayRef formDataArray;
    CFReadStreamRef currentStream;
    CFDataRef currentData;
    CFReadStreamRef formStream;
} FormStreamFields;

typedef struct {
    CFRunLoopRef runLoop;
    CFStringRef mode;
} SchedulePair;

static const void *pairRetain(CFAllocatorRef alloc, const void *value)
{
    const SchedulePair *pair = (const SchedulePair *)value;

    SchedulePair *result = CFAllocatorAllocate(alloc, sizeof(SchedulePair), 0);
    CFRetain(pair->runLoop);
    result->runLoop = pair->runLoop;
    result->mode = CFStringCreateCopy(alloc, pair->mode);
    return result;
}

static void pairRelease(CFAllocatorRef alloc, const void *value)
{
    const SchedulePair *pair = (const SchedulePair *)value;

    CFRelease(pair->runLoop);
    CFRelease(pair->mode);
    CFAllocatorDeallocate(alloc, (void *)pair);
}

static Boolean pairEqual(const void *a, const void *b)
{
    const SchedulePair *pairA = (const SchedulePair *)a;
    const SchedulePair *pairB = (const SchedulePair *)b;

    return pairA->runLoop == pairB->runLoop && CFEqual(pairA->mode, pairB->mode);
}

static CFHashCode pairHash(const void *value)
{
    const SchedulePair *pair = (const SchedulePair *)value;

    return ((CFHashCode)pair->runLoop) ^ CFHash(pair->mode);
}

static void closeCurrentStream(FormStreamFields *form)
{
    if (form->currentStream) {
        CFReadStreamClose(form->currentStream);
        CFReadStreamSetClient(form->currentStream, kCFStreamEventNone, NULL, NULL);
        CFRelease(form->currentStream);
        form->currentStream = NULL;
    }
    if (form->currentData) {
        CFRelease(form->currentData);
        form->currentData = NULL;
    }
}

static void scheduleWithPair(const void *value, void *context)
{
    const SchedulePair *pair = (const SchedulePair *)value;
    CFReadStreamRef stream = (CFReadStreamRef)context;

    CFReadStreamScheduleWithRunLoop(stream, pair->runLoop, pair->mode);
}

static void advanceCurrentStream(FormStreamFields *form)
{
    closeCurrentStream(form);

    // Handle the case where we're at the end of the array.
    if (CFArrayGetCount(form->formDataArray) == 0) {
        return;
    }

    // Create the new stream.
    CFAllocatorRef alloc = CFGetAllocator(form->formDataArray);
    CFTypeRef nextInput = CFArrayGetValueAtIndex(form->formDataArray, 0);
    if (CFGetTypeID(nextInput) == CFDataGetTypeID()) {
        // nextInput is a CFData containing an absolute path
        CFDataRef data = (CFDataRef)nextInput;
        form->currentStream = CFReadStreamCreateWithBytesNoCopy(alloc, CFDataGetBytePtr(data), CFDataGetLength(data), kCFAllocatorNull);
        form->currentData = data;
        CFRetain(data);
    } else {
        // nextInput is a CFString containing an absolute path
        CFStringRef path = (CFStringRef)nextInput;
        CFURLRef fileURL = CFURLCreateWithFileSystemPath(alloc, path, kCFURLPOSIXPathStyle, FALSE);
        form->currentStream = CFReadStreamCreateWithFile(alloc, fileURL);
        CFRelease(fileURL);
    }
    CFArrayRemoveValueAtIndex(form->formDataArray, 0);

    // Set up the callback.
    CFStreamClientContext context = { 0, form, NULL, NULL, NULL };
    CFReadStreamSetClient(form->currentStream, kCFStreamEventHasBytesAvailable | kCFStreamEventErrorOccurred | kCFStreamEventEndEncountered,
        formEventCallback, &context);

    // Schedule with the current set of run loops.
    CFSetApplyFunction(form->scheduledRunLoopPairs, scheduleWithPair, form->currentStream);
}

static void openNextStream(FormStreamFields *form)
{
    // Skip over any streams we can't open.
    // For some purposes we might want to return an error, but the current NSURLConnection
    // can't really do anything useful with an error at this point, so this is better.
    advanceCurrentStream(form);
    while (form->currentStream && !CFReadStreamOpen(form->currentStream)) {
        advanceCurrentStream(form);
    }
}

static void *formCreate(CFReadStreamRef stream, void *context)
{
    CFArrayRef formDataArray = (CFArrayRef)context;

    CFSetCallBacks runLoopAndModeCallBacks = { 0, pairRetain, pairRelease, NULL, pairEqual, pairHash };

    CFAllocatorRef alloc = CFGetAllocator(stream);
    FormStreamFields *newInfo = CFAllocatorAllocate(alloc, sizeof(FormStreamFields), 0);
    newInfo->scheduledRunLoopPairs = CFSetCreateMutable(alloc, 0, &runLoopAndModeCallBacks);
    newInfo->formDataArray = CFArrayCreateMutableCopy(alloc, CFArrayGetCount(formDataArray), formDataArray);
    newInfo->currentStream = NULL;
    newInfo->currentData = NULL;
    newInfo->formStream = stream; // Don't retain. That would create a reference cycle.
    return newInfo;
}

static void formFinalize(CFReadStreamRef stream, void *context)
{
    FormStreamFields *form = (FormStreamFields *)context;

    closeCurrentStream(context);
    CFRelease(form->scheduledRunLoopPairs);
    CFRelease(form->formDataArray);
    CFAllocatorDeallocate(CFGetAllocator(stream), context);
}

static Boolean formOpen(CFReadStreamRef stream, CFStreamError *error, Boolean *openComplete, void *context)
{
    FormStreamFields *form = (FormStreamFields *)context;

    openNextStream(form);

    *openComplete = TRUE;
    error->error = 0;
    return TRUE;
}

static CFIndex formRead(CFReadStreamRef stream, UInt8 *buffer, CFIndex bufferLength, CFStreamError *error, Boolean *atEOF, void *context)
{
    FormStreamFields *form = (FormStreamFields *)context;

    while (form->currentStream) {
        CFIndex bytesRead = CFReadStreamRead(form->currentStream, buffer, bufferLength);
        if (bytesRead < 0) {
            *error = CFReadStreamGetError(form->currentStream);
            return -1;
        }
        if (bytesRead > 0) {
            error->error = 0;
            *atEOF = FALSE;
            return bytesRead;
        }
        openNextStream(form);
    }

    error->error = 0;
    *atEOF = TRUE;
    return 0;
}

static Boolean formCanRead(CFReadStreamRef stream, void *context)
{
    FormStreamFields *form = (FormStreamFields *)context;

    while (form->currentStream && CFReadStreamGetStatus(form->currentStream) == kCFStreamStatusAtEnd) {
        openNextStream(form);
    }
    if (!form->currentStream) {
        CFReadStreamSignalEvent(stream, kCFStreamEventEndEncountered, NULL);
        return FALSE;
    }
    return CFReadStreamHasBytesAvailable(form->currentStream);
}

static void formClose(CFReadStreamRef stream, void *context)
{
    FormStreamFields *form = (FormStreamFields *)context;

    closeCurrentStream(form);
}

static void formSchedule(CFReadStreamRef stream, CFRunLoopRef runLoop, CFStringRef runLoopMode, void *context)
{
    FormStreamFields *form = (FormStreamFields *)context;

    if (form->currentStream) {
        CFReadStreamScheduleWithRunLoop(form->currentStream, runLoop, runLoopMode);
    }
    SchedulePair pair = { runLoop, runLoopMode };
    CFSetAddValue(form->scheduledRunLoopPairs, &pair);
}

static void formUnschedule(CFReadStreamRef stream, CFRunLoopRef runLoop, CFStringRef runLoopMode, void *context)
{
    FormStreamFields *form = (FormStreamFields *)context;

    if (form->currentStream) {
        CFReadStreamUnscheduleFromRunLoop(form->currentStream, runLoop, runLoopMode);
    }
    SchedulePair pair = { runLoop, runLoopMode };
    CFSetRemoveValue(form->scheduledRunLoopPairs, &pair);
}

static void formEventCallback(CFReadStreamRef stream, CFStreamEventType type, void *context)
{
    FormStreamFields *form = (FormStreamFields *)context;

    switch (type) {
    case kCFStreamEventHasBytesAvailable:
        CFReadStreamSignalEvent(form->formStream, kCFStreamEventHasBytesAvailable, NULL);
        break;
    case kCFStreamEventErrorOccurred: {
        CFStreamError readStreamError = CFReadStreamGetError(stream);
        CFReadStreamSignalEvent(form->formStream, kCFStreamEventErrorOccurred, &readStreamError);
        break;
    }
    case kCFStreamEventEndEncountered:
        openNextStream(form);
        if (!form->currentStream) {
            CFReadStreamSignalEvent(form->formStream, kCFStreamEventEndEncountered, NULL);
        }
        break;
    case kCFStreamEventNone:
        ERROR("unexpected kCFStreamEventNone");
        break;
    case kCFStreamEventOpenCompleted:
        ERROR("unexpected kCFStreamEventOpenCompleted");
        break;
    case kCFStreamEventCanAcceptBytes:
        ERROR("unexpected kCFStreamEventCanAcceptBytes");
        break;
    }
}

#endif // !BUILDING_ON_PANTHER

void webSetHTTPBody(NSMutableURLRequest *request, NSArray *formData)
{
    unsigned count = [formData count];

    // Handle the common special case of one piece of form data, with no files.
    if (count == 1) {
        id d = [formData objectAtIndex:0];
        if ([d isKindOfClass:[NSData class]]) {
            [request setHTTPBody:(NSData *)d];
            return;
        }
    }

#if !BUILDING_ON_PANTHER

    // Precompute the content length so NSURLConnection doesn't use chunked mode.
    long long length = 0;
    unsigned i;
    for (i = 0; i < count; ++i) {
        id data = [formData objectAtIndex:i];
        if ([data isKindOfClass:[NSData class]]) {
            NSData *d = data;
            length += [d length];
        } else if ([data isKindOfClass:[NSString class]]) {
            NSString *s = data;
            struct stat sb;
            int statResult = stat([s fileSystemRepresentation], &sb);
            if (statResult == 0 && (sb.st_mode & S_IFMT) == S_IFREG) {
                length += sb.st_size;
            }
        } else {
            ERROR("item in form data array is neither NSData nor NSString");
            return;
        }
    }

    // Set the length.
    [request setValue:[NSString stringWithFormat:@"%lld", length] forHTTPHeaderField:@"Content-Length"];

    // Create and set the stream.
    CFReadStreamCallBacks callBacks = { 1, formCreate, formFinalize, NULL,
        formOpen, NULL, formRead, NULL, formCanRead, formClose,
        NULL, NULL, NULL, formSchedule, formUnschedule
    };
    CFReadStreamRef stream = CFReadStreamCreate(NULL, &callBacks, formData);
    [request setHTTPBodyStream:(NSInputStream *)stream];
    CFRelease(stream);

#else

    NSMutableData *allData = [[NSMutableData alloc] init];

    unsigned i;
    for (i = 0; i < count; ++i) {
        id data = [formData objectAtIndex:i];
        if ([data isKindOfClass:[NSData class]]) {
            NSData *d = data;
            [allData appendData:d];
        } else if ([data isKindOfClass:[NSString class]]) {
            NSString *s = data;
            NSData *d = [[NSData alloc] initWithContentsOfFile:s];
            if (d != nil) {
                [allData appendData:d];
                [d release];
            }
        } else {
            ERROR("item in form data array is neither NSData nor NSString");
            return;
        }
    }

    [request setHTTPBody:allData];
    
    [allData release];

#endif
}
