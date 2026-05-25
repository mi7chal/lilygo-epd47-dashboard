#include "DisplayManager.h"
#include "epd_driver.h"
#include "firasans.h"

#include <cstdlib>
#include <cstring>
#include <cstdio>


void DisplayManager::init() {
    if (initialised) return;

    Serial.println("[Display] init");

    epd_init();
    epd_poweron();
    epd_clear();

    const size_t framebufferSize = EPD_WIDTH * EPD_HEIGHT / 2;
    Serial.printf("[Display] allocating framebuffer: %u bytes\n", static_cast<unsigned int>(framebufferSize));

    fb.reset(static_cast<uint8_t*>(ps_calloc(sizeof(uint8_t), framebufferSize)));
    if (!fb) {
        Serial.println("[Display] framebuffer allocation failed");
        while (1);
    }

    clearFramebuffer();

    initialised = true;
    Serial.println("[Display] init complete");
}

void DisplayManager::showLoadingScreen() {
    if (!initialised) return;

    Serial.println("[Display] show loading");
    clearFramebuffer();
    displayString("Loading...", 0, 50);
    presentFramebuffer();
    Serial.println("[Display] display updated (loading), powering off display");
    epd_poweroff_all();
}

void DisplayManager::showErrorScreen(const String& title, const String& message) {
    if (!initialised) return;

    Serial.println("[Display] show error");
    clearFramebuffer();
    displayString(title, 0, 50);
    displayString(message, 0, 70);
    presentFramebuffer();
    Serial.println("[Display] display updated (error), powering off display");
    epd_poweroff_all();
}

void DisplayManager::showDashboard(const DashboardData& data) {
    if (!initialised) return;

    Serial.printf("[Display] dashboard: %s / %s\n", data.cityName.c_str(), data.weatherDescription.c_str());

    // if display was powered off previously, ensure power is on for update
    epd_poweron();
    clearFramebuffer();

    displayString(data.cityName, 0, 0);
    displayString(data.weatherDescription, 0, 24);

    char line[48];
    std::snprintf(line, sizeof(line), "Temp: %.1f C", data.temperatureCelsius);
    displayString(line, 0, 64);
    std::snprintf(line, sizeof(line), "Humidity: %u %%", static_cast<unsigned int>(data.humidityPercent));
    displayString(line, 0, 88);
    std::snprintf(line, sizeof(line), "Pressure: %u hPa", static_cast<unsigned int>(data.pressureHpa));
    displayString(line, 0, 112);
    std::snprintf(line, sizeof(line), "Wind: %.1f m/s", data.windSpeedMs);
    displayString(line, 0, 136);

    presentFramebuffer();
    Serial.println("[Display] display updated (dashboard), powering off display");
    epd_poweroff_all();
}

void DisplayManager::goToSleep(uint32_t sleepDurationSeconds, uint32_t delayBeforeSleepMs) {
    if (!initialised) return;

    Serial.printf("[Display] sleep in %u ms for %u s\n", static_cast<unsigned int>(delayBeforeSleepMs), static_cast<unsigned int>(sleepDurationSeconds));

    delay(delayBeforeSleepMs);

    epd_poweroff_all();

    const uint64_t wakeupMicros = static_cast<uint64_t>(sleepDurationSeconds) * 1000000ULL; // seconds -> microseconds
    esp_sleep_enable_timer_wakeup(wakeupMicros); 
    Serial.println("[Display] entering deep sleep");
    esp_deep_sleep_start();
}

void DisplayManager::clearFramebuffer() {
    if (fb == nullptr) {
        return;
    }

    std::memset(fb.get(), 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);
}

void DisplayManager::presentFramebuffer() {
    if (fb == nullptr) {
        return;
    }

    epd_draw_grayscale_image(epd_full_screen(), fb.get());
}

void DisplayManager::displayString(const String& str, int32_t x, int32_t y, Alignment horizontalAlignment) {
    displayString(str.c_str(), x, y, horizontalAlignment);
}

void DisplayManager::displayString(const char* str, int32_t x, int32_t y, Alignment horizontalAlignment) {
    int x1, y1, w, h; 

    get_text_bounds(&FiraSans, str, &x, &y, &x1, &y1, &w, &h, NULL);
    
    int32_t cursor_x = x; 
    int32_t cursor_y = y + h; // make user given y coordinate the top of the text, not the baseline

    switch (horizontalAlignment) {
        case Alignment::Center:
            cursor_x -= w / 2;
            break;
        case Alignment::Right:
            cursor_x -= w;
            break;
        default:
            break;
    }

    write_string(&FiraSans, str, &cursor_x, &cursor_y, fb.get());
}