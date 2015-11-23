#include "rs485.h"
//#define DEBUG1

rs485::rs485 (uint8_t serial_dir) {
  ser_dir = serial_dir;
  pinMode(serial_dir, OUTPUT);    
  
  digitalWrite(serial_dir, RS485Receive);  // Init Transceiver   
  
  // Start the serial port, to another device
  Serial.begin(RS485_BAUD);   // set the data rate 
}

int rs485::send(uint8_t* data, uint8_t len) {

  digitalWrite(ser_dir, RS485Transmit);  // Enable RS485 Transmit   

  //delay(10);
  Serial.write(FLAG);
  for (uint8_t sByte = 0; sByte < len; sByte ++)  {
      if (data[sByte] == FLAG || data[sByte] == ESCAPE)
      {
          Serial.write(ESCAPE);            // Add escape byte
      }
      Serial.write(data[sByte]);          // Send byte to bus
  }
  Serial.write(FLAG);
  digitalWrite(ser_dir, RS485Receive);  // Disable RS485 Transmit       

  //delay(20);
}

int rs485::send(uint8_t data) {
  send(&data);
}

char rs485::receive() {
  char byteReceived = Serial.read();    // Read received byte
  return byteReceived;
}

int rs485::available() {
  Serial.available();
}

boolean rs485::get_packet(uint8_t dev_addr, uint8_t* packet) {
  for (int i = available(); i > 0; i--)  {
    byte byteReceived = receive();    // Read received byte
    if(byteReceived == FLAG)  {
      packetIndex = 0;
      //TODO: verify packet integrity
      //TODO: check addresses
      uint8_t length = packet[0];
      uint8_t src_address = packet[1];
      uint8_t dst_address = packet[2];
      uint8_t function = packet[3];
  
      #ifdef DEBUG1
      //Display packet info
      Serial.print("length: ");
      Serial.println(length);
      Serial.print("source address: ");
      Serial.println(src_address);
      Serial.print("destination address: ");
      Serial.println(dst_address);+
      Serial.print("function: ");
      Serial.println(function);
      Serial.print("data: ");
      for(uint8_t i = 4; i < length; i++)  {
        Serial.print(packet[i]);
        Serial.print(',');
      }
      Serial.println(' ');
      #endif
      Serial.print(dev_addr);
      Serial.print(" : ");
      Serial.println(dst_address);
      if (dst_address == dev_addr || dst_address == ADDR_BROADCAST)  {
        Serial.println("address match");
        //packet = lastPacket;
        return true;
      }
    }
    else  {
      packet[packetIndex++] = byteReceived; 
    }
  return false;
  }
}

void rs485::send_packet(uint8_t source_addr, uint8_t dest_addr, uint8_t function, uint8_t* payload, uint8_t len)  {
  uint8_t pos = 0;
  uint8_t packet_len = len + 6;
  uint8_t packet[packet_len];
  uint16_t CRC = compute_CRC(packet, packet_len);
  packet[pos++] = packet_len;
  packet[pos++] = source_addr;
  packet[pos++] = dest_addr;
  packet[pos++] = function;

  for (uint8_t i = 0; i < len; i++)  {
    packet[pos++] =  payload[i];
  }
  packet[pos++] = CRC >> 8;
  packet[pos++] = CRC & 0xFF;
  this->send(packet, packet_len);
}

uint16_t rs485::compute_CRC(uint8_t* data, uint8_t len)
{
  //TODO: implement 16 bit CRC
  return 0xFFFF;
}

