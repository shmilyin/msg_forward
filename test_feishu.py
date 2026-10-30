"""离线编译实际飞书发送/签名/校验/UI函数；HTTP只捕获，不连接网络。

宿主以Windows BCrypt实现mbedtls接口，Python hashlib/hmac独立校验签名；
ESP32实际mbedtls链接由 python build.py compile 验证。
"""
from pathlib import Path
import base64
import hashlib
import hmac
import json
import os
import re
import subprocess
import tempfile
import unittest
from html.parser import HTMLParser

from test_notifications import ROOT, function

MOCK = r'''
#include <string>
#include <cstdint>
#include <algorithm>
#include <cstring>
#include <ctime>
#include <cstdio>
#include <iostream>
#include <vector>
#include <windows.h>
#include <bcrypt.h>
#define CHECK(x) do { if(!(x)) { std::cerr << "检查失败: " << #x; std::exit(1); } } while(0)
class String {
public:
 std::string s;
 String(const char* p=""):s(p){} String(std::string p):s(p){} String(int n):s(std::to_string(n)){}
 size_t length()const{return s.size();} const char* c_str()const{return s.c_str();}
 bool startsWith(const char* p)const{return s.find(p)==0;}
 char charAt(unsigned i)const{return s[i];}
 char operator[](unsigned i)const{return s[i];}
 String substring(unsigned a,unsigned b=~0u)const{return s.substr(a,std::min(b,(unsigned)s.size())-a);}
 int indexOf(char c)const{auto i=s.find(c);return i==std::string::npos?-1:(int)i;}
 void replace(const String& a,const String& b){size_t p=0;while((p=s.find(a.s,p))!=std::string::npos){s.replace(p,a.length(),b.s);p+=b.length();}}
 String& operator+=(const String& b){s+=b.s;return *this;}
 String& operator+=(char c){s+=c;return *this;}
 friend String operator+(const String&a,const String&b){return a.s+b.s;}
 friend bool operator==(const String&a,const String&b){return a.s==b.s;}
};
struct { String text; void println(const String& s){text+=s;text+='\n';}
 template<class... Args> void printf(const char* f,Args... args){char b[256];snprintf(b,sizeof(b),f,args...);text+=b;}
} Serial;
struct { int pushFailed=0,pushSuccess=0; } stats;
int saved=0;void saveStats(){++saved;}
time_t epoch=1700000000;
int timeCalls=0;
time_t mockTime(time_t*){++timeCalls;return epoch;}
#define time mockTime
unsigned long clockMs=0;
unsigned long millis(){return clockMs;}
void delay(unsigned long n){clockMs+=n;}
__CONSTANTS__
struct PushChannel {bool enabled=true;PushType type=PUSH_TYPE_FEISHU;String name,url,key1,customBody;};
#define MAX_WIFI_NETWORKS 3
struct WifiNetwork {bool enabled=true;String ssid,password;};
struct {
 PushChannel pushChannels[MAX_PUSH_CHANNELS];WifiNetwork wifiNetworks[MAX_WIFI_NETWORKS];
 String webUser,webPass,smtpServer,smtpUser,smtpPass,smtpSendTo,timerPhone,timerMessage;
 bool emailEnabled=false,timerEnabled=false;int smtpPort=465,timerType=0,timerInterval=30;
} config;
// 实际HMAC运算交给系统密码库；保留生产函数对mbedtls的调用顺序和参数。
struct mbedtls_md_context_t {BCRYPT_ALG_HANDLE alg=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;std::vector<unsigned char> object;};
const int MBEDTLS_MD_SHA256=1;
void mbedtls_md_init(mbedtls_md_context_t*){}
int mbedtls_md_info_from_type(int){return 1;}
int mbedtls_md_setup(mbedtls_md_context_t* c,int,int){CHECK(BCryptOpenAlgorithmProvider(&c->alg,BCRYPT_SHA256_ALGORITHM,nullptr,BCRYPT_ALG_HANDLE_HMAC_FLAG)==0);return 0;}
int mbedtls_md_hmac_starts(mbedtls_md_context_t* c,unsigned char* key,size_t n){DWORD size=0,got=0;CHECK(BCryptGetProperty(c->alg,BCRYPT_OBJECT_LENGTH,(PUCHAR)&size,sizeof(size),&got,0)==0);c->object.resize(size);CHECK(BCryptCreateHash(c->alg,&c->hash,c->object.data(),size,key,n,0)==0);return 0;}
int mbedtls_md_hmac_update(mbedtls_md_context_t* c,unsigned char* data,size_t n){CHECK(BCryptHashData(c->hash,data,n,0)==0);return 0;}
int mbedtls_md_hmac_finish(mbedtls_md_context_t* c,unsigned char* out){CHECK(BCryptFinishHash(c->hash,out,32,0)==0);return 0;}
void mbedtls_md_free(mbedtls_md_context_t* c){BCryptDestroyHash(c->hash);BCryptCloseAlgorithmProvider(c->alg,0);}
int mbedtls_base64_encode(unsigned char* out,size_t cap,size_t* len,const unsigned char* data,size_t n){
 const char* abc="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";*len=0;
 CHECK(cap>=4*((n+2)/3));
 for(size_t i=0;i<n;i+=3){unsigned v=(unsigned)data[i]<<16;if(i+1<n)v|=(unsigned)data[i+1]<<8;if(i+2<n)v|=data[i+2];
 out[(*len)++]=abc[(v>>18)&63];out[(*len)++]=abc[(v>>12)&63];out[(*len)++]=i+1<n?abc[(v>>6)&63]:'=';out[(*len)++]=i+2<n?abc[v&63]:'=';}
 return 0;
}
String capturedUrl,capturedMethod,capturedType,capturedBody;int requests=0;
std::string responseBody="{\"code\":0}";
int responseCode=200,responseSize=-2,readCount=0,httpDestructs=0,tlsDestructs=0;
bool tlsAlive=false,beginOK=true,stalled=false,infinite=false,http10=false;
String transferEncoding;
struct WiFiClientSecure {
 WiFiClientSecure(){CHECK(!tlsAlive);tlsAlive=true;}
 ~WiFiClientSecure(){tlsAlive=false;++tlsDestructs;}
 void setInsecure(){} void setHandshakeTimeout(int){} void setConnectionTimeout(int){}
};
struct ResponseStream {
 size_t pos=0;
 int available(){return stalled?0:(infinite||pos<responseBody.size()?1:0);}
 int read(){++readCount;++clockMs;if(infinite)return ' ';return pos<responseBody.size()?(unsigned char)responseBody[pos++]:-1;}
};
struct HTTPClient {
 WiFiClientSecure* client=nullptr;ResponseStream stream;
 ~HTTPClient(){if(client)CHECK(tlsAlive);++httpDestructs;}
 void setConnectTimeout(int){}void setTimeout(int){}void setReuse(bool){}
 void useHTTP10(bool b){http10=b;}void collectHeaders(const char**,size_t){}
 bool begin(WiFiClientSecure& c,const String& u){client=&c;capturedUrl=u;return beginOK;}
 bool begin(const String& u){capturedUrl=u;return beginOK;}
 void addHeader(const char*,const String& t){capturedType=t;}
 int GET(){++requests;capturedMethod="GET";return responseCode;}
 int POST(const String& b){++requests;capturedMethod="POST";capturedBody=b;return responseCode;}
 String header(const char*){return transferEncoding;}
 int getSize(){return responseSize==-2?(int)responseBody.size():responseSize;}
 ResponseStream* getStreamPtr(){return &stream;}
 bool connected(){return stalled||infinite||stream.pos<responseBody.size();}
 String errorToString(int){return "模拟连接失败";}
 // 模拟ESP32 3.3失败end路径：连接断开时不清除_client，析构仍访问它。
 void end(){if(connected()&&responseCode>0)client=nullptr;}
};
'''
MAIN = r'''
int main(int argc,char** argv){
 CHECK(argc==2);std::string mode=argv[1];
 if(mode=="css"||mode=="css_stale"){
  server.version=mode=="css"?APP_CSS_VERSION:"old";
  config.webUser="private-config-sentinel";
  handleAppCss();CHECK(server.html.length()==sizeof(commonCss)-1);
  CHECK(server.headers["Cache-Control"]==(mode=="css"?"public, max-age=31536000, immutable":"no-cache"));
  CHECK(server.headers["X-Content-Type-Options"]=="nosniff");
  std::cout<<server.html.s;return 0;
 }
 if(mode=="ui"){
  for(auto& c:config.pushChannels){c.type=PUSH_TYPE_FEISHU;c.key1="key";}
  handleRoot();CHECK(server.ended);std::cout<<server.html.s;return 0;
 }
 if(mode=="ui_escape"){
  for(auto& c:config.pushChannels){c.type=PUSH_TYPE_CUSTOM;c.name="\"><script>bad()</script>%CONFIG%";c.url="a&b\"";c.key1="\"><script>bad()</script>&%COMMON_CSS%";c.customBody="</textarea><script>bad()</script>";}
  handleRoot();CHECK(server.ended);std::cout<<server.html.s;return 0;
 }
 if(mode=="ui_boundary"){
  config.webUser=String(std::string(127,'x')+"\"<&%COMMON_CSS%%CONFIG%");
  handleRoot();CHECK(server.ended);std::cout<<server.html.s;return 0;
 }
 if(mode=="ui_unauthorized"){
  authorized=false;handleRoot();CHECK(server.calls==0);std::cout<<"OK";return 0;
 }
 if(mode=="valid"){
  PushChannel c;CHECK(isPushChannelValid(c)==false);c.url="https://example.invalid/webhook/private-token";
  CHECK(isPushChannelValid(c));c.key1="secret";CHECK(isPushChannelValid(c));
  c.enabled=false;CHECK(!isPushChannelValid(c));c.enabled=true;c.key1="";
  for(int t=-1;t<=255;t++){c.type=(PushType)t;bool want=(t>=1&&t<=4)||t==8;CHECK(isPushChannelValid(c)==want);c.key1="legacy-key";CHECK(isPushChannelValid(c)==want);c.key1="";}
  std::cout<<"OK";return 0;
 }
 if(mode=="http_lifecycle"){
  for(int code:{200,204,400,-1,-11})for(bool ok:{false,true}){
   responseCode=code;beginOK=ok;responseBody="";
   int before=httpDestructs;
   sendHttpRequest("https://example.invalid/private-token","POST","application/json", "{}");
   CHECK(!tlsAlive&&httpDestructs==before+1&&tlsDestructs==httpDestructs);
  }
  CHECK(readCount==0);std::cout<<"OK";return 0;
 }
 PushChannel c;c.url="https://example.invalid/webhook/private-token?existing=1";
 if(mode=="business_fail")responseBody="{\"code\":19021,\"msg\":\"secret-0\"}";
 if(mode=="legacy_ok")responseBody="{\"StatusCode\":0,\"StatusMessage\":\"success\"}";
 if(mode=="legacy_fail")responseBody="{\"StatusCode\":1}";
 if(mode=="both_fail")responseBody="{\"code\":0,\"StatusCode\":1}";
 if(mode=="nested")responseBody="{\"data\":{\"code\":0}}";
 if(mode=="string_code")responseBody="{\"code\":\"0\"}";
 if(mode=="duplicate")responseBody="{\"code\":1,\"code\":0}";
 if(mode=="invalid")responseBody="{\"code\":0,}";
 if(mode=="trailing")responseBody="{\"code\":0}garbage";
 if(mode=="empty")responseBody="";
 if(mode=="huge"){responseBody=std::string(100000,'x');}
 if(mode=="infinite"){responseSize=-1;infinite=true;}
 if(mode=="stalled"){responseSize=-1;stalled=true;}
 if(mode=="truncated")responseSize=100;
 if(mode=="chunked")transferEncoding="chunked";
 if(mode=="http_error")responseCode=500;
 if(mode=="transport_error")responseCode=-1;
 if(mode=="generic_empty"){c.type=PUSH_TYPE_POST_JSON;responseCode=204;responseBody="";}
 if(mode=="generic_huge"){c.type=PUSH_TYPE_POST_JSON;responseBody=std::string(100000,'x');}
 if(mode=="signed"||mode=="unsynced"||mode=="negative")c.key1="secret-0";
 if(mode=="unsynced"||mode=="unsigned_unsynced")epoch=0;
 if(mode=="negative")epoch=-1;
 if(mode=="removed5")c.type=(PushType)5;
 if(mode=="removed6")c.type=(PushType)6;
 if(mode=="removed7")c.type=(PushType)7;
 if(mode=="disabled")c.enabled=false;
 if(mode=="empty_url")c.url="";
 sendToChannel(c,"发\"送\\者\t", "内容\n第二行\r\t\\\"", "2026-10-28\n12:34");
 std::cout<<"{\"requests\":"<<requests<<",\"failed\":"<<stats.pushFailed<<",\"timeCalls\":"<<timeCalls
 <<",\"success\":"<<stats.pushSuccess<<",\"saved\":"<<saved<<",\"read\":"<<readCount<<",\"elapsed\":"<<clockMs
 <<",\"url\":\""<<jsonEscape(capturedUrl).s<<"\",\"method\":\""<<capturedMethod.s
 <<"\",\"type\":\""<<capturedType.s<<"\",\"body\":\""<<jsonEscape(capturedBody).s
 <<"\",\"logs\":\""<<jsonEscape(Serial.text).s<<"\"}";
}
'''


class Options(HTMLParser):
    def __init__(self):
        super().__init__()
        self.selects = {}
        self.current = None
        self.keys = {}

    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if tag == 'select':
            self.current = attrs['name']
            self.selects[self.current] = []
        elif tag == 'option':
            self.selects[self.current].append(attrs)
        elif tag == 'input' and attrs.get('name', '').endswith('k1'):
            self.keys[attrs['name']] = attrs

    def handle_endtag(self, tag):
        if tag == 'select':
            self.current = None


class FeishuTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source = (ROOT / 'code/push_service.ino').read_text(encoding='utf-8')
        conf = (ROOT / 'code/config.h').read_text(encoding='utf-8')
        constants = re.search(r'enum PushType\s*\{.*?\};', conf, re.S).group()
        constants += '\n' + re.search(r'^#define MAX_PUSH_CHANNELS\s+\d+', conf, re.M).group()
        actual = 'int sendHttpRequest(const String&,const String&,const String&,const String&,bool=false);\n'
        actual += '\n'.join(function(source, sig) for sig in (
            'String urlEncode(', 'String jsonEscape(',
            'String hmacSha256Base64(', 'bool feishuResponseSucceeded(',
            'bool readFeishuResponse(', 'int sendHttpRequest(', 'void sendToChannel('))
        actual += '\n' + function((ROOT / 'code/config.ino').read_text(encoding='utf-8'),
                                  'bool isPushChannelValid(')
        handlers = (ROOT / 'code/web_handlers.ino').read_text(encoding='utf-8')
        actual += r'''
#include <map>
#define PROGMEM
const int CONTENT_LENGTH_UNKNOWN=-1;
bool authorized=true;
bool checkAuth(){return authorized;}
void yield(){}
struct MockIP { String toString(){return "192.0.2.10";} };
struct { String SSID(){return "";} MockIP localIP(){return MockIP();} } WiFi;
struct {
 String html;bool ended=false;int calls=0;int contentLength=0;String version;
 std::map<std::string,std::string> headers;
 String arg(const char*){return version;}
 void sendHeader(const char* key,const char* value){headers[key]=value;++calls;}
 void setContentLength(int n){++calls;contentLength=n;}
 void send(int code,const char* type,const char* body){
  ++calls;CHECK(code==200);CHECK(!*body);
  if(std::string(type)=="text/html; charset=utf-8"){
   CHECK(contentLength==CONTENT_LENGTH_UNKNOWN);CHECK(headers["Cache-Control"]=="no-store");
  }else{CHECK(std::string(type)=="text/css; charset=utf-8");CHECK(contentLength>9000);}
 }
 void sendContent(const char* p,size_t n){CHECK(!ended);CHECK(n>0&&n<=1024);html+=String(std::string(p,n));++calls;}
 void sendContent(const String& s){CHECK(!ended);CHECK(s.length()<=1024);++calls;if(!s.length())ended=true;else html+=s;}
 void sendContent_P(const char* p,size_t n){sendContent(p,n);}
} server;
'''
        actual += (ROOT / 'code/web_pages.h').read_text(encoding='utf-8') + '\n'
        for sig in ('String htmlEscape(', 'static void streamPageConfig(', 'void handleAppCss(', 'void handleRoot('):
            actual += function(handlers, sig) + '\n'
        cls.tmp = tempfile.TemporaryDirectory(prefix='msg_forward_feishu_')
        cls.addClassCleanup(cls.tmp.cleanup)
        cpp = Path(cls.tmp.name) / 'test.cpp'
        cls.exe = Path(cls.tmp.name) / 'test.exe'
        cpp.write_text(MOCK.replace('__CONSTANTS__', constants) + actual + MAIN, encoding='utf-8')
        compiler = ROOT / '.tools/w64devkit/bin/g++.exe'
        cls.env = dict(os.environ)
        cls.env['PATH'] = str(compiler.parent) + os.pathsep + cls.env.get('PATH', '')
        subprocess.run([str(compiler), '-std=c++11', '-Wall', '-Wextra', str(cpp),
                        '-o', str(cls.exe), '-lbcrypt'], check=True, env=cls.env, timeout=60)

    def run_case(self, name):
        result = subprocess.run([str(self.exe), name], capture_output=True, text=True,
                                encoding='utf-8', env=self.env, timeout=10)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return result.stdout

    def request(self, name):
        result = json.loads(self.run_case(name))
        self.assertNotIn('private-token', result['logs'])
        self.assertNotIn('secret-0', result['logs'])
        return result

    def test_unsigned_and_escaping(self):
        for name in ('unsigned', 'unsigned_unsynced'):
            with self.subTest(name=name):
                result = self.request(name)
                self.assertEqual((result['requests'], result['failed'], result['timeCalls']), (1, 0, 0))
                self.assertEqual(result['method'], 'POST')
                self.assertEqual(result['type'], 'application/json')
                self.assertEqual(result['url'], 'https://example.invalid/webhook/private-token?existing=1')
                self.assertEqual(json.loads(result['body']), {
                    'msg_type': 'text', 'content': {'text': '来自: 发"送\\者\t\n内容\n第二行\r\t\\"\n\n2026-10-28\n12:34'}})

    def test_signed_json_and_independent_crypto(self):
        result = self.request('signed')
        body = json.loads(result['body'])
        self.assertEqual((result['requests'], result['failed'], result['timeCalls']), (1, 0, 1))
        self.assertEqual(result['url'], 'https://example.invalid/webhook/private-token?existing=1')
        self.assertEqual((result['method'], result['type']), ('POST', 'application/json'))
        expected = base64.b64encode(hmac.new(b'1700000000\nsecret-0', b'', hashlib.sha256).digest()).decode()
        self.assertEqual(body['timestamp'], '1700000000')
        self.assertEqual(body['sign'], expected)
        self.assertTrue(any(c in expected for c in '+/'))
        self.assertNotIn('%', body['sign'])
        self.assertEqual(set(body), {'timestamp', 'sign', 'msg_type', 'content'})
        self.assertEqual(body['content'], json.loads(self.request('unsigned')['body'])['content'])
        self.assertNotIn(expected, result['logs'])

    def test_unsynced_fails_once_without_http(self):
        for name in ('unsynced', 'negative'):
            result = self.request(name)
            self.assertEqual((result['requests'], result['failed'], result['timeCalls']), (0, 1, 1))
            self.assertEqual(result['body'], '')

    def test_disabled_and_empty_url(self):
        for name in ('disabled', 'empty_url', 'removed5', 'removed6', 'removed7'):
            result = self.request(name)
            self.assertEqual((result['requests'], result['failed'], result['timeCalls']), (0, 0, 0))

    def test_real_http_lifecycle_and_business_response(self):
        self.assertEqual(self.run_case('http_lifecycle'), 'OK')
        for name in ('business_fail', 'legacy_fail', 'both_fail', 'nested', 'string_code',
                     'duplicate', 'invalid', 'trailing', 'empty', 'huge', 'infinite',
                     'stalled', 'truncated', 'chunked', 'http_error', 'transport_error'):
            with self.subTest(name=name):
                result = self.request(name)
                self.assertEqual((result['failed'], result['success']), (1, 0))
                self.assertLessEqual(result['read'], 2048)
                self.assertLessEqual(result['elapsed'], 2000)
        result = self.request('legacy_ok')
        self.assertEqual((result['failed'], result['success']), (0, 1))
        for name in ('generic_empty', 'generic_huge'):
            result = self.request(name)
            self.assertEqual((result['failed'], result['success'], result['read']), (0, 1, 0))

    def test_configuration_validation(self):
        self.assertEqual(self.run_case('valid'), 'OK')

    def test_three_rendered_channel_options_and_selected_keys(self):
        html = self.run_case('ui')
        parser = Options()
        parser.feed(html)
        self.assertEqual({name for name in parser.selects if name.endswith('type')}, {'push0type', 'push1type', 'push2type'})
        for removed in ('Telegram', '企业微信', '钉钉', 'Chat ID', 'SEC开头'):
            self.assertNotIn(removed, html)
        for i in range(3):
            options = parser.selects[f'push{i}type']
            self.assertEqual([o['value'] for o in options], list('12348'))
            self.assertEqual([o['value'] for o in options if 'selected' in o], ['8'])
            self.assertIn(f'id="k1{i}"', html)
            self.assertEqual(parser.keys[f'push{i}k1']['value'], 'key')

    def test_streamed_configuration_is_escaped_once(self):
        html = self.run_case('ui_escape')
        self.assertNotIn('<script>bad()', html)
        self.assertIn('&lt;/textarea&gt;', html)
        self.assertIn('%CONFIG%', html)
        self.assertIn('a&amp;b&quot;', html)

    def test_complete_stream_preserves_flash_css_and_template(self):
        source = (ROOT / 'code/web_pages.h').read_text(encoding='utf-8')
        css = re.search(r'R"CSS\((.*?)\)CSS"', source, re.S).group(1)
        template = re.search(r'R"PAGE\((.*?)\)PAGE"', source, re.S).group(1)
        html = self.run_case('ui')
        self.assertEqual(self.run_case('css'), css)
        self.assertEqual(self.run_case('css_stale'), css)
        self.assertNotIn('private-config-sentinel', css)
        original = subprocess.check_output(['git', 'show', 'c95518a:code/web_pages.h'], cwd=ROOT).decode('utf-8')
        original_css = re.search(r'<style>(.*?)</style>', original, re.S).group(1)
        self.assertTrue(css.startswith(original_css))
        original_html = re.search(r'R"rawliteral\((.*?)\)rawliteral"', original, re.S).group(1)
        self.assertEqual(re.findall(r'<div class="nav-icon"[^\n]+', template),
                         re.findall(r'<div class="nav-icon"[^\n]+', original_html))
        for token in ('统计数据', '状态看板', 'id="wifiS"', 'id="modNet"', 'id="modSig"',
                      'id="modSim"', 'id="chatHeader"', 'id="chatTitle"', 'id="chatMessages"',
                      'id="smEnSw"', 'id="tmEnSw"', 'id="chb0"', 'id="wfs0"'):
            self.assertIn(token, html)
        for removed in ('MQTT', 'Home Assistant', 'filterList', 'contentFilter', '号码黑白名单'):
            self.assertNotIn(removed, html)
        self.assertEqual(len(re.findall(r'(?:^|\n)label\{', css)), 1)
        air_css = css.split('/* 每日飞行时段：仅在本卡片内生效 */')[1]
        for selectors in re.findall(r'([^{}]+)\{', air_css):
            if selectors.strip().startswith('@media'):
                continue
            self.assertTrue(all(s.strip().startswith('.air-card ') for s in selectors.split(',')), selectors)
        self.assertEqual(css.count('.c{'), 1)
        self.assertEqual(css.count('.stat-num{'), 1)
        version = re.search(r'#define APP_CSS_VERSION "(.*?)"', source).group(1)
        self.assertIn(f'/assets/app.css?v={version}', html)
        routes = (ROOT / 'code/code.ino').read_text(encoding='utf-8')
        self.assertIn('server.on("/assets/app.css", HTTP_GET, handleAppCss)', routes)
        before, after = template.replace('%CSS_VERSION%', version).replace('%IP%', '192.0.2.10').split('%CONFIG%')
        self.assertTrue(html.startswith(before))
        self.assertTrue(html.endswith(after))
        self.assertNotRegex(html, r'%(?:COMMON_CSS|CONFIG)%')
        for selector in (':root', '.nav{', '.page{', 'input,select,textarea{',
                         ':focus-visible', '.contact-list', '.info-table', '@media(max-width:600px)'):
            self.assertIn(selector, css)
        self.assertIn('width:100%', css)
        self.assertEqual(html.count('<style>'), 0)
        self.assertEqual(html.count('</html>'), 1)

    def test_configuration_chunk_boundary_does_not_reinterpret_placeholders(self):
        html = self.run_case('ui_boundary')
        self.assertIn('x' * 127 + '&quot;&lt;&amp;%COMMON_CSS%%CONFIG%', html)
        self.assertEqual(html.count('<style>'), 0)
        self.assertTrue(html.endswith('</html>'))
        self.assertEqual(self.run_case('ui_unauthorized'), 'OK')

    def test_browser_layout_and_selector_coverage(self):
        chrome = Path('C:/Program Files/Google/Chrome/Application/chrome.exe')
        self.assertTrue(chrome.exists(), '需要现有Chrome进行真实布局回归')
        html = self.run_case('ui')
        # 只使用宿主真实handler生成的虚构配置，禁止浏览器联网和写操作。
        stub = '''<script>
window.addEventListener('error',e=>{const r=document.createElement('output');r.id='layout-result';r.textContent=JSON.stringify({width:innerWidth,failures:[e.message]});document.body.append(r)});
window.addEventListener('unhandledrejection',e=>{const r=document.createElement('output');r.id='layout-result';r.textContent=JSON.stringify({width:innerWidth,failures:[String(e.reason)]});document.body.append(r)});
window.calls=[];window.fetch=async(u,o)=>{calls.push([u,o&&o.body?o.body.toString():'']);return {json:async()=>u==='/history'?{history:[{s:'演示号码',t:'2026-10-29 12:00',m:'演示短信 <script>文本</scr'+'ipt> '+ '长内容'.repeat(500)},{s:'另一个演示号码',t:'2026-10-29 11:00',m:'第二组消息'}]}:u.startsWith('/query?')?{success:true,message:u.endsWith('network')?'<table class="info-table"><tr><td>网络注册</td><td>已注册，演示网络</td></tr><tr><td>运营商</td><td>演示运营商</td></tr><tr><td>数据连接</td><td>已激活</td></tr><tr><td>APN</td><td>（自动）</td></tr></table>':'<table class="info-table"><tr><td>信号强度 (RSRP)</td><td>-82 dBm（信号良好）</td></tr><tr><td>信号质量 (RSRQ)</td><td>-3.0 dB</td></tr></table>'}:u==='/stats'?{received:12,sent:3,boots:2,uptime:3600,wifiRssi:-55,freeHeap:140000,minHeap:65000,notificationQueue:0,notificationDropped:0}:{enabled:true,start:'23:00',end:'07:00',synced:true,beijing:'2026-10-29 12:00:03',targetKnown:true,target:false,actual:1,switching:false,restoring:false,simReady:true,registered:true,attached:true,message:'演示请求已接受',override:true,overrideUntil:1793314800}}};
</script>'''
        probe = r'''<script>
window.addEventListener('load',async()=>{
const failures=[];const check=(v,m)=>{if(!v)failures.push(m)};
const css=s=>getComputedStyle(document.querySelector(s));
const long='0123456789abcdef'.repeat(64);
document.getElementById('modNet').innerHTML='<table class="info-table"><tr><td>固件版本</td><td>'+long+'</td></tr></table>';
document.getElementById('modSig').textContent=long;
document.getElementById('chatMessages').innerHTML='<pre>'+long+'</pre>';
check(document.querySelector('.page.active').id==='overview','默认初始页错误');
check(css('.nav').position==='fixed','导航非固定底栏');
check(css('.card').borderRadius==='16px','卡片原版样式丢失');
check(css('.c').paddingLeft==='20px','原版容器间距被覆盖');
check(css('#control .fg label').marginBottom==='6px','原版标签间距被覆盖');
check(css('.nav-icon').webkitMaskImage.includes('svg'),'原版SVG遮罩丢失');
check(css('.stat-num').fontSize==='22.4px','原版指标字号被覆盖');
check(document.querySelector('#overview .card-t').textContent.includes('统计数据'),'原版统计卡片缺失');
check(document.querySelector('#modNet')&&document.querySelector('#modSig')&&document.querySelector('#modSim'),'原版自动查询指标缺失');
check(css('body').backgroundColor==='rgb(248, 250, 252)','背景样式丢失');
for(let n=0;n<4;n++){
 document.querySelectorAll('[role="tab"]')[n].click();
 check(document.querySelectorAll('.page.active').length===1,'激活页面数量');
 check(document.querySelector('.page.active').id===tabIds[n],'Tab未切换');
 check(location.hash==='#'+tabIds[n],'hash未同步');
 check(document.querySelectorAll('[aria-selected="true"]').length===1,'aria选中数量');
 check(document.querySelectorAll('[role="tab"]')[n].tabIndex===0,'Tab焦点错误');
 check(document.documentElement.scrollWidth<=innerWidth,'Tab页面水平溢出:'+n);
}
const tabs=[...document.querySelectorAll('[role="tab"]')];
for(const [key,want] of [['Home',0],['ArrowLeft',3],['ArrowRight',0],['End',3],['Enter',3],[' ',3]]){
 document.querySelector('[aria-selected="true"]').dispatchEvent(new KeyboardEvent('keydown',{key,bubbles:true,cancelable:true}));
 check(document.activeElement===tabs[want]&&tabs[want].getAttribute('aria-selected')==='true','键盘切换:'+key);
}
location.hash='#control';hashTab();check(!$('control').hidden,'hash定位错误');
location.hash='#invalid';hashTab();check(!$('overview').hidden,'无效hash未回概览');
swTab(1);
await refreshAir(true);for(const action of ['on','off','auto'])await airAction(action);
check(calls.filter(c=>c[0]==='/airplane/action').map(c=>c[1]).join(',')==='action=on,action=off,action=auto','飞行动作请求');
check($('airStatus').textContent.includes('临时覆盖'),'覆盖状态缺失');
const base={enabled:true,start:'23:00',end:'07:00',synced:true,beijing:'2026-10-29 12:00:03',targetKnown:true,target:false,actual:1,switching:false,restoring:false,simReady:true,registered:true,attached:true,override:false,overrideUntil:0,error:''};
const originalFetch=window.fetch;
let airData={...base};window.fetch=async(u,o)=>u==='/airplane'?{ok:true,json:async()=>({...airData})}:originalFetch(u,o);
for(const [name,patch,badge] of [
 ['正常',{},'正常运行'],['飞行',{target:true,actual:4,simReady:false,registered:false,attached:false},'飞行模式'],
 ['恢复',{restoring:true,registered:false,attached:false},'恢复中'],['切换',{switching:true},'切换中'],
 ['未校时',{synced:false,beijing:'未校时',targetKnown:false,actual:-1},'状态未知'],
 ['错误',{error:'<img src=x onerror="window.airInjected=true">'+long},'正常运行'],
 ['手动覆盖',{override:true,target:true,actual:4,overrideUntil:1793293200},'飞行模式'],
 ['覆盖待校时',{override:true,synced:false,beijing:'未校时',overrideUntil:0},'正常运行'],
 ['关闭调度',{enabled:false},'正常运行']]){
 airData={...base,...patch};await refreshAir(true);
 check($('airCurrent').textContent===badge,name+'状态badge');
 check(!/true|false/.test([...$('airStatus').children].filter(e=>e.id!=='airError').map(e=>e.textContent).join('')),name+'泄漏布尔值');
 check($('airError').hidden===!airData.error,name+'错误显隐');
 check($('airProgress').hidden===!(airData.switching||airData.restoring||airData.synced===false),name+'进度显隐');
 check(!$('airError').querySelector('img')&&!window.airInjected,'错误文本注入');
 check($('airAuto').disabled===!airData.enabled,name+'自动按钮状态');
 if(name==='正常')check($('airSim').textContent==='已就绪'&&$('airNetwork').textContent==='已注册'&&$('airAttach').textContent==='已附着','中文连接标签');
 if(name==='未校时')check($('airClock').textContent==='未校时'&&$('airTarget').textContent==='待校时确认','未校时仍显示日期或目标');
 if(name==='手动覆盖')check($('airOverride').textContent.includes('手动开启飞行')&&$('airOverride').textContent.includes('北京时间'),'覆盖说明');
 if(name==='覆盖待校时')check($('airOverride').textContent.includes('首次校时后确定'),'覆盖等待校时');
 check(document.documentElement.scrollWidth<=innerWidth,name+'页面水平溢出');
 for(const e of document.querySelectorAll('.air-card *')){const r=e.getBoundingClientRect();if(r.width)check(r.left>=0&&r.right<=innerWidth,name+'卡片元素溢出:'+e.id)}
}
airData={...base};airLoaded=false;airEdited=false;await refreshAir(true);
check($('airStart').value==='23:00'&&$('airEnd').value==='07:00'&&$('airEnabled').value==='true','首次配置回填');
check($('airRule').textContent.includes('跨日')&&$('airRule').textContent.includes('8 小时'),'跨日持续时间');
$('airStart').value='22:15';$('airStart').dispatchEvent(new Event('input',{bubbles:true}));
$('airEnabled').value='false';$('airEnabled').dispatchEvent(new Event('change',{bubbles:true}));
airData.start='01:00';await refresh();
check($('airStart').value==='22:15'&&$('airEnabled').value==='false','轮询覆盖用户编辑');
airLoaded=false;await refreshAir(true);check($('airStart').value==='22:15','首次响应晚于用户编辑时覆盖');
await saveAir({preventDefault(){},target:$('air')});
check(calls.some(c=>c[0]==='/airplane/save'&&c[1]==='enabled=false&start=22%3A15&end=07%3A00'),'规则保存API参数变化');
check($('airStart').value==='22:15','操作后回读覆盖编辑');
window.fetch=async()=>{throw new Error('<img src=x onerror=bad()>')};await refreshAir(true);
check(!$('airError').hidden&&!$('airError').querySelector('img'),'读取失败未安全提示');
window.fetch=async()=>({ok:false,status:503});await refreshAir(true);check(!$('airError').hidden,'HTTP错误未提示');
window.fetch=originalFetch;await refreshAir(false);check($('airError').hidden,'恢复读取后错误未清除');
check(css('#air .btn').backgroundColor==='rgb(99, 102, 241)','保存按钮失去主色');
check(css('.btn-d').backgroundColor==='rgb(239, 68, 68)','危险按钮失去原版颜色');
const fields=[$('airStart'),$('airEnd')].map(e=>e.getBoundingClientRect());
check(Math.abs(fields[1].top-fields[0].top)<1,'时间分栏断点错误');
check(innerWidth<=600||Math.abs($('airEnabled').getBoundingClientRect().top-fields[0].top)<1,'桌面调度与时间未对齐');
swTab(2);await loadHist();
check(document.querySelectorAll('.contact-item').length===2,'联系人分组丢失');
selContact('演示号码');
check(document.querySelector('.msg-bubble').textContent.includes('<script>文本'),'历史未安全显示文本');
check(innerWidth>600||document.querySelector('.chat-area').classList.contains('show'),'手机聊天未打开');
check(document.querySelector('.contact-name-row .contact-time'),'原版联系人时间行缺失');
check(document.querySelector('.contact-preview-row .contact-preview'),'原版联系人摘要行缺失');
check(document.querySelector('.chat-back svg path').getAttribute('d').startsWith('M20 11'),'原版返回SVG缺失');
backToList();check(!document.querySelector('.chat-area').classList.contains('show'),'手机聊天未返回');
check(innerWidth>600||getComputedStyle(document.querySelector('.chat-area')).visibility==='hidden','手机返回后聊天遮挡列表');
document.querySelectorAll('.contact-item')[1].click();check($('chatName').textContent==='另一个演示号码','第二联系人交互失败');
await loadHist();check($('chatName').textContent==='另一个演示号码','刷新丢失联系人');
const historyFetch=window.fetch;
let confirmed=false,clearCalls=0,resolveClear,resolveOld;
window.confirm=text=>{check(text.includes('全部')&&text.includes('所有联系人'),'确认范围不明确');return confirmed};
window.fetch=(u,o)=>{
 if(u==='/history/clear'){clearCalls++;check(o.method==='POST','清理方法不是POST');return new Promise(r=>resolveClear=r)}
 if(u==='/history')return new Promise(r=>resolveOld=r);
 return historyFetch(u,o);
};
await clearHist();check(clearCalls===0&&smsData.length===2,'取消清理仍请求或改数据');
const oldLoad=loadHist();confirmed=true;const clearing=clearHist();
check($('clearHistBtn').disabled&&histClearing,'清理期间未禁用');
await clearHist();await loadHist();check(clearCalls===1,'清理期间重复请求');
resolveClear({ok:true,json:async()=>({success:true,message:'清理完成'})});await clearing;
check(smsData.length===0&&curContact===null&&!document.querySelector('.contact-item'),'清理后联系人或选中状态残留');
check($('chatHeader').style.display==='none'&&$('chatCount').textContent===''&&!$('chatTitle').querySelector('.contact-avatar'),'清理后聊天标题残留');
check($('chatMessages').textContent.includes('暂无短信')&&!document.querySelector('.chat-area').classList.contains('show'),'清理后聊天空态错误');
check(!$('clearHistBtn').disabled&&!histClearing,'完成后按钮未恢复');
check($('toast').textContent==='清理完成','清理成功未提示');
resolveOld({ok:true,json:async()=>({history:[{s:'旧响应',t:'2026',m:'不应复活'}]})});await oldLoad;
check(smsData.length===0&&curContact===null,'旧读取响应覆盖清空结果');
window.fetch=historyFetch;await loadHist();selContact('演示号码');
window.fetch=async(u,o)=>u==='/history/clear'?{ok:false,json:async()=>({success:false,message:'模拟清理失败'})}:historyFetch(u,o);
await clearHist();check(smsData.length===2&&curContact==='演示号码'&&!$('clearHistBtn').disabled,'失败清空了页面或按钮未恢复');
check($('toast').textContent==='模拟清理失败','清理失败未提示');
window.fetch=async(u,o)=>u==='/history/clear'?{ok:true,json:async()=>({success:false})}:historyFetch(u,o);
await clearHist();check(smsData.length===2,'业务失败误报成功');
window.fetch=async()=>{throw new Error('模拟断网')};await clearHist();check(smsData.length===2&&!$('clearHistBtn').disabled,'网络失败误清空');
window.fetch=async(u,o)=>u==='/history'?{ok:true,json:async()=>({history:[]})}:historyFetch(u,o);
await loadHist();check(curContact===null&&smsData.length===0&&$('chatHeader').style.display==='none','空历史刷新未重置聊天状态');
window.fetch=historyFetch;
swTab(3);document.querySelectorAll('#config details').forEach(e=>e.open=true);for(let i=0;i<3;i++)fd(i);xToggle('smEn');
check(document.querySelectorAll('#config details').length===4,'配置折叠分组缺失');
check(document.documentElement.scrollWidth<=innerWidth,'展开配置水平溢出');
check(css('.info-table').tableLayout==='fixed','长查询值撑宽表格');
for(const e of document.querySelectorAll('input,select,textarea')){
 if(e.type==='hidden')continue;check(!!e.closest('label')||!!e.closest('.fg')?.querySelector('label'),'无关联标签');
 const r=e.getBoundingClientRect();if(r.width)check(r.right<=innerWidth&&r.left>=0,'字段越出视口');
}
for(let i=0;i<3;i++){const before=$('che'+i).value;chTog(i);check($('che'+i).value!==before,'通道开关无效');chTog(i);const w=$('wfe'+i).value;wfTog(i);check($('wfe'+i).value!==w,'WiFi开关无效');wfTog(i)}
const en=$('smEn').value;xToggle('smEn');check($('smEn').value!==en,'邮件开关无效');
for(let i=0;i<3;i++)for(const t of ['1','2','3','4','8']){
 document.getElementById('tp'+i).value=t;upd(i);
 check(getComputedStyle(document.getElementById('cf'+i)).display===(t==='4'?'block':'none'),'模板显隐');
 check(getComputedStyle(document.getElementById('k1'+i)).display===(t==='8'?'block':'none'),'密钥显隐');
}
const rules=[...document.styleSheets].flatMap(s=>[...s.cssRules]).flatMap(r=>r.type===CSSRule.MEDIA_RULE?[...r.cssRules]:[r]);
for(const e of document.querySelectorAll('[class]'))for(const c of e.classList){
 check(rules.some(r=>r.selectorText&&r.selectorText.includes('.'+c)&&(e.matches(r.selectorText)||(c==='air-card'&&e.querySelector(r.selectorText)))),'缺类:'+c);
}
const result=document.createElement('output');result.id='layout-result';result.textContent=JSON.stringify({width:innerWidth,failures});document.body.append(result);
});
</script>'''
        from html import unescape
        preview = Path(self.tmp.name) / 'ui-preview.html'
        preview_html = re.sub(r'<link rel="stylesheet"[^>]+>', lambda _: '<style>' + self.run_case('css') + '</style>', html)
        preview_stub = stub.replace('<script>', '<script>if(!location.hash)location.hash="control";', 1)
        preview.write_text(preview_html.replace('<script>', preview_stub + '<script>', 1), encoding='utf-8')
        with tempfile.TemporaryDirectory(prefix='web_layout_') as tmp:
            page = Path(tmp) / 'layout.html'
            css_file = Path(tmp) / 'app.css'
            css_file.write_text(self.run_case('css'), encoding='utf-8')
            html = re.sub(r'/assets/app.css\?v=[^" ]+', css_file.as_uri(), html)
            content = html.replace('<script>', stub + '<script>', 1).replace('</html>', probe + '</html>')
            inner_page = Path(tmp) / 'inner.html'
            inner_page.write_text(content, encoding='utf-8')
            # Chrome顶层窗口有最小宽度；iframe提供精确的移动端CSS视口。
            for width in (320, 390, 600, 768, 1024, 1440):
                page.write_text('<iframe onload="const f=this;setTimeout(()=>{const r=f.contentDocument.getElementById(\'layout-result\');if(r)document.body.append(r.cloneNode(true))},1000)" style="border:0;width:' + str(width) + 'px;height:900px" src="' +
                    inner_page.as_uri() + '"></iframe>', encoding='utf-8')
                with self.subTest(width=width):
                    result = subprocess.run([str(chrome), '--headless=new', '--disable-gpu',
                        '--no-proxy-server', '--disable-background-networking', '--no-first-run',
                        '--allow-file-access-from-files', '--virtual-time-budget=3000',
                        '--user-data-dir=' + str(Path(tmp) / str(width)), '--force-device-scale-factor=1',
                        f'--window-size={width},900', '--dump-dom', page.as_uri()],
                        capture_output=True, text=True, encoding='utf-8', timeout=45)
                    self.assertEqual(result.returncode, 0, result.stderr)
                    match = re.search(r'<output id="layout-result">(.*?)</output>', result.stdout)
                    self.assertIsNotNone(match, result.stdout[-1000:])
                    data = json.loads(unescape(match.group(1)))
                    self.assertEqual(data['width'], width)
                    self.assertEqual(data['failures'], [])
            for width, tab, name in ((1440, 'control', 'air-desktop'), (390, 'control', 'air-mobile')):
                shot = Path(tmp) / f'ui-{name}.png'
                page.write_text('<body style="margin:0"><iframe style="border:0;width:' + str(width) +
                    'px;height:900px" src="' + preview.as_uri() + '#' + tab + '"></iframe></body>', encoding='utf-8')
                subprocess.run([str(chrome), '--headless=new', '--disable-gpu', '--no-first-run',
                    '--allow-file-access-from-files', '--disable-background-networking', '--hide-scrollbars',
                    '--virtual-time-budget=1500', '--force-device-scale-factor=1',
                    '--user-data-dir=' + str(Path(tmp) / ('shot-' + name)),
                    f'--window-size={width},900', '--screenshot=' + str(shot), page.as_uri()],
                    capture_output=True, check=True, timeout=45)
                self.assertGreater(shot.stat().st_size, 1000)

    def test_actual_js_switch_all_channels(self):
        source = (ROOT / 'code/web_pages.h').read_text(encoding='utf-8')
        js = function(source, 'function upd(i)')
        harness = r'''
const assert=require('assert');const nodes={};const $=id=>nodes[id];
for(let i=0;i<3;i++){
 const input={};nodes['tp'+i]={value:'8'};nodes['cf'+i]={style:{}};
 nodes['k1'+i]={style:{},querySelector:()=>input};nodes['k1l'+i]={};
 for(const t of ['1','4','5','6','7','8','2']){
  nodes['tp'+i].value=t;upd(i);
  assert.equal(nodes['cf'+i].style.display,t==='4'?'block':'none');
  assert.equal(nodes['k1'+i].style.display,t==='8'?'block':'none');
  assert.equal(nodes['k1l'+i].innerText,'加签密钥 (可选)');
  assert.equal(input.placeholder,'飞书机器人签名校验密钥（可选）');
 }
}
'''
        subprocess.run(['node', '-e', js + harness], check=True, timeout=10)


if __name__ == '__main__':
    unittest.main(verbosity=2)
