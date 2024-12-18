#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <WebServer.h>
#include <PubSubClient.h>

#define TRIG_PIN 4
#define ECHO_PIN 5
#define RELAY_PIN 26
#define CLOSE_DISTANCE 5  // Distance in cm to trigger relay OFF

WebServer server(80);

// MQTT Configuration
const char* mqtt_broker = "broker.hivemq.com";
const int mqtt_port = 1883;
const char* mqtt_topic = "esp32/relay_status";
WiFiClient espClient;
PubSubClient mqtt_client(espClient);

#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

#define WIFI_SSID ""
#define WIFI_PASSWORD ""

#define API_KEY " "
#define DATABASE_URL " "

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

unsigned long sendDataPrevMillis = 0;
bool signupOK = false;
bool lastRelayState = true;  // To track relay state changes

float distance = 0.0;
bool relayState = true;      // Starting with relay ON

void reconnectMQTT() {
  while (!mqtt_client.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientId = "ESP32Client-" + String(random(0xffff), HEX);
    if (mqtt_client.connect(clientId.c_str())) {
      Serial.println("connected");
      // Publish initial state after reconnecting
      mqtt_client.publish(mqtt_topic, "0");
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqtt_client.state());
      Serial.println(" retrying in 5 seconds");
      delay(5000);
    }
  }
}

float measureDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  long duration = pulseIn(ECHO_PIN, HIGH);
  return duration * 0.034 / 2;
}

void sendSensorData() {
  String json = "{";
  json += "\"distance\":" + String(distance) + ",";
  json += "\"relayState\":" + String(relayState ? "true" : "false");
  json += "}";
  server.send(200, "application/json", json);
}
void handleRoot() {
  String html = R"====(
    <!DOCTYPE html>
    <html>
    <head>
      <title>Ultrasonic Sensor Dashboard</title>
      <style>
        body {
          font-family: Arial, sans-serif;
          text-align: center;
          background: linear-gradient(to bottom, #1abc9c, #16a085);
          color: white;
          margin: 0;
          padding: 0;
        }
        .container {
          max-width: 600px;
          margin: auto;
          padding: 20px;
          background: #ffffff10;
          border-radius: 15px;
          box-shadow: 0px 4px 20px rgba(0,0,0,0.3);
          margin-top: 50px;
        }
        .heading {
          font-size: 36px;
          margin-bottom: 20px;
          color: #f1c40f;
          text-shadow: 2px 2px 5px rgba(0, 0, 0, 0.7);
        }
        .circle {
          width: 150px;
          height: 150px;
          border-radius: 50%;
          background: #3498db;
          color: #fff;
          display: flex;
          align-items: center;
          justify-content: center;
          font-size: 32px;
          font-weight: bold;
          margin: 20px auto;
          box-shadow: 0px 4px 15px rgba(0,0,0,0.4);
          border: 4px solid #ffffff80;
        }
        .data {
          font-size: 24px;
          margin: 15px 0;
        }
        .relay {
          font-size: 28px;
          font-weight: bold;
          margin-top: 20px;
        }
        .relay.on {
          color: #2ecc71;
        }
        .relay.off {
          color: #e74c3c;
        }
        .relay.on::after {
          content: " ✅";
        }
        .relay.off::after {
          content: " ❌";
        }
        .footer {
          margin-top: 20px;
          font-size: 14px;
          color: #ecf0f1;
        }
      </style>
      <script>
        async function fetchSensorData() {
          const response = await fetch('/sensor');
          const data = await response.json();

          document.getElementById('distance').innerText = data.distance.toFixed(2) + " cm";
          const relayElement = document.getElementById('relayState');
          relayElement.innerText = data.relayState ? "Relay ON" : "Relay OFF";
          relayElement.className = "relay " + (data.relayState ? "on" : "off");
        }

        setInterval(fetchSensorData, 1000);
        window.onload = fetchSensorData;
      </script>
    </head>
    <body>
      <div class="container">
        <h1 class="heading">Ultrasonic Sensor Dashboard</h1>
        <div class="circle" id="distance">-- cm</div>
        <p id="relayState" class="relay">--</p>
        <p class="footer">Real-time Monitoring</p>
      </div>
    </body>
    </html>
  )====";

  server.send(200, "text/html", html);
}
void setup() {
  Serial.begin(115200);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);
  
  // Initialize relay to ON state
  digitalWrite(RELAY_PIN, LOW);
  relayState = true;

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());

  // Initialize MQTT and publish initial state
  mqtt_client.setServer(mqtt_broker, mqtt_port);
  reconnectMQTT();
  mqtt_client.publish(mqtt_topic, "0");  // Initial state is ON (0)
  Serial.println("Published initial state: 0 (ON)");

  server.on("/", handleRoot);
  server.on("/sensor", sendSensorData);
  server.begin();
  Serial.println("Web server started");

  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Sign up successful!");
    signupOK = true;
  } else {
    Serial.printf("Sign up error: %s\n", config.signer.signupError.message.c_str());
  }

  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void loop() {
  server.handleClient();

  if (!mqtt_client.connected()) {
    reconnectMQTT();
  }
  mqtt_client.loop();

  distance = measureDistance();

  // Update relay state based on distance
  if (distance < CLOSE_DISTANCE) {  // Object is too close
    if (relayState) {  // Only publish and change state if it was previously ON
      relayState = false;
      digitalWrite(RELAY_PIN, HIGH);
      mqtt_client.publish(mqtt_topic, "1");
      Serial.println("Object too close! Relay OFF - Published '1'");
    }
  } else {  // Normal distance
    if (!relayState) {  // Only publish and change state if it was previously OFF
      relayState = true;
      digitalWrite(RELAY_PIN, LOW);
      mqtt_client.publish(mqtt_topic, "0");
      Serial.println("Normal distance - Relay ON - Published '0'");
    }
  }

  if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 1000 || sendDataPrevMillis == 0)) {
    sendDataPrevMillis = millis();
    Firebase.RTDB.setFloat(&fbdo, "Ultrasonic/Distance", distance);
    Firebase.RTDB.setBool(&fbdo, "Relay/State", relayState);
  }
}