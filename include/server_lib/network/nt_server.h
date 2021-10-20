#pragma once

#include <server_lib/network/transport/nt_server_i.h>

#include <server_lib/network/nt_connection.h>
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
    class nt_server
    {
    public:
        nt_server() = default;

        nt_server(const nt_server&) = delete;

        ~nt_server();

        /**
         * Configurate
         *
         */
        static tcp_server_config configurate_tcp();

        using start_callback_type = transport_layer::nt_server_i::start_callback_type;
        using new_connection_callback_type = std::function<void(const std::shared_ptr<nt_connection>&)>;
        using stop_callback_type = transport_layer::nt_server_i::stop_callback_type;

        /**
         * Start the TCP server
         *
         */
        bool start(const tcp_server_config&);

        nt_server& on_start(start_callback_type&& callback);
        nt_server& on_new_connection(new_connection_callback_type&& callback);
        nt_server& on_stop(stop_callback_type&& callback);

        void stop();

        bool is_running(void) const;

        nt_unit_builder_i& protocol();

    private:
        void on_new_connection_impl(const std::shared_ptr<transport_layer::nt_connection_i>&);

        start_callback_type _start_callback = nullptr;
        new_connection_callback_type _new_connection_callback = nullptr;
        stop_callback_type _stop_callback = nullptr;
        std::shared_ptr<transport_layer::nt_server_i> _transport_layer;
        std::shared_ptr<nt_unit_builder_i> _protocol;
    };

} // namespace network
} // namespace server_lib
