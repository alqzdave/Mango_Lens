# 🚀 ESP32 Wireless Servo Control - Setup Guide
## Defense Ready: December 6, 2025

---

## 📦 What You Have

1. **ESP32-CAM** - Already installed sa machine with camera
2. **ESP32 #2** - For servo control (spare ESP32)
3. **2 Servos** - RIPE and UNRIPE sorting
4. **WiFi/Hotspot** - "Luis" (hello1234)

---

## 🔌 Hardware Connections

### **ESP32 #2 Servo Controller:**

```
Servo 1 (RIPE mangoes):
  Orange wire → ESP32 GPIO 12
  Red wire    → ESP32 5V
  Brown wire  → ESP32 GND

Servo 2 (UNRIPE mangoes):
  Orange wire → ESP32 GPIO 13
  Red wire    → ESP32 5V
  Brown wire  → ESP32 GND

Power:
  ESP32 #2 via USB (5V power bank or adapter)
```

**Total: 6 wires (3 per servo)**

---

## 💻 Software Setup

### **Step 1: Upload ESP32 Servo Controller Code**

1. Open Arduino IDE
2. Open file: `esp32_servo_controller.ino`
3. Install library: **ESP32Servo** (Sketch → Include Library → Manage Libraries → Search "ESP32Servo")
4. Select board: **ESP32 Dev Module**
5. Select correct COM port
6. Click **Upload**
7. Open **Serial Monitor** (115200 baud)
8. **IMPORTANT:** Write down the IP address shown! (Example: 192.168.43.50)

---

### **Step 2: Update ESP32-CAM Code**

1. Open file: `esp32_with_streaming.ino`
2. Find this line (around line 16):
   ```cpp
   const char* esp32ServoIP = "http://192.168.43.50";
   ```
3. **CHANGE `192.168.43.50` to the IP address from Step 1!**
4. Save file
5. Upload to ESP32-CAM (same as before)

---

## ⚙️ Configuration

### **Adjust Servo Positions (if needed):**

In `esp32_servo_controller.ino`, find these lines:

```cpp
const int SERVO_REST = 90;      // Idle position (0-180)
const int SERVO_SORT = 0;       // Push position (0-180)
const int SERVO_DELAY = 500;    // Hold time in milliseconds
```

**Test and adjust:**
- `SERVO_REST = 90` - Center position (servo at rest)
- `SERVO_SORT = 0` or `180` - Position to push mango
- `SERVO_DELAY = 500` - How long servo stays in sort position (500ms = 0.5 seconds)

---

## 🧪 Testing Procedure

### **Test 1: Servo Controller Standalone**

1. Power on ESP32 #2
2. Open Serial Monitor (115200 baud)
3. Wait for: "READY TO SORT!"
4. Note the IP address
5. Open web browser on phone/laptop
6. Go to: `http://[ESP32_IP]` (example: http://192.168.43.50)
7. Click "Test RIPE Servo" - servo should move!
8. Click "Test UNRIPE Servo" - servo should move!

**If servos don't move:**
- Check wiring (signal to GPIO 12/13)
- Check power (5V and GND connected)
- Adjust SERVO_SORT value (try 180 instead of 0)

---

### **Test 2: ESP32-CAM Communication**

1. Power on both ESP32-CAM and ESP32 #2
2. Open Serial Monitor for ESP32-CAM
3. Place mango in front of camera
4. Wait for detection (4 seconds)
5. Check ESP32-CAM Serial Monitor:
   ```
   Predicted Ripeness Level: Ripe
   ✓ Sorting is active, sending data...
   ✓ Data sent to Flask
   🤖 Sending to Servo Controller: Ripe
   ✓ Servo Controller response: {"success":true}
   ```
6. Check ESP32 #2 Serial Monitor:
   ```
   📦 SORT COMMAND RECEIVED: RIPE
   🟢 SORTING RIPE MANGO
   ✓ RIPE mango sorted! Total RIPE: 1
   ```
7. Servo should move!

---

### **Test 3: Full System**

1. Start Flask server (python app.py)
2. Power on ESP32-CAM
3. Power on ESP32 #2
4. Open web dashboard
5. Click "Start Sorting"
6. Place mango → ESP32-CAM detects → Servo sorts → Dashboard updates!

---

## 🐛 Troubleshooting

### **Servo Controller not responding:**

**Check Serial Monitor for:**
```
✗ Servo Controller error: -1
⚠ Check if ESP32 Servo Controller is running!
```

**Solutions:**
1. Verify ESP32 #2 is powered on
2. Check WiFi connection (both on same network!)
3. Verify IP address is correct in ESP32-CAM code
4. Restart ESP32 #2

---

### **WiFi Connection Issues:**

**ESP32 #2 Serial Monitor shows:**
```
✗ WiFi Connection Failed!
```

**Solutions:**
1. Check WiFi credentials (SSID and password)
2. Move ESP32 closer to router/hotspot
3. Restart router/hotspot
4. Check signal strength (should be > -70 dBm)

---

### **Servos moving to wrong position:**

**Adjust in `esp32_servo_controller.ino`:**
```cpp
const int SERVO_REST = 90;   // Try different values: 80, 90, 100
const int SERVO_SORT = 0;    // Try: 0, 45, 90, 135, 180
```

Upload again and test!

---

### **Servos not moving at all:**

1. **Check power:**
   - Servo red wire to 5V ✅
   - Servo brown wire to GND ✅

2. **Check signal:**
   - Servo orange wire to GPIO 12 (RIPE) ✅
   - Servo orange wire to GPIO 13 (UNRIPE) ✅

3. **Test with browser:**
   - Go to `http://[ESP32_IP]`
   - Click test buttons
   - Watch Serial Monitor for errors

4. **Try external power:**
   - If servos are large (MG995, etc.), they need external 5V supply
   - Arduino Uno 5V pin can only provide ~400mA
   - Large servos need 1-2A

---

## 📊 System Flow

```
┌─────────────────────────────────────────────────────────┐
│                  COMPLETE SYSTEM FLOW                   │
└─────────────────────────────────────────────────────────┘

1. Mango passes in front of ESP32-CAM
         ↓
2. AI Model detects ripeness (Ripe/Unripe)
         ↓
3. ESP32-CAM sends TWO commands:
         ├→ Flask (for logging & dashboard)
         └→ ESP32 #2 (for physical sorting)
         ↓
4. ESP32 #2 receives command via WiFi
         ↓
5. ESP32 #2 activates correct servo
         ↓
6. Servo pushes mango to correct bin
         ↓
7. Dashboard shows real-time update
```

---

## 🎯 For Defense Presentation

### **Key Points to Emphasize:**

1. **AI-Powered Detection** ✅
   - Edge Impulse machine learning model
   - 75%+ confidence threshold
   - Real-time classification

2. **Wireless Communication** ✅
   - No physical wires between camera and servos
   - WiFi-based command transmission
   - Scalable architecture

3. **Web Dashboard** ✅
   - Real-time data monitoring
   - Historical analytics
   - Remote control capability

4. **Standalone Operation** ✅
   - No laptop required (after deployment)
   - Battery-powered components
   - Self-contained system

---

### **If Panel Asks About Limitations:**

**Be honest:**
- Current setup uses same WiFi network (local)
- For internet deployment, would need cloud server (Heroku, AWS)
- Servo positions need calibration for specific setup
- Detection accuracy depends on lighting conditions

**Future improvements:**
- Add more variety detection (not just Carabao)
- Implement conveyor belt speed control
- Add weight sensors for size classification
- Cloud deployment for remote monitoring

---

## 📱 Quick Reference

### **IP Addresses:**
- ESP32-CAM: `192.168.43.28` (example)
- ESP32 #2: `192.168.43.50` (CHANGE THIS!)
- Flask Server: `192.168.43.106:5000`

### **WiFi:**
- SSID: `Luis`
- Password: `hello1234`

### **GPIO Pins:**
- RIPE Servo: GPIO 12
- UNRIPE Servo: GPIO 13

### **Serial Monitor Baud Rate:**
- 115200 for both ESP32s

---

## ✅ Pre-Defense Checklist

**Hardware:**
- [ ] ESP32 #2 connected to 2 servos
- [ ] Servos positioned correctly above bins
- [ ] ESP32-CAM installed and aimed at sorting area
- [ ] Power supplies ready (2x power banks or adapters)
- [ ] All wires secured with tape/zip ties

**Software:**
- [ ] ESP32 Servo Controller uploaded and tested
- [ ] ESP32-CAM code updated with correct IP
- [ ] Flask server running on laptop
- [ ] Web dashboard accessible
- [ ] Firebase data syncing

**Testing:**
- [ ] Manual servo test (web interface)
- [ ] ESP32-CAM detection working
- [ ] WiFi communication established
- [ ] End-to-end sorting tested with real mango
- [ ] Dashboard showing real-time data

**Backup Plan:**
- [ ] Have spare ESP32 ready
- [ ] Backup code on USB drive
- [ ] Screenshots of working system
- [ ] Video recording of successful sort

---

## 🎉 You're Ready!

**System Benefits:**
✅ Wireless communication (no long wires!)
✅ Standalone operation (no laptop after setup!)
✅ Real-time data logging (Flask + Firebase!)
✅ Professional web dashboard (mobile-responsive!)
✅ AI-powered detection (Edge Impulse model!)

**Good luck sa defense! Kaya mo yan!** 🚀🥭

---

## 🆘 Emergency Contacts

If may problem during defense:
1. Check Serial Monitor ng both ESP32
2. Verify WiFi connection (both devices)
3. Test servo manually via web interface
4. Restart both ESP32 devices
5. Check IP addresses (ping test)

**Remember:** The system is working! Just need proper setup and testing! 💪
