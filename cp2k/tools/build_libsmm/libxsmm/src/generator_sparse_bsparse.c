/******************************************************************************
** Copyright (c) 2015, Intel Corporation                                     **
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
/* Alexander Heinecke (Intel Corp.)
******************************************************************************/
/**
 * @file
 * This file is part of GemmCodeGenerator.
 *
 * @author Alexander Heinecke (alexander.heinecke AT mytum.de, http://www5.in.tum.de/wiki/index.php/Alexander_Heinecke,_M.Sc.,_M.Sc._with_honors)
 *
 * @section LICENSE
 * Copyright (c) 2012-2014, Technische Universitaet Muenchen
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE  USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * @section DESCRIPTION
 * <DESCRIPTION>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

#include "generator_common.h"
#include "generator_sparse_bsparse.h"

void libxsmm_generator_sparse_bsparse( libxsmm_generated_code*         io_generated_code,
                                       const libxsmm_xgemm_descriptor* i_xgemm_desc,
                                       const char*                     i_arch, 
                                       const unsigned int*             i_row_idx,
                                       const unsigned int*             i_column_idx,
                                       const double*                   i_values ) {
  char l_new_code[512];

  sprintf(l_new_code, "  unsigned int l_m = 0;\n");
  libxsmm_append_code_as_string( io_generated_code, l_new_code );

  /* reset C if beta is zero */
  if ( i_xgemm_desc->beta == 0 ) {
    sprintf(l_new_code, "  unsigned int l_n = 0;\n");
    libxsmm_append_code_as_string( io_generated_code, l_new_code );
    sprintf(l_new_code, "  for ( l_n = 0; l_n < %u; l_n++) {\n", i_xgemm_desc->n);
    libxsmm_append_code_as_string( io_generated_code, l_new_code );
    if ( i_xgemm_desc->m > 1 ) {
      sprintf(l_new_code, "    #pragma simd\n");
      libxsmm_append_code_as_string( io_generated_code, l_new_code );
      sprintf(l_new_code, "    #pragma vector aligned\n");
      libxsmm_append_code_as_string( io_generated_code, l_new_code );
    }
    if ( i_xgemm_desc->single_precision == 0 ) {
      sprintf(l_new_code, "    for ( l_m = 0; l_m < %u; l_m++) { C[(l_n*%u)+l_m] = 0.0; }\n", i_xgemm_desc->m, i_xgemm_desc->ldc);
    } else {
      sprintf(l_new_code, "    for ( l_m = 0; l_m < %u; l_m++) { C[(l_n*%u)+l_m] = 0.0f; }\n", i_xgemm_desc->m, i_xgemm_desc->ldc);
    }
    libxsmm_append_code_as_string( io_generated_code, l_new_code );
    sprintf(l_new_code, "  }\n");
    libxsmm_append_code_as_string( io_generated_code, l_new_code );
  }
  sprintf(l_new_code, "\n");
  libxsmm_append_code_as_string( io_generated_code, l_new_code );

  /* determine the correct simd pragma for each architecture */
  if ( ( strcmp( i_arch, "noarch" ) == 0 ) ||
       ( strcmp( i_arch, "wsm" ) == 0 )    ||
       ( strcmp( i_arch, "snb" ) == 0 )    ||
       ( strcmp( i_arch, "hsw" ) == 0 )       ) {
    if ( i_xgemm_desc->m > 7 ) {
       sprintf(l_new_code, "  #pragma simd vectorlength(8)\n");
       libxsmm_append_code_as_string( io_generated_code, l_new_code );
    } else if ( i_xgemm_desc->m > 3 ) {
       sprintf(l_new_code, "  #pragma simd vectorlength(4)\n");
       libxsmm_append_code_as_string( io_generated_code, l_new_code );
    } else if ( i_xgemm_desc->m > 1 ) {
      sprintf(l_new_code, "  #pragma simd vectorlength(2)\n");
      libxsmm_append_code_as_string( io_generated_code, l_new_code );
    } else {} 

    if ( (i_xgemm_desc->m > 1)          && 
         (i_xgemm_desc->aligned_a != 0) && 
         (i_xgemm_desc->aligned_c != 0)    ) {
      sprintf(l_new_code, "  #pragma vector aligned\n");
      libxsmm_append_code_as_string( io_generated_code, l_new_code );
    }
  } else if ( ( strcmp( i_arch, "knc" ) == 0 ) ||
              ( strcmp( i_arch, "knl" ) == 0 ) ||
              ( strcmp( i_arch, "skx" ) == 0 )    ) {
    if ( (i_xgemm_desc->m > 1)          && 
         (i_xgemm_desc->aligned_a != 0) && 
         (i_xgemm_desc->aligned_c != 0)    ) {
      sprintf(l_new_code, "  #pragma simd vectorlength(32)\n  #pragma vector aligned\n");
      libxsmm_append_code_as_string( io_generated_code, l_new_code );
    }
  } else {
    libxsmm_handle_error( io_generated_code, LIBXSMM_ERR_ARCH );
    return;
  } 

  /* generate the actuel kernel */
  sprintf(l_new_code, "  for ( l_m = 0; l_m < %u; l_m++) {\n", i_xgemm_desc->m);
  libxsmm_append_code_as_string( io_generated_code, l_new_code );

  unsigned int l_n;
  unsigned int l_z;
  unsigned int l_column_elements;
  unsigned int l_flop_count = 0;

  for ( l_n = 0; l_n < i_xgemm_desc->n; l_n++ ) {
    l_column_elements = i_column_idx[l_n+1] - i_column_idx[l_n];
    for ( l_z = 0; l_z < l_column_elements; l_z++ ) {
      /* check k such that we just use rows which actually need to be multiplied */
      if ( i_row_idx[i_column_idx[l_n] + l_z] < i_xgemm_desc->k ) {
        sprintf(l_new_code, "    C[%u+l_m] += A[%u+l_m] * B[%u];\n", l_n * i_xgemm_desc->ldc, i_row_idx[i_column_idx[l_n] + l_z]*i_xgemm_desc->lda, i_column_idx[l_n] + l_z);
        libxsmm_append_code_as_string( io_generated_code, l_new_code );
        l_flop_count += 2;
      }
    }
  }

  sprintf(l_new_code, "  }\n");
  libxsmm_append_code_as_string( io_generated_code, l_new_code );

  /* add flop counter */
  sprintf(l_new_code, "\n#ifndef NDEBUG\n#ifdef _OPENMP\n#pragma omp atomic\n#endif\nlibxsmm_num_total_flops += %u;\n#endif\n", l_flop_count * i_xgemm_desc->m);
  libxsmm_append_code_as_string( io_generated_code, l_new_code );
}

