#include "ESP8266WiFi.h"
#include "ESPAsyncTCP.h"
#include "ESPAsyncWebServer.h"
#define FASTLED_INTERRUPT_RETRY_COUNT 1
#define FASTLED_ESP8266_RAW_PIN_ORDER
#include "FastLED.h"
#include "WiFiUdp.h"

#define LED_COUNT 450
#define LED_DT 14 // LED pin
#define BUFFER_LEN 1024

// ***********************************************************************************
// Integers and constants
// WiFi data
const char* ssid = "SSID";
const char* password =  "PASS";
unsigned int localPort = 7777;

//LED setup vars
String sliderValue = "50";  //Default LED brightness

//Utility vars
int toled = LED_COUNT;
int fromled = 0;
uint16_t N = 0;
char packetBuffer[BUFFER_LEN];
int r, g, b, ledMode;
int BOTTOM_INDEX = 0;        // Start LED
int TOP_INDEX = int(LED_COUNT / 2);
int EVENODD = LED_COUNT % 2;
struct CRGB leds[LED_COUNT];
int ledsX[LED_COUNT][3];     //-ARRAY FOR COPYING WHATS IN THE LED STRIP CURRENTLY (FOR CELL-AUTOMATA, MARCH, ETC)
int j = 0;
int thisdelay = 20;          //-FX LOOPS DELAY VAR
int thisstep = 10;           //-FX LOOPS DELAY VAR
int thishue = 0;             //-FX LOOPS DELAY VAR
int thissat = 255;           //-FX LOOPS DELAY VAR
int count;

int thisindex = 0;
int thisRED = 0;
int thisGRN = 0;
int thisBLU = 0;

int idex = 0;                //-LED INDEX (0 to LED_COUNT-1
int ihue = 0;                //-HUE (0-255)
int ibright = 0;             //-BRIGHTNESS (0-255)
int isat = 0;                //-SATURATION (0-255)
int bouncedirection = 0;     //-SWITCH FOR COLOR BOUNCE (0-1)
float tcount = 0.0;          //-INC VAR FOR SIN LOOPS
int lcount = 0;              //-ANOTHER COUNTING VAR
String lastState = "OFF";
// BallFX color
byte ballColors[3][3] = {
  {0xff, 0, 0},
  {0xff, 0xff, 0xff},
  {0   , 0   , 0xff},
};
// ***********************************************************************************
// Async Server start
AsyncWebServer server(80);
WiFiUDP port;

// ***********************************************************************************
// Web page code
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>

<head>
  <meta name=\"viewport\" content=\"width=device-width, initial-scale=2\">
  <title>LED control</title>
  <link rel=\"icon\" href=\"data:,\">
</head>

<body>

  <h1 id="LEDSTATE">LED strip</h1> State:
  <hr>
  <span id="textSliderValue">%TEXTSLIDER%</span>
    <form>
      <input type='range' min='0' max="255" step="1" class="slider" value="%SLIDERBR%" id="brt">
    </form>
    <p>
    Choose range of leds
  </p>
    <p>
      From:<input type="number" value=0 onchange="SendValue(this);" oninput="CheckMaxValue(this);" id="startled">
      To:<input type="number" value=300 onchange="SendValue(this);" oninput="CheckMaxValue(this);" id="endled" >
    </p>
  <p><label for="colorpicker">Color Picker:</label>
    <input type="color" id="colorpicker" onchange="SendValue(this);">
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
      <button value="23" onclick=LEDmodeset(this);>Vertical rainbow</button>
      <button value="23" onclick=LEDmodeset(this);>Vertical rainbow</button>
    </p>
    <p>
      <button value="23" onclick=LEDmodeset(this);>Vertical rainbow</button>
      <button value="25" onclick=LEDmodeset(this);>Random color pop</button>
      <button value="37" onclick=LEDmodeset(this);>Rainbow cycle</button>
      <button value="38" onclick=LEDmodeset(this);>Rainbow twinkle</button>
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
    if (i > 300) {
      element.value=300;
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
  return String();

  if (var == "FROMLED") {
   return String(fromled);
  }
  if (var == "TOLED") {
    return String(toled);
  }
}

// ***********************************************************************************

void change_mode(int newmode) {
  thissat = 255;
  switch (newmode) {
    case 0: one_color_all(0, 0, 0); LEDS.show(); break; //---ALL OFF
    case 1: one_color_all(255, 255, 255); LEDS.show(); break; //---ALL ON
    case 2: thisdelay = 35; break;                      //---STRIP RAINBOW FADE
    case 3: thisdelay = 20; thisstep = 10; break;       //---RAINBOW LOOP
    case 4: thisdelay = 20; break;                      //---RANDOM BURST
    case 5: thisdelay = 20; thishue = 0; break;         //---CYLON v1
    case 6: thisdelay = 10; thishue = 0; break;         //---CYLON v2
    case 9: thishue = 160; thissat = 50; break;         //---STRIP FLICKER
    case 13: thisdelay = 100; break;                    //---CELL AUTO - RULE 30 (RED)
    case 14: thisdelay = 40; break;                     //---MARCH RANDOM COLORS
    case 23: thisdelay = 50; thisstep = 15; break;      //---VERITCAL RAINBOW
    case 25: thisdelay = 35; break;                     //---RANDOM COLOR POP
    case 30: thisdelay = 5; break;                      //---NEW RAINBOW LOOP
    case 37: thisdelay = 20; break;                     // rainbowCycle
    case 38: thisdelay = 10; break;                     // rainbowTwinkle
    case 39: thisdelay = 50; break;                     // RunningLights
    case 40: thisdelay = 0; break;                      // Sparkle
    case 41: thisdelay = 20; break;                     // SnowSparkle
    case 42: thisdelay = 30; break;                     // theaterChase
    case 43: thisdelay = 30; break;                     // theaterChaseRainbow
    case 44: thisdelay = 100; break;                    // Strobe

    case 101: one_color_all(255, 0, 0); LEDS.show(); break; //---ALL RED
    case 102: one_color_all(0, 255, 0); LEDS.show(); break; //---ALL GREEN
    case 103: one_color_all(0, 0, 255); LEDS.show(); break; //---ALL BLUE

    case 999: Serial.println("Breaking"); break;
  }
  bouncedirection = 0;
  one_color_all(0, 0, 0);
  ledMode = newmode;
}
// ***********************************************************************************
// WiFi setup
void setup() {
  Serial.begin(115200); // Serial port rate
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }

  Serial.println(WiFi.localIP());
  port.begin(localPort);
  
  LEDS.addLeds<WS2812B, LED_DT, GRB>(leds, LED_COUNT); // LED strip init
  LEDS.setBrightness(sliderValue.toInt());
  change_mode(0);
 
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
    Serial.print("Brightness is: ");
    Serial.println(inputMessage);
    request->send(200, "text/plain", "OK");
  });
  server.on("/brightness_get", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/plain", sliderValue.c_str());
  });

// Custom color  
  server.on("/customcolor", HTTP_GET, [](AsyncWebServerRequest *request) {
     String inputMessage1;
     String inputMessage2;
     String inputMessage3;
     if (request->hasParam("max") && request->hasParam("max") && request->hasParam("hex")) {
      inputMessage1 = request->getParam("max")->value();
      inputMessage2 = request->getParam("min")->value();
      inputMessage3 = request->getParam("hex")->value();
      toled = inputMessage1.toInt();
      fromled = inputMessage2.toInt();
      char *str = (char*) inputMessage3.c_str();
      sscanf(str, "%02x%02x%02x", &r, &g, &b);
      one_color_range(r, g, b, fromled, toled);
      LEDS.show();
      Serial.println("range of leds");
    }else if (request->hasParam("hex")) {
      inputMessage2 = request->getParam("hex")->value();
      char *str = (char*) inputMessage2.c_str();
      sscanf(str, "%02x%02x%02x", &r, &g, &b);
      Serial.println("custom color");
      one_color_all(r,g,b);
      ledMode = 999;
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
      Serial.print("Led mode is: ");
      Serial.println(ledMode);

  }
    request->send(200, "text/plain", "OK");
  });

// ***********************************************************************************
// Start server
  server.begin();
  
}


void loop() {

// ***********************************************************************************
// LED mode switch 

  switch (ledMode) {
      case 999: break;                           // пазуа
      case  2: rainbow_fade(); break;            // плавная смена цветов всей ленты
      case  3: rainbow_loop(); break;            // крутящаяся радуга
      case  4: random_burst(); break;            // случайная смена цветов
      case  5: color_bounce(); break;            // бегающий светодиод
      case  6: color_bounceFADE(); break;        // бегающий паровозик светодиодов
      case  9: flicker(); break;                 // случайный стробоскоп
      case 13: rule30(); break;                  // безумие красных светодиодов
      case 14: random_march(); break;            // безумие случайных цветов
      case 17: color_loop_vardelay(); break;     // красный светодиод бегает по кругу
      case 18: white_temps(); break;             // бело синий градиент (?)
      case 23: rainbow_vertical(); break;        // радуга в вертикаьной плоскости (кольцо)
      case 25: random_color_pop(); break;        // безумие случайных вспышек
      case 30: new_rainbow_loop(); break;        // крутая плавная вращающаяся радуга
      case 37: rainbowCycle(thisdelay); break;                                        // очень плавная вращающаяся радуга
      case 38: TwinkleRandom(20, thisdelay, 1); break;                                // случайные разноцветные включения (1 - танцуют все, 0 - случайный 1 диод)
      case 39: RunningLights(0xff, 0xff, 0x00, thisdelay); break;                     // бегущие огни
      case 40: Sparkle(0xff, 0xff, 0xff, thisdelay); break;                           // случайные вспышки белого цвета
      case 41: SnowSparkle(0x10, 0x10, 0x10, thisdelay, random(100, 1000)); break;    // случайные вспышки белого цвета на белом фоне
      case 42: theaterChase(0xff, 0, 0, thisdelay); break;                            // бегущие каждые 3 (ЧИСЛО СВЕТОДИОДОВ ДОЛЖНО БЫТЬ НЕЧЁТНОЕ)
      case 43: theaterChaseRainbow(thisdelay); break;                                 // бегущие каждые 3 радуга (ЧИСЛО СВЕТОДИОДОВ ДОЛЖНО БЫТЬ КРАТНО 3)
      case 44: Strobe(0xff, 0xff, 0xff, 10, thisdelay, 1000); break;                  // стробоскоп
      case 45: BouncingBalls(0xff, 0, 0, 3); break;                                   // прыгающие мячики
      case 46: BouncingColoredBalls(3, ballColors); break;                            // прыгающие мячики цветные
      case 900: ReactiveLED(); break;
  
      case 888: demo_modeA(); break;             // длинное демо
      case 889: demo_modeB(); break;             // короткое демо
    }
// ***********************************************************************************
//WiFi reconnection
    
    if (WiFi.status() == 6) {
      while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
      Serial.println("Reconnecting to WiFi..");
      }
      Serial.println(WiFi.localIP());
    }

 }
