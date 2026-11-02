// 同型号SDK: +MSTK:11,"<BER-TLV>"为主动命令；仅缓存SET UP MENU，绝不应答或选菜单。
static StkMenu stkMenu;
static unsigned long stkEvents = 0, stkMalformed = 0;
static int stkNibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}
static bool stkLength(const unsigned char* bytes, unsigned int end, unsigned int& pos, unsigned int& len) {
  if (pos >= end) return false;
  len = bytes[pos++];
  if (len == 0x81) { if (pos >= end) return false; len = bytes[pos++]; }
  else if (len >= 0x80) return false;
  return len <= end - pos;
}
static String stkText(const unsigned char* bytes, unsigned int len) {
  String text;
  if (len && (bytes[0] == 0x81 || bytes[0] == 0x82)) return "[暂不支持的压缩UCS2]";
  if (len && bytes[0] == 0x80) {
    if (len % 2 != 1) return "[无效UCS2]";
    for (unsigned int i = 1; i + 1 < len; i += 2) {
      unsigned int cp = (bytes[i] << 8) | bytes[i + 1];
      if (cp < 32 || (cp >= 0xd800 && cp <= 0xdfff)) text += '?';
      else if (cp < 0x80) text += (char)cp;
      else if (cp < 0x800) { text += (char)(0xc0 | (cp >> 6)); text += (char)(0x80 | (cp & 63)); }
      else { text += (char)(0xe0 | (cp >> 12)); text += (char)(0x80 | ((cp >> 6) & 63)); text += (char)(0x80 | (cp & 63)); }
    }
  } else {
    // GSM字母表的特殊字符及压缩UCS2暂不猜译，保留可读ASCII。
    for (unsigned int i = 0; i < len; ++i) {
      unsigned char c = bytes[i];
      text += (char)((c >= 32 && c <= 126 && c != 0x24 && c != 0x40 && c != 0x5b && c != 0x5c && c != 0x5d && c != 0x5e && c != 0x5f && c != 0x60 && c != 0x7b && c != 0x7c && c != 0x7d && c != 0x7e) ? c : '?');
    }
  }
  return text;
}
static bool parseStkMenu(const String& hex, StkMenu& menu) {
  if (!hex.length() || hex.length() % 2 || hex.length() > 480) return false;
  unsigned char bytes[240]; unsigned int end = hex.length() / 2;
  for (unsigned int i = 0; i < end; ++i) {
    int hi = stkNibble(hex[i * 2]), lo = stkNibble(hex[i * 2 + 1]);
    if (hi < 0 || lo < 0) return false;
    bytes[i] = (hi << 4) | lo;
  }
  unsigned int pos = 1, len;
  if (bytes[0] != 0xd0 || !stkLength(bytes, end, pos, len) || pos + len != end) return false;
  bool command = false, device = false, remove = false;
  while (pos < end) {
    unsigned int tag = bytes[pos++] & 0x7f;
    if (tag == 0x7f || !stkLength(bytes, end, pos, len)) return false;
    if (tag == 1) {
      if (command || len != 3 || bytes[pos + 1] != 0x25) return false;
      command = true;
    } else if (tag == 2) {
      if (device || len != 2 || bytes[pos] != 0x81 || bytes[pos + 1] != 0x82) return false;
      device = true;
    } else if (tag == 5) {
      if (len > 64) return false;
      menu.title = stkText(bytes + pos, len);
    } else if (tag == 15) {
      if (len == 0) { remove = true; }
      else {
        if (len > 65 || menu.count >= 16 || !bytes[pos]) return false;
        for (unsigned int i = 0; i < menu.count; ++i) if (menu.ids[i] == bytes[pos]) return false;
        menu.ids[menu.count] = bytes[pos];
        menu.items[menu.count++] = stkText(bytes + pos + 1, len - 1);
      }
    }
    pos += len;
  }
  if (!command || !device || (remove && menu.count)) return false;
  menu.available = menu.count > 0;
  return true;
}
void observeStkUrc(const String& line) {
  if (!line.startsWith("+MSTK:")) return;
  String value = line.substring(6); value.trim();
  int comma = value.indexOf(',');
  if (comma < 0) return;
  String type = value.substring(0, comma); type.trim();
  if (type != "11") return;
  ++stkEvents;
  String raw = value.substring(comma + 1); raw.trim();
  if (raw.length() < 2 || !raw.startsWith("\"") || !raw.endsWith("\"")) { ++stkMalformed; return; }
  StkMenu candidate;
  if (parseStkMenu(raw.substring(1, raw.length() - 1), candidate)) stkMenu = candidate;
  else ++stkMalformed;
}
void handleStkMenu() {
  if (!checkAuth()) return;
  server.send(200, "application/json", "{\"available\":" + String(stkMenu.available ? "true" : "false") +
    ",\"events\":" + String(stkEvents) + ",\"unparsed\":" + String(stkMalformed) +
    ",\"title\":\"" + jsonEscape(stkMenu.title) + "\",\"items\":" + [&]() {
      String items = "[";
      for (unsigned int i = 0; i < stkMenu.count; ++i) {
        if (i) items += ',';
        items += "{\"id\":" + String(stkMenu.ids[i]) + ",\"text\":\"" + jsonEscape(stkMenu.items[i]) + "\"}";
      }
      return items + "]";
    }() + ",\"message\":\"仅展示本次开机后被动接收的菜单；未收到不代表SIM不支持。未启用通知、下载Profile、释放缓存或选择菜单。\"}");
}

// 仅识别命令名；名称出现不证明存在可用菜单。
static bool stkCommandName(const String& name) {
  return name == "CSIM" || name == "CRSM" || name.startsWith("CUSAT") ||
    name.startsWith("STK") || name.startsWith("SAT") || name.startsWith("USAT") ||
    name.endsWith("STK") || name.endsWith("USAT");
}
static bool stkTokenChar(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}
static void observeStkLine(const String& line, bool overflow, void* context) {
  StkProbe& probe = *static_cast<StkProbe*>(context);
  if (overflow) { ++probe.lines; probe.truncated = true; return; }
  ++probe.lines;
  if (probe.collectRaw) {
    unsigned int room = 2048 - probe.raw.length();
    if (line.length() + 1 > room) probe.truncated = true;
    if (room) probe.raw += (line + "\n").substring(0, room);
  }
  if (line == "OK" || atErrorLine(line)) {
    probe.terminated = true; probe.error = line != "OK"; return;
  }
  for (unsigned int i = 0; i < line.length();) {
    // CLAC 可以逐行或用分隔符列出 AT+NAME / +NAME / NAME。
    if (!(stkTokenChar(line[i]) || line[i] == '+')) { ++i; continue; }
    unsigned int begin = i;
    while (i < line.length() && (stkTokenChar(line[i]) || line[i] == '+')) ++i;
    String token = line.substring(begin, i);
    if (token.startsWith("AT+")) token = token.substring(3);
    else if (token.startsWith("+")) token = token.substring(1);
    if (!stkCommandName(token)) continue;
    String candidate = "AT+" + token;
    if (candidate.length() > 80) { candidate = candidate.substring(0, 80); probe.truncated = true; }
    bool duplicate = false;
    for (unsigned int n = 0; n < probe.count; ++n) if (probe.candidates[n] == candidate) duplicate = true;
    if (duplicate) continue;
    if (probe.count == 32) { probe.truncated = true; continue; }
    probe.candidates[probe.count++] = candidate;
  }
}
void handleStkProbe() {
  if (!checkAuth()) return;
  if (server.method() != HTTP_POST) {
    server.send(405, "application/json", "{\"message\":\"仅接受POST\"}"); return;
  }
  String op = server.arg("op");
  if (server.args() != 1 || server.argName(0) != "op" ||
      (op != "model" && op != "commands" && op != "sim_access" && op != "mstk_test" && op != "profile")) {
    server.send(400, "application/json", "{\"message\":\"仅接受一个固定op参数\"}"); return;
  }
  if (modemDiagnosticSwitching()) {
    server.send(409, "application/json", "{\"message\":\"模组正在切换\"}"); return;
  }
  ATTransaction transaction;
  if (!transaction) {
    server.send(409, "application/json", "{\"message\":\"模组事务忙碌\"}"); return;
  }
  const char* command = op == "model" ? "ATI" : op == "commands" ? "AT+CLAC" :
    op == "mstk_test" ? "AT+MSTK=?" : op == "profile" ? "AT+MSTK=3" : "AT+CSIM=?";
  StkProbe probe;
  probe.collectRaw = op != "commands";
  transactionATCommandObserved(command, op == "commands" ? 6000 : 3000, observeStkLine, &probe);
  bool timeout = !probe.terminated;
  // 原始完整CLAC不返回；型号和测试响应最多保留2048字节。
  String candidates = "[";
  for (unsigned int i = 0; i < probe.count; ++i) {
    if (i) candidates += ',';
    candidates += "\"" + jsonEscape(probe.candidates[i]) + "\"";
  }
  candidates += ']';
  const char* evidence = probe.error ? "error" : timeout || probe.truncated ? "incomplete" :
    probe.count ? "advertised" : "not advertised";
  String json = "{\"op\":\"" + op + "\",\"command\":\"" + command +
    "\",\"terminated\":" + String(probe.terminated ? "true" : "false") +
    ",\"timeout\":" + String(timeout ? "true" : "false") +
    ",\"truncated\":" + String(probe.truncated ? "true" : "false") +
    ",\"error\":" + String(probe.error ? "true" : "false") +
    ",\"lines\":" + String(probe.lines) + ",\"candidates\":" + candidates +
    ",\"evidence\":\"" + String(op == "commands" ? evidence : "diagnostic only") +
    "\",\"raw\":\"" + jsonEscape(probe.raw) +
    "\",\"message\":\"" + String(op == "mstk_test" && probe.error ?
      "MSTK参数测试形式不支持；不能据此判断命令不存在。" : "") +
    "仅只读诊断；未列出不代表不支持，命令名或参数测试成功不证明STK菜单可用。未执行APDU、启用、菜单选择、envelope或Profile下载。\"}";
  server.send(200, "application/json", json);
}
