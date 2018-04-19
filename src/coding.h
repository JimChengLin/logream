#pragma once
#ifndef LOGREAM_CODING_H
#define LOGREAM_CODING_H

// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "slice.h"

namespace logream {
    constexpr unsigned int kMaxVarint32Length = 5;

    inline uint8_t CharToUint8(char c) {
        return *reinterpret_cast<uint8_t *>(&c);
    }

    inline char Uint8ToChar(uint8_t i) {
        return *reinterpret_cast<char *>(&i);
    }

    int VarintLength(uint64_t v);

    char * EncodeVarint32(char * dst, uint32_t v);

    inline void PutVarint32(std::string * dst, uint32_t v) {
        char buf[kMaxVarint32Length];
        char * ptr = EncodeVarint32(buf, v);
        dst->append(buf, static_cast<size_t>(ptr - buf));
    }

    bool GetVarint32(Slice * s, uint32_t * v);

    const char * GetVarint32(const char * p, const char * limit, uint32_t * v);
}

#endif //LOGREAM_CODING_H
