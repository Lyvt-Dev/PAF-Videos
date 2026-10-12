import os
import random
import sys
from pathlib import Path

# Bundled VLC must be discoverable before python-vlc loads its DLL.
if sys.platform == 'win32':
    root = Path(getattr(sys, '_MEIPASS', Path(__file__).resolve().parents[2])) / 'vlc'
    if root.exists():
        DLL_DIRECTORY = os.add_dll_directory(str(root))
        os.environ['VLC_PLUGIN_PATH'] = str(root / 'plugins')
        os.environ['PYTHON_VLC_LIB_PATH'] = str(root / 'libvlc.dll')
        os.environ['PYTHON_VLC_MODULE_PATH'] = str(root)
import vlc
from PySide6.QtCore import Qt, QTimer, QSettings, QStandardPaths, QUrl, QThread, Signal
from PySide6.QtGui import QAction, QDesktopServices, QKeySequence, QPixmap
from PySide6.QtWidgets import (QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout, QPushButton, QLabel, QSlider, QFileDialog, QListWidget, QLineEdit, QStackedWidget, QComboBox, QCheckBox, QMessageBox, QInputDialog, QScrollArea)
from . import __version__
from .core import scan, clock
from .updater import Updater, apply, BASE

class Scanner(QThread):
    result = Signal(str, object)
    def __init__(self, folders, kind):
        super().__init__()
        self.folders, self.kind = folders, kind
    def run(self):
        self.result.emit(self.kind, scan(self.folders, self.kind))

class Window(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle('PAF-Videos')
        self.resize(1280, 960)
        self.setMinimumSize(780, 600)
        self.settings = QSettings('PAF', 'PAF-Videos')
        self.engine = vlc.Instance('--no-video-title-show')
        if not self.engine:
            raise RuntimeError('VLC konnte nicht gestartet werden')
        self.player = self.engine.media_player_new()
        self.queue, self.index, self.pending = [], -1, None
        self.scanners, self.library = [], {}
        self.playlists = self.settings.value('playlists', {}) or {}
        self.expanded = False
        self.last_state = None
        root = QWidget(); self.setCentralWidget(root)
        outer = QVBoxLayout(root); outer.setContentsMargins(0, 0, 0, 0); outer.setSpacing(0)
        body = QHBoxLayout(); outer.addLayout(body, 1)
        self.sidebar = QWidget(); self.sidebar.setObjectName('sidebar'); self.sidebar.setFixedWidth(64)
        nav = QVBoxLayout(self.sidebar); body.addWidget(self.sidebar)
        self.nav_buttons = []
        self.button(nav, '☰', self.toggle_nav, 'Navigation öffnen')
        self.search = QLineEdit(); self.search.setPlaceholderText('Suchen'); self.search.setVisible(False); nav.addWidget(self.search)
        self.search.textChanged.connect(self.filter_items)
        for icon, name in [('⌂', 'Home'), ('♫', 'Musikbibliothek'), ('▣', 'Videobibliothek'), ('☷', 'Wiedergabewarteschlange'), ('▤', 'Wiedergabelisten')]:
            b = self.button(nav, icon, lambda checked=False, n=name: self.show_page(n), name)
            self.nav_buttons.append((b, icon, name))
        nav.addStretch()
        b = self.button(nav, '⚙', lambda: self.show_page('Einstellungen'), 'Einstellungen'); self.nav_buttons.append((b, '⚙', 'Einstellungen'))
        panel = QVBoxLayout(); panel.setContentsMargins(44, 14, 44, 0); body.addLayout(panel, 1)
        panel.addWidget(QLabel('◉   PAF-Videos'))
        self.heading = QLabel('Home'); self.heading.setObjectName('heading'); panel.addWidget(self.heading)
        self.pages = QStackedWidget(); panel.addWidget(self.pages, 1)
        self.page_map, self.lists = {}, {}
        home = QWidget(); h = QHBoxLayout(home); h.setContentsMargins(60, 0, 40, 0)
        art = QLabel(); art.setFixedSize(330, 330); art.setAlignment(Qt.AlignCenter)
        asset_root = Path(getattr(sys, '_MEIPASS', Path(__file__).resolve().parents[2]))
        art.setPixmap(QPixmap(str(asset_root / 'assets' / 'welcome.svg')).scaled(330, 330, Qt.KeepAspectRatio, Qt.SmoothTransformation)); h.addWidget(art)
        welcome = QVBoxLayout(); h.addLayout(welcome, 1); welcome.addStretch()
        title = QLabel('Dein neuer PAF-Videos Player'); title.setStyleSheet('font-size:30px;font-weight:600'); title.setWordWrap(True); welcome.addWidget(title)
        label = QLabel('Spiele deine Videos und Musik ab und entdecke deine persönlichen Bibliotheken.'); label.setWordWrap(True); welcome.addWidget(label)
        open_button = self.button(welcome, '▱  Datei(en) öffnen', self.open_files); open_button.setObjectName('accent'); open_button.setMaximumWidth(210)
        self.button(welcome, 'Ordner öffnen', self.open_folder)
        self.recent = QListWidget(); self.recent.setMaximumHeight(140); welcome.addWidget(self.recent); self.recent.itemDoubleClicked.connect(lambda item: self.enqueue([item.data(Qt.UserRole)]))
        welcome.addStretch(); self.add_page('Home', home)
        for name in ['Musikbibliothek', 'Videobibliothek', 'Wiedergabewarteschlange']:
            page = QWidget(); layout = QVBoxLayout(page)
            if name != 'Wiedergabewarteschlange':
                self.button(layout, 'Ordner hinzufügen', lambda checked=False, n=name: self.add_folder(n))
                self.button(layout, 'Bibliothek aktualisieren', lambda checked=False, n=name: self.refresh(n))
            else:
                self.button(layout, 'Warteschlange leeren', self.clear_queue)
                self.button(layout, 'Als Wiedergabeliste speichern', self.save_playlist)
            items = QListWidget(); layout.addWidget(items); self.lists[name] = items
            items.itemDoubleClicked.connect(lambda item, n=name: self.select_item(n, item)); self.add_page(name, page)
        page = QWidget(); layout = QVBoxLayout(page)
        self.playlist_names = QComboBox(); layout.addWidget(self.playlist_names)
        self.playlist_items = QListWidget(); layout.addWidget(self.playlist_items)
        self.playlist_names.currentTextChanged.connect(self.render_playlist)
        self.button(layout, 'Wiedergabeliste abspielen', lambda: self.enqueue(self.playlists.get(self.playlist_names.currentText(), [])))
        self.button(layout, 'Wiedergabeliste löschen', self.delete_playlist)
        self.playlist_items.itemDoubleClicked.connect(lambda item: self.enqueue([item.data(Qt.UserRole)])); self.add_page('Wiedergabelisten', page)
        self.video = QWidget(); self.video.setAttribute(Qt.WA_NativeWindow); self.video.setStyleSheet('background:black'); self.add_page('Wiedergabe', self.video)
        self.build_settings()
        controls = QWidget(); controls.setObjectName('controls'); outer.addWidget(controls)
        c = QVBoxLayout(controls); line = QHBoxLayout(); c.addLayout(line)
        self.elapsed = QLabel('00:00:00'); line.addWidget(self.elapsed)
        self.seek = QSlider(Qt.Horizontal); self.seek.setRange(0, 10000); line.addWidget(self.seek, 1); self.seek.sliderReleased.connect(self.seek_to)
        self.duration = QLabel('00:00:00'); line.addWidget(self.duration)
        self.track = QLabel('Keine Datei geöffnet'); c.addWidget(self.track)
        row = QHBoxLayout(); c.addLayout(row); row.addStretch()
        self.shuffle = QCheckBox('⤨'); self.shuffle.setToolTip('Zufällige Wiedergabe'); row.addWidget(self.shuffle)
        self.button(row, '⏮', lambda: self.step(-1), 'Vorheriger Titel')
        self.play_button = self.button(row, '▶', self.toggle_play, 'Wiedergabe / Pause'); self.play_button.setObjectName('play')
        self.button(row, '⏭', lambda: self.step(1), 'Nächster Titel')
        self.repeat = QCheckBox('↻'); self.repeat.setToolTip('Warteschlange wiederholen'); row.addWidget(self.repeat); row.addStretch()
        self.button(row, '♪', self.audio_track, 'Audiospur wählen'); self.button(row, 'CC', self.subtitle, 'Untertitel laden')
        self.speed = QComboBox(); self.speed.addItems(['0.5×', '0.75×', '1×', '1.25×', '1.5×', '2×']); self.speed.setCurrentIndex(2); row.addWidget(self.speed)
        self.speed.currentTextChanged.connect(lambda t: self.player.set_rate(float(t[:-1])))
        self.mute = QCheckBox('🔊'); self.mute.toggled.connect(self.player.audio_set_mute); row.addWidget(self.mute)
        self.volume = QSlider(Qt.Horizontal); self.volume.setMaximumWidth(100); self.volume.setRange(0, 100); self.volume.setValue(int(self.settings.value('volume', 80))); self.volume.valueChanged.connect(self.set_volume); row.addWidget(self.volume)
        self.button(row, '⛶', self.fullscreen, 'Vollbild')
        for shortcut, callback in [('Space', self.toggle_play), ('Ctrl+O', self.open_files), ('F11', self.fullscreen), ('Escape', lambda: self.showNormal()), ('Right', lambda: self.player.set_time(self.player.get_time() + 10000)), ('Left', lambda: self.player.set_time(max(0, self.player.get_time() - 10000)))]:
            action = QAction(self); action.setShortcut(QKeySequence(shortcut)); action.triggered.connect(callback); self.addAction(action)
        self.setAcceptDrops(True)
        self.timer = QTimer(self); self.timer.timeout.connect(self.tick); self.timer.start(250)
        self.update_timer = QTimer(self); self.update_timer.timeout.connect(self.automatic_update); self.update_timer.start(24 * 60 * 60 * 1000)
        self.set_volume(self.volume.value()); self.theme(); self.render_playlists(); self.render_recent(); self.show_page('Home')
        for name in ['Musikbibliothek', 'Videobibliothek']: self.refresh(name)
        QTimer.singleShot(2000, self.automatic_update)

    def button(self, layout, text, callback, tip=''):
        b = QPushButton(text); b.setToolTip(tip or text); b.clicked.connect(callback); layout.addWidget(b); return b
    def add_page(self, name, widget):
        self.page_map[name] = self.pages.addWidget(widget)
    def show_page(self, name):
        self.heading.setText(name); self.pages.setCurrentIndex(self.page_map[name]); self.filter_items(self.search.text())
        for b, icon, title in self.nav_buttons:
            b.setProperty('selected', name == title); b.style().unpolish(b); b.style().polish(b)
    def toggle_nav(self):
        self.expanded = not self.expanded; self.sidebar.setFixedWidth(300 if self.expanded else 64); self.search.setVisible(self.expanded)
        for b, icon, name in self.nav_buttons: b.setText(icon + '   ' + name if self.expanded else icon)
    def build_settings(self):
        scroll = QScrollArea(); scroll.setWidgetResizable(True); content = QWidget(); scroll.setWidget(content); layout = QVBoxLayout(content)
        layout.addWidget(QLabel('Bibliotheken'))
        for name in ['Musikbibliothek', 'Videobibliothek']:
            self.button(layout, name + ': Ordner hinzufügen', lambda checked=False, n=name: self.add_folder(n))
            self.button(layout, name + ': Ordner entfernen', lambda checked=False, n=name: self.remove_folder(n))
        self.button(layout, 'Bibliotheken aktualisieren', lambda: [self.refresh(n) for n in ['Musikbibliothek', 'Videobibliothek']])
        layout.addWidget(QLabel('Personalisierung'))
        self.theme_choice = QComboBox(); self.theme_choice.addItems(['System', 'Dunkel', 'Hell']); self.theme_choice.setCurrentText(self.settings.value('theme', 'Dunkel')); layout.addWidget(self.theme_choice); self.theme_choice.currentTextChanged.connect(self.theme)
        self.accent_choice = QComboBox(); self.accent_choice.addItems(['Orange', 'Violett', 'Blau', 'Grün']); self.accent_choice.setCurrentText(self.settings.value('accent', 'Orange')); layout.addWidget(self.accent_choice); self.accent_choice.currentTextChanged.connect(self.theme)
        layout.addWidget(QLabel('Datenschutz'))
        self.remember = QCheckBox('Zuletzt geöffnete Medien merken'); self.remember.setChecked(self.settings.value('remember', True, type=bool)); layout.addWidget(self.remember); self.remember.toggled.connect(self.remember_changed)
        self.button(layout, 'Verlauf löschen', self.clear_recent)
        layout.addWidget(QLabel('Updates'))
        self.auto = QCheckBox('Automatische Updates (beim Start und alle 24 Stunden)'); self.auto.setChecked(self.settings.value('automatic', True, type=bool)); layout.addWidget(self.auto); self.auto.toggled.connect(lambda enabled: self.settings.setValue('automatic', enabled))
        self.update_status = QLabel('Noch nicht geprüft'); self.update_status.setWordWrap(True); layout.addWidget(self.update_status)
        self.button(layout, 'Nach Updates suchen', self.check_update)
        self.install_button = self.button(layout, 'Jetzt installieren und neu starten', self.install_update); self.install_button.setEnabled(False)
        self.button(layout, 'GitHub-Releases öffnen', lambda: QDesktopServices.openUrl(QUrl(BASE)))
        layout.addWidget(QLabel('Über PAF-Videos\nVersion ' + __version__ + '\nWiedergabe mit libVLC. Medienpfade werden nicht an GitHub übertragen.'))
        layout.addStretch(); self.add_page('Einstellungen', scroll)
    def theme(self):
        mode = self.theme_choice.currentText(); light = mode == 'Hell' or (mode == 'System' and QApplication.styleHints().colorScheme() == Qt.ColorScheme.Light)
        accent = {'Orange': '#ff822b', 'Violett': '#b576ee', 'Blau': '#58b9ff', 'Grün': '#65cf98'}[self.accent_choice.currentText()]
        bg, card, fg = ('#f5f5f5', '#e5e5e5', '#202020') if light else ('#25282b', '#323436', '#f5f5f5')
        self.setStyleSheet(f'''QWidget {{background:{bg};color:{fg};font-family:"Segoe UI";font-size:15px;}} #heading {{font-size:48px;font-weight:600;padding:12px 0 20px;}} #sidebar,#controls {{background:{card};}} QPushButton {{background:{card};border:1px solid transparent;border-radius:5px;padding:12px;text-align:left;}} QPushButton:hover {{border-color:{accent};}} QPushButton[selected="true"] {{border-left:4px solid {accent};}} #accent {{background:{accent};color:#151515;}} #hero {{border-radius:135px;background:qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 #8b2aa5,stop:1 {accent});font-size:100px;}} #play {{font-size:28px;border:4px solid #505050;border-radius:30px;}} QListWidget,QLineEdit,QComboBox {{background:{card};border:0;border-radius:5px;padding:10px;}} QSlider::groove:horizontal {{height:4px;background:#686868;}} QSlider::handle:horizontal {{background:{accent};width:15px;margin:-6px 0;border-radius:7px;}} QCheckBox::indicator:checked {{background:{accent};}}''')
        self.settings.setValue('theme', mode); self.settings.setValue('accent', self.accent_choice.currentText())
    def open_files(self):
        paths, _ = QFileDialog.getOpenFileNames(self, 'Medien öffnen', '', 'Alle Dateien (*);;Untertitel (*.srt *.ass *.vtt)'); self.enqueue(paths)
    def open_folder(self):
        folder = QFileDialog.getExistingDirectory(self, 'Medienordner öffnen')
        if folder: self.enqueue(scan([folder], 'Videobibliothek') + scan([folder], 'Musikbibliothek'))
    def enqueue(self, paths):
        paths = [str(Path(p).resolve()) for p in paths if Path(p).is_file()]
        if not paths: return
        start = len(self.queue); self.queue.extend(paths); self.render_queue(); self.play_at(start)
    def render_queue(self):
        self.fill(self.lists['Wiedergabewarteschlange'], self.queue)
    def fill(self, widget, paths):
        widget.clear()
        for path in paths:
            widget.addItem(Path(path).name); widget.item(widget.count() - 1).setData(Qt.UserRole, path); widget.item(widget.count() - 1).setToolTip(path)
    def play_at(self, index):
        if not 0 <= index < len(self.queue): return
        self.index = index; path = self.queue[index]
        self.player.stop(); media = self.engine.media_new(path); self.player.set_media(media); media.release()
        handle = int(self.video.winId())
        if sys.platform == 'win32': self.player.set_hwnd(handle)
        elif sys.platform == 'darwin': self.player.set_nsobject(handle)
        else: self.player.set_xwindow(handle)
        self.show_page('Wiedergabe'); self.track.setText(Path(path).name)
        if self.player.play() == -1: QMessageBox.warning(self, 'Wiedergabe', 'Datei konnte nicht geöffnet werden.')
        self.player.set_rate(float(self.speed.currentText()[:-1]))
        if self.remember.isChecked():
            recent = self.settings.value('recent', []) or []; recent = [p for p in recent if p != path]; self.settings.setValue('recent', [path] + recent[:19]); self.render_recent()
    def toggle_play(self):
        if self.index < 0:
            if self.queue: self.play_at(0)
            else: self.open_files()
        elif self.player.get_state() in (vlc.State.Ended, vlc.State.Stopped, vlc.State.Error): self.play_at(self.index)
        else: self.player.pause()
    def step(self, delta):
        if not self.queue: return
        index = random.randrange(len(self.queue)) if self.shuffle.isChecked() else self.index + delta
        if self.repeat.isChecked(): index %= len(self.queue)
        if 0 <= index < len(self.queue): self.play_at(index)
        else: self.player.stop()
    def tick(self):
        state = self.player.get_state(); self.play_button.setText('Ⅱ' if state == vlc.State.Playing else '▶')
        length, position = self.player.get_length(), self.player.get_time(); self.elapsed.setText(clock(position)); self.duration.setText(clock(length))
        if not self.seek.isSliderDown(): self.seek.setValue(int(max(0, position) / max(1, length) * 10000))
        if state == vlc.State.Ended and self.last_state != vlc.State.Ended: self.step(1)
        elif state == vlc.State.Error:
            self.player.stop(); QMessageBox.warning(self, 'Wiedergabefehler', 'Diese Datei ist beschädigt, geschützt oder ihr Codec wird von libVLC nicht unterstützt.')
        self.last_state = state
    def seek_to(self): self.player.set_time(int(self.seek.value() / 10000 * max(0, self.player.get_length())))
    def set_volume(self, value): self.player.audio_set_volume(value); self.settings.setValue('volume', value)
    def fullscreen(self): self.showNormal() if self.isFullScreen() else self.showFullScreen()
    def subtitle(self):
        path, _ = QFileDialog.getOpenFileName(self, 'Untertitel öffnen', '', 'Untertitel (*.srt *.ass *.ssa *.vtt);;Alle Dateien (*)')
        if path and self.player.add_slave(vlc.MediaSlaveType.subtitle, QUrl.fromLocalFile(path).toString(), True) != 0: QMessageBox.warning(self, 'Untertitel', 'Untertitel konnten nicht geladen werden.')
    def audio_track(self):
        tracks = self.player.audio_get_track_description() or []
        names = [name.decode('utf-8', errors='replace') for _, name in tracks]
        if not names: return
        name, ok = QInputDialog.getItem(self, 'Audiospur', 'Spur auswählen', names, 0, False)
        if ok: self.player.audio_set_track(tracks[names.index(name)][0])
    def select_item(self, name, item):
        if name == 'Wiedergabewarteschlange': self.play_at(self.lists[name].row(item))
        else: self.enqueue([item.data(Qt.UserRole)])
    def clear_queue(self): self.player.stop(); self.queue.clear(); self.index = -1; self.render_queue(); self.track.setText('Keine Datei geöffnet')
    def add_folder(self, name):
        folder = QFileDialog.getExistingDirectory(self, 'Bibliotheksordner auswählen')
        if folder:
            folders = self.settings.value(name, []) or []
            if folder not in folders: folders.append(folder)
            self.settings.setValue(name, folders); self.refresh(name)
    def remove_folder(self, name):
        folders = self.settings.value(name, []) or []
        if not folders: return
        folder, ok = QInputDialog.getItem(self, 'Ordner entfernen', 'Ordner', folders, 0, False)
        if ok: folders.remove(folder); self.settings.setValue(name, folders); self.refresh(name)
    def refresh(self, name):
        if any(s.isRunning() and s.kind == name for s in self.scanners): return
        worker = Scanner(self.settings.value(name, []) or [], name); self.scanners.append(worker); worker.result.connect(self.scanned); worker.start()
    def scanned(self, name, paths): self.library[name] = paths; self.fill(self.lists[name], paths); self.filter_items(self.search.text())
    def filter_items(self, query):
        for items in self.lists.values():
            for i in range(items.count()): items.item(i).setHidden(query.casefold() not in items.item(i).text().casefold())
    def save_playlist(self):
        if not self.queue: return
        name, ok = QInputDialog.getText(self, 'Wiedergabeliste speichern', 'Name')
        if ok and name.strip():
            name = name.strip()
            if name in self.playlists and QMessageBox.question(self, 'Überschreiben', 'Vorhandene Wiedergabeliste ersetzen?') != QMessageBox.Yes: return
            self.playlists[name] = list(self.queue); self.settings.setValue('playlists', self.playlists); self.render_playlists()
    def render_playlists(self): self.playlist_names.clear(); self.playlist_names.addItems(sorted(self.playlists))
    def render_playlist(self, name): self.fill(self.playlist_items, self.playlists.get(name, []))
    def delete_playlist(self):
        name = self.playlist_names.currentText()
        if name in self.playlists: del self.playlists[name]; self.settings.setValue('playlists', self.playlists); self.render_playlists()
    def render_recent(self): self.fill(self.recent, self.settings.value('recent', []) or []); self.recent.setVisible(self.recent.count() > 0)
    def clear_recent(self): self.settings.setValue('recent', []); self.render_recent()
    def remember_changed(self, enabled):
        self.settings.setValue('remember', enabled)
        if not enabled: self.clear_recent()
    def automatic_update(self):
        if self.auto.isChecked(): self.check_update()
    def check_update(self):
        if hasattr(self, 'updater') and self.updater.isRunning(): return
        cache = Path(QStandardPaths.writableLocation(QStandardPaths.CacheLocation)) / 'updates'
        self.updater = Updater(cache); self.updater.status.connect(self.update_status.setText); self.updater.ready.connect(self.update_ready); self.updater.start()
    def update_ready(self, path, info): self.pending = (path, info); self.install_button.setEnabled(True)
    def install_update(self):
        if self.pending:
            try: apply(*self.pending); self.pending = None; self.close()
            except Exception as error: QMessageBox.warning(self, 'Update', str(error))
    def dragEnterEvent(self, event):
        if event.mimeData().hasUrls(): event.acceptProposedAction()
    def dropEvent(self, event): self.enqueue([u.toLocalFile() for u in event.mimeData().urls() if u.isLocalFile()])
    def closeEvent(self, event):
        if hasattr(self, 'updater') and self.updater.isRunning():
            self.update_status.setText('Update läuft noch. Bitte kurz warten und erneut schließen.'); event.ignore(); return
        if self.pending and self.auto.isChecked():
            try: apply(*self.pending, restart=False)
            except Exception: pass
        for worker in self.scanners: worker.wait()
        self.player.stop(); self.player.release(); self.engine.release(); self.settings.sync(); event.accept()

def main():
    app = QApplication(sys.argv); app.setApplicationName('PAF-Videos'); app.setOrganizationName('PAF')
    try: window = Window()
    except Exception as error:
        QMessageBox.critical(None, 'PAF-Videos', 'VLC konnte nicht geladen werden. Installiere VLC passend zur Python-Architektur.\n' + str(error)); return 1
    window.show()
    if len(sys.argv) > 1: window.enqueue(sys.argv[1:])
    return app.exec()
