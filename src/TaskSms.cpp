#include "TaskSms.h"
#include "../lib/ATCommands/ATCommands.h"
#include "../lib/SaturnoCore/SaturnoCore.h"
#include <Arduino.h>

// Usa as instâncias globais do main.cpp via extern (como ponteiros)
extern ATCommands* at;
extern SaturnoCore* saturno;
extern HardwareSerial SerialAT;
extern String smsString;
extern String lastSender;

void taskSms(void *parameter) {
  while (true) {
    // Verificar se os objetos foram inicializados
    if (at == nullptr || saturno == nullptr) {
      vTaskDelay(100 / portTICK_PERIOD_MS);
      continue;
    }
    
    // Verificar notificações do modem
    if (SerialAT.available()) {
      String notification = SerialAT.readString();
      Serial.println("\U0001F4E8 Notificação: " + notification);

      // Verificar se é SMS novo (+CMTI)
      if (notification.indexOf("+CMTI:") != -1) {
        int commaPos = notification.lastIndexOf(",");
        if (commaPos != -1) {
          int smsIndex = notification.substring(commaPos + 1).toInt();
          Serial.println("\U0001F4E8 SMS índice: " + String(smsIndex));

          // Ler SMS específico
          String smsData = at->sendATCommand("AT+CMGR=" + String(smsIndex), 3000);

          if (smsData.indexOf("+CMGR:") != -1) {
            // Extrair remetente
            int phoneStart = smsData.indexOf("\"+") + 2;
            int phoneEnd = smsData.indexOf("\"", phoneStart);

            if (phoneStart > 1 && phoneEnd > phoneStart) {
              lastSender = smsData.substring(phoneStart, phoneEnd);
              if (!lastSender.startsWith("+")) {
                lastSender = "+" + lastSender;
              }
              Serial.println("\U0001F4DE De: " + lastSender);

              // Extrair mensagem
              int firstNewline = smsData.indexOf("\n");
              int secondNewline = smsData.indexOf("\n", firstNewline + 1);

              if (secondNewline > 0) {
                int msgStart = secondNewline + 1;
                int msgEnd = smsData.indexOf("\n", msgStart);
                if (msgEnd == -1)
                  msgEnd = smsData.length();

                smsString = smsData.substring(msgStart, msgEnd);
                smsString.trim();
                smsString.replace("\r", "");

                if (smsString.length() > 0) {
                  Serial.println("\U0001F4C4 Msg: " + smsString);
                  saturno->processSMSCommand(smsString, lastSender);
                }
              }
            }
          }
          // Deletar SMS
          at->sendATCommand("AT+CMGD=" + String(smsIndex), 1000);
        }
      }
    }

    // Verificação periódica (30s)
    static unsigned long lastCheck = 0;
    if (millis() - lastCheck > 30000) {
      lastCheck = millis();
      Serial.println("\U0001F50D Verificação periódica...");

      String allSms = at->sendATCommand("AT+CMGL=\"REC UNREAD\"", 3000);

      if (allSms.indexOf("+CMGL:") != -1) {
        Serial.println("\U0001F4E8 SMS não lidos encontrados");
        int pos = 0;
        while ((pos = allSms.indexOf("+CMGL:", pos)) != -1) {
          int lineEnd = allSms.indexOf("\n", pos);
          if (lineEnd != -1) {
            String smsLine = allSms.substring(pos, lineEnd);
            int firstComma = smsLine.indexOf(",");
            if (firstComma != -1) {
              int smsIndex = smsLine.substring(7, firstComma).toInt();
              Serial.println("\U0001F4E8 Processando: " + String(smsIndex));
              String smsData = at->sendATCommand("AT+CMGR=" + String(smsIndex), 3000);
              if (smsData.indexOf("+CMGR:") != -1) {
                int phoneStart = smsData.indexOf("\"+") + 2;
                int phoneEnd = smsData.indexOf("\"", phoneStart);
                if (phoneStart > 1 && phoneEnd > phoneStart) {
                  lastSender = smsData.substring(phoneStart, phoneEnd);
                  if (!lastSender.startsWith("+")) {
                    lastSender = "+" + lastSender;
                  }
                  int firstNewline = smsData.indexOf("\n");
                  int secondNewline = smsData.indexOf("\n", firstNewline + 1);
                  if (secondNewline > 0) {
                    int msgStart = secondNewline + 1;
                    int msgEnd = smsData.indexOf("\n", msgStart);
                    if (msgEnd == -1)
                      msgEnd = smsData.length();
                    smsString = smsData.substring(msgStart, msgEnd);
                    smsString.trim();
                    smsString.replace("\r", "");
                    if (smsString.length() > 0) {
                      Serial.println("\U0001F4C4 Msg periódica: " + smsString);
                      saturno->processSMSCommand(smsString, lastSender);
                    }
                  }
                }
                at->sendATCommand("AT+CMGD=" + String(smsIndex), 1000);
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
