/**
 * @file    fadec_core.h
 * @brief   Interface du module de contrôle FADEC (Régulateur PID).
 */

#ifndef FADEC_CORE_H
#define FADEC_CORE_H

/**
 * @brief  Initialise le calculateur et réinitialise les mémoires d'erreur (Intégrale/Dérivée).
 * @retval None
 */
void Fadec_Init(void);

/**
 * @brief  Calcule la commande d'injection de carburant pour atteindre la vitesse cible.
 * @param  target_rpm  Consigne de vitesse désirée.
 * @param  current_rpm Vitesse réelle actuelle du système.
 * @retval Commande de la vanne de carburant (bornée entre 0.0 et 100.0 %).
 */
float Fadec_ComputeFuel(float target_rpm, float current_rpm);

#endif /* FADEC_CORE_H */