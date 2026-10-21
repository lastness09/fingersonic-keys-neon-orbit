#pragma once
extern "C" {
float expf(float);
float powf(float, float);
float floorf(float);
inline float fabsf(float x) { return x < 0 ? -x : x; }
}
