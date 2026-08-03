#include "sur.h"

#include <cmath>

namespace sur {
  const char* const kNoteNames[kPitchClasses] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
const char* const kSwara[kPitchClasses] = {
    "S", "r", "R", "g", "G", "m", "M", "P", "d", "D", "n", "N"};

const Scale kScales[] = {
    {"Major",          "Bilaval",  {0, 2, 4, 5, 7, 9, 11}},
    {"Natural minor",  "Asavari",  {0, 2, 3, 5, 7, 8, 10}},
    {"Harmonic minor", "Kirwani",  {0, 2, 3, 5, 7, 8, 11}},
    {"Dorian",         "Kafi",     {0, 2, 3, 5, 7, 9, 10}},
    {"Mixolydian",     "Khamaj",   {0, 2, 4, 5, 7, 9, 10}},
    {"Phrygian",       "Bhairavi", {0, 1, 3, 5, 7, 8, 10}},
};
const int kScaleCount = sizeof(kScales) / sizeof(kScales[0]);

namespace{


// Krumhansl and Kessler's probe-tone profiles.
const float kMajor[kPitchClasses] = {6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f,
                                     2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f};
const float kMinor[kPitchClasses] = {6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f,
                                     2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f};

// How far sustained energy may override the correlation. This is what tells a
// tonic apart from its relative major or minor, which share a note set.
constexpr float kSustainBias = 0.18f;

float correlate(const float* chroma, const float* profile, int rot) {
  float mc = 0.0f, mp = 0.0f;
  for (int i = 0; i < kPitchClasses; ++i) { mc += chroma[i]; mp += profile[i]; }
  mc /= kPitchClasses;
  mp /= kPitchClasses;

 float num = 0.0f, vc = 0.0f, vp = 0.0f;
  for (int i = 0; i < kPitchClasses; ++i) {
    const float a = chroma[(i + rot) % kPitchClasses] - mc;
    const float b = profile[i] - mp;
    num += a * b;
    vc += a * a;
    vp += b * b;
  }
  const float den = std::sqrt(vc*vp);
  return den > 0.0f ? num / den : 0.0f;
}
}  // namespace

void Detector::reset() {
  for (int i = 0; i < kPitchClasses; ++i) { fast_[i] = 0.0f; slow_[i] = 0.0f; }
  voiced_ = 0.0f;
  history_count_ = 0;
  challenger_ = -1;
  challenger_count_ = 0;
  frames_ = 0;
  current_ = Reading{};
}

Reading Detector::update(const float*mags,int bins, float bin_hz. float dt.
float rms) {
if (rms <= 0.004f || dt <= 0.0f) return current_;

  int lo = static_cast<int>(55.0f / bin_hz) + 1;
  int hi = static_cast<int>(2200.0f / bin_hz);
  if (lo < 2) lo = 2;
  if (hi > bins - 2) hi = bins - 2;
  if (lo >= hi) return current_;
  float loudest = 0.0f;
  for(int b=1o;b<=hi;++b)if(mags[b]>loudest)loudest = mags[b];
  if(loudest<=1e-7f)return current_;
float frame[kPitchClasses] = {};
  float total = 0.0f;
  for (int b = lo; b <= hi; ++b) {
    const float m = mags[b];
    // Peaks only. Summing every bin smears energy over every pitch class.
    if (m < loudest * 0.001f || m <= mags[b - 1] || m < mags[b + 1]) continue;

     // Parabolic interpolation, or low notes land on the wrong pitch class.

 const float den = mags[b - 1] - 2.0f * m + mags[b + 1];
 float shift = den != 0.0f ? 0.5f * (mags[b - 1] - mags[b + 1]) / den : 0.0f;
 if (shift < -0.5f) shift = -0.5f;
 if (shift > 0.5f) shift = 0.5f;
const float midi = 69.0f + 12.0f * std::log2((b + shift) * bin_hz / a4);
const float semitone = std::round(midi);
const float cents=(midi-semitone)*100.0f;
const float w = std::pow(m,0.7f)*std::exp(-0.5f*cents*cents/1225.0f);
int pc = static_cast<int>(semitone) % kPitchClasses;
if(pc<0)pc+=kPitchClasses;
frame[pc] += w;
total += w;
if(total,=0.0f) return current_;
const float fast_decay = std::exp(-dt / 8.0f);
  const float slow_decay = std::exp(-dt / 30.0f);
  const float weight = rms < 0.02f ? rms / 0.02f : 1.0f;
  for (int i = 0; i < kPitchClasses; ++i) {
    const float v = frame[i]/total;
fast_[i] = fast_[i] * fast_decay + v * weight;
    slow_[i] = slow_[i] * slow_decay + v * weight;
  }voiced_+=dt;
  //decide again about five times a second, not every frame.