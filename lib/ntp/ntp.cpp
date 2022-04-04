

/*
  NTP TZ DST - bare minimum
  NetWork Time Protocol - Time Zone - Daylight Saving Time

  Our target for this MINI sketch is:
  - get the SNTP request running
  - set the timezone
  - (implicit) respect daylight saving time
  - how to "read" time to be printed to Serial.Monitor
  
  This example is a stripped down version of the NTP-TZ-DST (v2)
  and works for ESP8266 core 2.7.4

  by noiasca
  2020-09-22
*/
#include <Arduino.h>

/* Necessary Includes */
#include <time.h>                   // time() ctime()
#include <coredecls.h>

// #include "ntp.h"

/* Globals */
time_t now;                         // this is the epoch

struct tm tm;

// tm tm;                              // the structure tm holds time information in a more convient way

constexpr uint32_t SYNC_INTERVAL = 12;                           // NTP Sync Interval in Stunden einstellen
bool timeSync;

/* Configuration of NTP */
#define MY_NTP_SERVER "pool.ntp.org"
#define MY_NTP_SERVER1 "de.pool.ntp.org"

#define MY_TZ  "CET-1CEST,M3.5.0,M10.5.03"
#define MY_TZ2 "CET-1CEST,M3.5.0/02,M10.5.0/03"  
#define MY_TZ3 "CET-1CEST,M3.5.0,M10.5.0/3" 



void showTime() {
  time(&now);                       // read the current time
  localtime_r(&now, &tm);           // update the structure tm with the current time
  Serial.print("year:");
  Serial.print(tm.tm_year + 1900);  // years since 1900
  Serial.print("\tmonth:");
  Serial.print(tm.tm_mon + 1);      // January = 0 (!)
  Serial.print("\tday:");
  Serial.print(tm.tm_mday);         // day of month
  Serial.print("\thour:");
  Serial.print(tm.tm_hour);         // hours since midnight  0-23
  Serial.print("\tmin:");
  Serial.print(tm.tm_min);          // minutes after the hour  0-59
  Serial.print("\tsec:");
  Serial.print(tm.tm_sec);          // seconds after the minute  0-61*
  Serial.print("\twday");
  Serial.print(tm.tm_wday);         // days since Sunday 0-6
  if (tm.tm_isdst == 1)             // Daylight Saving Time flag
    Serial.print("\tDST");
  else
    Serial.print("\tstandard");
  Serial.println();

}
void time_is_set() {                                             // Diese Funktion wird als Rückruf festgelegt, wenn Zeitdaten abgerufen wurden.
  timeSync = true;                                               // "timeSync" bleibt ab jetzt auf true um den Programmablauf nicht zu verzögern.
  Serial.println("********* NTP Server Timestap Synchronisation  *********");
}

// Eine schwache Funktion ist bereits definiert und gibt 0 zurück (RFC 4330 Best Practices Verletzung)
uint32_t sntp_startup_delay_MS_rfc_not_less_than_60000 () {      //  Optionale Funktion, SNTP-Startverzögerung ändern
  return random(INT32_MAX) % 2000;
}

uint32_t sntp_update_delay_MS_rfc_not_less_than_15000() {        // Optionale Funktion, für den Individuellen SNTP Update Intervall. Standart ist jede Stunde.
  return SYNC_INTERVAL * 36e5;                                   // SNTP-Aktualisierungsverzögerung ändern.
}

String localTime() {
  static char buf[26];                                           // je nach Format von "strftime" eventuell anpassen
  static time_t previous;
  time_t now = time(NULL);
  if (now != previous) {
    previous = now;
    if (timeSync) {
      localtime_r(&now, &tm);
      /* Verwendungbeispiele
        Serial.println(DAY_NAMES[tm.tm_wday]);                     // druckt den aktuellen Tag
        Serial.println(MONTH_NAMES[tm.tm_mon]);                    // druckt den aktuellen Monat
        Serial.println(DAY_SHORT[tm.tm_wday]);                     // druckt den aktuellen Tag (Abk.)
        Serial.println(MONTH_SHORT[tm.tm_mon]);                    // druckt den aktuellen Monat (Abk.)
      */
      strftime(buf, sizeof(buf), "%H:%M", &tm);   // http://www.cplusplus.com/reference/ctime/strftime/
      Serial.println(buf);
    }
    else {
      snprintf(buf, sizeof(buf), R"(["Warte","auf Sync"])");     // Optional für die Anzeige auf der Webseite
      Serial.println("Warten auf NTP Synchronisation!");
    }
  }
  return buf;
}



void setuptime() {

    configTime(MY_TZ, MY_NTP_SERVER,MY_NTP_SERVER1); // --> Here is the IMPORTANT ONE LINER needed in your sketch!
    settimeofday_cb(time_is_set);   
}

void looptime () {
  showTime();
  
}