/**
 ******************************************************************************
 * @file    wifi_mqtt.c
 * @version V2.0.0 (Masterpiece TinyML)
 * @date    2026-05-06
 * @brief   Implémentation finale FADEC Edge AI : Inférence IA + Télémétrie MQTT.
 * 
 * @details 
 * Ce module est le cœur de l'application IoT/IA. Il combine :
 * 1. L'inférence d'un réseau de neurones local (X-CUBE-AI).
 * 2. La normalisation des données à la volée (équivalent MinMaxScaler).
 * 3. La transmission de données Wi-Fi via la puce Inventek ISM43362.
 * 4. Un client MQTT Bi-directionnel codé "from scratch" via Sockets TCP.
 ******************************************************************************
 */

#include "wifi_mqtt.h"    /* Définitions des tâches et structures */
#include "wifi.h"         /* API Wi-Fi Officielle de STMicroelectronics */
#include "network.h"      /* API d'inférence générée par X-CUBE-AI */
#include "network_data.h" /* Poids et biais du modèle IA compressé (Poids Flash) */
#include "engine_sim.h"   /* Structure du Digital Twin (Jumeau Numérique) */
#include "cmsis_os.h"     /* OS Temps Réel (FreeRTOS API) */
#include "ai_platform.h"  /* Types de base IA STMicroelectronics */

#include <stdio.h>
#include <string.h>

/* ============================================================================== */
/* === VARIABLES GLOBALES PRIVÉES (Ressources IA & Réseau)                    === */
/* ============================================================================== */

/** @brief Pointeur de gestion vers l'instance du réseau de neurones */
static ai_handle engine_ai_handler = AI_HANDLE_NULL;

/** @brief Mémoire de travail (Activations) allouée pour l'inférence IA dans la RAM */
static uint8_t activations[AI_NETWORK_DATA_ACTIVATIONS_SIZE];

/** @brief Adresse IP du Broker MQTT distant (ex: HiveMQ) */
uint8_t broker_ip[4];

/** @brief Identifiant du Socket TCP matériel utilisé par la puce Wi-Fi */
uint32_t socket_id = 0; 

/* ============================================================================== */
/* === COUCHE RÉSEAU (WI-FI & TCP MQTT MANUEL)                                === */
/* ============================================================================== */

/**
 * @brief  Initialise le module Wi-Fi matériel et s'associe au point d'accès WPA2.
 * @retval 0 si la connexion a réussi, -1 en cas d'échec matériel.
 */
int8_t WifiMqtt_HardwareInit(void) {
    uint8_t macAddress[6];
    uint8_t ipAddress[4];

    printf("\r\n[WIFI] Initialisation du module Inventek ISM43362...\r\n");
    if (WIFI_Init() != WIFI_STATUS_OK) return -1;
    
    WIFI_GetMAC_Address(macAddress, 6);
    printf("[WIFI] MAC : %02X:%02X:%02X:%02X:%02X:%02X\r\n", 
           macAddress[0], macAddress[1], macAddress[2], 
           macAddress[3], macAddress[4], macAddress[5]);

    /* Connexion Wi-Fi (Les identifiants peuvent être déportés dans un secrets.h) */
    if (WIFI_Connect("Aod", "Diallo@2", WIFI_ECN_WPA2_PSK) != WIFI_STATUS_OK) return -1;
    
    WIFI_GetIP_Address(ipAddress, 4);
    printf("[WIFI] Connecte ! IP : %d.%d.%d.%d\r\n", 
           ipAddress[0], ipAddress[1], ipAddress[2], ipAddress[3]);
    return 0;
}

/**
 * @brief  Ouvre une socket TCP et forge la trame binaire MQTT CONNECT.
 * @retval 0 si l'handshake MQTT est accepté par le Broker, -1 en cas d'erreur.
 */
int8_t MQTT_ConnectToBroker(void) {
    /* Résolution DNS pour trouver l'IP du Broker public HiveMQ */
    if (WIFI_GetHostAddress("broker.hivemq.com", broker_ip, 4) != WIFI_STATUS_OK) return -1;

    /* Ouverture d'un Socket TCP brut sur le port 1883 */
    if (WIFI_OpenClientConnection(socket_id, WIFI_TCP_PROTOCOL, "MQTT", broker_ip, 1883, 0) != WIFI_STATUS_OK) return -1;

    /* Trame Binaire MQTT CONNECT v3.1.1 (KeepAlive: 60s, CleanSession: 1) */
    uint8_t connect_pkt[] = {
        0x10, 0x1B, 0x00, 0x04, 'M', 'Q', 'T', 'T', 
        0x04, 0x02, 0x00, 0x3C, 0x00, 0x0F, 
        'S','T','M','3','2','_','F','A','D','E','C','_','A','O','D'
    };

    uint16_t sent_len = 0;
    WIFI_SendData(socket_id, connect_pkt, sizeof(connect_pkt), &sent_len, 5000);
    printf("[MQTT] Connecte au Broker HiveMQ !\r\n");
    return 0;
}

/**
 * @brief  S'abonne (SUBSCRIBE) à un topic MQTT pour écouter les ordres du Dashboard.
 * @param  topic Le canal MQTT à écouter (ex: "stm32/aod/command").
 * @retval 0 si réussi, -1 sinon.
 */
int8_t MQTT_Subscribe(const char* topic) {
    /* Utilisation de 'static' pour éviter un débordement de la pile FreeRTOS (Stack Overflow) */
    static uint8_t sub_pkt[128];
    uint16_t topic_len = strlen(topic);
    uint16_t rem_len = 5 + topic_len; 
    uint16_t idx = 0;

    sub_pkt[idx++] = 0x82; sub_pkt[idx++] = (uint8_t)rem_len; /* Header SUBSCRIBE */
    sub_pkt[idx++] = 0x00; sub_pkt[idx++] = 0x01;             /* Packet ID: 1 */
    sub_pkt[idx++] = (topic_len >> 8) & 0xFF; sub_pkt[idx++] = topic_len & 0xFF;
    memcpy(&sub_pkt[idx], topic, topic_len); idx += topic_len;
    sub_pkt[idx++] = 0x00; /* QoS 0 */

    uint16_t sent_len = 0;
    WIFI_SendData(socket_id, sub_pkt, idx, &sent_len, 2000);
    printf("[MQTT] Abonne au canal de commande.\r\n");
    return 0;
}

/**
 * @brief  Construit dynamiquement et transmet une trame MQTT PUBLISH (QoS 0).
 * @param  topic   Canal de destination.
 * @param  payload Chaine JSON contenant la télémétrie et le diagnostic IA.
 */
int8_t MQTT_Publish(const char* topic, const char* payload) {
    static uint8_t pub_buf[256]; /* Buffer statique pour soulager la Stack */
    uint16_t topic_len = strlen(topic);
    uint16_t payload_len = strlen(payload);
    uint16_t rem_len = 2 + topic_len + payload_len;
    uint16_t idx = 0;

    pub_buf[idx++] = 0x30; pub_buf[idx++] = (uint8_t)rem_len; /* Header PUBLISH */
    pub_buf[idx++] = (topic_len >> 8) & 0xFF; pub_buf[idx++] = topic_len & 0xFF;
    memcpy(&pub_buf[idx], topic, topic_len); idx += topic_len;
    memcpy(&pub_buf[idx], payload, payload_len); idx += payload_len;

    uint16_t sent_len = 0;
    if(WIFI_SendData(socket_id, pub_buf, idx, &sent_len, 2000) != WIFI_STATUS_OK) return -1;
    printf("[MQTT TX] %s\r\n", payload);
    return 0;
}

/* ============================================================================== */
/* === FONCTION DE RÉCEPTION DES ORDRES (BI-DIRECTIONNEL)                     === */
/* ============================================================================== */

/**
 * @brief  Écoute non-bloquante du socket TCP pour recevoir les ordres Python.
 * @details Modifie les paramètres physiques du Jumeau Numérique en temps réel.
 */
void CheckForIncomingCommands(void) {
    static uint8_t rx_buffer[128];
    uint16_t rx_len = 0;
    
    /* Lecture avec un Timeout très court (100ms) pour ne pas bloquer l'OS Temps Réel */
    if (WIFI_ReceiveData(socket_id, rx_buffer, sizeof(rx_buffer)-1, &rx_len, 100) == WIFI_STATUS_OK) {
        if (rx_len > 0) {
            rx_buffer[rx_len] = '\0'; /* Terminaison sécurisée de la chaîne */
            
            /* 1. Commande globale du moteur (Allumage / Extinction) */
            if (strstr((char*)rx_buffer, "engine_cmd") != NULL) {
                if (strstr((char*)rx_buffer, "1") != NULL) {
                    printf("\r\n[CMD] 🟢 DEMARRAGE DU MOTEUR\r\n");
                    myEngine.is_running = 1;
                } else if (strstr((char*)rx_buffer, "0") != NULL) {
                    printf("\r\n[CMD] 🔴 ARRET DU MOTEUR\r\n");
                    myEngine.is_running = 0;
                }
            }

            /* 2. Commande d'injection d'anomalie physique (Fuite Carburant) */
            if (strstr((char*)rx_buffer, "leak_cmd") != NULL) {
                if (strstr((char*)rx_buffer, "1") != NULL) {
                    printf("\r\n[CMD] 🚨 INJECTION DE FUITE\r\n");
                    myEngine.has_leak = 1;
                } else if (strstr((char*)rx_buffer, "0") != NULL) {
                    printf("\r\n[CMD] 🛠️ REPARATION\r\n");
                    myEngine.has_leak = 0;
                }
            }
        }
    }
}

/* ============================================================================== */
/* === TÂCHE PRINCIPALE FREERTOS (CORE LOGIC : IA + TELEMETRIE)               === */
/* ============================================================================== */

/**
 * @brief Tâche temps-réel principale (Thread).
 * @param argument Pointeur générique FreeRTOS (non utilisé).
 */
void StartMqttTask(void *argument) {
    ai_error err;
    
    /* Déclaration en 'static' vitale : prévient le Stack Overflow sous FreeRTOS */
    static float in_data[AI_NETWORK_IN_1_SIZE];   
    static float out_data[AI_NETWORK_OUT_1_SIZE]; 
    static char json_buffer[256];

    /* --- 1. Initialisation du moteur d'inférence (X-CUBE-AI) --- */
    err = ai_network_create(&engine_ai_handler, AI_NETWORK_DATA_CONFIG);
    const ai_network_params params = { AI_NETWORK_PARAMS_INIT(
        AI_NETWORK_DATA_WEIGHTS(ai_network_data_weights_get()),
        AI_NETWORK_DATA_ACTIVATIONS(activations)
    )};
    ai_network_init(engine_ai_handler, &params);

    /* --- 2. Initialisation du Réseau (Wi-Fi + MQTT + Abonnement) --- */
    WifiMqtt_HardwareInit();
    MQTT_ConnectToBroker();
    MQTT_Subscribe("stm32/aod/command"); /* Écoute des ordres Python */

    /* --- 3. Préparation des structures de Tenseurs I/O --- */
    ai_buffer ai_input = AI_BUFFER_OBJ_INIT(AI_BUFFER_FORMAT_FLOAT, 1, 1, AI_NETWORK_IN_1_SIZE, 1, in_data);
    ai_buffer ai_output = AI_BUFFER_OBJ_INIT(AI_BUFFER_FORMAT_FLOAT, 1, 1, AI_NETWORK_OUT_1_SIZE, 1, out_data);

    printf("\r\n[SYSTEM] FADEC Task Ready. En attente de demarrage distant...\r\n");
    
    /* Sécurité : le moteur jumeau démarre éteint et sain */
    myEngine.is_running = 0; 
    myEngine.has_leak = 0; 

    /* --- 4. Boucle infinie FreeRTOS --- */
    for(;;) {
        
        /* L'IA et la télémétrie ne calculent que si le moteur tourne */
        if (myEngine.is_running) {
            
            /* ================================================================== */
            /* ETAPE A : ACQUISITION ET NORMALISATION (Sim2Real Gap Fix)          */
            /* Pour éviter la saturation des neurones, on reproduit le            */
            /* comportement du MinMaxScaler (Python) en divisant par les MAX.     */
            /* ================================================================== */
            in_data[0] = myEngine.current_rpm / 7562.3f;  
            in_data[1] = myEngine.engine_temp / 695.3f;   
            in_data[2] = myEngine.fuel_flow / 100.0f;     

            /* ETAPE B : Exécution de l'inférence locale (Edge AI) */
            if (ai_network_run(engine_ai_handler, &ai_input, &ai_output) > 0) {
                
                /* Lecture de la couche de sortie (Sigmoid -> Probabilité de 0 à 1) */
                float leak_proba = out_data[0];
                uint8_t warning = (leak_proba > 0.5f) ? 1 : 0; /* Seuil critique à 50% */

                /* ETAPE C : Sérialisation JSON. 
                   NOTE : On envoie les VRAIES valeurs au Dashboard, pas les valeurs normalisées */
                snprintf(json_buffer, sizeof(json_buffer), 
                         "{\"rpm\":%.1f,\"tmp\":%.1f,\"fuel\":%.1f,\"prob\":%.2f,\"warn\":%d}",
                         myEngine.current_rpm, myEngine.engine_temp, myEngine.fuel_flow, leak_proba, warning);

                /* ETAPE D : Transmission asynchrone via TCP/MQTT */
                MQTT_Publish("stm32/aod/fadec", json_buffer);
            }
        }
        
        /* Délai non-bloquant pour libérer le CPU (Fréquence de boucle : ~2Hz) */
        osDelay(500); 
        
        /* Écoute constante des ordres (même quand le moteur est arrêté) */
        CheckForIncomingCommands(); 
    }
}