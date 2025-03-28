#include <sys/time.h>

RTC_DATA_ATTR timeval lastTimerWakeup;
int timersleep = 15;

void setup() {
  Serial.begin(115200);

  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();



  timeval timeNow;
  gettimeofday(&timeNow, NULL);

  

  if(wakeup_reason == ESP_SLEEP_WAKEUP_TIMER){
    Serial.println("woke up by timer");
    gettimeofday(&lastTimerWakeup, NULL);
  }

  if(wakeup_reason == ESP_SLEEP_WAKEUP_EXT0){
    Serial.println("woke up ext0");
    timeval timeSinceTimerWakeup;
    timersub(&timeNow,&lastTimerWakeup,&timeSinceTimerWakeup);
    Serial.println("time since last timer wk: " +  String(timeSinceTimerWakeup.tv_sec));
    timersleep-=timeSinceTimerWakeup.tv_sec;

  }

  
  // case ESP_SLEEP_WAKEUP_EXT0:     Serial.println("Wakeup caused by external signal using RTC_IO"); break;
  // case ESP_SLEEP_WAKEUP_EXT1:     Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
  // case ESP_SLEEP_WAKEUP_TIMER:    Serial.println("Wakeup caused by timer"); break;
  // case ESP_SLEEP_WAKEUP_TOUCHPAD: Serial.println("Wakeup caused by touchpad"); break;
  // case ESP_SLEEP_WAKEUP_ULP:      Serial.println("Wakeup caused by ULP program"); break;
  // default:                        Serial.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason); break;
  

  // timeval timeNow, timeDiff;
  // gettimeofday(&timeNow, NULL);

  // timersub(&timeNow,&sleepTime,&timeDiff);

  //printf("Now: %"PRIu64"ms, Duration: %"PRIu64"ms\n", (timeNow.tv_sec * (uint64_t)1000) + (timeNow.tv_usec / 1000), (timeDiff.tv_sec * (uint64_t)1000) + (timeDiff.tv_usec / 1000));
  Serial.println("timenow: "+ String(timeNow.tv_sec));
  //delay(2000);
  
  // gettimeofday(&sleepTime, NULL);
  // printf("Sleeping...\n");
  // delay(2000);
  
  
  Serial.println("enabling timer sleep for: " +  String(timersleep));
  esp_sleep_enable_timer_wakeup(timersleep * 1000000);
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_39, 1);

  esp_deep_sleep_start();
}
void loop(){}