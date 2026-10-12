# PAF-Videos 0.1.1 – Native C++

Die Anwendung und der Update-Helfer sind jetzt vollständig in C++17 implementiert. Win32/GDI+ ersetzen Python/Qt.

- Eigenes cyan/violettes V-Logo im Stil von PAF-Photos: EXE, Fenster, Taskleiste und Installer.
- Explizites Laden der mitgelieferten VLC-DLLs und Plugins; kein separat installiertes VLC erforderlich.
- Sichtbare Fehlermeldung und `%LOCALAPPDATA%\PAF-Videos\startup.log` bei Startproblemen.
- Bibliotheken, Warteschlange, M3U8-Wiedergabelisten, Suche, Verlauf, Untertitel, Audiospuren, Geschwindigkeit und Design-Einstellungen.
- Nativer GitHub-Updater mit Größen-/SHA256-Prüfung und separatem Update-Helfer.

Die Windows-Pipeline entfernt global installiertes VLC vor den Laufzeittests. Sie prüft echte Videodecodierung in der portablen, installierten und erneut aktualisierten App sowie den gewöhnlichen Programmstart inklusive Ereignisschleife und Update-Start.

**Installation:** `setup.exe` kann über 0.1.0 installiert werden. Alte Python/Qt-Runtime-Dateien werden entfernt. Die native App verwendet neue lokale Einstellungsdateien; frühere Qt-Einstellungen werden nicht automatisch konvertiert. Für die portable Version das gesamte ZIP entpacken.

Der vorherige Fehler wurde auf dem Nutzergerät nicht reproduziert; die Laufzeit wurde durch eine native Implementierung ersetzt und die Startprüfungen erweitert. Eine vollständige Prüfung sämtlicher Codecs und ein Versionswechsel aus einer veröffentlichten Vorgängerversion sind durch die Wiederinstallationsprüfung nicht belegt.
