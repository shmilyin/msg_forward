"""离线编译真实STK handler、共享AT事务/reader；不访问设备。"""
import os
from pathlib import Path
import runpy
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent
base = runpy.run_path(str(ROOT / 'test_modem.py'))
mock = base['mock'].replace('String(int n):s(std::to_string(n)){}', 'String(int n):s(std::to_string(n)){} String(unsigned long n):s(std::to_string(n)){} String(unsigned int n):s(std::to_string(n)){}')
headers = (ROOT / 'code/airplane_mode.h').read_text(encoding='utf-8')
headers = headers[headers.index('typedef void'):headers.index('bool modemTransactionBusy')]
stk = (ROOT / 'code/stk.ino').read_text(encoding='utf-8')
stk = stk[stk.index('void handleStkMenu()'):]
push = (ROOT / 'code/push_service.ino').read_text(encoding='utf-8')
escape = push[push.index('String jsonEscape('):push.index('// HMAC-SHA256')]
server_mock = r'''
#include <vector>
#include <cstdint>
#define HTTP_POST 1
#define HTTP_GET 0
struct Server {
 bool auth=true;int verb=HTTP_POST,status=0;String body;
 std::vector<std::pair<String,String>> params;
 int method(){return verb;} int args(){return params.size();}
 String argName(int i){return params[i].first;}
 String arg(const char* key){for(auto& p:params)if(p.first==key)return p.second;return "";}
 void send(int code,const char*,const String& value){status=code;body=value;}
} server;
bool switching=false;
bool modemDiagnosticSwitching(){return switching;}
bool checkAuth(){if(!server.auth){server.status=401;return false;}return true;}
'''
tests = r'''
void resetProbe(){resetRx();server.auth=true;server.verb=HTTP_POST;server.params={{"op","commands"}};server.body="";server.status=0;switching=false;}
int main(){
 // 真实SET UP MENU TLV；ASCII与UCS2，畸形/重复项不覆盖已接收菜单。
 StkMenu menu;
 assert(parseStkMenu("D017810301250082028182850453494D318F06014D656E7531",menu));
 assert(menu.available&&menu.count==1&&menu.title=="SIM1"&&menu.items[0]=="Menu1");
 StkMenu removed;assert(parseStkMenu("D00B8103012500820281828F00",removed)&&!removed.available);
 StkMenu chinese;assert(parseStkMenu("D0148103012500820281828505804E2D65878F020141",chinese));
 assert(chinese.title.length()==6);
 for(auto hex:{"D00100","D080","D081FF","D00C8103012500820281828F00","D009810301210082028182","D00B8103012500820281828F01","GG"}){StkMenu bad;assert(!parseStkMenu(hex,bad));}
 observeStkUrc("+MSTK:11, \"D017810301250082028182850453494D318F06014D656E7531\"");
 assert(stkMenu.available&&stkMenu.count==1&&stkEvents==1);
 observeStkUrc("+MSTK:3, \"FF\"");assert(stkEvents==1);
 resetProbe();server.auth=false;handleStkMenu();assert(server.status==401);
 server.auth=true;handleStkMenu();assert(server.status==200&&server.body.indexOf("Menu1")>=0&&Serial1.tx.length()==0);
 // 同一个AT响应中交错STK通知和短信PDU，两者均保留，不触发业务处理。
 resetProbe();server.params={{"op","profile"}};
 Serial1.onCommand=[](String cmd){assert(cmd=="AT+MSTK=3");Serial1.feed("+MSTK:11, \"D017810301250082028182850453494D318F06014D656E7531\"\n+CMT: ,2\nAABB\n+MSTK:3, \"FF\"\nOK\n");};
 handleStkProbe();assert(server.status==200&&stkMenu.available&&smsPduCount==1&&processed==0&&server.body.indexOf("FF")>=0);
 observeStkUrc("+MSTK:11, \"D00100\"");assert(stkMenu.available);
 observeStkUrc("+MSTK:11, \"D00B8103012500820281828F00\"");assert(!stkMenu.available&&stkMenu.count==0);
 resetProbe();int sent=0;Serial1.onCommand=[&](String){++sent;};
 server.auth=false;handleStkProbe();assert(server.status==401&&sent==0);
 server.auth=true;server.verb=HTTP_GET;handleStkProbe();assert(server.status==405&&sent==0);
 server.verb=HTTP_POST;
 for(auto args:std::vector<std::vector<std::pair<String,String>>>{ {},{{"op","enable"}},{{"op","mstk_enable"}},{{"op","0"}},{{"op","1"}},{{"op","AT+MSTK?"}},{{"op","AT+MSTK=1"}},{{"cmd","AT+MSTK=?"}},{{"op","mstk_test"},{"cmd","AT+MSTK=1"}},{{"op","mstk_test"},{"operation","1"}},{{"cmd","ATI"}},{{"op","model"},{"op","commands"}},{{"op","commands"},{"method","GET"}},{{"op","model"},{"cmd","ATI"}} }){
  server.params=args;handleStkProbe();assert(server.status==400&&sent==0);
 }
 server.params={{"op","model"}};switching=true;handleStkProbe();assert(server.status==409&&sent==0);switching=false;
 {ATTransaction owner;handleStkProbe();assert(server.status==409&&sent==0&&modemTransactionBusy());}
 assert(!modemTransactionBusy());
 for(auto op: {"model","sim_access","commands","mstk_test","profile"}){
  resetProbe();server.params={{"op",op}};sent=0;
  Serial1.onCommand=[&](String cmd){assert(modemTransactionBusy());++sent;assert(cmd==(String(op)=="model"?"ATI":String(op)=="commands"?"AT+CLAC":String(op)=="mstk_test"?"AT+MSTK=?":String(op)=="profile"?"AT+MSTK=3":"AT+CSIM=?"));Serial1.feed("OK\n");};
  handleStkProbe();assert(sent==1&&server.status==200&&!modemTransactionBusy());assert(server.body.indexOf("\"terminated\":true")>=0);
 }
 // 非CLAC操作仍使用3000ms预算，MSTK的ERROR只说明测试形式不支持。
 for(auto op: {"model","sim_access","mstk_test"}){
  resetProbe();server.params={{"op",op}};Serial1.onCommand=[](String){};unsigned long begin=clockMs;
  handleStkProbe();assert(clockMs-begin>=3000&&clockMs-begin<5000&&atTimedOut);
 }
 resetProbe();server.params={{"op","mstk_test"}};Serial1.onCommand=[](String cmd){assert(cmd=="AT+MSTK=?");Serial1.feed("ERROR\n");};
 handleStkProbe();assert(server.body.indexOf("\"error\":true")>=0&&server.body.indexOf("\"timeout\":false")>=0&&server.body.indexOf("测试形式不支持")>=0&&server.body.indexOf("不能据此判断命令不存在")>=0&&server.body.indexOf("not advertised")<0);
 // 实机规模的CLAC在3.5秒才收到终结；等待期间仍yield并分发短信URC。
 resetProbe();unsigned long lateStart=clockMs,lateYields=yields;bool lateOK=false;
 Serial1.onCommand=[](String cmd){assert(cmd=="AT+CLAC");for(int i=0;i<371;i++)Serial1.feed("AT+STATUS\n");Serial1.feed("AT+MSTK\nAT+CSIM\nAT+CRSM\n+CMT: ,2\nAABB\n");};
 onTick=[&](){if(!lateOK&&clockMs-lateStart>=3500){lateOK=true;Serial1.feed("OK\n");}};
 handleStkProbe();assert(lateOK&&clockMs-lateStart>=3500&&clockMs-lateStart<6000&&yields>lateYields&&!atTimedOut&&smsPduCount==1&&processed==0);
 assert(server.body.indexOf("\"lines\":375")>=0&&server.body.indexOf("\"terminated\":true")>=0&&server.body.indexOf("\"timeout\":false")>=0&&server.body.indexOf("AT+MSTK")>=0&&server.body.indexOf("AT+CSIM")>=0&&server.body.indexOf("AT+CRSM")>=0);
 resetProbe();Serial1.onCommand=[](String){for(int i=0;i<300;i++)Serial1.feed("AT+STATUS\n");Serial1.feed("+CMT: ,2\nAABB\nAT+CUSATW\nAT+STK\nAT+CSIM\nAT+CRSM\nOK\n");};
 handleStkProbe();assert(server.status==200&&smsPduCount==1&&processed==0);assert(server.body.indexOf("AT+STK")>=0&&server.body.indexOf("AT+CUSATW")>=0);assert(server.body.indexOf("AT+STATUS")<0);assert(server.body.indexOf("\"lines\":305")>=0);assert(server.body.indexOf("\"truncated\":false")>=0&&server.body.indexOf("advertised")>=0);assert(server.body.length()<4000);
 resetProbe();Serial1.onCommand=[](String){Serial1.feed("AT+STATUS\nOK\n");};handleStkProbe();assert(server.body.indexOf("not advertised")>=0);
 resetProbe();Serial1.onCommand=[](String){Serial1.feed("ERROR\n");};handleStkProbe();assert(server.body.indexOf("\"error\":true")>=0&&server.body.indexOf("not advertised")<0&&server.body.indexOf("\"timeout\":false")>=0);
 resetProbe();Serial1.onCommand=[](String){Serial1.feed("TOKEN_OK_TEXT\n");};unsigned long start=clockMs;handleStkProbe();assert(clockMs-start>=6000&&clockMs-start<8000&&atTimedOut);assert(server.body.indexOf("\"timeout\":true")>=0&&server.body.indexOf("not advertised")<0);
 // 超时后沿用真实CFUN只读重同步；不把同步响应当作CLAC响应。
 std::vector<String> commands;Serial1.onCommand=[&](String c){commands.push_back(c);Serial1.feed(c=="AT+CFUN?"?"+CFUN: 4\nOK\n":"AT+STK\nOK\n");};start=clockMs;handleStkProbe();assert(clockMs-start<8000&&commands.size()==2&&commands[0]=="AT+CFUN?"&&commands[1]=="AT+CLAC"&&!atTimedOut);assert(server.body.indexOf("AT+STK")>=0);
 resetProbe();atTimedOut=true;Serial1.onCommand=[](String){};start=clockMs;handleStkProbe();assert(clockMs-start<8000&&atTimedOut&&server.body.indexOf("not advertised")<0);
 // 重同步接近1500ms后成功，CLAC再耗尽6000ms，总预算仍小于8000ms。
 resetProbe();atTimedOut=true;unsigned long syncStart=0;bool syncOK=false;commands.clear();
 Serial1.onCommand=[&](String cmd){commands.push_back(cmd);if(cmd=="AT+CFUN?")syncStart=clockMs;else assert(cmd=="AT+CLAC");};
 onTick=[&](){if(syncStart&&!syncOK&&clockMs-syncStart>=1450){syncOK=true;Serial1.feed("+CFUN: 4\nOK\n");}};
 start=clockMs;handleStkProbe();assert(syncOK&&commands.size()==2&&clockMs-start>=7700&&clockMs-start<8000&&atTimedOut&&server.body.indexOf("\"timeout\":true")>=0);
 resetProbe();Serial1.onCommand=[](String){Serial1.feed(std::string(700,'X')+"OK\nOK\n");};handleStkProbe();assert(server.body.indexOf("\"truncated\":true")>=0&&server.body.indexOf("\"terminated\":true")>=0);
 resetProbe();Serial1.onCommand=[](String){for(int i=0;i<40;i++)Serial1.feed("AT+STK"+std::to_string(i)+"\n");Serial1.feed("AT+STK"+std::string(100,'X')+"\nOK\n");};handleStkProbe();assert(server.body.indexOf("\"truncated\":true")>=0&&server.body.indexOf("AT+STK31")>=0&&server.body.indexOf("AT+STK32")<0);
 StkProbe p;observeStkLine("AT+STK"+String(std::string(100,'X')),false,&p);assert(p.count==1&&p.candidates[0].length()==80&&p.truncated);
 resetProbe();server.params={{"op","model"}};Serial1.onCommand=[](String){Serial1.feed("<script>alert(1)</script>\nOK\n");};handleStkProbe();assert(server.body.indexOf("<script>")>=0);
 resetProbe();Serial1.onCommand=[](String){Serial1.flood=true;};start=clockMs;auto oldYields=yields;handleStkProbe();assert(clockMs-start<8000&&yields>oldYields&&atTimedOut&&server.body.indexOf("\"truncated\":true")>=0);resetProbe();
 std::cout<<"PASS: STK actual handler/reader auth, method, args, transaction, long CLAC tail, URC, ERROR, timeout/resync, flood, bounds\n";
}
'''
reset = base['tests'][base['tests'].index('void resetRx()'):base['tests'].index('int main()')]
with tempfile.TemporaryDirectory(prefix='stk_mock_') as tmp:
    cpp, exe = Path(tmp) / 'test.cpp', Path(tmp) / 'test.exe'
    cpp.write_text(mock + headers + base['transaction_class'] + base['web'] + base['sms'] + server_mock + escape + (ROOT / 'code/stk.h').read_text(encoding='utf-8') + stk + reset + tests, encoding='utf-8')
    compiler = ROOT / '.tools/w64devkit/bin/g++.exe'
    env = dict(os.environ, PATH=str(compiler.parent) + os.pathsep + os.environ.get('PATH', ''))
    subprocess.run([str(compiler), '-std=c++11', '-Wall', '-Wextra', '-Wno-sign-compare', str(cpp), '-o', str(exe)], check=True, env=env)
    subprocess.run([str(exe)], check=True, env=env, timeout=20)

# 直接执行页面中的真实函数，禁止用innerHTML接收模组输出。
page = (ROOT / 'code/web_pages.h').read_text(encoding='utf-8')
js = page[page.index('let stkBusy='):page.index('let airLoaded=')]
assert 'innerHTML' not in js
assert all(page.count(f"probeStk('{op}')") == 1 for op in ('model', 'commands', 'sim_access', 'mstk_test', 'profile', 'menu'))
assert "probeStk('mstk_test')\">查询STK参数</button>" in page
assert 'HTTP_ANY, handleStkProbe' in (ROOT / 'code/code.ino').read_text(encoding='utf-8')
js_test = r'''
const assert=require('node:assert/strict');
const out={textContent:''},buttons=[{},{},{},{}];
const $=()=>out;const document={querySelectorAll:()=>buttons};
let calls=[],release;
const fetch=(url,options)=>{calls.push({url,options});return new Promise(r=>release=r)};
(async()=>{
 assert.equal(calls.length,0);
 const pending=probeStk('model');assert(buttons.every(b=>b.disabled));
 await probeStk('commands');await probeStk('cmd');assert.equal(calls.length,1);
 assert.equal(calls[0].url,'/stk/probe');assert.equal(calls[0].options.method,'POST');assert.equal(calls[0].options.body.toString(),'op=model');
 release({ok:true,json:async()=>({raw:'<img src=x onerror=alert(1)>'})});await pending;
 assert(out.textContent.includes('<img'));assert(buttons.every(b=>!b.disabled));
 for(const op of ['mstk_enable','cmd','AT+MSTK=?','AT+MSTK?','AT+MSTK=1','0','1',0,1])await probeStk(op);
 assert.equal(calls.length,1);
 const mstk=probeStk('mstk_test');assert.equal(calls.length,2);assert.equal(calls[1].options.method,'POST');assert.equal(calls[1].options.body.toString(),'op=mstk_test');
 release({ok:true,json:async()=>({command:'AT+MSTK=?',error:true,message:'MSTK参数测试形式不支持；不能据此判断命令不存在。'})});await mstk;
 assert(out.textContent.includes('不能据此判断命令不存在'));assert(buttons.every(b=>!b.disabled));
 const failed=probeStk('sim_access');release({ok:false,status:409,json:async()=>({message:'忙碌'})});await failed;assert(out.textContent.startsWith('HTTP 409'));
 const profile=probeStk('profile');assert.equal(calls.at(-1).options.body.toString(),'op=profile');release({ok:true,json:async()=>({raw:'FF'})});await profile;
 const menu=probeStk('menu');assert.equal(calls.at(-1).url,'/stk/menu');assert.equal(calls.at(-1).options,undefined);release({ok:true,json:async()=>({available:false,items:[]})});await menu;
 console.log('PASS: actual page JS fixed POST, passive GET cache, no auto probe, concurrent disable, raw textContent, error recovery');
})().catch(e=>{console.error(e);process.exitCode=1});
'''
subprocess.run(['node', '-e', js + js_test], check=True, timeout=20)
