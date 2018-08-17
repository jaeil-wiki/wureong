#include <ESP8266WiFi.h>
#include <WiFiClient.h>

#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

MDNSResponder mdns;
ESP8266WebServer server(80);
/*  GPIO  */
int BTN_GPIO = 0;
int RELAY_GPIO = 12;
int LED_GPIO = 13;

/*  STATUS VARIABLES  */
int led_stat = HIGH; // default off
int relay_stat = LOW; // default off

int health_chk_cnt = 0;
String resp;

/*  CONFIGS  */
// Turn on the relay When Power is supplied.
boolean TURN_ON_RELAY_WHEN_POWRED_ON = false;
int HEALTH_CHK_FREQ = 10; // get request to 
// Wifi Credentials
const char* WIFI_SSID = "lptime";
const char* WIFI_PASS = "tpwhdeotod1!@#";

const char* HUB_URL = "http://10.11.1.9:8000/ping/";

/* FUNCTION DEFINE */
void blink_led(int freq_on=250, int times=1, int freq_off=-1){
  int i;
  for(i=0;i<times;i++){
    digitalWrite(LED_GPIO, LOW);
    delay(freq_off > 0 ? freq_off:freq_on);
    digitalWrite(LED_GPIO, HIGH);
    delay(freq_on);
  }
  digitalWrite(LED_GPIO, led_stat);
}

void set_led(int flag){
  led_stat = flag ? LOW:HIGH;
  digitalWrite(LED_GPIO, led_stat);
}

void set_relay(int flag){
  relay_stat = flag ? HIGH:LOW;
  digitalWrite(RELAY_GPIO, relay_stat);
}

char* get_all_stat(){
  char buf[1024];
  sprintf(buf, "{\"STAT\": \"FACTORY\", \"LED\": %s, \"RELAY\": %s}", (led_stat) ? "false":"true", (relay_stat) ? "true":"false");
  return buf;
}

/* SETUP */
void setup(void){
  
  // preparing GPIOs
  pinMode(BTN_GPIO, INPUT);
  pinMode(RELAY_GPIO, OUTPUT);
  pinMode(LED_GPIO, OUTPUT);

  set_led(TURN_ON_RELAY_WHEN_POWRED_ON ? HIGH:LOW);
  set_relay(TURN_ON_RELAY_WHEN_POWRED_ON ? HIGH:LOW);
 
  Serial.begin(115200); 
  blink_led(40, 37);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.println("");

  while (WiFi.status() != WL_CONNECTED) {
    blink_led(150, 1, 850);
    Serial.print(".");
  }
  blink_led(250, 3);
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(WIFI_SSID);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
  if (mdns.begin("esp8266", WiFi.localIP())) {
    Serial.println("MDNS responder started");
  }

  server.on("/", [](){
    server.send(200, "text/html", get_all_stat());
  });
  
  server.on("/on", [](){
    blink_led(700,1); set_relay(HIGH);
    server.send(200, "text/html", get_all_stat());
  });
  server.on("/off", [](){
    blink_led(700,1); set_relay(LOW);
    server.send(200, "text/html", get_all_stat());
  });
  server.begin();
  Serial.println("HTTP server started");
}
 
void loop(void){
  health_chk_cnt++;
  if(health_chk_cnt > HEALTH_CHK_FREQ*3){
    Serial.print("Health Check  ");
    Serial.print(health_chk_cnt);
    Serial.print("/");
    Serial.println(HEALTH_CHK_FREQ);
    HTTPClient http;
    http.setTimeout(3000);
    http.begin(HUB_URL);
    int httpCode = http.GET();  //Send the request
    if (httpCode > 0) { // check HTTP Response Code
      String payload = http.getString();
      Serial.println(payload);
      health_chk_cnt=0;
      blink_led(10, 3);
    }
    else{
      blink_led(180,4);
    }
    http.end();   //Close connection
  }
  server.handleClient();
  delay(333);
} 
