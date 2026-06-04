#pragma once

#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include "epd_driver.h"


enum Screen : std::uint8_t { Empty, Loading, Dashboard, Error };
enum Alignment : std::uint8_t { Left, Center, Right };

struct CustomScreenData {
    std::string title;
    std::string message;
};

struct DashboardData {
    std::string weatherDescription;
    std::string cityName;
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
    void showErrorScreen(const std::string& title, const std::string& message);
    void showCustomScreen(const CustomScreenData& data);
    void showFontPreview();
    void showDashboard(const DashboardData& data);

    void goToSleep(uint32_t sleepDurationSeconds, uint32_t delayBeforeSleepMs = 2000U);
private:
    bool initialised = false;
    EpdiyHighlevelState displayState{};
    uint8_t* fb = nullptr;

    void clearFramebuffer();
    void presentFramebuffer();
    void displayString(const GFXfont& font, const std::string& str, int32_t x, int32_t y, Alignment horizontalAlignment = Alignment::Left);
    void displayString(const GFXfont& font, const char* str, int32_t x, int32_t y, Alignment horizontalAlignment = Alignment::Left);
    void displayString(const std::string& str, int32_t x, int32_t y, Alignment horizontalAlignment = Alignment::Left);
    void displayString(const char* str, int32_t x, int32_t y, Alignment horizontalAlignment = Alignment::Left);
    void displayWrappedString(const GFXfont& font, const std::string& str, int32_t x, int32_t y, int32_t maxWidth, Alignment horizontalAlignment = Alignment::Left, int32_t lineSpacing = 4);
    std::vector<std::string> wrapText(const GFXfont& font, const std::string& str, int32_t maxWidth);
    int32_t measureTextWidth(const GFXfont& font, const std::string& str);

};