#pragma once

#include <optional>
#include <cstdint>
#include <memory>
#include <string>

#include "StorageManager.h"

class WebPortal {
public:
    WebPortal(StorageManager& storage);
    ~WebPortal();

    void start();
    void processDNS();
    bool attemptGeocodeNow();

private:
    struct Impl;

    std::unique_ptr<Impl> impl;
    StorageManager& storage;
    void setupEndpoints();

};
