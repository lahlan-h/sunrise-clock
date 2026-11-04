#pragma once

class Connections {
  public:
    virtual ~Connections() {}
    virtual void begin() = 0;
    virtual void update() {}
};
