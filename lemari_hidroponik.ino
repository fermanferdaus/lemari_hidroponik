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
#define RELAY_KIPAS 26
#define RELAY_LAMPU 27
#define RELAY_PPM 14

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
const char* ssid = "UPCA GANK";
const char* password = "rondoanaklimo";
String serverUrl = "http://lemarihidroponik.coeagriculture.my.id/post_data.php";

void setup() {
  Serial.begin(115200);

  dht.begin();
  sensors.begin(); // Inisialisasi sensor suhu

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
  pinMode(RELAY_PPM, OUTPUT);
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

  // Membaca suhu dari DS18B20
  sensors.requestTemperatures();
  float suhuAir = sensors.getTempCByIndex(0); // Mengambil suhu dalam Celsius

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
  timeClient.update();                      // Perbarui waktu dari NTP
  String formattedTime = timeClient.getFormattedTime();
  return formattedTime.substring(0, 5);     // Ambil format HH:MM
}

void setRelaysKipas(int stateKipas) {
  digitalWrite(RELAY_KIPAS, stateKipas);
}

void controlRelays(String timeString, float suhuAir) {
  if (timeString >= jadwal1 && timeString <= jadwal2) {
    digitalWrite(RELAY_LAMPU, LOW);
    statusLampu = true;
    Serial.println("Lampu Aktif");
  } else {
    digitalWrite(RELAY_LAMPU, HIGH);
    statusLampu = false;
    Serial.println("Lampu Mati");
  }

  if (suhuAir >= 32) {
    digitalWrite(RELAY_KIPAS, LOW);
    statusKipas = true;
    Serial.println("Kipas Aktif");
  } else {
    digitalWrite(RELAY_KIPAS, HIGH);
    statusKipas = false;
    Serial.println("Kipas Mati");
  }
}
