"""Load Query/Supports only; never call Main or install game hooks."""
import ctypes as c,hashlib,json,struct,sys
from pathlib import Path

class Pre(c.Structure):
    _fields_=[('type',c.c_uint32),('number',c.c_uint32)]
class Sem(c.Structure):
    _fields_=[('major',c.c_uint8),('minor',c.c_uint16),('patch',c.c_uint32),('prerelease',Pre)]
class FileVer(c.Structure):
    _fields_=[('major',c.c_uint16),('minor',c.c_uint16),('build',c.c_uint16),('revision',c.c_uint16)]
class Info(c.Structure):
    _fields_=[('sdk',Sem),('name',c.c_wchar_p),('author',c.c_wchar_p),('version',Sem),('runtime',FileVer)]
path=Path(sys.argv[1]).resolve(); data=path.read_bytes()
pe=struct.unpack_from('<I',data,0x3c)[0]
dll=c.WinDLL(str(path)); info=Info()
dll.Query.argtypes=[c.POINTER(Info)];dll.Query.restype=None
dll.Supports.argtypes=[];dll.Supports.restype=c.c_uint32
dll.Query(c.byref(info))
sem=lambda s:[s.major,s.minor,s.patch,s.prerelease.type,s.prerelease.number]
result=dict(dll=str(path),sha256=hashlib.sha256(data).hexdigest(),machine=hex(struct.unpack_from('<H',data,pe+4)[0]),exports_present=[name for name in ('Main','Query','Supports') if getattr(dll,name,None)],name=info.name,author=info.author,version=sem(info.version),sdk=sem(info.sdk),runtime=[info.runtime.major,info.runtime.minor,info.runtime.build,info.runtime.revision],api=dll.Supports())
result['passed']=result['version']==[1,2,0,2,1] and result['sdk'][:3]==[1,0,0] and result['runtime']==[3,0,88,46235] and result['machine']=='0x8664' and result['api']==1 and len(result['exports_present'])==3 and 'Gustavo Kimura' in result['author']
Path(sys.argv[2]).write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(result,ensure_ascii=False,indent=2))
raise SystemExit(0 if result['passed'] else 1)
