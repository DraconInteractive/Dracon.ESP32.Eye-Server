#ifdef Template
#include "WiFi.h"
#include "Arduino.h"
#include "soc/soc.h"           // Disable brownout problems
#include "soc/rtc_cntl_reg.h"  // Disable brownout problems
#include "WebServer.h"
#include <StringArray.h>
#include <SPIFFS.h>
#include "AsyncUDP.h"
#include "camera_logic.h"

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

const char* ID = "espDracon02";

AsyncUDP udp;
AsyncUDP delayedUDP;
WebServer server(80);

char udpPacketBuffer[255];
const char* ap_ssid = "Dracon24";
const char* ap_password = "Rarceth1996!";

String IPAddressToString(const IPAddress& ipAddress)
{
  return String(ipAddress[0]) + String(".") +\
  String(ipAddress[1]) + String(".") +\
  String(ipAddress[2]) + String(".") +\
  String(ipAddress[3])  ;
}

void InitUDP () {
  Serial.print("Initialising UDP Stream");
  
  if (udp.listenMulticast(IPAddress(239,3,3,4), 11000)) {
    Serial.println ("UDP Listening on 239.3.3.4:11000");

    udp.onPacket([](AsyncUDPPacket packet) {
        Serial.printf("Packet received, UDP.\nLength: %i\nData: ", packet.length());
        Serial.write(packet.data(), packet.length());
        Serial.println();
        
        String dataString = (const char*)packet.data();
        if (dataString == "#Status") {
        packet.printf("ID: %s, IP: %s", ID, IPAddressToString(udp.listenIP()));
        } else if (dataString == "#ID") {
        packet.printf("Data received on %s", ID);
        } else if (dataString == "ping") {
        packet.print("pong");
        } else {
        packet.print(".");
        }
        Serial.print("|"+dataString+"|");
    });
  }
  delay(100);
}

void InitSTA () {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ap_ssid, ap_password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.println("_-");
  }
  Serial.println("Wifi Connected");
  Serial.println(WiFi.status());

  delay(100);

  Serial.println("IP Address: http://");
  Serial.println(WiFi.localIP());
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

void PinSetup () {
  pinMode(GPIO_NUM_21, OUTPUT);
  pinMode(GPIO_NUM_22, OUTPUT);
  digitalWrite(GPIO_NUM_21, HIGH);
  digitalWrite(GPIO_NUM_22, LOW);

  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
}

void WriteToLED (bool red, bool white, uint8_t value) {
    if (red) {
        digitalWrite(GPIO_NUM_21, value);
    }
    if (white) {
        digitalWrite(GPIO_NUM_22, value);
    }
}

void handle_jpg_stream() {
  Serial.print("Handling stream");
  WiFiClient client = server.client();
  String response = "HTTP/1.1 200 OK\r\n";
  response += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
  server.sendContent(response);

  GetJPGStream(client, server);
}

void handle_jpg() {
  Serial.println("Handling jpg");
  WiFiClient client = server.client();
  GetJPG(client, server);
}

void setup () {
    Serial.begin(115200);
    while (!Serial) {
        ;
    }
    PinSetup();
    InitSTA();
    InitUDP();

    CamStart();

    server.on("/", HTTP_GET, handleIndex);
    server.on("/stream", HTTP_GET, handle_jpg_stream);
    server.on("/jpg", HTTP_GET, handle_jpg);
    server.onNotFound(handleNotFound);
    server.begin();

    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

    Serial.print("Setup Finished.");
}

void loop () {
    if (WiFi.status() != WL_CONNECTED) {
        InitSTA();
    }

    server.handleClient();
    delay(1);
}
#endif