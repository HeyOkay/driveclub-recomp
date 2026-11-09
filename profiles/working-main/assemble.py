"""Assemble the pinned working-main source profile from this checkout and small overlays."""
from pathlib import Path
import argparse,hashlib,json
parser=argparse.ArgumentParser();parser.add_argument('--checkout',type=Path,required=True);parser.add_argument('--output',type=Path,required=True)
args=parser.parse_args();profile=Path(__file__).resolve().parent;checkout=args.checkout.resolve();output=args.output.resolve()
if output.exists():raise SystemExit('Output must be a new directory; existing builds are preserved')
if output==checkout or output in checkout.parents:raise SystemExit('Output cannot replace the checkout')
manifest=json.loads((profile/'manifest.json').read_text());prepared=[]
def canonical(b):
 if b'\0'not in b:
  try:b.decode('utf8');return b.replace(b'\r\n',b'\n')
  except UnicodeDecodeError:pass
 return b
for row in manifest['files']:
 base=checkout if 'checkout_source'in row else profile
 relative=row.get('checkout_source',row.get('overlay_source'));source=(base/relative).resolve()
 if not source.is_relative_to(base):raise SystemExit('Source escapes its root: '+relative)
 data=canonical(source.read_bytes())
 if hashlib.sha256(data).hexdigest()!=row['sha256']or len(data)!=row['size']:raise SystemExit('Pinned source changed: '+str(source))
 dest=(output/row['destination']).resolve()
 if not dest.is_relative_to(output):raise SystemExit('Destination escapes output')
 prepared.append((dest,data))
output.mkdir(parents=True)
for dest,data in prepared:dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(data)
(output/'profile-source-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print(json.dumps({'assembled_files':len(prepared),'output':str(output),'historical_build_inputs':False}))
