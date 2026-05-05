/**
 * @file    wifi_mqtt.c
 * @author  [Ton Nom / Ton Pseudo GitHub]
 * @brief   Implémentation de la surveillance FADEC par Intelligence Artificielle et communication MQTT.
 * @date    2026-05-05
 * 
 * @details 
 * Ce module est le cœur de la logique "Edge AI" du projet. Il gère une tâche FreeRTOS qui :
 *  1. Récupère les télémétries du Jumeau Numérique (Digital Twin) du moteur.
 *  2. Exécute une inférence locale via le modèle de réseau de neurones X-CUBE-AI.
 *  3. Formate les résultats en JSON et les publie via MQTT.
 * 
 * @note    Pour faciliter l'intégration continue (CI/CD) et les tests isolés, 
 *          la fonction de transport MQTT_Publish est implémentée ici sous forme de "Mock" (bouchon).
 */

#include "wifi_mqtt.h"    /* Définitions des tâches et initialisation HW */
#include "network.h"      /* API générée par X-CUBE-AI pour le modèle "network" */
#include "network_data.h" /* Poids et biais du réseau de neurones */
#include "engine_sim.h"   /* Structure de données du moteur simulé */
#include "cmsis_os.h"     // OS Temps Réel (FreeRTOS)
#include "ai_platform.h"  /* Types de base STMicroelectronics AI */
#include <stdio.h>
#include <string.h>

/* ============================================================================== */
/* === VARIABLES GLOBALES PRIVÉES (Ressources IA)                             === */
/* ============================================================================== */

/** @brief Pointeur opaque vers l'instance du réseau de neurones */
static ai_handle engine_ai_handler = AI_HANDLE_NULL;

/** @brief Mémoire de travail (RAM) allouée statiquement pour l'inférence IA */
static uint8_t activations[AI_NETWORK_DATA_ACTIVATIONS_SIZE];

/* ============================================================================== */
/* === HARDWARE ABSTRACTION LAYER (HAL) STUBS                                 === */
/* ============================================================================== */

/**
 * @brief  Initialise le matériel de communication (Bus SPI3 & Puce Inventek Wi-Fi).
 * @retval 0 en cas de succès, -1 en cas d'échec.
 */
int8_t WifiMqtt_HardwareInit(void) {
    /* TODO: Intégrer les appels aux drivers HAL_SPI_Transmit ici */
    printf("[HW] SPI3 Bus and Inventek Wi-Fi module initialized.\r\n");
    return 0; 
}

/**
 * @brief  Mock / Bouchon pour la publication MQTT.
 * @details Permet de compiler et valider la logique IA (Test-Driven Development) 
 *          sans dépendre de la connectivité réseau physique de la carte.
 * @param  topic   Le topic de destination (ex: "engine/telemetry").
 * @param  payload La chaîne de caractères formatée en JSON.
 * @retval 0 (Succès simulé).
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
 * @brief Tâche cyclique d'acquisition, d'inférence AI et de publication IoT.
 * @param argument Pointeur générique requis par l'API osThreadDef de FreeRTOS.
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
    /* 2. CONFIGURATION DES BUFFERS D'ENTRÉE/SORTIE DU MODÈLE                    */
    /* ------------------------------------------------------------------------- */
    float in_data[AI_NETWORK_IN_1_SIZE];   /* Entrées : [RPM, Temp, Fuel Flow] */
    float out_data[AI_NETWORK_OUT_1_SIZE]; /* Sortie  : [Probabilité de Fuite] */
    
    /* Wrappers ST pour mapper nos tableaux en RAM vers les entrées/sorties de l'IA */
    ai_buffer ai_input = AI_BUFFER_OBJ_INIT(AI_BUFFER_FORMAT_FLOAT, 1, 1, AI_NETWORK_IN_1_SIZE, 1, in_data);
    ai_buffer ai_output = AI_BUFFER_OBJ_INIT(AI_BUFFER_FORMAT_FLOAT, 1, 1, AI_NETWORK_OUT_1_SIZE, 1, out_data);

    char json_buffer[256];

    printf("[SYSTEM] FADEC Edge AI Task Started.\r\n");

    /* ------------------------------------------------------------------------- */
    /* 3. BOUCLE INFINIE DE LA TÂCHE (SUPER-LOOP)                                */
    /* ------------------------------------------------------------------------- */
    for(;;) {
        /* On exécute l'analyse uniquement si le moteur tourne */
        if (myEngine.is_running) {
            
            /* ÉTAPE A : Acquisition des données brutes */
            in_data[0] = myEngine.current_rpm;
            in_data[1] = myEngine.engine_temp;
            in_data[2] = myEngine.fuel_flow;

            /* ÉTAPE B : Inférence Edge AI (Temps de calcul < 1ms sur Cortex-M4) */
            if (ai_network_run(engine_ai_handler, &ai_input, &ai_output) > 0) {
                
                float leak_proba = out_data[0];
                uint8_t warning_flag = (leak_proba > 0.85f) ? 1 : 0; /* Seuil critique à 85% */

                /* ÉTAPE C : Sérialisation des données pour le cloud/cockpit */
                snprintf(json_buffer, sizeof(json_buffer), 
                         "{\"rpm\":%.1f,\"tmp\":%.1f,\"fuel\":%.1f,\"prob\":%.2f,\"warn\":%d}",
                         in_data[0], in_data[1], in_data[2], leak_proba, warning_flag);

                /* ÉTAPE D : Transmission via la couche d'abstraction matérielle */
                if (MQTT_Publish("engine/v1/telemetry", json_buffer) == 0) {
                    // Log UART optionnel pour le debug local
                    // printf("[AI STATUS] Proba: %.2f | Warn: %d\r\n", leak_proba, warning_flag);
                }
            }
        }
        
        /* Libération du CPU pour les autres tâches (Fréquence de boucle : 2 Hz) */
        osDelay(500); 
    }
}