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

#ifndef __OBJC_OBJC_H
#define __OBJC_OBJC_H

#include <sys/cdefs.h>
#include <sys/types.h>
#include <stddef.h>

#define nil     NULL
#ifndef Nil
 #define Nil    nil
#endif

#define NO      (BOOL)0
#define YES     (BOOL)1

#if !__has_feature(objc_arc)
#define __unsafe_unretained
#define __strong
#endif

/* BOOL shall be int, it has been said */
typedef int BOOL;
typedef id (*IMP)(id, SEL, ...);
typedef const char* SEL;

struct objc_method_description {
    SEL name;
    char *types;
};

typedef struct objc_class *Class;
typedef struct objc_method *Method;

#ifndef OBJCRT
#ifndef IW_NO_WINRT_ISA
struct winrt_isa {
    //  struct objc_class *realCls;
    //  The ObjC isa points to IW_IInspectable - the realCls member lives immediately BEHIND IW_Inspectable
    void *inspectable[6];
};

typedef struct winrt_isa *WinRT_isa;
#endif

struct objc_class {
#ifdef IW_NO_WINRT_ISA
    struct winrt_isa *isa;
#else
    Class isa;
#endif
    Class superclass;
    const char *name;
    unsigned long version;
    unsigned long info;
    unsigned long instance_size;
    struct objc_ivar_list *ivars;
    struct objc_method_list *methodlist;
    struct objc_sparsearray *dtable;
    Class *subclass_list;
    void *sibling_class;
    struct objc_protocol_list *protocols;
    void *gc_object_type;
    unsigned long abi_version;
    void *ivar_offsets;
    struct objc_property_list *properties;
};
#endif


#ifdef __OBJC__
    @class Protocol;
#else
    typedef struct objc_object Protocol;
#endif

typedef struct objc_ivar *Ivar;

__BEGIN_DECLS

struct objc_method_description *protocol_copyMethodDescriptionList(Protocol*, BOOL, BOOL, unsigned int*);

BOOL protocol_conformsToProtocol(Protocol*, Protocol*);

Protocol* __unsafe_unretained * protocol_copyProtocolList(Protocol*, unsigned int*);

#pragma clang diagnostic ignored "-Wincompatible-library-redeclaration"
id objc_getClass(const char *name);
Class objc_lookup_class(const char*);
unsigned long class_getInstanceSize(Class);
Class object_setClass(id obj_, Class cls);
Class object_getClass(id obj_);
Method *class_copyMethodList(Class classRef, unsigned int *outCount);
const char* class_getName(Class cls);
Class class_getSuperclass(Class);
BOOL class_conformsToProtocol(Class, Protocol*);

SEL method_getName(Method m);
char *method_copyReturnType(Method m);
unsigned int method_getNumberOfArguments(Method m);

const char* object_getClassName(id obj);
Ivar class_getInstanceVariable(Class cls, const char *name);
ptrdiff_t ivar_getOffset(Ivar ivar);
const char* sel_getName(SEL);
SEL sel_registerName(const char*);

int objc_getClassList(Class *classes, int maxCount);

id objc_storeWeak(id *addr, id obj);
id objc_loadWeak(id *object);

BOOL objc_isRetained(id obj);
size_t objc_getRetainCount(id obj);
id objc_retain_ref(id obj);
void objc_release_ref(id obj);

id objc_allocateObject(Class classRef, unsigned int extraBytes);
void objc_deallocateObject(id obj);

__END_DECLS

#endif
