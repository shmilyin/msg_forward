"""离线编译真实配置 handler；不联网、不接设备。"""
from pathlib import Path
import os
import subprocess
import tempfile
from test_notifications import function

ROOT = Path(__file__).resolve().parent
source = (ROOT / 'code/web_handlers.ino').read_text(encoding='utf-8')
config = (ROOT / 'code/config.ino').read_text(encoding='utf-8')
header = (ROOT / 'code/config.h').read_text(encoding='utf-8')
mock = r'''
#include <string>
#include <vector>
#include <map>
#include <cassert>
#include <iostream>
#include <cstdint>
#include <memory>
#include <new>
// 只注入生产代码显式 nothrow 分配故障，标准容器保持正常。
int allocationCountdown=-1;
void* operator new(std::size_t n,const std::nothrow_t&) noexcept {
 if(allocationCountdown==0)return nullptr;
 if(allocationCountdown>0)--allocationCountdown;
 try{return ::operator new(n);}catch(...){return nullptr;}
}
void* operator new[](std::size_t n,const std::nothrow_t&) noexcept {
 if(allocationCountdown==0)return nullptr;
 if(allocationCountdown>0)--allocationCountdown;
 try{return ::operator new[](n);}catch(...){return nullptr;}
}
class String {
public:
 std::string s;
 String(const char* p=""):s(p){} String(std::string p):s(p){}
 String(int n):s(std::to_string(n)){} String(unsigned long n):s(std::to_string(n)){}
 String(unsigned long long n):s(std::to_string(n)){}
 unsigned length()const{return s.size();} const char*c_str()const{return s.c_str();}
 char operator[](unsigned i)const{return s[i];}
 String substring(unsigned a,unsigned b)const{return s.substr(a,b-a);}
 bool startsWith(const String& p)const{return s.compare(0,p.length(),p.s)==0;}
 String& operator+=(char c){s+=c;return *this;}
 friend String operator+(const String&a,const String&b){return a.s+b.s;}
 friend bool operator==(const String&a,const String&b){return a.s==b.s;}
 friend bool operator!=(const String&a,const String&b){return !(a==b);}
};
'''
# 使用生产配置结构，避免 mock 字段偏离。
structs = header[header.index('enum PushType'):header.index('// 默认Web管理账号密码')]
mock += structs + r'''
Config config; bool configValid=false,timerPending=true;
unsigned long timerIntervalSec=86400; uint64_t timerLastMs=123;
int saves=0,sent=0; bool authorized=true,ready=true,smsSuccess=true;
String sentPhone,sentContent;
int64_t esp_timer_get_time(){return 9000000;}
void saveConfig(){++saves;} bool isConfigValid(){return true;}
void saveStats(){} Statistics stats={};
bool cellularReady(){return ready;} String cellularBlockReason(){return "blocked";}
String jsonEscape(const String& s){return s;}
bool smsUnknown=false,unknownOnSend=false;
bool smsSendResultUnknown(){return smsUnknown;}
unsigned long smsPduDropCount(){return 7;}
bool sendSMS(const char*p,const char*c){++sent;sentPhone=p;sentContent=c;if(unknownOnSend)smsUnknown=true;return smsSuccess;}
struct {int getMinFreeHeap(){return 123;}int getFreeHeap(){return 456;}}ESP;
struct {int RSSI(){return -50;}}WiFi;
const int MALLOC_CAP_8BIT=1;unsigned long maxLoopMs=4;
int heap_caps_get_largest_free_block(int){return 100;}int notificationQueueDepth(){return 2;}
int notificationDropCount(){return 3;}int esp_reset_reason(){return 1;}
const int HTTP_POST=1;
struct Server {
 std::vector<std::pair<String,String>> params; int verb=HTTP_POST,status=0; String output;
 int method(){return verb;} int args(){return params.size();}
 bool hasArg(const String& k){for(auto&a:params)if(a.first==k)return true;return false;}
 String arg(const String& k){for(auto&a:params)if(a.first==k)return a.second;return "";}
 String arg(int n){return params[n].second;} String argName(int n){return params[n].first;}
 void sendHeader(const char*,const char*){}
 void send(int code,const char*,const String& body){status=code;output=body;}
}server;
bool checkAuth(){if(!authorized)server.status=401;return authorized;}
struct {bool mount=false,formatOK=true;int formats=0,begins=0;bool begin(bool erase){assert(!erase);++begins;return mount;}bool format(){++formats;if(formatOK)mount=true;return formatOK;}}SPIFFS;
void recoverSmsStorage(){}
struct {void println(const char*){}}Serial;
'''
actual = config[config.index('static bool smsStorageMounted'):config.index('bool clearSmsHistory()')]
actual += source[source.index('// 仅接受有界的扁平对象'):source.index('void handleRestart()')]
actual += '\n' + function(source, 'void handleStats(')
tests = r'''
void request(std::initializer_list<std::pair<String,String>> args){server.params=args;server.status=0;server.verb=HTTP_POST;}
void timerJson(const char* body){request({{"plain",body}});handleTimer();}
int main(){
 config.smtpServer="original";config.smtpPort=465;config.webPass="secret";
 config.timerEnabled=true;config.timerType=0;config.timerInterval=1;config.timerMessage="keep";
 request({{"smtpServer","new"}});handleSave();assert(server.status==200&&config.smtpServer=="new"&&timerPending&&timerLastMs==123&&saves==1);
 request({{"timerEn","true"},{"timerType","0"},{"timerInterval","1"},{"timerPhone",""},{"timerMessage","keep"}});handleSave();assert(server.status==200&&timerPending&&timerLastMs==123);
 auto oldSaves=saves;
 request({{"smtpServer","bad"},{"smtpPort","465x"}});handleSave();assert(server.status==400&&config.smtpServer=="new"&&saves==oldSaves);
 request({{"smtpServer","bad"},{"smtpServer","duplicate"}});handleSave();assert(server.status==400&&config.smtpServer=="new");
 request({{"timerEn","true"},{"timerType","1"},{"timerInterval","3"},{"timerPhone","+12345"},{"timerMessage","hello"}});handleTimer();
 assert(server.status==200&&config.timerInterval==3&&config.smtpServer=="new"&&config.webPass=="secret"&&!timerPending&&timerLastMs==9000&&timerIntervalSec==259200);
 timerPending=true;timerLastMs=456;
 timerJson(R"({ "enabled": true, "type": 1, "interval": 3, "phone": "+12345", "message": "hello" })");assert(server.status==200&&timerPending&&timerLastMs==456);
 timerJson(R"({"message":"quote\"\\\n\u4e2d\ud83d\ude00","phone":"+12345","interval":4,"type":1,"enabled":true})");assert(server.status==200&&config.timerMessage.s=="quote\"\\\n中😀"&&config.smtpServer=="new");
 oldSaves=saves;String oldMessage=config.timerMessage;
 for(const char* bad:{
 R"({"enabled":true,"type":1,"interval":0,"phone":"123","message":"x"})",
 R"({"enabled":true,"type":1,"interval":3651,"phone":"123","message":"x"})",
 R"({"enabled":"true","type":1,"interval":1,"phone":"123","message":"x"})",
 R"({"enabled":true,"type":1,"interval":1.0,"phone":"123","message":"x"})",
 R"({"enabled":true,"type":1,"interval":01,"phone":"123","message":"x"})",
 R"({"enabled":true,"type":1,"interval":1,"phone":"+","message":"x"})",
 R"({"enabled":true,"type":1,"interval":1,"phone":"123","message":"\ud800"})",
 R"({"enabled":true,"type":1,"interval":1,"phone":"123","message":"\u0000"})",
 R"({"enabled":true,"type":1,"interval":1,"phone":"123","message":"x",})",
 R"({"enabled":true,"type":1,"interval":1,"phone":"123","message":"x"} garbage)",
 R"({"enabled":true,"type":1,"interval":1,"phone":"123","phone":"456"})",
 R"({"enabled":true,"type":1,"interval":1,"phone":"123"})",
 R"({"enabled":true,"type":1,"interval":1,"phone":"123","smtpServer":"bad"})",
 R"({"enabled":true,"type":1,"interval":1,"phone":"123","message":{}})"}){
 timerJson(bad);assert(server.status==400&&saves==oldSaves&&config.timerMessage==oldMessage);
 }
 request({{"timerEn","true"},{"timerType","0"},{"timerInterval","1"},{"timerPhone",""},{"smtpServer","bad"}});handleTimer();assert(server.status==400&&config.smtpServer=="new");
 request({{"plain",R"({"phone":"+123","content":"a\"b\n\u4e2d"})"}});handleSendSms();assert(server.status==200&&sent==1&&sentContent.s=="a\"b\n中"&&stats.smsSent==1);
 assert(server.output.s.find("\"unknown\":false")!=std::string::npos);
 smsSuccess=false;handleSendSms();assert(sent==2&&stats.smsSent==1&&server.output.s.find("\"unknown\":false")!=std::string::npos);smsSuccess=true;
 for(const char* bad:{R"({"phone":"+","content":"x"})",R"({"phone":123,"content":"x"})",R"({"phone":"123","content":""})",R"({"phone":"123","content":"x","content":"y"})"}){
 request({{"plain",bad}});handleSendSms();assert(server.status==400&&sent==2);
 }
 request({{"phone","123"},{"content","form"}});ready=false;handleSendSms();assert(server.status==409&&sent==2);ready=true;
 for(auto handler:{handleSave,handleTimer,handleSendSms,handleStorageInit}){
  authorized=false;handler();assert(server.status==401);authorized=true;
  server.verb=0;handler();assert(server.status==405);server.verb=HTTP_POST;
 }
 assert(saves==oldSaves&&sent==2);
 // 发送后结果未知不能计为成功，且再次请求不得触发重发。
 request({{"phone","123"},{"content","uncertain"}});unknownOnSend=true;handleSendSms();
 assert(server.status==200&&sent==3&&stats.smsSent==1&&server.output.s.find("\"unknown\":true")!=std::string::npos);
 assert(server.output.s.find("请勿重复发送")!=std::string::npos&&server.output.s.find("人工核实后重启")!=std::string::npos);
 handleSendSms();assert(server.status==409&&sent==3&&server.output.s.find("\"unknown\":true")!=std::string::npos);
 unknownOnSend=false;smsUnknown=false;
 // 字段数组以及配置快照分配失败均提前返回，不写配置或发送。
 for(auto handler:{handleSave,handleTimer,handleSendSms,handleStorageInit}){
  request({{"smtpServer","not saved"}});allocationCountdown=0;handler();allocationCountdown=-1;
  assert(server.status==503&&saves==oldSaves&&sent==3&&config.smtpServer=="new");
 }
 request({{"smtpServer","not saved"}});allocationCountdown=1;handleSave();allocationCountdown=-1;
 assert(server.status==503&&saves==oldSaves&&config.smtpServer=="new");
 request({{"timerEn","true"},{"timerType","0"},{"timerInterval","1"},{"timerPhone",""},{"timerMessage","x"}});
 allocationCountdown=1;handleTimer();allocationCountdown=-1;assert(server.status==503&&saves==oldSaves);
 static_assert(sizeof(ConfigRequest)<=32,"请求对象不得把字段数组放在栈上");
 SPIFFS.mount=false;initSmsStorage();assert(!smsStorageReady()&&SPIFFS.formats==0);
 request({{"confirm","wrong"}});handleStorageInit();assert(server.status==400&&SPIFFS.formats==0);
 request({{"confirm","ERASE_UNMOUNTED_STORAGE"}});SPIFFS.mount=true;handleStorageInit();assert(server.status==409&&SPIFFS.formats==0);
 SPIFFS.mount=false;initSmsStorage();SPIFFS.formatOK=false;handleStorageInit();assert(server.status==500&&SPIFFS.formats==1);
 SPIFFS.formatOK=true;handleStorageInit();assert(server.status==200&&SPIFFS.formats==2&&smsStorageReady());
 handleStorageInit();assert(server.status==409&&SPIFFS.formats==2);
 for(bool state:{false,true}){
  SPIFFS.mount=state;initSmsStorage();smsUnknown=state;handleStats();
  assert(server.status==200&&server.output.s.find(state?"\"storageReady\":true":"\"storageReady\":false")!=std::string::npos);
  assert(server.output.s.find(state?"\"smsResultUnknown\":true":"\"smsResultUnknown\":false")!=std::string::npos);
  assert(server.output.s.find("\"pduDropped\":7")!=std::string::npos&&server.output.s.find("secret")==std::string::npos);
 }
 authorized=false;handleStats();assert(server.status==401);authorized=true;
 std::cout<<"PASS: actual handlers; unknown send/no retry, boolean storage/SMS stats, PDU drops, RAII allocation failures, atomic config and legacy JSON regression; sizeof(ConfigRequest)="<<sizeof(ConfigRequest)<<", heap fields="<<sizeof(ConfigArg)*64<<", heap Config="<<sizeof(Config)<<" (host mock)\n";
}
'''
assert "post('/timer',d)" in source
assert "post('/save',new URLSearchParams(new FormData($('cf'))))" not in source
with tempfile.TemporaryDirectory(prefix='config_api_') as tmp:
    cpp = Path(tmp) / 'test.cpp'
    exe = Path(tmp) / 'test.exe'
    cpp.write_text(mock + actual + tests, encoding='utf-8')
    compiler = ROOT / '.tools/w64devkit/bin/g++.exe'
    env = dict(os.environ, PATH=str(compiler.parent) + os.pathsep + os.environ.get('PATH', ''))
    subprocess.run([str(compiler), '-std=c++11', '-Wall', '-Wextra', '-Werror', '-finput-charset=UTF-8', '-fexec-charset=UTF-8', str(cpp), '-o', str(exe)], check=True, env=env)
    subprocess.run([str(exe)], check=True, env=env)
