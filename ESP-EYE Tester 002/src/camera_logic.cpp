#include "OV2640.h"
#include "OV2640Streamer.h"
#include "WebServer.h"

OV2640 cam;

void GetJPGStream(WiFiClient &client, WebServer &server) {
    String response = "";
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

void GetJPG(WiFiClient &client, WebServer &server) {
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

void CamStart () {
    cam.init(espeyecam_config);
}