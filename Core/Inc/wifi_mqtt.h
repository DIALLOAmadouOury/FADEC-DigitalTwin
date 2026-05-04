/**
 * @file    wifi_mqtt.h
 * @brief   Module de communication IoT (Wi-Fi Inventek + MQTT).
 */

#ifndef WIFI_MQTT_H
#define WIFI_MQTT_H

#include <stdint.h>

/**
 * @brief  Initialise la puce Inventek Wi-Fi matérielle.
 * @retval 0 si succès, -1 si erreur.
 */
int8_t WifiMqtt_HardwareInit(void);

/**
 * @brief  Tâche FreeRTOS gérant la connexion réseau et la publication des données.
 * @note   À faire tourner à basse fréquence (ex: 2 Hz) pour ne pas bloquer le système.
 */
void StartMqttTask(void *argument);

#endif /* WIFI_MQTT_H */