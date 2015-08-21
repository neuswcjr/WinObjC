/*
 * Copyright (c) 2008, 2009, 2010, 2011, 2012
 *   Jonathan Schleifer <js@webkeks.org>
 *
 * All rights reserved.
 *
 * This file is part of ObjFW. It may be distributed under the terms of the
 * Q Public License 1.0, which can be found in the file LICENSE.QPL included in
 * the packaging of this file.
 *
 * Alternatively, it may be distributed under the terms of the GNU General
 * Public License, either version 2 or 3, which can be found in the file
 * LICENSE.GPLv2 or LICENSE.GPLv3 respectively included in the packaging of this
 * file.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
 
#include <assert.h>

#include "runtime.h"
#include "runtime-private.h"

#include "winrt-interop.h"

static struct objc_hashtable *classes = NULL;
static Class *load_queue = NULL;
static size_t load_queue_cnt = 0;
static struct objc_sparsearray *empty_dtable = NULL;

#ifndef IW_NO_WINRT_ISA
static struct objc_hashtable *isas = NULL;

struct winrt_isa *class_isa_for_class(Class cls)
{
    objc_global_mutex_lock();
    if (isas == NULL)
        isas = objc_hashtable_new(2);

    struct winrt_isa *ret = (struct winrt_isa *)objc_hashtable_get_ptr(isas, cls);
    if ( ret == NULL ) {
        ret = (struct winrt_isa *) malloc(sizeof(void *) * 8);

        //  Adjust pointer for realCls offset
        ret = (struct winrt_isa *) (((void **) ret) + 1);
        WINRT_ISA_REALCLS(ret) = cls;
        ret->IW_IInspectable[0] = (void *) _winrt_inspect_QueryInterface;
        ret->IW_IInspectable[1] = (void *) _winrt_inspect_AddRef;
        ret->IW_IInspectable[2] = (void *) _winrt_inspect_Release;
        ret->IW_IInspectable[3] = (void *) _winrt_inspect_GetIids;
        ret->IW_IInspectable[4] = (void *) _winrt_inspect_GetRuntimeClassName;
        ret->IW_IInspectable[5] = (void *) _winrt_inspect_GetTrustLevel;
        ret->IW_IInspectable[6] = (void *) _winrt_iwmsgsend_MsgSend;

        objc_hashtable_set_ptr(isas, cls, ret);
    }

    objc_global_mutex_unlock();

    return ret;
}
#endif

static void
register_class(struct objc_abi_class *cls)
{
    if (classes == NULL)
        classes = objc_hashtable_new(2);

    objc_hashtable_set(classes, cls->name, cls);

    if (empty_dtable == NULL)
        empty_dtable = objc_sparsearray_new();

    cls->dtable = empty_dtable;
#ifdef IW_NO_WINRT_ISA
    cls->metaclass = cls->metaclass;
    cls->metaclass->dtable = empty_dtable;
#else
    struct winrt_isa *isa = class_isa_for_class((Class) cls->metaclass);
    cls->metaclass = isa;
    WINRT_ISA_REALCLS(cls->metaclass)->dtable = empty_dtable;
#endif
}

BOOL
class_registerAlias_np(Class cls, const char *name)
{
    if (classes == NULL)
        return NO;

    objc_hashtable_set(classes, name, cls);

    return YES;
}

static void
register_selectors(struct objc_abi_class *cls)
{
    struct objc_abi_method_list *ml;
    unsigned int i;

    for (ml = cls->methodlist; ml != NULL; ml = ml->next)
        for (i = 0; i < ml->count; i++)
            objc_register_selector(
                (struct objc_abi_selector*)&ml->methods[i]);
}

Class
objc_classname_to_class(const char *name)
{
    Class c;

    if (classes == NULL)
        return Nil;

    objc_global_mutex_lock();
    c = (Class)objc_hashtable_get(classes, name);
    objc_global_mutex_unlock();

    return c;
}

static void
call_method(Class cls, const char *method)
{
    struct objc_method_list *ml;
    SEL selector;
    unsigned int i;

    selector = sel_registerName(method);

    for (ml = object_getClass((id) cls)->methodlist; ml != NULL; ml = ml->next)
        for (i = 0; i < ml->count; i++)
            if (sel_isEqual((SEL)&ml->methods[i].sel, selector))
                ((void(*)(id, SEL))ml->methods[i].imp)((id) cls,
                    selector);
}

static BOOL
has_load(Class cls)
{
    struct objc_method_list *ml;
    SEL selector;
    unsigned int i;

    selector = sel_registerName("load");

    for (ml = object_getClass((id) cls)->methodlist; ml != NULL; ml = ml->next)
        for (i = 0; i < ml->count; i++)
            if (sel_isEqual((SEL)&ml->methods[i].sel, selector))
                return YES;

    return NO;
}

static void
call_load(Class cls)
{
    if (cls->info & OBJC_CLASS_INFO_LOADED)
        return;

    if (cls->superclass != Nil)
        call_load(cls->superclass);

    call_method(cls, "load");

    cls->info |= OBJC_CLASS_INFO_LOADED;
}

void
objc_update_dtable(Class cls)
{
    struct objc_method_list *ml;
    struct objc_category **cats;
    unsigned int i;

    if (!(cls->info & OBJC_CLASS_INFO_DTABLE))
        return;

    if (cls->dtable == empty_dtable)
        cls->dtable = objc_sparsearray_new();

    if (cls->superclass != Nil)
        objc_sparsearray_copy(cls->dtable, cls->superclass->dtable);

    for (ml = cls->methodlist; ml != NULL; ml = ml->next)
        for (i = 0; i < ml->count; i++)
            objc_sparsearray_set(cls->dtable,
                (uint32_t)ml->methods[i].sel.uid,
                ml->methods[i].imp);

    if ((cats = objc_categories_for_class(cls)) != NULL) {
        for (i = 0; cats[i] != NULL; i++) {
            unsigned int j;

            ml = (cls->info & OBJC_CLASS_INFO_CLASS ?
                cats[i]->instance_methods : cats[i]->class_methods);

            for (; ml != NULL; ml = ml->next)
                for (j = 0; j < ml->count; j++)
                    objc_sparsearray_set(cls->dtable,
                        (uint32_t)ml->methods[j].sel.uid,
                        ml->methods[j].imp);
        }
    }

    if (cls->subclass_list != NULL)
        for (i = 0; cls->subclass_list[i] != NULL; i++)
            objc_update_dtable(cls->subclass_list[i]);
}

static void
add_subclass(Class cls)
{
    size_t i;

    if (cls->superclass->subclass_list == NULL) {
        if ((cls->superclass->subclass_list =
            malloc(2 * sizeof(Class))) == NULL)
            OBJC_ERROR("Not enough memory for subclass list of "
                "class %s!", cls->superclass->name);

        cls->superclass->subclass_list[0] = cls;
        cls->superclass->subclass_list[1] = Nil;

        return;
    }

    for (i = 0; cls->superclass->subclass_list[i] != Nil; i++);

    cls->superclass->subclass_list =
        realloc(cls->superclass->subclass_list, (i + 2) * sizeof(Class));

    if (cls->superclass->subclass_list == NULL)
        OBJC_ERROR("Not enough memory for subclass list of class %s\n",
            cls->superclass->name);

    cls->superclass->subclass_list[i] = cls;
    cls->superclass->subclass_list[i + 1] = Nil;
}

static void
setup_class(Class cls)
{
    const char *superclass;

    if (cls->info & OBJC_CLASS_INFO_SETUP)
        return;

    if ((superclass = ((struct objc_abi_class*)cls)->superclass) != NULL) {
        Class super = objc_classname_to_class(superclass);

        if (super == (Class) nil)
            return;

        setup_class(super);

        if (!(super->info & OBJC_CLASS_INFO_SETUP))
            return;

        cls->superclass = super;
        object_getClass((id) cls)->superclass = object_getClass((id) super);

        add_subclass(cls);
        add_subclass(object_getClass((id) cls));
    } else
        object_getClass((id) cls)->superclass = cls;

    //  Calculate size based on ivars
    int slide = 0;
    cls->instance_size = -(int) cls->instance_size;
    if ( cls->superclass ) {
        slide = cls->superclass->instance_size;
    }

    if ( slide ) {
        struct objc_ivar_list *list = (struct objc_ivar_list *) cls->ivars;
        if ( list ) {
            //  If first ivar has a negative offset (???) .. increase slide by that amount
            if ( list->count > 0 ) {
                if ( (int) list->ivars[0].offset < 0 ) {
                    printf("Warning: Class %s has first ivar %s with a negative offset of %d!\n", cls->name, list->ivars[0].name, list->ivars[0].offset);
                    slide += -(int) list->ivars[0].offset;
                }
            }

            for ( unsigned i = 0; i < list->count; i ++ ) {
                struct objc_ivar *curIvar = &list->ivars[i];
                curIvar->offset += slide;
                *((int **) cls->ivar_offsets)[i] += slide;
                assert(*((int **) cls->ivar_offsets)[i] == curIvar->offset);
            }
        }
    }
    cls->instance_size += slide;

    cls->info |= OBJC_CLASS_INFO_SETUP;
    object_getClass((id) cls)->info |= OBJC_CLASS_INFO_SETUP;
}

static void
initialize_class(Class cls)
{
    if (cls->info & OBJC_CLASS_INFO_INITIALIZED)
        return;

    if (cls->superclass)
        initialize_class(cls->superclass);

    cls->info |= OBJC_CLASS_INFO_DTABLE;
    object_getClass((id) cls)->info |= OBJC_CLASS_INFO_DTABLE;

    objc_update_dtable(cls);
    objc_update_dtable(object_getClass((id) cls));

    /*
     * Set it first to prevent calling it recursively due to message sends
     * in the initialize method
     */
    cls->info |= OBJC_CLASS_INFO_INITIALIZED;
    object_getClass((id) cls)->info |= OBJC_CLASS_INFO_INITIALIZED;

    call_method(cls, "initialize");
}

void
objc_initialize_class(Class cls)
{
    if (cls->info & OBJC_CLASS_INFO_INITIALIZED)
        return;

    objc_global_mutex_lock();

    /*
     * It's possible that two threads try to initialize a class at the same
     * time. Make sure that the thread which held the lock did not already
     * initialize it.
     */
    if (cls->info & OBJC_CLASS_INFO_INITIALIZED) {
        objc_global_mutex_unlock();
        return;
    }

    setup_class(cls);

    if (!(cls->info & OBJC_CLASS_INFO_SETUP)) {
        objc_global_mutex_unlock();
        return;
    }

    initialize_class(cls);

    objc_global_mutex_unlock();
}

void
objc_register_all_classes(struct objc_abi_symtab *symtab)
{
    uint_fast32_t i;

    for (i = 0; i < symtab->cls_def_cnt; i++) {
        struct objc_abi_class *cls =
            (struct objc_abi_class*)symtab->defs[i];

        register_class(cls);
        register_selectors(cls);
        register_selectors((struct objc_abi_class *) object_getClass((id) cls));
    }

    for (i = 0; i < symtab->cls_def_cnt; i++) {
        Class cls = (Class)symtab->defs[i];

        if (has_load(cls)) {
            setup_class(cls);

            if (cls->info & OBJC_CLASS_INFO_SETUP)
                call_load(cls);
            else {
                if (load_queue == NULL)
                    load_queue = malloc(sizeof(Class));
                else
                    load_queue = realloc(load_queue,
                        sizeof(Class) *
                        (load_queue_cnt + 1));

                if (load_queue == NULL)
                    OBJC_ERROR("Not enough memory for load "
                        "queue!");

                load_queue[load_queue_cnt++] = cls;
            }
        } else
            cls->info |= OBJC_CLASS_INFO_LOADED;
    }

    /* Process load queue */
    for (i = 0; i < load_queue_cnt; i++) {
        setup_class(load_queue[i]);

        if (load_queue[i]->info & OBJC_CLASS_INFO_SETUP) {
            call_load(load_queue[i]);

            load_queue_cnt--;

            if (load_queue_cnt == 0) {
                free(load_queue);
                load_queue = NULL;
                continue;
            }

            load_queue[i] = load_queue[load_queue_cnt];

            load_queue = realloc(load_queue,
                sizeof(Class) * load_queue_cnt);

            if (load_queue == NULL)
                OBJC_ERROR("Not enough memory for load queue!");
        }
    }
}

Class
objc_lookup_class(const char *name)
{
    Class cls = objc_classname_to_class(name);

    if (cls == NULL)
        return Nil;

    if (cls->info & OBJC_CLASS_INFO_SETUP)
        return cls;

    objc_global_mutex_lock();

    setup_class(cls);

    objc_global_mutex_unlock();

    if (!(cls->info & OBJC_CLASS_INFO_SETUP))
        return Nil;

    return cls;
}

id
objc_getClass(const char *name)
{
    Class cls;

    if ((cls = objc_lookup_class(name)) == Nil)
        OBJC_ERROR("Class %s not found!", name);

    return (id) cls;
}

const char*
class_getName(Class cls)
{
    return cls->name;
}

Class
class_getSuperclass(Class cls)
{
    objc_initialize_class(cls);
    return cls->superclass;
}

BOOL
class_isKindOfClass(Class cls1, Class cls2)
{
    objc_initialize_class(cls1);
    objc_initialize_class(cls2);

    Class iter;

    for (iter = cls1; iter != Nil; iter = iter->superclass)
        if (iter == cls2)
            return YES;

    return NO;
}

unsigned long
class_getInstanceSize(Class cls)
{
    objc_initialize_class(cls);

    return cls->instance_size;
}

Ivar
class_getInstanceVariable(Class cls, const char *name)
{
    objc_initialize_class(cls);

    struct objc_ivar_list *list = (struct objc_ivar_list *) cls->ivars;
    if ( list ) {
        for ( unsigned i = 0; i < list->count; i ++ ) {
            struct objc_ivar *curIvar = &list->ivars[i];

            if ( strcmp(curIvar->name, name) == 0 ) {
                return curIvar;
            }
        }
    }

    return NULL;
}

Method 
class_getInstanceMethod(Class cls, SEL name)
{
    objc_initialize_class(cls);

    while ( cls != NULL ) {
        struct objc_method_list *methodList = cls->methodlist;

        while ( methodList != NULL ) {
            for ( unsigned i = 0; i < methodList->count; i ++ ) {
                if ( &methodList->methods[i].sel == name ) {
                        return &methodList->methods[i];
                }
            }

            methodList = methodList->next;
        }

        cls = cls->superclass;
    }

    return NULL;
}

ptrdiff_t ivar_getOffset(Ivar ivar)
{
    return ivar->offset;
}

OBJCRT_EXPORT IMP
class_getMethodImplementation(Class cls, SEL sel)
{
    return objc_sparsearray_get(cls->dtable, (uint32_t)sel->uid);
}

OBJCRT_EXPORT BOOL object_isMethodFromClass(id dwObj, SEL pSel, const char *fromClass)
{
    Class pClass = object_getClass(dwObj);
    char *clsname = NULL;

    while ( pClass != NULL ) {
        clsname = (char *) pClass->name;

        struct objc_method_list *methodList = pClass->methodlist;

        while ( methodList != NULL ) {
            for ( unsigned i = 0; i < methodList->count; i ++ ) {
                if ( methodList->methods[i].sel.uid == pSel->uid ) {
                    if ( strcmp(clsname, fromClass) == 0 ) {
                        return TRUE;
                    } else {
                        return FALSE;
                    }
                }
            }

            methodList = methodList->next;
        }

        pClass = pClass->superclass;
    }

    return FALSE;
}

const char*
objc_get_type_encoding(Class cls, SEL sel)
{
    struct objc_method_list *ml;
    struct objc_category **cats;
    unsigned int i;

    objc_global_mutex_lock();

    for (ml = cls->methodlist; ml != NULL; ml = ml->next) {
        for (i = 0; i < ml->count; i++) {
            if (ml->methods[i].sel.uid == sel->uid) {
                const char *ret = ml->methods[i].sel.types;
                objc_global_mutex_unlock();
                return ret;
            }
        }
    }

    if ((cats = objc_categories_for_class(cls)) != NULL) {
        for (; *cats != NULL; cats++) {
            for (ml = (*cats)->instance_methods; ml != NULL;
                ml = ml->next) {
                for (i = 0; i < ml->count; i++) {
                    if (ml->methods[i].sel.uid ==
                        sel->uid) {
                        const char *ret =
                            ml->methods[i].sel.types;
                        objc_global_mutex_unlock();
                        return ret;
                    }
                }
            }
        }
    }

    objc_global_mutex_unlock();

    return NULL;
}

Method *class_copyMethodList(Class classRef, unsigned int *outCount) 
{
    struct objc_method_list *mi = classRef->methodlist;

    unsigned max = 0;
    while ( mi != NULL ) {
        max += mi->count;
        mi = mi->next;
    }

    //  [POTENTIAL BUG: This memory will be free'd by the caller, which may cause CRT difficulties.]
    Method *ret = (Method *) malloc(sizeof(Method) * max);

    objc_global_mutex_lock();
    unsigned  count = 0;
    mi = classRef->methodlist;
    while ( mi != NULL ) {
        for ( unsigned i = 0; i < mi->count; i ++ ) {
            assert(count < max);
            if ( count < max ) {
                ret[count ++] = &mi->methods[i];
            }
        }
        mi = mi->next;
    }

    objc_global_mutex_unlock();

    if ( outCount ) *outCount = count;

    return ret;
}

IMP
class_replaceMethod(Class cls, SEL sel, IMP newimp, const char *types)
{
    struct objc_method_list *ml;
    struct objc_category **cats;
    unsigned int i;
    IMP oldimp;

    objc_global_mutex_lock();

    for (ml = cls->methodlist; ml != NULL; ml = ml->next) {
        for (i = 0; i < ml->count; i++) {
            if (ml->methods[i].sel.uid == sel->uid) {
                oldimp = ml->methods[i].imp;

                ml->methods[i].imp = newimp;
                objc_update_dtable(cls);

                objc_global_mutex_unlock();

                return oldimp;
            }
        }
    }

    if ((cats = objc_categories_for_class(cls)) != NULL) {
        for (; *cats != NULL; cats++) {
            if (cls->info & OBJC_CLASS_INFO_METACLASS)
                ml = (*cats)->class_methods;
            else
                ml = (*cats)->instance_methods;

            for (; ml != NULL; ml = ml->next) {
                for (i = 0; i < ml->count; i++) {
                    if (ml->methods[i].sel.uid ==
                        sel->uid) {
                        oldimp = ml->methods[i].imp;

                        ml->methods[i].imp = newimp;
                        objc_update_dtable(cls);

                        objc_global_mutex_unlock();

                        return oldimp;
                    }
                }
            }
        }
    }

    /* FIXME: We need a way to free this at objc_exit() */
    if ((ml = malloc(sizeof(struct objc_method_list))) == NULL)
        OBJC_ERROR("Not enough memory to replace method!");

    ml->next = cls->methodlist;
    ml->count = 1;
    ml->methods[0].sel.uid = sel->uid;
    ml->methods[0].sel.types = types;
    ml->methods[0].imp = newimp;

    cls->methodlist = ml;

    objc_update_dtable(cls);

    objc_global_mutex_unlock();

    return (IMP)nil;
}

static void
free_class(Class rcls)
{
    struct objc_abi_class *cls = (struct objc_abi_class*)rcls;

    if (rcls->subclass_list != NULL) {
        free(rcls->subclass_list);
        rcls->subclass_list = NULL;
    }

    if (rcls->dtable != NULL && rcls->dtable != empty_dtable)
        objc_sparsearray_free(rcls->dtable);

    rcls->dtable = NULL;

    if (rcls->superclass != Nil)
        cls->superclass = rcls->superclass->name;
}

void
objc_free_all_classes(void)
{
    uint32_t i;

    if (classes == NULL)
        return;

    for (i = 0; i <= classes->last_idx; i++) {
        if (classes->data[i] != NULL) {
            free_class((Class)classes->data[i]->obj);
            free_class(object_getClass((id) classes->data[i]->obj));
        }
    }

    objc_hashtable_free(classes);
    classes = NULL;
}

Class
object_getClass(id obj_)
{
    struct objc_object *obj = (struct objc_object*)obj_;

    /* [HACK: blamb] 
     * 
     * Global block are emitted by the compiler as const structs, which can't directly
     * address variables imported from DLLs.  The isa member of the emitted block
     * will point to the DLL import table, which is patched with the actual address
     * of the imported symbol.  The loader (afaik) can't/won't patch
     * the isa member when it gets resolved at load time.
     *
     * To get around this, we check to see if the isa of the Class of the object
     * points to _NSConcreteGlobalBlock.  If it does, we'll do the extra deref here 
     * to get the object of the class. 
     */

#ifdef IW_NO_WINRT_ISA
    extern void *_NSConcreteGlobalBlock;

    if ( obj->isa->isa == (Class) &_NSConcreteGlobalBlock ) {
        reutrn (Class) &_NSConcreteGlobalBlock;
    }
    return obj->isa;
#else
    if ( obj->isa ) {
        extern struct winrt_isa _NSConcreteGlobalBlock;

        if ( ((Class) obj->isa)->isa == &_NSConcreteGlobalBlock ) {
            return (Class) WINRT_ISA_REALCLS(&_NSConcreteGlobalBlock);
        }
        return WINRT_ISA_REALCLS(obj->isa);
    } else {
        return nil;
    }
#endif
}

//  Sets an object's class without reading back the previous isa - needed for
//  objects which do not have a valid isa set
void _object_setClass(id obj_, Class cls)
{
    struct objc_object *obj = (struct objc_object*)obj_;

#ifdef IW_NO_WINRT_ISA
    old = obj->isa;
    obj->isa = cls;
#else
    obj->isa = class_isa_for_class(cls);
#endif
}

Class
object_setClass(id obj_, Class cls)
{
    struct objc_object *obj = (struct objc_object*)obj_;
    Class old = nil;

#ifdef IW_NO_WINRT_ISA
    old = obj->isa;
    obj->isa = cls;
#else
    if ( obj->isa ) {
        old = WINRT_ISA_REALCLS(obj->isa);
    }

    obj->isa = class_isa_for_class(cls);
#endif

    return old;
}

const char*
object_getClassName(id obj)
{
    return class_getName(object_getClass(obj));
}

BOOL
class_isMetaClass(Class cls_)
{
    struct objc_class *cls = (struct objc_class*)cls_;

    return (cls->info & OBJC_CLASS_INFO_METACLASS);
}

SEL method_getName(Method m)
{
    return &m->sel;
}

void method_exchangeImplementations(Method m1, Method m2)
{
    objc_global_mutex_lock();
    IMP tmp = m1->imp;
    m1->imp = m2->imp;
    m2->imp = tmp;
    objc_global_mutex_unlock();
}

char *method_copyReturnType(Method m)
{
    const char *curArg = m->sel.types;

    const char *typeStart = curArg;

    switch ( *typeStart ) {
        case '{': {
            int curCount = 0;
            while ( *curArg ) {
                if ( *curArg == '{' ) {
                    curCount ++;
                }
                if ( *curArg == '}' ) {
                    curCount --;
                    if ( curCount == 0 ) break;
                }
                curArg ++;
            }
            while ( *curArg && !isdigit(*curArg) ) curArg ++;
        }
            break;

        default: {
            int curCount = 0;
            while ( *curArg ) {
                if ( *curArg == '{' ) {
                    curCount ++;
                }
                if ( *curArg == '}' ) {
                    curCount --;
                    assert(curCount >= 0);
                }
                if ( isdigit(*curArg) && curCount == 0 ) break;
                curArg ++;
            }
        }
            break;
    }

    const char *typeEnd = curArg;

    int typeLength = typeEnd - typeStart;

    while ( *curArg && isdigit(*curArg) ) curArg ++;

    const char *argOffsetEnd = curArg;

    int offsetLength = argOffsetEnd - typeEnd;

    assert(typeLength > 0);
    assert(offsetLength > 0);

    if ( typeLength > 0 ) {
        char *ret = (char *) calloc(typeLength + 1, 1);
        memcpy(ret, typeStart, typeLength);
        ret[typeLength] = 0;
        return ret;
    } else {
        return NULL;
    }
}

unsigned int method_getNumberOfArguments(Method m)
{
    const char *curArg = m->sel.types;
    unsigned int argCount = 0;
    BOOL returnTypeFound = FALSE;

    while ( *curArg ) {
        const char *typeStart = curArg;

        switch ( *typeStart ) {
            case '{': {
                int curCount = 0;
                while ( *curArg ) {
                    if ( *curArg == '{' ) {
                        curCount ++;
                    }
                    if ( *curArg == '}' ) {
                        curCount --;
                        if ( curCount == 0 ) break;
                    }
                    curArg ++;
                }
                while ( *curArg && !isdigit(*curArg) ) curArg ++;
            }
                break;

            default: {
                int curCount = 0;
                while ( *curArg ) {
                    if ( *curArg == '{' ) {
                        curCount ++;
                    }
                    if ( *curArg == '}' ) {
                        curCount --;
                        assert(curCount >= 0);
                    }
                    if ( isdigit(*curArg) && curCount == 0 ) break;
                    curArg ++;
                }
            }
                break;
        }

        const char *typeEnd = curArg;

        int typeLength = typeEnd - typeStart;

        while ( *curArg && isdigit(*curArg) ) curArg ++;

        const char *argOffsetEnd = curArg;

        int offsetLength = argOffsetEnd - typeEnd;

        assert(typeLength > 0);
        assert(offsetLength > 0);

        if ( !returnTypeFound ) {
            returnTypeFound = TRUE;
        } else {
            argCount ++;
        }
    }

    return argCount;
}

int objc_getClassList(Class *classesRet, int maxCount)
{
    if (classes == NULL) {
        return 0;
    }

    objc_global_mutex_lock();
    int ret = 0;

    uint32_t i;

    for (i = 0; i <= classes->last_idx; i++) {
        if (classes->data[i] != NULL) {
            if ( classesRet && maxCount > 0 ) {
                *classesRet = (Class) classes->data[i]->obj;
                classesRet ++;
                maxCount --;
            }

            ret ++;
        }
    }

    objc_global_mutex_unlock();

    return ret;
}

OBJCRT_EXPORT void
objc_enumerationMutation(id object)
{
    //enumeration_mutation_handler(object);
    assert(0);
}

OBJCRT_EXPORT id objc_get_class(const char *cls)
{
    return (id) objc_lookup_class(cls);
}


