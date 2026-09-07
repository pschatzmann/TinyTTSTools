/**
 * @file StringUtils.h
 * @brief String utility functions for TinyTTSTools
 * @author Phil Schatzmann
 * @version 1.0.0
 * @date 2025-07-01
 *
 * @copyright Copyright (c) 2025 Phil Schatzmann
 */

#pragma once

#include <algorithm>
#include <string>
#include <vector>

/**
 * @brief String utility functions
 * @details Helper functions for string manipulation without external
 * dependencies
 */
class StringUtils {
 public:
  /**
   * @brief Convert string to lowercase
   * @param str Input string
   * @return Lowercase version of the input string
   */
  static std::string toLowerCase(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
  }

  /**
   * @brief Split string by delimiter
   * @param str Input string to split
   * @param delimiter Character to split on
   * @return Vector of string tokens
   */
  static std::vector<std::string> split(const std::string& str,
                                        char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    for (char c : str) {
      if (c == delimiter) {
        if (!token.empty()) {
          tokens.push_back(token);
          token.clear();
        }
      } else {
        token += c;
      }
    }
    if (!token.empty()) {
      tokens.push_back(token);
    }
    return tokens;
  }

  /**
   * @brief Trim whitespace from string
   * @param str Input string
   * @return String with leading and trailing whitespace removed
   */
  static std::string trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(" \t\n\r");
    return str.substr(start, end - start + 1);
  }
};
