"""Real Qt/libVLC integration; requires a working libVLC installation."""
import os
import sys
from pathlib import Path
os.environ.setdefault('QT_QPA_PLATFORM', 'offscreen')
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'src'))
from PySide6.QtCore import QSettings
from PySide6.QtWidgets import QApplication
from paf_videos.app import Window

def test_navigation_settings_and_queue(tmp_path):
    QSettings.setDefaultFormat(QSettings.IniFormat)
    QSettings.setPath(QSettings.IniFormat, QSettings.UserScope, str(tmp_path))
    app = QApplication.instance() or QApplication([])
    window = Window()
    window.auto.setChecked(False)
    try:
        window.toggle_nav()
        assert window.sidebar.width() == 300
        for name, index in window.page_map.items():
            window.show_page(name)
            assert window.pages.currentIndex() == index
        window.theme_choice.setCurrentText('Hell')
        assert window.settings.value('theme') == 'Hell'
        window.playlists['Test'] = ['sample.mkv']
        window.render_playlists()
        assert window.playlist_items.item(0).text() == 'sample.mkv'
        window.queue = ['one.mp4', 'two.mkv']
        window.render_queue()
        window.search.setText('two')
        assert window.lists['Wiedergabewarteschlange'].item(0).isHidden()
        assert not window.lists['Wiedergabewarteschlange'].item(1).isHidden()
        window.clear_queue()
        assert window.lists['Wiedergabewarteschlange'].count() == 0
        app.processEvents()
    finally:
        window.close()
