#define CONTROL Yes
#define MONITOR Yes

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ESP8266WebServer.h>
#include <NTPClient.h>
#include <TimeLib.h> // Für month(), day(), weekday()
#include "timestuff.h" // Hilfsfunktionen für Sommer-Winterzeit
#include <EEPROM.h> 
#include <pgmspace.h>
#include <ArduinoJson.h>
#include "htmlsettings.h"

#ifdef MONITOR
#include <OneWire.h>
#include <DallasTemperature.h>
#include "temperaturesvgpage.h"
#include <ESP_Mail_Client.h>
#include <LittleFS.h>
#define SMTP_HOST "smtp.gmail.com"
#define SMTP_PORT 465
#define SMTP_AUTH_EMAIL "sc..2@gmail.com"
#define SMTP_AUTH_PASSWORD "gcu...cfu " // 2-Faktor-Auth benötigt!
#define RECIPIENT_EMAIL "s...s...8@gmx.de"
#endif

#ifdef CONTROL
#include "htmltemplate.h"
#include "backlight_fader.hpp"
#include "html4light.h"
#endif



#define EEPROM_SIZE 4096
#define EEPROM_SCHEDULE_START 0
#define EEPROM_SCHEDULE_END 200 // 20x5 Byte (4Data+1Checksumme) Checksumme für Schedule)
#define SCHEDULE_SIZE 40
#define SCHEDULE_ENTRY_SIZE 5
#define EEPROM_SETTINGS_START 201 // Future Use
#define EEPROM_SETTINGS_END 510 // Future Use
#define EEPROM_LOG_START 512
#define EEPROM_LOG_END 4096
#define HISTORY_SIZE 700 // 700 >1 Woche Daten bei 20-Minuten-Intervallen / 5 Byte pro Eintrag
#define LOG_ENTRY_SIZE 5         // 4 Byte timestamp + 1 Byte temp

#pragma pack(push, 1)
struct parameter {
  float temp_alarmhigh_treshold = 28.5; //  Temperaturalarm zu hoch
  float temp_alarmlow_treshold = 19.5; //  Temperaturalarm zu hoch
  uint16_t Temp_Update_Interval_LIVE_mins = 20; // 30000 // 1200000 // 20-Minuten
  uint16_t Temp_Update_Interval_SIM_mins = 1; // 180000 // simulate (minimum: alle 60 Sekunden)
  uint16_t backupInterval_mins = 240; // Intervall, in dem Daten im EEprom gespeichert werden
  uint16_t maxcooling_mins = 180; // automatisches Abschalten der Kühlung
  bool simulateSensor = true; // Temperatursensor simulieren
  bool emailme = true; // Mails verschicken
  bool skipmail = true; // Funktion nicht tatsächlich ausführen (nur Konsole)
  bool serialout = true; // Ausgaben über den Serial Port
  bool measure = true; // Temperaturen erfassen (echt bzw. simuliert)
  uint16_t pwmfrequency = 1000;
  uint8_t weeklyReport_tm_wday = 5; // Wöchenentlicher Temperaturreport, Wochentag
  uint8_t weeklyReport_tm_hour = 22; // Wöchenentlicher Temperaturreport, Stunde
  uint8_t weeklyReport_tm_min = 0; // Wöchenentlicher Temperaturreport, Minute
  char ssid[20] = "fritzzzz"; // Router-SSID
  char password[24] = "51...0"; // Router-Passwort
  char ntpServer[16] = "fritz.box"; 
  char smtp_AUTH_EMAIL[32] = "sc..2@gmail.com";
  char smtp_AUTH_PASSWORD[20] = "gcu...cfu "; // 2-Faktor-Auth benötigt!
  char smtp_RECIPIENT_EMAIL[32] = "s...8@gmx.de";
}; 
#pragma pack(pop)

parameter para;
uint16_t Temp_Update_Interval_mins = para.Temp_Update_Interval_LIVE_mins;


#define TEMP_SENSOR_RESOLUTION 12


// Node-MCU für Nano-Aquarium
//#define ONE_WIRE_BUS D0
//#define LIGHT_PWMPIN D4 // D1? -> D4 High at boot
// D1 mini clone für WZ
#define COOLING_OFF_PIN D0 // Sensor für niedrigen Wasserstand
#define RELAYLIGHT_PIN D1 // Normales Licht
#define RELAYCO2_PIN D2 // CO2-Ventil
// D3 connected to FLASH button, boot fails if pulled LOW
// D4 HIGH at boot, connected to on-board LED, boot fails if pulled LOW
#define RELAYMOON_PIN LED_BUILTIN // D4 // Moonlight
#define LIGHT_PWMPIN D5 // GPIO1 -> NanoAquarium 
#define COOLING_PWMPIN D6 // Lüfter
#define ONE_WIRE_BUS D7 // GPIO13 -> Temperatursensor
// D8 SPI (CS), Boot fails if pulled HIGH

#define DEVICE_PWMLIGHT 0
#define DEVICE_COOLING 1
#define DEVICE_LIGHT 2
#define DEVICE_CO2 3
#define DEVICE_MOON 4

bool newEEPROMdata = false;

#ifdef MONITOR
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
unsigned long lastSaveTime = 0;

struct {
  float current = 0.0;
  int8_t history[HISTORY_SIZE] = {0};
  unsigned long timestamps[HISTORY_SIZE] = {0};
  uint16_t index = 0;
  unsigned long lastUpdate = 0;
} temperature;

#endif

#ifdef CONTROL
backlight Light;
backlight Cooling;

uint16_t lightLevels[7] = {
  0,    // Stufe 0: 0% (Index 0)
  50,   // Stufe 1: 5% (Index 1)
  150,  // Stufe 2: 15% (Index 2)
  250,  // Stufe 3: 25% (Index 3)
  512,  // Stufe 4: 50% (Index 4)
  860,  // Stufe 5: 84% (Index 5) 14.2V -> 12V
  1023  // Stufe 6: 100% (Index 6)
};

struct ScheduleEntry {
    uint8_t hour;        // 0-23 (5 Bits = 0-31)
    uint8_t minute;      // 0-59 (6 Bits = 0-63)
    uint8_t brightness;  // 0-100 (8 Bits = 0-255)
    uint8_t fadeMinutes; // 0-60 (6 Bits = 0-63)
    uint8_t device;      // 0-7 (3 Bits = 0-7)
}; 

ScheduleEntry Scheduleentries[20];
uint8_t Scheduleentries_count = 0;

#endif

ESP8266WebServer HTTPserver(80);
//AsyncWebServer HTTPserver(80);

unsigned long lastNTPSync = 0;
const int NTP_SYNC_HOUR = 0;    // 0 Uhr
const int NTP_SYNC_MINUTE = 30; // 30 Minuten



const char* ssid = "fritzzzz";
const char* password = "51...0";
const char* ntpServer = "fritz.box";

// NTP Client konfigurieren
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, ntpServer); 

String RAMinfo() {
  String info = "";
  info += "Freier Heap: " + String(ESP.getFreeHeap()) + " Bytes\n";
  info += "Max. freier Block: " + String(ESP.getMaxFreeBlockSize()) + " Bytes\n";
  info += "Heap-Fragmentierung: " + String(ESP.getHeapFragmentation()) + "%\n";
  info += "Stack-Level: " + String(ESP.getFreeContStack()) + " Bytes\n";
  return info;
}

#ifdef MONITOR
float temp_int2float(int8_t tempDeviation) {
  return 25.0 + (tempDeviation / 10.0);
}

int8_t temp_float2int(float absolutetemp) {
  return (int8_t)((absolutetemp - 25.0) * 10);
}
#endif

bool saveToEEPROM(bool clearAll = false) {
  EEPROM.begin(EEPROM_SIZE);
  if (!clearAll && !newEEPROMdata) return false; // abbrechen, wenn sich die Werte nicht geändert haben.
  uint8_t sentries = 0;
  // Schedule
  #ifdef CONTROL
  sentries = Scheduleentries_count;
    EEPROM.put(EEPROM_SCHEDULE_START, sentries);
  for(int i=0; i < Scheduleentries_count; i++) {
    int addr = EEPROM_SCHEDULE_START + 1 + (i * SCHEDULE_ENTRY_SIZE);
    EEPROM.put(addr + 0, Scheduleentries[i].hour);
    EEPROM.put(addr + 1, Scheduleentries[i].minute);
    EEPROM.put(addr + 2, Scheduleentries[i].brightness);
    EEPROM.put(addr + 3, Scheduleentries[i].fadeMinutes);
    EEPROM.put(addr + 4, Scheduleentries[i].device);
  }
  #endif

  // Settings
  EEPROM.put(EEPROM_SETTINGS_START, (uint8_t) 42);
  EEPROM.put(EEPROM_SETTINGS_START + 1, para);

  // TBD...

  // Temperatur-Logs
  #ifdef MONITOR
  uint16_t entries = HISTORY_SIZE;
  EEPROM.put(EEPROM_LOG_START, entries);
  EEPROM.put(EEPROM_LOG_START + 2, temperature.index);
  for(int i=0; i<HISTORY_SIZE; i++) {
    int addr = EEPROM_LOG_START + 4 + (i * LOG_ENTRY_SIZE);
    if (clearAll){
      EEPROM.put(addr, 0); // 0 als Marker für ungültige Zeit
      temperature.timestamps[i] = 0;
    } else {
      EEPROM.put(addr, temperature.timestamps[i]); // 4 byte
      // Temperatur als Abweichung von 25°C in Zehntelgrad
      // int8_t tempDeviation = (int8_t)((temperature.history[i] - 25.0) * 10);
      // EEPROM.put(addr + 4, tempDeviation); // 1 byte
      EEPROM.put(addr + 4, temperature.history[i]); // 1 byte
    }
  }
  #endif
  EEPROM.commit();
  EEPROM.end();
  // asm volatile ("" ::: "memory"); // Garantiert dass EEPROM.end() KOMPLETT abgeschlossen ist
  delay(500);
  newEEPROMdata = false; // wird erst erneut gespeichert, wenn das flag explizit auf true gesetzt wird.
  if (para.serialout) Serial.println("Schedule und Temperaturdaten gespeichert");
  return true;
}

bool loadSSIDfromEEPROM(){
  EEPROM.begin(EEPROM_SIZE);
  uint16_t entries = 0;
  EEPROM.get(EEPROM_SCHEDULE_START, entries); // dummy read für tests
  EEPROM.end();
  
  //para.ssid = ssid; // "fritzzzz";
  strlcpy(para.ssid, ssid, sizeof(para.ssid));
  //para.password = password; // = "51...0";
  strlcpy(para.password, password, sizeof(para.password));
  return true;
}

bool loadFromEEPROM() {
  static bool alreadyLoaded = false;
  if (alreadyLoaded) return true;
  EEPROM.begin(EEPROM_SIZE);
  #ifdef CONTROL
  uint8_t sentries = 0;
  EEPROM.get(EEPROM_SCHEDULE_START, sentries);
  if (sentries > SCHEDULE_SIZE) sentries = 0; // ungültige Einträge
  Scheduleentries_count = sentries;
  for(int i=0; i < sentries; i++) {
    int addr = EEPROM_SCHEDULE_START + 1 + (i * SCHEDULE_ENTRY_SIZE);
    EEPROM.get(addr + 0, Scheduleentries[i].hour);
    EEPROM.get(addr + 1, Scheduleentries[i].minute);
    EEPROM.get(addr + 2, Scheduleentries[i].brightness);
    EEPROM.get(addr + 3, Scheduleentries[i].fadeMinutes);
    EEPROM.get(addr + 4, Scheduleentries[i].device);
  }
  #endif

  // Settings
  uint8_t checkvalid = 0;
  EEPROM.get(EEPROM_SETTINGS_START, checkvalid);
  if (checkvalid == 42){
    EEPROM.get(EEPROM_SETTINGS_START+1, para);
  } else {
    if (para.serialout) Serial.println("ungültige Parameter in EEPROM werden ignoriert.");
  }
  

  // Temperatur-Logs
  #ifdef MONITOR
  uint16_t entries = 0;
  EEPROM.get(EEPROM_LOG_START, entries);
  if(entries == HISTORY_SIZE) { // Gültige Daten
    EEPROM.get(EEPROM_LOG_START + 2, temperature.index);
    if (temperature.index > HISTORY_SIZE) temperature.index = 0; // ungültige Daten -> von vorn beginnen
    for(int i=0; i<HISTORY_SIZE; i++) {
      int addr = EEPROM_LOG_START + 4 + (i * LOG_ENTRY_SIZE);
      EEPROM.get(addr, temperature.timestamps[i]);
      //int8_t tempDeviation = 0;
      //EEPROM.get(addr + 4, tempDeviation);
      EEPROM.get(addr + 4, temperature.history[i]);
      //temperature.history[i] = 25.0 + (tempDeviation / 10.0); // Temperatur zurückrechnen
    }
  }
  #endif
  
  EEPROM.end();

  //asm volatile ("" ::: "memory"); // Garantiert dass EEPROM.end() KOMPLETT abgeschlossen ist
  delay(500);
  if (para.serialout) Serial.println("Schedule und Temperaturdaten geladen");
  alreadyLoaded = true;
  return true;
}

void reconnectWifi(bool restartHTTPserver = true){
  Serial.printf("reconnectWifi\n");
  // if (handleHTTPserver) {
  //    HTTPserver.stop();
  //    Serial.printf("HTTP Server gestoppt\n");
  //    delay(1000);
  //  }

  WiFi.disconnect(true);  // Deaktiviert WiFi + löscht Einstellungen
  Serial.printf("WiFi.disconnect\n");
  delay(1000);           // Warte 1 Sekunde
  WiFi.mode(WIFI_STA);   // Setze Modus zurück (STA = Client-Modus)
  Serial.printf("WiFi WIFI_STA\n");
  WiFi.begin(para.ssid, para.password);
  //WiFi.begin(); // zuletzt verwendete Daten verwenden
  Serial.printf("WiFi.begin\n");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    if (para.serialout) Serial.print(".");
  }
  if (para.serialout) Serial.printf("\nWiFi verbunden, IP: %s\n", WiFi.localIP().toString().c_str());
  HTTPserver.begin();
  Serial.printf("HTTPserver.begin\n");
  if (restartHTTPserver) {
    HTTPserver.begin();
    if (para.serialout) Serial.printf("HTTP Server wieder gestartet\n");
  }
}

#ifdef MONITOR
bool sendEmail(bool isReboot = false, bool isAlarm = false) {
  static int AlarmSentDay = 0;
  int today = getDay();
  bool sentsuccessful = false;

  if (isAlarm && AlarmSentDay == today) {
      return false; // abbrechen, wenn der Alarm heute schon verschickt wurde
  }

  if (para.skipmail) {
    if (para.serialout) Serial.println("Skipping email");
    return true;
  }

  // Speicher prüfen
  if (ESP.getFreeHeap() < 18000) {
    if (para.serialout) Serial.println("Warnung: Zu wenig Speicher für E-Mail");
    return false;
  }
  
  HTTPserver.stop(); // Webserver beenden, damit nichts dazwischen kommt, evtl. bleibt auch mehr RAM übrig?
  delay(1000);
  yield();
  SMTPSession smtp;
  // E-Mail konfigurieren
  SMTP_Message message;
  message.sender.name = "Aquarium Monitor V4";
  message.sender.email = SMTP_AUTH_EMAIL;
  message.subject = "Wöchentlicher Temperaturreport";
  if (isReboot) message.subject = "Neustart - Temperaturdaten";
  if (isAlarm) message.subject = "Alarm Temperatur zu hoch oder zu niedrig";

  message.addRecipient("", RECIPIENT_EMAIL);
  message.text.content = "Angehängte Temperaturdaten";
  if (isAlarm) {
	  message.text.content = "Temperatur zu hoch oder zu niedrig";
    message.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_high;
  };
  message.text.charSet = "utf-8";

  // CSV-Anhang über LittleFS
  if (para.serialout) Serial.println("Schreibe CSV in LittleFS...");
  if (!writeCSVToLittleFS()) {
      if (para.serialout) Serial.println("Fehler beim Schreiben der CSV-Datei");
      return false;
  }
  yield();

  if (para.serialout) Serial.println("Füge Datei als Attachment hinzu...");
  if (!attachFileFromLittleFS(message)) {
      if (para.serialout) Serial.println("Fehler beim Hinzufügen des Datei-Attachments");
      
      // Aufräumen: Datei löschen
      LittleFS.remove("/temperatures.csv");
      return false;
  }
  yield();


  // Verbindung herstellen
  if (para.serialout) Serial.println("ESP_Mail_Session Verbindung herstellen..\n");
  ESP_Mail_Session session;
  session.server.host_name = SMTP_HOST;
  session.server.port = SMTP_PORT;
  session.login.email = SMTP_AUTH_EMAIL;
  session.login.password = SMTP_AUTH_PASSWORD;
  session.login.user_domain = "";

  if (!smtp.connect(&session)) {
    if (para.serialout) Serial.println("smtp.connect gescheitert\n");
    yield();
    delay(5000);
    yield();
    reconnectWifi();
    delay(5000);
    yield();
    smtp.connect(&session); // ein weiterer Versuch
    yield();
    //return;
  }
    
  // E-Mail senden
  sentsuccessful = MailClient.sendMail(&smtp, &message);
  if (sentsuccessful){
    if (para.serialout) Serial.println("Email verschickt.");
    if (isAlarm) AlarmSentDay = today;
  } else {
    if (para.serialout) Serial.println("Fehler beim Senden der Email.");
  }

  // Session immer schließen (auch bei Fehlern)
  smtp.closeSession();
  delay(5000);
  reconnectWifi(true); // closeSession schließt auch Wifi-Verbindung, daher neu verbinden + Webserver wieder starten
  return sentsuccessful;
}


void checkScheduledEmail() {
  static bool emailSent = false;
  static uint8_t lastSentDay = 255; // Initialwert außerhalb 0-6
  time_t now = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);

  // Jeden Freitag (Tag 5) um 22:00
  if(timeinfo.tm_wday == para.weeklyReport_tm_wday && timeinfo.tm_hour > para.weeklyReport_tm_hour && timeinfo.tm_min > para.weeklyReport_tm_min) {
    if(!emailSent || lastSentDay != timeinfo.tm_wday) {
      if (sendEmail(false)) {
        emailSent = true;
        lastSentDay = timeinfo.tm_wday;
      };
    }
  } 
  // Reset erst am Tag nachdem die Mail erfolgreich verschickt wurde.
  else if(emailSent && timeinfo.tm_wday != lastSentDay) {
    emailSent = false;
    lastSentDay = 255;
  }
}

void updateTemperature() {
  if (para.simulateSensor){
    temperature.current = random(19, 35);
  } else {
    sensors.requestTemperatures();
    temperature.current = sensors.getTempCByIndex(0);
    if ((temperature.current > 40) || (temperature.current < 10)){
      return; // Sensorfehler
    }
  }  
  
  temperature.history[temperature.index] = temp_float2int(temperature.current);
  temperature.timestamps[temperature.index] = getCorrectTimestamp();
  temperature.index = (temperature.index + 1) % HISTORY_SIZE;
  temperature.lastUpdate = millis();
  newEEPROMdata = true;
  if (para.serialout) Serial.printf("Temperatur: %.1f °C\n", temperature.current);
  
  if (temperature.current > para.temp_alarmhigh_treshold || temperature.current < para.temp_alarmlow_treshold){
    if (para.serialout) Serial.printf("Temperatur zu hoch oder niedrig\n");
    sendEmail(false,true); // Temperaturalarm auslösen
  }
}
#endif // Monitor


#ifdef CONTROL

void handleConfigSchedule() {
  HTTPserver.send(200, "text/html", html_schedule);
}

void handleLight() {
  HTTPserver.send(200, "text/html", html_directlight);
}



void handleGetSchedule() {
  String json = "[";
  for(uint8_t i = 0; i < Scheduleentries_count; i++) {
    if (para.serialout) Serial.printf("getentry: %s, %s:%s device: %s, brightness: %s\n", String(i), String(Scheduleentries[i].hour),String(Scheduleentries[i].minute),String(Scheduleentries[i].device),String(Scheduleentries[i].brightness));
    if(i > 0) json += ",";
    json += "{";
    json += "\"hour\":" + String(Scheduleentries[i].hour) + ",";
    json += "\"minute\":" + String(Scheduleentries[i].minute) + ",";
    json += "\"brightness\":" + String(Scheduleentries[i].brightness) + ",";
    json += "\"fadeDuration\":" + String(Scheduleentries[i].fadeMinutes) + ",";
    json += "\"device\":" + String(Scheduleentries[i].device);
    json += "}";
  }
  json += "]";
  
  HTTPserver.send(200, "application/json", json);
}

void handleSetSchedule() {
  if(HTTPserver.hasArg("plain")) {
    String body = HTTPserver.arg("plain");
    DynamicJsonDocument doc(1280);
    deserializeJson(doc, body);
    uint8_t count = 0;
    for(JsonObject item : doc.as<JsonArray>()) {
      if(count >= SCHEDULE_SIZE) break;
      Scheduleentries[count].hour = item["hour"];
      Scheduleentries[count].minute = item["minute"];
      Scheduleentries[count].brightness = item["brightness"];
      Scheduleentries[count].fadeMinutes = item["fadeDuration"];
      Scheduleentries[count].device = item["device"];
      if (para.serialout) Serial.printf("setentry: %s, %s:%s device: %s, brightness: %s\n", String(count), String(Scheduleentries[count].hour),String(Scheduleentries[count].minute),String(Scheduleentries[count].device),String(Scheduleentries[count].brightness));
      count++;
      Scheduleentries_count = count;    
    }
    newEEPROMdata = true;
    if(saveToEEPROM()) {
      HTTPserver.send(200, "application/json", "{\"status\":\"ok\"}");
    } else {
      HTTPserver.send(500, "application/json", "{\"status\":\"error\"}");
    }
  } else {
    HTTPserver.send(400, "application/json", "{\"status\":\"bad request\"}");
  }
}

#endif // CONTROL

#ifdef MONITOR

void handleinfo() {
  HTTPserver.send(200, "text/html", html_temperaturverlauf);
}

String generateCSV() {
  yield(); // Führt WDT-Reset UND andere Systemaufgaben durch
  String csv = "\xEF\xBB\xBFtimestamp;temp\n"; // UTF-8 BOM für Excel
  
  for(uint16_t i = 0; i < HISTORY_SIZE; i++) {
    int idx = (temperature.index + i) % HISTORY_SIZE;
    if(temperature.timestamps[idx] != 0) {
      time_t timestamp = temperature.timestamps[idx];
      struct tm timeinfo;
      localtime_r(&timestamp, &timeinfo);   
      char formattedTime[20];
      strftime(formattedTime, sizeof(formattedTime), "%Y-%m-%d %H:%M:%S", &timeinfo);
      String tempStr = String(temp_int2float(temperature.history[idx]), 1);
      tempStr.replace('.', ',');
      csv += String(formattedTime) + ";" + tempStr + "\n";
    }
  }
  return csv;
}

bool writeCSVToLittleFS() {
    File file = LittleFS.open("/temperatures.csv", "w");
    if (!file) {
        Serial.println("Fehler beim Öffnen der CSV-Datei in LittleFS");
        return false;
    }
    
    // UTF-8 BOM für Excel
    file.write(0xEF);
    file.write(0xBB);
    file.write(0xBF);
    
    // Header
    file.println("timestamp;temp");
    
    // Daten zeilenweise schreiben
    for(int i = 0; i < HISTORY_SIZE; i++) {
        char buffer[128];
        int idx = (temperature.index + i) % HISTORY_SIZE;
        
        if(temperature.timestamps[idx] != 0) {
            time_t timestamp = temperature.timestamps[idx];
            struct tm timeinfo;
            localtime_r(&timestamp, &timeinfo);   
            
            snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d;%.1f",
                    timeinfo.tm_year+1900, timeinfo.tm_mon+1, timeinfo.tm_mday,
                    timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
                    temp_int2float(temperature.history[idx]));
            
            file.println(buffer);
            
            // Alle 50 Zeilen yield() für Stabilität
            if (i % 50 == 0) yield();
        }
    }
    
    file.close();
    Serial.println("CSV erfolgreich in LittleFS geschrieben");
    return true;
}

bool attachFileFromLittleFS(SMTP_Message &message) {
    File file = LittleFS.open("/temperatures.csv", "r");
    if (!file) {
        Serial.println("Fehler beim Öffnen der CSV-Datei aus LittleFS");
        return false;
    }
    
    SMTP_Attachment att;
    att.descr.filename = "temperatures.csv";
    att.descr.mime = "text/csv";
    att.file.path = "/temperatures.csv";
    att.file.storage_type = esp_mail_file_storage_type_flash;
    
    message.addAttachment(att);
    file.close();
    
    Serial.println("Datei erfolgreich als Attachment hinzugefügt");
    return true;
}

void handleCSV() {
  HTTPserver.send(200, "text/csv", generateCSV());
}

void handleSendemail() {
  HTTPserver.send(200, "text/html", "Neustart-mail wird verschickt.");
  sendEmail(true); // tut so, als wäre es ein Neustart gewesen
}

void handlesaveTemperatureToEEPROM() {
  if (saveToEEPROM()){
    HTTPserver.send(200, "text/html", "Messdaten gespeichert.");
  } else {
    HTTPserver.send(200, "text/html", "Messdaten nicht gespeichert. (Evtl. keine Änderung?)");
  };
}

void handleDeleteHistory() {
  saveToEEPROM(true);
  HTTPserver.send(200, "text/html", "Messdaten gelöscht.");
}
#endif // MONITOR

void handlesettings() {
  HTTPserver.send(200, "text/html", html_settings);
}


void handleRAMinfo() {
  String html = RAMinfo(); 
  HTTPserver.send(200, "text/html", html);
}


void handleGetParameters() {
  DynamicJsonDocument doc(2048);
  
  // Float-Parameter
  doc["temp_alarmhigh_treshold"] = para.temp_alarmhigh_treshold;
  doc["temp_alarmlow_treshold"] = para.temp_alarmlow_treshold;
  
  doc["Temp_Update_Interval_LIVE_mins"] = para.Temp_Update_Interval_LIVE_mins;
  doc["Temp_Update_Interval_SIM_mins"] = para.Temp_Update_Interval_SIM_mins;
  doc["backupInterval_mins"] = para.backupInterval_mins;
  doc["maxcooling_mins"] = para.maxcooling_mins;
  
  // Boolean-Parameter
  doc["simulateSensor"] = para.simulateSensor;
  doc["emailme"] = para.emailme;
  doc["skipmail"] = para.skipmail;
  doc["serialout"] = para.serialout;
  doc["measure"] = para.measure;
  
  // Integer-Parameter
  doc["pwmfrequency"] = para.pwmfrequency;
  doc["weeklyReport_tm_wday"] = para.weeklyReport_tm_wday;
  Serial.println(para.weeklyReport_tm_wday);
  doc["weeklyReport_tm_hour"] = para.weeklyReport_tm_hour;
  doc["weeklyReport_tm_min"] = para.weeklyReport_tm_min;
  
  // String-Parameter
  doc["ssid"] = para.ssid;
  doc["password"] = para.password;
  doc["ntpServer"] = para.ntpServer;
  doc["smtp_AUTH_EMAIL"] = para.smtp_AUTH_EMAIL;
  doc["smtp_AUTH_PASSWORD"] = para.smtp_AUTH_PASSWORD;
  doc["smtp_RECIPIENT_EMAIL"] = para.smtp_RECIPIENT_EMAIL;
  
  String response;
  serializeJson(doc, response);
  
  HTTPserver.send(200, "application/json", response);
}

//Route zum Speichern der Parameter
void handleSaveParams() {
  // Float-Parameter
  if (HTTPserver.hasArg("temp_alarmhigh_treshold")) {
    para.temp_alarmhigh_treshold = HTTPserver.arg("temp_alarmhigh_treshold").toFloat();
  }
  if (HTTPserver.hasArg("temp_alarmlow_treshold")) {
    para.temp_alarmlow_treshold = HTTPserver.arg("temp_alarmlow_treshold").toFloat();
  }
  
  if (HTTPserver.hasArg("Temp_Update_Interval_LIVE_mins")) {
    para.Temp_Update_Interval_LIVE_mins = HTTPserver.arg("Temp_Update_Interval_LIVE_mins").toInt();
  }
  if (HTTPserver.hasArg("Temp_Update_Interval_SIM_mins")) {
    para.Temp_Update_Interval_SIM_mins = HTTPserver.arg("Temp_Update_Interval_SIM_mins").toInt();
  }
  if (HTTPserver.hasArg("backupInterval_mins")) {
    para.backupInterval_mins = HTTPserver.arg("backupInterval_mins").toInt();
  }
  if (HTTPserver.hasArg("maxcooling_mins")) {
    para.maxcooling_mins = HTTPserver.arg("maxcooling_mins").toInt();
  }
  
  // Boolean-Parameter
  para.simulateSensor = HTTPserver.hasArg("simulateSensor");
  para.emailme = HTTPserver.hasArg("emailme");
  para.skipmail = HTTPserver.hasArg("skipmail");
  para.serialout = HTTPserver.hasArg("serialout");
  para.measure = HTTPserver.hasArg("measure");
  
  // Integer-Parameter
  if (HTTPserver.hasArg("pwmfrequency")) {
    para.pwmfrequency = HTTPserver.arg("pwmfrequency").toInt();
  }
  if (HTTPserver.hasArg("weeklyReport_tm_wday")) {
    para.weeklyReport_tm_wday = HTTPserver.arg("weeklyReport_tm_wday").toInt();
    Serial.println(HTTPserver.arg("weeklyReport_tm_wday").toInt());
    Serial.println(para.weeklyReport_tm_wday);
  }
  if (HTTPserver.hasArg("weeklyReport_tm_hour")) {
    para.weeklyReport_tm_hour = HTTPserver.arg("weeklyReport_tm_hour").toInt();
  }
  if (HTTPserver.hasArg("weeklyReport_tm_min")) {
    para.weeklyReport_tm_min = HTTPserver.arg("weeklyReport_tm_min").toInt();
  }
  
  // String-Parameter
  if (HTTPserver.hasArg("ssid")) {
    HTTPserver.arg("ssid").toCharArray(para.ssid, sizeof(para.ssid));
  }
  if (HTTPserver.hasArg("password")) {
    HTTPserver.arg("password").toCharArray(para.password, sizeof(para.password));
  }
  if (HTTPserver.hasArg("ntpServer")) {
    HTTPserver.arg("ntpServer").toCharArray(para.ntpServer, sizeof(para.ntpServer));
  }
  if (HTTPserver.hasArg("smtp_AUTH_EMAIL")) {
    HTTPserver.arg("smtp_AUTH_EMAIL").toCharArray(para.smtp_AUTH_EMAIL, sizeof(para.smtp_AUTH_EMAIL));
  }
  if (HTTPserver.hasArg("smtp_AUTH_PASSWORD")) {
    HTTPserver.arg("smtp_AUTH_PASSWORD").toCharArray(para.smtp_AUTH_PASSWORD, sizeof(para.smtp_AUTH_PASSWORD));
  }
  if (HTTPserver.hasArg("smtp_RECIPIENT_EMAIL")) {
    HTTPserver.arg("smtp_RECIPIENT_EMAIL").toCharArray(para.smtp_RECIPIENT_EMAIL, sizeof(para.smtp_RECIPIENT_EMAIL));
  }
  newEEPROMdata = true;
  saveToEEPROM();
  HTTPserver.send(200, "text/html", "Save Ok.");
}



void handleRestart() {
  saveToEEPROM();
  HTTPserver.send(200, "text/html", "Aquacontrol wird in 3 Sekunden neu gestartet.");
  Serial.println("Neustart in 3 Sekunden...");
  delay(3000);
  ESP.restart();
  Serial.println("Neustart hat nicht geklappt...");
  delay(3000);
}

void handleStatus() {
  DynamicJsonDocument doc(256);
  
  doc["time"] = timeClient.getFormattedTime();
  doc["date"] = getFormattedDate();
  #ifdef MONITOR
  doc["temp"] = String(temperature.current);
  #endif
  #ifdef CONTROL
  doc["LIGHTPWM"] = Light.brightness;
  doc["COOLINGPWM"] = Cooling.brightness;
  doc["RELAYLIGHT"] = (digitalRead(RELAYLIGHT_PIN) == LOW) ? 1 : 0;
  doc["RELAYCO2"] = (digitalRead(RELAYCO2_PIN) == LOW) ? 1 : 0;
  doc["RELAYMOON"] = (digitalRead(RELAYMOON_PIN) == LOW) ? 1 : 0;
  #endif
  
  String json;
  serializeJson(doc, json);
  HTTPserver.send(200, "application/json", json);
}

#ifdef CONTROL
void setRelay(uint8_t pin, bool state) {
  digitalWrite(pin, state ? HIGH : LOW);
  delay(100);
}

void handleSetDeviceState() {
  if(HTTPserver.hasArg("plain")) {
    DynamicJsonDocument doc(128);
    DeserializationError error = deserializeJson(doc, HTTPserver.arg("plain"));
    
    if(error) {
      HTTPserver.send(400, "application/json", "{\"status\":\"parse error\"}");
      return;
    }
    uint8_t currentdevice = doc["device"];
    uint16_t value = doc["value"];
    bool state = value > 0; // doc["state"];
    switch(currentdevice){
      case DEVICE_PWMLIGHT:
        Light.fade(value, 500);
        break;
      case DEVICE_COOLING:
        Cooling.fade(value, 500);
        break;
      case DEVICE_LIGHT:
        setRelay(RELAYLIGHT_PIN, state ? LOW : HIGH); // active Low
        break;
      case DEVICE_CO2:
        setRelay(RELAYCO2_PIN, state ? LOW : HIGH); // active Low
        break;
      case DEVICE_MOON:
        setRelay(RELAYMOON_PIN, state ? LOW : HIGH); // active Low TBD
        break;
      default:
        if (para.serialout) Serial.printf("Unbekanntes Device: %d\n", currentdevice);
        break;
    }
    HTTPserver.send(200, "application/json", "{\"status\":\"ok\"}");
  } else {
    HTTPserver.send(400, "application/json", "{\"status\":\"missing data\"}");
  }
}

void checkSchedule(uint8_t hour2check, uint8_t minute2check) {
  for (uint8_t i = 0; i < Scheduleentries_count; i++) {
    if (Scheduleentries[i].hour == hour2check && Scheduleentries[i].minute == minute2check) {
      if (para.serialout) Serial.printf("chkentry: %s, %s:%s device: %s, brightness: %s\n", String(i), String(Scheduleentries[i].hour),String(Scheduleentries[i].minute),String(Scheduleentries[i].device),String(Scheduleentries[i].brightness));
      uint8_t currentdevice = Scheduleentries[i].device;
      uint8_t blevel = Scheduleentries[i].brightness;
      uint16_t targetBrightness = lightLevels[blevel];
      switch(currentdevice){
        case DEVICE_PWMLIGHT:
          if (blevel < 7) { // nur gültige Lightlevel nehmen
            Light.fade(targetBrightness, uint32_t(Scheduleentries[i].fadeMinutes * 60000) + 500);
            // Serial.println(timeClient.getFormattedTime());
            if (para.serialout) Serial.printf("DEVICE_PWMLIGHT Fade: %s / 1023\n", String(targetBrightness));
          }
          break;
        case DEVICE_COOLING:
          Cooling.fade(targetBrightness, uint32_t(Scheduleentries[i].fadeMinutes * 60000) + 500);
          if (para.serialout) Serial.printf("DEVICE_COOLING Fade: %s / 1023\n", String(targetBrightness));
          break;
        case DEVICE_LIGHT:
          if (para.serialout) Serial.printf("DEVICE_LIGHT: %s\n", String(blevel));
          setRelay(RELAYLIGHT_PIN, (blevel > 0) ? LOW : HIGH); // active Low
          break;
        case DEVICE_CO2:
          if (para.serialout) Serial.printf("DEVICE_CO2: %s\n", String(blevel));
          setRelay(RELAYCO2_PIN, (blevel > 0) ? LOW : HIGH); // active Low
          break;
        case DEVICE_MOON:
          if (para.serialout) Serial.printf("DEVICE_MOON: %s\n", String(blevel));
          setRelay(RELAYMOON_PIN, (blevel > 0) ? LOW : HIGH); // active Low
          break;
        default:
          if (para.serialout) Serial.printf("Unbekanntes Device: %d\n", currentdevice);
          break;
      }
    }
  }
}

void setOutputsAccording2Schedule(uint8_t hour2check, uint8_t minute2check) {
  // läuft nach dem Aufstarten, um den letzten Zustand der Relais bzw. Licht herzustellen
  for (uint8_t hour = 0; hour <= hour2check; hour++) {
    uint8_t max_minute = (hour == hour2check) ? minute2check : 59;
    for (uint8_t minute = 0; minute <= max_minute; minute++) {
      checkSchedule(hour, minute);
    }
  }
}

void checkmaxcoolingTime (){
  //return;
  static unsigned long CoolingshutoffMillis = 0;
  if (Cooling.brightness > 0){
    if (CoolingshutoffMillis == 0) {
      CoolingshutoffMillis = millis() + para.maxcooling_mins * 60000;
      if (para.serialout) Serial.printf("Cooling (Ventilator) ein erkannt.\n");
    } else if ((millis() > CoolingshutoffMillis) || (digitalRead(COOLING_OFF_PIN) == LOW)) {
      Cooling.fade(0,5); // sofort aus.
		  Cooling.upd();
		  delay(10);
		  setRelay(COOLING_PWMPIN, LOW);
      CoolingshutoffMillis = 0;
      if (para.serialout) Serial.printf("Cooling (Ventilator) automatisch ausgeschaltet.");
      if (digitalRead(COOLING_OFF_PIN) == LOW) {
        if (para.serialout) Serial.printf("Wasserstand niedrig.");
      }
    }
  }
}

void offAll(void){
  if (Light.brightness != 0) {
    Light.fade(0,5); // sofort aus.
    Light.upd();
    delay(10);
    setRelay(LIGHT_PWMPIN, LOW);
  }
  if (Cooling.brightness != 0) {
    Cooling.fade(0,5); // sofort aus.
    Cooling.upd();
    delay(10);
    setRelay(COOLING_PWMPIN, LOW);
  }
  setRelay(RELAYLIGHT_PIN, HIGH); // active Low
  setRelay(RELAYCO2_PIN, HIGH); // active Low
}
#endif // CONTROL

// Hauptfunktionen
void setup() {
  //StartupEndMillis = millis() + STARTUPMILLIS;
  const int PWM_FREQ = 4000; // 10000; // 10khz kaum hörbar 1000; // 1kHz
  const int PWM_RESOLUTION = 10; // 10-bit (0-1023)
  digitalWrite(LIGHT_PWMPIN, LOW);  // Zuerst sicher auf LOW setzen
  digitalWrite(COOLING_PWMPIN, LOW); // Zuerst sicher auf LOW setzen
  digitalWrite(RELAYLIGHT_PIN, HIGH);  // Zuerst sicher auf HIGH (active Low) setzen
  digitalWrite(RELAYCO2_PIN, HIGH);  // Zuerst sicher auf HIGH (active Low) setzen
  digitalWrite(RELAYMOON_PIN, HIGH);  // // Zuerst sicher auf HIGH (active Low) setzen
  delay(100);
  pinMode(LIGHT_PWMPIN, OUTPUT);
  pinMode(COOLING_PWMPIN, OUTPUT);
  pinMode(RELAYLIGHT_PIN, OUTPUT);
  pinMode(RELAYCO2_PIN, OUTPUT);
  pinMode(RELAYMOON_PIN, OUTPUT);
  pinMode(COOLING_OFF_PIN, INPUT);
  digitalWrite(COOLING_OFF_PIN, HIGH); // Pullup
  delay(100);
  //const int PWM_FREQ = para.pwmfrequency; // 10000; // 10khz kaum hörbar 1000; // 1kHz
  //analogWriteFreq((int)para.pwmfrequency);
  analogWriteFreq(1000);
  analogWriteRange(1 << PWM_RESOLUTION);
  #ifdef CONTROL
  Light.init();
  Cooling.init();
  #endif

  delay(500);
  Serial.begin(115200);
  if (para.serialout) Serial.println("Setup Start.");
  if (para.serialout) Serial.println(RAMinfo());
  WiFi.mode(WIFI_STA);   // Setze Modus zurück (STA = Client-Modus)
  //WiFi.begin("abc","123"); // begin ohne Daten verwendet die zuletzt genutzten Daten
  //WiFi.begin(); // ohne ssid -> zuletzt verwendete Zugangsdaten verwenden
  WiFi.begin(ssid, password);
  Serial.printf("WiFi begin (ohne Passwort)\n");
  uint8_t retrys = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    if (retrys == 10){
      loadSSIDfromEEPROM(); // EEPROM darf aus irgendeinem Grund erst gelesen werden, nachdem WIFI teilw. initialisiert wurde.
      WiFi.begin(para.ssid, para.password);
    }
    retrys++;
    if (para.serialout) Serial.print(".");
  }
  if (para.serialout) Serial.printf("\nWiFi verbunden, IP: %s\n", WiFi.localIP().toString().c_str());

  loadFromEEPROM(); // EEPROM darf aus irgendeinem Grund erst gelesen werden, nachdem WIFI teilw. initialisiert wurde.
  timeClient.begin();
  while(!timeClient.update()) { // Erste Zeitabfrage mit Wiederholungsmechanismus
    if (para.serialout) Serial.println("Zeitabfrage fehlgeschlagen, erneuter Versuch...");
    timeClient.forceUpdate();
    delay(1000);
  }
  adjustForDST();
	if (para.serialout) Serial.println(timeClient.getFormattedTime());

  #ifdef CONTROL
  analogWriteFreq((uint32_t) para.pwmfrequency);
  delay(100);
  setOutputsAccording2Schedule(timeClient.getHours(),timeClient.getMinutes());
  #endif

  #ifdef MONITOR
  // Temperatursensor initialisieren
  if (para.simulateSensor) {
    Temp_Update_Interval_mins = para.Temp_Update_Interval_SIM_mins;
  } else {
    sensors.begin();
    delay(500);
    sensors.setResolution(TEMP_SENSOR_RESOLUTION);
    if (para.measure) updateTemperature();
  }
  #endif

  // Webserver-Routen
  #ifdef CONTROL
  HTTPserver.on("/", HTTP_GET,handleLight);
  HTTPserver.on("/light", HTTP_GET,handleLight);
  HTTPserver.on("/schedule", HTTP_GET,handleConfigSchedule);
  HTTPserver.on("/getSchedule", HTTP_GET, handleGetSchedule);
  HTTPserver.on("/setSchedule", HTTP_POST, handleSetSchedule);
  HTTPserver.on("/setDeviceState", HTTP_POST, handleSetDeviceState);
  #endif
  #ifdef MONITOR
  // HTTPserver.on("/", HTTP_GET,handleinfo);
  HTTPserver.on("/email", HTTP_GET,handleSendemail);
  HTTPserver.on("/info", HTTP_GET,handleinfo);
  HTTPserver.on("/csv", HTTP_GET,handleCSV);
  HTTPserver.on("/save", HTTP_GET, handlesaveTemperatureToEEPROM);
  HTTPserver.on("/deletehistory321", HTTP_GET,handleDeleteHistory);
  #endif

  HTTPserver.on("/settings", HTTP_GET,handlesettings);
  HTTPserver.on("/restart", HTTP_GET,handleRestart);
  HTTPserver.on("/getParameters", HTTP_GET,handleGetParameters);
  HTTPserver.on("/saveParams", HTTP_POST, handleSaveParams);
  HTTPserver.on("/ram", HTTP_GET,handleRAMinfo);
  HTTPserver.on("/status", HTTP_GET, handleStatus);
	HTTPserver.begin();
  delay(1000);
  if (para.serialout) Serial.printf("HTTP-Sever erstmalig gestartet\n");

  #ifdef MONITOR
      // LittleFS initialisieren
  if (!LittleFS.begin()) {
      Serial.println("LittleFS Mount fehlgeschlagen");
      return;
  }
  Serial.println("LittleFS Mount erfolgreich");
  if (para.emailme) {
    sendEmail(true); // Nach Neustart Mail senden
    if (para.serialout) Serial.printf("Email Neustart OK\n");
  }
  #endif

  if (para.serialout) Serial.println("Setup beendet.");
}

void loop() {
  unsigned long currentMillis = millis();
  static unsigned long lastTempCheck = 0;
  static unsigned long lastBackup = 0;
  
  static int lastSyncedDay = -1;
  static int lastSec = -1;
  static int lastMin = -1;
  static int lastHour = -1;

  HTTPserver.handleClient();

  #ifdef CONTROL 
  if (timeClient.getSeconds() != lastSec){ // Sekündlich prüfen (z.B. Sekündlich Helligkeit anpassen)
    lastSec = timeClient.getSeconds();
    if (Light.upd()) {
      if (Light.brightness == 0){
        setRelay(LIGHT_PWMPIN, LOW);
      } else if (Light.brightness == 1023) {
        setRelay(LIGHT_PWMPIN, HIGH);
      } else {
        analogWrite(LIGHT_PWMPIN, int(Light.brightness));     // PWM auf 0 setzen
        delay(10); //Das Delay gibt dem ESP8266 angeblich (Nachtrag: tatsächlich nötig!) Zeit, den Duty-Cycle sauber umzuschalten.
      }
    };
    if (Cooling.upd()) {
      if (Cooling.brightness == 0){
        setRelay(COOLING_PWMPIN, LOW);
      } else if (Cooling.brightness == 1023) {
        setRelay(COOLING_PWMPIN, HIGH);
      } else {
        analogWrite(COOLING_PWMPIN, int(Cooling.brightness));     // PWM auf 0 setzen
        delay(10); 
      }
    };
  }
  #endif

  if (timeClient.getMinutes() != lastMin){ // Minütlich prüfen
    lastMin = timeClient.getMinutes();
    #ifdef CONTROL
    checkSchedule(timeClient.getHours(),timeClient.getMinutes());
    checkmaxcoolingTime();
    // Autooff für Alles um 23:00
    if ((timeClient.getHours() == 23) && (timeClient.getMinutes() == 00)) {
      offAll(); 
    }
    #endif
    // TimeSync 1x am Tag 5:15 - SommerWinterzeit-Zeitumstellung ist dann abgeschlossen.
    if ((getDay() != lastSyncedDay) && (timeClient.getHours() == 5) && (timeClient.getMinutes() == 15)) {
      reconnectWifi();
      if (timeClient.update()){
        lastSyncedDay = getDay();
        adjustForDST(); // Sommer-Winterzeit-Umstellung
      };
    }
    #ifdef MONITOR
    unsigned long currentMillis = millis();
    if (para.measure) {
      // Temperatur aktualisieren
      if(currentMillis - lastTempCheck >= (Temp_Update_Interval_mins * 60000)) {
        lastTempCheck = currentMillis;
        updateTemperature();
      }
      // 4-Stündliche Sicherung
      if(millis() - lastBackup >= (para.backupInterval_mins * 60000)) {
        lastBackup = millis();
        saveToEEPROM(); // Save passiert nur bei neuen Daten
      }
      if (para.emailme) { // Wöchentlicher Versand
        checkScheduledEmail(); 
      }
    }
    #endif
  }
  
  /* if (timeClient.getHours() != lastHour){ // stündlich prüfen
    lastHour = timeClient.getHours();
  } */
  
  delay(100); // Yield to background tasks
}