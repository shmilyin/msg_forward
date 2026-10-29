"""编译实际调度类，覆盖北京时间边界、覆盖失效及时间跳变。"""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent
header = (ROOT / 'code/airplane_mode.h').read_text(encoding='utf-8')
logic = header[header.index('struct AirSchedule'):header.index('class ATTransaction')]
source = r'''
#include <ctime>
#include <cassert>
#include <iostream>
''' + logic + r'''
time_t at(int minute, int day=0){return 1800000000/86400*86400-28800+day*86400+minute*60;}
int main(){
 assert(!AirSchedule::validTime(0,0));assert(!AirSchedule::validTime(-1,20));assert(!AirSchedule::validTime(10,1440));
 for(int minute=0;minute<1440;++minute){
  assert(AirSchedule::inside(minute,600,900)==(minute>=600&&minute<900));
  assert(AirSchedule::inside(minute,1380,420)==(minute>=1380||minute<420));
 }
 AirSchedule s;s.update(true,1380,420,false,0);assert(!s.targetKnown);
 s.update(true,1380,420,false,at(1380));assert(s.targetKnown&&s.target);
 s.update(true,1380,420,false,at(420));assert(!s.target);
 s.manual(false,true,1380,420,at(1400));assert(s.overrideUntil==at(420,1));
 s.update(true,1380,420,false,at(0,1));assert(!s.target&&s.overrideActive);
 s.update(true,1380,420,false,at(420,1));assert(!s.overrideActive&&!s.target);
 s.update(true,1380,420,false,at(1400));assert(!s.overrideActive&&s.target);
 s.manual(true,true,600,900,0);assert(s.overrideActive&&!s.overrideUntil);
 s.update(true,600,900,false,at(700));assert(s.overrideUntil==at(900)&&s.target);
 s.update(true,600,900,false,at(1000,4));assert(!s.overrideActive&&!s.target);
 s.manual(true,false,600,900,0);s.update(false,600,900,true,0);assert(s.targetKnown&&s.target);
 s.clearOverride();s.update(true,600,900,false,at(700));assert(s.target);
 AirSchedule reboot;reboot.update(true,600,900,false,0);assert(!reboot.targetKnown&&!reboot.overrideActive);
 reboot.update(true,600,900,false,at(800));assert(reboot.target);
 for(int minute=0;minute<1440;++minute){auto now=at(minute);auto next=AirSchedule::nextBoundary(now,1380,420);assert(next>now&&next-now<=86400);}
 std::cout<<"PASS: actual schedule, 2880 minute cases, boundaries, sync, restart, override, jumps\n";
}
'''
with tempfile.TemporaryDirectory(prefix='airplane_') as tmp:
    cpp = Path(tmp) / 'test.cpp'
    exe = Path(tmp) / 'test.exe'
    cpp.write_text(source, encoding='utf-8')
    compiler = ROOT / '.tools/w64devkit/bin/g++.exe'
    env = dict(os.environ, PATH=str(compiler.parent) + os.pathsep + os.environ.get('PATH', ''))
    subprocess.run([str(compiler), '-std=c++11', '-Wall', '-Wextra', '-Werror', str(cpp), '-o', str(exe)], check=True, env=env)
    subprocess.run([str(exe)], check=True, env=env)
