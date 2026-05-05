/**
 * @file    wifi_mqtt.c
 * @brief   Implémentation de la surveillance FADEC par IA et communication MQTT.
 * 
 * @details Ce module respecte les interfaces définies dans wifi_mqtt.h et mqtt.h.
 *          Il utilise le modèle "network" généré par X-CUBE-AI.
 */

#include "wifi_mqtt.h"    /* Pour WifiMqtt_HardwareInit et StartMqttTask */
#include "mqtt.h"         /* Pour MQTT_Publish et les types associés */
#include "network.h"      /* Pour ai_network_run et les buffers IA */
#include "network_data.h"
#include "engine_sim.h"
#include "cmsis_os.h"
#include "ai_platform.h"
#include <stdio.h>
#include <string.h>

/* --- Variables Globales Privées --- */
static ai_handle engine_ai_handler = AI_HANDLE_NULL;
static uint8_t activations[AI_NETWORK_DATA_ACTIVATIONS_SIZE];

/* --- Implémentation des fonctions de wifi_mqtt.h --- */

/**
 * @brief Initialisation matérielle (Interface SPI3 / Wi-Fi).
 * @return 0 si succès, -1 si erreur.
 */
int8_t WifiMqtt_HardwareInit(void) {
    // Ton code spécifique pour réveiller le SPI3 et le module Wi-Fi
    printf("[HW] SPI3 & Wi-Fi Ready.\r\n");
    return 0; 
}

/**
 * @brief Tâche FreeRTOS : Acquisition -> Inférence AI -> MQTT Publish.
 */
void StartMqttTask(void *argument) {
    ai_error err;
    
    /* 1. Initialisation de l'IA (respect des prototypes de network.h) */
    err = ai_network_create(&engine_ai_handler, AI_NETWORK_DATA_CONFIG);
    if (err.type != AI_ERROR_NONE) {
        osThreadTerminate(osThreadGetId());
    }

    const ai_network_params params = {
        AI_NETWORK_PARAMS_INIT(
            AI_NETWORK_DATA_WEIGHTS(ai_network_data_weights_get()),
            AI_NETWORK_DATA_ACTIVATIONS(activations)
        )
    };

    if (!ai_network_init(engine_ai_handler, &params)) {
        osThreadTerminate(osThreadGetId());
    }

    /* Configuration des buffers pour ai_network_run */
    float in_data[AI_NETWORK_IN_1_SIZE];
    float out_data[AI_NETWORK_OUT_1_SIZE];
    ai_buffer ai_input = AI_BUFFER_OBJ_INIT(AI_BUFFER_FORMAT_FLOAT, 1, 1, AI_NETWORK_IN_1_SIZE, 1, in_data);
    ai_buffer ai_output = AI_BUFFER_OBJ_INIT(AI_BUFFER_FORMAT_FLOAT, 1, 1, AI_NETWORK_OUT_1_SIZE, 1, out_data);

    char json_buffer[256];

    for(;;) {
        if (myEngine.is_running) {
            /* Acquisition des données du Digital Twin */
            in_data[0] = myEngine.current_rpm;
            in_data[1] = myEngine.engine_temp;
            in_data[2] = myEngine.fuel_flow;

            /* Inférence IA locale */
            if (ai_network_run(engine_ai_handler, &ai_input, &ai_output) > 0) {
                float proba = out_data[0];
                int warn = (proba > 0.85f) ? 1 : 0;

                /* Sérialisation JSON */
                snprintf(json_buffer, sizeof(json_buffer), 
                         "{\"rpm\":%.1f,\"tmp\":%.1f,\"fuel\":%.1f,\"prob\":%.2f,\"warn\":%d}",
                         in_data[0], in_data[1], in_data[2], proba, warn);

                /* Publication MQTT (respecte exactement le prototype de mqtt.h) */
                if (MQTT_Publish("engine/telemetry", json_buffer) == 0) {
                    printf("[IOT] Telemetry sent via SPI3\r\n");
                }
            }
        }
        osDelay(500); /* 2 Hz */
    }
}