// The SAI's DMA interrupt asks for the next block of samples: interleaved L/R 32-bit words, MSB-first.
#pragma once
#include <stdint.h>

void audio_feed(uint32_t *dst, uint32_t frames);
