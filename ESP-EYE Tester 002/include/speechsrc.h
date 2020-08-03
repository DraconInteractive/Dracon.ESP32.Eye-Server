#include "Arduino.h"

void micSetup();
void micLoop();
int16_t* micGet ();
void GetAudioStream(WiFiClient &client, WebServer &server);