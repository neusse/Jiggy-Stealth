#pragma once
#include <stdint.h>

namespace config {
constexpr const char* ProjectName = "Jiggy-Stealth";
constexpr const char* Version = "1.2.0";
constexpr int16_t MovementCounts = 3;
constexpr uint32_t IdleMs = 30000;
constexpr uint32_t IntervalMinMs = 30000;
constexpr uint32_t IntervalMaxMs = 60000;
constexpr uint32_t ReturnDelayMs = 100;
constexpr uint32_t ShiftHoldMs = 30;
constexpr uint32_t DebounceMs = 25;
constexpr uint32_t LongPressMs = 2000;
constexpr uint8_t MouseReportId = 1;
constexpr uint8_t KeyboardReportId = 2;
constexpr unsigned QueueCapacity = 128;
constexpr unsigned InterfaceCapacity = 8;
constexpr unsigned DescriptorCapacity = 256;
static_assert(IntervalMinMs >= IdleMs && IntervalMaxMs >= IntervalMinMs,
              "The random interval must permit the idle threshold.");
static_assert(IntervalMaxMs < 0x80000000u, "Intervals must fit wrap-safe deadlines.");
static_assert(MovementCounts > 0 && MovementCounts <= 127, "Keep jiggle movement small and positive.");
}
