"""Read-only verification of the exact executable against product signatures."""
import argparse, hashlib, json, re, struct
from pathlib import Path

parser=argparse.ArgumentParser()
parser.add_argument('executable',type=Path)
parser.add_argument('output',type=Path)
args=parser.parse_args()
data=args.executable.read_bytes()
u16=lambda o: struct.unpack_from('<H',data,o)[0]
u32=lambda o: struct.unpack_from('<I',data,o)[0]
pe=u32(0x3c); opt=pe+24
assert data[:2]==b'MZ' and data[pe:pe+4]==b'PE\0\0' and u16(opt)==0x20b
table=opt+u16(pe+20)
sections=[]
for i in range(u16(pe+6)):
    p=table+i*40
    sections.append(dict(name=data[p:p+8].rstrip(b'\0').decode(),rva=u32(p+12),virtual_size=u32(p+8),raw_size=u32(p+16),raw_offset=u32(p+20),characteristics=u32(p+36)))
def at(rva,n):
    for s in sections:
        off=rva-s['rva']
        if 0<=off and off+n<=s['raw_size']:
            start=s['raw_offset']+off
            return data[start:start+n]
    raise ValueError(f'RVA outside raw data: {rva:x}')
header=(Path(__file__).resolve().parents[1]/'src/BuildTarget.hpp').read_text()
checks=[]
for guard in ('a','b','c'):
    block=header.split('namespace guard_'+guard+'\n{',1)[1].split('} // namespace guard_'+guard,1)[0]
    rva=int(re.search(r'kSignatureRva = (0x[0-9A-Fa-f]+)',block)[1],16)
    sig=re.search(r'kSignature\{(.*?)\};',block,re.S)[1]
    expected=bytes(int(v,16) for v in re.findall(r'0x([0-9A-Fa-f]{2})\b',sig))
    actual=at(rva,len(expected))
    checks.append(dict(guard=guard.upper(),rva=hex(rva),size=len(expected),matched=actual==expected,actual=actual.hex(' ')))
hook=0x1ffadaf
call=at(hook,5)
callee=hook+5+struct.unpack('<i',call[1:])[0]
result=dict(executable=str(args.executable),sha256=hashlib.sha256(data).hexdigest(),machine=hex(u16(pe+4)),timestamp=hex(u32(pe+8)),size_of_image=hex(u32(opt+56)),sections=sections,signatures=checks,guard_c_call_target=hex(callee),guard_c_wrapper=at(0x1ffada0,40).hex(' '),guard_c_callee=at(callee,64).hex(' '))
result['passed']=all(c['matched'] for c in checks) and u32(pe+8)==0x6ad55daf and u32(opt+56)==0x581b000 and callee==0x22ca420
args.output.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(result,ensure_ascii=False,indent=2))
raise SystemExit(0 if result['passed'] else 1)
