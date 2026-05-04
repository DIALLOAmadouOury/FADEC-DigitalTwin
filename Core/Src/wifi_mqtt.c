/**
 * @file    wifi_mqtt.c
 * @brief   Implémentation de la télémétrie IoT.
 */

#include "wifi_mqtt.h"
#include "engine_sim.h" // Pour lire myEngine
#include "cmsis_os.h"   // Pour osDelay
#include <stdio.h>      // Pour snprintf (formatage du texte)

// Remarque : Ces fonctions fictives (WIFI_Connect, MQTT_Publish) seront remplacées 
// plus tard par les vraies fonctions du driver Inventek de STMicroelectronics.

int8_t WifiMqtt_HardwareInit(void) 
{
    // Ici, nous ferons l'appel au pilote SPI de l'Inventek
    // Exemple : WIFI_Init();
    return 0; // Succès
}

void StartMqttTask(void *argument)
{
    char json_payload[128];
    int timer_fuite = 0; // Notre chronomètre

    for(;;)
    {
        if (myEngine.is_running)
        {
            timer_fuite++;
            
            // Au bout de 20 secondes (40 boucles de 500ms), on perce le tuyau !
            if (timer_fuite == 40) {
                myEngine.has_leak = 1; 
            }

            // On ajoute la variable "leak" dans le JSON pour que l'IA connaisse la vérité (le Label)
            snprintf(json_payload, sizeof(json_payload), 
                     "{\"rpm\": %.1f, \"temp\": %.1f, \"fuel\": %.1f, \"leak\": %d}", 
                     myEngine.current_rpm, 
                     myEngine.engine_temp, 
                     myEngine.fuel_flow,
                     myEngine.has_leak);

            printf("%s\r\n", json_payload); 
        }

        osDelay(500); 
    }
}