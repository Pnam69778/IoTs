# IoTs
Air Quality Monitoring and Safety Alert System (ESP32)
This repository contains the firmware and documentation for a real-time environmental monitoring system. The project integrates IoT cloud platforms and AI chatbots to provide a comprehensive safety solution against air pollution and fire hazards.
Features:
-Real-time Monitoring: Tracking Temperature, Humidity, and Air Quality (Gas/Smoke).
-Visual Interface: Local data display on a 0.96" OLED screen.
-Multi-level Alerts: Local warnings via Buzzer and Tri-color LEDs (Green, Yellow, Red).
-Cloud Integration: Data logging on ThingSpeak and remote control/monitoring via Blynk.
-AI Integration: Interactive status queries via the XiaoZhi AI Chatbot.
Hardware:
Component                     Specification
Microcontroller               ESP32 DevKit V1 
Temperature/Humidity Sensor   DHT11
Air Quality Sensor            MQ135
Display                       OLED SSD1306 (I2C)
Actuators                     LEDs,Buzzer
Libraries: you should dowload all of it in Arduino IDE except 2 libraries(WebSocketMCP).It will be in the code file "IoTs_ck" .
-WiFi.h
-BlynkSimpleEsp32.h
-DHT.h
-Wire.h
-Adafruit_GFX.h
-Adafruit_SSD1306.h
-ThingSpeak.h
-ArduinoJson.h
-WebSocketMCP.h
-Adafruit Unified Sensor (Required dependency for DHT.h)
