#include "TaskConnect.h"
#include "TaskLed.h"
#include "../lib/ATCommands/ATCommands.h"
#include <Arduino.h>

// Usa a instância global do main.cpp via extern (como ponteiro)
extern ATCommands *at;

void taskConnect(void *parameter)
{
  bool lastConnectionState = false;
  unsigned long lastStateCheck = 0;

  Serial.println("🔧 TaskConnect iniciada no Core " + String(xPortGetCoreID()));

  while (true)
  {
    // Verificar se o objeto at foi inicializado
    if (at == nullptr)
    {
      setLedState(LED_STATE_INITIALIZING);
      vTaskDelay(1000 / portTICK_PERIOD_MS);
      continue;
    }

    // Verificar conexão a cada 10 segundos
    if (millis() - lastStateCheck > 30000)
    {
      Serial.println("🔍 Verificando conexão...");
      String response = at->sendATCommand("AT+CREG?", 1000);
      Serial.println("📡 Resposta: " + response);

      bool currentConnection = (response.indexOf(",1") != -1 || response.indexOf(",5") != -1);

      if (currentConnection != lastConnectionState)
      {
        Serial.println(currentConnection ? "🔗 Conexão estabelecida" : "❌ Conexão perdida");
        lastConnectionState = currentConnection;

        // Enviar novo estado para o LED
        if (currentConnection)
        {
          setLedState(LED_STATE_CONNECTED);
        }
        else
        {
          setLedState(LED_STATE_DISCONNECTED);

          // Tentar forçar reconexão quando perde conexão
          Serial.println("🔄 Tentando forçar reconexão...");

          // 1. Resetar função RF
          Serial.println("📡 Resetando RF...");
          at->sendATCommand("AT+CFUN=0", 3000); // Desligar RF
          vTaskDelay(2000 / portTICK_PERIOD_MS);
          at->sendATCommand("AT+CFUN=1", 5000); // Ligar RF
          vTaskDelay(3000 / portTICK_PERIOD_MS);

          // 2. Forçar busca manual de operadora
          Serial.println("🔍 Buscando operadoras...");
          at->sendATCommand("AT+COPS=0", 10000); // Registro automático
          vTaskDelay(5000 / portTICK_PERIOD_MS);

          // 3. Forçar modo de rede
          Serial.println("📶 Configurando modo de rede...");
          at->sendATCommand("AT+CNMP=2", 2000); // Automático
          at->sendATCommand("AT+CMNB=1", 2000); // CAT-M
          vTaskDelay(3000 / portTICK_PERIOD_MS);

          // 4. Verificar resultado
          String checkResponse = at->sendATCommand("AT+CREG?", 2000);
          Serial.println("🔍 Resultado reconexão: " + checkResponse);

          Serial.println("✅ Tentativa de reconexão concluída");
        }
      }
      lastStateCheck = millis();
    }

    // Apenas esperar - LED é controlado pela outra task
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}
