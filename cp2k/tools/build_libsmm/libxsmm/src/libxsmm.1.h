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
