/**
 * @file    engine_sim.h
 * @brief   Interface du module Jumeau Numérique (Digital Twin).
 */

#ifndef ENGINE_SIM_H
#define ENGINE_SIM_H

#include <stdint.h>

/**
 * @struct  VirtualEngine_t
 * @brief   Représente l'état complet du moteur virtuel en mémoire.
 */
typedef struct {
    /* Entrées (Inputs) */
    float target_thrust;   /**< Consigne de poussée souhaitée (0.0 à 100.0 %) */
    float fuel_flow;       /**< Débit de carburant injecté (0.0 à 100.0) */
    float ambient_temp;    /**< Température d'admission d'air (°C) */
    
    /* Sorties Physiques (Outputs) */
    float current_rpm;     /**< Vitesse de rotation de la turbine (RPM) */
    float engine_temp;     /**< Température des gaz d'échappement (EGT en °C) */
    
    /* États du système */
    uint8_t is_running;    /**< Statut : 0 = Arrêté, 1 = En fonctionnement */
    uint8_t fault_code;    /**< Code d'erreur : 0 = Nominal, >0 = Panne détectée */
} VirtualEngine_t;

/**
 * @brief Instance globale du moteur. 
 *        Accessible par les autres modules (ex: IA ou Télémétrie).
 */
extern VirtualEngine_t myEngine;

/**
 * @brief  Initialise la structure du moteur avec ses valeurs par défaut (à froid).
 * @retval None
 */
void EngineSim_Init(void);

/**
 * @brief  Calcule l'évolution physique du moteur (Inertie, Température) sur un pas de temps.
 * @note   Cette fonction doit être appelée de manière périodique par l'OS temps réel.
 * @retval None
 */
void EngineSim_Update(void);

#endif /* ENGINE_SIM_H */