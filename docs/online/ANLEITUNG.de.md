# DriveClub online mit Freunden (Kurzanleitung)

Experimentell. Private Lobbys und gemeinsame Rennen funktionieren, öffentliches Matchmaking, Clubs und Ghosts noch
nicht. Details: [README.md](README.md).

## Was du brauchst

- Deine eigene Kopie von DriveClub (PS4, CUSA00093, Version 1.28). Spieldateien sind nirgends dabei.
- Einen Build dieses Zweigs (oder ein Release-Paket).
- **Nur der Host:** den Community-Server (`driveclubsrv.exe`, siehe README) in einem Ordner `server` neben dem
  Spiel, mit `server-config.json` als `config.json`.
- **Wer nicht im selben Heimnetz ist:** ein gemeinsames VPN. Am schnellsten ist Tailscale (kostenlos, direkte
  Verbindungen); Hamachi und Radmin gehen auch.

## Einmalig einrichten (jeder)

1. Die Dateien aus `play/` und `override-0.tss` neben `DriveClub.exe` legen.
2. `Firewall-Freigabe.bat` doppelklicken, Admin-Frage mit Ja beantworten.
3. `DriveClub.exe` einmal starten, bei „Browse“ Update-Ordner (1.28, mit `eboot.bin`) und Basisspiel-Ordner
   (mit `game000.dat`) wählen. **Alle Spieler dieselbe feste Bildrate einstellen** (z. B. 60 FPS, „Unlocked“
   aus), dann schließen.
4. VPN: alle im selben Netz. Bei Tailscale lädt der Host die anderen unter *Users → Invite users* ein; sie wählen
   beim Anmelden das Tailnet des Hosts.

## Spielen

1. **Host:** `Online-Spielen.bat`, beim ersten Mal Name und `self` eingeben. Der Server startet, das Fenster zeigt
   deine IPs (Tailscale `100.x`, Hamachi `25.x`, Radmin `26.x`, Heimnetz `192.168.x`).
2. **Mitspieler:** `Online-Spielen.bat`, beim ersten Mal Name und die IP des Hosts eingeben, ins Hauptmenü gehen.
3. **Host:** Enter drücken – dein Spiel startet. Private Lobby erstellen, Mitspieler einladen, Event starten.

Wichtig: Das Spiel liest die Freundesliste nur beim Start. Darum erst die Mitspieler ins Spiel, dann der Host.

Name oder IP ändern: `online.ini` löschen.

## Wenn es hakt

- **„Server nicht erreichbar“:** VPN an? Host hat `Online-Spielen.bat` gestartet? Richtige IP? Firewall-Freigabe
  beim Host gemacht?
- **Mitspieler fehlt in der Freundesliste:** Host-Spiel neu starten, nachdem der Mitspieler im Menü ist.
- **„Cannot connect to players“:** Firewall-Freigabe bei beiden ausführen; bei Tailscale prüfen, dass beide im
  selben Tailnet sind (der Host-PC muss im Tailscale-Menü des Mitspielers auftauchen).
- **Rennzeiten oder Wetter laufen auseinander:** Jedes Spiel rechnet in seiner eigenen Geschwindigkeit, und die
  hängt an der Bildrate. Alle dieselbe feste Bildrate einstellen (z. B. 60 FPS, „Unlocked“ aus). Schafft ein PC
  die Bildrate nicht stabil, die nächstniedrigere für alle nehmen.
