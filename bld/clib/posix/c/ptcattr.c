/****************************************************************************
*
*                            Open Watcom Project
*
* Copyright (c) 2016 The Open Watcom Contributors. All Rights Reserved.
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
* Description:  POSIX thread condition variable attributes
*
* Author: J. Armstrong
*
****************************************************************************/

#include "variety.h"
#include <sys/types.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#include "rterrno.h"
#include "thread.h"


_WCRTLINK int pthread_condattr_init(pthread_condattr_t *__attr)
{
    /* unused parameters */ (void)__attr;

    return( 0 );
}

_WCRTLINK int pthread_condattr_destroy(pthread_condattr_t *__attr)
{
    /* unused parameters */ (void)__attr;

    return( 0 );
}

_WCRTLINK int pthread_condattr_getpshared(const pthread_condattr_t *__attr, int *__pshared)
{
    if(__attr == NULL || __pshared == NULL)
        return( EINVAL );

    *__pshared = PTHREAD_PROCESS_PRIVATE;

    return( 0 );
}

_WCRTLINK int pthread_condattr_setpshared(pthread_condattr_t *__attr, int __pshared)
{
    if(__attr == NULL)
        return( EINVAL );

    if(__pshared != PTHREAD_PROCESS_PRIVATE)
        return( ENOSYS );

    return( 0 );
}

_WCRTLINK int pthread_condattr_getclock( pthread_condattr_t *__attr, clockid_t *clk )
{
    if(__attr == NULL || clk == NULL)
        return( EINVAL );

    /* This function always returns CLOCK_MONOTONIC because conditions
     * rely on sem_timedwait, which itself uses CLOCK_MONOTONIC and
     * is not changeable.
     */
    *clk = CLOCK_MONOTONIC;

    return( 0 );
}

_WCRTLINK int pthread_condattr_setclock( pthread_condattr_t *__attr, clockid_t clk )
{
    if(__attr == NULL)
        return( EINVAL );

    if(clk != CLOCK_MONOTONIC)
        return( ENOSYS );

    return( 0 );
}
