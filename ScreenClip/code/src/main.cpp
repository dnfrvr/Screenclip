#include <Arduino.h>
#include <AnimatedGIF.h>
#include <FS.h>
#include <Preferences.h>
#include <SD_MMC.h>
#include <driver/ledc.h>
#include <esp_heap_caps.h>
#include <esp_memory_utils.h>

#include <algorithm>
#include <vector>

#include "reglages.h"
#include "capteur.h"
#include "orientation.h"
#include "ordre.h"
#include "bouton.h"
#include "rendu.h"

#include "batterie.h"
#include "texte.h"
#include "cle_usb.h"
#include "menus.h"
#include "telephone.h"

namespace {
AnimatedGIF gif;
Preferences prefs;

std::vector<String> gifFiles;
int currentIndex = 0;

uint8_t *gifData = nullptr;
bool gifOpen = false;
uint32_t nextFrameAt = 0;
uint8_t *gifCanvas = nullptr;
uint8_t *gifTurbo = nullptr;

constexpr int kUiW = 820, kUiH = 320;
Canvas ui = {nullptr, kUiW, kUiH};

uint32_t overlayUntil = 0;
bool overlayShown = false;

void showBatteryOverlay(uint32_t durationMs) {
#if BATTERY_OVERLAY
  battery::build();
  overlayUntil = millis() + durationMs;
#endif
}

// Mode rotation : on voit le vrai rendu pendant qu'on tourne l'image.
bool rotateMode = false;
uint32_t rotateTouchedAt = 0;
uint16_t *hintPixels = nullptr;

void compositeHint(uint16_t *panel) {
  const int sh = rendu::screenH();
  for (int oy = 0; oy < menus::kHintH; ++oy) {
    for (int ox = 0; ox < menus::kHintW; ++ox) {
      const int sx = 12 + ox, sy = sh - menus::kHintH - 12 + oy;
      int px, py;
      switch (rendu::rotation & 3) {
        case 0: px = sx; py = sy; break;
        case 1: px = PANEL_W - 1 - sy; py = sx; break;
        case 2: px = PANEL_W - 1 - sx; py = PANEL_H - 1 - sy; break;
        default: px = sy; py = PANEL_H - 1 - sx; break;
      }
      if (px >= 0 && px < PANEL_W && py >= 0 && py < PANEL_H) panel[py * PANEL_W + px] = hintPixels[oy * menus::kHintW + ox];
    }
  }
}

void compositeOverlay(uint16_t *panel) {
  if (rotateMode && hintPixels) compositeHint(panel);
  if ((int32_t)(overlayUntil - millis()) <= 0) return;
  battery::composite(panel, rendu::rotation);
  overlayShown = true;
}

bool autoRotate = AUTO_ROTATE_DEFAULT;
bool sleepEnabled = SLEEP_DEFAULT;
constexpr int kSlideSeconds[] = {0, 30, 60, 300};
constexpr int kSlideChoices = sizeof(kSlideSeconds) / sizeof(kSlideSeconds[0]);
int slideIndex = SLIDESHOW_DEFAULT_INDEX;

uint32_t lastActivityAt = 0;
uint32_t pageShownAt = 0;
bool sleeping = false;
bool saverOn = false;
uint32_t sleptAt = 0;
uint32_t saverMovedAt = 0;
bool swallowRelease = false;
bool motionSeen = false;

orientation::Tracker tracker(ROTATE_MIN_TILT_G, ROTATE_DOMINANCE, ROTATE_HOLD_MS);
orientation::Motion motion(MOTION_THRESHOLD_G);
uint8_t rotationForPosition[4] = {0, 1, 2, 3};
int currentPosition = -1;

void noteActivity() { lastActivityAt = millis(); }

void setBacklight(uint8_t level) {
  ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, level ? 255 - level : 256);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
}

void initBacklight(uint8_t level) {
  ledc_timer_config_t timer = {};
  timer.speed_mode = LEDC_LOW_SPEED_MODE;
  timer.duty_resolution = LEDC_TIMER_8_BIT;
  timer.timer_num = LEDC_TIMER_3;
  timer.freq_hz = 50000;
  timer.clk_cfg = LEDC_AUTO_CLK;
  ledc_timer_config(&timer);

  ledc_channel_config_t ch = {};
  ch.gpio_num = PIN_BACKLIGHT;
  ch.speed_mode = LEDC_LOW_SPEED_MODE;
  ch.channel = LEDC_CHANNEL_1;
  ch.timer_sel = LEDC_TIMER_3;
  ch.duty = 255 - level;
  ledc_channel_config(&ch);
}

void showUi() {
  rendu::setSource(ui.pixels, kUiW, kUiH, false);
  rendu::render();
}

void showMessage(const char *line1, const char *line2 = nullptr, int32_t spinnerMs = -1) {
  menus::drawMessage(ui, line1, line2, spinnerMs);
  showUi();
}

bool hasGifExtension(const String &name) {
  String lower = name;
  lower.toLowerCase();
  return lower.endsWith(".gif");
}

void scanFolder(const char *folder) {
  File dir = SD_MMC.open(folder);
  if (!dir || !dir.isDirectory()) return;
  const String base = String(folder) == "/" ? "" : String(folder);
  for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
    String name = f.name();
    const int slash = name.lastIndexOf('/');
    if (slash >= 0) name = name.substring(slash + 1);

    if (!f.isDirectory() && !name.startsWith(".") && hasGifExtension(name)) {
      gifFiles.push_back(base + "/" + name);
    }
    f.close();
  }
  dir.close();
}

bool mountSd() {
  SD_MMC.end();
  if (!SD_MMC.setPins(PIN_SD_CLK, PIN_SD_CMD, PIN_SD_D0)) return false;
  return SD_MMC.begin("/sdcard", true);
}

std::vector<String> readOrder() {
  std::vector<String> paths;
  File f = SD_MMC.open(ORDER_FILE);
  while (f && f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length()) paths.push_back(line);
  }
  if (f) f.close();
  return paths;
}

void loadGifList() {
  gifFiles.clear();
  scanFolder(GIF_FOLDER);
  if (gifFiles.empty()) scanFolder("/");
  std::sort(gifFiles.begin(), gifFiles.end(),
            [](const String &a, const String &b) { return a.compareTo(b) < 0; });
  applyOrder(gifFiles, readOrder());
}

void *allocInternal(size_t size) {
  if (size + 24 * 1024 > heap_caps_get_free_size(MALLOC_CAP_INTERNAL)) return nullptr;
  return heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}

void *allocFast(size_t size) {
  if (void *p = allocInternal(size)) return p;
  return heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

void closeGif() {
  if (gifOpen) gif.close();
  gifOpen = false;
  free(gifData);
  free(gifCanvas);
  free(gifTurbo);
  gifData = gifCanvas = gifTurbo = nullptr;
  rendu::clearSource();
}

bool openGif(int index) {
  closeGif();
  const String &path = gifFiles[index];
  File f = SD_MMC.open(path);
  if (!f) return false;

  const size_t size = f.size();
  if (size == 0 || size > GIF_MAX_BYTES) {
    f.close();
    return false;
  }

  gifData = static_cast<uint8_t *>(ps_malloc(size));
  if (!gifData) {
    f.close();
    return false;
  }
  const size_t read = f.read(gifData, size);
  f.close();
  if (read != size || !gif.open(gifData, size, nullptr)) {
    closeGif();
    return false;
  }
  gifOpen = true;
  const int width = gif.getCanvasWidth(), height = gif.getCanvasHeight();

  const size_t canvasBytes = (size_t)width * (height + 3) * 3;

  gifTurbo = static_cast<uint8_t *>(allocInternal(TURBO_BUFFER_SIZE + width * (height + 3)));
  gifCanvas = static_cast<uint8_t *>(allocFast(canvasBytes));
  if (!gifCanvas) {
    closeGif();
    return false;
  }
  memset(gifCanvas, 0, canvasBytes);
  gif.setDrawType(GIF_DRAW_COOKED);
  gif.setFrameBuf(gifCanvas);
  gif.setTurboBuf(gifTurbo);
  rendu::setSource(reinterpret_cast<const uint16_t *>(gifCanvas + width * height), width, height, true);
  return true;
}

bool sdMounted = false;
bool messagePage = false;
bool helpPage = false;

uint32_t helpLeaveAt = 0;
constexpr int kGifBase = HELP_ENABLED ? 1 : 0;
uint32_t lastRetryAt = 0;
void loadLibrary();

int gifSlots() { return gifFiles.empty() ? 1 : (int)gifFiles.size(); }
int pageCount() { return kGifBase + gifSlots(); }
bool isHelpIndex(int index) { return HELP_ENABLED && index == 0; }

void setFullSpeed(bool full) {
  static int current = 240;
  const int wanted = full ? 240 : CPU_MHZ_STATIC;
  if (wanted != current) {
    setCpuFrequencyMhz(wanted);
    current = wanted;
  }
}

void showPage(int index, int dir, bool quiet = false);

void showNoGifMessage() {
  if (!sdMounted) showMessage("Carte SD introuvable", "Vérifie qu'elle est bien insérée et formatée en FAT32");
  else showMessage("Aucun écran", "Appuie 1 seconde sur BOOT pour accéder aux réglages");
}

bool phonePage = false;
int phoneStep = -1;
uint32_t phoneActiveAt = 0;
String phoneSsid, phonePassword;

void loadPhoneIdentity() {
  phonePassword = prefs.getString("wifipass", "");
  if (phonePassword.length() != 8) {
    char digits[9];
    snprintf(digits, sizeof(digits), "%08lu", (unsigned long)(esp_random() % 100000000UL));
    phonePassword = digits;
    prefs.putString("wifipass", phonePassword);
  }
  char name[20];
  const uint64_t mac = ESP.getEfuseMac();
  snprintf(name, sizeof(name), "Screenclip-%02X%02X", (unsigned)((mac >> 32) & 0xFF), (unsigned)((mac >> 40) & 0xFF));
  phoneSsid = name;
}

void onPhoneLibraryChanged() {
  loadGifList();
}

void drawPhonePage(int step) {
  if (step == 0) {
    const String password = "Mot de passe " + phonePassword;
    const char *const lines[] = {"Vise ce code avec ton", "appareil photo, puis accepte", "le Wi-Fi proposé.", phoneSsid.c_str(), password.c_str()};
    const String code = "WIFI:T:WPA;S:" + phoneSsid + ";P:" + phonePassword + ";;";
    menus::drawPhone(ui, "Ajoute tes écrans", lines, 5, 3);
    telephone::drawQr(ui, 16, 64, 248, code.c_str());
  } else {
    const char *const lines[] = {"Vise ce nouveau code pour", "ouvrir ta page.", "", "Ou tape dans le navigateur :", "192.168.4.1"};
    menus::drawPhone(ui, "C'est connecté !", lines, 5, 2);
    telephone::drawQr(ui, 16, 64, 248, telephone::kUrl);
  }
  showUi();
}

void enterPhone() {
  if (phonePage) return;
  phonePage = true;
  phoneStep = -1;
  phoneActiveAt = millis();
  if (!telephone::start(phoneSsid, phonePassword, &gifFiles, onPhoneLibraryChanged)) {
    phonePage = false;
    messagePage = true;
    lastRetryAt = millis();
    showMessage("Wi-Fi indisponible", "Redémarre la barrette");
  }
}

void openPhone() {
  closeGif();
  setFullSpeed(true);
  messagePage = helpPage = false;
  enterPhone();
}

void leavePhone() {
  if (!phonePage) return;
  phonePage = false;
  telephone::stop();
}

void servePhone() {
  telephone::loop();
  const int step = telephone::clients() > 0 ? 1 : 0;
  if (step == 1) phoneActiveAt = millis();
  noteActivity();
  if (step != phoneStep) {
    phoneStep = step;
    drawPhonePage(step);
  }
  if (millis() - phoneActiveAt > PHONE_IDLE_S * 1000UL) showPage(kGifBase, 1, true);
}

void showPage(int index, int dir, bool quiet) {
  if (!quiet) showBatteryOverlay(BATTERY_OVERLAY_MS);
  pageShownAt = millis();
  bool reloaded = false;

  for (int tries = 0; tries <= pageCount() + 1; ++tries) {
    const int count = pageCount();
    currentIndex = ((index % count) + count) % count;
    leavePhone();
    if (isHelpIndex(currentIndex)) {
      closeGif();
      setFullSpeed(false);
      messagePage = false;
      helpPage = true;
      menus::drawHelp(ui);
      showUi();
      return;
    }
    helpPage = false;
    if (gifFiles.empty()) {
      closeGif();
      setFullSpeed(false);
      messagePage = true;
      lastRetryAt = millis();
      showNoGifMessage();
      return;
    }
    setFullSpeed(true);
    if (openGif(currentIndex - kGifBase)) {
      messagePage = false;
      nextFrameAt = millis();
      if (!quiet) prefs.putString("last", gifFiles[currentIndex - kGifBase]);
      return;
    }

    if (!reloaded) {
      reloaded = true;
      loadLibrary();
      if (gifFiles.empty()) {
        index = kGifBase;
        continue;
      }
    }
    index += dir;
  }
  setFullSpeed(false);
  messagePage = true;
  lastRetryAt = millis();
  showMessage("GIF illisibles", "Vérifie les fichiers sur la carte SD");
}

void step(int dir) {
  helpLeaveAt = 0;
  if (phonePage) showPage(kGifBase, 1);
  else showPage(currentIndex + dir, dir);
}

int savedIndex() {
  const String last = prefs.getString("last", "");
  for (size_t i = 0; i < gifFiles.size(); ++i) {
    if (gifFiles[i] == last) return kGifBase + i;
  }
  return kGifBase;
}

bool transferDone = false;

void loadLibrary() {
  sdMounted = mountSd();
  gifFiles.clear();
  if (!sdMounted) return;
  SD_MMC.mkdir(GIF_FOLDER);
  loadGifList();
#if USB_DRIVE
  if (!usbdrive::ready) usbdrive::begin();
#endif
}

void startLibrary() {
  if (HELP_ENABLED && HELP_AT_STARTUP && !gifFiles.empty()) {
    showPage(0, 1);
    helpLeaveAt = (millis() + HELP_STARTUP_MS) | 1;
  } else {
    showPage(savedIndex(), 1);
  }
}

void bootScreen() {
  const uint32_t start = millis();
  while (millis() - start < menus::kBootMs) {
    menus::drawBoot(ui, millis() - start);
    showUi();
  }
}

void rotateScreen() {
  rendu::setRotation(rendu::rotation + 1);
  prefs.putUChar("rot", rendu::rotation);
  // En rotation auto, le geste corrige la position actuelle et la retient.
  if (autoRotate && currentPosition >= 0) {
    rotationForPosition[currentPosition] = rendu::rotation;
    prefs.putBytes("rotmap", rotationForPosition, sizeof(rotationForPosition));
  }
}

bool settingsPage = false;
constexpr int kSettingsRows = 6;
int settingsRow = 0;
uint32_t settingsTouchedAt = 0;

void drawSettingsPage() {
  const char *const onOff[2] = {"désactivée", "activée"};
  const char *const missing = "capteur introuvable";
  static const char *const slides[kSlideChoices] = {"désactivé", "toutes les 30 s", "toutes les minutes", "toutes les 5 min"};
  const menus::SettingRow rows[] = {
      {"Rotation auto", capteur::present ? onOff[autoRotate] : missing},
      {"Tourner l'écran", "ouvrir"},
      {"Veille", capteur::present ? onOff[sleepEnabled] : missing},
      {"Diaporama", slides[slideIndex]},
      {"Ajouter des écrans", "ouvrir"},
      {"Quitter", ""},
  };
  menus::drawSettings(ui, rows, kSettingsRows, settingsRow);
  showUi();
}

void openSettings() {
  leavePhone();
  closeGif();
  setFullSpeed(false);
  settingsPage = true;
  settingsRow = 0;
  settingsTouchedAt = millis();
  drawSettingsPage();
}

void closeSettings() {
  settingsPage = false;
  showPage(currentIndex, 1, true);
}

void leaveRotateMode() {
  rotateMode = false;
  if (rendu::hasSource()) rendu::render();
}

void changeSetting() {
  switch (settingsRow) {
    case 0:
      if (!capteur::present) break;
      autoRotate = !autoRotate;
      prefs.putBool("autorot", autoRotate);
      if (autoRotate) currentPosition = -1;
      break;
    case 1:
      settingsPage = false;
      showPage(currentIndex, 1, true);
      rotateMode = true;
      rotateTouchedAt = millis();
      if (rendu::hasSource()) rendu::render();
      break;
    case 2:
      if (!capteur::present) break;
      sleepEnabled = !sleepEnabled;
      prefs.putBool("veille", sleepEnabled);
      break;
    case 3:
      slideIndex = (slideIndex + 1) % kSlideChoices;
      prefs.putUChar("diapo", slideIndex);
      break;
    case 4:
      settingsPage = false;
      if (sdMounted) openPhone();
      else showPage(currentIndex, 1, true);
      break;
    default:
      closeSettings();
      break;
  }
}

void drawSaver() {
  menus::drawSaver(ui, esp_random() % (kUiW - menus::kSaverW), esp_random() % (kUiH - menus::kSaverH));
  showUi();
  saverMovedAt = millis();
}

void enterSleep() {
  sleeping = true;
  saverOn = true;
  sleptAt = millis();
  motionSeen = false;
  closeGif();
  setFullSpeed(false);
  setBacklight(SAVER_BACKLIGHT);
  drawSaver();
}

void wakeUp(bool restore = true) {
  sleeping = false;
  saverOn = false;
  setBacklight(BACKLIGHT_LEVEL);
  noteActivity();
  if (restore) showPage(currentIndex, 1, true);
}

// Le petit écran de veille, peu lumineux, puis écran éteint.
void updateSaver() {
  if (!saverOn) return;
  if (millis() - sleptAt >= SAVER_S * 1000UL) {
    saverOn = false;
    setBacklight(0);
  } else if (millis() - saverMovedAt >= SAVER_STEP_MS) {
    drawSaver();
  }
}

void pollSensors() {
  static uint32_t lastPollAt = 0;
  if (!capteur::present || millis() - lastPollAt < SENSOR_PERIOD_MS) return;
  lastPollAt = millis();
  float ax, ay, az;
  if (!capteur::read(ax, ay, az)) return;
  if (motion.update(ax, ay, az)) {
    motionSeen = true;
    noteActivity();
  }
  if (!autoRotate || sleeping || rotateMode) return;
  const int position = tracker.update(ax, ay, millis());
  if (position < 0 || position == currentPosition) return;
  currentPosition = position;
  if (rotationForPosition[position] != rendu::rotation) rendu::setRotation(rotationForPosition[position]);
}

// Le premier appui en veille ne fait que réveiller la barrette.
bool handleSleep() {
  if (sleeping) {
    if (!motionSeen && !bouton::held && uxQueueMessagesWaiting(bouton::events) == 0) {
      updateSaver();
      delay(20);
      return true;
    }
    swallowRelease = bouton::held;
    xQueueReset(bouton::events);
    wakeUp();
    return false;
  }
  if (sleepEnabled && capteur::present && !settingsPage && !rotateMode && !bouton::held &&
      millis() - lastActivityAt >= SLEEP_AFTER_S * 1000UL) {
    enterSleep();
    return true;
  }
  return false;
}

void advanceSlideshow() {
  const int seconds = kSlideSeconds[slideIndex];
  if (!seconds || !gifOpen || rotateMode || gifFiles.size() < 2) return;
  if (millis() - pageShownAt < seconds * 1000UL) return;
  const int next = currentIndex + 1 >= kGifBase + gifSlots() ? kGifBase : currentIndex + 1;
  showPage(next, 1, true);
}

void handleBootButton() {
  static bool pending = false;
  static uint32_t pendingAt = 0;
  bouton::Event event;
  while (xQueueReceive(bouton::events, &event, 0) == pdTRUE) {
    noteActivity();
    if (swallowRelease) {
      swallowRelease = false;
      continue;
    }
    if (rotateMode) {
      rotateTouchedAt = millis();
      if (event.isLong) leaveRotateMode();
      else rotateScreen();
    } else if (settingsPage) {
      settingsTouchedAt = millis();
      if (event.isLong) changeSetting();
      else settingsRow = (settingsRow + 1) % kSettingsRows;
      if (settingsPage) drawSettingsPage();
    } else if (event.isLong) {
      pending = false;
      openSettings();
    } else if (pending && event.at - pendingAt <= DOUBLE_CLICK_MS) {
      pending = false;
      step(-1);
    } else {
      if (pending) step(+1);
      pending = true;
      pendingAt = event.at;
    }
  }

  // Un appui seul n'agit qu'une fois écarté un éventuel second appui.
  if (pending && !bouton::held && millis() - pendingAt > DOUBLE_CLICK_MS) {
    pending = false;
    step(+1);
  }
}

bool transferMode = false;
uint32_t transferStartMs = 0;

void enterTransfer() {
  if (sleeping) wakeUp(false);
  settingsPage = false;
  rotateMode = false;
  leavePhone();
  closeGif();
  setFullSpeed(true);
  overlayUntil = 0;
  transferStartMs = millis();
  transferMode = true;
  usbdrive::expose(true);
}

void exitTransfer() {
  usbdrive::expose(false);
  transferMode = false;
  noteActivity();
  transferDone = true;
  delay(200);
  loadLibrary();
  startLibrary();
}

bool handleTransfer() {
#if USB_DRIVE
  static uint32_t hostSince = 0;
  const bool host = usbdrive::hostConnected();
  if (!host) {
    hostSince = 0;
    transferDone = false;
  } else if (!hostSince) {
    hostSince = millis();
  }
  if (transferMode) {
    if (!host || usbdrive::ejected || transferDone) exitTransfer();
    else showMessage("Transfert USB", "Copie tes GIF sur le disque, puis éjecte-le", millis() - transferStartMs);
  } else if (host && !transferDone && sdMounted && usbdrive::ready && millis() - hostSince > 1000) {
    enterTransfer();
  }
#endif
  return transferMode;
}
}

void setup() {
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);

  Serial.enableReboot(false);

  initBacklight(BACKLIGHT_LEVEL);
  rendu::overlay = compositeOverlay;
  rendu::display.init();

  bouton::start();

  ui.pixels = static_cast<uint16_t *>(heap_caps_malloc(kUiW * kUiH * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  hintPixels = static_cast<uint16_t *>(heap_caps_malloc(menus::kHintW * menus::kHintH * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (hintPixels) {
    Canvas hint = {hintPixels, menus::kHintW, menus::kHintH};
    menus::drawRotateHint(hint);
  }

  gif.begin(GIF_PALETTE_RGB565_LE);

  prefs.begin("gifplayer", false);
  rendu::rotation = prefs.getUChar("rot", SCREEN_ROTATION) & 3;

  autoRotate = prefs.getBool("autorot", AUTO_ROTATE_DEFAULT);
  sleepEnabled = prefs.getBool("veille", SLEEP_DEFAULT);
  slideIndex = prefs.getUChar("diapo", SLIDESHOW_DEFAULT_INDEX) % kSlideChoices;
  if (prefs.getBytesLength("rotmap") == sizeof(rotationForPosition)) {
    prefs.getBytes("rotmap", rotationForPosition, sizeof(rotationForPosition));
  }
  capteur::begin();
  loadPhoneIdentity();
  noteActivity();

  for (int i = 0; i < 4; ++i) battery::update(true);
  loadLibrary();
#if BOOT_SCREEN
  showBatteryOverlay(menus::kBootMs + BATTERY_OVERLAY_MS);
  bootScreen();
#endif
  startLibrary();
}

void loop() {
  if (handleTransfer()) return;

#if BATTERY_OVERLAY
  battery::update();
#endif

  if (overlayShown && (int32_t)(overlayUntil - millis()) <= 0) {
    overlayShown = false;
    if (rendu::hasSource()) rendu::render();
  }

  pollSensors();
  if (handleSleep()) return;

  handleBootButton();

  if (rotateMode && millis() - rotateTouchedAt > SETTINGS_TIMEOUT_MS) leaveRotateMode();

  if (settingsPage) {
    if (millis() - settingsTouchedAt > SETTINGS_TIMEOUT_MS) closeSettings();
    delay(20);
    return;
  }

  if (phonePage) {
    servePhone();
    delay(1);
    return;
  }

  if (helpPage) {
    if (helpLeaveAt && (int32_t)(millis() - helpLeaveAt) >= 0) {
      helpLeaveAt = 0;
      showPage(savedIndex(), 1);
      return;
    }
    delay(20);
    return;
  }

  if (messagePage) {
    if (millis() - lastRetryAt > 3000) {
      lastRetryAt = millis();
      loadLibrary();
      if (!gifFiles.empty()) showPage(kGifBase, 1);
    }
    delay(20);
    return;
  }

  advanceSlideshow();

  if (gifOpen && (int32_t)(millis() - nextFrameAt) >= 0) {
    int delayMs = 0;
    const int result = gif.playFrame(false, &delayMs);
    if (result < 0) {
      step(+1);
      return;
    }
    rendu::render();
    if (result == 0) gif.reset();

    nextFrameAt += std::max(delayMs, 10);
    if ((int32_t)(millis() - nextFrameAt) > 100) nextFrameAt = millis();
  }

  delay(1);
}
