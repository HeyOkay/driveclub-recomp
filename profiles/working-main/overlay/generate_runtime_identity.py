from pathlib import Path
import hashlib,json,sys
runtime=Path(sys.argv[1]);header=Path(sys.argv[2]);digest=hashlib.sha256(runtime.read_bytes()).hexdigest()
header.parent.mkdir(parents=True,exist_ok=True)
header.write_text('#pragma once\ninline constexpr char kApprovedNativeRuntimeSha256[]="'+digest+'";\nstatic_assert(sizeof(kApprovedNativeRuntimeSha256)==65);\n')
header.with_suffix('.json').write_text(json.dumps({'runtime':str(runtime),'sha256':digest,'scope':'Finalized package runtime identity; no gameplay qualification implied'},indent=2)+'\n')
