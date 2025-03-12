#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <HTTPClient.h>
#include <ArduinoJson.h> 
#include <DHT.h>

#define TDS_PIN 33      // Pin analog tempat sensor TDS terhubung
#define ONE_WIRE_BUS 32 // Pin digital tempat sensor DS18B20 terhubung
#define DHTPIN 25
#define RELAY_LAMPU 26
#define RELAY_KIPAS 27

unsigned long previousMillisSensor = 0;
const long intervalSensor = 5000;

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

DHT dht(DHTPIN, DHT22);

// NTP Client setup
WiFiUDP udp;
NTPClient timeClient(udp, "pool.ntp.org", 7 * 3600, 60000);

// Variabel untuk menyimpan jadwal
String jadwal1 = "07:00";     //Jadwal Default
String jadwal2 = "17:00";     //Jadwal 

bool statusKipas = false;
bool statusLampu = false;

// WiFi setup
const char* ssid = "aiko";
const char* password = "menujubersama";
String serverUrl = "http://lemarihidroponik.coeagriculture.my.id/post_data.php";

void setup() {
  Serial.begin(115200);

  dht.begin();
  sensors.begin(); // Inisialisasi sensor suhu
  timeClient.begin();  // Mulai NTP Client

  initializeWiFi();
  initializePins();
}

void loop() {
  unsigned long currentMillis = millis();

  // Ambil waktu terkini
  String timeString = getFormattedTime();

  // Memeriksa koneksi WiFi
  checkWiFiConnection();

  if (currentMillis - previousMillisSensor >= intervalSensor) {
    previousMillisSensor = currentMillis; // Update waktu terakhir
    readSensors(timeString);
  }
}

void initializePins(){
  pinMode(TDS_PIN, INPUT);
  pinMode(ONE_WIRE_BUS, INPUT);

  // Set relay pins
  pinMode(RELAY_LAMPU, OUTPUT);
  pinMode(RELAY_KIPAS, OUTPUT);
}

void initializeWiFi() {
  Serial.print("Menyambungkan ke WiFi...");

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Terhubung ke WiFi!");
  delay(2000);
}

void checkWiFiConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi terputus! Menghubungkan ulang...");
    delay(1000);
    initializeWiFi();
  }
}

void readSensors(String timeString) {
  int analogValue = analogRead(TDS_PIN); // Membaca nilai analog dari sensor TDS

  float suhuRuang = dht.readTemperature();
  if (isnan(suhuRuang)) {
    Serial.println("Gagal membaca suhu ruangan!");
    return;
  }

  // Membaca suhu dari DS18B20
  sensors.requestTemperatures();
  float suhuAir = sensors.getTempCByIndex(0); // Mengambil suhu dalam Celsius
  if (suhuAir == DEVICE_DISCONNECTED_C) {
    Serial.println("Gagal membaca suhu air!");
    return;
  }

  Serial.print("TDS Value: ");
  Serial.println(analogValue);
  
  Serial.print("Suhu Air : ");
  Serial.print(suhuAir);
  Serial.println(" °C");

  Serial.print("Suhu Ruang : ");
  Serial.print(suhuRuang);
  Serial.println(" °C");

  Serial.print("Waktu: ");
  Serial.print(timeString);
  Serial.println(" WIB");

  // Kirim data ke server
  sendDataToServer(analogValue, suhuAir, suhuRuang);

  controlRelays(timeString, suhuAir);
}

void sendDataToServer(int analogValue, float suhuAir, float suhuRuang) {
  HTTPClient http;

  String data = "suhu_air=" + String(suhuAir) + 
  "&suhu_ruangan=" + String(suhuRuang) + 
  "&analog_tds=" + String(analogValue) + 
  "&status_lampu=" + (statusLampu ? "1" : "0") + 
  "&status_kipas=" + (statusKipas ? "1" : "0");

  http.begin(serverUrl);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  int httpResponseCode = http.POST(data);

  if (httpResponseCode > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
  } else {
    Serial.print("Error sending HTTP request: ");
    Serial.println(httpResponseCode);
  }

  http.end();
}

String getFormattedTime() {
  timeClient.update();                    
  int hours = timeClient.getHours();
  int minutes = timeClient.getMinutes();
  
  char formattedTime[6]; // Buffer untuk menyimpan format waktu "HH:MM"
  snprintf(formattedTime, sizeof(formattedTime), "%02d:%02d", hours, minutes);
  
  return String(formattedTime);  
}

void setRelaysLampu(int stateLampu) {
  digitalWrite(RELAY_LAMPU, stateLampu);
}

void setRelaysKipas(int stateKipas) {
  digitalWrite(RELAY_KIPAS, stateKipas);
}

int convertTimeToMinutes(String timeStr) {
  int h = timeStr.substring(0, 2).toInt();
  int m = timeStr.substring(3, 5).toInt();
  return (h * 60) + m;
}

void controlRelays(String timeString, float suhuAir) {
  int currentTime = convertTimeToMinutes(timeString);
  int startLampu = convertTimeToMinutes(jadwal1);
  int endLampu = convertTimeToMinutes(jadwal2);

  if (currentTime >= startLampu && currentTime <= endLampu) {
    setRelaysLampu(HIGH);
    statusLampu = true;
    Serial.println("Lampu Aktif");
  } else {
    setRelaysLampu(LOW);
    statusLampu = false;
    Serial.println("Lampu Mati");
  }

  if (suhuAir >= 32) {
    setRelaysKipas(HIGH);
    statusKipas = true;
    Serial.println("Kipas Aktif");
  } else {
    setRelaysKipas(LOW);
    statusKipas = false;
    Serial.println("Kipas Mati");
  }
}