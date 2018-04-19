#pragma once
#ifndef LOGREAM_CRC32C_H
#define LOGREAM_CRC32C_H

// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include <cstddef>
#include <cstdint>

namespace logream::crc32c {
    uint32_t Extend(uint32_t init_crc, const char * data, size_t n);

    inline uint32_t Value(const char * data, size_t n) {
        return Extend(0, data, n);
    }

    constexpr uint32_t kMaskDelta = 0xa282ead8ul;

    // Return a masked representation of crc.
    //
    // Motivation: it is problematic to compute the CRC of a string that
    // contains embedded CRCs.  Therefore we recommend that CRCs stored
    // somewhere (e.g., in files) should be masked before being stored.
    inline uint32_t Mask(uint32_t crc) {
        return ((crc >> 15) | (crc << 17)) + kMaskDelta;
    }

    // Return the crc whose masked representation is masked_crc.
    inline uint32_t Unmask(uint32_t masked_crc) {
        uint32_t rot = masked_crc - kMaskDelta;
        return ((rot >> 17) | (rot << 15));
    }
}

#endif //LOGREAM_CRC32C_H
