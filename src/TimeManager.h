#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

#include <NTPClient.h>
#include <WiFiUdp.h>

class TimeManager {
private:
    WiFiUDP ntpUDP;
    NTPClient ntpClient;

public:
    TimeManager() : ntpClient(ntpUDP, "pool.ntp.org") {}

    void begin() {
        ntpClient.begin();
        // UTC; used for dashboard clock and optional /set scheduling
        ntpClient.setTimeOffset(0);
        Serial.println("TimeManager (NTP only) started.");
    }

    void update() {
        if (WiFi.status() == WL_CONNECTED) {
            ntpClient.update();
        }
    }

    uint32_t getNow() {
        return ntpClient.getEpochTime();
    }

    String getFormattedTime() {
        return ntpClient.getFormattedTime();
    }
};

#endif