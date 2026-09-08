/**
 * @file PsramAllocator.h
 * @brief Standard C++ Allocator that prefers PSRAM for its backing storage
 * @author Phil Schatzmann
 * @version 1.0.0
 * @date 2025-09-08
 *
 * @copyright Copyright (c) 2025 Phil Schatzmann
 */

#pragma once

#include <cstddef>
#include <new>

#include "../Basic/TTSLogger.h"

#if defined(ESP32)
#include <esp_heap_caps.h>
#endif

/**
 * @brief A minimal C++11 Allocator that requests PSRAM on ESP32 and falls
 * back to internal RAM (still on-device) or plain `malloc` elsewhere.
 * @details Meets the standard Allocator requirements (`value_type`,
 * `allocate()`, `deallocate()`, and equality comparison), so it drops
 * straight into any container that accepts an allocator template
 * parameter, e.g. `std::vector<uint8_t, PsramAllocator<uint8_t>>`. Classes
 * in this library that own runtime-loaded buffers (SD/LittleFS file
 * reads, decoded audio) take an `Allocator` template parameter defaulted
 * to `std::allocator<uint8_t>` for exactly this purpose -- switching a
 * buffer to PSRAM is a one-word type change at the call site, with no
 * change to the class itself.
 *
 * On ESP32, `heap_caps_malloc(..., MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)` is
 * tried first; if PSRAM isn't present or is exhausted, it falls back to
 * `MALLOC_CAP_8BIT` (any internal RAM) rather than failing outright -- a
 * board without PSRAM, or one under memory pressure, still works, just
 * without the PSRAM benefit. On every other platform (desktop builds,
 * non-ESP32 boards) this is a thin wrapper around `malloc`/`free`, so it's
 * always safe to use `PsramAllocator` in portable code -- it just isn't
 * doing anything special off ESP32.
 *
 * @note PSRAM on ESP32 is accessed over a slower bus than internal RAM.
 * Prefer it for large, infrequently-touched buffers (a whole decoded audio
 * clip, a file read from SD) rather than small hot-path buffers that are
 * read/written every sample.
 */
template <typename T>
struct PsramAllocator {
  using value_type = T;

  PsramAllocator() = default;
  template <typename U>
  PsramAllocator(const PsramAllocator<U>&) noexcept {}

  T* allocate(size_t n) {
    size_t bytes = n * sizeof(T);
    void* p = nullptr;
#if defined(ESP32)
    p = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (p == nullptr) {
      // No PSRAM present, or PSRAM exhausted -- fall back to internal RAM
      // instead of failing outright.
      TTS_LOGW("PsramAllocator: PSRAM allocation of %zu bytes failed, "
               "falling back to internal RAM", bytes);
      p = heap_caps_malloc(bytes, MALLOC_CAP_8BIT);
    }
#else
    p = malloc(bytes);
#endif
    if (p == nullptr) {
      TTS_LOGE("PsramAllocator: allocation of %zu bytes failed", bytes);
      throw std::bad_alloc();
    }
    return static_cast<T*>(p);
  }

  void deallocate(T* p, size_t) noexcept {
#if defined(ESP32)
    heap_caps_free(p);
#else
    free(p);
#endif
  }
};

template <typename T, typename U>
bool operator==(const PsramAllocator<T>&, const PsramAllocator<U>&) noexcept {
  return true;
}
template <typename T, typename U>
bool operator!=(const PsramAllocator<T>&, const PsramAllocator<U>&) noexcept {
  return false;
}
