# FADEC Edge AI : Jet Engine Digital Twin & Predictive Maintenance

> **Système embarqué IoT de bout en bout (Sim2Real)** combinant un Jumeau Numérique thermodynamique, un OS temps réel (FreeRTOS) et un réseau de neurones embarqué (TinyML) pour la détection d'anomalies en vol.

![Demo Video/GIF](lien_vers_ta_video_ou_gif_ici.gif)
*(Aperçu du Dashboard EICAS réagissant en temps réel à l'inférence de la carte STM32)*

## ✈️ Le Projet en 3 phrases (Contexte & Cas d'usage)

**Que fait ce projet ?** 
Il simule le fonctionnement d'un moteur d'avion (Turboréacteur) en temps réel et utilise l'Intelligence Artificielle pour détecter une panne critique avant qu'elle ne devienne catastrophique. 

**Comment ça marche dans la vraie vie ?**
1. Un microcontrôleur embarqué près du moteur analyse en direct les capteurs physiques (Vitesse de rotation N1, Température d'échappement EGT, Débit de carburant).
2. Au lieu d'envoyer toutes ces données dans le Cloud (ce qui est impossible en plein vol), une **IA ultra-légère** intégrée directement sur la puce croise ces paramètres complexes pour repérer une anomalie invisible à l'œil nu (ex: une fuite de carburant).
3. Si l'IA détecte un comportement suspect, elle alerte instantanément l'écran de contrôle du pilote (Dashboard).

Ce projet reproduit l'intégralité de cette chaîne critique : de la physique du moteur jusqu'à l'écran du cockpit.

---

## 🏗️ Architecture Système & Flux de Données

Le système est divisé en trois couches distinctes :

1. **Le Modèle Physique (Digital Twin) :** Exécuté sur STM32. Modèle mathématique empirique simulant l'inertie du rotor (N1) et la thermodynamique des gaz d'échappement (EGT).
2. **Le Cerveau IA (TinyML) :** Modèle Keras optimisé et converti via STM32 X-CUBE-AI. Tourne dans la boucle FreeRTOS avec une empreinte RAM < 2 Ko.
3. **Le Réseau & l'IHM (Dashboard) :** Client MQTT "Bare-metal" sur STM32 communiquant avec un Broker public, interfacé avec un EICAS (Engine-Indicating and Crew-Alerting System) codé en Python.

## 🚀 Défis Techniques Relevés (Deep Dive)

### 1. Inférence "Edge AI" & Gestion du Sim2Real Gap
* **Déséquilibre des classes (Class Imbalance) :** Entraînement du modèle Keras avec pondération algorithmique (`class_weight`) pour pénaliser les faux négatifs sur la classe minoritaire (anomalies).
* **Normalisation à la volée :** Implémentation manuelle en C de l'algorithme `MinMaxScaler` (Scikit-learn) pour garantir une correspondance mathématique parfaite entre l'entraînement Python et l'inférence STM32.
* **Sortie Probabiliste :** Utilisation d'une fonction d'activation Sigmoïde en sortie de réseau, permettant un seuillage dynamique de sécurité (Alerte si Probabilité > 50%).

### 2. Connectivité Bas Niveau & Contournement MQTT
* **Gestion mémoire FreeRTOS :** Allocation statique des tenseurs d'activation IA et des buffers TCP pour prévenir les dépassements de pile (*Stack Overflow*).
* **Custom Payload Scanner :** Développement d'un parseur MQTT `strncpy`-based pour contourner la terminaison de chaîne implicite (`0x00` / Null-terminator) présente dans les headers binaires MQTT v3.1.1, garantissant 100% de fiabilité dans la réception des ordres de commande.

### 3. Simulation Thermodynamique Temps Réel
* Calculs itératifs non-bloquants (sans fonctions mathématiques lourdes type `pow` ou `exp`) adaptés à l'ALU d'un microcontrôleur.
* Simulation réaliste de l'inertie de la turbine (Spindown) et de la dissipation thermique ambiante (Cooldown).

## 🛠️ Stack Technologique

**Hardware & Embarqué (C) :**
* STM32 (ARM Cortex-M)
* Inventek ISM43362 (Module Wi-Fi)
* FreeRTOS (Task Management & Delays)
* STM32CubeIDE & X-CUBE-AI

**Data Science & IA (Python) :**
* TensorFlow / Keras (Réseau de neurones séquentiel)
* Scikit-learn (Preprocessing & Data splitting)
* Pandas / NumPy

**Interface & Réseau (Python) :**
* CustomTkinter (UI Moderne / Light Mode)
* Paho-MQTT v2
* HiveMQ (Public Broker)

## ⚙️ Comment exécuter le projet

### 1. Dashboard Keras / Python
```bash
pip install customtkinter paho-mqtt
python dashboard_fadec.py
