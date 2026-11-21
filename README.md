# Monsterspiel - Region 1 Test (melonDS)

1. Auf github.com ein neues Repository anlegen (z.B. "monsterspiel").
2. Den Inhalt dieses Ordners hochladen (Add file -> Upload files; der Ordner .github muss mit!).
3. Reiter "Actions" -> "Build NDS" laeuft automatisch (oder "Run workflow").
4. Fertig: Lauf anklicken -> unten "Artifacts" -> monsterspiel-nds herunterladen -> entpacken -> .nds in melonDS oeffnen.

Steuerung: Steuerkreuz laufen, A/B/X/Y Menue, SELECT Team, START weiter.
Gras = 'w' (wilde Monster), 'H' = Heiler/Laden.
Der Code ist ungetestet; bei Build-Fehlern den Fehlertext aus Actions hierher kopieren.

## Neu in dieser Version
- Alle Attacken-Effekte: Statusprobleme (Verbrennung, Paralyse, Gift, Schlaf, Frost, Verwirrung, Zurueckschrecken), Statuswerte +/-, Heilen, Absaugen, Rueckstoss, Mehrfachtreffer, Prioritaet, Volltreffer.
- Speichern: START auf der Karte oder automatisch beim Heiler. Datei: monsterspiel.sav auf der SD-Karte.
  In melonDS: Config -> DLDI -> "Enable DLDI" und ein SD-Image/Ordner angeben (sonst meldet das Spiel "Speichern aus").
