#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "EspMQTTClient.h"
#include <UniversalTelegramBot.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESPConnect.h>
#include <EEPROM.h>

AsyncWebServer server(80);

void onConnectionEstablishedClient2();

EspMQTTClient client1(
  "mqtt.zastudios.org",  // MQTT Broker server ip
  1883,
  "esp",   // Can be omitted if not needed
  "morse",   // Can be omitted if not needed
  "MorseLamp"      // Client name that uniquely identify your device
);

EspMQTTClient client2(
  "io.adafruit.com",
  1883,
  "ZaleAnderson",
  "aio_ywxt52FNdtoNYYFuHl0ss0XBYIfD",
  "ZaleAnderson"
);

#define BOT_TOKEN "5881226880:AAHIjt7Z46X6m_oBwBMymuDhhckkAs8nLf0"
#define CHAT_ID "5485248007"

WiFiClientSecure secured_client;
WiFiClient espClient;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);

const int LED_PIN = 6;
const int BEEP_PIN = 9;

const char *morseLetters[] = {".-", "-...", "-.-.", "-..", ".", "..-.", "--.", "....", "..", ".---", "-.-", ".-..", "--", "-.", "---", ".--.", "--.-", ".-.", "...", "-", "..-", "...-", ".--", "-..-", "-.--", "--.."};
const char *morseNumbers[] = {"-----", ".----", "..---", "...--", "....-", ".....", "-....", "--...", "---..", "----."};

bool ledstate = true;

void idle () {
  delay(2000);
  digitalWrite(LED_PIN, ledstate);
  client1.publish("log", "zzZZZZzzZZzZzzZZzZZzzZz");
}

void blinkMorse(char letter) {
  int index;
  if (isalpha(letter)) {
    index = toupper(letter) - 'A';
  } else if (isdigit(letter)) {
    index = letter - '0' + 26;
  } else if (letter == ' ') {
    delay(500);
    return;
  } else {
    return;
  }
  const char *morseCode = isalpha(letter) ? morseLetters[index] : morseNumbers[index - 26];
  for (int i = 0; morseCode[i] != '\0'; i++) {
    digitalWrite(LED_PIN, HIGH);
    digitalWrite(BEEP_PIN, HIGH);
    delay(morseCode[i] == '.' ? 125 : 375);
    digitalWrite(LED_PIN, LOW);
    digitalWrite(BEEP_PIN, LOW);
    delay(125);
  }
  delay(250);
}

void onConnectionEstablished() {
  client1.subscribe("morsecode", [] (const String & payload)  {
    if (payload == "restart") {
      client1.publish("log", "Commanded restart");
      ESP.restart();
    }
    if (payload == "soundoff") {
      digitalWrite(BEEP_PIN, HIGH);
      delay(100);
      digitalWrite(BEEP_PIN, LOW);
      pinMode(BEEP_PIN, INPUT);
      client1.publish("log", "Sound Off");
      return;
    }
    if (payload == "soundon") {
      pinMode(BEEP_PIN, OUTPUT);
      digitalWrite(BEEP_PIN, HIGH);
      delay(100);
      digitalWrite(BEEP_PIN, LOW);
      client1.publish("log", "Sound on");
      return;
    }
    if (payload == "erase") {
      client1.publish("log", "Erasing Wifi settings and restarting");
      ESPConnect.erase();
      digitalWrite(LED_PIN, LOW);
      delay(1000);
      ESP.restart();
    }
    Serial.println(payload);
    digitalWrite(LED_PIN, LOW);
    delay(1000);
    for (int i = 0; i < payload.length(); i++) {
      blinkMorse(payload[i]);
      delay(250);
    }
    bot.sendMessage(CHAT_ID, "Zale Says: " + payload, "");
    client1.publish("log", "Message recieved and sent");
    idle();
  });
}

// For client2
void onConnectionEstablishedClient2() {
  client2.subscribe("ZaleAnderson/feeds/espled", [](const String & payload) {
    Serial.println(payload);

    if (payload == "OFF") {
      ledstate = false;
      digitalWrite(LED_PIN, ledstate ? HIGH : LOW);
      client1.publish("log", "Light off");
      return;
    }

    if (payload == "ON") {
      ledstate = true;
      digitalWrite(LED_PIN, ledstate ? HIGH : LOW);
      client1.publish("log", "Light on");
      return;
    }
  });
}

void setup() {
  Serial.begin(115200);
  ESPConnect.autoConnect("Morse Lamp Setup");
  if (ESPConnect.begin(&server)) {
    Serial.println("Connected to WiFi");
    Serial.println("IPAddress: " + WiFi.localIP().toString());
  } else {
    Serial.println("Failed to connect to WiFi");
  }

  server.on("/", HTTP_GET, [&](AsyncWebServerRequest * request) {
    request->send(200, "text/plain", "Hello from ESP");
  });
  server.begin();

  EEPROM.begin(1);
  ledstate = EEPROM.read(0);
  digitalWrite(LED_PIN, ledstate);

  client2.setOnConnectionEstablishedCallback(onConnectionEstablishedClient2);
  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT);
  client1.enableMQTTPersistence();
  client2.enableMQTTPersistence();
  pinMode(LED_PIN, OUTPUT);
  pinMode(BEEP_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);
  idle();
}

void loop() {
  client1.loop();
  client2.loop();
  static unsigned long lastMillis = 0;
  unsigned long currentMillis = millis();

  if (currentMillis - lastMillis >= 60000) {
    lastMillis = currentMillis;

    if (ESPConnect.isConnected() == false) {
      digitalWrite(LED_PIN, LOW);
      ESP.restart();
    }
  }

}
