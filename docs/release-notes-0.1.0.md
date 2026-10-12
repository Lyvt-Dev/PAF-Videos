# PAF-Videos 0.1.0

Erste Windows-Version des PAF-Videos Desktop-Mediaplayers.

- An Windows Media Player angelehnte Oberfläche, einklappbare Navigation und eigenes Startmotiv.
- Musik-/Videobibliotheken, Suche, Wiedergabewarteschlange, lokal gespeicherte Wiedergabelisten und Verlauf.
- VLC-Wiedergabe inklusive Untertitel, Audiospurwahl, Lautstärke, Geschwindigkeit und Vollbild.
- Dunkles/helles Design und Akzentfarben.
- GitHub-Updates mit Größen- und SHA256-Prüfung, Installation im Benutzerprofil und separatem Update-Helfer.

## Downloads

- **setup.exe**: Windows-x64-Installer inklusive VLC; keine separate VLC-Installation erforderlich.
- **PAF-Videos-portable.zip**: vollständig entpacken und PAF-Videos.exe starten.
- **update.json**: Manifest für die integrierte Update-Prüfung.

Die Windows-Buildpipeline prüft Qt/libVLC-Start und Audiodecodierung in der gebündelten und installierten EXE sowie die erneute Installation über den Update-Helfer. Die Aktualisierung wird mit demselben Installer getestet; ein Versionswechsel von einer älteren Veröffentlichung und eine vollständige manuelle Prüfung aller Videoformate sind damit nicht nachgewiesen.

DRM, beschädigte Dateien und nicht von libVLC unterstützte Codecs können nicht wiedergegeben werden. Die EXE und der Installer sind nicht mit einem Herausgeberzertifikat signiert; Windows SmartScreen kann eine Warnung anzeigen. Prüfsummen ersetzen keine Herausgebersignatur.
