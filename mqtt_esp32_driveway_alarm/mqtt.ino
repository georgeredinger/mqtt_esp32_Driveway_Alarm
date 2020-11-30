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

void reconnect() {
  while (!client.connected()) {
    if (client.connect(ID)) {
      Serial.printf(" MQTT connected as %s\n", ID);
      client.subscribe("driveway/data");
    } else {
      Serial.print("MQTT connect failed with state ");
      Serial.println(client.state());
      delay(2000);
    }
  }
}




void callback(char* topic, byte* payload, unsigned int length) {
  int i;

  double number;
  char displayText[50];
  char displayload[50];
  payload[length] = '\0';
 Serial.printf("%s:%s\n",topic,payload);
 if(strstr("driveway/data",topic) !=NULL){
  startAlarm();
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
