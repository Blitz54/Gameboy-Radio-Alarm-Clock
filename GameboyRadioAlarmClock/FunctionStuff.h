#include <SPI.h>
#include <TFT_eSPI.h> // Hardware-specific library
#include <Wire.h>
#include "Free_Fonts.h" // Include the header file attached to this sketch
#include <RTClib.h>
#include <LittleFS.h>

extern RTC_DS3231 rtc;
extern TFT_eSPI tft;
extern int LCOLOR;
extern int DCOLOR;
extern fs::File file;
extern const uint16_t ColorPairs[][2];
extern int oldday;

const char daysOfWeek [7][12] = {
  "Sunday",
  "Monday",
  "Tuesday",
  "Wednesday",
  "Thursday",
  "Friday",
  "Saturday"
};

void freefontXYOpposite(int x, int y, int w, const GFXfont *f, char *msg)
{
  tft.setTextPadding(w);
  tft.setTextColor(LCOLOR, DCOLOR);
  tft.setFreeFont(f);
  tft.drawString(msg, x, y);
}

void freefontXY(int x, int y, int w, const GFXfont *f, char *msg)
{
  tft.setTextPadding(w);
  tft.setTextColor(DCOLOR, LCOLOR);
  tft.setFreeFont(f);
  tft.drawString(msg, x, y);
}

void LoadColor()
{
  file = LittleFS.open("/Colors.txt");
  String colorload = file.readString();
  Serial.println(colorload);
  LCOLOR = ColorPairs[colorload.toInt()][0];
  DCOLOR = ColorPairs[colorload.toInt()][1];
  Serial.println(LCOLOR);
  Serial.println(DCOLOR);
  file.close();
}

void readFile(fs::FS &fs, const char *path)
{
  Serial.printf("Reading file: %s\r\n", path);

  file = fs.open(path);
  if (!file || file.isDirectory())
  {
    Serial.println("- failed to open file for reading");
    return;
  }

  Serial.println("- read from file:");
  while (file.available())
  {
    Serial.print(file.read());
  }
  file.close();
}

void writeFile(fs::FS &fs, const char *path, char *message)
{ //const char *message is default
  Serial.printf("Writing file: %s\r\n", path);

  file = fs.open(path, FILE_WRITE);
  if (!file)
  {
    Serial.println("- failed to open file for writing");
    return;
  }
  if (file.print(message))
  {
    Serial.println("- file written");
  }
  else
  {
    Serial.println("- write failed");
  }
  file.close();
}

void UpdateDate()
{
  DateTime now = rtc.now();
  if (now.dayOfTheWeek() != oldday)
  {
    oldday = now.dayOfTheWeek();    
    char CurrentMonth[4] = "MMM";
    now.toString(CurrentMonth);
    char CurrentDate[20] = "DD, YYYY";
    sprintf(CurrentDate, "%s %d, %d", CurrentMonth, now.day(), now.year());
    freefontXY(140, 57, 120, FMB12, CurrentDate);
    char CurrentDay[15];
    sprintf(CurrentDay, "%s", daysOfWeek[oldday]);
    freefontXY(140, 77, 120, FMB12, CurrentDay);
  }
}