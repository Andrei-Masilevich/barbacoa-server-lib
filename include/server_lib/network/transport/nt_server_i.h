#pragma once

#include <server_lib/network/transport/nt_connection_i.h>

#include <server_lib/event_loop.h>

#include <cstdint>
#include <memory>

namespace server_lib {
namespace network {
    namespace transport_layer {

        /**
         * \ingroup network
         * \defgroup network_i Interfaces
         * Auxiliary network object interfaces
         * \defgroup network_utils Utilities
         * Helpers
         */

        /**
         * \ingroup network_i
         *
         * \brief Interface for transport implementation for async network server
         *
         * Implementations: nt_server
         */
        class nt_server_i
        {
        public:
            virtual ~nt_server_i() = default;

        public:
            /**
            * Callback called whenever a network server has been started
            *
            */
            using start_callback_type = std::function<void()>;

            /**
            * Callback called whenever a new client is connecting to the network server
            *
            * Takes as parameter a shared pointer to the nt_connection_i that wishes to connect
            */
            using new_connection_callback_type = std::function<void(const std::shared_ptr<nt_connection_i>&)>;

            /**
            * Callback called whenever a network server has been stopped
            *
            */
            using stop_callback_type = std::function<void()>;

            /**
            * Start the network server
            *
            * \param new_connection_callback
            * \param start_callback
            *
            */
            virtual void start(const start_callback_type& start_callback,
                               const new_connection_callback_type& new_connection_callback)
                = 0;

            /**
            * Disconnect the network server if it was currently running.
            *
            * \param stop_callback
            *
            */
            virtual void stop(const stop_callback_type& stop_callback) = 0;

            /**
            * \return whether the network server is currently running or not
            */
            virtual bool is_running() const = 0;
        };

    } // namespace transport_layer
} // namespace network
} // namespace server_lib
