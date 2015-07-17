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
from functools import reduce
import itertools
import operator
import sys


def upper_list(lists, level):
    upper = [level, level + len(lists)][1>level] - 1
    above = lists[upper]
    if above:
        return above
    else:
        return upper_list(lists, level - 1)


def load_mlist(argv):
    begin = 3; end = begin + int(argv[1])
    if (begin > end or end > len(argv)):
        sys.tracebacklimit = 0
        raise ValueError("load_mlist: wrong number of elements!")
    return map(int, argv[begin:end])


def load_nlist(argv):
    begin = 3 + int(argv[1]); end = begin + int(argv[2])
    if (begin > end or end > len(argv)):
        sys.tracebacklimit = 0
        raise ValueError("load_nlist: wrong number of elements!")
    return map(int, argv[begin:end])


def load_klist(argv):
    begin = 3 + int(argv[1]) + int(argv[2])
    if (begin > len(argv)):
        sys.tracebacklimit = 0
        raise ValueError("load_klist: wrong number of elements!")
    return map(int, argv[begin:])


def load_mnklist(argv, format, threshold):
    resultset = set()
    if (0 == format): # indexes format
        resultset = set(map(lambda mnk: tuple(map(int, mnk.split("_"))), argv))
    elif (-1 == format): # new input format
        groups = map(lambda group: [int(i) for i in group.split()], argv.split(","))
        resultset = set(itertools.chain(*[list(itertools.product(*(i, i, i))) for i in groups]))
    elif (-2 == format): # legacy format
        mlist, nlist, klist = load_mlist(argv), load_nlist(argv), load_klist(argv)
        mnk = [mlist, nlist, klist]
        top = [ \
          [mlist, upper_list(mnk, 0)][0==len(mlist)], \
          [nlist, upper_list(mnk, 1)][0==len(nlist)], \
          [klist, upper_list(mnk, 2)][0==len(klist)]  \
        ]
        for m in top[0]:
            for n in top[1]:
                if not nlist: n = m
                for k in top[2]:
                    if not klist: k = n
                    if not mlist: m = k
                    resultset.add((m, n, k))
    else:
        sys.tracebacklimit = 0
        raise ValueError("load_mnklist: unexpected format!")
    return sorted(filter(lambda mnk: (0 >= threshold or threshold >= (mnk[0] * mnk[1] * mnk[2])) and (0 < mnk[0]) and (0 < mnk[1]) and (0 < mnk[2]), resultset))


def max_mnk(mnklist, init = 0, index = None):
    return reduce(max, map(lambda mnk: \
      mnk[index] if (None != index and 0 <= index and index < 3) \
      else (mnk[0] * mnk[1] * mnk[2]), mnklist), init)


def median(list_of_numbers, fallback = None, average = True):
    size = len(list_of_numbers)
    if (0 < size):
        # TODO: use nth element
        list_of_numbers.sort()
        size2 = size / 2
        if (average and 0 == (size - size2 * 2)):
            result = int(0.5 * (list_of_numbers[size2-1] + list_of_numbers[size2]) + 0.5)
        else:
            result = list_of_numbers[size2]
        return result
    elif (None != fallback):
        return fallback
    else:
        sys.tracebacklimit = 0
        raise ValueError("median: empty list!")


def is_pot(num):
    return 0 <= num or 0 == (num & (num - 1))


def sanitize_alignment(alignment):
    if (0 >= alignment):
        alignment = [1, 64][0 != alignment]
    elif (False == is_pot(alignment)):
        sys.tracebacklimit = 0
        raise ValueError("sanitize_alignment: alignment must be a Power of Two (POT)!")
    return alignment


def align_value(n, typesize, alignment):
    if (0 < typesize and 0 < alignment):
        return (((n * typesize + alignment - 1) / alignment) * alignment) / typesize
    else:
        sys.tracebacklimit = 0
        raise ValueError("align_value: invalid input!")


if __name__ == "__main__":
    argc = len(sys.argv)
    format = int(sys.argv[1])
    if (3 < argc and -1 == format): # new input format
        dims = load_mnklist(str(*sys.argv[3:]), format, int(sys.argv[2]))
        print " ".join(map(lambda mnk: "_".join(map(str, mnk)), dims))
    elif (5 < argc and -2 == format): # legacy format
        dims = load_mnklist(sys.argv[2:], format, int(sys.argv[2]))
        print " ".join(map(lambda mnk: "_".join(map(str, mnk)), dims))
    elif (4 == argc and 0 < format):
        print align_value(int(sys.argv[2]), format, sanitize_alignment(int(sys.argv[3])))
    else:
        sys.tracebacklimit = 0
        raise ValueError(sys.argv[0] + ": wrong number of arguments!")
