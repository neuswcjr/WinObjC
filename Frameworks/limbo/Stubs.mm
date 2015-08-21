//******************************************************************************
//
// Copyright (c) 2015 Microsoft Corporation. All rights reserved.
//
// This code is licensed under the MIT License (MIT).
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
//******************************************************************************

#include "Starboard.h"

#include "CGImageInternal.h"
#include "CGContextInternal.h"
#include "AudioToolbox/AudioToolbox.h"
#include "AVFoundation/AVAudioPlayer.h"
#include "Social/SLServiceTypes.h"
#include "objc/runtime.h"
#include <CommonCrypto/CommonDigest.h>
#include <inttypes.h>
#include <mach/mach.h>
#include <dispatch/dispatch.h>

typedef unsigned int mach_port_t;

float _UINavigationControllerHideShowBarDuration = .25f;
EbrEvent _applicationStateChanged;
int g_browsersVisible = 0;
UIDeviceOrientation newDeviceOrientation = UIDeviceOrientationUnknown;
const float UIScrollViewDecelerationRateFast = 1.0f;

// Strings:
#define REGISTER_STRING(name)  UIKIT_EXPORT NSString* const name = @#name;

REGISTER_STRING(NSUnderlyingErrorKey)
REGISTER_STRING(NSLocalizedDescriptionKey)

void EbrSetKeyboardType(int)
{
}

void EbrPlatformShowKeyboard(void)
{
}

void EbrPlatformHideKeyboard(void)
{
}

void EbrSetApplicationBadgeNumber(int)
{
}

void EbrOpenURL(const char *url)
{
}

void EbrPauseSound(void)
{
}

void EbrResumeSound(void)
{
}

@implementation UIKeyboardRotationView : UIView
    -(UIView *) hitTest: (CGPoint) point withEvent: (UIEvent *) event
    {
        UIView *ret = [super hitTest: point withEvent: event];

        if ( ret == self ) return nil;

        return ret;
    }
@end

@implementation _TransitionNotifier
@end

@implementation __UIGroupEdgeView
@end

@implementation _UISettings
@end

@implementation NSLayoutConstraint
@end

@implementation NSUndoManager
@end

@implementation NSAppearanceSetter
@end

@implementation UIAppearanceSetter
+(void) _applyAppearance: (UIView *) view
{
}
+(void) _applyAppearance: (UIView *) view withAppearanceClass:(Class) cls withBaseView:(UIView *) baseView
{
}
@end

@implementation _UILoading
+(void) hideLoadingScreen
{
}
@end

#undef dispatch_once
/*
void dispatch_once(dispatch_once_t *predicate, dispatch_block_t block)
{
    if ( *predicate == 0 ) {
        *predicate = 1;
        block();
    }
}
*/

bool isSupportedControllerOrientation(id controller, UIInterfaceOrientation orientation)
{
    return false;
}

#import <UIKit/UIStoryboardPushSegueTemplate.h>

@implementation UIPageControl
@end

@implementation UIStepper
@end

@implementation UIAccelerometer : NSObject
    +(id) sharedAccelerometer { return nil; }
@end

@implementation GKAchievement
+(id) alloc
{
    return nil;
}
@end

@implementation UITextRange
@end

@implementation GKAchievementViewController
+(id) alloc
{
    return nil;
}
@end

@implementation GKLeaderboardViewController
+(id) alloc
{
    return nil;
}
@end

@implementation GKLocalPlayer
+(GKLocalPlayer *) localPlayer
{
    return nil;
}
@end

@implementation GKScore
+(id) alloc
{
    return nil;
}
@end

extern "C" NSString * const UIDeviceOrientationDidChangeNotification = (NSString * const) @"UIDeviceOrientationDidChangeNotification";

__declspec(dllexport)
extern "C" unsigned random()
{
    return rand();
}

__declspec(dllexport)
extern "C" int gettimeofday(struct timeval *tv, void *restrict) {
	EbrTimeval curtime;
    EbrGetTimeOfDay(&curtime);
	tv->tv_sec = curtime.tv_sec;
	tv->tv_usec = curtime.tv_usec;
    return 0;
}

__declspec(dllexport)
extern "C" void srandom(unsigned val)
{
    return srand(val);
}

NSData *UIImagePNGRepresentation(UIImage *img)
{
    return [NSData data];
}

NSData *UIImageJPEGRepresentation(UIImage *img, CGFloat quality)
{
    return [NSData data];
}

DEFINE_FUNCTION_STRET_1(CGPoint, CGPointFromString, idt(NSString), strPt)
{
    CGPoint ret;

    char *str = (char *) [strPt UTF8String];
    sscanf(str, "{%f, %f}", &ret.x, &ret.y);
    return ret;
}

DEFINE_FUNCTION_STRET_1(CGSize, CGSizeFromString, idt(NSString), strSize)
{
    CGSize ret;

    char *str = (char *) [strSize UTF8String];
    sscanf(str, "{%f, %f}", &ret.width, &ret.height);
    return ret;
}

DEFINE_FUNCTION_STRET_1(CGRect, CGRectFromString, idt(NSString), strRect)
{
    CGRect ret;

    char *str = (char *) [strRect UTF8String];
    sscanf(str, "{{%f, %f}, {%f, %f}}", &ret.origin.x, &ret.origin.y, &ret.size.width, &ret.size.height);
    return ret;
}

__declspec(dllexport) extern "C" uint64_t mach_absolute_time()
{
    static uint32_t start;

    if (start == 0) {
        start = EbrGetAbsoluteTime();
    }
    uint32_t ret = EbrGetAbsoluteTime();
    ret -= start;

    return ret;
}

void EbrRefreshKeyboard(void) {}
void EbrShowKeyboard(void) {}
void EbrHideKeyboard(void) {}

NSString *const SLServiceTypeTwitter = @"SLServiceTypeTwitter";
NSString *const SLServiceTypeFacebook = @"SLServiceTypeFacebook";

@implementation SLComposeViewController
@end

__declspec(dllexport)
extern "C" mach_port_t mach_host_self(void)
{
    return (mach_port_t) 0xBAADF00D;
}

__declspec(dllexport)
extern "C" int host_page_size(mach_port_t port, vm_size_t *sizeOut)
{
    return 65536;
}
int vm_page_size = 65536;

__declspec(dllexport)
extern "C"  int host_statistics(mach_port_t port, int type, host_info_t dataOut, mach_msg_type_number_t *dataOutSize)
{
    assert(type == HOST_VM_INFO);
    assert(*dataOutSize >= sizeof(vm_statistics));
    *dataOutSize = sizeof(vm_statistics);

    vm_statistics *ret = (vm_statistics *) dataOut;
    memset(ret, 0, sizeof(vm_statistics));

    ret->free_count = 512 * 1024 * 1024 / 65536;
    return 0;
}

__declspec(dllexport)
extern "C" const char *strnstr(const char *a, const char *b, int len)
{
    assert(0);
    return NULL;
}

__declspec(dllexport)
int CC_MD5_Init(CC_MD5_CTX *ctx)
{
    assert(0);
    return 0;
}

__declspec(dllexport)
int CC_MD5_Update(CC_MD5_CTX *ctx, const void *data, unsigned int len)
{
    assert(0);
    return 0;
}

__declspec(dllexport)
int CC_MD5_Final(unsigned char *out, CC_MD5_CTX *ctx)
{
    assert(0);
    return 0;
}

EbrPlatformInfo *EbrGetDeviceInfo()
{
    static EbrPlatformInfo info;
    static bool infoInited = false;

    if (!infoInited) {
        infoInited = true;

        info.platformName = "Windows 10";
        info.manufacturer = "Microsoft";
        info.model = "Desktop PC";
        info.deviceProductName = "Computer";
        info.serialNo = "12345";
        info.carrier = "Versalife, Inc";
        info.osVersion = "10.0";
        info.osVersionInt = 10;
        info.deviceResX = 788;
        info.deviceResY = 866;
        info.devicePhysWidth = 2.7f;
        info.devicePhysHeight = 5.f;
    }

    return &info;
}

extern "C" {
    #include "md5.h"

    __declspec(dllexport)
    unsigned char *CC_MD5(const void *data, long len, unsigned char *md)
    {
        MD5_CTX ctx;

        MD5Init(&ctx);
        MD5Update(&ctx, (unsigned char *) data, len);
        MD5Final(&ctx);

        memcpy(md, ctx.digest, 16);
        return md;
    }
}

extern "C" void UIImageWriteToSavedPhotosAlbum( UIImage *image, id completionTarget, SEL completionSelector, void *contextInfo )
{
}

@implementation CBCentralManager
@end

@implementation GCDAsyncSocket
@end

typedef void* DNSServiceRef;
typedef unsigned DNSServiceErrorType;
typedef unsigned DNSServiceFlags;
typedef unsigned DNSServiceProtocol;
typedef void (*DNSServiceBrowseReply) (DNSServiceRef sdRef, DNSServiceFlags flags, unsigned interfaceIndex, DNSServiceErrorType errorCode, const char *serviceName, const char *regtype, const char *replyDomain, void *context);
typedef void (*DNSServiceResolveReply) (DNSServiceRef sdRef, DNSServiceFlags flags, uint32_t interfaceIndex, DNSServiceErrorType errorCode, const char *fullname, const char *hosttarget, uint16_t port, /* In network byte order */ uint16_t txtLen, const unsigned char *txtRecord, void *context);
typedef void (*DNSServiceGetAddrInfoReply) (DNSServiceRef sdRef, DNSServiceFlags flags, uint32_t interfaceIndex, DNSServiceErrorType errorCode, const char *hostname, const struct sockaddr *address, uint32_t ttl, void *context);
typedef void (*DNSServiceRegisterReply) (DNSServiceRef sdRef, DNSServiceFlags flags, DNSServiceErrorType errorCode, const char *name, const char *regtype, const char *domain, void *context);

UIKIT_EXPORT
extern "C" void DNSServiceRefDeallocate( DNSServiceRef sdRef )
{
    [sdRef dealloc];
}

UIKIT_EXPORT
extern "C" DNSServiceErrorType DNSServiceSetDispatchQueue( DNSServiceRef service, dispatch_queue_t queue )
{
    assert(!"DNSServiceSetDispatchQueue");
    return 0;
}

UIKIT_EXPORT
extern "C"
DNSServiceErrorType DNSServiceBrowse( DNSServiceRef *sdRef, DNSServiceFlags flags, uint32_t interfaceIndex, const char *regtype, const char *domain, /* may be NULL */ DNSServiceBrowseReply callBack, void *context /* may be NULL */)
{
    assert(!"DNSServiceBrowse");
    return 0;
}

UIKIT_EXPORT
extern "C"
DNSServiceErrorType DNSServiceGetAddrInfo( DNSServiceRef *sdRef, DNSServiceFlags flags, uint32_t interfaceIndex, DNSServiceProtocol protocol, const char *hostname, DNSServiceGetAddrInfoReply callBack, void *context /* may be NULL */)
{
    assert(!"DNSServiceGetAddrInfo");
    return 0;
}

UIKIT_EXPORT
extern "C"
DNSServiceErrorType DNSServiceResolve(DNSServiceRef *sdRef, DNSServiceFlags flags, uint32_t interfaceIndex, const char *name, const char *regtype, const char *domain, DNSServiceResolveReply callBack, void *context /* may be NULL */)
{
    assert(!"DNSServiceResolve");
}

@interface NSFont : NSObject
@end

@implementation NSFont
@end

@interface UIFontDescriptor : NSObject
@end

@implementation UIFontDescriptor
@end

#include <Windows.h>
#include <inaddr.h>

UIKIT_EXPORT extern "C"
char *__inet_ntoa(struct in_addr addr)
{
    assert(!"__inet_ntoa");
    return 0;
}

UIKIT_EXPORT extern "C"
char *if_indextoname(unsigned int ifindex, char *ifname)
{
    assert(!"if_indextoname");
    return 0;
}

UIKIT_EXPORT extern "C"
DNSServiceErrorType DNSServiceRegister(DNSServiceRef *sdRef, DNSServiceFlags flags, uint32_t interfaceIndex, const char *name, /* may be NULL */ const char *regtype, const char *domain, /* may be NULL */ const char *host, /* may be NULL */ uint16_t port, /* In network byte order */ uint16_t txtLen, const void *txtRecord, /* may be NULL */ DNSServiceRegisterReply callBack, /* may be NULL */ void *context /* may be NULL */)
{
    assert(!"DNSServiceRegister");
    return 0;
}

@implementation NSPredicate
@end

