#include "ATCommands.h"

ATCommands::ATCommands(HardwareSerial &serial, SemaphoreHandle_t mutex)
    : serialAT(serial), serialMutex(mutex) {}

String ATCommands::sendATCommand(const String &command, int timeout) {
    if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(timeout)) == pdTRUE) {
        while (serialAT.available()) {
            serialAT.read();
        }
        if (command != "AT") {
            Serial.println("📤 AT: " + command);
        }
        serialAT.println(command);
        unsigned long startTime = millis();
        String response = "";
        bool foundEnd = false;
        while (millis() - startTime < timeout && !foundEnd) {
            if (serialAT.available()) {
                char c = serialAT.read();
                response += c;
                if (response.indexOf("OK\r\n") != -1 ||
                    response.indexOf("ERROR\r\n") != -1 ||
                    response.indexOf("OK\n") != -1 ||
                    response.indexOf("ERROR\n") != -1) {
                    foundEnd = true;
                }
            } else {
                delay(1);
            }
        }
        if (!foundEnd && response.length() > 0) {
            delay(100);
            while (serialAT.available()) {
                response += (char)serialAT.read();
            }
        }
        if (command != "AT" && response.length() > 0) {
            String cleanResponse = response;
            cleanResponse.trim();
            cleanResponse.replace("\r", "");
            cleanResponse.replace("\n", " ");
            Serial.println("📥 Resposta: " + cleanResponse);
        }
        xSemaphoreGive(serialMutex);
        return response;
    } else {
        Serial.println("❌ Timeout aguardando acesso à serial");
        return "";
    }
}
