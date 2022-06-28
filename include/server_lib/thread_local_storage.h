#pragma once

#include <thread>
#include <memory>
#include <map>
#include <mutex>
#include <atomic>
#include <vector>

namespace server_lib {

/**
 * \ingroup common
 *
 * This class is Thread-local storage (TLS) cross platform emulation
 * for set of buffers
 */
class thread_local_storage
{
    using buffers_type = unsigned char;
    using buffers_storage_type = std::vector<buffers_type>;
    using buffers_by_thread_type = std::map<std::thread::id, buffers_storage_type>;

public:
    thread_local_storage(uint8_t buffers_count);

    size_t size(uint8_t buff_idx = 0) const;

    // create or reinitialize buffer dedicated to calling thread
    buffers_type* create(size_t sz, uint8_t buff_idx = 0);
    // get buffer dedicated to calling thread
    buffers_type* get(uint8_t buff_idx = 0);
    // resize buffer dedicated to calling thread
    void resize(size_t new_sz, uint8_t buff_idx = 0);
    // delete buffer dedicated to calling thread.
    // You should call 'create' to get this buffer again
    void remove(uint8_t buff_idx = 0);

private:
    mutable std::mutex _tid_mutex;
    std::vector<buffers_by_thread_type> _buffers;
    std::atomic<uint8_t> _buffers_count;
};
} // namespace server_lib
