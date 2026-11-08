"""Build the authored rear-view shim; no game files or private binaries are inputs."""
from pathlib import Path
import argparse, hashlib, struct, subprocess, tempfile
parser=argparse.ArgumentParser()
parser.add_argument('--clang',default='clang')
parser.add_argument('--output',type=Path,required=True)
parser.add_argument('--binary-output',type=Path)
args=parser.parse_args()
with tempfile.TemporaryDirectory(dir=args.output.parent) as temp:
 objpath=Path(temp)/'rear.o'
 subprocess.run([args.clang,'--target=x86_64-unknown-linux-gnu','-c',str(Path(__file__).with_name('rear-native-stub.s')),'-o',str(objpath)],check=True)
 obj=bytearray(objpath.read_bytes())
 assert obj[:6]==b'\x7fELF\x02\x01'
 sho=struct.unpack_from('<Q',obj,40)[0];shs,shn,shi=struct.unpack_from('<HHH',obj,58)
 sections=[struct.unpack_from('<IIQQQQIIQQ',obj,sho+i*shs) for i in range(shn)]
 shstr=sections[shi];names=obj[shstr[4]:shstr[4]+shstr[5]]
 def cstr(s,i):return bytes(s[i:]).split(b'\0',1)[0].decode()
 byname={cstr(names,s[0]):s for s in sections}
 text=byname['.text'];syms=byname['.symtab'];strings=sections[syms[6]];st=obj[strings[4]:strings[4]+strings[5]]
 symbols=[cstr(st,struct.unpack_from('<I',obj,p)[0]) for p in range(syms[4],syms[4]+syms[5],syms[9])]
 base=0x1551b00;values={'return_5b95c3':0x5b95c3,'return_5c720f':0x5c720f,'return_fbd4d9':0xfbd4d9,'image_factory_slot':0x2749f50,'create_description':0x739f30,'construct_view':0x5ede70,'resume_view':0x5b92db}
 rs=byname['.rela.text'];receipts=[]
 for p in range(rs[4],rs[4]+rs[5],rs[9]):
  off,info,add=struct.unpack_from('<QQq',obj,p);kind=info&0xffffffff;name=symbols[info>>32]
  assert kind in (2,4) and add==-4
  delta=values[name]+add-(base+off);assert -2**31<=delta<2**31
  struct.pack_into('<i',obj,text[4]+off,delta)
  receipts.append({'offset':hex(off),'symbol':name,'target':hex(values[name]),'relative':delta})
 code=bytes(obj[text[4]:text[4]+text[5]])

 if len(code)!=714 or hashlib.sha256(code).hexdigest()!='a17cfedf77feefe54f3613fb57a12213f27c7bc589e9444f8539bc1589b06c8c':
  raise RuntimeError('Authored rear payload differs from the reviewed instruction sequence')
 header='#pragma once\n#include <cstddef>\nstruct RearNativePayload { const unsigned char* data; size_t size; const char* sha256; bool approved; };\nconstexpr unsigned char kRearNativeCode[] = {\n'
 header+=''.join('  '+','.join('0x%02x'%v for v in code[i:i+16])+',\n' for i in range(0,len(code),16))
 header+='};\nconstexpr RearNativePayload kRearNativePayload{kRearNativeCode,sizeof(kRearNativeCode),"'+hashlib.sha256(code).hexdigest()+'",true};\n'
 args.output.write_text(header)

 if args.binary_output: args.binary_output.write_bytes(code)
