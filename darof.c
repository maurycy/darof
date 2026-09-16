#include <CoreFoundation/CoreFoundation.h>
#include <DiskArbitration/DiskArbitration.h>
#include <dispatch/dispatch.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Private status handled by __DARequestMount() in Apple's DARequest.c. */
#define kDissentReadOnly ((DAReturn)0xF8DAFF02)

static DAReturn mode;
static dispatch_queue_t log_queue;
static dispatch_semaphore_t log_slots;

static void describe(DADiskRef disk, char *buf, size_t len)
{
    if (!len) return;
    const char *bsd = DADiskGetBSDName(disk);
    snprintf(buf, len, "%s", bsd ? bsd : "?");
    CFDictionaryRef d = DADiskCopyDescription(disk);
    if (!d) return;
    CFStringRef name = CFDictionaryGetValue(d, kDADiskDescriptionVolumeNameKey);
    if (name) {
        size_t n = strlen(buf);
        if (len - n > 2) {
            CFIndex used = 0;
            CFStringGetBytes(name, CFRangeMake(0, CFStringGetLength(name)),
                             kCFStringEncodingUTF8, 0, false,
                             (UInt8 *)(buf + n + 1), (CFIndex)(len - n - 2), &used);
            if (used) {
                buf[n] = ' ';
                buf[n + 1 + (size_t)used] = '\0';
            }
        }
    }
    CFRelease(d);
}

static void log_disk(void *ctx)
{
    printf("%s: %s\n", mode == kDissentReadOnly ? "read-only" : "deny", (char *)ctx);
    fflush(stdout);
    free(ctx);
    dispatch_semaphore_signal(log_slots);
}

static DADissenterRef approve(DADiskRef disk, void *ctx __unused)
{
    /* Drop logs if the output stalls; never wait for it in this callback. */
    if (dispatch_semaphore_wait(log_slots, DISPATCH_TIME_NOW) == 0) {
        char buf[512];
        describe(disk, buf, sizeof buf);
        char *message = strdup(buf);
        if (message) dispatch_async_f(log_queue, message, log_disk);
        else dispatch_semaphore_signal(log_slots);
    }
    return DADissenterCreate(kCFAllocatorDefault, mode, NULL);
}

int main(int argc, char **argv)
{
    int all = 0;
    mode = kDissentReadOnly;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--deny")) mode = kDAReturnNotPermitted;
        else if (!strcmp(argv[i], "--all")) all = 1;
        else {
            fprintf(stderr, "usage: %s [--deny] [--all]\n", argv[0]);
            return 2;
        }
    }

    DASessionRef session = DASessionCreate(kCFAllocatorDefault);
    if (!session) {
        fprintf(stderr, "darof: cannot create Disk Arbitration session\n");
        return 1;
    }

    CFMutableDictionaryRef match = NULL;
    if (!all) {
        match = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        if (!match) {
            fprintf(stderr, "darof: cannot create disk match dictionary\n");
            CFRelease(session);
            return 1;
        }
        CFDictionarySetValue(match, kDADiskDescriptionDeviceInternalKey, kCFBooleanFalse);
    }

    log_queue = dispatch_queue_create("com.maurycy.darof.log", DISPATCH_QUEUE_SERIAL);
    log_slots = dispatch_semaphore_create(64);
    if (!log_queue || !log_slots) {
        fprintf(stderr, "darof: cannot initialize logging\n");
        if (log_queue) dispatch_release(log_queue);
        if (log_slots) dispatch_release(log_slots);
        if (match) CFRelease(match);
        CFRelease(session);
        return 1;
    }
    signal(SIGPIPE, SIG_IGN);

    DARegisterDiskMountApprovalCallback(session, match, approve, NULL);
    if (match) CFRelease(match);
    DASessionScheduleWithRunLoop(session, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    CFRunLoopRun();
    return 0;
}
