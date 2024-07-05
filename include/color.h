#pragma once

#include <Arduino.h>

float fract(float x);
float mix(float a, float b, float t);
float step(float e, float x);
// float* hsv2rgb(float h, float s, float b, float* rgb);
// float* rgb2hsv(float r, float g, float b, float* hsv);
void shiftByHsv(byte *rgbData, byte hueShift, byte saturationShift);