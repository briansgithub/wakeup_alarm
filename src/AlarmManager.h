#ifndef ALARM_MANAGER_H
#define ALARM_MANAGER_H

#include <Arduino.h>
#include "RBDdimmer.h"

// Hardware Pin Definitions
#define RELAY_LIGHTS_DIMMER 13 // D7 (Relay 1)
#define RELAY_FLOODLIGHTS   2  // D4 (Relay 2)
#define RELAY_BLANKET       5  // D1 (Relay 3)
#define RELAY_AUX           4  // D2 (Relay 4)

#define DIMMER_PWM_PIN 12 // D6
#define DIMMER_ZC_PIN  14 // D5

// Brightness Constants
#define MIN_BRIGHT 22
#define MID_BRIGHT 55
#define MAX_BRIGHT 94

enum AlarmState {
    IDLE,
    WARMING_BLANKET,
    BRIGHTENING,
    FLOODLIGHT_WAIT,
    BLINKING,
    CLEANUP
};

struct AlarmProfile {
    bool floodlightBlink = true;
};

class AlarmManager {
private:
    dimmerLamp dimmer;
    AlarmState state = IDLE;
    unsigned long stateStartTime = 0;
    uint32_t scheduledTime = 0; // Unix timestamp
    AlarmProfile scheduledProfile;
    AlarmProfile activeProfile;

    // Timing configuration (ms)
    unsigned long durBlanketWarmup = 5 * 60 * 1000;
    unsigned long durBrightening = 3 * 60 * 1000;
    unsigned long pauseBeforeFloodlights = 2 * 60 * 1000;
    unsigned long pauseBeforeBlinking = 5 * 60 * 1000;
    unsigned long durBlinking = 4 * 60 * 1000;
    unsigned long durNotBlinking = 2 * 60 * 1000;
    unsigned long durUntilLightsOff = 2 * 60 * 60 * 1000;

    int currentBrightness = MIN_BRIGHT;
    unsigned long sequenceStartTime = 0;
    unsigned long lastBlinkToggle = 0;
    bool blinkState = false;

public:
    AlarmManager() : dimmer(DIMMER_PWM_PIN, DIMMER_ZC_PIN) {}

    void begin() {
        pinMode(RELAY_LIGHTS_DIMMER, OUTPUT);
        pinMode(RELAY_FLOODLIGHTS, OUTPUT);
        pinMode(RELAY_BLANKET, OUTPUT);
        pinMode(RELAY_AUX, OUTPUT);

        // Relays are active LOW
        digitalWrite(RELAY_LIGHTS_DIMMER, HIGH);
        digitalWrite(RELAY_FLOODLIGHTS, HIGH);
        digitalWrite(RELAY_BLANKET, HIGH);
        digitalWrite(RELAY_AUX, HIGH);

        dimmer.begin(NORMAL_MODE, ON);
        dimmer.setPower(MIN_BRIGHT);
    }

    void trigger(AlarmProfile profile = AlarmProfile()) {
        if (state == IDLE) {
            activeProfile = profile;
            Serial.println("Alarm Triggered!");
            Serial.println(activeProfile.floodlightBlink ? "Floodlights: blink" : "Floodlights: steady on");
            state = WARMING_BLANKET;
            stateStartTime = millis();
            sequenceStartTime = millis();
            digitalWrite(RELAY_BLANKET, LOW); // Turn on blanket
        }
    }

    void cancel() {
        Serial.println("Alarm Canceled");
        state = CLEANUP;
    }

    void setSchedule(uint32_t unixTimestamp, AlarmProfile profile = AlarmProfile()) {
        scheduledTime = unixTimestamp;
        scheduledProfile = profile;
        Serial.print("Alarm scheduled for: ");
        Serial.println(scheduledTime);
        Serial.println(scheduledProfile.floodlightBlink ? "Floodlights: blink" : "Floodlights: steady on");
    }

    void update(uint32_t currentTimeUnix) {
        // Check schedule
        if (state == IDLE && scheduledTime > 0 && currentTimeUnix >= scheduledTime) {
            AlarmProfile profile = scheduledProfile;
            scheduledTime = 0;
            scheduledProfile = AlarmProfile();
            trigger(profile);
        }

        unsigned long now = millis();
        unsigned long elapsed = now - stateStartTime;

        switch (state) {
            case WARMING_BLANKET:
                if (elapsed >= durBlanketWarmup) {
                    state = BRIGHTENING;
                    stateStartTime = now;
                    digitalWrite(RELAY_LIGHTS_DIMMER, LOW); // Power to dimmer
                    currentBrightness = MIN_BRIGHT;
                    Serial.println("Starting Brightening phase");
                }
                break;

            case BRIGHTENING: {
                unsigned long brighteningElapsed = now - stateStartTime;
                if (brighteningElapsed >= durBrightening) {
                    dimmer.setPower(MAX_BRIGHT);
                    state = FLOODLIGHT_WAIT;
                    stateStartTime = now;
                    Serial.println("Starting Floodlight Wait phase");
                } else {
                    // Two-stage ramping logic from original code
                    float progress = (float)brighteningElapsed / durBrightening;
                    int targetBrightness;

                    if (progress < 0.9f) {
                        float subProgress = progress / 0.9f;
                        targetBrightness = MIN_BRIGHT + (int)(subProgress * (MID_BRIGHT - MIN_BRIGHT));
                    } else {
                        float subProgress = (progress - 0.9f) / 0.1f;
                        targetBrightness = MID_BRIGHT + (int)(subProgress * (MAX_BRIGHT - MID_BRIGHT));
                    }

                    if (targetBrightness != currentBrightness) {
                        currentBrightness = targetBrightness;
                        dimmer.setPower(currentBrightness);
                        Serial.print("Power: "); Serial.println(currentBrightness);
                    }
                }
                break;
            }

            case FLOODLIGHT_WAIT:
                if (elapsed >= pauseBeforeFloodlights) {
                    digitalWrite(RELAY_FLOODLIGHTS, LOW); // Turn on floodlights
                    state = BLINKING;
                    stateStartTime = now;
                    lastBlinkToggle = now;
                    Serial.println(activeProfile.floodlightBlink ? "Starting Blinking phase" : "Starting Steady floodlight phase");
                }
                break;

            case BLINKING:
                if ((now - sequenceStartTime) >= durUntilLightsOff) {
                    state = CLEANUP;
                } else if (!activeProfile.floodlightBlink) {
                    digitalWrite(RELAY_FLOODLIGHTS, LOW); // Stay on
                } else {
                    // Blinking logic: Pulse for durBlinking, then stay on for durNotBlinking
                    unsigned long cycleTime = (now - stateStartTime) % (durBlinking + durNotBlinking);
                    if (cycleTime < durBlinking) {
                        if (now - lastBlinkToggle >= 500) {
                            blinkState = !blinkState;
                            digitalWrite(RELAY_FLOODLIGHTS, blinkState ? LOW : HIGH);
                            lastBlinkToggle = now;
                        }
                    } else {
                        digitalWrite(RELAY_FLOODLIGHTS, LOW); // Keep ON during "not blinking" period
                    }
                }
                break;

            case CLEANUP:
                digitalWrite(RELAY_LIGHTS_DIMMER, HIGH);
                digitalWrite(RELAY_FLOODLIGHTS, HIGH);
                digitalWrite(RELAY_BLANKET, HIGH);
                digitalWrite(RELAY_AUX, HIGH);
                dimmer.setPower(MIN_BRIGHT);
                state = IDLE;
                Serial.println("Cleanup complete. Back to IDLE.");
                break;

            case IDLE:
            default:
                break;
        }
    }

    String getStatus() {
        switch (state) {
            case IDLE: return "IDLE";
            case WARMING_BLANKET: return "WARMING BLANKET";
            case BRIGHTENING: return "BRIGHTENING";
            case FLOODLIGHT_WAIT: return "FLOODLIGHT WAIT";
            case BLINKING:
                return activeProfile.floodlightBlink ? "BLINKING" : "FLOODLIGHTS ON";
            case CLEANUP: return "CLEANUP";
            default: return "UNKNOWN";
        }
    }

    uint32_t getScheduledTime() { return scheduledTime; }
    bool getScheduledFloodlightBlink() { return scheduledProfile.floodlightBlink; }
};

#endif