/**
 * @file    wifi_mqtt.c
 * @brief   Implémentation de la télémétrie IoT.
 * @author  [Ton Nom]
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
    char json_payload[128]; // Chaîne de caractères qui contiendra notre message
    
    // 1. Connexion initiale (bloquante, mais ce n'est pas grave car c'est une tâche séparée)
    // WIFI_Connect("Ton_Reseau_WiFi", "Ton_Mot_de_Passe");
    // MQTT_Connect("broker.hivemq.com", 1883); // Un serveur MQTT gratuit pour les tests

    for(;;)
    {
        // 2. Si le moteur est allumé, on prépare le colis de données
        if (myEngine.is_running)
        {
            // Formatage en JSON. Exemple de résultat :
            // {"rpm":12050.5, "temp":450.2, "fuel":65.0}
            snprintf(json_payload, sizeof(json_payload), 
                     "{\"rpm\": %.1f, \"temp\": %.1f, \"fuel\": %.1f}", 
                     myEngine.current_rpm, 
                     myEngine.engine_temp, 
                     myEngine.fuel_flow);

            // 3. Envoi sur le réseau sur le "Topic" de ton avion
            // MQTT_Publish("avion/moteur_gauche/telemetrie", json_payload);
        }

        // 4. On attend 500 millisecondes (Fréquence de 2 Hz)
        // C'est largement suffisant pour de la télémétrie sur un tableau de bord Web.
        osDelay(500); 
    }
}