#include <fstream>
#include <iostream>
#include <thread>
#include <vector>

#include "../src/logream_lite.h"

namespace logream::lite_bench {
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
        constexpr unsigned int kThreadNum = 4;
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
        }

        WriterHelper w_helper;
        WriterLite writer(&w_helper, 0);
        {
            TIME_START;
            std::vector<std::thread> jobs;
            for (size_t i = 0; i < kThreadNum; ++i) {
                jobs.emplace_back([&](size_t nth) {
                    for (size_t j = 0; j < src.size(); ++j) {
                        if (j % kThreadNum == nth) {
                            size_t n = src[j].size();
                            writer.Add(src[j].data(), &n);
                        }
                    }
                }, i);
            }
            for (auto & job:jobs) {
                job.join();
            }
            TIME_END;
            PRINT_TIME(WriterLite - Add);
        }

        size_t total = 0;
        ReaderHelper r_helper(w_helper.mem_);
        ReaderLite reader(&r_helper);
        {
            TIME_START;
            size_t id = 0;
            std::string out;
            for (const auto & s:src) {
                id = reader.Get(id, &out);
                total += out.size();
                out.clear();
            }
            TIME_END;
            PRINT_TIME(ReaderLite - Get);
        }
        std::cout << "text_size: " << total << std::endl;
    }
}