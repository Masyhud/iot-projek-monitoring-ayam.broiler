#include <ESP8266WiFi.h>
#include <DHT.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

// Konfigurasi pin
#define DHTPIN D3        // Pin untuk sensor DHT11 (D2 pada NodeMCU)
#define DHTTYPE DHT11    // Tipe sensor DHT
#define FAN_PIN D5       // Pin untuk mengontrol kipas (D5 pada NodeMCU)
#define LIGHT_PIN D6     // Pin untuk mengontrol lampu (D6 pada NodeMCU)
#define MQ135_AOUT_PIN A0 // Pin ADC untuk MQ135 AOUT (A0 pada NodeMCU)
#define MQ135_DOUT_PIN D7 // Pin digital untuk MQ135 DOUT (D7 pada NodeMCU)

// Deklarasi variabel-variabel fuzzy logic
float suhuRendah[] = {29, 29 , 32}; 
float suhuNetral[] = {29, 32, 35}; 
float suhuTinggi[] = {32, 35, 35}; 

float kelRendah[] = {65, 75}; 
float kelTinggi[] = {75, 85}; 

float kipasMati[] = {1500,2300}; 
float kipasHidup[] = {1500, 2300}; 

float gasRendah[] = {1, 10}; 
float gasTinggi[] = {10, 20}; 

float lampuMati[] = {500, 1200}; 
float lampuHidup[] = {500, 1200}; 

float MUsuhuren, MUsuhunet, MUsuhutin; //derajat keanggotaan suhu
float MUkelemren, MUkelemtin; //derajat keangotaan kelembapan
float MUgasren, MUgastin; //derajat keanggotaan gas amonia

float R1kipas, R2kipas, R3kipas, R4kipas, R5kipas, R6kipas; //deklarasi variabel untuk pengendalian kipas
float Z1kipas, Z2kipas, Z3kipas, Z4kipas, Z5kipas, Z6kipas; //deklarasi variabel untuk pengendalian kipas
float R1lampu, R2lampu, R3lampu, R4lampu, R5lampu, R6lampu; //deklarasi variabel untuk pengendalian lampu
float Z1lampu, Z2lampu, Z3lampu, Z4lampu, Z5lampu, Z6lampu; //deklarasi variabel untuk pengendalian lampu
float Outputkipas, Outputlampu;

String kipas; //menampung on/off string kipas
String lampu; //menampung on/off string lampu

// Pengaturan WiFi dan Telegram
const char* ssid = "Nokia";
const char* password = "111111112";
const char* telegramBotToken = "7081558097:AAEJh-ed6xmFV9ltwpF6KunFjfqFo2_lqbY";
const char* chatId = "7377591981";

WiFiClientSecure client;
UniversalTelegramBot bot(telegramBotToken, client);

DHT dht(DHTPIN, DHTTYPE);

float normalizeAmmoniaLevel(int rawValue) {
  return (rawValue / 4095.0) * 20.0;
}

// Fungsi untuk menghitung fuzzy Tsukamoto kipas
float fuzzyTsukamotoKipas(float gasValue, float tempValue) {
  // Proses fuzifikasi kendali kipas
  
  // Mencari fungsi keanggotaan suhu rendah
  if (tempValue >= suhuRendah[1]) {
    MUsuhuren = 0;
  } else if (tempValue > suhuRendah[0] && tempValue < suhuRendah[1]) {
    MUsuhuren = (suhuRendah[1] - tempValue) / (suhuRendah[1] - suhuRendah[0]);
  } else if (tempValue <= suhuRendah[0]) {
    MUsuhuren = 1;
  }

  // Mencari fungsi keanggotaan suhu netral
  if (tempValue <= suhuNetral[0] || tempValue >= suhuNetral[2]) {
    MUsuhunet = 0;
  } else if (tempValue > suhuNetral[0] && tempValue < suhuNetral[1]) {
    MUsuhunet = (tempValue - suhuNetral[0]) / (suhuNetral[1] - suhuNetral[0]);
  } else if (tempValue >= suhuNetral[1] && tempValue < suhuNetral[2]) {
    MUsuhunet = (suhuNetral[2] - tempValue) / (suhuNetral[2] - suhuNetral[1]);
  }

  // Mencari fungsi keanggotaan suhu tinggi
  if (tempValue <= suhuTinggi[0]) {
    MUsuhutin = 0;
  } else if (tempValue > suhuTinggi[0] && tempValue < suhuTinggi[1]) {
    MUsuhutin = (tempValue - suhuTinggi[0]) / (suhuTinggi[1] - suhuTinggi[0]);
  } else if (tempValue >= suhuTinggi[1]) {
    MUsuhutin = 1;
  }

  // Mencari fungsi keanggotaan gas rendah
  if (gasValue >= gasRendah[1]) {
    MUgasren = 0;
  } else if (gasValue > gasRendah[0] && gasValue < gasRendah[1]) {
    MUgasren = (gasRendah[1] - gasValue) / (gasRendah[1] - gasRendah[0]);
  } else if (gasValue <= gasRendah[0]) {
    MUgasren = 1;
  }

  // Mencari fungsi keanggotaan gas tinggi
  if (gasValue <= gasTinggi[0]) {
    MUgastin = 0;
  } else if (gasValue > gasTinggi[0] && gasValue < gasTinggi[1]) {
    MUgastin = (gasValue - gasTinggi[0]) / (gasTinggi[1] - gasTinggi[0]);
  } else if (gasValue >= gasTinggi[1]) {
    MUgastin = 1;
  }

  // Membuat rules menggunakan operator AND (min) & OR (max)
 // Inference (Jika suhu tinggi dan gas tinggi maka kipas hidup)
  R1kipas = min(MUsuhutin, MUgastin);
  Z1kipas = (kipasHidup[0] +  (kipasHidup[1] - kipasHidup[0]) * R1kipas );
  //suhu netral dan gas amonia rendah , maka kipas mati
  R2kipas = min(MUsuhunet, MUgasren);
  Z2kipas = (kipasMati[1] -  (kipasMati[1] - kipasMati[0]) * R2kipas );
  //suhu rendah dan gas amonia rendah, maka kipas mati
  R3kipas = min(MUsuhuren, MUgasren);
  Z3kipas = (kipasMati[1] -  (kipasMati[1] - kipasMati[0]) * R3kipas );
  //suhu tinggi dan gas amonia rendah, maka kipas hidup
  R4kipas = min(MUsuhutin, MUgasren);
  Z4kipas = (kipasHidup[0] +  (kipasHidup[1] - kipasHidup[0]) * R4kipas );
  //suhu netral dan gas tinggi, maka kipas hidup
  R5kipas = min(MUsuhunet, MUgastin);
  Z5kipas = (kipasHidup[0] +  (kipasHidup[1] - kipasHidup[0]) * R5kipas );
  //suhu rendah gas amonia tinggi, maka kipas hidup
  R6kipas = min(MUsuhuren, MUgastin);
  Z6kipas = (kipaHidup[0] +  (kipasHidupi[1] - kipasHidup[0]) * R6kipas );
  

  Outputkipas = ((R1kipas * Z1kipas) + (R2kipas * Z2kipas) + (R3kipas * Z3kipas) + (R4kipas * Z4kipas) + (R5kipas * Z5kipas) + (R6kipas * Z6kipas)) /
                (R1kipas + R2kipas + R3kipas + R4kipas + R5kipas + R6kipas);
  return Outputkipas;
}

// Fungsi untuk menghitung fuzzy Tsukamoto lampu
float fuzzyTsukamotoLampu(float tempValue, float kelemValue) {
  // Proses fuzifikasi kendali lampu
  // Mencari fungsi keanggotaan suhu rendah
  if (tempValue >= suhuRendah[1]) {
    MUsuhuren = 0;
  } else if (tempValue > suhuRendah[0] && tempValue < suhuRendah[1]) {
    MUsuhuren = (suhuRendah[0] + tempValue) / (suhuRendah[1] - suhuRendah[0]);
  } else if (tempValue <= suhuRendah[0]) {
    MUsuhuren = 1;
  }

  // Mencari fungsi keanggotaan suhu netral
  if (tempValue <= suhuNetral[0] || tempValue >= suhuNetral[2]) {
    MUsuhunet = 0;
  } else if (tempValue > suhuNetral[0] && tempValue < suhuNetral[1]) {
    MUsuhunet = (tempValue - suhuNetral[0]) / (suhuNetral[1] - suhuNetral[0]);
  } else if (tempValue >= suhuNetral[1] && tempValue < suhuNetral[2]) {
    MUsuhunet = (suhuNetral[2] - tempValue) / (suhuNetral[2] - suhuNetral[1]);
  }

  // Mencari fungsi keanggotaan suhu tinggi
  if (tempValue <= suhuTinggi[0]) {
    MUsuhutin = 0;
  } else if (tempValue > suhuTinggi[0] && tempValue < suhuTinggi[1]) {
    MUsuhutin = (tempValue - suhuTinggi[0]) / (suhuTinggi[1] - suhuTinggi[0]);
  } else if (tempValue >= suhuTinggi[1]) {
    MUsuhutin = 1;
  }

  // Mencari fungsi keanggotaan kelembapan rendah
  if (kelemValue >= kelRendah[1]) {
    MUkelemren = 0;
  } else if (kelemValue > kelRendah[0] && kelemValue < kelRendah[1]) {
    MUkelemren = (kelRendah[1] - kelemValue) / (kelRendah[1] - kelRendah[0]);
  } else if (kelemValue <= kelRendah[0]) {
    MUkelemren = 1;
  }

  // Mencari fungsi keanggotaan kelembapan tinggi
  if (kelemValue <= kelTinggi[0]) {
    MUkelemtin = 0;
  } else if (kelemValue > kelTinggi[0] && kelemValue < kelTinggi[1]) {
    MUkelemtin = (kelemValue - kelTinggi[0]) / (kelTinggi[1] - kelTinggi[0]);
  } else if (kelemValue >= kelTinggi[1]) {
    MUkelemtin = 1;
  }

  // Membuat rules menggunakan operator AND (min) & OR (max)
  // Inference (Jika suhu rendah dan kelembapan rendah maka lampu hidup)
  R1lampu = min(MUsuhuren, MUkelemtin);
  Z1lampu = (lampuHidup[0] +  (lampuHidup[1] - lampuHidup[0]) * R1lampu );
    
  //suhu tinggi kelembapan tinggi, maka lampu mati
  R2lampu = min(MUsuhunet, MUkelemtin);
  Z2lampu = (lampuMati[1] -  (lampuMati[1] - lampuMati[0]) * R2lampu );

  //suhu netral dan kelembapan rendah, maka lampu mati
  R3lampu = min(MUsuhutin, MUkelemtin);
  Z3lampu = (lampuMati[1] -  (lampuMati[1] - lampuMati[0]) * R3lampu );

  //suhu tinggi dan kelembapan rendah, maka lampu mati
  R4lampu = min(MUsuhuren, MUkelemren);
  Z4lampu = (lampuHidup[0] + (lampuHidup[1] - lampuHidup[0]) * R4lampu  );

  //suhu rendah dan kelembapan tinggi , maka lampu hidup
  R5lampu = min(MUsuhunet, MUkelemren);
  Z5lampu = (lampuHidup[0] + (lampuHidup[1] - lampuHidup[0]) *R5lampu );

  //suhu netral dan kelembapan tinggi, maka lampu hidup
  R6lampu = min(MUsuhutin, MUkelemren);
  Z6lampu = (lampuHidup[0] + (lampuHidup[1] - lampuHidup[0]) * R6lampu);

   
  Outputlampu = ((R1lampu * Z1lampu) + (R2lampu * Z2lampu) + (R3lampu * Z3lampu) + (R4lampu * Z4lampu) + (R5lampu * Z5lampu) + (R6lampu * Z6lampu)) / (R1lampu + R2lampu + R3lampu + R4lampu + R5lampu + R6lampu);

  return Outputlampu;
}


// Fungsi untuk mengirim pesan Telegram
void sendTelegramMessage(float temperature, float humidity, float normalizedAmmoniaLevel, bool isFanOn, bool isLightOn) {
  String message = "Status Sensor:\n";
  message += "Suhu: " + String(temperature) + " C\n";
  message += "Kelembapan: " + String(humidity) + " %\n";
  message += "Gas Amonia: " + String(normalizedAmmoniaLevel, 1) + " ppm\n";
  message += "Output: \n";
  message += "Kipas: " + String(isFanOn ? "Hidup" : "Mati") + "\n";
  message += "Lampu: " + String(isLightOn ? "Hidup" : "Mati") + "\n";
  
  bot.sendMessage(chatId, message, "");
}

void setup() {
  Serial.begin(115200);
  dht.begin();

  pinMode(FAN_PIN, OUTPUT);
  pinMode(LIGHT_PIN, OUTPUT);
  pinMode(MQ135_DOUT_PIN, INPUT);

  digitalWrite(FAN_PIN, HIGH); // Matikan kipas
  digitalWrite(LIGHT_PIN, HIGH); // Matikan lampu

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("Terhubung ke WiFi");
  Serial.println("IP Address: ");
  Serial.println(WiFi.localIP());
  client.setInsecure();
}

unsigned long previousMillis = 0;
const long interval = 1800000; // 30 menit dalam milidetik

void loop() {
  delay(2000); // Penundaan 1 detik sebelum membaca sensor dan mengontrol perangkat

  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();

  int mq135AoutValue = analogRead(MQ135_AOUT_PIN);
  float normalizedAmmoniaLevel = normalizeAmmoniaLevel(mq135AoutValue);

  Outputkipas = fuzzyTsukamotoKipas(normalizedAmmoniaLevel, temperature);
  Outputlampu = fuzzyTsukamotoLampu(temperature, humidity);

delay(1000);
  // Cek on/off kipas
  bool isFanOn = (Outputkipas >= 1550);
  digitalWrite(FAN_PIN, isFanOn ? LOW : HIGH);

delay(1000);
  // Cek on/off lampu
  bool isLightOn = (Outputlampu >= 800);
  digitalWrite(LIGHT_PIN, isLightOn ? LOW : HIGH);


// Print data sensor dan output ke Serial Monitor
  Serial.println("======== Data Sensor dan Output Fuzzy ========");
  Serial.print("Suhu: ");
  Serial.print(temperature);
  Serial.println(" C");

  Serial.print("Kelembapan: ");
  Serial.print(humidity);
  Serial.println(" %");

  Serial.print("Gas Amonia: ");
  Serial.print(normalizedAmmoniaLevel);
  Serial.println(" ppm");

  Serial.print("Output Fuzzy Kipas: ");
  Serial.println(Outputkipas);

  Serial.print("Kipas: ");
  Serial.println(isFanOn ? "Hidup" : "Mati");

  Serial.print("Output Fuzzy Lampu: ");
  Serial.println(Outputlampu);

  Serial.print("Lampu: ");
  Serial.println(isLightOn ? "Hidup" : "Mati");
  Serial.println("=============================================");
  
  // Kirim pesan ke Telegram setiap interval waktu tertentu
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    sendTelegramMessage(temperature, humidity, normalizedAmmoniaLevel, isFanOn, isLightOn);
  }

  delay(1000); // Penundaan 1 detik sebelum membaca sensor dan mengontrol perangkat
}