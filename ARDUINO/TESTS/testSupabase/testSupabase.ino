/*
  Rui Santos
  Complete project details at Complete project details at https://RandomNerdTutorials.com/esp32-http-get-post-arduino/

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files.

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
*/

#include <WiFi.h>
//#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h> 


const char* ssid = "FishJuice";
const char* password = "Rookie86";

//Your Domain name with URL path or IP address with path
const char *locationDB_URL = "https://pllwfhcjryzuqsprjdqs.supabase.co/rest/v1/locationHistory";//"https://leonardorissato.requestcatcher.com/test"; 
const char *bridgeDB_URL = "https://pllwfhcjryzuqsprjdqs.supabase.co/rest/v1/bridgeDB?select=*";
const char* apiKey = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6InBsbHdmaGNqcnl6dXFzcHJqZHFzIiwicm9sZSI6ImFub24iLCJpYXQiOjE3MzA3OTA5NTUsImV4cCI6MjA0NjM2Njk1NX0.-tO3afiuTGl_CHUPSvhdOq0Na1sY5SpeENhplWg5r9U"; // Replace with your Supabase API key



void setup() {
  Serial.begin(115200); 

  WiFi.begin(ssid, password);
  Serial.println("Connecting");
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());
 
}

void loop() {

    //Check WiFi connection status
  if(WiFi.status()== WL_CONNECTED){

      JsonDocument jsondoc;
      String serializedJson;


      // create json data
      jsondoc["localtime"] = "25/03/13,15:43:49+44";
      jsondoc["spd"] = "121";
      jsondoc["lat"] = "-37.84557";
      jsondoc["lon"] = "145.2224";
      jsondoc["acc"] = "999";
      jsondoc["ign"] = "1";
      serializeJson(jsondoc, serializedJson);

      // WiFiClient client;
      // HTTPClient http;

      HTTPClient http;
    
      // Your Domain name with URL path or IP address with path
      http.begin(locationDB_URL);
      
      
      //http.setAuthorization("BEARER", "REPLACE_WITH_SERVER_PASSWORD");
      

      /*

      curl -X POST 'https://pllwfhcjryzuqsprjdqs.supabase.co/rest/v1/locationHistory' \
      -H "apikey: SUPABASE_CLIENT_ANON_KEY" \
      -H "Authorization: Bearer SUPABASE_CLIENT_ANON_KEY" \
      -H "Content-Type: application/json" \
      -H "Prefer: return=minimal" \
      -d '{ "some_column": "someValue", "other_column": "otherValue" }'
          
      */


      http.addHeader("Content-Type", "application/json");
      http.addHeader("apikey", String(apiKey));
      http.addHeader("Authorization", "Bearer " + String(apiKey));

      Serial.println(String(serializedJson));


      int httpResponseCode = http.POST(serializedJson);

     
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
        
      // // Free resources
      http.end();
  }
  else {
    Serial.println("WiFi Disconnected");
  }


  delay(30000);
  
}






