#include <nmmintrin.h>

#include "crc32c.h"

namespace logream::crc32c {
    uint32_t Extend(uint32_t init_crc, const char * data, size_t n) {
        const auto * p = reinterpret_cast<const uint8_t *>(data);
        const auto * e = p + n;
        uint32_t l = (init_crc ^ 0xffffffffu);

#define STEP1 do {                              \
    l = _mm_crc32_u8(l, *p++);                  \
} while (false)
#define STEP4 do {                              \
    l = _mm_crc32_u32(l, *(uint32_t*)p);        \
    p += 4;                                     \
} while (false)
#define STEP8 do {                              \
    l = _mm_crc32_u64(l, *(uint64_t*)p);        \
    p += 8;                                     \
} while (false)

        if (n > 16) {
            for (size_t i = reinterpret_cast<uintptr_t>(p) % 8; i != 0; --i) {
                STEP1;
            }

#if defined(_M_X64) || defined(__x86_64__)
            while ((e - p) >= 8) {
                STEP8;
            }
            if ((e - p) >= 4) {
                STEP4;
            }
#else  // !(defined(_M_X64) || defined(__x86_64__))
            while ((e-p) >= 4) {
                  STEP4;
                }
#endif  // defined(_M_X64) || defined(__x86_64__)
        }

        while (p != e) {
            STEP1;
        }
#undef STEP1
#undef STEP4
#undef STEP8
        return l ^ 0xffffffffu;
    }
}