#pragma once

#include <server_lib/base_queuered_loop.h>

#include <thread>

#include <server_lib/types.h>
#include <server_lib/timers.h>

#include "wait_asynch_request.h"

namespace server_lib {

DECLARE_PTR(event_loop);

/**
 * \ingroup common
 *
 * \brief This class provides threaded io-service
 * with the guarantee that none of posted handlers will execute
 * concurrently.
 */
class event_loop : public base_queuered_loop
{
    using base_class = base_queuered_loop;

public:
    using timer = server_lib::timer<event_loop>;
    using periodical_timer = server_lib::periodical_timer<event_loop>;

    event_loop(bool in_separate_thread = true);
    virtual ~event_loop();

    event_loop& change_loop_name(const std::string&);

    /**
     * Start loop
     *
     */
    virtual event_loop& start();

    /**
     * Stop loop
     *
     */
    virtual void stop();

    /**
     * \brief Invoke callback when loop will start
     *
     * \param callback
     *
     * \return loop object
     *
     */
    event_loop& on_start(callback_type&& callback);

    /**
     * \brief Invoke callback when loop will stop
     *
     * \param callback
     *
     * \return loop object
     *
     */
    event_loop& on_stop(callback_type&& callback);

    /**
     * Loop has run
     */
    bool is_run() const
    {
        return _is_run;
    }

    static bool is_main_thread();

    virtual bool is_this_loop() const
    {
        return _id.load() == std::this_thread::get_id();
    }

    long native_thread_id() const
    {
        return _native_thread_id.load();
    }

    /**
     * Callback that is invoked in thread owned by this event_loop
     * Multiple invokes are executed in queue. Post can be called
     * before or after starting. Callback wiil be invoked in 'run'
     * state
     *
     * \param callback
     *
     * \return loop object
     *
     */
    template <typename Handler>
    event_loop& post(Handler&& callback)
    {
        SRV_ASSERT(service());
        SRV_ASSERT(_pstrand);

        auto callback_ = register_queue(callback);

        //-------------POST(WRAP(...)) - design explanation:
        //
        //* The 'post' guarantees that the callback will only be called in a thread
        //  in which the 'run()'
        //* The 'strand' object guarantees that all 'post's are executed in queue
        //
        service()->post(_pstrand->wrap(std::move(callback_)));

        return *this;
    }

    /**
     * Callback that is invoked after certain timeout
     * in thread owned by this event_loop
     *
     * \param duration - Timeout (std::chrono::duration type)
     * \param callback - Callback that is invoked in thread owned by this event_loop
     *
     * \return loop object
     *
     */
    template <typename DurationType, typename Handler>
    event_loop& post(DurationType&& duration, Handler&& callback)
    {
        SRV_ASSERT(service());
        SRV_ASSERT(_pstrand);

        auto callback_ = register_queue(callback);

        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration);

        SRV_ASSERT(ms.count() > 0, "1 millisecond is minimum timer accuracy");

        auto timer = std::make_shared<boost::asio::deadline_timer>(*service());

        timer->expires_from_now(boost::posix_time::milliseconds(ms.count()));
        timer->async_wait(_pstrand->wrap([timer /*save timer object*/, callback_](const boost::system::error_code& ec) {
            if (!ec)
            {
                callback_();
            }
        }));
        return *this;
    }

    /**
     * Callback that is invoked periodically after certain timeout
     * in thread owned by this event_loop
     *
     * \param duration - Timeout (std::chrono::duration type)
     * \param callback - Callback that is invoked in thread owned by this event_loop
     *
     * \return loop object
     *
     */
    template <typename DurationType, typename Handler>
    event_loop& repeat(DurationType&& duration, Handler&& callback)
    {
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration);

        SRV_ASSERT(ms.count() > 0, "1 millisecond is minimum timer accuracy");

        auto timer = std::make_shared<periodical_timer>(*this);
        timer->start(duration, [timer, callback]() {
            callback();
        });

        //All periodic timers will be destroyed when this thread stops
        return *this;
    }

    /**
     * Waiting for callback with result endlessly
     *
     * \param initial_result - Result for case when callback can't be invoked
     * \param callback - Callback that is invoked in thread owned by this event_loop
     *
     */
    template <typename Result, typename AsynchFunc>
    Result wait_result(Result&& initial_result, AsynchFunc&& callback)
    {
        auto call = [this](auto callback) {
            this->post(callback);
        };
        return wait_async_result(std::forward<Result>(initial_result), call, callback);
    }

    /**
     * Waiting for callback with result
     *
     * \param initial_result - Result for case when callback can't be invoked
     * \param callback - Callback that is invoked in thread owned by this event_loop
     * \param duration - Timeout (std::chrono::duration type)
     *
     */
    template <typename Result, typename AsynchFunc, typename DurationType>
    Result wait_result(Result&& initial_result, AsynchFunc&& callback, DurationType&& duration)
    {
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration);

        SRV_ASSERT(ms.count() > 0, "1 millisecond is minimum waiting accuracy");

        auto call = [this](auto callback) {
            this->post(callback);
        };
        return wait_async_result(std::forward<Result>(initial_result), call, callback, ms.count());
    }

    /**
     * Waiting for callback without result (void) endlessly
     *
     * \param callback - Callback that is invoked in thread owned by this event_loop
     *
     */
    template <typename AsynchFunc>
    void wait(AsynchFunc&& callback)
    {
        auto call = [this](auto callback) {
            this->post(callback);
        };
        wait_async(call, callback);
    }

    /**
     * Waiting for callback without result (void)
     *
     * \param callback - Callback that is invoked in thread owned by this event_loop
     * only before this function return
     * \param duration - Timeout (std::chrono::duration type)
     *
     * \return bool - if callback was invoked before duration expiration
     *
     */
    template <typename AsynchFunc, typename DurationType>
    bool wait(AsynchFunc&& callback, DurationType&& duration)
    {
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration);

        SRV_ASSERT(ms.count() > 0, "1 millisecond is minimum waiting accuracy");

        auto call = [this](auto callback) {
            this->post(callback);
        };
        return wait_async(call, callback, ms.count());
    }

    /**
     * \brief Waiting for finish for starting procedure
     * or immediately (if event loop has already started)
     * return
     *
     */
    void wait();

protected:
    void reset() override;

private:
    const bool _run_in_separate_thread = false;
    std::atomic_bool _is_run;
    std::atomic<std::thread::id> _id;
    std::atomic_long _native_thread_id;
    std::unique_ptr<boost::asio::io_service::strand> _pstrand;
    std::unique_ptr<std::thread> _thread;
};

} // namespace server_lib
