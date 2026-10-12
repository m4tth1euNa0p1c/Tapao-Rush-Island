"""T-257: read-only source snapshots, ZIP64 archives and verified transfer manifest."""
import argparse
import hashlib
import importlib.metadata
import json
import os
from pathlib import Path
import shutil
import subprocess
import time
import zipfile
from datetime import datetime, timezone

ROOT = Path(__file__).resolve().parents[2]
SKIP_DIRS = {'.git', '.worktrees', '__pycache__', '.vs', 'Intermediate', 'DerivedDataCache', 'Saved', 'Binaries', 'node_modules', '.venv'}
SKIP_FILES = {'settings.local.json', 'editor.lock.json', 'editor.lock.tmp', 'editor.lock.log'}
MAIN_PARTS = ['monke_island/Source', 'monke_island/Content', 'monke_island/Config', 'monke_island/Build', 'monke_island/Plugins', 'monke_island/.vsconfig', 'monke_island/monke_island.uproject', 'scripts', 'docs', '.claude', '.codex', '.mcp.json', '.gitignore', '.gitattributes', 'AGENTS.md', 'CLAUDE.md']
ENGINE = Path('C:/Program Files/Epic Games/UE_5.8')


def git(*args, cwd=ROOT):
    env = dict(os.environ, GIT_OPTIONAL_LOCKS='0', GIT_LFS_SKIP_SMUDGE='1')
    return subprocess.check_output(['git', *args], cwd=cwd, env=env)


def digest(path):
    with path.open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()


def write_json(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding='utf-8')


def files(path, *, generated=False):
    if not path.exists():
        return
    if path.is_file():
        yield path
        return
    for folder, dirs, names in os.walk(path):
        dirs[:] = sorted(d for d in dirs if (generated or d not in SKIP_DIRS) and not (Path(folder) / d).is_symlink())
        for name in sorted(names):
            p = Path(folder) / name
            if name in SKIP_FILES or name.startswith('.env') or name.endswith(('.pyc', '.blend1', '.lnk')) or p.is_symlink():
                continue
            yield p


def selections(base, parts, prefix, *, generated=False):
    result = []
    for part in parts:
        for p in files(base / part, generated=generated):
            relative = p.relative_to(base).as_posix()
            if relative.startswith('docs/reference/') and 'docs/reference' not in parts:
                continue
            result.append((p, prefix + relative))
    return sorted(set(result), key=lambda pair: pair[1])


def archive(out, name, entries, *, asset_check=False):
    target = out / name
    pending = target.with_suffix('.zip.partial')
    before = {str(p): (p.stat().st_size, p.stat().st_mtime_ns) for p, _ in entries}
    records = []
    last = time.monotonic()
    with zipfile.ZipFile(pending, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=1, allowZip64=True) as z:
        for number, (source, arcname) in enumerate(entries, 1):
            stamp = source.stat()
            h = hashlib.sha256()
            with source.open('rb') as src:
                first = src.read(1024 * 1024)
                if asset_check and source.suffix in {'.uasset', '.umap'} and first.startswith(b'version https://git-lfs.github.com/spec/v1'):
                    raise RuntimeError(f'Asset non hydrate (pointeur LFS): {source}')
                with z.open(arcname, 'w', force_zip64=True) as dst:
                    chunk = first
                    while chunk:
                        h.update(chunk)
                        dst.write(chunk)
                        chunk = src.read(1024 * 1024)
            after = source.stat()
            if (stamp.st_size, stamp.st_mtime_ns) != (after.st_size, after.st_mtime_ns):
                raise RuntimeError(f'Fichier modifie pendant copie: {source}. Relancer la preparation.')
            records.append({'path': arcname, 'bytes': stamp.st_size, 'sha256': h.hexdigest()})
            if time.monotonic() - last > 15:
                print(f'{name}: {number}/{len(entries)} fichiers', flush=True)
                last = time.monotonic()
    changed = [p for p, signature in before.items() if not Path(p).exists() or (Path(p).stat().st_size, Path(p).stat().st_mtime_ns) != signature]
    if changed:
        raise RuntimeError(f'Sources modifiees pendant archive: {changed[:10]}. Relancer.')
    print(f'{name}: controle integral des CRC...', flush=True)
    with zipfile.ZipFile(pending) as z:
        bad = z.testzip()
        if bad:
            raise RuntimeError(f'CRC incorrect: {bad}')
    pending.replace(target)
    write_json(out / (name + '.files.json'), records)
    result = {'name': name, 'bytes': target.stat().st_size, 'sha256': digest(target), 'files': len(records), 'unpacked_bytes': sum(r['bytes'] for r in records), 'crc_verified': True}
    print(json.dumps(result), flush=True)
    return result


def prepare(out):
    out = out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    if (out / 'READY.json').exists():
        raise RuntimeError('Kit deja finalise: utiliser un nouveau dossier.')
    evidence = out / 'preuves'
    evidence.mkdir(exist_ok=True)
    package = ROOT / 'build/package/Windows'
    required = ['monke_island.exe', 'monke_island/Binaries/Win64/monke_island.exe', 'monke_island/Content/Paks/monke_island-Windows.ucas', 'monke_island/Content/Paks/monke_island-Windows.utoc']
    for name in required:
        if not (package / name).is_file():
            raise RuntimeError(f'Build incomplet: {name}')
    uat = Path(os.environ['APPDATA']) / 'Unreal Engine/AutomationTool/Logs/C+Program+Files+Epic+Games+UE_5.8/Log.txt'
    log = uat.read_text(encoding='utf-8', errors='replace')
    if 'BUILD SUCCESSFUL' not in log[-10000:] or 'ExitCode=0 (Success)' not in log[-10000:]:
        raise RuntimeError('Le dernier packaging ne prouve pas son succes.')
    (evidence / 'packaging-UAT.log').write_text(log, encoding='utf-8')
    for name in ['build-game.log', 'build-proof.json', 'source-baseline.json']:
        p = ROOT / 'monke_island/Saved/T234' / name
        if p.exists():
            shutil.copy2(p, evidence / ('T234-' + name))
    shutil.copy2(ENGINE / 'Engine/Build/Build.version', evidence / 'Unreal-Build.version')
    version = json.loads((evidence / 'Unreal-Build.version').read_text())
    started = datetime.now(timezone.utc).isoformat()
    head = git('rev-parse', 'HEAD').decode().strip()
    (evidence / 'worktrees.txt').write_bytes(git('worktree', 'list', '--porcelain'))
    (evidence / 'main-index.txt').write_bytes(git('ls-files', '--stage'))
    (evidence / 'main-staged.patch').write_bytes(git('diff', '--cached', '--binary', '--no-ext-diff'))
    bundle = evidence / 'historique.bundle'
    git('bundle', 'create', str(bundle), '--all')
    git('bundle', 'verify', str(bundle))
    dependencies = {}
    for name in ['numpy', 'scipy', 'soundfile', 'Pillow', 'requests']:
        dependencies[name] = importlib.metadata.version(name)
    (out / 'requirements-outils.txt').write_text(''.join(f'{k}=={v}\n' for k, v in dependencies.items()), encoding='utf-8')
    write_json(evidence / 'source-state.json', {'started_utc': started, 'git_head': head, 'engine': version, 'build_exe_mtime': datetime.fromtimestamp((package / required[1]).stat().st_mtime, timezone.utc).isoformat(), 'snapshot_includes_uncommitted': True, 'editor_tested_by_this_task': False, 'tested_on_second_pc': False})
    for name in ['restore_transfer.py', 'LIRE-MOI.txt']:
        shutil.copy2(Path(__file__).parent / name, out / name)
    prereqs = out / 'Prerequis-Windows'
    prereqs.mkdir(exist_ok=True)
    for name in ['vc_redist.x64.exe', 'GameInputRedist.msi']:
        shutil.copy2(ENGINE / 'Engine/Extras/Redist/en-us' / name, prereqs / name)

    archives = []
    build_entries = [(p, 'Jeu-Windows/' + p.relative_to(package).as_posix()) for p in files(package, generated=True)]
    prereq = ENGINE / 'Engine/Extras/Redist/en-us/UEPrereqSetup_x64.exe'
    if prereq.exists() and not any(n.endswith('UEPrereqSetup_x64.exe') for _, n in build_entries):
        build_entries.append((prereq, 'Jeu-Windows/Prerequis/UEPrereqSetup_x64.exe'))
    archives.append(archive(out, '01-Jeu-Windows.zip', build_entries))
    project_entries = selections(ROOT, MAIN_PARTS, 'monk-island/')
    archives.append(archive(out, '02-Projet-Unreal.zip', project_entries, asset_check=True))
    # Snapshot consistency across the archive and the enumeration (new/deleted files too).
    if [n for _, n in selections(ROOT, MAIN_PARTS, 'monk-island/')] != [n for _, n in project_entries]:
        raise RuntimeError('Inventaire projet modifie pendant copie. Relancer.')
    art = selections(ROOT, ['art', 'sprite', 'docs/reference', 'build/packs'], 'monk-island/')
    archives.append(archive(out, '03-Sources-Art-et-Cartographie.zip', art))
    history = [(bundle, 'historique.bundle')]
    history += [(p, 'lfs-objects/' + p.relative_to(ROOT / '.git/lfs/objects').as_posix()) for p in files(ROOT / '.git/lfs/objects')]
    archives.append(archive(out, '04-Historique-Git-LFS.zip', history))
    worktree_entries = []
    for tree in sorted((ROOT / '.worktrees').iterdir()):
        if not tree.is_dir():
            continue
        metadata = evidence / 'worktrees' / tree.name
        metadata.mkdir(parents=True, exist_ok=True)
        write_json(metadata / 'git-state.json', {'head': git('rev-parse', 'HEAD', cwd=tree).decode().strip(), 'branch': git('branch', '--show-current', cwd=tree).decode().strip(), 'deleted_tracked_files': git('ls-files', '--deleted', cwd=tree).decode().splitlines()})
        (metadata / 'index.txt').write_bytes(git('ls-files', '--stage', cwd=tree))
        (metadata / 'staged.patch').write_bytes(git('diff', '--cached', '--binary', '--no-ext-diff', cwd=tree))
        worktree_entries += selections(tree, MAIN_PARTS + ['art', 'sprite', 'docs/reference'], f'travaux-en-cours/{tree.name}/')
    archives.append(archive(out, '05-Travaux-en-cours.zip', worktree_entries))
    # The bundle is kept only inside the verified history archive, avoiding a duplicate upload.
    bundle.unlink()
    ready = {'task': 'T-257', 'created_utc': datetime.now(timezone.utc).isoformat(), 'git_head': head, 'engine_version': '5.8.3', 'archives': archives, 'total_bytes': sum(a['bytes'] for a in archives), 'build_packaging_success': True, 'new_gameplay_test': False, 'second_pc_test': False, 'cloud_sync_verified': False}
    write_json(out / 'READY.json', ready)
    print('KIT PRET: ' + str(out), flush=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    prepare(args.output)


if __name__ == '__main__':
    main()
