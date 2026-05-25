#pragma once

#include <Arduino.h>
#include <Preferences.h>


class PreferencesProxy {
private:
    Preferences preferences;
    bool preferencesOpened = false;

public:
    PreferencesProxy() = default;

    [[nodiscard]] bool arePreferencesOpened() const {
        return preferencesOpened;
    }

    bool begin(const char* namespaceName, bool readOnly) {
        if (!preferencesOpened) {
            preferencesOpened = preferences.begin(namespaceName, readOnly);
        }
        return preferencesOpened;
    }

    void end() {
        if (preferencesOpened) {
            preferences.end();
            preferencesOpened = false;
        }
    }

    String getString(const char* key, const String& defaultValue) {
        return preferences.getString(key, defaultValue.c_str());
    }

    void putString(const char* key, const String& value) {
        preferences.putString(key, value.c_str());
    }

};