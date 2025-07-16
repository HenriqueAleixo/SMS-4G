#ifndef SATURNO_CORE_H
#define SATURNO_CORE_H

#include <Arduino.h>
#define RELAY_PIN 21 // Defina o pino do relé conforme sua configuração

class SaturnoCore {
public:
    SaturnoCore(HardwareSerial &serial, SemaphoreHandle_t mutex);
    void processSMSCommand(String &smsString, String &lastSender);
    bool sendSMS(const String &number, const String &message);
    void diagnoseSMS();
private:
    HardwareSerial &serialAT;
    SemaphoreHandle_t serialMutex;
};

#endif // SATURNO_CORE_H
