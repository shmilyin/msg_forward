#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <Preferences.h>
#include <vector>

// 串口映射
#define TXD 3
#define RXD 4

// mDNS 主机名（访问地址为 http://sms.local）
#define MDNS_HOSTNAME "sms"

// 推送通道类型
enum PushType {
  PUSH_TYPE_NONE = 0,      // 未启用
  PUSH_TYPE_POST_JSON = 1, // POST JSON格式 {"sender":"xxx","message":"xxx","timestamp":"xxx"}
  PUSH_TYPE_BARK = 2,      // Bark格式 POST {"title":"xxx","body":"xxx"}
  PUSH_TYPE_GET = 3,       // GET请求，参数放URL中
  PUSH_TYPE_CUSTOM = 4,    // 自定义模板
  // 数值 5、6、7 保留不用，禁止复用旧配置类型。
  PUSH_TYPE_FEISHU = 8     // 飞书群聊机器人
};

// 最大推送通道数
#define MAX_PUSH_CHANNELS 3

// 短信历史记录设置
#define MAX_SMS_HISTORY 100

// 推送通道配置
struct PushChannel {
  bool enabled;
  PushType type;
  String name;
  String url;
  String key1;
  String key2;
  String customBody;
};

// 统计计数器结构
struct Statistics {
  unsigned long smsReceived;    // 收到短信数
  unsigned long smsSent;        // 发送短信数
  unsigned long pushSuccess;    // 推送成功数
  unsigned long pushFailed;     // 推送失败数
  unsigned long bootCount;      // 启动次数
};

// WiFi 配置结构
#define MAX_WIFI_NETWORKS 3
struct WifiNetwork {
  String ssid;
  String password;
  bool enabled;
};

// 配置参数结构体
struct Config {
  // WiFi 配置（支持多个网络）
  WifiNetwork wifiNetworks[MAX_WIFI_NETWORKS];
  
  String smtpServer;
  int smtpPort;
  String smtpUser;
  String smtpPass;
  String smtpSendTo;

  bool emailEnabled;
  PushChannel pushChannels[MAX_PUSH_CHANNELS];
  String webUser;
  String webPass;
  
  // 定时任务配置
  bool timerEnabled;
  int timerType;
  int timerInterval;
  String timerPhone;
  String timerMessage;
  
  bool airplaneEnabled;
  int airplaneStartMinute;
  int airplaneEndMinute;
  bool airplaneManualTarget;
};

// 默认Web管理账号密码
#define DEFAULT_WEB_USER "admin"
#define DEFAULT_WEB_PASS "admin123"

// 长短信合并相关定义
#define MAX_CONCAT_PARTS 10
#define CONCAT_TIMEOUT_MS 30000
#define MAX_CONCAT_MESSAGES 5

#define SERIAL_BUFFER_SIZE 500
#define MAX_PDU_LENGTH 300

// 长短信分段结构
struct SmsPart {
  bool valid;
  String text;
};

// 长短信缓存结构
struct ConcatSms {
  bool inUse;
  int refNumber;
  String sender;
  String timestamp;
  int totalParts;
  int receivedParts;
  unsigned long firstPartTime;
  SmsPart parts[MAX_CONCAT_PARTS];
};

// 全局变量声明 (extern)
extern Config config;
extern Preferences preferences;
extern bool configValid;
extern unsigned long lastPrintTime;
extern unsigned long lastTimerExec;
extern unsigned long timerIntervalSec;
extern ConcatSms concatBuffer[MAX_CONCAT_MESSAGES];
extern char serialBuf[SERIAL_BUFFER_SIZE];
extern int serialBufLen;

// 短信历史和统计
extern Statistics stats;

// 函数声明
void saveConfig();
void loadConfig();
bool isPushTypeSupported(int type);
bool isPushChannelValid(const PushChannel& ch);
bool isConfigValid();
String getDeviceUrl();
void initSmsStorage();
bool smsStorageReady();
bool initializeSmsStorage(const String& confirmation);
void addSmsToHistory(const char* sender, const char* message, const char* timestamp);
void streamSmsHistory();
void recoverSmsStorage();
bool clearSmsHistory();
void saveStats();
void loadStats();

#endif // CONFIG_H
