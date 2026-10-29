#ifndef AIRPLANE_MODE_H
#define AIRPLANE_MODE_H
#include <stdint.h>
#include <time.h>

// 纯调度逻辑与设备状态分离，供原生测试直接使用。
struct AirSchedule {
  bool overrideActive = false;
  bool overrideTarget = false;
  time_t overrideUntil = 0;
  bool targetKnown = false;
  bool target = false;
  static bool validTime(int start, int end) {
    return start >= 0 && start < 1440 && end >= 0 && end < 1440 && start != end;
  }
  static bool synced(time_t now) { return now >= 1700000000; }
  static bool inside(int minute, int start, int end) {
    return start < end ? minute >= start && minute < end : minute >= start || minute < end;
  }
  static time_t nextBoundary(time_t now, int start, int end) {
    time_t local = now + 28800;
    time_t day = local - local % 86400;
    time_t a = day + start * 60, b = day + end * 60;
    if (a <= local) a += 86400;
    if (b <= local) b += 86400;
    return (a < b ? a : b) - 28800;
  }
  void update(bool enabled, int start, int end, bool manual, time_t now) {
    bool clockValid = synced(now);
    if (overrideActive && enabled && clockValid) {
      if (!overrideUntil) overrideUntil = nextBoundary(now, start, end);
      if (now >= overrideUntil) clearOverride();
    }
    targetKnown = !enabled || overrideActive || clockValid;
    if (!enabled) target = manual;
    else if (overrideActive) target = overrideTarget;
    else if (clockValid) target = inside((now + 28800) % 86400 / 60, start, end);
  }
  void clearOverride() { overrideActive = false; overrideUntil = 0; }
  void manual(bool value, bool enabled, int start, int end, time_t now) {
    overrideActive = enabled;
    overrideTarget = value;
    overrideUntil = enabled && synced(now) ? nextBoundary(now, start, end) : 0;
    target = value; targetKnown = true;
  }
};

class ATTransaction {
 public:
  ATTransaction();
  ~ATTransaction();
  explicit operator bool() const { return acquired; }
 private:
  bool acquired;
  ATTransaction(const ATTransaction&) = delete;
  ATTransaction& operator=(const ATTransaction&) = delete;
};
bool modemTransactionBusy();
String transactionATCommand(const char* cmd, unsigned long timeout);
void markATTimeout();
void initAirplaneMode();
void serviceAirplaneMode();
bool cellularReady();
String cellularBlockReason();
void handleAirplaneStatus();
void handleAirplaneSave();
void handleAirplaneAction();
#endif
