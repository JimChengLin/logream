#include <chrono>
#include <fstream>
#include <iostream>

#include "../src/logream_compress.h"

namespace logream::compress_bench {
    class WriterHelper : public Writer::Helper {
    public:
        std::string mem_;

    public:
        ~WriterHelper() override = default;

    public:
        void Write(const Slice & s) override {
            mem_.append(s.data(), s.size());
        }
    };

    class ReaderHelper : public Reader::Helper {
    public:
        Slice s_;

    public:
        explicit ReaderHelper(const std::string & s)
                : s_(s) {}

        ~ReaderHelper() override = default;

    public:
        void ReadAt(size_t offset, size_t n, char * scratch) const override {
            memcpy(scratch, &s_[offset], n);
        }
    };

#define TIME_START auto start = std::chrono::high_resolution_clock::now()
#define TIME_END auto end = std::chrono::high_resolution_clock::now()
#define PRINT_TIME(name) \
std::cout << #name " took " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << " milliseconds" << std::endl

    void Run() {
        constexpr unsigned int kTestTimes = 100000;
        constexpr const char kPath[] = "/Users/yuanjinlin/Desktop/movies.txt";

        std::vector<std::string> src(kTestTimes);
        if (std::ifstream f(kPath); f.good()) {
            size_t i = 0;
            size_t j = 0;
            for (std::string line; std::getline(f, line);) {
                src[i] += line;
                if (++j == 9) {
                    j = 0;
                    if (++i == kTestTimes) {
                        break;
                    }
                }
            }
        } else {
            return;
        }

        size_t w_total = 0;
        WriterHelper w_helper;
        WriterCompress writer(&w_helper, 0);
        {
            TIME_START;
            for (const auto & s:src) {
                size_t n = s.size();
                w_total += n;
                writer.Add(s.data(), &n);
            }
            TIME_END;
            PRINT_TIME(WriterCompress - Add);
        }
        std::cout << "original_size: " << w_total << std::endl;
        std::cout << "compress_size: " << w_helper.mem_.size() << std::endl;

        size_t r_total = 0;
        ReaderHelper r_helper(w_helper.mem_);
        ReaderCompress reader(&r_helper);
        {
            TIME_START;
            size_t id = 0;
            std::string out;
            for (const auto & s:src) {
                id = reader.Get(id, &out);
                assert(out == s);
                r_total += out.size();
                out.clear();
            }
            TIME_END;
            PRINT_TIME(ReaderCompress - Get);
        }
        std::cout << "uncompress_size: " << r_total << std::endl;
    }
}