#include "SaturnoModem.h"

SaturnoModem::SaturnoModem() : at(nullptr), saturno(nullptr), ledPin(14) {} // LED_CONNECT = 14

void SaturnoModem::initModem(int pwrkey, int rst, int power_on, int tx, int rx)
{
    pinMode(pwrkey, OUTPUT);
    pinMode(rst, OUTPUT);
    pinMode(power_on, OUTPUT);
    digitalWrite(pwrkey, LOW);
    digitalWrite(rst, HIGH);
    digitalWrite(power_on, HIGH);
    delay(500);
    digitalWrite(ledPin, HIGH);
    delay(500);
    digitalWrite(ledPin, LOW);
    delay(500);
    digitalWrite(ledPin, HIGH);
    delay(500);
    digitalWrite(ledPin, LOW);
    Serial.println("Inicializando modem...");
    digitalWrite(power_on, HIGH);
    delay(1000);
    digitalWrite(pwrkey, HIGH);
    delay(1000);
    digitalWrite(pwrkey, LOW);
    delay(2000);
    digitalWrite(rst, LOW);
    delay(500);
    digitalWrite(rst, HIGH);
    delay(5000);
}

void SaturnoModem::fullInitialization(HardwareSerial &serialAT, SemaphoreHandle_t mutex, ATCommands **atPtr, SaturnoCore **saturnoPtr)
{
    Serial.println("=== INICIALIZAÇÃO COMPLETA DO MODEM ===");

    // 1. Criar objetos das bibliotecas
    Serial.println("Criando objetos das bibliotecas...");
    at = new ATCommands(serialAT, mutex);
    saturno = new SaturnoCore(serialAT, mutex);
    *atPtr = at; // Retornar ponteiros para o main
    *saturnoPtr = saturno;

    // 2. Inicialização do hardware do modem
    initModem(MODEM_PWRKEY, MODEM_RST, MODEM_POWER_ON, MODEM_TX, MODEM_RX);

    // 3. Inicializar Serial do modem
    Serial.println("Configurando comunicação serial...");
    serialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);

    // 4. Sequência completa de diagnóstico e configuração
    printModemInfo();
    checkSimCard();
    waitNetwork(10);
    configureSMS();

    Serial.println("✅ Inicialização completa do modem finalizada!");
}

bool SaturnoModem::checkSimCard()
{
    String simResponse = at->sendATCommand("AT+CPIN?", 1000);
    if (simResponse.indexOf("READY") != -1)
    {
        Serial.println("SIM card OK!");
        return true;
    }
    else
    {
        Serial.println("Problema com SIM card! Resposta: " + simResponse);
        return false;
    }
}

bool SaturnoModem::waitNetwork(int maxAttempts)
{
    Serial.println("Aguardando registro na rede...");
    for (int attempts = 0; attempts < maxAttempts; attempts++)
    {
        String response = at->sendATCommand("AT+CREG?", 2000);
        Serial.println("Status registro: " + response);
        if (response.indexOf(",1") != -1 || response.indexOf(",5") != -1)
        {
            Serial.println("Registrado na rede com sucesso!");
            String opResponse = at->sendATCommand("AT+COPS?", 1000);
            Serial.println("Operadora: " + opResponse);
            digitalWrite(ledPin, HIGH);
            return true;
        }
        if (attempts == maxAttempts - 1)
        {
            Serial.println("Não foi possível registrar na rede!");
            Serial.println("Continuando mesmo assim...");
        }
        else
        {
            Serial.printf("Tentativa %d/%d - Aguardando...\n", attempts + 1, maxAttempts);
            delay(3000);
        }
    }
    digitalWrite(ledPin, LOW);
    return false;
}

void SaturnoModem::configureSMS()
{
    at->sendATCommand("AT+CMGF=1", 1000);
    at->sendATCommand("AT+CSCS=\"GSM\"", 1000);
    at->sendATCommand("AT+CNMI=2,1,0,0,0", 1000);
    at->sendATCommand("AT+CPMS=\"SM\",\"SM\",\"SM\"", 1000);
    at->sendATCommand("AT+CMGD=1,4", 2000);
    at->sendATCommand("AT+CNMI?", 1000);
    at->sendATCommand("AT+CPMS?", 1000);
    Serial.println("Modo SMS configurado!");
}

void SaturnoModem::printModemInfo()
{
    Serial.println("Obtendo informações do modem...");
    at->sendATCommand("ATI", 1000);
    at->sendATCommand("AT+CSQ", 1000);
    at->sendATCommand("AT+CREG?", 1000);
}
