#include "WiFi.h"
#include "Arduino.h"
#include "soc/soc.h"           // Disable brownout problems
#include "soc/rtc_cntl_reg.h"  // Disable brownout problems
#include "WebServer.h"
#include <StringArray.h>
#include <SPIFFS.h>
#include "AsyncUDP.h"
#include "camera_logic.h"
#include "speechsrc.h"
#include <SPIFFS.h>

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
#define PCLK_GPIO_NUM    25
#define HREF_GPIO_NUM    27

const char* ID = "espDracon02";

AsyncUDP udp;
AsyncUDP udp2;
WebServer server(80);

char udpPacketBuffer[255];
const char* ap_ssid = "Dracon24";
const char* ap_password = "Rarceth1996!";

void InitUDP2 (IPAddress target);
void InitUDP();

String IPAddressToString(const IPAddress& ipAddress)
{
  return String(ipAddress[0]) + String(".") +\
  String(ipAddress[1]) + String(".") +\
  String(ipAddress[2]) + String(".") +\
  String(ipAddress[3])  ;
}

void HandleUDPCommands (String dataString, AsyncUDPPacket &packet) {
    Serial.println("Handling command data");
    if (dataString == "#Status") {
      packet.printf("ID: %s, IP: %s", ID, IPAddressToString(udp.listenIP()).c_str());
    } else if (dataString == "#ID") {
      packet.printf("Data received on %s", ID);
    } else if (dataString == "ping") {
      packet.print("pong");
    } else if (dataString == "reset server") {
      ESP.restart();
    } else if (dataString == "open stream") {
      Serial.println("Open Stream command detected");
      Serial.print("Attempting to open stream to: ");
      IPAddress _target = packet.remoteIP();
      Serial.print(IPAddressToString(_target) + ":11000");
      Serial.println();
      if (udp2.connected()) {
        udp2.close();
      }
      InitUDP2(_target);
    } else if (dataString == "close stream") {
      Serial.println("Closing UDP2");
      ESP.restart();
    } else {
      packet.print(".");
      Serial.println("No Command Detected for phrase \"" + dataString + "\"");
    }
}

void InitUDP2 (IPAddress target) {
  Serial.print ("Initialising UDP2 Stream on target IP. Target IP: ");
  Serial.print (target);
  Serial.println();

  if(udp2.connect(target, 11000)) {
    Serial.print("UDP2 connected to target: ");
    Serial.printf("%s", IPAddressToString(target).c_str());
    Serial.println();

    udp.onPacket([](AsyncUDPPacket packet) {
        Serial.printf("Packet received, UDP2.\nLength: %i\nData: ", packet.length());
        Serial.write(packet.data(), packet.length());
        Serial.println();

        String dataString = (const char*)packet.data();
        HandleUDPCommands(dataString, packet);
    });
  }
}

void InitUDP () {
  Serial.print("Initialising UDP Stream");
  
  if (udp.listenMulticast(IPAddress(239,3,3,3), 11000)) {
    Serial.println ("UDP Listening on 239.3.3.3:11000");

    udp.onPacket([](AsyncUDPPacket packet) {
        Serial.printf("Packet received, UDP.\nLength: %i\nData: ", packet.length());
        
        String dataString = (const char*)packet.data();
        Serial.println(dataString);

        HandleUDPCommands(dataString, packet);
        Serial.print("|"+dataString+"|");
        Serial.println();
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

#pragma region Endpoints
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

void handleRestart () {
  Serial.println("Restarting");
  server.sendHeader("Location","/");        // Add a header to respond with a new location for the browser to go to the home page again
  server.send(303);     
  delay(100);
  ESP.restart();
}

void handleAudio () {
  Serial.print("Handling audio stream");
  WiFiClient client = server.client();
  String response = "HTTP/1.1 200 OK\r\n";
  response += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
  server.sendContent(response);

  GetAudioStream(client, server);
}
#pragma endregion
const char* loginURL = "https://draconstorage.blob.core.windows.net/esp-dracon/loginData.txt?sp=r&st=2020-08-03T10:43:01Z&se=2050-08-03T18:43:01Z&spr=https&sv=2019-12-12&sr=b&sig=UbIdG3X3v18CdHB0GAuZ%2BvOtL2BghATFLK24PaaNq%2F4%3D";
String httpRequest (const char *host, uint16_t port) {
    WiFiClient client = server.client();
    String total = "";
    if (client.connect(host, port)) {
      Serial.println("connecting...");
      // send the HTTP GET request:
      client.println("GET / HTTP/1.1");
      client.printf("Host: %s\n",host);
      client.println("User-Agent: espDracon");
      client.println("Connection: close");
      client.println();
      delay(100);
      while (client.available()) {
          String dString = client.readString();
          total += dString;
          Serial.println(dString);
      }
    }
    Serial.print(total);
    Serial.println();
    return total;
}

void findSuitableNetwork () {
  WiFi.disconnect();
  int n = WiFi.scanNetworks();
  Serial.printf("Network scan finished, %i results\n", n);
  String finRes = "";
  for (int i = 0; i < n; ++i) {
    String res = WiFi.SSID(i);
    Serial.printf("%i: %s\n", i, res);
    if (res == "Dracon24") {
      finRes == res;
      break;
    } else if (res == "SJA_MODEM_02") {
      finRes = res;
      break;
    } else if (res == "AndroidAP391A") {
      finRes = res;
      break;
    }
  }
  if (finRes == "Dracon24") {
    Serial.println("Found Dracon Home Network");
    ap_ssid = "Dracon24";
    ap_password = "Rarceth1996!";
  } else if (finRes == "SJA_MODEM_02") {
    Serial.println("Found Dracon Work Network");
    ap_ssid = "SJA_MODEM_02";
    ap_password = "12345678";
  } else if (finRes == "AndroidAP391A") {
    Serial.println("Found Dracon Mobile Network");
    ap_ssid = "AndroidAP391A";
    ap_password = "12345678";
  }
/*
  File file = SPIFFS.open("/loginData.txt", FILE_READ);
  if (!file) {
    Serial.println("Failed to open login file");
  } else {
    String fileData = file.readString();
    Serial.printf("File Data: %s", fileData.c_str());
  }*/
}

void PinSetup () {
  pinMode(GPIO_NUM_21, OUTPUT);
  pinMode(GPIO_NUM_22, OUTPUT);
  digitalWrite(GPIO_NUM_21, HIGH);
  digitalWrite(GPIO_NUM_22, LOW);

  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
  pinMode(15, INPUT_PULLUP);
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
    if(!SPIFFS.begin(true)) {
      Serial.print("SPIFFS up failed");
    }
    PinSetup();
    findSuitableNetwork();
    InitSTA();
    InitUDP();

    CamStart();
    micSetup();

    server.on("/", HTTP_GET, handleIndex);
    server.on("/stream", HTTP_GET, handle_jpg_stream);
    server.on("/jpg", HTTP_GET, handle_jpg);
    server.on("/restart", HTTP_GET, handleRestart);
    server.on("/audio", HTTP_GET, handleAudio);
    server.onNotFound(handleNotFound);
    server.begin();

    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

    Serial.println("Sending HTTP Request to");
    Serial.println(loginURL);
    httpRequest(loginURL, 80);
    Serial.print("Setup Finished.");
}

int udp2Counter = 0;
int whiteValue = 0;
void loop () {
    if (digitalRead(15) == 0) {
        Serial.println("Resetting Wifi");
        WiFi.disconnect();
        InitSTA();
    }

    server.handleClient();
    if (udp2.connected()) {
      udp2Counter++;
      if (udp2Counter > 250) {
        udp2Counter = 0;
        udp2.print("dataSend");

        if (whiteValue == 0) {
          whiteValue = 1;
          WriteToLED(false, true, HIGH);
        } else {
          whiteValue = 0;
          WriteToLED(false, true, LOW);
        }
      }
    }
    
    delay(1);
}