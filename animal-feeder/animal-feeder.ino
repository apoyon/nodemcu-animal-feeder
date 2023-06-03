#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <WebSocketsServer.h>
#include <ESP8266WebServer.h>
#include <Hash.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

/*******************************************************************
/* This is the user preference variables
/* You can can change the values of this or leave it to deafault.
*/

// WiFi credentials that the ESP will connect
const char* Wifi_ssid = "WifiName";
const char* Wifi_passwd = "wifipassword";

// WiFi access point credentials that the ESP will create
const char* AP_ssid = "Feeder";
const char* AP_passwd = "password123";

// Pin number for the servo
uint8_t openerPin = 5;
uint8_t closerPin = 4;

// Timeout on how long the valve will stay opened
int openTimout = 2000;

/*******************************************************************/

/* Structure and variables declaration */
String feedLog[50];
int numFeedLog = 0; 

struct FeedSchedules {
  char description[50];
  char time[20]; 
  int selectedDays;
};

const int MAX_SCHEDULES = 10;  // Maximum number of schedules to store
FeedSchedules schedules[MAX_SCHEDULES];  // Array to store schedules
int numSchedules = 0;  // Number of schedules currently stored

unsigned long prevTime = 0;
String value;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

ESP8266WiFiMulti    WiFiMulti;
ESP8266WebServer    server(80);
WebSocketsServer    webSocket = WebSocketsServer(81);

/* Front end code (i.e. HTML, CSS, and JavaScript) */
char html_template[] PROGMEM = R"=====(
<html lang="en">
   <head>
      <meta charset="utf-8">
      <meta name="viewport" content="width=device-width, initial-scale=1">
      <title>FeedMe</title>
      <script>
         var socket = new WebSocket("ws://" + location.host + ":81");
         socket.onopen = function(e) {
             console.log("[socket] socket.onopen ");
         };
         socket.onerror = function(e) {
             console.log("[socket] socket.onerror ");
         };
         socket.onmessage = function(e) {
             console.log("[socket] " + e.data);
             var jsonData = JSON.parse(e.data);
             var cmd = jsonData.cmd;
            if (cmd === 1) {
               function addTableRow(description, time, selectedDays) {
                 var table = document.getElementById("scheduleTable");
                 var row = table.insertRow(-1);
                 var descriptionCell = row.insertCell(0);
                 var timeCell = row.insertCell(1);
                 var selectedDaysCell = row.insertCell(2);
                 descriptionCell.innerHTML = description;
                 timeCell.innerHTML = time;
                 selectedDaysCell.innerHTML = selectedDays;
               }
         
               var table = document.getElementById("scheduleTable");
               while (table.rows.length > 1) {
                 table.deleteRow(1);
               }
         
               var schedules = jsonData.schedules;
               for (var i = 0; i < schedules.length; i++) {
                 var schedule = schedules[i];
                 var description = schedule.description;
                 var time = schedule.time;
                 var selectedDays = schedule.selectedDays;
                 addTableRow(description, time, selectedDays);
               }
             }
             if (cmd === 2) {
               document.getElementById("current-time").innerHTML =jsonData.DateTime;
             }
             if (cmd === 3) {
               function addFeedLogToTable(data) {
                 var table = document.getElementById("feeding-log");
         
                 var rowCount = table.rows.length;
                 for (var i = rowCount - 1; i > 0; i--) {
                   table.deleteRow(i);
                 }
         
                 var feedLog = data.feedLog;
         
                 for (var i = 0; i < feedLog.length; i++) {
                   var row = table.insertRow(-1);
                   var cell = row.insertCell(0);
                   cell.innerHTML = feedLog[i];
                 }
               }
               function changeButtonForTime(elementId, newText, backgroundColor, duration) {
                 var button = document.getElementById(elementId);
                 var originalText = button.innerHTML;
                 var originalBackgroundColor = button.style.backgroundColor;
                 var originalDisabled = button.disabled;
                 button.disabled = true;
                 button.innerHTML = newText;
                 button.style.backgroundColor = backgroundColor;
                 setTimeout(function() {
                   button.innerHTML = originalText;
                   button.style.backgroundColor = originalBackgroundColor;
                   button.disabled = originalDisabled;
                 }, duration);
               }
               changeButtonForTime("feedNow", "Dispensing", "red", 5000);
               addFeedLogToTable(jsonData);
               }
         };
         window.onload = function() {
           function feedNow() {
             var command = {
              cmd: 0
             }
             socket.send(JSON.stringify(command));
           }
           function scheduleFeed() {
             var descriptionInput = document.getElementById("descriptionInput");
             var timeInput = document.getElementById("timeInput");
             var description = descriptionInput.value;
             var selectedDays = Array.from(document.querySelectorAll('input[name="days"]:checked')).reduce((sum, checkbox) => sum + parseInt(checkbox.value), 0);
             var time = timeInput.value;
             if (time === "") {
               alert("Time cannot be empty");
               return;
             }
             if (description === "") {
               description = "No description";
             }
             var command = {
               cmd: 1,
               description: description,
               time: time,
               selectedDays: selectedDays
             }
             console.log(command);
             socket.send(JSON.stringify(command));
             descriptionInput.value = ""; 
             timeInput.value = ""; 
           }
           document.getElementById("descriptionInput").addEventListener("keyup", function(event) {
             if (event.key === "Enter") scheduleFeed();
           });
           document.getElementById("timeInput").addEventListener("keyup", function(event) {
             if (event.key === "Enter") scheduleFeed();
           });
           document.getElementById("sendButton").addEventListener("click", function() {
             scheduleFeed();
           });
           document.getElementById("feedNow").addEventListener("click", function() {
             feedNow();
           });
         };
      </script>
      <style>
         body {
         font-family: Arial, sans-serif;
         margin: 2;
         padding: 3;
         }
         nav {
         background-color: #f2f2f2;
         padding: 10px;
         display: flex;
         justify-content: space-between;
         align-items: center;
         }
         #title {
         margin: 0;
         font-size: 18px;
         }
         span {
         font-size: 16px;
         font-weight: bold;
         }
         table {
         width: 100%;
         border-collapse: collapse;
         }
         th, td {
         padding: 8px;
         text-align: left;
         border-bottom: 1px solid gray;
         }
         th {
         background-color: #333;
         color: white;
         }
         #feedNow {
         background-color: green;
         color: white;
         border: none;
         padding: 10px;
         border-radius: 12px;
         }
         .container {
         display: flex;
         flex-wrap: wrap;
         justify-content: center;
         align-items: center;
         }
         input[type="text"],
         input[type="time"],
         button {
         margin: 2px;
         padding: 5px;
         }
      </style>
   </head>
   <body>
      <nav>
         <h1>Animal Feeder</h1>
         <span id="current-time"></span>
      </nav>
      <h3>
      Schedule an eating time:</h2>
      <div class="container">
         <input type="text" id="descriptionInput" placeholder="Description"/>
         <input type="time" id="timeInput" step="1" />
         <label><input type="checkbox" name="days" value="1"> Sunday</label>
         <label><input type="checkbox" name="days" value="2"> Monday</label>
         <label><input type="checkbox" name="days" value="4"> Tuesday</label>
         <label><input type="checkbox" name="days" value="8"> Wednesday</label>
         <label><input type="checkbox" name="days" value="16"> Thursday</label>
         <label><input type="checkbox" name="days" value="32"> Friday</label>
         <label><input type="checkbox" name="days" value="64"> Saturday</label>
         <button id="sendButton">Add</button>
      </div>
      <h3>
      Scheduled time</h2>
      <table id="scheduleTable">
         <tr>
            <th>Description</th>
            <th>Time</th>
            <th>Repeat</th>
         </tr>
      </table>
      <hr style="border: 2px solid gray; margin: 10px 0;">
      <div class="container">
         <button id="feedNow">Feed Now</button>
      </div>
      <table id="feeding-log">
         <tr>
            <th>History</th>
         </tr>
      </table>
   </body>
</html>
)=====";

/* Function declarations */
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length); // It handle all web socket responses
void handleMain(); 
void handleNotFound(); 
void dispenseFeed();
void addFeedLog(String dateTime);
void addSchedule(const char* description, const char* time, int selectedDays);
void deleteSchedule(int index);
void checkSchedule();
void printSchedules();
String getDayAbbreviation(uint8_t dayNumber);
String daySelectedToStr(int bitwiseDay);
void sendDateTime();
void sendFeedLog();
void sendSchedules();
bool isDaySelectedMatchedToday(int bitwiseDay);

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println();
  Serial.println("############ Serial Started ############");
  
  // Initialize ESP8266 in AP mode
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_ssid, AP_passwd);
  Serial.print("AP started: ");
  Serial.print(AP_ssid);
  Serial.print(" ~ ");
  Serial.println(AP_passwd);

  // Connect to a WiFi as client
  int attempt = 0;
  WiFiMulti.addAP(Wifi_ssid, Wifi_passwd);
  Serial.print("Connecting to: ");
  Serial.print(Wifi_ssid);
  while (WiFiMulti.run() != WL_CONNECTED) {
    delay(20);
    Serial.print(".");
    attempt++;
    if (attempt > 5 ) break;
  }
  Serial.println();
  if (WiFiMulti.run() == WL_CONNECTED) {
    Serial.print("Connected to: ");
    Serial.println(Wifi_ssid);
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else Serial.println("Error: Unable to connect to the WiFi");

  // Initialize GPIO pins
  pinMode(openerPin, OUTPUT);
  pinMode(closerPin, OUTPUT);
  digitalWrite(openerPin, LOW);
  digitalWrite(closerPin, LOW);
  Serial.println("Servo pins initialized");

  //Setup the time
  timeClient.begin();
  timeClient.setTimeOffset(8 * 3600); // Set time offset to 8 hours (Manila Time)
  timeClient.update(); // Sync time with NTP server
  // Print the current time
  Serial.print("Current time: ");
  Serial.println(timeClient.getFormattedTime());

  // begin the web socket
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  Serial.println("Web socket started");

  server.on("/", handleMain);
  server.onNotFound(handleNotFound);
  server.begin();
}

void loop() {
  unsigned long currentTime = millis();
  // run every seconds
  if (currentTime - prevTime >= 1000) {
    prevTime = currentTime;
    checkSchedule();
    sendDateTime();
  }
  webSocket.loop();
  server.handleClient();
}

// it handles all the web socket
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {

  switch (type) {
    
    case WStype_DISCONNECTED: {
      Serial.printf("[%u] Disconnected!\n", num);
      break;
    }
    case WStype_CONNECTED: {
      IPAddress ip = webSocket.remoteIP(num);
      Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
      // send message to client
      webSocket.sendTXT(num, "0");
      //sendDateTime();
      if(numSchedules) sendSchedules();
      if(numFeedLog) sendFeedLog();
      break;
    }
    case WStype_TEXT: {
      StaticJsonDocument<200> doc;
      deserializeJson(doc, payload, length);
      uint8_t cmd = doc["cmd"];
      Serial.print("cmd: ");
      Serial.print(cmd);
      /*
        0 - dispense
        1 - schedule
        2 - set time
        3 - feed log
      */
      switch (cmd) {
        case 0: {
          Serial.println();
          dispenseFeed();
          break;
        }  
        case 1: {
          const char* time = doc["time"];
          const char* description = doc["description"];
          int selectedDays = doc["selectedDays"];
          Serial.print(", description: ");
          Serial.print(description);
          Serial.print(", time: ");
          Serial.print(time);
          Serial.print(", SelectedDays: ");
          Serial.println(selectedDays);
          addSchedule(description, time, selectedDays);
          printSchedules();
          sendSchedules();
          break;
        }
        case 2: {
          
          break;
        }
      }
      break;
    }
    case WStype_BIN: {
      Serial.printf("[%u] get binary length: %u\n", num, length);
      hexdump(payload, length);
      // send message to client
      // webSocket.sendBIN(num, payload, length);
      break;
    }
  }
}

void handleMain() {
  server.send_P(200, "text/html", html_template ); 
}
void handleNotFound() {
  server.send(404,   "text/html", "<html><body><p>404 Error</p></body></html>" );
}

// Dispensing function
void dispenseFeed(){
    addFeedLog( getDayAbbreviation(timeClient.getDay()) + ", " + (String)timeClient.getFormattedTime());
  sendFeedLog();
  Serial.println("Dispensing");
  // Open the valve
  digitalWrite(openerPin, HIGH);
  delay(600);
  digitalWrite(openerPin, LOW);
  delay(openTimout);
  //close the valve
  digitalWrite(closerPin, HIGH);
  delay(610);
  digitalWrite(closerPin, LOW);
  Serial.println("Valve closed");
}

void addFeedLog(String dateTime){
  if(numFeedLog == 50)
    numFeedLog = 0;
  feedLog[numFeedLog] = dateTime;
  numFeedLog++;
}

// Function to add a new schedule and sort the array
void addSchedule(const char* description, const char* time, int selectedDays) {
  if (numSchedules < MAX_SCHEDULES) {
    // Create a new schedule with the provided values
    FeedSchedules newSchedule;
    strncpy(newSchedule.description, description, sizeof(newSchedule.description) - 1);
    newSchedule.description[sizeof(newSchedule.description) - 1] = '\0'; // Ensure null-termination
    strncpy(newSchedule.time, time, sizeof(newSchedule.time) - 1);
    newSchedule.time[sizeof(newSchedule.time) - 1] = '\0'; // Ensure null-termination
    newSchedule.selectedDays = selectedDays;
    schedules[numSchedules] = newSchedule;
    numSchedules++;
    Serial.println("Schedule added.");
  } else {
    Serial.println("Maximum number of schedules reached.");
  }
}

void deleteSchedule(int index) {
  if (index < 0 || index >= numSchedules) {
    // Invalid index
    return;
  }
  // Shift elements to the left
  for (int i = index; i < numSchedules - 1; i++) {
    schedules[i] = schedules[i + 1];
  }
  // Clear the last element
  FeedSchedules emptySchedule;
  memset(&emptySchedule, 0, sizeof(FeedSchedules));
  schedules[numSchedules - 1] = emptySchedule;
  numSchedules--;
}

// Function to print all schedules
void printSchedules() {
  Serial.println("Current Schedules:");
  for (int i = 0; i < numSchedules; i++) {
    Serial.println(schedules[i].description);
    Serial.println(schedules[i].time);
    Serial.println(schedules[i].selectedDays);
  }
}

String getDayAbbreviation(uint8_t dayNumber) {
  String dayAbbreviation;
  switch (dayNumber) {
    case 0:
      dayAbbreviation = "Sun";
      break;
    case 1:
      dayAbbreviation = "Mon";
      break;
    case 2:
      dayAbbreviation = "Tue";
      break;
    case 3:
      dayAbbreviation = "Wed";
      break;
    case 4:
      dayAbbreviation = "Thu";
      break;
    case 5:
      dayAbbreviation = "Fri";
      break;
    case 6:
      dayAbbreviation = "Sat";
      break;
    default:
      dayAbbreviation = "Invalid";
      break;
  }
  return dayAbbreviation;
}

// bitwise day to abbrevation
String daySelectedToStr(int bitwiseDay) {
  String selectedDays;
  if (bitwiseDay == 0) return "One Time";
  for (int i=0; i<7; i++)
    if (bitwiseDay & (1 << i)){
      selectedDays += " " + getDayAbbreviation(i);
    }
    return selectedDays;
}

void sendDateTime(){
  String data = "{\"cmd\":2, ";
  data +="\"DateTime\":\"";
  data += getDayAbbreviation(timeClient.getDay());
  data += ", ";
  data += (String)timeClient.getFormattedTime();
  data += "\"}";
  webSocket.broadcastTXT(data); // send a socket
}

void sendFeedLog() {
  const int JSON_BUFFER_SIZE = JSON_ARRAY_SIZE(20);
  const int STRING_BUFFER_SIZE = 200; // Adjust the size based on your needs
  StaticJsonDocument<JSON_BUFFER_SIZE> jsonBuffer;
  
  // Create the main JSON object
  JsonObject root = jsonBuffer.to<JsonObject>();
  
  // Create the JSON array for feedLog
  JsonArray feedLogArray = root.createNestedArray("feedLog");

  // Add each feed log entry to the JSON array
  for (int i = numFeedLog-1; i >=0 ; i--) {
    feedLogArray.add(feedLog[i]);
  }

  // Add the cmd element to the root object
  root["cmd"] = 3;

  // Serialize the JSON to a string
  char jsonString[STRING_BUFFER_SIZE];
  serializeJson(jsonBuffer, jsonString, sizeof(jsonString));

  webSocket.broadcastTXT(jsonString); // send the socket with feedLog as JSON
}

void sendSchedules() {
  const int JSON_BUFFER_SIZE = JSON_ARRAY_SIZE(MAX_SCHEDULES) + MAX_SCHEDULES * JSON_OBJECT_SIZE(3);
  DynamicJsonDocument jsonBuffer(JSON_BUFFER_SIZE);
  // Create the main JSON object
  JsonObject root = jsonBuffer.to<JsonObject>();
  // Create the JSON array for schedules
  JsonArray scheduleArray = root.createNestedArray("schedules");
  // Add each schedule to the JSON array
  for (int i = 0; i < numSchedules; i++) {
    JsonObject scheduleObject = scheduleArray.createNestedObject();
    scheduleObject["description"] = schedules[i].description;
    scheduleObject["time"] = schedules[i].time;
    scheduleObject["selectedDays"] = daySelectedToStr(schedules[i].selectedDays);
  }
  // Add the cmd element to the root object
  root["cmd"] = 1;
  // Serialize the JSON to a string
  String jsonData;
  serializeJson(jsonBuffer, jsonData);
  webSocket.broadcastTXT(jsonData); // send the socket with schedules as JSON
}

bool isDaySelectedMatchedToday(int bitwiseDay) {
  return (bitwiseDay & (1 << timeClient.getDay() ));
}

// if the schedule matched in current time run the dispense function
void checkSchedule(){
  String time;
  String currentTime = timeClient.getFormattedTime();
  for (int i = 0; i < numSchedules; i++) {
    if(isDaySelectedMatchedToday(schedules[i].selectedDays) || schedules[i].selectedDays == 0){
      time = schedules[i].time;
      if (time.equalsIgnoreCase(currentTime)){
        dispenseFeed();
        if(schedules[i].selectedDays == 0){
          deleteSchedule(i);
          sendSchedules();
        }
      }
    }
  }
}