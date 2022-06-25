/**
 ******************************************************************************
 * @file    main.cpp
 * @author  Professor Kleber Lima da Silva <kleber.lima@sp.senai.br
 * @version V0.1.0
 * @date    15-Jan-2021
 * @brief   Code for PWM Modbus Interface.
 ******************************************************************************
 */

/* Includes -----------------------------------------------------------------*/
#include <Arduino.h>
#include <ModbusMaster.h>
#include <WiFi.h>

/* Defines ------------------------------------------------------------------*/
#define MOTOR_CHANNEL   0   /* Motor PWM Channel */
#define ENABLE_MODBUS   1   /* 1 to enable Modbus */
#define ENABLE_PWM      1   /* 1 to enable PWM output */
#define CLP_ID          3   /* CLP slave Modbus ID */

const char* SSID = "ESP32_ESTEIRA";
const char* PASS = "esp32Senai";

/* Macros -------------------------------------------------------------------*/
#define PWM_OUTPUT(DUTY_CYCLE)  ( ledcWrite(MOTOR_CHANNEL, (uint32_t)(DUTY_CYCLE * 255) / 100) )

/* Pin numbers --------------------------------------------------------------*/
const int MOTOR_PWM = 15;   /* Motor PWM Pin */
const int LED_STATUS = 4;   /* LED Status Pin */
const int MAX485_DE = 5;    /* MAX485 DE Pin */

/* Private variables --------------------------------------------------------*/
ModbusMaster clpSlave;
WiFiServer server(80);
String header;
String pwmValueString = String(50);
int pwmValue = 50;

/* Modbus RS485 Callbacks ---------------------------------------------------*/
void RS485_TxCallback()
{
    digitalWrite(MAX485_DE, HIGH);
}

void RS485_RxCallback()
{
    digitalWrite(MAX485_DE, LOW);
}

/* Setup --------------------------------------------------------------------*/
void setup()
{
    /* Pins initializations */
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(LED_STATUS, OUTPUT);
    pinMode(MOTOR_PWM, OUTPUT);
    pinMode(MAX485_DE, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);
    digitalWrite(LED_STATUS, HIGH);
    digitalWrite(MOTOR_PWM, LOW);
    digitalWrite(MAX485_DE, LOW);

    /* Serial debug initialization */
    Serial.begin(9600);
    while (!Serial);
    Serial.println("PWM Modbus - ESP32 | SENAI 9.14");

    /* Connect to Wi-Fi network with SSID and password */
    Serial.print("Setting AP (Access Point)…");
    WiFi.softAP(SSID, PASS);
    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);
    server.begin();

    /* Start the Modbus RTU master */
#if ENABLE_MODBUS == 1
    Serial2.begin(9600);
    while (!Serial2);

    /* Modbus slave ID */
    clpSlave.begin(CLP_ID, Serial2);

    /* Interface driver callbacks */
    clpSlave.preTransmission(RS485_TxCallback);
    clpSlave.postTransmission(RS485_RxCallback);
#endif

    /* Configure and start the PWM */
    ledcSetup(MOTOR_CHANNEL, 100, 8);
    ledcAttachPin(MOTOR_PWM, MOTOR_CHANNEL);
    ledcWrite(MOTOR_CHANNEL, 0);
    PWM_OUTPUT(pwmValue);

    /* End of initializations */
    digitalWrite(LED_BUILTIN, LOW);
}

/* Main Loop ----------------------------------------------------------------*/
void loop()
{
    uint8_t modbus_result;
    uint8_t pwm = 0;

    /* Read Modbus PWM register (%MW100) */
#if ENABLE_MODBUS == 1
    digitalWrite(LED_BUILTIN, HIGH);
    modbus_result = clpSlave.readHoldingRegisters(100, 1);
    if (modbus_result == clpSlave.ku8MBSuccess)
    {
        pwm = clpSlave.getResponseBuffer(0);
    }
    else
    {
        Serial.print("Error to read registers: ");
        Serial.println(modbus_result, HEX);
    }
    digitalWrite(LED_BUILTIN, LOW);
#endif

    /* Motor speed control */
#if ENABLE_PWM == 1
    //PWM_OUTPUT(100);//PWM_OUTPUT(pwm);

    Serial.print("PWM: ");
    Serial.println(pwm);
#endif   

    /* STATUS LED blink */
    digitalWrite(LED_STATUS, !digitalRead(LED_STATUS));
    
    /* Main loop interval */
    delay(1000);


    /* WEB-SERVER */
    WiFiClient client = server.available();

    if (client)
    {                                  // If a new client connects,
        Serial.println("New Client."); // print a message out in the serial port
        String currentLine = "";       // make a String to hold incoming data from the client
        while (client.connected())
        { // loop while the client's connected
            if (client.available())
            {                           // if there's bytes to read from the client,
                char c = client.read(); // read a byte, then
                Serial.write(c);        // print it out the serial monitor
                header += c;
                if (c == '\n')
                { // if the byte is a newline character
                    // if the current line is blank, you got two newline characters in a row.
                    // that's the end of the client HTTP request, so send a response:
                    if (currentLine.length() == 0)
                    {
                        // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
                        // and a content-type so the client knows what's coming, then a blank line:
                        client.println("HTTP/1.1 200 OK");
                        client.println("Content-type:text/html");
                        client.println("Connection: close");
                        client.println();

                        if(header.indexOf("GET /-p") >=0 ) pwmValue -= 10;
                        else if(header.indexOf("GET /+p") >=0) pwmValue += 10;
                        else if(header.indexOf("GET /PWM") >=0) pwmValue = (pwmValue > 0 ? 0 : 50);

                        if(pwmValue > 100) pwmValue = 100;
                        else if(pwmValue < 0) pwmValue = 0;

                        PWM_OUTPUT(pwmValue);
                        pwmValueString = String(pwmValue);

                        // Display the HTML web page
                        client.println("<!DOCTYPE html><html>");
                        client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
                        client.println("<link rel=\"icon\" href=\"data:,\">");
                        // CSS to style the on/off buttons
                        client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
                        client.println(".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px;");
                        client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}</style></head>");

                        // Web Page Heading
                        client.println("<body><h1>ESP32 PWM Web Server</h1>");
                        client.println("<br>");
                        
                        client.println("<p><a href=\"/+p\"><button class=\"button\">+</button></a></p>");
                        client.println("<p><a href=\"/PWM\"><button class=\"button\">PWM(" + pwmValueString + ")</button></a></p>");
                        client.println("<p><a href=\"/-p\"><button class=\"button\">-</button></a></p>");
                        
                        client.println("<br>");
                        client.println("</body></html>");

                        // The HTTP response ends with another blank line
                        client.println();
                        // Break out of the while loop
                        break;
                    }
                    else
                    { // if you got a newline, then clear currentLine
                        currentLine = "";
                    }
                }
                else if (c != '\r')
                {                     // if you got anything else but a carriage return character,
                    currentLine += c; // add it to the end of the currentLine
                }
            }
        }
        // Clear the header variable
        header = "";
        // Close the connection
        client.stop();
        Serial.println("Client disconnected.");
        Serial.println("");
    }
}
