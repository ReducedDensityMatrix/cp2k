#include "libxsmm_dispatch.h"
#include "generator_extern_typedefs.h"
#include "libxsmm_crc32.h"
#include <libxsmm.h>

#define LIBXSMM_CACHESIZE (LIBXSMM_MAX_M) * (LIBXSMM_MAX_N) * (LIBXSMM_MAX_K) * 24
#define LIBXSMM_SEED 0


/** Filled with zeros due to C language rule. */
LIBXSMM_RETARGETABLE libxsmm_function libxsmm_cache[2][(LIBXSMM_CACHESIZE)];
LIBXSMM_RETARGETABLE int libxsmm_init = 0;


LIBXSMM_EXTERN_C LIBXSMM_RETARGETABLE libxsmm_function libxsmm_dispatch(const void* key, size_t key_size, size_t cache_id, libxsmm_function function)
{
  const unsigned int hash = libxsmm_crc32(key, key_size, LIBXSMM_SEED), i = hash % (LIBXSMM_CACHESIZE);
  libxsmm_function *const cache = libxsmm_cache[cache_id%2];
  const libxsmm_function f = cache[i];
  cache[i] = function;
  return f;
}


LIBXSMM_EXTERN_C LIBXSMM_RETARGETABLE libxsmm_function libxsmm_lookup(const void* key, size_t key_size, size_t cache_id)
{
  return libxsmm_cache[cache_id%2][libxsmm_crc32(key, key_size, LIBXSMM_SEED)%(LIBXSMM_CACHESIZE)];
}


LIBXSMM_EXTERN_C LIBXSMM_RETARGETABLE libxsmm_smm_function libxsmm_smm_dispatch(int m, int n, int k)
{
  libxsmm_xgemm_descriptor LIBXSMM_XGEMM_DESCRIPTOR(desc, 1/*single precision*/, LIBXSMM_PREFETCH, 'n', 'n', 1/*alpha*/, LIBXSMM_BETA,
    m, n, k, m, k, LIBXSMM_ALIGN_STORES(m, sizeof(float)));

  if (0 == libxsmm_init) {
    libxsmm_build_static();
    libxsmm_init = 1;
  }

  return (libxsmm_smm_function)libxsmm_lookup(&desc, LIBXSMM_XGEMM_DESCRIPTOR_SIZE, 1/*single precision*/);
}


LIBXSMM_EXTERN_C LIBXSMM_RETARGETABLE libxsmm_dmm_function libxsmm_dmm_dispatch(int m, int n, int k)
{
  libxsmm_xgemm_descriptor LIBXSMM_XGEMM_DESCRIPTOR(desc, 0/*double precision*/, LIBXSMM_PREFETCH, 'n', 'n', 1/*alpha*/, LIBXSMM_BETA,
    m, n, k, m, k, LIBXSMM_ALIGN_STORES(m, sizeof(double)));

  if (0 == libxsmm_init) {
    libxsmm_build_static();
    libxsmm_init = 1;
  }

  return (libxsmm_dmm_function)libxsmm_lookup(&desc, LIBXSMM_XGEMM_DESCRIPTOR_SIZE, 0/*double precision*/);
}
