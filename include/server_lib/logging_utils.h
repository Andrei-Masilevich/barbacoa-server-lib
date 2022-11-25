#pragma once

#include <map>
#include <set>
#include <vector>
#include <array>
#include <sstream>

namespace server_lib {

template <typename K, typename V>
std::string to_string(const std::map<K, V>& value)
{
    std::stringstream stream;

    bool is_first = true;
    stream << "{";
    for (const auto& _item : value)
    {
        if (is_first)
        {
            is_first = false;
        }
        else
        {
            stream << ", ";
        }
        stream << _item.first << ": " << _item.second;
    }
    stream << "}";

    return stream.str();
}


template <typename T>
std::string to_string(const std::set<T>& value)
{
    std::stringstream stream;

    stream << "(";
    for (const auto& _item : value)
    {
        stream << " " << _item;
    }
    stream << " )";

    return stream.str();
}


template <typename T>
const std::string to_json(const std::set<T>& value)
{
    std::stringstream stream;

    bool is_first = true;
    stream << "[";
    for (const auto& item : value)
    {
        if (is_first)
        {
            is_first = false;
        }
        else
        {
            stream << ",";
        }
        stream << "\"" << item << "\"";
    }
    stream << " ]";

    return stream.str();
}


template <typename T>
std::string to_string(const std::vector<T>& value)
{
    std::stringstream stream;

    stream << "[";
    for (const auto& _item : value)
    {
        stream << " " << _item;
    }
    stream << " ]";

    return stream.str();
}


template <typename T, size_t S>
std::string to_string(const std::array<T, S>& value)
{
    std::stringstream stream;

    stream << "[";
    for (const auto& _item : value)
    {
        stream << " " << _item;
    }
    stream << " ]";

    return stream.str();
}


template <typename T, size_t S>
std::string to_json(const std::array<T, S>& value)
{
    std::stringstream stream;

    bool is_first = true;
    stream << "[";
    for (const auto& item : value)
    {
        if (is_first)
        {
            is_first = false;
        }
        else
        {
            stream << ",";
        }
        stream << "\"" << item << "\"";
    }
    stream << " ]";

    return stream.str();
}

} // namespace server_lib
