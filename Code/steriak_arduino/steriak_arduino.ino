#include <RTClib.h>
#include <Nextion.h>
#include <NexButton.h>

#define RELAY_NEGION 7
#define RELAY_OZONE 6
#define RELAY_FAN 5
#define RELAY_UVC 4
#define RELAY_BUZZER 3
#define RELAY_OFFTURNER 2

#define PAGEID_INTRO 0
#define PAGEID_INITAL 1
#define PAGEID_MENU 2
#define PAGEID_SETTINGS 3
#define PAGEID_LANGUAGE 4
#define PAGEID_CYCLE 5
#define PAGEID_DISINF 6
#define PAGEID_STERILI 7
#define PAGEID_STARTING 8
#define PAGEID_RUNNING 9

#define SYSTEM_IDLE 0
#define SYSTEM_RUNNING 1
#define SYSTEM_FINISHED 2

#define PROG_RELAY_COUNT 4

// #define HEATSENSOR_PIN 9
// #define BUZZER_PIN 9
#define BUZZER_NOTE 440

/*
- 5 nolu Ses rölesi start a, cancel e dokununca 3 saniye açacak, bir de her döngü bittiğinde 8 saniye açacak.
- 6. Kapatma rölesi : döngü bittiğinde, ekrana dokunulmadığı sürece 15 dakika sayıp 1-2 saniye açacak darbe akım şalterine sinyal göndererek, sistemi kapatacak. yani bir nevi otomatik kapama yapacak. bu konuyla ilgili konuşalım.

*/

RTC_DS3231 permanentClock;

int timerDuration = 0, systemState = SYSTEM_IDLE, relayCheckInterval = 60, currentPageID = PAGEID_INTRO;
unsigned long currentTimerDuration = 0;
unsigned long startMillis, currentMillis;
unsigned long runTime = 0;
const unsigned long ONE_SEC = 10;
const unsigned long ONE_MIN = ONE_SEC*60;
unsigned long MS_buzzer;

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

NexButton slctCycleBtn = NexButton(1, 3, "slctCycleBtn");
NexButton menuBtn = NexButton(1, 4, "menuBtn");
NexButton mnTrnsBtn = NexButton(2, 1, "mnTrnsBtn");
NexButton mnHomeBtn = NexButton(2, 2, "mnHomeBtn");
NexButton mnDevSetBtn = NexButton(2, 3, "mnDevSetBtn");
NexButton setLNGBtn = NexButton(3, 1, "setLNGBtn");
NexButton setBackBtn = NexButton(3, 2, "setBackBtn");
NexButton setTIMEBtn = NexButton(3, 3, "setTIMEBtn");
NexButton lngTRBtn = NexButton(4, 1, "lngTRBtn");
NexButton lngBackBtn = NexButton(4, 2, "lngBackBtn");
NexButton lngENBtn = NexButton(4, 3, "lngENBtn");
NexButton cycleDisBtn = NexButton(5, 1, "cycleDisBtn");
NexButton cycleHomeBtn = NexButton(5, 2, "cycleHomeBtn");
NexButton cycleSterBtn = NexButton(5, 3, "cycleSterBtn");
NexButton disinfLowBtn = NexButton(6, 1, "disinfLowBtn");
NexButton disinfBackBtn = NexButton(6, 2, "disinfBackBtn");
NexButton disinfNormalBtn = NexButton(6, 3, "disinfNormalBtn");
NexButton disinfHighBtn = NexButton(6, 4, "disinfHighBtn");
NexButton sterStdBtn = NexButton(7, 1, "sterStdBtn");
NexButton sterSpeedBtn = NexButton(7, 2, "sterSpeedBtn");
NexButton sterBackBtn = NexButton(7, 3, "sterBackBtn");
NexButton sterPouchBtn = NexButton(7, 4, "sterPouchBtn");
NexButton startBackBtn = NexButton(8, 1, "startBackBtn");
NexButton startStartBtn = NexButton(8, 2, "startStartBtn");
NexButton runAbortBtn = NexButton(9, 11, "runAbortBtn");
NexButton runCompleteBtn = NexButton(9, 9, "runCompleteBtn");
NexButton runHomeBtn = NexButton(9, 10, "runHomeBtn");

NexTouch *nex_listen_list[] = {
  &slctCycleBtn,
  &menuBtn,
  &mnTrnsBtn,
  &mnHomeBtn,
  &mnDevSetBtn,
  &setLNGBtn,
  &setBackBtn,
  &setTIMEBtn,
  &lngTRBtn,
  &lngBackBtn,
  &lngENBtn,
  &cycleDisBtn,
  &cycleHomeBtn,
  &cycleSterBtn,
  &disinfLowBtn,
  &disinfBackBtn,
  &disinfNormalBtn,
  &disinfHighBtn,
  &sterStdBtn,
  &sterSpeedBtn,
  &sterBackBtn,
  &sterPouchBtn,
  &startBackBtn,
  &startStartBtn,
  &runAbortBtn,
  &runCompleteBtn,
  &runHomeBtn,
  NULL
};

String getHourPartVal() {
  if (currentTimerDuration <= 0) {
    currentTimerDuration = 0;
    return "0";
  }
  return String((int)(currentTimerDuration / 3600));
}
String getMinPartVal() {
  if (currentTimerDuration <= 0) {
    currentTimerDuration = 0;
    return "0";
  }
  return String(((int)(currentTimerDuration / 60)) % 60);
}
String getSecPartVal() {
  if (currentTimerDuration <= 0) {
    currentTimerDuration = 0;
    return "0";
  }
  return String(currentTimerDuration % 60);
}

void updateCurrentPageID(int pg_id) {
  currentPageID = pg_id;
}

void goToPreStart(int dur) {
  updateCurrentPageID(PAGEID_STARTING);
  timerDuration = dur;
  currentTimerDuration = timerDuration * 60;
  Serial.println("Current Timer Dur:" + String(currentTimerDuration));
  sendToNextion("startingPage.startHrPart.val=" + getHourPartVal(), "");
  sendToNextion("startingPage.startMinPart.val=" + getMinPartVal(), "");
  sendToNextion("startingPage.startSecPart.val=" + getSecPartVal(), "");
  sendToNextion("page startingPage", "");
}

void disinfLowBtn_Callback(void *ptr) {
  goToPreStart(10);
}

void disinfNormalBtn_Callback(void *ptr) {
  goToPreStart(15);
}

void disinfHighBtn_Callback(void *ptr) {
  goToPreStart(20);
}

void sterSpeedBtn_Callback(void *ptr) {
  goToPreStart(90);
}

void sterStdBtn_Callback(void *ptr) {
  goToPreStart(120);
}

void sterPouchBtn_Callback(void *ptr) {
  goToPreStart(180);
}

void menuBtn_Callback(void *ptr) {
  updateCurrentPageID(PAGEID_MENU);
  sendToNextion("page menuPage", "");
}

void mnTrnsBtn_Callback(void *ptr) {
  // TODO :: Set Page To Transfer Data
}

void mnDevSetBtn_Callback(void *ptr) {
  updateCurrentPageID(PAGEID_SETTINGS);
  sendToNextion("page settingsPage","");
}

void mnHomeBtn_Callback(void *ptr) {
  updateCurrentPageID(PAGEID_INITAL);
  sendToNextion("page initialPage","");
}

void setTIMEBtn_Callback(void *ptr) {
  // TODO
}

void setLNGBtn_Callback(void *ptr) {
  updateCurrentPageID(PAGEID_LANGUAGE);
  sendToNextion("page languagePage", "");
}

void setBackBtn_Callback(void *ptr) {
  updateCurrentPageID(PAGEID_MENU);
  sendToNextion("page menuPage", "");
}

void lngTRBtn_Callback(void *ptr) {
  // TODO
}

void lngENBtn_Callback(void *ptr) {
  // TODO
}

void lngBackBtn_Callback(void *ptr) {
  updateCurrentPageID(PAGEID_SETTINGS);
  sendToNextion("page settingsPage", "");
}

void slctCycleBtn_Callback(void *ptr) {
  updateCurrentPageID(PAGEID_CYCLE);
  sendToNextion("page cyclePage", "");
}

void cycleDisBtn_Callback(void *ptr) {
  updateCurrentPageID(PAGEID_DISINF);
  sendToNextion("page disinfPage", "");
}

void cycleSterBtn_Callback(void *ptr) {
  updateCurrentPageID(PAGEID_STERILI);
  sendToNextion("page steriliPage", "");
}

void cycleHomeBtn_Callback(void *ptr) {
  updateCurrentPageID(PAGEID_INITAL);
  sendToNextion("page initialPage", "");
}

void disinfBackBtn_Callback(void *ptr) {
  updateCurrentPageID(PAGEID_CYCLE);
  sendToNextion("page cyclePage", "");
}

void sterBackBtn_Callback(void *ptr) {
  updateCurrentPageID(PAGEID_CYCLE);
  sendToNextion("page cyclePage", "");
}

void startStartBtn_Callback(void *ptr) {
  updateCurrentPageID(PAGEID_RUNNING);
  updateNextionTimer();
  sendToNextion("runningPage.runTxt1.txt=", "\"The cycle continues\"");
  sendToNextion("runningPage.runTxt2.txt=", "\"Do not open the cover and\"");
  sendToNextion("runningPage.runTxt3.txt=", "\"do not cancel unless its an emergency.\"");
  sendToNextion("page runningPage", "");
  sendToNextion("vis runAbortBtn,1", "");
  sendToNextion("vis runCompleteBtn,0", "");
  sendToNextion("vis runHomeBtn,0", "");
  systemState = SYSTEM_RUNNING;
}

void startBackBtn_Callback(void *ptr) {
  updateCurrentPageID(PAGEID_CYCLE);
  sendToNextion("page cyclePage", "");
  timerDuration = 0;
}

void runAbortBtn_Callback(void *ptr) {
  // TODO :: Calculate runtime
  disableRelays();
  systemState = SYSTEM_IDLE;
  sendToNextion("runningPage.runTxt1.txt=", "\"The cycle is aborted.\"");
  sendToNextion("runningPage.runTxt2.txt=", "\"Do not open the cover\"");
  sendToNextion("runningPage.runTxt3.txt=", "\"without ventilating the room.\"");
  sendToNextion("vis runAbortBtn,0", "");
  sendToNextion("vis runCompleteBtn,0", "");
  sendToNextion("vis runHomeBtn,1", "");
}

/* void runCompleteBtn_Callback(void *ptr) {
  currentTimerDuration = 0;
  updateCurrentPageID(PAGEID_INITAL);
  sendToNextion("page initialPage", "");
} */

void runHomeBtn_Callback(void *ptr) {
  currentTimerDuration = 0;
  updateCurrentPageID(PAGEID_INITAL);
  sendToNextion("page initialPage", "");
}

/* void drSelectBtnPushCallback(void *ptr) {
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
} */

/* void startBtnPushCallback(void *ptr) {
  systemState = 1;
  startMillis = millis();
  sendToNextion("vis menuBtn,0","");
  sendToNextion("vis startBtn,0","");
  sendToNextion("vis stopBtn,1","");
  sendToNextion("mainStatusText.txt=","\"sterilizing...\"");
} */

/* void stopBtnPushCallback(void *ptr) {
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
} */

/* void completeBtnPushCallback(void *ptr) {
  systemState = SYSTEM_IDLE;
  timerDuration = 0;
  currentTimerDuration = 0;
  sendToNextion("vis menuBtn,1","");
  sendToNextion("vis stopBtn,0","");
  sendToNextion("vis printBtn,0","");
  sendToNextion("vis completeBtn,0","");
  sendToNextion("mainStatusText.txt=","\"waiting...\"");
  sendToNextion("mainStatusText.pco=1663","");
} */

void attachButtons() {
  disinfLowBtn.attachPush(disinfLowBtn_Callback, &disinfLowBtn);
  disinfNormalBtn.attachPush(disinfNormalBtn_Callback, &disinfNormalBtn);
  disinfHighBtn.attachPush(disinfHighBtn_Callback, &disinfHighBtn);
  sterSpeedBtn.attachPush(sterSpeedBtn_Callback, &sterSpeedBtn);
  sterStdBtn.attachPush(sterStdBtn_Callback, &sterStdBtn);
  sterPouchBtn.attachPush(sterPouchBtn_Callback, &sterPouchBtn);
  menuBtn.attachPush(menuBtn_Callback, &menuBtn);
  mnTrnsBtn.attachPush(mnTrnsBtn_Callback, &mnTrnsBtn);
  mnDevSetBtn.attachPush(mnDevSetBtn_Callback, &mnDevSetBtn);
  mnHomeBtn.attachPush(mnHomeBtn_Callback, &mnHomeBtn);
  setTIMEBtn.attachPush(setTIMEBtn_Callback, &setTIMEBtn);
  setLNGBtn.attachPush(setLNGBtn_Callback, &setLNGBtn);
  setBackBtn.attachPush(setBackBtn_Callback, &setBackBtn);
  lngTRBtn.attachPush(lngTRBtn_Callback, &lngTRBtn);
  lngENBtn.attachPush(lngENBtn_Callback, &lngENBtn);
  lngBackBtn.attachPush(lngBackBtn_Callback, &lngBackBtn);
  slctCycleBtn.attachPush(slctCycleBtn_Callback, &slctCycleBtn);
  cycleDisBtn.attachPush(cycleDisBtn_Callback, &cycleDisBtn);
  cycleSterBtn.attachPush(cycleSterBtn_Callback, &cycleSterBtn);
  cycleHomeBtn.attachPush(cycleHomeBtn_Callback, &cycleHomeBtn);
  disinfBackBtn.attachPush(disinfBackBtn_Callback, &disinfBackBtn);
  sterBackBtn.attachPush(sterBackBtn_Callback, &sterBackBtn);
  startStartBtn.attachPush(startStartBtn_Callback, &startStartBtn);
  startBackBtn.attachPush(startBackBtn_Callback, &startBackBtn);
  runAbortBtn.attachPush(runAbortBtn_Callback, &runAbortBtn);
  runCompleteBtn.attachPush(runHomeBtn_Callback, &runCompleteBtn);
  runHomeBtn.attachPush(runHomeBtn_Callback, &runHomeBtn);
}

void setup() {
  Serial.begin(9600);
  Serial2.begin(9600);
  nexInit();
  sendToNextion("introText.txt=","\"Initializing Relays...\"");
  setupRelays();
  delay(500);
  sendToNextion("introText.txt=","\"Relay initialized!\"");
  sendToNextion("introText.txt=","\"Initializing System Clock...\"");
  initializeClock();
  delay(500);
  sendToNextion("introText.txt=","\"System Clock initialized!\"");
  sendToNextion("introText.txt=","\"Initializing Screen Components...\"");
  attachButtons();
  sendToNextion("introText.txt=","\"Initialization Done. Starting...\"");
  delay(500);
  updateCurrentPageID(PAGEID_INITAL);
  sendToNextion("page initialPage","");
}

void loop() {
  nexLoop(nex_listen_list);
  currentMillis = millis();
  if (systemState == SYSTEM_RUNNING) {
    if (currentMillis - startMillis >= ONE_SEC) {
      if (currentTimerDuration <= 0) {
        finishSterilizing();
      } else {
        checkRelays();
        currentTimerDuration = currentTimerDuration - 1;
        startMillis = currentMillis;
      }
      updateNextionTimer();
    }
  } else if (currentPageID == PAGEID_INITAL && currentMillis % ONE_MIN == 0) {
    updateInitialPageGlobals();
  }
}

void updateNextionTimer() {
  sendToNextion("runningPage.runHrPart.val="+getHourPartVal(),"");
  sendToNextion("runningPage.runMinPart.val="+getMinPartVal(),"");
  sendToNextion("runningPage.runSecPart.val="+getSecPartVal(),"");
}

void updateInitialPageGlobals() {
  sendToNextion("dstClock.txt=","\"11:00\"");
  sendToNextion("dstDate.txt=","\"12 Nis 1996\"");
  sendToNextion("runtimeHr.txt", "\"00001\"");
  sendToNextion("runtimeMin.txt", "\"01\"");
}

void finishSterilizing() {
  systemState = SYSTEM_FINISHED;
  // TODO :: Calculate runtime
  disableRelays();
  sendToNextion("runningPage.runHrPart.val=0","");
  sendToNextion("runningPage.runMinPart.val=0","");
  sendToNextion("runningPage.runSecPart.val=0","");
  sendToNextion("runningPage.runTxt1.txt=", "\"The cycle is finished.\"");
  sendToNextion("runningPage.runTxt2.txt=", "\"Leave the cover open at the end of\"");
  sendToNextion("runningPage.runTxt3.txt=", "\"each cycle to allow the device to cool.\"");
  sendToNextion("vis runCompleteBtn,1", "");
  sendToNextion("vis runAbortBtn,0", "");
  sendToNextion("vis runHomeBtn,0", "");
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
int RELAY_PINS[PROG_RELAY_COUNT] = {
  RELAY_NEGION,
  RELAY_OZONE,
  RELAY_FAN,
  RELAY_UVC,
};

int relayConf10[PROG_RELAY_COUNT][10] = {
  {
    1, 0, 1, 0, 1,
    0, 1, 0, 1, 1
  },
  {
    1, 0, 1, 0, 1,
    0, 1, 0, 1, 1
  },
  {
    1, 0, 1, 0, 1,
    0, 1, 0, 1, 1
  },
  {
    1, 0, 1, 0, 1,
    0, 1, 0, 1, 1
  },
};

int relayConf15[PROG_RELAY_COUNT][15] = {
  {
    1, 0, 1, 0, 1,
    0, 1, 0, 1, 1,
    1, 1, 1, 1, 1
  },
  {
    1, 0, 1, 0, 1,
    0, 1, 0, 1, 1,
    1, 1, 1, 1, 1
  },
  {
    1, 0, 1, 0, 1,
    0, 1, 0, 1, 1,
    1, 1, 1, 1, 1
  },
  {
    1, 0, 1, 0, 1,
    0, 1, 0, 1, 1,
    1, 1, 1, 1, 1
  },
};

int relayConf20[PROG_RELAY_COUNT][20] = {
  {
    1, 0, 1, 0, 1,
    0, 1, 0, 1, 1,
    1, 1, 1, 1, 1,
    1, 1, 1, 1, 1
  },
  {
    1, 0, 1, 0, 1,
    0, 1, 0, 1, 1,
    1, 1, 1, 1, 1,
    1, 1, 1, 1, 1
  },
  {
    1, 0, 1, 0, 1,
    0, 1, 0, 1, 1,
    1, 1, 1, 1, 1,
    1, 1, 1, 1, 1
  },
  {
    1, 0, 1, 0, 1,
    0, 1, 0, 1, 1,
    1, 1, 1, 1, 1,
    1, 1, 1, 1, 1
  },
};

int relayConf90[PROG_RELAY_COUNT][90] = {
  {
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1
  },
  {
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1
  },
  {
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1
  },
  {
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1
  },
};

int relayConf120[PROG_RELAY_COUNT][120] = {
  {
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1
  },
  {
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1
  },
  {
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1
  },
  {
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1
  },
};

int relayConf180[PROG_RELAY_COUNT][180] = {
  {
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1
  },
  {
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1
  },
  {
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1
  },
  {
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 1
  },
};


void setupRelays() {
  for ( int relayIndex = 0; relayIndex < PROG_RELAY_COUNT; relayIndex++) {
    pinMode(RELAY_PINS[relayIndex], OUTPUT);
    digitalWrite(RELAY_PINS[relayIndex], LOW);
  }
  pinMode(RELAY_BUZZER, OUTPUT);
  digitalWrite(RELAY_BUZZER, LOW);

  pinMode(RELAY_OFFTURNER, OUTPUT);
  digitalWrite(RELAY_OFFTURNER, LOW);
}

void checkRelays() {
  if (systemState != SYSTEM_RUNNING) {
    disableRelays();
  }

  int currentInterval = ((timerDuration * 60) - currentTimerDuration) / relayCheckInterval;
  int relayOrder = 0;
  if (timerDuration == 10) {
    for( relayOrder = 0; relayOrder < PROG_RELAY_COUNT; relayOrder++) {
      if (relayConf10[relayOrder][currentInterval]) {
        Serial.println("Role Calısıyor");
        digitalWrite(RELAY_PINS[relayOrder], HIGH);
      } else {
        Serial.println("Role Kapalı");
        digitalWrite(RELAY_PINS[relayOrder], LOW);
      }
    }
  } else if (timerDuration == 15) {
    for( relayOrder = 0; relayOrder < PROG_RELAY_COUNT; relayOrder++) {
      if (relayConf15[relayOrder][currentInterval]) {
        digitalWrite(RELAY_PINS[relayOrder], HIGH);
      } else {
        digitalWrite(RELAY_PINS[relayOrder], LOW);
      }
    }
  } else if (timerDuration == 20) {
    for( relayOrder = 0; relayOrder < PROG_RELAY_COUNT; relayOrder++) {
      if (relayConf20[relayOrder][currentInterval]) {
        digitalWrite(RELAY_PINS[relayOrder], HIGH);
      } else {
        digitalWrite(RELAY_PINS[relayOrder], LOW);
      }
    }
  } else if (timerDuration == 90) {
    for( relayOrder = 0; relayOrder < PROG_RELAY_COUNT; relayOrder++) {
      if (relayConf90[relayOrder][currentInterval]) {
        digitalWrite(RELAY_PINS[relayOrder], HIGH);
      } else {
        digitalWrite(RELAY_PINS[relayOrder], LOW);
      }
    }
  } else if (timerDuration == 120) {
    for( relayOrder = 0; relayOrder < PROG_RELAY_COUNT; relayOrder++) {
      if (relayConf120[relayOrder][currentInterval]) {
        digitalWrite(RELAY_PINS[relayOrder], HIGH);
      } else {
        digitalWrite(RELAY_PINS[relayOrder], LOW);
      }
    }
  } else if (timerDuration == 180) {
    for( relayOrder = 0; relayOrder < PROG_RELAY_COUNT; relayOrder++) {
      if (relayConf180[relayOrder][currentInterval]) {
        digitalWrite(RELAY_PINS[relayOrder], HIGH);
      } else {
        digitalWrite(RELAY_PINS[relayOrder], LOW);
      }
    }
  } else {
    disableRelays();
  }
}

void disableRelays() {
  for ( int relayIndex = 0; relayIndex < PROG_RELAY_COUNT; relayIndex++) {
    digitalWrite(RELAY_PINS[relayIndex], LOW);
  }
}

/*---Section: Clock--------------------------------------------
  _____ _            _
 / ____| |          | |
| |    | | ___   ___| | __
| |    | |/ _ \ / __| |/ /
| |____| | (_) | (__|   <
 \_____|_|\___/ \___|_|\_\
--------------------------------------------------------------*/
void initializeClock() {
  // permanentClock.begin();
  // adjustTime();
  // permanentClock.disable32K();
}

void adjustTime() {
  permanentClock.adjust(DateTime(F(__DATE__), F(__TIME__)));
}

String getDate() {
  DateTime tempNow = permanentClock.now();
  char result[10];
  sprintf(result, "%04d-%02d-%02d", tempNow.year(), tempNow.month(), tempNow.day());
  return String(result);
}

String getTime() {
  DateTime tempNow = permanentClock.now();
  char result[10];
  sprintf(result, "%02d:%02d", tempNow.hour(), tempNow.minute());
  return String(result);
}
