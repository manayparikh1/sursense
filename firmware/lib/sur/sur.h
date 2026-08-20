#pragma once

namespace sur {

constexpr int kPitchClasses = 12;

extern const char* const kNoteNames[kPitchClasses];
extern const char* const kSwara[kPitchClasses];

struct Scale {
  const char* western;
  const char* thaat;
  int degrees[7];
};

extern const Scale kScales[];
extern const int kScaleCount;

struct Reading {
  int tonic = -1;
  const Scale* scale = nullptr;
  float confidence = 0.0f;
  bool locked = false;
};

// Works out which scale is being played, from a magnitude spectrum.
//
// It deliberately does not own an FFT. The caller supplies the spectrum, so the
// same code runs against esp-dsp on the ESP32-S3, any FFT on desktop, or the
// browser's AnalyserNode, with nothing platform-specific inside.
class Detector {
 public:
  void reset();

  // One analysis frame. `rms` gates quiet frames so noise cannot steer the
  // result. Returns the reading to display, which stays put once locked.
  Reading update(const float* mags, int bins, float bin_hz, float dt,
                 float rms);

  float a4 = 440.0f;

 private:
  void estimate();

  float fast_[kPitchClasses] = {};   // ~8s memory, drives the correlation
  float slow_[kPitchClasses] = {};   // ~30s memory, breaks relative maj/min ties
  float voiced_ = 0.0f;
  int history_[4] = {};
  int history_count_ = 0;
  int challenger_ = -1;
  int challenger_count_ = 0;
  int frames_ = 0;
  Reading current_;
};

}  // namespace sur
