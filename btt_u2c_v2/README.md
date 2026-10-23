# Firmware candleLight pour l'adaptateur BTT U2C V2 (Voron)

État au 24/10/2026 : **en service sur la Voron** (flashé le 23/10 à 23:31, validé au banc MANU-FIXE puis sous
charge sur la machine : 0 erreur CAN).

## Pourquoi

Le firmware BTT d'origine (`U2C_V2_STM32G0B1.bin`, janvier 2023, sans suite) :

- laisse `can0` bloqué en `ERROR-WARNING` après une rafale d'erreurs : il ne signale au noyau le retour à
  l'état normal qu'en sortie de passive ou de bus-off ;
- n'expose pas les compteurs d'erreurs TEC/REC (pas de `berr-counter` dans `ip -details`) ;
- annonce une commande de terminaison logicielle (PA1) sans effet connu sur cette carte, d'où le trompeur
  `termination 0`.

## Origine du code

| Élément | Référence |
|---|---|
| Base | `candle-usb/candleLight_fw` master `85e5b33` (17/09/2026) |
| Fichiers de carte G0B1 | PR #139 de Marc Kleine-Budde, `8ae4a1c` (ouverte, fusionnable) |
| Brochage U2C V2 | firmware BTT `bigtreetech/candleLight_fw`, branche `stm32g0_support`, commit `b1ecdd26` |
| Ajout (ce dépôt) | `src/boards/g0b1-btt_u2c_v2.c`, bloc `BOARD_btt_u2c_v2` de `include/config.h`, cible CMake |

Dépôt `gitlab.ewlm.fr/emmanuel/u2c-candlelight`, branche `btt-u2c-v2` : historique candleLight officiel, puis
PR #139, puis le commit de la carte. `out/btt-u2c-v2.patch` reprend ce même diff (par rapport à `pr139`).

## Ce qui change par rapport au firmware BTT

| Point | BTT | Ce firmware |
|---|---|---|
| Retour warning → normal signalé | non | oui |
| Compteurs TEC/REC lisibles (GET_STATE) | non | oui |
| Erreurs de trame sur demande (`berr-reporting on`) | envoyées en continu | oui, sur demande |
| Reprise après bus-off (BUS_OFF_RECOVERY) | non | annoncée, inutilisée par le noyau 6.18 |
| Commande de terminaison logicielle | annoncée (PA1) | retirée, cavaliers physiques |
| Horloge FDCAN | 64 MHz (PCLK) | 40 MHz (PLLQ) |
| Horloge USB HSI48 | non synchronisée | synchronisée sur l'USB (CRS) |
| CAN-FD | non annoncé | annoncé (exigé par `m_can.c`), jamais activé : Klipper reste en CAN classique |
| Chaînes USB | `budgetcan` / `budgetcan gs_usb` | `BIGTREETECH` / `U2C V2 gs_usb` |
| VID:PID | 1d50:606f | inchangé |

Inchangé, à l'état électrique près : CAN sur FDCAN2 PB5/PB6 (AF3), PA0 maintenu haut, PA1 maintenu bas,
LED PB3/PB4 actives à l'état haut, un seul canal (`can0`). `can0.service` sur le Pi n'a pas à changer.

## Compilation (WSL Ubuntu-24.04)

```bash
# à la racine du dépôt
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi-gcc.cmake
make -C build btt_u2c_v2_fw          # -Wall -Wextra -Werror : 0 avertissement
(cd btt_u2c_v2 && sha256sum -c SHA256SUMS)
```

Paquets : `gcc-arm-none-eabi libnewlib-arm-none-eabi cmake dfu-util` (gcc 13.2.1, cmake 3.28.3).

## Vérifications faites sans matériel

- Table des vecteurs : SP `0x20024000` (haut de RAM), Reset `0x08004e59` (flash, Thumb).
- Taille : 28 204 o de flash, 7 796 o de RAM.
- Fonctions annoncées `0x635b3` : BERR_REPORTING, GET_STATE, TDC, BUS_OFF_RECOVERY présents ; TERMINATION absent.
- `fclk_can` = 40 000 000.
- Adresses présentes dans le code : FDCAN2 `0x40006800`, GPIOB `0x50000400`, CRS `0x40006C00`.
  La seule référence à FDCAN1 est le diviseur partagé CKDIV du HAL, qui reste à sa valeur de reset (÷1).

## Banc PC MANU-FIXE (fait le 23/10/2026)

Mise en place :

- **usbipd-win 5.3.0** passe l'USB à WSL (Ubuntu-24.04, noyau 6.18.33 avec `gs_usb`). La U2C est sur le
  port **7-1** : `usbipd bind --busid 7-1` (administrateur, fait une fois), puis
  `usbipd attach --wsl --busid 7-1` après chaque branchement ou redémarrage de la carte,
  `usbipd detach --busid 7-1` pour la rendre à Windows.
- En DFU (0483:df11), Windows prenait le pilote Guillemot du joystick Thrustmaster (`GuiSTDFUDev`).
  Le pilote ST `stm32bootloader.inf` (oem25.inf, WinUSB) est forcé pour cette seule instance
  `USB\VID_0483&PID_DF11\2046326F5841`, ce qui rend la carte visible par STM32CubeProgrammer.

État de la puce lu en DFU : STM32G0B1 (0x467), chargeur ROM 0x011a, RDP niveau 0, aucune zone WRP ou PCROP,
`nBOOT_SEL = 0` (démarrage piloté par la broche BOOT0).

Commandes utilisées (CLI = `STM32_Programmer_CLI.exe`, carte en DFU) :

```text
dfu-util -d 1d50:606f -e                                   (WSL : DFU par logiciel, sans bouton BOOT)
CLI -c port=USB1 -ob displ                                 (lecture des octets d'option)
CLI -c port=USB1 -u 0x08000000 0x20000 retour-arriere\dump-u2c-20261023.bin
CLI -c port=USB1 -d out\btt_u2c_v2_fw.bin 0x08000000 -v -s 0x08000000
```

Règles tirées de l'incident du 23/09 : copie de la flash d'abord, pas de démarrage (`-s` / `:leave`)
après une simple lecture, sortie du DFU uniquement après une écriture, sinon débrancher et rebrancher.

Résultats, cavalier 120R de la U2C **retiré** pendant tout le banc :

| Essai | BTT d'origine | Ce firmware |
|---|---|---|
| Copie de la flash | 19 668 premiers octets identiques au binaire BTT officiel, reste vierge | sans objet |
| Identité USB | `budgetcan` / `budgetcan gs_usb` | `BIGTREETECH` / `U2C V2 gs_usb`, même numéro de série |
| `ip -details` | `termination 0 [0, 120]`, horloge 64 MHz, pas de compteurs | `berr-counter tx rx`, horloge 40 MHz, plus de ligne terminaison |
| Bouclage interne, 10 000 trames | 10 000/10 000, ordre respecté, 0 erreur | identique |
| Interface active sans émission | 0 erreur, ERROR-ACTIVE | non refait |
| Une trame seule (pas d'ACK, bus non terminé) | ~216 000 erreurs de bourrage en 3 s envoyées en continu, ERROR-PASSIVE | `berr-reporting off` : 4 trames (changements d'état) ; `on` : ~148 000 erreurs de bourrage ; ERROR-WARNING, `berr-counter tx 0 rx 127` |
| DFU par logiciel (`dfu-util -e`) | fonctionne | fonctionne ; réécriture, vérification et redémarrage validés |

La trame seule ne reçoit jamais d'ACK et le bus non terminé revient trop lentement à l'état récessif :
l'adaptateur relit du dominant, perd l'arbitrage et ne voit que des erreurs de réception. Ça prouve que toute
la chaîne TX → transceiver → bus → RX fonctionne (une broche fausse donnerait des erreurs de bit en émission).

Le `candump` d'Ubuntu 24.04 affiche « Error class 0x288 is invalid » : il ne connaît pas `CAN_ERR_CNT`
(0x200). Le noyau décode correctement.

Le retour de warning à l'état normal ne se teste qu'avec un vrai partenaire sur le bus : sur la Voron.

## Retour arrière

`dfu-util -d 1d50:606f -e` (ou BOOT maintenu au branchement), puis
`CLI -c port=USB1 -d retour-arriere\U2C_V2_STM32G0B1.bin 0x08000000 -v -s 0x08000000`.
Le binaire BTT tient sur moins de pages que celui-ci : ajouter `-e all` avant `-d` pour ne pas laisser la queue
de ce firmware en flash.

## Sur la Voron

Rien à changer sur le Pi : même VID:PID, même numéro de série, `can0.service` inchangé. La terminaison a été
modifiée côté Voron le 23/10 en même temps : la prochaine impression évalue les deux changements ensemble.

À relever après remontage puis après la première impression : `ip -details -statistics link show can0`
(état et `berr-counter`, l'état doit revenir à ERROR-ACTIVE après une rafale) et les `canstat_*` des `Stats`
de klippy.log, à comparer au 23/10 : 15 à 46 erreurs par minute en impression. Activer `berr-reporting on`
(type de chaque erreur dans `candump -e`) demande une ligne de plus dans `can0.service`, à valider à part.
