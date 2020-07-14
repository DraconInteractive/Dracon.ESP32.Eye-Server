//#define OLD
#define NEW
#define RTSP
#define WEBSERVER

#pragma region declarations
#ifdef OLD
#include "esp_camera.h"
#include "esp_timer.h"
#include <ESPAsyncWebServer.h>
#endif

#include "WiFi.h"
#include "img_converters.h"
#include "Arduino.h"
#include "soc/soc.h"           // Disable brownour problems
#include "soc/rtc_cntl_reg.h"  // Disable brownour problems
#include "driver/rtc_io.h"
#include "WebServer.h"
#include <StringArray.h>
#include <SPIFFS.h>
#include <FS.h>
#include "AsyncUDP.h"

#ifdef NEW
#include "CRtspSession.h"
#include "OV2640.h"
#include "OV2640Streamer.h"
#endif

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

#pragma endregion

#pragma region HTML
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
#pragma endregion
#ifdef OLD
boolean takeNewPhoto;
#define FILE_PHOTO "/photo.jpg"
AsyncWebServer server(80);
AsyncUDP udp;
const char* ap_ssid = "SJA_MODEM_01";
const char* ap_password = "12345678";

#pragma region espcam
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

void CamInit () {
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
    config.jpeg_quality = 16;
    config.fb_count = 1;
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
}
#pragma endregion

void InitUDP () {
  Serial.print("Initialising UDP Stream");
  IPAddress _targetIP = IPAddress(192,168,1,1);
  if (udp.listenMulticast(IPAddress(239,3,3,4), 11000)) {
    Serial.println ("UDP Listening on 239.3.3.4:11000");

    udp.onPacket([_targetIP](AsyncUDPPacket packet) {
      Serial.printf("Packet received, UDP.\nLength: %i\nData: ");
      Serial.write(packet.data(), packet.length());
      Serial.println();
      packet.print("Data received on DraconESP");
    });
  }
  delay(100);
}

void InitSTAWifi () {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ap_ssid, ap_password);
  while (WiFi.status() != WL_CONNECTED) {
    BlinkRed(350);
    delay(50);
    Serial.println("#Connecting#");
  }
  Serial.println("Wifi Connected");
  Serial.println(WiFi.status());

  delay(100);

  Serial.println("IP Address: http://");
  Serial.println(WiFi.localIP());
  
}
#endif

#ifdef NEW 
WebServer server(80);
OV2640 cam;
WiFiServer rtspServer(8554);
CStreamer *streamer;
CRtspSession *session;
IPAddress softIP;
void InitSoftAP () {
  const char *hostname = "draconESP";
  WiFi.mode(WIFI_MODE_AP);
  WiFi.persistent(false);
  bool result = WiFi.softAP(hostname, "12345678");
  WiFi.softAPConfig(IPAddress(192,168,1,1), IPAddress(192,168,1,1), IPAddress(255,255,255,0));
  
  delay(1000);
  if (!result) {
    Serial.println("Soft AP Config Failed");
    return;
  } else {
    Serial.println("Soft AP Config Success");
    Serial.print("Mac ADDR: ");
    Serial.println(WiFi.softAPmacAddress());
    softIP = WiFi.softAPIP();
    Serial.println(softIP);
  }
}

void handle_jpg_stream(void) {
  WiFiClient client = server.client();
  String response = "HTTP/1.1 200 OK\r\n";
  response += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
  server.sendContent(response);

  while (1) {
    cam.run();
    if (!client.connected()) {
      break;
    }
    response = "--frame\r\n";
    response += "Content-Type: image/jpeg\r\n\r\n";
    server.sendContent(response);

    client.write((char *)cam.getfb(), cam.getSize());
    server.sendContent("\r\n");
    if (!client.connected()) {
      break;
    }
  }
}

void handle_jpg(void) {
  Serial.println("Handling jpg");
  Serial.println("Getting client");
  WiFiClient client = server.client();
  Serial.println("Running cam");
  cam.run();
  if (!client.connected()) {
    Serial.println("Client not connected");
    return;
  }
  String response = "HTTP/1.1 200 OK\r\n";
  response += "Content-disposition: inline; filename=capture.jpg\r\n";
  response += "Content-type: image/jpeg\r\n\r\n";
  server.sendContent(response);
  client.write((char *)cam.getfb(), cam.getSize());
}

void handleNotFound() {
  String message = "Server is running (just not here!)\n\n";
    message += "URI: ";
    message += server.uri();
    message += "\nMethod: ";
    message += (server.method() == HTTP_GET) ? "GET" : "POST";
    message += "\nArguments: ";
    message += server.args();
    message += "\n";
    server.send(200, "text/plain", message);
}

void handleIndex () {
  String message = "Server is running!\n\n";
    message += "URI: ";
    message += server.uri();
    message += "\nMethod: ";
    message += (server.method() == HTTP_GET) ? "GET" : "POST";
    message += "\nArguments: ";
    message += server.args();
    message += "\n";
    server.send(200, "text/plain", message);
}

#endif

#pragma region Blink
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
#pragma endregion

void PinSetup () {
  pinMode(GPIO_NUM_21, OUTPUT);
  pinMode(GPIO_NUM_22, OUTPUT);
  digitalWrite(GPIO_NUM_21, HIGH);
  digitalWrite(GPIO_NUM_22, LOW);

  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
}


void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  while (!Serial) {
    ;
  }
  PinSetup();

#ifdef OLD
  InitSTAWifi();
  if(!SPIFFS.begin(true)) {
    Serial.print("SPIFFS up failed");
    ESP.restart();
  } else {
    delay(500);
  }

  CamInit();
  cam.init(esp32cam_config);
  //InitUDP();

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
  server.begin();
#endif

#ifdef NEW
  InitSoftAP();
  cam.init(espeyecam_config);
  #ifdef WEBSERVER 
    server.on("/", HTTP_GET, handleIndex);
    server.on("/stream", HTTP_GET, handle_jpg_stream);
    server.on("/jpg", HTTP_GET, handle_jpg);
    server.onNotFound(handleNotFound);
    server.begin();
  #endif
  #ifdef RTSP
    rtspServer.begin();
  #endif
#endif

  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  Serial.print("Setup Finished.");
}

void loop() {
  // put your main code here, to run repeatedly:
#ifdef OLD
  if (takeNewPhoto) {
    capturePhotoSaveSpiffs();
    takeNewPhoto = false;
  }
#endif
  #ifdef WEBSERVER
    server.handleClient();
    
  #endif

  #ifdef RTSP
    uint32_t msecPerFrame = 100;
    static uint32_t lastimage = millis();

    if (session) {
      session->handleRequests(0);
      uint32_t now = millis();
      if (now > lastimage + msecPerFrame || now < lastimage) {
        session->broadcastCurrentFrame(now);
        lastimage = now;

        now = millis();
        if (now > lastimage + msecPerFrame) {
          printf("Warning! Exceeding max framerate of %d ms\n", now - lastimage);
        }
      }

      if (session->m_stopped) {
        delete session;
        delete streamer;
        session = NULL;
        streamer = NULL;
      }
    } else {
      WiFiClient client = rtspServer.accept();
      if (client) {
        streamer = new OV2640Streamer(&client, cam);
        session = new CRtspSession(&client, streamer);
        Serial.print("Client: ");
        Serial.print(client.remoteIP());
        Serial.println();
      }
    }
  #endif
  delay(1);
}




