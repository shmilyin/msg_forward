#ifndef STK_H
#define STK_H
struct StkProbe {
  unsigned long lines = 0;
  unsigned int count = 0;
  bool terminated = false, error = false, truncated = false;
  String candidates[32];
  bool collectRaw = false;
  String raw;
};
struct StkMenu {
  bool available = false;
  unsigned int count = 0;
  String title;
  String items[16];
  unsigned int ids[16] = {};
};
void observeStkUrc(const String& line);
void handleStkMenu();
void handleStkProbe();
#endif
