//wroom esp32 moduledouble      ledcWriteTone(uint8_t channel, double freq);

//mqtt subscribe to driveway beam breaks
//drive 3 color tree with buzzer
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ESPAsyncWebServer.h>
#include <esp_task_wdt.h>
#include <esp_system.h>
#include "mqtt.h"

unsigned long alive;
#define WDT_TIMEOUT 30 //freertos task timeout,should be done with measurement and MQTT report and in deep sleep with in 30 sec

const char *ID = "drivewayStatus";  // Name of our device, must be unique

const byte cLight0 = 0;
const byte cLight1 = 1;
const byte cBeep = 2;
const byte cRed = 3;
const byte cYellow = 4;
const byte cGreen = 5;
int beat;
int pulser = 1;
// 8 open collector outputs
//uln2800

#define BEEP 17
#define GREEN 13
#define YELLOW 18
#define RED 16
#define LIGHT0 15
///#define UNDEF2 5
///#define UNDEF3 17
#define LIGHT1 4
const int freq = 440;
const int ledChannel = 0;
const int resolution = 8;
const int pins[] = {4, 5, 13, 14, 15, 16, 17, 18};
QueueHandle_t redQueue;
QueueHandle_t yellowQueue;
QueueHandle_t greenQueue;
QueueHandle_t beepQueue;
QueueHandle_t light0Queue;
QueueHandle_t light1Queue;
int queueSize = 10;

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


void cpulse(int cChannel, int duty, int t) {

  ledcWrite(cChannel, duty);
  delay(t);
  ledcWrite(cChannel, 0);
}

void cfade(int cChannel, int duty, int t) {
  int i;
  for ( i = 0; i < duty; i += 50) {
    esp_task_wdt_reset();
    cpulse(cChannel, i, t);
  }
  for (i = duty; i > 0; i -= 50) {
    esp_task_wdt_reset();
    cpulse(cChannel, i, t);
  }
}

void light0(void * parameter) {
  for (;;) { // infinite loop
    cfade(cLight0, 1000, 100);
    // Pause the task again for 500ms
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}
void light1(void * parameter) {
  for (;;) { // infinite loop
    cfade(cLight1, 2500, 100);
    // Pause the task again for 500ms
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}


void red(void * parameter) {
  int element;
  for (;;) { // infinite loop
    xQueueReceive(redQueue, &element, portMAX_DELAY);//block till a 0 or 1 indicating red off or on

    cfade(cRed, 2500, 75);
    // Pause the task again for 500ms
    vTaskDelay(5000 / portTICK_PERIOD_MS);
  }
}

void yellow(void * parameter) {
  for (;;) { // infinite loop
    cfade(cYellow, 2500, 55);
    // Pause the task again for 500ms
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}

void green(void * parameter) {
  int element;
  for (;;) { // infinite loop
    xQueueReceive(greenQueue, &element, portMAX_DELAY);

    ledcWrite(cGreen, element);
    vTaskDelay(element / portTICK_PERIOD_MS);
    ledcWrite(cGreen, 0 );

    // Pause the task again for 500ms
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void beep(void * parameter) {
  for (;;) { // infinite loop
    // cfade(cBeep, 1300, 50);
    ledcWrite(cBeep, 255);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    ledcWrite(cBeep, 0);

    // Pause the task again for 500ms

    vTaskDelay(10000 / portTICK_PERIOD_MS);
  }
}


void setup() {
  int wakeup_reason;
  char wakeup_string[3];

  pinMode( BEEP, OUTPUT);
  pinMode( GREEN , OUTPUT);
  pinMode( YELLOW , OUTPUT);
  pinMode(RED , OUTPUT);
  pinMode(LIGHT0 , OUTPUT);
  pinMode( LIGHT1 , OUTPUT);

  ledcSetup(cLight0, 1000, 16);
  ledcAttachPin(LIGHT0, 0);
  ledcSetup(cLight1, 1000, 16);

  ledcAttachPin(LIGHT1, 1);
  ledcAttachPin(BEEP, cBeep);
  ledcSetup(cBeep, 440, 8);

  ledcSetup(cRed  , 1000, 16);
  ledcAttachPin(RED, cRed);

  ledcSetup( cYellow , 1000, 16);
  ledcAttachPin(YELLOW, cYellow);

  ledcSetup( cGreen , 1000, 16);
  ledcAttachPin(GREEN, cGreen);

  Serial.begin(115200); // Start serial communication at 115200 baud
  delay(1000);
  redQueue = xQueueCreate( queueSize, sizeof( int ) );
  if (redQueue == NULL) {
    Serial.println("Error creating the greenQueue");
  }
  yellowQueue = xQueueCreate( queueSize, sizeof( int ) );
  if (yellowQueue == NULL) {
    Serial.println("Error creating the greenQueue");
  }
  greenQueue = xQueueCreate( queueSize, sizeof( int ) );
  if (greenQueue == NULL) {
    Serial.println("Error creating the greenQueue");
  }
  beepQueue = xQueueCreate( queueSize, sizeof( int ) );
  if (beepQueue == NULL) {
    Serial.println("Error creating the greenQueue");
  }
  light0Queue = xQueueCreate( queueSize, sizeof( int ) );
  if (light0Queue == NULL) {
    Serial.println("Error creating the greenQueue");
  }
  light1Queue = xQueueCreate( queueSize, sizeof( int ) );
  if (light1Queue == NULL) {
    Serial.println("Error creating the greenQueue");
  }


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
  ArduinoOTA.setHostname("alarm");

  ArduinoOTA
  .onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  })
  .onEnd([]() {
    Serial.println("\nEnd");
  })
  .onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  })
  .onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });

  ArduinoOTA.begin();

  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  MDNS.addService("http", "tcp", 80);
  MDNS.addServiceTxt("http", "tcp", "prop1", "test");
  MDNS.addServiceTxt("http", "tcp", "prop2", "test2");
  strcpy(webString, "nothing");

  server.on("/hello", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(200, "text/plain", webString);
  });
  server.begin();

  xTaskCreate(
    light0,    // Function that should be called
    "light0",   // Name of the task (for debugging)
    1000,            // Stack size (bytes)
    NULL,            // Parameter to pass
    1,               // Task priority
    NULL             // Task handle
  );

  xTaskCreate(
    light1,    // Function that should be called
    "light1",   // Name of the task (for debugging)
    1000,            // Stack size (bytes)
    NULL,            // Parameter to pass
    1,               // Task priority
    NULL             // Task handle
  );
  xTaskCreate(
    red,    // Function that should be called
    "red",   // Name of the task (for debugging)
    1000,            // Stack size (bytes)
    NULL,            // Parameter to pass
    1,               // Task priority
    NULL             // Task handle
  );

  xTaskCreate(
    yellow,    // Function that should be called
    "yellow",   // Name of the task (for debugging)
    1000,            // Stack size (bytes)
    NULL,            // Parameter to pass
    1,               // Task priority
    NULL             // Task handle
  );

  xTaskCreate(
    green,    // Function that should be called
    "green",   // Name of the task (for debugging)
    1000,            // Stack size (bytes)
    NULL,            // Parameter to pass
    1,               // Task priority
    NULL             // Task handle
  );

  xTaskCreate(
    beep,    // Function that should be called
    "beep",   // Name of the task (for debugging)
    1000,            // Stack size (bytes)
    NULL,            // Parameter to pass
    1,               // Task priority
    NULL             // Task handle
  );

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

  // /    esp_task_wdt_reset();
  //  delay(10);
  //  cfade(cRed, 10000, 1) ;
  //  cfade(cYellow, 5000, 1);
  //  esp_task_wdt_reset();
  //  //  delay(1000);
  //  cfade(cGreen, 2500, 1);
  //  //delay(1000);
  //  //  /ledcWrite(2, 1100);//beep

  esp_task_wdt_reset();




  esp_task_wdt_reset();
  digitalWrite(BEEP, HIGH);
  delay(100);
  digitalWrite(BEEP, LOW);
  delay(100);
  Serial.println();
  sprintf(webString, "%ld", millis() / 1000);
  //  delay(10000);
  // / Serial.println(webString);
  int m = millis() / 100;
  xQueueSend(greenQueue, &m, portMAX_DELAY);
  ArduinoOTA.handle();

}
