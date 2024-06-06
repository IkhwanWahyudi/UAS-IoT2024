#include <WiFi.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

const char *ssid = "Wokwi-GUEST";
const char *password = "";
const char *mqtt_broker = "broker.emqx.io";
const char *topicTemp = "UAS/Temp";
const char *topicHum = "UAS/Hum";
const char *topicOzon = "UAS/Ozon";
const char *topicUV = "UAS/UV";
const char *mqtt_username = "";
const char *mqtt_password = "";

bool StatusLampu = false;    // Status awal lampu (off)
bool StatusBuzzer = false;    // Status awal buzzer (off)
bool Status7Segment = false;    // Status awal servo (off)
bool Refresh = false;

WiFiClient espClient;
PubSubClient client(espClient);

// URL dan API Key dari Antares
const char* serverName = "https://platform.antares.id:8443/~/antares-cse/antares-id/AQWM/weather_airQuality_nodeCore_teknik/la";
const char* apiKey = "ee8d16c4466b58e1:3b8d814324c84c89";

int pins[] = {21, 19, 32, 33, 25, 23, 22};
int data[4][7] = {
  // {atas, kanan atas, kanan bawah, bawah, kiri bawah, kiri atas, tengah}

  {0, 0, 0, 0, 0, 0, 1}, //0
  {1, 0, 0, 1, 1, 1, 1}, //1
  {0, 0, 1, 0, 0, 1, 0}, //2
  {0, 0, 0, 0, 1, 1, 0}, //3
};

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(4, OUTPUT);
  pinMode(5, OUTPUT);
  pinMode(16, OUTPUT);
  for (int x = 0; x <= 6; x++) {
    pinMode(pins[x], OUTPUT);
  }

  // Menghubungkan ke WiFi
  connectToWiFi();

  client.setServer(mqtt_broker, 1883);
  while (!client.connected()) {
    String client_id = "mqttx_UAS";
    client_id += String(WiFi.macAddress());
    Serial.printf("The client %s connects to the public MQTT broker\n", client_id.c_str());
    if (client.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
      Serial.println("Public EMQX MQTT broker connected");
      client.subscribe("UAS/LED");
      client.subscribe("UAS/Buzzer");
      client.subscribe("UAS/7Segment");
      client.subscribe("UAS/Refresh");
    } else {
      Serial.print("Failed with state ");
      Serial.print(client.state());
      delay(2000);
    }
  }
  readData();
  client.setCallback(callback);
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Pesan diterima [");
  Serial.print(topic);
  Serial.print("] ");
  String data = "";  // variabel untuk menyimpan data yang berbentuk array char
  for (int i = 0; i < length; i++) {
    data += (char)payload[i];  // menyimpan kumpulan char kedalam string
  }

  Serial.println(data);

  if (String(topic) == "UAS/Refresh") {
    if (data == "Refresh") {
      Refresh = true;
    } else {
      Refresh = false;
    }
  }

  if (String(topic) == "UAS/LED") {
    if (data == "ON") {
      StatusLampu = true;
    } else {
      StatusLampu = false;
    }
  }

  if (String(topic) == "UAS/Buzzer") {
    if (data == "ON") {
      StatusBuzzer = true;
    } else {
      StatusBuzzer = false;
    }
  }

  if (String(topic) == "UAS/7Segment") {
    if (data == "ON") {
      Status7Segment = true;
    } else {
      Status7Segment = false;
    }
  }
}

void readData() {
  Serial.println("Membaca Data...");
  if (WiFi.status() == WL_CONNECTED) {
    // Membuat objek HTTPClient untuk melakukan request HTTP
    HTTPClient http;
    // Memulai koneksi HTTP ke server yang ditentukan oleh serverName
    http.begin(serverName);
    // Menambahkan header API key untuk otentikasi
    http.addHeader("X-M2M-Origin", apiKey);
    // Menambahkan header untuk menerima respons dalam format JSON
    http.addHeader("Accept", "application/json");

    // Melakukan HTTP GET request
    int httpResponseCode = http.GET();

    // Mengecek kode respons dari server
    if (httpResponseCode > 0) {
      // Mendapatkan payload atau data respons dari server
      String payload = http.getString();
      // Serial.println(payload);

      // Membuat dokumen JSON dinamis untuk menampung data dari payload
      DynamicJsonDocument doc(1024);
      // Mengurai (deserialisasi) payload JSON ke dalam dokumen JSON
      DeserializationError error = deserializeJson(doc, payload);

      // Mengecek apakah deserialisasi berhasil
      if (!error) {
        // Mengambil objek "m2m:cin" dari dokumen JSON
        JsonObject cinObj = doc["m2m:cin"];
        // Mengambil data "con" sebagai string
        String conData = cinObj["con"];

        // Membuat dokumen JSON statis untuk menampung data "con"
        StaticJsonDocument<256> conDoc;
        // Mengurai (deserialisasi) string "con" JSON ke dalam dokumen JSON
        DeserializationError conError = deserializeJson(conDoc, conData);

        // Mengecek apakah deserialisasi data "con" berhasil
        if (!conError) {
          // Mengambil dan mengonversi data suhu dari dokumen JSON
          float temperature = conDoc["Temp"].as<float>();
          // Mengambil dan mengonversi data kelembaban dari dokumen JSON
          float humidity = conDoc["Hum"].as<float>();
          // Mengambil dan mengonversi data ozon dari dokumen JSON
          float ozon = conDoc["Ozon"].as<float>();
          // Mengambil dan mengonversi data UV dari dokumen JSON
          float uv = conDoc["UV"].as<float>();
          // Mencetak data sensor ke Serial Monitor
          Serial.println("Data sensor:");
          Serial.print("Temperature : ");
          Serial.println(temperature, 2);  // Mencetak suhu dengan 2 angka desimal
          Serial.print("Humidity : ");
          Serial.println(humidity, 2);  // Mencetak kelembaban dengan 2 angka desimal
          Serial.print("Ozon : ");
          Serial.println(ozon, 2);  // Mencetak ozon dengan 2 angka desimal
          Serial.print("UV : ");
          Serial.println(uv, 2);  // Mencetak UV dengan 2 angka desimal
          String Temp = String(temperature);
          client.publish(topicTemp, Temp.c_str());

          String Hum = String(humidity);
          client.publish(topicHum, Hum.c_str());

          String Ozon = String(ozon);
          client.publish(topicOzon, Ozon.c_str());

          String UV = String(uv);
          client.publish(topicUV, UV.c_str());
          client.loop();
        } else {
          // Mencetak pesan error jika terjadi kesalahan saat mengurai data "con"
          Serial.print("Error parsing 'con' JSON: ");
          Serial.println(conError.c_str());
        }
      } else {
        // Mencetak pesan error jika deserialisasi payload JSON gagal
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
      }
    } else {
      // Mencetak pesan error jika terjadi kesalahan pada HTTP request
      Serial.print("Error on HTTP request: ");
      Serial.println(httpResponseCode);
    }

    // Mengakhiri koneksi HTTP
    http.end();
    Refresh = false;
  }
}

void connectToWiFi() {
  // Menghubungkan ke WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println(" connected!");
}

void loop() {
  // put your main code here, to run repeatedly:
  client.loop();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Koneksi WiFi Terputus, Mencoba menghubungkan ulang...");
    connectToWiFi();
  }

  if (Refresh) {
    readData();
  }

  if (StatusLampu) {
    digitalWrite(5, HIGH);
    digitalWrite(4, LOW);
    delay(500);
    digitalWrite(5, LOW);
    digitalWrite(4, HIGH);
    delay(500);
  } else {
    digitalWrite(5, LOW);
    digitalWrite(4, LOW);
  }

  if (StatusBuzzer) {
    digitalWrite(16, HIGH);
    // tone(16, 10);
  } else {
    digitalWrite(16, LOW);
    // noTone(16);
  }

  if (Status7Segment) {
    for (int y = 0; y <= 4; y++) {
      for (int x = 0; x <= 6; x++)
      {
        digitalWrite(pins[x], data[y][x]);
      }
      delay(800);
    }
  } else {
    for (int x = 0; x <= 6; x++) {
      digitalWrite(pins[x], 1);
    }
  }
}
