# Wakeup Alarm (Refactored)

A robust, non-blocking, and network-flexible wakeup sequence for NodeMCU (ESP8266). This system controls an AC dimmer and multiple relays to create a gradual "sunrise" effect, warming an electric blanket, and eventually pulsing floodlights to ensure you're awake.

## Key Features

- **ArduinoOTA Support:** Update your firmware wirelessly over WiFi. No more USB cables required once the device is deployed.
- **Captive Portal Configuration:** Powered by `WiFiManager`. If the device can't connect to WiFi, it starts a portal named `Wakeup-Alarm-Setup` for easy credential entry.
- **AutoRemote "Self-Finding" IP:** Automatically sends its IP address to your phone via AutoRemote on every boot.
- **mDNS Resolution:** Access the dashboard via `http://esp-wake.local`.
- **Non-blocking Execution:** The alarm sequence runs in the background; the web server and OTA listener remain responsive.
- **Duration-based Sequence:** Once triggered, phase timing uses `millis()` elapsed time — no RTC required.
- **Flexible API:** Start the sequence on demand (Tasker), cancel at any time, or optionally schedule a future start via NTP.

## Hardware Setup

### Components
- **Microcontroller:** NodeMCU v2 (ESP8266)
- **Dimmer:** RobotDyn AC Dimmer (PWM: D6, ZC: D5)
- **Relays (Active LOW):**
  - **Relay 1 (D7):** Main Power to Dimmer/Outlets
  - **Relay 2 (D4):** Floodlights
  - **Relay 3 (D1):** Electric Blanket
  - **Relay 4 (D2):** Aux / Spare

> [!NOTE]
> A physical RTC is not used. The wakeup sequence runs from elapsed time after a trigger request. NTP is only needed for the optional `/set` endpoint and the dashboard clock display.

## Getting Started

1.  **Local secrets:** Copy `.env.example` to `.env` and set `WAKEUP_AUTOREMOTE_KEY` if you use AutoRemote check-in. `.env` is gitignored and is never compiled into the repo. The firmware skips AutoRemote when the variable is empty.
2.  **Initial USB Flash:** Use PlatformIO to upload the code to your NodeMCU.
3.  **Initial WiFi Setup:**
    - Connect your phone to the WiFi network: `Wakeup-Alarm-Setup`.
    - A configuration portal will appear. Select your home WiFi and enter the password.
    - The device will save these credentials and reboot.
4.  **Wireless Updates (OTA):**
    - **PlatformIO**: Switch to the `env:nodemcuv2_ota` environment in the Task/Project sidebar and click **Upload**. It will target `esp-wake.local` automatically.
    - **Arduino IDE**: Once the device is on your network, it will appear as a Network Port: `esp8266-ota-node`.
    - You can now push updates wirelessly without a USB cable.

## Usage & API

### Trigger (Tasker integration — primary)
Tasker handles *when* to start (e.g. one hour before wake time). The ESP starts the sequence immediately when the HTTP request arrives.

- **Start sequence:** `GET /trigger`
- **With floodlight mode:** `GET /trigger?blink=1` (pulse, default) or `GET /trigger?blink=0` (steady on)
- **Legacy alias:** `GET /alarm/on` (same as `/trigger`)
- **Cancel:** `GET /cancel` or `GET /alarm/off`

**Tasker example:**
```
http://%Arduino_IP/trigger?blink=%blink
```
Set `%blink` to `1` or `0` before the HTTP Request (default is blink if omitted).

### Optional: ESP-side scheduling (`/set`)
Only needed if you want Tasker to send the alarm time *early* and have the ESP wait until that Unix timestamp (requires NTP). Most setups should use `/trigger` instead.

- **Endpoint:** `GET /set?time=<unix_timestamp>&blink=0`
- **Requires:** WiFi + NTP sync after boot

## Alarm Sequence
1.  **Warming Blanket (5 mins):** Relay 3 turns ON.
2.  **Brightening (3 mins):** Relay 1 turns ON. Dimmer ramps from 22% to 94%.
3.  **Floodlight Wait (2 mins):** Lights stay at max brightness.
4.  **Floodlights:** Relay 2 turns on — pulses every 500 ms if `blink=1`, or stays on if `blink=0`.
5.  **Steady On:** Lights stay on until the 2-hour safety cutoff is reached.
6.  **Cleanup:** Everything turns OFF.

## Troubleshooting

### "ISR not in IRAM" Crash
The included `RBDdimmer` library has been patched in this repository (`lib/RBDDimmer-master`) to include `ICACHE_RAM_ATTR` on interrupt handlers, preventing crashes on newer ESP8266 cores.

### Resetting WiFi Credentials
If you change your router or password:
1. The device will fail to connect and automatically restart the `Wakeup-Alarm-Setup` portal.
2. Alternatively, uncomment `wifiManager.resetSettings();` in `src/main.cpp` and reflash.

## Dependencies
- `tzapu/WiFiManager`
- `arduino-libraries/NTPClient`
- `RBDdimmer` (Included in `lib/`)
