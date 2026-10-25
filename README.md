# FADEC Edge AI — Jumeau numérique et détection d'anomalies sur STM32

**C · STM32L4 · FreeRTOS · X-CUBE-AI · MQTT · Python**

## Présentation

Ce projet est un prototype de **surveillance d'un moteur aéronautique simulé sur microcontrôleur STM32**. Il associe un modèle simplifié de dynamique moteur, une régulation PID, un réseau de neurones exécuté localement et une transmission de télémétrie par MQTT.

L'objectif est d'étudier l'intégration de plusieurs fonctions embarquées sur une cible contrainte, depuis la simulation et la commande jusqu'à l'inférence et à la communication.

> **Périmètre :** il s'agit d'un projet expérimental de simulation, **pas** d'un véritable calculateur FADEC aéronautique ni d'un dispositif de sûreté certifié. Les grandeurs moteur proviennent principalement du modèle logiciel ; elles ne constituent pas des mesures effectuées sur un turboréacteur réel.

## Architecture

```text
              Consigne de poussée
                      |
                      v
               Régulateur PID
                (fadec_core.c)
                      |
                Débit carburant
                      |
                      v
               Modèle moteur
                (engine_sim.c)
                      |
            Régime / Température / Débit
                      |
                      v
             Prétraitement en C
                      |
                      v
          Réseau de neurones X-CUBE-AI
                      |
             Probabilité d'anomalie
                      |
                      v
             JSON -> MQTT / Wi-Fi
                      |
               Supervision distante
```

Une fuite de carburant peut être injectée dans le modèle pour modifier son comportement et tester la chaîne de détection. Les modules de commande, de simulation et de communication sont séparés dans le code.

## Fonctionnalités implémentées

### 1. Simulation moteur en C

Le fichier [`engine_sim.c`](Core/Src/engine_sim.c) implémente un modèle empirique :

- évolution du régime de rotation en fonction du débit de carburant et d'un terme de traînée ;
- échauffement et refroidissement de la température simulée ;
- décélération progressive lors de l'arrêt ;
- simulation d'une fuite réduisant le carburant effectivement utilisé.

Les paramètres de ce modèle sont des coefficients de simulation. Il ne s'agit pas d'une modélisation thermodynamique validée sur un moteur réel.

### 2. Régulation et interface matérielle

Le fichier [`fadec_core.c`](Core/Src/fadec_core.c) calcule une commande par **PID**, limitée entre 0 et 100 %, avec un bornage du terme intégral.

Le module [`hardware_io.c`](Core/Src/hardware_io.c) regroupe les appels aux périphériques STM32 : lecture ADC, commande par timer/PWM et prise en compte d'un bouton d'arrêt. L'interface PWM représente ici une commande de démonstration et non le pilotage d'une vanne aéronautique réelle.

### 3. Organisation sous FreeRTOS

Dans [`main.c`](Core/Src/main.c), plusieurs tâches sont créées avec des priorités différentes :

| Tâche | Fonction | Délai configuré |
| --- | --- | --- |
| `SimTask` | Mise à jour du modèle moteur | 10 ms |
| `FadecTask` | Régulation PID et interface matérielle | 20 ms |
| `MqttTask` | Inférence, publication et réception MQTT | 500 ms |

Ces valeurs sont les délais demandés par le programme ; **ce ne sont pas des mesures de temps d'exécution, de latence ou de jitter**.

### 4. Inférence IA sur STM32

Le fichier [`wifi_mqtt.c`](Core/Src/wifi_mqtt.c) prépare trois valeurs d'entrée (régime, température et débit), applique une normalisation en C et appelle le réseau généré par **STM32 X-CUBE-AI**.

La sortie du modèle est comparée à un seuil de 0,5 pour produire un indicateur d'alerte.

Selon le [rapport de génération X-CUBE-AI](X-CUBE-AI/App/network_generate_report.txt) fourni avec ce dépôt :

| Caractéristique du réseau | Valeur |
| --- | --- |
| Architecture | 3 entrées → 8 → 4 → 1 sortie |
| Format du modèle généré | **Float32** |
| Paramètres | 292 octets |
| Mémoire d'activation | 48 octets |
| Opérations | 95 MACC |

Les chiffres ci-dessus décrivent **le réseau généré**, pas la mémoire RAM/Flash totale du firmware ni une mesure de vitesse d'inférence sur carte.

### 5. Communication Wi-Fi / MQTT

Le code utilise le module Wi-Fi Inventek ISM43362 et réalise des échanges MQTT sur une connexion TCP :

- publication de télémétrie JSON (régime, température, débit, score et alerte) ;
- réception de commandes de démarrage, d'arrêt et d'injection de fuite ;
- échange avec un broker MQTT public dans la configuration de démonstration.

Le client MQTT est une implémentation expérimentale simplifiée. Il ne doit pas être considéré comme une pile MQTT complète ou sécurisée.

## Matériel et outils

- **Cible :** microcontrôleur STM32L475 (ARM Cortex-M4), module Wi-Fi Inventek ISM43362 ;
- **Firmware :** C, HAL STM32, FreeRTOS / CMSIS-RTOS2 ;
- **IA embarquée :** STM32 X-CUBE-AI, modèle TensorFlow Lite intégré ;
- **Réseau :** TCP, MQTT, messages JSON ;
- **Développement :** STM32CubeIDE / STM32CubeMX, GNU Arm Embedded Toolchain, GitHub Actions.

## Structure du dépôt

| Chemin | Contenu |
| --- | --- |
| `Core/Src/engine_sim.c` | Modèle moteur et simulation de panne |
| `Core/Src/fadec_core.c` | Correcteur PID |
| `Core/Src/hardware_io.c` | Abstraction des périphériques STM32 |
| `Core/Src/wifi_mqtt.c` | Wi-Fi, MQTT et inférence IA |
| `Core/Src/main.c` | Initialisation et tâches FreeRTOS |
| `Core/Inc/` | Interfaces et configuration |
| `X-CUBE-AI/App/` | Code du modèle généré et rapport technique |
| `FADEC_DigitalTwin.ioc` | Configuration STM32CubeMX |
| `.github/workflows/build.yml` | Compilation automatisée |

Le dépôt inclut également des composants et bibliothèques de STMicroelectronics et de l'écosystème STM32. Ils ne constituent pas tous du code développé spécifiquement pour ce projet.

## Compilation

Le projet fournit un fichier de configuration CubeMX et un `Makefile`. Il peut être ouvert avec l'environnement STM32 correspondant.

Pour compiler avec la chaîne GNU Arm Embedded correctement installée :

```bash
make -j
```

Un [workflow GitHub Actions](.github/workflows/build.yml) compile le firmware et archive les fichiers produits. Une compilation réussie ne remplace pas les essais de fonctionnement sur la carte.

**Configuration réseau :** le firmware public contient une ancienne configuration Wi-Fi de démonstration. Les identifiants doivent être remplacés et retirés de l'historique avant toute réutilisation. Ne pas réutiliser de mot de passe exposé publiquement.

## État du projet et pistes d'amélioration

**Présent dans ce dépôt :** firmware C, code généré X-CUBE-AI, modèle `.tflite`, configuration STM32CubeMX et automatisation de compilation.

**Non inclus actuellement :** scripts Python d'entraînement, tableau de bord de supervision et captures/vidéos de démonstration. Les commandes permettant de reproduire toute la chaîne sur PC ne sont donc pas encore documentées.

**Améliorations identifiées :**

- retirer les identifiants réseau du code et de l'historique Git ;
- ajouter des essais de validation du modèle moteur et de la régulation ;
- vérifier les échanges et le décodage MQTT sur plusieurs scénarios ;
- documenter l'entraînement et les résultats du modèle IA ;
- mesurer les temps de traitement, l'utilisation mémoire et le comportement temporel sur la cible ;
- ajouter une démonstration réelle du montage et de la supervision.

## Auteur

**Amadou Oury Diallo**  
Élève ingénieur en instrumentation et systèmes embarqués  
Sup Galilée — Université Sorbonne Paris Nord

[LinkedIn](https://www.linkedin.com/in/amadou-oury-diallo-0120961a1/)
