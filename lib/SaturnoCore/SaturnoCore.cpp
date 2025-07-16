#include "SaturnoCore.h"

SaturnoCore::SaturnoCore(HardwareSerial &serial, SemaphoreHandle_t mutex)
    : serialAT(serial), serialMutex(mutex) {}

void SaturnoCore::processSMSCommand(String &smsString, String &lastSender)
{
    smsString.toUpperCase();
    smsString.trim();
    String response = "";
    if (smsString.indexOf("SATURNO,OFF") != -1)
    {
        digitalWrite(RELAY_PIN, HIGH);
        response = "Rele DESLIGADO";
        Serial.println("🔌 Relé desligado por " + lastSender);
    }
    else if (smsString.indexOf("SATURNO,ON") != -1)
    {
        digitalWrite(RELAY_PIN, LOW);
        response = "Rele LIGADO";
        Serial.println("🔌 Relé ligado por " + lastSender);
    }
    else if (smsString.indexOf("SATURNO,MSA,12") != -1)
    {
        digitalWrite(RELAY_PIN, HIGH);
        delay(10000);
        digitalWrite(RELAY_PIN, LOW);
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

bool SaturnoCore::sendSMS(const String &number, const String &message)
{
    Serial.println("📤 Enviando SMS para: " + number);
    if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(5000)) != pdTRUE)
    {
        Serial.println("❌ Não foi possível obter acesso à serial");
        return false;
    }
    while (serialAT.available())
    {
        serialAT.read();
    }
    serialAT.println("AT+CMGF?");
    delay(100);
    String response = "";
    unsigned long startTime = millis();
    while (millis() - startTime < 1000)
    {
        if (serialAT.available())
        {
            response += (char)serialAT.read();
        }
    }
    Serial.println("� Status modo SMS: " + response);
    if (response.indexOf("+CMGF: 1") == -1)
    {
        Serial.println("🔧 Configurando modo texto...");
        serialAT.println("AT+CMGF=1");
        delay(1000);
        while (serialAT.available())
        {
            serialAT.read();
        }
    }
    else
    {
        Serial.println("✅ Modo texto já estava configurado");
    }
    Serial.println("✅ Iniciando envio SMS...");
    String cmd = "AT+CMGS=\"" + number + "\"";
    serialAT.println(cmd);
    Serial.println("📟 Comando: " + cmd);
    startTime = millis();
    String promptBuffer = "";
    bool promptReceived = false;
    while (millis() - startTime < 10000)
    {
        if (serialAT.available())
        {
            char c = serialAT.read();
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
    Serial.println("✅ Enviando: '" + message + "'");
    serialAT.print(message);
    serialAT.write(26);
    startTime = millis();
    String resultBuffer = "";
    while (millis() - startTime < 30000)
    {
        if (serialAT.available())
        {
            char c = serialAT.read();
            resultBuffer += c;
            if (resultBuffer.indexOf("+CMGS:") != -1)
            {
                Serial.println("✅ SMS enviado com sucesso!");
                Serial.println("📱 " + number + " <- " + message);
                while (serialAT.available())
                {
                    serialAT.read();
                }
                xSemaphoreGive(serialMutex);
                return true;
            }
            else if (resultBuffer.indexOf("ERROR") != -1)
            {
                Serial.println("❌ Erro ao enviar SMS: " + resultBuffer);
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

void SaturnoCore::diagnoseSMS()
{
    Serial.println("=== DIAGNÓSTICO SMS ===");
    Serial.println("🔍 1. Verificando modo SMS atual...");
    serialAT.println("AT+CMGF?");
    delay(200);
    Serial.println("🔧 2. Configurando modo texto...");
    serialAT.println("AT+CMGF=1");
    delay(200);
    Serial.println("📝 3. Verificando charset...");
    serialAT.println("AT+CSCS?");
    delay(200);
    Serial.println("🔔 4. Verificando notificações SMS...");
    serialAT.println("AT+CNMI?");
    delay(200);
    Serial.println("💾 5. Verificando armazenamento...");
    serialAT.println("AT+CPMS?");
    delay(200);
    Serial.println("📶 6. Verificando sinal...");
    serialAT.println("AT+CSQ");
    delay(200);
    Serial.println("🌐 7. Verificando rede...");
    serialAT.println("AT+CREG?");
    delay(200);
    Serial.println("📞 8. Verificando centro SMS...");
    serialAT.println("AT+CSCA?");
    delay(200);
    Serial.println("💬 9. Teste comunicação AT...");
    serialAT.println("AT");
    delay(200);
    Serial.println("=== FIM DIAGNÓSTICO ===");
    Serial.println("💡 Use este log para identificar problemas");
}
