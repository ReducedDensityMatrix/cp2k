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
#include "libxstream_stream.hpp"
#include "libxstream_workqueue.hpp"
#include "libxstream_workitem.hpp"
#include "libxstream_event.hpp"

#include <libxstream_begin.h>
#include <algorithm>
#include <string>
#include <cstdio>
#if defined(LIBXSTREAM_STDFEATURES)
# include <atomic>
#endif
#include <libxstream_end.h>

// allows to wait for an event issued prior to the pending signal
//#define LIBXSTREAM_STREAM_WAIT_PAST
// check whether a signal is really pending; update internal state
//#define LIBXSTREAM_STREAM_CHECK_PENDING


namespace libxstream_stream_internal {

static/*IPO*/ class registry_type {
public:
  typedef libxstream_stream* value_type;

public:
  registry_type()
    : m_istreams(0)
  {
    std::fill_n(m_signals, LIBXSTREAM_MAX_NDEVICES, 0);
    std::fill_n(m_streams, LIBXSTREAM_MAX_NDEVICES * LIBXSTREAM_MAX_NSTREAMS, static_cast<value_type>(0));
  }

  ~registry_type() {
    const size_t n = max_nstreams();
    for (size_t i = 0; i < n; ++i) {
#if defined(LIBXSTREAM_DEBUG)
      if (0 != m_streams[i]) {
        LIBXSTREAM_PRINT(1, "stream=0x%llx (%s) is dangling!", reinterpret_cast<unsigned long long>(m_streams[i]), m_streams[i]->name());
      }
#endif
      libxstream_stream_destroy(m_streams[i]);
    }
  }

public:
  size_t priority_range(int device, int& least, int& greatest) {
    const size_t n = max_nstreams();
    for (size_t i = 0; i < n; ++i) {
      if (const value_type stream = m_streams[i]) {
        const int stream_device = stream->device();
        if (stream_device == device) {
          const int priority = stream->priority();
          least = std::min(least, priority);
          greatest = std::max(least, priority);
        }
      }
    }
    size_t result = 0;
    for (size_t i = 0; i < n; ++i) {
      if (const value_type stream = m_streams[i]) {
        const int stream_device = stream->device();
        if (stream_device == device) {
          const int priority = stream->priority();
          result += priority - greatest;
        }
      }
    }
    return result;
  }

  volatile value_type& allocate() {
#if !defined(LIBXSTREAM_STDFEATURES)
    libxstream_lock *const lock = libxstream_lock_get(this);
    libxstream_lock_acquire(lock);
#endif
    volatile value_type* i = m_streams + LIBXSTREAM_MOD(m_istreams++, (LIBXSTREAM_MAX_NDEVICES) * (LIBXSTREAM_MAX_NSTREAMS));
    while (0 != *i) i = m_streams + LIBXSTREAM_MOD(m_istreams++, (LIBXSTREAM_MAX_NDEVICES) * (LIBXSTREAM_MAX_NSTREAMS));
#if !defined(LIBXSTREAM_STDFEATURES)
    libxstream_lock_release(lock);
#endif
    return *i;
  }

  size_t max_nstreams() const {
    return std::min<size_t>(m_istreams, LIBXSTREAM_MAX_NDEVICES * LIBXSTREAM_MAX_NSTREAMS);
  }

  size_t nstreams(int device, const libxstream_stream* end = 0) const {
    const size_t n = max_nstreams();
    size_t result = 0;
    if (0 == end) {
      for (size_t i = 0; i < n; ++i) {
        const value_type stream = m_streams[i];
        result += (0 != stream && stream->device() == device) ? 1 : 0;
      }
    }
    else {
      for (size_t i = 0; i < n; ++i) {
        const value_type stream = m_streams[i];
        if (end != stream) {
          result += (0 != stream && stream->device() == device) ? 1 : 0;
        }
        else {
          i = n; // break
        }
      }
    }
    return result;
  }

  size_t nstreams() const {
    const size_t n = max_nstreams();
    size_t result = 0;
    for (size_t i = 0; i < n; ++i) {
      const value_type stream = m_streams[i];
      result += 0 != stream ? 1 : 0;
    }
    return result;
  }

  libxstream_signal& signal(int device) {
    LIBXSTREAM_ASSERT(-1 <= device && device <= LIBXSTREAM_MAX_NDEVICES);
    return m_signals[device+1];
  }

  volatile value_type* streams() {
    return m_streams;
  }

  int enqueue(libxstream_event& event, const libxstream_stream* exclude) {
    int result = LIBXSTREAM_ERROR_NONE;
    const size_t n = max_nstreams();
    bool reset = true;

    for (size_t i = 0; i < n; ++i) {
      const value_type stream = m_streams[i];

      if (stream != exclude) {
        result = event.enqueue(*stream, reset);
        LIBXSTREAM_CHECK_ERROR(result);
        reset = false;
      }
    }

    LIBXSTREAM_ASSERT(LIBXSTREAM_ERROR_NONE == result);
    return result;
  }

  value_type schedule(const libxstream_stream* exclude) {
    const size_t n = max_nstreams();
    value_type result = 0;

    size_t j = 0;
    for (size_t i = 0; i < n; ++i) {
      result = m_streams[i];
      if (result == exclude) {
        result = 0;
        j = i;
        i = n; // break
      }
    }

    const size_t end = j + n;
    for (size_t i = j + 1; i < end; ++i) {
      const value_type stream = m_streams[/*i%n*/i<n?i:(i-n)];

      if (0 != stream) {
        const libxstream_event *const events = stream->events();
        bool occurred = true;

        if (0 != events) {
          const size_t nevents = stream->nevents();
          for (size_t k = 0; k < nevents && occurred; ++k) {
            const libxstream_event& event = events[k];
            if (LIBXSTREAM_ERROR_NONE != event.query(occurred, stream)) {
              occurred = false;
              i = end; // break
            }
          }
        }

        if (occurred) {
          result = stream;
          i = end; // break
        }
      }
    }

    return result;
  }

  int sync(bool wait, int device) {
    const size_t n = max_nstreams();
    for (size_t i = 0; i < n; ++i) {
      if (const value_type stream = m_streams[i]) {
        const int stream_device = stream->device();
        if (stream_device == device) {
          const int result = stream->sync(wait, 0);
          LIBXSTREAM_CHECK_ERROR(result);
        }
      }
    }
    return LIBXSTREAM_ERROR_NONE;
  }

  int sync(bool wait) {
    const size_t n = max_nstreams();
    for (size_t i = 0; i < n; ++i) {
      if (const value_type stream = m_streams[i]) {
        const int result = stream->sync(wait, 0);
        LIBXSTREAM_CHECK_ERROR(result);
      }
    }
    return LIBXSTREAM_ERROR_NONE;
  }

private:
  // not necessary to be device-specific due to single-threaded offload
  libxstream_signal m_signals[LIBXSTREAM_MAX_NDEVICES + 1];
  volatile value_type m_streams[LIBXSTREAM_MAX_NDEVICES*LIBXSTREAM_MAX_NSTREAMS];
#if defined(LIBXSTREAM_STDFEATURES)
  std::atomic<size_t> m_istreams;
#else
  size_t m_istreams;
#endif
} registry;


template<typename A, typename E, typename D>
bool atomic_compare_exchange(A& atomic, E& expected, D desired)
{
#if defined(LIBXSTREAM_STDFEATURES)
  const bool result = std::atomic_compare_exchange_weak(&atomic, &expected, desired);
#elif defined(_OPENMP)
  bool result = false;
# pragma omp critical
  {
    result = atomic == expected;
    if (result) {
      atomic = desired;
    }
    else {
      expected = atomic;
    }
  }
#else // generic
  bool result = false;
  libxstream_lock *const lock = libxstream_lock_get(&atomic);
  libxstream_lock_acquire(lock);
  result = atomic == expected;
  if (result) {
    atomic = desired;
  }
  else {
    expected = atomic;
  }
  libxstream_lock_release(lock);
#endif
  return result;
}


template<typename A, typename T>
T atomic_store(A& atomic, T value)
{
  T result = value;
#if defined(LIBXSTREAM_STDFEATURES)
  result = std::atomic_exchange(&atomic, value);
#elif defined(_OPENMP)
# pragma omp critical
  {
    result = atomic;
    atomic = value;
  }
#else // generic
  libxstream_lock_acquire(registry.lock());
  result = atomic;
  atomic = value;
  libxstream_lock_release(registry.lock());
#endif
  return result;
}

} // namespace libxstream_stream_internal


/*static*/int libxstream_stream::priority_range_least()
{
#if defined(LIBXSTREAM_OFFLOAD) && (0 != LIBXSTREAM_OFFLOAD) && defined(LIBXSTREAM_ASYNC) && (2 == (2*LIBXSTREAM_ASYNC+1)/2)
  const int result = LIBXSTREAM_MAX_NTHREADS;
#else // not supported (empty range)
  const int result = 0;
#endif
  return result;
}


/*static*/int libxstream_stream::priority_range_greatest()
{
#if defined(LIBXSTREAM_OFFLOAD) && (0 != LIBXSTREAM_OFFLOAD) && defined(LIBXSTREAM_ASYNC) && (2 == (2*LIBXSTREAM_ASYNC+1)/2)
  const int result = 0;
#else // not supported (empty range)
  const int result = 0;
#endif
  return result;
}


/*static*/int libxstream_stream::enqueue(libxstream_event& event, const libxstream_stream* exclude)
{
  return libxstream_stream_internal::registry.enqueue(event, exclude);
}


/*static*/libxstream_stream* libxstream_stream::schedule(const libxstream_stream* exclude)
{
  return libxstream_stream_internal::registry.schedule(exclude);
}


/*static*/int libxstream_stream::sync_all(bool wait, int device)
{
  return libxstream_stream_internal::registry.sync(wait, device);
}


/*static*/int libxstream_stream::sync_all(bool wait)
{
  return libxstream_stream_internal::registry.sync(wait);
}


libxstream_stream::libxstream_stream(int device, int priority, const char* name)
  : m_device(device), m_priority(priority), m_thread(-1)
#if defined(LIBXSTREAM_OFFLOAD) && defined(LIBXSTREAM_ASYNC) && (2 == (2*LIBXSTREAM_ASYNC+1)/2)
  , m_handle(0) // lazy creation
  , m_npartitions(0)
#endif
{
  std::fill_n(m_pending, LIBXSTREAM_MAX_NTHREADS, static_cast<libxstream_signal>(0));
  std::fill_n(m_queues, LIBXSTREAM_MAX_NTHREADS, static_cast<libxstream_workqueue*>(0));

  // sanitize the stream priority
  const int priority_least = priority_range_least(), priority_greatest = priority_range_greatest();
  m_priority = std::max(priority_greatest, std::min(priority_least, priority));
#if defined(LIBXSTREAM_TRACE) && ((1 == ((2*LIBXSTREAM_TRACE+1)/2) && defined(LIBXSTREAM_DEBUG)) || 1 < ((2*LIBXSTREAM_TRACE+1)/2))
  if (m_priority != priority) {
    LIBXSTREAM_PRINT(2, "stream priority %i has been clamped to %i", priority, m_priority);
  }
#endif

#if defined(LIBXSTREAM_TRACE) && 0 != ((2*LIBXSTREAM_TRACE+1)/2) && defined(LIBXSTREAM_DEBUG)
  if (name && 0 != *name) {
    const size_t length = std::min(std::char_traits<char>::length(name), sizeof(m_name) - 1);
    std::copy(name, name + length, m_name);
    m_name[length] = 0;
  }
  else {
    m_name[0] = 0;
  }
#else
  libxstream_use_sink(name);
#endif

  using namespace libxstream_stream_internal;
  volatile registry_type::value_type& entry = libxstream_stream_internal::registry.allocate();
  entry = this;
}


libxstream_stream::~libxstream_stream()
{
  LIBXSTREAM_CHECK_CALL_ASSERT(sync(true/*wait*/, 0/*all*/));

  using namespace libxstream_stream_internal;
  volatile registry_type::value_type *const end = registry.streams() + registry.max_nstreams();
  volatile registry_type::value_type *const stream = std::find(registry.streams(), end, this);
  LIBXSTREAM_ASSERT(stream != end);
  *stream = 0; // unregister stream

  const size_t nthreads = nthreads_active();
  for (size_t i = 0; i < nthreads; ++i) {
    delete[] m_slots[i].events;
    delete m_queues[i];
  }

#if defined(LIBXSTREAM_OFFLOAD) && (0 != LIBXSTREAM_OFFLOAD) && !defined(__MIC__) && defined(LIBXSTREAM_ASYNC) && (2 == (2*LIBXSTREAM_ASYNC+1)/2)
  if (0 != m_handle) {
    _Offload_stream_destroy(m_device, m_handle);
  }
#endif
}


libxstream_signal libxstream_stream::signal() const
{
  return ++libxstream_stream_internal::registry.signal(m_device);
}


int libxstream_stream::sync(bool wait, libxstream_signal signal)
{
  const size_t nthreads = nthreads_active();

  LIBXSTREAM_ASYNC_BEGIN
  {
    const bool wait = val<const bool,0>();
    const libxstream_signal signal = val<const libxstream_signal,1>();
    libxstream_signal *const pending_signals = ptr<libxstream_signal,2>();
    const size_t nthreads = val<const size_t,3>();

    for (size_t i = 0; i < nthreads; ++i) {
      const libxstream_signal pending_signal = pending_signals[i];
      if (0 != pending_signal) {
#if defined(LIBXSTREAM_OFFLOAD) && defined(LIBXSTREAM_ASYNC) && (0 != (2*LIBXSTREAM_ASYNC+1)/2)
        if (wait && 0 <= LIBXSTREAM_ASYNC_DEVICE) {
# if defined(LIBXSTREAM_STREAM_WAIT_PAST)
          const libxstream_signal wait_pending = 0 != signal ? signal : pending_signal;
# else
          const libxstream_signal wait_pending = pending_signal;
# endif
#           pragma offload_wait LIBXSTREAM_ASYNC_TARGET wait(wait_pending)
        }
#else
        libxstream_use_sink(&wait);
#endif
        if (0 == signal) {
          pending_signals[i] = 0;
        }
#if defined(LIBXSTREAM_STREAM_WAIT_PAST)
        else {
          i = nthreads; // break
        }
#endif
      }
    }

    if (0 != signal) {
      for (size_t i = 0; i < nthreads; ++i) {
        if (signal == pending_signals[i]) {
          pending_signals[i] = 0;
        }
      }
    }
  }
  LIBXSTREAM_ASYNC_END(this, LIBXSTREAM_CALL_DEFAULT | LIBXSTREAM_CALL_SYNC, work, wait, signal, m_pending, nthreads);

  return work.wait();
}


int libxstream_stream::wait(libxstream_event& event)
{
  bool occurred = true;
  int result = event.query(occurred, this);

  if (LIBXSTREAM_ERROR_NONE == result && !occurred) { // avoids waiting for en empty event
    const int thread = this_thread_id();
    slot_type& slot = m_slots[thread];

    if (0 == slot.events) {
      slot.events = new libxstream_event[LIBXSTREAM_MAX_QSIZE];
    }
    LIBXSTREAM_ASSERT(0 != slot.events);

    for (size_t i = slot.size; 0 < i; --i) {
      LIBXSTREAM_ASSERT(0 < i && i <= LIBXSTREAM_MAX_QSIZE);
      const libxstream_event& eventi = slot.events[i-1];
      result = eventi.query(occurred, this);
      if (LIBXSTREAM_ERROR_NONE == result && occurred) {
        --slot.size;
      }
      else {
        i = 1; // break
      }
    }

    slot.events[slot.size].swap(event);
    LIBXSTREAM_ASSERT(slot.size < LIBXSTREAM_MAX_QSIZE);
    ++slot.size;
  }

  LIBXSTREAM_ASSERT(LIBXSTREAM_ERROR_NONE == result);
  return result;
}


libxstream_event* libxstream_stream::events()
{
  libxstream_event *const result = 0 <= m_thread ? m_slots[m_thread].events : 0;
  return result;
}


size_t libxstream_stream::nevents() const
{
  const size_t result = 0 <= m_thread ? m_slots[m_thread].size : 0;
  LIBXSTREAM_ASSERT(result <= LIBXSTREAM_MAX_QSIZE);
  return result;
}


void libxstream_stream::pending(int thread, libxstream_signal signal)
{
  LIBXSTREAM_ASSERT(0 <= thread && thread < LIBXSTREAM_MAX_NTHREADS);
  m_pending[thread] = signal;
}


libxstream_signal libxstream_stream::pending(int thread) const
{
  LIBXSTREAM_ASSERT(0 <= thread && thread < LIBXSTREAM_MAX_NTHREADS);
#if defined(LIBXSTREAM_OFFLOAD) && (0 != LIBXSTREAM_OFFLOAD) && !defined(__MIC__) && defined(LIBXSTREAM_ASYNC) && (0 != (2*LIBXSTREAM_ASYNC+1)/2) && defined(LIBXSTREAM_STREAM_CHECK_PENDING)
  const libxstream_signal lookup_signal = m_pending[thread];
  libxstream_signal signal = lookup_signal;
  if (0 != lookup_signal && 0 != _Offload_signaled(m_device, reinterpret_cast<void*>(lookup_signal))) {
    m_pending[thread] = 0;
    signal = 0;
  }
#else
  const libxstream_signal signal = m_pending[thread];
#endif
  return signal;
}


libxstream_workqueue::entry_type& libxstream_stream::enqueue(libxstream_workitem& workitem)
{
  const int thread = this_thread_id();
  libxstream_workqueue* queue = m_queues[thread];

  if (0 == queue) {
    queue = new libxstream_workqueue;
    m_queues[thread] = queue;
  }

  LIBXSTREAM_ASSERT(0 != queue);
  libxstream_workqueue::entry_type& entry = queue->allocate_entry();
  entry.push(workitem);
  return entry;
}


libxstream_workqueue* libxstream_stream::queue_begin()
{
  libxstream_workqueue* result = 0 <= m_thread ? m_queues[m_thread] : 0;

  if (0 == result || 0 == result->get().item()) {
    const int nthreads = static_cast<int>(nthreads_active());
    size_t size = result ? result->size() : 0;

    for (int i = 0; i < nthreads; ++i) {
      libxstream_workqueue *const queue = m_queues[i];
      const size_t queue_size = (0 != queue && 0 != queue->get().item()) ? queue->size() : 0;
      if (size < queue_size) {
        size = queue_size;
        result = queue;
        m_thread = i;
      }
    }

    const libxstream_workitem *const item = (0 == size && 0 != result) ? result->get().item() : 0;
    if (0 != item && 0 == (LIBXSTREAM_CALL_SYNC & item->flags())) {
      result = 0 <= m_thread ? m_queues[m_thread] : 0;
    }
  }

  return result;
}


libxstream_workqueue* libxstream_stream::queue_next()
{
  libxstream_workqueue* result = 0;

  if (0 <= m_thread) {
    const int nthreads = static_cast<int>(nthreads_active());
    const int end = m_thread + nthreads;
    for (int i = m_thread + 1; i < end; ++i) {
      const int thread = /*i % nthreads*/i < nthreads ? i : (i - nthreads);
      libxstream_workqueue *const queue = m_queues[thread];
      if (0 != queue) {
        result = queue;
        m_thread = thread;
        i = end; // break
      }
    }
  }

  return result;
}


#if defined(LIBXSTREAM_OFFLOAD) && (0 != LIBXSTREAM_OFFLOAD) && defined(LIBXSTREAM_ASYNC) && (2 == (2*LIBXSTREAM_ASYNC+1)/2)
_Offload_stream libxstream_stream::handle() const
{
  const size_t nstreams = libxstream_stream_internal::registry.nstreams(m_device);

  if (nstreams != m_npartitions) {
    if (0 != m_handle) {
      const_cast<libxstream_stream*>(this)->wait(0); // complete pending operations on old stream
      _Offload_stream_destroy(m_device, m_handle);
    }

    const int nthreads_total = omp_get_max_threads_target(TARGET_MIC, m_device) - 4/*reserved core: threads per core*/;
    const int priority_least = priority_range_least(), priority_greatest = priority_range_greatest();
    LIBXSTREAM_ASSERT(priority_greatest <= priority_least);

    int priority_least_device = priority_least, priority_greatest_device = priority_greatest;
    const size_t priority_sum = libxstream_stream_internal::registry.priority_range(m_device, priority_least_device, priority_greatest_device);
    LIBXSTREAM_ASSERT(priority_greatest_device <= priority_least_device && priority_greatest_device <= m_priority);
    const size_t priority_range_device = priority_least_device - priority_greatest_device;
    LIBXSTREAM_ASSERT(priority_sum <= priority_range_device);

    const size_t istream = libxstream_stream_internal::registry.nstreams(m_device, this); // index
    const size_t denominator = 0 == priority_range_device ? nstreams : (priority_range_device - priority_sum);
    const size_t nthreads = (0 == priority_range_device ? nthreads_total : (priority_range_device - (m_priority - priority_greatest_device))) / denominator;
    const size_t remainder = nthreads_total - nthreads * denominator;
    const int ithreads = static_cast<int>(nthreads + (istream < remainder ? 1/*imbalance*/ : 0));

    LIBXSTREAM_PRINT(3, "stream=0x%llx is mapped to %i threads", reinterpret_cast<unsigned long long>(this), ithreads);
    m_handle = _Offload_stream_create(m_device, ithreads);
    m_npartitions = nstreams;
  }

  return m_handle;
}
#endif


const libxstream_stream* cast_to_stream(const void* stream)
{
  return static_cast<const libxstream_stream*>(stream);
}


libxstream_stream* cast_to_stream(void* stream)
{
  return static_cast<libxstream_stream*>(stream);
}


const libxstream_stream* cast_to_stream(const libxstream_stream* stream)
{
  return stream;
}


libxstream_stream* cast_to_stream(libxstream_stream* stream)
{
  return stream;
}


const libxstream_stream* cast_to_stream(const libxstream_stream& stream)
{
  return &stream;
}


libxstream_stream* cast_to_stream(libxstream_stream& stream)
{
  return &stream;
}

#endif // defined(LIBXSTREAM_EXPORTED) || defined(__LIBXSTREAM)
