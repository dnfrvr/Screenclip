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

Sur la page Réglages, un appui passe à la ligne suivante et un appui long change la valeur ou ouvre la ligne choisie. Elle se ferme seule après 15 secondes.

| Réglage | Effet |
|---|---|
| Rotation automatique | l'image suit le sens de la barrette. Si elle est de travers dans une position, deux appuis la corrigent et la barrette retient cette position. |
| Veille | après 2 minutes sans mouvement, l'écran s'éteint et le GIF se met en pause. Un mouvement ou un appui le rallume. |
| Diaporama | change de GIF toutes les 30 s, 1 min ou 5 min. |

### Depuis un téléphone

Ouvre les réglages (appui de 2 secondes), choisis « Ajouter des écrans », puis suis ce qui s'affiche.

1. Scanne le premier QR code avec l'appareil photo et accepte le Wi-Fi proposé.
2. Scanne le second QR code : ton navigateur ouvre la page « Mes écrans ».
3. Ajoute une photo, une vidéo ou un texte, ou supprime un écran.

Rien à installer et pas besoin d'internet. Le Wi-Fi ne reste allumé que sur cette page. Un appui la ferme, et elle se ferme seule après 10 minutes sans téléphone connecté.

### Depuis un ordinateur

Pour ajouter des GIF, branche la barrette à un ordinateur : elle apparaît comme une clé USB. Copie tes `.gif` dans le dossier `gifs`, puis éjecte le disque.

- Taille conseillée : 410 x 160 pixels.
- 6 Mo maximum par GIF, 1024 pixels de large maximum.
- Carte microSD au format FAT32.

## Modifier

Le code est dans `code/` (projet PlatformIO). Le site du téléphone est dans `code/web/` : après un changement, lance `python3 code/outils/make_web.py`. Les réglages sont dans `code/include/reglages.h` : orientation, luminosité, durées. Relance ensuite le fichier d'installation.

## Licence

Projet dérivé de [ScreenClip](https://github.com/orouuu/Screenclip), créé par Orou. CC BY-NC-SA 4.0, voir `LICENSE`. Composants tiers dans `NOTICE.md`.
