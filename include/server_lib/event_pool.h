#pragma once

#include <server_lib/base_queuered_loop.h>

namespace server_lib {

/**
 * \ingroup common
 *
 * \brief This class provides multi-threaded io-service
 * with unsequesed post messages processing
 */
class event_pool : public base_queuered_loop
{
    using base_class = base_queuered_loop;

public:
    event_pool(uint8_t nb_threads);
    virtual ~event_pool();

    event_pool& change_pool_name(const std::string& name);

    /**
     * Start pool
     *
     */
    event_pool& start();

    /**
     * Stop pool
     *
     */
    void stop();

    /**
     * Loop has run
     */
    bool is_run() const
    {
        return _is_run;
    }

    /**
     * Check all threads from thread pull via comparison with calling thread
     *
     */
    bool is_this_loop() const;

    /**
     * \return number of threads
     *
     */
    size_t nb_threads() const
    {
        return _nb_threads;
    }

    event_pool& on_start(callback_type&& callback);

    event_pool& on_stop(callback_type&& callback);

    template <typename Handler>
    event_pool& post(Handler&& callback)
    {
        SRV_ASSERT(service());

        auto callback_ = register_queue(callback);

        service()->post(std::move(callback_));

        return *this;
    }

protected:
    void reset() override;

private:
    const size_t _nb_threads;
    std::atomic_bool _is_run;
    std::vector<std::unique_ptr<std::thread>> _threads;
    std::atomic<size_t> _waiting_threads;
    mutable std::mutex _thread_ids_mutex;
    std::vector<std::thread::id> _thread_ids;
};

} // namespace server_lib
