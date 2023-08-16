// Bibliotecas Necessárias
#if defined(ESP32)
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif
#include <Arduino.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include <time.h>
#include <Stepper.h>
#include <cmath>

// Váriaveis do WiFi
#define WIFI_SSID ""
#define WIFI_PASSWORD ""

//Váriavies do Firebase
// Insert Firebase project API Key
#define API_KEY ""

// Insert RTDB URLefine the RTDB URL */
#define DATABASE_URL ""

#define USER_EMAIL ""
#define USER_PASSWORD ""

//Define Firebase Data object
FirebaseData fbdo;

FirebaseAuth auth;
FirebaseConfig config;

FirebaseJson json;
FirebaseJsonData result;

// Definição de váriaveis
bool signupOK = false; 

int timezone = -4;
int dst = 0;

bool motorLigado = false;
bool jaAlimentou = false;

const int steps_per_rev = 200; //Set to 200 for NIMA 17

#define IN1 5
#define IN2 4
#define IN3 14
#define IN4 12

Stepper motor(steps_per_rev, IN1, IN2, IN3, IN4);

// Sensor Ultrassonico
const int trigPin = 13;
const int echoPin = 15;

long t;
float distance;
float percentageFood;

// Definir a altura do recipiente em centímetros
float max_food = 18.00;

// Setup
void setup() {
  // Inicialização do monitor serial
  Serial.begin(115200); 
  
  // Configurar os pinos do sensor ultrassônico como saída e entrada
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  /* Assign the api key (required) */
  config.api_key = API_KEY;

  /* Assign the RTDB URL (required) */
  config.database_url = DATABASE_URL;

  /* Assign the user sign in credentials */
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  /* Sign up */
  if (Firebase.signUp(&config, &auth, USER_EMAIL, USER_PASSWORD)) {
    Serial.println("ok");
    signupOK = true;
  }
  else {
    Serial.printf("%s\n Erro!", config.signer.signupError.message.c_str());
  }

  /* Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

   //Configuração do Timer
    configTime(timezone * 3600, dst * 0, "pool.ntp.org", "time.nist.gov");
    while (!time(nullptr)) {
    delay(1000);
  }

}

void loop() {
  // put your main code here, to run repeatedly:
  if (Firebase.ready()) {
    Serial.println(" - - - - - - - - - - - - - - - ");
    Serial.println(" Conectado ao banco de dados!");
    Firebase.RTDB.getJSON(&fbdo, "/alimentador01");
    json.setJsonData(fbdo.to<FirebaseJson>().raw());
    // Serial.println(fbdo.to<FirebaseJson>().raw());

    
    json.get(result, "AlimentarAgora");
    String almag =  result.to<String>();
    json.get(result, "AlimentarAgoraQtde");
    int almqtde = result.to<int>();
    
    
    
    if(almag == "true"){
      Serial.print(" Alimentando agora: " );
      Serial.println(almqtde);
      DiaMesHora();
      motoralimentando(10);
      Firebase.RTDB.setString(&fbdo, "/alimentador01/AlimentarAgora", "false"); // Envia informação para o Firebase // set envia
      delay(5000);
    }
     
    Serial.println();
    
    json.get(result, "qtde");
    int qtde = result.to<int>();
    String horaAtual = Hora();
    String msg = " Alimentação Automática: "+result.to<String>();
    if(qtde > 0 && horaAtual == "12:00"){
        if(!jaAlimentou){
           motoralimentando(qtde);
           jaAlimentou = true;
        }
    }else{
        jaAlimentou = false;
      }
   
    Serial.println(msg);
    Serial.println();
    
    delay(1000);

    calcRemainingFood();
    
  }
}

// Função responsável por controlar o motor de passo e alimentar
void motoralimentando(int qtde){
  String msg = " Alimentar por: "+ (String)qtde;
  Serial.println(msg);

  Serial.println(" Rotating Clockwise...");

  unsigned long m = millis();
  while (millis() - m < (qtde * 1000)) {
    motor.setSpeed(260); // Velocidade do Motor 
    motor.step(steps_per_rev); 
    yield();
  }
}


String Hora() {
  time_t now; // Hora atual
  struct tm * timeinfo;
  time(&now);
  timeinfo = localtime(&now);
  String timeAgora = "";

  // Hora e minuto
  int hora = timeinfo->tm_hour;
  Serial.println(hora);
  if (hora < 10) {
    timeAgora += "0" + (String)hora;
  } else {
    timeAgora += (String)hora;
  }
  timeAgora += ":";
  int minuto = timeinfo->tm_min;
  if (minuto < 10) {
    timeAgora += "0" + (String)minuto;
  } else {
    timeAgora += (String)minuto;
  }

  return timeAgora;
}

String DiaMesHora() {
  time_t now; // Hora atual
  struct tm * timeinfo;
  time(&now);
  timeinfo = localtime(&now);
  String msg = "";

  //Pegar o dia do Mês
  int dia = timeinfo->tm_mday;
  if (dia < 10) {
    msg += "0" + (String)dia;
  } else {
    msg += (String)dia;
  }
  Firebase.RTDB.setString(&fbdo, "/alimentador01/UltimaAlimentacao/dia", msg); // Envia informação para o Firebase // set envia
  msg = "";
 

  
  //Pegar o Mês Atual
  int mes = timeinfo->tm_mon + 1;
  if (mes < 10) {
    msg += "0" + (String)mes;
  } else {
    msg += (String)mes;
  }
  Firebase.RTDB.setString(&fbdo, "/alimentador01/UltimaAlimentacao/mes", msg); // Envia informação para o Firebase // set envia
  msg = "";
 

  // Hora e minuto
  int hora = timeinfo->tm_hour;
  if (hora < 10) {
    msg += "0" + (String)hora;
  } else {
    msg += (String)hora;
  }
  Firebase.RTDB.setString(&fbdo, "/alimentador01/UltimaAlimentacao/hora", msg); // Envia informação para o Firebase // set envia
  msg = "";
  
  int minuto = timeinfo->tm_min;
  if (minuto < 10) {
    msg += "0" + (String)minuto;
  } else {
    msg += (String)minuto;
  }
  Firebase.RTDB.setString(&fbdo, "/alimentador01/UltimaAlimentacao/minuto", msg); // Envia informação para o Firebase // set envia
  return msg;
}

// Nivel da ração
void rationLevel(float percentageFood){
  float roundedValue = roundf(percentageFood * 100) / 100;
  Firebase.RTDB.setFloat(&fbdo, "/alimentador01/NivelRacao", roundedValue); // Envia informação para o Firebase // set envia
}


// Monitorar o Nivel da Ração
void calcRemainingFood() {
  
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  t = (pulseIn(echoPin, HIGH) / 2);
  if (t == 0.00) {
    Serial.println("Failed to read from SR02");
    delay(1000);
    return;
  }
  distance = float(t * 0.0343);
  //Serial.println(distance);
  //Serial.println(t);
  percentageFood = (100 - ((100 / max_food) * distance));
  if (percentageFood < 0.00) {
    percentageFood = 0.00;
  }
  Serial.print("Remaining food:\t");
  Serial.print(percentageFood);
  Serial.println(" %");
  delay(5000);

  rationLevel(percentageFood);
}
