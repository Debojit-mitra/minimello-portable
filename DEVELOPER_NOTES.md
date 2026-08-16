# MiniMello Portable - Developer Reference & History

This document serves as a central hub for understanding the architecture of the MiniMello firmware and tracking the history of major features and bug fixes. Refer to this when returning to the codebase to understand *why* certain decisions were made and *where* features are located.

---

## 1. System Architecture Quick-Reference

MiniMello runs on an ESP32-C3 microcontroller. The firmware is heavily modularized into distinct manager classes to separate concerns.

### Core Modules
| Module | File(s) | Responsibility |
| :--- | :--- | :--- |
| **Main Setup/Loop** | `main.cpp` | Initializes all managers, contains the primary execution loop, and handles Night Mode logic. Includes a 2s boot delay to allow USB CDC to enumerate for early logs. |
| **Config Manager** | `config_manager.h/cpp` | Saves/loads preferences to non-volatile storage (Preferences API). |
| **Network Manager** | `network_manager.h/cpp` | Handles Wi-Fi connections, Captive Portal (DNS Server), and AP mode fallback. Features a 60s background retry loop when in AP mode to recover from router outages. |
| **Web Server** | `web_server.h/cpp` | Asynchronous Web UI endpoint (AsyncWebServer). Serves HTML/JS/CSS from Gzipped PROGMEM byte arrays instead of LittleFS. Exposes `/api/...` endpoints. |
| **Screen Manager** | `screen_manager.h/cpp` | Handles toggling and transition animations between the Emotion Engine and Clock Engine. |
| **Emotion Engine** | `emotion_engine.h/cpp` | Renders dynamic OLED faces (blinking, eye movements, varying emotions). |
| **Clock Engine** | `clock_engine.h/cpp` | Renders the time, date, battery status, and weather dashboard. |
| **Weather Service** | `weather_service.h/cpp` | Fetches data from OpenWeatherMap. Uses a 60s retry backoff on failures to prevent 30-minute lockouts. |
| **Touch Manager** | `touch_manager.h/cpp` | Handles TTP223 capacitive touch input (TAP, DOUBLE_TAP, LONG_PRESS, and a 5s VERY_LONG_PRESS for safe hardware restarts). |
| **OTA Manager** | `ota_manager.h/cpp` | Communicates with the public GitHub Releases repository to detect and install firmware updates. |

### Build System & Tooling
| Tool | Location | Purpose |
| :--- | :--- | :--- |
| **WebUI Bundler** | `scripts/build_webui.py` | A PlatformIO pre-build hook that automatically minifies and Gzips `data/` assets into `src/web_ui_data.h`. |

---

## 2. Development History & Changelog

A chronological log of features, UX improvements, and bug fixes to track the project's evolution.

### v0.2.1 (Unreleased)
| Type | Description | Files Affected |
| :--- | :--- | :--- |
| ✨ Feature | **OLED Burn-in Protection (Pixel Shift):** Implemented authentic, subtle pixel shifting (drifting by 1-2 pixels) for static clock UI. Added dynamic Web UI logic to forcibly enable this protection if Auto-Switch is disabled or the clock duration exceeds 60 seconds. | `clock_engine.cpp/h`, `config_manager.cpp/h`, `web_server.cpp`, `app.js`, `index.html` |
| 🐛 Bugfix | **Auto-Switch Freeze:** Fixed an issue where the global Auto-Switcher would permanently freeze if the device booted directly into the Clock face or if the user manually switched to it. Removed the `_autoSwitchedToClock` lock to guarantee infinite loop behavior, and ensured timers explicitly reset when saving Web UI settings. | `screen_manager.cpp/h`, `web_server.cpp` |
| 🐛 Bugfix | **Sub-screen Glitch:** Fixed an issue where the Clock's internal Time/Weather toggler paused while the Emotion face was active, causing the Clock to sometimes flash on screen for less than a second. Implemented `resetView()` to guarantee a fresh 5-second Time display every time the Clock engine activates. | `clock_engine.cpp/h`, `screen_manager.cpp` |
| ✨ Feature | **Open-Meteo Migration:** Completely replaced OpenWeatherMap API with Open-Meteo. This removes the need for API keys (and their 2-hour activation delay). Added "Auto-Detect Location" via IP and a built-in City Search via Open-Meteo's geocoding API to the WebUI. Switched ESP32 backend to use standard HTTP instead of HTTPS to save memory. `ConfigManager` now natively stores `weatherLat` and `weatherLon`. | `weather_service.cpp/h`, `config_manager.cpp/h`, `api_config.cpp`, `app.js`, `index.html` |
| 🐛 Bugfix | **Weather City Name Truncation:** City names saved with region suffixes (e.g. "Guwahati, Assam, In") were too long for the 128px OLED. Added server-side truncation at the first comma in `weather_service.cpp` and client-side fix in `app.js` to only save the primary city name. | `weather_service.cpp`, `app.js` |
| 🐛 Bugfix | **Weather Condition Text:** Open-Meteo doesn't provide textual descriptions, so the OLED was showing "Unknown" for all conditions. Added a WMO code-to-text mapping (Clear sky, Cloudy, Rain, Drizzle, Snow, Foggy, Thunderstorm, etc.) directly in the parse response. | `weather_service.cpp` |
| ✨ Feature | **Personalized Boot Greeting:** Added a `userName` field (max 15 chars) to NVS config, collected during first-time WiFi setup and editable in Settings. On every cold boot, a ~4s animated greeting plays: a happy face with a procedural waving hand, a time-aware message ("Good Morning", "Good Evening", etc.), and a random fun sub-line. If no name is set, MiniMello introduces itself and prompts the user to set a name at `minimello.local`. Skipped on deep-sleep wakes to avoid annoyance. | `greeting.h/cpp`, `config_manager.h/cpp`, `api_config.cpp`, `main.cpp`, `index.html`, `app.js` |
| 🚀 Build | **Project Modularization:** Completely refactored the monolithic `src/` directory into a domain-driven folder structure (e.g., `clock/`, `emotion/`, `network/`, `server/`, `ui/`). | `src/*` |
| 🚀 Build | **Shared UI Components:** Created `ui_components.h/cpp` to house common, reusable drawing primitives (e.g., `drawProgressBar`) to eliminate duplicate code across `main.cpp` and `boot_animation.cpp`. | `src/ui/ui_components.*`, `main.cpp`, `boot_animation.cpp` |
| 🎨 UI/UX | **Animated Boot Splash:** Replaced static boot text with a dynamic sub-pixel canvas scaling engine. Uses staggered 2D spring physics to individually pop each letter of the logo into place during boot without freezing the device. Extracted logic entirely to `src/ui/`. | `main.cpp`, `ui/boot_animation.cpp`, `ui/boot_animation.h` |
| 🐛 Bugfix | **Sub-screen Transition Race Condition:** Fixed a visual glitch where the Clock Engine would toggle to the Weather sub-screen a fraction of a second before the global Screen Manager auto-switched to the Emotion Engine. Exposed `getRemainingTime()` from the Screen Manager and suppressed internal clock transitions if a global switch is imminent (< 1.5s). | `clock_engine.cpp`, `screen_manager.h/cpp` |
| ✨ Feature | **Hard Restart Warning:** Added a visual full-screen "Hold to Restart!" warning that appears if the touch sensor is held for >3.0 seconds (well past the normal long-press threshold). Includes a live progress bar showing the remaining time until the 5-second hard reboot triggers. | `touch_manager.h/cpp`, `main.cpp` |
| ✨ Feature | **First-Time Setup Flow:** Implemented a blocking setup screen that prevents the main OS (ScreenManager) from launching if the device is unconfigured (AP Mode). The UI pauses during the boot animation (leaving the MiniMello logo visible) and slowly toggles connection instructions (`Setup Required / Connect to WiFi` -> `Minimello-xxxx / 192.168.4.1`) on the bottom half of the screen. Automatically resumes normal boot once the user configures WiFi via the Captive Portal. Also supports the full-screen "Hold to Restart!" warning sequence natively during setup. Supresses the normal loading progress bar if the device is entering setup mode. | `boot_animation.h/cpp`, `main.cpp` |
| 🐛 Bugfix | **AP Mode Instability & Captive Portal Drops:** Fixed an issue where phones would automatically disconnect from the setup AP or fail to see it. Resolved by ensuring `WIFI_AP` mode is explicitly used when no credentials exist (preventing radio instability from `WIFI_AP_STA`) and by actively polling `_dns->processNextRequest()` in the main loop so the captive portal responds to the OS internet checks. | `network_manager.cpp`, `web_server.h/cpp`, `main.cpp` |
| 🐛 Bugfix | **AP Mode Visibility & DHCP Drops:** Fixed two critical AP mode issues on ESP32-C3. First, removed a `WiFi.setSleep(false)` call during AP startup which broke the internal DHCP server (causing clients to hang at "Obtaining IP address"). Second, added a `WIFI_AP_PASSWORD` ("minimello") to the AP configuration since modern Android and iOS devices aggressively hide or deprioritize open (passwordless) networks. Added a third toggle state to the Setup UI to explicitly display this password to the user. Finally, added a hard radio reset (`WiFi.mode(WIFI_OFF)` + delay) during AP initialization to clear stale PHY state after soft resets (e.g. from esptool uploads) so the AP beacons broadcast reliably without requiring a hard power cycle. | `network_manager.cpp`, `config.h`, `boot_animation.cpp` |
| 🚀 Build | **Removed LittleFS Dependency:** Since the WebUI was migrated to a gzipped PROGMEM C-array (`web_ui_data.h`), the `LittleFS` filesystem was no longer being used. Removed `LittleFS.begin(true)` from the `MiniWebServer`, which fixed a known issue where `pio run -t erase` caused the ESP32 to crash via a Watchdog Timeout (WDT) while attempting to auto-format the blank partition. Also saved ~40KB of flash space! | `web_server.h/cpp` |
| 🎨 UI/UX | **AP Setup Polish:** Implemented a sleek linear scanning progress bar animation in the WebUI. Fixed a bug where WiFi scans would fail in pure AP mode by temporarily enabling `WIFI_AP_STA` during the scan cycle. Added logic to automatically trigger a WiFi scan when the WebUI loads in setup (AP) mode. | `index.html`, `style.css`, `app.js`, `api_system.cpp` |
| 🎨 UI/UX | **Greeting Hand Animation Redesign:** Upgraded the procedural waving hand during the boot greeting to a puffy, organic "Mickey Mouse" style glove with smooth pill-shaped fingers and a natural U-shaped wiping motion, replacing the rigid wrist-pivot stick figure. | `greeting.cpp` |
| 🐛 Bugfix | **Boot Progress Jitter:** Fixed a visual bug in `main.cpp` where the boot progress bar prematurely jumped to 100% ("Ready!") in the middle of initialization sequence, causing a jittery loading animation. | `main.cpp` |
| 🎨 UI/UX | **First-Time Setup Loading UI:** Fixed a bug where the loading progress bar was completely hidden during the first boot. Added dynamic contextual text ("Starting hotspot...", "Preparing setup...") while preparing the AP setup mode to provide clear user feedback between the splash screen and the captive portal instructions. | `main.cpp`, `boot_animation.cpp` |
| 🎨 UI/UX | **Contextual Web UI Elements:** Hid the "Your Nickname" input and the setup completion warning note from the WiFi tab when the device is operating in normal Station mode, cleaning up the UI for simple network switching. | `app.js`, `index.html` |
| 🚀 Build | **Unified UI Components:** Extracted the duplicated "Hold to Restart!" UI rendering code from `main.cpp` and `boot_animation.cpp` into a shared `UIComponents::drawRestartWarning()` helper function to enforce a consistent single-line layout and DRY the codebase. | `ui_components.h/cpp`, `main.cpp`, `boot_animation.cpp` |
| ✨ Feature | **Out-of-the-Box Weather:** Added logic to `main.cpp` to automatically geolocate the user via `ip-api.com` on the very first WiFi connection if no weather city is configured. This allows the weather dashboard to work instantly without requiring the user to manually trigger auto-detect in the Web UI. | `main.cpp` |

### v0.1.8
| Type | Description | Files Affected |
| :--- | :--- | :--- |
| ⚙️ Config | Changed OTA checking to point to the public `minimello-portable-releases` repository. This securely eliminates the need for hardcoded GitHub auth tokens. | `config.h` |
| 🎨 UI/UX | **WebUI Redesign:** Replaced harsh native browser `alert`s and `confirm`s with smooth, glassmorphic toast notifications and modal dialogues. | `index.html`, `style.css`, `app.js` |
| 🎨 UI/UX | **Typography:** Applied the "Lilita One" Google font to the WebUI brand logo. | `style.css` |
| 🐛 Bugfix | **Mobile Navigation:** Fixed horizontal tab menu squishing on mobile devices. Changed `.tab` CSS to `flex: 1 0 auto` to prevent text clipping and enable smooth horizontal swiping. | `style.css` |
| 🐛 Bugfix | **Ghost Sessions:** Fixed router "ghost session" connection timeouts (`Status: 6`) by forcing `WiFi.disconnect(true, true)` just prior to any MCU reset. | `main.cpp`, `ota_manager.cpp`, `web_server.cpp` |
| ✨ Feature | **Auto OTA Notifications:** Exposes `has_update` state in `/api/status`. The WebUI now polls this and automatically displays a toast notification and unlocks the "Install Update" button when a release is available. | `web_server.cpp`, `app.js` |
| ✨ Feature | **Wi-Fi Resilience:** Added a 60s background Wi-Fi auto-reconnect worker in AP mode. This gracefully recovers connections when the router reboots without requiring user intervention. | `network_manager.cpp` |

### v0.1.7
| Type | Description | Files Affected |
| :--- | :--- | :--- |
| 🐛 Bugfix | **Weather API Lockout:** Added a 60s retry backoff for OpenWeatherMap API fetch failures (`HTTP -11`) to prevent the device from waiting a full 30 minutes before retrying. | `weather_service.cpp` |

### v0.1.6
| Type | Description | Files Affected |
| :--- | :--- | :--- |
| ✨ Feature | **Touch Restarts:** Added a 5s `VERY_LONG_PRESS` touch event threshold to trigger a hard restart, circumventing the TTP223 auto-calibration hardware lockup. | `touch_manager.cpp`, `touch_manager.h`, `main.cpp` |

### v0.1.5
| Type | Description | Files Affected |
| :--- | :--- | :--- |
| 🚀 Build | **USB Logging:** Enabled `ARDUINO_USB_CDC_ON_BOOT=1` and `ARDUINO_USB_MODE=1` in `platformio.ini` to direct serial output over the native USB JTAG port. Added a 2s boot delay in `main.cpp` to prevent initial log truncation. | `platformio.ini`, `main.cpp` |

### v0.1.0
| Type | Description | Files Affected |
| :--- | :--- | :--- |
| 🚀 Build | **PROGMEM WebUI:** Implemented python pre-build script to package HTML/CSS/JS. Removed dependency on LittleFS flash partition hosting. | `scripts/build_webui.py`, `platformio.ini` |

---

## 3. Known Hardware Quirks & Limitations

1. **TTP223 Auto-Calibration Lockout**
   - The TTP223 capacitive touch sensor automatically recalibrates to the environment if held continuously for ~8 seconds. If this happens, it stops registering touches entirely.
   - **Workaround:** We implemented `VERY_LONG_PRESS` (5 seconds) to trigger an MCU restart *before* the hardware recalibrates.

2. **ESP32-C3 JTAG Serial Timing**
   - Because the C3 uses native USB CDC for serial rather than an external UART chip (like CP2102), the host OS takes a moment to enumerate the serial port after a reboot.
   - **Workaround:** `delay(2000)` in `setup()` ensures the port is open before the boot logs are transmitted.

3. **OLED Burn-in Protection**
   - Currently handled passively: the device auto-switches between the static Clock and the dynamic Emotion Face every 30 seconds. Additionally, "Night Mode" completely turns the screen off during configured hours. 
   - *(Note: If a user leaves the digital clock on 24/7 with auto-switch disabled, burn-in of static elements is possible long-term).*
