"""实际飞行状态机及认证API的离线串口测试，不访问设备和网络。"""
import runpy
import os
import subprocess
import tempfile
from pathlib import Path
ROOT = Path(__file__).resolve().parent
base = runpy.run_path(str(ROOT / 'test_modem.py'))
header = (ROOT / 'code/airplane_mode.h').read_text(encoding='utf-8')
logic = header[header.index('struct AirSchedule'):header.index('class ATTransaction')]
mock = base['mock'].replace('bool cellularReady(){return true;} String cellularBlockReason(){return "blocked";}', 'bool cellularReady(); String cellularBlockReason();')
mock = mock.replace('#include <string>', '#include <ctime>\n#include <cstring>\n#include <vector>\n#include <utility>\n#include <cstdint>\n#include <string>')
mock = mock.replace('String(int n):s(std::to_string(n)){}', 'String(int n):s(std::to_string(n)){} String(long long n):s(std::to_string(n)){} String(unsigned int n):s(std::to_string(n)){}')
extra = r'''
struct tm* gmtime_r(const time_t* p, struct tm* out){*out=*gmtime(p);return out;}
time_t epoch=1800000000;
time_t fakeTime(time_t*){return epoch;}
#define time fakeTime
struct {bool airplaneEnabled=false,airplaneManualTarget=false,timerEnabled=false;int airplaneStartMinute=1380,airplaneEndMinute=420,timerType=0;String timerPhone,timerMessage;} config;
bool configValid=true,timerPending=false;
uint64_t timerLastMs=0,monotonicMs=0;
unsigned long timerIntervalSec=10,lastTimerExec=0;
uint64_t esp_timer_get_time(){return monotonicMs*1000;}
struct {int smsSent=0;}stats;void saveStats(){}
const int HTTP_POST=1;
struct Server {
 bool authenticated=true;int code=0,verb=HTTP_POST;String body;
 std::vector<std::pair<String,String>> params;
 int args(){return params.size();}String argName(int n){return params[n].first;}
 String arg(const char* name){for(auto& p:params)if(p.first==name)return p.second;return "";}
 int method(){return verb;}
 void send(int c,const char*,const String& b){code=c;body=b;}
}server;
int saved=0;void saveConfig(){++saved;}
bool checkAuth(){if(!server.authenticated){server.code=401;return false;}return true;}
String jsonEscape(const String& s){return s;}
'''
tests = r'''
int physical=1;bool failSet=false,failCNMI=false,unknown=false,mismatch=false,timeoutSet=false;int sets=0;
void fixture(){clockMs=0;atTimedOut=false;Serial1.rx.clear();Serial1.feed("\n");readModemLine(false);physical=1;failSet=failCNMI=unknown=mismatch=timeoutSet=false;sets=0;config.airplaneEnabled=false;config.airplaneManualTarget=false;server.params.clear();server.authenticated=true;initAirplaneMode();
 Serial1.onCommand=[](String cmd){
  if(cmd=="AT+CFUN?")Serial1.feed(unknown?"OK\n":physical==4?"+CFUN: 4\nOK\n":"+CFUN: 1\nOK\n");
  else if(cmd.startsWith("AT+CFUN=")){++sets;if(!failSet&&!mismatch)physical=cmd=="AT+CFUN=4"?4:1;if(!timeoutSet)Serial1.feed(failSet?"ERROR\n":"OK\n");}
  else if(cmd=="AT+CNMI=2,2,0,0,0")Serial1.feed(failCNMI?"ERROR\n":"OK\n");
  else if(cmd=="AT+CPIN?")Serial1.feed("+CPIN: READY\nOK\n");
  else if(cmd=="AT+CEREG?")Serial1.feed("+CEREG: 2,1\nOK\n");
  else if(cmd=="AT+CGATT?")Serial1.feed("+CGATT: 1\nOK\n");
  else Serial1.feed("OK\n");
 };
}
int main(){
 fixture();serviceAirplaneMode();assert(airActual==1&&!airRestoring&&cellularReady());
 config.airplaneManualTarget=true;airAttempted=false;serviceAirplaneMode();assert(physical==4&&airActual==4&&!cellularReady());
 config.airplaneManualTarget=false;airAttempted=false;failCNMI=true;serviceAirplaneMode();assert(airActual==1&&airRestoring&&!cellularReady()&&airError.length());
 failCNMI=false;airAttempted=false;serviceAirplaneMode();assert(cellularReady());
 fixture();config.airplaneManualTarget=true;failSet=true;serviceAirplaneMode();assert(airActual==1&&airError.length()&&!cellularReady());
 fixture();config.airplaneManualTarget=true;mismatch=true;serviceAirplaneMode();assert(airActual==1&&airError.length());
 fixture();config.airplaneManualTarget=true;timeoutSet=true;serviceAirplaneMode();assert(physical==4&&airActual==4&&airError.length()==0);
 fixture();unknown=true;failSet=true;serviceAirplaneMode();assert(airActual==-1&&!cellularReady());
 fixture();config.airplaneEnabled=true;epoch=0;serviceAirplaneMode();assert(!airSchedule.targetKnown&&sets==0&&!cellularReady());
 fixture();{ATTransaction owner;assert(owner);ATTransaction denied;assert(!denied);serviceAirplaneMode();assert(!airAttempted);assert(!sendSMS("123","x"));}
 assert(!modemTransactionBusy());
 fixture();server.authenticated=false;handleAirplaneStatus();assert(server.code==401);int before=saved;handleAirplaneSave();assert(saved==before);
 fixture();server.params={{"enabled","true"},{"start","23:00"},{"end","07:00"}};handleAirplaneSave();assert(server.code==202&&config.airplaneEnabled);
 before=saved;server.params.push_back({"enabled","false"});handleAirplaneSave();assert(server.code==400&&saved==before&&config.airplaneEnabled);
 server.params={{"enabled","1"},{"start","23:00"},{"end","07:00"}};handleAirplaneSave();assert(server.code==400&&saved==before);
 server.params={{"enabled","true"},{"start","24:00"},{"end","07:00"}};handleAirplaneSave();assert(server.code==400&&saved==before);
 server.params={{"enabled","true"},{"start","07:00"},{"end","07:00"}};handleAirplaneSave();assert(server.code==400&&saved==before);
 server.params={{"action","on"}};handleAirplaneAction();assert(server.code==202&&airSchedule.overrideActive&&airSchedule.target);
 server.params={{"action","auto"}};handleAirplaneAction();assert(server.code==202&&!airSchedule.overrideActive);
 server.params={{"action","bad"}};handleAirplaneAction();assert(server.code==400);
 server.params={{"action","on"}};server.verb=0;handleAirplaneAction();assert(server.code==400);
 fixture();atTimedOut=true;Serial1.feed("OK\n+CMT: ,2\nAABB\n");String r=sendATCommand("ATI",100);assert(atResponseOK(r)&&smsPduCount>0);
 fixture();atTimedOut=true;Serial1.onCommand=[](String){Serial1.feed("OK\n");};r=sendATCommand("ATI",100);assert(!atResponseOK(r)&&atTimedOut);
 std::cout<<"PASS: actual CFUN state machine, mismatch/errors/unknown, CNMI recovery, ownership, auth/API validation and late OK resync\n";
}
'''
main_code = (ROOT / 'code/code.ino').read_text(encoding='utf-8')
timer_code = main_code[main_code.index('  if (config.timerEnabled && timerIntervalSec'):main_code.index('  if (watchdogReady) esp_task_wdt_reset();', main_code.index('  if (config.timerEnabled && timerIntervalSec'))]
tests = tests.replace(' std::cout<<"PASS:', ''' fixture();config.timerEnabled=true;config.airplaneManualTarget=true;monotonicMs=10000;runTimer();assert(timerPending&&stats.smsSent==0);
 monotonicMs=100000;runTimer();assert(timerPending);config.airplaneManualTarget=false;airAttempted=false;serviceAirplaneMode();runTimer();assert(!timerPending);runTimer();assert(!timerPending);
 std::cout<<"PASS:''')
source = mock + logic + base['transaction_class'] + extra + base['web'] + base['sms'] + (ROOT / 'code/airplane_mode.ino').read_text(encoding='utf-8') + '\nvoid runTimer(){\n' + timer_code + '\n}\n' + tests
with tempfile.TemporaryDirectory(prefix='air_device_') as tmp:
    cpp=Path(tmp)/'test.cpp';exe=Path(tmp)/'test.exe';cpp.write_text(source,encoding='utf-8')
    compiler=ROOT/'.tools/w64devkit/bin/g++.exe'
    env=dict(os.environ,PATH=str(compiler.parent)+os.pathsep+os.environ.get('PATH',''))
    subprocess.run([str(compiler),'-std=c++11','-Wall','-Wextra','-Werror','-Wno-sign-compare',str(cpp),'-o',str(exe)],check=True,env=env)
    subprocess.run([str(exe)],check=True,env=env,timeout=20)
