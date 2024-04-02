// The Y-Box is a convenient moral booster. It consists of what appears to be just a little clock with a single button on it.
// When the demoralised officeworker hits the button, it shows them the current amount of money they have earned so far this week.
//
// Basically it answers the question "why the hell am I working here again?" with a simple financial answer.
//
// It does need to know which weekdays are work days, and how many hours a week are worked, as well as the start and end times,
// as well as the weekly wage, either before or after tax (the "take home" amount), for it to be fully effective. These are hardcoded
// in, although if anyone wants to add this into a quick web-form instead, and save it to EPROM memory, be my guest.
//
// Original idea by Jack Machiela, (c) 2024
// 
// This version was released on April 2nd, 2024 via Github
// Latest version can be found on 

#include <WiFiManager.h>      // Install "WifiManager by tzapu" - lib at https://github.com/tzapu/WiFiManager - tested at v2.0.17
#include <ezTime.h>           // Install "ezTime by Rop Gongrijp" - lib at https://github.com/ropg/ezTime, docs at https://awesomeopensource.com/project/ropg/ezTime - tested at v0.8.3
#include <SPI.h>              // Standard library
#include <TFT_eSPI.h>         // Install "TFT_eSPI by Bodmer". Hardware-specific library for display - tested at v2.5.43. Full modification instructions on my github.
#include <Button.h>           // install "Button by Michael Adams" - lib at https://github.com/madleech/Button - tested at v1.0.0


// Define the end-user's starting time and hourly salary:
// Example:  start time:   9:00
//           lunch start: 12:00
//           lunch end:   12:30
//           end time:    17:00            // so, 7.5 hours a day
//           Working days 1234060          // mon=1, tue=2, etc   // total 30 hours/week
//           pay = $ 25.00 per hour        // before tax
//        or pay = $ 18.345645 per hour$   // Or after tax, your choice. Decimal points are fine (divide weekly wage on your bank account by your weekly hours)

int workStartHM  =  800 ;                     // These are integer values for times in 24h format, so 8:30am = 830, 4:30pm = 1630, etc.
int lunchStartHM = 1200 ;                     // 
int lunchEndHM   = 1230 ;                     // 
int workEndHM    = 1600 ;                     // 

String workingDays = "1234060" ;              // String containing digits 1-7 for the days actually worked, and zero for days not worked.
                                              // Mon=1, Sun=7
                                              // So, if you work Mon thru Thu plus Sat, it's "1234060"
                                              //
float hourlyPay = 18.345645 ;                 // We're assuming a normal week's pay divided by the standard working hours. Up to you if you want before or after tax.


float decTime(float number){ return int (number / 100) + ((( number / 100) - int(number / 100))/0.6); };              // This function turns 0730 (time) into 7.5 (hours)
float dailyHours = ((decTime(workEndHM) - decTime(workStartHM)) - (decTime(lunchEndHM) - decTime(lunchStartHM))) ;    // This defines an standard number of hours worked per day

int todayWeekday ;


// Define some wibbley wobbley timey wimey stuff
  String TimeZoneDB = "Pacific/Auckland";              // Olson format "Pacific/Auckland" - Posix: "NZST-12NZDT,M9.5.0,M4.1.0/3" (as at 2020-06-18)
                                                       // For other Olson codes, go to: https://en.wikipedia.org/wiki/List_of_tz_database_time_zones 
  String localNtpServer = "nz.pool.ntp.org";           // [Optional] - See full list of public NTP servers here: http://support.ntp.org/bin/view/Servers/WebHome
  Timezone myTimeZone;                                 // Define this as a global so all functions have access to correct timezoned time


// Define some screen stuff
  TFT_eSPI tft = TFT_eSPI();  // Invoke custom library, pins defined in C:\Shares\Application Folders (Falcon)\Arduino\libraries\TFT_eSPI\User_Setup.h
                          
// Define the button
  Button whyButton(D3); // Connect your button between pin 3 and GND

  static int previousHour   = -1;           // nonsense amounts so they fail the first test and get set properly
  static int previousMinute = -1;
  static int previousSecond = -1;

  static bool colonOn = true;

  int currentMode = 1;                // display modes: 1 = Show current time [default]
                                      //                2 = Show Y value


void setup(void) {
  // Initialise the screen
  tft.init();
  tft.setRotation(3);
  initScreen();
  tft.setTextColor(TFT_YELLOW, TFT_BLACK); // Note: the new fonts do not draw the background colour
  tft.setCursor (25, 20);
  tft.setTextSize(2);
  tft.print("The Y-Box");
  tft.setCursor (30, 50);
  tft.setTextSize(1);
  tft.print("By Jack Machiela");
  tft.setCursor (0, 0);
  tft.setTextSize(1);

  Serial.begin(74880);       // native speed of the WeMos Mini D1 R1

  // Start the Wifi stuff and initialise the RT clock
  WiFiManager wifiManager;             // Start a Wifi link
  wifiManager.autoConnect("Y-Box");    //   (on first use: will set up as a Wifi name; set up by selecting new Wifi network on iPhone for instance)

  setInterval(600);                    // [Optional] - How often should the time be synced to NTP server
  setDebug(INFO);                      // [Optional] - Print some info to the Serial monitor
  setServer(localNtpServer);           // [Optional] - Set the NTP server to the closest pool
  waitForSync();                       // Before you start the main loop, make sure the internal clock has synced at least once.
  myTimeZone.setLocation(TimeZoneDB);  // Set the Timezone
  myTimeZone.setDefault();             // Set the local timezone as the default time, so all time commands should now work local.

  whyButton.begin();           // initialise the button readings

  initScreen();

}


void loop() {
    if (whyButton.released()) {
       currentMode++ ;
        if (currentMode > 2) currentMode = 1;
        initScreen();
        Serial.println("whyButton pressed and released - currentMode = " + String(currentMode));
     }
  if (currentMode == 1) {
    modeTimeDisplay();
  }
  if (currentMode == 2) {
    modeWhyDisplay();
  }
  events();                        // Check if any scheduled ntp events, including NTP resyncs.
}

void checkNtpStatus() {
  if (timeStatus() == timeSet)       Serial.println("Status: Time Set.      ");
  if (timeStatus() == timeNotSet)    Serial.println("Status: Time NOT Set.  ");
  if (timeStatus() == timeNeedsSync) Serial.println("Status: Time needs Syncing.      ");
}

void initScreen() {
    tft.fillScreen(TFT_BLACK);
    tft.drawRect(0, 0, tft.width(), tft.height(), TFT_GREEN);
    tft.setTextSize(1);
    previousHour   = -1;
    previousMinute = -1;
    previousSecond = -1;
}


void modeTimeDisplay(){
  if (second() != previousSecond) {
    previousSecond = second();
    colonOn = !colonOn;               // Toggle the ":" character
    if (colonOn) { // Flash the colon               (second()%2)
      tft.setTextColor(0x39C4, TFT_BLACK);
      tft.drawChar(':',74,8,7);                            // xpos+= 
    }
    else {
      tft.setTextColor(0xFBE0, TFT_BLACK);
      tft.drawChar(':',74,8,7);
    }
    if (minute() != previousMinute) {
      previousMinute = minute();
      // Font 7 is to show a pseudo 7 segment display, and only contains characters [space] 0 1 2 3 4 5 6 7 8 9 0 : .
      tft.setTextColor(0x39C4, TFT_BLACK);  // Leave a 7 segment ghost image
      tft.drawString("88",87,8,7); // Overwrite the text to clear it               ("88:88",across,down,7)
      tft.setTextColor(0xFBE0); // Orange
      tft.drawString(((minute() < 10) ? "0" + String(minute()) : String(minute())),87,8,7);
      if (hour()   != previousHour)   {
        previousHour = hour();
        // Font 7 is to show a pseudo 7 segment display, and only contains characters [space] 0 1 2 3 4 5 6 7 8 9 0 : .
        tft.setTextColor(0x39C4, TFT_BLACK);  // Leave a 7 segment ghost image
        tft.drawString("88",10,8,7); // Overwrite the text to clear it               ("88:88",across,down,7)
        tft.setTextColor(0xFBE0); // Orange
        tft.drawString(((hour() < 10) ? "0" + String(hour()) : String(hour())),10,8,7);
      }
    }
  }
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setCursor (30, 65);
  tft.setTextSize(1);
  tft.print(myTimeZone.dateTime("D, M j, Y")); // This uses the standard ADAFruit small font
}


void modeWhyDisplay(){
  int hours        = hour();   // Get current hours
  int minutes      = minute(); // Get current minutes
  int seconds      = second(); // Get current seconds
  int todayWeekday = ((weekday()-1 == 0) ? 7 : weekday()-1 ) ;                // EZ-TimeLib uses Sun as day 1; this works better if Mon=1 & Sun=7.
  int daysWorked = 0 ;                                                        // total number of days worked this week so far
  float hoursWorkedToday = 0 ;

  // first, calculate how many complete days this week so far (not counting today)
  for (int i = 1; i < todayWeekday; i++) {                                    // loop once for every day this week until today (but don't count today)
    if (String(workingDays.charAt(i-1)) == String(i)) { daysWorked ++ ; }     // check if the standard schedule contains that day's daynumber
  }
  float hoursWorkedThisWeek = (daysWorked * dailyHours ) ;                    // Set the number of hours worked until today

  // next, work out how many hours worked today so far (if it's a working day)
  if (String(workingDays.charAt(todayWeekday-1)) == String(todayWeekday)) {               // check if today is actually a working day
    // Check if its working hours right now
    int timeNowHM = ((hours*100) + (minutes) ) ;
    if ( timeNowHM >= workStartHM && timeNowHM < workEndHM ) {                   // if the time is during the working hours
      if ( timeNowHM < lunchStartHM || timeNowHM >= lunchEndHM ) {                 // if the time is not during lunch time
        float totalSeconds = (minutes * 60) + seconds ; // Calculate total seconds elapsed in the hour
        float hoursWorkedToday = hours + (totalSeconds / 3600) - decTime(workStartHM); // Calculate percentage of an hour
        if ( timeNowHM >= lunchEndHM ) hoursWorkedToday = (hoursWorkedToday  - (decTime(lunchEndHM) - decTime(lunchStartHM))) ;
        hoursWorkedThisWeek = hoursWorkedThisWeek + hoursWorkedToday ;
      } else {               // if it's a work day but it's lunchtime
        hoursWorkedThisWeek = hoursWorkedThisWeek + ( decTime(lunchStartHM) - (decTime(workStartHM)));        // doing it this way avoids rounding errors when the counter is at standstill
      }
    }
  if ( timeNowHM >= lunchEndHM ) hoursWorkedToday = (hoursWorkedToday  - (decTime(lunchEndHM) - decTime(lunchStartHM))) ;        // if it's after lunch, don't forget to subtract lunchtime as non-paid time
  if ( timeNowHM >= workEndHM ) hoursWorkedThisWeek = hoursWorkedThisWeek + dailyHours ;            // if today is finished, add an extra day worked to the totals
  }
  tft.setTextColor(TFT_RED, TFT_BLACK); // Note: the new fonts do not draw the background colour
  tft.setCursor (32, 32);                  //(across, down)
  tft.setTextSize(2);
  tft.print(String(hoursWorkedThisWeek * hourlyPay,4));               // Pay this week so far, to 4 decimal places - your choice to add a $ before it or not - if
                                                                      // you're in a shared office you may not want people to know that this is your earning per second
}
