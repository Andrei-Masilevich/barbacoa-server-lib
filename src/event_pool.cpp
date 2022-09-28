#include <server_lib/event_pool.h>

#include <server_lib/thread_sync_helpers.h>

#include "logger_set_internal_group.h"

namespace server_lib {

event_pool::event_pool(uint8_t nb_threads)
    : _nb_threads(nb_threads)
{
    SRV_ASSERT(nb_threads > 0, "Threads are required");

    change_pool_name("io_service pool");

    SRV_LOGC_TRACE(SRV_FUNCTION_NAME_ << " in thread pool " << this->nb_threads());

    _thread_ids.reserve(this->nb_threads());

    reset();
}

event_pool::~event_pool()
{
    try
    {
        SRV_LOGC_TRACE(SRV_FUNCTION_NAME_);

        stop();
    }
    catch (const std::exception& e)
    {
        SRV_LOGC_ERROR(e.what());
    }
}

event_pool& event_pool::change_pool_name(const std::string& name)
{
    base_class::set_thread_name(name);
    SRV_ASSERT(!is_running(), "Not implemented for already runned");
    return *this;
}

event_pool& event_pool::start()
{
    if (is_running())
        return *this;

    SRV_LOGC_INFO(SRV_FUNCTION_NAME_);

    try
    {
        base_class::start_loop();

        // _waiting_threads = nb_threads
        _waiting_threads = nb_threads();

        SRV_LOGC_TRACE("Event pool is starting and waiting for " << this->_waiting_threads << " threads");

        service()->post([this]() {
            size_t waiting_threads = 0;
            // Check for _waiting_threads != 0
            while (!this->_waiting_threads.compare_exchange_weak(waiting_threads, 0))
            {
                if (!waiting_threads)
                    return;

                // Restore expected value
                waiting_threads = 0;
                spin_loop_pause();
            }

            SRV_LOGC_TRACE("Event pool has started");

            _is_run = true;

            base_class::notify_start();
        });

        for (size_t ci = 0; ci < _nb_threads; ++ci)
        {
            _threads.emplace_back(new std::thread([this]() {
                {
                    std::lock_guard<std::mutex> lock(_thread_ids_mutex);
                    _thread_ids.emplace_back(std::this_thread::get_id());
                }
                // _waiting_threads -= 1
                std::atomic_fetch_sub<size_t>(&_waiting_threads, 1);
                base_class::run();
            }));
        }
    }
    catch (const std::exception& e)
    {
        SRV_LOGC_ERROR(e.what());
        stop();

        SRV_THROW();
    }

    return *this;
}

void event_pool::stop()
{
    if (!is_running())
        return;

    SRV_LOGC_TRACE(SRV_FUNCTION_NAME_);

    try
    {
        base_class::stop_loop();

        for (auto&& thread : _threads)
        {
            thread->join();
        }
    }
    catch (const std::exception& e)
    {
        SRV_LOGC_ERROR(e.what());

        SRV_THROW();
    }

    _thread_ids.clear();
    _threads.clear();
    reset();
}

bool event_pool::is_this_loop() const
{
    std::lock_guard<std::mutex> lock(_thread_ids_mutex);
    for (auto&& thread_id : _thread_ids)
    {
        if (thread_id == std::this_thread::get_id())
            return true;
    }
    return false;
}

event_pool& event_pool::on_start(callback_type&& callback)
{
    base_class::on_start_loop(std::move(callback));
    return *this;
}

event_pool& event_pool::on_stop(callback_type&& callback)
{
    base_class::on_stop_loop(std::move(callback));
    return *this;
}

void event_pool::reset()
{
    base_class::reset();
    _is_run = false;
    _waiting_threads = 0;
}

} // namespace server_lib
