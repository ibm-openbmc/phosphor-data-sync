// SPDX-License-Identifier: Apache-2.0

#include "utils.hpp"

#include <print>

namespace datasynctool::utils
{

std::string extractEnumValue(const std::string& dbusValue)
{
    auto lastDot = dbusValue.find_last_of('.');
    if (lastDot != std::string::npos)
    {
        return dbusValue.substr(lastDot + 1);
    }
    return dbusValue;
}

std::string normalizePath(const std::string& path)
{
    if (path.empty() || path == "/")
    {
        return path;
    }

    // Remove trailing slashes
    std::string normalized = path;
    while (normalized.length() > 1 && normalized.back() == '/')
    {
        normalized.pop_back();
    }

    return normalized;
}

template <typename T>
void printParam(std::string key, const T& value)
{
    key.push_back(':');
    std::println("{:25}{}", key, value);
}

// Explicit template instantiations for common types
template void printParam<bool>(std::string, const bool&);
template void printParam<int>(std::string, const int&);
template void printParam<std::string>(std::string, const std::string&);

// Helper to recursively print any JSON value with indentation
static void printValue(const json& value, const std::string& indent = "  ")
{
    if (value.is_string())
    {
        std::println("{}{}", indent, value.get<std::string>());
    }
    else if (value.is_number_integer())
    {
        std::println("{}{}", indent, value.get<int>());
    }
    else if (value.is_boolean())
    {
        std::println("{}{}", indent, value.get<bool>());
    }
    else if (value.is_array())
    {
        for (const auto& item : value)
        {
            printValue(item, indent);
        }
    }
    else if (value.is_object())
    {
        for (const auto& [key, val] : value.items())
        {
            std::println("{}{}:", indent, key);
            printValue(val, indent + "  ");
        }
    }
    else
    {
        std::println("{}{}", indent, value.dump());
    }
}

void displayJsonAsText(const json& data)
{
    std::println();
    for (const auto& [name, value] : data.items())
    {
        if (value.is_object() || value.is_array())
        {
            std::println("{}:", name);
            printValue(value);
        }
        else if (value.is_boolean())
        {
            printParam(name, value.get<bool>());
        }
        else if (value.is_string())
        {
            printParam(name, value.get<std::string>());
        }
        else if (value.is_number_integer())
        {
            printParam(name, value.get<int>());
        }
        else
        {
            printParam(name, value.dump());
        }
    }
    std::println();
}

} // namespace datasynctool::utils
