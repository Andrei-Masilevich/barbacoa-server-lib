#pragma once

#include <functional>
#include <string>
#include <sstream>
#include <vector>
#include <atomic>
#include <utility>
#include <memory>

#include <server_lib/singleton.h>

namespace server_lib {

/**
 * \ingroup common
 *
 * \brief This is the Logger with different destinations
 * (standard output, files, syslog)
 * with extension ability.
 *
 * It is used with helpers macros:
 * \li \c LOG_TRACE
 * \li \c LOG_DEBUG
 * \li \c LOG_INFO
 * \li \c LOG_WARN
 * \li \c LOG_ERROR
 * \li \c LOG_FATAL
 * \n
 * \n
 * \li \c LOGC_TRACE (with log context locally defined by SRV_LOG_CONTEXT_)
 * \li \c LOGC_DEBUG
 * \li \c LOGC_INFO
 * \li \c LOGC_WARN
 * \li \c LOGC_ERROR
 * \li \c LOGC_FATAL
 *
 * \code{.c}
 *
 * LOG_TRACE("Foo");
 * LOG_WARN("Foo " << 42 << '!');
 *
 * \endcode
 *
 * \code{.c}
 * #define SRV_LOG_CONTEXT_ "payments> " << SRV_FUNCTION_NAME_ << ": "
 *
 * LOGC_ERROR("Invalid ticket. Id = " << id);
 *
 * \endcode
*/
class logger : public singleton<logger>
{
public:
    static const char* default_time_format;

    enum class level
    {
        fatal = 0x0, //not filtered
        error = 0x1,
        warning = 0x2,
        info = 0x4,
        debug = 0x8,
        trace = 0x10,
    };

    static const int level_trace; // 0
    static const int level_debug; // 16
    static const int level_info; // 24
    static const int level_warning; // 28
    static const int level_error; // 30

    enum class details
    {
        all = 0x0, //not filtered
        without_app_name = 0x1,
        without_time = 0x2,
        without_microseconds = 0x4,
        without_level = 0x8,
        without_thread_info = 0x10,
        without_source_code = 0x20,
    };

    static const int details_all; // 0
    static const int details_without_app_name; // 1
    static const int details_message_with_level; // 55
    static const int details_message_without_source_code; // 33
    static const int details_message_only; // 63

    struct log_message
    {
        friend class logger;

        unsigned long id; // for custom appenders
        level lv;
        std::string file;
        int line;
        std::string func;

        using thread_info_type = std::tuple<uint64_t, std::string, bool>;
        thread_info_type thread_info;

        std::stringstream stream;

        class trim_file_path
        {
            std::string _file;

        public:
            // accept string sizes calculated in compilation time
            trim_file_path(size_t file_sz,
                           const char* file,
                           size_t root_dir_sz =
#if defined(LOGGER_SOURCE_DIR)
                               sizeof(LOGGER_SOURCE_DIR)
#else
                               0
#endif
                               ,
                           const char* root_dir =
#if defined(LOGGER_SOURCE_DIR)
                               LOGGER_SOURCE_DIR
#else
                               nullptr
#endif
            );

            operator std::string() { return _file; }
        };

    private:
        log_message();

        static std::atomic_ulong s_id_counter;
    };

    using log_handler_type = std::function<void(const log_message&, int details_filter)>;

protected:
    logger();

    friend class singleton<logger>;

public:
    ~logger();

    logger& init_cli_log(bool async = false,
                         bool cerr = false);
    logger& init_file_log(const char* file_path,
                          const size_t rotation_size_kb);
    logger& init_sys_log();
    logger& init_debug_log(bool async, bool cerr)
    {
        return init_cli_log(async, cerr);
    }

    logger& set_level(int filter = logger::level_debug);
    logger& set_level_from_environment(const char* var_name);

    int level_filter() const
    {
        return _level_filter;
    }

    logger& set_details(int filter = logger::details_without_app_name);
    logger& set_details_from_environment(const char* var_name);

    int details_filter() const
    {
        return _details_filter;
    }

    logger& set_autoflush(bool = true);

    bool autoflush() const
    {
        return _autoflush;
    }

    logger& set_time_format(const char*);

    const std::string& time_format() const
    {
        return _time_format;
    }

    void lock();
    void unlock();

    void add_destination(log_handler_type&& handler);

    bool is_not_filtered(const logger::level) const;

    log_message create_message(const logger::level,
                               size_t file_sz, const char* file, const int line, const char* func);

    void write(const log_message& msg);

    size_t get_appender_count() const
    {
        return _appenders.size();
    }

    void flush();

private:
    template <typename SinkTypePtr>
    void add_boost_log_destination(const SinkTypePtr& sink, const std::string& dtf);
    void add_syslog_destination();

private:
    class boost_logger_impl;

    std::unique_ptr<boost_logger_impl> _boost_impl;
    std::vector<log_handler_type> _appenders;
    std::string _time_format = logger::default_time_format;
    int _level_filter = logger::level_trace;
    int _details_filter = logger::details_without_app_name;
    bool _autoflush = false;
    std::atomic_bool _logs_on;
};

} // namespace server_lib
