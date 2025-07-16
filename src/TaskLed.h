#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

// Estados do LED
typedef enum
{
    LED_STATE_DISCONNECTED, // LED fixo
    LED_STATE_CONNECTED,    // 3 piscadas + pausa
    LED_STATE_INITIALIZING  // Pisca rápido
} led_state_t;

// Queue para comunicação
extern QueueHandle_t ledQueue;

// Função da task
void taskLed(void *parameter);

// Função para enviar comando para o LED
void setLedState(led_state_t state);
