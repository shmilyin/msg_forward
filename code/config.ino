/* 配置、统计和有界短信历史存储。 */
Statistics stats = {0, 0, 0, 0, 0};

void saveConfig() {
  preferences.begin("sms_config", false);
  for (int i = 0; i < MAX_WIFI_NETWORKS; ++i) {
    String p = "wifi" + String(i);
    preferences.putString((p + "ssid").c_str(), config.wifiNetworks[i].ssid);
    preferences.putString((p + "pass").c_str(), config.wifiNetworks[i].password);
    preferences.putBool((p + "en").c_str(), config.wifiNetworks[i].enabled);
  }
  preferences.putString("smtpServer", config.smtpServer);
  preferences.putInt("smtpPort", config.smtpPort);
  preferences.putString("smtpUser", config.smtpUser);
  preferences.putString("smtpPass", config.smtpPass);
  preferences.putString("smtpSendTo", config.smtpSendTo);
  preferences.putBool("smtpEn", config.emailEnabled);
  preferences.putString("webUser", config.webUser);
  preferences.putString("webPass", config.webPass);
  for (int i = 0; i < MAX_PUSH_CHANNELS; ++i) {
    String p = "push" + String(i);
    const PushChannel& ch = config.pushChannels[i];
    preferences.putBool((p + "en").c_str(), ch.enabled);
    preferences.putUChar((p + "type").c_str(), (uint8_t)ch.type);
    preferences.putString((p + "url").c_str(), ch.url);
    preferences.putString((p + "name").c_str(), ch.name);
    preferences.putString((p + "k1").c_str(), ch.key1);
    preferences.putString((p + "k2").c_str(), ch.key2);
    preferences.putString((p + "body").c_str(), ch.customBody);
    yield();
  }
  preferences.putBool("timerEn", config.timerEnabled);
  preferences.putInt("timerType", config.timerType);
  preferences.putInt("timerInt", config.timerInterval);
  preferences.putString("timerPhone", config.timerPhone);
  preferences.putString("timerMsg", config.timerMessage);
  preferences.putBool("airEn", config.airplaneEnabled);
  preferences.putInt("airStart", config.airplaneStartMinute);
  preferences.putInt("airEnd", config.airplaneEndMinute);
  preferences.putBool("airManual", config.airplaneManualTarget);
  preferences.end();
}

void loadConfig() {
  preferences.begin("sms_config", true);
  for (int i = 0; i < MAX_WIFI_NETWORKS; ++i) {
    String p = "wifi" + String(i);
    config.wifiNetworks[i].ssid = preferences.getString((p + "ssid").c_str(), i == 0 ? WIFI_SSID : "");
    config.wifiNetworks[i].password = preferences.getString((p + "pass").c_str(), i == 0 ? WIFI_PASS : "");
    config.wifiNetworks[i].enabled = preferences.getBool((p + "en").c_str(), i == 0);
  }
  config.smtpServer = preferences.getString("smtpServer", "");
  config.smtpPort = preferences.getInt("smtpPort", 465);
  config.smtpUser = preferences.getString("smtpUser", "");
  config.smtpPass = preferences.getString("smtpPass", "");
  config.smtpSendTo = preferences.getString("smtpSendTo", "");
  config.emailEnabled = preferences.getBool("smtpEn", false);
  config.webUser = preferences.getString("webUser", DEFAULT_WEB_USER);
  config.webPass = preferences.getString("webPass", DEFAULT_WEB_PASS);
  for (int i = 0; i < MAX_PUSH_CHANNELS; ++i) {
    String p = "push" + String(i);
    PushChannel& ch = config.pushChannels[i];
    ch.enabled = preferences.getBool((p + "en").c_str(), false);
    ch.type = (PushType)preferences.getUChar((p + "type").c_str(), PUSH_TYPE_POST_JSON);
    ch.url = preferences.getString((p + "url").c_str(), "");
    ch.name = preferences.getString((p + "name").c_str(), "通道" + String(i + 1));
    ch.key1 = preferences.getString((p + "k1").c_str(), "");
    ch.key2 = preferences.getString((p + "k2").c_str(), "");
    ch.customBody = preferences.getString((p + "body").c_str(), "");
  }
  config.timerEnabled = preferences.getBool("timerEn", false);
  config.timerType = preferences.getInt("timerType", 0);
  config.timerInterval = preferences.getInt("timerInt", 30);
  if (config.timerInterval < 1 || config.timerInterval > 3650) config.timerInterval = 30;
  config.timerPhone = preferences.getString("timerPhone", "");
  config.timerMessage = preferences.getString("timerMsg", "保号短信");
  timerIntervalSec = (unsigned long)config.timerInterval * 86400UL;
  config.airplaneEnabled = preferences.getBool("airEn", false);
  config.airplaneStartMinute = preferences.getInt("airStart", 1380);
  config.airplaneEndMinute = preferences.getInt("airEnd", 420);
  if (config.airplaneStartMinute < 0 || config.airplaneStartMinute >= 1440 ||
      config.airplaneEndMinute < 0 || config.airplaneEndMinute >= 1440 ||
      config.airplaneStartMinute == config.airplaneEndMinute) {
    config.airplaneEnabled = false;
    config.airplaneStartMinute = 1380;
    config.airplaneEndMinute = 420;
  }
  config.airplaneManualTarget = preferences.getBool("airManual", false);
  preferences.end();
  loadStats();
}

bool isPushChannelValid(const PushChannel& ch) {
  if (!ch.enabled) return false;
  switch (ch.type) {
    case PUSH_TYPE_POST_JSON:
    case PUSH_TYPE_BARK:
    case PUSH_TYPE_GET:
    case PUSH_TYPE_CUSTOM:
    case PUSH_TYPE_WECOM:
    case PUSH_TYPE_DINGTALK:
    case PUSH_TYPE_FEISHU:
      return ch.url.length() > 0;
    case PUSH_TYPE_TELEGRAM:
      return ch.url.length() > 0 && ch.key1.length() > 0;
    default: return false;
  }
}
bool isConfigValid() {
  if (config.emailEnabled && config.smtpServer.length() && config.smtpUser.length() &&
      config.smtpPass.length() && config.smtpSendTo.length()) return true;
  for (int i = 0; i < MAX_PUSH_CHANNELS; ++i)
    if (isPushChannelValid(config.pushChannels[i])) return true;
  return false;
}
String getDeviceUrl() {
  return "http://" MDNS_HOSTNAME ".local 或 http://" + WiFi.localIP().toString();
}

// 先保留旧文件为备份；启动时只在主文件缺失时恢复备份。
void recoverSmsStorage() {
  if (!SPIFFS.exists("/sms.txt") && SPIFFS.exists("/sms.bak"))
    SPIFFS.rename("/sms.bak", "/sms.txt");
}
void initSmsStorage() {
  if (!SPIFFS.begin(false)) Serial.println("SPIFFS 初始化失败，保留原始数据");
  else recoverSmsStorage();
}

bool clearSmsHistory() {
  // 主文件最后删除，避免失败或掉电后由旧备份恢复已清理记录。
  const char* paths[] = {"/sms.tmp", "/sms.bak", "/sms.txt"};
  for (const char* path : paths) {
    if (SPIFFS.exists(path) && !SPIFFS.remove(path)) return false;
    if (SPIFFS.exists(path)) return false;
  }
  return true;
}

bool compactSmsStorage() {
  recoverSmsStorage();
  File src = SPIFFS.open("/sms.txt", "r");
  if (!src) return false;
  size_t size = src.size();
  if (SPIFFS.totalBytes() - SPIFFS.usedBytes() < size / 2 + 2048) { src.close(); return false; }
  if (!src.seek(size / 2)) { src.close(); return false; }
  while (src.available() && src.read() != '\n') yield();
  size_t begin = src.position(), completeEnd = begin;
  while (src.available()) { if (src.read() == '\n') completeEnd = src.position(); yield(); }
  if (!src.seek(begin)) { src.close(); return false; }
  size_t expected = completeEnd - begin;
  File dst = SPIFFS.open("/sms.tmp", "w");
  if (!dst) { src.close(); return false; }
  uint8_t buffer[512];
  size_t written = 0;
  bool ok = true;
  while (written < expected) {
    size_t remaining = expected - written;
    size_t n = src.read(buffer, remaining < sizeof(buffer) ? remaining : sizeof(buffer));
    if (!n || dst.write(buffer, n) != n) { ok = false; break; }
    written += n;
    yield();
  }
  dst.flush(); dst.close(); src.close();
  File verify = SPIFFS.open("/sms.tmp", "r");
  ok = ok && verify && written == expected && verify.size() == expected;
  verify.close();
  if (!ok) return false;
  if (SPIFFS.exists("/sms.bak") && !SPIFFS.remove("/sms.bak")) return false;
  if (!SPIFFS.rename("/sms.txt", "/sms.bak")) return false;
  if (!SPIFFS.rename("/sms.tmp", "/sms.txt")) { recoverSmsStorage(); return false; }
  return true;
}

void addSmsToHistory(const char* sender, const char* message, const char* timestamp) {
  stats.smsReceived++;
  recoverSmsStorage();
  File f = SPIFFS.open("/sms.txt", "r");
  size_t size = f ? f.size() : 0;
  bool partialTail = false;
  if (size && f.seek(size - 1)) partialTail = f.read() != '\n';
  f.close();
  if ((size > 50000 || partialTail) && !compactSmsStorage()) {
    Serial.println("历史裁剪失败，保留原文件并暂停追加");
    saveStats(); return;
  }
  String preview = String(message).substring(0, 200);
  // 不保留半个 UTF-8 字符。
  unsigned int cut = preview.length();
  if (strlen(message) > cut) {
    while (cut && (((uint8_t)message[cut] & 0xc0) == 0x80)) --cut;
    preview = preview.substring(0, cut);
  }
  String line = "{\"t\":\"" + jsonEscape(String(timestamp).substring(0, 64)) +
    "\",\"s\":\"" + jsonEscape(String(sender).substring(0, 64)) + "\",\"m\":\"" + jsonEscape(preview) + "\"}\n";
  f = SPIFFS.open("/sms.txt", "a");
  if (!f || f.print(line) != line.length()) Serial.println("历史写入失败");
  if (f) { f.flush(); f.close(); }
  saveStats();
}

void streamSmsHistory() {
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "application/json", "");
  server.sendContent("{\"history\":[");
  File f = SPIFFS.open("/sms.txt", "r");
  size_t offsets[MAX_SMS_HISTORY];
  unsigned int count = 0, next = 0;
  size_t start = 0;
  while (f && f.available()) {
    if (f.read() == '\n') {
      size_t len = f.position() - start;
      if (len > 10 && len <= 2048) {
        offsets[next] = start; next = (next + 1) % MAX_SMS_HISTORY;
        if (count < MAX_SMS_HISTORY) ++count;
      }
      start = f.position();
    }
    yield();
  }
  char line[2048];
  bool first = true;
  for (unsigned int i = 0; i < count; ++i) {
    f.seek(offsets[(next + MAX_SMS_HISTORY - 1 - i) % MAX_SMS_HISTORY]);
    size_t n = 0;
    while (f.available() && n < sizeof(line) - 1) {
      int c = f.read(); if (c == '\n') break; line[n++] = (char)c;
    }
    line[n] = 0;
    if (!first) server.sendContent(",");
    server.sendContent(line, n); first = false;
    yield();
  }
  f.close();
  server.sendContent("]}"); server.sendContent("");
}
void saveStats() {
  preferences.begin("sms_stats", false);
  preferences.putULong("received", stats.smsReceived);
  preferences.putULong("sent", stats.smsSent);
  preferences.putULong("pushOk", stats.pushSuccess);
  preferences.putULong("pushFail", stats.pushFailed);
  preferences.putULong("boots", stats.bootCount);
  preferences.end();
}
void loadStats() {
  preferences.begin("sms_stats", true);
  stats.smsReceived = preferences.getULong("received", 0);
  stats.smsSent = preferences.getULong("sent", 0);
  stats.pushSuccess = preferences.getULong("pushOk", 0);
  stats.pushFailed = preferences.getULong("pushFail", 0);
  stats.bootCount = preferences.getULong("boots", 0) + 1;
  preferences.end();
  preferences.begin("sms_stats", false);
  preferences.putULong("boots", stats.bootCount);
  preferences.end();
}
