#pragma once
#ifndef LOGREAM_BLOOM_H
#define LOGREAM_BLOOM_H

// Copyright (c) 2012 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "slice.h"

namespace logream {
    template<typename HASH>
    class Bloom {
    public:
        template<typename T>
        void CreateFilter(T getslice, size_t n, std::string * dst) const {
            // Compute bloom filter size (in both bits and bytes)
            size_t bits = n;

            // For small n, we can see a very high false positive rate.
            // Fix it by enforcing a minimum bloom filter length.
            if (bits < 64) bits = 64;

            size_t bytes = (bits + 7) / 8;
            bits = bytes * 8;

            dst->resize(bytes, 0);
            char * array = dst->data();
            for (Slice k; getslice(&k);) {
                const size_t h = HASH()(k);
                const size_t bitpos = h % bits;
                array[bitpos / 8] |= (1 << (bitpos % 8));
            }
        }

        bool KeyMayMatch(const Slice & k, const Slice & bloom_filter) const {
            const char * array = bloom_filter.data();
            const size_t bytes = bloom_filter.size();
            const size_t bits = bytes * 8;

            const size_t h = HASH()(k);
            const size_t bitpos = h % bits;
            return (array[bitpos / 8] >> (bitpos % 8)) & 1;
        }
    };
}

#endif //LOGREAM_BLOOM_H
