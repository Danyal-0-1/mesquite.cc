#define LILYGO_WATCH_2019_WITH_TOUCH
#include <LilyGoWatch.h> \\https://github.com/Xinyuan-LilyGO/TTGO_TWatch_Library/tree/master

TTGOClass *watch;
TFT_eSPI *tft;
bool isCharging = false;


const char *mac_address_str = "DC:DA:0C:17:10:A0";
uint8_t broadcastAddress[6];

// Array of arrays containing 2 strings each
String boneName[][2] = {
  { "HEAD", "" },
  { "SPINE", "" },
  { "HIPS", "" },
  { "LEFT", "UP ARM" },
  { "LEFT", "FOREARM" },
  { "LEFT", "HAND" },
  { "RIGHT", "UP ARM" },
  { "RIGHT", "FOREARM" },
  { "RIGHT", "HAND" },
  { "LEFT", "UP LEG" },
  { "LEFT", "LOW LEG" },
  { "LEFT", "FOOT" },
  { "RIGHT", "UP LEG" },
  { "RIGHT", "LOW LEG" },
  { "RIGHT", "FOOT" },
  { "LEFT",  "SHOULDER" },   // 15
  { "RIGHT", "SHOULDER" },   // 16
};


// =========================================================================
//  POD IDENTITY  --  Phase 2, addresses NODE-05 (promoted to BLOCKER in §4.2)
//
//  The bone id is now supplied by the BUILD, not by editing comments here.
//  Phase 1 found the id was chosen by uncommenting one of 17 mutually
//  exclusive lines; flashing a 17-device fleet three times that way will
//  eventually produce a duplicate sendID, which is SILENT at runtime and
//  presents as "a node didn't connect".
//
//  Build one image per pod:
//      arduino-cli compile \
//        --build-property "build.extra_flags=-DMESQ_POD_ID=3" ...
//  or run  tools/build_pods.sh  which generates all 17.
//
//  If MESQ_POD_ID is not defined the build FAILS. That is deliberate: a
//  build error is recoverable in seconds, a duplicate id costs a session.
// =========================================================================
#ifndef MESQ_POD_ID
#error "MESQ_POD_ID is not defined. Build with -DMESQ_POD_ID=<0..16> (see tools/build_pods.sh). Phase 2 removed the comment-toggle identity block; see system_assessment_2/ROLLOUT.md"
#endif
#if (MESQ_POD_ID < 0) || (MESQ_POD_ID > 16)
#error "MESQ_POD_ID out of range - valid bone ids are 0..16 (see boneName[] above)"
#endif

// Screen colours per bone id, transcribed verbatim from the Phase 1
// comment block so the on-watch appearance is unchanged.
static const uint16_t POD_BG[17] = {
  0xffff, 0xffff, 0xffff,          //  0 Head, 1 Spine, 2 HipsAlt
  0x62d6, 0x62d6, 0x62d6,          //  3-5   left arm chain
  0xf720, 0xf720, 0xf720,          //  6-8   right arm chain
  0xc086, 0xc086, 0xc086,          //  9-11  left leg chain
  0x3d89, 0x3d89, 0x3d89,          // 12-14  right leg chain
  0x62d6, 0xf720                   // 15 LeftShoulder, 16 RightShoulder
};
static const uint16_t POD_FG[17] = {
  0x0000, 0x0000, 0x0000,
  0xffff, 0xffff, 0xffff,
  0x0000, 0x0000, 0x0000,
  0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff,
  0xffff, 0x0000
};

const int sendID = MESQ_POD_ID;
uint16_t BG = POD_BG[MESQ_POD_ID];
uint16_t FG = POD_FG[MESQ_POD_ID];




#include <esp_now.h>
#include <esp_system.h>  // Phase 2 W0: esp_reset_reason(), esp_get_idf_version()
#include <esp_wifi.h>   // Needed for esp_wifi_set_channel / esp_wifi_set_ps /
                        // esp_wifi_set_max_tx_power. Without these the radio
                        // floats to whatever channel the environment pushes
                        // it to, which is exactly how the Tempe->Boston
                        // regression happened.
#include <EEPROM.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WebSocketsClient.h>  //https://github.com/Links2004/arduinoWebSockets
#include <ESPmDNS.h>
#include "ICM_20948.h"  // Click here to get the library: http://librarymanager/All#SparkFun_ICM_20948_IMU
#define AD0_VAL 0

// ===========================================================================
//  RADIO CONFIG -- MUST MATCH THE DONGLE
//  Both pods and dongle must run on the same WiFi channel for ESP-NOW to
//  deliver any packets. Without an explicit lock, the radio uses whatever
//  channel WiFi.mode(WIFI_STA) defaulted to (usually 1), but the dongle's
//  softAP can land on a different channel depending on local 2.4 GHz noise.
//  Lock both sides to a single, fixed channel. Channel 1 is a safe default
//  but if your client site has a heavy 2.4 GHz AP on ch 1, you can move to
//  6 or 11 -- the value just has to match the dongle.
// ===========================================================================
#define ESPNOW_WIFI_CHANNEL 1

// =========================================================================
//  PHASE 2 INSTRUMENTATION  (W1)  -- observational only, no behaviour change
//  Build with -DMESQ_INSTR=1 to enable. With it 0 (default) every hook
//  compiles to nothing and the image is behaviourally identical to Phase 1.
//
//  I9  read duration + Quat6/s   -> SENS-01 (~55 Hz DMP ceiling), SENS-02
//  I4  send-interval histogram   -> NODE-03 (tick quantisation, S2)
//  I8  reset reason + boot count -> NODE-04 (init hang, S3/S4)
//  I11 free heap                 -> fragmentation
//  N1  unit-norm violations      -> NODE-01 (torn cross-core quaternion)
//  N2  negative sqrt radicand    -> SENS-03 (NaN -> zero quaternion)
//  N3  sample-to-send age        -> NODE-02 / SYNC-02 (stamp at transmit)
// =========================================================================
#ifndef MESQ_INSTR
#define MESQ_INSTR 0
#endif

#if MESQ_INSTR
#include <esp_timer.h>
RTC_DATA_ATTR uint32_t mesq_bootCount = 0;   // survives reset, not power loss

// I9
static volatile uint32_t mesq_readN = 0, mesq_readMin = 0xFFFFFFFF,
                         mesq_readMax = 0; static volatile uint64_t mesq_readSum = 0;
static volatile uint32_t mesq_quat6N = 0;    // Quat6 FIFO packets this second
static volatile uint32_t mesq_fifoMoreN = 0; // reads reporting FIFOMoreDataAvail
// I4  - send intervals bucketed in ms: <20,20-29,30-39,40-49,50-59,60+
static volatile uint32_t mesq_sendBuckets[6] = {0,0,0,0,0,0};
static volatile uint32_t mesq_sendN = 0, mesq_sendMin = 0xFFFFFFFF, mesq_sendMax = 0;
// N1/N2/N3
static volatile uint32_t mesq_normBad = 0, mesq_radNeg = 0;
static volatile uint32_t mesq_ageN = 0, mesq_ageMax = 0; static volatile uint64_t mesq_ageSum = 0;
static volatile int64_t  mesq_lastSampleUs = 0;
// instrumentation self-cost (Rule 2)
static volatile uint64_t mesq_instrCostUs = 0;

static inline void mesq_bucketSend(uint32_t ms) {
  uint8_t b = (ms < 20) ? 0 : (ms < 30) ? 1 : (ms < 40) ? 2
            : (ms < 50) ? 3 : (ms < 60) ? 4 : 5;
  mesq_sendBuckets[b]++;
}
#endif

//#include "soc/rtc_wdt.h"
ICM_20948_I2C myICM;  // Otherwise create an ICM_20948_I2C object

//#include "Button2.h"
#define BUTTON_PIN 5
//Button2 button;

#include "esp_adc_cal.h"
#define BAT_ADC 35

// ---- Haptic (T-Watch 2019 Standard base plate) ----
// Motor lives on the base plate, GPIO 33. Power rail is AXP202 LDO3.
#define MOTOR_PIN 33
#define MOTOR_PULSE_MS 80

// ---- Power button ----
// T-Watch 2019 physical side button routed to GPIO 36 (input-only on ESP32).
#define PWR_BTN_PIN 36
#define LONG_PRESS_MS 2000

int fcount = 0;
int dccount = 0;
int count = 0;

void mac_string_to_uint8_array(const char *mac_str, uint8_t *mac_array) {
  if (mac_str == NULL || mac_array == NULL) {
    return;
  }

  int values[6];  // Temporary storage for parsed hexadecimal values
  int result = sscanf(mac_str, "%x:%x:%x:%x:%x:%x",
                      &values[0], &values[1], &values[2],
                      &values[3], &values[4], &values[5]);

  if (result != 6) {
    return;
  }

  // Convert parsed integer values to uint8_t
  for (int i = 0; i < 6; ++i) {
    mac_array[i] = (uint8_t)values[i];
  }

  return;
}




int lastOn = millis();
bool isOn = true;

int lastTouch = millis();

// =========================================================================
//  BINARY WIRE FORMAT  (16 bytes)
//  Replaces the old struct_message with a packed, fixed-size frame so the
//  payload is small (~9x smaller than the JSON the dongle used to print) and
//  free of the well-known "String inside a memcpy'd struct" heap-pointer bug.
//
//   off  size  field
//   0    1     sync0   = 0xAA
//   1    1     sync1   = 0x55
//   2    1     id      bone enum (see table at top of file)
//   3    1     batt    0..100 (%)
//   4    2     qx_i16  quaternion.x * 32767
//   6    2     qy_i16
//   8    2     qz_i16
//   10   2     qw_i16
//   12   2     count   uint16, wraps
//   14   2     ms_lo   low 16 bits of millis() at send time
// =========================================================================
typedef struct __attribute__((packed)) pod_packet_t {
  uint8_t  sync0;
  uint8_t  sync1;
  uint8_t  id;
  uint8_t  batt;
  int16_t  qx;
  int16_t  qy;
  int16_t  qz;
  int16_t  qw;
  uint16_t count;
  uint16_t ms_lo;
} pod_packet_t;

static_assert(sizeof(pod_packet_t) == 16, "pod_packet_t must be exactly 16 bytes");

pod_packet_t myData;

// Quantize a float in [-1, 1] to int16. Saturates rather than wrapping so an
// out-of-range value (e.g. NaN coerced to a huge number) doesn't flip sign.
static inline int16_t q_to_i16(float v) {
  if (v >  1.0f) v =  1.0f;
  if (v < -1.0f) v = -1.0f;
  if (isnan(v))  v = 0.0f;
  return (int16_t)(v * 32767.0f);
}

// Create peer interface
esp_now_peer_info_t peerInfo;

// callback when data is sent
// NOTE: ESP-NOW callback signature changed between ESP32 Arduino core 2.x and 3.x.
// 2.x: void(const uint8_t *mac_addr, esp_now_send_status_t status)
// 3.x: void(const wifi_tx_info_t *tx_info, esp_now_send_status_t status)
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
void OnDataSent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status) {
#else
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
#endif
  // Serial.print("\r\nLast Packet Send Status:\t");
 // Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");


  if (status == ESP_NOW_SEND_SUCCESS) {
    dccount = 0;
  } else {
    dccount++;
    //digitalWrite(3, HIGH);
    //Serial.println(dccount);
    // No-link auto-shutdown. Was 960 (~30 s @ 32 fps) which is brutal in a
    // congested 2.4 GHz environment -- a brief WiFi storm could shut every
    // pod down mid-capture. Raised to ~5 minutes; if the dongle is really
    // gone the pod still shuts itself down to save the battery, but normal
    // hiccups don't cascade into "the whole suit went dead". 32 fps * 300 s
    // = 9600.
    if (dccount > 9600) {
      //esp_deep_sleep_start();
      watch->shutdown();
    }
  }
}

String mac_address;


int fps = 32;

int batt_v = 0;
float quatI, quatJ, quatK, quatReal;

uint32_t readADC_Cal(int ADC_Raw) {
  esp_adc_cal_characteristics_t adc_chars;

  esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adc_chars);
  return (esp_adc_cal_raw_to_voltage(ADC_Raw, &adc_chars));
}

bool calibrated = false;


struct Quat {
  float x;
  float y;
  float z;
  float w;
} quat;

#define NB_RECS 5


char buff[256];
bool rtcIrq = false;
bool initial = 1;
bool otaStart = false;

uint8_t func_select = 0;
uint8_t omm = 99;
uint8_t xcolon = 0;
uint32_t targetTime = 0;  // for next 1 second timeout
uint32_t colour = 0;
int vref = 1100;

bool pressed = false;
uint32_t pressedTime = 0;
bool charge_indication = false;

uint8_t hh, mm, ss;
int pacnum = 0;



void hexdump(const void *mem, uint32_t len, uint8_t cols = 16) {
  const uint8_t *src = (const uint8_t *)mem;
  Serial.printf("\n[HEXDUMP] Address: 0x%08X len: 0x%X (%d)", (ptrdiff_t)src, len, len);
  for (uint32_t i = 0; i < len; i++) {
    if (i % cols == 0) {
      Serial.printf("\n[0x%08X] 0x%08X: ", (ptrdiff_t)src, i);
    }
    Serial.printf("%02X ", *src);
    src++;
  }
  Serial.printf("\n");
}



// define two tasks for Blink & AnalogRead
void TaskWifi(void *pvParameters);
void TaskReadIMU(void *pvParameters);

#if CONFIG_FREERTOS_UNICORE
#define ARDUINO_RUNNING_CORE 0
#else
#define ARDUINO_RUNNING_CORE 1
#endif


void setupIMU() {
  Wire.begin(21, 22);

  delay(500);
  Wire.setClock(400000);

  //myICM.enableDebugging();

  bool initialized = false;
  while (!initialized) {

    myICM.begin(Wire, AD0_VAL);

    Serial.print(F("Initialization of the sensor returned: "));
    Serial.println(myICM.statusString());
    if (myICM.status != ICM_20948_Stat_Ok) {
      Serial.println(F("Trying again..."));
      delay(500);
    } else {
      initialized = true;
    }
  }

  Serial.println(F("Device connected."));

  bool success = true;  // Use success to show if the DMP configuration was successful

  // Initialize the DMP. initializeDMP is a weak function. In this example we overwrite it to change the sample rate (see below)
  success &= (myICM.initializeDMP() == ICM_20948_Stat_Ok);

  // DMP sensor options are defined in ICM_20948_DMP.h
  //    INV_ICM20948_SENSOR_ACCELEROMETER               (16-bit accel)
  //    INV_ICM20948_SENSOR_GYROSCOPE                   (16-bit gyro + 32-bit calibrated gyro)
  //    INV_ICM20948_SENSOR_RAW_ACCELEROMETER           (16-bit accel)
  //    INV_ICM20948_SENSOR_RAW_GYROSCOPE               (16-bit gyro + 32-bit calibrated gyro)
  //    INV_ICM20948_SENSOR_MAGNETIC_FIELD_UNCALIBRATED (16-bit compass)
  //    INV_ICM20948_SENSOR_GYROSCOPE_UNCALIBRATED      (16-bit gyro)
  //    INV_ICM20948_SENSOR_STEP_DETECTOR               (Pedometer Step Detector)
  //    INV_ICM20948_SENSOR_STEP_COUNTER                (Pedometer Step Detector)
  //    INV_ICM20948_SENSOR_GAME_ROTATION_VECTOR        (32-bit 6-axis quaternion)
  //    INV_ICM20948_SENSOR_ROTATION_VECTOR             (32-bit 9-axis quaternion + heading accuracy)
  //    INV_ICM20948_SENSOR_GEOMAGNETIC_ROTATION_VECTOR (32-bit Geomag RV + heading accuracy)
  //    INV_ICM20948_SENSOR_GEOMAGNETIC_FIELD           (32-bit calibrated compass)
  //    INV_ICM20948_SENSOR_GRAVITY                     (32-bit 6-axis quaternion)
  //    INV_ICM2094
  //    INV_ICM20948_SENSOR_ORIENTATION                 (32-bit 9-axis quaternion + heading accuracy)

  // Enable the DMP orientation sensor
  success &= (myICM.enableDMPSensor(INV_ICM20948_SENSOR_GAME_ROTATION_VECTOR) == ICM_20948_Stat_Ok);

  // Enable any additional sensors / features
  success &= (myICM.enableDMPSensor(INV_ICM20948_SENSOR_RAW_GYROSCOPE) == ICM_20948_Stat_Ok);
  success &= (myICM.enableDMPSensor(INV_ICM20948_SENSOR_RAW_ACCELEROMETER) == ICM_20948_Stat_Ok);
  //success &= (myICM.enableDMPSensor(INV_ICM20948_SENSOR_MAGNETIC_FIELD_UNCALIBRATED) == ICM_20948_Stat_Ok);

  // Configuring DMP to output data at multiple ODRs:
  // DMP is capable of outputting multiple sensor data at different rates to FIFO.
  // Setting value can be calculated as follows:
  // Value = (DMP running rate / ODR ) - 1
  // E.g. For a 5Hz ODR rate when DMP is running at 55Hz, value = (55/5) - 1 = 10.
  success &= (myICM.setDMPODRrate(DMP_ODR_Reg_Quat6, 0) == ICM_20948_Stat_Ok);  // Set to the maximum
  //success &= (myICM.setDMPODRrate(DMP_ODR_Reg_Accel, 0) == ICM_20948_Stat_Ok); // Set to the maximum
  //success &= (myICM.setDMPODRrate(DMP_ODR_Reg_Gyro, 0) == ICM_20948_Stat_Ok); // Set to the maximum
  //success &= (myICM.setDMPODRrate(DMP_ODR_Reg_Gyro_Calibr, 0) == ICM_20948_Stat_Ok); // Set to the maximum
  //success &= (myICM.setDMPODRrate(DMP_ODR_Reg_Cpass, 0) == ICM_20948_Stat_Ok); // Set to the maximum
  //success &= (myICM.setDMPODRrate(DMP_ODR_Reg_Cpass_Calibr, 0) == ICM_20948_Stat_Ok); // Set to the maximum

  // Enable the FIFO
  success &= (myICM.enableFIFO() == ICM_20948_Stat_Ok);

  // Enable the DMP
  success &= (myICM.enableDMP() == ICM_20948_Stat_Ok);

  // Reset DMP
  success &= (myICM.resetDMP() == ICM_20948_Stat_Ok);

  // Reset FIFO
  success &= (myICM.resetFIFO() == ICM_20948_Stat_Ok);

  // Check success
  if (success) {
    Serial.println(F("DMP enabled."));
  } else {
    Serial.println(F("INIT_RESULT   : IMU_OK DMP_FAIL"));   // I8 -> NODE-04
    Serial.println(F("Enable DMP failed!"));
    Serial.println(F("Please check that you have uncommented line 29 (#define ICM_20948_USE_DMP) in ICM_20948_C.h..."));
    while (1)
      ;  // Do nothing more
  }



  Serial.println(F("IMU enabled"));
  Serial.println(F("INIT_RESULT   : IMU_OK DMP_OK"));   // I8 -> NODE-04
  calibrated = true;
}


void setup() {


  // Get TTGOClass instance
  watch = TTGOClass::getWatch();

  // Initialize the hardware, the BMA423 sensor has been initialized internally
  watch->begin();

  // ---- Haptic motor setup ----
  // Motor/speaker rail on the Standard base plate is AXP202 LDO3. Enable it,
  // otherwise toggling GPIO 33 does nothing (pin flips, motor has no power).
// Set LDO3 voltage explicitly before enabling (motor needs power)
watch->power->setLDO3Voltage(3300);                              // 3.3V
watch->power->setPowerOutPut(AXP202_LDO3, AXP202_ON);
delay(50);                                                        // let rail settle

pinMode(MOTOR_PIN, OUTPUT);
digitalWrite(MOTOR_PIN, LOW);

// Power button (GPIO 36) — input-only on ESP32
pinMode(PWR_BTN_PIN, INPUT);

// Boot indicator: two buzzes to confirm the pod has powered up
motorPulse(2);



  // Turn on the backlight
  watch->openBL();

  pinMode(TOUCH_INT, INPUT);


  watch->button->setPressedHandler(pressedB);
  watch->button->setReleasedHandler(released);


  //Receive objects for easy writing
  tft = watch->tft;
  tft->fillScreen(BG);
  tft->setTextColor(FG, BG);


  tft->setTextFont(7); 
  tft->drawCentreString("MESQUITE.cc", 120, 10, 4);


  if (boneName[sendID][1] == "") {
    tft->setTextSize(3);
    tft->drawCentreString(boneName[sendID][0], 120, 80, 4);
  } else {
    tft->setTextSize(3);
    tft->drawCentreString(boneName[sendID][0], 120, 65, 4);
    tft->setTextSize(2);
    tft->drawCentreString(boneName[sendID][1], 120, 130, 4);
  }

  tft->setTextSize(1);

  mac_string_to_uint8_array(mac_address_str, broadcastAddress);


  // pinMode(3, OUTPUT);
  Serial.begin(115200);
  delay(500);

  // ===== Phase 2 W0: provenance banner (resolves U2) =====
  // Printed once at boot on every pod. configTICK_RATE_HZ decides whether
  // vTaskDelay(1) is 1 ms or 10 ms, which decides whether the real transmit
  // rate is ~32 Hz or ~25 Hz (NODE-03). Three Phase 1 reports depend on it.
  Serial.println();
  Serial.println(F("===== MESQUITE POD BOOT ====="));
  Serial.printf("FW_BUILD      : %s %s\n", __DATE__, __TIME__);
  Serial.printf("POD_ID        : %d\n", sendID);
#ifdef ESP_ARDUINO_VERSION_STR
  Serial.printf("ARDUINO_CORE  : %s\n", ESP_ARDUINO_VERSION_STR);
#else
  Serial.println(F("ARDUINO_CORE  : <2.0.0 (macro absent)"));
#endif
  Serial.printf("IDF_VERSION   : %s\n", esp_get_idf_version());
  Serial.printf("TICK_RATE_HZ  : %d\n", (int)configTICK_RATE_HZ);
  Serial.printf("TICK_PERIOD_MS: %d\n", (int)portTICK_PERIOD_MS);
  Serial.printf("RESET_REASON  : %d\n", (int)esp_reset_reason());
  Serial.printf("CPU_FREQ_MHZ  : %d\n", (int)getCpuFrequencyMhz());
  Serial.printf("HEAP_FREE     : %u\n", (unsigned)ESP.getFreeHeap());
  Serial.printf("PSRAM_FREE    : %u\n", (unsigned)ESP.getFreePsram());
  Serial.printf("NOMINAL_FPS   : %d  (gate = 1000/%d = %d ms)\n", fps, fps, 1000/fps);
#if MESQ_INSTR
  mesq_bootCount++;                                  // I8
  Serial.printf("BOOT_COUNT    : %u  (RTC, survives reset)\n", mesq_bootCount);
  Serial.println(F("INSTR         : ENABLED (MESQ_INSTR=1)"));
#else
  Serial.println(F("INSTR         : disabled"));
#endif
  Serial.println(F("============================="));

  lastOn = millis();
  lastTouch = millis();


  WiFi.mode(WIFI_STA);

  // -- Radio hardening (root cause of the Tempe->Boston regression) --
  //  1) Pin to a known channel so we are guaranteed to share airwaves with
  //     the dongle no matter how noisy the local 2.4 GHz band is.
  //  2) Disable WiFi power-save: in PS modes the radio sleeps between beacons
  //     and ESP-NOW packets can be dropped during sleep windows. Pods are
  //     mains-/battery-powered with a 60 mA budget; PS savings don't justify
  //     the missing frames.
  //  3) Set TX power to the regulatory max (80 = 20 dBm). With pods worn at
  //     hip / arm height and the dongle 1-3 m away in a busy mocap volume,
  //     every dB helps.
  esp_wifi_set_channel(ESPNOW_WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_ps(WIFI_PS_NONE);
  esp_wifi_set_max_tx_power(80);

  // esp_deep_sleep_enable_gpio_wakeup(BIT(36), ESP_GPIO_WAKEUP_GPIO_LOW);


  Serial.println("Connecting");

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Once ESPNow is successfully Init, we will register for Send CB to
  // get the status of Trasnmitted packet
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);


  // Register peer. `channel = 0` used to mean "use current channel" but that
  // is fragile -- if the STA roams the peer's channel becomes stale and ESP-
  // NOW silently drops sends. Be explicit.
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = ESPNOW_WIFI_CHANNEL;
  peerInfo.encrypt = false;

  // Add peer
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }


  mac_address = WiFi.macAddress();
  Serial.println(mac_address);
  delay(100);


  setupIMU();



  xTaskCreatePinnedToCore(
    TaskWifi, "TaskWifi"  // A name just for humans
    ,
    10000  // This stack size can be checked & adjusted by reading the Stack Highwater
    ,
    NULL, 1  // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
    ,
    NULL, 0);

  // delay(1000);
  xTaskCreatePinnedToCore(
    TaskReadIMU, "TaskReadIMU", 10000  // Stack size
    ,
    NULL, 1  // Priority
    ,
    NULL, 1);

  handleBattDisplay();
}


void handleBattDisplay() {
  batt_v = getBattery();
  if (isCharging) {
    tft->fillRoundRect(0, 205, 240, 35, 0, TFT_GREEN);
    tft->setTextColor(TFT_BLACK, TFT_GREEN);
    tft->drawCentreString(String((int)batt_v) + "%, CHARGING", 120, 212, 4);
  } else {
    tft->fillRoundRect(0, 205, 240, 35, 0, TFT_RED);
    tft->setTextColor(TFT_WHITE, TFT_RED);
    tft->drawCentreString(String((int)batt_v) + "%, NOT Charging", 120, 212, 4);
  }
}

void loop() {
}

// Haptic helper: pulse the motor N times with a gap between pulses.
// Blocking: total time = count * MOTOR_PULSE_MS + (count-1) * 150 ms
void motorPulse(int count) {
  for (int i = 0; i < count; i++) {
    if (i > 0) delay(150);
    digitalWrite(MOTOR_PIN, HIGH);
    delay(MOTOR_PULSE_MS);
    digitalWrite(MOTOR_PIN, LOW);
  }
}

// ---- Screen-only helpers (no buzz) ----
// Used by touchscreen taps and the 5s idle auto-sleep.
void sleepScreen() {
  if (!isOn) return;
  watch->closeBL();
  watch->displayOff();
  isOn = false;
}

void wakeScreen() {
  if (isOn) return;
  watch->openBL();
  watch->displayWakeup();
  lastOn = millis();
  isOn = true;
  // Defensive: re-assert LDO3 (motor power rail) in case anything disabled it
  watch->power->setPowerOutPut(AXP202_LDO3, AXP202_ON);
}

void toggleScreen() {
  if (isOn) sleepScreen();
  else wakeScreen();
}

// ---- Long-press button state ----
// Short press does nothing (protects against accidental shutdowns during mocap).
// Hold for LONG_PRESS_MS to fully power down the pod via AXP202.
//
// We poll GPIO 36 (the physical power button) directly instead of using the
// TTGO button library's event handlers, because the library's timing is
// entangled with AXP202's built-in PEK handling and can swallow the event.
// Constants PWR_BTN_PIN and LONG_PRESS_MS are defined at the top of the file.

void pressedB() {
  // Kept as a registered handler for library compatibility. No-op.
}

void released() {
  // No-op. All long-press logic lives in TaskReadIMU's GPIO poll.
}


int getBattery() {
  watch->power->adc1Enable(AXP202_VBUS_VOL_ADC1 | AXP202_VBUS_CUR_ADC1 | AXP202_BATT_CUR_ADC1 | AXP202_BATT_VOL_ADC1, true);
  // get the values
  isCharging = watch->power->isChargeing();
  int per = watch->power->getBattPercentage();
  return per;
}

bool touchoff = false;

void TaskWifi(void *pvParameters) {
  for (;;) {
    // button.loop();

    
    static uint32_t prev_ms = millis();

    if (millis() > (prev_ms + (1000 / fps))) {
      fcount++;

      // Build the 16-byte binary packet. Bone name is no longer on the wire;
      // the dongle and browser both look up name-from-id via the same enum.
      myData.sync0 = 0xAA;
      myData.sync1 = 0x55;
      myData.id    = (uint8_t)sendID;
      // batt_v here is the integer percentage from getBattery(); clamp to 0..100
      // so the byte field is always in range (negative values can leak in
      // briefly during boot before the AXP202 ADC stabilises).
      int b = batt_v;
      if (b < 0)   b = 0;
      if (b > 100) b = 100;
      myData.batt  = (uint8_t)b;
      myData.qx    = q_to_i16(quat.x);
      myData.qy    = q_to_i16(quat.y);
      myData.qz    = q_to_i16(quat.z);
      myData.qw    = q_to_i16(quat.w);
      myData.count = (uint16_t)count;
      myData.ms_lo = (uint16_t)millis();

#if MESQ_INSTR
      {
        int64_t _ic0 = esp_timer_get_time();
        // N1: torn cross-core read shows up as a non-unit quaternion
        float _n = quat.x*quat.x + quat.y*quat.y + quat.z*quat.z + quat.w*quat.w;
        if (_n < 0.999f || _n > 1.001f) mesq_normBad++;
        // N3: age of the sample at the moment we transmit it
        if (mesq_lastSampleUs != 0) {
          uint32_t _age = (uint32_t)((_ic0 - mesq_lastSampleUs) / 1000);
          mesq_ageN++; mesq_ageSum += _age;
          if (_age > mesq_ageMax) mesq_ageMax = _age;
        }
        // I4: actual interval between sends
        static uint32_t _lastSend = 0;
        uint32_t _nowMs = millis();
        if (_lastSend) {
          uint32_t _iv = _nowMs - _lastSend;
          mesq_sendN++; mesq_bucketSend(_iv);
          if (_iv < mesq_sendMin) mesq_sendMin = _iv;
          if (_iv > mesq_sendMax) mesq_sendMax = _iv;
        }
        _lastSend = _nowMs;
        mesq_instrCostUs += (uint64_t)(esp_timer_get_time() - _ic0);
      }
#endif

      esp_now_send(broadcastAddress, (uint8_t *)&myData, sizeof(myData));

      prev_ms = millis();
      count++;

#if MESQ_INSTR
      // ---- 1 Hz instrumentation report, on core 0 (TaskWifi) so the
      // ---- sample task on core 1 is not disturbed. Pod Serial is its own
      // ---- USB port and is not the dongle's binary stream.
      {
        static uint32_t _lastRep = 0;
        uint32_t _n2 = millis();
        if (_n2 - _lastRep >= 1000) {
          _lastRep = _n2;
          Serial.printf(
            "[INSTR] id=%d quat6/s=%u read_us(min/mean/max)=%u/%u/%u fifoMore=%u "
            "send(n=%u min=%u max=%u b=%u/%u/%u/%u/%u/%u) age_ms(mean/max)=%u/%u "
            "normBad=%u radNeg=%u heap=%u boots=%u instr_us/s=%llu\n",
            sendID, mesq_quat6N,
            mesq_readN ? mesq_readMin : 0,
            mesq_readN ? (uint32_t)(mesq_readSum / mesq_readN) : 0,
            mesq_readMax, mesq_fifoMoreN,
            mesq_sendN, mesq_sendN ? mesq_sendMin : 0, mesq_sendMax,
            mesq_sendBuckets[0], mesq_sendBuckets[1], mesq_sendBuckets[2],
            mesq_sendBuckets[3], mesq_sendBuckets[4], mesq_sendBuckets[5],
            mesq_ageN ? (uint32_t)(mesq_ageSum / mesq_ageN) : 0, mesq_ageMax,
            mesq_normBad, mesq_radNeg,
            (unsigned)ESP.getFreeHeap(), mesq_bootCount,
            (unsigned long long)mesq_instrCostUs);
          mesq_quat6N = 0; mesq_readN = 0; mesq_readSum = 0;
          mesq_readMin = 0xFFFFFFFF; mesq_readMax = 0; mesq_fifoMoreN = 0;
          mesq_sendN = 0; mesq_sendMin = 0xFFFFFFFF; mesq_sendMax = 0;
          for (int _b = 0; _b < 6; _b++) mesq_sendBuckets[_b] = 0;
          mesq_ageN = 0; mesq_ageSum = 0; mesq_ageMax = 0;
          mesq_instrCostUs = 0;
        }
      }
#endif
    }
    //vTaskDelay(1/portTICK_PERIOD_MS);  // one tick delay (15ms) in between reads for stability
    vTaskDelay(1);
  }
}

float ax;
float ay;
float az;

void TaskReadIMU(void *pvParameters) {
  // Local state for direct GPIO-based long-press detection.
  // `static` inside a task is fine — persists across iterations.
  static bool btnWasPressed = false;
  static uint32_t btnPressStart = 0;
  static bool longPressDone = false;

  for (;;) {
    watch->button->loop();  // kept because other library internals use it

    // --- Direct GPIO long-press shutdown ---
    // Button is active-low: pressed = 0, released = 1.
    bool btnNowPressed = (digitalRead(PWR_BTN_PIN) == LOW);

    if (btnNowPressed && !btnWasPressed) {
      // Rising edge of a press
      btnPressStart = millis();
      longPressDone = false;
    }
    if (btnNowPressed && !longPressDone
        && (millis() - btnPressStart >= LONG_PRESS_MS)) {
      longPressDone = true;
      motorPulse(1);       // confirmation — user can release now
      delay(100);
      watch->shutdown();
      while (1) delay(1000);  // should never reach here
    }
    btnWasPressed = btnNowPressed;

    // --- Screen-only idle auto-sleep (no buzz) ---
    if (isOn && millis() - lastOn > 5000) {
      sleepScreen();
    }

    // --- Touchscreen: toggle screen only, no buzz ---
    int16_t x, y;
    if (watch->getTouch(x, y)) {
      if (millis() - lastTouch > 600) {
        toggleScreen();
      }
      lastTouch = millis();
    }

    static uint32_t prev_ms1 = millis();
    if (millis() > (prev_ms1 + 1000 * 3)) {
      // read battery every minute
      handleBattDisplay();
      prev_ms1 = millis();
    }


    icm_20948_DMP_data_t data;
#if MESQ_INSTR
    int64_t _t0 = esp_timer_get_time();
#endif
    myICM.readDMPdataFromFIFO(&data);
#if MESQ_INSTR
    {
      uint32_t _d = (uint32_t)(esp_timer_get_time() - _t0);
      mesq_readN++; mesq_readSum += _d;
      if (_d < mesq_readMin) mesq_readMin = _d;
      if (_d > mesq_readMax) mesq_readMax = _d;
      if (myICM.status == ICM_20948_Stat_FIFOMoreDataAvail) mesq_fifoMoreN++;
    }
#endif

    if ((myICM.status == ICM_20948_Stat_Ok) || (myICM.status == ICM_20948_Stat_FIFOMoreDataAvail))  // Was valid data available?
    {
      //Serial.print(F("Received data! Header: 0x")); // Print the header in HEX so we can see what data is arriving in the FIFO
      //if ( data.header < 0x1000) Serial.print( "0" ); // Pad the zeros
      //if ( data.header < 0x100) Serial.print( "0" );
      //if ( data.header < 0x10) Serial.print( "0" );
      //Serial.println( data.header, HEX );

      if ((data.header & DMP_header_bitmap_Quat6) > 0)  // We have asked for GRV data so we should receive Quat6
      {
#if MESQ_INSTR
        mesq_quat6N++;   // I9: this count per second IS the DMP output rate
#endif
        // Q0 value is computed from this equation: Q0^2 + Q1^2 + Q2^2 + Q3^2 = 1.
        // In case of drift, the sum will not add to 1, therefore, quaternion data need to be corrected with right bias values.
        // The quaternion data is scaled by 2^30.

        //Serial.printf("Quat6 data is: Q1:%ld Q2:%ld Q3:%ld\r\n", data.Quat6.Data.Q1, data.Quat6.Data.Q2, data.Quat6.Data.Q3);

        // Scale to +/- 1
        double q1 = ((double)data.Quat6.Data.Q1) / 1073741824.0;  // Convert to double. Divide by 2^30
        double q2 = ((double)data.Quat6.Data.Q2) / 1073741824.0;  // Convert to double. Divide by 2^30
        double q3 = ((double)data.Quat6.Data.Q3) / 1073741824.0;  // Convert to double. Divide by 2^30


        // Convert the quaternions to Euler angles (roll, pitch, yaw)
        // https://en.wikipedia.org/w/index.php?title=Conversion_between_quaternions_and_Euler_angles&section=8#Source_code_2

        double _rad = 1.0 - ((q1 * q1) + (q2 * q2) + (q3 * q3));
#if MESQ_INSTR
        if (_rad < 0.0) mesq_radNeg++;   // N2 -> SENS-03
#endif
        double q0 = sqrt(_rad);

        double q2sqr = q2 * q2;

        // roll (x-axis rotation)
        double t0 = +2.0 * (q0 * q1 + q2 * q3);
        double t1 = +1.0 - 2.0 * (q1 * q1 + q2sqr);
        double roll = atan2(t0, t1) * 180.0 / PI;

        // pitch (y-axis rotation)
        double t2 = +2.0 * (q0 * q2 - q3 * q1);
        t2 = t2 > 1.0 ? 1.0 : t2;
        t2 = t2 < -1.0 ? -1.0 : t2;
        double pitch = asin(t2) * 180.0 / PI;

        // yaw (z-axis rotation)
        double t3 = +2.0 * (q0 * q3 + q1 * q2);
        double t4 = +1.0 - 2.0 * (q2sqr + q3 * q3);
        double yaw = atan2(t3, t4) * 180.0 / PI;

        /*
      Serial.print(q0, 3);
      Serial.print(" ");
      Serial.print(q1, 3);
      Serial.print(" ");
      Serial.print(q2, 3);
      Serial.print(" ");
      Serial.print(q3, 3);
      Serial.println();
      */

        quat.w = q0;
        quat.x = q1;
        quat.y = q2;
        quat.z = q3;
#if MESQ_INSTR
        mesq_lastSampleUs = esp_timer_get_time();   // N3: when the sample was produced
#endif
      }
    }

    if (myICM.status != ICM_20948_Stat_FIFOMoreDataAvail)  // If more data is available then we should read it right away - and not delay
    {
      delay(10);
    }

    //vTaskDelay(1/portTICK_PERIOD_MS);  // one tick delay (15ms) in between reads for stability
    vTaskDelay(1);
  }
}




// NOTE: Recv callback signature also changed between core 2.x and 3.x.
// 2.x: void(const uint8_t *mac_addr, const uint8_t *data, int len)
// 3.x: void(const esp_now_recv_info_t *info, const uint8_t *data, int len)
// Binary control packet from the dongle:
//   [0xAA][0x55][0xFF][cmd]
// where cmd = 0x01 -> reboot. Anything else is ignored. The 0xFF in the id
// slot is the marker that distinguishes a control packet from a normal pod
// data frame (which has id 0..16).
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
void OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *incomingData, int len) {
#else
void OnDataRecv(const uint8_t *mac_addr, const uint8_t *incomingData, int len) {
#endif
  if (len >= 4
      && incomingData[0] == 0xAA
      && incomingData[1] == 0x55
      && incomingData[2] == 0xFF) {
    if (incomingData[3] == 0x01) {
      ESP.restart();
    }
  }
}
