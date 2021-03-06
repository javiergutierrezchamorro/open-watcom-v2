/****************************************************************************
*
*                            Open Watcom Project
*
*    Portions Copyright (c) 1983-2002 Sybase, Inc. All Rights Reserved.
*
*  ========================================================================
*
*    This file contains Original Code and/or Modifications of Original
*    Code as defined in and that are subject to the Sybase Open Watcom
*    Public License version 1.0 (the 'License'). You may not use this file
*    except in compliance with the License. BY USING THIS FILE YOU AGREE TO
*    ALL TERMS AND CONDITIONS OF THE LICENSE. A copy of the License is
*    provided with the Original Code and Modifications, and is also
*    available at www.sybase.com/developer/opensource.
*
*    The Original Code and all software distributed under the License are
*    distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
*    EXPRESS OR IMPLIED, AND SYBASE AND ALL CONTRIBUTORS HEREBY DISCLAIM
*    ALL SUCH WARRANTIES, INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF
*    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR
*    NON-INFRINGEMENT. Please see the License for the specific language
*    governing rights and limitations under the License.
*
*  ========================================================================
*
* Description:  WHEN YOU FIGURE OUT WHAT THIS FILE DOES, PLEASE
*               DESCRIBE IT HERE!
*
****************************************************************************/


#ifndef _RT_RTTI_H
#define _RT_RTTI_H
#include <typeinfo.h>
#include "rtexcept.h"
#include "wcpp.h"

// these are generated by the compiler
struct rtti_leap {
    uint_16             vb_index;       // index into virtual base table
    uint_16             control;        // RL_*
    unsigned            offset;         // offset to add to reach class
    type_info const     *id;            // id of class
};

struct rtti_class {
    type_info const     *id;    // id of most-derived class
    unsigned            vbptr;  // offset from most-derived of vbptr
    rtti_leap           leaps[];// list of classes in hierarchy
};

// RA_* bits are in 'from_here' offset since it will always be a multiple
// of sizeof( rtti_adjust )
struct rtti_adjust {
    unsigned    from_here;      // amount to add to this addr pt to reach most-derived class descriptor
    unsigned    delta;          // this -= delta;
};

// run-time support control object

typedef void (*rtti_bad)( void );

struct rtti_exec {
    void                *vfptr;         // vfptr addr point (in object)
    void                *md_addr;       // most-derived addr point (in object)
    rtti_class const    *md_class;      // most-derived class descriptor
    type_info const     *from_tid;      // type-id of static 'from' class
    type_info const     *to_tid;        // type-id of static 'to' class
    rtti_bad            throw_except;   // function to call for bad_cast
    unsigned            from_missing : 1;   // 'from' class not in hierarchy
    unsigned            ok : 1;         // conversion is OK
};

#define _INIT_EXEC( d, p, f, t, te ) \
        d.vfptr = p; \
        d.from_tid = f; \
        d.to_tid = t; \
        d.throw_except = te;

#endif

extern "C" {

extern void *__CalcMostDerived( rtti_exec * );
extern rtti_leap const *__MakeSureTidIsPresent( rtti_exec *, type_info const *, rtti_leap * );
extern void *__DoDynamicCast( rtti_exec * );

_WPRTLINK
void * CPPLIB( dcptr )(
    void *p,
    unsigned delta,
    type_info const *from,
    type_info const *to )
;
_WPRTLINK
void * CPPLIB( dcref )(
    void *p,
    unsigned delta,
    type_info const *from,
    type_info const *to )
;
_WPRTLINK
void * CPPLIB( dcvoid )(
    void *p,
    unsigned delta,
    type_info const *from )
;
_WPRTLINK
void const * CPPLIB( gettid )(
    void *p,
    unsigned delta,
    type_info const *from )
;

};
