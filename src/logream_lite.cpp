#include "coding.h"
#include "crc32c.h"
#include "logream_lite.h"

namespace logream {
    size_t WriterLite::Add(const char * data, size_t * n) {
        Writer w({data, *n});
        std::unique_lock l(mutex_);
        writers_.emplace_back(&w);
        w.cv.wait(l, [&]() {
            return w.done || &w == writers_.front();
        });

        // follower
        if (w.done) {
            if (w.eptr != nullptr) {
                std::rethrow_exception(w.eptr);
            } else {
                *n = w.len;
                return w.pos;
            }
        }

        // leader
        backup_.clear();
        Writer * last_writer = &w;
        for (Writer * writer:writers_) {
            writer->pos = cursor_ + backup_.size();
            writer->len = PutPlain(writer->s, &backup_);
            last_writer = writer;
        }

        {
            mutex_.unlock();
            try {
                helper_->Write(backup_);
                cursor_ += backup_.size();
            } catch (const std::exception & e) {
                w.eptr = std::current_exception();
            }
            mutex_.lock();
        }

        while (true) {
            Writer * ready = writers_.front();
            writers_.pop_front();
            if (ready != &w) {
                if (w.eptr != nullptr) {
                    ready->eptr = w.eptr;
                }
                ready->done = true;
                ready->cv.notify_one();
            }
            if (ready == last_writer) {
                break;
            }
        }
        if (!writers_.empty()) {
            writers_.front()->cv.notify_one();
        }

        if (w.eptr != nullptr) {
            std::rethrow_exception(w.eptr);
        } else {
            *n = w.len;
            return w.pos;
        }
    }

    size_t WriterLite::PutPlain(const Slice & s, std::string * dst) {
        const size_t old_size = dst->size();
        const size_t request = VarintLength(s.size()) + s.size() + sizeof(uint32_t);
        dst->resize(old_size + request);

        char * d = EncodeVarint32(dst->data() + old_size, static_cast<uint32_t>(s.size()));
        memcpy(d, s.data(), s.size());
        d += s.size();

        uint32_t crc = crc32c::Mask(crc32c::Value(s.data(), s.size()));
        memcpy(d, &crc, sizeof(crc));
        return request;
    }

    size_t ReaderLite::Get(size_t id, std::string * s) const {
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

        if (crc32c::Value(buf.data(), buf.size()) != crc) {
            return 0;
        }
        s->append(buf.data(), buf.size());
        return id + read_size;
    }
}