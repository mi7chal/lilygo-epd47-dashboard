#pragma once

#include <time.h>
#include <string>
#include "StorageManager.h"

class TimeManager {
public:
    explicit TimeManager(StorageManager& storage);

    // Start NTP client (non-blocking)
    void begin();

    // Return timezone offset in seconds for given coordinates (may perform HTTP request)
    // Returns 0 if unknown or on error.
    long fetchTimezoneOffsetSeconds(double lat, double lon);

    // Return local time string like "2026-05-27 14:12:03 +02:00" computed from NTP + timezone offset
    // If timezone cannot be fetched, offset 0 (UTC) is used.
    std::string getLocalTimeString(double lat, double lon);

private:
    StorageManager& storage;
    bool ntpStarted = false;
};
