#pragma once

#include <Arduino.h>
#include <Wire.h>

#include "reglages.h"

// Accéléromètre QMI8658, sur le bus I2C partagé avec la RTC (schéma Waveshare).
// Sa sortie INT1 n'est pas reliée (R14 non montée) : il faut l'interroger.
namespace capteur {
constexpr uint8_t kRegWhoAmI = 0x00, kRegCtrl1 = 0x02, kRegCtrl2 = 0x03, kRegCtrl7 = 0x08;
constexpr uint8_t kRegAccelX = 0x35, kRegReset = 0x60;
constexpr uint8_t kWhoAmI = 0x05;
constexpr float kLsbPerG = 8192.0f;

inline bool present = false;
inline uint8_t address = 0;

inline bool writeReg(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

inline bool readRegs(uint8_t reg, uint8_t *out, size_t len) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(address, (uint8_t)len) != len) return false;
  for (size_t i = 0; i < len; ++i) out[i] = Wire.read();
  return true;
}

inline bool probe() {
  if (!Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 400000)) return false;
  Wire.setTimeOut(20);
  for (uint8_t candidate : {(uint8_t)0x6B, (uint8_t)0x6A}) {
    address = candidate;
    uint8_t id = 0;
    if (readRegs(kRegWhoAmI, &id, 1) && id == kWhoAmI) return true;
  }
  return false;
}

inline bool configure() {
  writeReg(kRegReset, 0xB0);
  delay(20);
  return writeReg(kRegCtrl1, 0x40)    // adresses auto-incrémentées, petit-boutiste
         && writeReg(kRegCtrl2, 0x17)  // +/- 4 g, 62,5 Hz
         && writeReg(kRegCtrl7, 0x01);  // accéléromètre seul, gyroscope éteint
}

inline bool begin() {
  present = probe() && configure();
  if (!present) Wire.end();
  return present;
}

inline bool read(float &ax, float &ay, float &az) {
  uint8_t raw[6];
  if (!present || !readRegs(kRegAccelX, raw, sizeof(raw))) return false;
  ax = (int16_t)(raw[0] | raw[1] << 8) / kLsbPerG;
  ay = (int16_t)(raw[2] | raw[3] << 8) / kLsbPerG;
  az = (int16_t)(raw[4] | raw[5] << 8) / kLsbPerG;
  return true;
}
}
