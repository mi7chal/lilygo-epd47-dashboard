#include "DisplayManager.h"

#include <esp_sleep.h>
#include <fmt/format.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "Log.h"
#include "epd_board.h"
#include "epd_display.h"
#include "epd_driver.h"
#include "fonts/InterBold24.h"
#include "fonts/InterRegular12.h"
#include "fonts/InterRegular16.h"
#include "fonts/InterRegular24.h"


void DisplayManager::init() {
  if (initialised) return;

  logger::info("init");

  epd_init(&lilygo_board_s3, &ED047TC1, EPD_LUT_64K);
  epd_set_rotation(EPD_ROT_LANDSCAPE);
  displayState = epd_hl_init(EPD_BUILTIN_WAVEFORM);
  fb = epd_hl_get_framebuffer(&displayState);
  epd_poweron();

  clearFramebuffer();

  initialised = true;
  logger::info("init complete");
}

void DisplayManager::showCustomScreen(const CustomScreenData& data) {
  if (!initialised) return;

  logger::info("show loading");
  epd_poweron();
  clearFramebuffer();

  const int32_t contentWidth = epd_width() - 40;
  const auto titleLines = wrapText(InterBold24, data.title, contentWidth);
  const int32_t titleLineHeight = InterBold24.advance_y + 4;
  const int32_t titleBlockHeight = static_cast<int32_t>(titleLines.size()) * titleLineHeight;

  displayWrappedString(InterBold24, data.title, epd_width() / 2, epd_height() / 2 - titleBlockHeight - 24, contentWidth,
                       Alignment::Center);
  displayWrappedString(InterRegular16, data.message, epd_width() / 2, epd_height() / 2, contentWidth,
                       Alignment::Center);

  presentFramebuffer();
  logger::info("display updated (loading), powering off display");
  epd_poweroff();
}

void DisplayManager::showFontPreview() {
  if (!initialised) return;

  logger::info("show font preview");
  epd_poweron();
  clearFramebuffer();

  displayString(InterRegular12, "Inter Regular 12", 80, 90);
  displayString(InterRegular16, "Inter Regular 16", 80, 150);
  displayString(InterRegular24, "Inter Regular 24", 80, 230);
  displayString(InterBold24, "Inter Bold 24", 80, 330);

  presentFramebuffer();
  logger::info("display updated (font preview), powering off display");
  epd_poweroff();
}

void DisplayManager::showErrorScreen(const std::string& title, const std::string& message) {
  if (!initialised) return;

  logger::warn("show error");
  epd_poweron();
  clearFramebuffer();
  displayWrappedString(InterBold24, title, 20, 40, epd_width() - 40);
  displayWrappedString(InterRegular16, message, 20, 88, epd_width() - 40);
  presentFramebuffer();
  logger::info("display updated (error), powering off display");
  epd_poweroff();
}

void DisplayManager::showDashboard(const DashboardData& data) {
  if (!initialised) return;

  logger::info("dashboard: {} / {}", data.cityName.c_str(), data.weatherDescription.c_str());

  // if display was powered off previously, ensure power is on for update
  epd_poweron();
  clearFramebuffer();

  displayString(InterBold24, data.cityName, 0, 0);
  displayWrappedString(InterRegular16, data.weatherDescription, 0, 24, epd_width() - 20);

  const auto temperatureLine = fmt::format("Temp: {:.1f} C", data.temperatureCelsius);
  displayString(temperatureLine.c_str(), 0, 64);
  const auto humidityLine = fmt::format("Humidity: {} %", static_cast<unsigned int>(data.humidityPercent));
  displayString(humidityLine.c_str(), 0, 88);
  const auto pressureLine = fmt::format("Pressure: {} hPa", static_cast<unsigned int>(data.pressureHpa));
  displayString(pressureLine.c_str(), 0, 112);
  const auto windLine = fmt::format("Wind: {:.1f} m/s", data.windSpeedMs);
  displayString(windLine.c_str(), 0, 136);

  presentFramebuffer();
  logger::info("display updated (dashboard), powering off display");
  epd_poweroff();
}

void DisplayManager::goToSleep(uint32_t sleepDurationSeconds, uint32_t delayBeforeSleepMs) {
  if (!initialised) return;

  logger::info("sleep in {} ms for {} s", static_cast<unsigned int>(delayBeforeSleepMs),
               static_cast<unsigned int>(sleepDurationSeconds));

  vTaskDelay(pdMS_TO_TICKS(delayBeforeSleepMs));

  epd_poweroff();

  const uint64_t wakeupMicros = static_cast<uint64_t>(sleepDurationSeconds) * 1000000ULL;  // seconds -> microseconds
  esp_sleep_enable_timer_wakeup(wakeupMicros);
  logger::info("entering deep sleep");
  esp_deep_sleep_start();
}

void DisplayManager::clearFramebuffer() {
  if (fb == nullptr) {
    return;
  }

  epd_hl_set_all_white(&displayState);
}

void DisplayManager::presentFramebuffer() {
  if (fb == nullptr) {
    return;
  }

  epd_hl_update_screen(&displayState, static_cast<EpdDrawMode>(EPD_MODE_DEFAULT), 25);
}

void DisplayManager::displayString(const std::string& str, int32_t x, int32_t y, Alignment horizontalAlignment) {
  displayString(InterRegular16, str.c_str(), x, y, horizontalAlignment);
}

void DisplayManager::displayString(const char* str, int32_t x, int32_t y, Alignment horizontalAlignment) {
  displayString(InterRegular16, str, x, y, horizontalAlignment);
}

void DisplayManager::displayString(const GFXfont& font, const char* str, int32_t x, int32_t y,
                                   Alignment horizontalAlignment) {
  if (fb == nullptr || str == nullptr || *str == '\0') {
    return;
  }

  EpdFontProperties props = epd_font_properties_default();
  props.flags = EPD_DRAW_BACKGROUND;
  if (horizontalAlignment == Alignment::Center) {
    props.flags = static_cast<EpdFontFlags>(props.flags | EPD_DRAW_ALIGN_CENTER);
  } else if (horizontalAlignment == Alignment::Right) {
    props.flags = static_cast<EpdFontFlags>(props.flags | EPD_DRAW_ALIGN_RIGHT);
  } else {
    props.flags = static_cast<EpdFontFlags>(props.flags | EPD_DRAW_ALIGN_LEFT);
  }

  int cursorX = x;
  int cursorY = y;
  epd_write_string(&font, str, &cursorX, &cursorY, fb, &props);
}

void DisplayManager::displayString(const GFXfont& font, const std::string& str, int32_t x, int32_t y,
                                   Alignment horizontalAlignment) {
  displayString(font, str.c_str(), x, y, horizontalAlignment);
}

void DisplayManager::displayWrappedString(const GFXfont& font, const std::string& str, int32_t x, int32_t y,
                                          int32_t maxWidth, Alignment horizontalAlignment, int32_t lineSpacing) {
  const auto lines = wrapText(font, str, maxWidth);
  const int32_t lineHeight = (font.advance_y > 0 ? font.advance_y : 20) + lineSpacing;

  int32_t cursorY = y;
  for (const auto& line : lines) {
    displayString(font, line, x, cursorY, horizontalAlignment);
    cursorY += lineHeight;
  }
}

int32_t DisplayManager::measureTextWidth(const GFXfont& font, const std::string& str) {
  if (str.empty()) {
    return 0;
  }

  int cursorX = 0;
  int cursorY = 0;
  int x1 = 0;
  int y1 = 0;
  int width = 0;
  int height = 0;
  epd_get_text_bounds(&font, str.c_str(), &cursorX, &cursorY, &x1, &y1, &width, &height, nullptr);
  return width;
}

// todo refactor this function
std::vector<std::string> DisplayManager::wrapText(const GFXfont& font, const std::string& str, int32_t maxWidth) {
  std::vector<std::string> lines;

  if (str.empty()) {
    lines.emplace_back("");
    return lines;
  }

  std::string currentLine;
  std::string currentWord;

  auto flushCurrentWord = [&]() {
    if (currentWord.empty()) {
      return;
    }

    if (currentLine.empty()) {
      if (measureTextWidth(font, currentWord) <= maxWidth) {
        currentLine = currentWord;
      } else {
        std::string chunk;
        for (size_t index = 0; index < currentWord.length(); ++index) {
          const std::string candidate = chunk + currentWord[index];
          if (chunk.empty() || measureTextWidth(font, candidate) <= maxWidth) {
            chunk = candidate;
          } else {
            lines.push_back(chunk);
            chunk = std::string(1, currentWord[index]);
          }
        }

        currentLine = chunk;
      }
    } else {
      const std::string candidate = currentLine + " " + currentWord;
      if (measureTextWidth(font, candidate) <= maxWidth) {
        currentLine = candidate;
      } else {
        lines.push_back(currentLine);
        currentLine.clear();

        if (measureTextWidth(font, currentWord) <= maxWidth) {
          currentLine = currentWord;
        } else {
          std::string chunk;
          for (size_t index = 0; index < currentWord.length(); ++index) {
            const std::string candidateChunk = chunk + currentWord[index];
            if (chunk.empty() || measureTextWidth(font, candidateChunk) <= maxWidth) {
              chunk = candidateChunk;
            } else {
              lines.push_back(chunk);
              chunk = std::string(1, currentWord[index]);
            }
          }

          currentLine = chunk;
        }
      }
    }

    currentWord.clear();
  };

  for (size_t index = 0; index < str.length(); ++index) {
    const char character = str[index];
    if (character == '\r') {
      continue;
    }

    if (character == '\n') {
      flushCurrentWord();
      lines.push_back(currentLine);
      currentLine.clear();
      continue;
    }

    if (character == ' ') {
      flushCurrentWord();
      continue;
    }

    currentWord += character;
  }

  flushCurrentWord();

  if (!currentLine.empty() || lines.empty()) {
    lines.push_back(currentLine);
  }

  return lines;
}
