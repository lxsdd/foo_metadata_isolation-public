"""Fail-closed source-only provenance of centrally pinned SDK 2026-10-01."""
from pathlib import Path
import subprocess, json, re
ROOT=Path(__file__).resolve().parents[1]
NOTICES={'sdk/sdk-license.txt','sdk/pfc/pfc-license.txt','sdk/libPPUI/libPPUI-license.txt','wtl/MS-PL.txt','THIRD_PARTY_NOTICES.md'}
ALLOWED_SDK={'sdk/README.md','sdk/SDK-PIN.json','sdk/sdk-license.txt','sdk/pfc/pfc-license.txt','sdk/libPPUI/libPPUI-license.txt'}
PIN={"sdk_version":"2026-10-01","source_repository":"lxsdd/foobar2000_component_template","source_commit":"31a35947a7527826c2199491692802e3ce3d8bd7","source_tree":"2ba8caeb73f72633ad7f60ee5f72ed8074a0eb17","upstream_archive_sha256":"d4c55077336fae81bf8df0259b5b2748fa45ea84132c656ead93eb123cbcdc26","upstream_url":"https://www.foobar2000.org/downloads/SDK-2026-10-01.7z","archive_size_bytes":793738}
FORBIDDEN=re.compile(r'\.(?:mp3|flac|wv|ogg|opus|wav|aiff?|cue|mp4|m4a|db|sqlite|sqlite3|lib|dll|exe|fb2k-component|zip|7z|rar|pfx|p12|pem|key|kdbx|pdb|obj|ilk|idb|cache)$',re.I)
SECRETS={'.env','id_rsa','id_ed25519','credentials.json','secrets.json'}
def issues(paths,pin,notices):
    errors=[]
    for path in paths:
        if '\\\\' in path or path.startswith('/') or '..' in path.split('/'): errors.append('unsafe tracked path')
        if FORBIDDEN.search(path) or Path(path).name.lower() in SECRETS: errors.append('blocked tracked binary, media or credential: '+path)
        if path.startswith('sdk/') and path not in ALLOWED_SDK: errors.append('old/unreviewed SDK tracked file: '+path)
    errors += ['missing notice: '+v for v in NOTICES-paths]
    errors += ['missing SDK source pin/notice: '+v for v in ALLOWED_SDK-paths]
    for key,val in PIN.items():
        if pin.get(key)!=val: errors.append('incorrect SDK source pin: '+key)
    for path in NOTICES-{'THIRD_PARTY_NOTICES.md'}:
        if path not in notices: errors.append('unreferenced license notice: '+path)
    if 'sdk/SDK-PIN.json' not in notices:errors.append('missing central SDK source pin reference')
    return errors
def main():
    paths={z.decode('utf-8') for z in subprocess.check_output(['git','ls-files','-z'],cwd=ROOT).split(b'\0') if z}
    pin=json.loads((ROOT/'sdk/SDK-PIN.json').read_text(encoding='utf-8'))
    notices=(ROOT/'THIRD_PARTY_NOTICES.md').read_text(encoding='utf-8')
    found=issues(paths,pin,notices)
    if found:raise SystemExit('SOURCE_PROVENANCE_FAIL:\n- '+'\n- '.join(found))
    assert issues(paths|{'test-media/private.mp3'},pin,notices)
    assert issues(paths|{'sdk/foobar2000/shared/shared-x64.lib'},pin,notices)
    assert issues(paths|{'sdk/foobar2000/SDK/old.h'},pin,notices)
    assert issues(paths-{'sdk/sdk-license.txt'},pin,notices)
    assert issues(paths,{**pin,'source_tree':'0'*40},notices)
    print('SOURCE_PROVENANCE_SDK2026_CENTRAL_GIT_PIN=PASS')
    print('SOURCE_PROVENANCE_NO_TRACKED_SDK_BINARIES=PASS')
    print('SOURCE_PROVENANCE_MUTATION_CASES=PASS')
    print('OWN_SOURCE_LICENSE=PENDING_OWNER_DECISION')
    print('HISTORICAL_SECRET_AUDIT=SEPARATE_SOURCE_ONLY_WORKFLOW')
if __name__=='__main__':main()
