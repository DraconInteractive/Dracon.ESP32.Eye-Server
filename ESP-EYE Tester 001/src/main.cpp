#include "WiFi.h"
#include "esp_camera.h"
#include "esp_timer.h"
#include "img_converters.h"
#include "Arduino.h"
#include "soc/soc.h"           // Disable brownour problems
#include "soc/rtc_cntl_reg.h"  // Disable brownour problems
#include "driver/rtc_io.h"
#include <ESPAsyncWebServer.h>
#include <StringArray.h>
#include <SPIFFS.h>
#include <FS.h>

#define PWDN_GPIO_NUM    -1
#define RESET_GPIO_NUM   -1
#define XCLK_GPIO_NUM    4
#define SIOD_GPIO_NUM    18
#define SIOC_GPIO_NUM    23
#define Y9_GPIO_NUM      36
#define Y8_GPIO_NUM      37
#define Y7_GPIO_NUM      38
#define Y6_GPIO_NUM      39
#define Y5_GPIO_NUM      35
#define Y4_GPIO_NUM      14
#define Y3_GPIO_NUM      13
#define Y2_GPIO_NUM      34
#define VSYNC_GPIO_NUM   5
#define HREF_GPIO_NUM    27
#define PCLK_GPIO_NUM    25

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { text-align:center; }
    .vert { margin-bottom: 10%; }
    .hori{ margin-bottom: 0%; }
  </style>
</head>
<body>
  <div id="container">
    <h2>ESP32-CAM Last Photo</h2>
    <p>It might take more than 5 seconds to capture a photo.</p>
    <p>
      <button onclick="rotatePhoto();">ROTATE</button>
      <button onclick="capturePhoto();">CAPTURE PHOTO</button>
      <button onclick="location.reload();">REFRESH PAGE</button>
    </p>
  </div>
  <div><img src="saved-photo" id="photo" width="70%"></div>
</body>
<script>
  var deg = 0;
  function capturePhoto() {
    var xhr = new XMLHttpRequest();
    xhr.open('GET', "/capture", true);
    xhr.send();
  }
  function rotatePhoto() {
    var img = document.getElementById("photo");
    deg += 90;
    if(isOdd(deg/90)){ document.getElementById("container").className = "vert"; }
    else{ document.getElementById("container").className = "hori"; }
    img.style.transform = "rotate(" + deg + "deg)";
  }
  function isOdd(n) { return Math.abs(n % 2) == 1; }
</script>
</html>)rawliteral";

const char* ssid = "SJA_MODEM_01";
const char* password = "12345678";

boolean takeNewPhoto;
#define FILE_PHOTO "/photo.jpg"

AsyncWebServer server(80);

void BlinkBoth_Async () {
  digitalWrite(GPIO_NUM_21, HIGH);
  digitalWrite(GPIO_NUM_22, LOW);
  delay(250);
  digitalWrite(GPIO_NUM_22, HIGH);
  digitalWrite(GPIO_NUM_21, LOW);
}

void BlinkBoth_Sync (int interval) {
  digitalWrite(GPIO_NUM_21, HIGH);
  digitalWrite(GPIO_NUM_22, HIGH);
  delay(interval);
  digitalWrite(GPIO_NUM_22, LOW);
  digitalWrite(GPIO_NUM_21, LOW);
  delay(interval);
  digitalWrite(GPIO_NUM_21, HIGH);
  digitalWrite(GPIO_NUM_22, HIGH);
}

void BlinkRed (int interval) {
  digitalWrite(GPIO_NUM_21, HIGH);
  delay(interval);
  digitalWrite(GPIO_NUM_21, LOW);
  delay(interval);
  digitalWrite(GPIO_NUM_21, HIGH);
}

void BlinkWhite (int interval) {
  digitalWrite(GPIO_NUM_22, HIGH);
  delay(interval);
  digitalWrite(GPIO_NUM_22, LOW);
  delay(interval);
  digitalWrite(GPIO_NUM_22, HIGH);
}

void PinSetup () {
  pinMode(GPIO_NUM_21, OUTPUT);
  pinMode(GPIO_NUM_22, OUTPUT);
  digitalWrite(GPIO_NUM_21, HIGH);
  digitalWrite(GPIO_NUM_22, LOW);

  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
}

// Check if photo capture was successful
bool checkPhoto( fs::FS &fs ) {
  File f_pic = fs.open( FILE_PHOTO );
  unsigned int pic_sz = f_pic.size();
  return ( pic_sz > 100 );
}

// Capture Photo and Save it to SPIFFS
void capturePhotoSaveSpiffs( void ) {
  camera_fb_t * fb = NULL; // pointer
  bool ok = 0; // Boolean indicating if the picture has been taken correctly

  do {
    // Take a photo with the camera
    Serial.println("Taking a photo...");

    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      return;
    } else {
      Serial.printf("Camera capture success, size: %i\n", fb->len);
    }

    // Photo file name
    Serial.printf("Picture file name: %s\n", FILE_PHOTO);
    File file = SPIFFS.open(FILE_PHOTO, FILE_WRITE);

    // Insert the data in the photo file
    if (!file) {
      Serial.println("Failed to open file in writing mode");
    }
    else {
      file.write(fb->buf, fb->len); // payload (image), payload length
      Serial.print("The picture has been saved in ");
      Serial.print(FILE_PHOTO);
      Serial.print(" - Size: ");
      Serial.print(file.size());
      Serial.println(" bytes");
    }
    // Close the file
    file.close();
    esp_camera_fb_return(fb);

    // check if file has been correctly saved in SPIFFS
    ok = checkPhoto(SPIFFS);
  } while ( !ok );
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  PinSetup();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    BlinkRed(350);
    delay(50);
    Serial.println("#Connecting#");
  }
  Serial.println("Wifi Connected");
  Serial.println(WiFi.status());
  delay(100);

  if(!SPIFFS.begin(true)) {
    digitalWrite(GPIO_NUM_22, HIGH);
    ESP.deepSleep(60);
  } else {
    delay(500);
  }

  Serial.print("IP Address: http://");
  Serial.println(WiFi.localIP());

  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  // OV2640 camera module
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 8;
    config.fb_count = 2;
    Serial.println("PSRAM Found");
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
    Serial.println("PSRAM Not Found");
  }
  
  // Camera init

  esp_err_t err = esp_camera_init(&config);
  
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
  }

  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/html", index_html);
  });

  server.on("/capture", HTTP_GET, [](AsyncWebServerRequest * request) {
    takeNewPhoto = true;
    request->send_P(200, "text/plain", "Taking Photo");
  });

  server.on("/saved-photo", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, FILE_PHOTO, "image/jpg", false);
  });

  // Start server
  server.begin();

  Serial.println("Setup Finished. Server Active.");
}

void loop() {
  // put your main code here, to run repeatedly:
  if (takeNewPhoto) {
    capturePhotoSaveSpiffs();
    takeNewPhoto = false;
  }
  delay(1);
}




