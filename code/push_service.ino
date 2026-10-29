/*
 * push_service.ino - 推送服务函数实现
 * 
 * 支持 HTTPS 请求（跳过证书验证）
 * 支持钉钉加签验证
 */

#include <mbedtls/md.h>
#include <mbedtls/base64.h>

// URL 编码辅助函数
String urlEncode(const String& str) {
  String encoded = "";
  char c;
  char code0;
  char code1;
  for (unsigned int i = 0; i < str.length(); i++) {
    c = str.charAt(i);
    if (c == ' ') {
      encoded += '+';
    } else if (isalnum(c)) {
      encoded += c;
    } else {
      code1 = (c & 0xf) + '0';
      if ((c & 0xf) > 9) code1 = (c & 0xf) - 10 + 'A';
      c = (c >> 4) & 0xf;
      code0 = c + '0';
      if (c > 9) code0 = c - 10 + 'A';
      encoded += '%';
      encoded += code0;
      encoded += code1;
    }
  }
  return encoded;
}

// JSON 转义函数
String jsonEscape(const String& str) {
  String result = "";
  for (unsigned int i = 0; i < str.length(); i++) {
    char c = str.charAt(i);
    if (c == '"') result += "\\\"";
    else if (c == '\\') result += "\\\\";
    else if (c == '\n') result += "\\n";
    else if (c == '\r') result += "\\r";
    else if (c == '\t') result += "\\t";
    else if ((uint8_t)c < 0x20) {
      char escaped[7]; snprintf(escaped, sizeof(escaped), "\\u%04x", (unsigned int)(uint8_t)c); result += escaped;
    } else result += c;
  }
  return result;
}

// Telegram Markdown 转义函数
String telegramEscape(const String& str) {
  String result = "";
  for (unsigned int i = 0; i < str.length(); i++) {
    char c = str.charAt(i);
    // 转义 Markdown 特殊字符: _ * [ ] ( ) ~ ` > # + - = | { } . !
    if (c == '_' || c == '*' || c == '[' || c == ']' || c == '(' || c == ')' ||
        c == '~' || c == '`' || c == '>' || c == '#' || c == '+' || c == '-' ||
        c == '=' || c == '|' || c == '{' || c == '}' || c == '.' || c == '!') {
      result += '\\';
    }
    result += c;
  }
  return result;
}

// HMAC-SHA256 签名（用于钉钉加签）
String hmacSha256Base64(const String& secret, const String& data) {
  unsigned char hmacResult[32];
  
  mbedtls_md_context_t ctx;
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
  mbedtls_md_hmac_starts(&ctx, (unsigned char*)secret.c_str(), secret.length());
  mbedtls_md_hmac_update(&ctx, (unsigned char*)data.c_str(), data.length());
  mbedtls_md_hmac_finish(&ctx, hmacResult);
  mbedtls_md_free(&ctx);
  
  // Base64 编码
  unsigned char base64Result[64];
  size_t outLen;
  mbedtls_base64_encode(base64Result, sizeof(base64Result), &outLen, hmacResult, 32);
  base64Result[outLen] = 0;
  
  return String((char*)base64Result);
}

// 发送 HTTP/HTTPS 请求的通用函数
int sendHttpRequest(const String& url, const String& method, const String& contentType, const String& body) {
  HTTPClient http;
  http.setConnectTimeout(2000);
  http.setTimeout(2000);
  http.setReuse(false);
  WiFiClientSecure httpTls;
  httpTls.setInsecure();
  httpTls.setHandshakeTimeout(3);
  httpTls.setConnectionTimeout(2000);
  
  // 判断是否为 HTTPS
  if (url.startsWith("https://")) {
    // 使用全局的 ssl_client（在 code.ino 中已设置 setInsecure）
    http.begin(httpTls, url);
  } else {
    http.begin(url);
  }
  
  if (contentType.length() > 0) {
    http.addHeader("Content-Type", contentType);
  }
  
  int httpCode;
  if (method == "GET") {
    httpCode = http.GET();
  } else {
    httpCode = http.POST(body);
  }
  
  if (httpCode > 0) {
    Serial.printf("HTTP响应码: %d\n", httpCode);
    // 所有 2xx 状态码都视为成功 (200 OK, 201 Created, 202 Accepted, 204 No Content 等)
    if (httpCode >= 200 && httpCode < 300) {
      // 不下载响应正文：即便服务端返回巨量或无限正文，内存也不会随之增长。
      stats.pushSuccess++;
    } else {
      Serial.println("HTTP错误响应");
      stats.pushFailed++;
    }
  } else {
    Serial.printf("HTTP请求失败: %s\n", http.errorToString(httpCode).c_str());
    stats.pushFailed++;
  }
  
  http.end();
  return httpCode;
}

// 发送单个推送通道
void sendToChannel(const PushChannel& channel, const char* sender, const char* message, const char* timestamp) {
  if (!channel.enabled) return;
  if (channel.url.length() == 0) return;
  
  String channelName = channel.name.length() > 0 ? channel.name : ("通道" + String(channel.type));
  Serial.println("发送到推送通道: " + channelName);
  
  String senderEscaped, messageEscaped, timestampEscaped;
  if (channel.type == PUSH_TYPE_POST_JSON || channel.type == PUSH_TYPE_BARK || channel.type == PUSH_TYPE_CUSTOM) {
    senderEscaped = jsonEscape(String(sender));
    messageEscaped = jsonEscape(String(message));
    timestampEscaped = jsonEscape(String(timestamp));
  }
  
  switch (channel.type) {
    case PUSH_TYPE_POST_JSON: {
      // 标准 POST JSON 格式
      String jsonData = "{";
      jsonData += "\"sender\":\"" + senderEscaped + "\",";
      jsonData += "\"message\":\"" + messageEscaped + "\",";
      jsonData += "\"timestamp\":\"" + timestampEscaped + "\"";
      jsonData += "}";
      Serial.println("POST JSON: " + jsonData);
      sendHttpRequest(channel.url, "POST", "application/json", jsonData);
      break;
    }
    
    case PUSH_TYPE_BARK: {
      // Bark 推送格式
      String jsonData = "{";
      jsonData += "\"title\":\"" + senderEscaped + "\",";
      jsonData += "\"body\":\"" + messageEscaped + "\"";
      jsonData += "}";
      Serial.println("BARK: " + jsonData);
      sendHttpRequest(channel.url, "POST", "application/json", jsonData);
      break;
    }
    
    case PUSH_TYPE_GET: {
      // GET 请求，参数放 URL 里
      String getUrl = channel.url;
      if (getUrl.indexOf('?') == -1) {
        getUrl += "?";
      } else {
        getUrl += "&";
      }
      getUrl += "sender=" + urlEncode(String(sender));
      getUrl += "&message=" + urlEncode(String(message));
      getUrl += "&timestamp=" + urlEncode(String(timestamp));
      Serial.println("GET: " + getUrl);
      sendHttpRequest(getUrl, "GET", "", "");
      break;
    }
    
    case PUSH_TYPE_CUSTOM: {
      // 自定义模板
      if (channel.customBody.length() == 0) {
        Serial.println("自定义模板为空，跳过");
        return;
      }
      String body = channel.customBody;
      body.replace("{sender}", senderEscaped);
      body.replace("{message}", messageEscaped);
      body.replace("{timestamp}", timestampEscaped);
      Serial.println("自定义: " + body);
      sendHttpRequest(channel.url, "POST", "application/json", body);
      break;
    }
    
    case PUSH_TYPE_TELEGRAM: {
      // Telegram Bot 推送
      // URL格式: https://api.telegram.org/bot<TOKEN>/sendMessage
      // 使用 MarkdownV2 模式，需要转义特殊字符
      String senderTg = telegramEscape(String(sender));
      String messageTg = telegramEscape(String(message));
      String timestampTg = telegramEscape(String(timestamp));
      String text = "📱 *来自: " + senderTg + "*\n" + messageTg + "\n\n_" + timestampTg + "_";
      String jsonData = "{";
      jsonData += "\"chat_id\":\"" + channel.key1 + "\",";
      jsonData += "\"text\":\"" + jsonEscape(text) + "\",";
      jsonData += "\"parse_mode\":\"MarkdownV2\"";
      jsonData += "}";
      Serial.println("Telegram: " + jsonData);
      sendHttpRequest(channel.url, "POST", "application/json", jsonData);
      break;
    }
    
    case PUSH_TYPE_WECOM: {
      // 企业微信机器人 (Webhook)
      // URL格式: https://qyapi.weixin.qq.com/cgi-bin/webhook/send?key=xxx
      String content = "📱 来自: " + String(sender) + "\n" + String(message) + "\n\n" + String(timestamp);
      String jsonData = "{";
      jsonData += "\"msgtype\":\"text\",";
      jsonData += "\"text\":{\"content\":\"" + jsonEscape(content) + "\"}";
      jsonData += "}";
      Serial.println("企业微信: " + jsonData);
      sendHttpRequest(channel.url, "POST", "application/json", jsonData);
      break;
    }
    
    case PUSH_TYPE_DINGTALK: {
      // 钉钉机器人 (Webhook)
      // URL格式: https://oapi.dingtalk.com/robot/send?access_token=xxx
      // 如果配置了加签密钥（key1），则需要添加签名
      
      String requestUrl = channel.url;
      
      // 检查是否需要加签
      if (channel.key1.length() > 0) {
        // 获取当前时间戳（毫秒）- 使用 millis() 补充真实毫秒精度
        unsigned long long timestampMs = (unsigned long long)time(nullptr) * 1000ULL + (millis() % 1000);
        // 格式化为 13 位时间戳字符串
        char timestampBuf[16];
        snprintf(timestampBuf, sizeof(timestampBuf), "%llu", timestampMs);
        String timestampStr = String(timestampBuf);
        
        // 构造签名字符串
        String stringToSign = timestampStr + "\n" + channel.key1;
        
        // 计算 HMAC-SHA256 签名
        String sign = hmacSha256Base64(channel.key1, stringToSign);
        sign = urlEncode(sign);
        
        // 添加签名参数到 URL
        if (requestUrl.indexOf('?') == -1) {
          requestUrl += "?";
        } else {
          requestUrl += "&";
        }
        requestUrl += "timestamp=" + timestampStr;
        requestUrl += "&sign=" + sign;
        
        Serial.println("钉钉加签URL: " + requestUrl);
      }
      
      String content = "📱 来自: " + String(sender) + "\n" + String(message) + "\n\n" + String(timestamp);
      String jsonData = "{";
      jsonData += "\"msgtype\":\"text\",";
      jsonData += "\"text\":{\"content\":\"" + jsonEscape(content) + "\"}";
      jsonData += "}";
      Serial.println("钉钉: " + jsonData);
      sendHttpRequest(requestUrl, "POST", "application/json", jsonData);
      break;
    }
    
    case PUSH_TYPE_FEISHU: {
      String jsonData = "{";
      if (channel.key1.length() > 0) {
        time_t timestampSec = time(nullptr);
        // 校时由WiFi连接后台启动；签名路径不等待NTP。
        if (timestampSec < 100000) {
          Serial.println("时间未同步，跳过本次飞书加签推送");
          stats.pushFailed++;
          return;
        }
        char timestampBuf[24];
        snprintf(timestampBuf, sizeof(timestampBuf), "%llu", (unsigned long long)timestampSec);
        String timestampStr = String(timestampBuf);
        // 飞书以秒级时间戳、换行和密钥作HMAC密钥，消息为空。
        String sign = hmacSha256Base64(timestampStr + "\n" + channel.key1, "");
        jsonData += "\"timestamp\":\"" + jsonEscape(timestampStr) + "\",";
        jsonData += "\"sign\":\"" + jsonEscape(sign) + "\",";
      }
      String content = "来自: " + String(sender) + "\n" + String(message) + "\n\n" + String(timestamp);
      jsonData += "\"msg_type\":\"text\",\"content\":{\"text\":\"" + jsonEscape(content) + "\"}}";
      sendHttpRequest(channel.url, "POST", "application/json", jsonData);
      break;
    }

    default:
      Serial.println("未知推送类型");
      return;
  }
}

// 发送短信到所有启用的推送通道
void sendSMSToServer(const char* sender, const char* message, const char* timestamp) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi未连接，跳过推送");
    return;
  }
  
  bool hasEnabledChannel = false;
  for (int i = 0; i < MAX_PUSH_CHANNELS; i++) {
    if (isPushChannelValid(config.pushChannels[i])) {
      hasEnabledChannel = true;
      break;
    }
  }
  
  if (!hasEnabledChannel) {
    Serial.println("无HTTP推送通道");
    return;
  }
  
  Serial.println("\n=== 开始多通道推送 ===");
  for (int i = 0; i < MAX_PUSH_CHANNELS; i++) {
    if (isPushChannelValid(config.pushChannels[i])) {
      sendToChannel(config.pushChannels[i], sender, message, timestamp);
      delay(100); // 短暂延迟避免请求过快
    }
  }
  Serial.println("=== 多通道推送完成 ===\n");
  
  // 保存推送统计
  saveStats();
}

// 有界通知队列。主循环每轮只发送一个HTTP通道；SMTP使用库的异步状态机。
struct PendingNotification {
  String sender, message, timestamp, subject, body;
  unsigned long queuedAt = 0;
  int channel = 0;
  bool emailOnly = false;
};
static const unsigned int NOTIFICATION_CAPACITY = 8;
static PendingNotification notifications[NOTIFICATION_CAPACITY];
static unsigned int notificationHead = 0, notificationCount = 0;
static unsigned long droppedNotifications = 0;
static unsigned long mailStarted = 0;
static int mailStage = 0;

unsigned int notificationQueueDepth() { return notificationCount; }
unsigned long notificationDropCount() { return droppedNotifications; }

bool enqueueSmsNotification(const char* sender, const char* message, const char* timestamp) {
  if (!sender || !message || !timestamp || strlen(sender) > 64 ||
      strlen(message) > 4096 || strlen(timestamp) > 64 ||
      notificationCount == NOTIFICATION_CAPACITY) {
    droppedNotifications++;
    Serial.println("通知队列已满或消息超限；短信历史保留，拒绝新通知");
    return false;
  }
  PendingNotification& job = notifications[(notificationHead + notificationCount) % NOTIFICATION_CAPACITY];
  job = PendingNotification();
  job.sender = sender;
  job.message = message;
  job.timestamp = timestamp;
  job.queuedAt = millis();
  notificationCount++;
  return true;
}

void sendEmailNotification(const char* subject, const char* body) {
  if (!config.emailEnabled) return;
  if (!subject || !body || strlen(subject) > 256 || strlen(body) > 4608 ||
      notificationCount == NOTIFICATION_CAPACITY) {
    droppedNotifications++;
    return;
  }
  PendingNotification& job = notifications[(notificationHead + notificationCount) % NOTIFICATION_CAPACITY];
  job = PendingNotification();
  job.subject = subject;
  job.body = body;
  job.emailOnly = true;
  job.channel = MAX_PUSH_CHANNELS;
  job.queuedAt = millis();
  notificationCount++;
}

void finishNotification() {
  smtp.stop();
  smtp.getMessage().clear();
  ssl_client.stop();
  mailStage = 0;
  PendingNotification& completed = notifications[notificationHead];
  completed.sender = (const char*)nullptr;
  completed.message = (const char*)nullptr;
  completed.timestamp = (const char*)nullptr;
  completed.subject = (const char*)nullptr;
  completed.body = (const char*)nullptr;
  completed = PendingNotification();
  notificationHead = (notificationHead + 1) % NOTIFICATION_CAPACITY;
  notificationCount--;
}

void processNotifications() {
  if (!notificationCount) return;
  PendingNotification& job = notifications[notificationHead];
  if ((unsigned long)(millis() - job.queuedAt) >= 300000) {
    droppedNotifications++;
    Serial.println("通知等待超过5分钟，释放队列槽位");
    finishNotification();
    return;
  }
  if (WiFi.status() != WL_CONNECTED) {
    if (mailStage) finishNotification();
    return;
  }
  if (!job.emailOnly && job.channel < MAX_PUSH_CHANNELS) {
    const PushChannel& channel = config.pushChannels[job.channel++];
    if (isPushChannelValid(channel)) {
      sendToChannel(channel, job.sender.c_str(), job.message.c_str(), job.timestamp.c_str());
      saveStats();
    }
    return;
  }
  if (!config.emailEnabled || !config.smtpServer.length() || !config.smtpUser.length() ||
      !config.smtpPass.length() || !config.smtpSendTo.length()) {
    finishNotification();
    return;
  }
  // 校时在连接WiFi时后台启动；不在邮件路径等待NTP。
  if (time(nullptr) < 100000) {
    Serial.println("时间未同步，跳过本次邮件");
    stats.pushFailed++;
    finishNotification();
    return;
  }
  // 15秒是协作式轮询预算；同步TCP/TLS仍受底层超时约束，并非整个操作的绝对上限。
  if (mailStage && ((unsigned long)(millis() - mailStarted) >= 15000 ||
                    smtp.status().errorCode != 0)) {
    Serial.println("邮件失败或轮询已达15秒预算，关闭连接");
    stats.pushFailed++;
    finishNotification();
    return;
  }
  if (!mailStage) {
    mailStarted = millis();
    mailStage = 1;
    if (!smtp.connect(config.smtpServer, config.smtpPort, nullptr, true, false)) {
      stats.pushFailed++;
      finishNotification();
    }
    return;
  }
  smtp.loop();
  // ReadyMail的isConnected同时检查底层连接和server_greeting_ack，不只是TCP已连接。
  if (mailStage == 1 && smtp.isConnected()) {
    mailStage = 2;
    if (!smtp.authenticate(config.smtpUser, config.smtpPass, readymail_auth_password, false)) {
      stats.pushFailed++;
      finishNotification();
    }
  } else if (mailStage == 2 && smtp.isAuthenticated()) {
    if (!job.emailOnly) {
      String code = extractVerifyCode(job.message);
      job.subject = code.length() ? "[" + code + "] " + job.sender : job.sender + ": " + job.message.substring(0, 30);
      job.body = "发送者: " + job.sender + "\n时间: " + job.timestamp + "\n内容:\n" + job.message;
    }
    // await=false保留消息引用，必须使用库拥有的内部消息。
    SMTPMessage& msg = smtp.getMessage();
    msg.clear();
    msg.headers.add(rfc822_from, ("sms notify <" + config.smtpUser + ">").c_str());
    msg.headers.add(rfc822_to, config.smtpSendTo.c_str());
    msg.headers.add(rfc822_subject, job.subject.c_str());
    msg.text.body(job.body.c_str());
    msg.timestamp = time(nullptr);
    mailStage = 3;
    if (!smtp.send(msg, "", false)) {
      stats.pushFailed++;
      finishNotification();
    }
  } else if (mailStage == 3 && smtp.status().isComplete) {
    stats.pushSuccess++;
    Serial.println("邮件发送完成");
    finishNotification();
    saveStats();
  }
}
