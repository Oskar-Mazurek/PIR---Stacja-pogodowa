#include <M5Core2.h>  //podstawowa biblioteka do obsługi modułu M5Stack Core2
#include <WiFi.h>     //biblioteka umożliwiająca zastosowanie sieci WiFi
#include "M5_ENV.h"   //biblioteka umożliwająca odczyty z czujników
#include "FastLED.h"  //bibloteka umożliwająca wykorzystanie diod LED
#include <string.h>   //bibloteka dająca dostęp do typu String
#include <Arduino.h>  //bibloteka dająca dostęp do funkcjonalności modułu ESP32, w tym projekcie przydatna do skorzystania z karty SD

//Stałe
#define LEDS_PIN 25      //pin GPIO dla diod
#define LEDS_NUM 10      //ilość diod
#define TIMEDELTA 60000  //stałe czasowe do okresowego wywoływania funkcji
#define TIMEDELTA2 180000
#define TIMEDELTA3 259200000
#define TIMEDELTAWIFI 10000

//Pomocnicze zmienne
WiFiClient espClient;
CRGB ledsBuff[LEDS_NUM];
RTC_TimeTypeDef RTCtime;
RTC_DateTypeDef RTCDate;
SHT3X sht30;
QMP6988 qmp6988;

const char *ssid = "Brutek";  //dane do pierwszego połączenia z siecią WiFi
const char *password = "00000000";
const char *ssid2 = "PLAY Internet 4G LTE-195671";  //dane do drugiego połączenia z siecią WiFi
const char *password2 = "30195671";
const char *ntpServer = "tempus1.gum.gov.pl";  //adres serwera daty
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 3600;
float batVoltage = 0.0;  //zmienne do odczytu stanu baterii urządzenia
float batPercentage = 0.0;
char timeStrbuff[64];  //tablice do przechowywania daty i godziny
char timeStrbuff2[64];
int8_t currPage = 0, currPage2 = 0;  //zmienne sterujące ekranami
char dispRefresh = 1;                //zmienna do odświeżania ekranu
float temperature = 0.0;             //zmienne do przechowywania aktualnych odczytów
float humidity = 0.0;
float pressure = 0.0;
unsigned long now;  //zmienne do sterowania okresowością wywoływania funkcji
unsigned long last = 0;
unsigned long last2 = 0, last3 = 0;
unsigned long lastW = 0, nowW;
int ID;                                                                                          //zmienna przechowująca aktualny numer odczytu z czujników
float t1 = 0.0, t2 = 0.0, h1 = 0.0, h2 = 0.0, p1 = 0.0, p2 = 0.0, t3 = 0.0, h3 = 0.0, p3 = 0.0;  // zmienne do obliczania trendu zmiany odczytów
float maxT = 0.0, maxH = 0.0, maxP = 0.0, minT = 100, minH = 100, minP = 1000000;                //zmienne do przechowywania minimalnych i maksymalnych odczytów
char *path = "/readings.csv";                                                                    //ścieżka do pliku z odczytami zapisanymi na karcie SD
char *path2 = "/ID.txt";                                                                         //ścieżka do pliku z zapisanym numerem odczytu na karcie SD
char IDm[5];                                                                                     //tablica znakowa do przygotowania zmiennej ID do zapisu w pliku

float temperatures[320], humidities[320], pressures[320], hours[320], minutes[320];  //tablice pomocnicze przechowujące zawartość odcztów, służą do rysowania wykresów

//Prototypy funkcji
void setupWifi();                                                    // funkcja umożliwiająca ustanowienie połączenia z siecią WiFi
void displayBatPercentage();                                         //funkcja pokazująca aktualny procent naładowania akumulatora
void lightLeds(int R, int G, int B);                                 //funkcja zapaliająca ledy i ustawiająca ich kolor
void setTime();                                                      //pobranie aktualnego czasu z NTP i jego ustawienie
void displayTime();                                                  //funkcja pokazująca aktualny czas
void page1();                                                        //funkcja realizująca pierwszą stronę wizualizacji
void sensing();                                                      //funkcja odczytująca wartości z czujników
void page2();                                                        //funkcja realizująca drugą stronę wizualizacji
void page3();                                                        //funkcja realizująca trzecią stronę wizualizacji
void page4();                                                        //funkcja realizująca czwartą stronę wizualizacji
void page5();                                                        //funkcja realizująca piątą stronę wizualizacji
void page6();                                                        //funkcja realizująca szóstą stronę wizualizacji
void displayPageNum();                                               //funkcja wyświetlająca aktualną stronę wizualizacji
void drawAxes();                                                     //funkcja rysująca osie wykresów
void biggerT();                                                      //funkcja odpowadająca za wyświetlenia odczytu temperatury z użyciem większej czcionki
void biggerH();                                                      //funkcja odpowadająca za wyświetlenia odczytu wilgotności z użyciem większej czcionki
void biggerP();                                                      //funkcja odpowadająca za wyświetlenia odczytu ciśnienia z użyciem większej czcionki
void writeFile(fs::FS &fs, const char *path, const char *message);   //funkcja do zapisu pliku na karcie SD
void appendFile(fs::FS &fs, const char *path, const char *message);  //funkcja do dopisania nowych odczytów do pliku na karcie SD
void readFile(fs::FS &fs, const char *path);                         //funkcja do odczytu danych z pliku na karcie SD
void getID();                                                        //funkcja do odczytu aktualnego numeru odczytu z czujników
void setTime();                                                      //funkcja realizująca wizualizację umożliwiającą ręczne ustawienie daty i godziny
void setupTime();                                                    //funkcja ustawiająca datę i godzinę przy włączeniu urządzenia
void setTimeInternet();                                              //funkcja ustawiająca datę i godzinę pobraną z Internetu

HotZone d1(10, 10, 40, 40);  //wirtualne przyciski dotykowe do ręcznej zmiany daty i godziny
HotZone m1(10, 60, 40, 90);
HotZone y11(10, 120, 40, 150);
HotZone d2(110, 10, 140, 40);
HotZone m2(110, 60, 140, 90);
HotZone y2(110, 120, 140, 150);
HotZone h11(10, 170, 40, 200);
HotZone mm1(10, 200, 40, 230);
HotZone h22(110, 170, 140, 200);
HotZone mm2(110, 200, 140, 230);
HotZone TI(180, 120, 320, 150);
HotZone btnD(250, 150, 280, 180);

void setup() {
  // put your setup code here, to run once:
  M5.begin();
  M5.Lcd.setBrightness(255);
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextSize(3);
  M5.Lcd.setTextColor(OLIVE, BLACK);
  M5.Lcd.setCursor(10, 10);
  Serial.begin(115200);                                   //ustalenie prędkości transmisji UART
  FastLED.addLeds<SK6812, LEDS_PIN>(ledsBuff, LEDS_NUM);  //inicjacja diod led
  lightLeds(0, 0, 0);
  Wire.begin();    //inicjacja I2C
  qmp6988.init();  //inicjacja czujnika ciśnienia
  setupTime();
  if (!SD.begin(5)) {  //podłączenie karty pamięci i wypis danych na jej temat
    M5.Lcd.println("Card Mount Failed");
  }
  uint8_t cardType = SD.cardType();

  if (cardType == CARD_NONE) {
    M5.Lcd.println("No SD card attached");
  }

  M5.Lcd.print("SD Card Type: ");
  if (cardType == CARD_MMC) {
    M5.Lcd.println("MMC");
  } else if (cardType == CARD_SD) {
    M5.Lcd.println("SDSC");
  } else if (cardType == CARD_SDHC) {
    M5.Lcd.println("SDHC");
  } else {
    M5.Lcd.println("UNKNOWN");
  }

  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  M5.Lcd.printf("SD Card Size: %lluMB\n", cardSize);
  delay(1000);
  M5.Lcd.clear();
  sensing();  //pierwszy pomiar po włączeniu urządzenia
}

void loop() {
  // put your main code here, to run repeatedly:
  M5.update();
  now = millis();
  if (now - last > TIMEDELTA) {  //okresowo wykonywane pomiary temperatury, wilgotności i ciśnienia
    last = now;
    sensing();
  }
  if (dispRefresh) {  //odświerzenie zawartości ekranu po zmianie strony
    M5.Lcd.fillScreen(BLACK);
    dispRefresh = 0;
  }
  switch (currPage) {  //instrukcja realizująca możliwość przełącznia ekranów
    case 0:            //strona 1
      M5.Lcd.setTextSize(3);
      displayBatPercentage();
      displayPageNum();
      if (currPage2 == 0) page1();
      else if (currPage2 == 1) biggerT();
      else if (currPage2 == 2) biggerH();
      else if (currPage2 == 3) biggerP();
      else if (currPage2 == 4) setTime();
      break;
    case 1:  //strona 2
      M5.Lcd.setTextSize(2);
      displayBatPercentage();
      displayPageNum();
      page2();
      break;
    case 2:  //strona 3
      M5.Lcd.setTextSize(2);
      displayBatPercentage();
      displayPageNum();
      page3();
      break;
    case 3:  //strona 4
      M5.Lcd.setTextSize(2);
      displayBatPercentage();
      displayPageNum();
      page4();
      break;
    case 4:  //strona 5
      M5.Lcd.setTextSize(2);
      displayBatPercentage();
      displayPageNum();
      page5();
      break;
    case 5:  //strona 6
      M5.Lcd.setTextSize(2);
      displayBatPercentage();
      displayPageNum();
      page6();
      break;
  }
  //zmiana strony
  if (M5.BtnC.wasReleased()) {  //w prawo
    if (currPage >= 0 && currPage < 6) {
      currPage++;
      dispRefresh = 1;
    }
  } else if (M5.BtnA.wasReleased()) {  //w lewo
    if (currPage >= 0 && currPage < 6) {
      currPage--;
      dispRefresh = 1;
    }
  } else if (M5.BtnB.wasReleased()) {  //w górę
    if (currPage2 >= 0 && currPage < 5) {
      currPage2++;
      dispRefresh = 1;
    }
  }
  if (currPage <= 0) currPage = 0;  //zabezpieczenie przed wyjściem poza zakres stron
  if (currPage >= 5) currPage = 5;
  if (currPage2 > 4) currPage2 = 0;
  //miganie diod na niebiesko gdy za zimno
  if (temperature > 30.0 && now - last2 > TIMEDELTA2) {
    last2 = now;
    lightLeds(100, 0, 0);
    lightLeds(0, 0, 0);
  }
  //miganie diod na czerwono gdy za ciepło
  if (temperature < 0.0 && now - last2 > TIMEDELTA2) {
    last2 = now;
    lightLeds(0, 0, 100);
    lightLeds(0, 0, 0);
  }
  //miganie diod na zielono gdy w sam raz
  if (temperature >= 20 && temperature < 25 && now - last2 > TIMEDELTA2) {
    last2 = now;
    lightLeds(0, 100, 0);
    lightLeds(0, 0, 0);
  }
}

//Funkcje
void setupWifi() {  //ustanowienie połączenia z siecią wifi
  M5.Lcd.setCursor(10, 10);
  M5.Lcd.printf("Connecting to %s", ssid);
  WiFi.mode(WIFI_STA);
  nowW = millis();
  lastW = nowW;
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {  //próba połączenia z pierwszą siecią, jeśli się nie uda po 15s jest ona zaprzestana
    lightLeds(255, 195, 0);
    lightLeds(0, 0, 0);
    M5.Lcd.print(".");
    nowW = millis();
    if (nowW - lastW > TIMEDELTAWIFI) break;
  }

  if ((WiFi.status() == WL_CONNECTED)) {  //Jeśli się uda to wyświelenie komunikatu o sukcesie, odświerzenie ekranu i koniec funkcji
    M5.Lcd.printf("\nSuccess\n");
    lightLeds(0, 100, 0);
    delay(200);
    lightLeds(0, 0, 0);
    dispRefresh = 1;
    return;
  }
  lastW = nowW;

  M5.Lcd.setCursor(10, 25);
  M5.Lcd.printf("Connecting to %s", ssid2);
  WiFi.begin(ssid2, password2);
  while (WiFi.status() != WL_CONNECTED) {  //próba połączenia z drugą siecią, jeśli się nie uda po 15s jest ona zaprzestana
    lightLeds(100, 0, 100);
    lightLeds(0, 0, 0);
    M5.Lcd.print(".");
    nowW = millis();
    if (nowW - lastW > TIMEDELTAWIFI) break;
  }

  if ((WiFi.status() == WL_CONNECTED)) {  //Jeśli się uda to wyświelenie komunikatu o sukcesie, odświerzenie ekranu i koniec funkcji
    M5.Lcd.printf("\nSuccess\n");
    lightLeds(0, 100, 0);
    delay(200);
    lightLeds(0, 0, 0);
    dispRefresh = 1;
    return;
  }
}

void displayBatPercentage() {  //wyświetalnie poziomu naładowania baterii
  batVoltage = M5.Axp.GetBatVoltage();
  batPercentage = (batVoltage < 3.2) ? 0 : (batVoltage - 3.2) * 100;
  M5.Lcd.setTextColor(CYAN, BLACK);
  M5.Lcd.setCursor(270, 10);
  M5.Lcd.setTextSize(2);
  M5.Lcd.printf("%d%%\n", (int)batPercentage);
}

void lightLeds(int R, int G, int B) {  //zapalanie diod na określony kolor
  for (int i = 0; i < LEDS_NUM; i++) {
    ledsBuff[i].setRGB(G, R, B);
  }
  FastLED.show();
}

void setTimeInternet() {  //pobranie czasu z internetu
  struct tm timeInfo;
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  if (getLocalTime(&timeInfo)) {
    RTCtime.Hours = timeInfo.tm_hour;
    RTCtime.Minutes = timeInfo.tm_min;
    RTCtime.Seconds = timeInfo.tm_sec;
    if (!M5.Rtc.SetTime(&RTCtime)) M5.Lcd.println("Wrong time set!");
    else {
      M5.Lcd.println("Time set success!");
      lightLeds(0, 100, 0);
      delay(200);
      lightLeds(0, 0, 0);
    }
    RTCDate.Year = timeInfo.tm_year + 1900;
    RTCDate.Month = timeInfo.tm_mon + 1;
    RTCDate.Date = timeInfo.tm_mday;
    if (!M5.Rtc.SetDate(&RTCDate)) M5.Lcd.println("Wrong date set!");
    else {
      M5.Lcd.println("Date set success!");
      lightLeds(0, 100, 0);
      delay(200);
      lightLeds(0, 0, 0);
    }
  }
}

void displayTime() {  //wyświetlenie aktualnego czasu
  M5.Lcd.setTextSize(3);
  M5.Lcd.setCursor(10, 10);
  M5.Rtc.GetTime(&RTCtime);
  M5.Rtc.GetDate(&RTCDate);
  sprintf(timeStrbuff, "%2d:%02d:%02d\n%2d.%02d.%04d", RTCtime.Hours,
          RTCtime.Minutes, RTCtime.Seconds, RTCDate.Date, RTCDate.Month,
          RTCDate.Year);
  M5.Lcd.setTextColor(YELLOW, BLACK);
  M5.Lcd.println(timeStrbuff);
}

void sensing() {  //pobieranie wartości odczytów z czujników
  lightLeds(36, 100, 250);
  pressure = qmp6988.calcPressure();
  if (sht30.get() == 0) {
    temperature = sht30.cTemp;
    humidity = sht30.humidity;
  } else {
    temperature = 0;
    humidity = 0;
  }
  t3 = t2;
  t2 = t1;
  t1 = temperature;  //zapis temperatur do wyznaczenia trendu ich zmiany
  p3 = p2;
  p2 = p1;
  p1 = pressure;
  h3 = h2;
  h2 = h1;
  h1 = humidity;
  if (now - last3 < TIMEDELTA3) {  //szukanie watości maksymanych i minimalnych w ciągu 3 dni
    if (maxT <= temperature) maxT = temperature;
    if (minT >= temperature) minT = temperature;
    if (maxP <= pressure) maxP = pressure;
    if (minP >= pressure) minP = pressure;
    if (maxH <= humidity) maxH = humidity;
    if (minH >= humidity) minH = humidity;
  } else if (now - last3 > TIMEDELTA3) {
    last3 = now;
    maxT = 0.0;
    maxH = 0.0;
    maxP = 0.0;
    minT = 100;
    minH = 100;
    minP = 100000;
  }
  getID();  //pobór aktuanego ID odczytu
  ID++;     // zwiększenie ID o 1
  char message[64];
  sprintf(timeStrbuff2, "%02d:%02d:%02d %2d.%02d.%04d", RTCtime.Hours,
          RTCtime.Minutes, RTCtime.Seconds, RTCDate.Date, RTCDate.Month,
          RTCDate.Year);  //przechwycenie aktuanego czasu
  if (ID > TIMEDELTA3) {
    ID = 1;
    writeFile(SD, path, "ID;Date;temperature;humidity;pressure\n");
  }
  sprintf(message, "%d;%s;%f;%f;%f\n", ID, timeStrbuff2, temperature, humidity, pressure);  //przygotowanie nowej lini do pliku csv zawierającej dane o odczytach
  appendFile(SD, path, message);                                                            //dopisanie jej
  readFile(SD, path);                                                                       //odcztyt danych o odczytach z czujników
  sprintf(IDm, "%d", ID);                                                                   //przygotowanie ID odczytu do zapisu
  writeFile(SD, path2, IDm);                                                                //zapis nowego ID do pliku
  lightLeds(0, 0, 0);
}

void page1() {  //wyświetlenie aktuanych odczytów na pierwszej stronie
  M5.Lcd.setTextColor(YELLOW, BLACK);
  displayTime();
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.setTextSize(3);
  M5.Lcd.setCursor(0, 80);
  M5.Lcd.println("Current readings:");
  M5.Lcd.setCursor(0, 110);
  M5.Lcd.setTextColor(YELLOW, BLACK);
  M5.Lcd.printf("Temperature:%.1f%c\n", temperature, 'C');
  M5.Lcd.setCursor(0, 145);
  M5.Lcd.printf("Humidity:%.1f%%\n", humidity);
  M5.Lcd.setCursor(0, 185);
  M5.Lcd.printf("Pressure:%.1fhPa\n", pressure / 100);
}

void page2() {  //wyświetlenie trendu zmian odczytów na drugiej stronie
  M5.Lcd.setCursor(1, 30);
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.setTextSize(3);
  M5.Lcd.print("Trends of changes");
  M5.Lcd.setCursor(40, 70);
  M5.Lcd.setTextColor(0xAFE5, BLACK);
  M5.Lcd.setTextSize(3);
  M5.Lcd.printf("T= %.1f C", t1);
  if (t1 > t2 && t2 > t3) {
    M5.Lcd.setTextColor(GREEN, BLACK);
    M5.Lcd.println(" >>");
  } else if (t1 == t2 && t2 == t3) {
    M5.Lcd.setTextColor(WHITE, BLACK);
    M5.Lcd.println(" --");
  } else if (t1 < t2 && t2 < t3) {
    M5.Lcd.setTextColor(RED, BLACK);
    M5.Lcd.println(" <<");
  } else {
    M5.Lcd.setTextColor(WHITE, BLACK);
    M5.Lcd.println(" --");
  }
  M5.Lcd.setCursor(40, 100);
  M5.Lcd.setTextColor(YELLOW, BLACK);
  M5.Lcd.printf("H= %.1f%%", h1);
  if (h1 > h2 && h2 > h3) {
    M5.Lcd.setTextColor(GREEN, BLACK);
    M5.Lcd.println(" >>");
  } else if (h1 == h2 && h2 == h3) {
    M5.Lcd.setTextColor(WHITE, BLACK);
    M5.Lcd.println(" --");
  } else if (h1 < h2 && h2 < h3) {
    M5.Lcd.setTextColor(RED, BLACK);
    M5.Lcd.println(" <<");
  } else {
    M5.Lcd.setTextColor(WHITE, BLACK);
    M5.Lcd.println(" --");
  }
  M5.Lcd.setCursor(40, 130);
  M5.Lcd.setTextColor(0xFD20, BLACK);
  M5.Lcd.printf("P= %.1fhPa", p1 / 100);
  if (p1 > p2 && p2 > p3) {
    M5.Lcd.setTextColor(GREEN, BLACK);
    M5.Lcd.println(" >>");
  } else if (p1 == p2 && p2 == p3) {
    M5.Lcd.setTextColor(WHITE, BLACK);
    M5.Lcd.println(" --");
  } else if (p1 < p2 && p2 < p3) {
    M5.Lcd.setTextColor(RED, BLACK);
    M5.Lcd.println(" <<");
  } else {
    M5.Lcd.setTextColor(WHITE, BLACK);
    M5.Lcd.println(" --");
  }
}

void page3() {  //wyświetlenie tabeli z minimalnymi i maksymalnymi odczytami na trzeciej stronie
  M5.Lcd.setTextSize(2);
  M5.Lcd.drawLine(0, 80, 320, 80, WHITE);
  M5.Lcd.drawLine(136, 0, 136, 240, WHITE);
  M5.Lcd.drawLine(232, 0, 232, 240, WHITE);
  M5.Lcd.setCursor(0, 40);
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.print("Value");
  M5.Lcd.setCursor(0, 100);
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.print("Temperature");
  M5.Lcd.setCursor(0, 140);
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.print("Pressure");
  M5.Lcd.setCursor(0, 180);
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.print("Humidity");
  M5.Lcd.setCursor(140, 25);
  M5.Lcd.setTextColor(RED, BLACK);
  M5.Lcd.print("Max in");
  M5.Lcd.setCursor(140, 40);
  M5.Lcd.print("the 3");
  M5.Lcd.setCursor(140, 60);
  M5.Lcd.print("days");
  M5.Lcd.setCursor(140, 100);
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.printf("%.1f C", maxT);
  M5.Lcd.setCursor(140, 140);
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.printf("%.1f", maxP / 100);
  M5.Lcd.setCursor(140, 160);
  M5.Lcd.print("hPa");
  M5.Lcd.setCursor(140, 180);
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.printf("%.1f %%", maxH);
  M5.Lcd.setCursor(235, 25);
  M5.Lcd.setTextColor(BLUE, BLACK);
  M5.Lcd.print("Min in");
  M5.Lcd.setCursor(235, 40);
  M5.Lcd.print("the 3");
  M5.Lcd.setCursor(235, 60);
  M5.Lcd.print("days");
  M5.Lcd.setCursor(235, 100);
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.printf("%.1f C", minT);
  M5.Lcd.setCursor(235, 140);
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.printf("%.1f", minP / 100);
  M5.Lcd.setCursor(235, 160);
  M5.Lcd.print("hPa");
  M5.Lcd.setCursor(235, 180);
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.printf("%.1f %%", minH);
}

void drawAxes() {
  //osie wykresów
  M5.Lcd.drawLine(10, 220, 310, 220, CYAN);
  M5.Lcd.drawLine(10, 5, 10, 220, CYAN);
  M5.Lcd.drawLine(305, 215, 310, 220, CYAN);
  M5.Lcd.drawLine(305, 225, 310, 220, CYAN);
  M5.Lcd.drawLine(10, 5, 10, 10, CYAN);
  M5.Lcd.drawLine(15, 10, 10, 10, CYAN);
}

void page4() {  //wyświetlenie wykresu zmian temperatury na czwartej stronie
  //osie wykresów
  M5.Lcd.drawLine(10, 110, 310, 110, CYAN);
  M5.Lcd.drawLine(10, 5, 10, 220, CYAN);
  M5.Lcd.drawLine(305, 105, 310, 110, CYAN);
  M5.Lcd.drawLine(305, 115, 310, 110, CYAN);
  M5.Lcd.drawLine(10, 5, 10, 10, CYAN);
  M5.Lcd.drawLine(15, 10, 10, 10, CYAN);
  M5.Lcd.setCursor(50, 10);
  M5.Lcd.print("Temperature chart");
  float t;
  int x = 0;
  for (int i = 0; i < 320; i++) {
    t = 110.0 - temperatures[i];
    M5.Lcd.drawPixel(i + 11, t, YELLOW);
    if (i % 5 == 0 && i < 300) {
      M5.Lcd.drawLine(i + 11, 112, i + 11, 108, WHITE);
      M5.Lcd.setCursor(i + 11, 120);
      M5.Lcd.setTextSize(1);
      if ((x + 5) % 10 == 0) M5.Lcd.printf("%.0f:%.0f", hours[i], minutes[i]);
      x++;
    }
    if (i % 5 == 0 && i < 230 && i > 10) {
      M5.Lcd.drawLine(8, i, 12, i, WHITE);
    }
  }
  for (int t = 0; t < 200; t++) {
    M5.Lcd.setCursor(20, 205 - t);
    M5.Lcd.setTextSize(1);
    if (t % 30 == 0) M5.Lcd.printf("%d", t - 100);
  }
}
void page5() {  //wyświetlenie wykresu zmian wilgotności na piątej stronie
  //osie wykresów
  drawAxes();
  int x = 0;
  float y = 0.0;
  M5.Lcd.setCursor(50, 10);
  M5.Lcd.print("Humidity chart");
  for (int i = 0; i < 320; i++) {
    M5.Lcd.drawPixel(i + 11, 215 - ((215 * humidities[i]) / 100), RED);
    if (i % 5 == 0 && i < 300) {
      M5.Lcd.drawLine(i + 11, 222, i + 11, 218, WHITE);
      M5.Lcd.setCursor(i + 11, 200);
      M5.Lcd.setTextSize(1);
      if ((x + 5) % 10 == 0) M5.Lcd.printf("%.0f:%.0f", hours[i], minutes[i]);
      x++;
    }
    if (i % 5 == 0 && i < 230 && i > 10) {
      M5.Lcd.drawLine(8, i, 12, i, WHITE);
    }
  }
  for (int t = 0; t < 200; t++) {
    M5.Lcd.setCursor(20, 205 - t);
    M5.Lcd.setTextSize(1);
    if (t % 30 == 0) M5.Lcd.printf("%d", t / 2);
  }
}
void page6() {  //wyświetlenie wykresu zmian ciśnienia powietrza na szóstej stronie
  //osie wykresów
  drawAxes();
  int x = 0, y = 0;
  M5.Lcd.setCursor(50, 10);
  float hPa, p;
  M5.Lcd.print("Pressure chart");
  for (int i = 0; i < 320; i++) {
    hPa = pressures[i] / (float)100;
    p = hPa / (float)10;
    M5.Lcd.drawPixel(i + 11, 215 - p, BLUE);
    if (i % 5 == 0 && i < 300) {
      M5.Lcd.drawLine(i + 11, 222, i + 11, 218, WHITE);
      M5.Lcd.setCursor(i + 11, 200);
      M5.Lcd.setTextSize(1);
      if ((x + 5) % 10 == 0) M5.Lcd.printf("%.0f:%.0f", hours[i], minutes[i]);
      x++;
    }
    if (i % 5 == 0 && i < 230 && i > 10) {
      M5.Lcd.drawLine(8, i, 12, i, WHITE);
      M5.Lcd.setCursor(20, 230 - i);
      M5.Lcd.setTextSize(1);
      if (x % 5 == 0) M5.Lcd.printf("%d", y);
    }
    y += 9;
  }
}

void displayPageNum() {  //wyświetlanie numeru aktualnie wyświetlanej strony
  M5.Lcd.setCursor(270, 220);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.printf("%d/6", currPage + 1);
}

void biggerT() {  //wyświetlenie aktualnej temperatury większą czcionką
  M5.Lcd.setTextSize(4);
  M5.Lcd.setCursor(30, 50);
  M5.Lcd.setTextColor(YELLOW, BLACK);
  M5.Lcd.printf("Temperature:");
  M5.Lcd.setCursor(50, 100);
  M5.Lcd.printf("%.1f %c\n", temperature, 'C');
}

void biggerH() {  //wyświetlenie aktualnej wilgotności większą czcionką
  M5.Lcd.setTextSize(4);
  M5.Lcd.setCursor(50, 50);
  M5.Lcd.setTextColor(YELLOW, BLACK);
  M5.Lcd.printf("Humidity:");
  M5.Lcd.setCursor(50, 100);
  M5.Lcd.printf("%.1f %%\n", humidity);
}

void biggerP() {  //wyświetlenie aktualnego ciśnienia większą czcionką
  M5.Lcd.setTextSize(4);
  M5.Lcd.setCursor(50, 50);
  M5.Lcd.setTextColor(YELLOW, BLACK);
  M5.Lcd.printf("Pressure:");
  M5.Lcd.setCursor(50, 100);
  M5.Lcd.printf("%.1f hPa\n", pressure / 100);
}

void writeFile(fs::FS &fs, const char *path, const char *message) {  //zapis do pliku
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  if (file.print(message)) {
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
  file.close();
}

void readFile(fs::FS &fs, const char *path) {  //odczyt z pliku
  File file = fs.open(path);
  if (!file) {
    Serial.println("Failed to open file for reading");
    return;
  }
  int i = 0;
  char del = ';';
  char temp;
  String strs[20];
  Serial.print("Read from file: ");
  while (file.available()) {
    String str = file.readStringUntil('\n');  //podzielenie pliku po liniach
    int stringCount = 0;
    while (str.length() > 0) {
      int index = str.indexOf(del);  //podział lini pośrednikach
      if (index == -1) {
        strs[stringCount++] = str;
        break;
      } else {
        strs[stringCount++] = str.substring(0, index);
        str = str.substring(index + 1);
      }
    }
    for (int x = 0; x < stringCount; x++) {  //odczyt wartości z pliku po polach oddzielonych wcześniej średnikami
      if (x == 1) {
        hours[i] = atof((strs[x].substring(0, 2)).c_str());
        minutes[i] = atof((strs[x].substring(3, 5)).c_str());
      }
      if (x == 2) temperatures[i] = atof(strs[x].c_str());
      if (x == 3) humidities[i] = atof(strs[x].c_str());
      if (x == 4) pressures[i] = atof(strs[x].c_str());
      if (temperatures[i] > maxT) maxT = temperatures[i];  //wyznaczanie watrości minimalnych i maksymalnych odczytów
      if (humidities[i] > maxH) maxH = humidities[i];
      if (pressures[i] > maxP) maxP = pressures[i];
      if (temperatures[i] < minT || minT == 0.0) minT = temperatures[i];
      if (humidities[i] < minH || minH == 0.0) minH = humidities[i];
      if (pressures[i] < minP || (minP >= 0.0 && minP <= 800.0)) minP = pressures[i];
    }
    i++;
    if (i > 320) i = 0;  //zabezpieczenie przed przepełnieniem tablic
  }
  file.close();
}

void appendFile(fs::FS &fs, const char *path, const char *message) {  //dopisanie nowych wartości do pliku
  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if (!file) {
    Serial.println("Failed to open file for appending");
    return;
  }
  if (file.print(message)) {
    Serial.println("Message appended");
  } else {
    Serial.println("Append failed");
  }
  file.close();
}

void getID() {  //pobranie aktuanego ID pomiarów z pliku
  File fileR = SD.open(path2);
  if (!fileR) {
    Serial.println("Failed to open file for reading");
  }
  String r = fileR.readStringUntil('\n');
  int i = 0;
  int t = atoi(r.c_str());
  ID = t;
  fileR.close();
  Serial.printf("ID: %d\n", ID);
}

void setTime() {  //strona umożliwiająca ręczne dostosowanie i zapisanie czasu, dotykowymi przyciskami +/-
  M5.Lcd.setTextSize(2);
  M5.Lcd.drawRect(d1.x, d1.y, d1.w, d1.h, ORANGE);
  M5.Lcd.setCursor(d1.x + 5, d1.y + 5);
  M5.Lcd.print("+");
  M5.Lcd.setCursor(d1.x + 45, d1.y + 5);
  M5.Lcd.printf("%d", RTCDate.Date);
  M5.Lcd.drawRect(m1.x, m1.y, m1.w, m1.h, ORANGE);
  M5.Lcd.setCursor(m1.x + 5, m1.y + 5);
  M5.Lcd.print("+");
  M5.Lcd.setCursor(m1.x + 45, m1.y + 5);
  M5.Lcd.printf("%d", RTCDate.Month);
  M5.Lcd.drawRect(y11.x, y11.y, y11.w, y11.h, ORANGE);
  M5.Lcd.setCursor(y11.x + 5, y11.y + 5);
  M5.Lcd.print("+");
  M5.Lcd.setCursor(y11.x + 45, y11.y + 5);
  M5.Lcd.printf("%d", RTCDate.Year);
  M5.Lcd.drawRect(d2.x, d2.y, d2.w, d2.h, ORANGE);
  M5.Lcd.setCursor(d2.x + 5, d2.y + 5);
  M5.Lcd.print("-");
  M5.Lcd.drawRect(m2.x, m2.y, m2.w, m2.h, ORANGE);
  M5.Lcd.setCursor(m2.x + 5, m2.y + 5);
  M5.Lcd.print("-");
  M5.Lcd.drawRect(y2.x, y2.y, y2.w, y2.h, ORANGE);
  M5.Lcd.setCursor(y2.x + 5, y2.y + 5);
  M5.Lcd.print("-");
  M5.Lcd.drawRect(h11.x, h11.y, h11.w, h11.h, ORANGE);
  M5.Lcd.setCursor(h11.x + 5, h11.y + 5);
  M5.Lcd.print("+");
  M5.Lcd.setCursor(h11.x + 45, h11.y + 5);
  M5.Lcd.printf("%d", RTCtime.Hours);
  M5.Lcd.drawRect(mm1.x, mm1.y, mm1.w, mm1.h, ORANGE);
  M5.Lcd.setCursor(mm1.x + 5, mm1.y + 5);
  M5.Lcd.print("+");
  M5.Lcd.setCursor(mm1.x + 45, mm1.y + 5);
  M5.Lcd.printf("%d", RTCtime.Minutes);
  M5.Lcd.drawRect(h22.x, h22.y, h22.w, h22.h, ORANGE);
  M5.Lcd.setCursor(h22.x + 5, h22.y + 5);
  M5.Lcd.print("-");
  M5.Lcd.drawRect(mm2.x, mm2.y, mm2.w, mm2.h, ORANGE);
  M5.Lcd.setCursor(mm2.x + 5, mm2.y + 5);
  M5.Lcd.print("-");
  M5.Lcd.drawRect(TI.x, TI.y, TI.w, TI.h, ORANGE);
  M5.Lcd.drawRect(btnD.x, btnD.y, btnD.w, btnD.h, ORANGE);
  M5.Lcd.setCursor(btnD.x + 5, btnD.y + 5);
  M5.Lcd.print(">");
  M5.Lcd.setCursor(TI.x + 5, TI.y + 5);
  M5.Lcd.setTextSize(1);
  M5.Lcd.print("Set time from Internet");
  TouchPoint_t pos = M5.Touch.getPressPoint();  //obsługa naciśnięć przycisków
  if (d1.inHotZone(pos)) {
    if (RTCDate.Date < 31) RTCDate.Date++;
    else if (RTCDate.Date >= 31) RTCDate.Date = 1;
    M5.Lcd.clear(BLACK);
  }
  if (m1.inHotZone(pos)) {
    if (RTCDate.Month < 12) RTCDate.Month++;
    else if (RTCDate.Month >= 12) RTCDate.Month = 1;
    M5.Lcd.clear(BLACK);
  }
  if (y11.inHotZone(pos)) {
    if (RTCDate.Year < 2200) RTCDate.Year++;
    else if (RTCDate.Year >= 2200) RTCDate.Year = 2000;
    M5.Lcd.clear(BLACK);
  }
  if (d2.inHotZone(pos)) {
    if (RTCDate.Date > 1) RTCDate.Date--;
    else if (RTCDate.Date <= 1) RTCDate.Date = 1;
    M5.Lcd.clear(BLACK);
  }
  if (m2.inHotZone(pos)) {
    if (RTCDate.Month > 1) RTCDate.Month--;
    else if (RTCDate.Month <= 1) RTCDate.Month = 1;
    M5.Lcd.clear(BLACK);
  }
  if (y2.inHotZone(pos)) {
    if (RTCDate.Year > 1900) RTCDate.Year--;
    else if (RTCDate.Year <= 1900) RTCDate.Year = 1900;
    M5.Lcd.clear(BLACK);
  }
  if (h11.inHotZone(pos)) {
    if (RTCtime.Hours < 24) RTCtime.Hours++;
    else if (RTCtime.Hours >= 24) RTCtime.Hours = 1;
    M5.Lcd.clear(BLACK);
  }
  if (mm1.inHotZone(pos)) {
    if (RTCtime.Minutes < 60) RTCtime.Minutes++;
    else if (RTCtime.Minutes >= 60) RTCtime.Minutes = 0;
    M5.Lcd.clear(BLACK);
  }
  if (h22.inHotZone(pos)) {
    if (RTCtime.Hours > 1) RTCtime.Hours--;
    else if (RTCtime.Hours <= 1) RTCtime.Hours = 1;
    M5.Lcd.clear(BLACK);
  }
  if (mm2.inHotZone(pos)) {
    if (RTCtime.Minutes > 0) RTCtime.Minutes--;
    else if (RTCtime.Minutes <= 0) RTCtime.Minutes = 59;
    M5.Lcd.clear(BLACK);
  }
  if (TI.inHotZone(pos)) {
    Serial.printf("%d, %d\r\n", pos.x, pos.y);
    setupWifi();
    setTimeInternet();
  }
  if (btnD.inHotZone(pos)) {
    if (!M5.Rtc.SetTime(&RTCtime)) Serial.println("Wrong time set!");
    if (!M5.Rtc.SetDate(&RTCDate)) Serial.println("Wrong date set!");
    currPage2 = 0;
    dispRefresh = 1;
  }
}

void setupTime() {  //ustawienie czasu po włączeniu urządzenia
  RTCtime.Hours = 16;
  RTCtime.Minutes = 51;
  RTCtime.Seconds = 21;
  if (!M5.Rtc.SetTime(&RTCtime)) Serial.println("Wrong time set!");
  RTCDate.Year = 2023;
  RTCDate.Month = 3;
  RTCDate.Date = 21;
  if (!M5.Rtc.SetDate(&RTCDate)) Serial.println("Wrong date set!");
}
