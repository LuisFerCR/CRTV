/*
 *
 *IoT Technologies
 *
 *  Created on: 02-05-2019
 *  Basado en los siguientes proyectos:
 *  
 */

// Inclusión de librerías
#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <WiFiClient.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <WebSocketsClient.h>
#include <Hash.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <DNSServer.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <Ticker.h>


//Variables del entorno
Ticker ticker;
char host[] = "tvcloudservice.herokuapp.com"; //Nombre de la App en Heroku. Ejemplo:"miaplicación.herokuapp.com" 
MDNSResponder mdns;
ESP8266WebServer server(80);
IRsend irsend(4);  // Puerto GPIO pin 4 (D2) del ESP8266 que controla al LED Infrarrojo.
uint16_t rawData[41] = {2672, 882,  448, 892,  446, 444,  446, 446,  448, 874,  892, 450,  448, 444,  446, 446,  448, 442,  446, 446,  446, 444,  448, 444,  446, 446,  446, 444,  448, 444,  446, 444,  892, 892,  478, 414,  444, 448,  448, 444,  446};
int pingCount = 0;
String niveles;
int port = 80;
char path[] = "/ws"; 
ESP8266WiFiMulti WiFiMulti;
WebSocketsClient webSocket;
const size_t capacity = 6*JSON_ARRAY_SIZE(1) + 2*JSON_OBJECT_SIZE(0) + 10*JSON_OBJECT_SIZE(1) + 6*JSON_OBJECT_SIZE(2) + 4*JSON_OBJECT_SIZE(3) + 3*JSON_OBJECT_SIZE(4) + 4*JSON_OBJECT_SIZE(5) + JSON_OBJECT_SIZE(8) + 3760;
DynamicJsonDocument doc(capacity);
String currState, oldState, message;


void setup() {
  Serial.begin(115200);
  //Inicializa función IR.
  irsend.begin(); 
  Serial.setDebugOutput(true);
  //set led pin as output
  pinMode(BUILTIN_LED, OUTPUT);
  // inicializa parpadeo de indicador LED cada 0,5 segundos dado ingreso a modo AP del ESP8266.
  ticker.attach(0.6, tick);
  //WiFiManager
  //Inicialización de variable. Solo se requiere realizar una vez.
  WiFiManager wifiManager;
  //Si se desea evitar que el módulo WiFi utilice la información preconfigurada de SSID y password, se puede habilitar la siguiente línea.
  wifiManager.resetSettings();

  // Esta función se activa cuando la conexión a WiFi con la conexión falla utilizando el SSID y password almacenado anteriormente. Permite ingresar a modo de AP.
  wifiManager.setAPCallback(configModeCallback);

  //Almacena el SSID y password. Inicia conexión al WiFi.
  if (!wifiManager.autoConnect()) {
    Serial.println("Conexión fallida");
    //reinicia ESP para intentar nuevamente la conexión.
    ESP.reset();
    delay(1000);
  }

  //en este punto la conexión WiFi está establecida. Se desactiva parpadeo del LED y se mantiene permanentemente en modo encendido.
  ticker.detach();
  digitalWrite(BUILTIN_LED, LOW);

    
  Serial.println("Conectado a la red WiFi");
  Serial.println("Dirección IP: "+WiFi.localIP());
  //Inicializa websocket.
  webSocket.begin(host, port, path);
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);

  if (mdns.begin("esp8266", WiFi.localIP())) {
    Serial.println("MDNS responder inicializado");
  }
  //Activa servicios del servidor.
  server.on("/", handleRoot);
  server.on("/ir", handleIr);
  server.on("/inline", [](){
  server.send(200, "text/plain", "Servidor activado");
  });

  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("Servidor HTTP inicializado");
}

void loop() {
  server.handleClient();
  webSocket.loop();
  //Se establece un ciclo de 20s para mantener el keepalive activo entre Heroku y el EPS8266.
  delay(2000);
	if (pingCount > 10) {
		pingCount = 0;
		webSocket.sendTXT("\"heartbeat\":\"keepalive\"");
	}
	else {
		pingCount += 1;
	}
}

//Módulo para inversión de estado encendido/apagado del BUILTIN_LED. Se utiliza para indicar el estado de conexión a WiFi.
void tick()
{
  int state = digitalRead(BUILTIN_LED);  // Leer el estado actual del LED interno
  digitalWrite(BUILTIN_LED, !state);     // Invertir el estado actual del LED interno
}

// Inicializa estado de ingreso al modo de configuración WiFi
void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  Serial.println(myWiFiManager->getConfigPortalSSID());
  //Al ingresar al modo de configuración WiFi, el indicador LED parpadea más rápidamente.
  ticker.attach(0.2, tick);
}

//configuración base de página web.
void handleRoot() {
  server.send(200, "text/html",
              "<html>" \
                "<head><title>CRTV</title></head>" \
                "<body>" \
                  "<h1>Hola! Bienvenido a CRTV. </h1>Selecciona la opcion que deseas ejecutar. CRTV hara el resto por ti!</h1>" \
                  "<p><a href=\"ir?code=12\">Encendido/Apagado</a></p>" \
                  "<p><a href=\"ir?code=16\">Subir volumen</a></p>" \
                  "<p><a href=\"ir?code=17\">Bajar volumen</a></p>" \
                  "<p><a href=\"ir?code=32\">Subir canal</a></p>" \
                  "<p><a href=\"ir?code=33\">Bajar canal</a></p>" \
                  "<p><a href=\"ir?code=13\">Mute</a></p>" \        
                "</body>" \
              "</html>");
}

//Módulo de gestión de IR.
void handleIr() {
  for (uint8_t i = 0; i < server.args(); i++) {
    if (server.argName(i) == "code") {
      uint32_t code = strtoul(server.arg(i).c_str(), NULL, 10);
      irsend.sendRC6(code, 20);
      Serial.println(code);
      delay(100);
    }
  }
  handleRoot();
}

//Módulo de manejo de fallo.
void handleNotFound() {
  String message = "Archivo no encontrado\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++)
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  server.send(404, "text/plain", message);
}


//Gestión de eventos del Websocket
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) { 

    switch(type) {
        case WStype_DISCONNECTED:
            Serial.println("Desconectado!");
            Serial.println("Conectando...");
            webSocket.begin(host, port, path);          
            webSocket.onEvent(webSocketEvent);
            break;
            
        case WStype_CONNECTED:
            Serial.println("Conectado! ");
            //Enviar mensaje al servidor cuando se ha establecido conexión.
            webSocket.sendTXT("Connected");
            break;
            
        case WStype_TEXT:
            Serial.println("Datos recibidos");
            //Serial.println((char*)payload);
            Serial.println("Analizando...");
            //Analiza datos recibidos.
            processWebScoketRequest((char*)payload);
            break;
            
        case WStype_BIN:
            hexdump(payload, length);
            Serial.print("Datos binarios recibidos");
            // Enviar datos al servidor.
            //webSocket.sendBIN(payload, length);
            break;
    }
}

//Módulo de procesamiento de datos obtenidos a través del websocket
void processWebScoketRequest(char* data)
{
            String jsonResponse = "{\"version\": \"1.0\",\"sessionAttributes\": {},\"response\": {\"outputSpeech\": {\"type\": \"PlainText\",\"text\": \"<text>\"},\"shouldEndSession\": true}}";
            //Serial.println(data);
            //Arduino JSON Tool: https://arduinojson.org/v5/assistant/
            deserializeJson(doc, data, DeserializationOption::NestingLimit(11));
            JsonObject request = doc["request"];
            JsonObject request_intent = request["intent"];
            JsonObject request_intent_slots = request_intent["slots"];
            JsonObject request_intent_slots_instance = request_intent_slots["instancia"];
            JsonObject request_intent_slots_state = request_intent_slots["estado"];

            String instancia = request_intent_slots_instance["value"];
            String estate = request_intent_slots_state["value"];
            String pregunta = request_intent_slots["pregunta"]["confirmationStatus"];
            
            Serial.println(instancia);
            Serial.println(estate);
            Serial.println(pregunta);
            
            if(pregunta == "?"){ //if command then execute
              Serial.println("Consulta de estado recibida!");
             jsonResponse.replace("<text>", "Consulta de estado recibida");
             webSocket.sendTXT(jsonResponse);
                   
            }else if(instancia == "volumen"){ 
              if((estate == "suba") || (estate == "sube") || (estate == "incremente") || (estate == "incrementa")){
                  for (uint8_t i = 0; i < 5; i++){
                    irsend.sendRC6(16, 20);
                    delay(500);
                  }
                  Serial.println("Subir Volumen");
              }else if((estate == "baje") || (estate == "baja") || (estate == "disminuye") || (estate == "disminuya")){
                  for (uint8_t i = 0; i < 5; i++){
                    irsend.sendRC6(17, 20);
                    delay(500);
                  }
                  Serial.println("Bajar Volumen"); 
              }else if((estate == "suba 2") || (estate == "suba 3") || (estate == "suba 4")|| (estate == "suba 5")|| (estate == "suba 6")|| (estate == "suba 7")){
                  niveles = estate[5];
                  for (uint8_t i = 0; i < niveles.toInt(); i++){ //El caracter [5] del string estate corresponde al número de niveles.
                    irsend.sendRC6(16, 20);
                    delay(500);
                  }
                  Serial.println("Subir x niveles el volumen"); 
              }
              else if((estate == "baja 2") || (estate == "baja 3") || (estate == "baja 4")|| (estate == "baja 5")|| (estate == "baja 6")|| (estate == "baja 7")){
                  niveles = estate[5];
                  for (uint8_t i = 0; i < niveles.toInt(); i++){ //El caracter [5] del string estate corresponde al número de niveles.
                    irsend.sendRC6(17, 20);
                    delay(500);
                  }
                  Serial.println("Bajar x niveles el volumen"); 
              }        
              jsonResponse.replace("<text>", "Ok");
              webSocket.sendTXT(jsonResponse);    
            }else if(instancia == "silencio"){ 
              if((estate == "active")|| (estate == "activa") ){
                  irsend.sendRC6(13, 20);
                  Serial.println("Modo silencio activado");
              }else if((estate == "desactive") || (estate == "desactiva") ){
                  irsend.sendRC6(13, 20);
                  Serial.println("Modo silencio desactivado");
              }      
              jsonResponse.replace("<text>", "Ok");
              webSocket.sendTXT(jsonResponse);    
            }else if(instancia == "control"){ 
              if((estate == "activa") || (estate == "active") || (estate == "encienda")  || (estate == "enciende") ){
                  irsend.sendRC6(12, 20);
                  Serial.println("TV encendido");
              }else if((estate == "desactive")  || (estate == "apaga") || (estate == "apague")){
                  irsend.sendRC6(12, 20);
                  Serial.println("TV apagado");
              }    
              jsonResponse.replace("<text>", "Ok");
              webSocket.sendTXT(jsonResponse);    
            }else if(instancia == "canal"){ 
              if(estate == "siguiente"){
                  irsend.sendRC6(32, 20);
                  Serial.println("Subir canal");
              }else if(estate == "anterior"){
                  irsend.sendRC6(33, 20);
                  Serial.println("Bajar canal");
              }else if(estate == "7"){
                  irsend.sendRC6(7, 20);
                  Serial.println("Canal 7");
              }else if(estate == "45"){
                  irsend.sendRC6(4, 20);
                  delay(200);
                  irsend.sendRC6(5, 20);
                  Serial.println("Canal 45");
              }       
              jsonResponse.replace("<text>", "Ok");
              webSocket.sendTXT(jsonResponse);                
            }else{//en caso de que no se reconozca el comando.
                   Serial.println("Comando no reconocido!");
                   jsonResponse.replace("<text>", "repita el comando");
                   webSocket.sendTXT(jsonResponse);
            }
            Serial.print("Enviando respuesta");
            Serial.println(jsonResponse);
            webSocket.sendTXT(jsonResponse);

}
