#include <Arduino.h>
#include "TaskConnect.h"
#include "TaskSms.h"
#include "TaskLed.h"
#include "DebugSerial.h"

#include "../lib/ATCommands/ATCommands.h"
#include "../lib/SaturnoCore/SaturnoCore.h"
#include "../lib/SaturnoModem/SaturnoModem.h"

// Configuração da Serial do modem
HardwareSerial SerialAT(1);
// Mutex para controlar acesso à serial do modem
SemaphoreHandle_t serialMutex;
// Variáveis para SMS
String smsString = "";
String lastSender = "";

ATCommands *at;
SaturnoCore *saturno;
SaturnoModem *modem;

void setup()
{
  // Configurar pinos
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  pinMode(LED_CONNECT, OUTPUT);
  digitalWrite(LED_CONNECT, LOW);
  Serial.begin(115200);
  Serial.println("Iniciando sistema SMS com comandos AT diretos...");

  // Criar mutex para serial (precisa ser antes de qualquer uso do ATCommands)
  serialMutex = xSemaphoreCreateMutex();
  if (serialMutex == NULL)
  {
    Serial.println("❌ Erro ao criar mutex!");
    while (1)
      delay(1000);
  }

  // Inicializar objetos das bibliotecas após o mutex
  modem = new SaturnoModem();

  // Inicialização completa do modem (cria objetos + hardware + serial + configuração)
  modem->fullInitialization(SerialAT, serialMutex, &at, &saturno);

  Serial.println("=== SISTEMA SMS A7672/A7608 SIMPLIFICADO ===");
  Serial.println("✅ Sistema pronto para controle via SMS");
  Serial.println("✅ Sistema monitora SMS automaticamente");
  Serial.println("📱 Envie um SMS para testar o sistema!");
  Serial.println("💻 Digite HELP para comandos de teste");
  // Criar tasks essenciais em cores separados (após inicialização completa)
  Serial.println("Criando tasks em cores separados...");

  // Criar queue para comunicação com LED (tamanho 5)
  ledQueue = xQueueCreate(5, sizeof(led_state_t));
  if (ledQueue == NULL)
  {
    Serial.println("❌ Erro ao criar queue do LED!");
    while (1)
      delay(1000);
  }

  // TaskLed no Core 0 - responsável apenas pelo LED (sem comandos AT)
  xTaskCreatePinnedToCore(taskLed, "taskLed", 1024 * 2, NULL, 2, NULL, 0);

  // TaskConnect no Core 1 - responsável por verificar conexão e comandar LED
  xTaskCreatePinnedToCore(taskConnect, "taskConnect", 1024 * 2, NULL, 1, NULL, 1);

  // TaskSms no Core 1 - responsável por comandos AT e SMS
  xTaskCreatePinnedToCore(taskSms, "taskSms", 1024 * 4, NULL, 5, NULL, 1);

  Serial.println("✅ Tasks criadas em cores separados!");
  Serial.println("💡 TaskLed -> Core 0 (sem AT)");
  Serial.println("📌 TaskConnect -> Core 1 (verifica conexão)");
  Serial.println("📌 TaskSms -> Core 1 (AT/SMS)");
}

void loop()
{
  debugPrint();
  delay(10); // Pequeno delay para não sobrecarregar
}
