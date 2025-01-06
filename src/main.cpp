// Inisialisasi kebutuhan login Firebase
#define API_KEY "##########"
#define PROJECT_ID "##########"
#define DATABASE_URL "##########"
#define USER_EMAIL "##########"
#define USER_PASSWORD "##########"

// Inisialisasi pin komponen
#define KEYPAD_ROW 4
#define KEYPAD_COLUMN 4
#define RELAY 27
#define GREEN_LED 26
#define RED_LED 25
#define BUZZER 13
#define INSIDE_BUTTON 14
#define MAGNETIC_SWITCH 23

// Inisialisasi kebutuhan Library yang digunakan
#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiClient.h>
#include <Keypad.h>
#include <Adafruit_Fingerprint.h>
#include <LiquidCrystal_I2C.h>
#include <Firebase_ESP_Client.h>
#include <ArduinoJson.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>
#include <HTTPClient.h>
#include <time.h>

HTTPClient HttpClient;

// Penentuan jumlah baris dan kolom Keypad
char keys[KEYPAD_ROW][KEYPAD_COLUMN] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}};
byte pin_rows[KEYPAD_ROW] = {19, 18, 5, 4};
byte pin_column[KEYPAD_COLUMN] = {2, 15, 33, 32};
Keypad keypad = Keypad(makeKeymap(keys), pin_rows, pin_column, KEYPAD_ROW, KEYPAD_COLUMN);

// Penetapan variabel dan pin Serial Fingerprint yang digunakan
HardwareSerial mySerial(2);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

// Penetapan address LCD I2C, jumlah baris dan kolom LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Penetapan variabel library Firebase yang dibutuhkan
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Inisialisasi object dari library WiFiMulti
WiFiMulti wifiMulti;

// Inisialisasi NTP server untuk timestamp
const char *ntpServer = "pool.ntp.org";

// Simpan waktu millis sebelumnya
unsigned long sendDataPrevMillis = 0;

// Mendapatkan waktu timestamp
unsigned long getTime()
{
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    Serial.println("Failed to obtain time");
    return (0);
  }
  time(&now);
  return now;
}

// Inisialisasi variabel Boolean
bool outputState = false;
bool fingerUsed = false;
bool passwordUsed = false;

// Inisialisasi variabel Integer
int fingerWrong = 0;
int passwordWrong = 0;
int getInput = 0;
int fingerCount = 0;
int insideButton;
int timestamp;

// Inisialisasi variabel String
String password = "123456";
String passwordPath;
String inputPassword;
String uid;
String outputPath;
String doorPath;
String lockPath;
String fcmTokenPath;
String fcmToken;
String accessToken;

// Menambahkan karakter '*' sesuai panjang inputPassword
String getStar(int length)
{
  String result = "";
  for (int i = 0; i < length; i++)
  {
    result += "*";
  }
  return result;
}

// Function untuk menghasilkan output pada LCD
void lcdOutput(String firstRow, String secondRow)
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(firstRow);
  lcd.setCursor(0, 1);
  lcd.print(secondRow);
}

// Function untuk inisialisasi LCD
void lcdInit()
{
  lcd.init();
  lcd.backlight();
  Serial.println(F("LCD Init Success!"));
  lcdOutput("Halo!", "Selamat Datang");
  delay(1000);
}

// Function untuk inisialisasi WiFi
void wifiInit()
{
  delay(10);
  WiFi.mode(WIFI_STA);

  // Tambahkan list jaringan WiFi yang ingin disambungkan
  wifiMulti.addAP("SATA Alin", "dasadarma@1973");
  wifiMulti.addAP("Infinix 11s", "menorehpro");
  wifiMulti.addAP("Wifi kos 1", "alternatifsatu");

  // Mencari jaringan WiFi di sekitar
  lcdOutput("Mencari jaringan", "WiFi sekitar");
  int n = WiFi.scanNetworks();
  Serial.println(F("scan done"));
  if (n == 0)
  {
    Serial.println(F("no networks found"));
    while (1)
    {
      lcdOutput("Tidak ditemukan", "jaringan WiFi");
      delay(1000);
      lcdOutput("Periksa koneksi", "WiFi");
      delay(1000);
      lcdOutput("Dan reset", "alat ini");
      delay(1000);
    }
  }
  else
  {
    Serial.print(n);
    Serial.println(F(" networks found"));
    for (int i = 0; i < n; ++i)
    {
      // Print SSID and RSSI untuk setiap jaringan WiFi yang ditemukan
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(")");
      Serial.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? " " : "*");
      lcdOutput("Menyambung WiFi", "..........");
      delay(10);
    }
  }

  // Menyambung ke WiFi menggunakan WiFiMulti
  // (menyambungkan menggunakan SSID dengan sinyal paling kuat)
  Serial.println("Connecting Wifi...");
  if (wifiMulti.run() == WL_CONNECTED)
  {
    Serial.println(F(""));
    Serial.println(F("WiFi connected"));
    Serial.println(F("IP address: "));
    Serial.println(WiFi.localIP());
    lcdOutput("WiFi tersambung", "IP:");
    lcd.setCursor(3, 1);
    lcd.print(WiFi.localIP());
    delay(1000);
  }
}

// Function untuk inisialisasi Firebase
void firebaseInit()
{
  // Mengkonfigurasikan database URL dan API key Firebase
  config.database_url = DATABASE_URL;
  config.api_key = API_KEY;

  // Melakukan proses login ke Firebase
  lcdOutput("Memproses login", "pada Firebase");
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // Mendapatkan user UID setelah login ke Firebase
  Serial.println("Getting User UID");
  while ((auth.token.uid) == "")
  {
    Serial.print(F("."));
    delay(500);
  }
  Serial.println(F(""));
  uid = auth.token.uid.c_str();
  Serial.print(F("User UID: "));
  Serial.println(uid);
  lcdOutput("Login Firebase", "berhasil!");
  delay(1000);

  // Menetapkan variabel path yang sudah ada
  passwordPath = uid + "/password";
  outputPath = uid + "/door/takeImage";
  lockPath = uid + "/door/openLock";
  doorPath = uid + "/door/doorLock";
  fcmTokenPath = uid + "/fcmToken";

  // Menetapkan password sesuai dengan data dari Firebase
  if (Firebase.ready())
  {
    if (Firebase.RTDB.getInt(&fbdo, passwordPath))
    {
      if (fbdo.dataType() == "int")
      {
        int getData = fbdo.intData();
        String readData = String(getData);
        Serial.print(F("Password data received: "));
        Serial.println(readData);

        password = readData;
        Serial.print(F("Password changed: "));
        Serial.println(password);
      }
    }
  }
  else
  {
    Serial.println(fbdo.errorReason());
  }
}

// Function untuk inisialisasi Fingerprint Sensor
void fingerprintInit()
{
  Serial.println(F("Adafruit Fingerprint detect test"));
  lcdOutput("Memeriksa sensor", "sidik jari");

  // Menetapkan data rate untuk port serial sensor
  finger.begin(57600);
  delay(5);

  if (finger.verifyPassword())
  {
    // Jika sensor fingerprint ditemukan
    Serial.println(F("Found fingerprint sensor"));
    lcdOutput("Sensor sidik", "jari terdeteksi");
    delay(1000);
  }
  else
  {
    Serial.println(F("Did not find fingerprint sensor :("));
    while (1)
    {
      lcdOutput("Sensor sidikjari", "tidak ditemukan");
      delay(1000);
      lcdOutput("Harap periksa", "sensor sidikjari");
      delay(1000);
      lcdOutput("Kemudian lakukan", "reset sistem");
      delay(1000);
    }
  }

  // Mendapatkan jumlah template fingerprint dari sensor
  finger.getTemplateCount();
  Serial.print(F("Sensor contains "));
  Serial.print(finger.templateCount);
  Serial.println(F(" templates"));
  fingerCount = finger.templateCount;
  lcdOutput(String(finger.templateCount), "terdaftar");
  lcd.setCursor(3, 0);
  lcd.print("sidik jari");

  Serial.print(F("fingerCount: "));
  Serial.println(fingerCount);
  delay(1000);
}

// Function untuk mengirimkan foto ke aplikasi Website
void setImageApp()
{
  if (Firebase.ready())
  {
    // Mengirim instruksi ke ESP32CAM untuk mengambil gambar
    if (Firebase.RTDB.getInt(&fbdo, outputPath))
    {
      if (fbdo.dataType() == "int")
      {
        int readData = fbdo.intData();
        Serial.print(F("Data received: "));
        Serial.println(readData);
        lcdOutput("Mengambil gambar", "..........");

        if (readData == 0)
        {
          if (Firebase.RTDB.setInt(&fbdo, outputPath, 1))
          {
            Serial.println(F("Change data to 1 Success"));
          }
        }
        delay(500);
      }
    }
  }
  else
  {
    Serial.println(fbdo.errorReason());
  }
}

void getToken()
{
  String url = "https://doorlock-app.vercel.app/api/generate/token";

  HttpClient.begin(url);

  int httpResponse = HttpClient.GET();

  if (httpResponse > 0)
  {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponse);
    String payload = HttpClient.getString();

    JsonDocument doc;
    deserializeJson(doc, payload);

    String tokens = doc["accessToken"];
    accessToken = tokens;
  }
  else
  {
    Serial.print("Error code: ");
    Serial.println(httpResponse);
  }
  // Free resources
  HttpClient.end();
}

void sendNotification(String fcmToken, String title, String body)
{
  if (accessToken)
  {
    timestamp = getTime();

    String fcmUrl = "https://fcm.googleapis.com/v1/projects/" + String(PROJECT_ID) + "/messages:send";
    String auth = "Bearer " + accessToken;

    String data = "{\"message\":{\"token\":\"" + fcmToken + "\",\"notification\":{\"body\":\"" + body + "\", \"title\":\"" + title + "\"}}}";

    HttpClient.begin(fcmUrl);

    HttpClient.addHeader("Authorization", auth);
    HttpClient.addHeader("Content-Type", "application/json");

    int httpCode = HttpClient.POST(data);

    if (httpCode == HTTP_CODE_OK)
    {
      FirebaseJson notifJson;

      Serial.println(F("Notification Sent To The Phone"));

      String notifPath = uid + "/notifications/" + String(timestamp);

      notifJson.set("/message", body);
      notifJson.set("/timestamp", timestamp);
      notifJson.set("/status", title == "Info" ? "green" : "yellow");

      Serial.printf("Add notification to database... %s\n", Firebase.RTDB.setJSON(&fbdo, notifPath.c_str(), &notifJson) ? "send notif to database ok" : fbdo.errorReason().c_str());
    }
    else
    {
      Serial.println(F("Error on sending notification"));
      Serial.println(httpCode);
      Serial.println(HttpClient.getString());
    }
    HttpClient.end(); // Free the resources
  }
  else
  {
    Serial.println("Access Token undefined");
  }
}

// Function untuk kondisi input yang salah dimasukkan
void wrongEvent(String input)
{
  Serial.print(F("Wrong "));
  Serial.print(input);
  Serial.println(F(" input"));
  lcdOutput("Input", "yang masuk salah");
  lcd.setCursor(6, 0);
  lcd.print(input);

  for (int i = 0; i < 3; i++)
  {
    digitalWrite(BUZZER, HIGH);
    delay(200);
    digitalWrite(BUZZER, LOW);
    delay(200);
  }
  delay(500);

  if (input == "fingerprint")
  {
    fingerWrong += 1;
  }
  else if (input == "password")
  {
    passwordWrong += 1;
  }
  else
  {
    Serial.println(F("Invalid input"));
  }

  if (fingerWrong == 3 || passwordWrong == 3)
  {
    Serial.println(F("Wrong Input 3 times"));

    if (Firebase.ready())
    {
      if (Firebase.RTDB.getString(&fbdo, fcmTokenPath))
      {
        if (fbdo.dataType() == "string")
        {
          String readData = fbdo.stringData();
          Serial.print(F("FCM Token Received: "));
          Serial.println(readData);

          fcmToken = readData;
        }
      }
    }

    Serial.println(F("Send a notification..."));

    // Menampilkan input salah 3 kali terdeteksi pada LCD
    lcdOutput("Input", "salah masuk 3x");
    lcd.setCursor(6, 0);
    lcd.print(input);
    delay(500);
    // Membunyikan buzzer panjang 1 kali
    digitalWrite(BUZZER, HIGH);
    delay(1000);
    digitalWrite(BUZZER, LOW);

    // Mengirimkan gambar ke aplikasi Website
    setImageApp();

    if (fingerWrong == 3)
    {
      // Kirim notifikasi fingerprint salah
      if (fcmToken)
      {
        getToken();
        Serial.println(F("Send notification to website application"));
        sendNotification(fcmToken, "Warning", "Input fingerprint terdeteksi salah hingga 3 kali");
      }
    }

    if (passwordWrong == 3)
    {
      // Kirim notifikasi password salah
      if (fcmToken)
      {
        getToken();
        Serial.println(F("Send notification to website application"));
        sendNotification(fcmToken, "Warning", "Input nomor pin terdeteksi salah hingga 3 kali");
      }
    }

    // Mereset proses otentikasi
    getInput = 0;
    fingerWrong = 0;
    passwordWrong = 0;

    // Menampilkan indikator pada LCD bahwa input sudah di-reset
    lcdOutput("Input", "telah di-reset");
    lcd.setCursor(6, 0);
    lcd.print(input);
    delay(500);
    lcdOutput("Silahkan ulangi", "memasukkan input");
    delay(1000);
  }

  // Mengubah LCD ke tampilan default
  lcdOutput("Masukkan input", "untuk membuka");
}

void outputProcess(String source)
{
  Serial.println("Input sesuai terdeteksi!");
  if (Firebase.ready())
  {
    if (Firebase.RTDB.getString(&fbdo, fcmTokenPath))
    {
      if (fbdo.dataType() == "string")
      {
        String readData = fbdo.stringData();
        Serial.print(F("FCM Token Received: "));
        Serial.println(readData);

        fcmToken = readData;
      }
    }
  }

  // Mengubah kondisi Relay, LED merah, dan LED hijau
  digitalWrite(RELAY, HIGH);
  digitalWrite(RED_LED, LOW);
  digitalWrite(GREEN_LED, HIGH);

  if (source != "button")
  {
    setImageApp();
  }

  // Memberikan indikator pada tampilan LCD dan Buzzer
  lcdOutput("Kunci pintu", "terbuka!");
  Serial.println("Kunci pintu terbuka");
  digitalWrite(BUZZER, HIGH);
  delay(1000);
  digitalWrite(BUZZER, LOW);
  delay(1000);
  digitalWrite(BUZZER, HIGH);
  delay(1000);
  digitalWrite(BUZZER, LOW);
  delay(1000);

  // Kirim notifikasi ke aplikasi website
  if (fcmToken)
  {
    getToken();
    Serial.println(F("Send notification to website application"));
    sendNotification(fcmToken, "Info", "Kunci Pintu Terbuka");
  }

  // Ubah keterangan kunci pintu pada aplikasi Website
  if (Firebase.ready())
  {
    if (Firebase.RTDB.setInt(&fbdo, doorPath, 0))
    {
      Serial.println(F("Change doorPath data to 0"));
    }
  }
  else
  {
    Serial.println(fbdo.errorReason());
  }

  // Tunggu 2 detik dan baca kondisi pintu apakah terbuka atau tertutup, jika tertutup maka timer mengunci pintu akan berjalan
  delay(2000);
  Serial.println(F("Menunggu pintu tertutup"));
  Serial.println(F(""));
  lcdOutput("Menunggu pintu", "untuk tertutup");
  while (digitalRead(MAGNETIC_SWITCH) == 1)
  {
    Serial.print(".");
    delay(1000);
  }

  // Memberikan indikator pada LCD bahwa kunci pintu akan kembali terkunci dalam 5 detik
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Mengunci dalam");
  for (int i = 5; i > 0; i--)
  {
    lcd.setCursor(0, 1);
    lcd.print(i);
    lcd.print(" detik");
    delay(1000);
    lcd.setCursor(0, 1);
    lcd.print("          ");

    // Menetapkan pembacaan input dari tombol dan magnetic switch
    insideButton = digitalRead(INSIDE_BUTTON);

    if (insideButton == 0)
    {
      break;
    }
  }

  // Memberikan indikator pada LCD dan Buzzer pintu kembali terkunci
  lcdOutput("Pintu kembali", "terkunci");
  for (int i = 0; i < 5; i++)
  {
    digitalWrite(BUZZER, HIGH);
    delay(100);
    digitalWrite(BUZZER, LOW);
    delay(100);
  }
  delay(1000);

  // Mengunci pintu kembali dengan mengubah kondisi relay, LED merah, dan LED hijau
  digitalWrite(RELAY, LOW);
  digitalWrite(RED_LED, HIGH);
  digitalWrite(GREEN_LED, LOW);

  // Mengubah tampilan LCD kembali ke default
  lcdOutput("Masukkan input", "untuk membuka");

  // Kirim notifikasi ke aplikasi website
  if (fcmToken)
  {
    getToken();
    Serial.println(F("Send notification to website application"));
    sendNotification(fcmToken, "Info", "Kunci Pintu Kembali Terkunci");
  }

  // Ubah keterangan kunci pintu pada aplikasi Website dan kondisi tombol pada aplikasi
  if (Firebase.ready())
  {
    if (Firebase.RTDB.setInt(&fbdo, doorPath, 1))
    {
      Serial.println(F("Change doorPath data to 1"));
    }

    // Kembalikan data perintah buka kunci ke 0
    if (Firebase.RTDB.setInt(&fbdo, lockPath, 0))
    {
      Serial.println("Change data to default Success");
    }
  }
  else
  {
    Serial.println(fbdo.errorReason());
  }

  // Reset variabel getInput, fingerUsed, passwordUsed, fingerWrong, dan passwordWrong
  getInput = 0;
  fingerUsed = false;
  passwordUsed = false;
  fingerWrong = 0;
  passwordWrong = 0;
}

// Function untuk proses pendafataran sidik jari
uint8_t getFingerprintEnroll(int id)
{

  int p = -1;
  Serial.print("Waiting for valid finger to enroll as #");
  Serial.println(id);
  lcdOutput("Letakkan jari", "Anda pada sensor");
  for (int i = 0; i < 2; i++)
  {
    digitalWrite(BUZZER, HIGH);
    delay(100);
    digitalWrite(BUZZER, LOW);
    delay(100);
  }
  while (p != FINGERPRINT_OK)
  {
    p = finger.getImage();
    switch (p)
    {
    // case FINGERPRINT_OK:
    //   Serial.println("Image taken");
    //   break;
    // case FINGERPRINT_NOFINGER:
    //   Serial.print(".");
    //   break;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      lcdOutput("Terdapat error", "-Comm Error-");
      break;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println("Imaging error");
      lcdOutput("Terdapat error", "-Imaging Error-");
      break;
    default:
      Serial.println("Unknown error");
      lcdOutput("Terdapat error", "-Unknown Error-");
      break;
    }
  }

  // OK success!

  p = finger.image2Tz(1);
  switch (p)
  {
  // case FINGERPRINT_OK:
  //   Serial.println("Image converted");
  //   break;
  case FINGERPRINT_IMAGEMESS:
    Serial.println("Image too messy");
    lcdOutput("Ulangi scan jari", "dengan benar");
    return p;
  case FINGERPRINT_PACKETRECIEVEERR:
    Serial.println("Communication error");
    lcdOutput("Terdapat error", "-Comm error-");
    return p;
  case FINGERPRINT_FEATUREFAIL:
    Serial.println("Could not find fingerprint features");
    lcdOutput("Terdapat error", "-Feature Fail-");
    return p;
  case FINGERPRINT_INVALIDIMAGE:
    Serial.println("Could not find fingerprint features");
    lcdOutput("Terdapat error", "-Invalid Image-");
    return p;
  default:
    Serial.println("Unknown error");
    lcdOutput("Terdapat error", "-Unknown Error-");
    return p;
  }

  lcdOutput("Lepaskan jari", "Anda dari sensor");
  for (int i = 0; i < 1; i++)
  {
    digitalWrite(BUZZER, HIGH);
    delay(500);
    digitalWrite(BUZZER, LOW);
    delay(500);
  }
  delay(2000);
  p = 0;
  while (p != FINGERPRINT_NOFINGER)
  {
    p = finger.getImage();
  }
  Serial.print("ID ");
  Serial.println(id);
  p = -1;
  Serial.println("Place same finger again");
  lcdOutput("Letakkan kembali", "jari yang sama");
  for (int i = 0; i < 2; i++)
  {
    digitalWrite(BUZZER, HIGH);
    delay(100);
    digitalWrite(BUZZER, LOW);
    delay(100);
  }
  while (p != FINGERPRINT_OK)
  {
    p = finger.getImage();
    switch (p)
    {
    // case FINGERPRINT_OK:
    //   Serial.println("Image taken");
    //   break;
    // case FINGERPRINT_NOFINGER:
    //   Serial.print(".");
    //   break;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      lcdOutput("Terdapat error", "-Comm Error-");
      break;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println("Imaging error");
      lcdOutput("Terdapat error", "-imagine Error-");
      break;
    default:
      Serial.println("Unknown error");
      lcdOutput("Terdapat error", "-Unknown Error-");
      break;
    }
  }

  // OK success!

  p = finger.image2Tz(2);
  switch (p)
  {
  // case FINGERPRINT_OK:
  //   Serial.println("Image converted");
  //   break;
  case FINGERPRINT_IMAGEMESS:
    Serial.println("Image too messy");
    lcdOutput("Ulangi scan jari", "dengan benar");
    return p;
  case FINGERPRINT_PACKETRECIEVEERR:
    Serial.println("Communication error");
    lcdOutput("Terdapat error", "-Comm Error-");
    return p;
  case FINGERPRINT_FEATUREFAIL:
    Serial.println("Could not find fingerprint features");
    lcdOutput("Terdapat error", "-Feature Fail-");
    return p;
  case FINGERPRINT_INVALIDIMAGE:
    Serial.println("Could not find fingerprint features");
    lcdOutput("Terdapat error", "-Invalid Image-");
    return p;
  default:
    Serial.println("Unknown error");
    lcdOutput("Terdapat error", "-Unknown Error-");
    return p;
  }

  // OK converted!
  Serial.print("Creating model for #");
  Serial.println(id);
  lcdOutput("Menyimpan sidik", "Jari | ID:");
  lcd.setCursor(11, 1);
  lcd.print(id);

  p = finger.createModel();
  // if (p == FINGERPRINT_OK)
  // {
  //   Serial.println("Prints matched!");
  // }
  if (p == FINGERPRINT_PACKETRECIEVEERR)
  {
    Serial.println("Communication error");
    lcdOutput("Terdapat error", "-Comm Error-");
    return p;
  }
  else if (p == FINGERPRINT_ENROLLMISMATCH)
  {
    Serial.println("Fingerprints did not match");
    lcdOutput("Sidik jari sebe-", "lumnya taksesuai");
    return p;
  }
  else
  {
    Serial.println("Unknown error");
    lcdOutput("Terdapat error", "-Unknown Error-");
    return p;
  }

  Serial.print("ID ");
  Serial.println(id);
  p = finger.storeModel(id);
  // if (p == FINGERPRINT_OK)
  // {
  //   Serial.println("Stored!");
  // }
  if (p == FINGERPRINT_PACKETRECIEVEERR)
  {
    Serial.println("Communication error");
    lcdOutput("Terdapat error", "-Comm Error-");
    return p;
  }
  else if (p == FINGERPRINT_BADLOCATION)
  {
    Serial.println("Could not store in that location");
    lcdOutput("Sidik jari", "gagal disimpan");
    return p;
  }
  else if (p == FINGERPRINT_FLASHERR)
  {
    Serial.println("Error writing to flash");
    lcdOutput("Terdapat error", "-Flash Error-");
    return p;
  }
  else
  {
    Serial.println("Unknown error");
    lcdOutput("Terdapat error", "-Unknown Error-");
    return p;
  }

  lcdOutput("Sidik jari Anda", "sudah disimpan!");
  for (int i = 0; i < 1; i++)
  {
    digitalWrite(BUZZER, HIGH);
    delay(1000);
    digitalWrite(BUZZER, LOW);
    delay(1000);
  }

  // Mengubah tampilan LCD kembali ke default
  lcdOutput("Masukkan input", "untuk membuka");

  return true;
}

void checkFirebaseInput()
{
  if (Firebase.ready() && (millis() - sendDataPrevMillis > 1000 || sendDataPrevMillis == 0))
  {
    sendDataPrevMillis = millis();

    if (Firebase.RTDB.getInt(&fbdo, lockPath))
    {
      if (fbdo.dataType() == "int")
      {
        int readData = fbdo.intData();
        if (readData == 1)
        {
          getInput = 2;
          passwordUsed = true;
          fingerUsed = true;
        }
      }
    }
  }
  // else
  // {
  //   // Tampilkan error ketika Firebase error
  //   Serial.println(fbdo.errorReason());
  // }
}

// Function untuk proses input dari password
void passwordInput()
{
  char key = keypad.getKey();

  if (key != NO_KEY)
  {
    if (key == '*')
    {
      inputPassword = ""; // reset variabel inputPassword

      // Mengubah tampilan LCD ke default
      lcdOutput("Masukkan input", "untuk membuka");
    }
    else if (key == '#')
    {
      if (inputPassword == password)
      {
        // Ketika password yang dimasukkan benar
        Serial.println(F("Pin Password is valid"));
        lcdOutput("Nomor pin yang", "masuk benar!");

        // Bunyikan buzzer
        for (int i = 0; i < 3; i++)
        {
          digitalWrite(BUZZER, HIGH);
          delay(100);
          digitalWrite(BUZZER, LOW);
          delay(100);
        }

        // Tambahkan 1 input yang terdeteksi ke variabel getInput
        getInput += 1;
        // Ubah variabel passwordUsed menandakan password sudah digunakan
        passwordUsed = true;
        inputPassword = ""; // reset variabel inputPassword

        // Mengubah tampilan jumlah input benar pada LCD
        lcdOutput("Input terdeteksi", String(getInput));
        lcd.setCursor(1, 1);
        lcd.print("/2");
      }
      else if (inputPassword == "A")
      {
        Serial.println(F("Registering new fingerprint"));
        lcdOutput("Memproses daftar", "sidik jari baru");

        // Reset variabel getInput, fingerUsed, passwordUsed, fingerWrong,
        // passwordWrong dan inputPassword
        getInput = 0;
        fingerUsed = false;
        passwordUsed = false;
        fingerWrong = 0;
        passwordWrong = 0;
        inputPassword = "";

        int id = fingerCount + 1;
        while (!getFingerprintEnroll(id))
          ;
      }
      else
      {
        // Ketika password yang dimasukkan salah
        Serial.println(F("Pin password is wrong"));
        // lcdOutput("Nomor pin yang", "masuk");
        delay(500);
        inputPassword = ""; // reset variabel inputPassword

        // Jalankan function wrongEvent dengan parameter password
        wrongEvent("password");
      }
    }
    else
    {
      inputPassword += key; // Tambah karakter batu ke variabel inputPassword
      lcdOutput("Nomor pin:", inputPassword);
    }
  }
}

// Membaca input dari fingerprint dan aksinya
int fingerprintInput()
{
  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK)
    return -1;
  // OK success!
  p = finger.image2Tz();
  if (p != FINGERPRINT_OK)
    return -1;
  // OK converted!
  p = finger.fingerSearch();
  if (p == FINGERPRINT_OK)
  {
    Serial.println("Found a print match!");
  }
  else if (p == FINGERPRINT_NOTFOUND)
  {
    Serial.println("Did not find a match");
    wrongEvent("fingerprint");
    return p;
  }
  else
  {
    Serial.println("Unknown error");
    return p;
  }

  // Menemukan sidik jari yang sesuai
  Serial.print("Found ID #");
  Serial.print(finger.fingerID);
  Serial.print(" with confidence of ");
  Serial.println(finger.confidence);

  // Tambahkan 1 input yang terdeteksi ke variabel getInput
  getInput += 1;
  // Ubah variabel fingerUsed menandakan sensor fingerprint sudah digunakan
  fingerUsed = true;

  // Mengubah tampilan di LCD
  lcdOutput("Sensor sidik", "jari benar!");

  for (int i = 0; i < 3; i++)
  {
    digitalWrite(BUZZER, HIGH);
    delay(100);
    digitalWrite(BUZZER, LOW);
    delay(100);
  }

  // Mengubah tampilan jumlah input benar pada LCD
  lcdOutput("Input terdeteksi", String(getInput));
  lcd.setCursor(1, 1);
  lcd.print("/2");

  return finger.fingerID;
}

void setup()
{
  // Melakukan inisialisasi seluruh yang dibutuhkan
  Serial.begin(115200);
  mySerial.begin(57600, SERIAL_8N1, 16, 17);
  lcdInit();
  wifiInit();
  firebaseInit();
  fingerprintInit();

  // Mengkonfigurasikan timestamp menggunakan NTP server
  configTime(0, 0, ntpServer);

  inputPassword.reserve(6); // Membatasi inputPassword maksimal 6 karakter

  // Inisialisasi seluruh input dan output
  pinMode(RELAY, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(INSIDE_BUTTON, INPUT_PULLUP);
  pinMode(MAGNETIC_SWITCH, INPUT_PULLUP);

  // Menetapkan kondisi default
  digitalWrite(RELAY, LOW);
  digitalWrite(RED_LED, HIGH);
  digitalWrite(GREEN_LED, LOW);

  // Tampilan LCD bahwa sistem sudah siap
  lcdOutput("Sistem siap", "untuk digunakan");
  // Bunyikan buzzer
  for (int i = 0; i < 2; i++)
  {
    digitalWrite(BUZZER, HIGH);
    delay(100);
    digitalWrite(BUZZER, LOW);
    delay(100);
  }

  // Membaca kondisi output dan masukkan dalam variabel outputState
  if (digitalRead(RELAY) == HIGH)
  {
    outputState = true;
  }
  else
  {
    outputState = false;
  }

  // default tampilan LCD
  lcdOutput("Masukkan input", "untuk membuka");
}

void loop()
{
  if (wifiMulti.run() != WL_CONNECTED)
  {
    Serial.println("WiFi not connected!");
    lcdOutput("Jaringan WiFi", "telah terputus");
    delay(2000);
  }

  if (inputPassword == "")
  {
    // Memeriksa input openLock dari Firebase database
    checkFirebaseInput();
  }

  // Menetapkan pembacaan input dari tombol
  insideButton = digitalRead(INSIDE_BUTTON);

  // Kondisi jika variabel getInput sama dengan 2
  if (getInput == 2)
  {
    Serial.println("2 Input sudah terdeteksi masuk");
    outputProcess("");
  }
  else if (getInput < 2 && insideButton == 0)
  {
    Serial.println("Input tombol ditekan");
    // Jika tombol yang dipakai untuk membuka pintu dari dalam rumah ditekan
    outputProcess("button");
  }

  // Jika password belum digunakan
  if (passwordUsed != true)
  {
    passwordInput();
  }

  // Jika fingerprint belum digunakan dan tidak terdapat inputPassword yang masuk
  if (fingerUsed != true && inputPassword == "")
  {
    fingerprintInput();
    // Delay agar sensor tidak terlalu cepat membaca
    delay(50);
  }
}
