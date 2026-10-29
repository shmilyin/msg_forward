#ifndef PUSH_SERVICE_H
#define PUSH_SERVICE_H

#include <Arduino.h>
#include "config.h"

// 函数声明
int sendHttpRequest(const String& url, const String& method, const String& contentType,
                    const String& body, bool feishu = false);
String urlEncode(const String& str);
String jsonEscape(const String& str);
void sendToChannel(const PushChannel& channel, const char* sender, const char* message, const char* timestamp);
void sendSMSToServer(const char* sender, const char* message, const char* timestamp);
void sendEmailNotification(const char* subject, const char* body);
bool enqueueSmsNotification(const char* sender, const char* message, const char* timestamp);
void processNotifications();
unsigned int notificationQueueDepth();
unsigned long notificationDropCount();

#endif // PUSH_SERVICE_H
