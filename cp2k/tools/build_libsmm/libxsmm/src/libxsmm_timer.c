/******************************************************************************
** Copyright (c) 2009-2015, Intel Corporation                                **
** All rights reserved.                                                      **
**                                                                           **
** Redistribution and use in source and binary forms, with or without        **
** modification, are permitted provided that the following conditions        **
** are met:                                                                  **
** 1. Redistributions of source code must retain the above copyright         **
**    notice, this list of conditions and the following disclaimer.          **
** 2. Redistributions in binary form must reproduce the above copyright      **
**    notice, this list of conditions and the following disclaimer in the    **
**    documentation and/or other materials provided with the distribution.   **
** 3. Neither the name of the copyright holder nor the names of its          **
**    contributors may be used to endorse or promote products derived        **
**    from this software without specific prior written permission.          **
**                                                                           **
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS       **
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT         **
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR     **
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT      **
** HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,    **
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED  **
** TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR    **
** PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF    **
** LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING      **
** NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS        **
** SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.              **
******************************************************************************/
/* Hans Pabst (Intel Corp.)
******************************************************************************/
#include <libxsmm_timer.h>

#if defined(LIBXSMM_OFFLOAD_BUILD)
# pragma offload_attribute(push,target(LIBXSMM_OFFLOAD_TARGET))
#endif
#if defined(_WIN32)
# include <Windows.h>
#elif defined(__GNUC__)
# include <sys/time.h>
# include <time.h>
#endif
#if defined(LIBXSMM_OFFLOAD_BUILD)
# pragma offload_attribute(pop)
#endif


LIBXSMM_EXTERN_C LIBXSMM_RETARGETABLE unsigned long long libxsmm_timer_tick()
{
#if defined(_WIN32)
  LARGE_INTEGER t;
  QueryPerformanceCounter(&t);
  return (unsigned long long)t.QuadPart;
#elif defined(CLOCK_MONOTONIC)
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return 1000000000ULL * t.tv_sec + t.tv_nsec;
#else
  struct timeval t;
  gettimeofday(&t, 0);
  return 1000000ULL * t.tv_sec + t.tv_usec;
#endif
}


LIBXSMM_EXTERN_C LIBXSMM_RETARGETABLE double libxsmm_timer_duration(unsigned long long tick0, unsigned long long tick1)
{
  const double d = (double)((tick0 < tick1 ? tick1 : tick0) - tick0);
#if defined(_WIN32)
  LARGE_INTEGER frequency;
  QueryPerformanceFrequency(&frequency);
  return d / (double)frequency.QuadPart;
#elif defined(CLOCK_MONOTONIC)
  return d * 1E-9;
#else
  return d * 1E-6;
#endif
}
