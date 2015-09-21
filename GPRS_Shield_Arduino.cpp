/*
 * GPRS_Shield_Arduino.cpp
 * A library for SeeedStudio seeeduino GPRS shield 
 *  
 * Copyright (c) 2015 seeed technology inc.
 * Website    : www.seeed.cc
 * Author     : lawliet zou
 * Create Time: April 2015
 * Change Log :
 *
 * The MIT License (MIT)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdio.h>
#include "GPRS_Shield_Arduino.h"
#include "SoftwareSerial.h"

GPRS* GPRS::inst;

GPRS::GPRS(uint32_t baudRate)
{
    inst = this;
    sim900_init(&SERIAL_PORT_HARDWARE);
    SERIAL_PORT_HARDWARE.begin(baudRate);
}

GPRS::GPRS(uint8_t tx, uint8_t rx, uint32_t baudRate)
{
    inst = this;
    SoftwareSerial* gprsserial = new SoftwareSerial(tx, rx);
    stream = gprsserial ;
    sim900_init(stream);
    gprsserial ->begin(baudRate);    
}

bool GPRS::init(void)
{
    if(!sim900_check_with_cmd("AT\r\n","OK\r\n",CMD)){
		return false;
    }
    
    if(!sim900_check_with_cmd("AT+CFUN=1\r\n","OK\r\n",CMD)){
        return false;
    }
    
    if(!checkSIMStatus()) {
		return false;
    }
    delay(5000);
    return true;
}

bool GPRS::checkPowerUp(void)
{
  return sim900_check_with_cmd("AT\r\n","OK\r\n",CMD);
}

void GPRS::powerUpDown()
{
  pinMode(6, OUTPUT);
  if(digitalRead(7)!=1)
  {
    digitalWrite(6, HIGH);
    delay(3000);
  }
  digitalWrite(6, LOW);
  delay(3000);
}

void GPRS::powerOff()
{
  pinMode(6, OUTPUT);
  if(digitalRead(7)==1)
  {
    digitalWrite(6, HIGH);
    delay(3000);
  }
  digitalWrite(6, LOW);
  delay(3000);
}   
    
bool GPRS::checkSIMStatus(void)
{
    char gprsBuffer[32];
    int count = 0;
    sim900_clean_buffer(gprsBuffer,32);
    while(count < 3) {
        sim900_send_cmd("AT+CPIN?\r\n");
        sim900_read_buffer(gprsBuffer,32,DEFAULT_TIMEOUT);
        if((NULL != strstr(gprsBuffer,"+CPIN: READY"))) {
            break;
        }
        count++;
        delay(300);
    }
    if(count == 3) {
        return false;
    }
    return true;
}

//Here is where we ask for APN configuration, with F() so we can save MEMORY
//bool GPRS::join(const __FlashStringHelper *apn, const __FlashStringHelper *userName, const __FlashStringHelper *passWord)
bool GPRS::join(char* apn, char* userName, char* passWord, int timeout)
{
	  byte i;
    char *p, *s;
    char ipAddr[32];
/*    if(!sim900_check_with_cmd("AT+CIPSHUT\r\n","SHUT OK\r\n", CMD)) {
      Serial.write("Error = 1\r\n");
    return false;
    }
    delay(1000);
*/
    sim900_send_cmd("AT+CIPSHUT\r\n");
    delay(500);
    //Select multiple connection
    //sim900_check_with_cmd("AT+CIPMUX=1\r\n","OK",DEFAULT_TIMEOUT,CMD);

    //set APN. OLD VERSION
    //snprintf(cmd,sizeof(cmd),"AT+CSTT=\"%s\",\"%s\",\"%s\"\r\n",_apn,_userName,_passWord);
    //sim900_check_with_cmd(cmd, "OK\r\n", DEFAULT_TIMEOUT,CMD);
    sim900_send_cmd("AT+CSTT=\"");
    sim900_send_cmd(apn);
    sim900_send_cmd("\",\"");
    sim900_send_cmd(userName);
    sim900_send_cmd("\",\"");
    sim900_send_cmd(passWord);
    sim900_send_cmd("\"\r\n");
    delay(500);
    //Brings up wireless connection

    sim900_send_cmd("AT+CIICR\r\n");
    delay(4000);
    sim900_wait_for_resp("OK\r\n", CMD);
    delay(500);
//    sim900_check_with_cmd("AT+CIICR\r\n","OK\r\n", CMD);


    //Get local IP address
    sim900_send_cmd("AT+CIFSR\r\n");
    delay(500);
    sim900_clean_buffer(ipAddr,32);
    sim900_read_buffer(ipAddr,32,DEFAULT_TIMEOUT);

	//Response:
	//AT+CIFSR\r\n       -->  8 + 2
	//\r\n				 -->  0 + 2
	//10.160.57.120\r\n  --> 15 + 2 (max)   : TOTAL: 29 
	//Response error:
	//AT+CIFSR\r\n       
	//\r\n				 
	//ERROR\r\n
    if (NULL != strstr(ipAddr,"ERROR")) {
      Serial.write("Error = 2\r\n");
		return false;
	}
    s = ipAddr + 12;
    p = strstr((char *)(s),"\r\n"); //p is last character \r\n
    if (NULL != s) {
        i = 0;
        while (s < p) {
            ip_string[i++] = *(s++);
        }
        ip_string[i] = '\0';  
		Serial.println(ip_string);
    }
    _ip = str_to_ip(ip_string);
    if(_ip != 0) {
       
        return true;
    }
    Serial.write("Error = 3\r\n");
    return false;
} 

void GPRS::disconnect()
{
    sim900_send_cmd("AT+CIPSHUT\r\n");
}

bool GPRS::connect(Protocol ptl,const char * host, int port, int timeout)
{
    //char cmd[64];
	char num[4];
    char resp[96];

    //sim900_clean_buffer(cmd,64);
    if(ptl == TCP) {
		sim900_send_cmd("AT+CIPSTART=\"TCP\",\"");
		sim900_send_cmd(host);
		sim900_send_cmd("\",");
		itoa(port, num, 10);
		sim900_send_cmd(num);
		sim900_send_cmd("\r\n");
//        sprintf(cmd, "AT+CIPSTART=\"TCP\",\"%s\",%d\r\n",host, port);
    } else if(ptl == UDP) {
		sim900_send_cmd("AT+CIPSTART=\"UDP\",\"");
		sim900_send_cmd(host);
		sim900_send_cmd("\",");
		itoa(port, num, 10);
		sim900_send_cmd(num);
		sim900_send_cmd("\r\n");

	//        sprintf(cmd, "AT+CIPSTART=\"UDP\",\"%s\",%d\r\n",host, port);
    } else {
        return false;
    }
    
    delay(2000);
    //sim900_send_cmd(cmd);
    sim900_read_buffer(resp,96,timeout);

//Serial.print("Connect resp: "); Serial.println(resp);    
    if(NULL != strstr(resp,"CONNECT")) { //ALREADY CONNECT or CONNECT OK
        return true;
    }
    return false;
}

//Overload with F() macro to SAVE memory
bool GPRS::connect(Protocol ptl,const __FlashStringHelper *host, const __FlashStringHelper *port, int timeout)
{
    //char cmd[64];
    char resp[96];

    //sim900_clean_buffer(cmd,64);
    if(ptl == TCP) {
        sim900_send_cmd(F("AT+CIPSTART=\"TCP\",\""));   //%s\",%d\r\n",host, port);
    } else if(ptl == UDP) {
        sim900_send_cmd(F("AT+CIPSTART=\"UDP\",\""));   //%s\",%d\r\n",host, port);
    } else {
        return false;
    }
    sim900_send_cmd(host);
    sim900_send_cmd(F("\","));
    sim900_send_cmd(port);
    sim900_send_cmd(F("\r\n"));
//Serial.print("Connect: "); Serial.println(cmd);
    sim900_read_buffer(resp, 96, timeout);
//Serial.print("Connect resp: "); Serial.println(resp);    
    if(NULL != strstr(resp,"CONNECT")) { //ALREADY CONNECT or CONNECT OK
        return true;
    }
    return false;
}

bool GPRS::is_connected(void)
{
    char resp[96];
    sim900_send_cmd("AT+CIPSTATUS\r\n");
    sim900_read_buffer(resp,sizeof(resp),DEFAULT_TIMEOUT);
    if(NULL != strstr(resp,"CONNECTED")) {
        //+CIPSTATUS: 1,0,"TCP","216.52.233.120","80","CONNECTED"
        return true;
    } else {
        //+CIPSTATUS: 1,0,"TCP","216.52.233.120","80","CLOSED"
        //+CIPSTATUS: 0,,"","","","INITIAL"
        return false;
    }
}

bool GPRS::close()
{
    // if not connected, return
    if (!is_connected()) {
        return true;
    }
    return sim900_check_with_cmd("AT+CIPCLOSE\r\n", "CLOSE OK\r\n", CMD);
}

int GPRS::readable(void)
{
    return sim900_check_readable();
}

int GPRS::wait_readable(int wait_time)
{
    return sim900_wait_readable(wait_time);
}

int GPRS::wait_writeable(int req_size)
{
    return req_size+1;
}

int GPRS::send(const char * str, int len)
{
    //char cmd[32];
	char num[4];
    if(len > 0){
        //snprintf(cmd,sizeof(cmd),"AT+CIPSEND=%d\r\n",len);
		//sprintf(cmd,"AT+CIPSEND=%d\r\n",len);
		sim900_send_cmd("AT+CIPSEND=");
		itoa(len, num, 10);
		sim900_send_cmd(num);
		if(!sim900_check_with_cmd("\r\n",">",CMD)) {
        //if(!sim900_check_with_cmd(cmd,">",CMD)) {
            return 0;
        }
        /*if(0 != sim900_check_with_cmd(str,"SEND OK\r\n", DEFAULT_TIMEOUT * 10 ,DATA)) {
            return 0;
        }*/
        delay(500);
        sim900_send_cmd(str);
        delay(500);
        sim900_send_End_Mark();
        if(!sim900_wait_for_resp("SEND OK\r\n", DATA, DEFAULT_TIMEOUT * 10, DEFAULT_INTERCHAR_TIMEOUT * 10)) {
            return 0;
        }        
    }
    return len;
}

int GPRS::send(const char * str) {
    //char cmd[32];
  int len=strlen(str);
  char num[4];
    if(len > 0){
        //snprintf(cmd,sizeof(cmd),"AT+CIPSEND=%d\r\n",len);
    //sprintf(cmd,"AT+CIPSEND=%d\r\n",len);
    sim900_send_cmd("AT+CIPSEND=");
    itoa(len, num, 10);
    sim900_send_cmd(num);
    if(!sim900_check_with_cmd("\r\n",">",CMD)) {
        //if(!sim900_check_with_cmd(cmd,">",CMD)) {
            return 0;
        }
        /*if(0 != sim900_check_with_cmd(str,"SEND OK\r\n", DEFAULT_TIMEOUT * 10 ,DATA)) {
            return 0;
        }*/
        delay(500);
        sim900_send_cmd(str);
        delay(500);
        sim900_send_End_Mark();
        if(!sim900_wait_for_resp("SEND OK\r\n", DATA, DEFAULT_TIMEOUT * 10, DEFAULT_INTERCHAR_TIMEOUT * 10)) {
            return 0;
        }        
    }
    return len;
}   

int GPRS::recv(char* buf, int len)
{
    sim900_clean_buffer(buf,len);
    sim900_read_buffer(buf,len);   //Ya he llamado a la funcion con la longitud del buffer - 1 y luego le estoy añadiendo el 0
    return strlen(buf);
}

uint32_t GPRS::str_to_ip(const char* str)
{
    uint32_t ip = 0;
    char* p = (char*)str;
    for(int i = 0; i < 4; i++) {
        ip |= atoi(p);
        p = strchr(p, '.');
        if (p == NULL) {
            break;
        }
        ip <<= 8;
        p++;
    }
    return ip;
}

char* GPRS::getIPAddress()
{
    //I have already a buffer with ip_string: snprintf(ip_string, sizeof(ip_string), "%d.%d.%d.%d", (_ip>>24)&0xff,(_ip>>16)&0xff,(_ip>>8)&0xff,_ip&0xff); 
    return ip_string;
}

unsigned long GPRS::getIPnumber()
{
    return _ip;
}
/* NOT USED bool GPRS::gethostbyname(const char* host, uint32_t* ip)
{
    uint32_t addr = str_to_ip(host);
    char buf[17];
    //snprintf(buf, sizeof(buf), "%d.%d.%d.%d", (addr>>24)&0xff, (addr>>16)&0xff, (addr>>8)&0xff, addr&0xff);
    if (strcmp(buf, host) == 0) {
        *ip = addr;
        return true;
    }
    return false;
}
*/
