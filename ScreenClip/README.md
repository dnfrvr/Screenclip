# screenclip

![screenclip](screenclip.png)

Une barrette à écran : elle affiche en boucle les GIF d'une carte microSD sur un écran Waveshare ESP32-S3-LCD-3.16 (320 x 820).

## Installer

1. Télécharge ce dossier (bouton vert « Code », puis « Download ZIP ») et extrais tout le zip.
2. Branche la barrette en USB.
3. Double-clique sur `_MAC/flash_mac.command` ou `_WINDOWS/flash_windows.bat`.

La première fois, il faut internet et quelques minutes : les outils s'installent tout seuls. Si la barrette n'est pas en mode programmation, la fenêtre explique le geste à faire avec les boutons BOOT et RST.

## Utiliser

Tout se fait avec le bouton BOOT :

| Geste | Action |
|---|---|
| Un appui | image suivante |
| Appui long | image précédente |
| Deux appuis | tourne l'image de 90° |
| Appui de 2 secondes | ouvre ou ferme les réglages |

Sur la page Réglages, un appui passe à la ligne suivante et un appui long change la valeur. Elle se ferme seule après 15 secondes.

| Réglage | Effet |
|---|---|
| Rotation automatique | l'image suit le sens de la barrette. Si elle est de travers dans une position, deux appuis la corrigent et la barrette retient cette position. |
| Veille | après 2 minutes sans mouvement, l'écran s'éteint et le GIF se met en pause. Un mouvement ou un appui le rallume. |
| Diaporama | change de GIF toutes les 30 s, 1 min ou 5 min. |

Pour ajouter des GIF, branche la barrette à un ordinateur : elle apparaît comme une clé USB. Copie tes `.gif` dans le dossier `gifs`, puis éjecte le disque.

- Taille conseillée : 410 x 160 pixels.
- 6 Mo maximum par GIF, 1024 pixels de large maximum.
- Carte microSD au format FAT32.

## Modifier

Le code est dans `code/` (projet PlatformIO). Les réglages sont dans `code/include/reglages.h` : orientation, luminosité, durées, signature. Relance ensuite le fichier d'installation.

## Licence

CC BY-NC-SA 4.0, voir `LICENSE`. Composants tiers dans `NOTICE.md`.
