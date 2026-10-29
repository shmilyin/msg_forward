"""真实pdulib解码十段接收：离线串口交错AT，不访问设备。"""
import os
import runpy
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent
base = runpy.run_path(str(ROOT / 'test_modem.py'))
mock = base['mock']
a = mock.index('struct PDU{')
b = mock.index('String readModemLine(bool);', a)
mock = mock[:a] + '''#include <pdulib.h>
PDU pdu(4096);
int processed=0;String received;
void processSmsContent(const char*,const char* text,const char*){++processed;received=text;}
''' + mock[b:]
# SMS-DELIVER，UCS2正文和8位UDH；固定虚构号码，绝不发送。
frames = []
expected = ''
for part in range(1, 11):
    text = f'第{part}段' + '测' * (67 - len(f'第{part}段'))
    expected += text
    ud = bytes([5, 0, 3, 42, 10, part]) + text.encode('utf-16-be')
    frames.append('07912143658709F1400B912143658709F1000862010100000023' + f'{len(ud):02X}' + ud.hex().upper())
wire = ''.join('+CMT: ,0\r\n' + frame + '\r\n' for frame in reversed(frames)) + 'OK\r\n'
# 用UTF-8字节转义保证Windows编译器执行字符集不影响断言。
def cpp_bytes(text):
    return '"' + ''.join(f'\\x{x:02x}' for x in text.encode('utf-8')) + '"'

tests = '''
int main(){
 initConcatBuffer();
 Serial1.onCommand=[](String){Serial1.feed(WIRE);};
 String response=sendATCommand("ATI",1000);
 assert(atResponseOK(response)&&smsPduCount==10&&processed==0);
 for(int i=0;i<10;++i)checkSerial1URC();
 assert(processed==1&&received==EXPECTED&&smsPduCount==0&&smsPduDropCount()==0);
 assert(!concatBuffer[0].inUse);
 std::cout<<"PASS: real pdulib, ten out-of-order PDU segments during AT, one complete delivery\\n";
}
'''.replace('WIRE', cpp_bytes(wire)).replace('EXPECTED', cpp_bytes(expected))
lib = ROOT / '.arduino/user/libraries/pdulib/src'
with tempfile.TemporaryDirectory(prefix='sms_receive_') as tmp:
    cpp = Path(tmp) / 'test.cpp'
    exe = Path(tmp) / 'test.exe'
    cpp.write_text(mock + base['transaction_class'] + base['web'] + base['sms'] + tests, encoding='utf-8')
    compiler = ROOT / '.tools/w64devkit/bin/g++.exe'
    env = dict(os.environ, PATH=str(compiler.parent) + os.pathsep + os.environ.get('PATH', ''))
    subprocess.run([str(compiler), '-std=c++11', '-DDESKTOP_PDU', '-include', 'cstdio', '-I', str(lib), str(cpp), str(lib / 'pdulib.cpp'), '-o', str(exe)], check=True, env=env)
    subprocess.run([str(exe)], check=True, env=env, timeout=20)
