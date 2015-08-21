/*
Original Author: Michael Ash on 11/9/08
Copyright (c) 2008 Rogue Amoeba Software LLC

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

#include "Starboard.h"
#include "Foundation/NSOperation.h"
#include "Foundation/NSMutableArray.h"
#include "Platform/EbrPlatform.h"
#include "Foundation/NSString.h"
#include "Foundation/NSOperationQueue.h"
#include "Foundation/NSThread.h"
#include "Foundation/NSAutoreleasePool.h"
#include "Foundation/NSLock.h"

#include <time.h>

typedef void *gpointer;

struct NSAtomicListNode
{
    struct NSAtomicListNode *next;
    void *elt;
};
typedef struct NSAtomicListNode *NSAtomicListRef;

struct NSOperationQueuePriv
{
    NSAtomicListRef myQueues[NSOperationQueuePriority_Count];

    id _thread;
    EbrLock _threadRunningLock;
    
    DWORD _maxConcurrentOperationCount;
    id workAvailable;
    id suspendedCondition;
    id allWorkDone;
    id curOperation;
    BOOL isSuspended;
    
    void *queues[NSOperationQueuePriority_Count];
    id _name;

    NSOperationQueuePriv()
    {
        memset(this, 0, sizeof(NSOperationQueuePriv));
    }
};

extern pthread_key_t g_currentDispatchQueue;

static inline gpointer CompareExchangePointer(volatile gpointer *dest, gpointer exch, gpointer comp)
{
    return (gpointer) EbrCompareExchange((volatile int *) dest, (long) exch, (long) comp);
}

NSAtomicListRef NSAtomicListSteal( NSAtomicListRef *listPtr )
{
    NSAtomicListRef ret;
    do {
        ret = *listPtr;
    } while( CompareExchangePointer( (void **)listPtr, NULL, ret ) != ret );
    return ret;
}

void NSAtomicListReverse( NSAtomicListRef *listPtr )
{
    struct NSAtomicListNode *cur = *listPtr;
    struct NSAtomicListNode *prev = NULL;
    struct NSAtomicListNode *next = NULL;
    
    if( !cur )
        return;
    
    do {
        next = cur->next;
        cur->next = prev;
        
        if( next )
        {
            prev = cur;
            cur = next;
        }
    } while( next );
    
    *listPtr = cur;
}

void *NSAtomicListPop( NSAtomicListRef *listPtr)
{
    struct NSAtomicListNode *node = *listPtr;
    if( !node )
        return NULL;
    
    *listPtr = node->next;
    
    void *elt = node->elt;
    EbrFree( node );
    return elt;
}

void *NSAtomicListPeek( NSAtomicListRef *listPtr)
{
    struct NSAtomicListNode *node = *listPtr;
    if( !node )
        return NULL;
    
    void *elt = node->elt;

    return elt;
}

void NSAtomicListInsert( NSAtomicListRef *listPtr, void *elt )
{
    struct NSAtomicListNode *node = (struct NSAtomicListNode *) EbrMalloc( sizeof( *node ) );
    node->elt = elt;
    
    do {
        node->next = *listPtr;
    } while( CompareExchangePointer( (void **)listPtr, node, node->next ) != node->next );
}

static id _mainQueue;

@implementation NSOperationQueue : NSObject
    static id PopOperation( NSAtomicListRef *listPtr )
    {
        return [(id)(DWORD)NSAtomicListPop( listPtr ) autorelease];
    }
            
    static id PeekOperation( NSAtomicListRef *listPtr )
    {
        return (id) (NSAtomicListPeek( listPtr ));
    }
            
    static void ClearList( NSAtomicListRef *listPtr )
    {
        for (int i = 0; i < NSOperationQueuePriority_Count; i++) {
            while (PopOperation( &listPtr[i] )) ;   
        }
    }

    static BOOL RunOperationFromLists( NSAtomicListRef *listPtr, NSAtomicListRef *sourceListPtr, id *curOperation )
    {
        id op = PopOperation( listPtr );
        if( op == nil ) {
            *listPtr = NSAtomicListSteal( sourceListPtr );
            // source lists are in LIFO order, but we want to execute operations in the order they were enqueued
            // so we reverse the list before we do anything with it
            NSAtomicListReverse( listPtr );
            op = PopOperation( listPtr );
        }
        
        if ( op != nil ) {
            if ([op isReady]) {
                *curOperation = op;
                [op start];
                *curOperation = nil;
            } else {
                NSAtomicListInsert(sourceListPtr, (void *) [op retain]);
                return FALSE;
            }
        }
        
        return op != nil;
    }

    /* annotate with type */ -(id) _workThread {
        BOOL stop = FALSE;

        while ( !stop ) {
            [self _doMainWork];
            EbrLockEnter(priv->_threadRunningLock);
            if ( ![self hasMoreWork] ) {
                stop = TRUE;
                priv->_thread = nil;
            }
            EbrLockLeave(priv->_threadRunningLock);
        }
        [priv->allWorkDone signal];

        return self;
    }

    /* annotate with type */ -(id) _doMainWork {
        memset( priv->myQueues, 0, sizeof( priv->myQueues ) );
        
        BOOL didWork;

        id innerPool = [[NSAutoreleasePool alloc] init];
        
        do {
            didWork = FALSE;

            for (int i = 0; i < NSOperationQueuePriority_Count; i++) {
                if ( RunOperationFromLists( &priv->myQueues[i], ( NSAtomicListRef *)(&priv->queues[i] ), &priv->curOperation) ) {
                    didWork = TRUE;
                }
            }
        } while ( didWork );

        [innerPool release];
                
        return self;
    }

    -(BOOL) hasMoreWork {
        if ( priv->curOperation != nil ) return TRUE;

        for ( int i = 0; i < NSOperationQueuePriority_Count; i++) {
            if (priv->queues[i] != NULL ) {
                return TRUE;
            }
        }
     
        return FALSE;
    }

    /* annotate with type */ -(id) init {
        priv = new NSOperationQueuePriv();

        priv->_maxConcurrentOperationCount = 1;
        priv->workAvailable = [[NSCondition alloc] init];
        priv->suspendedCondition = [[NSCondition alloc] init];
        priv->allWorkDone = [[NSCondition alloc] init];
        priv->isSuspended = 0;
        EbrLockInit(&priv->_threadRunningLock);
                
        return self;
    }

    /* annotate with type */ -(id) _initMainThread {
        priv = new NSOperationQueuePriv();

        priv->_maxConcurrentOperationCount = 1;
        priv->workAvailable = [[NSCondition alloc] init];
        priv->suspendedCondition = [[NSCondition alloc] init];
        priv->allWorkDone = [[NSCondition alloc] init];
        priv->isSuspended = 0;
        EbrLockInit(&priv->_threadRunningLock);

        return self;
    }

    /* annotate with type */ -(void) addOperation:(id)op {
        /*
        //  Add any dependencies
        id dependencies = [op dependencies];
        int count = [dependencies count];
        for ( int i = 0; i < count; i ++ ) {
            id curDep = [dependencies objectAtIndex:i];
            [self addOperation:curDep];
        }
        */

        unsigned priority = 1;
        if ( [op queuePriority] < NSOperationQueuePriorityNormal ) {
            priority = 2;
        } else if ( [op queuePriority] > NSOperationQueuePriorityNormal ) {
            priority = 0;
        }
        
        NSAtomicListInsert( (NSAtomicListRef *)(&priv->queues[priority]), (void *) [op retain] );
        [priv->workAvailable signal];

        EbrLockEnter(priv->_threadRunningLock);
        if ( priv->_thread == nil ) {
            priv->_thread = [[NSThread alloc] initWithTarget:self selector:@selector(_workThread) object:nil];
            [priv->_thread start];
            [priv->_thread release];
        }
        EbrLockLeave(priv->_threadRunningLock);
    }

    /* annotate with type */ -(void) addOperationWithBlock:(void(^)())block {
        id op = [NSOperation new];
        [op setCompletionBlock:block];
        [self addOperation:op];
        [op release];
    }

    /* annotate with type */ -(void) addOperations:(id)operations waitUntilFinished:(BOOL)wait {
        for (id curOp in operations) {
            [self addOperation:curOp];
        }

        if ( wait ) {
            for (id curOp in operations) {
                [curOp waitUntilFinished];
            }
        }
    }

    -(void) setMaxConcurrentOperationCount:(NSInteger)count {
        priv->_maxConcurrentOperationCount = count;
    }

    -(NSInteger) maxConcurrentOperationCount {
        return priv->_maxConcurrentOperationCount;
    }

    /* annotate with type */ -(id) operations {
        id ret = [NSMutableArray array];

        EbrDebugLog("Should lock queue for this\n");
        id cur = priv->curOperation;
        if ( cur != nil ) {
            [ret addObject:cur];
        }

        for ( int i = 0; i < NSOperationQueuePriority_Count; i++) {
            if (priv->queues[i] != NULL ) {
                NSAtomicListNode *curNode = (NSAtomicListNode *) priv->queues[i];

                while ( curNode != NULL ) {
                    id node = (id) (DWORD) curNode->elt;
                    [ret addObject:node];
                    curNode = curNode->next;
                }
            }
            if (priv->myQueues[i] != NULL ) {
                NSAtomicListNode *curNode = (NSAtomicListNode *) priv->myQueues[i];

                while ( curNode != NULL ) {
                    id node = (id) (DWORD) curNode->elt;
                    [ret addObject:node];
                    curNode = curNode->next;
                }
            }
        }

        return ret;
    }

    -(unsigned) operationCount {
        DWORD ret = 0;

        id cur = priv->curOperation;
        if ( cur != nil ) {
            ret ++;
        }
        for ( int i = 0; i < NSOperationQueuePriority_Count; i++) {
            if (priv->queues[i] != NULL ) {
                NSAtomicListNode *curNode = (NSAtomicListNode *) priv->queues[i];

                while ( curNode != NULL ) {
                    ret ++;
                    curNode = curNode->next;
                }
            }
            if (priv->myQueues[i] != NULL ) {
                NSAtomicListNode *curNode = (NSAtomicListNode *) priv->myQueues[i];

                while ( curNode != NULL ) {
                    ret ++;
                    curNode = curNode->next;
                }
            }
        }

        return ret;
    }

    /* annotate with type */ -(void) cancelAllOperations {
        EbrDebugLog("Should lock queue for this\n");

        id cur = priv->curOperation;
        if ( cur != nil ) {
            [cur cancel];
        }

        for ( int i = 0; i < NSOperationQueuePriority_Count; i++) {
            if (priv->queues[i] != NULL ) {
                NSAtomicListNode *curNode = (NSAtomicListNode *) priv->queues[i];

                while ( curNode != NULL ) {
                    id node = (id) (DWORD) curNode->elt;
                    [node cancel];
                    curNode = curNode->next;
                }
            }
            if (priv->myQueues[i] != NULL ) {
                NSAtomicListNode *curNode = (NSAtomicListNode *) priv->myQueues[i];

                while ( curNode != NULL ) {
                    id node = (id) (DWORD) curNode->elt;
                    [node cancel];
                    curNode = curNode->next;
                }
            }
        }
    }

    /* annotate with type */ -(void) waitUntilAllOperationsAreFinished {
       BOOL isWorking;

       [priv->workAvailable lock];
       isWorking = [self hasMoreWork];

        if ( isWorking ) {
            [priv->allWorkDone wait];
            isWorking = [self hasMoreWork];
        }
        [priv->workAvailable unlock];
    }

    /* annotate with type */ -(void) setSuspended:(BOOL)suspend {
        if  ( suspend ) {
            [self suspend];
        } else {
            [self resume];
        }
    }

    -(BOOL) isSuspended {
       [priv->suspendedCondition lock];
        int ret = priv->isSuspended;
        [priv->suspendedCondition unlock];

        return ret;
    }

    /* annotate with type */ -(id) resume {
       [priv->suspendedCondition lock];
       if ( priv->isSuspended ) {
            priv->isSuspended = FALSE;
            [priv->suspendedCondition broadcast];
        }
        [priv->suspendedCondition unlock];

        return self;
    }

    /* annotate with type */ -(id) suspend {
        [priv->suspendedCondition lock];
        priv->isSuspended = TRUE;
        [priv->suspendedCondition unlock];

        return self;
    }

    /* annotate with type */ -(void) setName:(id)name {
        id oldName = priv->_name;
        priv->_name = [name copy];
        [oldName release];
    }

    /* annotate with type */ -(id) name {
        if ( priv->_name == nil ) {
            char szName[255];
            sprintf(szName, "NSOperationQueue %08x", (unsigned int)self);
            priv->_name = [[NSString stringWithCString: szName] retain];
        }

        return priv->_name;
    }

    /* annotate with type */ +(id) mainQueue {
        if ( _mainQueue == nil ) {
            _mainQueue = [[self alloc] _initMainThread];
        }

        return _mainQueue;
    }

    -(void) dealloc {
        [priv->workAvailable release];
        [priv->suspendedCondition release];
        [priv->allWorkDone release];
        EbrLockDestroy(priv->_threadRunningLock);
        delete priv;

        [super dealloc];
    }

    /* annotate with type */ -(id) retain {
        return [super retain];
    }

    -(void) release {
        [super release];
    }

    /* annotate with type */ +(id) currentQueue {
        if ( [NSThread isMainThread] ) {
            return [self mainQueue];
        }

        assert(0);

        return self;
    }

    
@end

