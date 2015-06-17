#include <OneWire.h>
#include <ESP8266WiFi.h>
 
 
#define DS18x20_PIN 4

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
 
OneWire *ds;  // on pin 2 (a 4.7K resistor is necessary)
 
void connectWifi();
void reconnectWifiIfLinkDown();
void initDs18b20(OneWire **ds);
void readDs18B20(OneWire *ds, float *temp);

void setup() {
    Serial.begin(115200);
    delay(10);
 
    connectWifi();
    initDs18b20(&ds, DS18x20_PIN);
}
 
void loop() {
    static float t_ds;

    readDs18B20(ds, &t_ds);
 
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
 

void initDs18b20(OneWire **ds, uint8_t pin) {
    *ds = new OneWire(pin);
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
