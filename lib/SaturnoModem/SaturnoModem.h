#ifndef SATURNO_MODEM_H
#define SATURNO_MODEM_H

#include <Arduino.h>
#include "../ATCommands/ATCommands.h"
#include "../SaturnoCore/SaturnoCore.h"

// Configurações do modem
#define MODEM_RST 5
#define MODEM_PWRKEY 4
#define MODEM_POWER_ON 23
#define MODEM_TX 27
#define MODEM_RX 26

class SaturnoModem
{
public:
    SaturnoModem();                                                                                                           // Construtor sem parâmetros
    void fullInitialization(HardwareSerial &serialAT, SemaphoreHandle_t mutex, ATCommands **atPtr, SaturnoCore **saturnoPtr); // Inicialização completa
    void initModem(int pwrkey, int rst, int power_on, int tx, int rx);
    bool checkSimCard();
    bool waitNetwork(int maxAttempts = 10);
    void configureSMS();
    void printModemInfo();

private:
    ATCommands *at;
    SaturnoCore *saturno;
    int ledPin;
};

#endif // SATURNO_MODEM_H
