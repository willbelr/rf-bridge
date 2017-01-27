#define BRIDGE_ADDRESS 0
#define RFLIGHT_ADDRESS 3
#define PAIRING 0
#define RXTX 1

#include <RHReliableDatagram.h>
#include <RH_NRF24.h>
#include <SPI.h>
#include <EEPROM.h>

const byte LED_RED = A4;
const byte LED_GREEN = A5;
const byte PAIRING_BUTTON = 5;
const unsigned long timeoutInterval = 30*1000L;
const unsigned int pingInterval = 30000;
const unsigned int ledInterval = 600;
const unsigned int ledTimeout = 25;
const unsigned int pairingTimeout = 6000; //10000
unsigned long previousTimeoutInterval;
unsigned long previousPingInterval;
unsigned long previousLedInterval;
unsigned long previousLedTimeout;
unsigned long previousPairingTimeout;
unsigned int ping;
uint8_t clientId = EEPROM.read(1);
uint8_t state = PAIRING;

RH_NRF24 nrf24;
RHReliableDatagram manager(nrf24, BRIDGE_ADDRESS);

int main(void)
{
	init();
	{
		Serial.begin(9600);
		Serial.setTimeout(50);
		delay(100); Serial.println(F("BRIDGE"));

		char buf[5]; sprintf(buf, "%02x", clientId);
		delay(100); Serial.print(F("client=")); Serial.println(buf);
		
		pinMode(LED_RED, OUTPUT); digitalWrite(LED_RED, LOW);
		pinMode(LED_GREEN, OUTPUT); digitalWrite(LED_GREEN, LOW);
	    pinMode(PAIRING_BUTTON, INPUT_PULLUP);
		if (!manager.init()) Serial.println(F("Error: init failed"));
	}
	while(1)
	{
		if (state == PAIRING)
		{
			if (millis() - previousPairingTimeout >= pairingTimeout)
			{
				state = RXTX;
				digitalWrite(LED_RED, LOW);
			}
			else if (millis() - previousLedInterval >= ledInterval)
			{
				digitalWrite(LED_GREEN, LOW);
				digitalWrite(LED_RED, !digitalRead(LED_RED));
	    		previousLedInterval = millis();
			}

		}
		else if (state == RXTX)
		{
			serial_listen();
			//rf_ping();

	    	if (digitalRead(PAIRING_BUTTON) == LOW)
	    	{
	    		previousPairingTimeout = millis();
	    		state = PAIRING;
	    	}
		}

		rf_listen();
		if (millis() - previousLedTimeout >= ledTimeout)
		{
			digitalWrite(LED_GREEN, LOW);
		}
	}
	return 0;
}

void serial_listen()
{
int i = 0;
	if (Serial.available())
	{
		char c;
		char serial_data[RH_NRF24_MAX_MESSAGE_LEN] = {0};
		while (Serial.available() > 0) //Read string
		{
			c = Serial.read();
			while (c != '\n')
			{
				if (c != (char)-1)
				{
					serial_data[i] = c;
					i++;
				}
			c = Serial.read();
			}
		}
		parseCmd(serial_data);
	}
}

void rf_listen()
{
	if (manager.available())
	{
		uint8_t rf_buf[RH_NRF24_MAX_MESSAGE_LEN];
		uint8_t len = sizeof(rf_buf);
		uint8_t from;
		
		if (manager.recvfromAck(rf_buf, &len, &from))
		{
			if (state == RXTX)
			{
				if (from == clientId && strcmp((char*)rf_buf, "ping"))
				{
					ping = 0;
					previousPingInterval = millis();
					char buf[5]; sprintf(buf, "%02x", from); //int to hex
					Serial.print(buf); Serial.print(F(" ")); Serial.println((char*)rf_buf);
					digitalWrite(LED_RED, LOW);
					digitalWrite(LED_GREEN, HIGH);
					previousLedTimeout = millis();
				}

			}
			else if (state == PAIRING)
			{
				clientId = from;
				char buf[5]; sprintf(buf, "%02x", clientId);
				Serial.print(F("client=")); Serial.println(buf);

				EEPROM.write(1, from);
				digitalWrite(LED_RED, LOW);
				state = RXTX;
			}
		}
	}
}

void rf_sendString(char str[RH_NRF24_MAX_MESSAGE_LEN])
{
    uint8_t rf_data[RH_NRF24_MAX_MESSAGE_LEN];
    strcpy((char*)rf_data, str); 
    digitalWrite(LED_RED, HIGH);
    manager.sendtoWait(rf_data, sizeof(rf_data), clientId);
    manager.sendtoWait(rf_data, sizeof(rf_data), RFLIGHT_ADDRESS);
    digitalWrite(LED_RED, LOW);
	if (strcmp((char*)rf_data, "ping")) { Serial.print(F("00 ")); Serial.println((char*)rf_data); }
}
/*
void rf_ping()
{
	if (millis() - previousPingInterval >= pingInterval)
	{
		previousPingInterval = millis();
		ping++;
		rf_sendString("ping");
		digitalWrite(LED_RED, HIGH); delay(50); digitalWrite(LED_RED, LOW);
	}
}
*/

void parseCmd(char cmd[RH_NRF24_MAX_MESSAGE_LEN])
{
	char paramBuf[RH_NRF24_MAX_MESSAGE_LEN];
	char* param = strtok(cmd, " ");
	strcpy(paramBuf, param);

	if (!strcmp(param, paramBuf)) { param = strtok(NULL, " "); }

	if (!strcmp(paramBuf, "pairing"))
	{
		Serial.println(F("Pairing..."));
		state = PAIRING;
		previousPairingTimeout = millis();
	}

	else if (!strcmp(paramBuf, "setclient"))
	{
		if (!strcmp(param, "0") || int(param) == 0)
		{
			parseCmd("pairing");
		}
		else
		{
			state = RXTX;
			clientId = atoi(param);
			char buf[5]; sprintf(buf, "%02x", clientId);
			Serial.print(F("client=")); Serial.println(buf);
		}
	}

	else { rf_sendString(cmd); } //Transmit string to controller	
}
