/* 
Feinstaubmessgerät

Verwendete Komponenten:
• Nova Fitness PM Sensor SDS011
• NodeMCU V2 Amica Modul 
• DHT22 AM2302 Temperatursensor und Luftfeuchtigkeitssensor
• HD44780 1602 LCD Modul 

Steffen Meeuw, Februar 2020
*/


//Load required libraries.
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include "DHT.h"
#include "SdsDustSensor.h"
#include <LiquidCrystal_I2C.h> 
#include <PubSubClient.h>


// ThingSpeak Settings.
const int channelID = 123456789; // channelID for your ThingSpeak Channel
String writeAPIKey = "your-write-API-Key"; // write API key for your ThingSpeak Channel
const char* serverThingspeak = "api.thingspeak.com";

// Wifi Settings.
// Recommended: Create connection with Hotspot function of your smartphone.
const char* ssid     = "ssid-of-hotspot"; 
const char* password = "password-of-hotspot";

// DHT22 Config.
#define DHTTYPE DHT22   
#define DHTPIN 13 
DHT dht(DHTPIN, DHTTYPE);

// SDS011 Config.
int rxPin = 0; 
int txPin = 2; 
SdsDustSensor sds(rxPin, txPin);

// LCD1602 Config.
LiquidCrystal_I2C lcd(0x27, 16, 2); 

// Set port of webserver.
ESP8266WebServer server(80);
WiFiClient client;

// Create var to safe http-request.
String header;


void setup() {
  Serial.begin(9600);

  lcd.init();   
  lcd.backlight(); 
  
  lcd.setCursor(0, 0);
  lcd.print("Feinstaub");
  lcd.setCursor(0, 1);
  lcd.print("Messgeraet");
  delay(5000);
  lcd.clear();

   // Start sensors.
  lcd.setCursor(0, 0);
  lcd.print("System startet ...");
  lcd.setCursor(0, 1);
  lcd.print("Starte Sensoren");
  delay(3000);
   sds.begin();
   dht.begin();
   
  getPM10();
  getPM25();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("System startet ...");
  lcd.setCursor(0, 1);
  lcd.print("Verbinde Hotspot");
  
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  server.begin();
  server.on("/", showMeasureResults);
  server.on("/refreshValues", refreshValues);

  Serial.println(sds.queryFirmwareVersion().toString()); // prints firmware version of sds.
  Serial.println(sds.setQueryReportingMode().toString()); // set reporting mode for longer lifetime.


  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("System bereit");
  delay(2000);
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("IP-Adresse:");
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP());
  
  sds.sleep();
}

void loop(){
  server.handleClient();  // wait for clients.
  // Optional: measure every 5mins.
  //refreshValues();
  //delay(300000);
  
}

String split(String s, char parser, int index) {
  String rs="";
  int parserIndex = index;
  int parserCnt=0;
  int rFromIndex=0, rToIndex=-1;
  while (index >= parserCnt) {
    rFromIndex = rToIndex+1;
    rToIndex = s.indexOf(parser,rFromIndex);
    if (index == parserCnt) {
      if (rToIndex == 0 || rToIndex == -1) return "";
      return s.substring(rFromIndex,rToIndex);
    } else parserCnt++;
  }
  return rs;
}


void refreshValues(){
  lcd.setCursor(0, 0);
  lcd.clear();
  lcd.print("Messung laeuft...");
  lcd.setCursor(0, 1);
  lcd.print("Bitte warten"); 
  float temp = getTemperature();
  float hum = getHumidity();
  float pm10 = getPM10();
  float pm25 = getPM25();
  if(pm10 <= 0 || pm25 <= 0)
  {
    pm10 = getPM10();
    pm25 = getPM25();
  }
  lcd.clear();
  lcd.setCursor(0, 0);
  String tempLCD = split(String(temp), '.' ,0);
  lcd.print("PM25:" + String(pm25) + "  " + tempLCD + "C"); 
  //lcd.print(String(temp) + "C" + " " + String(hum) + "%"); 
  lcd.setCursor(0, 1);
    //lcd.print("PM25:" + String(pm25) + " PM10:" + String(pm10)); 
    String humLCD = split(String(hum), '.' ,0);
     lcd.print("PM10:" + String(pm10) + "  " + humLCD + "%");
  SendToDatabase(pm25, pm10, temp, hum);

  // Display measure results on website.
  String json = "{ \"temperature\" : \"" + String(temp) + "\", \"humidity\" : \"" +String(hum) + "\", \"pm25\" : \"" + pm25 + "\", \"pm10\" : \"" + pm10 + "\"}";
  Serial.print(json);
  server.send(200, "application/json", json);
}


float getPM25(){
  Serial.println(sds.setActiveReportingMode().toString()); // ensures sensor is in 'active' reporting mode
   PmResult pm = sds.readPm();
  if (pm.isOk()) {
    Serial.print("PM2.5 = ");
    Serial.print(pm.pm25);
    Serial.print(", PM10 = ");
    Serial.println(pm.pm10);

    WorkingStateResult state = sds.sleep();

    return pm.pm25;

}
 WorkingStateResult state = sds.sleep(); // go back to sleeping mode.
}

float getPM10(){
  sds.wakeup();
  delay(15000); // working 15 seconds
   PmResult pm = sds.readPm();
  if (pm.isOk()) {
    Serial.print("PM2.5 = ");
    Serial.print(pm.pm25);
    Serial.print(", PM10 = ");
    Serial.println(pm.pm10);

    return pm.pm10;

}
  }

float getTemperature(){
  // Read temperature as Celsius (the default)
  // value for fail is:
  float t = 999.9;
  
  t = dht.readTemperature();

  if (isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
  }
  return t;
}

float getHumidity(){
  // Rread humidity
  float h = 999.9;
  h = dht.readHumidity();
  if (isnan(h)) {
    Serial.println("Failed to read from DHT sensor!");
    }
  return h;
  }


void showMeasureResults(){
String html ="<!DOCTYPE html> <html lang='de'> <head> <title>Feinstaubmessgerät</title> <meta charset='utf-8'> <meta name='viewport' content='width=device-width' initial-scale='1'> <style> #sehrgut {background-color:aqua;} #gut {background-color: aquamarine;} #mittel {background-color: yellow;} #schlecht {background-color: lightcoral;} #sehrschlecht {background-color: red;} #overlay {display: none;} .lds-default { display: inline-block; position: relative; width: 64px; height: 64px; } .lds-default div { position: absolute; width: 5px; height: 5px; background: #fff; border-radius: 50%; animation: lds-default 1.2s linear infinite; } .lds-default div:nth-child(1) { animation-delay: 0s; top: 29px; left: 53px; } .lds-default div:nth-child(2) { animation-delay: -0.1s; top: 18px; left: 50px; } .lds-default div:nth-child(3) { animation-delay: -0.2s; top: 9px; left: 41px; } .lds-default div:nth-child(4) { animation-delay: -0.3s; top: 6px; left: 29px; } .lds-default div:nth-child(5) { animation-delay: -0.4s; top: 9px; left: 18px; } .lds-default div:nth-child(6) { animation-delay: -0.5s; top: 18px; left: 9px; } .lds-default div:nth-child(7) { animation-delay: -0.6s; top: 29px; left: 6px; } .lds-default div:nth-child(8) { animation-delay: -0.7s; top: 41px; left: 9px; } .lds-default div:nth-child(9) { animation-delay: -0.8s; top: 50px; left: 18px; } .lds-default div:nth-child(10) { animation-delay: -0.9s; top: 53px; left: 29px; } .lds-default div:nth-child(11) { animation-delay: -1s; top: 50px; left: 41px; } .lds-default div:nth-child(12) { animation-delay: -1.1s; top: 41px; left: 50px; } keyframes lds-default { 0%, 20%, 80%, 100% { transform: scale(1); } 50% { transform: scale(1.5); } } </style> </head> <body> <img src='https://uol.de/fileadmin/_processed/3/f/csm_svs-logo-small_afa3bea387.png' height='50' width='100' alt='Logo' border='2' align='center'> <header> <h3>Feinstaubmessgerät</h3> </header> <section> <article> <hr> <h4>Aktuelle Messwerte</h4> <p id='pm25value'>PM2.5: <span id='pm25'></span></p> <p id='pm10value'>PM10: <span id='pm10'></span></p> <p>Temperatur: <span id='temperature'></span> °C</p> <p>Luftfeuchtigkeit: <span id='humidity'></span> %</p> <div id='message'></div> <script> function refresh() { console.log('Entering refresh...'); document.getElementById('overlay').style.display = 'flex'; fetch('/refreshValues') .then(response => response.json()) .then(json => { var spanTemp = document.getElementById('temperature'); spanTemp.innerText = json['temperature']; var spanHum = document.getElementById('humidity'); spanHum.innerText = json['humidity']; var span25 = document.getElementById('pm25'); span25.innerText = json['pm25']; var span10 = document.getElementById('pm10'); span10.innerText = json['pm10']; document.getElementById('overlay').style.display = 'none'; colorText(); }); } function successHandler(location) { var message = document.getElementById(message); html = []; html.push(\"<h4>Ihre Position</h4>); html.push(<p>Longitude: , location.coords.longitude, </p>); html.push(<p>Latitude: , location.coords.latitude, </p>); html.push(<p>Accuracy: , location.coords.accuracy, meters</p>\"); message.innerHTML = html.join(); } function errorHandler(error) { tryAPIGeolocation(); alert('Attempt to get location failed: ' + error.message); } function tryAPIGeolocation() {} function colorText() { var element = document.getElementById('pm25'); var value = parseFloat(element.innerHTML); switch (true) { case (value > 0 && value <= 20): document.getElementById('pm25value').style.backgroundColor = 'aqua'; break; case (value > 20 && value <= 35): document.getElementById('pm25value').style.backgroundColor = 'aquamarine'; break; case (value > 35 && value <= 50): document.getElementById('pm25value').style.backgroundColor = 'yellow'; break; case (value > 50 && value <= 100): document.getElementById('pm25value').style.backgroundColor = 'lightcoral'; break; case (value > 100): document.getElementById('pm25value').style.backgroundColor = 'red'; break; default: document.getElementById('pm25value').style.backgroundColor = 'grey'; break; } element = document.getElementById('pm10'); value = parseFloat(element.innerHTML); switch (true) { case (value > 0 && value <= 20): document.getElementById('pm10value').style.backgroundColor = 'aqua'; break; case (value > 20 && value <= 35): document.getElementById('pm10value').style.backgroundColor = 'aquamarine'; break; case (value > 35 && value <= 50): document.getElementById('pm10value').style.backgroundColor = 'yellow'; break; case (value > 50 && value <= 100): document.getElementById('pm10value').style.backgroundColor = 'lightcoral'; break; case (value > 100): document.getElementById('pm10value').style.backgroundColor = 'red'; break; default: document.getElementById('pm10value').style.backgroundColor = 'grey'; break; } } </script> <div id='overlay' style='position: absolute; top: 0px; left: 0px; width: 100%; height: 100%; background-color: rgba(169, 169, 169, 0.5); align-items: center; justify-content: center;'> <div class='lds-default'><div></div><div></div><div></div><div></div><div></div><div></div><div></div><div></div><div></div><div></div><div></div><div></div></div> </div> </article> </section> <p><button class='button' onclick='refresh();'>Messung starten</button></p> <p>Air Quality Index (AQI in μg/m³): <br/><span id='sehrgut'>sehr gut (0-20) </span><br/><span id='gut'>gut (21-35) </span><br/><span id='mittel'>mittel (36-50) </span><br/><span id='schlecht'>schlecht (51-100) </span><br/><span id='sehrschlecht'>sehr schlecht (> 100) </span></p><hr> <footer> <p>Carl von Ossietzky Universität Oldenburg</p> <p>Steffen Meeuw - Wintersemester 2019/20</p> </footer> </body> </html>";
server.send(200, "text/html", html); 
}

bool SendToDatabase(float pm25, float pm10, float temp, float hum) {
  // Connect to thingspeak database.
  if (client.connect(serverThingspeak, 80)) {
    // Construct API request body
    String body = "field1=";
           body += pm25;
           body += "&field2=";
           body += pm10;
            body += "&field3=";
           body += String(temp);
            body += "&field4=";
           body += String(hum);

    client.print("POST /update HTTP/1.1\n");
    client.print("Host: api.thingspeak.com\n");
    client.print("Connection: close\n");
    client.print("X-THINGSPEAKAPIKEY: " + writeAPIKey + "\n");
    client.print("Content-Type: application/x-www-form-urlencoded\n");
    client.print("Content-Length: ");
    client.print(body.length());
    client.print("\n\n");
    client.print(body);
    // end every post request with two blank lines.
    client.print("\n\n");
  }}
