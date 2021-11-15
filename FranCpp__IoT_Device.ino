// Version 1.0
#include <ESP8266WiFi.h>
/*TODO*/ // Add your librairies here
///////////////Parameters & Constants/////////////////
// WIFI params
char* WIFI_SSID = "S10 Léo"; //"Freebox-ABAAE6";    // Configure here the SSID of your WiFi Network
char* WIFI_PSWD = "poulette46"; //"esculum7-vehementem-parturio?-punctare";   // Configure here the PassWord of your WiFi Network
int WIFI_DELAY  = 100; //ms
// oneM2M : CSE params
String CSE_IP      = "192.168.177.76"; //Configure here the IP Address of your oneM2M CSE
int   CSE_HTTP_PORT = 8080;
String CSE_NAME    = "cse-in"; //"CseName";
String CSE_RELEASE = "3"; //Configure here the release supported by your oneM2M CSE
bool ACP_REQUIRED = false; //Configure here whether or not ACP is required controlling access
String ACPID = "";
// oneM2M : resources' params
String DESC_CNT_NAME = "DESCRIPTOR";
String DATA_CNT_NAME = "DATA";
String CMND_CNT_NAME = "COMMAND";
String ACP_NAME = "MYACP";
int TY_ACP  = 1;
int TY_AE  = 2;
int TY_CNT = 3;
int TY_CI  = 4;
int TY_SUB = 23;
String originator = "Undefined";
// HTTP constants
int LOCAL_PORT = 80;
char* HTTP_CREATED = "HTTP/1.1 201 CREATED";
char* HTTP_OK    = "HTTP/1.1 200 OK\r\n";
int REQUEST_TIME_OUT = 5000; //ms
int REQUEST_NR = 0;
//PINS. Adapt the pins' number accordding to your wiring.
int BTN_PIN = D1; //2;    // PIN for the Push Button.
int HALL_SENSOR_1_PIN = 10; // PIN for the Hall Sensor 1
int HALL_SENSOR_2_PIN = 9;  // PIN for the Hall Sensor 2
int RED_1_PIN = 5; // PIN for the LED
int RED_23_PIN = 4; // PIN for the LED
int RED_4_PIN = 0; // PIN for the LED
int GREEN_1_PIN = 14; // PIN for the LED
int GREEN_23_PIN = 12; // PIN for the LED
int GREEN_4_PIN = 13; // PIN for the LED
int GREEN_5_PIN = 16; // PIN for the LED
//stateMachine
#define State0 0
#define BothRED1 1
#define FirstRoadGreen 2
#define BothRED2 3
#define SecondRoadGreen 4
#define EmergencyVehicle 5
//sensors
bool Button = false;
bool H1 = false;
bool H2 = false;
// MISC
int SERIAL_SPEED  = 115200;
#define DEBUG
///////////////////////////////////////////
// Global variables
const long TIME_INTERVAL = 10000;
long currentMillis;
WiFiServer server(LOCAL_PORT);    // HTTP Server (over WiFi). Binded to listen on LOCAL_PORT constant
WiFiClient client0;
WiFiClient client1;
String context = "";        // The targeted actuator
String command = "";        // The received command
// Method for creating an HTTP POST with preconfigured oneM2M headers
// param : url  --> the url path of the targted oneM2M resource on the remote CSE
// param : ty --> content-type being sent over this POST request (2 for ae, 3 for cnt, etc.)
// param : rep  --> the representaton of the resource in JSON format
String doPOST(String url, String originator1, int ty, String rep) {
  String relHeader = "";
  if (CSE_RELEASE != "1") {
    relHeader = "X-M2M-RVI: " + CSE_RELEASE + "\r\n";
  }
  String postRequest = String() + "POST " + url + " HTTP/1.1\r\n" +
                       "Host: " + CSE_IP + ":" + CSE_HTTP_PORT + "\r\n" +
                       "X-M2M-Origin: " + originator + "\r\n" +
                       "Content-Type: application/vnd.onem2m-res+json;ty=" + ty + "\r\n" +
                       "Content-Length: " + rep.length() + "\r\n" +
                       relHeader +
                       "X-M2M-RI: req" + REQUEST_NR + "\r\n" +
                       "Connection: close\r\n\r\n" +
                       rep;
  // Connect to the CSE address
  Serial.println("connecting to " + CSE_IP + ":" + CSE_HTTP_PORT + " ...");
  // Get a client
  if (!client1.connect(CSE_IP, CSE_HTTP_PORT)) {
    Serial.println("Connection failed !");
    return "error";
  }
  // if connection succeeds, we show the request to be send
#ifdef DEBUG
  Serial.println(postRequest);
#endif
  // Send the HTTP POST request
  client1.print(postRequest);
  //Update request number after each sending
  REQUEST_NR += 1;
  // Manage a timeout
  unsigned long startTime = millis();
  while (client1.available() == 0) {
    if (millis() - startTime > REQUEST_TIME_OUT) {
      Serial.println("Client Timeout");
      client1.stop();
      return "error";
    }
  }
  // If success, Read the HTTP response
  String result = "";
  client1.setTimeout(500);
  if (client1.available()) {
    result = client1.readStringUntil('\r');
    Serial.println(result);
  }
  while (client1.available()) {
    String line = client1.readStringUntil('\r');
    Serial.print(line);
  }
  Serial.println();
  Serial.println("closing connection...");
  return result;
}
// Method for creating an ApplicationEntity(AE) resource on the remote CSE (this is done by sending a POST request)
// param : ae --> the AE name (should be unique under the remote CSE)
String createAE(String ae) {
  String srv = "";
  if (CSE_RELEASE != "1") {
    srv = ",\"srv\":[\"" + CSE_RELEASE + "\"]";
  }
  String aeRepresentation =
    "{\"m2m:ae\": {"
    "\"rn\":\"" + ae + "\","
    "\"api\":\"Norg.demo." + ae + "\","
    "\"rr\":true,"
    "\"poa\":[\"http://" + WiFi.localIP().toString() + ":" + LOCAL_PORT + "/" + ae + "\"]" +
    srv +
    "}}";
#ifdef DEBUG
  Serial.println(aeRepresentation);
#endif
  return doPOST("/" + CSE_NAME, originator, TY_AE, aeRepresentation);
}
// Method for creating an Access Control Policy(ACP) resource on the remote CSE under a specific AE (this is done by sending a POST request)
// param : ae --> the targeted AE name (should be unique under the remote CSE)
// param : acp  --> the ACP name to be created under this AE (should be unique under this AE)
String createACP(String ae, String acp) {
  String acpRepresentation =
    "{\"m2m:acp\": {"
    "\"rn\":\"" + acp + "\","
    "\"pv\":{\"acr\":[{\"acor\":[\"all\"],\"acop\":63}]},"
    "\"pvs\":{\"acr\":[{\"acor\":[\"all\"],\"acop\":63}]}"
    "}}";
  return doPOST("/" + CSE_NAME + "/" + ae, originator, TY_ACP, acpRepresentation);
}
// Method for creating an Container(CNT) resource on the remote CSE under a specific AE (this is done by sending a POST request)
// param : ae --> the targeted AE name (should be unique under the remote CSE)
// param : cnt  --> the CNT name to be created under this AE (should be unique under this AE)
String createCNT(String ae, String cnt) {
  String cntRepresentation =
    "{\"m2m:cnt\": {"
    "\"mni\":10,"         // maximum number of instances
    "\"rn\":\"" + cnt + "\"" +
    ACPID + //IF ACP created, it is associated to the container so that anyone has access
    "}}";
  return doPOST("/" + CSE_NAME + "/" + ae, originator, TY_CNT, cntRepresentation);
}
// Method for creating an ContentInstance(CI) resource on the remote CSE under a specific CNT (this is done by sending a POST request)
// param : ae --> the targted AE name (should be unique under the remote CSE)
// param : cnt  --> the targeted CNT name (should be unique under this AE)
// param : ciContent --> the CI content (not the name, we don't give a name for ContentInstances)
String createCI(String ae, String cnt, String ciContent) {
  String ciRepresentation =
    "{\"m2m:cin\": {"
    "\"con\":\"" + ciContent + "\""
    "}}";
  return doPOST("/" + CSE_NAME + "/" + ae + "/" + cnt, originator,  TY_CI, ciRepresentation);
}
// Method for creating an Subscription (SUB) resource on the remote CSE (this is done by sending a POST request)
// param : ae --> The AE name under which the SUB will be created .(should be unique under the remote CSE)
//          The SUB resource will be created under the COMMAND container more precisely.
String createSUB(String ae) {
  String subRepresentation =
    "{\"m2m:sub\": {"
    "\"rn\":\"SUB_" + ae + "\","
    "\"nu\":[\"" + CSE_NAME + "/" + ae  + "\"], "
    "\"nct\":1,"
    "\"enc\":{\"net\":[3]}"
    "}}";
  return doPOST("/" + CSE_NAME + "/" + ae + "/" + CMND_CNT_NAME, originator,  TY_SUB, subRepresentation);
}
// Method to register a module (i.e. sensor or actuator) on a remote oneM2M CSE
void registerModule(String module, bool isActuator, String intialDescription, String initialData) {
  if (WiFi.status() == WL_CONNECTED) {
    String result;
    // 1. Create the ApplicationEntity (AE) for this sensor
    result = createAE(module);
    if (result.equalsIgnoreCase(HTTP_CREATED)) {
#ifdef DEBUG
      Serial.println("AE " + module + " created  !");
#endif
      // 1.1 Create the AccessControlPolicy (ACP) for this sensor so that monitor can subscribe to it
      if (ACP_REQUIRED) {
        result = createACP(module, ACP_NAME);
        if (result.equalsIgnoreCase(HTTP_CREATED)) {
          ACPID = ",\"acpi\":[\"" + CSE_NAME + "/" + module + "/" + ACP_NAME + "\"]";
#ifdef DEBUG
          Serial.println("ACP " + module + " created  !");
#endif
        }
      }
      // 2. Create a first container (CNT) to store the description(s) of the sensor
      result = createCNT(module, DESC_CNT_NAME);
      if (result.equalsIgnoreCase(HTTP_CREATED)) {
#ifdef DEBUG
        Serial.println("CNT " + module + "/" + DESC_CNT_NAME + " created  !");
#endif
        // Create a first description under this container in the form of a ContentInstance (CI)
        result = createCI(module, DESC_CNT_NAME, intialDescription);
        if (result.equalsIgnoreCase(HTTP_CREATED)) {
#ifdef DEBUG
          Serial.println("CI " + module + "/" + DESC_CNT_NAME + "/{initial_description} created !");
#endif
        }
      }
      // 3. Create a second container (CNT) to store the data  of the sensor
      result = createCNT(module, DATA_CNT_NAME);
      if (result.equalsIgnoreCase(HTTP_CREATED)) {
#ifdef DEBUG
        Serial.println("CNT " + module + "/" + DATA_CNT_NAME + " created !");
#endif
        // Create a first data value under this container in the form of a ContentInstance (CI)
        result = createCI(module, DATA_CNT_NAME, initialData);
        if (result.equalsIgnoreCase(HTTP_CREATED)) {
#ifdef DEBUG
          Serial.println("CI " + module + "/" + DATA_CNT_NAME + "/{initial_data} created !");
#endif
        }
      }
      // 4. if the module is an actuator, create a third container (CNT) to store the received commands
      if (isActuator) {
        result = createCNT(module, CMND_CNT_NAME);
        if (result.equalsIgnoreCase(HTTP_CREATED)) {
#ifdef DEBUG
          Serial.println("CNT " + module + "/" + CMND_CNT_NAME + " created !");
#endif
          // subscribe to any command put in this container
          result = createSUB(module);
          if (result.equalsIgnoreCase(HTTP_CREATED)) {
#ifdef DEBUG
            Serial.println("SUB " + module + "/" + CMND_CNT_NAME + "/SUB_" + module + " created !");
#endif
          }
        }
      }
    }
  }
}
void init_IO() {
  Serial.begin(SERIAL_SPEED);
  pinMode(BTN_PIN, INPUT);
  pinMode(HALL_SENSOR_1_PIN, INPUT);
  pinMode(HALL_SENSOR_2_PIN, INPUT);
  pinMode(D0, OUTPUT);
  /*
    pinMode(RED_1_PIN, OUTPUT);
    pinMode(RED_23_PIN, OUTPUT);
    pinMode(RED_4_PIN, OUTPUT);
    pinMode(GREEN_1_PIN, OUTPUT);
    pinMode(GREEN_23_PIN, OUTPUT);
    pinMode(GREEN_4_PIN, OUTPUT);
    pinMode(GREEN_5_PIN, OUTPUT); */
}
void task_IO() {
}
void init_WiFi() {
  Serial.println("Connecting to  " + String(WIFI_SSID) + " ...");
  WiFi.persistent(false);
  WiFi.begin(WIFI_SSID, WIFI_PSWD);
  // wait until the device is connected to the wifi network
  while (WiFi.status() != WL_CONNECTED) {
    delay(WIFI_DELAY);
    Serial.print(".");
  }
  // Connected, show the obtained ip address
  Serial.println("WiFi Connected ==> IP Address = " + WiFi.localIP().toString());
}
void task_WiFi() {
}
void init_HTTPServer() {
  server.begin();
  Serial.println("Local HTTP Server started !");
}
void task_HTTPServer() {
  // Check if a client is connected
  client0 = server.available();
  if (client0) {
    // Wait until the client sends some data
    Serial.println("New client connected. Receiving request <=== ");
    while (!client0.available()) {
#ifdef DEBUG
      Serial.print(".");
#endif
      delay(5);
    }
    // Read the request
    client0.setTimeout(500);
    String request = client0.readString();
    Serial.println(request);
    int start, end;
    // identify the right module (sensor or actuator) that received the notification
    // the URL used is ip:port/ae
    start = request.indexOf("/");
    end = request.indexOf("HTTP") - 1;
    context = request.substring(start + 1, end);
#ifdef DEBUG
    Serial.println(String() + "context = [" + start + "," + end + "] -> " + context);
#endif
    // ingore verification messages
    if (request.indexOf("vrq") > 0) {
      client0.flush();
      client0.print(HTTP_OK);
      delay(5);
      client0.stop();
      Serial.println("Client disconnected");
      return;
    }
    //Parse the request and identify the requested command from the device
    //Request should be like "[operation_name]"
    start = request.indexOf("[");
    end = request.indexOf("]"); // first occurence fo
    command = request.substring(start + 1, end);
#ifdef DEBUG
    Serial.println(String() + + "command = [" +  start + "," + end + "] -> " + command);
#endif
    client0.flush();
    client0.print(HTTP_OK);
    delay(5);
    client0.stop();
    Serial.println("Client disconnected");
  }
}
void handle_HTTPMessage() {
  if (context != "")
  {
    if (context == "ESPActuator")
    {
      command_esp(command);
      task_esp(command);
    }
    else
    {
      Serial.println("The target AE does not exist ! ");
    }
  }
  // reset "command" and "context" variables for future received requests
  context = "";
  command = "";
}
// Code for the Button Sensor
int newButtonState;
int oldButtonState = 0;
void init_button() {
  String initialDescription = "Name=ButtonSensor | Location = SideWalk";
  String initialData = "0";
  originator = "Cae-ButtonSensor";
  registerModule("ButtonSensor", false, initialDescription, initialData);
}
void task_button() {
  newButtonState = digitalRead(BTN_PIN);
  if (oldButtonState == 0 && newButtonState == 1) {
#ifdef DEBUG
    Serial.print("Button Clicked -> Sending data : ButtonSensor value = 1");
#endif
    String ciContent = "1";
    originator = "Cae-ButtonSensor";
    createCI("ButtonSensor", DATA_CNT_NAME, ciContent);
  }
  oldButtonState = newButtonState;
}
void command_button(String cmd) {
}
// Code for the Sensor Hall 1
void init_hall1() {
  String initialDescription = "Name=Hall1Sensor | Location = Road";
  String initialData = "0";
  originator = "Cae-Hall1Sensor";
  registerModule("Hall1Sensor", false, initialDescription, initialData);
}
void task_hall1() {
  int sensorValue;
  sensorValue = digitalRead(HALL_SENSOR_1_PIN);
#ifdef DEBUG
  Serial.print("Sending data : Hall1Sensor value = ");
  Serial.println(sensorValue);
#endif
  String ciContent = String(sensorValue);
  originator = "Cae-Hall1Sensor";
  createCI("Hall1Sensor", DATA_CNT_NAME, ciContent);
}
void command_hall1(String cmd) {
}
// Code for the Sensor Hall 2
void init_hall2() {
  String initialDescription = "Name=Hall2Sensor | Location = Road";
  String initialData = "0";
  originator = "Cae-Hall2Sensor";
  registerModule("Hall2Sensor", false, initialDescription, initialData);
}
void task_hall2() {
  int sensorValue;
  sensorValue = digitalRead(HALL_SENSOR_2_PIN);
#ifdef DEBUG
  Serial.print("Sending data : Hall2Sensor value = ");
  Serial.println(sensorValue);
#endif
  String ciContent = String(sensorValue);
  originator = "Cae-Hall2Sensor";
  createCI("Hall2Sensor", DATA_CNT_NAME, ciContent);
}
void command_hall2(String cmd) {
}
// Code for the actuator esp
bool test = false;
void init_esp() {
  String initialDescription = "Name=ESPActuator | Location = all";
  String initialData = "nothing_active";
  originator = "Cae-ESPActuator";
  registerModule("ESPActuator", true, initialDescription, initialData);
}
void task_esp(String cmd) {
  originator = "Cae-ESPActuator";
  createCI("ESPActuator", DATA_CNT_NAME, cmd);
}
void command_esp(String cmd) {
  if (cmd == "Button") {
    Serial.println("Button active");
    Button = true;
    // mode1,mode2,mode3
  }
  else
    if (cmd == "H1") {
      Serial.println("Hall1 active");
      H1 = true;
    }
    else
      if (cmd == "H2") {
        Serial.println("Hall2 active");
        H2 = true;
      }
      else
        if (cmd == "TEST") {
          Serial.println("TEST Command");
          test = !test;
          digitalWrite(D0, test ? HIGH : LOW);
        }
}
// Code for the actuator Red Led 1
/*
  void init_redled1() {
  String initialDescription = "Name=RedLed1Actuator | Location = TrafficLight1";
  String initialData = "switchOff";
  originator = "Cae-RedLed1Actuator";
  registerModule("RedLed1Actuator", true, initialDescription, initialData);
  }
  void task_redled1(String cmd) {
  originator = "Cae-RedLed1Actuator";
  createCI("RedLed1Actuator", DATA_CNT_NAME, cmd);
  }
  //switchon -> mode1
  void command_redled1(String cmd) {
  if (cmd == "switchOn") {
    #ifdef DEBUG
      Serial.println("Switching on the Red LED 1 ...");
    #endif
    digitalWrite(RED_1_PIN, HIGH);
  }
  else
    if (cmd == "switchOff") {
      #ifdef DEBUG
        Serial.println("Switching off the Red LED 1 ...");
      #endif
      digitalWrite(RED_1_PIN, LOW);
    }
  }*/
void setup() {
  // intialize the serial liaison
  Serial.begin(SERIAL_SPEED);
  // configure sensors and actuators HW
  init_IO();
  // Connect to WiFi network
  init_WiFi();
  // Start HTTP server
  init_HTTPServer();
  // register sensors and actuators
  init_button();
  //init_hall1();
  //init_hall2();
  init_esp();
}
void FiniteStateMachine() {
#ifdef DEBUG
  Serial.println("begin MAE");
#endif
  static uint8_t currentState = State0;
  static uint32_t t0 = 0;
  //default value of each light
  digitalWrite(RED_1_PIN, LOW);
  digitalWrite(RED_23_PIN, LOW);
  digitalWrite(RED_4_PIN, LOW);
  digitalWrite(GREEN_1_PIN, LOW);
  digitalWrite(GREEN_23_PIN, LOW);
  digitalWrite(GREEN_4_PIN, LOW);
  digitalWrite(GREEN_5_PIN, LOW);
  switch (currentState)
  {
    case State0: //initialization
      //all lights are red
      digitalWrite(RED_1_PIN, HIGH);
      digitalWrite(RED_23_PIN, HIGH);
      digitalWrite(RED_4_PIN, HIGH);
      t0 = millis();
      //setTimeout(HandleTimer, 1000); //set up of 1s timer
      currentState = BothRED1; //can directly go to the first state, only a timer set up state
      break;
    case BothRED1:
      //all lights are red
      digitalWrite(RED_1_PIN, HIGH);
      digitalWrite(RED_23_PIN, HIGH);
      digitalWrite(RED_4_PIN, HIGH);
      if ((H1 == true) || (H2 == true)) { //if an emergency vehicle passed through one of the sensor
        currentState = EmergencyVehicle;
      }
      else if ( (millis() - t0 > 1000) ) { //when 1s timer is over
        currentState = FirstRoadGreen; //next state : first road lights are green
        t0 = millis(); //timer_start(3000); //set up 3s timer
      }
      break;
    case FirstRoadGreen:
      //only first road lights are green
      digitalWrite(GREEN_1_PIN, HIGH);
      digitalWrite(GREEN_4_PIN, HIGH);
      digitalWrite(RED_23_PIN, HIGH);
      if ((H1 == true) || (H2 == true)) { //if an emergency vehicle passed through one of the sensor
        currentState = EmergencyVehicle;
      }
      else if ( (millis() - t0 > 3000) ) { //when 3s timer is over
        currentState = BothRED2; //next state : all lights are red
        t0 = millis(); //set up 1s timer
      }
      break;
    case BothRED2:
      //all lights are red
      digitalWrite(RED_1_PIN, HIGH);
      digitalWrite(RED_23_PIN, HIGH);
      digitalWrite(RED_4_PIN, HIGH);
      if ((H1 == true) || (H2 == true)) { //if an emergency vehicle passed through one of the sensor
        currentState = EmergencyVehicle;
      }
      else if ( (millis() - t0 > 1000) ) { //when 1s timer is over
        currentState = SecondRoadGreen; //next state : only second road lights are green
        t0 = millis();
      }
      break;
    case SecondRoadGreen:
      //only second road and pedestrians lights are green
      digitalWrite(RED_1_PIN, HIGH);
      digitalWrite(RED_4_PIN, HIGH);
      digitalWrite(GREEN_23_PIN, HIGH);
      digitalWrite(GREEN_5_PIN, HIGH);
      if ((H1 == true) || (H2 == true)) {  //if an emergency vehicle passed through one of the sensor
        currentState = EmergencyVehicle;
      }
      else if ( Button == false ) {
        if (millis() - t0 > 3000) {
          currentState = BothRED1; //next state : all lights are red
          t0 = millis();
        }
        else if (Button == true) {
          if (millis() - t0 > 6000) {
            currentState = BothRED1; //next state : all lights are red
            t0 = millis();
          }
          //when 3s or 6s timer is over
        }
        break;
      case EmergencyVehicle: //When an emergency vehicle passed through one of the sensor
        if ((H1 == true) && (H2 == false)) { //Emergency Vehicle is coming from the same road as the number 1 light
          //only number 1 light is green
          digitalWrite(GREEN_1_PIN, HIGH);
          digitalWrite(RED_23_PIN, HIGH);
          digitalWrite(RED_4_PIN, HIGH);
        }
        else if (H2 == true) { //Emergency Vehicle is coming from the same road as the number 4 light
          // only number 4 light in green
          digitalWrite(GREEN_4_PIN, HIGH);
          digitalWrite(RED_1_PIN, HIGH);
          digitalWrite(RED_23_PIN, HIGH);
        }
        if ((H1 == true) && (H2 == true)) { //Emergency Vehicle totally crossed the road //
          t0 = millis();
          currentState = BothRED1; //trafic can return to normal
          //sensor reset
          H1 = false;
          H2 = false;
        }
        break;
      default:
        currentState = State0;
        break;
      }
  }
}
// Main loop of the µController
void loop() {
  //Serial.println("Debut programme");
  // Check if a client is connected
  task_HTTPServer();
  // analyse the received command (if any)
  handle_HTTPMessage();
  // handle sensors
  task_button();
  Serial.println("fin task 1");
  task_hall1();
  Serial.println("fin task 2");
  task_hall2();
  Serial.println("fin task 3 debut MAE");
  FiniteStateMachine();
  //Serial.println("fin MAE");
}
