#include "Module.h"

Module::Module(int rx, int tx) {
  _cs = -1;
  _rx = rx;
  _tx = tx;
  _int0 = -1;
  _int1 = -1;

  ModuleSerial = new SoftwareSerial(_rx, _tx);
}

Module::Module(int cs, int int0, int int1, SPIClass& spi, SPISettings spiSettings) {
  _cs = cs;
  _rx = -1;
  _tx = -1;
  _int0 = int0;
  _int1 = int1;
  _spi = &spi;
  _spiSettings = spiSettings;
}

Module::Module(int cs, int int0, int int1, int rx, int tx, SPIClass& spi, SPISettings spiSettings) {
  _cs = cs;
  _rx = rx;
  _tx = tx;
  _int0 = int0;
  _int1 = int1;
  _spi = &spi;
  _spiSettings = spiSettings;

  ModuleSerial = new SoftwareSerial(_rx, _tx);
}

Module::Module(int cs, int int0, int int1, int int2, SPIClass& spi, SPISettings spiSettings) {
  _cs = cs;
  _rx = int2;
  _tx = -1;
  _int0 = int0;
  _int1 = int1;
  _spi = &spi;
  _spiSettings = spiSettings;
}

void Module::init(uint8_t interface, uint8_t gpio) {
  // select interface
  switch(interface) {
    case USE_SPI:
      pinMode(_cs, OUTPUT);
      digitalWrite(_cs, HIGH);
      _spi->begin();
      break;
    case USE_UART:
      ModuleSerial->begin(baudrate);
      break;
    case USE_I2C:
      break;
  }

  // select GPIO
  switch(gpio) {
    case INT_NONE:
      break;
    case INT_0:
      pinMode(_int0, INPUT);
      break;
    case INT_1:
      pinMode(_int1, INPUT);
      break;
    case INT_BOTH:
      pinMode(_int0, INPUT);
      pinMode(_int1, INPUT);
      break;
  }
}

void Module::term() {
  // stop SPI
  _spi->end();
}

void Module::ATemptyBuffer() {
  while(ModuleSerial->available() > 0) {
    ModuleSerial->read();
  }
}

bool Module::ATsendCommand(const char* cmd) {
  ATemptyBuffer();
  ModuleSerial->print(cmd);
  ModuleSerial->print(AtLineFeed);
  return(ATgetResponse());
}

bool Module::ATsendData(uint8_t* data, uint32_t len) {
  ATemptyBuffer();
  for(uint32_t i = 0; i < len; i++) {
    ModuleSerial->write(data[i]);
  }

  ModuleSerial->print(AtLineFeed);
  return(ATgetResponse());
}

bool Module::ATgetResponse() {
  String data = "";
  uint32_t start = millis();
  while (millis() - start < _ATtimeout) {
    while(ModuleSerial->available() > 0) {
      char c = ModuleSerial->read();
      DEBUG_PRINT(c);
      data += c;
    }

    if(data.indexOf("OK") != -1) {
      DEBUG_PRINTLN();
      return(true);
    } else if (data.indexOf("ERROR") != -1) {
      DEBUG_PRINTLN();
      return(false);
    }

  }
  DEBUG_PRINTLN();
  return(false);
}

int16_t Module::SPIgetRegValue(uint8_t reg, uint8_t msb, uint8_t lsb) {
  if((msb > 7) || (lsb > 7) || (lsb > msb)) {
    return(ERR_INVALID_BIT_RANGE);
  }

  uint8_t rawValue = SPIreadRegister(reg);
  uint8_t maskedValue = rawValue & ((0b11111111 << lsb) & (0b11111111 >> (7 - msb)));
  return(maskedValue);
}

int16_t Module::SPIsetRegValue(uint8_t reg, uint8_t value, uint8_t msb, uint8_t lsb, uint8_t checkInterval) {
  if((msb > 7) || (lsb > 7) || (lsb > msb)) {
    return(ERR_INVALID_BIT_RANGE);
  }

  uint8_t currentValue = SPIreadRegister(reg);
  uint8_t mask = ~((0b11111111 << (msb + 1)) | (0b11111111 >> (8 - lsb)));
  uint8_t newValue = (currentValue & ~mask) | (value & mask);
  SPIwriteRegister(reg, newValue);

  // check register value each millisecond until check interval is reached
  // some registers need a bit of time to process the change (e.g. SX127X_REG_OP_MODE)
  uint32_t start = micros();
  uint8_t readValue = 0;
  while(micros() - start < (checkInterval * 1000)) {
    readValue = SPIreadRegister(reg);
    if(readValue == newValue) {
      // check passed, we can stop the loop
      return(ERR_NONE);
    }
  }

  // check failed, print debug info
  DEBUG_PRINTLN();
  DEBUG_PRINT(F("address:\t0x"));
  DEBUG_PRINTLN(reg, HEX);
  DEBUG_PRINT(F("bits:\t\t"));
  DEBUG_PRINT(msb);
  DEBUG_PRINT(' ');
  DEBUG_PRINTLN(lsb);
  DEBUG_PRINT(F("value:\t\t0b"));
  DEBUG_PRINTLN(value, BIN);
  DEBUG_PRINT(F("current:\t0b"));
  DEBUG_PRINTLN(currentValue, BIN);
  DEBUG_PRINT(F("mask:\t\t0b"));
  DEBUG_PRINTLN(mask, BIN);
  DEBUG_PRINT(F("new:\t\t0b"));
  DEBUG_PRINTLN(newValue, BIN);
  DEBUG_PRINT(F("read:\t\t0b"));
  DEBUG_PRINTLN(readValue, BIN);
  DEBUG_PRINTLN();

  return(ERR_SPI_WRITE_FAILED);
}

void Module::SPIreadRegisterBurst(uint8_t reg, uint8_t numBytes, uint8_t* inBytes) {
  SPItransfer(SPIreadCommand, reg, NULL, inBytes, numBytes);
}

uint8_t Module::SPIreadRegister(uint8_t reg) {
  uint8_t resp;
  SPItransfer(SPIreadCommand, reg, NULL, &resp, 1);
  return(resp);
}

void Module::SPIwriteRegisterBurst(uint8_t reg, uint8_t* data, uint8_t numBytes) {
  SPItransfer(SPIwriteCommand, reg, data, NULL, numBytes);
}

void Module::SPIwriteRegister(uint8_t reg, uint8_t data) {
  SPItransfer(SPIwriteCommand, reg, &data, NULL, 1);
}

void Module::SPItransfer(uint8_t cmd, uint8_t reg, uint8_t* dataOut, uint8_t* dataIn, uint8_t numBytes) {
  // start SPI transaction
  _spi->beginTransaction(_spiSettings);

  // pull CS low
  digitalWrite(_cs, LOW);

  // send SPI register address with access command
  _spi->transfer(reg | cmd);

  // send data or get response
  if(cmd == SPIwriteCommand) {
    for(size_t n = 0; n < numBytes; n++) {
      _spi->transfer(dataOut[n]);
    }
  } else if (cmd == SPIreadCommand) {
    for(size_t n = 0; n < numBytes; n++) {
      dataIn[n] = _spi->transfer(0x00);
    }
  }

  // release CS
  digitalWrite(_cs, HIGH);

  // end SPI transaction
  _spi->endTransaction();
}
