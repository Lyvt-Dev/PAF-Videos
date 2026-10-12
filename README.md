# PAF-Videos · Native C++

Windows-Desktop-Mediaplayer in **C++17 / Win32 / GDI+**. Kein Python, kein Qt und keine Webview im Anwendungspaket. Mit eigenem cyan/violetten V-Logo im Stil von PAF-Photos.

## Download und Start

https://github.com/Lyvt-Dev/PAF-Videos/releases/latest

`setup.exe` installiert im Benutzerprofil ohne Administratorrechte. Für die portable Version das **gesamte ZIP entpacken**, dann `PAF-Videos.exe` öffnen. Der Ordner `vlc` mit DLLs und Plugins muss neben der EXE erhalten bleiben. Eine separate VLC-Installation ist nicht nötig. Die App und der Installer sind nicht mit einem Herausgeberzertifikat signiert.

Bei Startproblemen wird eine Fehlermeldung angezeigt und ein Protokoll unter `%LOCALAPPDATA%\PAF-Videos\startup.log` geschrieben. Die App lädt ausschließlich den mitgelieferten VLC-Runtime-Ordner und fällt nicht auf eine globale VLC-Installation zurück.

## Funktionen

- Einklappbare Navigation, Home, Musik-/Videobibliotheken, Suche, Warteschlange und M3U8-Wiedergabelisten.
- Dateien öffnen oder ins Fenster ziehen; Bibliotheksordner hinzufügen, entfernen und im Hintergrund durchsuchen.
- Play/Pause, Seek, Lautstärke, Stummschaltung, Zufallswiedergabe, Wiederholung, Geschwindigkeit und Vollbild.
- Externe Untertitel und Audiospurwahl.
- Dunkles, helles und Systemdesign, vier Akzentfarben, lokaler Verlauf und Datenschutz-Einstellungen.
- Automatische GitHub-Updates mit Größen-/SHA256-Prüfung und eigenem nativem Update-Helfer.

Strg+O öffnet Dateien, Leertaste pausiert, F11 schaltet Vollbild. Einstellungen lassen sich mit dem Mausrad scrollen. Alle Dateien werden im Dialog angenommen; libVLC entscheidet anhand des Inhalts, ob sie decodierbar sind. DRM, beschädigte Dateien und unbekannte Codecs können nicht garantiert wiedergegeben werden.

## C++-Entwicklung

Windows: Visual Studio 2022/2025 mit Desktop-C++-Werkzeugen und CMake.

```sh
cmake -S . -B build -A x64
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure
```

Zum Ausführen den VLC-3.x-x64-Runtime-Ordner als `build/Release/vlc` bereitstellen. Benötigt werden `libvlc.dll`, `libvlccore.dll`, deren mitgelieferte DLL-Abhängigkeiten und das gesamte `plugins`-Verzeichnis. Die GitHub-Pipeline stellt diese Dateien aus dem verifizierten VLC-Paket bereit und entfernt anschließend die globale VLC-Installation, bevor die App getestet wird.

Linux unterstützt die plattformunabhängigen C++-Tests:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

## Updates und Releases

Die installierte App prüft beim Start und alle 24 Stunden `releases/latest/download/update.json`, lädt `setup.exe` im Hintergrund und prüft Dateigröße und SHA256. „Jetzt installieren“ startet nach dem Update neu; ansonsten erfolgt die Installation beim Beenden. Portable Apps zeigen verfügbare Releases an. Medienpfade werden nicht an GitHub übertragen. Einstellungen, Verlauf, Bibliotheksordner und M3U8-Listen liegen unter `%LOCALAPPDATA%\PAF-Videos`.

Version in `CMakeLists.txt` aktualisieren, Release-Notizen in `docs/release-notes-VERSION.md` hinzufügen und `vVERSION` taggen. GitHub Actions veröffentlicht erst nach erfolgreichen C++-Tests, normalem Start, echten Videodecode-Prüfungen und Installer-/Update-Helfer-Tests die drei Assets. Der Update-Helfer-Test installiert denselben Installer erneut; ein Versionswechsel aus einer bereits veröffentlichten Vorgängerversion ist dadurch nicht bewiesen.

Die native Version 0.1.1 ersetzt die Python/Qt-Version 0.1.0. Der Installer entfernt deren nicht mehr benötigten `_internal`-Runtime-Ordner; die native App speichert Einstellungen in eigenen lokalen Dateien. Frühere Qt-Einstellungen werden nicht automatisch konvertiert.
