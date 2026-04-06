// Last_Wish.h — ESP32 Last Wish
// Servo Position Preservation on Sudden Power Loss
// "When power dies, the ESP32 fulfils its last wish — saving its position before the final reset."
//
// GitHub: https://github.com/yourname/ESP32_Last_Wish
// License: MIT
//
// ⚠️  POWER-UP CAUTION:
//     1. Power the servo/motor FIRST, then power the ESP32 circuit.
//     2. NEVER power sensors or other devices from the ESP32 board or
//        this circuit — doing so will drain the backup capacitor and
//        cause the save to fail due to insufficient hold-up power.
//     3. For power-DOWN: cut ESP32 power first, then motor power.
//     4. Using a single main switch that cuts both simultaneously is
//        also safe and acceptable.

#pragma once
#include <Arduino.h>
#include <Preferences.h>

static Preferences _lw_prefs;
static TaskHandle_t _lw_saveHandle = NULL;
static volatile int* _lw_curPtr    = nullptr;
static int _lw_minUs = 0;
static int _lw_maxUs = 0;
static volatile bool _lw_saveDone  = false;

// ---------------------------------------------------------------
// ISR — fires in ~1µs on power-cut detection (FALLING edge)
// Only job: wake the save task via direct-to-task notification
// ---------------------------------------------------------------
void IRAM_ATTR _lw_powerCutISR() {
    if (_lw_saveDone) return;
    portDISABLE_INTERRUPTS();
    BaseType_t woken = pdFALSE;
    vTaskNotifyGiveFromISR(_lw_saveHandle, &woken);
    portYIELD_FROM_ISR(woken);
}

// ---------------------------------------------------------------
// Core 0 save task — priority 25
// Sleeps with zero CPU until ISR wakes it.
// Priority 25 automatically freezes loop() (1), WiFi (19), BT (22).
// ---------------------------------------------------------------
void _lw_saveTask(void* param) {
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (_lw_saveDone) continue;
        _lw_saveDone = true;
        int snapshot = *_lw_curPtr;
        _lw_prefs.putUInt("pwmVal", (uint32_t)snapshot);
        Serial.print("[LastWish SAVED]: ");
        Serial.println(snapshot);
        vTaskSuspend(NULL);
    }
}

// ---------------------------------------------------------------
// LastWish_begin()   — Call FIRST in setup()
//   pin    : GPIO connected to voltage-divider midpoint
//   cur    : volatile int holding live PWM value
//   minUs  : minimum valid PWM µs (e.g. 801)
//   maxUs  : maximum valid PWM µs (e.g. 1641)
// ---------------------------------------------------------------
void LastWish_begin(int pin, volatile int& cur, int minUs, int maxUs) {
    _lw_curPtr = &cur;
    _lw_minUs  = minUs;
    _lw_maxUs  = maxUs;
    _lw_prefs.begin("lw_servo", false);
    xTaskCreatePinnedToCore(
        _lw_saveTask, "LastWishTask",
        4096, NULL, 25,
        &_lw_saveHandle, 0
    );
    pinMode(pin, INPUT);   // external voltage divider handles safe voltage
    attachInterrupt(digitalPinToInterrupt(pin), _lw_powerCutISR, FALLING);
}

// ---------------------------------------------------------------
// LastWish_load()   — Call AFTER LastWish_begin() in setup()
//   defaultVal : returned on first boot or if stored value is invalid
// ---------------------------------------------------------------
int LastWish_load(int defaultVal) {
    int v = (int)_lw_prefs.getUInt("pwmVal", (uint32_t)defaultVal);
    return (v >= _lw_minUs && v <= _lw_maxUs) ? v : defaultVal;
}

// ---------------------------------------------------------------
// LastWish_clear()  — Erase saved value (factory reset / testing)
// ---------------------------------------------------------------
void LastWish_clear() {
    _lw_prefs.clear();
    Serial.println("[LastWish] Saved position cleared.");
}