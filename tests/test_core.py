import hashlib
import json
import sys
from pathlib import Path
import pytest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'src'))
from paf_videos.core import version, manifest, scan, clock
from paf_videos.updater import verified

def test_version_order():
    assert version('0.1.0') < version('0.1.0-fix') < version('0.1.1')
    for invalid in ['01.2.3', '1.2', '1.2.3/bad', '65536.0.0']:
        with pytest.raises(ValueError): version(invalid)

def test_manifest_rejects_unsafe_data():
    good = {'version': '0.1.1', 'sha256': 'a' * 64, 'size': 2048}
    assert manifest(json.dumps(good)) == good
    for changes in [{'size': True}, {'size': 0}, {'sha256': '../evil'}, {'version': '../evil'}, {'extra': 'bad'}]:
        with pytest.raises(ValueError): manifest(json.dumps(good | changes))

def test_download_integrity(tmp_path):
    path = tmp_path / 'setup.exe'; path.write_bytes(b'x' * 2048)
    info = {'size': 2048, 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}
    assert verified(path, info)
    path.write_bytes(b'y' * 2048)
    assert not verified(path, info)
    path.unlink(); assert not verified(path, info)

def test_library_scan(tmp_path):
    (tmp_path / 'nested').mkdir()
    video = tmp_path / 'nested' / 'clip.MKV'; video.touch()
    song = tmp_path / 'track.flac'; song.touch()
    (tmp_path / 'notes.txt').touch()
    assert scan([str(tmp_path), str(tmp_path)], 'Videobibliothek') == [str(video)]
    assert scan([str(tmp_path)], 'Musikbibliothek') == [str(song)]
    assert clock(3661000) == '01:01:01'

def test_fetch_limits_and_tls_destination(monkeypatch, tmp_path):
    import io
    from paf_videos.updater import fetch
    class Response(io.BytesIO):
        url = 'https://github.com/asset'
    monkeypatch.setattr('urllib.request.urlopen', lambda *a, **k: Response(b'abc'))
    assert fetch('https://github.com/asset', 3) == b'abc'
    with pytest.raises(ValueError): fetch('https://github.com/asset', 2)
    destination = tmp_path / 'download'
    fetch('https://github.com/asset', 3, destination)
    assert destination.read_bytes() == b'abc'
    Response.url = 'http://github.com/asset'
    with pytest.raises(ValueError): fetch('https://github.com/asset', 3)
