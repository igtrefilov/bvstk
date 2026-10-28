#!/usr/bin/env python3
"""Compare a local flash:/ file backup with a running BVSTK HTTP server."""
import argparse
import hashlib
import json
from pathlib import Path
import urllib.parse
import urllib.request

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--host',default='192.168.0.10')
p.add_argument('--backup',type=Path,required=True)
p.add_argument('--manifest',type=Path,required=True)
a=p.parse_args()
root=a.backup.resolve()
if not root.is_dir() or any(x.is_symlink() for x in root.rglob('*')):raise SystemExit('Invalid backup directory or symbolic link')
local={x.relative_to(root).as_posix().casefold():hashlib.sha256(x.read_bytes()).hexdigest() for x in root.rglob('*') if x.is_file()}
if len(local) != sum(1 for x in root.rglob('*') if x.is_file()): raise ValueError('Ambiguous filenames in backup')
http=urllib.request.build_opener(urllib.request.ProxyHandler({}))
def get(route,**params):
 with http.open('http://'+a.host+route+'?'+urllib.parse.urlencode(params),timeout=120) as f:return f.read()
remote={}
def walk(path):
 offset=0
 while True:
  data=json.loads(get('/api/ui/files',volume='flash',path=path,limit=128,offset=offset))
  if not data['ok']:raise ValueError('Directory read failed')
  for entry in data['entries']:
   child=path.rstrip('/')+'/'+entry['name']
   if entry['directory']:walk(child)
   else:
    key=child.lstrip('/').casefold()
    if key in remote:raise ValueError('Ambiguous filenames on device')
    remote[key]=hashlib.sha256(get('/api/ui/file',volume='flash',path=child)).hexdigest()
  if data['next'] is None:break
  offset=data['next']
walk('/')
if local!=remote:
 raise ValueError('Backup differs: missing=%r extra=%r changed=%r' %(sorted(remote.keys()-local.keys()),sorted(local.keys()-remote.keys()),sorted(k for k in local.keys()&remote.keys() if local[k]!=remote[k])))
a.manifest.write_text(json.dumps(remote,ensure_ascii=False,indent=2)+'\n')
print('Verified files:',len(remote))
