#pragma once
#ifndef LOGREAM_LOGREAM_LITE_IMPL_H
#define LOGREAM_LOGREAM_LITE_IMPL_H

// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

/*
 * 不进行压缩, 以极限速度将数据写入且线程安全
 *
 * 单记录最大长度: 64KB 格式: varint + data + crc32c
 */

#include <condition_variable>
#include <deque>
#include <mutex>

#include "logream.h"

namespace logream {
    class WriterLite : public Writer {
    private:
        Helper * const helper_;
        size_t cursor_;
        std::string backup_;

        struct Writer {
            Slice s;
            size_t pos;
            size_t len;
            std::condition_variable cv;
            std::exception_ptr eptr;
            bool done;

            explicit Writer(const Slice & slice)
                    : s(slice),
                      pos(0),
                      len(0),
                      done(false) {}
        };

        std::deque<Writer *> writers_;
        std::mutex mutex_;

    public:
        WriterLite(Helper * helper, size_t cursor)
                : helper_(helper),
                  cursor_(cursor) {}

        WriterLite(const WriterLite &) = delete;

        WriterLite & operator=(const WriterLite &) = delete;

        ~WriterLite() override = default;

    public:
        size_t Add(const char * data, size_t * n) override;

    private:
        static size_t PutPlain(const Slice & s, std::string * dst);
    };

    class ReaderLite : public Reader {
    private:
        Helper * const helper_;

    public:
        explicit ReaderLite(Helper * helper)
                : helper_(helper) {}

        ReaderLite(const ReaderLite &) = delete;

        ReaderLite & operator=(const ReaderLite &) = delete;

        ~ReaderLite() override = default;

    public:
        size_t Get(size_t id, std::string * s) const override;
    };
}

#endif //LOGREAM_LOGREAM_LITE_IMPL_H
