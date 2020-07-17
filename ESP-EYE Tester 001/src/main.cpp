#define WEBSERVER

//#define APWIFI
#define STAWIFI
#define MUDP

#pragma region declarations

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

#include "CRtspSession.h"
#include "OV2640.h"
#include "OV2640Streamer.h"

#include <ArduinoJson.h>

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

#define BLE_SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define BLE_CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

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

#define CONFIG_LOCATION "/config.json"
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

#define ID "espDracon01"

AsyncUDP udp;
char udpPacketBuffer[255];
const char* ap_ssid = "SJA_MODEM_01";
const char* ap_password = "12345678";

void openConfig () {
  File file = SPIFFS.open(CONFIG_LOCATION, FILE_READ);
  String data = file.readString();
  Serial.println();
  Serial.print("Data: ");
  Serial.print(data);
  Serial.println();
  file.close();
}

void writeConfig (char* _mode, char* _ssid, char* _password, bool reset) {
  File file = SPIFFS.open(CONFIG_LOCATION, FILE_WRITE);
  if (file) {
    Serial.println("Writing to file");
    if (file.printf("{\"mode\":\"%s\",\"ssid\":\"%s\",\"password\":\"%s\"}", _mode, _ssid, _password)) {
      Serial.println("Write successful");
    } else {
      Serial.println ("Write Failed");
    }    
  } else {
    Serial.println("Cant write to file");
  }
  file.close();

  if (reset) {
    ESP.restart();
  }
}
String IpAddress2String(const IPAddress& ipAddress)
{
  return String(ipAddress[0]) + String(".") +\
  String(ipAddress[1]) + String(".") +\
  String(ipAddress[2]) + String(".") +\
  String(ipAddress[3])  ;
}

void InitUDPPacket () {
  IPAddress _targetIP = IPAddress(192,168,1,100);
  udp.onPacket([_targetIP](AsyncUDPPacket packet) {
    Serial.printf("Packet received, UDP.\nLength: %i\nData: ", packet.length());
    Serial.write(packet.data(), packet.length());
    Serial.println();

    String dataString = (const char*)packet.data();
    Serial.print("|"+dataString+"|");

    if (dataString == "#Status\n") {
      packet.printf("ID: %s, IP: %s", ID, IpAddress2String(udp.listenIP()));
    } else if (dataString == "#ID") {
      packet.printf("Data received on %s", ID);
    } else if (dataString == "ping") {
      packet.print("pong");
    } else if (dataString == "rconfig") {
      openConfig();
      packet.print("Read config");
    } else if (dataString == "wconfig") {
      writeConfig("STA", "SJA_MODEM_01", "12345678", false);
      packet.print("Wrote config");
    } else if (dataString == "network") {
      Serial.print("Updating network");
      writeConfig("STA", "SJA_MODEM_01", "12345678", true);
    } else {
      packet.print(".");
    }
    
  });
}

void InitUDP () {
  Serial.print("Initialising UDP Stream");
  
  if (udp.listenMulticast(IPAddress(239,3,3,4), 11000)) {
    Serial.println ("UDP Listening on 239.3.3.4:11000");

    InitUDPPacket();
  }
  delay(100);
}

IPAddress softIP;
void InitSoftAP (char* _ssid, char* _password) {
  const char *hostname = _ssid;
  WiFi.mode(WIFI_MODE_AP);
  WiFi.persistent(false);
  bool result = WiFi.softAP(hostname, _password);
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

void InitSTA (char* _ssid, char* _password) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(_ssid, _password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(50);
    Serial.println("#Connecting#");
  }
  Serial.println("Wifi Connected");
  Serial.println(WiFi.status());

  delay(100);

  Serial.println("IP Address: http://");
  Serial.println(WiFi.localIP());
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

void InitWiFi () {
  Serial.println("Initializing wifi connection");
  Serial.println("Opening config");
  File file = SPIFFS.open(CONFIG_LOCATION, FILE_READ);
  std::unique_ptr<char[]> buf(new char[file.size()]);
  if (file) {
    Serial.println("File opened. Reading");
    file.readBytes(buf.get(), file.size());

    DynamicJsonDocument doc(512);
    DeserializationError err = deserializeJson(doc, buf.get());
    if (err) {
      Serial.println("Config isnt json, or doesnt exist. F+");
      InitSoftAP("draconESP", "12345678");
    } else {
      JsonObject obj = doc.as<JsonObject>();
      String _s = obj["ssid"];
      String _p = obj["password"];
      int sn = _s.length();
      int pn = _p.length();
      char _ssid[sn + 1];
      char _password[pn + 1];
      strcpy(_ssid, _s.c_str());
      strcpy(_password, _p.c_str());
      Serial.printf("SSID: %s", _ssid);
      Serial.printf("Pass: %s", _password);
      if (doc["mode"] == "STA") {
        InitSTA(_ssid, _password);
      } else {
        InitSoftAP(_ssid, _password);
      }
    }
    file.close();
  } else {
    Serial.println("Config isnt json, or doesnt exist. F-");
    InitSoftAP("draconESP", "12345678");
    file.close();
    return;
  }
}

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string value = pCharacteristic->getValue();

      if (value.length() > 0) {
        Serial.println("*********");
        Serial.print("New value: ");
        for (int i = 0; i < value.length(); i++)
          Serial.print(value[i]);

        Serial.println();
        Serial.println("*********");
      }
      pCharacteristic->setValue(value);
    }
};

void InitBLE () {
  Serial.println("Intializing BLE");
  BLEDevice::init("draconESP01");
  BLEServer *pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService(BLE_SERVICE_UUID);
  BLECharacteristic *pCharacteristic = pService->createCharacteristic(BLE_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
  pCharacteristic->setCallbacks(new MyCallbacks());
  pCharacteristic->setValue("Hello World draconBLE");
  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(BLE_SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  Serial.println("BLE Characteristic Defined");
}
WebServer server(80);
OV2640 cam;

#pragma region WebServerHandlers
void handle_jpg_stream(void) {
  Serial.print("Handling stream");
  WiFiClient client = server.client();
  String response = "HTTP/1.1 200 OK\r\n";
  response += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
  server.sendContent(response);

  while (client.connected()) {
    cam.run();
    if (!client.connected()) {
      Serial.print("Client disconnected");
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
  Serial.print("Finished");
}

void handle_jpg(void) {
  Serial.println("Handling jpg");
  WiFiClient client = server.client();
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
  Serial.println("Serving not found");
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
  Serial.println("Serving index");
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
#pragma endregion

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

  if(!SPIFFS.begin(true)) {
    Serial.print("SPIFFS up failed");
    ESP.restart();
  } else {
    delay(500);
  }

  InitBLE();
  //InitWiFi();
  InitSTA("SJA_MODEM_02","12345678");
#ifdef MUDP
  //InitUDP();
#endif
Serial.println("Initialising camera");
cam.init(espeyecam_config);
Serial.println ("Setting up web server endpoints");
#ifdef WEBSERVER 
  server.on("/", HTTP_GET, handleIndex);
  server.on("/stream", HTTP_GET, handle_jpg_stream);
  server.on("/jpg", HTTP_GET, handle_jpg);
  server.onNotFound(handleNotFound);
  server.begin();
#endif

  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  Serial.print("Setup Finished.");
}

void loop() {
  #ifdef WEBSERVER
    server.handleClient();
  #endif

  delay(1);
}




