#include <server_lib/event_loop.h>
#include <server_lib/simple_observer.h>

#include <boost/utility/in_place_factory.hpp>

#if defined(SERVER_LIB_PLATFORM_LINUX)
#include <pthread.h>
#include <sys/syscall.h>
#endif

#include "logger_set_internal_group.h"

namespace server_lib {

#if !defined(SERVER_LIB_PLATFORM_LINUX)
static std::thread::id MAIN_THREAD_ID = std::this_thread::get_id();
#endif

event_loop::event_loop(bool in_separate_thread /*= true*/)
    : _run_in_separate_thread(in_separate_thread)
{
    change_loop_name("io_service loop");

    if (_run_in_separate_thread)
    {
        SRV_LOGC_TRACE(SRV_FUNCTION_NAME_ << " in separate thread");
    }
    else
    {
        SRV_LOGC_TRACE(SRV_FUNCTION_NAME_);
    }

    reset();
}

event_loop::~event_loop()
{
    try
    {
        if (_run_in_separate_thread)
        {
            SRV_LOGC_TRACE(SRV_FUNCTION_NAME_ << " in separate thread");
        }
        else
        {
            SRV_LOGC_TRACE(SRV_FUNCTION_NAME_);
        }

        stop();
    }
    catch (const std::exception& e)
    {
        SRV_LOGC_ERROR(e.what());
    }
}

void event_loop::reset()
{
    base_class::reset();
    _is_run = false;
    _id = std::this_thread::get_id();
    _native_thread_id = 0l;

    _pstrand = std::make_unique<boost::asio::io_service::strand>(*service());
    if (_run_in_separate_thread)
        _thread.reset();
}

bool event_loop::is_main_thread()
{
#if defined(SERVER_LIB_PLATFORM_LINUX)
    return getpid() == syscall(SYS_gettid);
#else
    return std::this_thread::get_id() == MAIN_THREAD_ID;
#endif
}

event_loop& event_loop::change_loop_name(const std::string& name)
{
    base_class::set_thread_name(name);
    if (!_run_in_separate_thread)
    {
        apply_thread_name();
    }
    else if (is_running())
    {
        post([this]() {
            apply_thread_name();
        });
    }
    return *this;
}

event_loop& event_loop::start()
{
    if (is_running())
        return *this;

    SRV_LOGC_INFO(SRV_FUNCTION_NAME_);

    try
    {
        base_class::start_loop();

        post([this]() {
            SRV_LOGC_TRACE("Event loop has started");

            _is_run = true;

            notify_start();
        });

        if (_run_in_separate_thread)
        {
            _thread.reset(new std::thread([this]() {
#if defined(SERVER_LIB_PLATFORM_LINUX)
                _native_thread_id = syscall(SYS_gettid);
#elif defined(SERVER_LIB_PLATFORM_WINDOWS)
                _native_thread_id = ::GetCurrentThreadId();
#else
                _native_thread_id = static_cast<long>(std::this_thread::get_id().native_handle());
#endif
                base_class::run();
            }));
        }
        else
        {
            base_class::run();
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

void event_loop::stop()
{
    if (!is_running())
        return;

    SRV_LOGC_TRACE(SRV_FUNCTION_NAME_);

    try
    {
        base_class::stop_loop();

        if (_thread && _thread->joinable())
        {
            _thread->join();
        }
    }
    catch (const std::exception& e)
    {
        SRV_THROW();
    }

    reset();
}

event_loop& event_loop::on_start(callback_type&& callback)
{
    base_class::on_start_loop(std::move(callback));
    return *this;
}

event_loop& event_loop::on_stop(callback_type&& callback)
{
    base_class::on_stop_loop(std::move(callback));
    return *this;
}

void event_loop::wait()
{
    // TODO: Check!!!
    if (!is_running())
    {
        wait([]() {});
    }
}

} // namespace server_lib
