
//
// ESP32 Wrover Module
// Partition scheme: Huge APP (3MB No OTA)
//
// --> Remember to short IO0 and GND pins
//
//
// Work ispired by:
// Ai-Thinker ESP32-CAM in the Arduino IDE [https://robotzero.one/esp32-cam-arduino-ide/]

#define __DEBUG__
#define __DEBUG_LEVEL 3

#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>

// ESP32-CAM AIThinker Camera
#include "esp_camera.h"

#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// WebServer
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

AsyncWebServer server(80);

// ArduinoJson
// https://arduinojson.org/
#include <ArduinoJson.h>

// NTP ClientLib 
// https://github.com/gmag11/NtpClient
#include <NtpClientLib.h>

NTPSyncEvent_t ntpEvent;
bool syncEventTriggered = false; // True if a time even has been triggered

// File System
#include <FS.h>   
#include "SPIFFS.h"

// Format SPIFFS if mount failed
#define FORMAT_SPIFFS_IF_FAILED 1

// SD
#include "SD.h"
#include "SPI.h"

#define SD_CLK 14
#define SD_CMD 15
#define SD_DATA0 2
#define SD_DATA1 4
#define SD_DATA2 12
#define SD_DATA3 13

// Firmware data
const char BUILD[] = __DATE__ " " __TIME__;
#define FW_NAME         "esp32-cam-module"
#define FW_VERSION      "0.0.1"

#include "soc/soc.h" //disable brownour problems
#include "soc/rtc_cntl_reg.h" //disable brownour problems

// Config
struct Config {
  // WiFi config
  char wifi_essid[16];
  char wifi_password[16];
  // NTP Config
  char ntp_server[16];
  int8_t ntp_timezone;
  // Host config
  char hostname[16];
};

#define CONFIG_FILE "/config.json"

File configFile;
Config config; // Global config object

static int last; // millis counter

#define PIR_PIN 16 // PIR on GPIO16
#define LED_PIN 4  // GPIO4 is the Flash LED


bool sdState=false;

// ************************************
// DEBUG_PRINT()
//
// send message via RSyslog (if enabled) or fallback on Serial 
// ************************************
void DEBUG_PRINT(String message,uint8_t level=10) {
#ifdef __DEBUG__
  if(level > __DEBUG_LEVEL) {
    Serial.println(message);
  }
#endif
}

// ************************************
// pirTimer()
//
// routine triggered on PIR sensor detection
// ************************************
hw_timer_t *pirTimer = NULL;

#define PIR_COUNTER_TRIGGER 10 // Re-trigger every 10 seconds

volatile bool pirState=false;
volatile bool pirTriggered=false;
volatile uint8_t pirCounter=0;

void IRAM_ATTR onPirTimer() {
  bool temp = digitalRead(PIR_PIN);
  if(temp != pirState) {
    pirState = temp;
    pirTimer = 0;
    if(pirState == true) {
      DEBUG_PRINT("PIR high");
      pirTriggered=true;
    } else {
      DEBUG_PRINT("PIR low");
    }
  } else {
    pirCounter++;
    if(pirCounter > PIR_COUNTER_TRIGGER) {
      pirTriggered=true;
    }
  }
}

// ************************************
// initCamera()
//
// Initialize ESP32 Camera
// ************************************
bool initCamera() {
  camera_config_t camera_config;
  camera_config.ledc_channel = LEDC_CHANNEL_0;
  camera_config.ledc_timer = LEDC_TIMER_0;
  camera_config.pin_d0 = Y2_GPIO_NUM;
  camera_config.pin_d1 = Y3_GPIO_NUM;
  camera_config.pin_d2 = Y4_GPIO_NUM;
  camera_config.pin_d3 = Y5_GPIO_NUM;
  camera_config.pin_d4 = Y6_GPIO_NUM;
  camera_config.pin_d5 = Y7_GPIO_NUM;
  camera_config.pin_d6 = Y8_GPIO_NUM;
  camera_config.pin_d7 = Y9_GPIO_NUM;
  camera_config.pin_xclk = XCLK_GPIO_NUM;
  camera_config.pin_pclk = PCLK_GPIO_NUM;
  camera_config.pin_vsync = VSYNC_GPIO_NUM;
  camera_config.pin_href = HREF_GPIO_NUM;
  camera_config.pin_sscb_sda = SIOD_GPIO_NUM;
  camera_config.pin_sscb_scl = SIOC_GPIO_NUM;
  camera_config.pin_pwdn = PWDN_GPIO_NUM;
  camera_config.pin_reset = RESET_GPIO_NUM;
  camera_config.xclk_freq_hz = 20000000;
  camera_config.pixel_format = PIXFORMAT_JPEG;
  camera_config.frame_size = FRAMESIZE_UXGA;
  camera_config.jpeg_quality = 10;
  camera_config.fb_count = 2;

  esp_err_t err = esp_camera_init(&camera_config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    return false;
  }
  
  return true;
}

// ************************************
// saveJpg()
//
// get and save JPG from ESP32-Camera
// ************************************
bool saveJpg() {
  camera_fb_t *fb = NULL;
  size_t jpg_buf_len = 0;
  uint8_t * jpg_buf = NULL;
  char jpg_file_name[32];

  fb = esp_camera_fb_get();
  if (!fb) {
    DEBUG_PRINT("Camera capture FAILED");
    return false;
  }
  if(fb->format != PIXFORMAT_JPEG) {
    bool jpeg_converted = frame2jpg(fb, 80, &jpg_buf, &jpg_buf_len);
    esp_camera_fb_return(fb);
    if(!jpeg_converted){
      DEBUG_PRINT("JPEG compression failed!");
      return false;
    } else {
      jpg_buf_len = fb->len;
      jpg_buf = fb->buf;
      if(sdState) {
        time_t t=now();
        sprintf(jpg_file_name,"CAPT_%d%d_%d%d%d.jpg",hour(t),minute(t),second(t),day(t),month(t),year(t));
        writeFile(jpg_file_name,(char *)jpg_buf);
        DEBUG_PRINT("Camera capture DONE: "+String(jpg_file_name));
      }
      return true;
    }
  }
}

// ************************************
// connectToWifi()
//
// connect to configured WiFi network
// ************************************
bool connectToWifi() {
  uint8_t timeout=0;

  if(strlen(config.wifi_essid) > 0) {
    DEBUG_PRINT("[INIT] Connecting to "+String(config.wifi_essid));

    WiFi.begin(config.wifi_essid, config.wifi_password);

    while((WiFi.status() != WL_CONNECTED)&&(timeout < 10)) {
      digitalWrite(LED_PIN, HIGH);
      delay(250);
      digitalWrite(LED_PIN, LOW);
      delay(250);
      timeout++;
    }
    if(WiFi.status() == WL_CONNECTED) {
      DEBUG_PRINT("OK. IP:"+WiFi.localIP().toString());

      if (MDNS.begin(config.hostname)) {
        DEBUG_PRINT("[INIT] MDNS responder started");
        // Add service to MDNS-SD
        MDNS.addService("http", "tcp", 80);
      }
      
      NTP.onNTPSyncEvent([](NTPSyncEvent_t event) {
        ntpEvent = event;
        syncEventTriggered = true;
      });

      // NTP
      DEBUG_PRINT("[INIT] NTP sync time on "+String(config.ntp_server));
      NTP.begin(config.ntp_server, config.ntp_timezone, true, 0);
      NTP.setInterval(63);
      
      return true;  
    } else {
      DEBUG_PRINT("[ERROR] Failed to connect to WiFi");
      return false;
    }
  } else {
    DEBUG_PRINT("[ERROR] Please configure Wifi");
    return false; 
  }
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
   
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();
  Serial.println();
  Serial.println();
  Serial.print(FW_NAME);
  Serial.print(" ");
  Serial.print(FW_VERSION);
  Serial.print(" ");
  Serial.println(BUILD);
  delay(5000);

// Initialize SPIFFS
  if(!SPIFFS.begin()){
    DEBUG_PRINT("[SPIFFS] Mount Failed. Try formatting...");
    if(SPIFFS.format()) {
      DEBUG_PRINT("[SPIFFS] Initialized successfully");
    } else {
      DEBUG_PRINT("[SPIFFS] Fatal error: restart!");
      ESP.restart();
    }
  } else {
    DEBUG_PRINT("[SPIFFS] OK");
  }

  // Load configuration
  loadConfigFile();

  // Initialize SD card (if present)
  sdState= initSDCard();

  // Connect to WiFi network
  connectToWifi();

  // Initialize Camera
  initCamera();

  // Init PIR sensor
  pinMode(PIR_PIN, INPUT);
  digitalWrite(PIR_PIN, LOW);
  pirTimer = timerBegin(0, 80, true);
  timerAttachInterrupt(pirTimer, &onPirTimer, true);
  timerAlarmWrite(pirTimer, 1000000, true); // Alarm on every second
  yield();
  timerAlarmEnable(pirTimer);

  // Initialize Web Server
  initWebServer();

  DEBUG_PRINT("Setup COMPLETE. Run!");
}

void loop() {
  // NTP ?
  if(syncEventTriggered) {
    if (ntpEvent) {
      DEBUG_PRINT("[NTP] Time Sync error: ");
      if (ntpEvent == noResponse)
        DEBUG_PRINT("[NTP] NTP server not reachable");
      else if (ntpEvent == invalidAddress)
        DEBUG_PRINT("[NTP] Invalid NTP server address");
    } else {
      DEBUG_PRINT("[NTP] Got NTP time: "+String(NTP.getTimeDateString(NTP.getLastNTPSync())));
    }
    syncEventTriggered = false;
  }
  
  if((millis() - last) > 5100) {    
    if(WiFi.status() != WL_CONNECTED) {
      connectToWifi();
    }
    last = millis ();
  }

  if(pirTriggered) {
    saveJpg();
    pirTriggered=false;
  }
}
