#include <array>

#include "bloom.h"
#include "coding.h"
#include "crc32c.h"
#include "divsufsort.h"
#include "logream_compress.h"
#include "prefetch.h"

namespace logream {
    constexpr unsigned int kInlineSize = 63;
    enum Mark : unsigned char {
        kWarZone = 0,                  //  0
        kWarZoneClose = kWarZone + 63, // 63

        kBattlefield = kWarZoneClose + 1,
        kBattlefieldClose = kBattlefield + 63,

        kFrontline = kBattlefieldClose + 1,
        kFrontlineClose = kFrontline + 63,

        kNormal = kFrontlineClose + 1,
        kNormalClose = kNormal + 63,
    };
    static_assert(kNormalClose == UINT8_MAX);
    typedef Bloom<SliceHasher> BloomFilter;

    size_t WriterCompress::Add(const char * data, size_t * n) {
        Slice s(data, *n);
        assert(kMaxVarint32Length * 2 + s.size() + sizeof(uint32_t) <= kBattlefieldSize);
        const size_t result = cursor_;
        const size_t n_war_zone = result / kWarZoneSize;
        const size_t war_zone_r = result % kWarZoneSize;
        switch (n_war_zone) {

            case 0: {
                const size_t left = kWarZoneSize - war_zone_r;
                const Slice & dat = GeneratePlain(s);
                if (left > dat.size()) {
                    war_zone_.append(dat.data(), dat.size());
                } else {
                    war_zone_.append(dat.data(), left);
                    BuildSA(reinterpret_cast<unsigned char *>(war_zone_.data()),
                            &war_zone_sa_, &lcp_, &war_zone_lcplr_, &war_zone_bloom_filter_,
                            kMinRepeatWarZone, kWarZoneSize);
                    battlefield_.append(dat.data() + left, dat.size() - left);

                    lcp_.clear();
                    lcp_.shrink_to_fit();
                }
                Write(dat);
                *n = dat.size();
                break;
            }

            default: {
                const size_t n_battlefield = war_zone_r / kBattlefieldSize;
                const size_t battlefield_r = war_zone_r % kBattlefieldSize;
                switch (n_battlefield) {

                    case 0: {
                        const size_t left = kBattlefieldSize - battlefield_r;
                        const Slice & dat = GeneratePlain(s);
                        if (left > dat.size()) {
                            battlefield_.append(dat.data(), dat.size());
                        } else {
                            battlefield_.append(dat.data(), left);
                            BuildSA(reinterpret_cast<unsigned char *>(battlefield_.data()),
                                    &battlefield_sa_, &lcp_, &battlefield_lcplr_, &battlefield_bloom_filter_,
                                    kMinRepeatBattlefield, kBattlefieldSize);
                        }
                        Write(dat);
                        *n = dat.size();
                        break;
                    }

                    default: {
                        const size_t left = kWarZoneSize - war_zone_r;
                        const Slice & dat = GenerateCompressed(s);
                        if (left > dat.size()) {
                        } else {
                            battlefield_.clear();
                            battlefield_.append(dat.data() + left, dat.size() - left);
                        }
                        Write(dat);
                        *n = dat.size();
                        break;
                    }
                }
                break;
            }
        }
        return result;
    }

    Slice WriterCompress::GeneratePlain(const Slice & s) {
        size_t request = VarintLength(s.size()) + s.size() + sizeof(uint32_t);
        backup_.resize(request);

        char * dst = EncodeVarint32(backup_.data(), static_cast<uint32_t>(s.size()));
        memcpy(dst, s.data(), s.size());
        dst += s.size();

        uint32_t crc = crc32c::Mask(crc32c::Value(s.data(), s.size()));
        memcpy(dst, &crc, sizeof(crc));
        return backup_;
    }

    Slice WriterCompress::GenerateCompressed(const Slice & s) {
        assert(war_zone_.size() == kWarZoneSize);
        assert(battlefield_.size() == kBattlefieldSize);
        backup_.resize(kMaxVarint32Length);

        auto emit_mark = [&](Mark mark, size_t len) {
            assert(len > 0);
            if (len <= kInlineSize) {
                backup_ += Uint8ToChar(mark + len);
            } else {
                backup_ += Uint8ToChar(mark);
                PutVarint32(&backup_, static_cast<uint32_t>(len));
            }
        };

        Slice literal;
        auto add_literal = [&](const Slice & l) {
            assert(literal.size() == 0 || literal.data() + literal.size() == l.data());
            literal = {l.data() - literal.size(), l.size() + literal.size()};
        };
        auto emit_literal = [&]() {
            if (literal.size() == 0) {
                return;
            }
            emit_mark(kNormal, literal.size());
            backup_.append(literal.data(), literal.size());
            literal = {};
        };

        auto emit_war_zone = [&](size_t pos, size_t len) {
            emit_mark(kWarZone, len);
            backup_.append(reinterpret_cast<char *>(&pos), 3);
        };
        auto emit_battlefield = [&](size_t pos, size_t len) {
            emit_mark(kBattlefield, len);
            backup_.append(reinterpret_cast<char *>(&pos), 2);
        };
        auto emit_frontline = [&](size_t pos, size_t len) {
            emit_mark(kFrontline, len);
            backup_.append(reinterpret_cast<char *>(&pos), 1);
        };

        size_t i = 0;
        while (true) {
            Slice pattern(s.data() + i, s.size() - i);
            if (pattern.size() < kMinRepeat) {
                add_literal(pattern);
                break;
            }

            auto[wz_pos, wz_len] = FindLongestRepeat(war_zone_.data(), war_zone_sa_,
                                                     pattern, kMinRepeatWarZone, war_zone_lcplr_,
                                                     war_zone_bloom_filter_);
            auto[bf_pos, bf_len] = FindLongestRepeat(battlefield_.data(), battlefield_sa_,
                                                     pattern, kMinRepeatBattlefield, battlefield_lcplr_,
                                                     battlefield_bloom_filter_);
            auto[fl_pos, fl_len] = FindLongestRepeat(s, i);

            std::array<ssize_t, 3> profit_arr{
                    static_cast<ssize_t>(wz_len) - (3 /* pos */ + 1 /* mark */)
                    - (wz_len <= kInlineSize ? 0 /* inline */ : VarintLength(wz_len)),
                    static_cast<ssize_t>(bf_len) - (2 /* pos */ + 1 /* mark */)
                    - (bf_len <= kInlineSize ? 0 /* inline */ : VarintLength(bf_len)),
                    static_cast<ssize_t>(fl_len) - (1 /* pos */ + 1 /* mark */)
                    - (fl_len <= kInlineSize ? 0 /* inline */ : VarintLength(fl_len))
            };
            std::array<size_t, 3> length_arr{
                    wz_len,
                    bf_len,
                    fl_len
            };
            auto max_ele = std::max_element(profit_arr.cbegin(), profit_arr.cend());
            auto max_sol = max_ele - profit_arr.cbegin();
            auto max_len = length_arr[max_sol];

            if (*max_ele > 0) {
                emit_literal();
                switch (max_sol) {
                    case 0:
                        emit_war_zone(wz_pos, wz_len);
                        break;

                    case 1:
                        emit_battlefield(bf_pos, bf_len);
                        break;

                    default:
                        assert(max_sol == 2);
                        emit_frontline(fl_pos, fl_len);
                        break;
                }
            } else {
                max_len = 1;
                add_literal({pattern.data(), max_len});
            }
            i += max_len;
        }
        emit_literal();

        size_t size = backup_.size() - kMaxVarint32Length;
        int varint_size = VarintLength(size);
        char * dst = &backup_[kMaxVarint32Length - varint_size];
        EncodeVarint32(dst, static_cast<uint32_t>(size));

        uint32_t crc = crc32c::Mask(crc32c::Value(s.data(), s.size()));
        backup_.append(reinterpret_cast<char *>(&crc),
                       reinterpret_cast<char *>(&crc + 1));
        return {dst, varint_size + size + sizeof(crc)};
    }

    void WriterCompress::Write(const Slice & s) {
        helper_->Write(s);
        cursor_ += s.size();
    }

    void WriterCompress::BuildSA(const unsigned char * src,
                                 std::vector<int> * sa,
                                 std::vector<int> * lcp, std::vector<int> * lcplr,
                                 std::string * bloom_filter, size_t min_repeat,
                                 size_t n) {
        sa->resize(n);
        divsufsort(src, sa->data(), static_cast<int>(n), 0);
        BuildLCP(src, *sa, lcplr /* as inverse_sa */, lcp);
        BuildLCPLR(*lcp, lcplr);
        BuildBloomFilter(src, n, min_repeat, bloom_filter);
    }

    // https://stackoverflow.com/questions/11373453/how-does-lcp-help-in-finding-the-number-of-occurrences-of-a-pattern
    std::pair<size_t, size_t>
    WriterCompress::FindLongestRepeat(const char * src, const std::vector<int> & sa,
                                      const Slice & pattern,
                                      size_t min_repeat,
                                      const std::vector<int> & lcplr,
                                      const std::string & bloom_filter) {
        if (pattern.size() < min_repeat
            || !BloomFilter().KeyMayMatch({pattern.data(), min_repeat}, bloom_filter)) {
            return {{}, 0};
        }

        const auto n = static_cast<int>(sa.size());
        const auto m = static_cast<int>(pattern.size());

        auto grow = false;
        auto compare_to = [src, &sa, &pattern, n, m, &grow](int num, int start) -> int {
            assert(Slice(src + sa[num], start) == Slice(pattern.data(), start));
            auto i = sa[num] + start;
            for (; i < n && start < m && src[i] == pattern[start]; ++i, ++start) {}
            grow = (i < n ? CharToUint8(src[i]) : 0) < (start < m ? CharToUint8(pattern[start]) : 0);
            return start;
        };

        auto i = 1;
        struct {
            int l;
            int r;
        } lr = {0, n - 1};
        auto commons = 0;
        auto matches = 0;

        while (true) {
            auto mid = (lr.l + lr.r) / 2;
            if (commons > matches) {
                /*
                if (grow) {
                    // M ... M' ... R
                    // |-----|
                    //       M' ... R
                    lr.l = mid;
                    i = i * 2 + 1;
                } else {
                    // L ... M' ... M
                    //       |------|
                    // L ... M'
                    lr.r = mid;
                    i = i * 2;
                }
                */
                *(&lr.l + (!grow)) = mid;
                i = i * 2 + grow;
            } else if (commons < matches) {
                /*
                if (grow) {
                    // M ... M' ... R
                    // |-----|
                    // M ... M'
                    lr.r = mid;
                    i = i * 2;
                } else {
                    // L ... M' ... M
                    //       |------|
                    //       M' ... M
                    lr.l = mid;
                    i = i * 2 + 1;
                }
                */
                *(&lr.l + grow) = mid;
                i = i * 2 + (!grow);
            } else {

                LOGREAM_PREFETCH(&sa[mid], 0, 1);
                LOGREAM_PREFETCH(pattern.data() + matches, 0, 1);
                LOGREAM_PREFETCH (src + sa[mid] + matches, 0, 1);

                // L ... M ... R
                //       |
                matches = compare_to(mid, matches);
                /*
                if (grow) {
                    lr.l = mid;
                    i = i * 2 + 1;
                    // M ... M' ... R
                    // |-----|
                    // |------------|
                } else {
                    lr.r = mid;
                    i = i * 2;
                    // L ... M' ... M
                    //       |------|
                    // |------------|
                }
                */
                *(&lr.l + (!grow)) = mid;
                i = i * 2 + grow;
            }

            LOGREAM_PREFETCH(&lcplr[i * 2], 0, 1);

            if (lr.r - lr.l <= 2) {
                break;
            }
            /*
            if (grow) {
                commons = lcplr[i * 2];
            } else {
                commons = lcplr[i * 2 + 1];
            }
            */
            commons = lcplr[i * 2 + (!grow)];
        }

        size_t pos = 0;
        size_t len = 0;
        for (int j = lr.l; j <= lr.r; ++j) {
            auto from = sa[j];
            const auto * target = src + from;
            size_t common_prefix = std::mismatch(pattern.data(),
                                                 pattern.data() + std::min(m, n - from),
                                                 target).first - pattern.data();
            if (common_prefix > len) {
                pos = static_cast<size_t>(from);
                len = common_prefix;
            }
        }
        return {pos, len};
    }

    std::pair<size_t, size_t>
    WriterCompress::FindLongestRepeat(const Slice & s, size_t before) {
        const char * target = s.data() + before;
        std::string_view view(s.data(), s.size());
        size_t begin = before > kFrontlineSize ? (kFrontlineSize - before) : 0;
        size_t i = 0;
        for (; i < view.size(); ++i) {
            auto pos = view.find({target, i + 1}, begin);
            if (pos == std::string_view::npos || pos >= before) {
                break;
            }
            begin = pos;
        }
        assert(Slice(s.data() + begin, i) == Slice(s.data() + before, i));
        return {before - begin - 1, i};
    }

    // Kasai's Algorithm
    // https://www.geeksforgeeks.org/%C2%AD%C2%ADkasais-algorithm-for-construction-of-lcp-array-from-suffix-array/
    void WriterCompress::BuildLCP(const unsigned char * src, const std::vector<int> & sa,
                                  std::vector<int> * inverse_sa, std::vector<int> * lcp) {
        std::vector<int> & isa = *inverse_sa;
        std::vector<int> & lcp_arr = *lcp;

        isa.resize(sa.size());
        for (int i = 0; i < sa.size(); ++i) {
            isa[sa[i]] = i;
        }

        int p = 0;
        lcp_arr.resize(sa.size());
        for (int i = 0; i < sa.size(); ++i) {
            if (isa[i] == (sa.size() - 1)) {
                p = 0;
                lcp_arr.back() = INT_MAX;
                continue;
            }

            int j = sa[isa[i] + 1];
            while (i + p < sa.size() && j + p < sa.size() && src[i + p] == src[j + p]) {
                ++p;
            }

            lcp_arr[isa[i]] = p;
            if (p > 0) {
                --p;
            }
        }
    }

    void WriterCompress::BuildLCPLR(const std::vector<int> & lcp, std::vector<int> * lcplr) {
        std::vector<int> & lcp_lr = *lcplr;

        auto build = [&lcp, &lcp_lr](int i, int l, int r, auto && func) -> std::pair<int, int> {
            int common_prefix;
            int range_min;
            int gap = r - l;
            switch (gap) {
                case 1: {
                    common_prefix = lcp[l];
                    range_min = std::min(common_prefix, lcp[r]);
                    break;
                }

                case 2: {
                    common_prefix = std::min(lcp[l], lcp[l + 1]);
                    range_min = std::min(common_prefix, lcp[r]);
                    break;
                }

                default: {
                    assert(gap > 2);
                    int j = i * 2;
                    int k = j + 1;
                    int m = (l + r) / 2;

                    auto a = func(j, l, m, func);
                    auto b = func(k, m, r, func);
                    common_prefix = std::min(a.second, b.first);
                    range_min = std::min(a.second, b.second);
                    break;
                }
            }
            lcp_lr[i] = common_prefix;
            return {common_prefix, range_min};
        };

        lcp_lr.resize(lcp.size());
        build(1, 0, static_cast<int>(lcp.size()) - 1, build);
    }

    void WriterCompress::BuildBloomFilter(const unsigned char * src, size_t n, size_t min_repeat,
                                          std::string * bloom_filter) {
        const unsigned char * p = src;
        const unsigned char * limit = src + n;
        BloomFilter().CreateFilter([&p, min_repeat, limit](Slice * s) {
            if (p + min_repeat > limit) {
                return false;
            }
            *s = {p++, min_repeat};
            return true;
        }, n, bloom_filter);
    }

    size_t ReaderCompress::Get(size_t id, std::string * s) const {
        auto & b = const_cast<std::string &>(backup_);
        b.resize(kMaxVarint32Length);
        helper_->ReadAt(id, kMaxVarint32Length, b.data());
        Slice buf(b);
        uint32_t size;
        if (!GetVarint32(&buf, &size)) {
            return 0;
        }

        const size_t varint_size = b.size() - buf.size();
        const size_t data_size = varint_size + size;
        const size_t read_size = data_size + sizeof(uint32_t);
        b.resize(read_size);
        helper_->ReadAt(id + kMaxVarint32Length,
                        read_size - kMaxVarint32Length,
                        &b[kMaxVarint32Length]);

        uint32_t crc;
        memcpy(&crc, &b[data_size], sizeof(crc));
        crc = crc32c::Unmask(crc);
        buf = {&b[varint_size], size};

        auto read_plain = [&]() -> size_t {
            if (crc32c::Value(buf.data(), buf.size()) != crc) {
                return 0;
            }
            s->append(buf.data(), buf.size());
            return id + read_size;
        };

        auto read_compressed = [&](size_t battlefield_pos) -> size_t {
            const size_t dst_size = s->size();
            const char * p = buf.data();
            const char * limit = p + buf.size();
            while (p != limit) {

                auto mark = CharToUint8(*p++);
                if (mark >= kNormal && mark <= kNormalClose) {
                    uint32_t len = mark - kNormal;
#define LOAD_LEN()                                         \
                    if (len == 0) {                        \
                        Slice cursor(p, limit - p);        \
                        if (!GetVarint32(&cursor, &len)) { \
                            return 0;                      \
                        }                                  \
                        p = cursor.data();                 \
                    }
                    LOAD_LEN();

                    s->append(p, len);
                    p += len;
                } else {
                    uint32_t len = 0;
                    uint32_t pos = 0;

                    if (mark >= kWarZone && mark <= kWarZoneClose) {
                        len = mark - kWarZone;
                        LOAD_LEN();
#define LOAD_POS(n)                           \
                        memcpy(&pos, p, (n)); \
                        p += (n);
                        LOAD_POS(3);
#define LOAD_DAT(o)                                                     \
                        size_t i = s->size();                           \
                        s->resize(i + len);                             \
                        helper_->ReadAt(pos + (o), len, s->data() + i);
                        LOAD_DAT(0);
                    } else if (mark >= kBattlefield && mark <= kBattlefieldClose) {
                        len = mark - kBattlefield;
                        LOAD_LEN();
                        LOAD_POS(2);
                        LOAD_DAT(battlefield_pos);
                    } else {
                        assert(mark >= kFrontline && mark <= kFrontlineClose);
                        len = mark - kFrontline;
                        LOAD_LEN();
                        LOAD_POS(1);

                        size_t idx = s->size() - (pos + 1);
                        for (size_t i = 0; i < len; ++i) {
                            (*s) += (*s)[idx++];
                        }
                    }
                }
#undef LOAD_LEN
#undef LOAD_POS
#undef LOAD_DAT
            }

            if (crc32c::Value(s->data() + dst_size, s->size() - dst_size) != crc) {
                return 0;
            }
            return id + read_size;
        };

        const size_t n_war_zone = id / kWarZoneSize;
        const size_t war_zone_r = id % kWarZoneSize;
        switch (n_war_zone) {
            case 0:
                return read_plain();

            default: {
                const size_t n_battlefield = war_zone_r / kBattlefieldSize;
                switch (n_battlefield) {
                    case 0:
                        return read_plain();

                    default:
                        return read_compressed(id - war_zone_r);
                }
            }
        }
    }
}