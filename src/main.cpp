#include "Adafruit_FONA.h"
HardwareSerial SerialAT(1);

const int RELAY_PIN = 21;
const int FONA_RST = 5;

char replybuffer[255];
uint8_t readline(char *buff, uint8_t maxbuff, uint16_t timeout = 0);
String smsString = "";
char fonaNotificationBuffer[64];          //for notifications from the FONA
char smsBuffer[250];

HardwareSerial *fonaSerial = &SerialAT;

Adafruit_FONA_3G fona = Adafruit_FONA_3G(FONA_RST);

void setup() 
{
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  pinMode(5, OUTPUT);
  digitalWrite(5, HIGH);
  
  Serial.begin(115200);
  Serial.println(F("FONA SMS caller ID test"));
  Serial.println(F("Initializing....(May take 3 seconds)"));
  delay(5000);
  fonaSerial->begin(115200,SERIAL_8N1,26,27, false);
  if (! fona.begin(*fonaSerial))
  {
    Serial.println(F("Couldn't find FONA"));
    while(1);
  }
  Serial.println(F("FONA is OK"));
  
  fonaSerial->print("AT+CNMI=2,1\r\n"); 
  delay(5000); //set up the FONA to send a +CMTI notifiaacation when an SMS is received
  Serial.println("FONA Ready");
}

void loop() 
{
  char* bufPtr = fonaNotificationBuffer;    //handy buffer pointer
  
  if (fona.available())      //Há algum dado disponível da FONA?
  {
    int slot = 0;            //this will be the slot number of the SMS
    int charCount = 0;
    //Read the notification into fonaInBuffer
    do
    {
      *bufPtr = fona.read();
      Serial.write(*bufPtr);
      delay(1);
    } while ((*bufPtr++ != '\n') && (fona.available()) && (++charCount < (sizeof(fonaNotificationBuffer)-1)));

    //Add a terminal NULL to the notification string
    *bufPtr = 0;

    //Scan the notification string for an SMS received notification.
    //  If it's an SMS message, we'll get the slot number in 'slot'
    if (1 == sscanf(fonaNotificationBuffer, "+CMTI: " FONA_PREF_SMS_STORAGE ",%d", &slot)) 
    {
      Serial.print("slot: "); Serial.println(slot);

      char callerIDbuffer[32];  //we'll store the SMS sender number in here

      // Retrieve SMS sender address/phone number.
      if (! fona.getSMSSender(slot, callerIDbuffer, 31)) 
      {
        Serial.println("Didn't find SMS message in slot!");
      }
      Serial.print(F("FROM: ")); Serial.println(callerIDbuffer);

      // Retrieve SMS value.
      uint16_t smslen;
      if (fona.readSMS(slot, smsBuffer, 250, &smslen)) // pass in buffer and max len!
      { 
        smsString = String(smsBuffer);
        Serial.println(smsString);
      }

      if (smsString == "Saturno,msa,12")  //Change "On" to something secret like "On$@8765"
      {
        digitalWrite(RELAY_PIN, HIGH);
        Serial.println("Rebotando.");
        delay(100);
        fona.sendSMS(callerIDbuffer,"Rebotando.");
        delay(10000);
        digitalWrite(RELAY_PIN, LOW);
        fona.sendSMS(callerIDbuffer,"Reboot Concluido.");
     
      }
      else if(smsString == "Saturno,cos") //Change "On" to something secret like "Off&%4235"
      {
        Serial.println("Conectado.");
        delay(100);
        fona.sendSMS(callerIDbuffer, "Conectado.");
      }
       
      if (fona.deleteSMS(slot)) {
        Serial.println(F("OK!"));
      } else {
        Serial.print(F("Couldn't delete SMS in slot ")); 
        Serial.println(slot);
        fona.print(F("AT+CMGD=?\r\n"));
      }
    }  
  }
}

//947843900