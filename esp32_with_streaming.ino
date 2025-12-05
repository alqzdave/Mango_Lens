/* Edge Impulse Arduino examples with WiFi Streaming
 * Modified to work with MangoLens Flask System
 * Now includes live camera streaming!
 */

#include <Mango_inferencing.h>
#include "edge-impulse-sdk/dsp/image/image.hpp"
#include "esp_camera.h"
#include <WiFi.h>
#include "esp_http_server.h"
#include <HTTPClient.h>

// ===== CHANGE THESE =====
const char* ssid = "Luis";           // Your WiFi name
const char* password = "hello1234";    // Your WiFi password
const char* flaskServer = "http://192.168.43.106:5000";  // Your computer's IP
const char* esp32ServoIP = "http://192.168.43.237";  // ESP32 Servo Controller IP ✅ UPDATED!
// ========================

#define CAMERA_MODEL_AI_THINKER

#define EI_CAMERA_RAW_FRAME_BUFFER_COLS           320
#define EI_CAMERA_RAW_FRAME_BUFFER_ROWS           240
#define EI_CAMERA_FRAME_BYTE_SIZE                 3

static bool debug_nn = false;
static bool is_initialised = false;
uint8_t *snapshot_buf;
httpd_handle_t stream_httpd = NULL;
bool sortingActive = false;
bool detectionInProgress = false;

static camera_config_t camera_config = {
    .pin_pwdn = 32,
    .pin_reset = -1,
    .pin_xclk = 0,
    .pin_sscb_sda = 26,
    .pin_sscb_scl = 27,
    .pin_d7 = 35,
    .pin_d6 = 34,
    .pin_d5 = 39,
    .pin_d4 = 36,
    .pin_d3 = 21,
    .pin_d2 = 19,
    .pin_d1 = 18,
    .pin_d0 = 5,
    .pin_vsync = 25,
    .pin_href = 23,
    .pin_pclk = 22,
    .xclk_freq_hz = 20000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,
    .pixel_format = PIXFORMAT_JPEG,
    .frame_size = FRAMESIZE_QVGA,
    .jpeg_quality = 12,
    .fb_count = 1,
    .fb_location = CAMERA_FB_IN_PSRAM,
    .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
};

bool ei_camera_init(void);
void ei_camera_deinit(void);
bool ei_camera_capture(uint32_t img_width, uint32_t img_height, uint8_t *out_buf);
static int ei_camera_get_data(size_t offset, size_t length, float *out_ptr);
void connectWiFi();
void startStreamServer();
void sendDetectionToFlask(String ripeness, float confidence);

// Stream handler for live video
static esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t * fb = NULL;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t * _jpg_buf = NULL;
  char * part_buf[64];

  static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=frame";
  static const char* _STREAM_BOUNDARY = "\r\n--frame\r\n";
  static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

  res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  if(res != ESP_OK){
    return res;
  }

  while(true){
    // Skip if detection is using camera
    if (detectionInProgress) {
      delay(50);  // Reduced delay for faster response
      continue;
    }
    
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      res = ESP_FAIL;
    } else {
      if(fb->format != PIXFORMAT_JPEG){
        bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
        esp_camera_fb_return(fb);
        fb = NULL;
        if(!jpeg_converted){
          Serial.println("JPEG compression failed");
          res = ESP_FAIL;
        }
      } else {
        _jpg_buf_len = fb->len;
        _jpg_buf = fb->buf;
      }
    }
    if(res == ESP_OK){
      size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);
      res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
    }
    if(res == ESP_OK){
      res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
    }
    if(res == ESP_OK){
      res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
    }
    if(fb){
      esp_camera_fb_return(fb);
      fb = NULL;
      _jpg_buf = NULL;
    } else if(_jpg_buf){
      free(_jpg_buf);
      _jpg_buf = NULL;
    }
    if(res != ESP_OK){
      break;
    }
  }
  return res;
}

void setup() {
    Serial.begin(115200);
    while (!Serial);

    Serial.println("\n========================================");
    Serial.println("  MangoLens ESP32-CAM with Detection");
    Serial.println("========================================\n");

    // Initialize camera
    if (!ei_camera_init()) {
        ei_printf("Failed to initialize Camera!\r\n");
    } else {
        ei_printf("✓ Camera initialized\r\n");
    }

    // Connect to WiFi
    connectWiFi();

    // Start streaming server
    startStreamServer();

    Serial.println("\n========================================");
    Serial.println("         SYSTEM READY!");
    Serial.println("========================================");
    Serial.print("Camera Stream URL: http://");
    Serial.print(WiFi.localIP());
    Serial.println(":81/stream");
    Serial.println("\nEnter this IP in your dashboard:");
    Serial.println(WiFi.localIP());
    Serial.println("========================================\n");

    ei_printf("\nStarting continuous inference in 2 seconds...\n");
    ei_sleep(2000);
}

void loop() {
    // Detection runs every 2 seconds - balanced for smooth streaming
    delay(2000);  // Wait 2 seconds between detections

    // Check if camera is initialized before proceeding
    if (!is_initialised) {
        ei_printf("Camera not initialized, attempting to initialize...\n");
        if (!ei_camera_init()) {
            ei_printf("Failed to initialize camera, retrying...\n");
            delay(1000);
            return;
        }
    }

    detectionInProgress = true;  // Lock camera for detection
    
    snapshot_buf = (uint8_t*)malloc(EI_CAMERA_RAW_FRAME_BUFFER_COLS * EI_CAMERA_RAW_FRAME_BUFFER_ROWS * EI_CAMERA_FRAME_BYTE_SIZE);
    if (snapshot_buf == nullptr) {
        ei_printf("ERR: Failed to allocate snapshot buffer!\n");
        detectionInProgress = false;
        return;
    }

    ei::signal_t signal;
    signal.total_length = EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT;
    signal.get_data = &ei_camera_get_data;

    if (!ei_camera_capture(EI_CLASSIFIER_INPUT_WIDTH, EI_CLASSIFIER_INPUT_HEIGHT, snapshot_buf)) {
        ei_printf("Failed to capture image\r\n");
        free(snapshot_buf);
        detectionInProgress = false;
        return;
    }

    ei_impulse_result_t result = { 0 };
    EI_IMPULSE_ERROR err = run_classifier(&signal, &result, debug_nn);
    if (err != EI_IMPULSE_OK) {
        ei_printf("ERR: Failed to run classifier (%d)\n", err);
        free(snapshot_buf);
        detectionInProgress = false;
        return;
    }

    // Find the highest confidence score
    int highest_class_index = 0;
    float highest_score = result.classification[0].value;

    for (uint16_t i = 1; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
        if (result.classification[i].value > highest_score) {
            highest_score = result.classification[i].value;
            highest_class_index = i;
        }
    }

    // Apply confidence threshold
    float CONFIDENCE_THRESHOLD = 0.75;
    if (highest_score > CONFIDENCE_THRESHOLD) {
        String ripeness = String(ei_classifier_inferencing_categories[highest_class_index]);
        
        // Translate Tagalog to English for Flask
        if (ripeness == "Hilaw") ripeness = "Unripe";
        else if (ripeness == "Hinog") ripeness = "Ripe";
        else if (ripeness == "Sobrang Hinog") ripeness = "Overripe";
        
        ei_printf("Predicted Ripeness Level: %s with confidence: %.2f\r\n",
                  ripeness.c_str(), highest_score);
        
        // Send to Flask server
        sendDetectionToFlask(ripeness, highest_score * 100);
    } else {
        ei_printf("No mango detected (confidence too low: %.2f)\r\n", highest_score);
    }

    free(snapshot_buf);
    detectionInProgress = false;  // Release camera for streaming
}

void connectWiFi() {
    Serial.print("Connecting to WiFi: ");
    Serial.println(ssid);
    
    WiFi.begin(ssid, password);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    Serial.println();
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("✓ WiFi connected!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("✗ WiFi connection failed!");
    }
}

void startStreamServer() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 81;

    httpd_uri_t stream_uri = {
        .uri       = "/stream",
        .method    = HTTP_GET,
        .handler   = stream_handler,
        .user_ctx  = NULL
    };

    if (httpd_start(&stream_httpd, &config) == ESP_OK) {
        httpd_register_uri_handler(stream_httpd, &stream_uri);
        Serial.println("✓ Stream server started on port 81");
    } else {
        Serial.println("✗ Failed to start stream server");
    }
}

void sendDetectionToFlask(String ripeness, float confidence) {
    if (WiFi.status() == WL_CONNECTED) {
        // First check if sorting is active
        HTTPClient checkHttp;
        String checkUrl = String(flaskServer) + "/api/sorting-control";
        checkHttp.begin(checkUrl);
        int checkCode = checkHttp.GET();
        
        bool shouldContinue = true;
        if (checkCode == 200) {
            String response = checkHttp.getString();
            Serial.print("Control response: ");
            Serial.println(response);
            
            // Check if response contains "active": true (with or without spaces)
            if (response.indexOf("true") < 0) {
                Serial.println("⏸ Sorting stopped by web interface");
                shouldContinue = false;
            } else {
                Serial.println("✓ Sorting is active, sending data...");
            }
        } else {
            Serial.print("✗ Control check failed with code: ");
            Serial.println(checkCode);
        }
        checkHttp.end();
        
        if (!shouldContinue) {
            return;  // Don't send data if sorting is stopped
        }
        
        // Send to Flask for data logging
        HTTPClient http;
        String url = String(flaskServer) + "/api/sorting-data";
        
        http.begin(url);
        http.addHeader("Content-Type", "application/json");
        
        // Assuming variety detection or default to "Carabao"
        String jsonData = "{\"variety\":\"Carabao\",\"ripeness\":\"" + ripeness + 
                         "\",\"confidence\":" + String(confidence) + ",\"count\":1}";
        
        int httpResponseCode = http.POST(jsonData);
        
        if (httpResponseCode > 0) {
            Serial.print("✓ Data sent to Flask: ");
            Serial.println(http.getString());
        } else {
            Serial.print("✗ Error sending data: ");
            Serial.println(httpResponseCode);
        }
        
        http.end();
        
        // ===== NEW: Send to ESP32 Servo Controller =====
        sendToServoController(ripeness);
        // ===============================================
    }
}

void sendToServoController(String ripeness) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("⚠ WiFi not connected, can't send to servo controller");
        return;
    }
    
    HTTPClient http;
    String url = String(esp32ServoIP) + "/sort?ripeness=" + ripeness;
    
    Serial.print("🤖 Sending to Servo Controller: ");
    Serial.println(ripeness);
    
    http.begin(url);
    int httpResponseCode = http.POST("");
    
    if (httpResponseCode > 0) {
        String response = http.getString();
        Serial.print("✓ Servo Controller response: ");
        Serial.println(response);
    } else {
        Serial.print("✗ Servo Controller error: ");
        Serial.println(httpResponseCode);
        Serial.println("⚠ Check if ESP32 Servo Controller is running!");
        Serial.print("⚠ Expected IP: ");
        Serial.println(esp32ServoIP);
    }
    
    http.end();
}

bool ei_camera_init(void) {
    if (is_initialised) return true;

    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed with error 0x%x\n", err);
        return false;
    }

    sensor_t *s = esp_camera_sensor_get();
    if (s->id.PID == OV3660_PID) {
        s->set_vflip(s, 1);
        s->set_brightness(s, 1);
        s->set_saturation(s, 0);
    }

    is_initialised = true;
    return true;
}

void ei_camera_deinit(void) {
    esp_err_t err = esp_camera_deinit();
    if (err != ESP_OK) {
        ei_printf("Camera deinit failed\n");
        return;
    }

    is_initialised = false;
}

bool ei_camera_capture(uint32_t img_width, uint32_t img_height, uint8_t *out_buf) {
    bool do_resize = false;

    if (!is_initialised) {
        ei_printf("ERR: Camera is not initialized\r\n");
        return false;
    }

    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        ei_printf("Camera capture failed\n");
        return false;
    }

    bool converted = fmt2rgb888(fb->buf, fb->len, PIXFORMAT_JPEG, snapshot_buf);
    esp_camera_fb_return(fb);

    if (!converted) {
        ei_printf("Conversion failed\n");
        return false;
    }

    if (img_width != EI_CAMERA_RAW_FRAME_BUFFER_COLS || img_height != EI_CAMERA_RAW_FRAME_BUFFER_ROWS) {
        do_resize = true;
    }

    if (do_resize) {
        ei::image::processing::crop_and_interpolate_rgb888(
            out_buf,
            EI_CAMERA_RAW_FRAME_BUFFER_COLS,
            EI_CAMERA_RAW_FRAME_BUFFER_ROWS,
            out_buf,
            img_width,
            img_height);
    }

    return true;
}

static int ei_camera_get_data(size_t offset, size_t length, float *out_ptr) {
    size_t pixel_ix = offset * 3;
    size_t pixels_left = length;
    size_t out_ptr_ix = 0;

    while (pixels_left != 0) {
        out_ptr[out_ptr_ix] = (snapshot_buf[pixel_ix + 2] << 16) +
                              (snapshot_buf[pixel_ix + 1] << 8) +
                              snapshot_buf[pixel_ix];
        out_ptr_ix++;
        pixel_ix += 3;
        pixels_left--;
    }
    return 0;
}

#if !defined(EI_CLASSIFIER_SENSOR) || EI_CLASSIFIER_SENSOR != EI_CLASSIFIER_SENSOR_CAMERA
#error "Invalid model for current sensor"
#endif
