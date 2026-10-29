/*
 * SMS Forwarding - 短信转发器
 * 
 * 文件结构:
 * - code.ino          : 主文件（全局变量、setup、loop）
 * - config.h          : 配置结构体和常量定义
 * - config.ino        : 配置相关函数实现
 * - web_pages.h       : HTML 页面内容
 * - web_handlers.h    : Web 处理函数声明
 * - web_handlers.ino  : Web 处理函数实现
 * - sms_handler.h     : 短信处理函数声明
 * - sms_handler.ino   : 短信处理函数实现
 * - push_service.h    : 推送服务函数声明
 * - push_service.ino  : 推送服务函数实现
 * - airplane_mode.h/.ino : 飞行调度和射频恢复
 * - wifi_config.h     : WiFi 配置
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <SPIFFS.h>
#include <pdulib.h>
#define ENABLE_SMTP
#define ENABLE_DEBUG
#include <ReadyMail.h>
#include <HTTPClient.h>
#include <base64.h>
#include <esp_task_wdt.h>
#include <esp_timer.h>
#include <esp_system.h>
#include <esp_heap_caps.h>
#include <time.h>

// 项目配置文件
#include "wifi_config.h"
#include "config.h"
#include "web_pages.h"
#include "web_handlers.h"
#include "sms_handler.h"
#include "push_service.h"
#include "airplane_mode.h"

// ========== 全局变量定义 ==========
Config config;
Preferences preferences;
WiFiMulti WiFiMulti;
PDU pdu = PDU(4096);
WiFiClientSecure ssl_client;
SMTPClient smtp(ssl_client);
WebServer server(80);

bool configValid = false;
unsigned long lastPrintTime = 0;
unsigned long lastTimerExec = 0;
unsigned long timerIntervalSec = 0;
static uint64_t timerLastMs = 0;
static bool timerPending = false;
static unsigned long wifiRetryAt = 0;
static bool watchdogReady = false;
static unsigned long maxLoopMs = 0;
static unsigned long diagnosticsAt = 0;

char serialBuf[SERIAL_BUFFER_SIZE];
int serialBufLen = 0;

ConcatSms concatBuffer[MAX_CONCAT_MESSAGES];


// ========== setup 函数 ==========
void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  Serial.begin(115200);
  Serial.printf("复位原因: %d\n", (int)esp_reset_reason());
  // RX缓冲大小必须在begin前设置；为网络短时阻塞预留缓冲。
  Serial1.setRxBufferSize(8192);
  Serial1.begin(115200, SERIAL_8N1, RXD, TXD);
  
  // 初始化长短信缓存
  initConcatBuffer();
  
  // 初始化短信存储(SPIFFS)
  initSmsStorage();
  
  // 加载配置
  loadConfig();
  configValid = isConfigValid();
  
  // 添加所有启用的 WiFi 网络（WiFiMulti 会自动选择信号最强的）
  int wifiCount = 0;
  for (int i = 0; i < MAX_WIFI_NETWORKS; i++) {
    if (config.wifiNetworks[i].enabled && config.wifiNetworks[i].ssid.length() > 0) {
      WiFiMulti.addAP(config.wifiNetworks[i].ssid.c_str(), config.wifiNetworks[i].password.c_str());
      Serial.printf("已添加WiFi: %s\n", config.wifiNetworks[i].ssid.c_str());
      wifiCount++;
    }
  }
  
  if (wifiCount == 0) {
    Serial.println("警告: 未配置任何WiFi网络，请检查配置");
  }
  
  Serial.println("连接WiFi...");
  WiFi.setAutoReconnect(true);
  unsigned long wifiStart = millis();
  while (WiFiMulti.run(2000) != WL_CONNECTED && millis() - wifiStart < 15000) blink_short(50);
  configTime(0, 0, "ntp.ntsc.ac.cn", "pool.ntp.org");
  Serial.printf("WiFi已连接: %s, IP: %s\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
  
  // 启动 mDNS 服务
  if (MDNS.begin(MDNS_HOSTNAME)) {
    MDNS.addService("http", "tcp", 80);  // 注册 HTTP 服务
    Serial.println("mDNS 已启动");
    Serial.printf("访问地址: http://%s.local 或 http://%s\n", MDNS_HOSTNAME, WiFi.localIP().toString().c_str());
  } else {
    Serial.println("mDNS 启动失败，请使用 IP 访问");
  }
  
  // 启动 HTTP 服务器
  server.on("/", handleRoot);
  server.on("/assets/app.css", HTTP_GET, handleAppCss);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/tools", handleToolsPage);
  server.on("/sendsms", HTTP_POST, handleSendSms);
  server.on("/ping", HTTP_POST, handlePing);
  server.on("/timer", HTTP_POST, handleTimer);
  server.on("/query", handleQuery);
  server.on("/restart", HTTP_POST, handleRestart);     // 重启
  server.on("/history/clear", handleClearSmsHistory);
  server.on("/history", handleSmsHistory);             // 短信历史
  server.on("/stats", handleStats);                    // 统计信息
  server.on("/airplane", HTTP_GET, handleAirplaneStatus);
  server.on("/airplane/save", HTTP_POST, handleAirplaneSave);
  server.on("/airplane/action", HTTP_POST, handleAirplaneAction);
  server.begin();
  Serial.println("HTTP服务器已启动");
  
  ssl_client.setInsecure();
  ssl_client.setHandshakeTimeout(3);
  ssl_client.setConnectionTimeout(2000);
  unsigned long modemStart = millis();
  while (!sendATandWaitOK("AT", 1000) && millis() - modemStart < 10000) {
    Serial.println("AT未响应，限时重试...");
    blink_short(50);
  }
  Serial.println("模组AT响应正常");
  
  initAirplaneMode();
  serviceAirplaneMode();
  

  Serial.println("模组初始化完成");
  digitalWrite(LED_BUILTIN, LOW);
  
  // 发送启动通知
  if (configValid) {
    String body = "设备已启动\n地址: " + getDeviceUrl() + "\n启动次数: " + String(stats.bootCount);
    sendEmailNotification("短信转发器已启动", body.c_str());
  }
  
  // 启用看门狗 (30秒超时)
  // 适配 ESP32 Arduino Core v3.0+
  esp_task_wdt_config_t wdt_config = {
      .timeout_ms = 30000,
      .idle_core_mask = (1 << 0), // ESP32-C3 是单核，使用 core 0
      .trigger_panic = true
  };
  esp_err_t wdtResult = esp_task_wdt_init(&wdt_config);
  if (wdtResult == ESP_ERR_INVALID_STATE) wdtResult = esp_task_wdt_reconfigure(&wdt_config);
  esp_err_t taskResult = esp_task_wdt_status(NULL);
  if (taskResult != ESP_OK) taskResult = esp_task_wdt_add(NULL);
  watchdogReady = wdtResult == ESP_OK && taskResult == ESP_OK;
  Serial.printf("看门狗初始化: %d, 任务注册: %d\n", wdtResult, taskResult);
  timerLastMs = esp_timer_get_time() / 1000ULL;
}

// ========== loop 函数 ==========
void loop() {
  unsigned long loopStarted = millis();
  checkSerial1URC();
  if (WiFi.status() != WL_CONNECTED && millis() - wifiRetryAt >= 30000) {
    WiFiMulti.run(2000);
    wifiRetryAt = millis();
    if (WiFi.status() == WL_CONNECTED) {
      MDNS.end();
      if (MDNS.begin(MDNS_HOSTNAME)) MDNS.addService("http", "tcp", 80);
      configTime(0, 0, "ntp.ntsc.ac.cn", "pool.ntp.org");
    }
  }
  // 处理 HTTP 请求
  server.handleClient();
  if (watchdogReady) esp_task_wdt_reset();
  checkSerial1URC();
  
  // 如果配置无效，每 30 秒打印一次提示
  if (!configValid) {
    if (millis() - lastPrintTime >= 30000) {
      lastPrintTime = millis();
      Serial.printf("请访问 %s 配置\n", getDeviceUrl().c_str());
    }
  }
  
  serviceAirplaneMode();
  if (watchdogReady) esp_task_wdt_reset();
  // 飞行期间只保留一个到期标记，恢复后执行一次。
  if (config.timerEnabled && timerIntervalSec > 0 && configValid) {
    uint64_t nowMs = esp_timer_get_time() / 1000ULL;
    if (nowMs - timerLastMs >= (uint64_t)timerIntervalSec * 1000ULL) {
      timerLastMs = nowMs;
      lastTimerExec = millis();
      timerPending = true;
    }
    if (timerPending && cellularReady()) {
      timerPending = false;
      if (config.timerType == 0) {
        String result;
        runModemPing("8.8.8.8", result);
      } else if (config.timerType == 1 && config.timerPhone.length() > 0) {
        bool success = sendSMS(config.timerPhone.c_str(), config.timerMessage.c_str());
        if (success) { stats.smsSent++; saveStats(); }
      }
    }
  }
  
  if (watchdogReady) esp_task_wdt_reset();
  checkSerial1URC();
  // 检查长短信超时
  checkConcatTimeout();
  
  // 本地按行查询共享AT事务，避免原始字节破坏CMGS/CFUN事务。
  static String consoleLine;
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n') {
      consoleLine.trim();
      if (consoleLine == "AT" || consoleLine == "ATI" || consoleLine == "AT+CFUN?" ||
          consoleLine == "AT+CPIN?" || consoleLine == "AT+CEREG?" || consoleLine == "AT+CGATT?")
        Serial.println(sendATCommand(consoleLine.c_str(), 1500));
      else Serial.println("串口仅允许只读查询；射频和短信请使用认证页面");
      consoleLine = String();
    } else if (c != '\r' && consoleLine.length() < 80) consoleLine += c;
  }
  
  // 检查 URC 和解析
  checkSerial1URC();
  
  if (watchdogReady) esp_task_wdt_reset();
  processNotifications();
  checkSerial1URC();
  unsigned long loopMs = millis() - loopStarted;
  if (loopMs > maxLoopMs) maxLoopMs = loopMs;
  if (millis() - diagnosticsAt >= 60000) {
    diagnosticsAt = millis();
    Serial.printf("health uptime=%llu heap=%u min_heap=%u largest=%u max_loop_ms=%lu queue=%u dropped=%lu\n",
      (unsigned long long)(esp_timer_get_time() / 1000000ULL), (unsigned int)ESP.getFreeHeap(), (unsigned int)ESP.getMinFreeHeap(),
      (unsigned int)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT), maxLoopMs,
      notificationQueueDepth(), notificationDropCount());
  }
  if (watchdogReady) esp_task_wdt_reset();
  delay(1);
}
