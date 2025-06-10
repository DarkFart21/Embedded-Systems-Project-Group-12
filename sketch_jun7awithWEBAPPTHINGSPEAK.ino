#include <ESP32Servo.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <WiFi.h>

WebServer server(80);

// Wi-Fi credentials
const char* ssid = "GAZOFAMBAM2.4G";
const char* pass = "rolando112968";

// ThingSpeak API Key
const String apiKey = "WJFWE7SRFH7TDLBW";
const String ledApiKey = "V1PWKRVT1AXWRHIQ";

// LED and light threshold
const int LED_PIN = 25;
const int lightThreshold = 2000; // Higher means brighter threshold
bool ledState = false;

// Servo setup
Servo servoH, servoV;
const int servoHPin = 2;
const int servoVPin = 13;
int servohori = 90;
int servovert = 45;

// LDR pins
const int ltPin = 36, rtPin = 39, ldPin = 34, rdPin = 32;
int ltVal, rtVal, ldVal, rdVal;

// Filtered deltas
float alpha = 0.33;
float filtered_dvert = 0, filtered_dhoriz = 0;
float last_dvert = 0, last_dhoriz = 0;

// Delta and servo limits
const int tol = 50;
const int deadband = 20;
const int servohoriMin = 5, servohoriMax = 175;
const int servovertMin = 1, servovertMax = 100;
const int movementDelay = 100;

// Timing
unsigned long lastMoveTime = 0;
unsigned long lastSendTime = 0;
const unsigned long sendInterval = 15000;
unsigned long lastLEDLogTime = 0;
const unsigned long ledLogInterval = 3000;

void setServo(Servo &servo, int angle) {
  angle = constrain(angle, 0, 180);
  servo.write(angle);
}

void sendDataToServer() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "https://api.thingspeak.com/update?api_key=" + apiKey;
    url += "&field1=" + String(ltVal);
    url += "&field2=" + String(rtVal);
    url += "&field3=" + String(ldVal);
    url += "&field4=" + String(rdVal);
    url += "&field5=" + String(filtered_dvert);
    url += "&field6=" + String(filtered_dhoriz);
    url += "&field7=" + String(servovert);
    url += "&field8=" + String(servohori);
    http.begin(url);
    int responseCode = http.GET();
    Serial.print("📡 ThingSpeak response: ");
    Serial.println(responseCode);
    http.end();
  }
}

void sendLEDStatusToThingSpeak() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "https://api.thingspeak.com/update?api_key=" + ledApiKey;
    url += "&field1=" + String(ledState ? 1 : 0); // 1 for ON, 0 for OFF
    http.begin(url);
    int responseCode = http.GET();
    Serial.print("💡 LED status sent to ThingSpeak: ");
    Serial.println(responseCode);
    http.end();
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, pass);
  servoH.attach(servoHPin);
  servoV.attach(servoVPin);
  setServo(servoH, servohori);
  setServo(servoV, servovert);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Route to return all data
  server.on("/data", []() {
    String json = "{";
    json += "\"lt\":" + String(ltVal) + ",";
    json += "\"rt\":" + String(rtVal) + ",";
    json += "\"ld\":" + String(ldVal) + ",";
    json += "\"rd\":" + String(rdVal) + ",";
    json += "\"dvert\":" + String((int)filtered_dvert) + ",";
    json += "\"dhoriz\":" + String((int)filtered_dhoriz) + ",";
    json += "\"servovert\":" + String(servovert) + ",";
    json += "\"servohori\":" + String(servohori) + ",";
    json += "\"led\":\"" + String(ledState ? "ON" : "OFF") + "\"";
    json += "}";
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", json);
  });

  // ✅ New route to get only LED status
  server.on("/led", []() {
    String ledJson = "{\"led\":\"" + String(ledState ? "ON" : "OFF") + "\"}";
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", ledJson);
  });

  server.begin();
  analogReadResolution(12);
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    server.handleClient();
  }

  // Read LDRs
  ltVal = analogRead(ltPin);
  rtVal = analogRead(rtPin);
  ldVal = analogRead(ldPin);
  rdVal = analogRead(rdPin);

  // Averages
  int topAvg = (ltVal + rtVal) / 2;
  int bottomAvg = (ldVal + rdVal) / 2;
  int leftAvg = (ltVal + ldVal) / 2;
  int rightAvg = (rtVal + rdVal) / 2;

  // Delta direction: move toward the brighter side
  int dvert = topAvg - bottomAvg;
  int dhoriz = leftAvg - rightAvg;

  filtered_dvert = alpha * dvert + (1 - alpha) * filtered_dvert;
  filtered_dhoriz = alpha * dhoriz + (1 - alpha) * filtered_dhoriz;

  unsigned long now = millis();

  // Night/day logic
  int avgLDR = (ltVal + rtVal + ldVal + rdVal) / 4;
if (avgLDR < lightThreshold) {
  // It's dark → turn LED ON
  if (!ledState && now - lastLEDLogTime > ledLogInterval) {
    Serial.println("LED turned ON (night detected)");
    lastLEDLogTime = now;
  }
  digitalWrite(LED_PIN, HIGH);
  ledState = true;
} else {
  // It's bright → turn LED OFF
  if (ledState && now - lastLEDLogTime > ledLogInterval) {
    Serial.println("LED turned OFF (day detected)");
    lastLEDLogTime = now;
  }
  digitalWrite(LED_PIN, LOW);
  ledState = false;
}
  // Servo movement
  if (now - lastMoveTime >= movementDelay) {
    bool moved = false;

    // Vertical: top vs bottom
    if (abs(filtered_dvert) > tol && abs(filtered_dvert - last_dvert) > deadband) {
      if (filtered_dvert > 0) {
        servovert = constrain(servovert - 1, servovertMin, servovertMax);  // Move up
      } else {
        servovert = constrain(servovert + 1, servovertMin, servovertMax);  // Move down
      }
      setServo(servoV, servovert);
      last_dvert = filtered_dvert;
      moved = true;
    }

    // Horizontal: left vs right
    if (abs(filtered_dhoriz) > tol && abs(filtered_dhoriz - last_dhoriz) > deadband) {
      if (filtered_dhoriz > 0) {
        servohori = constrain(servohori - 1, servohoriMin, servohoriMax);  // Move left
      } else {
        servohori = constrain(servohori + 1, servohoriMin, servohoriMax);  // Move right
      }
      setServo(servoH, servohori);
      last_dhoriz = filtered_dhoriz;
      moved = true;
    }

    if (moved) lastMoveTime = now;
  }

  // Logging
  static unsigned long lastLog = 0;
  if (now - lastLog >= 5000) {
    Serial.printf("📊 LDRs - LT:%d RT:%d LD:%d RD:%d | ΔV:%d ΔH:%d | Servo V:%d H:%d\n",
                  ltVal, rtVal, ldVal, rdVal, dvert, dhoriz, servovert, servohori);
    lastLog = now;
  }

  // Send to ThingSpeak
  if (WiFi.status() == WL_CONNECTED && now - lastSendTime >= sendInterval) {
    sendDataToServer();
    lastSendTime = now;
  }

  // Send LED status to second ThingSpeak channel every 15s
  static unsigned long lastLEDSendTime = 0;
  const unsigned long ledSendInterval = 15000;

  if (WiFi.status() == WL_CONNECTED && now - lastLEDSendTime >= ledSendInterval) {
  sendLEDStatusToThingSpeak();
  lastLEDSendTime = now;
  }

}
