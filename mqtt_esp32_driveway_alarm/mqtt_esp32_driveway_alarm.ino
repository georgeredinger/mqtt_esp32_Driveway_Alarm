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

typedef struct  {
  QueueHandle_t qHandle;
  int cChannel;
  int onTime;
  int brightness;
  char *name;
} annunciatorParms;

const int  cLight0 = 0;
const int cLight1 = 1;
const int cBeep = 2;
const int cRed = 3;
const int cYellow = 4;
#define cGreen  5
int beat;
int pulser = 1;
// 8 open collector outputs
//uln2800

#define BEEP 17
#define GREEN 13
#define YELLOW 18
#define RED 16
#define LIGHT0 15
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

void indicate(void * parameter) {
  int topPad;//seems to be a memory overwrite from xQueueReceive
  int element = 0;
  int bottomPad;//seems to be a memory overwrite from xQueueReceive
  QueueHandle_t qHandle = ((annunciatorParms *)parameter)->qHandle;
  int cChannel = ((annunciatorParms *)parameter)->cChannel;
  int onTime = ((annunciatorParms *)parameter)->onTime;
  int brightness = ((annunciatorParms *)parameter)->brightness;
  char *name = ((annunciatorParms *)parameter)->name;
  int bTime = 0;
  for (;;) { // infinite loop
    xQueueReceive(qHandle, &element, portMAX_DELAY);
    bTime = element * onTime;

    ledcWrite(cChannel,  bTime);

    Serial.printf("indicate: %s %d\r\n", name,bTime);
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
  ledcAttachPin(LIGHT0, cLight0);

  ledcSetup(cLight1, 1000, 16);
  ledcAttachPin(LIGHT1, cLight1);

  ledcAttachPin(BEEP, cBeep);
  ledcSetup(cBeep, 440, 16);

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
    Serial.println("Error creating the redQueue");
  }
  yellowQueue = xQueueCreate( queueSize, sizeof( int ) );
  if (yellowQueue == NULL) {
    Serial.println("Error creating the yellowQueue");
  }
  greenQueue = xQueueCreate( queueSize, sizeof( int ));
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

  Serial.printf("QueueHandle_t size = %d\r\n", sizeof(QueueHandle_t));

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


  //channel,ontime,brightness

  annunciatorParms redParms;
  redParms.qHandle = redQueue;
  redParms.cChannel = cRed;
  redParms.onTime = 1234;
  redParms.brightness = 11234;
  redParms.name = "red";

  xTaskCreate(
    indicate,    // Function that should be called
    "red",   // Name of the task (for debugging)
    1000,            // Stack size (bytes)
    (void *)&redParms,            // Parameter to pass
    1,               // Task priority
    NULL             // Task handle
  );

  annunciatorParms yellowParms;
  yellowParms.qHandle = yellowQueue;
  yellowParms.cChannel = cYellow;
  yellowParms.onTime = 1234;
  yellowParms.brightness = 21034;
  yellowParms.name = "yellow";

                     xTaskCreate(
                       indicate,    // Function that should be called
                       "yellow",   // Name of the task (for debugging)
                       1000,            // Stack size (bytes)
                       (void *)&yellowParms,            // Parameter to pass
                       1,               // Task priority
                       NULL             // Task handle
                     );



  annunciatorParms greenParms;
  greenParms.qHandle = greenQueue;
  greenParms.cChannel = cGreen;
  greenParms.onTime = 1234;
  greenParms.brightness = 22234;
  greenParms.name = "green";
  xTaskCreate(
    indicate,    // Function that should be called
    "green",   // Name of the task (for debugging)
    1000,            // Stack size (bytes)
    (void *)&greenParms,            // Parameter to pass
    1,               // Task priority
    NULL             // Task handle
  );


  annunciatorParms beepParms;
  beepParms.qHandle = beepQueue;
  beepParms.cChannel = cBeep;
  beepParms.onTime = 1234;
  beepParms.brightness = 2314;
  beepParms.name = "beep";
  xTaskCreate(
    indicate,    // Function that should be called
    "beep",   // Name of the task (for debugging)
    1000,            // Stack size (bytes)
    (void *)&beepParms,            // Parameter to pass
    1,               // Task priority
    NULL             // Task handle
  );
  //

  annunciatorParms light0Parms;
  light0Parms.qHandle = light0Queue;
  light0Parms.cChannel = cLight0;
  light0Parms.onTime = 1234;
  light0Parms.brightness = 2134;
  light0Parms.name = "light0";
  xTaskCreate(
    indicate,    // Function that should be called
    "light0",   // Name of the task (for debugging)
    1000,            // Stack size (bytes)
    (void *)&light0Parms,            // Parameter to pass
    1,               // Task priority
    NULL             // Task handle
  );


  annunciatorParms light1Parms;
  light1Parms.qHandle = light1Queue;
  light1Parms.cChannel = cLight1;
  light1Parms.onTime = 1234;
  light1Parms.brightness = 1234;
  light1Parms.name = "light1";
  xTaskCreate(
    indicate,    // Function that should be called
    "light1",   // Name of the task (for debugging)
    1000,            // Stack size (bytes)
    (void *)&light1Parms,            // Parameter to pass
    1,               // Task priority
    NULL             // Task handle
  );

  Serial.println("setup");
}

int m;
int ticker = 0;
int timer = 2000;
bool offIt = false;
void loop() {
  esp_task_wdt_reset();
  if (!client.connected())
    reconnect();
  client.loop();


  //  if (millis() > timer) {
  //    m = 1;
  //    timer = millis() + 5000;
  //    xQueueSend(greenQueue, &m, portMAX_DELAY);
  //    offIt = true;
  //  }
  //  if (millis() > timer / 2) {
  //    m = 0;
  //    if (offIt) {
  //      xQueueSend(greenQueue, &m, portMAX_DELAY);
  //      offIt = false;
  //    }
  //  }


  ticker++;

  ArduinoOTA.handle();


}
