#include <Servo.h>  

Servo horizontal; // Horizontal Servo Motor
int servohori = 180;  
int servohoriLimitHigh = 175;
int servohoriLimitLow = 5;

Servo vertical; // Vertical Servo Motor
int servovert = 45;  
int servovertLimitHigh = 100;
int servovertLimitLow = 1;

// F LDR pin connections
int ldrlt = A0; // Top Left LDR
int ldrrt = A3; // Top Right LDR
int ldrld = A1; // Bottom Left LDR
int ldrrd = A2; // Bottom Right LDR

void setup() {
  horizontal.attach(2);
  vertical.attach(13);
  horizontal.write(servohori);
  vertical.write(servovert);
  delay(2500);  // Allow servos to settle

  Serial.begin(9600);  // Start serial communication
}

void loop() {
  // ----  Read LDR sensor values ----
  int lt = analogRead(ldrlt); // Top Left
  int rt = analogRead(ldrrt); // Top Right
  int ld = analogRead(ldrld); // Bottom Left
  int rd = analogRead(ldrrd); // Bottom Right

  int dtime = 10;  
  int tol = 90; // Movement threshold

  // Calculate average light values ----
  int avt = (lt + rt) / 2;  // Average Top
  int avd = (ld + rd) / 2;  // Average Bottom
  int avl = (lt + ld) / 2;  // Average Left
  int avr = (rt + rd) / 2;  // Average Right

  //  Calculate differences ----
  int dvert = avt - avd;    
  int dhoriz = avl - avr;   

  // ----  Decision logic ----
  // VERTICAL TRACKING
  if (abs(dvert) > tol) {
    if (dvert > 0) {
      servovert = servovert + 1;
      if (servovert > servovertLimitHigh) servovert = servovertLimitHigh;
    } else {
      servovert = servovert - 1;
      if (servovert < servovertLimitLow) servovert = servovertLimitLow;
    }
    vertical.write(servovert);
  }

  // HORIZONTAL TRACKING
  if (abs(dhoriz) > tol) {
    if (dhoriz > 0) {
      servohori = servohori - 1;
      if (servohori < servohoriLimitLow) servohori = servohoriLimitLow;
    } else {
      servohori = servohori + 1;
      if (servohori > servohoriLimitHigh) servohori = servohoriLimitHigh;
    }
    horizontal.write(servohori);
  }


  Serial.print("LDRs - LT: "); Serial.print(lt);
  Serial.print(" RT: "); Serial.print(rt);
  Serial.print(" LD: "); Serial.print(ld);
  Serial.print(" RD: "); Serial.print(rd);
  Serial.print(" | ΔV: "); Serial.print(dvert);
  Serial.print(" ΔH: "); Serial.print(dhoriz);
  Serial.print(" | Servo V: "); Serial.print(servovert);
  Serial.print(" H: "); Serial.println(servohori);

  delay(dtime);
}
