import hashlib
import os
import subprocess
import sys
import urllib.request
from pathlib import Path
from PySide6.QtCore import QThread, Signal
from .core import manifest, version
from . import __version__

BASE = 'https://github.com/Lyvt-Dev/PAF-Videos/releases'

def fetch(url, limit, destination=None):
    request = urllib.request.Request(url, headers={'User-Agent': 'PAF-Videos-Updater'})
    with urllib.request.urlopen(request, timeout=20) as response:
        if not response.url.startswith('https://'):
            raise ValueError('Unsichere Weiterleitung')
        data = bytearray()
        output = open(destination, 'wb') if destination else None
        try:
            total = 0
            while chunk := response.read(65536):
                total += len(chunk)
                if total > limit:
                    raise ValueError('Download größer als erwartet')
                if output:
                    output.write(chunk)
                else:
                    data.extend(chunk)
            return bytes(data)
        finally:
            if output:
                output.close()

def verified(path, info):
    p = Path(path)
    if not p.is_file() or p.stat().st_size != info['size']:
        return False
    with p.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest() == info['sha256']

def installed():
    if sys.platform != 'win32' or not getattr(sys, 'frozen', False):
        return False
    import winreg
    try:
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, r'Software\PAFVideos') as key:
            directory = winreg.QueryValueEx(key, 'InstallDir')[0]
        return Path(directory).resolve() == Path(sys.executable).parent.resolve() and (Path(directory) / 'unins000.exe').exists()
    except OSError:
        return False

class Updater(QThread):
    status = Signal(str)
    ready = Signal(str, object)
    def __init__(self, cache, automatic=False):
        super().__init__()
        self.cache, self.automatic = Path(cache), automatic
    def run(self):
        try:
            self.status.emit('Suche nach Updates …')
            info = manifest(fetch(BASE + '/latest/download/update.json', 8192))
            if version(info['version']) <= version(__version__):
                self.status.emit('PAF-Videos ist aktuell.')
                return
            if not installed():
                self.status.emit(f"Version {info['version']} verfügbar. Portable Version: Release herunterladen.")
                return
            self.status.emit(f"Lade Version {info['version']} …")
            self.cache.mkdir(parents=True, exist_ok=True)
            path = self.cache / ('setup-' + info['version'] + '.exe')
            if not verified(path, info):
                part = path.with_suffix('.part')
                try:
                    fetch(BASE + '/download/v' + info['version'] + '/setup.exe', info['size'], part)
                    if not verified(part, info):
                        raise ValueError('SHA256- oder Größenprüfung fehlgeschlagen')
                    part.replace(path)
                finally:
                    part.unlink(missing_ok=True)
            self.ready.emit(str(path), info)
            self.status.emit(f"Version {info['version']} bereit zur Installation.")
        except Exception as error:
            self.status.emit('Update nicht möglich: ' + str(error))

def apply(path, info, restart=True):
    if not installed() or not verified(path, info):
        raise ValueError('Installation oder Prüfsumme ungültig')
    # Separate frozen helper waits for the app process before starting Inno Setup.
    import shutil
    helper = Path(path).parent / ('apply-' + str(os.getpid()) + '.exe')
    source = Path(sys.executable).parent / 'PAF-Videos.Update.exe'
    if not source.is_file():
        raise ValueError('Update-Helfer fehlt')
    shutil.copy2(source, helper)
    subprocess.Popen([str(helper), str(os.getpid()), path, str(Path(sys.executable).parent), info['sha256'], str(info['size']), str(int(restart))], creationflags=subprocess.CREATE_NO_WINDOW)
