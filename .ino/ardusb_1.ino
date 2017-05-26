#include <LiquidCrystal_I2C.h> // Using version 1.2.1
#include <SoftwareSerial.h>
#include <ArduinoJson.h>
#include <Wire.h>

// --IDE------------------------------------------------------
// --LCD------------------------------------------------------
LiquidCrystal_I2C lcd(0x3F, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);
byte uno[8] = {B11111, B11111, B11111, B11111, B11111, B11111, B11111, B11111};
byte zero[8] = {B00000, B00000, B00000, B00000, B00000, B00000, B00000, B00000};
byte gradi[8] = {B01000, B10100, B01000, B00111, B01000, B01000, B01000, B00111};
// --SENSORE 503----------------------------------------------
// 503 sul termistore significa 50.000 di impedenza di riferimento da cui:
#define SERIESRESISTOR 50000
#define THERMISTORNOMINAL 50000
int ThermistorPin = 0; //pin analogico A0
// --GPS------------------------------------------------------
SoftwareSerial gps(0, 1);
boolean finito = false;
String inps = "";
String avalori[15];
String temps;
int idx = 0;
// --SCS011---------------------------------------------------
SoftwareSerial serialSDS(10, 11);// VERDE, BLU
#define LEN 9
unsigned char incomingByte = 0;
unsigned char buf[LEN];
int PM2_5Val = 0;
int PM10Val = 0;
int PM[1];



// --GLOBALI OUTPUT--------------------------------------------
String messaggioJSON, mexJSONGPS, messaggioMQTT, JSONsds, JSONtemp, JSONgps;
unsigned long avvio, pausa, attivita, faseprecedente;
boolean INattivita, f_sds, f_gps, f_temp, f_send;
char json[75];



void setup() {
  Serial.begin(115200);//solo per il monitor layout IDE / RED
  serialSDS.begin(9600);


  avvio = millis();
  faseprecedente = millis();
  pausa = 120000; //60 SECONDI
  attivita = 10000; //3 SECONDI
  INattivita = false;


  //carica caratteri personalizzati
  lcd.createChar(0, uno);
  lcd.begin(20, 4);
  lcd.createChar(0, zero);
  lcd.begin(20, 4);
  lcd.createChar(0, gradi);
  lcd.begin(20, 4);

  digitalWrite(7, HIGH); // GPS ATTIVO

  gps.begin(9600);


  String sigla1, sigla2, sigla3;
  sigla1 = "(A)RDUSB-------rel.1";
  sigla2 = "Autocontrollo mobile";
  sigla3 = "qualita'  ambientale";

  lcd.clear(); // SIGLA LCD
  aggiornalcd(0, 0, sigla1);
  aggiornalcd(0, 1, sigla2);
  aggiornalcd(0, 2, sigla3);
  delay(2000);

  lcd.clear(); // MASCHERA VALORI
  aggiornalcd(0, 0, "ora:");
  aggiornalcd(12, 0, "PM10:");
  aggiornalcd(15, 1, "2:");
  aggiornalcd(0, 1, "lat:");
  aggiornalcd(0, 2, "lon:");
  aggiornalcd(0, 3, "T:");
  lcd.setCursor(7, 3);
  lcd.write(byte(0));
  aggiornalcd(9, 3, "alt:");

  f_temp = false;
  f_send = false;
  f_gps = false;
  f_sds = false;

  messaggioJSON = "";
}

void loop() {
  int i;
  unsigned char checksum;
  unsigned long ora = millis();
  String errsds = "";

  if (INattivita && (ora - faseprecedente >= attivita)) {
    serialSDS.end();
    INattivita = false;
    lcd.setCursor(17, 1);
    lcd.print(" ");
    faseprecedente = millis();
    stop_SDS();
    //lcd.noBacklight(); // ATTIVARE IN PRODUZIONE PER LIMITARE CONSUMI
    aggiornalcd(19, 3, "P");
    gps.listen();

  }
  else if (!INattivita && (ora - faseprecedente > pausa)) {
    gps.end();
    gps.flush();
    INattivita = true;
    faseprecedente = millis();
    aggiornalcd(19, 3, "A");
    start_SDS();
    lcd.setCursor(17, 1);
    lcd.print(serialSDS.available());
    serialSDS.begin(9600);
    serialSDS.listen();

    //lcd.backlight();
  }


  while ((gps.available() > 0) && (!INattivita)) {
    char inchar = (char)gps.read();
    inps += inchar;
    temps += inchar;
    if (inchar == ',' ) {
      if (inps.startsWith("$GPGGA")  ) {
        temps.remove(temps.length() - 1);
        avalori[idx] = temps;
        temps = "";
        idx++;
      }
    }
    else if (inchar == '\n' ) {
      if (inps.startsWith("$GPGGA") ) {

        String ora;
        String confuso;
        confuso = String(avalori[1].substring(0, 6).toInt() + 20000);
        if (confuso.length() > 5) {
          ora = confuso.substring(0, 2) + confuso.substring(2, 4) + "'" + confuso.substring(4) + "\"";
        }
        else {
          ora = confuso.substring(0, 1) + confuso.substring(1, 3) + "'" + confuso.substring(3) + "\"";
        }
        aggiornalcd(4, 0, ora);
        aggiornalcd(4, 1, avalori[2]);
        aggiornalcd(4, 2, avalori[4]);
        aggiornalcd(13, 3, avalori[9]);
        temps = "";
        idx = 0;
        f_gps = 1;
        f_temp = false;
      }
      else {
        inps = "";
        idx = 0;
        temps = "";

      }
    }
  }



  if ((serialSDS.available() > 0) && (INattivita) ) {
    incomingByte = serialSDS.read();
    if (incomingByte == 0xAA) {
      serialSDS.readBytes(buf, LEN);
      if ((buf[0] == 0xC0) && (buf[8] == 0xAB)) {
        for (i = 1; i <= 6; i++) {
          checksum = checksum + buf[i];
        }
        if (checksum == buf[7]) {
          PM2_5Val = ((buf[2] << 8) + buf[1]) / 10;
          PM10Val = ((buf[4] << 8) + buf[3]) / 10;
          PM[0] = PM2_5Val;
          PM[1] = PM10Val;
          errsds = "";
          f_sds = 1;
          lcd.setCursor(17, 0);

          if (PM[1] > 0) {
            String app;
            app = String(PM[1]);
            switch ((app.length())) {
              case 1:
                lcd.print("  ");
                break;
              case 2:
                lcd.print(" ");
                break;
            }
            lcd.print(PM[1]);
            lcd.setCursor(17, 1);
            app = String(PM[0]);
            switch ((app.length())) {
              case 1:
                lcd.print("  ");
                break;
              case 2:
                lcd.print(" ");
                break;
            }
            lcd.print(PM[0]);

            JSONsds = "\"PM2\":";
            JSONsds += PM[0];
            JSONsds += ",\"PM10\":";
            JSONsds += PM[1];

          }
          else
          { lcd.print(errsds);
            f_sds = false;
          }
        }
        else {
          errsds = "CSE";
        }
      }
      else {
        errsds = "FER";
      }
    }
  }


  /*
    if (INattivita) {
      lcd.setCursor(17, 0);

      if (PM[1] > 0) {
        String app;
        app = String(PM[1]);
        switch ((app.length())) {
          case 1:
            lcd.print("  ");
            break;
          case 2:
            lcd.print(" ");
            break;
        }
        lcd.print(PM[1]);
        lcd.setCursor(17, 1);
        app = String(PM[0]);
        switch ((app.length())) {
          case 1:
            lcd.print("  ");
            break;
          case 2:
            lcd.print(" ");
            break;
        }
        lcd.print(PM[0]);

        JSONsds = frammentoJSON("PM2", (String)PM[0], 0);
        JSONsds += "," + frammentoJSON("PM10", (String)PM[1] , 0);

      }
      else
      { lcd.print(errsds);
        f_sds = false;
      }
    }
  */

  if ((!INattivita) && (!f_temp)) {
    float temperatura = temp503();
    aggiornalcd(2, 3, (String)temperatura);
    JSONtemp = "\"temp\":";
    JSONtemp += temperatura;
    f_temp = !f_temp;
  }

  if ((!INattivita) && (f_gps) && (f_sds) && (f_temp)) {
    f_send = !f_send;
  } else {
    lcd.setCursor(16, 2);
    lcd.print(f_sds);
    lcd.print(f_gps);
    lcd.print(f_temp);
    lcd.print(f_send);
  }

  if ((!INattivita) && (f_send)) {
    // MANCA LA VIRGOLA TRA GPS E SDS
    messaggioJSON = """{" + JSONsds + "," + JSONtemp + "}""";
    String app = messaggioJSON ;
    /*DynamicJsonBuffer jsonBuffer;
    messaggioJSON.toCharArray(json, 75);
    JsonObject& root = jsonBuffer.parseObject(json);

    if (!root.success())
    {
      //Serial.println("parseObject() failed");*/
      Serial.println(app);/*
      } else {
      String pl;
      root.printTo(pl);
      Serial.println(pl);
    }*/
    Serial.flush();

    f_send = !f_send;
    f_gps = !f_gps;
    f_sds = !f_sds;
    f_temp = !f_temp;

  }

}


float temp503() {
  // vengono fatte 3 letture
  //calcolo media delle letture
  float average = 0;
  for (int i = 0; i < 3; i++) {
    average += analogRead(ThermistorPin);//samples[i];
  }
  average /= 3;
  average = 1023 / average - 1;
  average = SERIESRESISTOR / average;         //RESISTENZA IN SERIE
  float steinhart;
  steinhart = average / THERMISTORNOMINAL;     // (R/Ro)  PER 503=50000
  steinhart = log(steinhart);                  // ln(R/Ro)
  steinhart /= 3950;                            // 1/B * ln(R/Ro)
  steinhart += 1.0 / (25 + 273.15);             // + (1/To)
  steinhart = 1.0 / steinhart;                 // Invert
  steinhart -= 273.15;                         // convert to C
  return steinhart;
}

void aggiornalcd(int posc, int posr, String val) {
  lcd.setCursor(posc, posr);
  lcd.print(val);
}

String frammentoJSON(String campo, String valore, int strin) {
  String ritorno = "\"" + campo + "\":";
  if (strin = 1) {
    ritorno += "\"" + valore + "\"";
  } else if (strin = 2) {
    ritorno += (String)valore.toFloat();
  } else {
    ritorno += valore;
  }
  return ritorno ;
}

void start_SDS() {
  const uint8_t start_SDS_cmd[] = {0xAA, 0xB4, 0x06, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x06, 0xAB};
  serialSDS.write(start_SDS_cmd, sizeof(start_SDS_cmd));
}

void stop_SDS() {
  const uint8_t stop_SDS_cmd[] = {0xAA, 0xB4, 0x06, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x05, 0xAB};
  serialSDS.write(stop_SDS_cmd, sizeof(stop_SDS_cmd));
}


static const char hex[] = "0123456789ABCDEF";

boolean gpsChecksumOk(const char *gpsMessage) {
  int8_t max = 90; // NMEA says 82, but there could have longer proprietary messages
  if (*gpsMessage != '$') return false;
  byte v = 0;
  for (;;) {
    if (--max <= 0) return false; // Protect from run away if no *
    byte b = *++gpsMessage;
    if (b == '*') break;
    v ^= b;
  }
  unsigned char digit0 = hex[(v & 0xf0) >> 4];
  unsigned char digit1 = hex[(v & 0x0f)];
  if (gpsMessage[1] != digit0 || gpsMessage[2] != digit1) return false;
  return true;
}
