#include <server_lib/options_helper.h>
#include <server_lib/asserts.h>
#include <server_lib/revision.h>

#include <boost/filesystem.hpp>
#include <boost/version.hpp>
#include <boost/algorithm/string/replace.hpp>

#include <iostream>
#include <set>
#include <string>
#include <fstream>
#include <iomanip>

#include <openssl/opensslv.h>

namespace server_lib {

namespace impl {
    class deduplicator
    {
    public:
        deduplicator()
            : modifier(nullptr)
        {
        }

        const boost::shared_ptr<bpo::option_description> next(const boost::shared_ptr<bpo::option_description>& o)
        {
            const std::string name = o->long_name();
            if (seen.find(name) != seen.end())
                return nullptr;
            seen.insert(name);
            return o;
        }

    private:
        std::set<std::string> seen;
        const boost::shared_ptr<bpo::option_description> (*modifier)(const boost::shared_ptr<bpo::option_description>&);
    };

    void create_new_config_file(const options::path& config_ini_path,
                                const bpo::options_description& cfg_options)
    {
        deduplicator dedup;
        std::ofstream out_cfg(config_ini_path.generic_string());
        for (const boost::shared_ptr<bpo::option_description> opt : cfg_options.options())
        {
            const boost::shared_ptr<bpo::option_description> od = dedup.next(opt);
            if (!od)
                continue;

            std::string description = od->description();
            if (!description.empty())
                out_cfg << "# " << description << "\n";
            boost::any store;
            if (!od->semantic()->apply_default(store))
                out_cfg << "# " << od->long_name() << " = \n";
            else
            {
                out_cfg << od->long_name() << " = ";

                auto example = od->format_parameter();
                if (example.empty())
                    // This is a boolean switch
                    out_cfg << "false\n";
                else if (example.length() > 6)
                {
                    // The string is formatted "arg (=<interesting part>)"
                    example.erase(0, 6);
                    example.erase(example.length() - 1);

                    bool suggest_bool_type = false;
                    if (example.size() == 1)
                    {
                        if (example.at(0) == '1' || example.at(0) == '0')
                        {
                            suggest_bool_type = description.find("Switch") != std::string::npos;
                            if (suggest_bool_type)
                            {
                                if (example.at(0) == '1')
                                    out_cfg << "true\n";
                                else
                                    out_cfg << "false\n";
                            }
                        }
                    }
                    if (!suggest_bool_type)
                        out_cfg << example << "\n";
                }
                else
                {
                    out_cfg << "\n";
                }
            }
            out_cfg << "\n";
        }
        out_cfg.close();
    }
} // namespace impl

std::string options::get_approximate_relative_time_string(const int64_t& event_time,
                                                          const int64_t& relative_to_time,
                                                          const std::string& default_ago /*= " ago"*/)
{
    SRV_ASSERT(relative_to_time >= event_time);
    std::string ago = default_ago;
    int32_t seconds_ago = static_cast<int32_t>(relative_to_time - event_time);
    if (seconds_ago < 0)
    {
        ago = " in the future";
        seconds_ago = -seconds_ago;
    }
    std::stringstream result;
    if (seconds_ago < 90)
    {
        result << seconds_ago << " second" << (seconds_ago > 1 ? "s" : "") << ago;
        return result.str();
    }
    uint32_t minutes_ago = (seconds_ago + 30) / 60;
    if (minutes_ago < 90)
    {
        result << minutes_ago << " minute" << (minutes_ago > 1 ? "s" : "") << ago;
        return result.str();
    }
    uint32_t hours_ago = (minutes_ago + 30) / 60;
    if (hours_ago < 90)
    {
        result << hours_ago << " hour" << (hours_ago > 1 ? "s" : "") << ago;
        return result.str();
    }
    uint32_t days_ago = (hours_ago + 12) / 24;
    if (days_ago < 90)
    {
        result << days_ago << " day" << (days_ago > 1 ? "s" : "") << ago;
        return result.str();
    }
    uint32_t weeks_ago = (days_ago + 3) / 7;
    if (weeks_ago < 70)
    {
        result << weeks_ago << " week" << (weeks_ago > 1 ? "s" : "") << ago;
        return result.str();
    }
    uint32_t months_ago = (days_ago + 15) / 30;
    if (months_ago < 12)
    {
        result << months_ago << " month" << (months_ago > 1 ? "s" : "") << ago;
        return result.str();
    }
    uint32_t years_ago = days_ago / 365;
    result << years_ago << " year" << (months_ago > 1 ? "s" : "");
    if (months_ago < 12 * 5)
    {
        uint32_t leftover_days = days_ago - (years_ago * 365);
        uint32_t leftover_months = (leftover_days + 15) / 30;
        if (leftover_months)
            result << leftover_months << " month" << (months_ago > 1 ? "s" : "");
    }
    result << ago;
    return result.str();
}

const char* options::get_config_file_name() const
{
    return "config.ini";
}

void options::print_version(std::ostream& stream) const
{
    bool header = false;
    const char* papplication_name = get_application_name();
    const char* papplication_art_name = get_application_art_name();
    if (papplication_art_name && std::strlen(papplication_art_name))
    {
        stream << papplication_art_name << "\n";
        header = true;
    }

    const char* papplication_internal_ver = get_application_version();
    if (papplication_internal_ver && std::strlen(papplication_internal_ver))
    {
        stream << " - ";
        if (papplication_name && std::strlen(papplication_name))
        {
            stream << papplication_name;
        }
        else
        {
            stream << "Version";
        }
        stream << ": " << papplication_internal_ver << "\n";
        header = true;
    }
    if (header)
    {
        stream << '\n';
    }
    stream << "Linked with: \n";
    stream << " - " << PRJ_NAME << ": " << PRJ_VER << " (" << PRJ_GIT_REVISION_SHA << ")"
           << "\n";
    stream << " - "
           << "Boost C++ Lib: " << boost::replace_all_copy(std::string(BOOST_LIB_VERSION), "_", ".") << "\n";
    stream << " - "
           << "SSL: " << OPENSSL_VERSION_TEXT << "\n";
}

void options::print_options(std::ostream& stream, const bpo::options_description& options) const
{
    for (const boost::shared_ptr<bpo::option_description> od : options.options())
    {
        if (!od->description().empty())
            stream << "# " << od->description() << "\n";

        boost::any store;
        if (!od->semantic()->apply_default(store))
        {
            stream << "# " << od->long_name() << " = \n";
        }
        else
        {
            auto example = od->format_parameter();
            if (example.empty())
            {
                // This is a boolean switch
                stream << od->long_name() << " = "
                       << "false\n";
            }
            else
            {
                // The string is formatted "arg (=<interesting part>)"
                example.erase(0, 6);
                example.erase(example.length() - 1);
                stream << od->long_name() << " = " << example << "\n";
            }
        }
        stream << "\n";
    }
}

options::path options::get_config_file_path(const bpo::variables_map& options, const char* opt_name) const
{
    namespace bfs = boost::filesystem;

    path config_ini_path = bfs::current_path() / get_config_file_name();

    if (options.count(opt_name))
    {
        config_ini_path = bfs::absolute(options[opt_name].as<bfs::path>());
    }

    return config_ini_path;
}

void options::create_config_file_if_not_exist(std::ostream& stream,
                                              const path& config_ini_path,
                                              const bpo::options_description& cfg_options)
{
    if (!config_ini_path.is_absolute())
        stream << "config-file path " << config_ini_path << " should be absolute and exist"
               << "\n";

    if (!boost::filesystem::exists(config_ini_path))
    {
        stream << "Writing new config file template at " << config_ini_path.generic_string() << "\n";

        if (!boost::filesystem::exists(config_ini_path.parent_path()))
        {
            boost::filesystem::create_directories(config_ini_path.parent_path());
        }

        impl::create_new_config_file(config_ini_path, cfg_options);
    }
}

void options::load_config_file(std::ostream& stream,
                               const path& config_ini_path,
                               const bpo::options_description& cfg_options,
                               bpo::variables_map& options)
{
    stream << "Load configuration from " << config_ini_path.generic_string() << "\n";

    // Get the basic options
    bpo::store(bpo::parse_config_file<char>(config_ini_path.generic_string().c_str(), cfg_options, true), options);
}

template <typename Ch>
bool is_pass_char(Ch ch)
{
    return (ch == '\'' || ch == '"' || ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r');
}

template <typename S>
S remove_quotes_impl(const S& with_quotes)
{
    auto left_it = with_quotes.cbegin();
    while (left_it != with_quotes.cend())
    {
        if (!is_pass_char(*left_it))
            break;
        ++left_it;
    }

    if (left_it == with_quotes.cend())
        return S {};

    auto right_it = with_quotes.crbegin();
    while (right_it != with_quotes.crend())
    {
        if (!is_pass_char(*right_it))
            break;
        ++right_it;
    }

    S result { left_it, right_it.base() };
    return result;
}

std::string options::remove_quotes(const std::string& with_quotes)
{
    return remove_quotes_impl(with_quotes);
}

std::wstring options::remove_quotes(const std::wstring& with_quotes)
{
    return remove_quotes_impl(with_quotes);
}

} // namespace server_lib
