#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <EEPROM.h>

class Config
{
  public:
    bool IsFirstRun();
    void SaveAddress(byte address);
    byte GetAddress();

  private:
  
};

#endif