// Proves the detector on audio whose answer is known: twelve synthesised
// scales, one per tonic, run through the real pipeline.

#include<cmath>
#include <cstdio>
#include<vector>
#include "sur.h"

using namespace sur;
using namespace sur;

namespace {

constexpr int kN = 2048;
constexpr float kSR = 44100.0f;

void fft(std::vector<float>& re, std::vector<float>& im) {
  const int n = static_cast<int>(re.size());
  for (int i = 1, j = 0; i < n; ++i) {
    int bit = n >> 1;
    for (; j & bit; bit >>= 1) j ^= bit;
    j ^= bit;
    if (i < j) { std::swap(re[i], re[j]); std::swap(im[i], im[j]); }
  }
  for (int len = 2; len <= n; len <<= 1) {
    const float ang = -6.283185307f / len;
    const float wr = std::cos(ang), wi = std::sin(ang);
    for (int i = 0; i < n; i += len) {
      float cr = 1.0f, ci = 0.0f;
      for (int k = 0; k < len / 2; ++k) {
        const int a = i + k, b = a + len / 2;
        const float xr = re[b] * cr - im[b] * ci;
        const float xi = re[b] * ci + im[b] * cr;
        re[b] = re[a] - xr; im[b] = im[a] - xi;
        re[a] += xr;        im[a] += xi;
        const float t = cr * wr - ci * wi;
        ci = cr * wi + ci * wr;
        cr = t;
      }
    }
}
}

void add_note(std::vector<float>& out, float midi, float secs) {
  const float f0 = 440.0f * std::pow(2.0f, (midi - 69.0f) / 12.0f);
  const float amp[5] = {1.0f, 0.5f, 0.32f, 0.2f, 0.12f};
  for (int i = 0; i < static_cast<int>(secs * kSR); ++i) {
    const float t = i / kSR;
    const float env = std::exp(-2.2f * t) * (1.0f - std::exp(-90.0f * t));
    float s = 0.0f;
    // Harmonics, not a pure sine — harmonic bleed is the whole difficulty.
    for (int h = 0; h < 5; ++h) s += amp[h] * std::sin(6.283185307f * f0 * (h + 1) * t);
    out.push_back(0.22f * env * s);
  }
}