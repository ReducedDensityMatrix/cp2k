/******************************************************************************
** Copyright (c) 2014-2015, Intel Corporation                                **
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
#if defined(LIBXSTREAM_EXPORTED) || defined(__LIBXSTREAM)
#include "libxstream_capture.hpp"
#include "libxstream_queue.hpp"

#include <libxstream_begin.h>
#include <algorithm>
#include <cstdio>
#if defined(LIBXSTREAM_STDFEATURES)
# include <thread>
# include <atomic>
#else
# if defined(__GNUC__)
#   include <pthread.h>
# else
#   include <Windows.h>
# endif
#endif
#include <libxstream_end.h>

#define LIBXSTREAM_CAPTURE_TERMINATOR reinterpret_cast<libxstream_capture_base*>(-1)

//#define LIBXSTREAM_CAPTURE_DEBUG
//#define LIBXSTREAM_CAPTURE_UNLOCK_LATE


namespace libxstream_capture_internal {

static/*IPO*/ class scheduler_type {
public:
  scheduler_type()
    : m_queue()
    , m_status(LIBXSTREAM_ERROR_NONE)
#if defined(LIBXSTREAM_STDFEATURES)
    , m_thread() // do not start here
#else
    , m_thread(0)
#endif
  {
#if defined(LIBXSTREAM_STDFEATURES)
    std::thread(run, &m_queue).swap(m_thread);
#else
# if defined(__GNUC__)
    pthread_create(&m_thread, 0, run, &m_queue);
# else
    m_thread = CreateThread(0, 0, run, &m_queue, 0, 0);
# endif
#endif
  }

  ~scheduler_type() {
    // terminates the background thread
    push(LIBXSTREAM_CAPTURE_TERMINATOR, true);

#if defined(LIBXSTREAM_STDFEATURES)
    m_thread.detach();
#else
# if defined(__GNUC__)
    pthread_detach(m_thread);
# else
    CloseHandle(m_thread);
# endif
#endif
  }

public:
  int status(int code) {
#if defined(LIBXSTREAM_STDFEATURES)
    return std::atomic_exchange(&m_status, code);
#elif defined(_OPENMP)
    int result = 0;
#   pragma omp critical
    {
      result = m_status;
      m_status = code;
    }
    return result;
#else // generic
    int result = 0;
    libxstream_lock_acquire(libxstream_lock_get(this));
    result = m_status;
    m_status = code;
    libxstream_lock_release(libxstream_lock_get(this));
    return result;
#endif
  }

  void push(const libxstream_capture_base& capture_region, bool wait) {
    push(&capture_region, wait);
  }

private:
  void push(const libxstream_capture_base* capture_region, bool wait) {
    LIBXSTREAM_ASSERT(capture_region);
    libxstream_capture_base *const item = (LIBXSTREAM_CAPTURE_TERMINATOR) != capture_region
      ? capture_region->clone()
      : (LIBXSTREAM_CAPTURE_TERMINATOR);

    libxstream_capture_base** entry = reinterpret_cast<libxstream_capture_base**>(m_queue.allocate_push());
    *entry = item; // push

    if (wait) {
      while (item == *entry) {
        this_thread_yield();
      }
    }
  }

#if defined(LIBXSTREAM_STDFEATURES) || defined(__GNUC__)
  static void* run(void* queue)
#else
  static DWORD WINAPI run(_In_ LPVOID queue)
#endif
  {
    libxstream_queue& q = *static_cast<libxstream_queue*>(queue);
    void* item = 0;
    bool always = true;

#if defined(LIBXSTREAM_ASYNCHOST) && (201307 <= _OPENMP)
#   pragma omp parallel
#   pragma omp master
#endif
    for (; always;) {
      size_t cycle = 0;
      while (0 == (item = q.get())) {
        if ((LIBXSTREAM_WAIT_ACTIVE_CYCLES) > cycle) {
          this_thread_yield();
          ++cycle;
        }
        else {
          this_thread_sleep();
        }
      }

      if ((LIBXSTREAM_CAPTURE_TERMINATOR) != item) {
        libxstream_capture_base *const capture_region = static_cast<libxstream_capture_base*>(item);
        (*capture_region)();
#if defined(LIBXSTREAM_ASYNCHOST) && (201307 <= _OPENMP)
#       pragma omp taskwait
#endif
        delete capture_region;
        q.pop();
      }
      else {
        q.pop();
      }
    }

#if defined(LIBXSTREAM_STDFEATURES) || defined(__GNUC__)
    return queue;
#else
    return EXIT_SUCCESS;
#endif
  }

private:
  libxstream_queue m_queue;
#if defined(LIBXSTREAM_STDFEATURES)
  std::atomic<int> m_status;
  std::thread m_thread;
#elif defined(__GNUC__)
  int m_status;
  pthread_t m_thread;
#else
  int m_status;
  HANDLE m_thread;
#endif
#if defined(LIBXSTREAM_CAPTURE_DEBUG)
};
#else
} scheduler;
#endif

} // namespace libxstream_capture_internal


libxstream_capture_base::libxstream_capture_base(size_t argc, const arg_type argv[], libxstream_stream* stream, int flags)
  : m_function(0)
  , m_stream(stream)
  , m_flags(flags)
#if defined(LIBXSTREAM_THREADLOCAL_SIGNALS) && defined(LIBXSTREAM_ASYNC) && (0 != (2*LIBXSTREAM_ASYNC+1)/2)
  , m_thread(this_thread_id())
#endif
#if defined(LIBXSTREAM_CAPTURE_UNLOCK_LATE)
  , m_unlock(false)
#else
  , m_unlock(true)
#endif
{
  if (2 == argc && (argv[0].signature() || argv[1].signature())) {
    const libxstream_argument* signature = 0;
    if (argv[1].signature()) {
      m_function = *reinterpret_cast<const libxstream_function*>(argv + 0);
      signature = static_cast<const libxstream_argument*>(libxstream_get_value(argv[1]).const_pointer);
    }
    else {
      LIBXSTREAM_ASSERT(argv[0].signature());
      m_function = *reinterpret_cast<const libxstream_function*>(argv + 1);
      signature = static_cast<const libxstream_argument*>(libxstream_get_value(argv[0]).const_pointer);
    }

    size_t arity = 0;
    if (signature) {
      LIBXSTREAM_CHECK_CALL_ASSERT(libxstream_get_arity(signature, &arity));
      LIBXSTREAM_PRAGMA_LOOP_COUNT(0, LIBXSTREAM_MAX_NARGS, LIBXSTREAM_MAX_NARGS/2)
      for (size_t i = 0; i < arity; ++i) m_signature[i] = signature[i];
    }
    LIBXSTREAM_CHECK_CALL_ASSERT(libxstream_construct(m_signature, arity, libxstream_argument::kind_invalid, 0, LIBXSTREAM_TYPE_INVALID, 0, 0));
  }
  else {
    LIBXSTREAM_ASSERT(argc <= (LIBXSTREAM_MAX_NARGS));
    for (size_t i = 0; i < argc; ++i) m_signature[i] = argv[i];
    LIBXSTREAM_CHECK_CALL_ASSERT(libxstream_construct(m_signature, argc, libxstream_argument::kind_invalid, 0, LIBXSTREAM_TYPE_INVALID, 0, 0));
#if defined(LIBXSTREAM_DEBUG)
    size_t arity = 0;
    LIBXSTREAM_ASSERT(LIBXSTREAM_ERROR_NONE == libxstream_get_arity(m_signature, &arity) && arity == argc);
#endif
  }

  if (stream) {
    if (0 == (flags & LIBXSTREAM_CALL_WAIT) && 0 != stream->demux()) {
      stream->lock(0 > stream->demux());
    }
    stream->begin();
  }
}


libxstream_capture_base::~libxstream_capture_base()
{
  if (m_unlock && m_stream) {
    m_stream->end();
    if (0 != (m_flags & LIBXSTREAM_CALL_UNLOCK) && 0 != m_stream->demux()) {
      m_stream->unlock();
    }
  }
}


int libxstream_capture_base::status(int code)
{
#if !defined(LIBXSTREAM_CAPTURE_DEBUG)
  return libxstream_capture_internal::scheduler.status(code);
#else
  return code;
#endif
}


int libxstream_capture_base::thread() const
{
#if defined(LIBXSTREAM_THREADLOCAL_SIGNALS) && defined(LIBXSTREAM_ASYNC) && (0 != (2*LIBXSTREAM_ASYNC+1)/2)
  return m_thread;
#else
  return 0;
#endif
}


libxstream_capture_base* libxstream_capture_base::clone() const
{
  libxstream_capture_base *const instance = virtual_clone();
#if defined(LIBXSTREAM_CAPTURE_UNLOCK_LATE)
  instance->m_unlock = true;
#else
  instance->m_unlock = false;
#endif
  return instance;
}


void libxstream_capture_base::operator()()
{
  virtual_run();
}


int libxstream_enqueue(const libxstream_capture_base& capture_region, bool wait)
{
#if !defined(LIBXSTREAM_CAPTURE_DEBUG)
# if defined(LIBXSTREAM_SYNCHRONOUS)
  libxstream_use_sink(&wait);
  libxstream_capture_internal::scheduler.push(capture_region, true);
# else
  libxstream_capture_internal::scheduler.push(capture_region, wait);
# endif
  return libxstream_capture_internal::scheduler.status(LIBXSTREAM_ERROR_NONE);
#else
  libxstream_use_sink(&wait);
  libxstream_capture_base *const capture_region_clone = capture_region.clone();
  (*capture_region_clone)();
  delete capture_region_clone;
  return LIBXSTREAM_ERROR_NONE;
#endif
}

#endif // defined(LIBXSTREAM_EXPORTED) || defined(__LIBXSTREAM)
