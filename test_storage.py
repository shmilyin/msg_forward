"""提取实际历史存储函数，验证限长流式读取和裁剪失败恢复。"""
from pathlib import Path
import tempfile
import os
import subprocess
from test_notifications import function
ROOT=Path(__file__).resolve().parent
config=(ROOT/'code/config.ino').read_text(encoding='utf-8')
mock=r'''
#include <string>
#include <cstdint>
#include <map>
#include <vector>
#include <memory>
#include <algorithm>
#include <cstring>
#include <cassert>
#include <iostream>
using std::size_t;
class String {public:std::string s;String(const char* p=""):s(p?p:""){}String(std::string p):s(p){}size_t length()const{return s.size();}const char*c_str()const{return s.c_str();}char operator[](unsigned i)const{return s[i];}String substring(unsigned a,unsigned b=~0u)const{return s.substr(a,std::min(b,(unsigned)s.size())-a);}String&operator+=(const String&b){s+=b.s;return *this;}String&operator+=(char c){s+=c;return *this;}friend String operator+(const String&a,const String&b){return a.s+b.s;}};
struct File {
 std::shared_ptr<std::string> data;size_t pos=0;bool writable=false;
 operator bool()const{return bool(data);}size_t size(){return data?data->size():0;}size_t position(){return pos;}bool available(){return data&&pos<data->size();}
 bool seek(size_t p){if(!data||p>data->size())return false;pos=p;return true;}
 int read(){return available()?(unsigned char)(*data)[pos++]:-1;}
 size_t read(uint8_t* out,size_t n){n=std::min(n,size()-pos);memcpy(out,data->data()+pos,n);pos+=n;return n;}
 size_t write(const uint8_t* in,size_t n);
 size_t print(const String& s){return write((const uint8_t*)s.c_str(),s.length());}
 void flush(){}void close(){data.reset();}
};
struct FS {
 std::map<std::string,std::shared_ptr<std::string>> files;bool failWrite=false,failPromote=false,failRecover=false,failRead=false;std::string failRemove;std::vector<std::string> removed;size_t capacity=200000;
 bool begin(bool){return true;}bool exists(const char* p){return files.count(p);}
 bool remove(const char* p){removed.push_back(p);if(failRemove==p)return false;return files.erase(p);}
 bool rename(const char*a,const char*b){if(failRecover&&std::string(a)=="/sms.bak")return false;if(failPromote&&std::string(a)=="/sms.tmp")return false;if(!files.count(a)||files.count(b))return false;files[b]=files[a];files.erase(a);return true;}
 File open(const char*p,const char*mode){if(mode[0]=='r'&&(failRead||!files.count(p)))return {};if(mode[0]=='w'||!files.count(p))files[p]=std::make_shared<std::string>();File f;f.data=files[p];f.writable=mode[0]!='r';f.pos=mode[0]=='a'?f.size():0;return f;}
 size_t totalBytes(){return capacity;}size_t usedBytes(){size_t n=0;for(auto& p:files)n+=p.second->size();return n;}
}SPIFFS;
size_t File::write(const uint8_t* in,size_t n){if(!data||!writable||SPIFFS.failWrite)return 0;data->append((const char*)in,n);pos+=n;return n;}
struct {void println(const char*){}}Serial;
void yield(){}
#define MAX_SMS_HISTORY 100
#define CONTENT_LENGTH_UNKNOWN -1
struct {unsigned long smsReceived=0;}stats;
void saveStats(){}
const int HTTP_POST=1;bool authorized=true,storageMounted=true;bool checkAuth(){return authorized;}
bool smsStorageReady(){return storageMounted;}
struct {std::string output;int status=0,verb=HTTP_POST;std::map<std::string,std::string> headers;size_t maxChunk=0;int method(){return verb;}void sendHeader(const char*k,const char*v){headers[k]=v;}void setContentLength(int){}void send(int code,const char*,const char*body){status=code;output=body;maxChunk=0;}void sendContent(const char* p,size_t n){output.append(p,n);maxChunk=std::max(maxChunk,n);}void sendContent(const char*p){sendContent(p,strlen(p));}}server;
'''
actual='\n'.join(function(config,sig) for sig in ('void recoverSmsStorage(', 'bool clearSmsHistory(', 'bool rewriteSmsStorage(', 'bool compactSmsStorage(', 'bool repairSmsStorageTail(', 'void addSmsToHistory(', 'void streamSmsHistory('))
actual=function((ROOT/'code/push_service.ino').read_text(encoding='utf-8'),'String jsonEscape(').replace('str.charAt(i)','str[i]')+'\n'+actual
actual+='\n'+function((ROOT/'code/web_handlers.ino').read_text(encoding='utf-8'),'void handleClearSmsHistory(')
assert 'server.on("/history/clear", handleClearSmsHistory);' in (ROOT/'code/code.ino').read_text(encoding='utf-8')
tests=r'''
void put(const char* path,const std::string& data){SPIFFS.files[path]=std::make_shared<std::string>(data);}
int main(){
 std::string rows;for(int i=0;i<150;++i)rows+="{\"t\":\"2026\",\"s\":\""+std::to_string(i)+"\",\"m\":\"x\"}\n";
 put("/sms.txt",rows);streamSmsHistory();assert(server.maxChunk<=2047);assert(server.output.find("\"s\":\"149\"")<server.output.find("\"s\":\"50\""));assert(server.output.find("\"s\":\"49\"")==std::string::npos);
 put("/sms.txt",std::string(100000,'X')+"\n"+rows);streamSmsHistory();assert(server.output.find("XXXXX")==std::string::npos);
 put("/sms.txt",rows);SPIFFS.failRead=true;addSmsToHistory("blocked","read failure","now");assert(*SPIFFS.files["/sms.txt"]==rows);SPIFFS.failRead=false;
 SPIFFS.files.erase("/sms.txt");put("/sms.bak",rows);SPIFFS.failRecover=true;addSmsToHistory("blocked","recover failure","now");assert(!SPIFFS.exists("/sms.txt")&&*SPIFFS.files["/sms.bak"]==rows);SPIFFS.failRecover=false;recoverSmsStorage();
 put("/sms.txt",rows);SPIFFS.failWrite=true;assert(!compactSmsStorage());assert(*SPIFFS.files["/sms.txt"]==rows);SPIFFS.failWrite=false;
 SPIFFS.failPromote=true;assert(!compactSmsStorage());assert(*SPIFFS.files["/sms.txt"]==rows);SPIFFS.failPromote=false;
 put("/sms.txt",rows+"{partial");
 SPIFFS.failWrite=true;assert(!repairSmsStorageTail());assert(*SPIFFS.files["/sms.txt"]==rows+"{partial");SPIFFS.failWrite=false;
 SPIFFS.failPromote=true;assert(!repairSmsStorageTail());assert(*SPIFFS.files["/sms.txt"]==rows+"{partial");SPIFFS.failPromote=false;
 assert(repairSmsStorageTail());assert(*SPIFFS.files["/sms.txt"]==rows);
 put("/sms.txt",rows+"{partial");addSmsToHistory("tail","appended","now");assert(SPIFFS.files["/sms.txt"]->substr(0,rows.size())==rows);assert(SPIFFS.files["/sms.txt"]->find("partial")==std::string::npos);
 put("/sms.txt",rows+"{partial");
 assert(compactSmsStorage());assert(SPIFFS.files["/sms.txt"]->find("partial")==std::string::npos);assert(SPIFFS.files["/sms.txt"]->front()=='{');assert(SPIFFS.files["/sms.txt"]->back()=='\n');assert(SPIFFS.files["/sms.txt"]->size()<rows.size());
 SPIFFS.files.erase("/sms.txt");recoverSmsStorage();assert(*SPIFFS.files["/sms.txt"]==rows+"{partial");
 SPIFFS.capacity=SPIFFS.usedBytes();assert(!compactSmsStorage());SPIFFS.capacity=200000;
 addSmsToHistory("a\"b","中文\n\"\\\x01","now");streamSmsHistory();assert(server.output.find("a\\\"b")!=std::string::npos);assert(server.output.find("\\u0001")!=std::string::npos);
 const auto received=stats.smsReceived;
 put("/other.txt","keep");put("/config.json","config");
 for(const char* failed:{"/sms.tmp","/sms.bak","/sms.txt"}){
  put("/sms.txt",rows);put("/sms.bak","backup");put("/sms.tmp","temporary");
  SPIFFS.failRemove=failed;SPIFFS.removed.clear();handleClearSmsHistory();
  assert(server.status==500&&server.output.find("false")!=std::string::npos);
  assert(SPIFFS.exists("/sms.txt"));recoverSmsStorage();assert(*SPIFFS.files["/sms.txt"]==rows);
  SPIFFS.failRemove.clear();
 }
 put("/sms.txt",rows);put("/sms.bak","backup");put("/sms.tmp","temporary");
 authorized=false;SPIFFS.removed.clear();handleClearSmsHistory();assert(SPIFFS.removed.empty());authorized=true;
 for(int verb:{0,2,3,4}){server.verb=verb;handleClearSmsHistory();assert(server.status==405&&server.headers["Allow"]=="POST"&&SPIFFS.removed.empty());}
 server.verb=HTTP_POST;storageMounted=false;handleClearSmsHistory();
 assert(server.status==500&&server.output.find("\"success\":false")!=std::string::npos&&SPIFFS.removed.empty()&&*SPIFFS.files["/sms.txt"]==rows);
 storageMounted=true;handleClearSmsHistory();assert(server.status==200&&server.output.find("true")!=std::string::npos);
 assert((SPIFFS.removed==std::vector<std::string>{"/sms.tmp","/sms.bak","/sms.txt"}));
 recoverSmsStorage();assert(!SPIFFS.exists("/sms.txt")&&!SPIFFS.exists("/sms.bak")&&!SPIFFS.exists("/sms.tmp"));
 assert(*SPIFFS.files["/other.txt"]=="keep"&&*SPIFFS.files["/config.json"]=="config"&&stats.smsReceived==received);
 assert(clearSmsHistory());streamSmsHistory();assert(server.output=="{\"history\":[]}");
 storageMounted=false;SPIFFS.removed.clear();handleClearSmsHistory();assert(server.status==500&&SPIFFS.removed.empty());storageMounted=true;
 put("/sms.bak",rows);put("/sms.tmp","temporary");SPIFFS.failRemove="/sms.bak";assert(!clearSmsHistory());recoverSmsStorage();assert(*SPIFFS.files["/sms.txt"]==rows);SPIFFS.failRemove.clear();assert(clearSmsHistory());
 put("/sms.bak",rows);assert(clearSmsHistory());recoverSmsStorage();assert(!SPIFFS.exists("/sms.txt"));
 addSmsToHistory("new","after clear","now");streamSmsHistory();assert(server.output.find("after clear")!=std::string::npos);
 std::cout<<"PASS: clear main/backup/temp, ordered deletion, failures/recovery, auth/method, unrelated files/stats preserved; actual streaming history, 100 offsets, long-line bound, 512-byte compaction, write/rename/space failures and recovery\n";
}
'''
with tempfile.TemporaryDirectory(prefix='storage_') as tmp:
    cpp=Path(tmp)/'test.cpp';exe=Path(tmp)/'test.exe';cpp.write_text(mock+actual+tests,encoding='utf-8')
    compiler=ROOT/'.tools/w64devkit/bin/g++.exe';env=dict(os.environ,PATH=str(compiler.parent)+os.pathsep+os.environ.get('PATH',''))
    subprocess.run([str(compiler),'-std=c++11','-Wall','-Wextra','-Werror',str(cpp),'-o',str(exe)],check=True,env=env)
    subprocess.run([str(exe)],check=True,env=env)
