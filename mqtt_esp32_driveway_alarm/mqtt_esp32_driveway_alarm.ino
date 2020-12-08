//wroom esp32 module
//mqtt subscribe to driveway beam breaks
//drive 3 color tree with buzzer
#include <ESPmDNS.h>
#include <ESPAsyncWebServer.h>
#include <esp_task_wdt.h>
#include <esp_system.h>
#include "mqtt.h"

unsigned long alive;
#define WDT_TIMEOUT 30 //freertos task timeout,should be done with measurement and MQTT report and in deep sleep with in 30 sec

const char *ID = "drivewayStatus";  // Name of our device, must be unique

// 8 open collector outputs
//uln2800

#define BEEP 4
#define GREEN 5
#define YELLOW 13
#define RED 14
#define UNDEF1 15
#define UNDEF2 16
#define UNDEF3 17
#define UNDEF4 18

bool drivewayAlarming = false;
int alarmOnTime;
int alarmCountDown;
bool drivewayAlarmStart = false;
#define ALARM_ON_COUNT 10;
AsyncWebServer server(80);
char webString[50];
void blink(int color, int count, int ontime, int offtime) {
  int i;
  //  for (i = 0; i < count; i++) {
  //    pinMode(color, OUTPUT);
  //    digitalWrite(color, HIGH);
  //    delay(ontime);
  //    pinMode(color, INPUT);// green LED circuit has a leakage bug such that driving low turns on a little
  //    digitalWrite(color, LOW);
  //    delay(offtime);
  //  }
  //  pinMode(color, INPUT);
}

void staticLight(int color, bool state) {
  if (state == 1) {
    pinMode(color, OUTPUT);
    digitalWrite(color, HIGH);
  } else {
    pinMode(color, INPUT);
    digitalWrite(color, LOW);
  }
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
    blink(YELLOW, 3, 100, 500);
    blink(GREEN, 3, 100, 500);
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

  pinMode(RED, OUTPUT);
  pinMode(GREEN , OUTPUT);
  pinMode( YELLOW , OUTPUT);
  pinMode( UNDEF1 , OUTPUT);
  pinMode( UNDEF2 , OUTPUT);
  pinMode( UNDEF3 , OUTPUT);
  pinMode( UNDEF4 , OUTPUT);

  digitalWrite(RED, LOW);
  digitalWrite(GREEN, LOW);
  digitalWrite(YELLOW, LOW);
  digitalWrite(BEEP, LOW);

  Serial.begin(115200); // Start serial communication at 115200 baud
  delay(1000);
  if (mdns_init() != ESP_OK) {
    Serial.println("mDNS failed to start");
    return;
  }
  Serial.println("mDNS started");
  if (!MDNS.begin("alarm")) {
    Serial.println("Error starting mDNS");
    return;
  }
  wakeup_reason = print_wakeup_reason();
  sprintf(wakeup_string, "%1d", wakeup_reason);
  esp_task_wdt_init(WDT_TIMEOUT, true); //enable panic so ESP32 restarts
  esp_task_wdt_add(NULL); //add current thread to WDT watch
  setup_mqtt();
  MDNS.addService("http", "tcp", 80);
  MDNS.addServiceTxt("http", "tcp", "prop1", "test");
  MDNS.addServiceTxt("http", "tcp", "prop2", "test2");
    strcpy(webString,"nothing");

  server.on("/hello", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(200, "text/plain", webString);
  });
  server.begin();
  Serial.println("setup");
}

void loop() {
    esp_task_wdt_reset();
  //  if (!client.connected())
  //    reconnect();
  //  client.loop();
  //  if (millis() % 60000*3 ==0) {
  //    startAlarm();
  //  }
  //  checkAlarm();
  digitalWrite(  BEEP, HIGH);
  digitalWrite( GREEN , HIGH);
  digitalWrite( YELLOW, HIGH);
  digitalWrite( RED , HIGH);
  digitalWrite(UNDEF1, HIGH);
  digitalWrite( UNDEF2, HIGH);
  digitalWrite( UNDEF3 , HIGH);
  digitalWrite( UNDEF4 , HIGH);

  sprintf(webString,"%ld",millis()/1000);
  delay(1000);
  Serial.println(webString);
  digitalWrite(BEEP, LOW);


}
