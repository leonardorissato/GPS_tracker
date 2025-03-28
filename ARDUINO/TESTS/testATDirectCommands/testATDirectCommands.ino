
#define MODEM_BAUDRATE                      (115200)
#define MODEM_DTR_PIN                       (25)
#define MODEM_TX_PIN                        (26)
#define MODEM_RX_PIN                        (27)
// The modem boot pin needs to follow the startup sequence.
#define BOARD_PWRKEY_PIN                    (4)
#define BOARD_ADC_PIN                       (35)
// The modem power switch must be set to HIGH for the modem to supply power.
#define BOARD_POWERON_PIN                   (12)
#define MODEM_RING_PIN                      (33)
#define MODEM_RESET_PIN                     (5)
#define BOARD_MISO_PIN                      (2)
#define BOARD_MOSI_PIN                      (15)
#define BOARD_SCK_PIN                       (14)
#define BOARD_SD_CS_PIN                     (13)
#define BOARD_BAT_ADC_PIN                   (35)
#define MODEM_RESET_LEVEL                   HIGH
#define SerialAT                            Serial1

#define MODEM_GPS_ENABLE_GPIO               (-1)
#define MODEM_GPS_ENABLE_LEVEL              (-1)

// It is only available in V1.4 version. In other versions, IO36 is not connected.
#define BOARD_SOLAR_ADC_PIN                 (36)




#include "TinyGPS++.h"
TinyGPSPlus gps;




void powerOnA7670() {

    pinMode(BOARD_POWERON_PIN, OUTPUT);
    digitalWrite(BOARD_POWERON_PIN, HIGH);


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

    delay(8000);

    Serial.println("board powered on");

 
}

String sendAtCommand(String Command){
  SerialAT.println(Command);

  while(!SerialAT.available()){
    //donothing untill something arrives
  }
  
  String answer = "";
  char character;
  while (SerialAT.available()) {
    //Serial.write(SerialAT.read());
    character =  SerialAT.read();
    answer.concat(character);
    delay(1);
  }

  delay(300);

  // catch any rest of message?
  while (SerialAT.available()) {
    //Serial.write(SerialAT.read());
    character =  SerialAT.read();
    answer.concat(character);
    delay(1);
  }

  return answer;
}

void setup() {
    Serial.begin(115200);
    SerialAT.begin(MODEM_BAUDRATE, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);

    Serial.println("------------------------SETUP STARTED");

    powerOnA7670();

    Serial.println("------------------");

    String response = sendAtCommand("AT");
    Serial.println(response);

    delay(1000);

    response = sendAtCommand("AT+SIMCOMATI");
    Serial.println(response);

    
}

void loop() {
    String gps = sendAtCommand("AT+CGNSSINFO");
    Serial.println(gps);
    delay(10000);
}