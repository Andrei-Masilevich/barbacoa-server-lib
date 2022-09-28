#pragma once

#include <boost/asio.hpp>
#include <boost/optional.hpp>
#include <functional>
#include <atomic>

#include <server_lib/asserts.h>

namespace server_lib {

class start_observable_type;
class stop_observable_type;

class base_queuered_loop
{
public:
    using callback_type = std::function<void(void)>;

protected:
    template <typename Handler>
    callback_type register_queue(Handler&& callback)
    {
        SRV_ASSERT(_pservice);
        auto callback_ = [this, callback = std::move(callback)]() mutable {
            callback();
            std::atomic_fetch_sub<uint64_t>(&this->_queue_size, 1);
        };
        std::atomic_fetch_add<uint64_t>(&_queue_size, 1);
        return callback_;
    }

    void set_thread_name(const std::string&);
    void apply_thread_name();

protected:
    base_queuered_loop();

public:
    virtual ~base_queuered_loop();

    operator boost::asio::io_service&()
    {
        SRV_ASSERT(_pservice);
        return *_pservice;
    }

    std::shared_ptr<boost::asio::io_service> service() const
    {
        return _pservice;
    }

    auto queue_size() const
    {
        return _queue_size.load();
    }

    bool is_running() const
    {
        return _is_running;
    }

protected:
    void start_loop();

    void stop_loop();

    void on_start_loop(callback_type&& callback);

    void on_stop_loop(callback_type&& callback);

    void run();

    virtual void reset();

    void notify_start();

    void notify_stop();

private:
    std::string _base_name = "io_service";
    std::atomic_bool _is_running;
    std::shared_ptr<boost::asio::io_service> _pservice;
    boost::optional<boost::asio::io_service::work> _loop_maintainer;
    std::atomic_uint64_t _queue_size;
    std::unique_ptr<start_observable_type> _start_observer;
    std::unique_ptr<stop_observable_type> _stop_observer;
};

} // namespace server_lib
