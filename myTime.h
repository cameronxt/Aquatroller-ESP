#ifndef myTime_h
#define myTime_h

#include <Arduino.h>
#include "include/time.h"


// constants for number of seconds in standard units of time
const unsigned long SecondsPerHour = 60UL * 60;
const unsigned long SecondsPerMinute = 60UL;

struct TimeData {
// NFS server
  char ntpServer[32] = "pool.ntp.org";
  long  gmtOffset_sec = -21400;
  int   daylightOffset_sec = 3600;

};

class MyTime {

  TimeData timeData;  // time preference struct
  MyTime();

  void init();

  // Updates internal rtc
  tm getNtpTime();
  

  unsigned long getTimeInSeconds(int hours, int minutes, int seconds);
  unsigned long getTimeInSeconds(tm* getTime);
  unsigned long getTimeInSeconds();

};

#endif