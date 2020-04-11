#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>

#include <ESPMiio.h>
#include <ESPVacuum.h>

#define AP_SSID "ssid"
#define AP_PASSWORD "password"

const char *ssid = AP_SSID;
const char *pass = AP_PASSWORD;

IPAddress ip(192, 168, 1, 20);     //Node static IP
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);


#define MIROBO_TOKEN "mysecrettoken"

IPAddress mirobo_ip(192, 168, 1, 100);
MiioDevice mirobo(&mirobo_ip, MIROBO_TOKEN, 2000);


void setup() {
  Serial.begin(115200);
  Serial.println("Booting");

  WiFi.mode(WIFI_STA);

  WiFi.begin(ssid, pass);
  WiFi.config(ip, gateway, subnet);

  //Wifi connection
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(1000);
    ESP.restart();
  }

  Serial.println("Connected");

    connect();
    
   //WAIT WHILE CONNECT:
   long start_connect_time = millis();
   while (mirobo.isBusy()){
    delay(50);
    if (millis() - start_connect_time > 5000){
         Serial.println("Couldn't connect to MiIO device! Rebooting...");
         ESP.restart();
      }
   }

    if (mirobo.isConnected()){
      //led_red_ok();
      Serial.printf("Connected to device");

       //SEND app_start command
       mirobo.send("app_start", NULL,  [](MiioError e){
            //beep_error();
            Serial.printf("app_start error: %d\n", e);
      });
    
    }
}

bool connect(){
  if (!mirobo.isConnected()){
  //CONNECT TO MiIO DEVICE
   mirobo.connect([](MiioError e){
    //HANDLE CONNECT ERRORS HERE
    //led_red_error_1();
        Serial.printf("connecting error: %d\n", e);
   });
  }
   return mirobo.isConnected();
}

//check every 100 millis
#define DELTA_CHECK_TIME 100

long check_time = 0;

void loop() {
  long t = millis();
  if (t-check_time > DELTA_CHECK_TIME){
    check_time = t;
    if (connect()){
    //check mirobo status
    if (!mirobo.send("get_status", [](MiioResponse response){
          //HANDLE RESPONSE
          if (!response.getResult().isNull()){
            JsonVariant state = response.getResult()["state"];
            if (!state.isNull()){
              //led_red_ok();
              int s = state.as<int>();
              Serial.printf("state=%d\n", s);
            }
          }else{
            if (!response.getError().isNull()){
              //led_red_error_2();
              mirobo.disconnect();
              Serial.println("response error");
            }
          }
        }, [](MiioError e){
          //led_red_error_2();
          mirobo.disconnect();
          Serial.printf("get_status error: %d\n", e);
        }
    )){
      //led_red_error_2();
      mirobo.disconnect();
      Serial.printf("PT send get_status error");
    }
  }    
  }
}
