/**
 * @file TTSLogger.h
 * @brief Unified logging class for TinyTTSTools library
 * @author Phil Schatzmann
 * @version 1.0.0
 * @date 2025-08-25
 *
 * @copyright Copyright (c) 2025 Phil Schatzmann
 */

#pragma once

#include <string>
#include <cstdarg>
#include <cstdio>

#ifdef ARDUINO
#include "Arduino.h"
#endif

/**
 * @brief Logging levels for TTS operations
 */
enum class TTSLogLevel {
  DEBUG = 0,
  INFO = 1,
  WARNING = 2,
  ERROR = 3,
  NONE = 4
};

#ifdef ARDUINO
#  define DEFAULT_ERROR_OUT &Serial
#else
#  define DEFAULT_ERROR_OUT nullptr
#endif
/**
 * @brief Unified logging class for TinyTTSTools library
 * @details Provides consistent logging across different platforms (Arduino, PC)
 *          with configurable log levels and output destinations.
 */
class TTSLoggerCLass {
 public:
  /**
   * @brief Initialize the logger with log level and output stream
   * @param level Minimum log level to output
   * @param output Print object for output (e.g., Serial, File, custom stream)
   * @return true if initialization successful, false otherwise
   */
  bool begin(TTSLogLevel level, Print& output) {
    logLevel_ = level;
    output_ = &output;
    enabled_ = true;
    return true;
  }

  /**
   * @brief Cleanup and shutdown the logger
   */
  void end() {
    enabled_ = false;
    output_ = nullptr;
  }

  /**
   * @brief Get the current log level
   * @return Current minimum log level
   */
  TTSLogLevel logLevel() const { return logLevel_; }

  /**
   * @brief Log a formatted message with specified level (printf-style)
   * @param level Log level for this message
   * @param format Printf-style format string
   * @param ... Variable arguments for formatting
   */
  void log(TTSLogLevel level, const char* format, ...) {
    if (level < logLevel_) {
      return;  // Message level is below threshold
    }

    if (enabled_ && format) {
      va_list args;
      va_start(args, format);
      
      // Format the message into a buffer
      char buffer[TTS_LOG_BUFFER_SIZE];
      vsnprintf(buffer, sizeof(buffer), format, args);
      buffer[sizeof(buffer) - 1] = '\0';  // Ensure null termination
      
      va_end(args);
      
      outputMessage(level, buffer);
    }
  }

 protected:
  static constexpr size_t TTS_LOG_BUFFER_SIZE = 256;  // Buffer size for formatted messages
  TTSLogLevel logLevel_ = TTSLogLevel::INFO;
  bool enabled_ = true;
  Print* output_ = DEFAULT_ERROR_OUT;

  /**
   * @brief Output a formatted message
   * @param level Log level
   * @param message Message to output
   */
  void outputMessage(TTSLogLevel level, const char* message) {
    if (!message) return;

    switch (level) {
      case TTSLogLevel::DEBUG:
        print("[DEBUG] ");
        break;
      case TTSLogLevel::INFO:
        print("[INFO] ");
        break;
      case TTSLogLevel::WARNING:
        print("[WARN] ");
        break;
      case TTSLogLevel::ERROR:
        print("[ERROR] ");
        break;
      case TTSLogLevel::NONE:
        break;
    }
    print(message);
    print("\n");
  }

  void print(const char* message) {
#ifdef ARDUINO
    output_->print(message);
#else
    printf("%s", message);
#endif
  }
};

static TTSLoggerCLass TTSLogger;

// Logging macros with printf-style formatting support
#define TTS_LOGD(fmt, ...) TTSLogger.log(TTSLogLevel::DEBUG, fmt, ##__VA_ARGS__)
#define TTS_LOGI(fmt, ...) TTSLogger.log(TTSLogLevel::INFO, fmt, ##__VA_ARGS__)
#define TTS_LOGW(fmt, ...) TTSLogger.log(TTSLogLevel::WARNING, fmt, ##__VA_ARGS__)
#define TTS_LOGE(fmt, ...) TTSLogger.log(TTSLogLevel::ERROR, fmt, ##__VA_ARGS__)

