#include <EEPROM.h>
#include <ESP8266WiFi.h>

#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

#define EEPROM_SIZE 512
#define EEPROM_CONFIG_BASE_OFFSET 32
#define SIZE_WIFI_SSID 32
#define SIZE_WIFI_PASS 32
#define SIZE_HOST_IP 15

#define MODE_IDLE 0
#define MODE_FACTORY 1
#define MODE_WUREONG 2

MDNSResponder mdns;
ESP8266WebServer server(80);

/*  GPIO  */
const int BTN_GPIO = 0;
const int RELAY_GPIO = 12;
const int LED_GPIO = 13;

/* EEPROM MAPPING */
const int OFFSET_MODE_FLAG = EEPROM_CONFIG_BASE_OFFSET-1;
const int OFFSET_WIFI_SSID = EEPROM_CONFIG_BASE_OFFSET;
const int OFFSET_WIFI_PASS = OFFSET_WIFI_SSID+SIZE_WIFI_SSID;

/*  STATUS VARIABLES  */
int led_stat = HIGH; // default off
int relay_stat = LOW; // default off
int current_mode;

int health_chk_cnt = 0;
String resp;

/*  CONFIGS  */
const boolean TURN_ON_RELAY_WHEN_POWRED_ON = false;     // Turn on the relay When Power is supplied.
const int HEALTH_CHK_FREQ = 0;                         // Health check request every %HEALTH_CHK_FREQ% seconds. 0 is disabled.

char WIFI_SSID[SIZE_WIFI_SSID] = ""; // Wifi Credentials
char WIFI_PASS[SIZE_WIFI_PASS] = ""; // Wifi Credentials
char HOST_IP[SIZE_HOST_IP] = "";

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

/* EEPROM */
void reset_config(){
  int i;
  set_led(HIGH);
  for(i=EEPROM_CONFIG_BASE_OFFSET;i<EEPROM_SIZE;i++) EEPROM.write(i, 0);
  EEPROM.write(OFFSET_MODE_FLAG, MODE_FACTORY);
  EEPROM.commit();
  EEPROM.end();
  delay(3000);
  blink_led(100, 5);
  set_led(LOW);
  ESP.restart();
}
void save_config(){
  int i, addr=EEPROM_CONFIG_BASE_OFFSET;
  set_led(HIGH);
  addr = OFFSET_WIFI_SSID;
  for(i=0;i<SIZE_WIFI_SSID;i++) EEPROM.write(addr++, WIFI_SSID[i]); // read wifi_ssid(32)
  addr = OFFSET_WIFI_PASS;
  for(i=0;i<SIZE_WIFI_SSID;i++) EEPROM.write(addr++, WIFI_PASS[i]); //read wifi_pass(32)
  EEPROM.write(OFFSET_MODE_FLAG, MODE_WUREONG);
  EEPROM.commit();
  EEPROM.end();
  delay(3000);
  blink_led(250, 3); // 2 seconds
  set_led(LOW);
  ESP.restart();
}

void load_config(){
  int i, addr=EEPROM_CONFIG_BASE_OFFSET;
  for(i=0;i<SIZE_WIFI_SSID;i++) WIFI_SSID[i]=EEPROM.read(addr++); // read wifi_ssid(32)
  for(i=0;i<SIZE_WIFI_SSID;i++) WIFI_PASS[i]=EEPROM.read(addr++); //read wifi_pass(32)
}

/* WEB UTIL */ 

char* get_all_stat(){
  char buf[1024]={0,};
  if(current_mode == MODE_FACTORY) sprintf(buf, "{\"STAT\": %d, \"LED\": %d, \"RELAY\": %d, \"WIFI_SSID\": %s, \"WIFI_PASS\": %s,}", current_mode ,led_stat, relay_stat, WIFI_SSID, WIFI_PASS);
  else sprintf(buf, "{\"STAT\": %d, \"LED\": %d, \"RELAY\": %d}", current_mode ,led_stat, relay_stat);
  return buf;
}

int get_mode() {
  return EEPROM.read(OFFSET_MODE_FLAG);
}


boolean btn_trigger(){
  unsigned int hold_cnt = 0;
  unsigned int i;
  for(i=0;i<500;i++){
    if(!digitalRead(BTN_GPIO)) hold_cnt++;
    else break;
    delay(10);
  }
  if(hold_cnt) Serial.println(hold_cnt);
  if (hold_cnt == 500){
    blink_led(40, 62); // 5 seconds
    reset_config();
    return 0;
  }
  else if(hold_cnt >= 100){
    blink_led(1000, 1);
    save_config();
    return 0;
  }
}

void api_stat(){
  server.send(200, "application/json", get_all_stat());
}

void api_relay_on(){
  blink_led(700,1);
  set_relay(HIGH);
  server.send(200, "application/json", get_all_stat());
}

void api_relay_off(){
  blink_led(700,1);
  set_relay(LOW);
  server.send(200, "application/json", get_all_stat());
}

void api_config(){
  if(current_mode != MODE_FACTORY) server.send(400);
  for(int i=0;i<server.args();i++){
    if(server.argName(i) == "wifi_ssid") server.arg(i).toCharArray(WIFI_SSID, SIZE_WIFI_SSID);
    else if(server.argName(i) == "wifi_pw") server.arg(i).toCharArray(WIFI_PASS, SIZE_WIFI_PASS);
    else if(server.argName(i) == "host_ip") server.arg(i).toCharArray(HOST_IP, SIZE_HOST_IP);
  }
  server.send(200, "application/json", get_all_stat());
}

void api_save_config(){
  save_config();
  server.send(200, "application/json", get_all_stat());
}

/* SETUP */
void setup(void){
  // preparing EEPROM
  EEPROM.begin(EEPROM_SIZE);
  
  // preparing GPIOs
  pinMode(BTN_GPIO, INPUT);
  pinMode(RELAY_GPIO, OUTPUT);
  pinMode(LED_GPIO, OUTPUT);

  Serial.begin(115200); 
  current_mode = EEPROM.read(OFFSET_MODE_FLAG);
  blink_led(40, 37); // 3 seconds

  set_led(TURN_ON_RELAY_WHEN_POWRED_ON ? HIGH:LOW);
  set_relay(TURN_ON_RELAY_WHEN_POWRED_ON ? HIGH:LOW);


  Serial.print("MODE: ");
  Serial.println(current_mode);
  if(current_mode == MODE_FACTORY){
    WiFi.softAP("WUREONG", "12341234");  //Start HOTspot removing password will disable security
    Serial.print("Server IP address: ");
    Serial.println(WiFi.softAPIP());
  
    blink_led(250, 3); // 1.5 seconds    
    server.on("/", api_stat);
    server.on("/config", api_config);
    server.on("/save", api_save_config);
    server.begin();
   
    Serial.println("HTTP server started");
  }
  else if(current_mode == MODE_WUREONG){
    load_config();
    
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.println("");
  
    while (WiFi.status() != WL_CONNECTED) {
      btn_trigger();
      blink_led(150, 1, 850);
      Serial.print(".");
    }
    blink_led(250, 3); // 1.5 seconds
    Serial.println("Connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    
    if (mdns.begin("esp8266", WiFi.localIP())) Serial.println("MDNS responder started");
  
    server.on("/", api_stat);
    server.on("/on", api_relay_on);
    server.on("/off", api_relay_off);
    server.begin();
    Serial.println("HTTP server started");
  }
}

void loop(void){
  btn_trigger();
  if(current_mode == MODE_FACTORY) {
    blink_led(100, 1);
    server.handleClient();
  }
  else if(current_mode == MODE_WUREONG) {
    health_chk_cnt++;
    if(HEALTH_CHK_FREQ){
      if(health_chk_cnt > HEALTH_CHK_FREQ*3){
        Serial.print("Health Check  ");
        Serial.print(health_chk_cnt);
        Serial.print("/");
        Serial.println(HEALTH_CHK_FREQ);
        HTTPClient http;
        http.setTimeout(3000);
        http.begin(HOST_IP);
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
    }
  server.handleClient();
  delay(300);
  }
  delay(33);
} 
