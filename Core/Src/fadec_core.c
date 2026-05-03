/**
 * @file    fadec_core.c
 * @brief   Calculateur d'injection de carburant (Contrôleur PID).
 * @note    Cet algorithme compare la consigne pilote à l'état réel du moteur
 *          pour calculer la commande de la vanne (0 à 100%).
 */

#include "fadec_core.h"

/* --- Gains du Correcteur PID --- */
// À calibrer selon la réponse du jumeau numérique
#define PID_KP 0.05f  // Proportionnel : Réactivité immédiate
#define PID_KI 0.001f // Intégral : Annulation de l'erreur statique
#define PID_KD 0.01f  // Dérivé : Amortissement des dépassements (overshoot)

/* --- Limites de la vanne de carburant --- */
#define FUEL_MIN 0.0f
#define FUEL_MAX 100.0f

// Variables d'état persistantes du correcteur
static float integral_error = 0.0f;
static float previous_error = 0.0f;

/**
 * @brief Réinitialise la mémoire du régulateur (à appeler au démarrage du moteur).
 */
void Fadec_Init(void) 
{
    integral_error = 0.0f;
    previous_error = 0.0f;
}

/**
 * @brief  Calcule le pourcentage d'ouverture de la vanne de carburant.
 * @param  target_rpm  Vitesse cible demandée par le système
 * @param  current_rpm Vitesse réelle actuelle mesurée
 * @retval Commande de carburant bridée entre 0.0 et 100.0
 */
float Fadec_ComputeFuel(float target_rpm, float current_rpm) 
{
    float error = target_rpm - current_rpm;
    
    // Accumulation de l'erreur dans le temps (Action Intégrale)
    integral_error += error;
    
    // Protection Anti-Windup : Empêche l'intégrale d'exploser si le moteur bloque
    if (integral_error > 50000.0f) integral_error = 50000.0f;
    if (integral_error < -50000.0f) integral_error = -50000.0f;
    
    // Anticipation de la tendance (Action Dérivée)
    float derivative_error = error - previous_error;
    
    // Équation de commande PID
    float fuel_cmd = (PID_KP * error) + (PID_KI * integral_error) + (PID_KD * derivative_error);
    
    // Sauvegarde de l'erreur pour le prochain cycle
    previous_error = error;
    
    // Saturation physique de la sortie (on ne peut pas ouvrir la vanne à 110%)
    if (fuel_cmd > FUEL_MAX) return FUEL_MAX;
    if (fuel_cmd < FUEL_MIN) return FUEL_MIN;
    
    return fuel_cmd;
}