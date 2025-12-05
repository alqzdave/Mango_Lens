# ESP32-CAM Setup Guide for MangoLens

## 📋 What You Need

### Hardware:
- ESP32-CAM (AI-Thinker module)
- FTDI Programmer (or USB to TTL converter)
- Jumper wires
- Micro USB cable

### Software:
- Arduino IDE (or use PlatformIO in VS Code)
- ESP32 board support
- Required libraries (already in Arduino IDE)

---

## 🔧 Step 1: Install Arduino IDE & ESP32 Support

### If using Arduino IDE:

1. **Download Arduino IDE**: https://www.arduino.cc/en/software
2. **Add ESP32 Board Support**:
   - Open Arduino IDE
   - Go to `File > Preferences`
   - In "Additional Board Manager URLs", add:
     ```
     https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
     ```
   - Click OK
   - Go to `Tools > Board > Boards Manager`
   - Search for "esp32"
   - Install "esp32 by Espressif Systems"

3. **Select ESP32-CAM Board**:
   - Go to `Tools > Board > ESP32 Arduino`
   - Select "AI Thinker ESP32-CAM"

---

## 🔌 Step 2: Wire ESP32-CAM to FTDI Programmer

Connect the following pins:

| ESP32-CAM | FTDI Programmer |
|-----------|-----------------|
| 5V        | VCC (5V)        |
| GND       | GND             |
| U0R       | TX              |
| U0T       | RX              |
| IO0       | GND (for upload)|

**IMPORTANT**: 
- Connect IO0 to GND **only when uploading code**
- After upload, disconnect IO0 from GND and press RESET button

---

## 💻 Step 3: Configure and Upload Code

1. **Open the Arduino Code**:
   - Open `esp32_cam_code.ino` in Arduino IDE

2. **Edit WiFi Credentials** (Lines 18-19):
   ```cpp
   const char* ssid = "YOUR_WIFI_NAME";        // Change this
   const char* password = "YOUR_WIFI_PASSWORD"; // Change this
   ```

3. **Edit Flask Server URL** (Line 24):
   
   **For Local Testing** (computer and ESP32 on same WiFi):
   ```cpp
   const char* flaskServer = "http://192.168.1.100:5000";
   ```
   - Replace `192.168.1.100` with your computer's IP address
   - To find your IP:
     - Windows: Open CMD and type `ipconfig` → Look for "IPv4 Address"
     - Mac/Linux: Open Terminal and type `ifconfig` → Look for "inet"

   **For Production** (using your domain):
   ```cpp
   const char* flaskServer = "https://mangolens.me";
   ```

4. **Upload the Code**:
   - Make sure IO0 is connected to GND
   - Click the Upload button (→)
   - Wait for "Connecting........"
   - Press the RESET button on ESP32-CAM when you see dots
   - Wait for upload to complete

5. **After Upload**:
   - Disconnect IO0 from GND
   - Press RESET button on ESP32-CAM
   - Open Serial Monitor (`Tools > Serial Monitor`)
   - Set baud rate to `115200`
   - You should see startup messages

---

## 📡 Step 4: Get ESP32-CAM IP Address

1. **Open Serial Monitor** (baud rate: 115200)
2. Press RESET button on ESP32-CAM
3. You'll see output like:
   ```
   === ESP32-CAM MangoLens System ===
   ✓ Camera initialized successfully
   Connecting to WiFi: YourWiFiName
   ✓ WiFi connected!
   IP Address: 192.168.1.150
   
   === System Ready ===
   Camera Stream URL: http://192.168.1.150:81/stream
   ```

4. **Copy the IP address** (e.g., 192.168.1.150)

---

## 🌐 Step 5: Connect Camera in Web Dashboard

1. **Run Your Flask App**:
   ```powershell
   python app.py
   ```

2. **Open Browser**: Go to `http://localhost:5000`

3. **Login** to your MangoLens account

4. **Go to Sorting Page**: Click "Sorting" in the navigation

5. **Enter ESP32-CAM IP**: 
   - Type the IP address you got from Serial Monitor
   - Example: `192.168.1.150`
   - Click "Connect Camera"

6. **You should see the live camera feed!** 🎉

---

## 🎮 Step 6: Test the System

1. Click **"Start Sorting"** button
2. Place a mango in front of the camera
3. Watch the detection results appear!
4. Check the statistics panel for real-time counts
5. Click **"Stop Sorting"** to pause

---

## 🐛 Troubleshooting

### Camera Won't Upload Code:
- Make sure IO0 is connected to GND during upload
- Press RESET button when you see "Connecting......."
- Check all wire connections
- Try a different USB cable or FTDI programmer

### Can't Connect to WiFi:
- Check WiFi credentials are correct
- Make sure WiFi is 2.4GHz (ESP32 doesn't support 5GHz)
- Check WiFi signal strength

### Camera Feed Not Showing in Browser:
- Make sure ESP32 and computer are on **same WiFi network**
- Check firewall isn't blocking port 81
- Verify IP address is correct
- Try accessing directly: `http://ESP32_IP:81/stream` in browser

### "Failed to initialize camera":
- Check all camera module connections
- Make sure camera ribbon cable is properly inserted
- Camera module might be defective

### Low Frame Rate:
- Reduce image quality in code (increase `jpeg_quality` value)
- Use lower frame size (change `FRAMESIZE_VGA` to `FRAMESIZE_QVGA`)

---

## 📊 Camera Stream URLs

Once ESP32-CAM is running, you can access:

- **Live Stream**: `http://ESP32_IP:81/stream`
- **Single Snapshot**: `http://ESP32_IP/capture`

Test these in your browser to verify camera is working!

---

## ⚡ Power Supply Notes

- ESP32-CAM needs stable 5V power supply
- FTDI programmer power might not be enough
- For reliable operation, use external 5V power supply
- Connect GND between ESP32-CAM and power supply

---

## 🚀 Next Steps

After successful setup:

1. **Integrate Edge Impulse Model**: Add your trained model to the code
2. **Add Servo Control**: Connect servos to sort mangoes into bins
3. **Deploy to Production**: Update Flask server URL to your domain
4. **Test with Real Mangoes**: Calibrate detection accuracy

---

## 📝 Important Notes

- **Development Mode**: Keep Serial Monitor open to see debug messages
- **Production Mode**: ESP32 will auto-start after power on
- **IP Address**: May change after restart (consider using static IP)
- **Update Interval**: Currently detects every 3 seconds (adjustable in code)

---

## 📞 Need Help?

If you encounter issues:
1. Check Serial Monitor for error messages
2. Verify all connections
3. Test camera independently
4. Check network connectivity

Your ESP32-CAM is now ready to stream live video to your MangoLens dashboard! 🎥🥭
