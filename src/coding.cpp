#include "coding.h"

namespace logream {
    int VarintLength(uint64_t v) {
        int len = 1;
        while (v >= 128) {
            v >>= 7;
            ++len;
        }
        return len;
    }

    char * EncodeVarint32(char * dst, uint32_t v) {
        auto * ptr = reinterpret_cast<unsigned char *>(dst);
        constexpr int B = 128;
        if (v < (1 << 7)) {
            *(ptr++) = v;
        } else if (v < (1 << 14)) {
            *(ptr++) = v | B;
            *(ptr++) = v >> 7;
        } else if (v < (1 << 21)) {
            *(ptr++) = v | B;
            *(ptr++) = (v >> 7) | B;
            *(ptr++) = v >> 14;
        } else if (v < (1 << 28)) {
            *(ptr++) = v | B;
            *(ptr++) = (v >> 7) | B;
            *(ptr++) = (v >> 14) | B;
            *(ptr++) = v >> 21;
        } else {
            *(ptr++) = v | B;
            *(ptr++) = (v >> 7) | B;
            *(ptr++) = (v >> 14) | B;
            *(ptr++) = (v >> 21) | B;
            *(ptr++) = v >> 28;
        }
        return reinterpret_cast<char *>(ptr);
    }

    bool GetVarint32(Slice * s, uint32_t * v) {
        const char * p = s->data();
        const char * limit = p + s->size();
        const char * q = GetVarint32(p, limit, v);
        if (q == nullptr) {
            return false;
        } else {
            *s = {q, static_cast<size_t>(limit - q)};
            return true;
        }
    }

    const char * GetVarint32(const char * p, const char * limit, uint32_t * v) {
        if (p < limit) {
            uint32_t result = *reinterpret_cast<const unsigned char *>(p);
            if ((result & 128) == 0) {
                *v = result;
                return p + 1;
            }
        }

        uint32_t result = 0;
        for (uint32_t shift = 0; shift <= 28 && p < limit; shift += 7) {
            uint32_t byte = *reinterpret_cast<const unsigned char *>(p++);
            if (byte & 128) {
                result |= ((byte & 127) << shift);
            } else {
                result |= (byte << shift);
                *v = result;
                return p;
            }
        }
        return nullptr;
    }
}