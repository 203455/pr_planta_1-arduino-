#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include "DHT.h"
#include <virtuabotixRTC.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "hide.h"
#include <ArduinoJson.h>

//Definir ssid y contrseña
const char* ssid = SECRET_SSID;
const char* passwd = SECRET_PSW;

// Definimos el pin digital donde se conecta el sensor
#define DHTPIN 4
// Dependiendo del tipo de sensor
#define DHTTYPE DHT11

//Definiendo puertos para sensor ultrasonico
#define trigPin 5
#define echoPin 18


//Definiendo puerto analógico en el que trabaja el sensor de humedad de suelo. 
int hs = 34 ;

//Definiendo salida digital donde trabaja el motor
int motor = 32;
 
// Inicializamos el sensor DHT11
DHT dht(DHTPIN, DHTTYPE);

//Pines de conexion de reloj CLK, DATA, RST
virtuabotixRTC myRTC(14, 12, 27);

//Crear el objeto lcd  dirección  0x3F y 20 columnas x 4 filas
LiquidCrystal_I2C lcd(0x27,20,4);  //

byte customChar[] = {
  B01110,
  B01010,
  B01110,
  B00000,
  B00000,
  B00000,
  B00000,
  B00000
};

void setup() {
  Serial.begin(115200);
  // inicializar el sensor DHT
  dht.begin();
  // Inicializar el LCD
  lcd.init();
  lcd.backlight();
  lcd.createChar(0, customChar);

  //inicializar pines de ultrasonico
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  //inicializar pin de motor
  pinMode(motor, OUTPUT);

  //conexion a red Wifi
  conexion_wifi();
  
}

void loop() {
  bool riego = false; // Para determinar el riego diario
  bool alerta = false; //Para alertar la insuficiencia de agua

  for(int i=0; i<4;i++){

    
    //Obtención de datos
    //lectura de temperatura y humedad dht11
    float temperatura = dht.readTemperature();
    float humedad = dht.readHumidity();
    //lectura de distancia ultrasonico
    float distancia = obtencion_distancia(); //obtención de distancia
    //Lectura de sensor de humedad
    int humSuelo = analogRead(hs);

    //Acciones de alerta
    alerta = alerta_deposito(distancia);


    //Impresion de datos en pantalla

    //Agregación de tiempo
    lcd.clear();
    myRTC.updateTime();
    lcd.setCursor(0,0); 
    lcd.print(myRTC.hours);
    lcd.print(":");
    lcd.print(myRTC.minutes);

    
    //Impresion de Temperatura y humedad
    lcd.setCursor(0,1);
    lcd.print("Temp:");
    lcd.print(temperatura);
    lcd.write(byte(0));
    lcd.print("C");
    lcd.setCursor(0,2);
    lcd.print("Humd:");
    lcd.print(humedad);
    //Impresion de distancia (No necesaria)
    //    lcd.setCursor(0,2);
//    lcd.print("Agua:");
//    if(distancia >= 100 || distancia <=0){
//      lcd.print("Fuera de Rango");
//      Serial.println("Fuera de Rango");
//      Serial.println(distancia);
//    }else{
//      lcd.print(distance);
//      lcd.print("cm");
//      Serial.println(distancia);
//    }

      //Impresion de Humedad de Suelo 
      lcd.setCursor(0,3);
      lcd.print("Suelo:");
      lcd.print(humSuelo);
    

    if(alerta==false){
      if(riego==false){
        //Acciones respecto a riego de la planta
        riego = tiempo_riego(riego, humSuelo);
      }
    }else{
      delay(10000);
    }
    
    delay(20000);

    if(i==3){
      envio_datos(temperatura, humedad, humSuelo, alerta);
      riego = false; 
    }
  }
}

float obtencion_distancia(){
  int duracion, distancia;
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(1000);
  digitalWrite(trigPin, LOW);
  duracion = pulseIn(echoPin, HIGH);
  distancia =(duracion/2)/29.1 ; //cm
  return distancia;
}

bool tiempo_riego(bool riego, int humedad){
  int hora = myRTC.hours;
  int minutos = myRTC.minutes;
  if(hora == 8){
    if(minutos==33){
      if(humedad>700){
        lcd.clear();
        lcd.print("REGANDO");
        digitalWrite(motor, HIGH);
        delay(10000);
        digitalWrite(motor, LOW);
        return true; 
      }else{
        lcd.clear();
        lcd.print("ESTADO OPTIMO");
        delay(10000);
        return true; 
      }
    }else{
      delay(10000);
    }
  }else{
    delay(10000);
  }
  return false;
}

bool alerta_deposito(float distancia){
    if(distancia>=11){
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("ALERTA");
      lcd.setCursor(0,2);
      lcd.print("AGUA INSUFICIENTE");
      delay(10000);
      return true; 
    }
    return false; 
  }


void conexion_wifi(){
  WiFi.begin(ssid, passwd);
  Serial.println("Conectando");
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
}

void envio_datos(float temperatura, float humedad, int humSuelo, bool alerta){

  int estado_bomba=digitalRead(motor);

  if (WiFi.status() == WL_CONNECTED){
    
    HTTPClient http;

    http.begin("http://192.168.0.8/API/guardar_riego.php");
    http.addHeader("Content-Type", "application/json"); // Mandamos el encabezado haciendo referencia que los datos enviados serán por medio de un json

    StaticJsonDocument<200> doc;

    doc["temperatura"] = temperatura;
    doc["humedad"] = humedad;
    doc["humedad_suelo"] = humSuelo;
    doc["estado_agua"] = estado_bomba;

    //JsonArray data = doc.createNestedArray("data");
    String requestBody;
    serializeJson(doc,requestBody);
    
    int status_response = http.POST(requestBody); // Enviamos el post pasándole, los datos que queremos enviar.
    if (status_response > 0){
      Serial.println("Código HTTP ► " + String(status_response));

      if (status_response == 200){
        String data_response= http.getString();
        Serial.println("El servidor respondió ▼ ");
        Serial.println(data_response);
      }
    }else{

      Serial.print(" Error enviando POST, código: ");
      Serial.println(status_response);
    }
    http.end(); 
  }
}
