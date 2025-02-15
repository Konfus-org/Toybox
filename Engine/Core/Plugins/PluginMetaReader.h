#pragma once
#include "Debug/DebugAPI.h"
#include <map>
#include <string>
#include <sstream>
#include <fstream>

class PluginMetaReader
{
public:
    static std::map<std::string, std::string, std::less<>> Read(const std::string& jsonPath)
    {
        // Open the JSON file
        std::ifstream file(jsonPath);
        if (!file)
        {
            TBX_ERROR("Failed to open JSON file: {0}", jsonPath);
            return {};
        }

        // Read file into buffer
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string jsonContent = buffer.str();

        // Parse JSON into result map
        std::map<std::string, std::string, std::less<>> result;
        size_t pos = 0;
        while (pos < jsonContent.size())
        {
            // Skip whitespace
            while (pos < jsonContent.size() && std::isspace(jsonContent[pos]))
            {
                pos++;
            }

            // Check for opening bracket
            if (jsonContent[pos] != '{')
            {
                TBX_ERROR("Invalid JSON format");
                return {};
            }
            pos++;

            // Parse key-value pairs
            while (pos < jsonContent.size())
            {
                // Skip whitespace
                while (pos < jsonContent.size() && std::isspace(jsonContent[pos]))
                {
                    pos++;
                }

                // Check for closing bracket
                if (jsonContent[pos] == '}')
                {
                    pos++;
                    break;
                }

                // Parse key
                std::string key;
                if (jsonContent[pos] != '"')
                {
                    TBX_ERROR("Invalid JSON format");
                    return {};
                }
                pos++;
                while (pos < jsonContent.size() && jsonContent[pos] != '"')
                {
                    key += jsonContent[pos];
                    pos++;
                }
                pos++;

                // Skip colon
                if (jsonContent[pos] != ':')
                {
                    TBX_ERROR("Invalid JSON format");
                    return {};
                }
                pos++;

                // Parse value
                std::string value;
                if (jsonContent[pos] != '"')
                {
                    TBX_ERROR("Invalid JSON format");
                    return {};
                }
                pos++;
                while (pos < jsonContent.size() && jsonContent[pos] != '"')
                {
                    value += jsonContent[pos];
                    pos++;
                }
                pos++;

                // Add key-value pair to result map
                result[key] = value;

                // Skip comma
                if (pos < jsonContent.size() && jsonContent[pos] == ',')
                {
                    pos++;
                }
            }
        }

        return result;
    }
};