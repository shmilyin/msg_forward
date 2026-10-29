"""离线提取通知实际代码；原生 mock 回归与本地 ReadyMail 语义审查。

运行：python test_notifications.py
按本地 ReadyMail 实际契约验证异步状态推进，所有测试均须通过。
不链接网络库；C++ 和可执行文件仅写入自动清理的临时目录。
"""
from pathlib import Path
import os
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parent
LIB = ROOT / '.arduino/user/libraries/ReadyMail/src/smtp'


def function(source, signature):
    """按下一行独立闭括号提取这些文件中的完整函数（保留实际函数体）。"""
    start = source.index(signature)
    opening = source.index('{', start)
    # 调用点均以签名开头；闭括号缩进从原文件签名所在行获取。
    line_start = source.rfind('\n', 0, start) + 1
    indent = source[line_start:start]
    end = re.search(r'(?m)^' + re.escape(indent) + r'}', source[opening:])
    if end is None:
        raise ValueError('未找到函数结束：' + signature)
    return source[start:opening + end.end()]


MOCK = r'''
#include <string>
#include <algorithm>
#include <cstring>
#include <ctime>
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <new>
#include <cstddef>
// 计数所有经 C++ new/delete 的实时分配，而非只比较队列深度或 RSS。
struct alignas(std::max_align_t) Allocation { size_t bytes; };
static size_t liveBytes=0, liveBlocks=0;
void* operator new(size_t n) {
    auto p=static_cast<Allocation*>(std::malloc(sizeof(Allocation)+n));
    if(!p) throw std::bad_alloc();
    p->bytes=n; liveBytes+=n; ++liveBlocks; return p+1;
}
void operator delete(void* p) noexcept {
    if(p) { auto h=static_cast<Allocation*>(p)-1; liveBytes-=h->bytes; --liveBlocks; std::free(h); }
}
void* operator new[](size_t n) { return ::operator new(n); }
void operator delete[](void* p) noexcept { ::operator delete(p); }
#define CHECK(x) do { if(!(x)) { std::cerr<<"FAIL line "<<__LINE__<<": "<<#x<<"\n"; std::exit(1); } } while(0)
class String {
public:
    std::string s;
    String(const char* p=""):s(p?p:""){} String(std::string p):s(p){}
    size_t length()const{return s.size();} const char* c_str()const{return s.c_str();}
    String substring(unsigned a,unsigned b=~0u)const {
        if(a>b)std::swap(a,b); a=std::min(a,(unsigned)s.size());
        b=std::min(b,(unsigned)s.size()); return s.substr(a,b-a);
    }
    friend String operator+(const String&a,const String&b){return a.s+b.s;}
    friend bool operator==(const String&a,const String&b){return a.s==b.s;}
};
unsigned long clockMs=0;
unsigned long millis(){return clockMs;}
time_t epoch=1700000000;
time_t mockTime(time_t* p){if(p)*p=epoch; return epoch;}
#define time mockTime
struct { void println(const char*){} } Serial;
const int WL_CONNECTED=3;
struct { int state=WL_CONNECTED; int status(){return state;} } WiFi;
struct { int stops=0; void stop(){++stops;} } ssl_client;
__CHANNEL_CONSTANTS__
struct PushChannel { bool enabled=false; int type=PUSH_TYPE_POST_JSON; String url,key1; };
struct Config {
    bool emailEnabled=true; String smtpServer="mock.invalid",smtpUser="a@example.invalid";
    String smtpPass="mock",smtpSendTo="b@example.invalid"; int smtpPort=465;
    PushChannel pushChannels[MAX_PUSH_CHANNELS];
} config;
struct { int pushSuccess=0,pushFailed=0; } stats;
int saved=0,httpCalls=0,lastChannel=-1;
String lastSender,lastMessage,lastTimestamp;
void saveStats(){++saved;}
void sendToChannel(const PushChannel& ch,const char* a,const char* b,const char* c){
    ++httpCalls; lastChannel=&ch-config.pushChannels;
    lastSender=a; lastMessage=b; lastTimestamp=c;
}
// 验证码解析属于上游依赖，本测试只控制其结果。
String extractVerifyCode(const String& s){return s=="code 123456"?"123456":"";}
enum { rfc822_from,rfc822_to,rfc822_subject,readymail_auth_password };
struct SMTPMessage {
    struct Headers { String values[3]; void add(int k,const char* p){values[k]=p;} } headers;
    struct Text { String value; void body(const char* p){value=p;} } text;
    time_t timestamp=0;
    void clear(){*this=SMTPMessage();}
};
struct Status { bool isComplete=false; int errorCode=0; };
struct SMTP {
    bool connectOK=true,authOK=true,sendOK=true,authenticated=false,connected=false;
    bool server_greeting_ack=false;
    int phase=0,event=0,connects=0,auths=0,sends=0,loops=0,stops=0;
    int failInsideLoop=0; unsigned long loopCost=0,connectCost=0;
    Status state; SMTPMessage captured,local_msg; SMTPMessage* msg_ptr=nullptr;
    Status status(){return state;}
    SMTPMessage& getMessage(){return local_msg;}
    bool isConnected(){return connected&&server_greeting_ack;}
    bool connect(const String&,int,void* cb,bool ssl,bool await){
        CHECK(cb==nullptr&&ssl&&!await); ++connects; phase=1; state=Status();
        clockMs+=connectCost; connected=connectOK; server_greeting_ack=false; return connectOK;
    }
    bool authenticate(const String&,const String&,int kind,bool await){
        CHECK(kind==readymail_auth_password&&!await&&isConnected());
        ++auths; phase=2; state.isComplete=false; return authOK;
    }
    bool isAuthenticated(){return authenticated;}
    bool send(SMTPMessage& msg,const String& notify,bool await){
        CHECK(!await&&notify.length()==0); ++sends; state.isComplete=false;
        msg_ptr=&msg;
        if(local_msg.headers.values[rfc822_from].length()==0){state.errorCode=-999; return false;}
        CHECK(msg_ptr==&local_msg); captured=msg; phase=3; return sendOK;
    }
    void loop(){
        ++loops; clockMs+=loopCost;
        if(failInsideLoop){state.errorCode=failInsideLoop; return;}
        if(event==1&&phase==1){server_greeting_ack=true;}
        if(event==2&&phase==2){authenticated=true;}
        if(phase==3){CHECK(msg_ptr==&local_msg); captured=*msg_ptr;}
        if(event==3&&phase==3){state.isComplete=true; local_msg.clear();}
        event=0;
    }
    // 实际stop不清理local_msg；应用必须释放失败/中断后留下的消息。
    void stop(){++stops; state=Status(); phase=0; authenticated=connected=server_greeting_ack=false;
        msg_ptr=nullptr; captured.clear();}
} smtp;
'''

TESTS = r'''
void emptySlots(){
    CHECK(notificationCount==0&&mailStage==0);
    CHECK(smtp.getMessage().headers.values[rfc822_from].length()==0);
    CHECK(smtp.getMessage().text.value.length()==0&&smtp.msg_ptr==nullptr);
    for(auto& j:notifications){
        CHECK(j.sender.length()==0&&j.message.length()==0&&j.timestamp.length()==0);
        CHECK(j.subject.length()==0&&j.body.length()==0&&j.channel==0&&!j.emailOnly&&j.queuedAt==0);
    }
}
void queueMail(){unsigned before=notificationQueueDepth();sendEmailNotification("subject","body"); CHECK(notificationQueueDepth()==before+1);}
void start(){int before=smtp.connects;queueMail();processNotifications();CHECK(smtp.connects==before+1);if(smtp.connectOK)CHECK(mailStage==1);}
void connectDone(){smtp.event=1;processNotifications();}
void authDone(){smtp.event=2;processNotifications();}
void sendDone(){smtp.event=3;processNotifications();}
void advance(int stage){start();if(stage>=2)connectDone();if(stage>=3)authDone();CHECK(mailStage==stage);}
void completeMail(){advance(3);sendDone();emptySlots();}
int main(int argc,char** argv){
    CHECK(argc==2);std::string test=argv[1];
    if(test=="capacity"){
        for(unsigned i=0;i<NOTIFICATION_CAPACITY;i++) CHECK(enqueueSmsNotification("s","m","t"));
        CHECK(!enqueueSmsNotification("x","y","z"));sendEmailNotification("s","b");
        CHECK(notificationQueueDepth()==8&&notificationDropCount()==2&&notifications[0].sender=="s");
        for(unsigned i=0;i<NOTIFICATION_CAPACITY;i++)finishNotification();emptySlots();
        for(unsigned i=0;i<NOTIFICATION_CAPACITY;i++)sendEmailNotification("s","b");
        CHECK(notificationQueueDepth()==8);finishNotification();sendEmailNotification("new","body");
        CHECK(notificationQueueDepth()==8&&notifications[0].subject=="new");
    } else if(test=="bounds"){
        std::string a(64,'s'),b(4096,'m'),c(64,'t'),d(256,'h'),e(4608,'b');
        CHECK(enqueueSmsNotification(a.c_str(),b.c_str(),c.c_str()));finishNotification();
        CHECK(!enqueueSmsNotification((a+"x").c_str(),"m","t"));
        CHECK(!enqueueSmsNotification("s",(b+"x").c_str(),"t"));
        CHECK(!enqueueSmsNotification("s","m",(c+"x").c_str()));
        CHECK(!enqueueSmsNotification(nullptr,"m","t"));CHECK(!enqueueSmsNotification("s",nullptr,"t"));
        CHECK(!enqueueSmsNotification("s","m",nullptr));
        sendEmailNotification(d.c_str(),e.c_str());CHECK(notificationCount==1);finishNotification();
        sendEmailNotification((d+"x").c_str(),"b");sendEmailNotification("s",(e+"x").c_str());
        sendEmailNotification(nullptr,"b");sendEmailNotification("s",nullptr);
        CHECK(notificationDropCount()==10);emptySlots();
        config.emailEnabled=false;sendEmailNotification("s","b");CHECK(droppedNotifications==10);
    } else if(test=="offline_expiry"){
        WiFi.state=0;clockMs=100;CHECK(enqueueSmsNotification("s","m","t"));queueMail();
        clockMs=300099;processNotifications();CHECK(notificationCount==2&&notificationHead==0);
        clockMs=300100;processNotifications();CHECK(notificationCount==1&&notificationHead==1);
        CHECK(droppedNotifications==1&&httpCalls==0&&smtp.connects==0);
        processNotifications();CHECK(droppedNotifications==2);emptySlots();
    } else if(test=="clock_wrap"){
        WiFi.state=0;clockMs=~0UL-100;queueMail();clockMs+=299999;
        processNotifications();CHECK(notificationCount==1);++clockMs;processNotifications();emptySlots();
        WiFi.state=WL_CONNECTED;clockMs=~0UL-100;start();clockMs+=14999;
        processNotifications();CHECK(notificationCount==1);++clockMs;processNotifications();emptySlots();
    } else if(test=="channels"){
        config.emailEnabled=false;
        for(auto& ch:config.pushChannels){ch.enabled=true;ch.url="mock.invalid";}
        CHECK(enqueueSmsNotification("first","message","timestamp"));
        CHECK(enqueueSmsNotification("second","message","timestamp"));
        WiFi.state=0;processNotifications();CHECK(httpCalls==0&&notificationCount==2);
        WiFi.state=WL_CONNECTED;
        for(int i=0;i<MAX_PUSH_CHANNELS;i++){
            processNotifications();CHECK(httpCalls==i+1&&lastChannel==i&&notificationCount==2);
            CHECK(lastSender=="first"&&lastMessage=="message"&&lastTimestamp=="timestamp");
            if(i==0){WiFi.state=0;processNotifications();CHECK(httpCalls==1);WiFi.state=WL_CONNECTED;}
        }
        processNotifications();CHECK(notificationCount==1&&saved==MAX_PUSH_CHANNELS);
        processNotifications();CHECK(lastSender=="second");
    } else if(test=="invalid_channels"){
        config.emailEnabled=false;config.pushChannels[0].enabled=true;
        config.pushChannels[1].enabled=true;config.pushChannels[1].type=PUSH_TYPE_TELEGRAM;
        config.pushChannels[1].url="mock.invalid";
        CHECK(enqueueSmsNotification("s","m","t"));
        for(int i=0;i<=MAX_PUSH_CHANNELS;i++)processNotifications();CHECK(httpCalls==0);emptySlots();
    } else if(test=="unsynced"){
        epoch=99999;queueMail();processNotifications();emptySlots();
        CHECK(stats.pushFailed==1&&saved==1&&smtp.connects==0&&smtp.loops==0&&clockMs==0);
    } else if(test=="missing_config"){
        config.smtpPass="";queueMail();processNotifications();emptySlots();CHECK(smtp.connects==0);
    } else if(test=="real_success"){
        advance(3);CHECK(smtp.auths==1&&smtp.sends==1&&stats.pushSuccess==0);
        CHECK(smtp.captured.headers.values[rfc822_subject]=="subject"&&smtp.captured.text.value=="body");
        CHECK(smtp.captured.headers.values[rfc822_from]=="sms notify <a@example.invalid>");
        CHECK(smtp.captured.headers.values[rfc822_to]==config.smtpSendTo&&smtp.captured.timestamp==epoch);
        processNotifications();CHECK(notificationCount==1&&stats.pushSuccess==0);
        sendDone();emptySlots();CHECK(stats.pushSuccess==1&&stats.pushFailed==0&&saved==1);
        CHECK(smtp.stops==1&&ssl_client.stops==1);
    } else if(test=="sms_mail"){
        CHECK(enqueueSmsNotification("sender","code 123456","timestamp"));
        for(int i=0;i<MAX_PUSH_CHANNELS;i++)processNotifications();
        processNotifications();connectDone();authDone();
        CHECK(smtp.captured.headers.values[rfc822_subject]=="[123456] sender");
        CHECK(smtp.captured.text.value=="发送者: sender\n时间: timestamp\n内容:\ncode 123456");
        sendDone();emptySlots();
        CHECK(enqueueSmsNotification("sender","plain message","t"));
        for(int i=0;i<=MAX_PUSH_CHANNELS;i++)processNotifications();connectDone();authDone();
        CHECK(smtp.captured.headers.values[rfc822_subject]=="sender: plain message");sendDone();emptySlots();
    } else if(test=="immediate_failures"){
        smtp.connectOK=false;start();emptySlots();CHECK(stats.pushFailed==1);
        smtp.connectOK=true;smtp.authOK=false;start();connectDone();emptySlots();CHECK(stats.pushFailed==2);
        smtp.authOK=true;smtp.sendOK=false;start();connectDone();authDone();emptySlots();CHECK(stats.pushFailed==3);
    } else if(test=="errors_deadline"){
        for(int stage=1;stage<=3;stage++){
            advance(stage);smtp.state.errorCode=535;processNotifications();emptySlots();
            advance(stage);unsigned long begin=mailStarted;clockMs=begin+14999;
            processNotifications();CHECK(notificationCount==1&&mailStage==stage);
            clockMs=begin+15000;int loops=smtp.loops;processNotifications();emptySlots();CHECK(smtp.loops==loops);
            advance(stage);smtp.failInsideLoop=550;processNotifications();
            CHECK(notificationCount==1);smtp.failInsideLoop=0;processNotifications();emptySlots();
        }
        CHECK(stats.pushFailed==9&&saved==9&&stats.pushSuccess==0);
    } else if(test=="disconnect_active"){
        for(int stage=1;stage<=3;stage++){
            advance(stage);WiFi.state=0;processNotifications();emptySlots();WiFi.state=WL_CONNECTED;
        }
        CHECK(stats.pushFailed==3&&saved==3&&stats.pushSuccess==0&&droppedNotifications==0);
        for(int i=0;i<5;i++)processNotifications();CHECK(smtp.connects==3);
        WiFi.state=0;queueMail();processNotifications();CHECK(notificationCount==1&&saved==3);
        WiFi.state=WL_CONNECTED;processNotifications();connectDone();authDone();sendDone();emptySlots();
    } else if(test=="config_changes"){
        Config original=config;
        for(int stage=1;stage<=3;stage++)for(int field=0;field<6;field++){
            config=original;advance(stage);int auths=smtp.auths,sends=smtp.sends;
            if(field==0)config.smtpServer="new.invalid";
            if(field==1)config.smtpUser="new@example.invalid";
            if(field==2)config.smtpPass="new-password-sentinel";
            if(field==3)config.smtpSendTo="new-recipient@example.invalid";
            if(field==4)config.smtpPort=587;
            if(field==5)config.emailEnabled=false;
            processNotifications();emptySlots();CHECK(smtp.auths==auths&&smtp.sends==sends);
            for(const char c:activeMail.pass)CHECK(c==0);
        }
        CHECK(stats.pushFailed==18&&saved==18);
        config=original;config.smtpPass=std::string(513,'x');queueMail();processNotifications();emptySlots();
        CHECK(stats.pushFailed==19&&saved==19);
    } else if(test=="real_connection_repro"||test=="connection_contract"){
        start();CHECK(smtp.connected&&!smtp.isConnected()&&!smtp.status().isComplete);
        // TCP成功和停留在握手阶段都不能触发认证。
        processNotifications();CHECK(mailStage==1&&smtp.auths==0);
        connectDone();CHECK(smtp.isConnected()&&!smtp.status().isComplete);
        CHECK(mailStage==2&&smtp.auths==1);
        processNotifications();CHECK(smtp.auths==1&&smtp.sends==0);
        authDone();sendDone();emptySlots();CHECK(stats.pushSuccess==1&&stats.pushFailed==0);
    } else if(test=="real_message_repro"||test=="message_contract"){
        start();connectDone();authDone();
        CHECK(mailStage==3&&notificationCount==1&&smtp.sends==1);
        CHECK(smtp.msg_ptr==&smtp.getMessage());
        CHECK(smtp.getMessage().headers.values[rfc822_subject]=="subject");
        CHECK(smtp.getMessage().text.value=="body");
        // 跨多轮loop读取保留的引用，直到服务器确认发送完成。
        for(int i=0;i<3;i++){processNotifications();CHECK(mailStage==3&&notificationCount==1);}
        CHECK(smtp.captured.text.value=="body"&&stats.pushSuccess==0);
        sendDone();emptySlots();CHECK(stats.pushSuccess==1&&stats.pushFailed==0);
    } else if(test=="synchronous_connect_budget"){
        smtp.connectCost=16000;start();CHECK(clockMs==16000&&mailStage==1);
        processNotifications();emptySlots();CHECK(stats.pushFailed==1&&smtp.loops==0);
    } else if(test=="deadline_overrun_repro"){
        advance(3);clockMs=mailStarted+14999;smtp.loopCost=2;sendDone();emptySlots();
        CHECK(clockMs==mailStarted+15001&&stats.pushSuccess==1);
    } else if(test=="stress"){
        // 预热所有分支后，逐次核对实时堆分配与槽位，禁止只看最终深度。
        std::string body(4608,'b');
        auto cycle=[&](int i){
            sendEmailNotification("subject",body.c_str());processNotifications();
            if(i%4==0){clockMs=mailStarted+15000;processNotifications();}
            else if(i%4==1){smtp.state.errorCode=550;processNotifications();}
            else if(i%4==2){WiFi.state=0;processNotifications();WiFi.state=WL_CONNECTED;}
            else {connectDone();authDone();sendDone();}
            emptySlots();
            CHECK(enqueueSmsNotification("sender",body.substr(0,4096).c_str(),"timestamp"));
            for(int c=0;c<=MAX_PUSH_CHANNELS;c++)processNotifications();
            connectDone();authDone();sendDone();emptySlots();
        };
        for(int i=0;i<16;i++)cycle(i);
        size_t bytes=liveBytes,blocks=liveBlocks;
        for(int i=0;i<100000;i++){cycle(i);CHECK(liveBytes==bytes&&liveBlocks==blocks);}
        std::cout<<"100000 cycles / 200000 jobs: live bytes="<<bytes<<", blocks="<<blocks<<", growth=0\n";
    } else {CHECK(false);}
    std::cout<<"PASS "<<test<<"\n";
}
'''


class NotificationsTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source = (ROOT / 'code/push_service.ino').read_text(encoding='utf-8')
        config_h = (ROOT / 'code/config.h').read_text(encoding='utf-8')
        config_ino = (ROOT / 'code/config.ino').read_text(encoding='utf-8')
        constants = re.search(r'enum PushType\s*\{.*?\};', config_h, re.S).group(0)
        constants += '\n' + re.search(r'^#define MAX_PUSH_CHANNELS\s+\d+', config_h, re.M).group(0)
        actual = source[source.index('struct PendingNotification {'):]
        valid = function(config_ino, 'bool isPushChannelValid(')
        cls.tmp = tempfile.TemporaryDirectory(prefix='msg_forward_notifications_')
        cls.addClassCleanup(cls.tmp.cleanup)
        cpp = Path(cls.tmp.name) / 'test.cpp'
        cls.exe = Path(cls.tmp.name) / 'test.exe'
        cpp.write_text(MOCK.replace('__CHANNEL_CONSTANTS__', constants) + valid + '\n' + actual + TESTS,
                       encoding='utf-8')
        compiler = ROOT / '.tools/w64devkit/bin/g++.exe'
        cls.env = dict(os.environ)
        cls.env['PATH'] = str(compiler.parent) + os.pathsep + cls.env.get('PATH', '')
        subprocess.run([str(compiler), '-std=c++11', '-Wall', '-Wextra',
                        '-Wno-misleading-indentation', '-O0', '-g', str(cpp), '-o', str(cls.exe)],
                       check=True, env=cls.env, timeout=60)

    def run_case(self, name):
        result = subprocess.run([str(self.exe), name], env=self.env, timeout=30,
                                capture_output=True, text=True, encoding='utf-8')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        if name == 'stress':
            print(result.stdout.strip())

    def test_regressions(self):
        for name in ('capacity', 'bounds', 'offline_expiry', 'clock_wrap', 'channels',
                     'invalid_channels', 'unsynced', 'missing_config', 'real_success',
                     'sms_mail', 'immediate_failures', 'errors_deadline', 'disconnect_active', 'config_changes',
                     'real_connection_repro', 'real_message_repro', 'synchronous_connect_budget',
                     'deadline_overrun_repro'):
            with self.subTest(scenario=name):
                self.run_case(name)

    def test_100000_cycles_without_live_heap_growth(self):
        self.run_case('stress')

    def test_ready_mail_connection_should_advance_to_authentication(self):
        self.run_case('connection_contract')

    def test_ready_mail_async_message_should_be_accepted(self):
        self.run_case('message_contract')

    def test_real_wstring_slot_release(self):
        core = ROOT / '.arduino/data/packages/esp32/hardware/esp32/3.3.0/cores/esp32'
        header = (core / 'WString.h').read_text(encoding='utf-8').replace('#include <pgmspace.h>', '')
        header = header.replace('protected:', 'public:')  # 只开放容量观察，不修改实现。
        impl = (core / 'WString.cpp').read_text(encoding='utf-8')
        impl = re.sub(r'^#include .*$', '', impl, flags=re.M)
        # 使用原版WString完整实现；仅屏蔽宿主没有的硬件日志和未使用的数字转换链接。
        prelude = r'''
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cctype>
#include <cstdarg>
#include <utility>
#define log_e(...) ((void)0)
#define log_w(...) ((void)0)
#define PROGMEM
#define PSTR(x) (x)
#define memcpy_P memcpy
#define itoa host_itoa
#define ltoa host_ltoa
#define lltoa host_lltoa
#define utoa host_utoa
#define ultoa host_ultoa
#define ulltoa host_ulltoa
#define dtostrf host_dtostrf
char* host_itoa(int,char*,int){std::abort();}char* host_ltoa(long,char*,int){std::abort();}
char* host_lltoa(long long,char*,int){std::abort();}char* host_utoa(unsigned,char*,int){std::abort();}
char* host_ultoa(unsigned long,char*,int){std::abort();}char* host_ulltoa(unsigned long long,char*,int){std::abort();}
char* host_dtostrf(double,int,unsigned,char*){std::abort();}
'''
        source = (ROOT / 'code/push_service.ino').read_text(encoding='utf-8')
        release = function(source, 'void finishNotification()')
        release = release[release.index('  PendingNotification& completed'):]
        main = r'''
struct PendingNotification { String sender,message,timestamp,subject,body; };
PendingNotification notifications[1];unsigned notificationHead=0,notificationCount=1;
const unsigned NOTIFICATION_CAPACITY=1;
void releaseSlot() {
__RELEASE__
int main(){
 for(int n=0;n<10000;++n){
  notificationCount=1;
  for(String* s:{&notifications[0].sender,&notifications[0].message,&notifications[0].timestamp,
                 &notifications[0].subject,&notifications[0].body}){
   s->reserve(4608);*s="payload";if(s->capacity()<4608)return 1;
  }
  releaseSlot();
  for(String* s:{&notifications[0].sender,&notifications[0].message,&notifications[0].timestamp,
                 &notifications[0].subject,&notifications[0].body})
   if(s->length()||(!s->isSSO()&&s->capacity()!=0))return 2;
 }
}
'''.replace('__RELEASE__', release)
        cpp = Path(self.tmp.name) / 'wstring.cpp'
        exe = Path(self.tmp.name) / 'wstring.exe'
        cpp.write_text(prelude + header + impl + main, encoding='utf-8')
        compiler = ROOT / '.tools/w64devkit/bin/g++.exe'
        subprocess.run([str(compiler), '-std=c++11', '-ffunction-sections', '-fdata-sections',
                        '-Wl,--gc-sections', str(cpp), '-o', str(exe)],
                       env=self.env, check=True, timeout=60)
        subprocess.run([str(exe)], env=self.env, check=True, timeout=30)

    def test_local_readymail_semantics(self):
        # 源码证据保护 faithful mock 的前提；升级库后需要重新审查。
        conn = (LIB / 'SMTPConnection.h').read_text(encoding='utf-8')
        sender = (LIB / 'SMTPSend.h').read_text(encoding='utf-8')
        client = (LIB / 'SMTPClient.h').read_text(encoding='utf-8')
        connect = function(conn, 'bool connect(const String &host, uint16_t port, const String &domain,')
        auth = function(conn, 'void authenticate(smtp_function_return_code &ret)')
        self.assertIn('smtp_ctx->status->isComplete = false;', connect)
        self.assertIn('server_greeting_ack = true;', auth)
        self.assertNotIn('isComplete', auth)
        self.assertIn('bool isConnected() { return conn.isConnected(); }', client)
        self.assertIn('bool isConnected() { return serverConnected() && smtp_ctx->server_status->server_greeting_ack; }', conn)
        self.assertIn('cState() = smtp_state_prompt;', auth)
        # stop仅断开连接，不会清理失败发送留下的内部消息。
        stop = function(client, 'void stop()')
        self.assertNotIn('local_msg.clear()', stop)
        message = (LIB / 'SMTPMessage.h').read_text(encoding='utf-8')
        clear = function(message[message.index('class SMTPMessage\n'):], 'void clear()')
        self.assertIn('headers.clear();', clear)
        self.assertIn('text.clear();', clear)
        impl = function(sender, 'bool sendImpl(SMTPMessage &msg, bool await)')
        self.assertIn('msg_ptr = &msg;', impl)
        self.assertIn('if (!await && local_msg.headers.size() == 0)', impl)
        self.assertIn('SMTP_ERROR_UNINITIALIZE_LOCAL_SMTP_MESSAGE', impl)
        self.assertNotIn('local_msg = msg', impl)
        send = function(client, 'bool send(SMTPMessage &message,')
        self.assertIn('sender.send(message, notify, await)', send)
        self.assertIn('SMTPMessage &getMessage() { return sender.local_msg; }', client)
        completion = sender[sender.index('case smtp_state_data_termination:', sender.index('smtp_function_return_code loop()')):]
        completion = completion[:completion.index('break;')]
        self.assertIn('smtp_ctx->status->isComplete = true;', completion)
        self.assertIn('local_msg.clear();', completion)


if __name__ == '__main__':
    unittest.main(verbosity=2)
