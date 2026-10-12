"""Stage the immutable T193 OneDrive delivery, verifying each copy."""
import datetime as dt
import hashlib
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
SOURCE=Path('C:/Users/matth/OneDrive/maquette-blender')
PACKAGE=SOURCE/'maquettes/fort-francais'
STAGE=ROOT/'art/export/Fort_T193'

def main():
    files={}
    sources=list((PACKAGE/'exports').glob('*.fbx'))+list(PACKAGE.glob('*.json'))
    sources+=list((PACKAGE/'modules').glob('*.json'))+[PACKAGE/'BRIEF.md']
    sources+=list((SOURCE/'factory/kits').glob('fort_*.py'))
    for source in sources:
        relative=source.relative_to(PACKAGE) if source.is_relative_to(PACKAGE) else Path('SourceReferences')/source.name
        target=STAGE/relative
        before=source.stat(); data=source.read_bytes(); after=source.stat()
        assert (before.st_size,before.st_mtime_ns)==(after.st_size,after.st_mtime_ns),f'Sync in progress: {source}'
        assert data,source
        target.parent.mkdir(parents=True,exist_ok=True)
        target.write_bytes(data)
        files[relative.as_posix()]={'source':str(source),'bytes':len(data),'mtime_ns':after.st_mtime_ns,
                                   'sha256':hashlib.sha256(data).hexdigest()}
    assert len(list((STAGE/'exports').glob('*.fbx')))==68
    result={'task':'T-193','version':'0.2.0','staged_at':dt.datetime.now(dt.timezone.utc).isoformat(),'files':files}
    (STAGE/'source-manifest.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    print('T193_STAGE',len(files),'files',sum(x['bytes'] for x in files.values()),'bytes',flush=True)

if __name__=='__main__':main()
