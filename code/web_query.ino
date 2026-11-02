/*
 * web_query.ino - 模组查询和网络测试处理函数
 */

// 按完整响应行匹配，避免把URC或后续OK拼进字段。
static String queryResponseLine(const String& response, const char* prefix) {
  if (!atResponseOK(response)) return "";
  for (unsigned int start = 0; start < response.length();) {
    int end = response.indexOf('\n', start);
    if (end < 0) end = response.length();
    String line = response.substring(start, end); line.trim();
    if (line.startsWith(prefix)) return line;
    start = end + 1;
  }
  return "";
}
static bool operatorDigits(const String& value, unsigned int min, unsigned int max) {
  if (value.length() < min || value.length() > max) return false;
  for (unsigned int i = 0; i < value.length(); ++i)
    if (value[i] < '0' || value[i] > '9') return false;
  return true;
}
// COPS的oper允许逗号及空格，不能简单按逗号拆分。
static bool parseOperator(const String& response, int& format, String& name) {
  String line = queryResponseLine(response, "+COPS:");
  int mode = -1, used = 0; format = -1; name = "";
  if (sscanf(line.c_str(), "+COPS: %d , %d , %n", &mode, &format, &used) != 2 ||
      !used || mode < 0 || mode > 4 || format < 0 || format > 2) return false;
  String rest = line.substring(used); rest.trim();
  if (!rest.startsWith("\"")) return false;
  int end = rest.indexOf('"', 1);
  if (end < 0) return false;
  name = rest.substring(1, end);
  String tail = rest.substring(end + 1); tail.trim();
  if (tail.length() && !tail.startsWith(",")) { name = ""; return false; }
  if (!name.length() || name.length() > 96 ||
      (format == 2 && !operatorDigits(name, 5, 6))) { name = ""; return false; }
  return true;
}
static String operatorName(const String& plmn) {
  if (plmn == "46000" || plmn == "46002" || plmn == "46004" || plmn == "46007" || plmn == "46008" || plmn == "46013") return "中国移动";
  if (plmn == "46001" || plmn == "46006" || plmn == "46009") return "中国联通";
  if (plmn == "46003" || plmn == "46005" || plmn == "46011") return "中国电信";
  if (plmn == "46015") return "中国广电";
  if (plmn == "45403" || plmn == "45404" || plmn == "45405") return "3（中国香港）";
  if (plmn == "45400" || plmn == "45402" || plmn == "45418") return "CSL（中国香港）";
  if (plmn == "45412" || plmn == "45413") return "中国移动（中国香港）";
  if (plmn == "45406" || plmn == "45415" || plmn == "45417") return "数码通（中国香港）";
  if (plmn == "45407") return "中国联通（中国香港）";
  return "";
}
static String homeOperator(const String& response) {
  if (!atResponseOK(response)) return "未知";
  // CIMI无前缀，仅接受完整15位IMSI；不将完整用户标识输出到网络卡片。
  for (unsigned int start = 0; start < response.length();) {
    int end = response.indexOf('\n', start);
    if (end < 0) end = response.length();
    String imsi = response.substring(start, end); imsi.trim(); start = end + 1;
    if (!operatorDigits(imsi, 15, 15)) continue;
    if (imsi.startsWith("460") || imsi.startsWith("454")) {
      String plmn = imsi.substring(0, 5), name = operatorName(plmn);
      if (name.length()) return name + " (" + plmn + ")";
    }
    // 其他MCC的MNC可能为2位或3位，未读EF-AD前不猜归属PLMN。
    return "SIM归属 MCC " + imsi.substring(0, 3) + "（MNC未确认）";
  }
  return "未知";
}
static int networkRegistration(const String& response) {
  if (!atResponseOK(response)) return -1;
  for (unsigned int start = 0; start < response.length();) {
    int end = response.indexOf('\n', start);
    if (end < 0) end = response.length();
    String line = response.substring(start, end); line.trim(); start = end + 1;
    int mode = -1, stat = -1, used = 0;
    if (sscanf(line.c_str(), "+CEREG: %d , %d %n", &mode, &stat, &used) == 2 &&
        used && mode >= 0 && mode <= 5 && stat >= 0 && stat <= 10 &&
        (!line[used] || line[used] == ',')) return stat;
  }
  return -1;
}

// 处理模组信息查询请求
void handleQuery() {
  if (!checkAuth()) return;
  
  String type = server.arg("type");
  if (type != "wifi" && !cellularReady()) {
    server.send(409, "application/json", "{\"success\":false,\"message\":\"" + jsonEscape(cellularBlockReason()) + "\"}"); return;
  }
  ATTransaction transaction;
  if (!transaction) { server.send(409, "text/plain", "模组事务忙碌"); return; }
  String json = "{";
  bool success = false;
  String message = "";
  
  if (type == "ati") {
    // 固件信息查询
    String resp = transactionATCommand("ATI", 2000);
    Serial.println("ATI响应: " + resp);
    
    if (resp.indexOf("OK") >= 0) {
      success = true;
      String manufacturer = "未知";
      String model = "未知";
      String version = "未知";
      
      int lineStart = 0;
      int lineNum = 0;
      for (int i = 0; i < resp.length(); i++) {
        if (resp.charAt(i) == '\n' || i == resp.length() - 1) {
          String line = resp.substring(lineStart, i);
          line.trim();
          if (line.length() > 0 && line != "ATI" && line != "OK") {
            lineNum++;
            if (lineNum == 1) manufacturer = line;
            else if (lineNum == 2) model = line;
            else if (lineNum == 3) version = line;
          }
          lineStart = i + 1;
        }
      }
      
      message = "<table class='info-table'>";
      message += "<tr><td>制造商</td><td>" + htmlEscape(manufacturer) + "</td></tr>";
      message += "<tr><td>模组型号</td><td>" + htmlEscape(model) + "</td></tr>";
      message += "<tr><td>固件版本</td><td>" + htmlEscape(version) + "</td></tr>";
      message += "</table>";
    } else {
      message = "查询失败";
    }
  }
  else if (type == "signal") {
    // 信号质量查询
    String resp = transactionATCommand("AT+CESQ", 2000);
    Serial.println("CESQ响应: " + resp);
    
    if (resp.indexOf("+CESQ:") >= 0) {
      success = true;
      int idx = resp.indexOf("+CESQ:");
      String params = resp.substring(idx + 6);
      int endIdx = params.indexOf('\r');
      if (endIdx < 0) endIdx = params.indexOf('\n');
      if (endIdx > 0) params = params.substring(0, endIdx);
      params.trim();
      
      String values[6];
      int valIdx = 0;
      int startPos = 0;
      for (int i = 0; i <= params.length() && valIdx < 6; i++) {
        if (i == params.length() || params.charAt(i) == ',') {
          values[valIdx] = params.substring(startPos, i);
          values[valIdx].trim();
          valIdx++;
          startPos = i + 1;
        }
      }
      
      int rsrp = values[5].toInt();
      String rsrpStr;
      if (rsrp == 99 || rsrp == 255) {
        rsrpStr = "未知";
      } else {
        int rsrpDbm = -140 + rsrp;
        rsrpStr = String(rsrpDbm) + " dBm";
        if (rsrpDbm >= -80) rsrpStr += " (信号极好)";
        else if (rsrpDbm >= -90) rsrpStr += " (信号良好)";
        else if (rsrpDbm >= -100) rsrpStr += " (信号一般)";
        else if (rsrpDbm >= -110) rsrpStr += " (信号较弱)";
        else rsrpStr += " (信号很差)";
      }
      
      int rsrq = values[4].toInt();
      String rsrqStr;
      if (rsrq == 99 || rsrq == 255) {
        rsrqStr = "未知";
      } else {
        float rsrqDb = -19.5 + rsrq * 0.5;
        rsrqStr = String(rsrqDb, 1) + " dB";
      }
      
      message = "<table class='info-table'>";
      message += "<tr><td>信号强度 (RSRP)</td><td>" + rsrpStr + "</td></tr>";
      message += "<tr><td>信号质量 (RSRQ)</td><td>" + rsrqStr + "</td></tr>";
      message += "<tr><td>原始数据</td><td>" + htmlEscape(params) + "</td></tr>";
      message += "</table>";
    } else {
      message = "查询失败";
    }
  }
  else if (type == "siminfo") {
    // SIM 卡信息查询
    success = true;
    message = "<table class='info-table'>";
    
    String resp = transactionATCommand("AT+CIMI", 2000);
    String imsi = "未知";
    if (resp.indexOf("OK") >= 0) {
      int start = resp.indexOf('\n');
      if (start >= 0) {
        int end = resp.indexOf('\n', start + 1);
        if (end < 0) end = resp.indexOf('\r', start + 1);
        if (end > start) {
          imsi = resp.substring(start + 1, end);
          imsi.trim();
          if (imsi == "OK" || imsi.length() < 10) imsi = "未知";
        }
      }
    }
    message += "<tr><td>IMSI</td><td>" + imsi + "</td></tr>";
    
    resp = transactionATCommand("AT+ICCID", 2000);
    String iccid = "未知";
    if (resp.indexOf("+ICCID:") >= 0) {
      int idx0 = resp.indexOf("+ICCID:");
      String tmp = resp.substring(idx0 + 7);
      int endIdx0 = tmp.indexOf('\r');
      if (endIdx0 < 0) endIdx0 = tmp.indexOf('\n');
      if (endIdx0 > 0) iccid = tmp.substring(0, endIdx0);
      iccid.trim();
    }
    message += "<tr><td>ICCID</td><td>" + iccid + "</td></tr>";
    
    resp = transactionATCommand("AT+CNUM", 2000);
    String phoneNum = "未存储或不支持";
    if (resp.indexOf("+CNUM:") >= 0) {
      int idx0 = resp.indexOf(",\"");
      if (idx0 >= 0) {
        int endIdx0 = resp.indexOf("\"", idx0 + 2);
        if (endIdx0 > idx0) {
          phoneNum = resp.substring(idx0 + 2, endIdx0);
        }
      }
    }
    message += "<tr><td>本机号码</td><td>" + phoneNum + "</td></tr>";
    message += "</table>";
  }
  else if (type == "network") {
    // 网络状态查询
    success = true;
    message = "<table class='info-table'>";
    
    String resp = transactionATCommand("AT+CEREG?", 2000);
    String regStatus = "未知";
    int registration = networkRegistration(resp);
    switch (registration) {
      case 0: regStatus = "未注册，未搜索"; break;
      case 1: regStatus = "已注册，本地网络"; break;
      case 2: regStatus = "未注册，正在搜索"; break;
      case 3: regStatus = "注册被拒绝"; break;
      case 5: regStatus = "已注册，漫游"; break;
      default: break;
    }
    message += "<tr><td>网络注册</td><td>" + regStatus + "</td></tr>";
    String home = homeOperator(transactionATCommand("AT+CIMI", 1500));
    message += "<tr><td>原始运营商（SIM归属）</td><td>" + htmlEscape(home) + "</td></tr>";

    resp = transactionATCommand("AT+COPS?", 1500);
    int format; String oper, plmn;
    bool operatorOK = parseOperator(resp, format, oper);
    bool registered = registration == 1 || registration == 5;
    bool restored = true;
    if (operatorOK && registered) {
      if (format == 2) plmn = oper;
      else {
        // mode=3仅改变显示格式，不执行选网；查询后总是恢复原格式。
        bool numeric = atResponseOK(transactionATCommand("AT+COPS=3,2", 1000));
        if (numeric) {
          int numericFormat; String numericName;
          if (parseOperator(transactionATCommand("AT+COPS?", 1500), numericFormat, numericName) && numericFormat == 2)
            plmn = numericName;
        }
        String restore = "AT+COPS=3," + String(format);
        restored = atResponseOK(transactionATCommand(restore.c_str(), 1000));
      }
    }
    String current = !registered ? "未驻网" : operatorOK ? oper : "未知";
    if (registered && plmn.length()) {
      String resolved = operatorName(plmn);
      if (resolved.length() && resolved != current) current += " / " + resolved;
      if (current != plmn) current += " (" + plmn + ")";
    }
    message += "<tr><td>" + String(registration == 5 ? "漫游运营商（当前驻网）" : "当前驻网运营商") +
      "</td><td>" + htmlEscape(current) + "</td></tr>";
    message += "<tr><td>驻网PLMN</td><td>" + String(!registered ? "未驻网" : plmn.length() ? plmn : "未获取") + "</td></tr>";
    if (!restored) message += "<tr><td>查询提示</td><td>运营商显示格式恢复未确认，下次查询将按模组实际格式显示</td></tr>";
    
    resp = transactionATCommand("AT+CGACT?", 2000);
    String pdpStatus = "未激活";
    if (resp.indexOf("+CGACT: 1,1") >= 0) {
      pdpStatus = "已激活";
    } else if (resp.indexOf("+CGACT:") >= 0) {
      pdpStatus = "未激活";
    }
    message += "<tr><td>数据连接</td><td>" + pdpStatus + "</td></tr>";
    
    resp = transactionATCommand("AT+CGDCONT?", 2000);
    String apn = "未知";
    if (resp.indexOf("+CGDCONT:") >= 0) {
      int idx0 = resp.indexOf(",\"");
      if (idx0 >= 0) {
        idx0 = resp.indexOf(",\"", idx0 + 2);
        if (idx0 >= 0) {
          int endIdx0 = resp.indexOf("\"", idx0 + 2);
          if (endIdx0 > idx0) {
            apn = resp.substring(idx0 + 2, endIdx0);
            if (apn.length() == 0) apn = "(自动)";
          }
        }
      }
    }
    message += "<tr><td>APN</td><td>" + htmlEscape(apn) + "</td></tr>";
    message += "</table>";
  }
  else if (type == "wifi") {
    // WiFi 状态查询
    success = true;
    message = "<table class='info-table'>";
    
    String wifiStatus = WiFi.isConnected() ? "已连接" : "未连接";
    message += "<tr><td>连接状态</td><td>" + wifiStatus + "</td></tr>";
    
    String ssid = WiFi.SSID();
    if (ssid.length() == 0) ssid = "未知";
    message += "<tr><td>当前SSID</td><td>" + htmlEscape(ssid) + "</td></tr>";
    
    int rssi = WiFi.RSSI();
    String rssiStr = String(rssi) + " dBm";
    if (rssi >= -50) rssiStr += " (信号极好)";
    else if (rssi >= -60) rssiStr += " (信号很好)";
    else if (rssi >= -70) rssiStr += " (信号良好)";
    else if (rssi >= -80) rssiStr += " (信号一般)";
    else if (rssi >= -90) rssiStr += " (信号较弱)";
    else rssiStr += " (信号很差)";
    message += "<tr><td>信号强度 (RSSI)</td><td>" + rssiStr + "</td></tr>";
    
    message += "<tr><td>IP地址</td><td>" + WiFi.localIP().toString() + "</td></tr>";
    message += "<tr><td>网关</td><td>" + WiFi.gatewayIP().toString() + "</td></tr>";
    message += "<tr><td>子网掩码</td><td>" + WiFi.subnetMask().toString() + "</td></tr>";
    message += "<tr><td>DNS服务器</td><td>" + WiFi.dnsIP().toString() + "</td></tr>";
    message += "<tr><td>MAC地址</td><td>" + WiFi.macAddress() + "</td></tr>";
    message += "<tr><td>路由器BSSID</td><td>" + WiFi.BSSIDstr() + "</td></tr>";
    message += "<tr><td>WiFi信道</td><td>" + String(WiFi.channel()) + "</td></tr>";
    message += "</table>";
  }
  else {
    message = "未知的查询类型";
  }
  
  json += "\"success\":" + String(success ? "true" : "false") + ",";
  json += "\"message\":\"" + jsonEscape(message) + "\"";
  json += "}";
  
  server.send(200, "application/json", json);
}

// 处理 Ping 请求
void handlePing() {
  if (!checkAuth()) return;
  
  String host = server.hasArg("host") ? server.arg("host") : String("8.8.8.8");
  String result;
  bool success = runModemPing(host, result);
  String json = "{\"success\":" + String(success ? "true" : "false") +
                ",\"message\":\"" + jsonEscape(result) + "\"}";
  
  server.send(200, "application/json", json);
}
