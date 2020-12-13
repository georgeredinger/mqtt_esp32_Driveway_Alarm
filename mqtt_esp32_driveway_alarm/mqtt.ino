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



void callback(char* topic, byte* payload, unsigned int length) {
  int i;
  int m;
  double number;
  char displayText[50];
  char displayload[50];
  payload[length] = '\0';
  notices nots[] = {  //queues not created till setup() is complete
    {"hotwater/", redQueue},
    {"cat/", yellowQueue},
    {"catfood/", greenQueue},
    {"driveway/", beepQueue},
    {"pump/", light0Queue},
    {"fence/", light1Queue}
  };
  Serial.printf("callback: %s %s\r\n", topic, (char *) payload);
  if ((strstr(topic, "data") != 0 ) || (strstr(topic, "power") != 0)|| (strstr(topic, "purch") != 0)) {
    for (i = 0; i < 6; i++) {
      if (strstr(topic, nots[i].name)) {
        Serial.printf("nots[i].name topic match %s %s\r\n",nots[i].name ,topic);
       if (isdigit(payload[0])) {
          m = ((char )payload[0] == '1') ? 1 : 0;
        } else {
          m = (strstr((char *)payload, "ON") != NULL) ? 1 : 0;
        }
        xQueueSend(nots[i].Q, &m, portMAX_DELAY);
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
