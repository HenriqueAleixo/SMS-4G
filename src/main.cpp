#include "Adafruit_FONA.h"
#define LED_CONNECT 14

HardwareSerial SerialAT(1);

const int RELAY_PIN = 21;
const int FONA_RST = 5;

char replybuffer[255];
uint8_t readline(char *buff, uint8_t maxbuff, uint16_t timeout = 0);
String smsString = "";
char fonaNotificationBuffer[64]; // for notifications from the FONA
char smsBuffer[250];

HardwareSerial *fonaSerial = &SerialAT;

Adafruit_FONA_3G fona = Adafruit_FONA_3G(FONA_RST);

void taskConnect(void *parameter)
{
  while (1)
  {
    int status = fona.getNetworkStatus();
    printf("Status = %d \n", status);

    if (status == 1)
    {
      for (int i = 0; i < 3; i++)
      {
        digitalWrite(LED_CONNECT, HIGH);
        delay(100);
        digitalWrite(LED_CONNECT, LOW);
        delay(100);
      }
    }
    else
    {
      digitalWrite(LED_CONNECT, HIGH);
    }
    delay(2000);
  }
}

void taskSms(void *parameter)
{
  while (1)
  {
    char *bufPtr = fonaNotificationBuffer; // handy buffer pointer

    if (fona.available()) // Há algum dado disponível da FONA?
    {
      int slot = 0; // this will be the slot number of the SMS
      int charCount = 0;
      // Read the notification into fonaInBuffer
      do
      {
        *bufPtr = fona.read();
        Serial.write(*bufPtr);
        delay(1);
      } while ((*bufPtr++ != '\n') && (fona.available()) && (++charCount < (sizeof(fonaNotificationBuffer) - 1)));

      // Add a terminal NULL to the notification string
      *bufPtr = 0;

      // Scan the notification string for an SMS received notification.
      //   If it's an SMS message, we'll get the slot number in 'slot'
      if (1 == sscanf(fonaNotificationBuffer, "+CMTI: " FONA_PREF_SMS_STORAGE ",%d", &slot))
      {
        Serial.print("slot: ");
        Serial.println(slot);

        char callerIDbuffer[32]; // we'll store the SMS sender number in here

        // Retrieve SMS sender address/phone number.
        if (!fona.getSMSSender(slot, callerIDbuffer, 31))
        {
          Serial.println("Didn't find SMS message in slot!");
        }
        Serial.print(F("FROM: "));
        Serial.println(callerIDbuffer);

        // Retrieve SMS value.
        uint16_t smslen;
        if (fona.readSMS(slot, smsBuffer, 250, &smslen)) // pass in buffer and max len!
        {
          smsString = String(smsBuffer);
          Serial.println(smsString);
        }

        if (smsString == "Saturno,msa,12") // Change "On" to something secret like "On$@8765"
        {
          digitalWrite(RELAY_PIN, HIGH);
          Serial.println("Rebotando.");
          delay(100);
          fona.sendSMS(callerIDbuffer, "Inicializando Reboot.");
          delay(10000);
          digitalWrite(RELAY_PIN, LOW);
          fona.sendSMS(callerIDbuffer, "Reboot Concluido.");
        }
        else if (smsString == "Saturno,cos") // Change "On" to something secret like "Off&%4235"
        {
          Serial.println("Conectado.");
          delay(100);
          if (digitalRead(RELAY_PIN) == HIGH)
          {
            // concactena a string para enviar na mensagem
            String msg = "Conectado. Sistema Desligado.";
            char msgChar[100];
            msg.toCharArray(msgChar, 100);
            fona.sendSMS(callerIDbuffer, msgChar);
          }
          else if (digitalRead(RELAY_PIN) == LOW)
          {
            // concactena a string para enviar na mensagem
            String msg = "Conectado. Sistema Ligado.";
            char msgChar[100];
            msg.toCharArray(msgChar, 100);
            fona.sendSMS(callerIDbuffer, msgChar);
          }
        }
        else if (smsString == "Saturno,on") // Change "On" to something secret like "Off&%4235"
        {
          digitalWrite(RELAY_PIN, LOW);
          delay(100);
          fona.sendSMS(callerIDbuffer, "Sistema Ligado");
        }
        else if (smsString == "Saturno,off") // Change "On" to something secret like "Off&%4235"
        {
          digitalWrite(RELAY_PIN, HIGH);
          delay(100);
          fona.sendSMS(callerIDbuffer, "Sistema Desligado");
        }

        if (fona.deleteSMS(slot))
        {
          Serial.println(F("OK!"));
        }
        else
        {
          Serial.print(F("Couldn't delete SMS in slot "));
          Serial.println(slot);
          fona.print(F("AT+CMGD=?\r\n"));
        }
      }
    }
    delay(10);
  }
}

void setup()
{
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  pinMode(LED_CONNECT, OUTPUT);
  digitalWrite(LED_CONNECT, LOW);

  delay(500);
  digitalWrite(LED_CONNECT, HIGH);
  delay(500);
  digitalWrite(LED_CONNECT, LOW);
  delay(500);
  digitalWrite(LED_CONNECT, HIGH);
  delay(500);
  digitalWrite(LED_CONNECT, LOW);

  Serial.begin(115200);
  Serial.println(F("FONA SMS caller ID test"));
  Serial.println(F("Initializing....(May take 3 seconds)"));
  delay(5000);
  fonaSerial->begin(115200, SERIAL_8N1, 26, 27, false);
  if (!fona.begin(*fonaSerial))
  {
    Serial.println(F("Couldn't find FONA"));
    while (1)
      ;
  }
  Serial.println(F("FONA is OK"));

  fonaSerial->print("AT+CNMI=2,1\r\n");
  delay(5000); // set up the FONA to send a +CMTI notifiaacation when an SMS is received
  Serial.println("FONA Ready");

  // criar task
  xTaskCreate(taskConnect, "taskConnect", 1024 * 2, NULL, 1, NULL);
  xTaskCreate(taskSms, "taskSms", 1024 * 4, NULL, 5, NULL);
}

void loop()
{
}

// 947843900