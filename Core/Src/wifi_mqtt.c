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

    for(;;)
    {
        if (myEngine.is_running)
        {
            // 1. On fabrique le texte JSON
            snprintf(json_payload, sizeof(json_payload), 
                     "{\"rpm\": %.1f, \"temp\": %.1f, \"fuel\": %.1f}", 
                     myEngine.current_rpm, 
                     myEngine.engine_temp, 
                     myEngine.fuel_flow);

            // 2. On l'envoie sur le câble USB (qui affichera ça sur le PC)
            // Le \r\n permet de passer à la ligne suivante
            printf("%s\r\n", json_payload); 
        }

        osDelay(500); // 2 fois par seconde
    }
}