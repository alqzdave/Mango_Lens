/*
 * ESP32 Servo Controller - Wireless Mango Sorting System
 * Receives WiFi commands from ESP32-CAM and controls 2 servos
 * For defense: December 6, 2025
 */

#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

// ===== WIFI SETTINGS (MUST MATCH ESP32-CAM!) =====
const char* ssid = "Luis";           // Your WiFi/Hotspot name
const char* password = "hello1234";  // Your WiFi/Hotspot password
// =================================================

// ===== SERVO SETTINGS =====
Servo ripeServo;      // Servo for RIPE mangoes
Servo unripeServo;    // Servo for UNRIPE mangoes

const int RIPE_SERVO_PIN = 12;       // GPIO 12 for ripe servo
const int UNRIPE_SERVO_PIN = 13;     // GPIO 13 for unripe servo

// Servo positions - MG996R SERVO (544-2400us)
const int SERVO_REST = 544;          // Rest position (0° = 544us for MG996R)
const int SERVO_SORT = 1008;         // Sort position (45° = 1008us)
const int SERVO_OPEN_TIME = 2500;    // Hold open for 2.5 seconds
const int SERVO_COOLDOWN = 200;      // Wait 0.2s before next sort - FASTER!
// ==========================

// Web server on port 80
WebServer server(80);

// Statistics
int ripeCount = 0;
int unripeCount = 0;
unsigned long lastSortTime = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n========================================");
  Serial.println("  ESP32 Servo Controller");
  Serial.println("  Mango Sorting System");
  Serial.println("========================================\n");

  // Initialize servos for MG996R (544-2400us range)
  ripeServo.setPeriodHertz(50);      // Standard 50Hz servo
  ripeServo.attach(RIPE_SERVO_PIN, 544, 2400);  // MG996R pulse width range
  
  unripeServo.setPeriodHertz(50);
  unripeServo.attach(UNRIPE_SERVO_PIN, 544, 2400);
  
  // Set to rest position
  ripeServo.writeMicroseconds(SERVO_REST);
  unripeServo.writeMicroseconds(SERVO_REST);
  
  Serial.println("✓ Servos initialized");
  Serial.print("  - RIPE servo on GPIO ");
  Serial.println(RIPE_SERVO_PIN);
  Serial.print("  - UNRIPE servo on GPIO ");
  Serial.println(UNRIPE_SERVO_PIN);

  // Connect to WiFi
  connectWiFi();

  // Setup web server routes
  server.on("/", handleRoot);
  server.on("/sort", HTTP_POST, handleSort);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/test", HTTP_GET, handleTest);
  server.onNotFound(handleNotFound);

  // Start server
  server.begin();
  Serial.println("✓ Web server started");
  
  Serial.println("\n========================================");
  Serial.println("         READY TO SORT!");
  Serial.println("========================================");
  Serial.print("Server IP: ");
  Serial.println(WiFi.localIP());
  Serial.println("Waiting for commands from ESP32-CAM...");
  Serial.println("========================================\n");
}

void loop() {
  server.handleClient();
}

void connectWiFi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✓ WiFi Connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Signal Strength: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
  } else {
    Serial.println("\n✗ WiFi Connection Failed!");
    Serial.println("Please check WiFi credentials and restart.");
  }
}

void handleRoot() {
  String html = "<html><head><title>ESP32 Servo Controller</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>body{font-family:Arial;padding:20px;background:#f0f0f0;}";
  html += ".card{background:white;padding:20px;margin:10px 0;border-radius:8px;box-shadow:0 2px 4px rgba(0,0,0,0.1);}";
  html += "h1{color:#2c3e50;}button{padding:10px 20px;margin:5px;font-size:16px;cursor:pointer;border-radius:5px;border:none;}";
  html += ".ripe{background:#27ae60;color:white;}.unripe{background:#e67e22;color:white;}</style></head><body>";
  html += "<h1>🥭 Mango Sorting Controller</h1>";
  html += "<div class='card'><h2>Status</h2>";
  html += "<p><strong>IP Address:</strong> " + WiFi.localIP().toString() + "</p>";
  html += "<p><strong>RIPE Count:</strong> " + String(ripeCount) + "</p>";
  html += "<p><strong>UNRIPE Count:</strong> " + String(unripeCount) + "</p>";
  html += "<p><strong>Total Sorted:</strong> " + String(ripeCount + unripeCount) + "</p>";
  html += "</div>";
  html += "<div class='card'><h2>Manual Test</h2>";
  html += "<button class='ripe' onclick='fetch(\"/test?servo=ripe\")'>Test RIPE Servo</button>";
  html += "<button class='unripe' onclick='fetch(\"/test?servo=unripe\")'>Test UNRIPE Servo</button>";
  html += "</div></body></html>";
  
  server.send(200, "text/html", html);
}

void handleSort() {
  if (!server.hasArg("ripeness")) {
    server.send(400, "application/json", "{\"error\":\"Missing ripeness parameter\"}");
    return;
  }
  
  String ripeness = server.arg("ripeness");
  ripeness.toUpperCase();
  
  Serial.println("\n========================================");
  Serial.print("📦 SORT COMMAND RECEIVED: ");
  Serial.println(ripeness);
  
  if (ripeness == "RIPE" || ripeness == "HINOG" || ripeness == "MATURE") {
    sortRipe();
    server.send(200, "application/json", "{\"success\":true,\"action\":\"RIPE sorted\"}");
  } 
  else if (ripeness == "UNRIPE" || ripeness == "HILAW" || ripeness == "IMMATURE") {
    sortUnripe();
    server.send(200, "application/json", "{\"success\":true,\"action\":\"UNRIPE sorted\"}");
  }
  else {
    Serial.print("⚠ Unknown ripeness: ");
    Serial.println(ripeness);
    server.send(400, "application/json", "{\"error\":\"Unknown ripeness level\"}");
  }
  
  Serial.println("========================================\n");
}

void handleStatus() {
  String json = "{";
  json += "\"ripeCount\":" + String(ripeCount) + ",";
  json += "\"unripeCount\":" + String(unripeCount) + ",";
  json += "\"totalSorted\":" + String(ripeCount + unripeCount) + ",";
  json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
  json += "\"rssi\":" + String(WiFi.RSSI());
  json += "}";
  
  server.send(200, "application/json", json);
}

void handleTest() {
  if (!server.hasArg("servo")) {
    server.send(400, "text/plain", "Missing servo parameter");
    return;
  }
  
  String servo = server.arg("servo");
  servo.toLowerCase();
  
  Serial.println("🧪 Manual Test: " + servo);
  
  if (servo == "ripe") {
    sortRipe();
    server.send(200, "text/plain", "RIPE servo tested");
  } else if (servo == "unripe") {
    sortUnripe();
    server.send(200, "text/plain", "UNRIPE servo tested");
  } else {
    server.send(400, "text/plain", "Invalid servo name");
  }
}

void handleNotFound() {
  server.send(404, "text/plain", "Not Found");
}

void sortRipe() {
  Serial.println("🟢 SORTING RIPE MANGO");
  Serial.println("  → Opening gate to 45°...");
  
  // Smooth swing to 45° using microseconds
  ripeServo.writeMicroseconds(SERVO_SORT);
  delay(SERVO_OPEN_TIME);
  
  Serial.println("  → Closing gate to 0°...");
  // Smooth swing back to 0°
  ripeServo.writeMicroseconds(SERVO_REST);
  delay(SERVO_COOLDOWN);
  
  ripeCount++;
  lastSortTime = millis();
  
  Serial.print("✓ RIPE sorted! Total: ");
  Serial.println(ripeCount);
  Serial.println("  → Ready for next mango\n");
}

void sortUnripe() {
  Serial.println("🟡 SORTING UNRIPE MANGO");
  Serial.println("  → Opening gate to 45°...");
  
  // Smooth swing to 45° using microseconds
  unripeServo.writeMicroseconds(SERVO_SORT);
  delay(SERVO_OPEN_TIME);
  
  Serial.println("  → Closing gate to 0°...");
  // Smooth swing back to 0°
  unripeServo.writeMicroseconds(SERVO_REST);
  delay(SERVO_COOLDOWN);
  
  unripeCount++;
  lastSortTime = millis();
  
  Serial.print("✓ UNRIPE sorted! Total: ");
  Serial.println(unripeCount);
  Serial.println("  → Ready for next mango\n");
}
