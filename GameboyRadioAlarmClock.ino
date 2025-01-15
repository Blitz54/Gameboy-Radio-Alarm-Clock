//                Enable these in User_Setup.h
//#define ILI9341_DRIVER       // Generic driver for common displays
//#define TFT_RGB_ORDER TFT_RGB  // Colour order Red-Green-Blue
//                Enable these in User_Setup_Select.h
//#include <User_Setups/Setup42_ILI9341_ESP32.h>           // Setup file for ESP32 and SPI ILI9341 240x320

#include <SPI.h>
#include <TFT_eSPI.h> // Hardware-specific library
#include <Wire.h>
#include "esp_system.h" 
#include "freertos/semphr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "PublicPixel20.h"
#include "Free_Fonts.h" // Include the header file attached to this sketch
#include <RTClib.h>
#include <LittleFS.h>
#include "FunctionStuff.h"
#include <ezButton.h>
#include "DacESP32.h"

#define BLACK     0x0000
#define WHITE     0xFFFF
#define RED       0xF800
#define ORANGE    0xFC00
#define LGREEN    0x07E0
#define DGREEN    0x0320

TFT_eSPI tft = TFT_eSPI(320, 240);//we must provide size otherwise it doesn't use full screen
String formattedTime;
String read_;
String fulldigits;
RTC_DS3231 rtc;
int oldminutes = -1;
int oldday = -1;

//25+26 are DAC. Probs not using tho
DacESP32 dac1(DAC_CHAN_0); //pin 25
//DacESP32 buzzer(25);
ezButton butStart(13);
ezButton butSelect(5);
ezButton butA(17);
ezButton butB(12);
ezButton butUp(14);
ezButton butDown(32);
ezButton butLeft(33);
ezButton butRight(27);
float pagetime;

fs::File file;
int selectedweekday = 0;
char CurrentTime[16];
char AlarmTime[] = "0745";
char AlarmDays[8] = "0111110";
int CurrentAlarmOption = 1;
char readchar [8];
char ColorPairSelected[] = "2";

int CLOCK_INTERRUPT_PIN = 16;
int CurrentPage = 1;
int SelectedPage = 1;
int startedalarm = 0;

int Speaker = 25; //25 and 26 are the DAC pins apparently

const uint16_t ColorPairs[][2] = {
  {LGREEN, DGREEN}, //pair0
  {RED, ORANGE}, //pair1
  {BLACK, WHITE}, //pair2
};

int DCOLOR = 0x0320;
int LCOLOR = 0x07E0;

#define FORMAT_LITTLEFS_IF_FAILED true
SemaphoreHandle_t  LCDMutex;

TaskHandle_t FMRadioPage;
TaskHandle_t AlarmPage;
TaskHandle_t AlarmPlayingPage;
TaskHandle_t WifiRadioPage;
TaskHandle_t SettingsPage;
TaskHandle_t LoopingButtons;

void AlarmSchedule(int d, int h, int m)
{
  rtc.setAlarm1(DateTime(2024,12,d+1,h,m,0), DS3231_A1_Day); //December 7 2024 was a saturday. But RTClib uses 0-6, so we need to ADD 1 to the day when setting alarm.
}

void DrawPage()
{
  if (pagetime+600 <= millis())
    {
      if (CurrentPage >= 3)
        {
          CurrentPage = 1;
        }
      else
        {
          CurrentPage++;
        }
      pagetime = millis();
      switch(CurrentPage){  
        case 1:
          vTaskSuspend(FMRadioPage);
          vTaskSuspend(WifiRadioPage);
          vTaskSuspend(SettingsPage);
          vTaskSuspend(AlarmPlayingPage);
          PrintAlarmPage();
          vTaskResume(AlarmPage);
        break;
        case 2:
          vTaskSuspend(AlarmPage);
          vTaskSuspend(WifiRadioPage);
          vTaskSuspend(SettingsPage);
          vTaskSuspend(AlarmPlayingPage);          
          PrintFMRadioPage();
          vTaskResume(FMRadioPage);    
        break;
        case 3:
          vTaskSuspend(AlarmPage);
          vTaskSuspend(FMRadioPage);
          vTaskSuspend(SettingsPage);
          vTaskSuspend(AlarmPlayingPage);          
          PrintWifiRadioPage();
          vTaskResume(WifiRadioPage);
        break;
        case 4:
          vTaskSuspend(AlarmPage);
          vTaskSuspend(FMRadioPage);
          vTaskSuspend(WifiRadioPage);
          vTaskSuspend(AlarmPlayingPage);          
          PrintWifiRadioPage();
          vTaskResume(SettingsPage);
        break;
        default:
          Serial.println("DRAWPAGEDEFAULT");
        break;
        }
    }
}

void UpdateTime()
{
  DateTime now = rtc.now();
  if (now.minute() != oldminutes)
  {
    oldminutes = now.minute();
    if (now.isPM())
    {
      sprintf(CurrentTime, "%d:%02d PM", now.twelveHour(), now.minute());
    }
    else
    {
      sprintf(CurrentTime, "%d:%02d AM", now.twelveHour(), now.minute());
    }  
    switch(CurrentPage){
      case 1:
        tft.fillRect(0, 0, 320, 50, LCOLOR);
        freefontXY(140, 25, 50, FMB24, CurrentTime); //prints to alarm page location
        UpdateDate();
      break;
      case 2:
      break;
      case 3:
      break;
      default:
      break;
    }
  }
}

void PrintAlarmPage()
{
  xSemaphoreTake(LCDMutex, portMAX_DELAY);  
  tft.fillScreen(LCOLOR);
  tft.setTextDatum(MC_DATUM);
  file = LittleFS.open("/AlarmDays.txt");
  String filedays = file.readString();
  filedays.trim();
  tft.fillRect(15, 105, 249, 4, DCOLOR);
  tft.fillRect(15, 125, 249, 4, DCOLOR);
  tft.fillRect(15, 165, 249, 4, DCOLOR);
  tft.fillRect(15, 105, 4, 60, DCOLOR);
  tft.fillRect(50, 105, 4, 60, DCOLOR);
  tft.fillRect(85, 105, 4, 60, DCOLOR);
  tft.fillRect(120, 105, 4, 60, DCOLOR);
  tft.fillRect(155, 105, 4, 60, DCOLOR);
  tft.fillRect(190, 105, 4, 60, DCOLOR);
  tft.fillRect(225, 105, 4, 60, DCOLOR);
  tft.fillRect(260, 105, 4, 60, DCOLOR);
  if (filedays.charAt(0) == '1')
  {
    tft.fillRect(25, 115, 20, 10, DCOLOR);
    freefontXY(35, 145, 10, FMB18, "S");    
  }
  else 
  {
    freefontXY(35, 145, 10, FMB18, "S");
  }
  if (filedays.charAt(1) == '1')
  {
    tft.fillRect(60, 115, 20, 10, DCOLOR);
    freefontXY(70, 145, 10, FMB18, "M");
  }
  else
  {
    freefontXY(70, 145, 10, FMB18, "M");
  }
  if (filedays.charAt(2) == '1')
  {
    tft.fillRect(95, 115, 20, 10, DCOLOR);
    freefontXY(105, 145, 10, FMB18, "T");
  }
  else
  {
    freefontXY(105, 145, 10, FMB18, "T");
  }
  if (filedays.charAt(3) == '1')
  {
    tft.fillRect(130, 115, 20, 10, DCOLOR);
    freefontXY(140, 145, 10, FMB18, "W");
  }
  else
  {
    freefontXY(140, 145, 10, FMB18, "W");
  }
  if (filedays.charAt(4) == '1')
  {
    tft.fillRect(165, 115, 20, 10, DCOLOR);
    freefontXY(175, 145, 10, FMB18, "T");
  }
  else
  {
    freefontXY(175, 145, 10, FMB18, "T");
  }
  if (filedays.charAt(5) == '1')
  {
    tft.fillRect(200, 115, 20, 10, DCOLOR);
    freefontXY(210, 145, 10, FMB18, "F");
  }
  else
  {
    freefontXY(210, 145, 10, FMB18, "F");
  }
  if (filedays.charAt(6) == '1')
  {
    tft.fillRect(235, 115, 20, 10, DCOLOR);  
    freefontXY(245, 145, 10, FMB18, "S");
  }
  else
  {
    freefontXY(245, 145, 10, FMB18, "S");
  }
  file.close();
  DateTime now = rtc.now();
  if (now.isPM())
  {
    sprintf(CurrentTime, "%d:%02d PM", now.twelveHour(), now.minute());
  }
  else
  {
    sprintf(CurrentTime, "%d:%02d AM", now.twelveHour(), now.minute());
  }  
  freefontXY(140, 25, 120, FMB24, CurrentTime);
  freefontXY(140, 200, 120, FMB12, "Alarm Time");
  UpdateDate();
  file = LittleFS.open("/AlarmTime.txt");
  read_ = file.readString();
  int timeValue = read_.toInt(); // Convert the input string to an integer
  int hourval = timeValue / 100;   // Integer division to get the hour
  int minuteval = timeValue % 100; // Modulus to get the minutes
  String period = (hourval < 12) ? "AM" : "PM";
  if (hourval == 0)
  {
    hourval = 12; // Handle midnight as 12 AM
  }
  else if (hourval > 12)
  {
    hourval -= 12; // Convert to 12-hour format
  }
  formattedTime = (String(hourval) + ":" + (minuteval < 10 ? "0" : "") + String(minuteval) + " " + period);
  tft.drawString(formattedTime, 140, 224); //alarm time at bottom
  file.close();
  CurrentAlarmOption = 1;
  xSemaphoreGive(LCDMutex);  // release mutex  
}

void AlarmSettingsOption2()
{
  butA.loop();
  butB.loop();
  if (butA.isPressed())
  {
    tft.fillRect(0, 170, 320, 20, LCOLOR);
    AlarmDays[selectedweekday] = '1';
    tft.fillRect(25 + (selectedweekday*35), 115, 20, 10, DCOLOR);    
    selectedweekday++;
    if (selectedweekday >= 7)
    {
      file = LittleFS.open("/AlarmDays.txt", "r");
      String filedays = file.readString();
      Serial.println(filedays);
      file.close();
      String alarmdaysstring = String(AlarmDays);
      alarmdaysstring.trim();      
      filedays.trim();
      Serial.println(alarmdaysstring);
      Serial.println(filedays);
      if (!filedays.equals(String(alarmdaysstring)))
      {
        writeFile(LittleFS, "/AlarmDays.txt", AlarmDays);
      }
      selectedweekday = 0;      
      CurrentAlarmOption = 3;
      tft.fillTriangle(60, 215, 70, 225, 60, 235, DCOLOR); // erases L triangle for alarm time
      tft.fillTriangle(210, 215, 200, 225, 210, 235, DCOLOR); // erases R triangle for alarm time      
      file = LittleFS.open("/AlarmTime.txt");
      fulldigits = file.readString();
      file.close();
      NavBeep();
    }
    else
    {
      tft.fillTriangle((35*selectedweekday+25), 185, (35*selectedweekday+35), 175, (35*selectedweekday+45), 185, DCOLOR);
      NavBeep();
    }  
  }
  if (butB.isPressed())
  {
    tft.fillRect(0, 170, 320, 20, LCOLOR);     
    AlarmDays[selectedweekday] = '0';
    tft.fillRect(25 + (selectedweekday*35), 115, 20, 10, LCOLOR);    
    selectedweekday++;
    if (selectedweekday >= 7)
    {
      file = LittleFS.open("/AlarmDays.txt", "r");
      String filedays = file.readString();
      Serial.println(filedays);
      file.close();
      String alarmdaysstring = String(AlarmDays);
      alarmdaysstring.trim();
      filedays.trim();
      if (!filedays.equals(String(alarmdaysstring)))
      {
        writeFile(LittleFS, "/AlarmDays.txt", AlarmDays);
      }
      selectedweekday = 0;      
      CurrentAlarmOption = 3;
      tft.fillTriangle(60, 215, 70, 225, 60, 235, DCOLOR); // prints L triangle for alarm time
      tft.fillTriangle(210, 215, 200, 225, 210, 235, DCOLOR); // prints R triangle for alarm time   
      file = LittleFS.open("/AlarmTime.txt");
      fulldigits = file.readString();
      file.close();      
      NavBeep();
    }
    else
    {
      tft.fillTriangle((35*selectedweekday+25), 185, (35*selectedweekday+35), 175, (35*selectedweekday+45), 185, DCOLOR);
      NavBeep();
    }
  }  
}

void AlarmSettingsOption3()
{
  butA.loop();
  butB.loop();
  butLeft.isPressed();
  butRight.isPressed();
  butUp.isPressed();
  butDown.isPressed();
  if (butA.isPressed())
  {
    CurrentAlarmOption = 1;
    tft.fillTriangle(60, 215, 70, 225, 60, 235, LCOLOR); // erases L triangle for alarm time
    tft.fillTriangle(210, 215, 200, 225, 210, 235, LCOLOR); // erases R triangle for alarm time
    //save stuff, and set alarm for next day after changing alarm time
    // if powered off after saving weekday, it doesnt matter because on reboot we will set alarm then.
  }
  if (butLeft.isPressed())
  {
    String minutedigits = fulldigits.substring(2,4);
    int valueminutes = minutedigits.toInt();
    valueminutes--;
    if (valueminutes > 59) valueminutes = 0; // Reset to 0 if it exceeds 99
    if (valueminutes < 0) valueminutes = 59;
    String newminutedigits = String(valueminutes);
    if (newminutedigits.length() == 1)
    {
      newminutedigits = "0" + newminutedigits; // Add leading zero if necessary
    }
    fulldigits = fulldigits.substring(0, 2) + newminutedigits;
    Serial.println(fulldigits);
  }
  if (butRight.isPressed())
  {
    String minutedigits = fulldigits.substring(2,4);
    int valueminutes = minutedigits.toInt();
    valueminutes++;
    if (valueminutes > 59) valueminutes = 0; // Reset to 0 if it exceeds 99
    if (valueminutes < 0) valueminutes = 59;
    String newminutedigits = String(valueminutes);
    if (newminutedigits.length() == 1)
    {
      newminutedigits = "0" + newminutedigits; // Add leading zero if necessary
    }
    fulldigits = fulldigits.substring(0, 2) + newminutedigits;
    Serial.println(fulldigits);
  }
  if (butUp.isPressed())
  {
    
  }
  if (butDown.isPressed())
  {

  }
  //maybe do but B to switch between AM PM?
}

void onAlarm()
{
    startedalarm = 1;
    vTaskSuspend(FMRadioPage);
    vTaskSuspend(WifiRadioPage);
    vTaskSuspend(SettingsPage);
    vTaskSuspend(AlarmPage);
    vTaskResume(AlarmPlayingPage);
    Serial.println("Alarm occured!");
}

void PrintFMRadioPage()
{
  xSemaphoreTake(LCDMutex, portMAX_DELAY);    
  tft.fillScreen(LCOLOR);
  tft.setTextDatum(MC_DATUM);
  freefontXY(140, 45, 50, FMB18, "103.1 FM");
  tft.fillTriangle(30, 260, 50, 240, 50, 280, DCOLOR);//arrow for changing channels
  tft.fillTriangle(210, 260, 190, 240, 190, 280, DCOLOR);//arrow for changing channels
  xSemaphoreGive(LCDMutex);  // release mutex  
  //EVENTUALLY DO FM RADIO STUFF
  //Left/Right Dpad for manual adjustment
  //select to save preset
  //B to cycle presets
  //A to select preset
  //Up/Down to auto seek stations
  //start to change screen to next one
}

void PrintWifiRadioPage()
{
  xSemaphoreTake(LCDMutex, portMAX_DELAY);    
  tft.fillScreen(LCOLOR);
  tft.setTextDatum(MC_DATUM);
  freefontXY(120, 45, 50, FMB18, "WIFI RADIO");
  tft.fillTriangle(30, 260, 50, 240, 50, 280, DCOLOR);//arrow for changing channels
  tft.fillTriangle(210, 260, 190, 240, 190, 280, DCOLOR);//arrow for changing channels
  xSemaphoreGive(LCDMutex);  // release mutex  
  //EVENTUALLY DO FM RADIO STUFF
  //Left/Right Dpad for manual adjustment
  //select to save preset
  //B to cycle presets
  //A to select preset
  //Up/Down to auto seek stations
  //start to change screen to next one
}


void setup(void) {
  Serial.begin(115200);
  LCDMutex = xSemaphoreCreateMutex();
  if (! rtc.begin())
  {
    Serial.println("Couldn't find RTC");
    while (1);
  }
  if(!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED))
  {
    Serial.println("LittleFS Mount Failed");
    return;
  }
  else
  {
    Serial.println("Little FS Mounted Successfully");
  }
  if(LittleFS.exists("/AlarmTime.txt"))
  {
    Serial.println("File exists!");
  }
  else
  {
    writeFile(LittleFS, "/AlarmTime.txt", AlarmTime);
  }
  if(LittleFS.exists("/AlarmDays.txt"))
  {
    Serial.println("File exists!");
  }
  else
  {
    writeFile(LittleFS, "/AlarmDays.txt", AlarmDays);
  }
  if(LittleFS.exists("/Colors.txt"))
  {
    Serial.println("File exists!");
  }
  else
  {
    writeFile(LittleFS, "/Colors.txt", ColorPairSelected);
  }
  readFile(LittleFS, "/AlarmTime.txt"); // test read
  readFile(LittleFS, "/AlarmDays.txt"); // test read
  readFile(LittleFS, "/Colors.txt"); // test read
  pinMode(CLOCK_INTERRUPT_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(CLOCK_INTERRUPT_PIN), onAlarm, FALLING);
  LoadColor();
  tft.begin();
  rtc.disable32K();
  rtc.clearAlarm(1);
  rtc.clearAlarm(2);
  rtc.disableAlarm(2);
  rtc.writeSqwPinMode(DS3231_OFF); // Place SQW pin into alarm interrupt mode
  DateTime now = rtc.now();
  AlarmSchedule(3, 14, 36);// need to read alarm stuff, this is temp
  setCpuFrequencyMhz(40); //240, 160, 80 all let wifi/bluetooth/spi work. 40/20/10 only let i2c and spi work.
                          //we will try 160 for now, I suggest dynamically changing the speed according to each page. Alarm page could run at 80, and MAYBE even lower as we won't be using the wifi
                          //but FM radio and especially internet radio could probably use faster, although if screen updates smoothly we could use 80 across whole project
  //rtc.adjust(DateTime(2025, 1, 5, 5, 32, 0)); // use for initial upload
  tft.setRotation(0);
  tft.fillScreen(LCOLOR);
  butStart.setDebounceTime(50);
  PrintAlarmPage();          
  xTaskCreate(
                    AlarmPageCode,   /* Task function. */
                    "Alarm",     /* name of task. */
                    4000,       /* Stack size of task --has about 3300 spare--*/
                    NULL,        /* parameter of the task */
                    2,           /* priority of the task */
                    &AlarmPage);      
  delay(50);               
  xTaskCreate(
                    FMRadioPageCode,   /* Task function. */
                    "FM Radio",     /* name of task. */
                    4000,       /* Stack size of task --has about 3300 spare--*/
                    NULL,        /* parameter of the task */
                    2,           /* priority of the task */
                    &FMRadioPage);
  vTaskSuspend(FMRadioPage);      
  delay(50);           
  xTaskCreate(
                    WifiRadioPageCode,   /* Task function. */
                    "Wifi Radio",     /* name of task. */
                    4000,       /* Stack size of task --has about 3300 spare--*/
                    NULL,        /* parameter of the task */
                    2,           /* priority of the task */
                    &WifiRadioPage);   
  vTaskSuspend(WifiRadioPage);                          
  delay(50);   
  xTaskCreate(
                    AlarmPlayingPageCode,   /* Task function. */
                    "Alarm Playing",     /* name of task. */
                    4000,       /* Stack size of task --has about 3300 spare--*/
                    NULL,        /* parameter of the task */
                    2,           /* priority of the task */
                    &AlarmPlayingPage);   
  vTaskSuspend(AlarmPlayingPage);                          
  delay(50);     
  xTaskCreate(
                    SettingsPageCode,   /* Task function. */
                    "FM Radio",     /* name of task. */
                    4000,       /* Stack size of task --has about 3300 spare--*/
                    NULL,        /* parameter of the task */
                    2,           /* priority of the task */
                    &SettingsPage);    
  vTaskSuspend(SettingsPage);                         
  delay(50);
  xTaskCreate(
                    LoopingButtonsCode,   /* Task function. */
                    "Button Loop",     /* name of task. */
                    4000,       /* Stack size of task --has about 3300 spare--*/
                    NULL,        /* parameter of the task */
                    1,           /* priority of the task */
                    &LoopingButtons);
  delay(50);            
  //vTaskStartScheduler();   
}          

void LoopingButtonsCode(void * pvParameters){
  for(;;)
  { 
    vTaskDelay(50);
    butStart.loop();
    butUp.loop();
    butDown.loop();
    butLeft.loop();
    butRight.loop();
    if (butStart.isPressed())
    { 
      dac1.disable();
      DrawPage();
      NavBeep();
    }
  }
}

void FMRadioPageCode(void * pvParameters){
  for(;;)
  {
    vTaskDelay(40);
    Serial.println("FM TASK RUNNING");
  }
}

void AlarmPageCode(void * pvParameters){
  for(;;)
  {
    butSelect.loop();
    vTaskDelay(50);
    UpdateTime();
    //run loop check for each button in each state. ex, 1 is nothing, 2 is weekday settings, 3 is time.
    switch(CurrentAlarmOption){
      case 1:
        if (butSelect.isPressed()) // enter settings
          {
            //run separate functions so it's not so messy maybe
            tft.fillTriangle(25, 185, 35, 175, 45, 185, DCOLOR);
            CurrentAlarmOption = 2;
            NavBeep();
          }
      break;
      case 2:
        //if select not pressed, now we check other buttons
        AlarmSettingsOption2();
      break;
      case 3:
        static int FlashTimer = 0;
        if (FlashTimer+500 < millis())
        {
          FlashTimer = millis();
          //flash the alarm time/arrows?
        }
        AlarmSettingsOption3();
      break;
      default:
      break;
      }
    //initially nothing selected
    //press select once goes to day editing
    //Left/Right selects day on chart
    //A turns the day on
    //B turns the day off
    //press select to save alarm DAYS then goes to alarm time
    //Check if old saved date is same as new, if so DONT save the time, to limit the writes to flash
    //flashes selected alarm setting, or maybe highlights it?
    //Up/Down changes hours
    //Left/Right changes minutes
    //A is AM, B is PM
    //select exits editing and saves time, again only overwrite if file different
  }
}

void WifiRadioPageCode(void * pvParameters){
  for(;;)
  {
      vTaskDelay(40);
      Serial.println("WIFI TASK RUNNING");
  }    
}

void SettingsPageCode(void * pvParameters){
  for(;;)
    {
      vTaskDelay(40);
      Serial.println("SETTINGS TASK RUNNING");
    }
    //stuff like changing color of screen, or font maybe? maybe setting time too because
    //alarm page will have lots of settings to change
    //option to turn power light off
    //need options for wifi password
    //MAYBE have settings for turning off screen after certain time?
    //the contrast wheel would be GREAT for turning brightness down on screen, that or we need to do PWM on a 3.3 pin.
}

void AlarmPlayingPageCode(void * pvParameters){
  for(;;)
  {
    butA.loop();
    vTaskDelay(40);
    if (startedalarm == 1)
    {
      startedalarm = 0;
      dac1.enable();
      dac1.setCwScale(DAC_COSINE_ATTEN_DB_18);   // 1/2 amplitude (-6dB)
      dac1.outputCW(3000); //MAKES NOISE
      xSemaphoreTake(LCDMutex, portMAX_DELAY);  
      tft.fillScreen(LCOLOR);
      tft.setTextDatum(MC_DATUM);
      freefontXY(140, 120, 50, FMB18, "WAKE UP");
      xSemaphoreGive(LCDMutex);
    }
    //should print time on top too
    if (butA.isPressed())
    {
      dac1.disable();
      PrintAlarmPage();
      vTaskResume(AlarmPage);
      vTaskSuspend(AlarmPlayingPage);
    }
  }
}

void loop() {vTaskDelete(NULL);} // disable main loop