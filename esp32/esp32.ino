#include "WiFi.h"
#include "IRrecv.h"
#include "IRutils.h"
#include "ArduinoOTA.h"
#include "AsyncTCP.h"
#include "WiFiUdp.h"
#include "SPIFFS.h"
#include "Arduino_JSON.h"
#include "ESPAsyncWebServer.h"
#define FASTLED_INTERRUPT_RETRY_COUNT 1
#include "FastLED.h"



#define LED_COUNT 450
#define LED_DT 27 // LED pin
#define BUFFER_LEN 1024

#define SECONDS_PER_PALETTE 60




// ***********************************************************************************
// Integers and constants
// WiFi data
const char* ssid = "SSID";
const char* password =  "PASSWORD";
unsigned int localPort = 7777;

//LED setup vars
String sliderValue = "50";  //Default LED brightness



//Utility vars
String inputMessage1;
String inputMessage2;
String inputMessage3;
char *str;
String message = "";
int toled = LED_COUNT;
int fromled = 0;
String color = "#000000";
String HEXsign = "#";
uint16_t N = 0;
char packetBuffer[BUFFER_LEN];
int r, g, b, ledMode, count;
int BOTTOM_INDEX = 0;        // Start LED
int TOP_INDEX = int(LED_COUNT / 2);
int EVENODD = LED_COUNT % 2;
struct CRGB leds[LED_COUNT];
int ledsX[LED_COUNT][3];     //-ARRAY FOR COPYING WHATS IN THE LED STRIP CURRENTLY (FOR CELL-AUTOMATA, MARCH, ETC)

byte num_modes[] = {2,3,4,5,6,9,13,14,23,25,30,37,39,40,41,42,43,44,45,46};
volatile int modeCounter;
byte num_modes_count = sizeof(num_modes);

int thisdelay = 20;          //-FX LOOPS DELAY VAR
int thisstep = 10;           //-FX LOOPS DELAY VAR
int thishue = 0;             //-FX LOOPS DELAY VAR
int thissat = 255;           //-FX LOOPS DELAY VAR

int thisindex = 0;
int thisRED = 0;
int thisGRN = 0;
int thisBLU = 0;

int j = 0;
int idex = 0;                //-LED INDEX (0 to LED_COUNT-1
int ihue = 0;                //-HUE (0-255)
int ibright = 0;             //-BRIGHTNESS (0-255)
int isat = 0;                //-SATURATION (0-255)
int bouncedirection = 0;     //-SWITCH FOR COLOR BOUNCE (0-1)
float tcount = 0.0;          //-INC VAR FOR SIN LOOPS
int lcount = 0;              //-ANOTHER COUNTING VAR
String lastState = "OFF";

//Holiday Lights utility
 extern const TProgmemRGBGradientPalettePtr gGradientPalettes[];
 extern const uint8_t gGradientPaletteCount;

 uint8_t gCurrentPaletteNumber = 0;

 CRGBPalette16 gCurrentPalette( CRGB::Black);
 CRGBPalette16 gTargetPalette( gGradientPalettes[0] );


JSONVar elementValues;


IRrecv irrecv(33);
decode_results results;

  
// ***********************************************************************************
// Network init
AsyncWebServer server(80);
WiFiUDP port;
AsyncWebSocket ws("/ws");


String getElementValues(){
  elementValues["brt"] = String(sliderValue);
  elementValues["colorpicker"] = color;

  String jsonString = JSON.stringify(elementValues);
  return jsonString;
}


// ***********************************************************************************
// WebSocket functions
void notifyClients(String WS_message) {
  ws.textAll(WS_message);
}


void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    message = (char*)data;
    if (message.indexOf("brt") >= 0) {
      sliderValue = message.substring(3);
      Serial.println(sliderValue);
      LEDS.setBrightness(sliderValue.toInt());
      LEDS.show();
      notifyClients(getElementValues());
    }    
    if (message.indexOf("colorpicker") >= 0) {
      inputMessage1 = message.substring(12);
      color = HEXsign + inputMessage1;
      str = (char*) inputMessage1.c_str();
      sscanf(str, "%02x%02x%02x", &r, &g, &b);
      one_color_all(r,g,b);
      if (ledMode !=255 ) ledMode = 255;
      LEDS.show();
      notifyClients(getElementValues());
    }
    if (strcmp((char*)data, "getValues") == 0) {
      notifyClients(getElementValues());
    }
  }
}


void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
 void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}


IPAddress local_IP(192, 168, 1, 184);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 0, 0);


// ***********************************************************************************
// WiFi setup
void setup() {
  Serial.begin(115200); // Serial port rate
  if(!SPIFFS.begin()){
    Serial.println("SPIFFS Mount Failed");
    return;
  }
  btStop();
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
  }

  Serial.println(WiFi.localIP());
  port.begin(localPort);

  FastLED.setMaxPowerInVoltsAndMilliamps(5,15000);
  LEDS.addLeds<WS2812B, LED_DT, GRB>(leds, LED_COUNT); // LED strip init
  LEDS.setBrightness(sliderValue.toInt());
  memset(leds, 0,  LED_COUNT * sizeof(struct CRGB));
  change_mode(0);


// ***********************************************************************************
// OTA update handler
  ArduinoOTA.begin();
  

// ***********************************************************************************
// Server requests
  ws.onEvent(onEvent);
  server.addHandler(&ws);

// Web Page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html", String(), false);
  });

  server.serveStatic("/", SPIFFS, "/");

// Custom color  
  server.on("/customcolor", HTTP_GET, [](AsyncWebServerRequest *request) {
    
     if (request->hasParam("min") && request->hasParam("max")) {
      inputMessage1 = request->getParam("max")->value();
      inputMessage2 = request->getParam("min")->value();
      inputMessage3 = request->getParam("hex")->value();
      toled = inputMessage1.toInt();
      fromled = inputMessage2.toInt();
      char *str = (char*) inputMessage3.c_str();
      sscanf(str, "%02x%02x%02x", &r, &g, &b);
      one_color_range(r, g, b, fromled, toled);
      LEDS.show();
    } 
    request->send(200);
  });

// LED strip mode
  server.on("/setmode", HTTP_GET, [](AsyncWebServerRequest *request) {
        String LEDmodevalue;
    if (request->hasParam("mode")) {
      LEDmodevalue = request->getParam("mode")->value();
      
      ledMode = LEDmodevalue.toInt();
      change_mode(ledMode);
  }
    request->send(200);
  });

  server.begin();

  irrecv.enableIRIn();
      
} //Setup end


void loop() {
  // ***********************************************************************************
  // LED mode switch 

      switch (ledMode) {
        case 999: lastState="OFF"; color = "#000000"; break;
        case 255: lastState="Custom color"; break;// пазуа
        case  2: rainbow_fade(); lastState="Rainbow Fade"; break;            // плавная смена цветов всей ленты
        case  3: rainbow_loop(); lastState="Rainbow loop"; break;            // крутящаяся радуга
        case  4: random_burst(); lastState="Random brust"; break;            // случайная смена цветов
        case  5: color_bounce(); lastState="Color bounce"; break;            // бегающий светодиод
        case  6: color_bounceFADE(); lastState="Color bounce(with fade)"; break;        // бегающий паровозик светодиодов
        case  9: flicker(); lastState="Flicker"; break;                 // случайный стробоскопs
        case 13: rule30(); lastState="Cell auto"; break;                  // безумие красных светодиодов
        case 14: random_march(); lastState="Random march"; break;            // безумие случайных цветов
        case 17: color_loop_vardelay(); lastState="Red light"; break;     // красный светодиод бегает по кругу
        case 23: rainbow_vertical(); lastState="Vertical rainbow"; break;        // радуга в вертикаьной плоскости (кольцо)
        case 25: random_color_pop(); lastState="Random color pop"; break;        // безумие случайных вспышек
        case 30: new_rainbow_loop(); lastState="Rainbow loop NEW"; break;        // крутая плавная вращающаяся радуга
        case 37: rainbowCycle(thisdelay); lastState="Rainbow cycle"; break;                                        // очень плавная вращающаяся радуга
        case 39: RunningLights(0xff, 0xff, 0x00, thisdelay); lastState="Running lights"; break;                     // бегущие огни
        case 40: Sparkle(0xff, 0xff, 0xff, thisdelay); lastState="Sparkle"; break;                           // случайные вспышки белого цвета
        case 41: SnowSparkle(0x10, 0x10, 0x10, thisdelay, random(100, 1000)); lastState="Snow sparkle"; break;    // случайные вспышки белого цвета на белом фоне
        case 42: theaterChase(0xff, 0, 0, thisdelay); lastState="Theater chase"; break;                            // бегущие каждые 3 (ЧИСЛО СВЕТОДИОДОВ ДОЛЖНО БЫТЬ НЕЧЁТНОЕ)
        case 43: theaterChaseRainbow(thisdelay); lastState="Theater chase RGB"; break;                                 // бегущие каждые 3 радуга (ЧИСЛО СВЕТОДИОДОВ ДОЛЖНО БЫТЬ КРАТНО 3)
        case 44: Strobe(0xff, 0xff, 0xff, 10, thisdelay, 1000); lastState="Strobe"; break;                  // стробоскоп
        case 45: GradientLights(); lastState="Holiday lights"; break;
        case 46: HolidayLights2(); lastState="Holiday lights 2"; break;
        case 900: ReactiveLED(); lastState="Reaction to music"; break;
  
    }
    
  ws.cleanupClients();
  
  if (WiFi.status() == WL_DISCONNECTED) {
      while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
      Serial.println("Reconnecting to WiFi..");
      }
    }
    ArduinoOTA.handle();

// ***********************************************************************************
// IR handler
   if (irrecv.decode(&results)) {
    switch(results.value){
      case 0xFFE01F: break;    //On
      case 0xFF609F: change_mode(999); break;    //Off
      case 0xFF00FF: {int bright = sliderValue.toInt(); bright+=10; if(bright > 255){bright = 255;} sliderValue=String(bright); FastLED.setBrightness(bright); FastLED.show(); break;}    //Dim up
      case 0xFF40BF: {int bright = sliderValue.toInt(); bright-=10; if(bright < 0){bright = 0;} sliderValue=String(bright); FastLED.setBrightness(bright); FastLED.show();   break;}    //Dim down
      case 0xFF10EF: change_mode(255); one_color_all(255,0,0); FastLED.show();     break;    //Red
      case 0xFF906F: change_mode(255); one_color_all(0,255,0); FastLED.show();     break;    //Green
      case 0xFF50AF: change_mode(255); one_color_all(0,0,255); FastLED.show();     break;    //Blue
      case 0xFFC03F: change_mode(255); one_color_all(255,255,255); FastLED.show(); break;    //White
      case 0xFF30CF: change_mode(255); one_color_all(255,69,0); FastLED.show(); break;    //Orange-red
      case 0xFFB04F: change_mode(255); one_color_all(144,238,144); FastLED.show(); break;    //Light-green
      case 0xFF708F: change_mode(255); one_color_all(173,216,230); FastLED.show(); break;    //Light-blue
      case 0xFF08F7: change_mode(255); one_color_all(255,165,0); FastLED.show(); break;    //Orange
      case 0xFF8877: change_mode(255); one_color_all(0,206,209); FastLED.show(); break;    //Cyan
      case 0xFF48B7: change_mode(255); one_color_all(82,0,82); FastLED.show(); break;    //Dark-purple
      case 0xFF28D7: change_mode(255); one_color_all(255,165,0); FastLED.show(); break;    //Yellow-orange
      case 0xFFA857: change_mode(255); one_color_all(32,178,170); FastLED.show(); break;    //Greenish-blue
      case 0xFF6897: change_mode(255); one_color_all(128,0,128); FastLED.show(); break;    //Purple
      case 0xFF18E7: change_mode(255); one_color_all(255,255,0); FastLED.show(); break;    //Yellow
      case 0xFF9867: change_mode(255); one_color_all(0,139,139); FastLED.show(); break;    //Dark-greenish-blue
      case 0xFF58A7: change_mode(255); one_color_all(255,20,147); FastLED.show(); break;    //Pink
      case 0xFFF00F: change_mode(45); break;    //Mode: Flashing lights
      case 0xFFC837: change_mode(44); break;    //Mode: Strobe
      case 0xFFE817: change_mode(2); break;    //Mode: Fade
      case 0xFFD827: CycleModes(); break;    //Cycle through modes
    }
    irrecv.resume();
  }
   
 } //Loop end
