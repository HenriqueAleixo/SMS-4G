#include <Arduino.h>

#define LED_CONNECT 14

// Configurações do modem
#define MODEM_RST 5
#define MODEM_PWRKEY 4
#define MODEM_POWER_ON 23
#define MODEM_TX 27
#define MODEM_RX 26

// Configurações do relé
const int RELAY_PIN = 21;

// Configuração da Serial do modem
HardwareSerial SerialAT(1);

// Mutex para controlar acesso à serial do modem
SemaphoreHandle_t serialMutex;

// Variáveis para SMS
String smsString = "";
String lastSender = "";

// Declaração das funções
void processSMSCommand();
bool sendSMS(String number, String message);
void diagnoseSMS(); // Nova função de diagnóstico
void taskConnect(void *parameter);
void taskSms(void *parameter);
String sendATCommand(String command, int timeout = 1000);

String sendATCommand(String command, int timeout)
{
  // Aguardar mutex para evitar conflitos
  if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(timeout)) == pdTRUE)
  {
    // Limpar buffer de entrada
    while (SerialAT.available())
    {
      SerialAT.read();
    }

    // Log do comando sendo enviado
    if (command != "AT") // Evitar spam do comando AT básico
    {
      Serial.println("📤 AT: " + command);
    }

    // Enviar comando
    SerialAT.println(command);

    // Aguardar resposta
    unsigned long startTime = millis();
    String response = "";
    bool foundEnd = false;

    while (millis() - startTime < timeout && !foundEnd)
    {
      if (SerialAT.available())
      {
        char c = SerialAT.read();
        response += c;

        // Verificar se chegou ao fim da resposta
        if (response.indexOf("OK\r\n") != -1 ||
            response.indexOf("ERROR\r\n") != -1 ||
            response.indexOf("OK\n") != -1 ||
            response.indexOf("ERROR\n") != -1)
        {
          foundEnd = true;
        }
      }
      else
      {
        delay(1);
      }
    }

    // Se não encontrou OK/ERROR, aguardar um pouco mais para respostas completas
    if (!foundEnd && response.length() > 0)
    {
      delay(100);
      while (SerialAT.available())
      {
        response += (char)SerialAT.read();
      }
    }

    // Log da resposta (apenas para comandos importantes)
    if (command != "AT" && response.length() > 0)
    {
      String cleanResponse = response;
      cleanResponse.trim();
      cleanResponse.replace("\r", "");
      cleanResponse.replace("\n", " ");
      Serial.println("📥 Resposta: " + cleanResponse);
    }

    // Liberar mutex
    xSemaphoreGive(serialMutex);
    return response;
  }
  else
  {
    Serial.println("❌ Timeout aguardando acesso à serial");
    return "";
  }
}

void setup()
{
  // Configurar pinos
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  pinMode(LED_CONNECT, OUTPUT);
  digitalWrite(LED_CONNECT, LOW);

  // Configurar pinos do modem
  pinMode(MODEM_PWRKEY, OUTPUT);
  pinMode(MODEM_RST, OUTPUT);
  pinMode(MODEM_POWER_ON, OUTPUT);

  digitalWrite(MODEM_PWRKEY, LOW);
  digitalWrite(MODEM_RST, HIGH);
  digitalWrite(MODEM_POWER_ON, HIGH);

  // Sequência de inicialização LED
  delay(500);
  digitalWrite(LED_CONNECT, HIGH);
  delay(500);
  digitalWrite(LED_CONNECT, LOW);
  delay(500);
  digitalWrite(LED_CONNECT, HIGH);
  delay(500);
  digitalWrite(LED_CONNECT, LOW);

  Serial.begin(115200);
  Serial.println("Iniciando sistema SMS com comandos AT diretos...");

  // Criar mutex para serial
  serialMutex = xSemaphoreCreateMutex();
  if (serialMutex == NULL)
  {
    Serial.println("❌ Erro ao criar mutex!");
    while (1) delay(1000);
  }

  // Inicializar Serial do modem
  SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);

  Serial.println("Inicializando modem A7672/A7608...");

  // Sequência de inicialização do modem
  digitalWrite(MODEM_POWER_ON, HIGH);
  delay(1000);

  // Power key pulse para ligar o modem
  digitalWrite(MODEM_PWRKEY, HIGH);
  delay(1000);
  digitalWrite(MODEM_PWRKEY, LOW);
  delay(2000);

  // Reset do modem
  digitalWrite(MODEM_RST, LOW);
  delay(500);
  digitalWrite(MODEM_RST, HIGH);
  delay(5000);

  Serial.println("Enviando comandos AT de teste...");

  // Teste básico de comunicação
  for (int i = 0; i < 10; i++)
  {
    String response = sendATCommand("AT", 1000);

    if (response.indexOf("OK") != -1)
    {
      Serial.println("Comunicação AT estabelecida!");
      break;
    }
    Serial.printf("Tentativa AT %d/10\n", i + 1);
  }

  // Configuração básica do modem
  Serial.println("Configurando modem...");

  sendATCommand("AT+CPIN?", 1000);
  sendATCommand("AT+CSQ", 1000);
  sendATCommand("AT+CREG?", 1000);

  // Verificar informações do modem
  Serial.println("Obtendo informações do modem...");
  sendATCommand("ATI", 1000);

  // Verificar SIM card
  Serial.println("Verificando SIM card...");
  String simResponse = sendATCommand("AT+CPIN?", 1000);

  if (simResponse.indexOf("READY") != -1)
  {
    Serial.println("SIM card OK!");
  }
  else
  {
    Serial.println("Problema com SIM card! Resposta: " + simResponse);
  }

  // Aguardar registro na rede
  Serial.println("Aguardando registro na rede...");
  for (int attempts = 0; attempts < 10; attempts++)
  {
    String response = sendATCommand("AT+CREG?", 2000);
    Serial.println("Status registro: " + response);

    if (response.indexOf(",1") != -1 || response.indexOf(",5") != -1)
    {
      Serial.println("Registrado na rede com sucesso!");
      String opResponse = sendATCommand("AT+COPS?", 1000);
      Serial.println("Operadora: " + opResponse);
      break;
    }

    if (attempts == 9)
    {
      Serial.println("Não foi possível registrar na rede!");
      Serial.println("Continuando mesmo assim...");
    }
    else
    {
      Serial.printf("Tentativa %d/10 - Aguardando...\n", attempts + 1);
      delay(3000);
    }
  }

  // Configurar modo SMS
  Serial.println("Configurando modo SMS...");

  sendATCommand("AT+CMGF=1", 1000);
  sendATCommand("AT+CSCS=\"GSM\"", 1000);
  sendATCommand("AT+CNMI=2,1,0,0,0", 1000);
  sendATCommand("AT+CPMS=\"SM\",\"SM\",\"SM\"", 1000);
  sendATCommand("AT+CMGD=1,4", 2000);

  // Verificar configurações
  sendATCommand("AT+CNMI?", 1000);
  sendATCommand("AT+CPMS?", 1000);

  Serial.println("=== SISTEMA SMS A7672/A7608 SIMPLIFICADO ===");
  Serial.println("✅ Sistema pronto para controle via SMS");
  Serial.println("✅ Comandos aceitos: LIGAR, DESLIGAR, STATUS");
  Serial.println("✅ Também aceita: ON, OFF, 1, 0, ?");
  Serial.println("✅ Sistema monitora SMS automaticamente");
  Serial.println("📱 Envie um SMS para testar o sistema!");
  Serial.println("💻 Digite HELP para comandos de teste");

  // Criar tasks essenciais
  xTaskCreate(taskConnect, "taskConnect", 1024 * 2, NULL, 1, NULL);
  xTaskCreate(taskSms, "taskSms", 1024 * 4, NULL, 5, NULL);
}

void loop()
{
  // Interface via Serial com buffer para comandos completos
  static String inputBuffer = "";

  if (Serial.available())
  {
    char c = Serial.read();

    if (c >= 32 && c <= 126) // Caracteres imprimíveis
    {
      Serial.print(c); // Echo do que foi digitado
      inputBuffer += c;
    }
    else if (c == '\n' || c == '\r') // Enter pressionado
    {
      Serial.println();

      if (inputBuffer.length() > 0)
      {
        String command = inputBuffer;
        command.trim();
        command.toUpperCase();
        inputBuffer = ""; // Limpar buffer

        Serial.println("📥 Comando: " + command);

        if (command.startsWith("SMS:"))
        {
          int firstColon = command.indexOf(':');
          int secondColon = command.indexOf(':', firstColon + 1);

          if (firstColon != -1 && secondColon != -1)
          {
            String number = command.substring(firstColon + 1, secondColon);
            String message = command.substring(secondColon + 1);

            Serial.println("📤 Enviando para " + number + ": " + message);
            if (sendSMS(number, message))
            {
              Serial.println("✅ SMS enviado!");
            }
            else
            {
              Serial.println("❌ Erro ao enviar");
            }
          }
          else
          {
            Serial.println("❌ Use: SMS:numero:mensagem");
          }
        }
        else if (command == "TEST")
        {
          Serial.println("🔍 Testando SMS...");
          String smsCheck = sendATCommand("AT+CMGL=\"ALL\"", 3000);
          Serial.println("Resultado: " + smsCheck);
        }
        else if (command == "DIAG")
        {
          diagnoseSMS();
        }
        else if (command == "TESTSMS")
        {
          String testNumber = "+5511987071003"; // Seu número com +
          String testMessage = "Teste ESP32 " + String(millis());
          Serial.println("🚀 Enviando SMS de teste para " + testNumber + "...");
          if (sendSMS(testNumber, testMessage))
          {
            Serial.println("✅ Teste concluído com sucesso!");
          }
          else
          {
            Serial.println("❌ Teste falhou!");
          }
        }
        else if (command == "QUICKSMS")
        {
          // Pausar tasks temporariamente para teste isolado
          Serial.println("🛑 Pausando tasks para teste isolado...");
          vTaskSuspend(NULL); // Suspender task atual não funciona, usar outro método
          
          Serial.println("🚀 Teste isolado de SMS...");
          String testNumber = "+5511987071003";
          String testMessage = "Teste isolado " + String(millis());
          
          if (sendSMS(testNumber, testMessage))
          {
            Serial.println("✅ Teste isolado OK!");
          }
          else
          {
            Serial.println("❌ Teste isolado falhou!");
          }
        }
        else if (command == "STATUS")
        {
          Serial.println("📊 Relé: " + String(digitalRead(RELAY_PIN) ? "LIGADO" : "DESLIGADO"));
          String net = sendATCommand("AT+CREG?", 1000);
          Serial.println("📶 Rede: " + net);
        }
        else if (command == "HELP")
        {
          Serial.println("=== COMANDOS ===");
          Serial.println("SMS:numero:mensagem - Enviar SMS");
          Serial.println("TESTSMS - Teste rápido de SMS");
          Serial.println("QUICKSMS - Teste SMS isolado");
          Serial.println("TEST - Ver SMS recebidos");
          Serial.println("DIAG - Diagnóstico SMS");
          Serial.println("STATUS - Status sistema");
          Serial.println("PING - Teste básico");
          Serial.println("HELP - Esta ajuda");
        }
        else if (command == "PING")
        {
          Serial.println("🏓 PONG!");
        }
        else if (command.length() > 0)
        {
          Serial.println("❌ Comando desconhecido");
          Serial.println("💡 Digite HELP");
        }
      }
    }
    else if (c == 8 || c == 127) // Backspace
    {
      if (inputBuffer.length() > 0)
      {
        inputBuffer.remove(inputBuffer.length() - 1);
        Serial.print("\b \b"); // Apagar caractere na tela
      }
    }
  }

  delay(10); // Pequeno delay para não sobrecarregar
}

// Task para verificar conexão com a rede
void taskConnect(void *parameter)
{
  while (true)
  {
    String response = sendATCommand("AT+CREG?", 1000);

    if (response.indexOf(",1") != -1 || response.indexOf(",5") != -1)
    {
      digitalWrite(LED_CONNECT, HIGH);
    }
    else
    {
      digitalWrite(LED_CONNECT, LOW);
    }

    vTaskDelay(30000 / portTICK_PERIOD_MS); // Verificar a cada 30s em vez de 10s
  }
}

// Task para verificar SMS recebidos
void taskSms(void *parameter)
{
  while (true)
  {
    // Verificar notificações do modem
    if (SerialAT.available())
    {
      String notification = SerialAT.readString();
      Serial.println("📨 Notificação: " + notification);

      // Verificar se é SMS novo (+CMTI)
      if (notification.indexOf("+CMTI:") != -1)
      {
        int commaPos = notification.lastIndexOf(",");
        if (commaPos != -1)
        {
          int smsIndex = notification.substring(commaPos + 1).toInt();
          Serial.println("📨 SMS índice: " + String(smsIndex));

          // Ler SMS específico
          String smsData = sendATCommand("AT+CMGR=" + String(smsIndex), 3000);

          if (smsData.indexOf("+CMGR:") != -1)
          {
            // Extrair remetente
            int phoneStart = smsData.indexOf("\"+") + 2;
            int phoneEnd = smsData.indexOf("\"", phoneStart);

            if (phoneStart > 1 && phoneEnd > phoneStart)
            {
              lastSender = smsData.substring(phoneStart, phoneEnd);
              
              // Garantir que o número tenha o formato correto (+55...)
              if (!lastSender.startsWith("+"))
              {
                lastSender = "+" + lastSender;
              }
              
              Serial.println("📞 De: " + lastSender);

              // Extrair mensagem
              int firstNewline = smsData.indexOf("\n");
              int secondNewline = smsData.indexOf("\n", firstNewline + 1);

              if (secondNewline > 0)
              {
                int msgStart = secondNewline + 1;
                int msgEnd = smsData.indexOf("\n", msgStart);
                if (msgEnd == -1)
                  msgEnd = smsData.length();

                smsString = smsData.substring(msgStart, msgEnd);
                smsString.trim();
                smsString.replace("\r", "");

                if (smsString.length() > 0)
                {
                  Serial.println("📄 Msg: " + smsString);
                  processSMSCommand();
                }
              }
            }
          }

          // Deletar SMS
          sendATCommand("AT+CMGD=" + String(smsIndex), 1000);
        }
      }
    }

    // Verificação periódica (30s)
    static unsigned long lastCheck = 0;
    if (millis() - lastCheck > 30000)
    {
      lastCheck = millis();
      Serial.println("🔍 Verificação periódica...");

      String allSms = sendATCommand("AT+CMGL=\"REC UNREAD\"", 3000);

      if (allSms.indexOf("+CMGL:") != -1)
      {
        Serial.println("📨 SMS não lidos encontrados");

        int pos = 0;
        while ((pos = allSms.indexOf("+CMGL:", pos)) != -1)
        {
          int lineEnd = allSms.indexOf("\n", pos);
          if (lineEnd != -1)
          {
            String smsLine = allSms.substring(pos, lineEnd);
            int firstComma = smsLine.indexOf(",");
            if (firstComma != -1)
            {
              int smsIndex = smsLine.substring(7, firstComma).toInt();
              Serial.println("📨 Processando: " + String(smsIndex));

              String smsData = sendATCommand("AT+CMGR=" + String(smsIndex), 3000);

              if (smsData.indexOf("+CMGR:") != -1)
              {
                int phoneStart = smsData.indexOf("\"+") + 2;
                int phoneEnd = smsData.indexOf("\"", phoneStart);

                if (phoneStart > 1 && phoneEnd > phoneStart)
                {
                  lastSender = smsData.substring(phoneStart, phoneEnd);
                  
                  // Garantir que o número tenha o formato correto (+55...)
                  if (!lastSender.startsWith("+"))
                  {
                    lastSender = "+" + lastSender;
                  }

                  int firstNewline = smsData.indexOf("\n");
                  int secondNewline = smsData.indexOf("\n", firstNewline + 1);

                  if (secondNewline > 0)
                  {
                    int msgStart = secondNewline + 1;
                    int msgEnd = smsData.indexOf("\n", msgStart);
                    if (msgEnd == -1)
                      msgEnd = smsData.length();

                    smsString = smsData.substring(msgStart, msgEnd);
                    smsString.trim();
                    smsString.replace("\r", "");

                    if (smsString.length() > 0)
                    {
                      Serial.println("📄 Msg periódica: " + smsString);
                      processSMSCommand();
                    }
                  }
                }

                sendATCommand("AT+CMGD=" + String(smsIndex), 1000);
              }
            }
          }
          pos = lineEnd + 1;
        }
      }
    }

    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

// Função para processar comandos SMS
void processSMSCommand()
{
  smsString.toUpperCase();
  smsString.trim();

  String response = "";

  // IMPORTANTE: Verificar DESLIGAR primeiro para evitar conflito com LIGAR
  if (smsString.indexOf("SATURNO,OFF") != -1 )
  {
    digitalWrite(RELAY_PIN, LOW);
    response = "Rele DESLIGADO";
    Serial.println("🔌 Relé desligado por " + lastSender);
  }
  else if (smsString.indexOf("SATURNO,ON") != -1)
  {
    digitalWrite(RELAY_PIN, HIGH);
    response = "Rele LIGADO";
    Serial.println("🔌 Relé ligado por " + lastSender);
  }
  else if (smsString.indexOf("SATURNO,MSA,12") != -1)
  {
    digitalWrite(RELAY_PIN, LOW);
    delay(10000);
    digitalWrite(RELAY_PIN, HIGH);
    response = "Comando MSA recebido";
    Serial.println("📊 Comando MSA recebido por " + lastSender);
  }
  else if (smsString.indexOf("SATURNO,COS") != -1)
  {
    bool relayState = digitalRead(RELAY_PIN);
    response = "Status: " + String(relayState ? "LIGADO" : "DESLIGADO");
    Serial.println("📊 Status solicitado por " + lastSender);
  }
  else
  {
    response = "Comandos: ON, OFF, COS";
    Serial.println("❌ Comando inválido de " + lastSender + ": " + smsString);
  }

  // Enviar resposta
  if (response.length() > 0 && lastSender.length() > 0)
  {
    if (sendSMS(lastSender, response))
    {
      Serial.println("✅ Resposta enviada para " + lastSender);
    }
    else
    {
      Serial.println("❌ Erro ao enviar resposta");
    }
  }
}

// Função para enviar SMS
bool sendSMS(String number, String message)
{
  Serial.println("📤 Enviando SMS para: " + number);

  // Obter controle exclusivo da serial
  if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(5000)) != pdTRUE)
  {
    Serial.println("❌ Não foi possível obter acesso à serial");
    return false;
  }

  // Limpar buffer serial antes de começar
  while (SerialAT.available())
  {
    SerialAT.read();
  }

  // Verificar se já está no modo texto
  SerialAT.println("AT+CMGF?");
  delay(100);
  
  String response = "";
  unsigned long startTime = millis();
  while (millis() - startTime < 1000)
  {
    if (SerialAT.available())
    {
      response += (char)SerialAT.read();
    }
  }
  
  Serial.println("� Status modo SMS: " + response);

  // Configurar modo texto se necessário
  if (response.indexOf("+CMGF: 1") == -1)
  {
    Serial.println("🔧 Configurando modo texto...");
    SerialAT.println("AT+CMGF=1");
    delay(1000);
    
    // Limpar resposta
    while (SerialAT.available())
    {
      SerialAT.read();
    }
  }
  else
  {
    Serial.println("✅ Modo texto já estava configurado");
  }

  Serial.println("✅ Iniciando envio SMS...");

  // Enviar comando CMGS
  String cmd = "AT+CMGS=\"" + number + "\"";
  SerialAT.println(cmd);
  Serial.println("📟 Comando: " + cmd);

  // Aguardar prompt '>' com timeout
  startTime = millis();
  String promptBuffer = "";
  bool promptReceived = false;

  while (millis() - startTime < 10000) // 10 segundos
  {
    if (SerialAT.available())
    {
      char c = SerialAT.read();
      promptBuffer += c;
      
      if (c == '>')
      {
        Serial.println("📨 Prompt '>' recebido!");
        promptReceived = true;
        break;
      }
      else if (promptBuffer.indexOf("ERROR") != -1)
      {
        Serial.println("❌ Erro no comando CMGS: " + promptBuffer);
        xSemaphoreGive(serialMutex);
        return false;
      }
    }
    delay(10);
  }

  if (!promptReceived)
  {
    Serial.println("❌ Timeout aguardando prompt '>'");
    Serial.println("📨 Buffer: " + promptBuffer);
    xSemaphoreGive(serialMutex);
    return false;
  }

  // Enviar mensagem + Ctrl+Z
  Serial.println("✅ Enviando: '" + message + "'");
  SerialAT.print(message);
  SerialAT.write(26); // Ctrl+Z
  
  // Aguardar confirmação +CMGS
  startTime = millis();
  String resultBuffer = "";
  
  while (millis() - startTime < 30000) // 30 segundos
  {
    if (SerialAT.available())
    {
      char c = SerialAT.read();
      resultBuffer += c;
      
      if (resultBuffer.indexOf("+CMGS:") != -1)
      {
        Serial.println("✅ SMS enviado com sucesso!");
        Serial.println("📱 " + number + " <- " + message);
        
        // Limpar buffer final
        while (SerialAT.available())
        {
          SerialAT.read();
        }
        
        xSemaphoreGive(serialMutex);
        return true;
      }
      else if (resultBuffer.indexOf("ERROR") != -1)
      {
        Serial.println("❌ Erro ao enviar SMS: " + resultBuffer);
        
        // Mostrar detalhes do erro CMS
        if (resultBuffer.indexOf("+CMS ERROR") != -1)
        {
          Serial.println("💡 Erro CMS detectado - possível problema com:");
          Serial.println("   - Formato do número: " + number);
          Serial.println("   - Créditos/saldo do SIM");
          Serial.println("   - Configuração da operadora");
        }
        
        xSemaphoreGive(serialMutex);
        return false;
      }
    }
    delay(100);
  }

  Serial.println("❌ Timeout aguardando confirmação +CMGS");
  Serial.println("📨 Buffer final: " + resultBuffer);
  xSemaphoreGive(serialMutex);
  return false;
}

// Função de diagnóstico SMS
void diagnoseSMS()
{
  Serial.println("=== DIAGNÓSTICO SMS ===");

  // 1. Verificar modo SMS
  Serial.println("🔍 1. Verificando modo SMS atual...");
  String modeCheck = sendATCommand("AT+CMGF?", 2000);

  // 2. Configurar modo texto se necessário
  Serial.println("🔧 2. Configurando modo texto...");
  String modeSet = sendATCommand("AT+CMGF=1", 2000);

  // 3. Verificar configuração de caracteres
  Serial.println("📝 3. Verificando charset...");
  String charsetCheck = sendATCommand("AT+CSCS?", 1000);

  // 4. Verificar configurações de notificação
  Serial.println("🔔 4. Verificando notificações SMS...");
  String notifCheck = sendATCommand("AT+CNMI?", 1000);

  // 5. Verificar armazenamento SMS
  Serial.println("💾 5. Verificando armazenamento...");
  String storageCheck = sendATCommand("AT+CPMS?", 1000);

  // 6. Verificar qualidade do sinal
  Serial.println("📶 6. Verificando sinal...");
  String signalCheck = sendATCommand("AT+CSQ", 1000);

  // 7. Verificar registro na rede
  Serial.println("🌐 7. Verificando rede...");
  String networkCheck = sendATCommand("AT+CREG?", 1000);

  // 8. Verificar centro de SMS
  Serial.println("📞 8. Verificando centro SMS...");
  String smscCheck = sendATCommand("AT+CSCA?", 2000);

  // 9. Teste básico de comunicação
  Serial.println("💬 9. Teste comunicação AT...");
  String atTest = sendATCommand("AT", 1000);

  Serial.println("=== FIM DIAGNÓSTICO ===");
  Serial.println("💡 Use este log para identificar problemas");
}