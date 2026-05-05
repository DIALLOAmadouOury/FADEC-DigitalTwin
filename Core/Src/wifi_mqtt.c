/**
 * @file    wifi_mqtt.c
 * @author  [Ton Nom / Ton Pseudo GitHub]
 * @brief   Implémentation de la surveillance FADEC par IA et connexion Wi-Fi.
 * @date    2026-05-05
 * 
 * @details 
 * Ce module gère la connexion au réseau Wi-Fi via la puce Inventek ISM43362
 * et exécute l'inférence locale via le modèle de réseau de neurones X-CUBE-AI.
 */

#include "wifi_mqtt.h"    /* Définitions des tâches */
#include "wifi.h"         /* API Wi-Fi Officielle de STMicroelectronics */
#include "network.h"      /* API générée par X-CUBE-AI */
#include "network_data.h" /* Poids et biais du réseau de neurones */
#include "engine_sim.h"   /* Structure de données du moteur simulé */
#include "cmsis_os.h"     /* OS Temps Réel (FreeRTOS) */
#include "ai_platform.h"  /* Types de base STMicroelectronics AI */
#include <stdio.h>
#include <string.h>

/* ============================================================================== */
/* === VARIABLES GLOBALES PRIVÉES (Ressources IA)                             === */
/* ============================================================================== */

/** @brief Pointeur vers l'instance du réseau de neurones */
static ai_handle engine_ai_handler = AI_HANDLE_NULL;

/** @brief Mémoire de travail (RAM) allouée pour l'inférence IA */
static uint8_t activations[AI_NETWORK_DATA_ACTIVATIONS_SIZE];


/* ============================================================================== */
/* === COUCHE RÉSEAU & MOCK MQTT                                              === */
/* ============================================================================== */

/**
 * @brief  Initialise le module Inventek (SPI) et se connecte à la Box Internet.
 * @retval 0 si connecté, -1 en cas d'erreur.
 */
int8_t WifiMqtt_HardwareInit(void) {
    uint8_t macAddress[6];
    uint8_t ipAddress[4];

    printf("\r\n[WIFI] Initialisation du module Inventek ISM43362...\r\n");

    /* 1. Initialisation de la puce via le bus SPI3 */
    if (WIFI_Init() != WIFI_STATUS_OK) {
        printf("[ERREUR] Impossible d'initialiser le module Wi-Fi.\r\n");
        return -1;
    }
    
    /* Ajout du paramètre '6' exigé par la nouvelle version du driver ST */
    WIFI_GetMAC_Address(macAddress, 6);
    printf("[WIFI] Module OK. MAC : %02X:%02X:%02X:%02X:%02X:%02X\r\n", 
           macAddress[0], macAddress[1], macAddress[2], 
           macAddress[3], macAddress[4], macAddress[5]);

    /* 2. Connexion au réseau */
    printf("[WIFI] Tentative de connexion au reseau...\r\n");
    
    if (WIFI_Connect("Aod", "Diallo@2", WIFI_ECN_WPA2_PSK) != WIFI_STATUS_OK) {
        printf("[ERREUR] Echec de la connexion Wi-Fi. (Mauvais mot de passe ou reseau introuvable ?)\r\n");
        return -1;
    }
    
    /* 3. Récupération de l'adresse IP (Ajout du paramètre '4') */
    WIFI_GetIP_Address(ipAddress, 4);
    printf("[WIFI] Connecte avec succes a Internet ! \r\n");
    printf("[WIFI] Adresse IP locale : %d.%d.%d.%d\r\n", 
           ipAddress[0], ipAddress[1], ipAddress[2], ipAddress[3]);

    return 0;
}

/**
 * @brief  Bouchon (Mock) pour la publication MQTT.
 * @details On l'utilise en attendant d'implémenter la vraie pile MQTT LwIP/coreMQTT.
 */
int8_t MQTT_Publish(const char* topic, const char* payload) {
    printf("\r\n[MQTT MOCK TX] >>>\r\n");
    printf("  ├─ Topic   : %s\r\n", topic);
    printf("  └─ Payload : %s\r\n", payload);
    return 0; 
}


/* ============================================================================== */
/* === TÂCHE PRINCIPALE FREERTOS (CORE LOGIC)                                 === */
/* ============================================================================== */

/**
 * @brief Tâche cyclique d'acquisition, d'inférence AI et de publication.
 */
void StartMqttTask(void *argument) {
    ai_error err;
    
    /* ------------------------------------------------------------------------- */
    /* 1. INITIALISATION DU MOTEUR D'INFÉRENCE (X-CUBE-AI)                       */
    /* ------------------------------------------------------------------------- */
    err = ai_network_create(&engine_ai_handler, AI_NETWORK_DATA_CONFIG);
    if (err.type != AI_ERROR_NONE) {
        printf("[ERROR] AI initialization failed. Terminating thread.\r\n");
        osThreadTerminate(osThreadGetId());
    }

    const ai_network_params params = {
        AI_NETWORK_PARAMS_INIT(
            AI_NETWORK_DATA_WEIGHTS(ai_network_data_weights_get()),
            AI_NETWORK_DATA_ACTIVATIONS(activations)
        )
    };

    if (!ai_network_init(engine_ai_handler, &params)) {
        printf("[ERROR] AI parameter linking failed. Terminating thread.\r\n");
        osThreadTerminate(osThreadGetId());
    }

    /* ------------------------------------------------------------------------- */
    /* 2. INITIALISATION MATÉRIELLE (WI-FI)                                      */
    /* ------------------------------------------------------------------------- */
    if (WifiMqtt_HardwareInit() != 0) {
        printf("[FATAL] Arret de la tache reseau suite a une erreur Wi-Fi.\r\n");
        osThreadTerminate(osThreadGetId());
    }

    /* ------------------------------------------------------------------------- */
    /* 3. CONFIGURATION DES BUFFERS D'ENTRÉE/SORTIE DU MODÈLE                    */
    /* ------------------------------------------------------------------------- */
    float in_data[AI_NETWORK_IN_1_SIZE];   /* Entrées : [RPM, Temp, Fuel Flow] */
    float out_data[AI_NETWORK_OUT_1_SIZE]; /* Sortie  : [Probabilité de Fuite] */
    
    ai_buffer ai_input = AI_BUFFER_OBJ_INIT(AI_BUFFER_FORMAT_FLOAT, 1, 1, AI_NETWORK_IN_1_SIZE, 1, in_data);
    ai_buffer ai_output = AI_BUFFER_OBJ_INIT(AI_BUFFER_FORMAT_FLOAT, 1, 1, AI_NETWORK_OUT_1_SIZE, 1, out_data);

    char json_buffer[256];

    printf("\r\n[SYSTEM] FADEC Edge AI Task Started & Ready.\r\n");

    /* ------------------------------------------------------------------------- */
    /* 4. BOUCLE INFINIE DE LA TÂCHE (SUPER-LOOP)                                */
    /* ------------------------------------------------------------------------- */
    for(;;) {
        if (myEngine.is_running) {
            
            /* Acquisition des données brutes */
            in_data[0] = myEngine.current_rpm;
            in_data[1] = myEngine.engine_temp;
            in_data[2] = myEngine.fuel_flow;

            /* Inférence Edge AI */
            if (ai_network_run(engine_ai_handler, &ai_input, &ai_output) > 0) {
                
                float leak_proba = out_data[0];
                uint8_t warning_flag = (leak_proba > 0.85f) ? 1 : 0;

                /* Sérialisation JSON */
                snprintf(json_buffer, sizeof(json_buffer), 
                         "{\"rpm\":%.1f,\"tmp\":%.1f,\"fuel\":%.1f,\"prob\":%.2f,\"warn\":%d}",
                         in_data[0], in_data[1], in_data[2], leak_proba, warning_flag);

                /* Publication MQTT (Mock pour le moment) */
                MQTT_Publish("engine/v1/telemetry", json_buffer);
            }
        }
        
        /* Pause de 500ms (Fréquence de boucle : 2 Hz) */
        osDelay(500); 
    }
}