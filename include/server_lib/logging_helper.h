#pragma once

#include <server_lib/logger.h>
#include <server_lib/platform_config.h>

#ifndef SRV_FUNCTION_NAME_
#if defined(SERVER_LIB_PLATFORM_WINDOWS)
#define SRV_FUNCTION_NAME_ __FUNCTION__
#else //*NIX
#define SRV_FUNCTION_NAME_ __func__
#endif
#endif

namespace server_lib {

#define LOGGER_REFERENCE server_lib::logger::instance()

#define LOG_LOG(LEVEL, FILE, LINE, FUNC, ARG)                                                      \
    SRV_EXPAND_MACRO(                                                                              \
        SRV_MULTILINE_MACRO_BEGIN {                                                                \
            if (LOGGER_REFERENCE.is_not_filtered(LEVEL))                                           \
            {                                                                                      \
                auto msg = LOGGER_REFERENCE.create_message(LEVEL, sizeof(FILE), FILE, LINE, FUNC); \
                msg.stream << ARG;                                                                 \
                LOGGER_REFERENCE.write(msg);                                                       \
            }                                                                                      \
        } SRV_MULTILINE_MACRO_END)

#define LOG_TRACE(ARG) LOG_LOG(server_lib::logger::level::trace, __FILE__, __LINE__, SRV_FUNCTION_NAME_, ARG)
#define LOG_DEBUG(ARG) LOG_LOG(server_lib::logger::level::debug, __FILE__, __LINE__, SRV_FUNCTION_NAME_, ARG)
#define LOG_INFO(ARG) LOG_LOG(server_lib::logger::level::info, __FILE__, __LINE__, SRV_FUNCTION_NAME_, ARG)
#define LOG_WARN(ARG) LOG_LOG(server_lib::logger::level::warning, __FILE__, __LINE__, SRV_FUNCTION_NAME_, ARG)
#define LOG_ERROR(ARG) LOG_LOG(server_lib::logger::level::error, __FILE__, __LINE__, SRV_FUNCTION_NAME_, ARG)
#define LOG_FATAL(ARG) LOG_LOG(server_lib::logger::level::fatal, __FILE__, __LINE__, SRV_FUNCTION_NAME_, ARG)

#define LOGC_TRACE(ARG) LOG_TRACE(SRV_LOG_CONTEXT_ << ARG)
#define LOGC_DEBUG(ARG) LOG_DEBUG(SRV_LOG_CONTEXT_ << ARG)
#define LOGC_INFO(ARG) LOG_INFO(SRV_LOG_CONTEXT_ << ARG)
#define LOGC_WARN(ARG) LOG_WARN(SRV_LOG_CONTEXT_ << ARG)
#define LOGC_ERROR(ARG) LOG_ERROR(SRV_LOG_CONTEXT_ << ARG)
#define LOGC_FATAL(ARG) LOG_FATAL(SRV_LOG_CONTEXT_ << ARG)

#if defined(SERVER_LIB_SUPPRESS_LOGS)
#if defined(SERVER_LIB_LOGS)
#undef SERVER_LIB_LOGS
#endif
#else // SERVER_LIB_SUPPRESS_LOGS
#if !defined(SERVER_LIB_LOGS)
#define SERVER_LIB_LOGS
#endif
#endif // !SERVER_LIB_SUPPRESS_LOGS

#ifdef SERVER_LIB_LOGS
#define SRV_LOG_TRACE(ARG) LOG_TRACE(ARG)
#define SRV_LOG_DEBUG(ARG) LOG_DEBUG(ARG)
#define SRV_LOG_INFO(ARG) LOG_INFO(ARG)
#define SRV_LOG_WARN(ARG) LOG_WARN(ARG)
#define SRV_LOG_ERROR(ARG) LOG_ERROR(ARG)
#define SRV_LOG_FATAL(ARG) LOG_FATAL(ARG)

#define SRV_LOGC_TRACE(ARG) LOGC_TRACE(ARG)
#define SRV_LOGC_DEBUG(ARG) LOGC_DEBUG(ARG)
#define SRV_LOGC_INFO(ARG) LOGC_INFO(ARG)
#define SRV_LOGC_WARN(ARG) LOGC_WARN(ARG)
#define SRV_LOGC_ERROR(ARG) LOGC_ERROR(ARG)
#define SRV_LOGC_FATAL(ARG) LOGC_FATAL(ARG)
#else
#define SRV_LOG_TRACE(ARG)
#define SRV_LOG_DEBUG(ARG)
#define SRV_LOG_INFO(ARG)
#define SRV_LOG_WARN(ARG)
#define SRV_LOG_ERROR(ARG)
#define SRV_LOG_FATAL(ARG)

#define SRV_LOGC_TRACE(ARG)
#define SRV_LOGC_DEBUG(ARG)
#define SRV_LOGC_INFO(ARG)
#define SRV_LOGC_WARN(ARG)
#define SRV_LOGC_ERROR(ARG)
#define SRV_LOGC_FATAL(ARG)
#endif

// Alternative syntaxic
//
#define LOG_LOG_ALTERNATIVE(LEVEL, FILE, LINE, FUNC) \
    LOGGER_REFERENCE.create_stream_message(LEVEL, sizeof(FILE), FILE, LINE, FUNC).stream()

#define LOG(LEVEL) LOG_LOG_ALTERNATIVE(LEVEL, __FILE__, __LINE__, SRV_FUNCTION_NAME_)
#define LOGC(LEVEL) LOG(LEVEL) << SRV_LOG_CONTEXT_

} // namespace server_lib
