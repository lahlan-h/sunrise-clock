#pragma once

class Peripheral{
  public: 
    virtual ~Peripheral() {}
    virtual void begin() = 0;
    virtual void update() {}
};