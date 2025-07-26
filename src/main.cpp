#include <WiFi.h>
#include "time.h"
#include "TM1637Display.h"
#include "ThingSpeak.h"
#include "DHT.h"

//TM1637 pins
#define CLK 5
#define DIO 4

#define LED_WIFI 2
//#define DHTTYPE DHT11   // DHT 11
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
//#define DHTTYPE DHT21   // DHT 21 (AM2301)

#define DHTPIN 13     // Digital pin connected to the DHT sensor
// Feather HUZZAH ESP8266 note: use pins 3, 4, 5, 12, 13 or 14 --
// Pin 15 can work but DHT must be disconnected during program upload.

DHT dht(DHTPIN, DHTTYPE);

TM1637Display display(CLK, DIO);

//-----ThingSpeak channel details

unsigned long myChannelNumber = 1641851;

const char * myWriteAPIKey = "8M6KTQPW4V1CSKAO";

WiFiClient  client;
//WiFi.begin("Wokwi-GUEST", "", 6);
const char *ssid = "Wokwi-GUEST";
const char *password = "tech@12345";

const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -10800 ; // IST (GMT+3 para cordoba)
const int daylightOffset_sec = 0;

struct data_time{  
    int hour = 0;
    int minute = 0;
    int c_ask_time=0;
    int c_ask_wheather=0;
    int error_asking_time = 0;
};

data_time TIMING;

struct sensor_data{  
    //valores del sensor
    float humidity = 0;
    float temp_c = 0;
    float temp_f =0;
    //salen por calculo 
    float sens_term_hif=0; //sensacion termica en ºF
    float sens_term_hic=0; //sensacion termica en ºC    
};

sensor_data DHT_SENSOR;

void connectWiFi() {
  Serial.print("Connecting to ");
  Serial.println(ssid);
  //WiFi.begin(ssid, password);
  WiFi.begin("Wokwi-GUEST", "", 6);
  while (WiFi.status() != WL_CONNECTED) 
  {
    Serial.print(".");
    for (int i = 0; i < 4; i++) 
    {
      digitalWrite(LED_WIFI, HIGH);
      display.showNumberDec(0, false, 1, i);
      delay(150);
      digitalWrite(LED_WIFI, LOW);
      display.clear();
    }
  }
  Serial.println("\nWiFi connected.");
  display.showNumberDec(0, true);
  digitalWrite(LED_WIFI, HIGH);
}

void printLocalTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    //return 0;//si quiero agregar un retorno con o sin error quitr el void
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  Serial.print("Hour (12 hour format): ");
  Serial.println(&timeinfo, "%I");
  Serial.print("AM/PM: ");
  Serial.println(timeinfo.tm_hour >= 12 ? "PM" : "AM");
  Serial.println();
  
}

void setup() {
  pinMode(LED_WIFI, OUTPUT);
  digitalWrite(LED_WIFI, LOW);

  Serial.begin(115200);
  display.setBrightness(0x0f);

  // Connect to Wi-Fi
  connectWiFi();

  // Initialize NTP
  Serial.print("Initializing NTP..");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.print("Local Time Printing..");
  printLocalTime();

  Serial.println(F("DHTxx test!"));
  dht.begin();

  ThingSpeak.begin(client); // Initialize ThingSpeak
}

int getTime(int &hour, int &minute){
    //static int lastHour = 12, lastMinute = 0; // Default to 12:00
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)){
      Serial.println("Failed to obtain time");
      display.showNumberDec(8888, false);
      delay(100);
      return 1;//ERROR
    }else{
      // Update time
      hour = timeinfo.tm_hour;  // 24 hour
      //int hour = timeinfo.tm_hour % 12 == 0 ? 12 : timeinfo.tm_hour % 12;  // 12 hour 
      minute = timeinfo.tm_min;
      //lastHour = TIMING_DATA.hour;
      //lastMinute = TIMING_DATA.minute;
      Serial.print(hour);
      Serial.print(F(":"));
      Serial.print(minute);
      Serial.println();
    }    
    return 0;//no error 
}

void blinkColon(int hour, int minute, bool showColon){
  // Display hour with colon (if showColon is true) and leading zeros
  display.showNumberDecEx(hour, showColon ? 0b01000000 : 0, true, 2, 0);
  // Display minute with leading zeros
  display.showNumberDecEx(minute, 0, true, 2, 2);
}

int sensor(){
  // Wait a few seconds between measurements.
  //delay(2000);
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float h = dht.readHumidity();   
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();  
  // Read temperature as Fahrenheit (isFahrenheit = true)
  float f = dht.readTemperature(true);
  
  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t) || isnan(f)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    return 1;
  }

  // Compute heat index in Fahrenheit (the default)
  float hif = dht.computeHeatIndex(f, h);
  // Compute heat index in Celsius (isFahreheit = false)
  float hic = dht.computeHeatIndex(t, h, false);

  DHT_SENSOR.humidity = h;
  DHT_SENSOR.temp_c = t;
  DHT_SENSOR.temp_f = f;
  DHT_SENSOR.sens_term_hif = hif;
  DHT_SENSOR.sens_term_hic = hic;


  Serial.print(F("Humidity: "));
  Serial.print(h);
  Serial.print(F("%  Temperature: "));
  Serial.print(t);
  Serial.print(F("°C "));
  Serial.print(f);
  Serial.print(F("°F  Heat index: "));
  Serial.print(hic);
  Serial.print(F("°C "));
  Serial.print(hif);
  Serial.println(F("°F"));

  return 0;
}
int updte_sensor(){
  ThingSpeak.setField(1, DHT_SENSOR.temp_c);
  ThingSpeak.setField(2, DHT_SENSOR.temp_f);
  ThingSpeak.setField(3, DHT_SENSOR.humidity);
  ThingSpeak.setField(4, DHT_SENSOR.sens_term_hic);
  ThingSpeak.setField(5, DHT_SENSOR.sens_term_hif);

  // Write to ThingSpeak. There are up to 8 fields in a channel, allowing you to store up to 8 different

  // pieces of information in a channel. Here, we write to field 1.

  int x = ThingSpeak.writeFields(myChannelNumber,myWriteAPIKey);

    if(x == 200){
    Serial.println("Channel update successful.");
    }
    else{
      Serial.println("Problem updating channel. HTTP error code " + String(x));
    }

}
void loop() {
  // Check Wi-Fi connection
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected. Reconnecting...");
    digitalWrite(LED_WIFI, LOW);
    connectWiFi();
  }

//si pasaron 2 seg pedir hora
  if(TIMING.c_ask_time<4){          
    TIMING.error_asking_time = getTime(TIMING.hour, TIMING.minute);        
    TIMING.c_ask_time = 0;        
  }
//si pasaron 10 min pedir datos
  if(TIMING.c_ask_wheather>1200){        
    //if no error about sensor reading, write data over thingspeak
    if(sensor() == 0){
      updte_sensor();     
    }
    TIMING.c_ask_wheather=0;
  }
  //if no error in asking time, refresh display
  if(TIMING.error_asking_time == 0){
    // Blink colon
    static bool colonState = false;
    blinkColon(TIMING.hour, TIMING.minute, colonState);
    colonState = !colonState;
  }
  
  delay(500);

  TIMING.c_ask_time++;
  TIMING.c_ask_wheather++;

}