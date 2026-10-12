import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent / 'src'))

if __name__ == '__main__':
    if len(sys.argv) == 5 and sys.argv[1] == '--test-update':
        from paf_videos.updater import apply
        apply(sys.argv[2], {'sha256': sys.argv[3], 'size': int(sys.argv[4])}, restart=False)
    elif len(sys.argv) == 3 and sys.argv[1] == '--smoke-test':
        # CI uses the actual frozen EXE, Qt platform plugin and bundled libVLC.
        import tempfile
        import wave
        import time
        from PySide6.QtCore import QSettings
        from PySide6.QtWidgets import QApplication
        from paf_videos.app import Window
        import vlc
        report = Path(sys.argv[2])
        try:
            with tempfile.TemporaryDirectory(prefix='paf-smoke-') as folder:
                QSettings.setDefaultFormat(QSettings.IniFormat)
                QSettings.setPath(QSettings.IniFormat, QSettings.UserScope, folder)
                app = QApplication(sys.argv)
                window = Window()
                window.auto.setChecked(False)
                window.show()
                app.processEvents()
                assert len(window.page_map) == 7
                assert not window.video.size().isEmpty()
                assert window.findChildren(type(window.recent))
                fixture = Path(folder) / 'tone.wav'
                with wave.open(str(fixture), 'wb') as stream:
                    stream.setparams((1, 2, 44100, 0, 'NONE', 'not compressed'))
                    stream.writeframes(b'\x00\x00' * 44100 * 3)
                engine = vlc.Instance('--aout=dummy', '--vout=dummy')
                media = engine.media_new(str(fixture))
                player = engine.media_player_new()
                player.set_media(media)
                assert player.play() == 0
                deadline = time.monotonic() + 10
                stats = vlc.MediaStats()
                while time.monotonic() < deadline:
                    media.get_stats(stats)
                    if stats.decoded_audio > 0:
                        break
                    time.sleep(0.05)
                assert stats.decoded_audio > 0, 'Bundled VLC did not decode audio'
                player.stop(); player.release(); media.release(); engine.release()
                window.close()
                report.write_text(json.dumps({'ok': True, 'pages': 7, 'decoded_audio': stats.decoded_audio}), encoding='utf-8')
        except Exception as error:
            import traceback
            report.write_text(json.dumps({'ok': False, 'error': str(error), 'traceback': traceback.format_exc()}), encoding='utf-8')
            raise SystemExit(1)
    else:
        from paf_videos.app import main
        raise SystemExit(main())
