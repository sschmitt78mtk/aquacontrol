#include <TimeLib.h> // F�r month(), day(), weekday()
#include <NTPClient.h>
extern NTPClient timeClient; 

int getDay() {
  // Nutzt den internen Zeitpuffer des NTPClients
  time_t rawtime = timeClient.getEpochTime();
  struct tm *ti;
  ti = localtime(&rawtime);
  return ti->tm_mday + 1;
}

int getMonth() {
  // Nutzt den internen Zeitpuffer des NTPClients
  time_t rawtime = timeClient.getEpochTime();
  struct tm *ti;
  ti = localtime(&rawtime);
  return ti->tm_mon + 1;
}

int getDayOfWeek() {
  time_t rawtime = timeClient.getEpochTime();
  struct tm *timeinfo;
  timeinfo = localtime(&rawtime);
  return timeinfo->tm_wday; // 0=Sonntag, 6=Samstag
}

String getFormattedDate() {
  time_t now = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  char buffer[30];
  strftime(buffer, sizeof(buffer), "%d.%m.%Y", &timeinfo);
  return String(buffer);
}

time_t getCorrectTimestamp() {
  return timeClient.getEpochTime(); // Bereits korrigierte Zeit
}

int getLastSundayOfMonth(int year, int month) {
    // March has 31 days, October has 31 days
    int lastDay = 31;
    // Zeller's congruence for last day of month
    int m = month < 3 ? month + 12 : month;
    int y = month < 3 ? year - 1 : year;
    int dayOfWeek = (lastDay + 13*(m+1)/5 + y%100 + y/400 + 5*(y/100) - y/100) % 7;
    return lastDay - dayOfWeek;
}

void adjustForDST() { // Sommer-Winterzeitumstellung
  timeClient.update();
  time_t rawtime = timeClient.getEpochTime();
  struct tm *ti = localtime(&rawtime);
  int year = ti->tm_year + 1900; 
  int month = ti->tm_mon + 1;
  int day = ti->tm_mday;     // Tag im Monat (1..31)
  int dayOfWeek = ti->tm_wday; // 0 = Sonntag, 1 = Montag, ..., 6 = Samstag
  Serial.println(year);
  Serial.println(month);
  Serial.println(day);
  Serial.println(dayOfWeek);
  int lastSundayMarch = getLastSundayOfMonth(year, 3);
  int lastSundayOctober = getLastSundayOfMonth(year, 10);
  // DST-Regeln f�r Deutschland
  bool isDST = (month > 3 && month < 10) ||
           (month == 3 && (day > lastSundayMarch || (day == lastSundayMarch ))) ||
           (month == 10 && (day < lastSundayOctober || (day == lastSundayOctober )));
  if (isDST) {
    Serial.println("Sommerzeit");
  } else {
    Serial.println("Normalzeit");
  }

  timeClient.setTimeOffset(isDST ? 7200 : 3600); // MESZ oder MEZ
}
