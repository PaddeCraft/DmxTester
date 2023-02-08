#include <WebServer.h>
#include <ESPmDNS.h>
#include <Arduino.h>
#include <WiFi.h>

#include <esp_dmx.h>           // esp_dmx
#include <ArduinoJson.h>       // ArduinoJson
#include <LiquidCrystal_I2C.h> // LiquidCrystal I2C by Frank de Brabander

String version = "v1.0.0";

bool write_mode = false;

int transmitPin = 17;
int receivePin = 16;
int enablePin = 21;

int potPin = 36;
int wiringModePin = 2;

dmx_port_t dmxPort = 1;

byte data[DMX_PACKET_SIZE];

IPAddress local_ip(192, 168, 178, 1);
IPAddress gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 255, 0);

WebServer server(80);

LiquidCrystal_I2C lcd(0x27, 20, 4); // Visit https://www.electronics-lab.com/project/using-16x2-i2c-lcd-display-esp32/ and go to 'Detecting the I2C Address of the LCD' to get the hex address

int channel = 1;

byte zero[] = {
    B00000,
    B00000,
    B00000,
    B00000,
    B00000,
    B00000,
    B00000,
    B00000};
byte one[] = {
    B10000,
    B10000,
    B10000,
    B10000,
    B10000,
    B10000,
    B10000,
    B10000};
byte two[] = {
    B11000,
    B11000,
    B11000,
    B11000,
    B11000,
    B11000,
    B11000,
    B11000};
byte three[] = {
    B11100,
    B11100,
    B11100,
    B11100,
    B11100,
    B11100,
    B11100,
    B11100};
byte four[] = {
    B11110,
    B11110,
    B11110,
    B11110,
    B11110,
    B11110,
    B11110,
    B11110};
byte five[] = {
    B11111,
    B11111,
    B11111,
    B11111,
    B11111,
    B11111,
    B11111,
    B11111};

void setup()
{
    // Configure lcd
    lcd.init();
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("DmxTester " + version);
    lcd_set_starting_status("LCD");

    lcd.createChar(0, zero);
    lcd.createChar(1, one);
    lcd.createChar(2, two);
    lcd.createChar(3, three);
    lcd.createChar(4, four);
    lcd.createChar(5, five);

    // Setup DMX
    lcd_set_starting_status("DMX");
    dmx_set_pin(dmxPort, transmitPin, receivePin, enablePin);
    dmx_driver_install(dmxPort, DMX_DEFAULT_INTR_FLAGS);

    // Configure WiFi AP
    lcd_set_starting_status("WiFi");
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(local_ip, gateway, subnet);
    WiFi.softAP("dmx-tester-ap");

    // Configure WebServer
    delay(500);
    lcd_set_starting_status("server");
    server.on("/", route_index);
    server.on("/single", route_status_single);
    server.on(" de/api/set", route_api_set);
    server.on("/api/all", route_api_all);
    server.on("/api/single", route_api_single);
    server.onNotFound(route_404);

    // Start DNS
    lcd_set_starting_status("DNS");
    MDNS.begin("dmxtester");

    lcd_set_starting_status("done.");
    delay(500);
}

void loop()
{
    write_mode = digitalRead(wiringModePin);
    int pot_val = analogRead(potPin);

    display_channel(channel, data[channel]);

    if (write_mode)
    {
        data[channel] = map(pot_val, 0, 4095, 0, 255);
        dmx_write(dmxPort, data, DMX_PACKET_SIZE);
        dmx_send(dmxPort, DMX_PACKET_SIZE);
        dmx_wait_sent(dmxPort, DMX_TIMEOUT_TICK);
    }
    else
    {
        dmx_packet_t packet;

        if (dmx_receive(dmxPort, &packet, DMX_TIMEOUT_TICK))
        { /* If this code gets called, it means we've received DMX data! */

            /* We should check to make sure that there weren't any DMX errors. */
            if (!packet.err)
            {
                dmx_read(dmxPort, data, packet.size);
            }
        }

        channel = map(pot_val, 0, 4095, 1, 512);
    }
}

void lcd_set_starting_status(String unit)
{
    lcd.setCursor(0, 1);
    lcd.print("Starting " + unit);
}

void update_progress_bar(unsigned long count, unsigned long totalCount, int lineToPrintOn)
{
    // Source: https://www.instructables.com/Simple-Progress-Bar-for-Arduino-and-LCD/

    double factor = totalCount / 80.0;
    int percent = (count + 1) / factor;
    int number = percent / 5;
    int remainder = percent % 5;
    if (number > 0)
    {
        for (int j = 0; j < number; j++)
        {
            lcd.setCursor(j, lineToPrintOn);
            lcd.write(5);
        }
    }
    lcd.setCursor(number, lineToPrintOn);
    lcd.write(remainder);
    if (number < 16) // If using a 20 character display, this should be 20!
    {
        for (int j = number + 1; j <= 16; j++) // If using a 20 character display, this should be 20!
        {
            lcd.setCursor(j, lineToPrintOn);
            lcd.write(0);
        }
    }
}

void display_channel(int ch_num, int ch_val)
{
    String ch_str = String(ch_num);
    String val_str = String(ch_val);

    lcd.setCursor(0, 0);
    lcd.print(String(write_mode ? "W" : "R") + " Ch: " + ch_str + ": " + val_str);

    update_progress_bar(ch_val, 255, 1);
}

void route_index()
{
    server.send(200, "text/html", "<!DOCTYPE html><html lang='en'> <head> <meta charset='UTF-8'/> <meta http-equiv='X-UA-Compatible' content='IE=edge'/> <meta name='viewport' content='width=device-width, initial-scale=1.0'/> <title>DmxTester | All Channels</title> <style>body{font-family: 'Courier New', Courier, monospace;}.channel{background-color: rgb(192, 192, 192); width: 60px; height: 60px; padding: 5px; margin: 2px; display: inline-block;}.ch-nr{display: block; font-weight: 900; font-size: 20px;}.ch-val{font-size: 18px;}.ch-progress{width: 100%;}</style> </head> <body> <h1>DmxTester - All values</h1> <div id='channels'></div><script>ch_container=document.getElementById('channels'); function update(){fetch('/api/all') .then((data)=> data.json()) .then(function (data){var values=data.data; ch_container.innerHTML=''; for (i=1; i <=512; i++){ch_container.innerHTML +=` <a href='/single?ch=${i}'> <div class='channel'> <span class='ch-nr'>CH${i .toString() .padStart(3, '0')}</span> <span class='ch-val'>VAL${values[i - 1] .toString() .padStart(3, '0')}</span> <progress max='255' value='${values[i - 1]}' class='ch-progress'></progress> </div></a>`;}});}update(); setInterval(update, 1000); </script> </body></html>");
}

void route_api_single()
{
    if (server.hasArg("ch"))
    {
        int ch = atoi(server.arg("ch").c_str());

        if (ch >= 1 && ch <= 512)
        {
            server.send(200, "text/plain", String(data[ch]));
        }
        else
        {
            server.send(400, "text/plain", "1 >= channel <= 512");
        }
    }
    else
    {
        server.send(400, "text/plain", "Please provide the channel using the ch parameter");
    }
}

void route_api_all()
{
    DynamicJsonDocument resp(4096);
    resp["data"] = data;

    String output;
    serializeJson(resp, output);

    server.send(200, "application/json", output);
}

void route_api_set()
{
    if (server.hasArg("ch") && server.hasArg("val"))
    {
        int ch = atoi(server.arg("ch").c_str());
        int val = atoi(server.arg("val").c_str());

        if (ch >= 1 && ch <= 512 && val >= 0 && val <= 255)
        {
            data[ch] = val;

            server.send(200, "text/plain", "success");
        }
        else
        {
            server.send(400, "text/plain", "1 >= channel <= 512 && 0 >= value <= 255");
        }
    }
    else
    {
        server.send(400, "text/plain", "Please provide the parameters ch and val for channel and value");
    }
}

void route_status_single()
{
    server.send(200, "text/html", "<!DOCTYPE html><html lang='en'> <head> <meta charset='UTF-8'/> <meta http-equiv='X-UA-Compatible' content='IE=edge'/> <meta name='viewport' content='width=device-width, initial-scale=1.0'/> <title>DmxTester | Single Channel</title> </head> <style>body{font-family: 'Courier New', Courier, monospace;}#ch-val-wrapper{width: 100%; display: flex;}#ch-val-progress{flex-grow: 100; margin-left: 10px;}</style> <body> <h1>DmxTester - Channel <span id='ch-num'>0</span></h1> <div id='ch-val-wrapper'> Loading <progress id='ch-val-progress'></progress> </div><br/> <label for='set-val-inp'>Set value</label> <input type='number' id='set-val-inp' value='255' min='0' max='255'/> <button id='set-val-set'>Set</button> <script>var channel=parseInt( new URL(window.location).searchParams.get('ch') ); if (!channel || channel < 1 || channel > 512){channel=1;}document.getElementById('ch-num').innerText=channel; async function update(){var resp=await fetch('/api/single'); document.getElementById('ch-val-wrapper').innerHTML=` VAL${resp.text.padStart(3, '0')}<progress max='255' value='${resp.text}' id='ch-val-progress'></progress> `;}update(); setInterval(update, 500); document .getElementById('set-val-set') .addEventListener('click', function (){fetch( '/api/set?ch=' + channel + '&val=' + document.getElementById('set-val-inp').value );}); </script> </body></html>");
}

void route_404()
{
    server.send(404, "text/plain", "404 Not Found");
}