/*****************************************************************************
 *  CP2K: A general program to perform molecular dynamics simulations        *
 *  Copyright (C) 2000 - 2017  CP2K developers group                         *
 *****************************************************************************/

//! **************************************************************************
//!> \author Hans Pabst (Intel Corp.)
//! **************************************************************************

#if defined(__LIBXSMM) || (defined(__ACC) && defined(__ACC_MIC) && defined(__DBCSR_ACC) && defined(__LIBXSTREAM))
#include "libxsmm_acc.h"

#if defined(__ACC) && defined(__ACC_MIC) && defined(__DBCSR_ACC) && defined(__LIBXSTREAM)
# include <libxstream_begin.h>
#endif
#include <algorithm>
#include <cassert>
#include <cstdlib>
#if defined(LIBXSMM_ACC_OPENMP)
# include <omp.h>
#endif
#if defined(__ACC) && defined(__ACC_MIC) && defined(__DBCSR_ACC) && defined(__LIBXSTREAM)
# include <libxstream_end.h>
#endif

#define LIBXSMM_ACC_MAX_RESULT_SIZE (LIBXSMM_ACC_MAX_M * LIBXSMM_ACC_MAX_N)

LIBXSMM_ACC_EXTERN LIBXSMM_ACC_RETARGETABLE void LIBXSMM_ACC_FSYMBOL(dgemm)(
  const char*, const char*, const int*, const int*, const int*,
  const double*, const double*, const int*, const double*, const int*,
  const double*, double*, const int*);
LIBXSMM_ACC_EXTERN LIBXSMM_ACC_RETARGETABLE void LIBXSMM_ACC_FSYMBOL(sgemm)(
  const char*, const char*, const int*, const int*, const int*,
  const float*, const float*, const int*, const float*, const int*,
  const float*, float*, const int*);

#if defined(CP2K_CONFIG_PREFIX) && defined(LIBXSMM_ACC_SORT)
LIBXSMM_ACC_EXTERN void LIBXSMM_ACC_FSYMBOL(LIBXSMM_ACC_CONCATENATE(CP2K_CONFIG_PREFIX, dbcsr_get_default_config))(LIBXSMM_ACC_CONFIG_SIGNATURE_GETTER);
#endif


namespace libxsmm_process_private {

/**
 * Below lock mechanism is only needed if processing matrix-stack is parallelized by itself.
 * This might be due to CP2K's ACCeleration layer, or due to a future implementation where
 * the (CPU-)code path allows for nested parallelism.
 */
#if defined(LIBXSMM_ACC_OPENMP) && defined(LIBXSMM_ACC_SYNCHRONIZATION) && (1 < (LIBXSMM_ACC_SYNCHRONIZATION))
LIBXSMM_ACC_RETARGETABLE class LIBXSMM_ACC_RETARGETABLE lock_type {
public:
  lock_type() {
    for (int i = 0; i < (LIBXSMM_ACC_SYNCHRONIZATION); ++i) omp_init_lock(m_lock + i);
  }
  ~lock_type() {
    for (int i = 0; i < (LIBXSMM_ACC_SYNCHRONIZATION); ++i) omp_destroy_lock(m_lock + i);
  }

public:
  void acquire(const void* address) {
    omp_set_lock(m_lock + LIBXSMM_ACC_HASH2(address, LIBXSMM_ACC_ALIGNMENT, LIBXSMM_ACC_SYNCHRONIZATION));
  }
  void release(const void* address) {
    omp_unset_lock(m_lock + LIBXSMM_ACC_HASH2(address, LIBXSMM_ACC_ALIGNMENT, LIBXSMM_ACC_SYNCHRONIZATION));
  }

private:
  omp_lock_t m_lock[LIBXSMM_ACC_SYNCHRONIZATION];
} lock;
#endif


/**
 * This functor provides all services needed to perform a (Small)Matrix-Matrix multiplication.
 * The primary purpose is to uniform the different function signatures provided of the different
 * internal implementations. In particular it allows to operate LIBXSMM's SMM, but also providing
 * a fall-back in case LIBXSMM is not present. Further if LIBXSMM is present, the internal
 * implementation might be (pre-)dispatched depending on attributes of the matrix-stack (def_mnk).
 */
template<typename T, typename U>
class LIBXSMM_ACC_RETARGETABLE smm_type {
private:
#if defined(__LIBXSMM)
  typedef libxsmm_mmfunction<T> xfunc_type;
#else
  typedef const void* xfunc_type; // dummy
#endif

  mutable/*offload attribute*/ xfunc_type m_predispatched;
  void (*m_function)(const xfunc_type&, U, U, U, U,
    const T *LIBXSMM_ACC_RESTRICT, const T *LIBXSMM_ACC_RESTRICT, T *LIBXSMM_ACC_RESTRICT,
    const T*, const T*, const T*);

public:
  smm_type(U def_mnk, U m, U n, U k)
#if defined(__LIBXSMM)
    : m_predispatched((0 != def_mnk && (LIBXSMM_MAX_MNK) >= (m * n * k))
      ? xfunc_type(LIBXSMM_FLAGS, m, n, k, T(1)/*alpha*/, T(1)/*beta*/, LIBXSMM_PREFETCH)
      : xfunc_type())
# if defined(__LIBXSMM) && LIBXSMM_VERSION4(1, 8, 1, 951) <= LIBXSMM_VERSION4(LIBXSMM_VERSION_MAJOR, LIBXSMM_VERSION_MINOR, \
                                                                             LIBXSMM_VERSION_UPDATE, LIBXSMM_VERSION_PATCH)
    , m_function(0 != kernel().xmm ? smm_type::xmm/*(pre-)dispatched*/
# else
    , m_function(m_predispatched ? smm_type::xmm/*(pre-)dispatched*/
# endif
      : ((LIBXSMM_MAX_MNK) >= (m * n * k) ? smm_type::amm : smm_type::bmm))
#else
    : m_predispatched(0), m_function(smm_type::bmm/*fall-back if no LIBXSMM is present*/)
#endif
  {
    LIBXSMM_ACC_ASSERT(m_function);
  }

public:
#if defined(__LIBXSMM) && LIBXSMM_VERSION4(1, 8, 1, 951) <= LIBXSMM_VERSION4(LIBXSMM_VERSION_MAJOR, LIBXSMM_VERSION_MINOR, \
                                                                             LIBXSMM_VERSION_UPDATE, LIBXSMM_VERSION_PATCH)
  libxsmm_xmmfunction kernel() const { return m_predispatched.kernel(); }
  bool efficient() const { return 0 != kernel().xmm; }
#else
  bool efficient() const { return 0 != m_predispatched; }
#endif

  void zero_c(T *LIBXSMM_ACC_RESTRICT c, U size) const {
    LIBXSMM_ACC_PRAGMA_LOOP_COUNT(1, LIBXSMM_ACC_LOOP_MAX_M*LIBXSMM_ACC_LOOP_MAX_N, LIBXSMM_ACC_LOOP_AVG_M*LIBXSMM_ACC_LOOP_AVG_N)
    for (U i = 0; i < size; ++i) c[i] = 0;
  }

  void copy_c(const T *LIBXSMM_ACC_RESTRICT c, T *LIBXSMM_ACC_RESTRICT out, U m, U n, U ldc) const {
#if defined(LIBXSMM_ACC_OPENMP) && defined(LIBXSMM_ACC_SYNCHRONIZATION) && (0 < (LIBXSMM_ACC_SYNCHRONIZATION))
# if (1 == (LIBXSMM_ACC_SYNCHRONIZATION))
#   pragma omp critical(libxsmm_process)
# else
    lock.acquire(out);
# endif
#endif
    {
      for (U j = 0; j < n; ++j) {
        LIBXSMM_ACC_PRAGMA_LOOP_COUNT(1, LIBXSMM_ACC_LOOP_MAX_M, LIBXSMM_ACC_LOOP_AVG_M)
        for (U i = 0; i < m; ++i) {
          const T value = c[j*ldc+i];
#if defined(LIBXSMM_ACC_OPENMP) && (!defined(LIBXSMM_ACC_SYNCHRONIZATION) || (0 == (LIBXSMM_ACC_SYNCHRONIZATION)))
#         pragma omp atomic
#endif
          out[j*m+i] += value;
        }
      }
    }
#if defined(LIBXSMM_ACC_OPENMP) && defined(LIBXSMM_ACC_SYNCHRONIZATION) && (1 < (LIBXSMM_ACC_SYNCHRONIZATION))
    lock.release(out);
#endif
  }

  void operator()(U m, U n, U k, U ldc,
    const T *LIBXSMM_ACC_RESTRICT a, const T *LIBXSMM_ACC_RESTRICT b, T *LIBXSMM_ACC_RESTRICT c,
    const T* pa = 0, const T* pb = 0, const T* pc = 0) const
  {
    m_function(m_predispatched, m, n, k, ldc, a, b, c, pa, pb, pc);
  }

private:
#if defined(__LIBXSMM)
  LIBXSMM_ACC_RETARGETABLE static void xmm(const xfunc_type& xfunc, U /*m*/, U /*n*/, U /*k*/, U /*ldc*/,
    const T *LIBXSMM_ACC_RESTRICT a, const T *LIBXSMM_ACC_RESTRICT b, T *LIBXSMM_ACC_RESTRICT c,
    const T* pa, const T* pb, const T* pc)
  {
    LIBXSMM_ACC_ASSERT(xfunc);
# if (0 != LIBXSMM_PREFETCH || 0 != LIBXSMM_JIT)
    xfunc(a, b, c, pa, pb, pc);
# else
    LIBXSMM_ACC_UNUSED(pa);
    LIBXSMM_ACC_UNUSED(pb);
    LIBXSMM_ACC_UNUSED(pc);
    xfunc(a, b, c);
# endif
  }

  LIBXSMM_ACC_RETARGETABLE static void amm(const xfunc_type& /*xfunc*/, U m, U n, U k, U ldc,
    const T *LIBXSMM_ACC_RESTRICT a, const T *LIBXSMM_ACC_RESTRICT b, T *LIBXSMM_ACC_RESTRICT c,
    const T* pa, const T* pb, const T* pc)
  {
    LIBXSMM_ACC_ASSERT((LIBXSMM_MAX_MNK) >= (m * n * k));
    const xfunc_type xfunc(LIBXSMM_FLAGS, m, n, k, T(1)/*alpha*/, T(1)/*beta*/, LIBXSMM_PREFETCH);
    if (xfunc) {
# if (0 != LIBXSMM_PREFETCH || 0 != LIBXSMM_JIT)
      xfunc(a, b, c, pa, pb, pc);
# else
      LIBXSMM_ACC_UNUSED(pa);
      LIBXSMM_ACC_UNUSED(pb);
      LIBXSMM_ACC_UNUSED(pc);
      xfunc(a, b, c);
# endif
    }
    else {
      blasmm(m, n, k, ldc, a, b, c);
    }
  }
#endif

  LIBXSMM_ACC_RETARGETABLE static void bmm(const xfunc_type& /*xfunc*/, U m, U n, U k, U ldc,
    const T *LIBXSMM_ACC_RESTRICT a, const T *LIBXSMM_ACC_RESTRICT b, T *LIBXSMM_ACC_RESTRICT c,
    const T* /*pa*/, const T* /*pb*/, const T* /*pc*/)
  {
    blasmm(m, n, k, ldc, a, b, c);
  }

  static void blasmm(U m, U n, U k, U ldc, const float* a, const float* b, float* c) {
    static float alpha = 1.f, beta = 1.f;
    static char trans = 'N';
    int im = static_cast<int>(m), in = static_cast<int>(n), ik = static_cast<int>(k), ildc = static_cast<int>(ldc);
#if defined(__LIBXSMM) && LIBXSMM_VERSION4(1, 5, 0, 0) <= LIBXSMM_VERSION4(LIBXSMM_VERSION_MAJOR, LIBXSMM_VERSION_MINOR, \
                                                                           LIBXSMM_VERSION_UPDATE, LIBXSMM_VERSION_PATCH)
    // original symbol, which avoids to go through the wrapper
# if LIBXSMM_VERSION4(1, 8, 1, 1113) <= LIBXSMM_VERSION4(LIBXSMM_VERSION_MAJOR, LIBXSMM_VERSION_MINOR, \
                                                         LIBXSMM_VERSION_UPDATE, LIBXSMM_VERSION_PATCH)
    LIBXSMM_ORIGINAL_GEMM(float)
# else
    LIBXSMM_BLAS_GEMM_SYMBOL(float)
# endif
#else
    LIBXSMM_ACC_FSYMBOL(sgemm)
#endif
      (&trans, &trans, &im, &in, &ik, &alpha, const_cast<float*>(a), &im, const_cast<float*>(b), &ik, &beta, c, &ildc);
  }

  static void blasmm(U m, U n, U k, U ldc, const double* a, const double* b, double* c) {
    static double alpha = 1.0, beta = 1.0;
    static char trans = 'N';
    int im = static_cast<int>(m), in = static_cast<int>(n), ik = static_cast<int>(k), ildc = static_cast<int>(ldc);
#if defined(__LIBXSMM) && LIBXSMM_VERSION4(1, 5, 0, 0) <= LIBXSMM_VERSION4(LIBXSMM_VERSION_MAJOR, LIBXSMM_VERSION_MINOR, \
                                                                           LIBXSMM_VERSION_UPDATE, LIBXSMM_VERSION_PATCH)
    // original symbol, which avoids to go through the wrapper
# if LIBXSMM_VERSION4(1, 8, 1, 1113) <= LIBXSMM_VERSION4(LIBXSMM_VERSION_MAJOR, LIBXSMM_VERSION_MINOR, \
                                                         LIBXSMM_VERSION_UPDATE, LIBXSMM_VERSION_PATCH)
    LIBXSMM_ORIGINAL_GEMM(double)
# else
    LIBXSMM_BLAS_GEMM_SYMBOL(double)
# endif
#else
    LIBXSMM_ACC_FSYMBOL(dgemm)
#endif
      (&trans, &trans, &im, &in, &ik, &alpha, const_cast<double*>(a), &im, const_cast<double*>(b), &ik, &beta, c, &ildc);
  }
};


template<size_t N, typename T, typename U>
LIBXSMM_ACC_RETARGETABLE void work(const U *LIBXSMM_ACC_RESTRICT stack, size_t stacksize, const smm_type<T,U>& smm,
  const T *LIBXSMM_ACC_RESTRICT a, const T *LIBXSMM_ACC_RESTRICT b, T *LIBXSMM_ACC_RESTRICT c)
{
#if defined(__LIBXSMM) && LIBXSMM_VERSION4(1, 8, 1, 1169) <= LIBXSMM_VERSION4(LIBXSMM_VERSION_MAJOR, LIBXSMM_VERSION_MINOR, \
                                                                              LIBXSMM_VERSION_UPDATE, LIBXSMM_VERSION_PATCH)
  if (0 != smm.kernel().xmm) {
    const libxsmm_blasint *const params = reinterpret_cast<const libxsmm_blasint*>(stack);
    LIBXSMM_ACC_ASSERT(sizeof(U) == sizeof(libxsmm_blasint));
    libxsmm_mmbatch(smm.kernel(), 1/*index_base*/, (LIBXSMM_ACC_NPARAMS) * sizeof(libxsmm_blasint),
      params + LIBXSMM_ACC_PARAM_A, params + LIBXSMM_ACC_PARAM_B, params + LIBXSMM_ACC_PARAM_C,
      a, b, c, static_cast<libxsmm_blasint>(stacksize), 0/*tid*/, 1/*nthreads*/);
  }
  else
#endif
  {
    const int nstacksize = static_cast<int>(stacksize * N);
#if defined(LIBXSMM_ACC_OPENMP)
# pragma omp parallel for LIBXSMM_ACC_SCHEDULE
#endif
    for (int s = 0; s < nstacksize;
#if (defined(LIBXSMM_ACC_NLOCAL) && (1 < (LIBXSMM_ACC_NLOCAL)))
      s += ((LIBXSMM_ACC_NLOCAL) * N))
#else
      s += N)
#endif
    {
#if (defined(LIBXSMM_ACC_NLOCAL) && (1 < (LIBXSMM_ACC_NLOCAL)))
      T tmp[LIBXSMM_ACC_MAX_RESULT_SIZE];
#endif
      U current[LIBXSMM_ACC_PARAM_COUNT], next[LIBXSMM_ACC_PARAM_COUNT];
      U *pcur = current, *pnxt = next, i = s;
      for (U j = 0; j < LIBXSMM_ACC_PARAM_COUNT; ++j) current[j] = stack[s+j];
#if (defined(LIBXSMM_ACC_NLOCAL) && (1 < (LIBXSMM_ACC_NLOCAL)))
      const int end = s + std::min(static_cast<int>((LIBXSMM_ACC_NLOCAL) * N), nstacksize - s);
      do
#endif
      {
        const U ldc = pcur[LIBXSMM_ACC_PARAM_M];
        T *const ci = c + pcur[LIBXSMM_ACC_PARAM_C] - 1;
#if (defined(LIBXSMM_ACC_NLOCAL) && (1 < (LIBXSMM_ACC_NLOCAL)))
        smm.zero_c(tmp, ldc * pcur[LIBXSMM_ACC_PARAM_N]);
#else
        T *const tmp = ci;
#endif
#if (defined(LIBXSMM_ACC_NLOCAL) && (1 < (LIBXSMM_ACC_NLOCAL)))
        for (;;)
#endif
        {
          i += N; // next
          if (i < nstacksize) {
            for (U j = 0; j < LIBXSMM_ACC_PARAM_COUNT; ++j) pnxt[j] = stack[i+j];
#if defined(__LIBXSMM) && (0 != LIBXSMM_PREFETCH || 0 != LIBXSMM_JIT)
            smm(pcur[LIBXSMM_ACC_PARAM_M], pcur[LIBXSMM_ACC_PARAM_N], pcur[LIBXSMM_ACC_PARAM_K], ldc,
              a + pcur[LIBXSMM_ACC_PARAM_A] - 1, b + pcur[LIBXSMM_ACC_PARAM_B] - 1, tmp,
              LIBXSMM_PREFETCH_A(a + pnxt[LIBXSMM_ACC_PARAM_A] - 1),
              LIBXSMM_PREFETCH_B(b + pnxt[LIBXSMM_ACC_PARAM_B] - 1),
              LIBXSMM_PREFETCH_C(ci));
#else
            smm(pcur[LIBXSMM_ACC_PARAM_M], pcur[LIBXSMM_ACC_PARAM_N], pcur[LIBXSMM_ACC_PARAM_K], ldc,
              a + pcur[LIBXSMM_ACC_PARAM_A] - 1, b + pcur[LIBXSMM_ACC_PARAM_B] - 1, tmp);
#endif
#if (defined(LIBXSMM_ACC_NLOCAL) && (1 < (LIBXSMM_ACC_NLOCAL)))
            if (pcur[LIBXSMM_ACC_PARAM_C] != pnxt[LIBXSMM_ACC_PARAM_C] || end <= i) {
              break;
            }
            std::swap(pcur, pnxt);
#endif
          }
          else { // last element of the matrix-stack (there is no next element to be prefetched)
            const T *const ai = a + pcur[LIBXSMM_ACC_PARAM_A] - 1, *const bi = b + pcur[LIBXSMM_ACC_PARAM_B] - 1;
            smm(pcur[LIBXSMM_ACC_PARAM_M], pcur[LIBXSMM_ACC_PARAM_N], pcur[LIBXSMM_ACC_PARAM_K], ldc, ai, bi, tmp);
#if (defined(LIBXSMM_ACC_NLOCAL) && (1 < (LIBXSMM_ACC_NLOCAL)))
            break;
#endif
          }
        }
#if (defined(LIBXSMM_ACC_NLOCAL) && (1 < (LIBXSMM_ACC_NLOCAL)))
        smm.copy_c(tmp, ci, pcur[LIBXSMM_ACC_PARAM_M], pcur[LIBXSMM_ACC_PARAM_N], ldc);
#endif
        std::swap(pcur, pnxt);
      }
#if (defined(LIBXSMM_ACC_NLOCAL) && (1 < (LIBXSMM_ACC_NLOCAL)))
      while (i < end);
#endif
    }
  }
}


#if defined(CP2K_CONFIG_PREFIX) && defined(LIBXSMM_ACC_SORT)
class LIBXSMM_ACC_RETARGETABLE sort_type {
public:
  sort_type(): binsize(static_cast<size_t>(-1)), nbins(static_cast<size_t>(-1)) {
    const char *const env = getenv("CP2K_SORT");
    if (env && *env && 0 != atoi(env)) {
      LIBXSMM_ACC_CONFIG_SIGNATURE_VALS;
      LIBXSMM_ACC_FSYMBOL(LIBXSMM_ACC_CONCATENATE(CP2K_CONFIG_PREFIX, dbcsr_get_default_config))(LIBXSMM_ACC_CONFIG_SIGNATURE_CALL);
      binsize = static_cast<size_t>(accdrv_binning_binsize);
      nbins = static_cast<size_t>(accdrv_binning_nbins);
    }
    LIBXSMM_ACC_ASSERT((static_cast<size_t>(-1) == binsize && static_cast<size_t>(-1) == nbins) ||
                       (static_cast<size_t>(-1) != binsize && static_cast<size_t>(-1) != nbins));
  }
  void operator()(libxsmm_acc_param_type* stack, size_t stacksize) const {
    static const size_t invalid = static_cast<size_t>(-1);
    if (invalid != binsize) {
      LIBXSMM_ACC_ASSERT(invalid != nbins);
      run(stack, stacksize, binsize, nbins);
    }
  }

private:
  static bool less(const libxsmm_acc_param_type& a, const libxsmm_acc_param_type& b) {
    return a.component.ic < b.component.ic;
  }
  static void run(libxsmm_acc_param_type* stack, size_t stacksize, size_t min_grain, size_t max_nbins) {
    const size_t size2 = (stacksize >> 1), nbins2 = (max_nbins >> 1);
    libxsmm_acc_param_type *const stack2 = stack + size2;
    std::nth_element(stack, stack2, stack + stacksize, less);
    if (min_grain < size2 && 1 < nbins2) {
      run(stack, size2, min_grain, nbins2);
      run(stack2, stacksize - size2, min_grain, nbins2);
    }
  }
  size_t binsize, nbins;
} sort;


#endif /*defined(CP2K_CONFIG_PREFIX) && defined(LIBXSMM_ACC_SORT)*/


template<size_t N, typename T, typename U>
LIBXSMM_ACC_RETARGETABLE void context(/*const*/ U* stack, const U* stacksize, const U* def_mnk,
  const U* max_m, const U* max_n, const U* max_k, const T* a, const T* b, T* c,
  U* efficient/*Boolean*/)
{
  const smm_type<T,U> smm(*def_mnk, *max_m, *max_n, *max_k);
  LIBXSMM_ACC_ASSERT(((LIBXSMM_ACC_NPARAMS) * sizeof(int)) == sizeof(libxsmm_acc_param_type));
#if defined(CP2K_CONFIG_PREFIX) && defined(LIBXSMM_ACC_SORT)
  sort(reinterpret_cast<libxsmm_acc_param_type*>(stack), *stacksize);
#endif
  work<LIBXSMM_ACC_NPARAMS,T,U>(stack, *stacksize, smm, a, b, c);
  if (efficient) *efficient = static_cast<int>(smm.efficient());
}


template<typename T, bool Complex, typename U>
int process(/*const*/ U* stack, U stacksize, U nparams, U def_mnk,
  U max_m, U max_n, U max_k, const void* a_data, const void* b_data, void* c_data,
  void* stream_or_boolean)
{
  LIBXSMM_ACC_CHECK_CONDITION(
    0 != stack && 0 <= stacksize && LIBXSMM_ACC_NPARAMS == nparams &&
    0 <= max_m && 0 <= max_n && 0 <= max_k &&
    0 != a_data && 0 != b_data && 0 != c_data);
#if defined(__ACC) && defined(__ACC_MIC) && defined(__DBCSR_ACC) && defined(__LIBXSTREAM)
  const size_t shape = stacksize;
  libxstream_argument* signature = 0;
  LIBXSMM_ACC_CHECK_CALL_ASSERT(libxstream_fn_signature(&signature));
  LIBXSMM_ACC_CHECK_CALL_ASSERT(libxstream_fn_input (signature, 0,      stack, libxstream_map_to<U>::type(), 1, &shape));
  LIBXSMM_ACC_CHECK_CALL_ASSERT(libxstream_fn_input (signature, 1, &stacksize, libxstream_map_to<U>::type(), 0, 0));
  LIBXSMM_ACC_CHECK_CALL_ASSERT(libxstream_fn_input (signature, 2,   &def_mnk, libxstream_map_to<U>::type(), 0, 0));
  LIBXSMM_ACC_CHECK_CALL_ASSERT(libxstream_fn_input (signature, 3,     &max_m, libxstream_map_to<U>::type(), 0, 0));
  LIBXSMM_ACC_CHECK_CALL_ASSERT(libxstream_fn_input (signature, 4,     &max_n, libxstream_map_to<U>::type(), 0, 0));
  LIBXSMM_ACC_CHECK_CALL_ASSERT(libxstream_fn_input (signature, 5,     &max_k, libxstream_map_to<U>::type(), 0, 0));
  LIBXSMM_ACC_CHECK_CALL_ASSERT(libxstream_fn_input (signature, 6,     a_data, libxstream_map_to<T>::type(), 1, 0/*unknown*/));
  LIBXSMM_ACC_CHECK_CALL_ASSERT(libxstream_fn_input (signature, 7,     b_data, libxstream_map_to<T>::type(), 1, 0/*unknown*/));
  LIBXSMM_ACC_CHECK_CALL_ASSERT(libxstream_fn_inout (signature, 8,     c_data, libxstream_map_to<T>::type(), 1, 0/*unknown*/));
  LIBXSMM_ACC_CHECK_CALL_ASSERT(libxstream_fn_output(signature, 9,          0, libxstream_map_to<U>::type(), 0, 0));
  const libxstream_function libxsmm_process_function = reinterpret_cast<libxstream_function>(context<LIBXSMM_ACC_NPARAMS,T,U>);
  LIBXSMM_ACC_CHECK_CALL_ASSERT(libxstream_fn_call(libxsmm_process_function, signature,
    static_cast<libxstream_stream*>(stream_or_boolean), LIBXSTREAM_CALL_DEFAULT));
#else
  context<LIBXSMM_ACC_NPARAMS>(stack, &stacksize, &def_mnk, &max_m, &max_n, &max_k,
    static_cast<const T*>(a_data), static_cast<const T*>(b_data), static_cast<T*>(c_data),
    static_cast<U*>(stream_or_boolean));
#endif
  return LIBXSMM_ACC_ERROR_NONE;
}

} // namespace libxsmm_process_private

#if defined(__ACC) && defined(__ACC_MIC) && defined(__DBCSR_ACC) && defined(__LIBXSTREAM)
// workaround for issue "cannot find address of function" (use unoptimized build or apply mic attribute globally)
const libxstream_function libxsmm_process_function = reinterpret_cast<libxstream_function>(libxsmm_process_private::context<LIBXSMM_ACC_NPARAMS,double,int>);
#endif


extern "C" int libsmm_acc_process(void* param_stack, int stacksize, int nparams, int datatype, void* a_data, void* b_data, void* c_data, int max_m, int max_n, int max_k, int def_mnk, void* stream_or_boolean)
{
#if defined(LIBXSMM_ACC_PRETRANSPOSE)
  LIBXSMM_ACC_ASSERT(false/*TODO: implement C = A * B which is assuming that B is (pre-)transposed (B^T).*/);
#endif
#if defined(CP2K_CONFIG_PREFIX) && defined(__ACC) && defined(__ACC_MIC) && defined(__DBCSR_ACC) && defined(__LIBXSTREAM)
  const int mflops = 0 != stream_or_boolean ? static_cast<int>(2E-6 * stacksize * max_m * max_n * max_k + 0.5) : LIBXSMM_ACC_ACCDRV_MIN_MFLOPS_PERSTACK;
  int result = (LIBXSMM_ACC_ACCDRV_MIN_MFLOPS_PERSTACK) <= mflops ? LIBXSMM_ACC_ERROR_NONE : LIBXSMM_ACC_NOT_SUPPORTED;
#else
  int result = LIBXSMM_ACC_ERROR_NONE;
#endif
  if (LIBXSMM_ACC_ERROR_NONE == result) {
    int *const stack = static_cast<int*>(param_stack);
    switch(static_cast<libxsmm_acc_elem_type>(datatype)) {
      case LIBXSMM_ACC_ELEM_F32: {
#if defined(__ACC) && defined(__ACC_MIC) && defined(__DBCSR_ACC) && defined(__LIBXSTREAM)
        result = LIBXSMM_ACC_NOT_SUPPORTED; // none of the other ACC/Offload-layers seems to handle single precision
#else // non-offload use case
        result = libxsmm_process_private::process<float,false>(stack, stacksize, nparams, def_mnk, max_m, max_n, max_k, a_data, b_data, c_data, stream_or_boolean);
#endif
      } break;
      case LIBXSMM_ACC_ELEM_F64: {
        result = libxsmm_process_private::process<double,false>(stack, stacksize, nparams, def_mnk, max_m, max_n, max_k, a_data, b_data, c_data, stream_or_boolean);
      } break;
      case LIBXSMM_ACC_ELEM_C32: {
        result = LIBXSMM_ACC_NOT_SUPPORTED;
      } break;
      case LIBXSMM_ACC_ELEM_C64: {
        result = LIBXSMM_ACC_NOT_SUPPORTED;
      } break;
      default:
        result = LIBXSMM_ACC_ERROR_CONDITION;
    }
  }
  return result;
}

#endif // defined(__LIBXSMM) || (defined(__ACC) && defined(__ACC_MIC) && defined(__DBCSR_ACC) && defined(__LIBXSTREAM))
