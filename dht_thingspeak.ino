#include "DHT.h"
#include <OneWire.h>
#include <ESP8266WiFi.h>


#define DS18x20_PIN 4

#define DHTPIN 2     // what pin we're connected to
#define DHTTYPE DHT22   // DHT 22  (AM2302)

#define DEBUG

#define DEBUG_PRINTER Serial

#ifdef DEBUG
#define DEBUG_PRINT(...) { DEBUG_PRINTER.print(__VA_ARGS__); }
#define DEBUG_PRINTLN(...) { DEBUG_PRINTER.println(__VA_ARGS__); }
#else
#define DEBUG_PRINT(...) {}
#define DEBUG_PRINTLN(...) {}
#endif

const char* ssid     = "OpenWrt_NAT_500GP.101";
const char* password = "activegateway";

DHT *dht;
OneWire *ds;  // on pin 2 (a 4.7K resistor is necessary)

void connectWifi();
void reconnectWifiIfLinkDown();
void initDht(DHT **dht, uint8_t pin, uint8_t dht_type);
void initDs18b20(OneWire **ds, uint8_t pin);
void readDs18B20(OneWire *ds, float *temp);
void readDht(DHT *dht, float *temp, float *humid);
void uploadThingsSpeak(float t, float );

void setup() {
    Serial.begin(115200);
    delay(10);

    connectWifi();

    initDht(&dht, DHTPIN, DHTTYPE);
    initDs18b20(&ds, DS18x20_PIN);
}

void loop() {
    static float t_ds;
    static float t_dht;
    static float h_dht;

    readDht(dht, &t_ds, &h_dht);
    readDs18B20(ds, &t_ds);

    //uploadFn(t, t);
    DEBUG_PRINTLN(t_ds);

    // Wait a few seconds between measurements.
    delay(10 * 1000);

    reconnectWifiIfLinkDown();
}

void reconnectWifiIfLinkDown() {
    if (WiFi.status() != WL_CONNECTED) {
        DEBUG_PRINTLN("WIFI DISCONNECTED");
        connectWifi();
    }
}

void connectWifi() {
    DEBUG_PRINTLN();
    DEBUG_PRINTLN();
    DEBUG_PRINT("Connecting to ");
    DEBUG_PRINTLN(ssid);

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        DEBUG_PRINT(".");
    }

    DEBUG_PRINTLN("");
    DEBUG_PRINTLN("WiFi connected");
    DEBUG_PRINTLN("IP address: ");
    DEBUG_PRINTLN(WiFi.localIP());
}

void initDht(DHT **dht, uint8_t pin, uint8_t dht_type) {
    // Connect pin 1 (on the left) of the sensor to +5V
    // NOTE: If using a board with 3.3V logic like an Arduino Due connect pin 1
    // to 3.3V instead of 5V!
    // Connect pin 2 of the sensor to whatever your DHTPIN is
    // Connect pin 4 (on the right) of the sensor to GROUND
    // Connect a 10K resistor from pin 2 (data) to pin 1 (power) of the sensor

    // Initialize DHT sensor for normal 16mhz Arduino
    // NOTE: For working with a faster chip, like an Arduino Due or Teensy, you
    // might need to increase the threshold for cycle counts considered a 1 or 0.
    // You can do this by passing a 3rd parameter for this threshold.  It's a bit
    // of fiddling to find the right value, but in general the faster the CPU the
    // higher the value.  The default for a 16mhz AVR is a value of 6.  For an
    // Arduino Due that runs at 84mhz a value of 30 works.
    // Example to initialize DHT sensor for Arduino Due:
    //DHT dht(DHTPIN, DHTTYPE, 30);

    *dht = new DHT(pin, dht_type, 30);
    (*dht)->begin();
    DEBUG_PRINTLN(F("DHTxx test!"))  ;
}


void initDs18b20(OneWire **ds, uint8_t pin) {
    *ds = new OneWire(pin);
}

void uploadThingsSpeak(float t, float h) {
    static const char* host = "api.thingspeak.com";
    static const char* apiKey = "14UX64T1VR0YG0CE";

    // Use WiFiClient class to create TCP connections
    WiFiClient client;
    const int httpPort = 80;
    if (!client.connect(host, httpPort)) {
        DEBUG_PRINTLN("connection failed");
        return;
    }

    // We now create a URI for the request
    String url = "/update/";
    //  url += streamId;
    url += "?key=";
    url += apiKey;
    url += "&field1=";
    url += t;
    url += "&field2=";
    url += h;

    DEBUG_PRINT("Requesting URL: ");
    DEBUG_PRINTLN(url);

    // This will send the request to the server
    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: " + host + "\r\n" +
                 "Connection: close\r\n\r\n");
}

void readDht(DHT *dht, float *temp, float *humid) {

    if (dht == NULL) {
        DEBUG_PRINTLN(F("[dht22] is not initialised. please call initDht() first."));
        return;
    }

    // Reading temperature or humidity takes about 250 milliseconds!
    // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
    float h = dht->readHumidity();

    // Read temperature as Celsius
    float t = dht->readTemperature();
    // Read temperature as Fahrenheit
    float f = dht->readTemperature(true);

    // Check if any reads failed and exit early (to try again).
    if (isnan(h) || isnan(t) || isnan(f)) {
        DEBUG_PRINTLN("Failed to read from DHT sensor!");
        return;
    }

    // Compute heat index
    // Must send in temp in Fahrenheit!
    float hi = dht->computeHeatIndex(f, h);

    DEBUG_PRINT("Humidity: ");
    DEBUG_PRINT(h);
    DEBUG_PRINT(" %\t");
    DEBUG_PRINT("Temperature: ");
    DEBUG_PRINT(t);
    DEBUG_PRINT(" *C ");
    DEBUG_PRINT(f);
    DEBUG_PRINT(" *F\t");
    DEBUG_PRINT("Heat index: ");
    DEBUG_PRINT(hi);
    DEBUG_PRINTLN(" *F");

    *temp = t;
    *humid = f;

}

void readDs18B20(OneWire *ds, float *temp) {
    if (ds == NULL) {
        DEBUG_PRINTLN(F("[ds18b20] is not initialised. please call initDs18b20() first."));
        return;
    }


    byte i;
    byte present = 0;
    byte type_s;
    byte data[12];
    byte addr[8];
    float celsius, fahrenheit;

    if ( !ds->search(addr)) {
        DEBUG_PRINTLN("No more addresses.");
        DEBUG_PRINTLN();
        ds->reset_search();
        delay(250);
        return;
    }

    DEBUG_PRINT("ROM =");
    for ( i = 0; i < 8; i++) {
        Serial.write(' ');
        DEBUG_PRINT(addr[i], HEX);
    }

    if (OneWire::crc8(addr, 7) != addr[7]) {
        DEBUG_PRINTLN("CRC is not valid!");
        return;
    }
    DEBUG_PRINTLN();

    // the first ROM byte indicates which chip
    switch (addr[0]) {
    case 0x10:
        DEBUG_PRINTLN("  Chip = DS18S20");  // or old DS1820
        type_s = 1;
        break;
    case 0x28:
        DEBUG_PRINTLN("  Chip = DS18B20");
        type_s = 0;
        break;
    case 0x22:
        DEBUG_PRINTLN("  Chip = DS1822");
        type_s = 0;
        break;
    default:
        DEBUG_PRINTLN("Device is not a DS18x20 family device.");
        return;
    }

    ds->reset();
    ds->select(addr);
    ds->write(0x44, 1);        // start conversion, with parasite power on at the end

    delay(1000);     // maybe 750ms is enough, maybe not
    // we might do a ds->depower() here, but the reset will take care of it.

    present = ds->reset();
    ds->select(addr);
    ds->write(0xBE);         // Read Scratchpad

    DEBUG_PRINT("  Data = ");
    DEBUG_PRINT(present, HEX);
    DEBUG_PRINT(" ");
    for ( i = 0; i < 9; i++) {           // we need 9 bytes
        data[i] = ds->read();
        DEBUG_PRINT(data[i], HEX);
        DEBUG_PRINT(" ");
    }
    DEBUG_PRINT(" CRC=");
    DEBUG_PRINT(OneWire::crc8(data, 8), HEX);
    DEBUG_PRINTLN();

    // Convert the data to actual temperature
    // because the result is a 16 bit signed integer, it should
    // be stored to an "int16_t" type, which is always 16 bits
    // even when compiled on a 32 bit processor.
    int16_t raw = (data[1] << 8) | data[0];
    if (type_s) {
        raw = raw << 3; // 9 bit resolution default
        if (data[7] == 0x10) {
            // "count remain" gives full 12 bit resolution
            raw = (raw & 0xFFF0) + 12 - data[6];
        }
    } else {
        byte cfg = (data[4] & 0x60);
        // at lower res, the low bits are undefined, so let's zero them
        if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
        else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
        else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
        //// default is 12 bit resolution, 750 ms conversion time
    }

    celsius = (float)raw / 16.0;
    fahrenheit = celsius * 1.8 + 32.0;

    DEBUG_PRINT("  Temperature = ");
    DEBUG_PRINT(celsius);
    DEBUG_PRINT(" Celsius, ");
    DEBUG_PRINT(fahrenheit);
    DEBUG_PRINTLN(" Fahrenheit");


    *temp = celsius;
}