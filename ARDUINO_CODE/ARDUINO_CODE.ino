
#include "utilities.h"
#include <TinyGsmClient.h>
#include <ArduinoJson.h> 
#include <esp32-hal-adc.h>
#include <Arduino.h>


//=========================================================
// Supabase API URL and Key
const char *locationDB_URL = "https://pllwfhcjryzuqsprjdqs.supabase.co/rest/v1/locationHistory";
const char *bridgeDB_URL = "https://pllwfhcjryzuqsprjdqs.supabase.co/rest/v1/bridgeDB?select=*";
const char* apiKey = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6InBsbHdmaGNqcnl6dXFzcHJqZHFzIiwicm9sZSI6ImFub24iLCJpYXQiOjE3MzA3OTA5NTUsImV4cCI6MjA0NjM2Njk1NX0.-tO3afiuTGl_CHUPSvhdOq0Na1sY5SpeENhplWg5r9U"; // Replace with your Supabase API key
//=========================================================
          


int updateNormalDelay = 10000;
int updateRealtimeDelay = 5000;

bool ignitionlock = false;
bool realtime = false;


float lat2      = 0;
float lon2      = 0;
float speed2    = 0;
float alt2      = 0;
int   vsat2     = 0;
int   usat2     = 0;
float accuracy2 = 0;
int   year2     = 0;
int   month2    = 0;
int   day2      = 0;
int   hour2     = 0;
int   min2      = 0;
int   sec2      = 0;
uint8_t    fixMode   = 0;


#define NETWORK_APN     "live.vodafone.com"//"sxzcat1"        //CHN-CT: China Telecom


TinyGsm modem(SerialAT);
TinyGsmClient client(modem);




void setup()
{
    Serial.begin(115200); // Set console baud rate
    SerialAT.begin(115200, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN); // start connection with model

    // Make sure modem is started
    #ifdef BOARD_POWERON_PIN
        pinMode(BOARD_POWERON_PIN, OUTPUT);
        digitalWrite(BOARD_POWERON_PIN, HIGH);
    #endif

    // Set modem reset pin ,reset modem
    pinMode(MODEM_RESET_PIN, OUTPUT);
    digitalWrite(MODEM_RESET_PIN, !MODEM_RESET_LEVEL); delay(100);
    digitalWrite(MODEM_RESET_PIN, MODEM_RESET_LEVEL); delay(2600);
    digitalWrite(MODEM_RESET_PIN, !MODEM_RESET_LEVEL);


    pinMode(BOARD_PWRKEY_PIN, OUTPUT);
    digitalWrite(BOARD_PWRKEY_PIN, LOW);
    delay(100);
    digitalWrite(BOARD_PWRKEY_PIN, HIGH);
    delay(100);
    digitalWrite(BOARD_PWRKEY_PIN, LOW);


    // start GPS
    Serial.print("Warm up GPS");
    while (!modem.enableGPS(MODEM_GPS_ENABLE_GPIO)) {
    Serial.print(".");
    }

    // warm up gps
    getGPS();

    // start and connect modem to internet
    startModem();


    // send data as soon as it starts
    sendDataToSupabase();

}



int batterylevel = 0;
uint32_t timeStampGPS = 0;
uint32_t timeStampBridge = 0;


void loop()
{
  if (millis() - timeStampGPS > updateNormalDelay) {
    Serial.println("--------------------------------------------");
    timeStampGPS = millis();


    float latbeforeupdate = lat2;
    float lonbeforeupdate = lon2;

    if(getGPS()){
      Serial.print("GOT GPS - Accuracy:  "); Serial.println(accuracy2);

      //calculate distance, only update if more than 0.0001 deg difference
      float difflat = abs(abs(latbeforeupdate) - abs(lat2))*10000;
      float difflon = abs(abs(lonbeforeupdate) - abs(lon2))*10000;

      Serial.println("DIFFERENCE");
      Serial.println(difflat);
      Serial.println(difflon);

      if(difflat >= 1 || difflon >= 1){
        // send data
        sendDataToSupabase();
      };

  
    }else{
      Serial.println("ERROR GETTING GPS DATA");
    };

  }

  // update bridge db every 20 sec
  if (millis() - timeStampBridge > 10000) {
    Serial.println("--------------------------------------------");
    timeStampBridge = millis();

    // get battery level and send it to database
    uint32_t battery_voltage = analogReadMilliVolts(BOARD_BAT_ADC_PIN) ;
    battery_voltage *= 2;   //The hardware voltage divider resistor is half of the actual voltage, multiply it by 2 to get the true voltage
    updateBridgeDB("batterylevel",String(battery_voltage));

    // get localtime and send it to database
    String GSMlocaltime = modem.getGSMDateTime(DATE_FULL);
    updateBridgeDB("lastupdate",String(GSMlocaltime));  


  }





}

void updatebattery(){
  uint32_t battery_voltage = analogReadMilliVolts(BOARD_BAT_ADC_PIN) ;
  battery_voltage *= 2;   //The hardware voltage divider resistor is half of the actual voltage, multiply it by 2 to get the true voltage
  
  batterylevel = battery_voltage;
}

bool getGPS(){
  Serial.println("Requesting current GPS/GNSS/GLONASS location");
  for (;;) {
      
      if (modem.getGPS(&fixMode, &lat2, &lon2, &speed2, &alt2, &vsat2, &usat2, &accuracy2,
                        &year2, &month2, &day2, &hour2, &min2, &sec2)) {

          // Serial.print("FixMode:"); Serial.println(fixMode);
          // Serial.print("Latitude:"); Serial.print(lat2, 6); Serial.print("\tLongitude:"); Serial.println(lon2, 6);
          // Serial.print("Speed:"); Serial.print(speed2); Serial.print("\tAltitude:"); Serial.println(alt2);
          // Serial.print("Visible Satellites:"); Serial.print(vsat2); Serial.print("\tUsed Satellites:"); Serial.println(usat2);
          // Serial.print("Accuracy:"); Serial.println(accuracy2);

          // Serial.print("Year:"); Serial.print(year2);
          // Serial.print("\tMonth:"); Serial.print(month2);
          // Serial.print("\tDay:"); Serial.println(day2);

          // Serial.print("Hour:"); Serial.print(hour2);
          // Serial.print("\tMinute:"); Serial.print(min2);
          // Serial.print("\tSecond:"); Serial.println(sec2);

          return true;
      } else {
          Serial.print(".");
          delay(1000);
      }
  }
  return false;
}

bool startModem(){
    // Check if the modem is online
    Serial.println("Start modem...");

    int retry = 0;
    while (!modem.testAT(1000)) {
        Serial.println(".");
        if (retry++ > 10) {
            digitalWrite(BOARD_PWRKEY_PIN, LOW);
            delay(100);
            digitalWrite(BOARD_PWRKEY_PIN, HIGH);
            delay(1000);
            digitalWrite(BOARD_PWRKEY_PIN, LOW);
            retry = 0;
        }
    }
    Serial.println();

    // Check if SIM card is online
    SimStatus sim = SIM_ERROR;
    while (sim != SIM_READY) {
        sim = modem.getSimStatus();
        switch (sim) {
        case SIM_READY:
            Serial.println("SIM card online");
            break;
        case SIM_LOCKED:
            Serial.println("The SIM card is locked. Please unlock the SIM card first.");
            // const char *SIMCARD_PIN_CODE = "123456";
            // modem.simUnlock(SIMCARD_PIN_CODE);
            break;
        default:
            break;
        }
        delay(1000);

    }



    #ifdef NETWORK_APN
        Serial.printf("Set network apn : %s\n", NETWORK_APN);
        modem.sendAT(GF("+CGDCONT=1,\"IP\",\""), NETWORK_APN, "\"");
        if (modem.waitResponse() != 1) {
            Serial.println("Set network apn error !");
        }
    #endif

    // Check network registration status and network signal status
    int16_t sq ;
    Serial.print("Wait for the modem to register with the network.");
    RegStatus status = REG_NO_RESULT;
    while (status == REG_NO_RESULT || status == REG_SEARCHING || status == REG_UNREGISTERED) {
        status = modem.getRegistrationStatus();
        switch (status) {
        case REG_UNREGISTERED:
        case REG_SEARCHING:
            sq = modem.getSignalQuality();
            Serial.printf("[%lu] Signal Quality:%d\n", millis() / 1000, sq);
            delay(1000);
            break;
        case REG_DENIED:
            Serial.println("Network registration was rejected, please check if the APN is correct");
            return false;
        case REG_OK_HOME:
            Serial.println("Online registration successful");
            break;
        case REG_OK_ROAMING:
            Serial.println("Network registration successful, currently in roaming mode");
            break;
        default:
            Serial.printf("Registration Status:%d\n", status);
            delay(1000);
            break;
        }
    }
    Serial.println();


    Serial.printf("Registration Status:%d\n", status);
    delay(1000);

    String ueInfo;
    if (modem.getSystemInformation(ueInfo)) {
        Serial.print("Inquiring UE system information:");
        Serial.println(ueInfo);
    }

    if (!modem.enableNetwork()) {
        Serial.println("Enable network failed!");
        return false;
    }

    delay(5000);

    String ipAddress = modem.getLocalIP();
    Serial.print("Network IP:"); Serial.println(ipAddress);

    return true;
}

void sendDataToSupabase() {

  // JSON HANDLING
  JsonDocument jsondoc;
  String serializedJson;

  // GET local TIME
  String GSMlocaltime = modem.getGSMDateTime(DATE_FULL);


  // create fake data
  jsondoc["localtime"] = GSMlocaltime;
  jsondoc["spd"] = 0;
  jsondoc["lat"] = lat2;
  jsondoc["lon"] = lon2;
  jsondoc["acc"] = round(accuracy2);
  jsondoc["ign"] = false;
  serializeJson(jsondoc, serializedJson);



  
 
  if(modem.enableNetwork()){
    modem.https_end();
    delay(500);
    modem.https_begin();
    delay(500);

    if (!modem.https_set_url(locationDB_URL)) {
        Serial.println("Failed to set the URL. Please check the validity of the URL!");
        return;
        delay(6000);
    }


    delay(500);
    // fix error code 715 
    modem.sendAT("+CSSLCFG=\"enableSNI\",0,1");
    delay(500);


    modem.https_set_content_type("application/json");

    modem.https_add_header("apikey", String(apiKey));
    modem.https_add_header("Authorization", "Bearer " + String(apiKey));

    
    int httpCode = modem.https_post(serializedJson);
    
    if (httpCode != 200 && httpCode != 201) {
        Serial.print("HTTP post failed ! error code = "); 
        Serial.println(httpCode); 


        // Get HTTPS header information
        String header = modem.https_header();
        Serial.print("HTTP Header : ");
        Serial.println(header);

        // Get HTTPS response
        String body = modem.https_body();
        Serial.print("HTTP body : ");
        Serial.println(body);

        return;
    }

    if(httpCode == 201){
      Serial.println("SUCCESSS MOTHERFUCKER!!!!!!!!!!!!!!");
    }

    



  }else{
    Serial.println("ERROR - NETWORK NOT ENABLED");
  }

}

void updateBridgeDB(String variable,String value){
  if(modem.enableNetwork()){
    modem.https_end();
    delay(500);
    modem.https_begin();
    delay(500);

    if (!modem.https_set_url("https://pllwfhcjryzuqsprjdqs.supabase.co/rest/v1/bridgeDB?variable=eq."+variable)) {
        Serial.println("Failed to set the URL. Please check the validity of the URL!");
        return;
        delay(6000);
    }


    delay(500);
    // fix error code 715 
    modem.sendAT("+CSSLCFG=\"enableSNI\",0,1");
    delay(500);


    modem.https_set_content_type("application/json");
    modem.https_add_header("apikey", String(apiKey));
    modem.https_add_header("Authorization", "Bearer " + String(apiKey));

    
    int httpCode = modem.https_patch("{\"value\": \""+String(value)+"\"}");
    
    if (httpCode != 200 && httpCode != 201 && httpCode != 204) {
        Serial.print("HTTP post failed ! error code = "); 
        Serial.println(httpCode); 


        // Get HTTPS header information
        String header = modem.https_header();
        Serial.print("HTTP Header : ");
        Serial.println(header);

        // Get HTTPS response
        String body = modem.https_body();
        Serial.print("HTTP body : ");
        Serial.println(body);

        return;
    }

    if(httpCode == 204){
      Serial.println("bridgeDB UPDATED");
    }

    



  }else{
    Serial.println("ERROR - NETWORK NOT ENABLED");
  }

          
}

