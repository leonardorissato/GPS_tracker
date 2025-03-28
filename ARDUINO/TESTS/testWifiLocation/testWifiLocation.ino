/*
  Example from WiFi > WiFiScan
  Complete details at https://RandomNerdTutorials.com/esp32-useful-wi-fi-functions-arduino/
*/
#include <Preferences.h>
#include "WiFi.h"

Preferences preferences; // deal with persistent data



void setup() {
  Serial.begin(115200);

  preferences.begin("memory", false);

  // Set WiFi to station mode and disconnect from an AP if it was previously connected

  delay(100);

  Serial.println("Setup done");
}


void loop() {
  
  Serial.println("--------------");

  

  bool test = isSameWifiLocation();





  // Wait a bit before scanning again
  delay(10000);
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