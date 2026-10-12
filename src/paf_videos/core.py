import json
import re
from pathlib import Path

VIDEO = {'.mp4', '.mkv', '.avi', '.mov', '.wmv', '.webm', '.m4v', '.mpeg', '.mpg', '.ts', '.mts', '.m2ts', '.flv', '.ogv', '.vob', '.3gp', '.hevc', '.h264', '.mxf', '.divx', '.asf', '.rm', '.rmvb'}
AUDIO = {'.mp3', '.flac', '.wav', '.aac', '.m4a', '.ogg', '.opus', '.wma', '.aiff', '.alac'}

def version(text):
    if not re.fullmatch(r'(0|[1-9]\d{0,4})\.(0|[1-9]\d{0,4})\.(0|[1-9]\d{0,4})(-fix)?', text):
        raise ValueError('Ungültige Version')
    parts = tuple(map(int, text.removesuffix('-fix').split('.')))
    if max(parts) > 65535:
        raise ValueError('Ungültige Version')
    return parts + (int(text.endswith('-fix')),)

def manifest(data):
    if len(data) > 8192:
        raise ValueError('Manifest zu groß')
    item = json.loads(data)
    if set(item) != {'version', 'sha256', 'size'}:
        raise ValueError('Ungültiges Manifest')
    version(item['version'])
    if not re.fullmatch('[0-9a-f]{64}', item['sha256']) or type(item['size']) is not int or not 1024 <= item['size'] <= 512 * 1024 * 1024:
        raise ValueError('Ungültige Prüfdaten')
    return item

def scan(folders, kind):
    extensions = AUDIO if kind == 'Musikbibliothek' else VIDEO
    result = set()
    for folder in folders:
        try:
            result.update(str(p.resolve()) for p in Path(folder).rglob('*') if p.is_file() and p.suffix.lower() in extensions)
        except OSError:
            continue
    return sorted(result, key=lambda p: Path(p).name.casefold())

def clock(ms):
    seconds = max(0, ms // 1000)
    return f'{seconds // 3600:02}:{seconds // 60 % 60:02}:{seconds % 60:02}'
