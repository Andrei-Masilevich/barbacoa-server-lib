#include <server_lib/base_queuered_loop.h>

#include <server_lib/simple_observer.h>

#include <boost/utility/in_place_factory.hpp>

#include "logger_set_internal_group.h"

namespace server_lib {

class start_observable_type : public protected_observable<base_queuered_loop::callback_type>
{
};

class stop_observable_type : public protected_observable<base_queuered_loop::callback_type>
{
};


void base_queuered_loop::set_thread_name(const std::string& name)
{
    static int MAX_THREAD_NAME_SZ = 15;
    if (name.length() > MAX_THREAD_NAME_SZ)
        _base_name = name.substr(0, MAX_THREAD_NAME_SZ);
    else
        _base_name = name;
}

void base_queuered_loop::apply_thread_name()
{
#if defined(SERVER_LIB_PLATFORM_LINUX)
    SRV_ASSERT(0 == pthread_setname_np(pthread_self(), _base_name.c_str()));
#endif
}

base_queuered_loop::base_queuered_loop()
{
    _is_running = false;
    _queue_size = 0;
}

base_queuered_loop::~base_queuered_loop()
{
}

void base_queuered_loop::start_loop()
{
    if (is_running())
        return;

    SRV_LOGC_INFO(SRV_FUNCTION_NAME_);

    try
    {
        _is_running = true;

        // The 'work' object guarantees that 'run()' function
        // will not exit while work is underway, and that it does exit
        // when there is no unfinished work remaining.
        // It is quite like infinite loop (in CCU safe manner of course)
        _loop_maintainer = boost::in_place(std::ref(*_pservice));
    }
    catch (const std::exception& e)
    {
        SRV_LOGC_ERROR(e.what());
        stop_loop();

        SRV_THROW();
    }
}

void base_queuered_loop::stop_loop()
{
    if (!is_running())
        return;

    SRV_LOGC_TRACE(SRV_FUNCTION_NAME_);

    try
    {
        _is_running = false;

        _loop_maintainer = boost::none; // finishing 'infinite loop'
        // It will stop all threads (exit from all 'run()' functions)
        _pservice->stop();
    }
    catch (const std::exception& e)
    {
        SRV_LOGC_ERROR(e.what());

        SRV_THROW();
    }
}

void base_queuered_loop::on_start_loop(callback_type&& callback)
{
    SRV_ASSERT(_start_observer);
    _start_observer->subscribe(std::forward<callback_type>(callback));
}

void base_queuered_loop::on_stop_loop(callback_type&& callback)
{
    SRV_ASSERT(_stop_observer);
    _stop_observer->subscribe(std::forward<callback_type>(callback));
}

void base_queuered_loop::run()
{
    try
    {
        SRV_ASSERT(_pservice);

        apply_thread_name();

        SRV_LOGC_TRACE("Event loop is starting");

        _pservice->run();

        SRV_LOGC_TRACE("Event loop has stopped");

        notify_stop();
    }
    catch (const std::exception& e)
    {
        SRV_LOGC_ERROR(e.what());

        SRV_THROW();
    }
}

void base_queuered_loop::reset()
{
    _is_running = false;
    _start_observer = std::make_unique<start_observable_type>();
    _stop_observer = std::make_unique<stop_observable_type>();
    _pservice = std::make_shared<boost::asio::io_service>();
    _queue_size = 0;
}

void base_queuered_loop::notify_start()
{
    SRV_ASSERT(_start_observer);
    _start_observer->notify();
}

void base_queuered_loop::notify_stop()
{
    SRV_ASSERT(_stop_observer);
    _stop_observer->notify();
}

} // namespace server_lib
