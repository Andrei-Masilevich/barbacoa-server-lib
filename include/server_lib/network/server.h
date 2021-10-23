#pragma once

#include <server_lib/network/transport/server_impl_i.h>

#include <server_lib/network/connection.h>
#include <server_lib/network/server_config.h>

#include <string>
#include <functional>
#include <memory>

namespace server_lib {
namespace network {

    /**
     * \ingroup network
     *
     * \brief Simple async server with application connection
     */
    class server
    {
    public:
        server() = default;

        server(const server&) = delete;

        ~server();

        /**
         * Configurate
         *
         */
        static tcp_server_config configurate_tcp();

        using start_callback_type = transport_layer::server_impl_i::start_callback_type;
        using new_connection_callback_type = std::function<void(const std::shared_ptr<connection>&)>;
        using fail_callback_type = transport_layer::server_impl_i::fail_callback_type;

        /**
         * Start the TCP server
         *
         */
        bool start(const tcp_server_config&);

        server& on_start(start_callback_type&& callback);
        server& on_new_connection(new_connection_callback_type&& callback);
        server& on_fail(fail_callback_type&& callback);

        void stop(bool wait_for_removal);

        bool is_running(void) const;

    private:
        void on_new_client(const std::shared_ptr<transport_layer::connection_impl_i>&);
        void on_client_disconnected(size_t);

        std::shared_ptr<transport_layer::server_impl_i> _transport_layer;
        std::shared_ptr<unit_builder_i> _protocol;

        std::map<size_t, std::shared_ptr<connection>> _connections;
        std::mutex _connections_mutex;

        start_callback_type _start_callback = nullptr;
        new_connection_callback_type _new_connection_callback = nullptr;
        fail_callback_type _fail_callback = nullptr;
    };

} // namespace network
} // namespace server_lib