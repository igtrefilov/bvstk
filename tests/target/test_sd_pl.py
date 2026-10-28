#!/usr/bin/env python3
"""SD-PL acceptance; --require-media writes only a private test file then removes it."""
import argparse
import hashlib
import json
import secrets
import socket
import struct
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--host',default='192.168.0.10')
p.add_argument('--output',type=Path,required=True)
p.add_argument('--require-media',action='store_true')
a=p.parse_args()
c=urllib.request.build_opener(urllib.request.ProxyHandler({}))
def request(endpoint,method='GET',data=None,**query):
    if query: endpoint+='?'+urllib.parse.urlencode(query)
    with c.open(urllib.request.Request('http://'+a.host+endpoint,data=data,method=method),timeout=60) as r:
        return r.read()
def read(path,**query): return json.loads(request(path,**query))
def health():
    r=read('/api/runtime')
    assert r['capabilities']['sd_pl'] is True
    assert read('/api/ui/bus',bus='i2c')['ready']
    assert read('/api/ui/bus',bus='mdio')['ready']
    with socket.create_connection((a.host,8889),timeout=5) as s:
        s.sendall(b'DCP2'+struct.pack('>HHBBH',2,4,0,0,1))
        data=b''
        while len(data)<14:
            part=s.recv(14-len(data));assert part;data+=part
        assert data[:4]==b'DCP2' and data[-2:]==b'\0\0'
    return r
report={'runtime':health(),'sd':read('/api/ui/sd-pl')}
v=next(v for v in read('/api/ui/volumes')['volumes'] if v['name']=='sd-pl')
assert v['supported'] is True and report['sd']['supported'] is True
assert report['sd']['format_allowed'] is False
if a.require_media:
    assert report['sd']['ready'] and report['sd']['mounted'] and v['ready']
    name='/.bvstk-stage9-'+secrets.token_hex(8)
    data=secrets.token_bytes(4096)
    try:
        request('/api/ui/file','PUT',data,volume='sd-pl',path=name,confirm='true')
        health()
        actual=request('/api/ui/file',volume='sd-pl',path=name)
        assert actual==data
        report['sha256']=hashlib.sha256(actual).hexdigest()
    finally:
        request('/api/ui/files','PUT',b'',volume='sd-pl',path=name,action='remove',confirm='true')
    report['media_io']='passed'
else:
    assert not v['ready'] and not report['sd']['ready'] and not report['sd']['mounted']
    try:
        request('/api/ui/files',volume='sd-pl',path='/')
        raise AssertionError('unmounted volume must not expose RAM mountpoint')
    except urllib.error.HTTPError as error:
        assert error.code==503, error.code
    report['no_media']='passed'
    report['media_io']='not tested: no external card connected'
report['final_runtime']=health()
a.output.parent.mkdir(parents=True,exist_ok=True)
a.output.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
print('SD-PL acceptance passed:', 'private file round-trip' if a.require_media else 'no media, services remain available')
