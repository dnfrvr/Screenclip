# screenclip

Une barrette à écran : elle affiche en boucle les GIF d'une carte microSD sur un écran Waveshare ESP32-S3-LCD-3.16 (320 x 820).

## Installer

1. Télécharge ce dépôt (bouton vert « Code », puis « Download ZIP ») et extrais tout le zip.
2. Branche la barrette en USB.
3. Double-clique sur `ScreenClip/_MAC/flash_mac.command` ou `ScreenClip/_WINDOWS/flash_windows.bat`.

La première fois, il faut internet et quelques minutes : les outils s'installent tout seuls. Si la barrette n'est pas en mode programmation, la fenêtre explique le geste à faire avec les boutons BOOT et RST.

## Utiliser

Tout se fait avec le bouton BOOT :

| Geste | Action |
|---|---|
| Appui court | écran suivant |
| Deux appuis | écran précédent |
| Appui long (1 seconde) | ouvre les réglages |

Dans les réglages, un appui court passe à la ligne suivante et un appui long valide. La ligne Quitter les ferme, et ils se ferment seuls après 15 secondes.

| Réglage | Effet |
|---|---|
| Rotation automatique | l'image suit le sens de la barrette. Si elle est de travers dans une position, la ligne Tourner l'écran la corrige et la barrette retient cette position. |
| Tourner l'écran | revient sur l'image : chaque appui la tourne de 90°, un appui long valide. |
| Veille | après 2 minutes sans mouvement, une petite étoile se déplace sur fond noir avec un rétroéclairage faible. L'écran s'éteint 5 minutes plus tard. Un mouvement ou un appui le rallume. |
| Diaporama | change de GIF toutes les 30 s, 1 min ou 5 min. |

### Depuis un téléphone

Ouvre les réglages (appui long), choisis « Ajouter des écrans », puis suis ce qui s'affiche.

1. Scanne le premier QR code avec l'appareil photo et accepte le Wi-Fi proposé.
2. Scanne le second QR code : ton navigateur ouvre la page « Mes écrans ».
3. Ajoute une photo, une vidéo ou un texte, change l'ordre des écrans ou supprimes-en.

Rien à installer et pas besoin d'internet. Le Wi-Fi ne reste allumé que sur cette page. Un appui la ferme, et elle se ferme seule après 10 minutes sans téléphone connecté.

### Depuis un ordinateur

Pour ajouter des GIF, branche la barrette à un ordinateur : elle apparaît comme une clé USB. Copie tes `.gif` dans le dossier `gifs`, puis éjecte le disque.

- Taille conseillée : 410 x 160 pixels.
- 6 Mo maximum par GIF, 1024 pixels de large maximum.
- Carte microSD au format FAT32.

## Modifier

Le code est dans `ScreenClip/code/` (projet PlatformIO). Les réglages sont dans `ScreenClip/code/include/reglages.h` : relance ensuite le fichier d'installation.

| Dossier ou fichier | Contenu |
|---|---|
| `ScreenClip/code/src/main.cpp` | Carrousel, réglages, veille et boucle principale |
| `ScreenClip/code/include/` | Écran, rendu, menus, bouton, capteur, Wi-Fi et clé USB |
| `ScreenClip/code/web/` | Site du téléphone. Après un changement, lance `python3 ScreenClip/code/outils/make_web.py`. |
| `ScreenClip/code/outils/` | Génération des polices, du logo et du site, script de flash |
| `ScreenClip/code/tests/` | Tests de la logique de rotation, à lancer sur ordinateur (commande en tête du fichier) |

`fonts_data.h`, `logo_image.h` et `page_web.h` sont générés : ne les modifie pas à la main.

## Licence

Projet dérivé de [ScreenClip](https://github.com/orouuu/Screenclip), créé par Orou. CC BY-NC-SA 4.0, voir `ScreenClip/LICENSE`. Composants tiers dans `ScreenClip/NOTICE.md`.
