#include <server_lib/network/transport/nt_server_i.h>
#include <server_lib/network/server_config.h>

#include <boost/asio.hpp>

#include <mutex>
#include <map>

namespace server_lib {
namespace network {
    namespace transport_layer {

        class tcp_server_connection_impl;

        class tcp_server_impl : public nt_server_i,
                                public std::enable_shared_from_this<tcp_server_impl>
        {
        public:
            tcp_server_impl() = default;
            ~tcp_server_impl() override;

            void config(const tcp_server_config&);

            bool start(const start_callback_type& start_callback,
                       const new_connection_callback_type& new_connection_callback,
                       const fail_callback_type& fail_callback) override;

            void stop(bool wait_for_removal) override;

            bool is_running() const override;

        private:
            void accept();
            void on_client_disconnected(size_t connection_id);

            event_loop _worker;

            std::unique_ptr<boost::asio::ip::tcp::acceptor> _acceptor;

            size_t _next_connection_id = 0;

            std::unique_ptr<tcp_server_config> _config;

            new_connection_callback_type _new_connection_callback = nullptr;
            fail_callback_type _fail_callback = nullptr;

            std::map<size_t, std::shared_ptr<tcp_server_connection_impl>> _connections;
            std::mutex _connections_mutex;
        };

    } // namespace transport_layer
} // namespace network
} // namespace server_lib
