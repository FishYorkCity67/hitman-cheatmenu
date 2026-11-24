# Cheat Menu für HITMAN World of Assassination

Ein Cheat-Menü für den Einzelspieler-Modus als Plugin für das [ZHMModSDK](https://github.com/OrfeasZ/ZHMModSDK).

## Funktionen

| Bereich | Cheats |
|---|---|
| **Start** | Kacheln für alle wichtigen Schalter und Sofort-Aktionen, Kennzahlen (aktive Cheats, Bewusstlose, Phantom-Treffer) |
| **Spieler** | Unverwundbar · Unsichtbar · Überwachungskameras aus · Auto-Heilung · Sofort heilen |
| **Waffen** | Unendlich Munition · Kein Nachladen · Perfekte Präzision · Kein Rückstoß · Schnellfeuer · One-Hit-Kill · Phantom-Munition für alle Waffen |
| **Inventar** | Alle Gegenstände des Spiels als Karten mit Bild und Namen, nach Art gefiltert und durchsuchbar. Ein Klick legt sie ins Inventar (oder vor dich), 1–10 Stück. Dazu die erfundene Waffe **Phantom X-47** |
| **Kräfte & NPCs** | Kraftstoß · Abheben · Alle k.o. · Alle entwaffnen · Alle ausschalten · NPCs einfrieren · Alle NPCs/Gegenstände zu mir. Reichweite, Stoßkraft und K.o.-Dauer einstellbar |
| **Bewegung** | Noclip mit einstellbarer Geschwindigkeit und Turbo · Teleport nach vorne · 4 Positions-Slots |
| **Welt** | Zeitlupe und Zeitraffer von 0,05x bis 100x (logarithmischer Regler, Schnellwahl bis 100x) |
| **Outfits** | Durchsuchbare Liste aller Outfits · Outfit vom nächsten NPC übernehmen |
| **Einstellungen** | 5 Akzentfarben · HUD mit aktiven Cheats · Übersicht der Tastenkürzel |

### Phantom X-47

Eine erfundene Pistole: Wer von ihr getroffen wird, stirbt sofort oder fällt bewusstlos um (wählbar: Tödlich, Bewusstlos, Zufall).
Im Spiel sieht sie aus wie eine vorhandene Pistole (die Karte zeigt, welche). Mit „Phantom-Munition für alle Waffen“
wirkt jede deiner Waffen so.

„Bewusstlos“ heißt: der NPC fällt um und bleibt liegen, solange die eingestellte Zeit läuft (bis „für immer“).

### Bilder im Inventar

Die Karten zeigen die echten Renderbilder der Gegenstände aus dem Spiel. Sie kommen aus dem Bildarchiv von HITMAPS
(`media.hitmaps.com/img/hitman3/unlockables/item_perspective_<ID>_0.jpg`), werden beim ersten Anzeigen im Hintergrund
geladen und unter `mods\CheatMenu\images\` gespeichert. Gibt es zu einem Gegenstand kein Bild, bleibt die gezeichnete
Silhouette. In den Einstellungen lässt sich das Nachladen aus dem Internet abschalten.

## Tastenkürzel

| Taste | Aktion |
|---|---|
| `^` (QWERTZ) / `~` | SDK-Overlay öffnen, dort **CHEAT MENU** anklicken |
| `Strg + G` | Unverwundbar |
| `Strg + I` | Unsichtbar |
| `Strg + B` | Noclip |
| `Strg + T` | Zeitmanipulation |
| `Strg + F` | Kraftstoß |
| `Strg + K` | Alle in der Nähe k.o. |
| `Strg + H` | Sofort heilen |
| `WASD` + `Shift` | Noclip-Steuerung |

## Bauen

Im SDK-Baum `D:\HitmanModding\ZHMModSDK` (MSVC-Umgebung, `~\.cargo\bin` im PATH):

```
cmake --build _build/x64-Release --target CheatMenu
```

Die DLL liegt dann in `_build\x64-Release\Mods\CheatMenu\CheatMenu.dll` und kommt nach `HITMAN 3\Retail\mods\`.
Die Version vor dem Umbau liegt unter `D:\HitmanModding\Backups\CheatMenu-2026-11-03\CheatMenu.dll`
(Quellcode der alten Version: `Documents\Claude\FishSMP\CheatMenu`).

## Hinweis

Nutze das Menü nur im Einzelspieler. Bei Elusive Targets, Bestenlisten oder im Multiplayer verfälschen Cheats Ergebnisse und können anderen gegenüber unfair sein.

## Lizenz

GPL-3.0 – siehe [LICENSE](LICENSE). Die Mod baut auf dem [ZHMModSDK](https://github.com/OrfeasZ/ZHMModSDK) auf, das ebenfalls unter der GPL-3.0 steht.

Copyright (C) 2026 FishYorkCity67. Inoffizielle Fan-Mod, nicht mit IO Interactive verbunden.

Gedacht nur für den Einzelspieler-Modus.
