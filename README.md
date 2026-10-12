# PAF-Videos

Windows-Desktop-Mediaplayer mit einer an den Windows Media Player angelehnten Oberfläche: einklappbare Navigation, Home, Musik- und Videobibliothek, Warteschlange, Wiedergabelisten, dunkles/helles Design und Akzentfarben. Dateien lassen sich öffnen oder ins Fenster ziehen. Mit Lautstärke, Stummschaltung, Seek, Geschwindigkeit, Wiederholung, Zufallswiedergabe, Vollbild, externen Untertiteln und Audiospurwahl.

## Entwicklung

Python 3.12 und VLC 3.x in derselben Architektur installieren (Windows: 64 Bit). Unter Debian zusätzlich `libvlc5 vlc-plugin-base vlc-plugin-video-output` installieren.

```sh
python -m venv .venv
# Windows: .venv\Scripts\activate
# Linux: source .venv/bin/activate
python -m pip install -r requirements.txt
python run.py
```

Tastatur: Strg+O öffnet Dateien, Leertaste pausiert, Pfeile springen 10 Sekunden, F11 schaltet Vollbild, Escape beendet Vollbild.

## Formate

Der Dateidialog nimmt jede Datei an. libVLC übernimmt Demuxing und Decodierung, einschließlich verbreiteter MP4-, MKV-, AVI-, MOV-, WebM-, MPEG-, TS- und Audioformate. Bibliotheken erkennen die Erweiterungen in `src/paf_videos/core.py`. Eine Garantie für **alle** Videoformate ist technisch nicht möglich: DRM, beschädigte Dateien und unbekannte/proprietäre Codecs können nicht abgespielt werden. Es findet keine automatische Konvertierung statt.

## Updates wie bei PAF-Photos

Beim Start und alle 24 Stunden wird `https://github.com/Lyvt-Dev/PAF-Videos/releases/latest/download/update.json` geprüft. Das Manifest enthält `version`, `sha256` und `size`. Installierte Windows-Versionen laden `setup.exe` im Hintergrund, prüfen Größe und SHA256 und installieren beim Beenden oder über „Jetzt installieren“ ohne Administratorrechte. Ein separater Prozess wartet auf das Ende des Players. Einstellungen und Bibliotheken bleiben in QSettings erhalten. Portable Versionen verweisen zum Release. Offline-Wiedergabe bleibt möglich. Prüfsummen ersetzen keine Herausgebersignatur.

GitHub Actions baut einen Installer, ein Manifest und ein portables ZIP als Artefakt. Für ein Update diese drei Dateien an einem GitHub-Release `v0.1.0` (bzw. der jeweiligen Version in `__init__.py`) anhängen. Bei einem Versions-Tag `v0.1.0` veröffentlicht der Workflow nach erfolgreichen Windows-Prüfungen diese drei Dateien automatisch als GitHub-Release. Die Pipeline prüft die gebündelte EXE, die installierte EXE, Audiodecodierung und eine erneute Installation durch den Update-Helfer. Ein Versionswechsel zu einer zukünftigen Version muss zusätzlich getestet werden. Downloads: https://github.com/Lyvt-Dev/PAF-Videos/releases/latest

## Tests

```sh
python -m pip install pytest
python -m pytest -q
```

Der Windows-Build läuft in `.github/workflows/build.yml`. Lokale Linux-Tests ersetzen keine Prüfung des Windows-Installers. Bibliotheks-Scans laufen im Hintergrund; Verlauf, Ordner, Wiedergabelisten und Einstellungen werden lokal gespeichert. Online-Cover-Suche und eine originalgetreue Kopie der Microsoft-Illustration sind nicht enthalten.

## Drittanbieter

Qt/PySide6 und libVLC sind externe Komponenten; die jeweiligen Lizenzdateien müssen bei Distribution erhalten bleiben. Der Windows-Build bündelt die VLC-Installation samt Plugins und Lizenzdateien. Siehe `THIRD_PARTY_NOTICES.md`.
