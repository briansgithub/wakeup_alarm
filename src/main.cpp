#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include "AlarmManager.h"
#include "TimeManager.h"

// AutoRemote key comes from local-only WAKEUP_AUTOREMOTE_KEY (see .env / scripts/load_env.py).
#ifndef WAKEUP_AUTOREMOTE_KEY
#define WAKEUP_AUTOREMOTE_KEY ""
#endif
static const char* AR_KEY = WAKEUP_AUTOREMOTE_KEY;

ESP8266WebServer server(80);
AlarmManager alarm;
TimeManager timeManager;

AlarmProfile profileFromRequest() {
    AlarmProfile profile;
    if (server.hasArg("blink")) {
        String v = server.arg("blink");
        v.toLowerCase();
        profile.floodlightBlink = !(v == "0" || v == "false" || v == "off" || v == "no");
    }
    return profile;
}

// Sends the current IP address to your phone via AutoRemote.
// Tasker profile catches "WakeupIP=:=" and stores the value in %Arduino_IP.
void sendAutoRemoteCheckIn() {
    if (AR_KEY == nullptr || AR_KEY[0] == '\0') {
        Serial.println("AutoRemote skipped: WAKEUP_AUTOREMOTE_KEY is not set.");
        return;
    }

    WiFiClientSecure client;
    client.setInsecure(); // Skip certificate pinning for simplicity
    HTTPClient https;

    String ip  = WiFi.localIP().toString();
    // "=:=" URL-encoded is "%3D%3A%3D"
    String url = "https://autoremotejoaomgcd.appspot.com/sendmessage?key=";
    url += AR_KEY;
    url += "&message=WakeupIP%3D%3A%3D";
    url += ip;

    Serial.println("Sending AutoRemote check-in for IP: " + ip);
    if (https.begin(client, url)) {
        int code = https.GET();
        Serial.println("AutoRemote response code: " + String(code));
        https.end();
    } else {
        Serial.println("AutoRemote: failed to connect.");
    }
}

void handleRoot() {
    String html = "<html><head><title>Wakeup Alarm</title>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<style>body{font-family:sans-serif;text-align:center;padding:20px;}";
    html += ".btn{display:inline-block;padding:15px 30px;margin:10px;color:white;text-decoration:none;border-radius:5px;font-size:18px;}";
    html += ".trigger{background:#2ecc71;}.cancel{background:#e74c3c;}</style></head><body>";
    html += "<h1>Wakeup Alarm Control</h1>";
    html += "<p>Status: <strong>" + alarm.getStatus() + "</strong></p>";
    html += "<p>Current Time: " + timeManager.getFormattedTime() + "</p>";
    html += "<p><em>Sequence timing runs from elapsed time after trigger.</em></p>";

    uint32_t scheduled = alarm.getScheduledTime();
    if (scheduled > 0) {
        html += "<p>Scheduled start (optional /set): " + String(scheduled) + " (Unix)</p>";
        html += "<p>Floodlights: <strong>" +
                String(alarm.getScheduledFloodlightBlink() ? "blink" : "steady on") +
                "</strong></p>";
    }

    html += "<a href='/trigger' class='btn trigger'>Trigger (blink)</a>";
    html += "<a href='/trigger?blink=0' class='btn trigger'>Trigger (steady)</a>";
    html += "<a href='/cancel' class='btn cancel'>Cancel</a>";
    html += "<hr><p><em>Tasker: GET /trigger?blink=0 &nbsp;|&nbsp; OTA: esp8266-ota-node</em></p>";
    html += "</body></html>";
    server.send(200, "text/html", html);
}

void handleTrigger() {
    alarm.trigger(profileFromRequest());
    server.sendHeader("Location", "/");
    server.send(303);
}

void handleCancel() {
    alarm.cancel();
    server.sendHeader("Location", "/");
    server.send(303);
}

// Optional: ESP-side scheduling via NTP. Primary integration is /trigger (Tasker sends at start time).
void handleSet() {
    if (server.hasArg("time")) {
        uint32_t t = server.arg("time").toInt();
        AlarmProfile profile = profileFromRequest();
        alarm.setSchedule(t, profile);
        String response = "Scheduled for " + String(t);
        response += profile.floodlightBlink ? ", blink=1" : ", blink=0";
        server.send(200, "text/plain", response);
    } else {
        server.send(400, "text/plain", "Missing 'time' parameter");
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("\nStarting Wakeup Alarm System...");

    // 1. WiFiManager
    WiFiManager wifiManager;
    // wifiManager.resetSettings();
    if (!wifiManager.autoConnect("Wakeup-Alarm-Setup")) {
        Serial.println("Failed to connect and hit timeout");
        delay(3000);
        ESP.restart();
    }

    Serial.println("WiFi Connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    // 2. mDNS Setup
    if (MDNS.begin("esp-wake")) {
        Serial.println("MDNS responder started: http://esp-wake.local");
    }

    // 3. ArduinoOTA Setup
    ArduinoOTA.setHostname("esp8266-ota-node");
    ArduinoOTA.onStart([]() {
        String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
        Serial.println("Start updating " + type);
    });
    ArduinoOTA.onEnd([]() { Serial.println("\nEnd"); });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("Error[%u]: ", error);
    });
    ArduinoOTA.begin();

    // 4. Component Init
    sendAutoRemoteCheckIn();
    timeManager.begin();
    alarm.begin();

    // 5. API Endpoints — /trigger is primary; /set is optional ESP-side scheduling
    server.on("/", handleRoot);
    server.on("/trigger", handleTrigger);
    server.on("/alarm/on", handleTrigger);
    server.on("/cancel", handleCancel);
    server.on("/alarm/off", handleCancel);
    server.on("/set", handleSet); // optional: requires NTP, GET /set?time=<unix>&blink=0

    server.begin();
    Serial.println("HTTP server started and OTA ready");
}

void loop() {
    ArduinoOTA.handle();
    server.handleClient();
    MDNS.update();
    timeManager.update();
    alarm.update(timeManager.getNow());
}
