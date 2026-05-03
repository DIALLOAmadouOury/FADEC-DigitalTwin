/**
 * @file    hardware_io.c
 * @brief   Interface matérielle (Hardware Abstraction Layer).
 * @note    C'est le SEUL fichier autorisé à appeler les fonctions HAL STMicroelectronics.
 *          Assure la portabilité du code de contrôle et du jumeau numérique.
 */

#include "hardware_io.h"
#include "main.h" 

// Variables globales générées par CubeMX
extern ADC_HandleTypeDef hadc1;
extern TIM_HandleTypeDef htim2;

// Drapeau d'interruption volatile (car modifié en asynchrone par l'EXTI)
static volatile uint8_t emergency_stop_flag = 0;

/**
 * @brief Démarre les périphériques analogiques et les timers.
 */
void HardwareIO_Init(void) 
{
    HAL_ADC_Start(&hadc1);
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
}

/**
 * @brief  Lit le capteur de température interne du MCU.
 * @retval Température approximée en degrés Celsius.
 */
float HardwareIO_ReadAmbientTemp(void) 
{
    HAL_ADC_Start(&hadc1);
    if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK) 
    {
        uint32_t raw_adc = HAL_ADC_GetValue(&hadc1);
        
        // Conversion ADC vers Tension (3.3V / 12 bits) puis vers Température
        float voltage = (float)raw_adc * (3.3f / 4095.0f);
        float temp = ((voltage - 0.76f) / 0.0025f) + 25.0f;
        
        return temp;
    }
    return 25.0f; // Valeur de sécurité si l'ADC échoue
}

/**
 * @brief Pilote le timer matériel pour générer un signal PWM.
 * @param fuel_percentage Valeur de commande (0.0 à 100.0).
 */
void HardwareIO_SetFuelValve(float fuel_percentage) 
{
    uint32_t ccr_value = (uint32_t)fuel_percentage;
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, ccr_value);
}

/**
 * @brief Vérifie si une interruption d'urgence a été levée.
 */
uint8_t HardwareIO_IsEmergencyStopRequested(void) 
{
    return emergency_stop_flag;
}

/**
 * @brief Acquitte l'arrêt d'urgence pour autoriser un redémarrage.
 */
void HardwareIO_ResetEmergencyStop(void) 
{
    emergency_stop_flag = 0;
}

/**
 * @brief  Callback système appelé automatiquement lors d'une interruption externe.
 * @note   Connecté au bouton bleu (PC13). Coupe la vanne physiquement par sécurité logicielle.
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == USER_BUTTON_Pin) 
    {
        emergency_stop_flag = 1;
        __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 0); // Action matérielle immédiate
    }
}