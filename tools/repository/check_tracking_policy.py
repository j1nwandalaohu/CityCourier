"""Check staged paths and LFS pointers; never changes project files."""
import subprocess
import sys
from pathlib import Path


def git(*args, data=None):
    result = subprocess.run(
        ['git', '-c', f'safe.directory={ROOT.as_posix()}', *args],
        cwd=ROOT, input=data, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
    )
    if result.returncode not in (0, 1) or (result.returncode == 1 and args[0] != 'check-ignore'):
        raise RuntimeError(result.stderr.decode('utf-8', errors='replace'))
    return result.stdout


ROOT = Path(__file__).resolve().parents[2]


def main():
    paths = git('ls-files', '-z')
    ignored = git('check-ignore', '--no-index', '-z', '--stdin', data=paths)
    local_only = {p for p in ignored.decode('utf-8').split('\0') if p}
    errors = ['Tracked local-only file: '+p for p in sorted(local_only)]
    resources = [p for p in paths.decode('utf-8').split('\0') if p.endswith(('.uasset', '.umap')) and p not in local_only]
    for path in resources:
        blob = git('show', ':'+path)
        if not blob.startswith(b'version https://git-lfs.github.com/spec/v1\n'):
            errors.append('Gameplay binary is not an LFS pointer in the index: '+path)
    for error in errors:
        print(error, file=sys.stderr)
    if errors:
        print(f'FAILED: {len(errors)} tracking-policy violations', file=sys.stderr)
        return 1
    print(f'PASS: {len(resources)} gameplay resources use LFS; no tracked local-only files')
    return 0


if __name__ == '__main__':
    sys.exit(main())
