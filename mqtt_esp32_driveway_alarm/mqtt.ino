#include "mqtt.h"
const char *ssid =  "balanced";   // name of your WiFi network
const char *password =  "leapyear"; // password of the WiFi network
IPAddress broker(192, 168, 1, 113); // IP address of your MQTT broker eg. 192.168.1.50
WiFiClient wclient;
PubSubClient client(wclient); // Setup MQTT client

// Connect to WiFi network
void setup_wifi() {
  WiFi.begin(ssid, password); // Connect to network
  while (WiFi.status() != WL_CONNECTED) { // Wait for connection
    delay(1500);
  }
  Serial.println("connected");
  Serial.println(ssid);
}

extern QueueHandle_t redQueue;
extern QueueHandle_t yellowQueue;
extern QueueHandle_t greenQueue;
extern QueueHandle_t beepQueue;
extern QueueHandle_t light0Queue;
extern QueueHandle_t light1Queue;


void reconnect() {
  while (!client.connected()) {
    if (client.connect(ID)) {
      Serial.printf(" MQTT connected as %s\r\n", ID);
      client.subscribe("#");


    } else {
      Serial.print("MQTT connect failed with state ");
      Serial.println(client.state());
      delay(2000);
    }
  }
}



typedef struct  {
  char *name;
  QueueHandle_t Q;
} notices;

typedef struct {
  char *name;
  int state;
} states;

void callback(char* topic, byte* payload, unsigned int length) {
  int i, j;
  int m;
  double number;
  char displayText[50];
  char displayload[50];
  payload[length] = '\0';
  notices nots[] = {  //queues not created till setup() is complete
    {"hotwater/power", redQueue},
    {"cat/purch", yellowQueue},
    {"catfood/presence", greenQueue},
    {"driveway/data", beepQueue},
    {"pump/power", light0Queue},
    {"fence/data", light1Queue},
    {"THEEND", light1Queue}
  };
  states  state[] = {
    {"IN", 1},
    {"OUT", 0},
    {"ON", 1},
    {"OFF", 0},
    {"1", 1},
    {"0", 0},
    {"THEEND", 0}
  };

  Serial.printf("callback: %s %s\r\n", topic, (char *) payload);
  for (i = 0; i < 6; i++) {
    if (strstr(topic, nots[i].name) != NULL) {
      Serial.printf("nots[i].name topic match %s %s\r\n", nots[i].name , topic);
      for (j = 0; j < 6; j++) {
        if (strstr((char *)payload, state[j].name) != NULL ) {
          xQueueSend(nots[i].Q, &state[j].state, portMAX_DELAY);
        }
        break;
      }
    }
  }
}
void publish(const char *topic, const char *data) {
  if (!client.connected()) { // Reconnect if connection is lost
    reconnect();
  }
  client.loop();
  client.publish(topic, data);
  Serial.printf("%s %s\n", topic , data);
}

void setup_mqtt() {
  setup_wifi(); // Connect to network
  client.setServer(broker, 1883);
  client.setCallback(callback);
}
