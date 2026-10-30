bool checkAuth() {
  if (!server.authenticate(config.webUser.c_str(), config.webPass.c_str())) {
    server.requestAuthentication(BASIC_AUTH, "SMS Forwarding", "请输入管理员账号密码"); return false;
  }
  return true;
}
static bool atBusy = false;
static bool atTimedOut = false;
bool modemTransactionBusy() { return atBusy; }
void markATTimeout() { atTimedOut = true; }
ATTransaction::ATTransaction() : acquired(!atBusy) { if (acquired) atBusy = true; }
ATTransaction::~ATTransaction() { if (acquired) atBusy = false; }
bool atResponseOK(const String& response) { return response.endsWith("\nOK\n"); }
bool atErrorLine(const String& line) {
  return line == "ERROR" || line.startsWith("+CME ERROR:") || line.startsWith("+CMS ERROR:");
}
String waitATResponse(unsigned long timeout, bool prompt, bool ping) {
  const unsigned int MAX_RESPONSE = 2048;
  unsigned long start = millis();
  String response = "\n";
  while ((unsigned long)(millis() - start) < timeout) {
    String line = readModemLine(prompt);
    if (line.length() && !dispatchModemURC(line)) {
      bool terminal = atErrorLine(line) || (prompt ? line == ">" : (ping ? line.startsWith("+MPING:") : line == "OK"));
      if (response.length() + line.length() + 1 > MAX_RESPONSE && terminal) response = "\n";
      if (response.length() + line.length() + 1 <= MAX_RESPONSE) { response += line; response += '\n'; }
      if (terminal) return response;
    }
    delay(1);
  }
  markATTimeout(); return response;
}
String transactionATCommand(const char* cmd, unsigned long timeout) {
  if (!atBusy || !cmd) return "\nERROR\n";
  if (atTimedOut) {
    unsigned long start = millis();
    while ((unsigned long)(millis() - start) < 250) {
      String line = readModemLine(false);
      if (line.length()) dispatchModemURC(line);
      delay(1);
    }
    // 使用带专属正文的响应重新建立边界；通用OK不能单独证明同步成功。
    Serial1.println("AT+CFUN?");
    String sync = waitATResponse(1500, false, false);
    if (!atResponseOK(sync) || sync.indexOf("\n+CFUN:") < 0) return "\nERROR\n";
    atTimedOut = false;
  }
  unsigned long start = millis();
  for (int i = 0; i < 32 && Serial1.available() && (unsigned long)(millis() - start) < 20; ++i) {
    String line = readModemLine(false);
    if (line.length()) dispatchModemURC(line);
    yield();
  }
  Serial1.println(cmd);
  return waitATResponse(timeout, String(cmd).startsWith("AT+CMGS="), String(cmd).startsWith("AT+MPING="));
}
String sendATCommand(const char* cmd, unsigned long timeout) {
  ATTransaction transaction;
  if (!transaction) return "\nERROR\n";
  return transactionATCommand(cmd, timeout);
}
bool sendATandWaitOK(const char* cmd, unsigned long timeout) { return atResponseOK(sendATCommand(cmd, timeout)); }
bool waitCGATT1() { return sendATCommand("AT+CGATT?", 2000).indexOf("+CGATT: 1\n") >= 0; }
bool validPingHost(const String& host) {
  if (!host.length() || host.length() > 253) return false;
  for (unsigned int i = 0; i < host.length(); ++i) {
    char c = host[i];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '-')) return false;
  }
  return true;
}
bool runModemPing(const String& host, String& result) {
  if (!validPingHost(host)) { result = "无效的Ping目标"; return false; }
  if (!cellularReady()) { result = cellularBlockReason(); return false; }
  ATTransaction transaction;
  if (!transaction) { result = "模组事务忙碌"; return false; }
  if (!atResponseOK(transactionATCommand("AT+CGACT=1,1", 3000))) {
    transactionATCommand("AT+CGACT=0,1", 2000);
    result = "数据连接激活失败"; return false;
  }
  String response = transactionATCommand(("AT+MPING=\"" + host + "\",8,1").c_str(), 10000);
  bool cleaned = atResponseOK(transactionATCommand("AT+CGACT=0,1", 2000));
  String dataState = transactionATCommand("AT+CGACT?", 1000);
  cleaned = cleaned && atResponseOK(dataState) && dataState.indexOf("\n+CGACT: 1,0\n") >= 0;
  if (!cleaned) { result = "数据连接清理失败"; return false; }
  int pos = response.indexOf("+MPING:");
  if (pos < 0) { result = "未收到Ping结果"; return false; }
  result = response.substring(pos + 7, response.indexOf('\n', pos)); result.trim();
  int comma = result.indexOf(',');
  String status = comma >= 0 ? result.substring(0, comma) : result;
  status.trim(); return status == "0" || status == "1";
}
// 重启模组不用于飞行切换。
void blink_short(unsigned long gap_time) {
  digitalWrite(LED_BUILTIN, LOW); delay(50); digitalWrite(LED_BUILTIN, HIGH); delay(gap_time);
}

String htmlEscape(const String& text) {
  String out;
  for (unsigned int i = 0; i < text.length(); ++i) {
    char c = text[i];
    if (c == '&') out += "&amp;";
    else if (c == '<') out += "&lt;";
    else if (c == '>') out += "&gt;";
    else if (c == '"') out += "&quot;";
    else if (c == '\'') out += "&#39;";
    else out += c;
  }
  return out;
}
static void streamPageConfig() {
  server.sendContent("  <details open>\n    <summary>WiFi 网络 <span class=\"card-sub\">自动选择信号最强的</span></summary>\n    <div class=\"det-body\">\n      ");
  for (int i = 0; i < MAX_WIFI_NETWORKS; i++) {
    String idx = String(i);
    String enabledSw = config.wifiNetworks[i].enabled ? "on" : "";
    String enabledVal = config.wifiNetworks[i].enabled ? "true" : "false";
    String currentSsid = WiFi.SSID();
    bool isCurrent = config.wifiNetworks[i].ssid.length() > 0 && config.wifiNetworks[i].ssid == currentSsid;
    
    server.sendContent("<div style=\"padding:10px;background:#f8fafc;border-radius:8px;margin-bottom:8px;border:1px solid " + String(isCurrent ? "var(--success)" : "#e2e8f0") + "\">");
    server.sendContent("<div class=\"sw-row\" onclick=\"wfTog(" + idx + ")\">");
    server.sendContent("<span style=\"font-weight:600\">网络 " + String(i + 1));
    if (isCurrent) server.sendContent(" <span class=\"badge b-ok\">当前</span>");
    server.sendContent("</span>");
    server.sendContent("<div id=\"wfs" + idx + "\" class=\"sw " + enabledSw + "\"></div>");
    server.sendContent("<input type=\"hidden\" id=\"wfe" + idx + "\" name=\"wifi" + idx + "en\" value=\"" + enabledVal + "\">");
    server.sendContent("</div>");
    server.sendContent("<div class=\"grid-2\" style=\"margin-top:8px\">");
    server.sendContent("<div class=\"fg\" style=\"margin-bottom:0\"><label>SSID</label><input name=\"wifi" + idx + "ssid\" value=\"" + htmlEscape(config.wifiNetworks[i].ssid) + "\" placeholder=\"WiFi名称\"></div>");
    // 密码不回显明文，使用占位符提示
    String passPlaceholder = config.wifiNetworks[i].password.length() > 0 ? "（已保存，留空则保留）" : "WiFi密码";
    server.sendContent("<div class=\"fg\" style=\"margin-bottom:0\"><label>密码</label><input name=\"wifi" + idx + "pass\" type=\"password\" placeholder=\"" + passPlaceholder + "\"></div>");
    server.sendContent("</div></div>");
  }
  server.sendContent("\n      <div style=\"font-size:0.85em;color:var(--text-light);margin-top:8px\">\n        可配置多个WiFi，设备会自动连接信号最强的网络。修改后需重启生效。\n      </div>\n    </div>\n  </details>\n\n  <details>\n    <summary>Web 管理 & 邮箱</summary>\n    <div class=\"det-body\">\n      <div class=\"grid-2\">\n        <div class=\"fg\"><label>Web账号</label><input name=\"webUser\" value=\"");
  { String value=config.webUser; for(unsigned int j=0;j<value.length();j+=128) server.sendContent(htmlEscape(value.substring(j,j+128))); }
  server.sendContent("\"></div>\n        <div class=\"fg\"><label>Web密码</label><input name=\"webPass\" type=\"password\" placeholder=\"留空保留\" value=\"\"></div>\n      </div>\n      <div class=\"sw-row\" onclick=\"xToggle('smEn')\">\n         <span>启用 SMTP 邮件推送</span>\n         <div id=\"smEnSw\" class=\"sw ");
  { String value=String(config.emailEnabled ? "on" : ""); for(unsigned int j=0;j<value.length();j+=128) server.sendContent(htmlEscape(value.substring(j,j+128))); }
  server.sendContent("\"></div>\n         <input type=\"hidden\" id=\"smEn\" name=\"smtpEn\" value=\"");
  { String value=String(config.emailEnabled ? "true" : "false"); for(unsigned int j=0;j<value.length();j+=128) server.sendContent(htmlEscape(value.substring(j,j+128))); }
  server.sendContent("\">\n      </div>\n      <div id=\"smBox\" style=\"display:");
  { String value=String(config.emailEnabled ? "block" : "none"); for(unsigned int j=0;j<value.length();j+=128) server.sendContent(htmlEscape(value.substring(j,j+128))); }
  server.sendContent("\">\n        <div class=\"fg\"><label>SMTP 服务器</label><input name=\"smtpServer\" value=\"");
  { String value=config.smtpServer; for(unsigned int j=0;j<value.length();j+=128) server.sendContent(htmlEscape(value.substring(j,j+128))); }
  server.sendContent("\"></div>\n        <div class=\"grid-2\">\n           <div class=\"fg\"><label>端口</label><input name=\"smtpPort\" type=\"number\" value=\"");
  { String value=String(config.smtpPort); for(unsigned int j=0;j<value.length();j+=128) server.sendContent(htmlEscape(value.substring(j,j+128))); }
  server.sendContent("\"></div>\n           <div class=\"fg\"><label>发件账号</label><input name=\"smtpUser\" value=\"");
  { String value=config.smtpUser; for(unsigned int j=0;j<value.length();j+=128) server.sendContent(htmlEscape(value.substring(j,j+128))); }
  server.sendContent("\"></div>\n        </div>\n        <div class=\"fg\"><label>授权码/密码</label><input name=\"smtpPass\" type=\"password\" placeholder=\"留空保留\" value=\"\"></div>\n        <div class=\"fg\"><label>接收邮箱</label><input name=\"smtpSendTo\" value=\"");
  { String value=config.smtpSendTo; for(unsigned int j=0;j<value.length();j+=128) server.sendContent(htmlEscape(value.substring(j,j+128))); }
  server.sendContent("\"></div>\n      </div>\n    </div>\n  </details>\n\n<details>\n    <summary>Web 通道</summary>\n    <div class=\"det-body\">\n      ");
  for (int i = 0; i < MAX_PUSH_CHANNELS; i++) {
    String idx = String(i);
    String enabledSw = config.pushChannels[i].enabled ? "on" : "";
    String enabledVal = config.pushChannels[i].enabled ? "true" : "false";
    
    // 动态生成折叠卡片（虽然放在 details 下，但每个通道本身也是个小块）
    // 为了美观，这里每个通道用一个带开关的面板
    server.sendContent("<div class=\"card\" style=\"border:1px solid #e2e8f0;padding:12px;margin-bottom:8px;box-shadow:none\">");
    
    // 头部开关行
    server.sendContent("<div class=\"sw-row\" onclick=\"chTog(" + idx + ")\">");
    server.sendContent("<span style=\"font-weight:600\">通道 " + String(i + 1) + "</span>");
    server.sendContent("<div id=\"chs" + idx + "\" class=\"sw " + enabledSw + "\"></div>");
    server.sendContent("<input type=\"hidden\" id=\"che" + idx + "\" name=\"push" + idx + "en\" value=\"" + enabledVal + "\">");
    server.sendContent("</div>");

    // 详情区域 (点击名称展开/点击折叠按钮? 简化处理，直接显示)
    // 这里使用一个简单的折叠按钮
    server.sendContent("<div style=\"font-size:0.85em;color:var(--primary);text-align:right;cursor:pointer\" onclick=\"fd(" + idx + ")\">展开/收起 <span id=\"chi" + idx + "\" style=\"display:inline-block;transition:.2s\">></span></div>");
    
    server.sendContent("<div id=\"chb" + idx + "\" style=\"display:none;margin-top:12px;border-top:1px solid #f1f5f9;padding-top:12px\">");
    
    server.sendContent("<div class=\"fg\"><label>名称</label><input name=\"push" + idx + "name\" value=\"" + "");
    for(unsigned int j=0;j<config.pushChannels[i].name.length();j+=128) server.sendContent(htmlEscape(config.pushChannels[i].name.substring(j,j+128)));
    server.sendContent("\"></div>");
    
    server.sendContent("<div class=\"fg\"><label>类型</label>");
    server.sendContent("<select name=\"push" + idx + "type\" id=\"tp" + idx + "\" onchange=\"upd(" + idx + ")\">");
    server.sendContent("<option value=\"1\"" + String(config.pushChannels[i].type == PUSH_TYPE_POST_JSON ? " selected" : "") + ">POST JSON</option>");
    server.sendContent("<option value=\"2\"" + String(config.pushChannels[i].type == PUSH_TYPE_BARK ? " selected" : "") + ">Bark</option>");
    server.sendContent("<option value=\"3\"" + String(config.pushChannels[i].type == PUSH_TYPE_GET ? " selected" : "") + ">GET请求</option>");
    server.sendContent("<option value=\"4\"" + String(config.pushChannels[i].type == PUSH_TYPE_CUSTOM ? " selected" : "") + ">自定义模板</option>");
    server.sendContent("<option value=\"8\"" + String((int)config.pushChannels[i].type == 8 ? " selected" : "") + ">飞书群聊机器人</option></select></div>");
    
    server.sendContent("<div class=\"fg\"><label>URL</label><input name=\"push" + idx + "url\" value=\"" + "");
    for(unsigned int j=0;j<config.pushChannels[i].url.length();j+=128) server.sendContent(htmlEscape(config.pushChannels[i].url.substring(j,j+128)));
    server.sendContent("\" placeholder=\"https://...\"></div>");
    
    // 仅飞书使用加签密钥。
    bool showK1 = config.pushChannels[i].type == PUSH_TYPE_FEISHU;
    String k1Label = "加签密钥 (可选)";
    String k1Hint = "飞书机器人签名校验密钥（可选）";
    server.sendContent("<div id=\"k1" + idx + "\" style=\"display:" + (showK1 ? "block" : "none") + "\"><div class=\"fg\"><label id=\"k1l" + idx + "\">" + k1Label + "</label><input name=\"push" + idx + "k1\" value=\"" + "");
    for(unsigned int j=0;j<config.pushChannels[i].key1.length();j+=128) server.sendContent(htmlEscape(config.pushChannels[i].key1.substring(j,j+128)));
    server.sendContent("\" placeholder=\"" + k1Hint + "\"></div></div>");
    
    // 自定义模板 Body (显示条件: type == 4)
    server.sendContent("<div id=\"cf" + idx + "\" style=\"display:" + (config.pushChannels[i].type == PUSH_TYPE_CUSTOM ? "block" : "none") + "\"><div class=\"fg\"><label>Body模板</label><textarea name=\"push" + idx + "body\" rows=\"3\">" + "");
    for (unsigned int j=0;j<config.pushChannels[i].customBody.length();j+=128) server.sendContent(htmlEscape(config.pushChannels[i].customBody.substring(j,j+128)));
    server.sendContent("</textarea></div></div>");
    
    server.sendContent("</div></div>");
  }
  server.sendContent("\n    </div>\n  </details>\n\n\n\n  <details>\n    <summary>定时任务 <span id=\"tmBadge\" class=\"badge b-wait\" style=\"margin-left:8px\">未启用</span></summary>\n    <div class=\"det-body\">\n      <div class=\"sw-row\" onclick=\"xToggle('tmEn');updTmInfo()\">\n         <span>启用任务</span>\n         <div id=\"tmEnSw\" class=\"sw ");
  { String value=String(config.timerEnabled ? "on" : ""); for(unsigned int j=0;j<value.length();j+=128) server.sendContent(htmlEscape(value.substring(j,j+128))); }
  server.sendContent("\"></div>\n         <input type=\"hidden\" id=\"tmEn\" name=\"timerEn\" value=\"");
  { String value=String(config.timerEnabled ? "true" : "false"); for(unsigned int j=0;j<value.length();j+=128) server.sendContent(htmlEscape(value.substring(j,j+128))); }
  server.sendContent("\">\n      </div>\n      <div class=\"fg\"><label>类型 & 间隔(天)</label>\n        <div class=\"grid-2\">\n          <select id=\"tmType\" name=\"timerType\" onchange=\"$('tmSms').style.display=this.value==1?'block':'none';updTmInfo()\"><option value=\"0\" ");
  { String value=String(config.timerType == 0 ? "selected" : ""); for(unsigned int j=0;j<value.length();j+=128) server.sendContent(htmlEscape(value.substring(j,j+128))); }
  server.sendContent(">Ping保活</option><option value=\"1\" ");
  { String value=String(config.timerType == 1 ? "selected" : ""); for(unsigned int j=0;j<value.length();j+=128) server.sendContent(htmlEscape(value.substring(j,j+128))); }
  server.sendContent(">发送短信</option></select>\n          <input value=\"");
  { String value=String(config.timerInterval); for(unsigned int j=0;j<value.length();j+=128) server.sendContent(htmlEscape(value.substring(j,j+128))); }
  server.sendContent("\" type=\"number\" id=\"tmInt\" name=\"timerInterval\" min=\"1\" onchange=\"updTmInfo()\">\n        </div>\n      </div>\n      <div id=\"tmSms\" style=\"display:");
  { String value=String(config.timerType == 1 ? "block" : "none"); for(unsigned int j=0;j<value.length();j+=128) server.sendContent(htmlEscape(value.substring(j,j+128))); }
  server.sendContent("\">\n        <div class=\"fg\"><label>对方号码</label><input value=\"");
  { String value=config.timerPhone; for(unsigned int j=0;j<value.length();j+=128) server.sendContent(htmlEscape(value.substring(j,j+128))); }
  server.sendContent("\" id=\"tmPh\" name=\"timerPhone\"></div>\n        <div class=\"fg\"><label>短信内容</label><input value=\"");
  { String value=config.timerMessage; for(unsigned int j=0;j<value.length();j+=128) server.sendContent(htmlEscape(value.substring(j,j+128))); }
  server.sendContent("\" id=\"tmMsg\" name=\"timerMessage\"></div>\n      </div>\n      <div style=\"background:#f0f9ff;border:1px solid #bae6fd;border-radius:8px;padding:12px;margin-bottom:12px\" id=\"tmInfo\"></div>\n      <button type=\"button\" class=\"btn btn-w\" onclick=\"var d=new URLSearchParams();['timerEn','timerType','timerInterval','timerPhone','timerMessage'].forEach(function(k){d.set(k,document.querySelector('[name='+k+']').value)});post('/timer',d)\">仅保存任务设置</button>\n    </div>\n  </details>\n\n");
}
void handleAppCss() {
  server.sendHeader("Cache-Control", server.arg("v") == APP_CSS_VERSION ? "public, max-age=31536000, immutable" : "no-cache");
  server.sendHeader("X-Content-Type-Options", "nosniff");
  server.setContentLength(sizeof(commonCss) - 1);
  server.send(200, "text/css; charset=utf-8", "");
  for (size_t offset = 0; offset < sizeof(commonCss) - 1; offset += 1024) {
    size_t remaining = sizeof(commonCss) - 1 - offset;
    server.sendContent_P(commonCss + offset, remaining < 1024 ? remaining : 1024);
    yield();
  }
}
void handleRoot() {
  if (!checkAuth()) return;
  server.sendHeader("Cache-Control", "no-store");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN); server.send(200, "text/html; charset=utf-8", "");
  char buffer[1024]; size_t n = 0;
  const char* cursor = htmlPage;
  while (*cursor) {
    if (strncmp(cursor, "%CSS_VERSION%", 13) == 0) {
      if (n) { server.sendContent(buffer, n); n = 0; }
      server.sendContent(APP_CSS_VERSION); cursor += 13;
    } else if (strncmp(cursor, "%IP%", 4) == 0) {
      if (n) { server.sendContent(buffer, n); n = 0; }
      server.sendContent(htmlEscape(WiFi.localIP().toString())); cursor += 4;
    } else if (strncmp(cursor, "%CONFIG%", 8) == 0) {
      if (n) { server.sendContent(buffer, n); n = 0; }
      streamPageConfig(); cursor += 8;
    } else {
      buffer[n++] = *cursor++;
      if (n == sizeof(buffer)) { server.sendContent(buffer, n); n = 0; yield(); }
    }
  }
  if (n) server.sendContent(buffer, n);
  server.sendContent("");
}
void handleToolsPage() {
  if (!checkAuth()) return;
  server.sendHeader("Location", "/", true); server.send(302, "text/plain", "");
}
#include <memory>
#include <new>
// 仅接受有界的扁平对象；字段数组和配置快照放堆上，避免占用 loop 栈。
struct ConfigArg { String name, value; char type; };
struct ConfigRequest {
  std::unique_ptr<ConfigArg[]> fields{new (std::nothrow) ConfigArg[64]};
  unsigned count = 0;
  bool add(const String& key, const String& value, char type) {
    if (!fields || count == 64 || !key.length() || key.length() > 32 || value.length() > 4096) return false;
    for (unsigned i = 0; i < count; ++i) if (fields[i].name == key) return false;
    fields[count++] = {key, value, type}; return true;
  }
};
class ConfigJsonReader {
  const String& s; unsigned p = 0;
  void ws() { while (p < s.length() && (s[p]==' ' || s[p]=='\n' || s[p]=='\r' || s[p]=='\t')) ++p; }
  bool hex(unsigned& n) {
    n = 0;
    for (int i=0;i<4;++i) {
      if (p == s.length()) return false;
      char c=s[p++]; int d=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;
      if (d<0) return false;
      n=n*16+d;
    }
    return true;
  }
  bool text(String& out) {
    if (p == s.length() || s[p++] != '"') return false;
    while (p < s.length()) {
      unsigned char c=s[p++];
      if (c=='"') return true;
      if (c<32) return false;
      if (c=='\\') {
        if (p==s.length()) return false;
        c=s[p++];
        if (c=='u') {
          unsigned n; if (!hex(n) || !n) return false;
          if (n>=0xd800 && n<=0xdbff) {
            if (p+2>s.length() || s[p++]!='\\' || s[p++]!='u') return false;
            unsigned low; if (!hex(low) || low<0xdc00 || low>0xdfff) return false;
            n=0x10000+((n-0xd800)<<10)+(low-0xdc00);
          } else if (n>=0xdc00 && n<=0xdfff) return false;
          if (n<0x80) out+=(char)n;
          else if (n<0x800) { out+=(char)(0xc0|(n>>6)); out+=(char)(0x80|(n&63)); }
          else if (n<0x10000) { out+=(char)(0xe0|(n>>12)); out+=(char)(0x80|((n>>6)&63)); out+=(char)(0x80|(n&63)); }
          else { out+=(char)(0xf0|(n>>18)); out+=(char)(0x80|((n>>12)&63)); out+=(char)(0x80|((n>>6)&63)); out+=(char)(0x80|(n&63)); }
          continue;
        }
        if (c=='n') c='\n'; else if (c=='r') c='\r'; else if (c=='t') c='\t';
        else if (c=='b') c='\b'; else if (c=='f') c='\f';
        else if (c!='"' && c!='\\' && c!='/') return false;
      }
      out+=(char)c;
    }
    return false;
  }
public:
  explicit ConfigJsonReader(const String& body):s(body) {}
  bool parse(ConfigRequest& req) {
    if (s.length()>8192) return false;
    ws(); if (p==s.length() || s[p++]!='{') return false;
    ws(); if (p<s.length() && s[p]=='}') { ++p; ws(); return p==s.length(); }
    while (p<s.length()) {
      String key,value; if (!text(key)) return false;
      ws(); if (p==s.length() || s[p++]!=':') return false;
      ws(); char type='s';
      if (p<s.length() && s[p]=='"') { if (!text(value)) return false; }
      else {
        type='n'; unsigned start=p;
        while (p<s.length() && s[p]!=',' && s[p]!='}' && s[p]!=' ' && s[p]!='\t' && s[p]!='\n' && s[p]!='\r') ++p;
        value=s.substring(start,p);
        if (value=="true" || value=="false") type='b';
        else {
          if (!value.length() || (value.length()>1 && value[0]=='0')) return false;
          for (unsigned i=0;i<value.length();++i) if (value[i]<'0'||value[i]>'9') return false;
        }
      }
      if (!req.add(key,value,type)) return false;
      ws(); if (p==s.length()) return false;
      char end=s[p++]; ws();
      if (end=='}') return p==s.length();
      if (end!=',') return false;
    }
    return false;
  }
};
static bool readConfigRequest(ConfigRequest& req, bool allowJson) {
  if (server.hasArg("plain")) {
    if (!allowJson || server.args()!=1) return false;
    String body=server.arg("plain"); return ConfigJsonReader(body).parse(req);
  }
  size_t bytes=0;
  for (int i=0;i<server.args();++i) {
    String name=server.argName(i), value=server.arg(i); bytes+=name.length()+value.length();
    if (bytes>16384 || !req.add(name,value,'f')) return false;
  }
  return req.count>0;
}
static bool configPost() {
  if (!checkAuth()) return false;
  server.sendHeader("Cache-Control", "no-store");
  if (server.method()==HTTP_POST) return true;
  server.sendHeader("Allow", "POST"); server.send(405,"application/json","{\"success\":false,\"message\":\"请使用 POST\"}"); return false;
}
static void configBadRequest() { server.send(400,"application/json","{\"success\":false,\"message\":\"参数错误\"}"); }
static bool configMemoryAvailable(const void* allocation) {
  if (allocation) return true;
  server.send(503,"application/json","{\"success\":false,\"message\":\"内存不足，请稍后重试\"}"); return false;
}
static bool configNumber(const ConfigArg& a, int min, int max, int& out) {
  if (a.type!='f' && a.type!='n') return false;
  if (!a.value.length() || a.value.length()>5) return false;
  int n=0;
  for (unsigned i=0;i<a.value.length();++i) { char c=a.value[i]; if (c<'0'||c>'9') return false; n=n*10+c-'0'; }
  if (n<min || n>max) return false;
  out=n; return true;
}
static bool configBool(const ConfigArg& a, bool& out) {
  if ((a.type!='f' && a.type!='b') || (a.value!="true" && a.value!="false")) return false;
  out=a.value=="true"; return true;
}
static bool configText(const ConfigArg& a, String& out, unsigned max, bool retainEmpty=false) {
  if ((a.type!='f' && a.type!='s') || a.value.length()>max) return false;
  for (unsigned i=0;i<a.value.length();++i) if (!a.value[i]) return false;
  if (!retainEmpty || a.value.length()) out=a.value;
  return true;
}
static bool configPhone(const String& phone) {
  if (phone.length()>32) return false;
  for (unsigned i=0;i<phone.length();++i) if ((phone[i]<'0'||phone[i]>'9') && !(i==0 && phone[i]=='+' && phone.length()>1)) return false;
  return true;
}
static bool applyTimerArg(Config& next, ConfigArg a, bool legacy) {
  if (legacy) {
    if (a.name=="enabled") a.name="timerEn"; else if (a.name=="type") a.name="timerType";
    else if (a.name=="interval") a.name="timerInterval"; else if (a.name=="phone") a.name="timerPhone";
    else if (a.name=="message") a.name="timerMessage";
  }
  if (a.name=="timerEn") return configBool(a,next.timerEnabled);
  if (a.name=="timerType") return configNumber(a,0,1,next.timerType);
  if (a.name=="timerInterval") return configNumber(a,1,3650,next.timerInterval);
  if (a.name=="timerPhone") return configText(a,next.timerPhone,32) && configPhone(next.timerPhone);
  if (a.name=="timerMessage") return configText(a,next.timerMessage,1024);
  return false;
}
static bool timerValid(const Config& c) {
  return !c.timerEnabled || c.timerType==0 || (c.timerPhone.length() && c.timerMessage.length());
}
static void commitConfig(const Config& next) {
  bool changed=next.timerEnabled!=config.timerEnabled || next.timerType!=config.timerType ||
    next.timerInterval!=config.timerInterval || next.timerPhone!=config.timerPhone || next.timerMessage!=config.timerMessage;
  config=next;
  if (changed) { timerIntervalSec=(unsigned long)config.timerInterval*86400UL; timerPending=false; timerLastMs=esp_timer_get_time()/1000ULL; }
  saveConfig(); configValid=isConfigValid();
}
void handleSave() {
  if (!configPost()) return;
  ConfigRequest req;
  if (!configMemoryAvailable(req.fields.get())) return;
  if (!readConfigRequest(req,false)) { configBadRequest(); return; }
  std::unique_ptr<Config> snapshot(new (std::nothrow) Config(config));
  if (!configMemoryAvailable(snapshot.get())) return;
  Config& next=*snapshot;
  for (unsigned n=0;n<req.count;++n) {
    const ConfigArg& a=req.fields[n]; bool ok=false, known=true;
    if (a.name.startsWith("timer")) ok=applyTimerArg(next,a,false);
    else if (a.name=="webUser") ok=configText(a,next.webUser,64,true);
    else if (a.name=="webPass") ok=configText(a,next.webPass,128,true);
    else if (a.name=="smtpEn") ok=configBool(a,next.emailEnabled);
    else if (a.name=="smtpPort") ok=configNumber(a,1,65535,next.smtpPort);
    else if (a.name=="smtpServer") ok=configText(a,next.smtpServer,253);
    else if (a.name=="smtpUser") ok=configText(a,next.smtpUser,254);
    else if (a.name=="smtpPass") ok=configText(a,next.smtpPass,256,true);
    else if (a.name=="smtpSendTo") ok=configText(a,next.smtpSendTo,254);
    else known=false;
    for (int i=0;!known && i<MAX_WIFI_NETWORKS;++i) {
      String p="wifi"+String(i); WifiNetwork& w=next.wifiNetworks[i];
      if (a.name==p+"ssid") { known=true; ok=configText(a,w.ssid,32,true); }
      else if (a.name==p+"pass") { known=true; ok=configText(a,w.password,64,true); }
      else if (a.name==p+"en") { known=true; ok=configBool(a,w.enabled); }
    }
    for (int i=0;!known && i<MAX_PUSH_CHANNELS;++i) {
      String p="push"+String(i); PushChannel& ch=next.pushChannels[i];
      if (a.name==p+"en") { known=true; ok=configBool(a,ch.enabled); }
      else if (a.name==p+"type") { known=true; int type; ok=configNumber(a,0,8,type) && isPushTypeSupported(type); if(ok) ch.type=(PushType)type; }
      else if (a.name==p+"name") { known=true; ok=configText(a,ch.name,128); }
      else if (a.name==p+"url") { known=true; ok=configText(a,ch.url,2048); }
      else if (a.name==p+"k1") { known=true; ok=configText(a,ch.key1,512); }
      else if (a.name==p+"k2") { known=true; ok=configText(a,ch.key2,512); }
      else if (a.name==p+"body") { known=true; ok=configText(a,ch.customBody,4096); }
    }
    if (!known || !ok) { configBadRequest(); return; }
  }
  if (!timerValid(next)) { configBadRequest(); return; }
  commitConfig(next);
  server.send(200, "application/json", "{\"success\":true,\"message\":\"配置已保存，WiFi变更需重启\"}");
}
void handleSendSms() {
  if (!configPost()) return;
  ConfigRequest req; String phone,content;
  if (!configMemoryAvailable(req.fields.get())) return;
  if (!readConfigRequest(req,true) || req.count!=2) { configBadRequest(); return; }
  for (unsigned i=0;i<req.count;++i) {
    const ConfigArg& a=req.fields[i]; bool ok=false;
    if (a.name=="phone") ok=configText(a,phone,32) && configPhone(phone);
    else if (a.name=="content") ok=configText(a,content,1024);
    if (!ok) { configBadRequest(); return; }
  }
  if (!phone.length() || !content.length()) { configBadRequest(); return; }
  if (smsSendResultUnknown()) {
    server.send(409, "application/json", "{\"success\":false,\"unknown\":true,\"message\":\"短信发送结果未知，请勿重复发送；请人工核实后重启设备\"}"); return;
  }
  if (!cellularReady()) { server.send(409, "application/json", "{\"success\":false,\"unknown\":false,\"message\":\"" + jsonEscape(cellularBlockReason()) + "\"}"); return; }
  bool success = sendSMS(phone.c_str(), content.c_str());
  bool unknown = smsSendResultUnknown();
  success = success && !unknown;
  if (success) { ++stats.smsSent; saveStats(); }
  server.send(200, "application/json", "{\"success\":" + String(success ? "true" : "false") + ",\"unknown\":" + String(unknown ? "true" : "false") + ",\"message\":\"" + String(unknown ? "短信发送结果未知，请勿重复发送；请人工核实后重启设备" : success ? "短信发送成功" : "短信发送失败") + "\"}");
}
void handleTimer() {
  if (!configPost()) return;
  ConfigRequest req;
  if (!configMemoryAvailable(req.fields.get())) return;
  if (!readConfigRequest(req,true) || req.count!=5) { configBadRequest(); return; }
  bool legacy=server.hasArg("plain");
  std::unique_ptr<Config> snapshot(new (std::nothrow) Config(config));
  if (!configMemoryAvailable(snapshot.get())) return;
  Config& next=*snapshot;
  for (unsigned i=0;i<req.count;++i) {
    if (!applyTimerArg(next,req.fields[i],legacy)) { configBadRequest(); return; }
    // JSON 只接受原版字段，避免同一字段通过别名重复覆盖。
    if (legacy && req.fields[i].name.startsWith("timer")) { configBadRequest(); return; }
  }
  if (!timerValid(next)) { configBadRequest(); return; }
  commitConfig(next);
  server.send(200,"application/json","{\"success\":true,\"message\":\"任务已保存\",\"remain\":"+String(timerIntervalSec)+"}");
}
void handleStorageInit() {
  if (!configPost()) return;
  ConfigRequest req;
  if (!configMemoryAvailable(req.fields.get())) return;
  if (!readConfigRequest(req,false) || req.count!=1 || req.fields[0].name!="confirm" || req.fields[0].value!="ERASE_UNMOUNTED_STORAGE") { configBadRequest(); return; }
  if (!initializeSmsStorage(req.fields[0].value)) {
    server.send(smsStorageReady()?409:500,"application/json","{\"success\":false,\"message\":\"已挂载存储禁止初始化，或初始化失败\"}"); return;
  }
  server.send(200,"application/json","{\"success\":true,\"message\":\"存储已初始化\"}");
}
void handleRestart() {
  if (!checkAuth()) return;
  server.send(200, "application/json", "{\"success\":true,\"message\":\"设备将在2秒后重启\"}"); delay(2000); ESP.restart();
}
void handleSmsHistory() { if (checkAuth()) streamSmsHistory(); }
void handleClearSmsHistory() {
  if (!checkAuth()) return;
  server.sendHeader("Cache-Control", "no-store");
  if (server.method() != HTTP_POST) {
    server.sendHeader("Allow", "POST");
    server.send(405, "application/json", "{\"success\":false,\"message\":\"请使用 POST 清理历史\"}");
    return;
  }
  if (!smsStorageReady() || !clearSmsHistory()) {
    server.send(500, "application/json", "{\"success\":false,\"message\":\"短信历史清理失败，请重试\"}");
    return;
  }
  server.send(200, "application/json", "{\"success\":true,\"message\":\"已清空本机全部短信历史\"}");
}
void handleStats() {
  if (!checkAuth()) return;
  String json = "{\"received\":" + String(stats.smsReceived) + ",\"sent\":" + String(stats.smsSent) +
    ",\"pushOk\":" + String(stats.pushSuccess) + ",\"pushFail\":" + String(stats.pushFailed) +
    ",\"boots\":" + String(stats.bootCount) + ",\"uptime\":" + String((unsigned long long)(esp_timer_get_time() / 1000000ULL)) +
    ",\"minHeap\":" + String(ESP.getMinFreeHeap()) + ",\"largestBlock\":" + String(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)) +
    ",\"maxLoopMs\":" + String(maxLoopMs) + ",\"notificationQueue\":" + String(notificationQueueDepth()) +
    ",\"notificationDropped\":" + String(notificationDropCount()) + ",\"resetReason\":" + String((int)esp_reset_reason()) +
    ",\"storageReady\":" + String(smsStorageReady() ? "true" : "false") +
    ",\"pduDropped\":" + String(smsPduDropCount()) + ",\"smsResultUnknown\":" + String(smsSendResultUnknown() ? "true" : "false") +
    ",\"freeHeap\":" + String(ESP.getFreeHeap()) + ",\"wifiRssi\":" + String(WiFi.RSSI()) + "}";
  server.send(200, "application/json", json);
}
