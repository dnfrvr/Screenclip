#pragma once

#include <Arduino.h>

#include "reglages.h"

// Lit le bouton BOOT, partagé avec une broche de l'écran, dans une tâche à part.
namespace bouton {
struct Event {
  bool isLong;
  uint32_t at;
};
inline QueueHandle_t events = nullptr;
inline volatile bool held = false;

// Un appui long agit dès que le délai est atteint, sans attendre le relâchement.
inline void task(void *) {
  bool pressed = false, longSent = false;
  uint32_t changedAt = 0, pressedAt = 0;
  for (;;) {
    const bool down = digitalRead(PIN_LCD_CS) == LOW;
    const uint32_t now = millis();
    if (down != pressed && now - changedAt >= 25) {
      pressed = down;
      changedAt = now;
      held = down;
      if (down) {
        pressedAt = now;
        longSent = false;
      } else if (!longSent) {
        const Event event = {false, now};
        xQueueSend(events, &event, 0);
      }
    } else if (pressed && !longSent && now - pressedAt >= LONG_PRESS_MS) {
      longSent = true;
      const Event event = {true, now};
      xQueueSend(events, &event, 0);
    }
    vTaskDelay(pdMS_TO_TICKS(4));
  }
}

inline void start() {
  pinMode(PIN_LCD_CS, INPUT_PULLUP);
  events = xQueueCreate(8, sizeof(Event));
  xTaskCreatePinnedToCore(task, "bouton", 2048, nullptr, 3, nullptr, 0);
}
}
