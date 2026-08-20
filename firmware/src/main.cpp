// SurSense firmware
//
// Core 0 captures audio on I2S0, runs the FFT and feeds sur::Detector.
// Core 1 renders the round display and reads the encoder and buttons.
// The two talk through one FreeRTOS queue holding the latest Reading.

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <driver/i2s.h>
#include <math.h>

#include "pins.h"
#include "sur.h"

// ------------------------------------------------------------------ audio

constexpr int kSampleRate = 22050;   // enough for the top of the range we chase
constexpr int kFFT = 1024;           // 21.5 Hz per bin
constexpr float kBinHz = (float)kSampleRate / kFFT;

static float g_re[kFFT];
static float g_im[kFFT];
static float g_mag[kFFT / 2];
static float g_window[kFFT];
static int32_t g_raw[kFFT];

static sur::Detector g_detector;
static QueueHandle_t g_readings;

// Radix-2 FFT, in place. Replace with esp-dsp's dsps_fft2r_fc32 if you want
// the assembly-optimised version; this is here so the build has no extra deps.
static void fft(float* re, float* im, int n) {
  for (int i = 1, j = 0; i < n; ++i) {
    int bit = n >> 1;
    for (; j & bit; bit >>= 1) j ^= bit;
    j ^= bit;
    if (i < j) {
      float t = re[i]; re[i] = re[j]; re[j] = t;
      t = im[i]; im[i] = im[j]; im[j] = t;
    }
  }
  for (int len = 2; len <= n; len <<= 1) {
    const float ang = -6.283185307f / len;
    for (int i = 0; i < n; i += len) {
      float cr = 1.0f, ci = 0.0f;
      for (int k = 0; k < len / 2; ++k) {
        const int a = i + k, b = a + len / 2;
        const float tr = re[b] * cr - im[b] * ci;
        const float ti = re[b] * ci + im[b] * cr;
        re[b] = re[a] - tr;  im[b] = im[a] - ti;
        re[a] += tr;         im[a] += ti;
        const float ncr = cr * cosf(ang) - ci * sinf(ang);
        ci = cr * sinf(ang) + ci * cosf(ang);
        cr = ncr;
      }
    }
  }
}

static void mic_begin() {
  i2s_config_t cfg = {};
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
  cfg.sample_rate = kSampleRate;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  cfg.dma_buf_count = 4;
  cfg.dma_buf_len = 256;
  cfg.use_apll = false;

  i2s_pin_config_t pins = {};
  pins.bck_io_num = PIN_MIC_SCK;
  pins.ws_io_num = PIN_MIC_WS;
  pins.data_in_num = PIN_MIC_SD;
  pins.data_out_num = I2S_PIN_NO_CHANGE;

  i2s_driver_install(I2S_NUM_0, &cfg, 0, nullptr);
  i2s_set_pin(I2S_NUM_0, &pins);
}

static void audio_task(void*) {
  for (int i = 0; i < kFFT; ++i) {
    g_window[i] = 0.5f * (1.0f - cosf(6.283185307f * i / (kFFT - 1)));
  }

  uint32_t last = millis();

  for (;;) {
    size_t got = 0;
    i2s_read(I2S_NUM_0, g_raw, sizeof(g_raw), &got, portMAX_DELAY);
    const int n = got / sizeof(int32_t);
    if (n < kFFT) continue;

    // INMP441 gives 24 bits left justified in a 32 bit slot.
    float rms = 0.0f;
    for (int i = 0; i < kFFT; ++i) {
      const float s = (float)(g_raw[i] >> 8) / 8388608.0f;
      rms += s * s;
      g_re[i] = s * g_window[i];
      g_im[i] = 0.0f;
    }
    rms = sqrtf(rms / kFFT);

    fft(g_re, g_im, kFFT);
    for (int i = 0; i < kFFT / 2; ++i) {
      g_mag[i] = sqrtf(g_re[i] * g_re[i] + g_im[i] * g_im[i]);
    }

    const uint32_t now = millis();
    const float dt = (now - last) / 1000.0f;
    last = now;

    sur::Reading r = g_detector.update(g_mag, kFFT / 2, kBinHz, dt, rms);
    xQueueOverwrite(g_readings, &r);
  }
}

// ---------------------------------------------------------------- display

static Arduino_DataBus* bus = new Arduino_ESP32SPI(
    PIN_TFT_DC, PIN_TFT_CS, PIN_TFT_SCL, PIN_TFT_SDA, GFX_NOT_DEFINED);
static Arduino_GFX* gfx = new Arduino_GC9A01(bus, PIN_TFT_RST, 0, true);

static void draw(const sur::Reading& r) {
  gfx->fillScreen(BLACK);

  gfx->setTextColor(r.locked ? GREEN : DARKGREY);
  gfx->setTextSize(2);
  gfx->setCursor(84, 40);
  gfx->print(r.locked ? "LOCKED" : "LISTEN");

  if (r.tonic < 0) return;

  gfx->setTextColor(WHITE);
  gfx->setTextSize(6);
  gfx->setCursor(80, 90);
  gfx->print(sur::kNoteNames[r.tonic]);

  if (r.scale) {
    gfx->setTextSize(2);
    gfx->setTextColor(CYAN);
    gfx->setCursor(50, 155);
    gfx->print(r.scale->thaat);
    gfx->print(" . ");
    gfx->print(r.scale->western);
  }
}

// -------------------------------------------------------------------- app

void setup() {
  Serial.begin(115200);

  pinMode(PIN_AMP_SD, OUTPUT);
  digitalWrite(PIN_AMP_SD, LOW);   // amp muted while we listen
  pinMode(PIN_MOTOR, OUTPUT);
  digitalWrite(PIN_MOTOR, LOW);
  pinMode(PIN_BTN_MODE, INPUT_PULLUP);
  pinMode(PIN_BTN_TAP, INPUT_PULLUP);
  pinMode(PIN_ENC_A, INPUT_PULLUP);
  pinMode(PIN_ENC_B, INPUT_PULLUP);
  pinMode(PIN_ENC_SW, INPUT_PULLUP);

  pinMode(PIN_TFT_BLK, OUTPUT);
  digitalWrite(PIN_TFT_BLK, HIGH);
  gfx->begin();
  gfx->fillScreen(BLACK);

  g_readings = xQueueCreate(1, sizeof(sur::Reading));
  mic_begin();
  g_detector.reset();

  // audio is hard real time, pin it to core 0 and leave core 1 for the UI
  xTaskCreatePinnedToCore(audio_task, "audio", 8192, nullptr, 5, nullptr, 0);
}

void loop() {
  sur::Reading r;
  if (xQueueReceive(g_readings, &r, pdMS_TO_TICKS(100)) == pdTRUE) {
    draw(r);
  }
}
