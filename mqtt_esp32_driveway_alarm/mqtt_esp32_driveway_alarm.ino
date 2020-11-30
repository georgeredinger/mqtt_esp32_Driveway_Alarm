//wroom esp32 module
//mqtt subscribe to driveway beam breaks
//drive 3 color tree with buzzer

#include <esp_task_wdt.h>
#include <esp_system.h>
#include "mqtt.h"

unsigned long alive;
#define WDT_TIMEOUT 30 //freertos task timeout,should be done with measurement and MQTT report and in deep sleep with in 30 sec

const char *ID = "drivewayStatus";  // Name of our device, must be unique


#define BEEP 12
#define GREEN 21
#define YELLOW 22
#define RED 26

bool drivewayAlarming = false;
int alarmOnTime;
int alarmCountDown;
bool drivewayAlarmStart = false;
#define ALARM_ON_COUNT 10;

void blink(int color, int count, int ontime, int offtime) {
  int i;
  for (i = 0; i < count; i++) {
    pinMode(color, OUTPUT);
    digitalWrite(color, HIGH);
    delay(ontime);
    pinMode(color, INPUT);// green LED circuit has a leakage bug such that driving low turns on a little
    digitalWrite(color, LOW);
    delay(offtime);
  }
  pinMode(color, INPUT);
}

void    startAlarm() {
  drivewayAlarmStart = true;
  drivewayAlarming = true;
  alarmCountDown = ALARM_ON_COUNT;
}

void checkAlarm() {
  if (drivewayAlarming) {
    if (drivewayAlarmStart) {
      drivewayAlarmStart = false;
    }
    blink(RED, 3, 100, 500);
    blink(YELLOW,3,100,500);
    blink(GREEN,3,100,500);
    delay(1000);
    blink(BEEP, 5, 10, 200);
    if (alarmCountDown <= 0) {
      drivewayAlarming = false;
      drivewayAlarmStart = false;
    }
    alarmCountDown--;


  }
}

int print_wakeup_reason() {
  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();
  switch (wakeup_reason)  {
    case 0  : Serial.println("Wakeup caused by 0 what is that?"); break;//power up, or watchdog
    case 1  : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case 2  : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break; //cat purch interrupt
    case 4  : Serial.println("Wakeup caused by touchpad"); break;
    case 5  : Serial.println("Wakeup caused by ULP program"); break;
    case 6  : Serial.println("Wakeup caused by 6 what's that?"); break;
    default : Serial.printf("Wakeup was not caused %d", wakeup_reason); break;
  }
  return (wakeup_reason);
}

void IRAM_ATTR resetModule() {
  esp_restart();
}


void setup() {
  int wakeup_reason;
  char wakeup_string[3];

  //  pinMode(RED, OUTPUT);
  //  pinMode(GREEN , OUTPUT);
  //  pinMode( YELLOW , OUTPUT);
  //  pinMode( BEEP , OUTPUT);
  digitalWrite(RED, LOW);
  digitalWrite(GREEN, LOW);
  digitalWrite(YELLOW, LOW);
  digitalWrite(BEEP, LOW);

  Serial.begin(115200); // Start serial communication at 115200 baud
  delay(1000);
  wakeup_reason = print_wakeup_reason();
  sprintf(wakeup_string, "%1d", wakeup_reason);
  esp_task_wdt_init(WDT_TIMEOUT, true); //enable panic so ESP32 restarts
  esp_task_wdt_add(NULL); //add current thread to WDT watch
  setup_mqtt();
  Serial.println("setup");
}

void loop() {
  esp_task_wdt_reset();
  if (!client.connected())
    reconnect();
  client.loop();
//  if (millis() % 60000*3 ==0) {
//    startAlarm();
//  }
  checkAlarm();



}
