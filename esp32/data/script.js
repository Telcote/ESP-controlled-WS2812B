
// document.getElementById("brt").onchange = function UpdateValueSlider() {
// document.getElementById("textSliderValue").innerHTML = document.getElementById("brt").value;
// sendElementValue(document.getElementById("brt"));
// }

var xhr = new XMLHttpRequest();

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
    element.value=300;
  }
}

function compareValue(min, max) {
  var i = min.value;
  var z = max.value;
  if (i > z) {

  } else if (z < i) {

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

var gateway = `ws://${window.location.hostname}/ws`;
var websocket;
window.addEventListener('load', onload);

function onload(event) {
    initWebSocket();
}

function getValues(){
    websocket.send("getValues");
}

function initWebSocket() {
    console.log('Trying to open a WebSocket connection…');
    websocket = new WebSocket(gateway);
    websocket.onopen = onOpen;
    websocket.onclose = onClose;
    websocket.onmessage = onMessage;
}

function onOpen(event) {
    console.log('Connection opened');
    getValues();
}

function onClose(event) {
    console.log('Connection closed');
    setTimeout(initWebSocket, 2000);
}

function sendElementValue(element) {
    var elementValue = document.getElementById(element.id).value;
    console.log(elementValue);
    console.log(element.id);
    websocket.send(element.id+elementValue.toString());
}

function onMessage(event) {
    console.log(event.data);
    var myObj = JSON.parse(event.data);
    var keys = Object.keys(myObj);
    for (var i = 0; i < keys.length; i++){
        var key = keys[i];
        document.getElementById(key).value = myObj[key];
        document.getElementById(key+"Value").innerHTML = myObj[key];
    }
}
