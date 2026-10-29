"""提取实际固件函数，使用离线串口/PDU mock 编译和运行边界测试。"""
from pathlib import Path
import subprocess
import os
import tempfile

ROOT = Path(__file__).resolve().parent
sms = (ROOT / 'code/sms_handler.ino').read_text(encoding='utf-8')
web = (ROOT / 'code/web_handlers.ino').read_text(encoding='utf-8')
# 网络发送段由主代理维护；测试以计数器替代，确保AT中不会调用它。
start = sms.index('void processSmsContent(')
end = sms.index('// 分批读取', start)
sms = sms[:start] + sms[end:]
web = web[web.index('static bool atBusy'):web.index('// 重启模组')]
air_header = (ROOT / 'code/airplane_mode.h').read_text(encoding='utf-8')
transaction_class = air_header[air_header.index('class ATTransaction'):air_header.index('bool modemTransactionBusy') ]
mock = r'''
#include <string>
#include <deque>
#include <functional>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <iostream>
class String {
 public:
 std::string s;
 String(const char* p=""):s(p?p:""){} String(std::string p):s(p){} String(int n):s(std::to_string(n)){}
 unsigned int length() const{return s.size();} const char* c_str()const{return s.c_str();}
 char operator[](unsigned i)const{return i<s.size()?s[i]:0;} char charAt(unsigned i)const{return (*this)[i];}
 String substring(unsigned a,unsigned b=~0u)const{if(a>b)std::swap(a,b); a=std::min(a,(unsigned)s.size()); b=std::min(b,(unsigned)s.size());return s.substr(a,b-a);}
 int indexOf(const String& x,unsigned p=0)const{auto n=s.find(x.s,p);return n==s.npos?-1:(int)n;}
 int indexOf(char c,unsigned p=0)const{auto n=s.find(c,p);return n==s.npos?-1:(int)n;}
 int lastIndexOf(char c)const{auto n=s.rfind(c);return n==s.npos?-1:(int)n;}
 bool endsWith(const char* p)const{std::string x=p;return s.size()>=x.size()&&s.compare(s.size()-x.size(),x.size(),x)==0;}
 bool startsWith(const char* p)const{return s.rfind(p,0)==0;} bool equals(const char* p)const{return s==p;}
 long toInt()const{return std::strtol(s.c_str(),nullptr,10);}
 void trim(){auto a=s.find_first_not_of(" \r\n\t");if(a==s.npos){s.clear();return;}s=s.substr(a,s.find_last_not_of(" \r\n\t")-a+1);}
 String& operator+=(const String& x){s+=x.s;return *this;} String& operator+=(char c){s+=c;return *this;}
 friend String operator+(String a,const String& b){a+=b;return a;}
 friend bool operator==(const String&a,const String&b){return a.s==b.s;}
 friend bool operator!=(const String&a,const String&b){return !(a==b);}
};
unsigned long clockMs=0, yields=0;
unsigned long millis(){return clockMs;}
void yield(){++yields;++clockMs;} void delay(unsigned long n){clockMs+=n;++yields;}
struct HardwareSerial {
 std::deque<char> rx; bool flood=false; std::function<void(String)> onCommand; String tx;
 int available(){return flood||!rx.empty();} int read(){if(!rx.empty()){char c=rx.front();rx.pop_front();return (unsigned char)c;}return flood?'X':-1;}
 void feed(const std::string&s){for(char c:s)rx.push_back(c);}
 void println(const String&s){if(s=="AT+CGACT?"){feed("+CGACT: 1,0\nOK\n");return;}if(onCommand)onCommand(s);} void println(){}
 void print(const String& value){tx+=value;} template<class... T>void printf(const char*,T...){}
 void write(int c){if(c==26)feed("\r\n+CMT: ,2\r\nAABB\r\n+CMGS: 1\r\nOK\r\n");}
} Serial1, Serial;
#define MAX_CONCAT_MESSAGES 5
#define MAX_CONCAT_PARTS 10
#define SERIAL_BUFFER_SIZE 500
#define CONCAT_TIMEOUT_MS 30000
struct SmsPart{bool valid;String text;};
struct ConcatSms{bool inUse;int refNumber;String sender,timestamp;int totalParts,receivedParts;unsigned long firstPartTime;SmsPart parts[MAX_CONCAT_PARTS];} concatBuffer[MAX_CONCAT_MESSAGES];
struct PDU{
 int info[3]={0,0,0};int decodes=0;String data="AABB";
 void setSCAnumber(){} int encodePDU(const char*,const char*){data="1122";return 2;}
 const char*getSMS(){return data.c_str();} bool decodePDU(const char*){++decodes;data="FFFF";return true;}
 const char*getSender(){return "123";}const char*getText(){return "hello";}const char*getTimeStamp(){return "26010100000032";}int*getConcatInfo(){return info;}
}pdu;
int processed=0;
void processSmsContent(const char*,const char*,const char*){++processed;}
String readModemLine(bool);bool dispatchModemURC(const String&);bool isHexString(const String&);
String sendATCommand(const char*,unsigned long);String waitATResponse(unsigned long,bool,bool);bool atResponseOK(const String&);
void clearConcatSlot(int);
bool cellularReady(){return true;} String cellularBlockReason(){return "blocked";}
void markATTimeout();
String transactionATCommand(const char*,unsigned long);
'''
tests = r'''
void resetRx(){atTimedOut=false;Serial1.flood=false;Serial1.rx.clear();Serial1.feed("\n");readModemLine(false);waitingSmsPdu=false;smsPduCount=0;smsPduHead=0;Serial1.onCommand=nullptr;}
int main(){
 initConcatBuffer();
 assert(findOrCreateConcatSlot(1,"x",11)==-1);
 assert(findOrCreateConcatSlot(1,"x",0)==-1);
 int slot=findOrCreateConcatSlot(1,"x",2);assert(slot>=0);
 assert(findOrCreateConcatSlot(1,"x",3)==-1);
 assert(assembleConcatSms(-1)=="");assert(assembleConcatSms(5)=="");clearConcatSlot(-1);clearConcatSlot(5);
 concatBuffer[slot].totalParts=999;assert(assembleConcatSms(slot)=="");
 initConcatBuffer();clockMs=20;
 for(int i=0;i<5;i++){concatBuffer[i].inUse=true;concatBuffer[i].refNumber=i;concatBuffer[i].sender="x";concatBuffer[i].firstPartTime=i==0?~0UL-100:10+i;}
 assert(findOrCreateConcatSlot(99,"x",2)==0);
 assert(formatTimestamp("26000112000032")=="26000112000032");
 assert(formatTimestamp("26130112000032")=="26130112000032");
 assert(formatTimestamp("26023112000032")=="26023112000032");
 assert(formatTimestamp("24123120000000")=="2025-01-01 04:00:00");
 assert(formatTimestamp("24022920000000")=="2024-03-01 04:00:00");
 resetRx();Serial1.onCommand=[](String){Serial1.feed("\r\n+CMT: ,2\r\nAABB\r\nTOKEN_OK_TEXT\r\nOK\r\n");};
 String r=sendATCommand("ATI",200);assert(atResponseOK(r));assert(smsPduCount==1);assert(processed==0&&pdu.decodes==0);
 checkSerial1URC();assert(processed==1&&pdu.decodes==1);
 resetRx();Serial1.feed("+CMT: ,2\r\nAA");while(Serial1.available())dispatchModemURC(readModemLine(false));
 Serial1.onCommand=[](String){Serial1.feed("BB\r\nOK\r\n");};assert(atResponseOK(sendATCommand("ATI",200)));assert(smsPduCount==1);
 resetRx();for(int i=0;i<9;i++){dispatchModemURC("+CMT: ,2");dispatchModemURC("AABB");}assert(smsPduCount==8);
 resetRx();Serial1.onCommand=[](String){Serial1.feed(std::string(600,'X')+"OK\nERROR\n");};r=sendATCommand("ATI",300);assert(!atResponseOK(r));assert(r.indexOf("ERROR")>=0);
 resetRx();Serial1.onCommand=[](String){for(int i=0;i<100;i++)Serial1.feed(std::string(100,'A')+"\n");Serial1.feed("OK\n");};r=sendATCommand("ATI",1000);assert(r.length()<=2048&&atResponseOK(r));
 resetRx();Serial1.flood=true;unsigned long begin=clockMs,y=yields;r=sendATCommand("ATI",100);assert(clockMs-begin<=125&&yields>y);resetRx();
 clockMs=~0UL-30;begin=clockMs;r=waitATResponse(100,false,false);assert((unsigned long)(clockMs-begin)>=100);
 resetRx();Serial1.onCommand=[](String c){if(c.startsWith("AT+MPING="))Serial1.feed("OK\r\n+CMT: ,2\r\nAABB\r\n+MPING: 2,\"1.2.3.4\",999\r\n");else Serial1.feed("OK\r\n");};
 String result;assert(!runModemPing("example.com",result));assert(result.startsWith("2,"));assert(smsPduCount==1);
 Serial1.onCommand=[](String c){Serial1.feed(c.startsWith("AT+MPING=")?"OK\r\n+MPING: 0,\"1.2.3.4\",32,20,64\r\n":"OK\r\n");};assert(runModemPing("8.8.8.8",result));
 resetRx();begin=clockMs;assert(!runModemPing("8.8.8.8",result));assert(clockMs-begin<=15100);
 resetRx();Serial1.onCommand=[](String c){if(c.startsWith("AT+MPING="))Serial1.feed("OK\r\n");else Serial1.feed("OK\r\n");};begin=clockMs;assert(!runModemPing("8.8.8.8",result));assert(clockMs-begin>=10000&&clockMs-begin<=12000);
 assert(!validPingHost("x\"\r\nAT"));assert(!validPingHost("x,y"));assert(!validPingHost(String(std::string(254,'a'))));
 resetRx();Serial1.onCommand=[](String){Serial1.feed("+CMT: ,2\r\nAABB\r\n> ");};int before=processed;assert(sendSMS("123","hello"));assert(processed==before&&smsPduCount==2);assert(Serial1.tx=="1122");
 resetRx();pdu.info[1]=3;pdu.info[2]=2;dispatchModemURC("+CMT: ,2");dispatchModemURC("AABB");before=processed;checkSerial1URC();assert(processed==before);
 pdu.info[1]=0;pdu.info[2]=11;dispatchModemURC("+CMT: ,2");dispatchModemURC("AABB");checkSerial1URC();assert(processed==before);
 resetRx();initConcatBuffer();pdu.info[0]=42;pdu.info[1]=2;pdu.info[2]=2;
 auto deliver=[](){dispatchModemURC("+CMT: ,2");dispatchModemURC("AABB");checkSerial1URC();};
 before=processed;deliver();deliver();assert(processed==before);assert(concatBuffer[0].receivedParts==1);
 pdu.info[1]=1;deliver();assert(processed==before+1&&!concatBuffer[0].inUse);
 resetRx();Serial1.onCommand=[](String){Serial1.feed("TOKEN_OK_TEXT\n");};begin=clockMs;r=sendATCommand("ATI",100);assert(!atResponseOK(r)&&clockMs-begin>=100);
 resetRx();Serial1.onCommand=[](String){Serial1.feed("+CMS ERROR: 500\n");};r=sendATCommand("ATI",100);assert(r.indexOf("+CMS ERROR:")>=0&&!atResponseOK(r));
 resetRx();begin=clockMs;assert(!sendSMS("123","hello"));assert(clockMs-begin>=5000&&clockMs-begin<=5050);
 resetRx();pdu.info[1]=0;pdu.info[2]=0;Serial1.onCommand=[](String){Serial1.feed("OK\n+CMT: ,2\nAABB\n");};assert(atResponseOK(sendATCommand("ATI",200)));assert(smsPduCount==0);before=processed;checkSerial1URC();assert(processed==before+1);
 resetRx();Serial1.flood=true;begin=clockMs;checkSerial1URC();assert(clockMs-begin<=12);resetRx();
 std::cout<<"PASS: actual AT/SMS functions, URC queue, bounds, flood timeout, wrap, timestamp, ping, sendSMS\n";
}
'''
with tempfile.TemporaryDirectory(prefix='msg_forward_mock_') as tmp:
    source = Path(tmp) / 'test.cpp'
    exe = Path(tmp) / 'test.exe'
    source.write_text(mock + transaction_class + web + sms + tests, encoding='utf-8')
    compiler = ROOT / '.tools/w64devkit/bin/g++.exe'
    env = dict(os.environ)
    env['PATH'] = str(compiler.parent) + os.pathsep + env.get('PATH', '')
    subprocess.run([str(compiler), '-std=c++11', '-Wall', '-Wextra', '-Wno-sign-compare', '-O0', '-g', str(source), '-o', str(exe)], check=True, env=env)
    subprocess.run([str(exe)], check=True, timeout=20)
# 确认两个入口实际调用共享helper，没有遗留独立串口读取。
query = (ROOT / 'code/web_query.ino').read_text(encoding='utf-8')
assert 'runModemPing(host,' in query
assert 'Serial1.read()' not in query
assert not (ROOT / 'code/mqtt_handler.ino').exists()
print('PASS: Web shared ping helper integration and MQTT removal')
