#define LED 4  // or use LED_BUILTIN for on-board LED
#define INTERRUPT_PIN 2
RTC_DATA_ATTR int bootCount = 0;

#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  30        /* Time ESP32 will go to sleep (in seconds) */

#include "driver/rtc_io.h"
#define WAKEUP_GPIO              GPIO_NUM_34     // Only R

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(LED, OUTPUT);
  pinMode(INTERRUPT_PIN, INPUT);
  digitalWrite(LED, LOW);

  ++bootCount;
  Serial.println("Boot number: " + String(bootCount));

  print_wakeup_reason();

  Serial.println("Going to blink the LED 10 times");
  blink(3);  // Allow time for wakeup
  Serial.println("Going to sleep now");

  delay(500);


  
  // clock wakeup
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  Serial.println("Setup ESP32 to sleep for " + String(TIME_TO_SLEEP) + " Seconds");

  Serial.flush(); 



  pinMode(WAKEUP_GPIO,INPUT);
  bool wakeupstatus = digitalRead(WAKEUP_GPIO);
  Serial.println("GPIO WAKEUP = " +  String(wakeupstatus));
  esp_sleep_enable_ext0_wakeup(WAKEUP_GPIO, !wakeupstatus);  // set the wakeup trigger to the opposite of what it is now
  // Configure pullup/downs via RTCIO to tie wakeup pins to inactive level during deepsleep.
  // EXT0 resides in the same power domain (RTC_PERIPH) as the RTC IO pullup/downs.
  // No need to keep that power domain explicitly, unlike EXT1.
  rtc_gpio_pullup_dis(WAKEUP_GPIO);
  rtc_gpio_pulldown_en(WAKEUP_GPIO);

  esp_deep_sleep_start();

  Serial.println("This will never be printed");
}

void loop() { }




void blink(int times) {
  for (int i = 0; i < times; i++) {
    digitalWrite(LED, HIGH);
    delay(1000);

    digitalWrite(LED, LOW);
 
    Serial.print("Blink ");
    Serial.println(i + 1);
    delay(1000);
  }
}

void print_wakeup_reason() {
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch (wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0:
      Serial.println("Wakeup caused by external signal using RTC_IO");
      break;
    case ESP_SLEEP_WAKEUP_EXT1:
      Serial.println("Wakeup caused by external signal using RTC_CNTL");
      break;
    case ESP_SLEEP_WAKEUP_TIMER:
      Serial.println("Wakeup caused by timer");
      break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD:
      Serial.println("Wakeup caused by touchpad");
      break;
    case ESP_SLEEP_WAKEUP_ULP:
      Serial.println("Wakeup caused by ULP program");
      break;
    default:
      Serial.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason);
      break;
  }
}