!*****************************************************************************!
!* Copyright (c) 2013-2015, Intel Corporation                                *!
!* All rights reserved.                                                      *!
!*                                                                           *!
!* Redistribution and use in source and binary forms, with or without        *!
!* modification, are permitted provided that the following conditions        *!
!* are met:                                                                  *!
!* 1. Redistributions of source code must retain the above copyright         *!
!*    notice, this list of conditions and the following disclaimer.          *!
!* 2. Redistributions in binary form must reproduce the above copyright      *!
!*    notice, this list of conditions and the following disclaimer in the    *!
!*    documentation and/or other materials provided with the distribution.   *!
!* 3. Neither the name of the copyright holder nor the names of its          *!
!*    contributors may be used to endorse or promote products derived        *!
!*    from this software without specific prior written permission.          *!
!*                                                                           *!
!* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS       *!
!* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT         *!
!* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR     *!
!* A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT      *!
!* HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,    *!
!* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED  *!
!* TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR    *!
!* PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF    *!
!* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING      *!
!* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS        *!
!* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.              *!
!*****************************************************************************!
!* Hans Pabst (Intel Corp.)                                                  *!
!*****************************************************************************!

MODULE LIBXSMM
  USE, INTRINSIC :: ISO_C_BINDING
  IMPLICIT NONE

  ! Kind of types used to parameterize the implementation.
  INTEGER, PARAMETER :: LIBXSMM_SINGLE_PRECISION  = KIND(1.0)
  INTEGER, PARAMETER :: LIBXSMM_DOUBLE_PRECISION  = KIND(1D0)
  INTEGER, PARAMETER :: LIBXSMM_INTEGER_TYPE      = KIND(1)

  ! Parameters the library was built for.
  INTEGER(LIBXSMM_INTEGER_TYPE), PARAMETER :: LIBXSMM_ALIGNMENT       = $ALIGNMENT
  INTEGER(LIBXSMM_INTEGER_TYPE), PARAMETER :: LIBXSMM_ALIGNED_STORES  = $ALIGNED_STORES
  INTEGER(LIBXSMM_INTEGER_TYPE), PARAMETER :: LIBXSMM_ALIGNED_LOADS   = $ALIGNED_LOADS
  INTEGER(LIBXSMM_INTEGER_TYPE), PARAMETER :: LIBXSMM_ALIGNED_MAX     = $ALIGNED_MAX
  INTEGER(LIBXSMM_INTEGER_TYPE), PARAMETER :: LIBXSMM_ROW_MAJOR       = $ROW_MAJOR
  INTEGER(LIBXSMM_INTEGER_TYPE), PARAMETER :: LIBXSMM_COL_MAJOR       = $COL_MAJOR
  INTEGER(LIBXSMM_INTEGER_TYPE), PARAMETER :: LIBXSMM_MAX_MNK         = $MAX_MNK
  INTEGER(LIBXSMM_INTEGER_TYPE), PARAMETER :: LIBXSMM_MAX_M           = $MAX_M
  INTEGER(LIBXSMM_INTEGER_TYPE), PARAMETER :: LIBXSMM_MAX_N           = $MAX_N
  INTEGER(LIBXSMM_INTEGER_TYPE), PARAMETER :: LIBXSMM_MAX_K           = $MAX_K
  INTEGER(LIBXSMM_INTEGER_TYPE), PARAMETER :: LIBXSMM_AVG_M           = $AVG_M
  INTEGER(LIBXSMM_INTEGER_TYPE), PARAMETER :: LIBXSMM_AVG_N           = $AVG_N
  INTEGER(LIBXSMM_INTEGER_TYPE), PARAMETER :: LIBXSMM_AVG_K           = $AVG_K

  ! Overloaded BLAS routines (single/double precision)
  INTERFACE libxsmm_blasmm
    MODULE PROCEDURE libxsmm_sblasmm, libxsmm_dblasmm
  END INTERFACE

  ! Overloaded optimized routines (single/double precision)
  INTERFACE libxsmm_imm
    MODULE PROCEDURE libxsmm_simm, libxsmm_dimm
  END INTERFACE

  ! Overloaded auto-dispatch routines (single/double precision)
  INTERFACE libxsmm_mm
    MODULE PROCEDURE libxsmm_smm, libxsmm_dmm
  END INTERFACE

  ! Type of a function generated for a specific M, N, and K
  ABSTRACT INTERFACE
    PURE SUBROUTINE LIBXSMM_XMM_FUNCTION(a, b, c) BIND(C)
      IMPORT :: C_PTR
      TYPE(C_PTR), VALUE, INTENT(IN) :: a, b, c
    END SUBROUTINE
  END INTERFACE

  !DIR$ ATTRIBUTES OFFLOAD:MIC :: sgemm, dgemm, libxsmm_smm_dispatch, libxsmm_dmm_dispatch
  INTERFACE
    SUBROUTINE sgemm(transa, transb, m, n, k, alpha, a, lda, b, ldb, beta, c, ldc)
      IMPORT LIBXSMM_INTEGER_TYPE, LIBXSMM_SINGLE_PRECISION
      CHARACTER(1), INTENT(IN) :: transa, transb
      INTEGER(LIBXSMM_INTEGER_TYPE), INTENT(IN) :: m, n, k, lda, ldb, ldc
      REAL(LIBXSMM_SINGLE_PRECISION), INTENT(IN) :: a(lda,*), b(ldb,*), alpha, beta
      REAL(LIBXSMM_SINGLE_PRECISION), INTENT(INOUT) :: c(ldc,*)
    END SUBROUTINE
    SUBROUTINE dgemm(transa, transb, m, n, k, alpha, a, lda, b, ldb, beta, c, ldc)
      IMPORT LIBXSMM_INTEGER_TYPE, LIBXSMM_DOUBLE_PRECISION
      CHARACTER(1), INTENT(IN) :: transa, transb
      INTEGER(LIBXSMM_INTEGER_TYPE), INTENT(IN) :: m, n, k, lda, ldb, ldc
      REAL(LIBXSMM_DOUBLE_PRECISION), INTENT(IN) :: a(lda,*), b(ldb,*), alpha, beta
      REAL(LIBXSMM_DOUBLE_PRECISION), INTENT(INOUT) :: c(ldc,*)
    END SUBROUTINE

    ! Query the pointer of a generated function; zero if it does not exist, single-precision.
    TYPE(C_FUNPTR) PURE FUNCTION libxsmm_smm_dispatch(m, n, k) BIND(C)
      IMPORT :: C_FUNPTR, C_INT
      INTEGER(C_INT), VALUE, INTENT(IN) :: m, n, k
    END FUNCTION
    ! Query the pointer of a generated function; zero if it does not exist; double-precision.
    TYPE(C_FUNPTR) PURE FUNCTION libxsmm_dmm_dispatch(m, n, k) BIND(C)
      IMPORT :: C_FUNPTR, C_INT
      INTEGER(C_INT), VALUE, INTENT(IN) :: m, n, k
    END FUNCTION$MNK_INTERFACE_LIST
  END INTERFACE

CONTAINS
  !DIR$ ATTRIBUTES OFFLOAD:MIC :: libxsmm_up
  !DIR$ ATTRIBUTES INLINE :: libxsmm_up
  PURE FUNCTION libxsmm_up(n, up) RESULT(nup)
    INTEGER(LIBXSMM_INTEGER_TYPE), INTENT(IN) :: n, up
    INTEGER(LIBXSMM_INTEGER_TYPE) :: nup
    nup = ((n + up - 1) / up) * up
  END FUNCTION

  !DIR$ ATTRIBUTES OFFLOAD:MIC :: libxsmm_align_value
  !DIR$ ATTRIBUTES INLINE :: libxsmm_align_value
  PURE FUNCTION libxsmm_align_value(n, typesize, alignment) RESULT(na)
    INTEGER(LIBXSMM_INTEGER_TYPE), INTENT(IN) :: n, typesize, alignment
    INTEGER(LIBXSMM_INTEGER_TYPE) :: na
    na = libxsmm_up(n * typesize, alignment) / typesize
  END FUNCTION

  !DIR$ ATTRIBUTES OFFLOAD:MIC :: libxsmm_ld
  !DIR$ ATTRIBUTES INLINE :: libxsmm_ld
  PURE FUNCTION libxsmm_ld(m, n) RESULT(ld)
    INTEGER(LIBXSMM_INTEGER_TYPE), INTENT(IN) :: m, n
    INTEGER(LIBXSMM_INTEGER_TYPE) :: ld
    ld = MERGE(m, n, 0.NE.LIBXSMM_COL_MAJOR)
  END FUNCTION

  ! Non-dispatched matrix-matrix multiplication using BLAS; single-precision.
  !DIR$ ATTRIBUTES OFFLOAD:MIC :: libxsmm_sblasmm
  !DIR$ ATTRIBUTES INLINE :: libxsmm_sblasmm
  SUBROUTINE libxsmm_sblasmm(m, n, k, a, b, c)
    INTEGER(LIBXSMM_INTEGER_TYPE), PARAMETER :: T = LIBXSMM_SINGLE_PRECISION
    INTEGER(LIBXSMM_INTEGER_TYPE), INTENT(IN) :: m, n, k
    REAL(T), INTENT(IN) :: a($SHAPE_AS1,$SHAPE_AS2), b($SHAPE_BS1,$SHAPE_BS2)
    REAL(T), INTENT(INOUT) :: c($SHAPE_C1,$SHAPE_C2)
    REAL(T), PARAMETER :: alpha = 1, beta = 1
    IF (0.NE.LIBXSMM_COL_MAJOR) THEN
      CALL sgemm('N', 'N', m, n, k, alpha, a, m, b, k, beta, c, SIZE(c, 1))
    ELSE
      CALL sgemm('N', 'N', n, m, k, alpha, b, n, a, k, beta, c, SIZE(c, 1))
    ENDIF
  END SUBROUTINE

  ! Non-dispatched matrix-matrix multiplication using BLAS; double-precision.
  !DIR$ ATTRIBUTES OFFLOAD:MIC :: libxsmm_dblasmm
  !DIR$ ATTRIBUTES INLINE :: libxsmm_dblasmm
  SUBROUTINE libxsmm_dblasmm(m, n, k, a, b, c)
    INTEGER(LIBXSMM_INTEGER_TYPE), PARAMETER :: T = LIBXSMM_DOUBLE_PRECISION
    INTEGER(LIBXSMM_INTEGER_TYPE), INTENT(IN) :: m, n, k
    REAL(T), INTENT(IN) :: a($SHAPE_AS1,$SHAPE_AS2), b($SHAPE_BS1,$SHAPE_BS2)
    REAL(T), INTENT(INOUT) :: c($SHAPE_C1,$SHAPE_C2)
    REAL(T), PARAMETER :: alpha = 1, beta = 1
    IF (0.NE.LIBXSMM_COL_MAJOR) THEN
      CALL dgemm('N', 'N', m, n, k, alpha, a, m, b, k, beta, c, SIZE(c, 1))
    ELSE
      CALL dgemm('N', 'N', n, m, k, alpha, b, n, a, k, beta, c, SIZE(c, 1))
    ENDIF
  END SUBROUTINE

  ! Non-dispatched matrix-matrix multiplication using optimized code; single-precision.
  !DIR$ ATTRIBUTES OFFLOAD:MIC :: libxsmm_simm
  !DIR$ ATTRIBUTES INLINE :: libxsmm_simm
  SUBROUTINE libxsmm_simm(m, n, k, a, b, c)
    INTEGER(LIBXSMM_INTEGER_TYPE), PARAMETER :: T = LIBXSMM_SINGLE_PRECISION
    INTEGER(LIBXSMM_INTEGER_TYPE), INTENT(IN) :: m, n, k
    INTEGER(LIBXSMM_INTEGER_TYPE) :: i, j
    REAL(T), INTENT(IN) :: a($SHAPE_AS1,$SHAPE_AS2), b($SHAPE_BS1,$SHAPE_BS2)
    REAL(T), INTENT(INOUT) :: c($SHAPE_C1,$SHAPE_C2)
    REAL(T) :: x($SHAPE_AT1,$SHAPE_AT2), y($SHAPE_BT1,$SHAPE_BT2)
    IF (0.NE.LIBXSMM_COL_MAJOR) THEN
      !DIR$ OMP SIMD COLLAPSE(2)
      DO j = LBOUND(b, 2), LBOUND(b, 2) + n - 1
        !DIR$ LOOP COUNT(1, LIBXSMM_MAX_M, LIBXSMM_AVG_M)
        DO i = LBOUND(a, 1), LBOUND(a, 1) + m - 1
          c(i,j) = c(i,j) + DOT_PRODUCT(a(i,:), b(:,j))
        END DO
      END DO
    ELSE
      x = RESHAPE(b, SHAPE(x))
      y = RESHAPE(a, SHAPE(y))
      !DIR$ OMP SIMD COLLAPSE(2)
      DO j = LBOUND(y, 2), LBOUND(y, 2) + m - 1
        !DIR$ LOOP COUNT(1, LIBXSMM_MAX_N, LIBXSMM_AVG_N)
        DO i = LBOUND(x, 1), LBOUND(x, 1) + n - 1
          c(i,j) = c(i,j) + DOT_PRODUCT(x(i,:), y(:,j))
        END DO
      END DO
    ENDIF
  END SUBROUTINE

  ! Non-dispatched matrix-matrix multiplication using optimized code; double-precision.
  !DIR$ ATTRIBUTES OFFLOAD:MIC :: libxsmm_dimm
  !DIR$ ATTRIBUTES INLINE :: libxsmm_dimm
  SUBROUTINE libxsmm_dimm(m, n, k, a, b, c)
    INTEGER(LIBXSMM_INTEGER_TYPE), PARAMETER :: T = LIBXSMM_DOUBLE_PRECISION
    INTEGER(LIBXSMM_INTEGER_TYPE), INTENT(IN) :: m, n, k
    INTEGER(LIBXSMM_INTEGER_TYPE) :: i, j
    REAL(T), INTENT(IN) :: a($SHAPE_AS1,$SHAPE_AS2), b($SHAPE_BS1,$SHAPE_BS2)
    REAL(T), INTENT(INOUT) :: c($SHAPE_C1,$SHAPE_C2)
    REAL(T) :: x($SHAPE_AT1,$SHAPE_AT2), y($SHAPE_BT1,$SHAPE_BT2)
    IF (0.NE.LIBXSMM_COL_MAJOR) THEN
      !DIR$ OMP SIMD COLLAPSE(2)
      DO j = LBOUND(b, 2), LBOUND(b, 2) + n - 1
        !DIR$ LOOP COUNT(1, LIBXSMM_MAX_M, LIBXSMM_AVG_M)
        DO i = LBOUND(a, 1), LBOUND(a, 1) + m - 1
          c(i,j) = c(i,j) + DOT_PRODUCT(a(i,:), b(:,j))
        END DO
      END DO
    ELSE
      x = RESHAPE(b, SHAPE(x))
      y = RESHAPE(a, SHAPE(y))
      !DIR$ OMP SIMD COLLAPSE(2)
      DO j = LBOUND(y, 2), LBOUND(y, 2) + m - 1
        !DIR$ LOOP COUNT(1, LIBXSMM_MAX_N, LIBXSMM_AVG_N)
        DO i = LBOUND(x, 1), LBOUND(x, 1) + n - 1
          c(i,j) = c(i,j) + DOT_PRODUCT(x(i,:), y(:,j))
        END DO
      END DO
    ENDIF
  END SUBROUTINE

  ! Query the pointer of a generated function; zero if it does not exist.
  !DIR$ ATTRIBUTES OFFLOAD:MIC :: libxsmm_mm_dispatch
  !DIR$ ATTRIBUTES INLINE :: libxsmm_mm_dispatch
  FUNCTION libxsmm_mm_dispatch(m, n, k, type) RESULT(f)
    INTEGER(LIBXSMM_INTEGER_TYPE), INTENT(IN) :: m, n, k, type
    !PROCEDURE(LIBXSMM_XMM_FUNCTION), POINTER :: f
    TYPE(C_FUNPTR) :: f
    f = MERGE( &
      libxsmm_dmm_dispatch(m, n, k), &
      libxsmm_smm_dispatch(m, n, k), &
      LIBXSMM_DOUBLE_PRECISION.EQ.type)
  END FUNCTION

  ! Dispatched matrix-matrix multiplication; single-precision.
  !DIR$ ATTRIBUTES OFFLOAD:MIC :: libxsmm_smm
  !DIR$ ATTRIBUTES INLINE :: libxsmm_smm
  SUBROUTINE libxsmm_smm(m, n, k, a, b, c)
    INTEGER(LIBXSMM_INTEGER_TYPE), PARAMETER :: T = LIBXSMM_SINGLE_PRECISION
    INTEGER(LIBXSMM_INTEGER_TYPE), INTENT(IN) :: m, n, k
    REAL(T), TARGET, INTENT(IN) :: a($SHAPE_AS1,$SHAPE_AS2), b($SHAPE_BS1,$SHAPE_BS2)
    REAL(T), TARGET, INTENT(INOUT) :: c($SHAPE_C1,$SHAPE_C2)
    !DIR$ ATTRIBUTES OFFLOAD:MIC :: xmm
    PROCEDURE(LIBXSMM_XMM_FUNCTION), POINTER :: xmm
    TYPE(C_FUNPTR) :: f
    IF (LIBXSMM_MAX_MNK.GE.(m * n * k)) THEN
      f = libxsmm_smm_dispatch(m, n, k)
      IF (C_ASSOCIATED(f)) THEN
        CALL C_F_PROCPOINTER(f, xmm)
        CALL xmm(C_LOC(a), C_LOC(b), C_LOC(c))
      ELSE
        CALL libxsmm_simm(m, n, k, a, b, c)
      ENDIF
    ELSE
      CALL libxsmm_sblasmm(m, n, k, a, b, c)
    ENDIF
  END SUBROUTINE

  ! Dispatched matrix-matrix multiplication; double-precision.
  !DIR$ ATTRIBUTES OFFLOAD:MIC :: libxsmm_dmm
  !DIR$ ATTRIBUTES INLINE :: libxsmm_dmm
  SUBROUTINE libxsmm_dmm(m, n, k, a, b, c)
    INTEGER(LIBXSMM_INTEGER_TYPE), PARAMETER :: T = LIBXSMM_DOUBLE_PRECISION
    INTEGER(LIBXSMM_INTEGER_TYPE), INTENT(IN) :: m, n, k
    REAL(T), TARGET, INTENT(IN) :: a($SHAPE_AS1,$SHAPE_AS2), b($SHAPE_BS1,$SHAPE_BS2)
    REAL(T), TARGET, INTENT(INOUT) :: c($SHAPE_C1,$SHAPE_C2)
    !DIR$ ATTRIBUTES OFFLOAD:MIC :: xmm
    PROCEDURE(LIBXSMM_XMM_FUNCTION), POINTER :: xmm
    TYPE(C_FUNPTR) :: f
    IF (LIBXSMM_MAX_MNK.GE.(m * n * k)) THEN
      f = libxsmm_dmm_dispatch(m, n, k)
      IF (C_ASSOCIATED(f)) THEN
        CALL C_F_PROCPOINTER(f, xmm)
        CALL xmm(C_LOC(a), C_LOC(b), C_LOC(c))
      ELSE
        CALL libxsmm_dimm(m, n, k, a, b, c)
      ENDIF
    ELSE
      CALL libxsmm_dblasmm(m, n, k, a, b, c)
    ENDIF
  END SUBROUTINE
END MODULE
