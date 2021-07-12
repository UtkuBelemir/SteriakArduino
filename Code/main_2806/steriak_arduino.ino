#include <MQ131.h>
#include <Nextion.h>
#include <NexButton.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

#define RELAY1_PIN 7
#define RELAY2_PIN 6
#define RELAY3_PIN 5
#define RELAY4_PIN 4
#define RELAY5_PIN 3

#define RELAY_COUNT 5

#define HEATSENSOR_PIN 9
// #define BUZZER_PIN 9
#define BUZZER_NOTE 440

#define MQ131_DG_PIN 8
#define MQ131_AN_PIN A1

DHT HeatSensor(HEATSENSOR_PIN, DHT22);

int timerDuration = 0, currentTimerDuration = 0, systemState = 0;
unsigned long startMillis, currentMillis;
unsigned long runTime = 0;
const unsigned long ONE_SEC = 1000;
const unsigned long ONE_MIN = ONE_SEC*60;
const unsigned long TWO_MIN = ONE_MIN*2;
unsigned long MS_temperatureLast, MS_humidityLast, MS_buzzer;
float temperatureValue, humidityValue;

float MQ131_calibrationValue = 14753846.00;
long MQ131_timeToRead = 31;
float maxPPM = -1, avgPPM = -1, currentPPM = -1, totalPPM = -1;
int PPMSampleCount = 0;

void sendToNextion(String instruction, String value) {
  Serial2.print(instruction);
  if (value.length() > 0) {
    Serial2.print(value);
  }
  Serial2.write(0xff);
  Serial2.write(0xff);
  Serial2.write(0xff);
  Serial.println(instruction + value);
}

NexButton menuBtn = NexButton(1, 28, "menuBtn");
NexButton startBtn = NexButton(1, 29, "startBtn");
NexButton stopBtn = NexButton(1, 30, "stopBtn");
NexButton completeBtn = NexButton(1, 32, "completeBtn");

NexButton dr_10 = NexButton(2, 6, "dr_10"); // TODO
NexButton dr_30 = NexButton(2, 4, "dr_30");
NexButton dr_60 = NexButton(2, 1, "dr_60");
NexButton drSelectBtn = NexButton(2, 2, "drSelectBtn");

NexTouch *nex_listen_list[] = {
  &dr_10,
  &dr_30,
  &dr_60,
  &drSelectBtn,
  &menuBtn,
  &startBtn,
  &stopBtn,
  &completeBtn,
  NULL
};

String getMinPartVal() {
  return String((int)(currentTimerDuration/60));
}
String getSecPartVal() {
  return String(currentTimerDuration%60);
}

void dr_10PushCallback(void *ptr) {
  timerDuration = 10;
}

void dr_30PushCallback(void *ptr) {
  timerDuration = 30;
}

void dr_60PushCallback(void *ptr) {
  timerDuration = 60;
}

void drSelectBtnPushCallback(void *ptr) {
  currentTimerDuration = timerDuration * 60;
  systemState = 0;
  sendToNextion("page mainPage", "");
  sendToNextion("mainStatusText.txt=", "\"waiting...\"");
  sendToNextion("mainStatusText.pco=1663","");
  sendToNextion("vis startBtn,0","");
  sendToNextion("vis stopBtn,0","");
  sendToNextion("vis printBtn,0","");
  sendToNextion("vis completeBtn,0","");
  if (timerDuration > 0 && systemState == 0) {
    sendToNextion("vis startBtn,1","");
    updateNextionTimer();
  }
}

void startBtnPushCallback(void *ptr) {
  systemState = 1;
  startMillis = millis();
  sendToNextion("vis menuBtn,0","");
  sendToNextion("vis startBtn,0","");
  sendToNextion("vis stopBtn,1","");
  sendToNextion("mainStatusText.txt=","\"sterilizing...\"");
}

void stopBtnPushCallback(void *ptr) {
  systemState = 0;
  disableRelays();
  runTime = (timerDuration * 60) - (currentTimerDuration);
  timerDuration = 0;
  currentTimerDuration = 0;
  updateNextionTimer();
  sendToNextion("vis menuBtn,1","");
  sendToNextion("vis stopBtn,0","");
  sendToNextion("mainStatusText.txt=","\"waiting...\"");
  sendToNextion("mainStatusText.pco=1663","");
}

void completeBtnPushCallback(void *ptr) {
  systemState = 0;
  timerDuration = 0;
  currentTimerDuration = 0;
  sendToNextion("vis menuBtn,1","");
  sendToNextion("vis stopBtn,0","");
  sendToNextion("vis printBtn,0","");
  sendToNextion("vis completeBtn,0","");
  sendToNextion("mainStatusText.txt=","\"waiting...\"");
  sendToNextion("mainStatusText.pco=1663","");
}

void setup() {
  Serial.begin(9600);
  nexInit();
  delay(500);
  sendToNextion("initText.txt=","\"Initializing O3 Sensor...\"");
  initializeMQ131();
  delay(2000);
  sendToNextion("initText.txt=","\"Initializing Heat Sensor...\"");
  initializeHeatSensor();
  sendToNextion("initText.txt=","\"Initializing Done. Starting...\"");
  delay(1000);
  sendToNextion("page mainPage","");
  dr_10.attachPush(dr_10PushCallback, &dr_10);
  dr_30.attachPush(dr_30PushCallback, &dr_30);
  dr_60.attachPush(dr_60PushCallback, &dr_60);
  drSelectBtn.attachPush(drSelectBtnPushCallback, &drSelectBtn);
  startBtn.attachPush(startBtnPushCallback, &startBtn);
  stopBtn.attachPush(stopBtnPushCallback, &stopBtn);
  completeBtn.attachPush(completeBtnPushCallback, &completeBtn);
}

void loop() {
  nexLoop(nex_listen_list);
  currentMillis = millis();
  if (systemState == 1 && currentMillis - startMillis >= ONE_SEC) {
    if (currentTimerDuration <= 0) {
      finishSterilizing();
    } else {
      checkRelays();
      currentTimerDuration = currentTimerDuration - 1;
      startMillis = currentMillis;
    }
    updateNextionTimer();
  }
  setMeasurementVals();
  checkMQ131Data();
}
void updateNextionTimer() {
  sendToNextion("minPart.val="+getMinPartVal(),"");
  sendToNextion("secPart.val="+getSecPartVal(),"");
}
void finishSterilizing() {
  systemState = 2;
  runTime = ((timerDuration * 60) - currentTimerDuration);
  disableRelays();
  sendToNextion("vis stopBtn,0","");
  sendToNextion("vis printBtn,1","");
  sendToNextion("vis completeBtn,1","");
  sendToNextion("mainStatusText.txt=","\"finished\"");
  sendToNextion("mainStatusText.pco=13926","");
}

void setMeasurementVals() {
  if (!isnan(temperatureValue) && !isnan(humidityValue)) {
   MQ131.setEnv(temperatureValue, humidityValue); 
  }
  setTemperatureText();
  setHumidityText();
  setO3Texts();
}

/*--------------------------------------------------------------
 _    _            _    _____                           
| |  | |          | |  / ____|                          
| |__| | ___  __ _| |_| (___   ___ _ __  ___  ___  _ __ 
|  __  |/ _ \/ _` | __|\___ \ / _ \ '_ \/ __|/ _ \| '__|
| |  | |  __/ (_| | |_ ____) |  __/ | | \__ \ (_) | |   
|_|  |_|\___|\__,_|\__|_____/ \___|_| |_|___/\___/|_|   
--------------------------------------------------------------*/
void initializeHeatSensor() {
  HeatSensor.begin();
  delay(2500);
}
bool diffMS(unsigned long compareVal, int threshold) {
  if ( (currentMillis - compareVal) > threshold ){
    return true;
  }
  return false;
}
void setTemperatureText() {
  if (!diffMS(MS_temperatureLast, 2000)){
    return;
  }
  temperatureValue = HeatSensor.readTemperature();
  if (isnan(temperatureValue)){
    sendToNextion("temperatureVal.txt=","\"NaN\"");
  } else {
    sendToNextion("temperatureVal.txt=","\""+String(temperatureValue)+"\"");
  }
  MS_temperatureLast = currentMillis;
}
void setHumidityText() {
  if (!diffMS(MS_humidityLast, 2000)){
    return;
  }
  humidityValue = HeatSensor.readHumidity();
  if (isnan(humidityValue)) {
    sendToNextion("humidityVal.txt=","\"NaN\"");
  } else {
    sendToNextion("humidityVal.txt=","\""+String(humidityValue)+"\"");
  }
  MS_humidityLast = currentMillis;
}
/*--------------------------------------------------------------
______     _             
| ___ \   | |            
| |_/ /___| | __ _ _   _ 
|    // _ \ |/ _` | | | |
| |\ \  __/ | (_| | |_| |
\_| \_\___|_|\__,_|\__, |
                    __/ |
                   |___/ 
--------------------------------------------------------------*/
int RELAY_PINS[RELAY_COUNT] = {
  RELAY1_PIN,
  RELAY2_PIN,
  RELAY3_PIN,
  RELAY4_PIN,
  RELAY5_PIN,
};
/*
30:00 29:45 29:30 29:15 29:00 28:45 28:30 28:15
  0  ,  0  ,  0  ,  0  ,  0  ,  0  ,  0  ,  0
28:00 27:45 27:30 27:15 27:00 26:45 26:30 26:15
  0  ,  0  ,  0  ,  0  ,  0  ,  0  ,  0  ,  0
26:00 25:45 25:30 25:15 25:00 24:45 24:30 24:15
  0  ,  0  ,  0  ,  0  ,  0  ,  0  ,  0  ,  0
24:00 23:45 23:30 23:15 23:00 22:45 22:30 22:15
  0  ,  0  ,  0  ,  0  ,  0  ,  0  ,  0  ,  0
22:00 21:45 21:30 21:15 21:00 20:45 20:30 20:15
  0  ,  0  ,  0  ,  0  ,  0  ,  0  ,  0  ,  0
20:00 19:45 19:30 19:15 19:00 18:45 18:30 18:15
  0  ,  0  ,  0  ,  0  ,  0  ,  0  ,  0  ,  0
18:00 17:45 17:30 17:15 17:00 16:45 16:30 16:15
  0  ,  0  ,  0  ,  0  ,  0  ,  0  ,  0  ,  0
16:00 15:45 15:30 15:15 15:00 14:45 14:30 14:15
  0  ,  0  ,  0  ,  0  ,  0  ,  0  ,  0  ,  0
14:00 13:45 13:30 13:15 13:00 12:45 12:30 12:15
  0  ,  0  ,  0  ,  0  ,  0  ,  0  ,  0  ,  0
12:00 11:45 11:30 11:15 11:00 10:45 10:30 10:15
  0  ,  0  ,  0  ,  0  ,  0  ,  0  ,  0  ,  0
10:00 09:45 09:30 09:15 09:00 08:45 08:30 08:15
  0  ,  0  ,  0  ,  0  ,  0  ,  0  ,  0  ,  0
08:00 07:45 07:30 07:15 07:00 06:45 06:30 06:15
  0  ,  0  ,  0  ,  0  ,  0  ,  0  ,  0  ,  0
06:00 05:45 05:30 05:15 05:00 04:45 04:30 04:15
  0  ,  0  ,  0  ,  0  ,  0  ,  0  ,  0  ,  0
04:00 03:45 03:30 03:15 03:00 02:45 02:30 02:15
  0  ,  0  ,  0  ,  0  ,  0  ,  0  ,  0  ,  0
02:00 01:45 01:30 01:15 01:00 00:45 00:30 00:15
  0  ,  0  ,  0  ,  0  ,  0  ,  0  ,  0  ,  0
00:00
  0
*/
int relayConf10[RELAY_COUNT][41] = {
  {
    0, 0, 0, 0, 0, 0, 0, 0, // 1st Relay 10:00 - 08:15
    0, 0, 0, 0, 0, 0, 0, 0, // 1st Relay 08:00 - 06:15
    0, 0, 0, 0, 0, 0, 0, 0, // 1st Relay 06:00 - 04:15
    0, 0, 0, 0, 0, 0, 0, 0, // 1st Relay 04:00 - 02:15
    0, 0, 0, 0, 0, 0, 0, 0, // 1st Relay 02:00 - 00:15
    0                       // 1st Relay 00:15 - 00:00
  },
  {
    0, 0, 0, 0, 0, 0, 0, 0, // 2nd Relay 10:00 - 08:15
    0, 0, 0, 0, 0, 0, 0, 0, // 2nd Relay 08:00 - 06:15
    0, 0, 0, 0, 0, 0, 0, 0, // 2nd Relay 06:00 - 04:15
    0, 0, 0, 0, 0, 0, 0, 0, // 2nd Relay 04:00 - 02:15
    0, 0, 0, 0, 0, 0, 0, 0, // 2nd Relay 02:00 - 00:15
    0                       // 2nd Relay 00:15 - 00:00
  },
  {
    0, 0, 0, 0, 0, 0, 0, 0, // 3rd Relay 10:00 - 08:15
    0, 0, 0, 0, 0, 0, 0, 0, // 3rd Relay 08:00 - 06:15
    0, 0, 0, 0, 0, 0, 0, 0, // 3rd Relay 06:00 - 04:15
    0, 0, 0, 0, 0, 0, 0, 0, // 3rd Relay 04:00 - 02:15
    0, 0, 0, 0, 0, 0, 0, 0, // 3rd Relay 02:00 - 00:15
    0                       // 3rd Relay 00:15 - 00:00
  },
  {
    0, 0, 0, 0, 0, 0, 0, 0, // 4th Relay 10:00 - 08:15
    0, 0, 0, 0, 0, 0, 0, 0, // 4th Relay 08:00 - 06:15
    0, 0, 0, 0, 0, 0, 0, 0, // 4th Relay 06:00 - 04:15
    0, 0, 0, 0, 0, 0, 0, 0, // 4th Relay 04:00 - 02:15
    0, 0, 0, 0, 0, 0, 0, 0, // 4th Relay 02:00 - 00:15
    0                       // 4th Relay 00:15 - 00:00
  },
  {
    0, 0, 0, 0, 0, 0, 0, 0, // 5th Relay 10:00 - 08:15
    0, 0, 0, 0, 0, 0, 0, 0, // 5th Relay 08:00 - 06:15
    0, 0, 0, 0, 0, 0, 0, 0, // 5th Relay 06:00 - 04:15
    0, 0, 0, 0, 0, 0, 0, 0, // 5th Relay 04:00 - 02:15
    0, 0, 0, 0, 0, 0, 0, 0, // 5th Relay 02:00 - 00:15
    0                       // 5th Relay 00:15 - 00:00
  },
};

int relayConf30[RELAY_COUNT][121] = {
  {
    0, 0, 0, 0, 0, 0, 0, 0, // 1st Relay 30:00 - 28:15
    0, 0, 0, 0, 0, 0, 0, 0, // 1st Relay 28:00 - 26:15
    0, 0, 0, 0, 0, 0, 0, 0, // 1st Relay 26:00 - 24:15
    0, 0, 0, 0, 0, 0, 0, 0, // 1st Relay 24:00 - 22:15
    0, 0, 0, 0, 0, 0, 0, 0, // 1st Relay 22:00 - 20:15
    0, 0, 0, 0, 0, 0, 0, 0, // 1st Relay 20:00 - 18:15
    0, 0, 0, 0, 0, 0, 0, 0, // 1st Relay 18:00 - 16:15
    0, 0, 0, 0, 0, 0, 0, 0, // 1st Relay 16:00 - 14:15
    0, 0, 0, 0, 0, 0, 0, 0, // 1st Relay 14:00 - 12:15
    0, 0, 0, 0, 0, 0, 0, 0, // 1st Relay 12:00 - 10:15
    0, 0, 0, 0, 0, 0, 0, 0, // 1st Relay 10:00 - 08:15
    0, 0, 0, 0, 0, 0, 0, 0, // 1st Relay 08:00 - 06:15
    0, 0, 0, 0, 0, 0, 0, 0, // 1st Relay 06:00 - 04:15
    0, 0, 0, 0, 0, 0, 0, 0, // 1st Relay 04:00 - 02:15
    0, 0, 0, 0, 0, 0, 0, 0, // 1st Relay 02:00 - 00:15
    0                       // 1st Relay 00:15 - 00:00
  },
  {
    0, 0, 0, 0, 0, 0, 0, 0, // 2nd Relay 30:00 - 28:15
    0, 0, 0, 0, 0, 0, 0, 0, // 2nd Relay 28:00 - 26:15
    0, 0, 0, 0, 0, 0, 0, 0, // 2nd Relay 26:00 - 24:15
    0, 0, 0, 0, 0, 0, 0, 0, // 2nd Relay 24:00 - 22:15
    0, 0, 0, 0, 0, 0, 0, 0, // 2nd Relay 22:00 - 20:15
    0, 0, 0, 0, 0, 0, 0, 0, // 2nd Relay 20:00 - 18:15
    0, 0, 0, 0, 0, 0, 0, 0, // 2nd Relay 18:00 - 16:15
    0, 0, 0, 0, 0, 0, 0, 0, // 2nd Relay 16:00 - 14:15
    0, 0, 0, 0, 0, 0, 0, 0, // 2nd Relay 14:00 - 12:15
    0, 0, 0, 0, 0, 0, 0, 0, // 2nd Relay 12:00 - 10:15
    0, 0, 0, 0, 0, 0, 0, 0, // 2nd Relay 10:00 - 08:15
    0, 0, 0, 0, 0, 0, 0, 0, // 2nd Relay 08:00 - 06:15
    0, 0, 0, 0, 0, 0, 0, 0, // 2nd Relay 06:00 - 04:15
    0, 0, 0, 0, 0, 0, 0, 0, // 2nd Relay 04:00 - 02:15
    0, 0, 0, 0, 0, 0, 0, 0, // 2nd Relay 02:00 - 00:15
    0                       // 2nd Relay 00:15 - 00:00
  },
  {
    0, 0, 0, 0, 0, 0, 0, 0, // 3rd Relay 30:00 - 28:15
    0, 0, 0, 0, 0, 0, 0, 0, // 3rd Relay 28:00 - 26:15
    0, 0, 0, 0, 0, 0, 0, 0, // 3rd Relay 26:00 - 24:15
    0, 0, 0, 0, 0, 0, 0, 0, // 3rd Relay 24:00 - 22:15
    0, 0, 0, 0, 0, 0, 0, 0, // 3rd Relay 22:00 - 20:15
    0, 0, 0, 0, 0, 0, 0, 0, // 3rd Relay 20:00 - 18:15
    0, 0, 0, 0, 0, 0, 0, 0, // 3rd Relay 18:00 - 16:15
    0, 0, 0, 0, 0, 0, 0, 0, // 3rd Relay 16:00 - 14:15
    0, 0, 0, 0, 0, 0, 0, 0, // 3rd Relay 14:00 - 12:15
    0, 0, 0, 0, 0, 0, 0, 0, // 3rd Relay 12:00 - 10:15
    0, 0, 0, 0, 0, 0, 0, 0, // 3rd Relay 10:00 - 08:15
    0, 0, 0, 0, 0, 0, 0, 0, // 3rd Relay 08:00 - 06:15
    0, 0, 0, 0, 0, 0, 0, 0, // 3rd Relay 06:00 - 04:15
    0, 0, 0, 0, 0, 0, 0, 0, // 3rd Relay 04:00 - 02:15
    0, 0, 0, 0, 0, 0, 0, 0, // 3rd Relay 02:00 - 00:15
    0                       // 3rd Relay 00:15 - 00:00
  },
  {
    0, 0, 0, 0, 0, 0, 0, 0, // 4th Relay 30:00 - 28:15
    0, 0, 0, 0, 0, 0, 0, 0, // 4th Relay 28:00 - 26:15
    0, 0, 0, 0, 0, 0, 0, 0, // 4th Relay 26:00 - 24:15
    0, 0, 0, 0, 0, 0, 0, 0, // 4th Relay 24:00 - 22:15
    0, 0, 0, 0, 0, 0, 0, 0, // 4th Relay 22:00 - 20:15
    0, 0, 0, 0, 0, 0, 0, 0, // 4th Relay 20:00 - 18:15
    0, 0, 0, 0, 0, 0, 0, 0, // 4th Relay 18:00 - 16:15
    0, 0, 0, 0, 0, 0, 0, 0, // 4th Relay 16:00 - 14:15
    0, 0, 0, 0, 0, 0, 0, 0, // 4th Relay 14:00 - 12:15
    0, 0, 0, 0, 0, 0, 0, 0, // 4th Relay 12:00 - 10:15
    0, 0, 0, 0, 0, 0, 0, 0, // 4th Relay 10:00 - 08:15
    0, 0, 0, 0, 0, 0, 0, 0, // 4th Relay 08:00 - 06:15
    0, 0, 0, 0, 0, 0, 0, 0, // 4th Relay 06:00 - 04:15
    0, 0, 0, 0, 0, 0, 0, 0, // 4th Relay 04:00 - 02:15
    0, 0, 0, 0, 0, 0, 0, 0, // 4th Relay 02:00 - 00:15
    0                       // 4th Relay 00:15 - 00:00
  },
  {
    0, 0, 0, 0, 0, 0, 0, 0, // 5th Relay 30:00 - 28:15
    0, 0, 0, 0, 0, 0, 0, 0, // 5th Relay 28:00 - 26:15
    0, 0, 0, 0, 0, 0, 0, 0, // 5th Relay 26:00 - 24:15
    0, 0, 0, 0, 0, 0, 0, 0, // 5th Relay 24:00 - 22:15
    0, 0, 0, 0, 0, 0, 0, 0, // 5th Relay 22:00 - 20:15
    0, 0, 0, 0, 0, 0, 0, 0, // 5th Relay 20:00 - 18:15
    0, 0, 0, 0, 0, 0, 0, 0, // 5th Relay 18:00 - 16:15
    0, 0, 0, 0, 0, 0, 0, 0, // 5th Relay 16:00 - 14:15
    0, 0, 0, 0, 0, 0, 0, 0, // 5th Relay 14:00 - 12:15
    0, 0, 0, 0, 0, 0, 0, 0, // 5th Relay 12:00 - 10:15
    0, 0, 0, 0, 0, 0, 0, 0, // 5th Relay 10:00 - 08:15
    0, 0, 0, 0, 0, 0, 0, 0, // 5th Relay 08:00 - 06:15
    0, 0, 0, 0, 0, 0, 0, 0, // 5th Relay 06:00 - 04:15
    0, 0, 0, 0, 0, 0, 0, 0, // 5th Relay 04:00 - 02:15
    0, 0, 0, 0, 0, 0, 0, 0, // 5th Relay 02:00 - 00:15
    0                       // 5th Relay 00:15 - 00:00
  },
};

int relayConf60[RELAY_COUNT][241] = {
  {
    0, 0, 0, 0, 0, 0, 0, 0, // 1st Relay 60:00 - 58:15
    0, 0, 0, 0, 0, 0, 0, 0, // 1st Relay 58:00 - 56:15
    0, 0, 0, 0, 0, 0, 0, 0, // 1st Relay 56:00 - 54:15
    0, 0, 0, 0, 0, 0, 0, 0, // 1st Relay 54:00 - 52:15
    0, 0, 0, 0, 0, 0, 0, 0, // 1st Relay 52:00 - 50:15
    0, 0, 0, 0, 0, 0, 0, 0, // 1st Relay 50:00 - 48:15
    0, 0, 0, 0, 0, 0, 0, 0, // 1st Relay 48:00 - 46:15
    0, 0, 0, 0, 0, 0, 0, 0, // 1st Relay 46:00 - 44:15
    0, 0, 0, 0, 0, 0, 0, 0, // 1st Relay 44:00 - 42:15
    0, 0, 0, 0, 0, 0, 0, 0, // 1st Relay 42:00 - 40:15
    0, 0, 0, 0, 0, 0, 0, 0, // 1st Relay 40:00 - 38:15
    0, 0, 0, 0, 0, 0, 0, 0, // 1st Relay 38:00 - 36:15
    0, 0, 0, 0, 0, 0, 0, 0, // 1st Relay 36:00 - 34:15
    0, 0, 0, 0, 0, 0, 0, 0, // 1st Relay 34:00 - 32:15
    0, 0, 0, 0, 0, 0, 0, 0, // 1st Relay 32:00 - 30:15
    0, 0, 0, 0, 0, 0, 0, 0, // 1st Relay 30:00 - 28:15
    0, 0, 0, 0, 0, 0, 0, 0, // 1st Relay 28:00 - 26:15
    0, 0, 0, 0, 0, 0, 0, 0, // 1st Relay 26:00 - 24:15
    0, 0, 0, 0, 0, 0, 0, 0, // 1st Relay 24:00 - 22:15
    0, 0, 0, 0, 0, 0, 0, 0, // 1st Relay 22:00 - 20:15
    0, 0, 0, 0, 0, 0, 0, 0, // 1st Relay 20:00 - 18:15
    0, 0, 0, 0, 0, 0, 0, 0, // 1st Relay 18:00 - 16:15
    0, 0, 0, 0, 0, 0, 0, 0, // 1st Relay 16:00 - 14:15
    0, 0, 0, 0, 0, 0, 0, 0, // 1st Relay 14:00 - 12:15
    0, 0, 0, 0, 0, 0, 0, 0, // 1st Relay 12:00 - 10:15
    0, 0, 0, 0, 0, 0, 0, 0, // 1st Relay 10:00 - 08:15
    0, 0, 0, 0, 0, 0, 0, 0, // 1st Relay 08:00 - 06:15
    0, 0, 0, 0, 0, 0, 0, 0, // 1st Relay 06:00 - 04:15
    0, 0, 0, 0, 0, 0, 0, 0, // 1st Relay 04:00 - 02:15
    0, 0, 0, 0, 0, 0, 0, 0, // 1st Relay 02:00 - 00:15
    0                       // 1st Relay 00:15 - 00:00
  },
  { 
    0, 0, 0, 0, 0, 0, 0, 0, // 2nd Relay 60:00 - 58:15
    0, 0, 0, 0, 0, 0, 0, 0, // 2nd Relay 58:00 - 56:15
    0, 0, 0, 0, 0, 0, 0, 0, // 2nd Relay 56:00 - 54:15
    0, 0, 0, 0, 0, 0, 0, 0, // 2nd Relay 54:00 - 52:15
    0, 0, 0, 0, 0, 0, 0, 0, // 2nd Relay 52:00 - 50:15
    0, 0, 0, 0, 0, 0, 0, 0, // 2nd Relay 50:00 - 48:15
    0, 0, 0, 0, 0, 0, 0, 0, // 2nd Relay 48:00 - 46:15
    0, 0, 0, 0, 0, 0, 0, 0, // 2nd Relay 46:00 - 44:15
    0, 0, 0, 0, 0, 0, 0, 0, // 2nd Relay 44:00 - 42:15
    0, 0, 0, 0, 0, 0, 0, 0, // 2nd Relay 42:00 - 40:15
    0, 0, 0, 0, 0, 0, 0, 0, // 2nd Relay 40:00 - 38:15
    0, 0, 0, 0, 0, 0, 0, 0, // 2nd Relay 38:00 - 36:15
    0, 0, 0, 0, 0, 0, 0, 0, // 2nd Relay 36:00 - 34:15
    0, 0, 0, 0, 0, 0, 0, 0, // 2nd Relay 34:00 - 32:15
    0, 0, 0, 0, 0, 0, 0, 0, // 2nd Relay 32:00 - 30:15
    0, 0, 0, 0, 0, 0, 0, 0, // 2nd Relay 30:00 - 28:15
    0, 0, 0, 0, 0, 0, 0, 0, // 2nd Relay 28:00 - 26:15
    0, 0, 0, 0, 0, 0, 0, 0, // 2nd Relay 26:00 - 24:15
    0, 0, 0, 0, 0, 0, 0, 0, // 2nd Relay 24:00 - 22:15
    0, 0, 0, 0, 0, 0, 0, 0, // 2nd Relay 22:00 - 20:15
    0, 0, 0, 0, 0, 0, 0, 0, // 2nd Relay 20:00 - 18:15
    0, 0, 0, 0, 0, 0, 0, 0, // 2nd Relay 18:00 - 16:15
    0, 0, 0, 0, 0, 0, 0, 0, // 2nd Relay 16:00 - 14:15
    0, 0, 0, 0, 0, 0, 0, 0, // 2nd Relay 14:00 - 12:15
    0, 0, 0, 0, 0, 0, 0, 0, // 2nd Relay 12:00 - 10:15
    0, 0, 0, 0, 0, 0, 0, 0, // 2nd Relay 10:00 - 08:15
    0, 0, 0, 0, 0, 0, 0, 0, // 2nd Relay 08:00 - 06:15
    0, 0, 0, 0, 0, 0, 0, 0, // 2nd Relay 06:00 - 04:15
    0, 0, 0, 0, 0, 0, 0, 0, // 2nd Relay 04:00 - 02:15
    0, 0, 0, 0, 0, 0, 0, 0, // 2nd Relay 02:00 - 00:15
    0                       // 2nd Relay 00:15 - 00:00
  },
  { 
    0, 0, 0, 0, 0, 0, 0, 0, // 3rd Relay 60:00 - 58:15
    0, 0, 0, 0, 0, 0, 0, 0, // 3rd Relay 58:00 - 56:15
    0, 0, 0, 0, 0, 0, 0, 0, // 3rd Relay 56:00 - 54:15
    0, 0, 0, 0, 0, 0, 0, 0, // 3rd Relay 54:00 - 52:15
    0, 0, 0, 0, 0, 0, 0, 0, // 3rd Relay 52:00 - 50:15
    0, 0, 0, 0, 0, 0, 0, 0, // 3rd Relay 50:00 - 48:15
    0, 0, 0, 0, 0, 0, 0, 0, // 3rd Relay 48:00 - 46:15
    0, 0, 0, 0, 0, 0, 0, 0, // 3rd Relay 46:00 - 44:15
    0, 0, 0, 0, 0, 0, 0, 0, // 3rd Relay 44:00 - 42:15
    0, 0, 0, 0, 0, 0, 0, 0, // 3rd Relay 42:00 - 40:15
    0, 0, 0, 0, 0, 0, 0, 0, // 3rd Relay 40:00 - 38:15
    0, 0, 0, 0, 0, 0, 0, 0, // 3rd Relay 38:00 - 36:15
    0, 0, 0, 0, 0, 0, 0, 0, // 3rd Relay 36:00 - 34:15
    0, 0, 0, 0, 0, 0, 0, 0, // 3rd Relay 34:00 - 32:15
    0, 0, 0, 0, 0, 0, 0, 0, // 3rd Relay 32:00 - 30:15
    0, 0, 0, 0, 0, 0, 0, 0, // 3rd Relay 30:00 - 28:15
    0, 0, 0, 0, 0, 0, 0, 0, // 3rd Relay 28:00 - 26:15
    0, 0, 0, 0, 0, 0, 0, 0, // 3rd Relay 26:00 - 24:15
    0, 0, 0, 0, 0, 0, 0, 0, // 3rd Relay 24:00 - 22:15
    0, 0, 0, 0, 0, 0, 0, 0, // 3rd Relay 22:00 - 20:15
    0, 0, 0, 0, 0, 0, 0, 0, // 3rd Relay 20:00 - 18:15
    0, 0, 0, 0, 0, 0, 0, 0, // 3rd Relay 18:00 - 16:15
    0, 0, 0, 0, 0, 0, 0, 0, // 3rd Relay 16:00 - 14:15
    0, 0, 0, 0, 0, 0, 0, 0, // 3rd Relay 14:00 - 12:15
    0, 0, 0, 0, 0, 0, 0, 0, // 3rd Relay 12:00 - 10:15
    0, 0, 0, 0, 0, 0, 0, 0, // 3rd Relay 10:00 - 08:15
    0, 0, 0, 0, 0, 0, 0, 0, // 3rd Relay 08:00 - 06:15
    0, 0, 0, 0, 0, 0, 0, 0, // 3rd Relay 06:00 - 04:15
    0, 0, 0, 0, 0, 0, 0, 0, // 3rd Relay 04:00 - 02:15
    0, 0, 0, 0, 0, 0, 0, 0, // 3rd Relay 02:00 - 00:15
    0                       // 3rd Relay 00:15 - 00:00
  },
  {
    0, 0, 0, 0, 0, 0, 0, 0, // 4th Relay 60:00 - 58:15
    0, 0, 0, 0, 0, 0, 0, 0, // 4th Relay 58:00 - 56:15
    0, 0, 0, 0, 0, 0, 0, 0, // 4th Relay 56:00 - 54:15
    0, 0, 0, 0, 0, 0, 0, 0, // 4th Relay 54:00 - 52:15
    0, 0, 0, 0, 0, 0, 0, 0, // 4th Relay 52:00 - 50:15
    0, 0, 0, 0, 0, 0, 0, 0, // 4th Relay 50:00 - 48:15
    0, 0, 0, 0, 0, 0, 0, 0, // 4th Relay 48:00 - 46:15
    0, 0, 0, 0, 0, 0, 0, 0, // 4th Relay 46:00 - 44:15
    0, 0, 0, 0, 0, 0, 0, 0, // 4th Relay 44:00 - 42:15
    0, 0, 0, 0, 0, 0, 0, 0, // 4th Relay 42:00 - 40:15
    0, 0, 0, 0, 0, 0, 0, 0, // 4th Relay 40:00 - 38:15
    0, 0, 0, 0, 0, 0, 0, 0, // 4th Relay 38:00 - 36:15
    0, 0, 0, 0, 0, 0, 0, 0, // 4th Relay 36:00 - 34:15
    0, 0, 0, 0, 0, 0, 0, 0, // 4th Relay 34:00 - 32:15
    0, 0, 0, 0, 0, 0, 0, 0, // 4th Relay 32:00 - 30:15
    0, 0, 0, 0, 0, 0, 0, 0, // 4th Relay 30:00 - 28:15
    0, 0, 0, 0, 0, 0, 0, 0, // 4th Relay 28:00 - 26:15
    0, 0, 0, 0, 0, 0, 0, 0, // 4th Relay 26:00 - 24:15
    0, 0, 0, 0, 0, 0, 0, 0, // 4th Relay 24:00 - 22:15
    0, 0, 0, 0, 0, 0, 0, 0, // 4th Relay 22:00 - 20:15
    0, 0, 0, 0, 0, 0, 0, 0, // 4th Relay 20:00 - 18:15
    0, 0, 0, 0, 0, 0, 0, 0, // 4th Relay 18:00 - 16:15
    0, 0, 0, 0, 0, 0, 0, 0, // 4th Relay 16:00 - 14:15
    0, 0, 0, 0, 0, 0, 0, 0, // 4th Relay 14:00 - 12:15
    0, 0, 0, 0, 0, 0, 0, 0, // 4th Relay 12:00 - 10:15
    0, 0, 0, 0, 0, 0, 0, 0, // 4th Relay 10:00 - 08:15
    0, 0, 0, 0, 0, 0, 0, 0, // 4th Relay 08:00 - 06:15
    0, 0, 0, 0, 0, 0, 0, 0, // 4th Relay 06:00 - 04:15
    0, 0, 0, 0, 0, 0, 0, 0, // 4th Relay 04:00 - 02:15
    0, 0, 0, 0, 0, 0, 0, 0, // 4th Relay 02:00 - 00:15
    0                       // 4th Relay 00:15 - 00:00
  },
  {
    0, 0, 0, 0, 0, 0, 0, 0, // 5th Relay 60:00 - 58:15
    0, 0, 0, 0, 0, 0, 0, 0, // 5th Relay 58:00 - 56:15
    0, 0, 0, 0, 0, 0, 0, 0, // 5th Relay 56:00 - 54:15
    0, 0, 0, 0, 0, 0, 0, 0, // 5th Relay 54:00 - 52:15
    0, 0, 0, 0, 0, 0, 0, 0, // 5th Relay 52:00 - 50:15
    0, 0, 0, 0, 0, 0, 0, 0, // 5th Relay 50:00 - 48:15
    0, 0, 0, 0, 0, 0, 0, 0, // 5th Relay 48:00 - 46:15
    0, 0, 0, 0, 0, 0, 0, 0, // 5th Relay 46:00 - 44:15
    0, 0, 0, 0, 0, 0, 0, 0, // 5th Relay 44:00 - 42:15
    0, 0, 0, 0, 0, 0, 0, 0, // 5th Relay 42:00 - 40:15
    0, 0, 0, 0, 0, 0, 0, 0, // 5th Relay 40:00 - 38:15
    0, 0, 0, 0, 0, 0, 0, 0, // 5th Relay 38:00 - 36:15
    0, 0, 0, 0, 0, 0, 0, 0, // 5th Relay 36:00 - 34:15
    0, 0, 0, 0, 0, 0, 0, 0, // 5th Relay 34:00 - 32:15
    0, 0, 0, 0, 0, 0, 0, 0, // 5th Relay 32:00 - 30:15
    0, 0, 0, 0, 0, 0, 0, 0, // 5th Relay 30:00 - 28:15
    0, 0, 0, 0, 0, 0, 0, 0, // 5th Relay 28:00 - 26:15
    0, 0, 0, 0, 0, 0, 0, 0, // 5th Relay 26:00 - 24:15
    0, 0, 0, 0, 0, 0, 0, 0, // 5th Relay 24:00 - 22:15
    0, 0, 0, 0, 0, 0, 0, 0, // 5th Relay 22:00 - 20:15
    0, 0, 0, 0, 0, 0, 0, 0, // 5th Relay 20:00 - 18:15
    0, 0, 0, 0, 0, 0, 0, 0, // 5th Relay 18:00 - 16:15
    0, 0, 0, 0, 0, 0, 0, 0, // 5th Relay 16:00 - 14:15
    0, 0, 0, 0, 0, 0, 0, 0, // 5th Relay 14:00 - 12:15
    0, 0, 0, 0, 0, 0, 0, 0, // 5th Relay 12:00 - 10:15
    0, 0, 0, 0, 0, 0, 0, 0, // 5th Relay 10:00 - 08:15
    0, 0, 0, 0, 0, 0, 0, 0, // 5th Relay 08:00 - 06:15
    0, 0, 0, 0, 0, 0, 0, 0, // 5th Relay 06:00 - 04:15
    0, 0, 0, 0, 0, 0, 0, 0, // 5th Relay 04:00 - 02:15
    0, 0, 0, 0, 0, 0, 0, 0, // 5th Relay 02:00 - 00:15
    0                       // 5th Relay 00:15 - 00:00
  }
};

void checkRelays() {
  if (systemState != 1) {
    disableRelays();
  }

  int currentInterval = currentTimerDuration / 120;
  int relayOrder = 0;
  if (timerDuration == 10) {
    for( relayOrder = 0; relayOrder < RELAY_COUNT; relayOrder++) {
      if (relayConf10[relayOrder][currentInterval]) {
        pinMode(RELAY_PINS[relayOrder], OUTPUT);
      } else {
        pinMode(RELAY_PINS[relayOrder], INPUT);
      }
    }
  } else if (timerDuration == 30) {
    for( relayOrder = 0; relayOrder < RELAY_COUNT; relayOrder++) {
      if (relayConf30[relayOrder][currentInterval]) {
        pinMode(RELAY_PINS[relayOrder], OUTPUT);
      } else {
        pinMode(RELAY_PINS[relayOrder], INPUT);
      }
    }
  } else if (timerDuration == 60) {
    for( relayOrder = 0; relayOrder < RELAY_COUNT; relayOrder++) {
      if (relayConf60[relayOrder][currentInterval]) {
        pinMode(RELAY_PINS[relayOrder], OUTPUT);
      } else {
        pinMode(RELAY_PINS[relayOrder], INPUT);
      }
    }
  } else {
    disableRelays();
  }
}

void disableRelays() {
  for ( int relayOrder = 0; relayOrder < 5; relayOrder++ ) {
    pinMode(RELAY_PINS[relayOrder], INPUT);
  }
}
/*--------------------------------------------------------------
 __  __  ____        __ ____  __ 
|  \/  |/ __ \      /_ |___ \/_ |
| \  / | |  | |______| | __) || |
| |\/| | |  | |______| ||__ < | |
| |  | | |__| |      | |___) || |
|_|  |_|\___\_\      |_|____/ |_|
--------------------------------------------------------------*/
void initializeMQ131() {
  MQ131.begin(MQ131_DG_PIN, MQ131_AN_PIN, HIGH_CONCENTRATION, 1000000);
  MQ131.setR0(MQ131_calibrationValue);
  MQ131.setTimeToRead(MQ131_timeToRead);
}
void checkMQ131Data() {
  MQ131.sample();
  float newPPM = MQ131.getO3(PPM);
  if (newPPM == -1.0) {
     return;
  }
  currentPPM = newPPM;
  if (currentPPM > maxPPM) {
    maxPPM = currentPPM;
  }
  totalPPM = totalPPM + currentPPM;
  PPMSampleCount = PPMSampleCount + 1;
  avgPPM = totalPPM / PPMSampleCount;
}
void setO3Texts() {
  if (currentPPM == -1.0) {
    sendToNextion("currentO3Val.txt=","\"Calc...\"");
    sendToNextion("maxO3Val.txt=","\"Calc...\"");
    sendToNextion("avgO3Val.txt=","\"Calc...\"");
    return;
  }
  sendToNextion("currentO3Val.txt=","\"" + String(currentPPM) + "\"");
  sendToNextion("maxO3Val.txt=","\"" + String(maxPPM) + "\"");
  sendToNextion("avgO3Val.txt=","\"" + String(avgPPM) + "\"");
}
