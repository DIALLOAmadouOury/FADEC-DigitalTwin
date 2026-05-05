/**
 ******************************************************************************
 * @file    wifi_mqtt.c
 * @author  [Ton Nom / Ton Pseudo GitHub]
 * @version V1.0.0
 * @date    2026-05-05
 * @brief   Implémentation de la surveillance FADEC Edge AI avec télémétrie Wi-Fi.
 * 
 * @details 
 * Ce module combine l'inférence d'un réseau de neurones local (X-CUBE-AI) 
 * et la transmission de données IoT via la puce Wi-Fi Inventek ISM43362.
 * 
 * Fonctionnalités :
 * - Inférence Edge AI (détection d'anomalies de pression/carburant en temps réel).
 * - Connexion Wi-Fi WPA2.
 * - Implémentation d'un client MQTT complet "from scratch" via Sockets TCP.
 * - PUBLISH : Envoi de la télémétrie formatée en JSON vers HiveMQ.
 * - SUBSCRIBE : Abonnement et réception des ordres pour interaction bi-directionnelle.
 ******************************************************************************
 */

#include "wifi_mqtt.h"    /* Définitions des tâches et structures */
#include "wifi.h"         /* API Wi-Fi Officielle de STMicroelectronics */
#include "network.h"      /* API d'inférence générée par X-CUBE-AI */
#include "network_data.h" /* Poids et biais du modèle IA compressé */
#include "engine_sim.h"   /* Structure du Digital Twin (Jumeau Numérique) */
#include "cmsis_os.h"     /* OS Temps Réel (FreeRTOS API) */
#include "ai_platform.h"  /* Types de base IA STMicroelectronics */
#include "secrets.h"      /* Identifiants Wi-Fi sécurisés (Ignoré par git) */

#include <stdio.h>
#include <string.h>

/* ============================================================================== */
/* === VARIABLES GLOBALES PRIVÉES (Ressources IA & Réseau)                    === */
/* ============================================================================== */

/** @brief Pointeur de gestion vers l'instance du réseau de neurones en RAM */
static ai_handle engine_ai_handler = AI_HANDLE_NULL;

/** @brief Mémoire de travail (Activations) allouée pour l'inférence IA */
static uint8_t activations[AI_NETWORK_DATA_ACTIVATIONS_SIZE];

/** @brief Adresse IP du Broker MQTT distant */
uint8_t broker_ip[4];

/** @brief Identifiant du Socket TCP matériel utilisé par la puce Inventek */
uint32_t socket_id = 0; 


/* ============================================================================== */
/* === COUCHE RÉSEAU (WI-FI & TCP MQTT MANUEL)                                === */
/* ============================================================================== */

/**
 * @brief  Initialise le module Wi-Fi matériel et se connecte au point d'accès.
 * @retval 0 si la connexion a réussi, -1 en cas d'échec.
 */
int8_t WifiMqtt_HardwareInit(void) {
    uint8_t macAddress[6];
    uint8_t ipAddress[4];

    printf("\r\n[WIFI] Initialisation du module Inventek ISM43362...\r\n");
    
    if (WIFI_Init() != WIFI_STATUS_OK) {
        printf("[ERREUR] Defaillance d'initialisation SPI/Wi-Fi.\r\n");
        return -1;
    }
    
    WIFI_GetMAC_Address(macAddress, 6);
    printf("[WIFI] Module OK. MAC : %02X:%02X:%02X:%02X:%02X:%02X\r\n", 
           macAddress[0], macAddress[1], macAddress[2], 
           macAddress[3], macAddress[4], macAddress[5]);

    /* --- Identifiants Wi-Fi --- */
    char *test_ssid = "Aod";
    char *test_pass = "Diallo@2";
    
    printf("[WIFI] Tentative : SSID=[%s] PWD=[%s]\r\n", test_ssid, test_pass);
    
    if (WIFI_Connect(test_ssid, test_pass, WIFI_ECN_WPA2_PSK) != WIFI_STATUS_OK) {
        printf("[ERREUR] Echec WPA2. Le module ne parvient pas a s'associer.\r\n");
        return -1;
    }
    
    WIFI_GetIP_Address(ipAddress, 4);
    printf("[WIFI] Connecte ! IP : %d.%d.%d.%d\r\n", 
           ipAddress[0], ipAddress[1], ipAddress[2], ipAddress[3]);
           
    return 0;
}

/**
 * @brief  Ouvre une socket TCP et forge la trame MQTT CONNECT.
 * @retval 0 si connecté au Broker, -1 en cas d'erreur.
 */
int8_t MQTT_ConnectToBroker(void) {
    printf("[MQTT] Resolution DNS pour broker.hivemq.com...\r\n");
    
    if (WIFI_GetHostAddress("broker.hivemq.com", broker_ip, 4) != WIFI_STATUS_OK) {
        printf("[ERREUR MQTT] Serveur introuvable (Erreur DNS).\r\n");
        return -1;
    }

    printf("[MQTT] Ouverture Socket TCP vers %d.%d.%d.%d:1883...\r\n", 
           broker_ip[0], broker_ip[1], broker_ip[2], broker_ip[3]);
           
    if (WIFI_OpenClientConnection(socket_id, WIFI_TCP_PROTOCOL, "MQTT", broker_ip, 1883, 0) != WIFI_STATUS_OK) {
        printf("[ERREUR MQTT] Echec d'ouverture du Socket TCP.\r\n");
        return -1;
    }

    /* Trame MQTT CONNECT v3.1.1 */
    uint8_t connect_pkt[] = {
        0x10, 0x1B, // Control Header
        0x00, 0x04, 'M', 'Q', 'T', 'T', 
        0x04, 0x02, 0x00, 0x3C,         
        0x00, 0x0F, 'S','T','M','3','2','_','F','A','D','E','C','_','A','O','D'
    };

    uint16_t sent_len = 0;
    if (WIFI_SendData(socket_id, connect_pkt, sizeof(connect_pkt), &sent_len, 5000) != WIFI_STATUS_OK) {
        printf("[ERREUR MQTT] Echec de transmission de la requete CONNECT.\r\n");
        return -1;
    }
    
    printf("[MQTT] Handshake reussi. Connecte au Broker public HiveMQ !\r\n");
    return 0;
}

/**
 * @brief  S'abonne officiellement à un topic MQTT (Trame Hexadécimale) pour écouter les ordres.
 * @param  topic Le canal à écouter.
 * @retval 0 si réussi, -1 sinon.
 */
int8_t MQTT_Subscribe(const char* topic) {
    uint8_t sub_pkt[128];
    uint16_t topic_len = strlen(topic);
    uint16_t rem_len = 2 + 2 + topic_len + 1; // ID + Longueur + Topic + QoS
    
    uint16_t idx = 0;
    sub_pkt[idx++] = 0x82; // Commande SUBSCRIBE
    sub_pkt[idx++] = (uint8_t)rem_len;
    
    sub_pkt[idx++] = 0x00; // Packet ID MSB
    sub_pkt[idx++] = 0x01; // Packet ID LSB
    
    sub_pkt[idx++] = (topic_len >> 8) & 0xFF;
    sub_pkt[idx++] = topic_len & 0xFF;
    
    memcpy(&sub_pkt[idx], topic, topic_len);
    idx += topic_len;
    
    sub_pkt[idx++] = 0x00; // QoS 0
    
    uint16_t sent_len = 0;
    if (WIFI_SendData(socket_id, sub_pkt, idx, &sent_len, 2000) != WIFI_STATUS_OK) {
        printf("[ERREUR] Echec de l'abonnement MQTT.\r\n");
        return -1;
    }
    printf("[MQTT] Abonne avec succes au topic : %s\r\n", topic);
    return 0;
}

/**
 * @brief  Construit dynamiquement et transmet une trame MQTT PUBLISH (QoS 0).
 */
int8_t MQTT_Publish(const char* topic, const char* payload) {
    uint8_t pub_buf[256];
    uint16_t topic_len = strlen(topic);
    uint16_t payload_len = strlen(payload);
    
    uint16_t rem_len = 2 + topic_len + payload_len; 
    uint16_t idx = 0;

    pub_buf[idx++] = 0x30; 
    pub_buf[idx++] = (uint8_t)rem_len; 
    
    pub_buf[idx++] = (topic_len >> 8) & 0xFF; 
    pub_buf[idx++] = topic_len & 0xFF;        
    
    memcpy(&pub_buf[idx], topic, topic_len);
    idx += topic_len;

    memcpy(&pub_buf[idx], payload, payload_len);
    idx += payload_len;

    uint16_t sent_len = 0;
    if(WIFI_SendData(socket_id, pub_buf, idx, &sent_len, 2000) != WIFI_STATUS_OK) {
         printf("[MQTT TX] Perte de connexion lors de la publication.\r\n");
         return -1;
    }

    printf("[MQTT TX] %s\r\n", payload);
    return 0;
}

/* ============================================================================== */
/* === FONCTION DE RÉCEPTION DES ORDRES (BI-DIRECTIONNEL)                     === */
/* ============================================================================== */

/**
 * @brief  Écoute le socket TCP pour recevoir les ordres du Dashboard Python.
 * @details Modifie les paramètres physiques du Jumeau Numérique en temps réel.
 */
void CheckForIncomingCommands(void) {
    uint8_t rx_buffer[128];
    uint16_t rx_len = 0;
    
    /* Lecture non-bloquante du socket TCP (Timeout très court : 50ms) */
    if (WIFI_ReceiveData(socket_id, rx_buffer, sizeof(rx_buffer), &rx_len, 50) == WIFI_STATUS_OK) {
        if (rx_len > 0) {
            rx_buffer[rx_len] = '\0'; /* Sécuriser la fin de la chaîne */
            
            /* Recherche de la clé "leak_cmd" envoyée par le Dashboard Python */
            if (strstr((char*)rx_buffer, "\"leak_cmd\": 1") != NULL) {
                printf("\r\n[CMD RECUE] 🚨 INJECTION D'ANOMALIE DECLENCHEE 🚨\r\n");
                myEngine.has_leak = 1; /* Activation de la fuite dans le jumeau numérique */
            } 
            else if (strstr((char*)rx_buffer, "\"leak_cmd\": 0") != NULL) {
                printf("\r\n[CMD RECUE] 🛠️ REPARATION DU MOTEUR\r\n");
                myEngine.has_leak = 0; /* Désactivation de la fuite (Moteur sain) */
            }
        }
    }
}

/* ============================================================================== */
/* === TÂCHE PRINCIPALE FREERTOS (CORE LOGIC)                                 === */
/* ============================================================================== */

/**
 * @brief Tâche temps-réel principale (Thread).
 */
void StartMqttTask(void *argument) {
    ai_error err;
    
    /* --- 1. INITIALISATION DU MOTEUR D'INFÉRENCE (X-CUBE-AI) --- */
    err = ai_network_create(&engine_ai_handler, AI_NETWORK_DATA_CONFIG);
    if (err.type != AI_ERROR_NONE) {
        printf("[FATAL IA] Erreur de creation du modele.\r\n");
        osThreadTerminate(osThreadGetId());
    }

    const ai_network_params params = {
        AI_NETWORK_PARAMS_INIT(
            AI_NETWORK_DATA_WEIGHTS(ai_network_data_weights_get()),
            AI_NETWORK_DATA_ACTIVATIONS(activations)
        )
    };
    
    if (!ai_network_init(engine_ai_handler, &params)) {
        printf("[FATAL IA] Erreur d'initialisation des poids.\r\n");
        osThreadTerminate(osThreadGetId());
    }

    /* --- 2. INITIALISATION DU RÉSEAU (WI-FI + MQTT + ABONNEMENT) --- */
    if (WifiMqtt_HardwareInit() != 0) {
        printf("[FATAL RESEAU] Coupure module Wi-Fi.\r\n");
        osThreadTerminate(osThreadGetId());
    }
    
    if (MQTT_ConnectToBroker() != 0) {
        printf("[FATAL RESEAU] Impossible de joindre le cloud.\r\n");
        osThreadTerminate(osThreadGetId());
    }

    /* On s'abonne au canal pour recevoir les ordres Python */
    MQTT_Subscribe("stm32/aod/command");

    /* --- 3. PRÉPARATION DES TENSEURS D'ENTRÉE/SORTIE --- */
    float in_data[AI_NETWORK_IN_1_SIZE];   
    float out_data[AI_NETWORK_OUT_1_SIZE]; 
    
    ai_buffer ai_input = AI_BUFFER_OBJ_INIT(AI_BUFFER_FORMAT_FLOAT, 1, 1, AI_NETWORK_IN_1_SIZE, 1, in_data);
    ai_buffer ai_output = AI_BUFFER_OBJ_INIT(AI_BUFFER_FORMAT_FLOAT, 1, 1, AI_NETWORK_OUT_1_SIZE, 1, out_data);
    
    char json_buffer[256]; 

    printf("\r\n[SYSTEM] FADEC Edge AI & Telemetry Task Started & Ready.\r\n");

    /* --- 4. BOUCLE DE TRAITEMENT INFINIE --- */
    for(;;) {
        if (myEngine.is_running) {
            
            /* Etape A : Acquisition des données brutes */
            in_data[0] = myEngine.current_rpm;
            in_data[1] = myEngine.engine_temp;
            in_data[2] = myEngine.fuel_flow;

            /* Etape B : Exécution de l'inférence locale (Edge AI) */
            if (ai_network_run(engine_ai_handler, &ai_input, &ai_output) > 0) {
                
                float leak_proba = out_data[0];
                uint8_t warning_flag = (leak_proba > 0.85f) ? 1 : 0; 

                /* Etape C : Sérialisation JSON */
                snprintf(json_buffer, sizeof(json_buffer), 
                         "{\"rpm\":%.1f,\"tmp\":%.1f,\"fuel\":%.1f,\"prob\":%.2f,\"warn\":%d}",
                         in_data[0], in_data[1], in_data[2], leak_proba, warning_flag);

                /* Etape D : Transmission MQTT */
                MQTT_Publish("stm32/aod/fadec", json_buffer);
            }
        }
        
        /* Pause pour laisser le processeur respirer */
        osDelay(500); 
        
        /* Etape E : Vérification des ordres distants Python */
        CheckForIncomingCommands();
    }
}