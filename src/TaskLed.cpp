#include "TaskLed.h"
#include "TaskConnect.h"

// Queue global para comunicação com a task do LED
QueueHandle_t ledQueue;

void taskLed(void *parameter)
{
    led_state_t currentState = LED_STATE_INITIALIZING;
    led_state_t newState;

    Serial.println("💡 TaskLed iniciada no Core " + String(xPortGetCoreID()));

    while (true)
    {
        // Verificar se há novo comando na queue (não-bloqueante)
        if (xQueueReceive(ledQueue, &newState, 0) == pdTRUE)
        {
            currentState = newState;
            Serial.println("💡 LED mudou para estado: " + String(currentState));
        }

        // Executar padrão baseado no estado atual
        switch (currentState)
        {
        case LED_STATE_INITIALIZING:
            // Pisca rápido durante inicialização
            digitalWrite(LED_CONNECT, HIGH);
            vTaskDelay(100 / portTICK_PERIOD_MS);
            digitalWrite(LED_CONNECT, LOW);
            vTaskDelay(100 / portTICK_PERIOD_MS);
            break;

        case LED_STATE_CONNECTED:
            // 3 piscadas rápidas + pausa longa
            for (int i = 0; i < 3; i++)
            {
                digitalWrite(LED_CONNECT, HIGH);
                vTaskDelay(100 / portTICK_PERIOD_MS);
                digitalWrite(LED_CONNECT, LOW);
                vTaskDelay(100 / portTICK_PERIOD_MS);
            }
            vTaskDelay(800 / portTICK_PERIOD_MS); // Pausa entre sequências
            break;

        case LED_STATE_DISCONNECTED:
            // LED fixo ligado
            digitalWrite(LED_CONNECT, HIGH);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            break;
        }
    }
}

void setLedState(led_state_t state)
{
    if (ledQueue != NULL)
    {
        // Enviar comando para a queue (não-bloqueante)
        xQueueSend(ledQueue, &state, 0);
    }
}
