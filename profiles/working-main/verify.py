from pathlib import Path
import argparse,hashlib,json,re
p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);a=p.parse_args();root=a.source.resolve();build=root/'build'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
runtime=build/'finalized-runtime/DriveClubGame.exe';backend=build/'DriveClub.Backend.exe';native=build/'DriveClub.Native.exe'
header=(build/'generated/launcher/main_runtime_identity.generated.h').read_text();digest=sha(runtime)
assert digest in header and sha(backend)==sha(native)
manifest=json.loads((root/'profile-source-manifest.json').read_text());mismatches=[]
for row in manifest['files']:
 path=root/row['destination']
 if sha(path)!=row['sha256']:mismatches.append(row['destination'])
assert not mismatches,mismatches
db=json.loads((build/'compile_commands.json').read_text());sources=[Path(r['file']).resolve()for r in db]
assert all(p.is_relative_to(root)for p in sources)
receipt={'result':'SOURCE_BUILD_AND_FINALIZED_BACKEND_IDENTITY_PASS','runtime_sha256':digest,'backend_sha256':sha(backend),'compiled_translation_units':len(sources),'all_sources_inside_assembled_profile':True,'source_hashes_unchanged':True,'gpu_qualification':False,'historical_c15_byte_identity_claimed':False}
(build/'profile-verification.json').write_text(json.dumps(receipt,indent=2)+'\n');print(json.dumps(receipt,indent=2))
