/*
 * File: affinity1.c
 *
 *
 * --------------------------------------------------------------------------
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
 *
 * --------------------------------------------------------------------------
 *
 * Test Synopsis:
 * - Test thread affinityexplicit setting
 *
 * Test Method (Validation or Falsification):
 * -
 *
 * Requirements Tested:
 * -
 *
 * Features Tested:
 * -
 *
 * Cases Tested:
 * -
 *
 * Description:
 * -
 *
 * Environment:
 * -
 *
 * Input:
 * - None.
 *
 * Output:
 * - File name, Line number, and failed expression on failure.
 * - No output on success.
 *
 * Assumptions:
 * -
 *
 * Pass Criteria:
 * - Process returns zero exit status.
 *
 * Fail Criteria:
 * - Process returns non-zero exit status.
 */

#include "test.h"

static void *
func(void * arg)
{
  struct timespec interval =
    {
      10, 500000000L
    };

  assert(pthread_delay_np(&interval) == 0);

  return (void *) 0;
}

int pthread_test_affinity1()
{
  pthread_t t;
  pthread_attr_t attr;
  void * result = NULL;

  cpu_set_t cpuset;

  assert(pthread_attr_init(&attr) == 0);

  assert(pthread_create(&t, &attr, func, (void *) &attr) == 0);

#ifdef __vita__
  CPU_ZERO(&cpuset);
  pthread_getaffinity_np(t, sizeof(cpuset), &cpuset);
  assert(cpuset == 0x7);
#endif

  CPU_ZERO(&cpuset);
  CPU_SET(0, &cpuset);
  assert(CPU_ISSET(0,  &cpuset) == 1);
  assert(CPU_COUNT(&cpuset) == 1);
  assert(pthread_setaffinity_np(t, sizeof(cpuset), &cpuset) == 0);

  CPU_ZERO(&cpuset);
  pthread_getaffinity_np(t, sizeof(cpuset), &cpuset);
  assert(cpuset == 0x1);
  assert(CPU_ISSET(0,  &cpuset) == 1);
  assert(CPU_COUNT(&cpuset) == 1);

  assert(pthread_join(t, &result) == 0);

  return 0;
}
