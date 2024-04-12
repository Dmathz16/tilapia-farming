#define BLYNK_TEMPLATE_ID "TMPL6j0y9GR1I"
#define BLYNK_TEMPLATE_NAME "IoT based Monitoring System for Tilapia Production"
#define BLYNK_AUTH_TOKEN "gkHsTbn7IxEDXfRFy5vjXBWR60nVy8qB"

#define BLYNK_PRINT Serial
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <RTClib.h>

char auth[] = BLYNK_AUTH_TOKEN;
char ssid[] = "Dmathz";
char pass[] = "Dmathz@2024";

// relay
#define RELAY_PIN 4

// RTC
#define SDA_PIN 21
#define SCL_PIN 22
RTC_DS3231 rtc;
String dateToday = "";

// Feeder time class
struct FeedingTime {
  const char *time;
  int duration;
  int status;
};

// Define Feeder time details
FeedingTime feedingTime[] = {
  { "11:53", 15, 0 },
  { "13:00", 30, 0 },
  { "13:30", 60, 0 }
};

// ========= TP =========
#include <OneWire.h>
#include <DallasTemperature.h>
#define SENSOR_PIN 17
OneWire oneWire(SENSOR_PIN);
DallasTemperature DS18B20(&oneWire);

// ========= TB =========
int sensorPin = 35;

// ========= PH =========
#define sensorPH 33
float calibration_value = 25.6;
int phval = 0;
unsigned long int avgval;
int buffer_arr[10], temp;
float ph_act;

// ========= DO =========
#define DO_PIN 34
#define VREF 5000    //VREF (mv)
#define ADC_RES 1024 //ADC Resolution
//Single-point calibration Mode=0
//Two-point calibration Mode=1
#define TWO_POINT_CALIBRATION 0
#define READ_TEMP (25)
//Single point calibration needs to be filled CAL1_V and CAL1_T
#define CAL1_V (131) //mv
#define CAL1_T (25)  //℃
const uint16_t DO_Table[41] = {
  14460, 14220, 13820, 13440, 13090, 12740, 12420, 12110, 11810, 11530,
  11260, 11010, 10770, 10530, 10300, 10080, 9860, 9660, 9460, 9270,
  9080, 8900, 8730, 8570, 8410, 8250, 8110, 7960, 7820, 7690,
  7560, 7430, 7300, 7180, 7070, 6950, 6840, 6730, 6630, 6530, 6410
};
uint8_t Temperaturet;
uint16_t ADC_Raw;
uint16_t ADC_Voltage;
uint16_t DO;

// timer
BlynkTimer timer;

void sendTemperature() {
  DS18B20.requestTemperatures();
  float tempC = DS18B20.getTempCByIndex(0);
  if (tempC != DEVICE_DISCONNECTED_C) {
    Serial.print("Temperature: ");
    Serial.println(tempC);
    Blynk.virtualWrite(V1, tempC);
  } else {
    Serial.println("Error: Could not read temperature data");
    Serial.println("Check wiring and sensor connection");
  }
}

void sendTurbidity() {

  int sensorValue = analogRead(sensorPin);
  int turbidity = map(sensorValue, 0, 750, 0, 100);
  // Serial.print("Turbidity sensor: ");
  // Serial.println(sensorValue);
  Serial.print("Turbidity: ");
  Serial.println(turbidity);
  Blynk.virtualWrite(V2, turbidity);

  // Serial.print("Turbidity: ");
  // Serial.print(turbidity);
  // if (turbidity < 20) {
  //   Serial.print(" - CLEAR");
  // }
  // if ((turbidity >= 20) && (turbidity < 50)) {
  //   Serial.print(" - CLOUDY");
  // }
  // if (turbidity >= 50) {
  //   Serial.print(" - DIRTY");
  // }
  // Serial.println(" ");
  // delay(1000);
}

void sendPH() {

  for (int i = 0; i < 10; i++) {
    buffer_arr[i] = analogRead(sensorPH);
    delay(30);
  }
  for (int i = 0; i < 9; i++) {
    for (int j = i + 1; j < 10; j++) {
      if (buffer_arr[i] > buffer_arr[j]) {
        temp = buffer_arr[i];
        buffer_arr[i] = buffer_arr[j];
        buffer_arr[j] = temp;
      }
    }
  }

  avgval = 0;
  for (int i = 2; i < 8; i++)
    avgval += buffer_arr[i];
  float volt = (float)avgval * 3.3 / 4096.0 / 6;
  ph_act = -5.70 * volt + calibration_value;

  Serial.print("pH Val: ");
  Serial.println(ph_act);
  Blynk.virtualWrite(V3, ph_act);
}

int16_t _readDO(uint32_t voltage_mv, uint8_t temperature_c)
{
  #if TWO_POINT_CALIBRATION == 00
    uint16_t V_saturation = (uint32_t)CAL1_V + (uint32_t)35 * temperature_c - (uint32_t)CAL1_T * 35;
    return (voltage_mv * DO_Table[temperature_c] / V_saturation);
  #else
    uint16_t V_saturation = (int16_t)((int8_t)temperature_c - CAL2_T) * ((uint16_t)CAL1_V - CAL2_V) / ((uint8_t)CAL1_T - CAL2_T) + CAL2_V;
    return (voltage_mv * DO_Table[temperature_c] / V_saturation);
  #endif
}

void sendDissolvedOxygen() {


  Temperaturet = (uint8_t)READ_TEMP;
  ADC_Raw = analogRead(DO_PIN);
  ADC_Voltage = uint32_t(VREF) * ADC_Raw / ADC_RES;
 
  // Serial.print("Temperaturet:\t" + String(Temperaturet) + "\t");
  // Serial.print("ADC RAW:\t" + String(ADC_Raw) + "\t");
  // Serial.print("ADC Voltage:\t" + String(ADC_Voltage) + "\t");
  // Serial.println("DO:\t" + String(_readDO(ADC_Voltage, Temperaturet)) + "\t");

  Serial.print("DO Val: ");
  Serial.println(String(_readDO(ADC_Voltage, Temperaturet)));
  Serial.println("==========================");
  Blynk.virtualWrite(V4, String(_readDO(ADC_Voltage, Temperaturet)));
}

int recurseFeedingTime(int index, DateTime now, int isDone) {

  // End recursion if all index exceeds
  if (index >= sizeof(feedingTime) / sizeof(feedingTime[0])) return isDone;

  char str[6];                           // Assuming the time format is "HH:MM"
  strcpy(str, feedingTime[index].time);  // Copying the time string to a char array
  char *hourStr = strtok(str, ":");      // Splitting the time string
  char *minuteStr = strtok(NULL, ":");   // Splitting the time string

  int hour = atoi(hourStr);      // Convert hour string to integer
  int minute = atoi(minuteStr);  // Convert minute string to integer

  int status = 1;

  if (feedingTime[index].status == 0) {

    if ((now.hour() == hour) && (now.minute() == minute)) {
      // Open relay here
      digitalWrite(RELAY_PIN, LOW);
      delay(feedingTime[index].duration * 1000);
      digitalWrite(RELAY_PIN, HIGH);
    }

    if ((hour == now.hour() && minute > now.minute()) || hour > now.hour()) status = 0;
  }

  feedingTime[index].status = status;

  if (isDone == 1 && status == 0) isDone = 0;

  Serial.print(feedingTime[index].status);
  Serial.print("    ");
  Serial.print(hour);
  Serial.print(":");
  Serial.println(minute);

  // Recursive call to process the next element in the array
  return recurseFeedingTime(index + 1, now, isDone);
}

void motorCheck() {

  DateTime now = rtc.now();

  Serial.print("Date and time: ");
  Serial.print(dateToday);
  Serial.print(" ");
  Serial.print(now.hour());
  Serial.print(":");
  Serial.println(now.minute());

  int isDone = recurseFeedingTime(0, now, 1);
  Serial.print("Done Today: ");
  Serial.println(isDone);
  Serial.println("==========================");
  if (dateToday.isEmpty()) dateToday = String(now.month()) + "-" + String(now.day());
  if (isDone == 1) motorCheckReset(now);
}

void motorCheckReset(DateTime now) {

  if (dateToday != (String(now.month()) + "-" + String(now.day()))) {
    dateToday = String(now.month()) + "-" + String(now.day());
    for (int i = 0; i < sizeof(feedingTime) / sizeof(feedingTime[0]); i++) {
      feedingTime[i].status = 0;
    }
  }
}

void setup() {
  Serial.begin(9600);
  Serial.println("Initializing...");

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(sensorPH, INPUT);
  digitalWrite(RELAY_PIN, HIGH);
  Wire.begin(SDA_PIN, SCL_PIN);
  DS18B20.begin();

  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  Blynk.begin(auth, ssid, pass);
  timer.setInterval(1000L, motorCheck);
  timer.setInterval(1000L, sendTemperature);
  timer.setInterval(1000L, sendTurbidity);
  timer.setInterval(1000L, sendPH);
  timer.setInterval(1000L, sendDissolvedOxygen);
}

void loop() {
  Blynk.run();
  timer.run();
  // Serial.println("=====================");
}
