#include <Wire.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <BH1750.h>
#include <Adafruit_AHTX0.h>
#include <ArduinoJson.h>
#include <OneButton.h>

const char* ssid = "Chauchan";
const char* password = "chauchan";
const char* mqtt_server = "cd80185684ab4cee86ac167bdf8f629a.s1.eu.hivemq.cloud";
const int mqtt_port = 8883;
const char* mqtt_user = "Project1";
const char* mqtt_pass = "Project1";
const char* mqtt_topic = "sensor/data";

WiFiClientSecure espClient;
PubSubClient client(espClient);
BH1750 lightMeter;
Adafruit_AHTX0 aht;

// Chân kết nối
const int RELAY_PIN = 14;
const int BUTTON_MODE_PIN = 12;
const int BUTTON_RELAY_PIN = 13;
const int LED_PIN = 16;  
String receivedData = "";
String detectedPlant = "None"; 
String predetectedPlant = ""; 
int mode = 0;  // 0 = Manual, 1 = AutoMode
String modeStatus  = "";
String motorStatus = "";
unsigned long lastSensorRead = 0;
const unsigned long sensorInterval = 1000;

// Nút nhấn
OneButton buttonMode(BUTTON_MODE_PIN, true, true);
OneButton buttonRelay(BUTTON_RELAY_PIN, true, true);

void blinkLED(int times) {
    for (int i = 0; i < times; i++) {
        digitalWrite(LED_PIN, HIGH);  
        delay(200);
        digitalWrite(LED_PIN, LOW);  
        delay(200);
    }
}

void setup_wifi() {
    Serial.print("Connecting to WiFi...");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(100);
        Serial.print(".");
    }
    Serial.println("\nConnected to WiFi!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    blinkLED(2);  
}

void reconnect() {
    while (!client.connected()) {
        Serial.print("Attempting MQTT connection...");
        String clientID = "ESPClient-";
        clientID += String(random(0xffff), HEX);
        
        if (client.connect(clientID.c_str(), mqtt_user, mqtt_pass)) {
            Serial.println("Connected to MQTT!");
            client.subscribe("sensor/client"); 
            blinkLED(5);
        } else {
            Serial.print("Failed, rc=");
            Serial.print(client.state());
            Serial.println(" -> Try again in 5 seconds");
            blinkLED(1);
            delay(5000); 
        }
    }
}

void setAutoMode() {
    mode = 1;

    Serial.println("Switch AutoMode");

}

void setManualMode() {
    mode = 0;

    Serial.println("Switch ManualMode");
}

void toggleRelay() {
    if (mode == 0) {
        digitalWrite(RELAY_PIN, !digitalRead(RELAY_PIN));
        Serial.print("Manual Mode: Relay ");
        Serial.println(digitalRead(RELAY_PIN) ? "ON" : "OFF");
    }
}

void Automode(float temp, float hum) {
    if (temp > 31 && hum >= 46) {
        digitalWrite(RELAY_PIN, HIGH);
        Serial.println("Relay ON (AutoMode)");
    } else {
        digitalWrite(RELAY_PIN, LOW);
        Serial.println("Relay OFF (AutoMode)");
    }
}

void callback(char* topic, byte* payload, unsigned int length) {
    String incomingMessage = "";
    for (int i = 0; i < length; i++) {
        incomingMessage += (char)payload[i];
    }

    Serial.println("Received message from MQTT");
    Serial.print("Topic: ");
    Serial.println(topic);
    Serial.print("Payload: ");
    Serial.println(incomingMessage);

    DynamicJsonDocument doc(200);
    DeserializationError error = deserializeJson(doc, incomingMessage);

    if (error) {
        Serial.print("JSON parse error: ");
        Serial.println(error.c_str());
        return;
    }

    JsonObject obj = doc.as<JsonObject>();

    if (obj.containsKey("mode")) {
        int modeCommand = obj["mode"];
        if (modeCommand == 1) {
            mode = 1; 
            Serial.println("Switch Auto Mode");
        } else if (modeCommand == 2) {
            mode = 0; 
            Serial.println("Switch Manual Mode");
        }
    }

    if (obj.containsKey("Motor")) {
        int motorCommand = obj["Motor"];
        Serial.print("Motor command: ");
        Serial.println(motorCommand);

        if (motorCommand == 1) {
            digitalWrite(RELAY_PIN, HIGH);
            Serial.println("On Manual Mode");
        } else if (motorCommand == 2) {
            digitalWrite(RELAY_PIN, LOW);
            Serial.println("Off Manual Mode");
        }
    }
}

void readSerialData() {
    while (Serial.available()) {
        char c = Serial.read();  
        receivedData += c;       

        if (c == '\n') {  
            receivedData.trim(); 
            if (receivedData.startsWith("Pepper, ") || receivedData.startsWith("Caffe, ")) {
                Serial.println("Detected: " + receivedData);  
                detectedPlant = receivedData;
                predetectedPlant = detectedPlant; 
            } else {
                detectedPlant = predetectedPlant; 
            }

            receivedData = "";  
        }
    }
}


void setup() {
    Serial.begin(115200);
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    client.setCallback(callback);

    Wire.begin();
    setup_wifi();
    espClient.setInsecure();
    client.setServer(mqtt_server, mqtt_port);
    lightMeter.begin();
    
    if (!aht.begin()) {
        Serial.println("Failed to initialize AHT10 sensor!");
    }

    buttonMode.attachClick(setAutoMode);
    buttonMode.attachDoubleClick(setManualMode);
    buttonRelay.attachClick(toggleRelay);

    Serial.println("ESP8266 Ready!");
}

void loop() {
    if (!client.connected()) {
        reconnect();
    }
    client.loop();

    buttonMode.tick();
    buttonRelay.tick();

    if (millis() - lastSensorRead >= sensorInterval) {
    lastSensorRead = millis();

    sensors_event_t humidity, temp;
    aht.getEvent(&humidity, &temp);
    float lightLevel = lightMeter.readLightLevel();

    int temperature_int = round(temp.temperature);
    int humidity_int = round(humidity.relative_humidity);
    int light_int = round(lightLevel);

    while (Serial.available()) {
        char c = Serial.read();  
        receivedData += c;       

        if (c == '\n') {  
            receivedData.trim(); 
            if (receivedData.startsWith("Pepper, ") || receivedData.startsWith("Caffe, ")) {
                Serial.println("Detected: " + receivedData);  
                detectedPlant = receivedData; 
                predetectedPlant = detectedPlant; 
            } else {
                detectedPlant = predetectedPlant; 
            }

            receivedData = "";  
        }
    }
    if (mode == 1) {
        modeStatus = "Auto";
        Automode(temperature_int, humidity_int);
    }
    else {
          modeStatus = "No-Auto";

    }
    if(digitalRead(RELAY_PIN) == 1)
    {
      motorStatus = "On";
    }
    else{
      motorStatus = "Off";
    }
    StaticJsonDocument<200> jsonDoc;
    jsonDoc["temperature"] = temperature_int;  
    jsonDoc["humidity"] = humidity_int;        
    jsonDoc["light"] = light_int;             
    jsonDoc["relay_status"] = motorStatus; 
    jsonDoc["mode"] = modeStatus; 
    jsonDoc["plant"] = detectedPlant;

    char buffer[256];
    serializeJson(jsonDoc, buffer);
    client.publish(mqtt_topic, buffer);

    Serial.println("Data sent to MQTT:");
    Serial.println(buffer);
}

}
