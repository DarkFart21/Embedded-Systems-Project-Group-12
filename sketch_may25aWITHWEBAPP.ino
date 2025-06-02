#define BLYNK_TEMPLATE_ID "TMPL6_qnUCkEx"
#define BLYNK_TEMPLATE_NAME "Solar Tracker"
#define BLYNK_AUTH_TOKEN "cs9OLm9t5mjCl_fX1mScbYw22dAUikeV"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <ESP32Servo.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

WebServer server(80);

// Wi-Fi credentials
char ssid[] = "GAZOFAMBAM2.4G";
char pass[] = "rolando112968";
char auth[] = BLYNK_AUTH_TOKEN;

//Automatic Light Logic
const int LED_PIN = 25;     // GPIO pin controlling the relay
const int lightThreshold = 1000; // Adjust this threshold for night detection
bool ledState = false;      // To track current LED state


// Servo objects and pins
Servo servoH, servoV;
const int servoHPin = 2;
const int servoVPin = 13;

// LDR pins
const int ltPin = 36, rtPin = 39, ldPin = 34, rdPin = 32;

// ADC values
int ltVal, rtVal, ldVal, rdVal;
int dvert, dhoriz;
int servohori = 90, servovert = 45;

// Filtered deltas
float alpha = 0.33;  // Smoothing factor for exponential moving average
float filtered_dvert = 0;
float filtered_dhoriz = 0;

// Limits
const int tol = 50;
const int deadband = 20;
const int movementDelay = 100;
const int servohoriMin = 5, servohoriMax = 175;
const int servovertMin = 1, servovertMax = 100;

// Timing
unsigned long lastMoveTime = 0;
unsigned long lastLogTime = 0;
unsigned long lastBlynkSendTime = 0;
const unsigned long blynkInterval = 15000;
unsigned long lastLEDLogTime = 0;
const unsigned long ledLogInterval = 3000; // 3 seconds between LED log prints


// Deltas
float last_dvert = 0;
float last_dhoriz = 0;

void sendDataToServer() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin("https://solar-tracker-webpage.onrender.com/update");
    http.addHeader("Content-Type", "application/json");

    StaticJsonDocument<256> doc;
    doc["lt"] = ltVal;
    doc["rt"] = rtVal;
    doc["ld"] = ldVal;
    doc["rd"] = rdVal;
    doc["dvert"] = dvert;
    doc["dhoriz"] = dhoriz;
    doc["servovert"] = servovert;
    doc["servohori"] = servohori;

    String requestBody;
    serializeJson(doc, requestBody);

    int responseCode = http.POST(requestBody);
    Serial.print("📡 POST response code: ");
    Serial.println(responseCode);
    http.end();
  } else {
    Serial.println("⚠️ WiFi not connected, cannot send data.");
  }
}

void setServo(Servo &servo, int angle) {
  angle = constrain(angle, 0, 180);
  servo.write(angle);
}

// Blynk manual override
BLYNK_WRITE(V0) {
  servohori = constrain(param.asInt(), servohoriMin, servohoriMax);
  setServo(servoH, servohori);
}

BLYNK_WRITE(V1) {
  servovert = constrain(param.asInt(), servovertMin, servovertMax);
  setServo(servoV, servovert);
}

void setup() {
  Serial.begin(115200);

  servoH.attach(servoHPin);
  servoV.attach(servoVPin);
  setServo(servoH, servohori);
  setServo(servoV, servovert);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW); // Make sure LED is off initially


  Blynk.begin(auth, ssid, pass);

  server.on("/data", []() {
    String json = "{";
    json += "\"lt\":" + String(ltVal) + ",";
    json += "\"rt\":" + String(rtVal) + ",";
    json += "\"ld\":" + String(ldVal) + ",";
    json += "\"rd\":" + String(rdVal) + ",";
    json += "\"dvert\":" + String(dvert) + ",";
    json += "\"dhoriz\":" + String(dhoriz) + ",";
    json += "\"servovert\":" + String(servovert) + ",";
    json += "\"servohori\":" + String(servohori);
    json += "}";

    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", json);
  });

  server.begin();
  analogReadResolution(12);
}

void loop() {
  Blynk.run();
  server.handleClient();

  // Read sensors
  ltVal = analogRead(ltPin);
  rtVal = analogRead(rtPin);
  ldVal = analogRead(ldPin);
  rdVal = analogRead(rdPin);

  int avt = (ltVal + rtVal) / 2;
  int avd = (ldVal + rdVal) / 2;
  int avl = (ltVal + ldVal) / 2;
  int avr = (rtVal + rdVal) / 2;

  dvert = avt - avd;
  dhoriz = avl - avr;

  // Apply smoothing filter
  filtered_dvert = alpha * dvert + (1 - alpha) * filtered_dvert;
  filtered_dhoriz = alpha * dhoriz + (1 - alpha) * filtered_dhoriz;

  unsigned long now = millis();

 
int averageLDR = (ltVal + rtVal + ldVal + rdVal) / 4;

if (averageLDR < lightThreshold) {
  if (!ledState && now - lastLEDLogTime > ledLogInterval) {
    Serial.println("LED turned ON (night detected)");
    lastLEDLogTime = now;
  }
  digitalWrite(LED_PIN, HIGH);
  ledState = true;
} else {
  if (ledState && now - lastLEDLogTime > ledLogInterval) {
    Serial.println("LED turned OFF (day detected)");
    lastLEDLogTime = now;
  }
  digitalWrite(LED_PIN, LOW);
  ledState = false;
}




  if (now - lastMoveTime >= movementDelay) {
    bool moved = false;

    if (abs(filtered_dvert) > tol && abs(filtered_dvert - last_dvert) > deadband){
      servovert += (filtered_dvert > 0) ? -1 : 1;
      servovert = constrain(servovert, servovertMin, servovertMax);
     setServo(servoV, servovert);
      last_dvert = filtered_dvert;
      moved = true;
    }

    
  
    if (abs(filtered_dhoriz) > tol && abs(filtered_dhoriz - last_dhoriz) > deadband) {
      servohori += (filtered_dhoriz > 0) ? -1 : 1;
      servohori = constrain(servohori, servohoriMin, servohoriMax);
      setServo(servoH, servohori);
      last_dhoriz = filtered_dhoriz;
      moved = true;
    }

    if (moved) lastMoveTime = now;
  }

  if (now - lastLogTime >= 5000) {
    Serial.printf("📊 LDRs - LT:%d RT:%d LD:%d RD:%d | ΔV:%d ΔH:%d | Servo V:%d H:%d\n",
                  ltVal, rtVal, ldVal, rdVal, dvert, dhoriz, servovert, servohori);
    lastLogTime = now;
  }

  if (now - lastBlynkSendTime >= blynkInterval) {
    Blynk.virtualWrite(V0, ledState); // 1 if ON, 0 if OFF
    Blynk.virtualWrite(V2, ltVal);
    Blynk.virtualWrite(V3, rtVal);
    Blynk.virtualWrite(V4, ldVal);
    Blynk.virtualWrite(V5, rdVal);
    Blynk.virtualWrite(V6, dvert);
    Blynk.virtualWrite(V7, dhoriz);
    Blynk.virtualWrite(V8, servovert);
    Blynk.virtualWrite(V9, servohori);

    sendDataToServer();
    lastBlynkSendTime = now;
  }
}