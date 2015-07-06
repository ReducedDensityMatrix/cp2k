#if (0 != LIBXSMM_ROW_MAJOR)
# define LIBXSMM_LD(M, N) N
#else
# define LIBXSMM_LD(M, N) M
#endif
#if (1 < LIBXSMM_ALIGNED_STORES)
# define LIBXSMM_ASSUME_ALIGNED_STORES(A) LIBXSMM_ASSUME_ALIGNED(A, LIBXSMM_ALIGNED_STORES)
# define LIBXSMM_LDC(REAL, UINT, M, N) LIBXSMM_ALIGN_VALUE(UINT, REAL, LIBXSMM_LD(M, N), LIBXSMM_ALIGNED_STORES)
#else
# define LIBXSMM_ASSUME_ALIGNED_STORES(A)
# define LIBXSMM_LDC(REAL, UINT, M, N) LIBXSMM_LD(M, N)
#endif
#if (1 < LIBXSMM_ALIGNED_LOADS)
# define LIBXSMM_ASSUME_ALIGNED_LOADS(A) LIBXSMM_ASSUME_ALIGNED(A, LIBXSMM_ALIGNED_LOADS)
#else
# define LIBXSMM_ASSUME_ALIGNED_LOADS(A)
#endif

#define LIBXSMM_MAX_SIMD LIBXSMM_MAX(LIBXSMM_ALIGNED_MAX / sizeof(float), 1)
#define LIBXSMM_MAX_SIZE LIBXSMM_MAX(LIBXSMM_MAX( \
  LIBXSMM_LD(LIBXSMM_MAX_M, LIBXSMM_MAX_K) * LIBXSMM_UP(LIBXSMM_LD(LIBXSMM_MAX_K, LIBXSMM_MAX_M), LIBXSMM_MAX_SIMD),  \
  LIBXSMM_LD(LIBXSMM_MAX_K, LIBXSMM_MAX_N) * LIBXSMM_UP(LIBXSMM_LD(LIBXSMM_MAX_N, LIBXSMM_MAX_K), LIBXSMM_MAX_SIMD)), \
  LIBXSMM_LD(LIBXSMM_MAX_M, LIBXSMM_MAX_N) * LIBXSMM_UP(LIBXSMM_LD(LIBXSMM_MAX_N, LIBXSMM_MAX_M), LIBXSMM_MAX_SIMD))

#define LIBXSMM_BLASMM(REAL, UINT, M, N, K, A, B, C) { \
  UINT libxsmm_m_ = LIBXSMM_LD(M, N), libxsmm_n_ = LIBXSMM_LD(N, M), libxsmm_k_ = (K); \
  UINT libxsmm_ldc_ = LIBXSMM_LDC(REAL, UINT, M, N); \
  REAL libxsmm_alpha_ = 1, libxsmm_beta_ = 1; \
  char libxsmm_trans_ = 'N'; \
  LIBXSMM_FSYMBOL(LIBXSMM_BLASPREC(, REAL, gemm))(&libxsmm_trans_, &libxsmm_trans_, \
    &libxsmm_m_, &libxsmm_n_, &libxsmm_k_, \
    &libxsmm_alpha_, (REAL*)LIBXSMM_LD(A, B), &libxsmm_m_, (REAL*)LIBXSMM_LD(B, A), &libxsmm_k_, \
    &libxsmm_beta_, (C), &libxsmm_ldc_); \
}

#if defined(MKL_DIRECT_CALL_SEQ) || defined(MKL_DIRECT_CALL)
# define LIBXSMM_IMM(REAL, UINT, M, N, K, A, B, C) LIBXSMM_BLASMM(REAL, UINT, M, N, K, A, B, C)
#else
# define LIBXSMM_IMM(REAL, UINT, M, N, K, A, B, C) { \
    const REAL *const libxsmm_a_ = LIBXSMM_LD(B, A), *const libxsmm_b_ = LIBXSMM_LD(A, B); \
    const UINT libxsmm_ldc_ = LIBXSMM_LDC(REAL, UINT, M, N); \
    UINT libxsmm_i_, libxsmm_j_, libxsmm_k_; \
    REAL *const libxsmm_c_ = (C); \
    LIBXSMM_ASSUME_ALIGNED_STORES(libxsmm_c_); \
    /*TODO: LIBXSMM_ASSUME_ALIGNED_LOADS(libxsmm_a_);*/ \
    /*TODO: LIBXSMM_ASSUME_ALIGNED_LOADS(libxsmm_b_);*/ \
    LIBXSMM_PRAGMA_SIMD_COLLAPSE(2) \
    for (libxsmm_j_ = 0; libxsmm_j_ < LIBXSMM_LD(M, N); ++libxsmm_j_) { \
      LIBXSMM_PRAGMA_LOOP_COUNT(1, LIBXSMM_LD(LIBXSMM_MAX_N, LIBXSMM_MAX_M), LIBXSMM_LD(LIBXSMM_AVG_N, LIBXSMM_AVG_M)) \
      for (libxsmm_i_ = 0; libxsmm_i_ < LIBXSMM_LD(N, M); ++libxsmm_i_) { \
        const UINT libxsmm_index_ = libxsmm_i_ * libxsmm_ldc_ + libxsmm_j_; \
        REAL libxsmm_r_ = libxsmm_c_[libxsmm_index_]; \
        LIBXSMM_PRAGMA_SIMD_REDUCTION(+:libxsmm_r_) \
        LIBXSMM_PRAGMA_UNROLL \
        for (libxsmm_k_ = 0; libxsmm_k_ < (K); ++libxsmm_k_) { \
          libxsmm_r_ += libxsmm_a_[libxsmm_i_*(K)+libxsmm_k_] * libxsmm_b_[libxsmm_k_*LIBXSMM_LD(M,N)+libxsmm_j_]; \
        } \
        libxsmm_c_[libxsmm_index_] = libxsmm_r_; \
      } \
    } \
  }
#endif

/**
 * Execute a generated function, inlined code, or fall back to the linked LAPACK implementation.
 * If M, N, and K does not change for multiple calls, it is more efficient to query and reuse
 * the function pointer (libxsmm_?mm_dispatch).
 */
#define LIBXSMM_MM(REAL, M, N, K, A, B, C) \
  if ((LIBXSMM_MAX_MNK) >= ((M) * (N) * (K))) { \
    const LIBXSMM_BLASPREC(libxsmm_, REAL, mm_function) libxsmm_mm_function_ = \
      LIBXSMM_BLASPREC(libxsmm_, REAL, mm_dispatch)((M), (N), (K)); \
    if (libxsmm_mm_function_) { \
      libxsmm_mm_function_((A), (B), (C)); \
    } \
    else { \
      LIBXSMM_IMM(REAL, int, M, N, K, A, B, C); \
    } \
  } \
  else { \
    LIBXSMM_BLASMM(REAL, int, M, N, K, A, B, C); \
  }

/** Type of a function generated for a specific M, N, and K. */
typedef LIBXSMM_TARGET(mic) void (*libxsmm_smm_function)(const float*, const float*, float*);
typedef LIBXSMM_TARGET(mic) void (*libxsmm_dmm_function)(const double*, const double*, double*);

/** Query the pointer of a generated function; zero if it does not exist. */
LIBXSMM_EXTERN_C LIBXSMM_TARGET(mic) libxsmm_smm_function libxsmm_smm_dispatch(int m, int n, int k);
LIBXSMM_EXTERN_C LIBXSMM_TARGET(mic) libxsmm_dmm_function libxsmm_dmm_dispatch(int m, int n, int k);

/** Dispatched matrix-matrix multiplication; single-precision. */
LIBXSMM_INLINE LIBXSMM_TARGET(mic) void libxsmm_smm(int m, int n, int k, const float *LIBXSMM_RESTRICT a, const float *LIBXSMM_RESTRICT b, float *LIBXSMM_RESTRICT c) {
  LIBXSMM_MM(float, m, n, k, a, b, c);
}

/** Dispatched matrix-matrix multiplication; double-precision. */
LIBXSMM_INLINE LIBXSMM_TARGET(mic) void libxsmm_dmm(int m, int n, int k, const double *LIBXSMM_RESTRICT a, const double *LIBXSMM_RESTRICT b, double *LIBXSMM_RESTRICT c) {
  LIBXSMM_MM(double, m, n, k, a, b, c);
}

/** Non-dispatched matrix-matrix multiplication using inline code; single-precision. */
LIBXSMM_INLINE LIBXSMM_TARGET(mic) void libxsmm_simm(int m, int n, int k, const float *LIBXSMM_RESTRICT a, const float *LIBXSMM_RESTRICT b, float *LIBXSMM_RESTRICT c) {
  LIBXSMM_IMM(float, int, m, n, k, a, b, c);
}

/** Non-dispatched matrix-matrix multiplication using inline code; double-precision. */
LIBXSMM_INLINE LIBXSMM_TARGET(mic) void libxsmm_dimm(int m, int n, int k, const double *LIBXSMM_RESTRICT a, const double *LIBXSMM_RESTRICT b, double *LIBXSMM_RESTRICT c) {
  LIBXSMM_IMM(double, int, m, n, k, a, b, c);
}

/** Non-dispatched matrix-matrix multiplication using BLAS; single-precision. */
LIBXSMM_INLINE LIBXSMM_TARGET(mic) void libxsmm_sblasmm(int m, int n, int k, const float *LIBXSMM_RESTRICT a, const float *LIBXSMM_RESTRICT b, float *LIBXSMM_RESTRICT c) {
  LIBXSMM_BLASMM(float, int, m, n, k, a, b, c);
}

/** Non-dispatched matrix-matrix multiplication using BLAS; double-precision. */
LIBXSMM_INLINE LIBXSMM_TARGET(mic) void libxsmm_dblasmm(int m, int n, int k, const double *LIBXSMM_RESTRICT a, const double *LIBXSMM_RESTRICT b, double *LIBXSMM_RESTRICT c) {
  LIBXSMM_BLASMM(double, int, m, n, k, a, b, c);
}

#if defined(__cplusplus)

/** Dispatched matrix-matrix multiplication. */
LIBXSMM_TARGET(mic) inline void libxsmm_mm(int m, int n, int k, const float *LIBXSMM_RESTRICT a, const float *LIBXSMM_RESTRICT b, float *LIBXSMM_RESTRICT c)        { libxsmm_smm(m, n, k, a, b, c); }
LIBXSMM_TARGET(mic) inline void libxsmm_mm(int m, int n, int k, const double *LIBXSMM_RESTRICT a, const double *LIBXSMM_RESTRICT b, double *LIBXSMM_RESTRICT c)     { libxsmm_dmm(m, n, k, a, b, c); }

/** Non-dispatched matrix-matrix multiplication using inline code. */
LIBXSMM_TARGET(mic) inline void libxsmm_imm(int m, int n, int k, const float *LIBXSMM_RESTRICT a, const float *LIBXSMM_RESTRICT b, float *LIBXSMM_RESTRICT c)       { libxsmm_simm(m, n, k, a, b, c); }
LIBXSMM_TARGET(mic) inline void libxsmm_imm(int m, int n, int k, const double *LIBXSMM_RESTRICT a, const double *LIBXSMM_RESTRICT b, double *LIBXSMM_RESTRICT c)    { libxsmm_dimm(m, n, k, a, b, c); }

/** Non-dispatched matrix-matrix multiplication using BLAS. */
LIBXSMM_TARGET(mic) inline void libxsmm_blasmm(int m, int n, int k, const float *LIBXSMM_RESTRICT a, const float *LIBXSMM_RESTRICT b, float *LIBXSMM_RESTRICT c)    { libxsmm_sblasmm(m, n, k, a, b, c); }
LIBXSMM_TARGET(mic) inline void libxsmm_blasmm(int m, int n, int k, const double *LIBXSMM_RESTRICT a, const double *LIBXSMM_RESTRICT b, double *LIBXSMM_RESTRICT c) { libxsmm_dblasmm(m, n, k, a, b, c); }

/** Call libxsmm_smm_dispatch, or libxsmm_dmm_dispatch depending on T. */
template<typename T> class LIBXSMM_TARGET(mic) libxsmm_mm_dispatch { typedef void function_type; };

template<> class LIBXSMM_TARGET(mic) libxsmm_mm_dispatch<float> {
  typedef libxsmm_smm_function function_type;
  mutable/*target:mic*/ function_type m_function;
public:
  libxsmm_mm_dispatch(): m_function(0) {}
  libxsmm_mm_dispatch(int m, int n, int k): m_function(libxsmm_smm_dispatch(m, n, k)) {}
  operator function_type() const { return m_function; }
};

template<> class LIBXSMM_TARGET(mic) libxsmm_mm_dispatch<double> {
  typedef libxsmm_dmm_function function_type;
  mutable/*target:mic*/ function_type m_function;
public:
  libxsmm_mm_dispatch(): m_function(0) {}
  libxsmm_mm_dispatch(int m, int n, int k): m_function(libxsmm_dmm_dispatch(m, n, k)) {}
  operator function_type() const { return m_function; }
};

#endif /*__cplusplus*/
