#include <server_lib/thread_local_storage.h>
#include <server_lib/asserts.h>

#include <memory.h>

namespace server_lib {

thread_local_storage::thread_local_storage(uint8_t buffers_count)
    : _buffers(buffers_count)
    , _buffers_count(buffers_count)
{
    SRV_ASSERT(_buffers.size() > 0);
}

size_t thread_local_storage::size(uint8_t buff_idx) const
{
    SRV_ASSERT(buff_idx < _buffers_count.load(), "Unregistered buffer");

    auto tid = std::this_thread::get_id();

    const std::lock_guard<std::mutex> lock(_tid_mutex);
    auto& storage = _buffers[buff_idx];
    auto it = storage.find(tid);
    SRV_ASSERT(it != storage.end());
    return it->second.size();
}

thread_local_storage::buffers_type* thread_local_storage::create(size_t sz, uint8_t buff_idx)
{
    SRV_ASSERT(buff_idx < _buffers_count.load(), "Unregistered buffer");

    auto tid = std::this_thread::get_id();

    const std::lock_guard<std::mutex> lock(_tid_mutex);
    auto& storage = _buffers[buff_idx];
    auto it = storage.find(tid);
    if (storage.end() == it)
    {
        it = storage.emplace(std::piecewise_construct, std::forward_as_tuple(tid), std::forward_as_tuple(sz)).first;
    }
    else
    {
        it->second.resize(sz);
        memset(it->second.data(), 0, sz);
    }

    return it->second.data();
}

thread_local_storage::buffers_type* thread_local_storage::create(const string_ref& src, uint8_t buff_idx)
{
    SRV_ASSERT(buff_idx < _buffers_count.load(), "Unregistered buffer");

    auto tid = std::this_thread::get_id();

    const std::lock_guard<std::mutex> lock(_tid_mutex);
    auto& storage = _buffers[buff_idx];
    auto it = storage.find(tid);
    if (storage.end() == it)
    {
        it = storage.emplace(std::piecewise_construct, std::forward_as_tuple(tid), std::forward_as_tuple(src.size())).first;
    }
    else
    {
        it->second.resize(src.size());
    }
    memcpy(it->second.data(), src.data(), src.size());

    return it->second.data();
}

thread_local_storage::buffers_type* thread_local_storage::get(uint8_t buff_idx)
{
    SRV_ASSERT(buff_idx < _buffers_count.load(), "Unregistered buffer");

    auto tid = std::this_thread::get_id();

    const std::lock_guard<std::mutex> lock(_tid_mutex);
    auto& storage = _buffers[buff_idx];
    auto it = storage.find(tid);
    SRV_ASSERT(storage.end() != it, "Not found");
    return it->second.data();
}

void thread_local_storage::resize(size_t new_sz, uint8_t buff_idx)
{
    SRV_ASSERT(buff_idx < _buffers_count.load(), "Unregistered buffer");
    SRV_ASSERT(new_sz > 0, "Invalid size");

    auto tid = std::this_thread::get_id();

    const std::lock_guard<std::mutex> lock(_tid_mutex);
    auto& storage = _buffers[buff_idx];
    auto it = storage.find(tid);
    SRV_ASSERT(storage.end() != it, "Not found");
    it->second.resize(new_sz);
}

void thread_local_storage::remove(uint8_t buff_idx)
{
    SRV_ASSERT(buff_idx < _buffers_count.load(), "Unregistered buffer");

    auto tid = std::this_thread::get_id();

    const std::lock_guard<std::mutex> lock(_tid_mutex);
    auto& storage = _buffers[buff_idx];
    auto it = storage.find(tid);
    if (storage.end() != it)
        storage.erase(it);
}

string_ref thread_local_storage::get_ref(uint8_t buff_idx)
{
    SRV_ASSERT(buff_idx < _buffers_count.load(), "Unregistered buffer");

    auto tid = std::this_thread::get_id();

    const std::lock_guard<std::mutex> lock(_tid_mutex);
    auto& storage = _buffers[buff_idx];
    auto it = storage.find(tid);
    if (storage.end() == it)
        return {};

    return { reinterpret_cast<char*>(it->second.data()), it->second.size() };
}

} // namespace server_lib
