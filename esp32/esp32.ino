#include "WiFi.h"
#include "IRrecv.h"
#include "IRutils.h"
#include "ArduinoOTA.h"
#include "SPIFFS.h"
#include "AsyncTCP.h"
#include "WiFiUdp.h"
#include "ESPAsyncWebServer.h"
#define FASTLED_INTERRUPT_RETRY_COUNT 1
#include "FastLED.h"



#define LED_COUNT 450
#define LED_DT 27 // LED pin
#define BUFFER_LEN 1024

#define SECONDS_PER_PALETTE 60


// ***********************************************************************************
// Web page code
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>

<head>
  <meta name="viewport" content="width=device-width, initial-scale=2">
  <title>LED control</title>
  <link rel="icon" href="data:,">
</head>

<body>

  <h1 id="LEDSTATE">LED strip</h1> State: %LEDSTATE%
  <hr>
  <span id="textSliderValue">%TEXTSLIDER%</span>
    <form>
      <input type="range" min='0' max="255" step="1" class="slider" value=%SLIDERBR% id="brt">
    </form>
    <p>
    Choose range of leds
  </p>
    <p>
      From:<input type="number" value=%FROMLED% onchange="SendValue(this);" oninput="CheckMinValue(this);" id="startled">
      To:<input type="number" value=%TOLED% onchange="SendValue(this);" oninput="CheckMaxValue(this);" id="endled" >
    </p>
  <p><label for="colorpicker">Color Picker:</label>
    <input type="color" id="colorpicker" oninput="SendValue(this);" value=%CURRENTCOLOR%>
  </p>
</hr>
  <div>
    <p>
       <button value="2" onclick=LEDmodeset(this);>Rainbow Fade</button>
       <button value="3" onclick=LEDmodeset(this);>Rainbow Loop</button>
       <button value="4" onclick=LEDmodeset(this);>Random Burst</button>
       <button value="14" onclick=LEDmodeset(this);>Random March</button>
   </p>
    <p>
      <button value="5" onclick=LEDmodeset(this);>Color Bounce</button>
      <button value="6" onclick=LEDmodeset(this);>Color Bounce(with fade)</button>
      <button value="900" onclick=LEDmodeset(this);>Reaction to Music</button>
      <button value="9" onclick=LEDmodeset(this);>Flicker</button>
    </p>
    <p>
      <button value="30" onclick=LEDmodeset(this);>New rainbow loop</button>
      <button value="13" onclick=LEDmodeset(this);>Cell Auto</button>
    </p>
    <p>
      <button value="23" onclick=LEDmodeset(this);>Vertical rainbow</button>
      <button value="25" onclick=LEDmodeset(this);>Random color pop</button>
      <button value="37" onclick=LEDmodeset(this);>Rainbow cycle</button>
      
    </p>
    <p>
      <button value="39" onclick=LEDmodeset(this);>Running lights</button>
      <button value="40" onclick=LEDmodeset(this);>Sparkle</button>
      <button value="41" onclick=LEDmodeset(this);>Snow Sparkle</button>
      <button value="42" onclick=LEDmodeset(this);>Theater chase</button>
    </p>
    <p>
      <button value="43" onclick=LEDmodeset(this);>Theather chase RGB</button>
      <button value="44" onclick=LEDmodeset(this);>Strobe</button>
      <button value="45" onclick=LEDmodeset(this);>Gradient Lights</button>
      <button value="46" onclick=LEDmodeset(this);>Holiday Lights</button>
    </p>
    <p>
     <button value="999" onclick=LEDmodeset(this);>OFF</button>
    </p>
  </div>
</body>

<script>

  var slider = document.getElementById("brt");
  var xhr = new XMLHttpRequest();
  document.getElementById("textSliderValue").onload = function() {
  slider.innerHTML = this.responseText;
  xhr.open("GET", "/brightness_get", true);
  xhr.send();
 }
 slider.onchange = function updateSliderPWM(element) {
 var sliderValue = document.getElementById("brt").value;
  document.getElementById("textSliderValue").innerHTML = sliderValue;
  xhr.open("GET", "/brightness?value="+sliderValue, true);
  xhr.send();
  }

  function LEDmodeset(element) {
    var mode = element.value;
    xhr.open("GET", "/setmode?mode="+mode, true);
    xhr.send();
  }
  function CheckMinValue(element) {
    var i = element.value;
    if (i < 0) {
      element.value=0;
    }
  }
  function CheckMaxValue(element) {
    var i = element.value;
    if (i > 450) {
      element.value=450;
    }
  }
  function SendValue(element) {
    var color = document.getElementById('colorpicker').value;
    color = color.replace("#", "");
    if (element.id == "colorpicker") {
      xhr.open("GET", "/customcolor?hex="+color, true);
      xhr.send();
    } else {
      var max = document.getElementById("endled").value;
      var min = document.getElementById("startled").value;
      xhr.open("GET", "/customcolor?max="+max+"&min="+min+"&hex="+color, true);
      xhr.send();
      }
  }
</script>
</html>
)rawliteral";



// ***********************************************************************************
// Integers and constants
// WiFi data
const char* ssid = "HOME";
const char* password =  "40ue6307";
unsigned int localPort = 7777;

//LED setup vars
String sliderValue = "50";  //Default LED brightness



//Utility vars
String inputMessage1;
String inputMessage2;
String inputMessage3;
char *str;
int toled = LED_COUNT;
int fromled = 0;
String color = "#000000";
String HEXsign = "#";
uint16_t N = 0;
char packetBuffer[BUFFER_LEN];
int r, g, b, ledMode, count, LastBright;
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

IRrecv irrecv(33);
decode_results results;


  
// ***********************************************************************************
// Async Server start
AsyncWebServer server(80);
WiFiUDP port;



// ***********************************************************************************
// Sliders and buttons proccesor
String processor(const String& var){
  if (var == "SLIDERBR"){
    return sliderValue;
  }
  
  if (var == "TEXTSLIDER") {
    return sliderValue;
  }
  
  if (var == "LEDSTATE") {
    return lastState;
  }

  if (var == "TOLED") {
    String TOLED = String(toled);
    return TOLED;
  }

  if (var == "FROMLED") {
    String FROMLED = String(fromled);
    return FROMLED;
  }

  if (var == "CURRENTCOLOR") {
    return color;
  }
}


// ***********************************************************************************
// WiFi setup
void setup() {

 SPIFFS.begin();
  
  Serial.begin(115200); // Serial port rate
  btStop();
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
//    Serial.println("Connecting to WiFi..");
  }

//  Serial.println(WiFi.localIP());
  port.begin(localPort);

  FastLED.setMaxPowerInVoltsAndMilliamps(5,15000);
  LEDS.addLeds<WS2812B, LED_DT, GRB>(leds, LED_COUNT); // LED strip init
  LEDS.setBrightness(sliderValue.toInt());
  memset(leds, 0,  LED_COUNT * sizeof(struct CRGB));
  change_mode(0);

// ***********************************************************************************
// OTA update handler
  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
//      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
//      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
//      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
    });

  ArduinoOTA.begin();
  
 
// ***********************************************************************************
// Server requests

// Web Page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });

// Change brightness 
  server.on("/brightness", HTTP_GET, [](AsyncWebServerRequest *request) {
     String inputMessage;
     if (request->hasParam("value")) {
      inputMessage = request->getParam("value")->value();
      sliderValue = inputMessage;
      LEDS.setBrightness(sliderValue.toInt());
      LEDS.show();
    }
    else {
      inputMessage = "No message sent";
    }
//    Serial.print("Brightness is: ");
//    Serial.println(inputMessage);
    request->send(200, "text/plain", "OK");
  });
  server.on("/brightness_get", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/plain", sliderValue.c_str());
  });

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
//      Serial.println("range of leds");
    }else if (request->hasParam("hex")) {
      inputMessage2 = request->getParam("hex")->value();
      color = HEXsign + inputMessage2;
//      Serial.println(color);
      str = (char*) inputMessage2.c_str();
      sscanf(str, "%02x%02x%02x", &r, &g, &b);
//      Serial.println("custom color");
      if (ledMode != 255) ledMode = 255;
      one_color_all(r,g,b);
      
      LEDS.show();
      } 
    request->send(200, "text/plain", "OK");
  });

// LED strip mode
  server.on("/setmode", HTTP_GET, [](AsyncWebServerRequest *request) {
        String LEDmodevalue;
    if (request->hasParam("mode")) {
      LEDmodevalue = request->getParam("mode")->value();
      
      ledMode = LEDmodevalue.toInt();
      change_mode(ledMode);
//      Serial.print("Led mode is: ");
//      Serial.println(ledMode);

  }
    request->send(200, "text/plain", "OK");
  });

// ***********************************************************************************
// Start server
  server.begin();

  irrecv.enableIRIn();
    
}


void loop() {
  // ***********************************************************************************
    // LED mode switch 

      switch (ledMode) {
        case 999: lastState="OFF"; one_color_all(0,0,0); LEDS.show();  break;// пазуа
        case  255: lastState="Custom color"; break;
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
  if (WiFi.status() == 6) {
      while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
      Serial.println("Reconnecting to WiFi..");
      }
      Serial.println(WiFi.localIP());
    }
    ArduinoOTA.handle();

    // ***********************************************************************************
// IR handler
   if (irrecv.decode(&results)) {
    Serial.print("ass");
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

  
 }

 
