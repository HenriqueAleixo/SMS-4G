#ifndef ATCOMMANDS_H
#define ATCOMMANDS_H

#include <Arduino.h>

class ATCommands {
public:
    ATCommands(HardwareSerial &serial, SemaphoreHandle_t mutex);
    String sendATCommand(const String &command, int timeout = 1000);
private:
    HardwareSerial &serialAT;
    SemaphoreHandle_t serialMutex;
};

#endif // ATCOMMANDS_H
