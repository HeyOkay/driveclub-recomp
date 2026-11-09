from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess
p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);p.add_argument('--package',type=Path,required=True);p.add_argument('--toolchain',type=Path,required=True);p.add_argument('--dlss',type=Path);a=p.parse_args()
root=a.source.resolve();package=a.package.resolve();runtime=package/'runtime';tools=a.toolchain.resolve()
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def copy(src,dest):
 dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(src,dest);assert sha(src)==sha(dest)
pending=[runtime/'DriveClubGame.exe'];seen=set();dlls={p.name.lower():p for p in (tools/'bin').glob('*.dll')}
while pending:
 binary=pending.pop();key=binary.name.lower()
 if key in seen:continue
 seen.add(key)
 text=subprocess.check_output([str(tools/'bin/llvm-objdump.exe'),'-p',str(binary)],text=True)
 for name in re.findall(r'DLL Name:\s*(\S+)',text):
  low=name.lower()
  if low in seen:continue
  if low in dlls:
   dest=runtime/name
   if not dest.exists():copy(dlls[low],dest)
   pending.append(dest)
  elif not low.startswith(('api-ms-','ext-ms-'))and not(Path(os.environ['WINDIR'])/'System32'/name).is_file():raise RuntimeError('Unresolved runtime DLL: '+name)
for rel,dest in [('patches/DriveClub.xml','runtime/patches/DriveClub.xml'),('packaging/release/trophies.tsv','runtime/trophies.tsv'),('LICENSE','LICENSE'),('packaging/release/THIRD_PARTY_NOTICES.txt','THIRD_PARTY_NOTICES.txt')]:copy(root/'host'/rel,package/dest)
# Optional DLSS uses the matching source-built bridge, never a modern ABI substitution.
dlss=a.dlss if a.dlss else root/'host/packaging/dlss_bin'
for name in ('bbport_dlss.dll','nvngx_dlss.dll','nvngx_dlssg.dll'):copy(dlss/name,runtime/name)
identity=json.loads((root/'build/generated/launcher/main_runtime_identity.generated.json').read_text())
digest=sha(runtime/'DriveClubGame.exe');assert digest==identity['sha256']
receipt={'profile':'working-main','runtime_sha256':digest,'backend_sha256':sha(package/'DriveClub.Backend.exe'),'native_sha256':sha(package/'DriveClub.Native.exe'),'runtime_finalized_before_backend':True,'post_identity_runtime_modification':False,'files':{p.relative_to(package).as_posix():sha(p)for p in package.rglob('*')if p.is_file()}}
(package/'build-info.json').write_text(json.dumps(receipt,indent=2)+'\n')
(runtime/'files.txt').write_text('\n'.join(sorted(p.relative_to(runtime).as_posix()for p in runtime.rglob('*')if p.is_file()and p.name!='files.txt'))+'\n')
print(json.dumps({'runtime_sha256':digest,'runtime_dlls':len(list(runtime.glob('*.dll'))),'package':str(package)}))
