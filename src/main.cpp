// main.cpp — ESP32 Last Wish demo
// Smoothly sweeps a servo 0–180° and saves its position on power loss.
//
// ⚠️  POWER-UP ORDER:
//   1. Apply power to the servo/motor FIRST.
//   2. Then apply power to the ESP32 circuit.
//   Never power other devices from this circuit — it drains the backup capacitor.
//
// ⚠️  POWER-DOWN ORDER:
//   1. Cut ESP32 power first.
//   2. Then cut motor/servo power.

//             OR

//use a single main switch to cut both simultaneously.

#include <Arduino.h>
#include "Last_Wish.h"

// ── Pin configuration ─────────────────────────────────────────
const int servoPin = 14;   // PWM output — must be output-capable GPIO
const int savePin  = 33;   // Power sense — voltage divider midpoint

// ── LEDC (PWM) configuration ──────────────────────────────────
const int ch = 0;           // LEDC channel
const int f  = 50;          // 50 Hz servo frequency
const int r  = 16;          // 16-bit resolution

// ── Servo PWM range (µs) ──────────────────────────────────────
const int min_us = 801;     // ≈ 0°
const int max_us = 1641;    // ≈ 180°
const int step   = 3;       // Smooth sweep increment

// ── Live position (volatile — ISR reads this) ─────────────────
volatile int cur = min_us;
int tgt = max_us;

// ── Helper: microseconds → 16-bit LEDC duty ───────────────────
uint32_t us_to_duty(int us) {
    return (uint32_t)((us / 20000.0) * ((1 << r) - 1));
}

void setup() {
    Serial.begin(115200);

    // Configure LEDC PWM
    ledcSetup(ch, f, r);
    ledcAttachPin(servoPin, ch);

    // 1️⃣  Init Last Wish system FIRST
    LastWish_begin(savePin, cur, min_us, max_us);

    // 2️⃣  Load last saved position
    cur = LastWish_load(min_us);
    tgt = max_us;

    ledcWrite(ch, us_to_duty(cur));
    Serial.print("[LastWish] Loaded position: ");
    Serial.println(cur);
}

void loop() {
    // Smooth sweep toward target
    if (abs(cur - tgt) <= step) cur = tgt;
    else if (cur < tgt)         cur += step;
    else                        cur -= step;

    ledcWrite(ch, us_to_duty(cur));

    // Reverse direction at limits
    if (cur == max_us) tgt = min_us;
    if (cur == min_us) tgt = max_us;

    delay(30);  // ISR fires instantly regardless of delay()
}