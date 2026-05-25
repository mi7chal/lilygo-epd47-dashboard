#pragma once

#include <Arduino.h>
#include <cstdint>
#include <memory>

enum Screen : std::uint8_t { Empty, Loading, Dashboard, Error };
enum Alignment : std::uint8_t { Left, Center, Right };

struct DashboardData {
    String weatherDescription;
    String cityName;
    float temperatureCelsius;
    std::uint8_t humidityPercent;
    std::uint16_t pressureHpa;
    float windSpeedMs;
};

class DisplayManager {
public:
    DisplayManager() = default;

    DisplayManager(const DisplayManager&) = delete;
    DisplayManager& operator=(const DisplayManager&) = delete;
    DisplayManager(DisplayManager&&) = delete;
    DisplayManager& operator=(DisplayManager&&) = delete;

    void init();
    void showLoadingScreen();
    void showErrorScreen(const String& title, const String& message);
    void showDashboard(const DashboardData& data);

    void goToSleep(uint32_t sleepDurationSeconds, uint32_t delayBeforeSleepMs = 2000U);
private:
    bool initialised = false;
    std::unique_ptr<uint8_t, decltype(&std::free)> fb{nullptr, &std::free};

    void clearFramebuffer();
    void presentFramebuffer();
    void displayString(const String& str, int32_t x, int32_t y, Alignment horizontalAlignment = Alignment::Left);
    void displayString(const char* str, int32_t x, int32_t y, Alignment horizontalAlignment = Alignment::Left);

};