/**
 * @file    hardware_io.h
 * @brief   Interface d'Abstraction Matérielle (HAL Wrapper).
 */

#ifndef HARDWARE_IO_H
#define HARDWARE_IO_H

#include <stdint.h>

/**
 * @brief  Démarre les périphériques de la carte (ADC, Timers).
 * @retval None
 */
void HardwareIO_Init(void);

/**
 * @brief  Déclenche une conversion ADC et retourne la température ambiante de la carte.
 * @retval Température en degrés Celsius.
 */
float HardwareIO_ReadAmbientTemp(void);

/**
 * @brief  Modifie le rapport cyclique (PWM) du timer contrôlant la vanne physique.
 * @param  fuel_percentage Pourcentage d'ouverture demandé (0.0 à 100.0).
 * @retval None
 */
void HardwareIO_SetFuelValve(float fuel_percentage);

/**
 * @brief  Vérifie si une demande d'arrêt d'urgence a été émise (via bouton physique).
 * @retval 1 si arrêt d'urgence demandé, 0 sinon.
 */
uint8_t HardwareIO_IsEmergencyStopRequested(void);

/**
 * @brief  Acquitte l'arrêt d'urgence et réarme le système.
 * @retval None
 */
void HardwareIO_ResetEmergencyStop(void);

#endif /* HARDWARE_IO_H */