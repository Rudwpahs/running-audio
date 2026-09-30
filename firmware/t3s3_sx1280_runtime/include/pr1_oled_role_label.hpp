#pragma once

// Boot-only role label on the SSD1306 OLED so boards can be told apart by eye.
// Runs once in setup() before the radio starts; nothing touches I2C afterwards,
// so the RX/TX runtime timing path is unchanged.

#include <Arduino.h>
#include <Wire.h>

#include <array>
#include <cstdint>
#include <cstring>

#include "pr1_board_config.hpp"

#ifndef PR1_OLED_ROLE_LABEL
#define PR1_OLED_ROLE_LABEL 1
#endif

namespace pr1::runtime::oled {

constexpr int kWidth = 128;
constexpr int kHeight = 64;
constexpr std::uint8_t kAddresses[] = {0x3C, 0x3D};

// Classic 5x7 glyphs, column-major, bit 0 = top row. Only the letters needed.
struct Glyph {
  char ch;
  std::uint8_t cols[5];
};

constexpr Glyph kGlyphs[] = {
    {'A', {0x7E, 0x11, 0x11, 0x11, 0x7E}}, {'E', {0x7F, 0x49, 0x49, 0x49, 0x41}},
    {'F', {0x7F, 0x09, 0x09, 0x09, 0x01}}, {'R', {0x7F, 0x09, 0x19, 0x29, 0x46}},
    {'S', {0x46, 0x49, 0x49, 0x49, 0x31}}, {'T', {0x01, 0x01, 0x7F, 0x01, 0x01}},
    {'X', {0x63, 0x14, 0x08, 0x14, 0x63}},
};

inline const Glyph* findGlyph(char ch) {
  for (const auto& glyph : kGlyphs) {
    if (glyph.ch == ch) return &glyph;
  }
  return nullptr;
}

inline void command(std::uint8_t address, std::uint8_t value) {
  Wire.beginTransmission(address);
  Wire.write(0x00);
  Wire.write(value);
  Wire.endTransmission();
}

// Draws `text` centred at the largest integer scale that fits the panel.
inline void render(const char* text, std::array<std::uint8_t, kWidth * kHeight / 8>& fb) {
  fb.fill(0);
  const int len = static_cast<int>(std::strlen(text));
  if (len == 0) return;
  int scale = 1;
  while ((len * 6 - 1) * (scale + 1) <= kWidth && 7 * (scale + 1) <= kHeight) ++scale;
  const int x0 = (kWidth - (len * 6 - 1) * scale) / 2;
  const int y0 = (kHeight - 7 * scale) / 2;
  for (int i = 0; i < len; ++i) {
    const Glyph* glyph = findGlyph(text[i]);
    if (glyph == nullptr) continue;
    for (int col = 0; col < 5; ++col) {
      for (int row = 0; row < 7; ++row) {
        if ((glyph->cols[col] >> row & 1U) == 0U) continue;
        for (int dx = 0; dx < scale; ++dx) {
          for (int dy = 0; dy < scale; ++dy) {
            const int x = x0 + (i * 6 + col) * scale + dx;
            const int y = y0 + row * scale + dy;
            fb[x + (y / 8) * kWidth] |= static_cast<std::uint8_t>(1U << (y & 7));
          }
        }
      }
    }
  }
}

// Returns the I2C address used, or 0 when no OLED answered (label skipped).
inline std::uint8_t showRoleLabel(const char* text) {
  Wire.begin(board::kOledPins.sda, board::kOledPins.scl, 400000U);
  std::uint8_t address = 0;
  for (const std::uint8_t candidate : kAddresses) {
    Wire.beginTransmission(candidate);
    if (Wire.endTransmission() == 0) {
      address = candidate;
      break;
    }
  }
  if (address == 0) {
    Wire.end();
    return 0;
  }

  static constexpr std::uint8_t kInit[] = {
      0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40, 0x8D, 0x14, 0x20, 0x00,
      0xA1, 0xC8, 0xDA, 0x12, 0x81, 0xCF, 0xD9, 0xF1, 0xDB, 0x40, 0xA4, 0xA6,
      0x21, 0x00, 0x7F, 0x22, 0x00, 0x07, 0xAF,
  };
  for (const std::uint8_t value : kInit) command(address, value);

  static std::array<std::uint8_t, kWidth * kHeight / 8> fb{};
  render(text, fb);
  constexpr std::size_t kChunk = 16;
  for (std::size_t offset = 0; offset < fb.size(); offset += kChunk) {
    Wire.beginTransmission(address);
    Wire.write(0x40);
    Wire.write(fb.data() + offset, kChunk);
    Wire.endTransmission();
  }
  Wire.end();
  return address;
}

}  // namespace pr1::runtime::oled
