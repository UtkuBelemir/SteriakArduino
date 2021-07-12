#include <MQ131.h>
#include <Nextion.h>
#include <NexButton.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

#define RELAY1_PIN 7
#define RELAY2_PIN 6
#define RELAY3_PIN 5

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

NexButton dr_30 = NexButton(2, 4, "dr_30");
NexButton dr_60 = NexButton(2, 3, "dr_60");
NexButton drSelectBtn = NexButton(2, 2, "drSelectBtn");

NexTouch *nex_listen_list[] = {
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
int RELAY_PINS[3] = {
  RELAY1_PIN,
  RELAY2_PIN,
  RELAY3_PIN,
};

int relayConf30[3][30] = {
  {
    1, 1, 0, 0, 0, 0, 0, 0, 0, 0, // 1st Relay, Mins 30 - 21
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 1st Relay, Mins 20 - 11
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1 // 1st Relay, Mins 10 - 1
  },
  { 
    0, 0, 0, 1, 1, 1, 0, 0, 1, 1, // 2nd Relay, Mins 30 - 21
    1, 0, 0, 1, 1, 1, 0, 0, 1, 1, // 2nd Relay, Mins 20 - 11
    1, 0, 0, 0, 1, 1, 1, 1, 0, 0 // 2nd Relay, Mins 10 - 1
  },
  { 
    0, 0, 1, 1, 1, 1, 1, 1, 1, 1, // 3rd Relay, Mins 30 - 21
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 3rd Relay, Mins 20 - 11
    1, 1, 0, 0, 0, 0, 0, 0, 0, 0 // 3rd Relay, Mins 10 - 1
  }
};

int relayConf60[3][60] = {
  {
    1, 1, 0, 0, 0, 0, 0, 0, 0, 0, // 1st Relay, Mins 60 - 51
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 1st Relay, Mins 50 - 41
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, // 1st Relay, Mins 40 - 31
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 1st Relay, Mins 30 - 21
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 1st Relay, Mins 20 - 11
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1 // 1st Relay, Mins 10 - 1
  },
  { 
    0, 0, 0, 1, 1, 0, 0, 1, 1, 1, // 2nd Relay, Mins 60 - 51
    0, 0, 1, 1, 1, 0, 0, 1, 1, 1, // 2nd Relay, Mins 50 - 41
    0, 0, 0, 1, 1, 1, 1, 0, 0, 1, // 2nd Relay, Mins 40 - 31
    1, 1, 1, 0, 0, 1, 1, 1, 0, 0, // 2nd Relay, Mins 30 - 21
    0, 1, 1, 1, 0, 0, 1, 1, 1, 0, // 2nd Relay, Mins 20 - 11
    0, 0, 1, 1, 1, 1, 0, 0, 0, 0 // 2nd Relay, Mins 10 - 1
  },
  { 
    0, 0, 1, 1, 1, 1, 1, 1, 1, 1, // 3rd Relay, Mins 60 - 51
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 3rd Relay, Mins 50 - 41
    1, 1, 0, 0, 0, 0, 0, 0, 0, 0, // 3rd Relay, Mins 40 - 31
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 3rd Relay, Mins 30 - 21
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 3rd Relay, Mins 20 - 11
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0 // 3rd Relay, Mins 10 - 1
  }
};

void checkRelays() {
  if (systemState != 1) {
    disableRelays();
  }
  int currentMinute = timerDuration  - (currentTimerDuration / 60);
  int relayOrder = 0;
  if (currentTimerDuration == 0) {
    currentMinute = 29;
  }
  if (currentTimerDuration == timerDuration * 60) {
    currentMinute = 0;
  }
  // Serial.println(currentMinute);
  if (timerDuration == 30) {
    for( relayOrder = 0; relayOrder < 3; relayOrder++) {
      if (relayConf30[relayOrder][currentMinute]) {
        pinMode(RELAY_PINS[relayOrder], OUTPUT);
      } else {
        pinMode(RELAY_PINS[relayOrder], INPUT);
      }
    }
  } else if (timerDuration == 60) {
    for( relayOrder = 0; relayOrder < 3; relayOrder++) {
      if (relayConf60[relayOrder][currentMinute]) {
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
  for ( int relayOrder = 0; relayOrder < 3; relayOrder++ ) {
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
