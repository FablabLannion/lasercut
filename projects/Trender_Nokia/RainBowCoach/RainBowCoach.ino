 /* SecretGarden Projet , aka "antiburnout machine".
 *  project built in  Fablab Lannion
 * Connections:
 * WeMos D1 Mini   ConnectTo    Description
 * (ESP8266)       
 *
 * D1              In            Cmd for Neopixel. Use a 479ohm resistor between IO & neopixel input
 * 3V3             Vcc           3.3V from ESP to neopixel
 * G               Gnd           Ground for neopixel &  left pad from switch 
 
// https://learn.sparkfun.com/tutorials/esp8266-thing-hookup-guide/example-sketch-ap-web-server
//Adress for access point : http://192.168.4.1/ , to be confirmed using serial line from arduino IDE.

//Environment to be used to build the project:
// IDE: ARDUINO 1.6.9
// Board to flash: Wemos D1 R2 & Mini
// HW : see http://www.banggood.com/3Pcs-D1-Mini-NodeMcu-Lua-WIFI-ESP8266-Development-Board-p-1047943.html for details


// TODOLIST : 
      - manage access point to store SSID , Wifi & thingspeak infos
	  - add multiuser profile to thingspeak
//
*/

#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

	 

	 /**********************************************************/
	 /****            TO BE CLEANED          ****/
 /**********************************************************/
 unsigned int time1=30; //in seconds
 unsigned int time2=50;
 unsigned int time3=60;
 unsigned int nbOfSecondsElapsed=0;
 unsigned char stepNumber=0;
 unsigned int abortCycle=0;
 
unsigned char colorsKeeper[][3] = { {0,0,255},
                    {200,255,4},
					{255,0,0},{0,0,0}};
					
//Default profiles:

// #1: 1mn : 50%, 90%
// #2: 10mn : 50%,90%
// #3: 1H   : 40mn,55mn
unsigned int profilesKeeper[][3] = { {30,54,60},{300,540,600},{2400,3300,3600}};	
#define kDynamicProfile 2 // id where the profile updated by webserver will be overridden
unsigned char selectedProfile=0;				
					
		




 /**********************************************************/
 /****    USER DATA, TO BE REMOVED BEFORE ANY COMMIT!!! ****/
 /**********************************************************/
//thinkgspeak API KEY for SecretGarden Channel ( NOT TO BE SHARED!!!)
const char* channelID = "236531";   
      
 /**********************************************************/
 /****            THINGSPEAK CONFIGURATION              ****/
 /**********************************************************/
#define SERVER "api.thingspeak.com"  
#define THINGSPEAK_PORT_NUMBER 80
#define SLEEP_TIME 15000





 /**********************************************************/
 /****            JSON PARSER CONFIGURATION              ****/
 /**********************************************************/
#include <ArduinoJson.h>
const char* ThingSpeakServer = "api.thingspeak.com";  // server's address
const unsigned char  ThingSpeakPort = 80; 
const char* resource1 = "/channels/";   // http resource
const char* resource2 = "/feeds/last.json"; 

const unsigned long HTTP_TIMEOUT = 20000;  // max response time from server
const size_t MAX_CONTENT_SIZE = 512;       // max size of the HTTP response


// The type of data that we want to extract from the page
struct UserData {
  char created_at[128]; 
  unsigned char field1; 
  unsigned char field2; 
  unsigned char field3;  
  unsigned char field4;   
  unsigned char field5;   
  unsigned char field6;   
  unsigned char field7;   
};

unsigned char level=1; //default value
unsigned char badge=0; //default value


unsigned char newSampleDetected=0;
char timestamp[32]="rindutout";
StaticJsonBuffer<200> jsonBuffer;


/**********************************************************/
/****            SECRETGARDEN CONFIGURATION             ****/
/**********************************************************/
unsigned char colors[][3] = { {255,0,194},
                    {0,239,255},
                    {200,255,4},
                    {31,167,34},
					{32,67,136},
					{30,175,195},
					{102,255,234},
					{90,19,170},
					{253,172,204},{255,255,255}};


 /**********************************************************/
 /****            BRIGTHNESS CONTROLLER                 ****/
 /**********************************************************/
unsigned long lastConnectionTime = 0;            // last time brightness was updated, in milliseconds
const unsigned long postingInterval = 1000L * 60L * 5L; // delay between updates, in milliseconds
int direction = 1; //decide wether we have to increase or decrease brightness

	 
	
					


 /**********************************************************/
 /****            WIFI CONFIGURATION                ****/
 /**********************************************************/


 
 
//////////////////////
// WiFi Definitions //
//////////////////////
const char WiFiAPPSK[] = "secretgarden"; //Wifi password for access point
IPAddress myIp;
ESP8266WebServer wicoServer(80);
int  wicoConfigAddr = 0; /**< address in eeprom to store user profiles */
uint8_t wicoIsConfigSet = 0; /**< is wifi configuration has been set through webserver ? */
#define NB_TRY 10 /**< duration of wifi connection trial before aborting (seconds) */
WiFiClient client;


//List possible values for WIFI Spots
unsigned char wifiSpotIndex=1;
unsigned char firstAttempt=true;

char ssid[] = "Freebox-Murlockc";      //  your network SSID (name) // TODO : shall be configurable from webserver
char pass[] = "doudousterenn";   // your network password // TODO : shall be configurable from webserver


//Following define is a pure mistery. Depending on WIFI connection, we receive one additionnal header to filter out. WTF is this???
#define SKIP_TP_HEADER
 


 /**********************************************************/
 /****            NEOPIXEL CONFIGURATION                ****/
 /**********************************************************/



#include <Adafruit_NeoPixel.h>
#define PIN            D2 //sets the pin on which the neopixels are connected
#define NUMPIXELS      8//defines the number of pixels in the strip
#define interval       50 //defines the delay interval between running the functions
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

uint32_t red = pixels.Color(255, 0, 0);
uint32_t blue = pixels.Color(0, 0, 255);
uint32_t green = pixels.Color(0, 255, 0);
uint32_t pixelColour;
uint32_t lastColor;
float activeColor[] = {255, 0, 0};//sets the default color to red
boolean NeoState[] = {false, false, false, false, false, false, false, false, false, false, false, false, false, false, true}; //Active Neopixel Function (off by default)
uint8_t brightness = 255; //sets the default brightness value
uint8_t R=0;
uint8_t G=0;
uint8_t B=0;




//-------------------------------------------------------
// WIFI FUNCTIONS
//-------------------------------------------------------

/** read wifi config stored in eeprom.
 *  
 *  There must be at least 35+64 bytes after given configAddr.
 *  returned value should be configAddr+32+32+32
 *  eeprom must have been initialised : EEPROM.begin(size);
 *  
 *  @param[in] configAddr eeprom address for begining of configuration
 *  @param[out] time1 read time1 in seconds : 32 Bytes
 *  @param[out] time2 read time2 in seconds : 32 Bytes
 *  @param[out] time3 read time3 in seconds : 32 Bytes 
 *  @return 1 if success, 0 if failure
 */
//------------------------------------------------------- 
int wicoReadWifiConfig (int configAddr, char* time1, char* time2, char* time3) {
//-------------------------------------------------------
  int i = 0;
  
  for (i = 0; i < 32; i++)
    {
      time1[i] = char(EEPROM.read(i+configAddr));
    }
  for (i=0; i < 32; i++)
    {
      time2[i] = char(EEPROM.read(i+configAddr+32));
    }
  for (i=0; i < 32; i++)
    {
      time3[i] = char(EEPROM.read(i+configAddr+64));
    }	
    return 1;
}



/** write wifi config to eeprom.
 *  
 *  There must be at least 35+64 bytes after given configAddr.
 *  returned value should be configAddr+32+64
 *  eeprom must have been initialised : EEPROM.begin(size);
 *  commit() is done here
 *  
 *  @param[in] configAddr eeprom address for begining of configuration
 *  @param[in] ssid read ssid : 32 Bytes
 *  @param[in] pwd read wifi password : 64 Bytes
 *  @return 1 if success, 0 if failure
 */
int wicoWriteWifiConfig (int configAddr, const char time1[32], const char time2[32], const char time3[32]) {
  int i = 0;
  
  for (i = 0; i < 32; i++)
    {
      EEPROM.write(i+configAddr,time1[i]);
    }
  for (i=0; i < 32; i++)
    {
      EEPROM.write(i+configAddr+32,time2[i]);
    }
  for (i=0; i < 32; i++)
    {
      EEPROM.write(i+configAddr+64,time3[i]);
    }	
    EEPROM.commit();
    return 1;
}


//-------------------------------------------------------
void setupWiFi()
//-------------------------------------------------------
{


  myIp = wicoSetupWifi (ssid, pass);
  if (myIp != IPAddress(0,0,0,0)) {
    // success
	Serial.println("Successfully Connected to WIFI :-)");  
	//Blue Color
	
	R=0;
	G=0;
	B=255;	
	writeLEDS(R, G, B); 
	delay(500);	
	//Breath();	
	//Black Color
	R=0;
	G=0;
	B=0;	
	writeLEDS(R, G, B);      
	delay (50);	
	

 	
  } 
  else
  {
	Serial.println("Failed connecting to WIFI, create Access Point to adapt Wifi parameters ");   
	myIp =setupAccessPoint();	
	//Green Color, waiting for update
	R=0;
	G=255;
	B=0;	
	writeLEDS(R, G, B);   
	delay (50);		
  }
 

 


}




//-------------------------------------------------------
IPAddress setupAccessPoint()
//-------------------------------------------------------
{
  WiFi.mode(WIFI_AP);

  // Do a little work to get a unique-ish name. Append the
  // last two bytes of the MAC (HEX'd) to "Thing-":
  uint8_t mac[WL_MAC_ADDR_LENGTH];
  WiFi.softAPmacAddress(mac);
  String macID = String(mac[WL_MAC_ADDR_LENGTH - 2], HEX) +
                 String(mac[WL_MAC_ADDR_LENGTH - 1], HEX);
  macID.toUpperCase();
  String AP_NameString = "SecretGarden " + macID;

  char AP_NameChar[AP_NameString.length() + 1];
  memset(AP_NameChar, 0, AP_NameString.length() + 1);

  for (int i=0; i<AP_NameString.length(); i++)
    AP_NameChar[i] = AP_NameString.charAt(i);
Serial.println("AP_NameChar:");
Serial.println(AP_NameChar);

  WiFi.softAP(AP_NameChar, WiFiAPPSK);
  return  WiFi.softAPIP();  
}


/** reset wifi config in eeprom
 *  
 *  write \0 all over the place.
 *  
 *  @param[in] configAddr eeprom address for begining of configuration
 *  @return 1 if success, 0 if failure
 */
int wicoResetWifiConfig (int configAddr) {
  char d[64];
  memset (d, 0, 64);
  return wicoWriteWifiConfig (configAddr, d, d, d);
  
}



/** handle / URI for AP webserver
 */
//------------------------------------------------------- 
void wicoHandleRoot (void) {
//-------------------------------------------------------
  String s = "<html><body><h1>SecretGarden Config</h1>";

  // arguments management
  if (wicoServer.hasArg("reset") ) {
    Serial.println("reset");
    wicoResetWifiConfig (wicoConfigAddr);
  } else if (wicoServer.hasArg("time1")) {
    Serial.print("save:");
    Serial.println(wicoServer.arg("time1").c_str());
    Serial.print("save:");
    Serial.println(wicoServer.arg("time2").c_str());
    Serial.print("save:");
    Serial.println(wicoServer.arg("time3").c_str());

	Serial.println("**** BEFORE:*****");
	Serial.print("time1:");
	Serial.println(time1);
	Serial.print("time2:");
	Serial.println(time2);
	Serial.print("time3:");
	Serial.println(time3);	
	
	time1 = atoi(wicoServer.arg("time1").c_str());

	time2 = atoi(wicoServer.arg("time2").c_str());

	time3 = atoi(wicoServer.arg("time3").c_str());	
	
	
	/*
	Serial.println("**** AFTER:*****");
	Serial.print("time1:");
	Serial.println(time1);
	Serial.print("time2:");
	Serial.println(time2);
	Serial.print("time3:");
	Serial.println(time3);	
	//Refresh profilesKeeper
     profilesKeeper[kDynamicProfile][0]=time1;
     profilesKeeper[kDynamicProfile][1]=time2;
     profilesKeeper[kDynamicProfile][2]=time3;
	 */
 	
	
    wicoWriteWifiConfig (wicoConfigAddr, wicoServer.arg("time1").c_str(), wicoServer.arg("time2").c_str(), wicoServer.arg("time3").c_str());
    wicoIsConfigSet = 1;
  }

  // construct <form>
  s += "<p><form>WIFI SSID: ";

  s += "<input type=text name=time1>";

  s += "<br>WIFI PWD: <input type='text' name='time2'><br/><br>SECRET ID: <input type='text' name='time3'><br/><br/><input type='submit' value='send'></form></p></body></html>\n";
  
  wicoServer.send ( 200, "text/html", s );
}

/** start web server and wait for wifi configuration.
 *  @return only when wifi configuration has been set
 */
//------------------------------------------------------- 
void wicoSetupWebServer (void) {
//-------------------------------------------------------
  wicoIsConfigSet = 0;
  wicoServer.on("/", wicoHandleRoot);
  wicoServer.begin();
  Serial.println("HTTP server started");
  while (!wicoIsConfigSet) {
    wicoServer.handleClient();
  }
  wicoServer.stop();
}





/** 
 *  read recorded informations from eeprom
 */
//------------------------------------------------------- 
int LoadUserProfile (int configAddr) {
//------------------------------------------------------- 
  char time1_[32];
  char time2_[32];  
  char time3_[32];

  wicoConfigAddr = configAddr;

  // read config
  wicoReadWifiConfig (configAddr, time1_, time2_, time3_);
  Serial.print(time1_);
  Serial.print("/");
  Serial.print(time2_);
  Serial.println("/");
  Serial.print(time3_);
  Serial.println("/"); 
  
	
	time1 = atoi(time1_);
	time2 = atoi(time2_);
	time3 = atoi(time3_);	

/*
	Serial.println("**** AFTER:*****");
	Serial.print("time1:");
	Serial.println(time1);
	Serial.print("time2:");
	Serial.println(time2);
	Serial.print("time3:");
	Serial.println(time3);	
	*/
	//Refresh profilesKeeper
	 profilesKeeper[kDynamicProfile][0]=time1;
	 profilesKeeper[kDynamicProfile][1]=time2;
	 profilesKeeper[kDynamicProfile][2]=time3;  

  return 0;
}



//-------------------------------------------------------
/** connect to an existing wifi network
 *  @param[in] ssid the ssid of the network
 *  @param[in] pwd the password of the network
 *  @return assigned ip address
 */
IPAddress wicoSetupWifi(char* ssid, char* pwd) {
//-------------------------------------------------------
    // try connecting to a WiFi network
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, pwd);
  for (int i=0; i<2*NB_TRY; i++) {
    if (WiFi.status() == WL_CONNECTED) {
      return WiFi.localIP();
    }
    Serial.print(WiFi.status());
    delay(500);
	//Twist for a while to show we are still looking for wifi
	/*
	   for (int i=0; i <= 3; i++){
			//  White	 Band
			R=128;
			G=0;
			B=0;	
			writeLEDS(R, G, B);   
			delay (50);
			R=0;
			G=0;
			B=0;	
			writeLEDS(R, G, B);   
			delay (50);		
	   } 	
	   */
  }
  return IPAddress (0,0,0,0);
}




//-------------------------------------------------------
// MAIN FUNCTIONS
//-------------------------------------------------------





//-------------------------------------------------------
void initHardware()
//-------------------------------------------------------
{
  EEPROM.begin(512);
  Serial.begin(9600); // initialize serial communication



  //Load Dynamic Profile Values from eeprom
  LoadUserProfile(0);   

  pixels.begin(); //starts the neopixels
  pixels.setBrightness(brightness); // sets the inital brightness of the neopixels
  pixels.show();
  delay(1000);  

  Serial.println("RainBowCoach initiated");
  //ColorCycle(SLEEP_TIME);	  

   R=0;
   G=0;
   B=0;
   writeLEDS(R, G, B); //set neopixel to black by default, because blue is a cool value :-)
  
	   

}



//-------------------------------------------------------
void setup() 
//-------------------------------------------------------
{

  
  initHardware();
  

  setupWiFi();
  
  Serial.println("Wifi Successfully Configured, IP: ");  
  Serial.println(myIp);   

}



void loop__ ()
{ 
/*
writeLEDS(255, 0, 0);   
delay (10000);
writeLEDS(0,255, 0);   
delay (10000);
	writeLEDS(0,0,255);   
delay (10000);
*/
writeLEDS(255,0,194);   
delay (10000);
writeLEDS(255,255,255);   
delay (10000);
}			

//-------------------------------------------------------
void loop() 
//-------------------------------------------------------
{



		//------------------------------------ 
		// Interaction with ThingSpeak  
		//------------------------------------	
		// Retrieve latest value from ThingSpeak Database
		if (client.connect(ThingSpeakServer,ThingSpeakPort)) {
			Serial.println("connected");  
			
			 
			 

			  /*
			  sendRequest(ThingSpeakServer);
			  // Read all the lines of the reply from server and print them to Serial
			  Serial.println("Respond:");
			  while(client.available()){
				String line = client.readStringUntil('\r');
				Serial.print(line);
			  }
			  */

  
			
			if (sendRequest(ThingSpeakServer) && skipResponseHeaders() && skipThingSpeakHeader() ) {
			  char response[MAX_CONTENT_SIZE];
			   delay(2000);
			  readReponseContent(response, sizeof(response));
			  UserData userData;
			  if (parseUserData(response, &userData)) {
				printUserData(&userData);
			  }
			}
			
		}
		else{
			Serial.println("ThingSpeakServer connection error");
		}
	  
		// close the connection:
		//WiFi.disconnectFromServer();
		delay(2000); 

}




 /**********************************************************/
 /****            JSON FUNCTIONS                        ****/
 /**********************************************************/




// Send the HTTP GET request to the server
//---------------------------------------- 
bool sendRequest(const char* host) {
//---------------------------------------- 
  
  Serial.println("Is there anybody out there?");
  //Serial.println(resource);
  

  client.print("GET ");
  
  client.print(resource1);
  client.print(channelID);
  client.print(resource2);
  client.println(" HTTP/1.1");
  client.print("Host: ");
  client.println(host);
  client.println("Connection: close");
  client.println();

  return true;
}




// Skip HTTP headers so that we are at the beginning of the response's body
//---------------------------------------- 
bool skipResponseHeaders() {
//---------------------------------------- 
  // HTTP headers end with an empty line
  char endOfHeaders[] = "\r\n\r\n";

  client.setTimeout(HTTP_TIMEOUT);
  bool ok = client.find(endOfHeaders);

  if (!ok) {
    Serial.println("No response or invalid response!");
	
	//Twist for a while to show we found something wrong
	   for (int i=0; i <= 10; i++){
			//  Red	 Band
			R=255;
			G=0;
			B=0;	
			writeLEDS(R, G, B);   
			delay (50);
			//  White	 Band
			R=0;
			G=0;
			B=0;	
			writeLEDS(R, G, B);   
			delay (50);		
	   } 
			//  Red	 Band
			R=255;
			G=0;
			B=0;	
			writeLEDS(R, G, B);   	
  }

  return ok;
}

// Skip HTTP headers so that we are at the beginning of the response's body
//-------------------------------------------------------
bool skipThingSpeakHeader() {
//-------------------------------------------------------

#ifdef SKIP_TP_HEADER
  // Flush first line to go to align to json body
  char endOfHeaders[] = "\r\n";

  client.setTimeout(HTTP_TIMEOUT);
  bool ok = client.find(endOfHeaders);

  if (!ok) {
    Serial.println("ThingSpeak Header not found :-(");
	//Twist for a while to show we found something wrong
	   for (int i=0; i <= 10; i++){
			//  Red	 Band
			R=255;
			G=0;
			B=0;	
			writeLEDS(R, G, B);   
			delay (50);
			//  White	 Band
			R=0;
			G=0;
			B=0;	
			writeLEDS(R, G, B);   
			delay (50);		
	   } 
			//  Red	 Band
			R=255;
			G=0;
			B=0;	
			writeLEDS(R, G, B);   	   
	   
  }
#else 
 bool ok=true;
#endif  


  return ok;
}



// Read the body of the response from the HTTP server
//-------------------------------------------------------
void readReponseContent(char* content, size_t maxSize) {
//-------------------------------------------------------
  size_t length = client.readBytes(content, maxSize);
  content[length] = 0;
  Serial.println(content);
}



// Parse the JSON from the input string and extract the interesting values
//-------------------------------------------------------
bool parseUserData(char* content, struct UserData* userData) {
//-------------------------------------------------------
  // Compute optimal size of the JSON buffer according to what we need to parse.
  // This is only required if you use StaticJsonBuffer.
  const size_t BUFFER_SIZE =
      JSON_OBJECT_SIZE(8)     // the root object has 8 elements
      + JSON_OBJECT_SIZE(5)   // the "address" object has 5 elements
      + JSON_OBJECT_SIZE(2)   // the "geo" object has 2 elements
      + JSON_OBJECT_SIZE(3);  // the "company" object has 3 elements

  // Allocate a temporary memory pool on the stack
  StaticJsonBuffer<BUFFER_SIZE> jsonBuffer;
  // If the memory pool is too big for the stack, use this instead:
  // DynamicJsonBuffer jsonBuffer;

  JsonObject& root = jsonBuffer.parseObject(content);

  if (!root.success()) {
    Serial.println("JSON parsing failed!");
    return false;
  }

  // Here were copy the strings we're interested in
  
  strcpy(userData->created_at, root["created_at"]);  
  userData->field1=atoi(root["field1"]);
  userData->field2=atoi(root["field2"]);
  userData->field3=atoi(root["field3"]);
  userData->field4=atoi(root["field4"]);
  userData->field5=atoi(root["field5"]);	
  userData->field6=atoi(root["field6"]);	
  userData->field7=atoi(root["field7"]);
  //strcpy(userData->field2, root["field2"]);
  //strcpy(userData->field2, root["company"]["name"]);
  // It's not mandatory to make a copy, you could just use the pointers
  // Since, they are pointing inside the "content" buffer, so you need to make
  // sure it's still in memory when you read the string

  return true;
}

// Print the data extracted from the JSON
//-------------------------------------------------------
void printUserData(const struct UserData* userData) {
//-------------------------------------------------------

  
  Serial.print("old timestamp: ");
  Serial.println(timestamp);
  
 
  Serial.print("created_at = ");
  Serial.println(userData->created_at);
  Serial.print("Field1 = ");
  Serial.println(userData->field1);
  Serial.print("Field2 = ");
  Serial.println(userData->field2);
  Serial.print("Field3 = ");
  Serial.println(userData->field3);  
  Serial.print("Field4 = ");
  Serial.println(userData->field4);
  Serial.print("Field5 = ");
  Serial.println(userData->field5);
  Serial.print("Field6 = ");
  Serial.println(userData->field6); 
  Serial.print("Field7 = ");
  Serial.println(userData->field7);      
  
  //Check If this is a new sample
  if (strcmp(timestamp,userData->created_at)  != 0)
  {
    Serial.println("Hey Dude! We have something new here, let's rock!");
	newSampleDetected=1;
	

		//Twist for a while to show we found something new!
		/*
		   for (int i=0; i <= 10; i++){
				//  White	 Band
				R=255;
				G=255;
				B=255;	
				writeLEDS(R, G, B);   
				delay (50);
				//  Black	 Band
				R=0;
				G=0;
				B=0;	
				writeLEDS(R, G, B);   
				delay (50);		
		   } 		
	*/
	
	R=userData->field1;
	G=userData->field2;
	B=userData->field3;	
	level=userData->field5;	
	badge=userData->field6;	
	//ColorCycle(5000);
	

	strcpy(timestamp,userData->created_at);
	
	
	//writeLEDS(47, 55, 118); //Don't know which color is it, but no matter :-p  
    //Sleep for a while to display the color
	//delay(3000);	
	
	
	//writeLEDS(R, G, B);
	//Breath();

	
	
  }
  else
  { 
    Serial.println("Ronpichhhhhhh");
	newSampleDetected=0;	


	
	//HeartBeat();
	//Breath();
	
  }  
  
 /*
 	Serial.println("Static Color: ");  
	writeLEDS(colors[0][0],colors[0][1],colors[0][2]);
	delay (2000);
	Serial.println("Pulse Color: ");  
	pulseOneColor();
	Serial.println("RainBow 2 colors: ");  
	rainbowColors(2,200);	
	Serial.println("RainBow 2 colors fast: ");  
	rainbowColors(2,50);
	Serial.println("theaterRainbow :-) ");  	
	theaterRainbow(50);
	*/
 
	switch (badge) {
		case 0:
		  writeLEDS(0,0,0);
		  break;
		case 1:
		  writeLEDS(colors[0][0],colors[0][1],colors[0][2]);
		  break;		  
		case 2:
		  pulseOneColor();
		  break;		  
		case 3:
		  rainbowColors(1,200);	
		  break;
		case 4:
		  theaterRainbow(50);
		  break;
		case 5:
		  rainbowColors(2,50);	
		  break;		  
		case 6:
		  rainbowColors(1,50);	
		  theaterRainbow(50);
		  rainbowColors(2,50);	
		  break;		  
		default: 
		  // if nothing else matches, do the default
		  // default is optional
		  rainbow(50);
		break;
	  }  
	  
}






 /**********************************************************/
 /****            NEOPIXEL FUNCTIONS                    ****/
 /**********************************************************/


//-------------------------------------------------------
void writeLEDS(byte R, byte G, byte B)//basic write colors to the neopixels with RGB values
//-------------------------------------------------------
{
  for (int i = 0; i < pixels.numPixels(); i ++)
  {
    pixels.setPixelColor(i, pixels.Color(R, G, B));
  }
  pixels.show();
}

//-------------------------------------------------------
void writeLEDS(byte R, byte G, byte B, byte bright)//same as above with brightness added
//-------------------------------------------------------
{
  float fR = (R / 255) * bright;
  float fG = (G / 255) * bright;
  float fB = (B / 255) * bright;
  for (int i = 0; i < pixels.numPixels(); i ++)
  {
    pixels.setPixelColor(i, pixels.Color(R, G, B));
  }
  pixels.show();
}




//Theatre-style crawling lights with rainbow effect
//-------------------------------------------------------
void theaterChaseRainbow(uint8_t wait) {
//-------------------------------------------------------
  for (int j=0; j < 128; j++) {     // cycle all 256 colors in the wheel
    for (int q=0; q < 3; q++) {
      for (int i=0; i < pixels.numPixels(); i=i+3) {
        pixels.setPixelColor(i+q, Wheel( (i+2*j) % 255));    //turn every third pixel on
      }
      pixels.show();

      delay(wait);

      for (int i=0; i < pixels.numPixels(); i=i+3) {
        pixels.setPixelColor(i+q, 0);        //turn every third pixel off
      }
    }
  }
}



//-------------------------------------------------------
void rainbow(uint8_t wait) {
//-------------------------------------------------------
  uint16_t i, j;

  for(j=0; j<256; j++) {
    for(i=0; i<pixels.numPixels(); i++) {
      pixels.setPixelColor(i, Wheel((i+j) & 255));
    }
    pixels.show();
    delay(wait);
  }
}



// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
//-------------------------------------------------------
uint32_t Wheel(byte WheelPos) {
//-------------------------------------------------------
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) {
    return pixels.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if(WheelPos < 170) {
    WheelPos -= 85;
    return pixels.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return pixels.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}






//-------------------------------------------------------
void theaterRainbow(int delayValue) {
//-------------------------------------------------------


	  Serial.println("Start theaterRainbow");
	  unsigned int done=0;
	  lastConnectionTime = millis();
	  pixels.setBrightness(255);
	while (done==0)
	{
		theaterChaseRainbow(delayValue);
		
		if (millis() - lastConnectionTime > postingInterval) {
		done=1;
		}			
	}
	Serial.println("Stop theaterRainbow");  


}



//-------------------------------------------------------
void rainbowColors(int numberOfColors ,int delayValue) {
//-------------------------------------------------------
 
	  Serial.println("Start rainbowColors");
	  unsigned int done=0;
	  lastConnectionTime = millis();
	  pixels.setBrightness(255);
	while (done==0)
	{
		//Downward
		Serial.println("Downward");
			//Color1->Color2 : increase B up to max value
			Serial.println("bstart:");
			Serial.println(colors[0][2]);

			for (int b=colors[0][2]; b<=255; b+=5) {  writeLEDS(255,0,b);pixels.show(); delay(delayValue); }
			//Now decrease  R down to min value
			for (int r=255; r>=0; r-=5) 
			{ 
			//Serial.print(r);Serial.println(";");
			writeLEDS(r,0,255);pixels.show(); delay(delayValue); 
			}
			//Now increase  G up to Color2 value
			Serial.println("gend:");
			Serial.println(colors[1][1]);
			for (int g=0; g<colors[1][1]; g+=5) {  writeLEDS(0,g,255);pixels.show(); delay(delayValue); }
			
			//Reverse now :-)
			Serial.println("Backward");
			//
			for (int g=colors[1][1]; g>=0; g-=5) {  writeLEDS(0,g,255);pixels.show(); delay(delayValue); }
			for (int r=0; r<=255; r+=5) { writeLEDS(r,0,255);pixels.show(); delay(delayValue); }			
			for (int b=255; b>=colors[0][2]; b-=5) {  writeLEDS(255,0,b);pixels.show(); delay(delayValue); }
			
			if (millis() - lastConnectionTime > postingInterval) {
			done=1;
			}			
	}
	Serial.println("Stop rainbowColors");  
 	
}


//-------------------------------------------------------
void pulseOneColor() {
//-------------------------------------------------------
	
	  Serial.println("Start pulseOneColor");
	  unsigned int done=0;
	  lastConnectionTime = millis();
	while (done==0)
	{

		for (int i=50; i<200; i++) { pixels.setBrightness(i); writeLEDS(colors[0][0],colors[0][1],colors[0][2]);pixels.show(); delay(15); }
		for (int i=200; i>50; i--) { pixels.setBrightness(i); writeLEDS(colors[0][0],colors[0][1],colors[0][2]);pixels.show(); delay(15); }
		if (millis() - lastConnectionTime > postingInterval) {
		done=1;
		}		
	}
	Serial.println("Stop pulseOneColor");  
	pixels.setBrightness(128);
	writeLEDS(R,G,B);
	pixels.show();              
	delay(3);  
}

//-------------------------------------------------------
uint32_t Breath() {
//-------------------------------------------------------

	  Serial.println("Start Breath");
	  unsigned int done=0;
	  lastConnectionTime = millis();
	while (done==0)
	{

		for (int i=50; i<200; i++) { pixels.setBrightness(i); writeLEDS(R,G,B);pixels.show(); delay(30); }
		for (int i=200; i>50; i--) { pixels.setBrightness(i); writeLEDS(R,G,B);pixels.show(); delay(30); }
		if (millis() - lastConnectionTime > postingInterval) {
		done=1;
		}		
	}
	Serial.println("Stop Breath");  
	pixels.setBrightness(128);
	writeLEDS(R,G,B);
	pixels.show();              
	delay(3);  
}



//-------------------------------------------------------
uint32_t ColorCycle(int duration) {
//-------------------------------------------------------

  Serial.println("Start ColorCycle");
  unsigned int done=0;
  lastConnectionTime = millis();

  while (done==0)
  {
		Serial.println("Level:");
		Serial.println(level);
		for (int i =0; i < 2+level; i++) {
			/*
			Serial.println("Selected Color");
			Serial.println(colors[i][0]);
			Serial.println(colors[i][1]);
			Serial.println(colors[i][2]);
			*/
			writeLEDS(colors[i][0],colors[i][1],colors[i][2]);
			pixels.show();              
			//delay(3000);  
			//for (int j=50; j<200; j=j+10) { pixels.setBrightness(j); writeLEDS(colors[i][0],colors[i][1],colors[i][2]);pixels.show(); delay(30); }
			delay(1300); 
			//for (int j=200; j>50; j--) { pixels.setBrightness(j); writeLEDS(colors[i][0],colors[i][1],colors[i][2]);pixels.show(); delay(30); }				
		}
		delay(3000); 

		
	  	  
		if (millis() - lastConnectionTime > duration) {
		done=1;
		}
  }
  
  
	Serial.println("Stop ColorCycle");  
	 pixels.setBrightness(255);
	 pixels.show();              
	 delay(3);  
}



//-------------------------------------------------------
uint32_t HeartBeat() {
//-------------------------------------------------------

  Serial.println("Start HeartBeat");
  unsigned int done=0;
  lastConnectionTime = millis();

  while (done==0)
  {


	   int x = 3;
	   for (int ii = 1 ; ii <252 ; ii = ii = ii + x){
		 pixels.setBrightness(ii);
		 pixels.show();              
		 delay(5);
		}
		
		x = 3;
	   for (int ii = 252 ; ii > 3 ; ii = ii - x){
		 pixels.setBrightness(ii);
		 pixels.show();              
		 delay(3);
		 }
	   delay(10);
	   
	   x = 6;
	  for (int ii = 1 ; ii <255 ; ii = ii = ii + x){
		 pixels.setBrightness(ii);
		 pixels.show();              
		 delay(2);  
		 }
	   x = 6;
	   for (int ii = 255 ; ii > 1 ; ii = ii - x){
		 pixels.setBrightness(ii);
		 pixels.show();              
		 delay(3);
	   }
	  delay (50);  

	  	  
		if (millis() - lastConnectionTime > postingInterval) {
		done=1;
		}
  }
  
  
	Serial.println("Stop HeartBeat");  
	 pixels.setBrightness(255);
	 pixels.show();              
	 delay(3);  
}




