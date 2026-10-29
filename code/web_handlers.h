#ifndef WEB_HANDLERS_H
#define WEB_HANDLERS_H

#include <Arduino.h>
#include <WebServer.h>
#include "config.h"

extern WebServer server;
struct ConfigArg;
struct ConfigRequest;

// Web 页面处理
bool checkAuth();
void handleRoot();
void handleAppCss();
void handleToolsPage();
void handleQuery();
void handleSendSms();
void handlePing();
void handleTimer();
void handleSave();

// 新功能
void handleRestart();
void handleSmsHistory();
void handleClearSmsHistory();
void handleStorageInit();
void handleStats();
String htmlEscape(const String& text);

// AT 命令
String sendATCommand(const char* cmd, unsigned long timeout);
bool sendATandWaitOK(const char* cmd, unsigned long timeout);
bool waitCGATT1();
bool atResponseOK(const String& response);
bool atErrorLine(const String& line);
String waitATResponse(unsigned long timeout, bool prompt, bool ping);
bool validPingHost(const String& host);
bool runModemPing(const String& host, String& result);
void blink_short(unsigned long gap_time = 500);

#endif
