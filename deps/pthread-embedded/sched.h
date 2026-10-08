/*
 * Module: sched.h
 *
 * Purpose:
 *      Provides an implementation of POSIX realtime extensions
 *      as defined in
 *
 *              POSIX 1003.1b-1993      (POSIX.1b)
 *
 * --------------------------------------------------------------------------
 *
 *      Pthreads-embedded (PTE) - POSIX Threads Library for embedded systems
 *      Copyright(C) 2008 Jason Schmidlapp
 *
 *      Contact Email: jschmidlapp@users.sourceforge.net
 *
 *
 *      Pthreads-embedded (PTE) - POSIX Threads Library for embedded systems
 *      Copyright(C) 2008 Jason Schmidlapp
 *
 *      Contact Email: jschmidlapp@users.sourceforge.net
 *
 *
 *      Based upon Pthreads-win32 - POSIX Threads Library for Win32
 *      Copyright(C) 1998 John E. Bossom
 *      Copyright(C) 1999,2005 Pthreads-win32 contributors
 *
 *      Contact Email: rpj@callisto.canberra.edu.au
 *
 *      The original list of contributors to the Pthreads-win32 project
 *      is contained in the file CONTRIBUTORS.ptw32 included with the
 *      source code distribution. The list can also be seen at the
 *      following World Wide Web location:
 *      http://sources.redhat.com/pthreads-win32/contributors.html
 *
 *      This library is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU Lesser General Public
 *      License as published by the Free Software Foundation; either
 *      version 2 of the License, or (at your option) any later version.
 *
 *      This library is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *      Lesser General Public License for more details.
 *
 *      You should have received a copy of the GNU Lesser General Public
 *      License along with this library in the file COPYING.LIB;
 *      if not, write to the Free Software Foundation, Inc.,
 *      59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */
#ifndef _SCHED_H
#define _SCHED_H

#include <sys/types.h>
#include <sys/sched.h>

#include <pte_types.h>

#ifdef __cplusplus
extern "C"
  {
#endif                          /* __cplusplus */

    int sched_yield (void);

    int sched_get_priority_min (int policy);

    int sched_get_priority_max (int policy);

    int sched_setscheduler (pid_t pid, int policy);

//    int sched_setaffinity(pid_t pid, size_t cpusetsize,
//                             const cpu_set_t *mask);
//    int sched_getaffinity(pid_t pid, size_t cpusetsize,
//                             cpu_set_t *mask);

    extern int __sched_cpucount(const cpu_set_t *setp);

    #define CPU_ZERO(cpusetp) \
        do *cpusetp = 0; while (0)

    #define CPU_SET(cpu, cpusetp) \
        do *cpusetp |= (1U << (cpu)); while (0)

    #define CPU_CLR(cpu, cpusetp) \
        do *cpusetp &= ~(1U << (cpu)); while (0)

    #define CPU_ISSET(cpu, cpusetp) \
        (!!(*cpusetp & (1U << (cpu))))

    #define CPU_COUNT(cpusetp) \
        __sched_cpucount(cpusetp)

    #define CPU_EQUAL(cpusetp1, cpusetp2) \
        (*cpusetp1 == *cpusetp2)

    /*
     * Note that this macro returns ENOTSUP rather than
     * ENOSYS as might be expected. However, returning ENOSYS
     * should mean that sched_get_priority_{min,max} are
     * not implemented as well as sched_rr_get_interval.
     * This is not the case, since we just don't support
     * round-robin scheduling. Therefore I have chosen to
     * return the same value as sched_setscheduler when
     * SCHED_RR is passed to it.
     */
#define sched_rr_get_interval(_pid, _interval) \
  ( errno = ENOTSUP, (int) -1 )


#ifdef __cplusplus
  }                               /* End of extern "C" */
#endif                          /* __cplusplus */

#endif                          /* !_SCHED_H */

