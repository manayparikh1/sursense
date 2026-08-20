#pragma once

// SurSense pin map. One ESP32-S3-WROOM-1, 20 GPIO of 44.
// Strapping pins 0, 3, 45 and 46 are left clear.
// This file is the single source of truth; the README table mirrors it.

// INMP441 microphone, I2S peripheral 0. L/R is tied to GND for the left slot.
constexpr int PIN_MIC_SCK = 4;
constexpr int PIN_MIC_WS  = 5;
constexpr int PIN_MIC_SD  = 6;

// MAX98357A amplifier, I2S peripheral 1.
constexpr int PIN_AMP_DIN  = 7;
constexpr int PIN_AMP_BCLK = 15;
constexpr int PIN_AMP_LRC  = 16;
constexpr int PIN_AMP_SD   = 38;  // shutdown, driven low to mute while detecting

// GC9A01 round display, SPI.
constexpr int PIN_TFT_RST = 8;
constexpr int PIN_TFT_DC  = 9;
constexpr int PIN_TFT_CS  = 10;
constexpr int PIN_TFT_SDA = 11;
constexpr int PIN_TFT_SCL = 12;
constexpr int PIN_TFT_BLK = 14;  // backlight, PWM capable

// EC11 encoder, driven by the knurled bezel.
constexpr int PIN_ENC_A  = 17;
constexpr int PIN_ENC_B  = 18;
constexpr int PIN_ENC_SW = 21;

// Buttons and haptics.
constexpr int PIN_BTN_MODE = 13;
constexpr int PIN_BTN_TAP  = 2;
constexpr int PIN_MOTOR    = 41;  // coin motor through an N-channel MOSFET

// Battery sense. Divider is 2x 100k, so the ADC reads half of VBAT.
constexpr int PIN_VBAT = 1;
constexpr float VBAT_DIVIDER = 2.0f;
