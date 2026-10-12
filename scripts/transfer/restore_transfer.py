"""Restore T-257 archives on another PC, without overwriting an existing project."""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import subprocess
import zipfile


def sha256(path):
    with path.open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()


def extract(path, target):
    with zipfile.ZipFile(path) as z:
        for info in z.infolist():
            name = PurePosixPath(info.filename)
            if name.is_absolute() or '..' in name.parts or '\\' in info.filename or ':' in info.filename:
                raise ValueError(f'Chemin archive invalide: {info.filename}')
        z.extractall(target)


def run(*args, cwd=None):
    subprocess.run(args, cwd=cwd, check=True, env=dict(os.environ, GIT_LFS_SKIP_SMUDGE='1'))


def main():
    parser = argparse.ArgumentParser(description='Restaurer le kit de developpement ou verifier son telechargement.')
    parser.add_argument('--destination', type=Path)
    parser.add_argument('--verify-only', action='store_true')
    parser.add_argument('--engine', type=Path, default=Path('C:/Program Files/Epic Games/UE_5.8'))
    parser.add_argument('--without-git', action='store_true', help='Restaurer les fichiers sans historique Git.')
    args = parser.parse_args()
    kit = Path(__file__).resolve().parent
    ready = json.loads((kit / 'READY.json').read_text(encoding='utf-8'))
    for record in ready['archives']:
        path = kit / record['name']
        print('Verification SHA256: ' + path.name, flush=True)
        if path.stat().st_size != record['bytes'] or sha256(path) != record['sha256']:
            raise RuntimeError(f'Archive incomplete/corrompue: {path}')
    if args.verify_only:
        print('Toutes les archives sont integres.')
        return
    if not args.destination:
        parser.error('--destination est obligatoire pour restaurer.')
    destination = args.destination.resolve()
    if destination.exists() and any(destination.iterdir()):
        raise RuntimeError('Choisir un nouveau dossier vide: aucun fichier existant ne sera ecrase.')
    if not args.without_git and not shutil.which('git'):
        raise RuntimeError('Installer Git pour Windows, ou utiliser --without-git.')
    if not args.without_git:
        subprocess.run(['git', 'lfs', 'version'], check=True)
    destination.mkdir(parents=True, exist_ok=True)
    project = destination / 'monk-island'
    if not args.without_git:
        history = destination / 'Historique-Git'
        extract(kit / '04-Historique-Git-LFS.zip', history)
        run('git', 'clone', '--no-checkout', '--branch', 'main', str(history / 'historique.bundle'), str(project))
        # Preserve all local branches from the bundle before removing the temporary remote.
        run('git', 'fetch', str(history / 'historique.bundle'), 'refs/heads/*:refs/heads/*', '--update-head-ok', cwd=project)
        run('git', 'remote', 'remove', 'origin', cwd=project)
        run('git', 'lfs', 'install', '--local', cwd=project)
        shutil.copytree(history / 'lfs-objects', project / '.git/lfs/objects', dirs_exist_ok=True)
        run('git', 'read-tree', 'HEAD', cwd=project)
    for name in ['01-Jeu-Windows.zip', '02-Projet-Unreal.zip', '03-Sources-Art-et-Cartographie.zip', '05-Travaux-en-cours.zip']:
        print('Extraction: ' + name, flush=True)
        extract(kit / name, destination)
    shutil.copytree(kit / 'preuves', destination / 'preuves', dirs_exist_ok=True)
    shutil.copy2(kit / 'LIRE-MOI.txt', destination / 'LIRE-MOI.txt')
    shutil.copy2(kit / 'requirements-outils.txt', destination / 'requirements-outils.txt')
    if (kit / 'Prerequis-Windows').is_dir():
        shutil.copytree(kit / 'Prerequis-Windows', destination / 'Prerequis-Windows', dirs_exist_ok=True)
    # The source PC uses an installation GUID: it is intentionally not rewritten in the live repo.
    # For the restored copy register a matching local engine and associate it with its absolute path.
    descriptor = project / 'monke_island/monke_island.uproject'
    engine_build = args.engine / 'Engine/Build/Build.version'
    if engine_build.exists():
        installed = json.loads(engine_build.read_text(encoding='utf-8-sig'))
        actual = tuple(installed[k] for k in ['MajorVersion', 'MinorVersion', 'PatchVersion'])
        if actual != (5, 8, 3):
            print(f'ATTENTION: moteur {actual}, snapshot cree avec 5.8.3. Association conservee.')
        elif os.name == 'nt':
            import winreg
            import uuid
            association = '{' + str(uuid.uuid4()).upper() + '}'
            with winreg.CreateKey(winreg.HKEY_CURRENT_USER, r'Software\Epic Games\Unreal Engine\Builds') as key:
                winreg.SetValueEx(key, association, 0, winreg.REG_SZ, str(args.engine.resolve()))
            data = json.loads(descriptor.read_text(encoding='utf-8-sig'))
            data['EngineAssociation'] = association
            descriptor.write_text(json.dumps(data, indent='\t') + '\n', encoding='utf-8')
            print('Association du projet au moteur 5.8.3 local effectuee.')
    else:
        print('Installer UE 5.8.3 puis associer le .uproject au moteur local (voir LIRE-MOI).')
    print(f'Projet restaure: {descriptor}')
    print('Travaux non fusionnes: travaux-en-cours/. Lire les consignes T-234 avant toute fusion.')
    print('Aucun build ni lancement du jeu execute par cette restauration.')


if __name__ == '__main__':
    main()
