#include <server_lib/logger.h>
#include <server_lib/platform_config.h>
#include <server_lib/asserts.h>

#include "logging_trace.h"

#include <boost/shared_ptr.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/filesystem.hpp>

#include <boost/log/common.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/attributes.hpp>
#include <boost/log/sinks.hpp>
#include <boost/log/sources/logger.hpp>
#include <boost/log/trivial.hpp>

#include <boost/log/utility/manipulators/add_value.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/support/date_time.hpp>

#include <boost/core/null_deleter.hpp>

#if defined(SERVER_LIB_PLATFORM_LINUX)
#include <pthread.h>
#include <sys/syscall.h> //SYS_gettid
#include <syslog.h>
#include <unistd.h>
#include <sys/syscall.h>
#elif defined(SERVER_LIB_PLATFORM_WINDOWS)
#include <windows.h>
#endif //SERVER_LIB_PLATFORM_LINUX

#include <cassert>

namespace server_lib {
namespace impl {
    using thread_info_type = logger::log_message::thread_info_type;

    auto get_thread_info()
    {
        uint64_t thread_id = 0;
        std::string thread_name;
#if defined(SERVER_LIB_PLATFORM_LINUX)
        thread_id = static_cast<decltype(thread_id)>(syscall(SYS_gettid));
        static int MAX_THREAD_NAME_SZ = 15;
        char buff[MAX_THREAD_NAME_SZ + 1];
        if (!pthread_getname_np(pthread_self(), buff, sizeof(buff)))
        {
            thread_name = buff;
        }
#endif
        return std::make_tuple(thread_id, thread_name, false);
    }

    uint64_t get_thread_id(const thread_info_type& thread_info)
    {
        return std::get<0>(thread_info);
    }

    const char* get_thread_name(const thread_info_type& thread_info)
    {
        const auto& thread_name = std::get<1>(thread_info);
        if (thread_name.empty())
            return "?";
        return thread_name.c_str();
    }

    bool get_thread_main(const thread_info_type& thread_info)
    {
        return std::get<2>(thread_info);
    }

    void set_thread_main(thread_info_type& thread_info)
    {
        thread_info = std::make_tuple(get_thread_id(thread_info),
                                      get_thread_name(thread_info),
                                      true);
    }

    static auto s_main_thread_info = get_thread_info();

    std::string get_application_name()
    {
        std::string name;
#if defined(SERVER_LIB_PLATFORM_LINUX)

        std::ifstream("/proc/self/comm") >> name;

        // TODO: trimmed!

#elif defined(SERVER_LIB_PLATFORM_WINDOWS)

        char buf[MAX_PATH];
        GetModuleFileNameA(nullptr, buf, MAX_PATH);
        name = buf;

#endif
        return name;
    }

    static auto s_this_application_name = get_application_name();

} // namespace impl

std::atomic_ulong logger::log_message::s_id_counter(0u);

const char* logger::default_time_format = "%Y-%m-%dT%H:%M:%S";

const int logger::level_trace = static_cast<int>(logger::level::fatal);
const int logger::level_debug = static_cast<int>(logger::level::trace);
const int logger::level_info = level_debug + static_cast<int>(logger::level::debug);
const int logger::level_warning = level_info + static_cast<int>(logger::level::info);
const int logger::level_error = level_warning + static_cast<int>(logger::level::warning);

const int logger::details_all = static_cast<int>(logger::details::all);
const int logger::details_without_app_name = static_cast<int>(logger::details::without_app_name);
// clang-format off
const int logger::details_message_with_level = static_cast<int>(logger::details::without_app_name) +
                                               static_cast<int>(logger::details::without_time) +
                                               static_cast<int>(logger::details::without_microseconds) +
                                               static_cast<int>(logger::details::without_thread_info) +
                                               static_cast<int>(logger::details::without_source_code);
const int logger::details_message_without_source_code = static_cast<int>(logger::details::without_app_name) +
                                               static_cast<int>(logger::details::without_source_code);
const int logger::details_message_only = details_message_with_level +
                                         static_cast<int>(logger::details::without_level);
// clang-format on


logger::log_message::log_message()
    : id(s_id_counter++)
{
}

logger::log_message::trim_file_path::trim_file_path(size_t file_sz,
                                                    const char* file,
                                                    size_t root_dir_sz,
                                                    const char* root_dir)
{
    if (root_dir)
    {
        if (root_dir_sz < file_sz)
            file_sz -= root_dir_sz;

        _file.reserve(file_sz);
        const char* pfile = file;
        const char* proot = root_dir;
        while (*pfile && file_sz > 0)
        {
            auto ch = *pfile;
#if defined(SERVER_LIB_PLATFORM_WINDOWS)
            if (ch == '\\')
                ch = '/';
#endif
            if (!proot)
            {
                _file.push_back(ch);
                --file_sz;
            }
            else if (ch != *proot)
            {
                proot = nullptr;
            }
            else
            {
                proot++;
            }

            pfile++;
        }
    }
    else
    {
        _file.assign(file, file_sz);
    }
}

#if defined(SERVER_LIB_PLATFORM_WINDOWS)
namespace impl {
    static bool _opt_debugger_on = false;
    const char* get_debug_output_endl()
    {
        if (_opt_debugger_on)
        {
            return "\r\n";
        }
        return "";
    }
} // namespace impl
#endif

class logger::boost_logger_impl
{
public:
    boost_logger_impl() = default;

    boost::log::sources::severity_logger<boost::log::trivial::severity_level>* get()
    {
        return &_boost_logger;
    }

private:
    boost::log::sources::severity_logger<boost::log::trivial::severity_level> _boost_logger;
};

logger::logger()
{
    _logs_on = false;
}

logger::~logger()
{
}

logger& logger::init_cli_log(bool async, bool cerr)
{
    using namespace boost::log;

    using text_sink_base_type = sinks::aux::make_sink_frontend_base<sinks::text_ostream_backend>::type;

    boost::shared_ptr<text_sink_base_type> sink;
#if defined(SERVER_LIB_PLATFORM_WINDOWS)
    if (IsDebuggerPresent())
    {
        impl::_opt_debugger_on = true;
        auto sink_impl = boost::make_shared<sinks::asynchronous_sink<sinks::debug_output_backend>>();
        sink = sink_impl;
    }
    else
#endif
    {
        boost::shared_ptr<std::ostream> stream { (cerr) ? &std::cerr : &std::cout,
                                                 boost::null_deleter {} };

        if (async)
        {
            auto sink_impl = boost::make_shared<sinks::asynchronous_sink<sinks::text_ostream_backend>>();
            sink_impl->locked_backend()->add_stream(stream);
            sink_impl->locked_backend()->auto_flush(_autoflush);
            sink = sink_impl;
        }
        else
        {
            auto sink_impl = boost::make_shared<sinks::synchronous_sink<sinks::text_ostream_backend>>();
            sink_impl->locked_backend()->add_stream(stream);
            sink_impl->locked_backend()->auto_flush(_autoflush);
            sink = sink_impl;
        }
    }

    add_boost_log_destination(sink, _time_format);

    unlock();

    return *this;
}

logger& logger::init_file_log(const char* file_path, const size_t rotation_size_kb)
{
    using namespace boost::log;

    boost::shared_ptr<sinks::text_file_backend> backend = boost::make_shared<sinks::text_file_backend>(
        keywords::file_name = file_path,
        keywords::rotation_size = rotation_size_kb * 1024);

    using text_file_sink = sinks::asynchronous_sink<sinks::text_file_backend>;

    boost::shared_ptr<text_file_sink> sink = boost::make_shared<text_file_sink>(backend);

    add_boost_log_destination(sink, _time_format);

    unlock();

    return *this;
}

logger& logger::init_sys_log()
{
    //Use init_sys_log to switch on syslog instead obsolete macro USE_NATIVE_SYSLOG
#if defined(SERVER_LIB_PLATFORM_LINUX)
    openlog(NULL, LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL2);
#endif
    add_syslog_destination();

    unlock();

    return *this;
}

logger& logger::set_level(int filter)
{
    // clang-format off
    static const int max_filter = static_cast<int>(level::error) +
                                  static_cast<int>(level::warning) +
                                  static_cast<int>(level::info) +
                                  static_cast<int>(level::debug) +
                                  static_cast<int>(level::trace);
    // clang-format on
    SRV_ASSERT(filter >= 0 && filter <= max_filter);

    _level_filter = filter;
    return *this;
}

logger& logger::set_level_from_environment(const char* var_name)
{
    char* var_val = getenv(var_name);
    if (var_val != NULL)
    {
        char* end;
        auto input_filter = strtol(var_val, &end, 10);
        if (!*end)
            set_level(input_filter);
        else if (std::string(var_val).find("TRACE") == 0)
            set_level(level_trace);
        else if (std::string(var_val).find("DEBUG") == 0)
            set_level(level_debug);
        else if (std::string(var_val).find("INF") == 0)
            set_level(level_info);
        else if (std::string(var_val).find("WARN") == 0)
            set_level(level_warning);
        else if (std::string(var_val).find("ERR") == 0)
            set_level(level_error);
        else
        {
            SRV_ERROR("Invalid enviroment variable value for logger level");
        }
    }

    return *this;
}

logger& logger::set_details(int filter)
{
    SRV_ASSERT(!_boost_impl, "Details should be set before boost appender creation");

    // clang-format off
    static const int max_filter = static_cast<int>(details::without_app_name) +
                                  static_cast<int>(details::without_time) +
                                  static_cast<int>(details::without_microseconds) +
                                  static_cast<int>(details::without_level) +
                                  static_cast<int>(details::without_thread_info) +
                                  static_cast<int>(details::without_source_code);
    // clang-format on
    SRV_ASSERT(filter >= 0 && filter <= max_filter);

    _details_filter = filter;
    return *this;
}

logger& logger::set_details_from_environment(const char* var_name)
{
    char* var_val = getenv(var_name);
    if (var_val != NULL)
    {
        char* end;
        auto input_filter = strtol(var_val, &end, 10);
        if (!*end)
            set_details(input_filter);
    }

    return *this;
}

logger& logger::set_autoflush(bool flush)
{
    SRV_ASSERT(!_boost_impl, "Autoflush option should be set before boost appender creation");

    _autoflush = flush;
    return *this;
}

logger& logger::set_time_format(const char* time_format)
{
    SRV_ASSERT(!_boost_impl, "Time format should be set before boost appender creation");

    SRV_ASSERT(time_format && strlen(time_format) > 1, "Invalid time format");
    _time_format = time_format;
    return *this;
}

void logger::lock()
{
    _logs_on = false;
}

void logger::unlock()
{
    _logs_on = true;
}

void logger::add_destination(log_handler_type&& handler)
{
    _appenders.push_back(std::move(handler));
}

bool logger::is_not_filtered(const logger::level level) const
{
    auto level_ = static_cast<int>(level);
    return (!level_ || ~_level_filter & level_);
}

logger::log_message logger::create_message(const logger::level level,
                                           size_t file_sz,
                                           const char* file,
                                           const int line,
                                           const char* func)
{
    log_message msg;
    msg.lv = level;
    if (~_details_filter & static_cast<int>(logger::details::without_thread_info))
    {
        msg.thread_info = impl::get_thread_info();
#if defined(SERVER_LIB_PLATFORM_LINUX)
        if (impl::get_thread_id(msg.thread_info) == impl::get_thread_id(impl::s_main_thread_info))
            impl::set_thread_main(msg.thread_info);
#endif
    }
    if (~_details_filter & static_cast<int>(logger::details::without_source_code))
    {
        msg.file = log_message::trim_file_path(file_sz, file);
        msg.line = line;
        msg.func = func;
    }
    return msg;
}

void logger::write(const log_message& msg)
{
    if (!_logs_on.load())
        return;

    try
    {
        if (is_not_filtered(msg.lv))
        {
            for (const auto& appender : _appenders)
            {
                appender(msg, _details_filter); // can throw exception
            }
        }
    }
    catch (std::exception& e)
    {
        SRV_TRACE_SIGNAL(e.what());
    }
}

void logger::flush()
{
    if (!_logs_on.load())
        return;

    if (!_appenders.empty())
    {
        using namespace boost::log;

        core::get()->flush();
    }
}

template <typename SinkTypePtr>
void logger::add_boost_log_destination(const SinkTypePtr& sink, const std::string& time_format)
{
    SRV_ASSERT(sink);

    auto to_boost_level = [](level lv) -> boost::log::trivial::severity_level {
        switch (lv)
        {
        case server_lib::logger::level::trace:
            return boost::log::trivial::trace;
        case server_lib::logger::level::debug:
            return boost::log::trivial::debug;
        case server_lib::logger::level::info:
            return boost::log::trivial::info;
        case server_lib::logger::level::warning:
            return boost::log::trivial::warning;
        case server_lib::logger::level::error:
            return boost::log::trivial::error;
        case server_lib::logger::level::fatal:
            return boost::log::trivial::fatal;
        }

        return boost::log::trivial::trace;
    };

    namespace expr = boost::log::expressions;
    using namespace boost::log;

    std::stringstream ss_formatter;
    bool prefix = false;
    if (~_details_filter & static_cast<int>(logger::details::without_app_name))
    {
        ss_formatter << "%1%: ";
        prefix |= true;
    }
    if (~_details_filter & static_cast<int>(logger::details::without_time))
    {
        ss_formatter << "%2% ";
        prefix |= true;
    }
    if (~_details_filter & static_cast<int>(logger::details::without_level))
    {
        ss_formatter << "[%3%]";
        prefix |= true;
    }
    if (~_details_filter & static_cast<int>(logger::details::without_thread_info))
    {
        ss_formatter << "[%4%-%5%]";
        prefix |= true;
    }
    if (prefix)
    {
        ss_formatter << " ";
    }
    ss_formatter << "%6%";
    if (~_details_filter & static_cast<int>(logger::details::without_source_code))
    {
        ss_formatter << " (from %7%:%8%)";
    }

    const auto timestamp_attribute_name = aux::default_attribute_names::timestamp();
    const auto line_id_attribute_name = aux::default_attribute_names::line_id();

    auto time_format_ = time_format;
    if (~_details_filter & static_cast<int>(logger::details::without_microseconds))
    {
        time_format_ += ".%f";
    }
    sink->set_formatter(
        expr::format(ss_formatter.str())
        % expr::attr<std::string>("AppName")
        % expr::format_date_time<boost::posix_time::ptime>(timestamp_attribute_name, time_format_)
        % expr::attr<trivial::severity_level>("Severity")
        % expr::attr<unsigned long>("ThreadId")
        % expr::attr<std::string>("ThreadName")
        % expr::smessage
        % expr::attr<std::string>("File")
        % expr::attr<int>("Line"));

    auto pcore = core::get();

    pcore->remove_all_sinks();
    pcore->add_sink(sink);

    pcore->add_global_attribute(timestamp_attribute_name, attributes::local_clock());

    _boost_impl = std::make_unique<boost_logger_impl>();

    auto boost_write = [pl = this->_boost_impl->get(), to_boost_level](const log_message& msg, int details_filter) {
        BOOST_LOG_SEV(*pl, to_boost_level(msg.lv))
            << boost::log::add_value("AppName", (~details_filter & static_cast<int>(logger::details::without_app_name)) ? impl::s_this_application_name.c_str() : "")
            << boost::log::add_value("Line", msg.line)
            << boost::log::add_value("File", (~details_filter & static_cast<int>(logger::details::without_source_code)) ? msg.file.c_str() : "")
            << boost::log::add_value("ThreadId", (~details_filter & static_cast<int>(logger::details::without_thread_info)) ? static_cast<unsigned long>(impl::get_thread_id(msg.thread_info)) : 0)
            << boost::log::add_value("ThreadName", (~details_filter & static_cast<int>(logger::details::without_thread_info)) ? impl::get_thread_name(msg.thread_info) : "")
            << msg.stream.str()
#if defined(SERVER_LIB_PLATFORM_WINDOWS)
            << impl::get_debug_output_endl()
#endif
            ;
    };

    add_destination(std::move(boost_write));
}

void logger::add_syslog_destination()
{
#if defined(SERVER_LIB_PLATFORM_LINUX)
    auto to_syslog_level = [](level lv) -> int {
        switch (lv)
        {
        case server_lib::logger::level::trace:
            return LOG_DEBUG;
        case server_lib::logger::level::debug:
            return LOG_DEBUG;
        case server_lib::logger::level::info:
            return LOG_INFO;
        case server_lib::logger::level::warning:
            return LOG_WARNING;
        case server_lib::logger::level::error:
            return LOG_ERR;
        case server_lib::logger::level::fatal:
            return LOG_CRIT;
        default:;
        }
        return LOG_DEBUG;
    };

    auto syslog_write = [to_syslog_level](const log_message& msg, int details_filter) {
        if (~details_filter & static_cast<int>(logger::details::without_source_code))
        {
            syslog(to_syslog_level(msg.lv), "%s (from %s:%d)",
                   msg.stream.str().c_str(),
                   msg.file.c_str(), msg.line);
        }
        else
        {
            syslog(to_syslog_level(msg.lv), "%s",
                   msg.stream.str().c_str());
        }
    };
    add_destination(std::move(syslog_write));
#else
    SRV_ERROR("Not implemented");
#endif
}

} // namespace server_lib
