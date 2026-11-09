from pathlib import Path
import hashlib,json,shutil,subprocess,sys
source,dest=map(Path,sys.argv[1:3]);dest.parent.mkdir(parents=True,exist_ok=True)
shutil.copyfile(source,dest)
if sys.argv[3].upper() in ('ON','TRUE','1'):subprocess.run([sys.argv[4],'--strip-all',str(dest)],check=True)
dest.with_suffix('.identity.json').write_text(json.dumps({'sha256':hashlib.sha256(dest.read_bytes()).hexdigest(),'finalization':'copied and optionally stripped; package must copy bytes unchanged'},indent=2)+'\n')
