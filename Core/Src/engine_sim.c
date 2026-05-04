/**
 * @file    engine_sim.c
 * @brief   Modèle mathématique du Jumeau Numérique (Digital Twin).
 * @note    Ce module simule l'inertie de la turbine et la thermodynamique
 *          sans interagir avec le matériel (STM32). Il est purement mathématique.
 */

#include "engine_sim.h"

// Instance globale du moteur virtuel
VirtualEngine_t myEngine;

/* --- Paramètres Empiriques de Simulation (Calibrés Réalistes) --- */
#define SIM_ACCEL_FACTOR   2.5f     // Force de poussée de la combustion
#define SIM_DRAG_FACTOR    0.015f   // Frottements mécaniques et aérodynamiques
#define SIM_MAX_RPM        15000.0f // Plafond physique de la turbine
#define SIM_COAST_DRAG     0.02f    // Perte de vitesse en roue libre (moteur coupé)

/* --- NOUVELLE THERMODYNAMIQUE --- */
#define SIM_HEAT_FACTOR    1.2f     // Chaleur générée (Baissé drastiquement)
#define SIM_COOLING_RATE   0.1f     // Dissipation dynamique (Refroidit 2x plus vite)
#define SIM_AMBIENT_COOL   0.02f    // Refroidissement à l'arrêt

/**
 * @brief Initialise les paramètres du jumeau numérique à froid.
 */
void EngineSim_Init(void) 
{
    myEngine.target_thrust = 0.0f;
    myEngine.fuel_flow     = 0.0f;
    myEngine.ambient_temp  = 25.0f; // Température arbitraire avant lecture capteur
    myEngine.current_rpm   = 0.0f;
    myEngine.engine_temp   = 25.0f;
    myEngine.is_running    = 1;
    myEngine.fault_code    = 0;
}

/**
 * @brief Calcule un pas de temps physique du système.
 * @note  À appeler à fréquence fixe (ex: 100Hz) via une tâche FreeRTOS.
 */
void EngineSim_Update(void) 
{
    if (myEngine.is_running) 
    {
        /* 1. Dynamique du rotor : La vitesse dépend de l'accélération moins les frottements */
        float accel = myEngine.fuel_flow * SIM_ACCEL_FACTOR;
        float drag  = myEngine.current_rpm * SIM_DRAG_FACTOR;
        
        myEngine.current_rpm += (accel - drag);
        
        // Saturation physique pour éviter l'emballement mathématique
        if (myEngine.current_rpm > SIM_MAX_RPM) 
        {
            myEngine.current_rpm = SIM_MAX_RPM;
        }

        /* 2. Thermodynamique : La température dépend du feu et du flux d'air froid */
        float heat = myEngine.fuel_flow * SIM_HEAT_FACTOR;
        float cooling = (myEngine.engine_temp - myEngine.ambient_temp) * SIM_COOLING_RATE;
        
        myEngine.engine_temp += (heat - cooling);
    } 
    else 
    {
        /* 3. Séquence d'arrêt : Perte progressive d'inertie (Spindown) */
        if (myEngine.current_rpm > 10.0f) 
        {
            myEngine.current_rpm -= myEngine.current_rpm * SIM_COAST_DRAG;
        } 
        else 
        {
            myEngine.current_rpm = 0.0f;
        }
        
        /* Dissipation thermique naturelle (Cooldown) vers la température ambiante */
        if (myEngine.engine_temp > myEngine.ambient_temp) 
        {
            myEngine.engine_temp -= (myEngine.engine_temp - myEngine.ambient_temp) * SIM_AMBIENT_COOL;
        }
    }
}