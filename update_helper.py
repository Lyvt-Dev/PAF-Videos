import ctypes
import hashlib
import json
import subprocess
import sys
from pathlib import Path


def verified(path, info):
    p = Path(path)
    if not p.is_file() or p.stat().st_size != info['size']:
        return False
    with p.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest() == info['sha256']


def helper(args):
    pid, setup, directory, digest, size, restart = args
    setup = Path(setup)
    result_file = setup.with_suffix('.result.json')
    result = {'ok': False}
    try:
        kernel = ctypes.WinDLL('kernel32', use_last_error=True)
        kernel.OpenProcess.argtypes = [ctypes.c_uint32, ctypes.c_int, ctypes.c_uint32]
        kernel.OpenProcess.restype = ctypes.c_void_p
        kernel.WaitForSingleObject.argtypes = [ctypes.c_void_p, ctypes.c_uint32]
        kernel.CloseHandle.argtypes = [ctypes.c_void_p]
        handle = kernel.OpenProcess(0x100000, False, int(pid))
        if handle:
            try:
                if kernel.WaitForSingleObject(handle, 120000) != 0:
                    raise RuntimeError('Player did not exit within 120 seconds')
            finally:
                kernel.CloseHandle(handle)
        elif ctypes.get_last_error() != 87:
            raise OSError('Cannot wait for the player process')
        if not verified(setup, {'sha256': digest, 'size': int(size)}):
            raise ValueError('Installer checksum or size mismatch')
        completed = subprocess.run([str(setup), '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/DIR=' + directory, '/LOG=' + str(setup.with_suffix('.install.log'))])
        if completed.returncode != 0:
            raise RuntimeError(f'Installer returned {completed.returncode}')
        if restart == '1':
            subprocess.Popen([str(Path(directory) / 'PAF-Videos.exe')])
        result = {'ok': True, 'installer_exit_code': completed.returncode}
    except Exception as error:
        result['error'] = str(error)
    finally:
        temporary = result_file.with_suffix('.result.tmp')
        temporary.write_text(json.dumps(result), encoding='utf-8')
        temporary.replace(result_file)
    return 0 if result['ok'] else 1


if __name__ == '__main__':
    if sys.platform == 'win32' and len(sys.argv) == 7:
        raise SystemExit(helper(sys.argv[1:]))
    raise SystemExit(2)
