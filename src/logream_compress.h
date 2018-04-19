#pragma once
#ifndef LOGREAM_LOGREAM_COMPRESS_IMPL_H
#define LOGREAM_LOGREAM_COMPRESS_IMPL_H

/*
 * 对数据进行压缩, 同时支持按条(ID)随机读取
 *
 * 单记录最大长度: 64KB 格式: varint + data + crc32c
 *
 * 每 16MB 作为一个战区, 第一个战区被称为"首战区", 不进行任何压缩
 * 除了首战区之外的任何可压缩记录, 都可以用 3bytes 的地址索引首战区
 *
 * 战区中每 64KB 作为一个战场, 第一个战场被称为"首战场", 不进行任何压缩
 * 战区中除了首战场之外的任何记录, 都可以用 2Bytes 的地址索引首战场
 *
 * 每条记录被称为"前线", 都可以用 1byte 的偏移量索引自身滑动窗口
 *
 * 压缩时进行3次比较:
 * 1. 首战区引用
 * 2. 首战场引用
 * 3. 滑动窗口引用
 * 取最有"利润"的选项, 若无利润, 直接写入原数据
 */

#include <vector>

#include "logream.h"

namespace logream {
    constexpr unsigned int kWarZoneSize = 16777216;  // 16MB = 2 ** 24
    constexpr unsigned int kBattlefieldSize = 65536; // 64KB = 2 ** 16
    constexpr unsigned int kFrontlineSize = 256;     // 256bytes = 2 ** 8

    class WriterCompress : public Writer {
    private:
        Helper * const helper_;
        size_t cursor_;
        std::string backup_;
        std::string war_zone_;
        std::string battlefield_;
        std::vector<int> war_zone_sa_;
        std::vector<int> battlefield_sa_;

    public:
        WriterCompress(Helper * helper, size_t cursor)
                : helper_(helper),
                  cursor_(cursor) {}

        WriterCompress(const WriterCompress &) = delete;

        WriterCompress & operator=(const WriterCompress &) = delete;

        ~WriterCompress() override = default;

    public:
        size_t Add(const char * data, size_t * n) override;

    private:
        enum {
            kMinRepeat = 3,
            kMinRepeatBattlefield = kMinRepeat + 1,
            kMinRepeatWarZone = kMinRepeatBattlefield + 1
        };

        Slice GeneratePlain(const Slice & s);

        Slice GenerateCompressed(const Slice & s);

        void Write(const Slice & s);

        static void BuildSA(const unsigned char * src,
                            std::vector<int> * sa,
                            std::vector<int> * lcp, std::vector<int> * lcplr,
                            std::string * bloom_filter, size_t min_repeat,
                            size_t n);

        std::pair<size_t /* pos */, size_t /* len */>
        static FindLongestRepeat(const char * src, const std::vector<int> & sa,
                                 const Slice & pattern,
                                 size_t min_repeat,
                                 const std::vector<int> & lcplr,
                                 const std::string & bloom_filter);

        std::pair<size_t, size_t>
        static FindLongestRepeat(const Slice & s, size_t before);

    private:
        std::vector<int> lcp_;
        std::vector<int> war_zone_lcplr_;
        std::vector<int> battlefield_lcplr_;
        std::string war_zone_bloom_filter_;
        std::string battlefield_bloom_filter_;

        static void BuildLCP(const unsigned char * src, const std::vector<int> & sa,
                             std::vector<int> * inverse_sa, std::vector<int> * lcp);

        static void BuildLCPLR(const std::vector<int> & lcp, std::vector<int> * lcplr);

        static void BuildBloomFilter(const unsigned char * src, size_t n, size_t min_repeat,
                                     std::string * bloom_filter);
    };

    class ReaderCompress : public Reader {
    private:
        Helper * const helper_;
        std::string backup_;

    public:
        explicit ReaderCompress(Helper * helper)
                : helper_(helper) {}

        ReaderCompress(const ReaderCompress &) = delete;

        ReaderCompress & operator=(const ReaderCompress &) = delete;

        ~ReaderCompress() override = default;

    public:
        size_t Get(size_t id, std::string * s) const override;
    };
}

#endif //LOGREAM_LOGREAM_COMPRESS_IMPL_H
