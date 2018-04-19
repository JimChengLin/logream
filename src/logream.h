#pragma once
#ifndef LOGREAM_LOGREAM_H
#define LOGREAM_LOGREAM_H

/*
 * 作者: 左吉吉
 * 发布协议: AGPL
 * 完成时间: 2018夏初
 */

#include "slice.h"

namespace logream {
    class Writer {
    public:
        class Helper {
        public:
            Helper() = default;

            virtual ~Helper() = default;

        public:
            virtual void Write(const Slice & s) = 0;
        };

    public:
        Writer() = default;

        virtual ~Writer() = default;

    public:
        virtual size_t Add(const char * data, size_t * n) = 0;
    };

    class Reader {
    public:
        class Helper {
        public:
            Helper() = default;

            virtual ~Helper() = default;

        public:
            virtual void ReadAt(size_t offset, size_t n, char * scratch) const = 0;
        };

    public:
        Reader() = default;

        virtual ~Reader() = default;

    public:
        // Return 0 on error
        virtual size_t Get(size_t id, std::string * s) const = 0;
    };
}

#endif //LOGREAM_LOGREAM_H
