#pragma once

#include <Arduino.h>
#include <DNSServer.h>
#include <SD_MMC.h>
#include <WebServer.h>
#include <WiFi.h>
#include <qrcode.h>

#include <vector>

#include "page_web.h"
#include "reglages.h"
#include "texte.h"

// La barrette émet son propre Wi-Fi et sert le site qui gère les écrans.
namespace telephone {
inline const IPAddress kIp(192, 168, 4, 1);
inline const char *const kUrl = "http://192.168.4.1/";
constexpr const char *kTmp = GIF_FOLDER "/.envoi.tmp";

inline DNSServer dns;
inline WebServer server(80);
inline std::vector<String> *files = nullptr;
inline void (*onChanged)() = nullptr;
inline bool running = false;
inline bool routesReady = false;
inline String ssid, password;

inline File upload;
inline size_t uploaded = 0;
inline int uploadError = 0;

inline bool known(const String &path) {
  for (const String &f : *files) {
    if (f == path) return true;
  }
  return false;
}

inline String jsonEscape(const String &s) {
  String out;
  for (size_t i = 0; i < s.length(); ++i) {
    const char c = s[i];
    if (c == '"' || c == '\\') out += '\\';
    out += c;
  }
  return out;
}

inline String nextPath() {
  int highest = 0;
  for (const String &f : *files) {
    const int at = f.lastIndexOf("/ecran-");
    if (at >= 0) highest = max(highest, (int)f.substring(at + 7).toInt());
  }
  char name[32];
  for (int n = highest + 1;; ++n) {
    snprintf(name, sizeof(name), GIF_FOLDER "/ecran-%04d.gif", n);
    if (!SD_MMC.exists(name)) return name;
  }
}

inline void sendList() {
  String json = "{\"ecrans\":[";
  for (size_t i = 0; i < files->size(); ++i) {
    if (i) json += ',';
    json += "{\"chemin\":\"" + jsonEscape((*files)[i]) + "\"}";
  }
  json += "]}";
  server.send(200, "application/json", json);
}

inline void sendFile() {
  const String path = server.arg("chemin");
  if (!known(path)) return server.send(404, "text/plain", "introuvable");
  File f = SD_MMC.open(path);
  if (!f) return server.send(404, "text/plain", "illisible");
  server.streamFile(f, "image/gif");
  f.close();
}

inline void removeFile() {
  const String path = server.arg("chemin");
  if (!known(path)) return server.send(404, "text/plain", "introuvable");
  if (!SD_MMC.remove(path)) return server.send(500, "text/plain", "suppression impossible");
  onChanged();
  server.send(200, "text/plain", "ok");
}

inline void receiveUpload() {
  HTTPUpload &part = server.upload();
  if (part.status == UPLOAD_FILE_START) {
    uploadError = 0;
    uploaded = 0;
    SD_MMC.mkdir(GIF_FOLDER);
    SD_MMC.remove(kTmp);
    upload = SD_MMC.open(kTmp, FILE_WRITE);
    if (!upload) uploadError = 500;
  } else if (part.status == UPLOAD_FILE_WRITE) {
    if (uploadError) return;
    uploaded += part.currentSize;
    if (uploaded > GIF_MAX_BYTES) uploadError = 413;
    else if (upload.write(part.buf, part.currentSize) != part.currentSize) uploadError = 507;
  } else if (part.status == UPLOAD_FILE_END) {
    if (upload) upload.close();
  } else if (part.status == UPLOAD_FILE_ABORTED) {
    if (upload) upload.close();
    SD_MMC.remove(kTmp);
  }
}

inline void finishUpload() {
  bool valid = false;
  if (!uploadError) {
    File f = SD_MMC.open(kTmp);
    char head[4] = {};
    valid = f && f.read((uint8_t *)head, 4) == 4 && !memcmp(head, "GIF8", 4);
    f.close();
    if (!valid) uploadError = 415;
  }
  if (uploadError || !SD_MMC.rename(kTmp, nextPath())) {
    SD_MMC.remove(kTmp);
    return server.send(uploadError ? uploadError : 500, "text/plain", "refusé");
  }
  onChanged();
  server.send(200, "text/plain", "ok");
}

// Les téléphones vérifient l'accès à internet : on répond « tout va bien » pour
// qu'ils restent sur ce Wi-Fi et que le site s'ouvre dans le vrai navigateur.
inline void answerOther() {
  const String uri = server.uri();
  if (uri == "/hotspot-detect.html" || uri.startsWith("/library/test/success")) {
    return server.send(200, "text/html", "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>");
  }
  if (uri == "/generate_204" || uri == "/gen_204") return server.send(204);
  if (uri == "/connecttest.txt") return server.send(200, "text/plain", "Microsoft Connect Test");
  server.sendHeader("Location", kUrl);
  server.send(302, "text/plain", "");
}

inline void addRoutes() {
  if (routesReady) return;
  routesReady = true;
  server.on("/", HTTP_GET, [] {
    server.sendHeader("Content-Encoding", "gzip");
    server.send_P(200, "text/html; charset=utf-8", reinterpret_cast<const char *>(kPageWeb), kPageWebSize);
  });
  server.on("/api/ecrans", HTTP_GET, sendList);
  server.on("/api/fichier", HTTP_GET, sendFile);
  server.on("/api/supprimer", HTTP_POST, removeFile);
  server.on("/api/ecrans", HTTP_POST, finishUpload, receiveUpload);
  server.onNotFound(answerOther);
}

inline bool start(const String &name, const String &pass, std::vector<String> *list, void (*changed)()) {
  if (running) return true;
  ssid = name;
  password = pass;
  files = list;
  onChanged = changed;
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(kIp, kIp, IPAddress(255, 255, 255, 0));
  if (!WiFi.softAP(ssid.c_str(), password.c_str(), 1, 0, 2)) {
    WiFi.mode(WIFI_OFF);
    return false;
  }
  dns.start(53, "*", kIp);
  addRoutes();
  server.begin();
  running = true;
  return true;
}

inline void stop() {
  if (!running) return;
  server.stop();
  dns.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  running = false;
}

inline void loop() {
  if (!running) return;
  dns.processNextRequest();
  server.handleClient();
}

inline int clients() { return running ? WiFi.softAPgetStationNum() : 0; }

inline Canvas *qrCanvas = nullptr;
inline int qrX = 0, qrY = 0, qrBox = 0;

inline void paintQr(esp_qrcode_handle_t code) {
  const int size = esp_qrcode_get_size(code);
  const int quiet = 3;
  const int scale = qrBox / (size + 2 * quiet);
  const int side = scale * (size + 2 * quiet);
  const int x0 = qrX + (qrBox - side) / 2, y0 = qrY + (qrBox - side) / 2;
  qrCanvas->fillRect(x0, y0, side, side, 0xFFFF);
  for (int y = 0; y < size; ++y) {
    for (int x = 0; x < size; ++x) {
      if (esp_qrcode_get_module(code, x, y)) {
        qrCanvas->fillRect(x0 + (x + quiet) * scale, y0 + (y + quiet) * scale, scale, scale, 0x0000);
      }
    }
  }
}

inline bool drawQr(Canvas &c, int x, int y, int box, const char *text) {
  qrCanvas = &c;
  qrX = x;
  qrY = y;
  qrBox = box;
  esp_qrcode_config_t cfg = ESP_QRCODE_CONFIG_DEFAULT();
  cfg.display_func = paintQr;
  cfg.max_qrcode_version = 8;
  cfg.qrcode_ecc_level = ESP_QRCODE_ECC_MED;
  return esp_qrcode_generate(&cfg, text) == ESP_OK;
}
}
