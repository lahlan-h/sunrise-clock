#pragma once

class RotaryEncode{
  public:
    RotaryEncode(uint8_t clkPin, uint8_t dtPin);
    
    void begin(); // Set pins and attach interrupts
    void update(); // Process steps (call every loop)

    void reset();
    uint8_t getCount();
  

  private:
    uint8_t clkPin, dtPin;

    volatile uint8_t steps = 0;
    volatile uint8_t state = 0;

    int  count = 0;
    int  delta = 0;

    const int8_t table[16] = {
      0, -1,  1,  0,
      1,  0,  0, -1,
     -1,  0,  0,  1,
      0,  1, -1,  0,
    }

    void IRAM_ATTR readEncoder();
  
};
