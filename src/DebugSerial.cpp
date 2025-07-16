#include "DebugSerial.h"

#include "../lib/ATCommands/ATCommands.h"
#include "../lib/SaturnoCore/SaturnoCore.h"

// Referências aos objetos globais do main.cpp
extern ATCommands* at;
extern SaturnoCore* saturno;

void debugPrint() {
  // Verificar se os objetos foram inicializados
  if (at == nullptr || saturno == nullptr) {
    return; // Sair silenciosamente se não inicializados
  }

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
            if (saturno->sendSMS(number, message))
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
          String smsCheck = at->sendATCommand("AT+CMGL=\"ALL\"", 3000);
          Serial.println("Resultado: " + smsCheck);
        }
        else if (command == "LISTALL")
        {
          Serial.println("📋 Listando TODAS as mensagens armazenadas...");
          String allSms = at->sendATCommand("AT+CMGL=\"ALL\"", 5000);
          Serial.println("=== TODAS AS MENSAGENS ===");
          Serial.println(allSms);
          Serial.println("=== FIM DA LISTA ===");
        }
        else if (command == "LISTUNREAD")
        {
          Serial.println("📬 Listando mensagens NÃO LIDAS...");
          String unreadSms = at->sendATCommand("AT+CMGL=\"REC UNREAD\"", 5000);
          Serial.println("=== MENSAGENS NÃO LIDAS ===");
          Serial.println(unreadSms);
          Serial.println("=== FIM DA LISTA ===");
        }
        else if (command == "LISTREAD")
        {
          Serial.println("📖 Listando mensagens LIDAS...");
          String readSms = at->sendATCommand("AT+CMGL=\"REC READ\"", 5000);
          Serial.println("=== MENSAGENS LIDAS ===");
          Serial.println(readSms);
          Serial.println("=== FIM DA LISTA ===");
        }
        else if (command == "CLEARSMS")
        {
          Serial.println("🗑️ Limpando TODAS as mensagens...");
          String result = at->sendATCommand("AT+CMGDA=\"DEL ALL\"", 3000);
          Serial.println("Resultado: " + result);
          if (result.indexOf("OK") != -1) {
            Serial.println("✅ Todas as mensagens foram deletadas!");
          } else {
            Serial.println("❌ Erro ao deletar mensagens");
          }
        }
        else if (command == "DIAG")
        {
          saturno->diagnoseSMS();
        }
        else if (command == "TESTSMS")
        {
          String testNumber = "+5511987071003"; // Seu número com +
          String testMessage = "Teste ESP32 " + String(millis());
          Serial.println("🚀 Enviando SMS de teste para " + testNumber + "...");
          if (saturno->sendSMS(testNumber, testMessage))
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
          
          if (saturno->sendSMS(testNumber, testMessage))
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
          String net = at->sendATCommand("AT+CREG?", 1000);
          Serial.println("📶 Rede: " + net);
        }
        else if (command == "HELP")
        {
          Serial.println("=== COMANDOS DE DEBUG ===");
          Serial.println("SMS:numero:mensagem - Enviar SMS");
          Serial.println("TESTSMS - Teste rápido de SMS");
          Serial.println("QUICKSMS - Teste SMS isolado");
          Serial.println("TEST - Ver SMS recebidos (comando bruto)");
          Serial.println("LISTALL - Listar TODAS as mensagens");
          Serial.println("LISTUNREAD - Listar mensagens NÃO LIDAS");
          Serial.println("LISTREAD - Listar mensagens LIDAS");
          Serial.println("CLEARSMS - Deletar TODAS as mensagens");
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
}