static AirSchedule airSchedule;
static int airActual = -1;
static bool airRestoring = true;
static bool airSwitching = false;
static bool airSimReady = false, airRegistered = false, airAttached = false;
static String airError;
static unsigned long airRetryAt = 0;
static unsigned int airFailures = 0;
static bool airAttempted = false;

void initAirplaneMode() {
  airSchedule = AirSchedule();
  airActual = -1; airRestoring = true; airAttempted = false;
}
static void updateAirTarget() {
  airSchedule.update(config.airplaneEnabled, config.airplaneStartMinute,
    config.airplaneEndMinute, config.airplaneManualTarget, time(nullptr));
}
bool cellularReady() {
  updateAirTarget();
  return airSchedule.targetKnown && !airSchedule.target && airActual == 1 &&
    !airRestoring && !airSwitching && !modemTransactionBusy() && airSimReady && airRegistered;
}
String cellularBlockReason() {
  if (modemTransactionBusy()) return "模组事务忙碌";
  if (!airSchedule.targetKnown) return "等待校时，飞行目标未知";
  if (airSchedule.target || airActual == 4) return "飞行模式中";
  if (airActual < 0) return "模组射频状态未知";
  if (airRestoring || airSwitching) return "模组恢复中";
  return "SIM或网络尚未就绪";
}
static int queryCFUN() {
  String r = transactionATCommand("AT+CFUN?", 1500);
  if (!atResponseOK(r)) return -1;
  if (r.indexOf("\n+CFUN: 4\n") >= 0) return 4;
  if (r.indexOf("\n+CFUN: 1\n") >= 0) return 1;
  return -1;
}
void serviceAirplaneMode() {
  updateAirTarget();
  if (modemTransactionBusy()) return;
  unsigned long now = millis();
  if (airAttempted && (int32_t)(now - airRetryAt) < 0) return;
  bool desiredMatches = airSchedule.targetKnown && airActual == (airSchedule.target ? 4 : 1);
  if (desiredMatches && !airRestoring && airActual == 4) return;
  ATTransaction transaction;
  if (!transaction) return;
  airAttempted = true;
  airSwitching = true;
  int previousActual = airActual;
  airActual = queryCFUN();
  if (previousActual != airActual && airActual == 1) airRestoring = true;
  // 未校时仅查询，不主动开启射频，也不初始化数据连接。
  if (!airSchedule.targetKnown) {
    airSwitching = false; airRetryAt = millis() + 10000; return;
  }
  int desired = airSchedule.target ? 4 : 1;
  if (airActual != desired) {
    airRestoring = true;
    airSimReady = airRegistered = airAttached = false;
    transactionATCommand(desired == 4 ? "AT+CFUN=4" : "AT+CFUN=1", 3000);
    airActual = queryCFUN();
  }
  bool ok = airActual == desired;
  if (!ok) airError = "CFUN设置后回读未确认目标";
  if (ok && desired == 1) {
    if (airRestoring) {
      bool pduOK = atResponseOK(transactionATCommand("AT+CMGF=0", 1500));
      bool cnmiOK = atResponseOK(transactionATCommand("AT+CNMI=2,2,0,0,0", 1500));
      ok = pduOK && cnmiOK;
      if (!ok) airError = "恢复CMGF/CNMI失败";
    }
    if (ok) {
      airRestoring = false;
      String sim = transactionATCommand("AT+CPIN?", 1000);
      airSimReady = atResponseOK(sim) && sim.indexOf("\n+CPIN: READY\n") >= 0;
      String reg = transactionATCommand("AT+CEREG?", 1000);
      int n = -1, stat = -1;
      int pos = reg.indexOf("\n+CEREG:");
      airRegistered = atResponseOK(reg) && pos >= 0 &&
        sscanf(reg.c_str() + pos + 8, "%d,%d", &n, &stat) == 2 && (stat == 1 || stat == 5);
      String attached = transactionATCommand("AT+CGATT?", 1000);
      airAttached = atResponseOK(attached) && attached.indexOf("\n+CGATT: 1\n") >= 0;
    }
  } else if (ok) {
    airRestoring = false;
    airSimReady = airRegistered = airAttached = false;
  }
  airSwitching = false;
  if (ok) { airError = String(); airFailures = 0; }
  else if (airFailures < 5) ++airFailures;
  airRetryAt = millis() + (ok ? 15000UL : (1000UL << airFailures));
}

static String airTime(int minute) {
  char buf[24]; snprintf(buf, sizeof(buf), "%02d:%02d", minute / 60, minute % 60); return String(buf);
}
static bool parseAirTime(const String& value, int& minute) {
  if (value.length() != 5 || value[2] != ':') return false;
  for (int i = 0; i < 5; ++i) if (i != 2 && (value[i] < '0' || value[i] > '9')) return false;
  int hour = (value[0] - '0') * 10 + value[1] - '0';
  int min = (value[3] - '0') * 10 + value[4] - '0';
  minute = hour * 60 + min; return hour < 24 && min < 60;
}
static bool uniqueAirArgs(const char* const* names, int count) {
  if (server.args() != count) return false;
  for (int i = 0; i < count; ++i) {
    int found = 0;
    for (int j = 0; j < server.args(); ++j) if (server.argName(j) == names[i]) ++found;
    if (found != 1) return false;
  }
  return true;
}
void handleAirplaneStatus() {
  if (!checkAuth()) return;
  updateAirTarget();
  time_t now = time(nullptr), local = now + 28800;
  struct tm tmValue;
  gmtime_r(&local, &tmValue);
  char clock[32]; strftime(clock, sizeof(clock), "%Y-%m-%d %H:%M:%S", &tmValue);
  String json = "{\"enabled\":" + String(config.airplaneEnabled ? "true" : "false") +
    ",\"start\":\"" + airTime(config.airplaneStartMinute) + "\",\"end\":\"" + airTime(config.airplaneEndMinute) +
    "\",\"synced\":" + String(AirSchedule::synced(now) ? "true" : "false") +
    ",\"beijing\":\"" + String(AirSchedule::synced(now) ? clock : "未校时") +
    "\",\"targetKnown\":" + String(airSchedule.targetKnown ? "true" : "false") +
    ",\"target\":" + String(airSchedule.target ? "true" : "false") +
    ",\"actual\":" + String(airActual) + ",\"restoring\":" + String(airRestoring ? "true" : "false") +
    ",\"switching\":" + String(airSwitching ? "true" : "false") +
    ",\"override\":" + String(airSchedule.overrideActive ? "true" : "false") +
    ",\"overrideUntil\":" + String((long long)airSchedule.overrideUntil) +
    ",\"simReady\":" + String(airSimReady ? "true" : "false") +
    ",\"registered\":" + String(airRegistered ? "true" : "false") +
    ",\"attached\":" + String(airAttached ? "true" : "false") +
    ",\"error\":\"" + jsonEscape(airError) + "\"}";
  server.send(200, "application/json", json);
}
void handleAirplaneSave() {
  if (!checkAuth()) return;
  const char* names[] = {"enabled", "start", "end"};
  int start, end;
  String enabled = server.arg("enabled");
  if (server.method() != HTTP_POST || !uniqueAirArgs(names, 3) ||
      (enabled != "true" && enabled != "false") || !parseAirTime(server.arg("start"), start) ||
      !parseAirTime(server.arg("end"), end) || !AirSchedule::validTime(start, end)) {
    server.send(400, "application/json", "{\"success\":false,\"message\":\"飞行时段参数无效或重复\"}"); return;
  }
  updateAirTarget();
  if (enabled == "false" && config.airplaneEnabled) {
    if (!airSchedule.targetKnown && airActual < 0) {
      server.send(409, "application/json", "{\"success\":false,\"message\":\"目标未知，请先手动指定状态\"}"); return;
    }
    config.airplaneManualTarget = airSchedule.targetKnown ? airSchedule.target : airActual == 4;
  }
  config.airplaneEnabled = enabled == "true";
  config.airplaneStartMinute = start; config.airplaneEndMinute = end;
  airSchedule.clearOverride(); updateAirTarget(); airAttempted = false;
  saveConfig();
  server.send(202, "application/json", "{\"success\":true,\"message\":\"规则已保存，等待模组确认\"}");
}
void handleAirplaneAction() {
  if (!checkAuth()) return;
  const char* names[] = {"action"};
  String action = server.arg("action");
  if (server.method() != HTTP_POST || !uniqueAirArgs(names, 1) ||
      (action != "on" && action != "off" && action != "auto") ||
      (action == "auto" && !config.airplaneEnabled)) {
    server.send(400, "application/json", "{\"success\":false,\"message\":\"飞行动作无效\"}"); return;
  }
  if (action == "auto") airSchedule.clearOverride();
  else {
    airSchedule.manual(action == "on", config.airplaneEnabled, config.airplaneStartMinute,
      config.airplaneEndMinute, time(nullptr));
    if (!config.airplaneEnabled) { config.airplaneManualTarget = action == "on"; saveConfig(); }
  }
  updateAirTarget(); airAttempted = false;
  server.send(202, "application/json", "{\"success\":true,\"message\":\"动作已接受，等待回读确认\"}");
}
