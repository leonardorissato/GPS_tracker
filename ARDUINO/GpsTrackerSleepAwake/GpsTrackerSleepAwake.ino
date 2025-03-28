/* TODO
- I'm sending battery levels on spd to supabase

- change gps mode to #3 , add galileo?

- save battery on while runretrieveloop

- get charging status and update interface? charging status can be ignition

- understand what this does
---- rtc_gpio_pullup_dis(WAKEUP_GPIO);
---- rtc_gpio_pulldown_en(WAKEUP_GPIO);

- check network health by pinging some website?

- try lbs based location when GPS not available?



- maybe broadcast a wifi network with debug details?

*/


#include <Preferences.h>
#include "utilities.h"
#include <TinyGsmClient.h>
#include <ArduinoJson.h> 
#include <esp32-hal-adc.h>
#include <Arduino.h>
#include "driver/rtc_io.h"
#include <driver/gpio.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include "WiFi.h"

// FOR BATTERY CALCULATION
#include <vector>
#include <algorithm>
#include <numeric>

class GPSModule{
  public:
    float lat      = 0;
    float lon      = 0;
    float speed    = 0;
    float alt     = 0;
    int   vsat     = 0;
    int   usat     = 0;
    float accuracy = 0;
    int   year     = 0;
    int   month    = 0;
    int   day      = 0;
    int   hour     = 0;
    int   min     = 0;
    int   sec     = 0;
    uint8_t    fixMode   = 0;
    bool working = false;
    int rebootcounter = 0;
    int samepositioncounter = 0;
    int failedattemptscounter=0;
    int getGPSTimeout = 90000;



};  // note semicolon ends declaration

class LTENetwork{
  public:
      String NETWORK_APN = "sxzcat1";
    // Supabase API URL and Key
    const char *locationDB_URL = "https://pllwfhcjryzuqsprjdqs.supabase.co/rest/v1/locationHistory";
    const char *bridgeDB_URL = "https://pllwfhcjryzuqsprjdqs.supabase.co/rest/v1/bridgeDB?select=*";
    const char* apiKey = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6InBsbHdmaGNqcnl6dXFzcHJqZHFzIiwicm9sZSI6ImFub24iLCJpYXQiOjE3MzA3OTA5NTUsImV4cCI6MjA0NjM2Njk1NX0.-tO3afiuTGl_CHUPSvhdOq0Na1sY5SpeENhplWg5r9U"; // Replace with your Supabase API key

    String localtime = "";
    bool working = false;
    int rebootcounter = 0;








};  // note semicolon ends declaration


// random important variables
RTC_DATA_ATTR int bootCount = 0;
#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
int timeDeepSleep = 1200;        // 20 MINUTES NORMALLY
int timeLightSleep = 20;        // 30 seconds
#define VOLTAGE_LOW_LEVEL       3700            //battery is low, update less frequently, still have vibration wakeup
#define VOLTAGE_SUPERLOW_LEVEL  3450            //battery is superlow, update less frequently, disable vibration wakeup
#define VOLTAGE_SHUTDOWN_LEVEL  3200            //shutdown, disable vibration wakeup and sleep for 3h
#define CHARGINGSTATUSPIN       39              // High means it's charging, low means it's not, there's a 1Mohm resistor between USBVbus and pin 39
bool enablevibrationwakeup = true;
bool ignitionOn = false;





// classes and modules instances
GPSModule gps;  // construct an instance of the gps class
LTENetwork network; // construct an instance of the network class
Preferences preferences; // deal with persistent data
TinyGsm modem(SerialAT);
TinyGsmClient client(modem);


// main variables
bool runRetrieveLoop;
#define numberOfSamePosBeforeSleep 2
#define WAKEUP_GPIO              GPIO_NUM_34    
uint32_t battery_voltage;
#define logfilename "/logfile.txt"

// debug
bool overridesamepositioncounter = false; // act like we're in a different position, it will sleep anyways after consecutive positions
bool overridesamewifilocation = false;
bool serialDebug = true;
bool sdCardDebug = false;




// ========================== MAIN STUFF =====================================
void setup() {
  pinMode(CHARGINGSTATUSPIN, INPUT);

 

  // for serialDebug
  if(serialDebug){
    Serial.begin(115200);
    debugLn("****************************");
  }


  startSD();
  
 
  debugLn(" --- STARTED SETUP");

  //Create a namespace called "memory"
  preferences.begin("memory", false);

  runRetrieveLoop = true;

  wakeup();


  
}

void wakeup(){
  // Right after the board wakes up
  // triggers are timer, vibration sensor, and ignition
  debugLn("-- waking up");


  int wkreason = getWakeupReason(); // vibration is #1
  getBatteryVoltage();
  int samewifisleeps = preferences.getInt("samewifisleeps",0);
  debugLn("SameWifiSleeps: " + String(samewifisleeps));


  if(!overridesamewifilocation){
    // check if we are in the same wifi location before proceeding
    // don't ignore vibration for more than 5 times
    // override if necessary
    if(wkreason == 1 && isSameWifiLocation() && samewifisleeps<5 ){
        debugLn("we are in the same location as before bed, enter deep sleep");
        preferences.putInt("samewifisleeps",samewifisleeps+1);
        deepSleep();
    }else{
      debugLn("Different Wifi Location - keep running");
    }
  }

  preferences.putInt("samewifisleeps",0);

  // if ignition is on, don't change any sleep settings
  if(!ignitionOn){
    // if it's low battery, update every 30 minutes
    if(battery_voltage < VOLTAGE_LOW_LEVEL){
      timeDeepSleep = 2400; // 40 minutes
    }

    if(battery_voltage < VOLTAGE_SUPERLOW_LEVEL){
      timeDeepSleep = 3600; // 1 hour
      enablevibrationwakeup = false;
    }

    // if battery is dead, don't run at all
    if(battery_voltage < VOLTAGE_SHUTDOWN_LEVEL){
      runRetrieveLoop = false;
      timeDeepSleep = 4800; // 3 hours
      enablevibrationwakeup = false;
      deepSleep();
      return;
    }
  }


  // warmup cycle
  startModem();
  startNetwork();
  warmUpGPS();



  




  while(runRetrieveLoop){
    GPSRetrieveLoop();
    
    // save some battery while waiting for a few secs
    lightSleep();

  }


  // make it deep sleep
  deepSleep();


}

void GPSRetrieveLoop(){
  debugLn("------------------ start gpsretrieve loop --------------------");

  checkNetworkHealth();
  getBatteryVoltage();
  

  // // if gps module not working
  // if(!gps.working){
  //   if(gps.rebootcounter < 5){
  //     gps.rebootcounter += 1;
  //     debug("trying to reset gps - #"); debugLn(String(gps.rebootcounter));
  //     warmUpGPS();
  //   }else{
  //     // restart the whole board
  //     debug("gps reboot counter too big");
  //     restartBoard();
  //   }
  // }

  if(!network.working){
    if(network.rebootcounter < 5){
      network.rebootcounter += 1;
      debug("trying to reset network");
      startNetwork();
    }else{
      // restart the whole board
      debug("network reboot counter too big");
      restartBoard();
    }
  }

  // get location and check against last known
  if(gps.working){
    gps.rebootcounter = 0;

    debugLn("GPS working");
    getLocation();
    // check if we are on the same position as before, We're only calculating based on seconds of lat/lon difference, not actually distance in metres
    float lastLatOnServer = preferences.getFloat("lat", 0);
    float lastLonOnServer = preferences.getFloat("lon", 0);

    float difflat = abs(gps.lat-lastLatOnServer)*10000;
    float difflon = abs(gps.lon-lastLonOnServer)*10000;

    debugLn("difference: lat=" + String(difflat)+"    lon="+ String(difflon));
  

  
    if(difflat <2 && difflon <2){
      gps.samepositioncounter += 1;
      debug("same position counter: "); debugLn(String(gps.samepositioncounter));
  
    }else{
      debugLn("different position");
      gps.samepositioncounter = 0;
      preferences.putFloat("lat", gps.lat);
      preferences.putFloat("lon", gps.lon);
    }
  }

  // update bridge database every iteration if network is working
  if(network.working){
    debugLn("--- Updating bridgeDB");
    network.rebootcounter = 0;

    // send battery level to database
    if(ignitionOn){
      updateBridgeDB("batterylevel","Charging");
    }else{
      updateBridgeDB("batterylevel",String(battery_voltage));
    }

    // get localtime and send it to database
    String GSMlocaltime = modem.getGSMDateTime(DATE_FULL);
    updateBridgeDB("lastupdate",GSMlocaltime);  
    debugLn("-----------------------------> time: " + GSMlocaltime);

    // send gps status to database
    if(gps.working){
      updateBridgeDB("gpsstatus","on"); 
    }else{
      updateBridgeDB("gpsstatus","off"); 
    }
  }


  // we have a different location, update on database
  if(gps.working && network.working && (gps.samepositioncounter == 0 || overridesamepositioncounter)){

    sendDataToLocationDB();


  }



  // if in the same position for long, turn off the loop, go back to sleep
  if(gps.samepositioncounter >= numberOfSamePosBeforeSleep ){
    runRetrieveLoop = false;
  }

  debugLn("------------------ end gpsretrieve loop --------------------");
}









// ========================== GPS =====================================
bool getLocation(){
  debugLn(" --- getting gps location");

  uint32_t getGPStimestart = millis();

  for (;;) {
    if (modem.getGPS(&gps.fixMode, &gps.lat, &gps.lon, &gps.speed, &gps.alt, &gps.vsat, &gps.usat, &gps.accuracy, &gps.year, &gps.month, &gps.day, &gps.hour, &gps.min, &gps.sec)) {
        gps.working = true; // good data
        debugLn("got location");
        return true;
    } else {
        gps.working=false;

        debug(".");
        delay(1000);
        if(millis() > getGPStimestart + gps.getGPSTimeout ){
          debugLn("GPS TIMED OUT");
          return false;
        }

    }
  }
  return false; // whatever went wrong
}

String sendATCommand(String command){
  SerialAT.println(command);


  while(!SerialAT.available()){
    delay(100);
  }


  String answer = "";
  char character;
  while (SerialAT.available()) {
    //Serial.write(SerialAT.read());
    character =  SerialAT.read();
    answer.concat(character);
    delay(10);
  }

  return answer;
}

void warmUpGPS(){
  unsigned long startGPStime = millis();

  debugLn(" --- warming gps up");
  // set to false, it will be set back to true on getLocation if it's working properly
  gps.working = false;




  for(int i=0;i<5;i++){
    debugLn("Enabling GPS - " + String(i));



    if(getLocation()){
      gps.working=true;
      debugLn("Warmup successfull - time =" + String((millis() - startGPStime)/1000) + " seconds" );
      return;
    } 
  }


  debugLn("Warmup error - time = " + String((millis() - startGPStime)/1000) + " seconds" );

}



// ========================== NETWORK =====================================
bool startNetwork(){
  debugLn(" --- Starting network");
  network.working = false;

  //Check if the modem is online
  int retry = 0;
  while (!modem.testAT(1000)) {
      debug(".");
      if (retry++ > 10) {
          // try to reset modem
          digitalWrite(BOARD_PWRKEY_PIN, LOW);
          delay(100);
          digitalWrite(BOARD_PWRKEY_PIN, HIGH);
          delay(1000);
          digitalWrite(BOARD_PWRKEY_PIN, LOW);
          retry = 0;
      }
  }
  debugLn("connected to modem");

  // Check if SIM card is online
  SimStatus sim = SIM_ERROR;
  while (sim != SIM_READY) {
      sim = modem.getSimStatus();
      switch (sim) {
      case SIM_READY:
          debugLn("SIM card online");
          break;
      case SIM_LOCKED:
          debugLn("The SIM card is locked. Please unlock the SIM card first.");
          break;
      default:
          break;
      }
      delay(1000);

  }



  #ifdef NETWORK_APN
      modem.sendAT(GF("+CGDCONT=1,\"IP\",\""), NETWORK_APN, "\"");
      if (modem.waitResponse() != 1) {
          debugLn("Set network apn error !");
      }
  #endif

  // Check network registration status and network signal status
  int16_t sq ;
  debugLn("Wait for the modem to register with the network.");
  RegStatus status = REG_NO_RESULT;
  while (status == REG_NO_RESULT || status == REG_SEARCHING || status == REG_UNREGISTERED) {
      status = modem.getRegistrationStatus();
      switch (status) {
      case REG_UNREGISTERED:
      case REG_SEARCHING:
          sq = modem.getSignalQuality();
          debug(" Signal Quality: "); debugLn(String(sq));
          delay(1000);
          break;
      case REG_DENIED:
          debugLn("Network registration was rejected, please check if the APN is correct");
          return false;
      case REG_OK_HOME:
          debugLn("Online registration successful");
          break;
      case REG_OK_ROAMING:
          debugLn("Network registration successful, currently in roaming mode");
          break;
      default:
          debugLn("Registration Status:%d");
          debugLn(String(status));
          delay(1000);
          break;
      }
  }
  debugLn(" ");


  debug("Registration Status: "); debugLn(String(status));
  delay(1000);

  String ueInfo;
  if (modem.getSystemInformation(ueInfo)) {
      debug("Inquiring UE system information:"); debugLn(ueInfo);
  }

  if (!modem.setNetworkActive()) {
      debugLn("Enable network failed!");
      network.working = false;
      return false;
  }

  delay(5000);

  String ipAddress = modem.getLocalIP();
  debug("Network IP:"); debugLn(ipAddress);

  checkNetworkHealth();
  return true;
}

void checkNetworkHealth(){
  debugLn(" --- Checking network health");
  // ping something and update working bool variable
  if(modem.isNetworkConnected()){
    debugLn(" -- connected");
    network.working = true;
  }else{
    debugLn(" -- disconnected");
    network.working = false;
  }
  

}




// ========================== DATABASE =====================================
bool sendDataToLocationDB() {
  debugLn(" --- Sending location data");

  // JSON HANDLING
  JsonDocument jsondoc;
  String serializedJson;

  // GET local TIME
  String GSMlocaltime = modem.getGSMDateTime(DATE_FULL);


  // create json data
  jsondoc["localtime"] = GSMlocaltime;
  jsondoc["spd"] = String(battery_voltage);
  jsondoc["lat"] = gps.lat;
  jsondoc["lon"] = gps.lon;
  jsondoc["acc"] = round(gps.accuracy);
  jsondoc["ign"] = ignitionOn;
  serializeJson(jsondoc, serializedJson);

  if(modem.setNetworkActive()){
    modem.https_end();
    delay(500);
    modem.https_begin();
    delay(500);

    if (!modem.https_set_url(network.locationDB_URL)) {
        debugLn("Failed to set the URL. Please check the validity of the URL!");
        return false;
        delay(6000);
    }


    delay(500);

    // fix error code 715 
    modem.sendAT("+CSSLCFG=\"enableSNI\",0,1");
    delay(500);


    modem.https_set_content_type("application/json");

    modem.https_add_header("apikey", String(network.apiKey));
    modem.https_add_header("Authorization", "Bearer " + String(network.apiKey));

    
    int httpCode = modem.https_post(serializedJson);
    
    if (httpCode != 200 && httpCode != 201) {
        debug("HTTP post failed ! error code = "); debugLn(String(httpCode)); 

        // Get HTTPS header information
        String header = modem.https_header();
        debug("HTTP Header : "); debugLn(String(header));

        // Get HTTPS response
        String body = modem.https_body();
        debug("HTTP body : "); debugLn(String(body));

        return false;
    }

    if(httpCode == 201){
      debugLn("Data sent to database successfully");
      return true;
    }

    



  }else{
    debugLn("ERROR - NETWORK NOT ENABLED");
    return false;
  }

}


bool updateBridgeDB(String variable,String value){
  //debug(" --- updating bridge db: "); debug(variable + " = ");  debugLn(value);


  if(modem.setNetworkActive()){
    modem.https_end();
    delay(500);
    modem.https_begin();
    delay(500);

    String link = "https://pllwfhcjryzuqsprjdqs.supabase.co/rest/v1/bridgeDB?variable=eq."+variable;
    if(!modem.https_set_url(link)) {
        debugLn("Failed to set the URL. Please check the validity of the URL!");
        return false;
    }



    modem.https_set_content_type("application/json");
    modem.https_add_header("apikey", String(network.apiKey));
    modem.https_add_header("Authorization", "Bearer " + String(network.apiKey));

    String data = "{\"value\": \""+String(value)+"\"}";

    // patch code added to tinygsm library
    if(false){
      /*
      
      int https_patch(uint8_t *payload, size_t size, uint32_t inputTimeout = 10000)
      {
          if (payload) {
              thisModem().sendAT("+HTTPDATA=", size, ",", inputTimeout);
              if (thisModem().waitResponse(30000UL, "DOWNLOAD") != 1) {
                  return -1;
              }
              thisModem().stream.write(payload, size);
              if (thisModem().waitResponse(30000UL) != 1) {
                  return -1;
              }
          }
          thisModem().sendAT("+HTTPACTION=5");
          if (thisModem().waitResponse(3000) != 1) {
              return -1;
          }
          if (thisModem().waitResponse(60000UL, "+HTTPACTION:") == 1) {
              int action = thisModem().streamGetIntBefore(',');
              int status = thisModem().streamGetIntBefore(',');
              int length = thisModem().streamGetIntBefore('\r');
              DBG("action:"); DBG(action);
              DBG("status:"); DBG(status);
              DBG("length:"); DBG(length);
              return status;
          }
          return -1;
      }

      int https_patch(const String &payload)
      {
          return https_patch((uint8_t *) payload.c_str(), payload.length());
      }
      */
    }


    int httpCode = modem.https_patch(data);
    
    if (httpCode != 200 && httpCode != 201 && httpCode != 204) {
        debug("HTTP post failed ! error code = "); debugLn(String(httpCode)); 


        // Get HTTPS header information
        String header = modem.https_header();
        debug("HTTP Header : ");debugLn(String(header));

        // Get HTTPS response
        String body = modem.https_body();
        debug("HTTP body : ");debugLn(String(body));

        return false;
    }

    if(httpCode == 204){
      //debugLn("bridgeDB  updated");
      return true;
    }

    



  }else{
    debugLn("ERROR UPDATING BRIDGEDB - NETWORK NOT ENABLED");
    return false;
  }

          
}



// ========================== BOARD MANAGE =====================================
void restartBoard(){
  debugLn(" --- RESTARTING ALL BOARD");
  // Restart the whole board
  
  ESP.restart();
}

void deepSleep(){
  debugLn(" --- starting deep sleep");

  // calling the function makes it read the wifi available around and save on memory
  isSameWifiLocation();

  // power off module
  modem.poweroff();


  
  digitalWrite(BOARD_POWERON_PIN, LOW);

  #ifdef MODEM_RESET_PIN
      // Keep it low during the sleep period. If the module uses GPIO5 as reset, 
      // there will be a pulse when waking up from sleep that will cause the module to start directly.
      // https://github.com/Xinyuan-LilyGO/LilyGO-T-A76XX/issues/85
      digitalWrite(MODEM_RESET_PIN, !MODEM_RESET_LEVEL);
      gpio_hold_en((gpio_num_t)MODEM_RESET_PIN);
      gpio_deep_sleep_hold_en();
  #endif

  // clock wakeup
  esp_sleep_enable_timer_wakeup(timeDeepSleep * uS_TO_S_FACTOR);
  debugLn("timer wakeup enabled - " + String(timeDeepSleep) + "s");

  // clear serial buffer
  if(serialDebug){
    Serial.flush(); 
  }
  SerialAT.flush();


  // read actual status of the vibration sensor, it can be randomly on high or low state depending on how it stopped
  if(enablevibrationwakeup){
    debugLn("Vibration wakeup enabled");
    pinMode(WAKEUP_GPIO,INPUT);
    bool wakeuppinstatus = digitalRead(WAKEUP_GPIO);
    esp_sleep_enable_ext0_wakeup(WAKEUP_GPIO, !wakeuppinstatus);  // set the wakeup trigger to the opposite of what the sensor is now
    // Configure pullup/downs via RTCIO to tie wakeup pins to inactive level during deepsleep.
    // EXT0 resides in the same power domain (RTC_PERIPH) as the RTC IO pullup/downs.
    // No need to keep that power domain explicitly, unlike EXT1.
    rtc_gpio_pullup_dis(WAKEUP_GPIO);
    rtc_gpio_pulldown_en(WAKEUP_GPIO);
  }

  // hold reset pin the same value during sleep
  gpio_hold_en((gpio_num_t)MODEM_RESET_PIN);
  gpio_deep_sleep_hold_en();


  debugLn(" ===================================== Deep Sleep =======================================");




  esp_deep_sleep_start();

  debugLn("This will never be printed");
}

void lightSleep(){
  debugLn(" --- Light Sleep - "+String(timeLightSleep));
  esp_sleep_enable_timer_wakeup(timeLightSleep * uS_TO_S_FACTOR);
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);

  delay(200);
  esp_light_sleep_start();
  
  
  debugLn(" --- Light Sleep Ended");
}

void startModem(){
  debugLn(" --- rebooting modem");

  SerialAT.end();
  delay(1000);

  // release modem reset pin hold
  gpio_hold_dis((gpio_num_t)MODEM_RESET_PIN);

  pinMode(BOARD_POWERON_PIN, OUTPUT);
  digitalWrite(BOARD_POWERON_PIN, HIGH);

  // reset modem
  pinMode(MODEM_RESET_PIN, OUTPUT);
  digitalWrite(MODEM_RESET_PIN, !MODEM_RESET_LEVEL); delay(100);
  digitalWrite(MODEM_RESET_PIN, MODEM_RESET_LEVEL); delay(2600);
  digitalWrite(MODEM_RESET_PIN, !MODEM_RESET_LEVEL);

  // Turn on modem
  pinMode(BOARD_PWRKEY_PIN, OUTPUT);
  digitalWrite(BOARD_PWRKEY_PIN, LOW);
  delay(100);
  digitalWrite(BOARD_PWRKEY_PIN, HIGH);
  delay(1000);
  digitalWrite(BOARD_PWRKEY_PIN, LOW);

  // Set modem baud
  SerialAT.begin(115200, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);

  debugLn("Start modem...");
  delay(3000);

  int retry = 0;
  while (!modem.testAT(1000)) {
      debug(".");
      if (retry++ > 10) {
          digitalWrite(BOARD_PWRKEY_PIN, LOW);
          delay(100);
          digitalWrite(BOARD_PWRKEY_PIN, HIGH);
          delay(1000);
          digitalWrite(BOARD_PWRKEY_PIN, LOW);
          retry = 0;
      }
  }

  debugLn("Modem Started");

  bool gotpwrready = false;

  String resp = sendATCommand("AT+CGNSSPWR=1,1");
  if(resp.indexOf("+CGNSSPWR: READY!") != -1){
    gotpwrready = true;
  }

  // wait for +CGNSSPWR: READY!
  String waitmessage = "";
  while(!gotpwrready){
    if(SerialAT.available()){
      char character = SerialAT.read();
      waitmessage.concat(character);
      delay(10);
    }
    if(waitmessage.indexOf("+CGNSSPWR: READY!") != -1){
      gotpwrready = true;
    }

  }

  // delay(1000);
  // resp = sendATCommand("AT+CAGPS");
  // delay(1000);
  // resp = sendATCommand("AT+CGPSWARM");
  // delay(1000);
  // resp = sendATCommand("AT+CGNSSMODE?");
    



}

int getWakeupReason() {
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch (wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0:
      debugLn("Wakeup caused by Vibration");
      return 1;
      break;
    case ESP_SLEEP_WAKEUP_EXT1:
      debugLn("Wakeup caused by external signal using RTC_CNTL");
      return 2;
      break;
    case ESP_SLEEP_WAKEUP_TIMER:
      debugLn("Wakeup caused by timer");
      return 3;
      break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD:
      debugLn("Wakeup caused by touchpad");
      return 4;
      break;
    case ESP_SLEEP_WAKEUP_ULP:
      debugLn("Wakeup caused by ULP program");
      return 5;
      break;
    default:
      debug("Wakeup was not caused by deep sleep: ");
      debugLn(String(wakeup_reason));
      return 0;
      break;
  }
}

void getBatteryVoltage(){
  // get an average of some readings
  std::vector<uint32_t> data;
  for (int i = 0; i < 30; ++i) {
      uint32_t val = analogReadMilliVolts(BOARD_BAT_ADC_PIN);
      data.push_back(val);
      delay(30);
  }
  std::sort(data.begin(), data.end());
  data.erase(data.begin());
  data.pop_back();
  int sum = std::accumulate(data.begin(), data.end(), 0);
  double average = static_cast<double>(sum) / data.size();

  battery_voltage =  average * 2;

  // GET charging status
  ignitionOn = digitalRead(CHARGINGSTATUSPIN);

  debug("Battery Voltage: "); debugLn(String(battery_voltage));
  debug("Charging: "); debugLn(String(ignitionOn));

}

bool isSameWifiLocation(){
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  // =================== NETWORKS AVAILABLE NOW ========================
  int networkNumber = WiFi.scanNetworks();
  String networklist = "";
  String NetworkListArray[networkNumber];
  int sameNetCounter = 0;


  // loop through all networks found now
  for (int i = 0; i < networkNumber; i++) {
    String thisnetwork = WiFi.SSID(i+1);
    networklist += thisnetwork + "\n";
    NetworkListArray[i] = thisnetwork;
    delay(10);


  }
  
  // continue only if we have more than 3 networks now
  if(networkNumber <= 3){
    return false;
  }

  // get old list before we update it
  String oldNetworkList = preferences.getString("oldNetList","");
  int oldNetworkNumber = preferences.getInt("oldNetNumber",0);
  String oldNetworkListArray[oldNetworkNumber];

  // update list on memory
  preferences.putString("oldNetList",networklist);
  preferences.putInt("oldNetNumber",networkNumber);



  // =================== LAST NETWORKS SAVED ON MEMORY ========================
  // only proceed if we have more than 3
  if(oldNetworkNumber <=3 ){
    return false;
  }

  // parse old network list 
  int StringCount = 0;

  // break old networks string into array
  while (oldNetworkList.length() > 0){
    int index = oldNetworkList.indexOf('\n');
    if (index == -1) // No separator found, it's the last element, or it's empty
    {
      oldNetworkListArray[StringCount++] = oldNetworkList;
      break;
    }
    else
    {
      oldNetworkListArray[StringCount++] = oldNetworkList.substring(0, index);
      oldNetworkList = oldNetworkList.substring(index+1);
    }
  }


  // loop through all wifi now and check against memory list
  for (int i = 0; i < networkNumber; i++) {
    for(int j = 0; j < oldNetworkNumber; j++){
      if(oldNetworkListArray[j] == NetworkListArray[i]){
        sameNetCounter++;
      }
    }

  }
  


  



  float percentage = 0;
  if(oldNetworkNumber > 0){ // theoretically, we'd never get here if it was 0, but just checking anyways.
    percentage =  ((float)sameNetCounter/(float)oldNetworkNumber)*100;
  }

  Serial.println(networklist);
  Serial.println("same network counter = " + String(sameNetCounter) + " - " + String(percentage) + "%" );

  if(percentage<50){
    Serial.println(">>>>>>>>>>>>> consider We are in a different position!!!!!!!!!");
    return false;
  }

  // if it passed wverything, consider we are in the same position
  Serial.println("We are in the same location");
  return true;
}

// ========================== DEBUG =====================================
void debugLn(String text){
  if(serialDebug){
    Serial.println(text);
  }

  if(sdCardDebug){
    logText(text + '\n');
  }
}

void debug(String text){
  if(serialDebug){
    Serial.print(text);
  }
  if(sdCardDebug){
    logText(text);
  }
}


// ========================== SD CARD =====================================
void startSD(){
    SPI.end();
    SD.end();
    delay(1000);
    SPI.begin(BOARD_SCK_PIN, BOARD_MISO_PIN, BOARD_MOSI_PIN);
    if (!SD.begin(BOARD_SD_CS_PIN)) {
        //Serial.println("Card Mount Failed");
        return;
    }
    uint8_t cardType = SD.cardType();

    if (cardType == CARD_NONE) {
        //Serial.println("No SD card attached");
        return;
    }

    //Serial.print("SD Card Type: ");
    if (cardType == CARD_MMC) {
        //Serial.println("MMC");
    } else if (cardType == CARD_SD) {
        //Serial.println("SDSC");
    } else if (cardType == CARD_SDHC) {
        //Serial.println("SDHC");
    } else {
        //Serial.println("UNKNOWN");
    }

    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    //Serial.printf("SD Card Size: %lluMB\n", cardSize);


    File filecreate = SD.open(logfilename);
    if(!filecreate) {
      //Serial.println("File doens't exist");
      //Serial.println("Creating file...");

      

      // write first line of file
      fs::FS fs = SD;
      File filewrite = SD.open(logfilename, FILE_WRITE);
      if(!filewrite) {
        // Serial.println("Failed to open file for writing");
        return;
      }
      if(filewrite.print("FILE CREATED KARALEO")) {
        // Serial.println("File written");
      } else {
        // Serial.println("Write failed");
      }
      filewrite.close();

    }
    else {
      // Serial.println("File already exists");  
    }
    filecreate.close();


}

void logText(String message){
    //Serial.printf("Appending to file: %s\n", logfilename);
    fs::FS fs = SD;
    File file = fs.open(logfilename, FILE_APPEND);


    if (!file) {
        // Serial.println("Failed to open file for appending");
        startSD();
        return;
    }
    if (file.print(message)) {
        //Serial.println("Message appended");
    } else {
        Serial.println("Append failed");
    }
    file.close();
}

// ========================== UNUSED =====================================
void loop() {
  // put your main code here, to run repeatedly:

}






