#! /usr/bin/env python
###############################################################################
## Copyright (c) 2013-2015, Intel Corporation                                ##
## All rights reserved.                                                      ##
##                                                                           ##
## Redistribution and use in source and binary forms, with or without        ##
## modification, are permitted provided that the following conditions        ##
## are met:                                                                  ##
## 1. Redistributions of source code must retain the above copyright         ##
##    notice, this list of conditions and the following disclaimer.          ##
## 2. Redistributions in binary form must reproduce the above copyright      ##
##    notice, this list of conditions and the following disclaimer in the    ##
##    documentation and/or other materials provided with the distribution.   ##
## 3. Neither the name of the copyright holder nor the names of its          ##
##    contributors may be used to endorse or promote products derived        ##
##    from this software without specific prior written permission.          ##
##                                                                           ##
## THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS       ##
## "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT         ##
## LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR     ##
## A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT      ##
## HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,    ##
## SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED  ##
## TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR    ##
## PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF    ##
## LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING      ##
## NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS        ##
## SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.              ##
###############################################################################
## Hans Pabst (Intel Corp.)
###############################################################################
import libxsmm_utilities
import sys
import os


def create_dispatch(mnklist):
    print "libxsmm_xgemm_descriptor LIBXSMM_XGEMM_DESCRIPTOR(desc, 0/*precision*/, LIBXSMM_PREFETCH,"
    print "  1 < (LIBXSMM_ALIGNED_LOADS) ? (LIBXSMM_ALIGNED_LOADS) : 0, 1 < (LIBXSMM_ALIGNED_STORES) ? (LIBXSMM_ALIGNED_STORES) : 0,"
    print "  'n', 'n', 1/*alpha*/, LIBXSMM_BETA, 0/*m*/, 0/*n*/, 0/*k*/, 0/*lda*/, 0/*ldb*/, 0/*ldc*/);"
    print "libxsmm_function* cache;"
    print "unsigned int indx;"
    for mnk in mnklist:
        mnkstr, mstr, nstr, kstr = "_".join(map(str, mnk)), str(mnk[0]), str(mnk[1]), str(mnk[2])
        print "desc.m = " + mstr + "; desc.n = " + nstr + "; desc.k = " + kstr + "; desc.lda = " + mstr + "; desc.ldb = " + kstr + ";"
        print "desc.ldc = LIBXSMM_ALIGN_STORES(" + mstr + ", sizeof(float)); desc.single_precision = 1;"
        print "indx = libxsmm_crc32(&desc, LIBXSMM_XGEMM_DESCRIPTOR_SIZE, LIBXSMM_BUILD_SEED) % (LIBXSMM_BUILD_CACHESIZE);"
        print "cache = libxsmm_cache[1/*single precision*/];"
        print "assert(0 == cache[indx]); /*TODO: handle collision*/"
        print "cache[indx] = (libxsmm_function)libxsmm_smm_" + mnkstr + ";"
        print "desc.ldc = LIBXSMM_ALIGN_STORES(" + mstr + ", sizeof(double)); desc.single_precision = 0;"
        print "indx = libxsmm_crc32(&desc, LIBXSMM_XGEMM_DESCRIPTOR_SIZE, LIBXSMM_BUILD_SEED) % (LIBXSMM_BUILD_CACHESIZE);"
        print "cache = libxsmm_cache[0/*double precision*/];"
        print "assert(0 == cache[indx]); /*TODO: handle collision*/"
        print "cache[indx] = (libxsmm_function)libxsmm_dmm_" + mnkstr + ";"


if __name__ == "__main__":
    argc = len(sys.argv)
    if (2 < argc):
        threshold = int(sys.argv[1])
        mnklist = libxsmm_utilities.load_mnklist(sys.argv[2:], 0, threshold)
        create_dispatch(mnklist)
    elif (1 < argc):
        print "/* no static code */"
    else:
        sys.tracebacklimit = 0
        raise ValueError(sys.argv[0] + ": wrong number of arguments!")
