/*

███████╗ ██████╗ ██████╗ ██╗    ██╗ █████╗ ████████╗ ██████╗██╗  ██╗    ██╗   ██╗ ██╗    ██████╗
██╔════╝██╔════╝██╔═══██╗██║    ██║██╔══██╗╚══██╔══╝██╔════╝██║  ██║    ██║   ██║███║   ██╔═████╗
█████╗  ██║     ██║   ██║██║ █╗ ██║███████║   ██║   ██║     ███████║    ██║   ██║╚██║   ██║██╔██║
██╔══╝  ██║     ██║   ██║██║███╗██║██╔══██║   ██║   ██║     ██╔══██║    ╚██╗ ██╔╝ ██║   ████╔╝██║
███████╗╚██████╗╚██████╔╝╚███╔███╔╝██║  ██║   ██║   ╚██████╗██║  ██║     ╚████╔╝  ██║██╗╚██████╔╝
╚══════╝ ╚═════╝ ╚═════╝  ╚══╝╚══╝ ╚═╝  ╚═╝   ╚═╝    ╚═════╝╚═╝  ╚═╝      ╚═══╝   ╚═╝╚═╝ ╚═════╝

[VK] : https://vk.com/madjogger
[TG] : https://t.me/madjogger

[30.06.22]
V1.0:
    added functions:
        1. Displaying time
        2. Getting CO2 value
        3. Getting amount of PM2.5 and PM10 in the air
        4. Monitoring of temperature, humidity and pressure lvl
        5. Display mode changing by sensor button
        6. Fast menu for bot
        7. White list for users ID
        8. Danger/bad air condision alert (bot)
[02.07.22]
V1.1:
    added functions:
        1. auto update for global acsess
        2. sensor button interrupt reading
        3. sensors value filters
        4. telegram UI update
        5. display update modification
*/

#include <Arduino.h>

#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <FastBot.h>
#include "GyverTM1637.h"

#define WIFI_SSID "name" 
#define WIFI_PASS "pass"   
#define BOT_TOKEN "token"

#define CLK D5
#define DIO D6
#define PM25 D0
#define PM10 D8
#define BTN D7
#define CO2 D3

#define CO2_MAX 1100
#define PM_MAX 190
#define TEMP_MAX 36

FastBot bot(BOT_TOKEN);
Adafruit_BME280 bme;
GyverTM1637 disp(CLK, DIO);

float bot_temp = 0;
float bot_pres = 0;
float bot_humid = 0;
long bot_pm25 = 0;
volatile long bot_pm10 = 0;
volatile long bot_co2 = 0;
long bot_time = 0;
long long tick_id = 0;

volatile long pm_10_read_id = 0;
long pm_25_read_id = 0;
volatile long pm_10_buff[7] = {-9, -9, -9, -9, -9, -9, -9};
long pm_25_buff[7] = {-9, -9, -9, -9, -9, -9, -9};

long long update_timer = 0;
long long global_update_timer = 0;
long long button_change_timer = 0;
long long update_delay = 1000;
long long global_update_delay = 2000;
long long button_change_delay = 300;

int display_mode = 0;
int last_display_mode = 0;
int disp_val = 0;
int last_disp_val = 0;
int points_val = 0;

volatile long long co2_tmr = 0;
volatile long long pm25_tmr = 0;
volatile long long pm10_tmr = 0;

//bool local_mode = 0;
//bool light_sleep = 0;
bool gloabal_acsess = 0;
bool send_uart_val = 0;
bool max_co2 = 0;
bool max_pm = 0;

#define TEMP_ST1 28
#define TEMP_ST2 32
#define TEMP_ST3 36
#define TEMP_ST4 40
#define TEMP_ST21 15
#define TEMP_ST22 11
#define TEMP_ST23 7
#define TEMP_ST24 3

#define HUMID_ST1 30
#define HUMID_ST2 70

#define CO2_ST1 500
#define CO2_ST2 800
#define CO2_ST3 1100
#define CO2_ST4 1600

#define PM_ST1 10
#define PM_ST2 20
#define PM_ST3 30
#define PM_ST4 50

String state_box[4] = {"🟩", "🟨", "🟧", "🟥"};

String white_id_list[3] = {" ",         // enter your ids
                           " ",
                           " "};
String admin_id = " ";

void connectWiFi();
void printValues();
void newMsg(FB_msg &msg);
IRAM_ATTR void getCO2();
IRAM_ATTR void getPM10();
IRAM_ATTR void sButton();
void getPM25();
void getBMEdata();
void getTime();
String getState(int sensorNum);

void setup()
{
    pinMode(A0, OUTPUT);
    pinMode(D7, INPUT);
    pinMode(D5, OUTPUT);
    pinMode(D6, OUTPUT);
    pinMode(D0, INPUT);
    pinMode(CO2, INPUT_PULLUP);
    pinMode(D8, INPUT);

    disp.clear();
    disp.brightness(1);

    Serial.begin(9600);
    connectWiFi();

    Wire.begin(4, 5);
    if (!bme.begin(0x77, &Wire))
    {
        Serial.println("Could not find a valid BME280 sensor, check wiring!");
        while (1)
            ;
    }

    bot.attach(newMsg);
    bot.clearServiceMessages(true);
    attachInterrupt(digitalPinToInterrupt(0), getCO2, CHANGE);
    attachInterrupt(digitalPinToInterrupt(15), getPM10, CHANGE);
    attachInterrupt(digitalPinToInterrupt(13), sButton, RISING);
    update_timer = millis() + update_delay;
}

void loop()
{
    bot.tick();
    FB_Time t = bot.getTime(3);
    bot_time = t.hour * 100 + t.minute;
    switch (display_mode)
    {
    case 0:
        disp_val = bot_time;
        points_val = 1;
        break;
    case 1:
        disp_val = int(bot_temp * 100);
        points_val = 1;
        break;
    case 2:
        disp_val = int(bot_humid);
        points_val = 0;
        break;
    case 3:
        disp_val = int(bot_pres);
        points_val = 0;
        break;
    case 4:
        disp_val = bot_co2;
        points_val = 0;
        break;
    case 5:
        disp_val = -200 - bot_pm25;
        points_val = 1;
        break;
    case 6:
        disp_val = -900 - bot_pm10;
        points_val = 0;
        break;
    default:
        break;
    }
    if ((display_mode != last_display_mode) || (last_disp_val != disp_val))
    {
        last_display_mode = display_mode;
        last_disp_val = disp_val;
        disp.clear();
        disp.displayInt(disp_val);
        disp.point(points_val);
    }
    if (millis() >= update_timer)
    {
        update_timer = millis() + update_delay;

        if (send_uart_val)
            printValues();
        if (tick_id % 4 == 0)
            getPM25();
        getBMEdata();

        if (bot_temp >= TEMP_MAX)
            bot.sendMessage("!!! Превышение максимальной температуры   !!!", admin_id);
        if ((bot_co2 >= CO2_MAX) && (!max_co2))
        {
            bot.sendMessage(String(bot_co2) + "\n" + "!!!  Превышение максимальной концентрации CO2 !!!", admin_id);
            max_co2 = 1;
        }
        else if ((bot_co2 < CO2_MAX))
            max_co2 = 0;
        if (((bot_pm25 >= PM_MAX) || (bot_pm10 >= PM_MAX)) && (!max_pm))
        {
            bot.sendMessage("!!! Превышение максимального содержания частиц  !!!", admin_id);
            max_pm = 1;
        }
        else if (((bot_pm25 < PM_MAX) && (bot_pm10 < PM_MAX)))
            max_pm = 0;
        if (global_update_timer <= millis())
        {
            bot.setChatTitle(String(bot_temp) + " " + String(bot_humid) + " " + String(bot_pres) + " " + String(bot_co2) + " " + String(bot_pm25) + " " + String(bot_pm10), "chat id");
            global_update_timer = millis() + global_update_delay;
        }
        tick_id++;
    }
}

void connectWiFi()
{
    delay(2000);
    Serial.println();

    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
        if (millis() > 20000)
            ESP.restart();
    }
    Serial.println("Connected");
}

IRAM_ATTR void getCO2()
{
    if (digitalRead(CO2))
    {
        co2_tmr = millis();
    }
    else
        bot_co2 = 4 * (millis() - co2_tmr - 2);
}

void getPM25()
{
    bot_pm25 = pulseIn(PM25, HIGH, 1004000) / 1000 - 2;
    if (pm_25_read_id <= 6)
        pm_25_buff[pm_25_read_id] = bot_pm25;
    else
    {
        int pm25 = bot_pm25 +
                   pm_25_buff[0] +
                   pm_25_buff[1] +
                   pm_25_buff[2] +
                   pm_25_buff[3] +
                   pm_25_buff[4] +
                   pm_25_buff[5] +
                   pm_25_buff[6];
        for (int i = 0; i < 6; i++)
            pm_25_buff[i] = pm_25_buff[i + 1];
        pm_25_buff[6] = bot_pm25;
        bot_pm25 = pm25 >> 3;
    }

    // В случае подклчючения через прерывания
    // if (digitalRead(PM25))
    //    pm25_tmr = millis();
    // else
    //    bot_pm25 = millis() - pm25_tmr - 2;
}

IRAM_ATTR void getPM10()
{
    // bot_pm10 = pulseIn(PM10, HIGH, 1004000) / 1000 - 2;
    if (digitalRead(PM10))
        pm10_tmr = millis();
    else
    {
        bot_pm10 = millis() - pm10_tmr - 2;
        if (pm_10_read_id <= 6)
            pm_10_buff[pm_10_read_id] = bot_pm10;
        else
        {
            volatile int pm10 = bot_pm10 +
                                pm_10_buff[0] +
                                pm_10_buff[1] +
                                pm_10_buff[2] +
                                pm_10_buff[3] +
                                pm_10_buff[4] +
                                pm_10_buff[5] +
                                pm_10_buff[6];
            for (volatile int i = 0; i < 6; i++)
                pm_10_buff[i] = pm_10_buff[i + 1];
            pm_10_buff[6] = bot_pm10;
            bot_pm10 = pm10 >> 3;
        }
        pm_10_read_id++;
    }
}

IRAM_ATTR void sButton()
{
    if (button_change_timer <= millis())
    {
        button_change_timer = millis() + button_change_delay;
        if (display_mode < 6)
            display_mode++;
        else
            display_mode = 0;
    }
}

void getBMEdata()
{
    bot_temp = bme.readTemperature() - 0.5F;
    bot_pres = bme.readPressure() / 100.0F;
    bot_humid = bme.readHumidity();
}

void filterValues()
{
    if (tick_id < 10)
    {
        pm_10_buff[tick_id] = bot_pm10;
        pm_10_buff[tick_id] = bot_pm10;
    }
}

void printValues()
{

    Serial.print("Temperature = ");
    Serial.print(bot_temp);
    Serial.println(" *C");

    Serial.print("Pressure = ");
    Serial.print(bot_pres);
    Serial.println(" hPa");

    Serial.print("Humidity = ");
    Serial.print(bot_humid);
    Serial.println(" %");

    Serial.print("pm2.5 = ");
    Serial.print(bot_pm25);
    Serial.println(" ug/m3");

    Serial.print("pm10 = ");
    Serial.print(bot_pm10);
    Serial.println(" ug/m3");

    Serial.print("co2 = ");
    Serial.print(bot_co2);
    Serial.println(" ppm");

    Serial.println();
}

String getState(int sensorNum)
{
    if (sensorNum == 0)
    { // temperature
        if ((bot_temp >= TEMP_ST21) && (bot_temp <= TEMP_ST1))
            return state_box[0];
        else if ((bot_temp >= TEMP_ST4) || (bot_temp <= TEMP_ST24))
            return state_box[3];
        else if ((bot_temp >= TEMP_ST3) || (bot_temp <= TEMP_ST23))
            return state_box[2];
        else
            return state_box[1];
    }
    if (sensorNum == 1)
    { // humid
        if (bot_humid <= HUMID_ST1)
            return state_box[1];
        if (bot_humid >= HUMID_ST2)
            return state_box[1];
        else
            return state_box[0];
    }
    if (sensorNum == 2)
    {                        // pressure
        return state_box[0]; // idk how to properly use pressure degree
    }
    if (sensorNum == 3)
    { // CO2
        if (bot_co2 <= CO2_ST1)
            return state_box[0];
        else if (bot_co2 <= CO2_ST2)
            return state_box[1];
        else if (bot_co2 <= CO2_ST3)
            return state_box[2];
        else
            return state_box[3];
    }
    if (sensorNum == 4)
    { // PM2.5
        if (bot_pm25 <= PM_ST1)
            return state_box[0];
        else if (bot_pm25 <= PM_ST2)
            return state_box[1];
        else if (bot_pm25 <= PM_ST3)
            return state_box[2];
        else
            return state_box[3];
    }
    if (sensorNum == 5)
    { // PM10
        if (bot_pm10 <= PM_ST1)
            return state_box[0];
        else if (bot_pm10 <= PM_ST2)
            return state_box[1];
        else if (bot_pm10 <= PM_ST3)
            return state_box[2];
        else
            return state_box[3];
    }
    return " ";
}

void newMsg(FB_msg &msg)
{
    if ((msg.chatID == white_id_list[0]) || (msg.chatID == white_id_list[1]) || (msg.chatID == white_id_list[2]))
    {
        Serial.println(msg.toString());
        if (msg.text == "Температура")
        {
            bot.sendMessage(getState(0) + " " + String(bot_temp) + " °C", msg.chatID);
        }
        else if (msg.text == "Давление")
        {
            bot.sendMessage(getState(2) + " " + String(bot_pres) + " hPa", msg.chatID);
        }
        else if (msg.text == "Влажность")
        {
            bot.sendMessage(getState(1) + " " + String(bot_humid) + "%", msg.chatID);
        }
        else if (msg.text == "PM2.5")
        {
            bot.sendMessage(getState(4) + " " + String(bot_pm25) + " ug/m3", msg.chatID);
        }
        else if (msg.text == "PM10")
        {
            bot.sendMessage(getState(5) + " " + String(bot_pm10) + " ug/m3", msg.chatID);
        }
        else if (msg.text == "CO2")
        {
            bot.sendMessage(getState(3) + " " + String(bot_co2) + " ppm", msg.chatID);
        }
        else if (msg.text == "Что это")
        {
            for (int i = 0; i < 10; i++)
            {
                if (millis() % 2)
                    bot.sendSticker("CAACAgIAAxkBAAEFKDxivGP8jP1kFasBqN4NEdQEiZ3pngACcwIAAladvQqoc6WsC0Ee0SkE", msg.chatID);
                else
                    bot.sendSticker("CAACAgIAAxkBAAEFKl1ivaqWSFblZGG8tuzswNwnQ0bFqwACHgADwDZPE6FgWy2rAAHeBCkE", msg.chatID);
            }
        }
        
        else if (msg.text == "Alice_ask")
            bot.sendMessage(String(bot_temp) + " " + String(bot_humid) + " " + String(bot_pres) + " " + String(bot_pm25) + " " + String(bot_co2), msg.chatID);

        else if (msg.text == "Всё") //🟩🟨🟧🟥
            bot.sendMessage(getState(0) + "Температура: " + String(bot_temp) + "\n" +
                                getState(1) + "Влажность: " + String(bot_humid) + "\n" +
                                getState(2) + "Давление: " + String(bot_pres) + "\n" +
                                getState(3) + "CO2: " + String(bot_co2) + "\n" +
                                getState(4) + "PM2.5: " + String(bot_pm25) + "\n" +
                                getState(5) + "PM10: " + String(bot_pm10),
                            msg.chatID);

        else
            bot.showMenu("Всё \n Температура \t Давление \n Влажность \t PM2.5 \n PM10 \t  CO2 \n Что это", msg.chatID, 0);
    }
}