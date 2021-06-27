#include "RF69.h"
#if !defined(RADIOLIB_EXCLUDE_RF69)

RF69::RF69(Module* module) : PhysicalLayer(RF69_FREQUENCY_STEP_SIZE, RF69_MAX_PACKET_LENGTH)  {
  _mod = module;
}

int16_t RF69::begin(float freq, float br, float freqDev, float rxBw, int8_t power, uint8_t preambleLen) {
  // set module properties
  _mod->init(RADIOLIB_USE_SPI);
  Module::pinMode(_mod->getIrq(), INPUT);

  // try to find the RF69 chip
  uint8_t i = 0;
  bool flagFound = false;
  while((i < 10) && !flagFound) {
    // reset the module
    reset();

    // check version register
    int16_t version = getChipVersion();
    if(version == RF69_CHIP_VERSION) {
      flagFound = true;
    } else {
      #ifdef RADIOLIB_DEBUG
        RADIOLIB_DEBUG_PRINT(F("RF69 not found! ("));
        RADIOLIB_DEBUG_PRINT(i + 1);
        RADIOLIB_DEBUG_PRINT(F(" of 10 tries) RF69_REG_VERSION == "));

        char buffHex[7];
        sprintf(buffHex, "0x%04X", version);
        RADIOLIB_DEBUG_PRINT(buffHex);
        RADIOLIB_DEBUG_PRINT(F(", expected 0x0024"));
        RADIOLIB_DEBUG_PRINTLN();
      #endif
      Module::delay(10);
      i++;
    }
  }

  if(!flagFound) {
    RADIOLIB_DEBUG_PRINTLN(F("No RF69 found!"));
    _mod->term(RADIOLIB_USE_SPI);
    return(ERR_CHIP_NOT_FOUND);
  } else {
    RADIOLIB_DEBUG_PRINTLN(F("M\tRF69"));
  }

  // configure settings not accessible by API
  int16_t state = config();
  RADIOLIB_ASSERT(state);

  // configure publicly accessible settings
  state = setFrequency(freq);
  RADIOLIB_ASSERT(state);

  // configure bitrate
  _rxBw = rxBw;
  state = setBitRate(br);
  RADIOLIB_ASSERT(state);

  // configure default RX bandwidth
  state = setRxBandwidth(rxBw);
  RADIOLIB_ASSERT(state);

  // configure default frequency deviation
  state = setFrequencyDeviation(freqDev);
  RADIOLIB_ASSERT(state);

  // configure default TX output power
  state = setOutputPower(power);
  RADIOLIB_ASSERT(state);

  // configure default preamble length
  state = setPreambleLength(preambleLen);
  RADIOLIB_ASSERT(state);

  // set default packet length mode
  state = variablePacketLengthMode();
  RADIOLIB_ASSERT(state);

  // set default sync word
  uint8_t syncWord[] = RF69_DEFAULT_SW;
  state = setSyncWord(syncWord, sizeof(syncWord));
  RADIOLIB_ASSERT(state);

  // set default data shaping
  state = setDataShaping(RADIOLIB_SHAPING_NONE);
  RADIOLIB_ASSERT(state);

  // set default encoding
  state = setEncoding(RADIOLIB_ENCODING_NRZ);
  RADIOLIB_ASSERT(state);

  // set CRC on by default
  state = setCrcFiltering(true);
  RADIOLIB_ASSERT(state);

  return(state);
}

void RF69::reset() {
  Module::pinMode(_mod->getRst(), OUTPUT);
  Module::digitalWrite(_mod->getRst(), HIGH);
  Module::delay(1);
  Module::digitalWrite(_mod->getRst(), LOW);
  Module::delay(10);
}

int16_t RF69::transmit(uint8_t* data, size_t len, uint8_t addr) {
  // calculate timeout (5ms + 500 % of expected time-on-air)
  uint32_t timeout = 5000000 + (uint32_t)((((float)(len * 8)) / (_br * 1000.0)) * 5000000.0);

  // start transmission
  int16_t state = startTransmit(data, len, addr);
  RADIOLIB_ASSERT(state);

  // wait for transmission end or timeout
  uint32_t start = Module::micros();
  while(!Module::digitalRead(_mod->getIrq())) {
    Module::yield();

    if(Module::micros() - start > timeout) {
      standby();
      clearIRQFlags();
      return(ERR_TX_TIMEOUT);
    }
  }

  // set mode to standby
  standby();

  // clear interrupt flags
  clearIRQFlags();

  return(ERR_NONE);
}

int16_t RF69::receive(uint8_t* data, size_t len) {
  // calculate timeout (500 ms + 400 full 64-byte packets at current bit rate)
  uint32_t timeout = 500000 + (1.0/(_br*1000.0))*(RF69_MAX_PACKET_LENGTH*400.0);

  // start reception
  int16_t state = startReceive();
  RADIOLIB_ASSERT(state);

  // wait for packet reception or timeout
  uint32_t start = Module::micros();
  while(!Module::digitalRead(_mod->getIrq())) {
    Module::yield();

    if(Module::micros() - start > timeout) {
      standby();
      clearIRQFlags();
      return(ERR_RX_TIMEOUT);
    }
  }

  // read packet data
  return(readData(data, len));
}

int16_t RF69::sleep() {
  // set RF switch (if present)
  _mod->setRfSwitchState(LOW, LOW);

  // set module to sleep
  return(setMode(RF69_SLEEP));
}

int16_t RF69::standby() {
  // set RF switch (if present)
  _mod->setRfSwitchState(LOW, LOW);

  // set module to standby
  return(setMode(RF69_STANDBY));
}

int16_t RF69::transmitDirect(uint32_t frf) {
  // set RF switch (if present)
  _mod->setRfSwitchState(LOW, HIGH);

  // user requested to start transmitting immediately (required for RTTY)
  if(frf != 0) {
    _mod->SPIwriteRegister(RF69_REG_FRF_MSB, (frf & 0xFF0000) >> 16);
    _mod->SPIwriteRegister(RF69_REG_FRF_MID, (frf & 0x00FF00) >> 8);
    _mod->SPIwriteRegister(RF69_REG_FRF_LSB, frf & 0x0000FF);

    return(setMode(RF69_TX));
  }

  // activate direct mode
  int16_t state = directMode();
  RADIOLIB_ASSERT(state);

  // start transmitting
  return(setMode(RF69_TX));
}

int16_t RF69::receiveDirect() {
  // set RF switch (if present)
  _mod->setRfSwitchState(HIGH, LOW);

  // activate direct mode
  int16_t state = directMode();
  RADIOLIB_ASSERT(state);

  // start receiving
  return(setMode(RF69_RX));
}

int16_t RF69::directMode() {
  // set mode to standby
  int16_t state = setMode(RF69_STANDBY);
  RADIOLIB_ASSERT(state);

  // set DIO mapping
  state = _mod->SPIsetRegValue(RF69_REG_DIO_MAPPING_1, RF69_DIO1_CONT_DCLK | RF69_DIO2_CONT_DATA, 5, 2);
  RADIOLIB_ASSERT(state);

  // set continuous mode
  return(_mod->SPIsetRegValue(RF69_REG_DATA_MODUL, RF69_CONTINUOUS_MODE_WITH_SYNC, 6, 5));
}

int16_t RF69::packetMode() {
  return(_mod->SPIsetRegValue(RF69_REG_DATA_MODUL, RF69_PACKET_MODE, 6, 5));
}

void RF69::setAESKey(uint8_t* key) {
  _mod->SPIwriteRegisterBurst(RF69_REG_AES_KEY_1, key, 16);
}

int16_t RF69::enableAES() {
  return(_mod->SPIsetRegValue(RF69_REG_PACKET_CONFIG_2, RF69_AES_ON, 0, 0));
}

int16_t RF69::disableAES() {
  return(_mod->SPIsetRegValue(RF69_REG_PACKET_CONFIG_2, RF69_AES_OFF, 0, 0));
}

int16_t RF69::startReceive() {
  // set mode to standby
  int16_t state = setMode(RF69_STANDBY);
  RADIOLIB_ASSERT(state);

  // set RX timeouts and DIO pin mapping
  state = _mod->SPIsetRegValue(RF69_REG_DIO_MAPPING_1, RF69_DIO0_PACK_PAYLOAD_READY, 7, 4);
  state |= _mod->SPIsetRegValue(RF69_REG_RX_TIMEOUT_1, RF69_TIMEOUT_RX_START);
  state |= _mod->SPIsetRegValue(RF69_REG_RX_TIMEOUT_2, RF69_TIMEOUT_RSSI_THRESH);
  RADIOLIB_ASSERT(state);

  // clear interrupt flags
  clearIRQFlags();

  // set RF switch (if present)
  _mod->setRfSwitchState(HIGH, LOW);

  // set mode to receive
  state = _mod->SPIsetRegValue(RF69_REG_OCP, RF69_OCP_ON | RF69_OCP_TRIM);
  state |= _mod->SPIsetRegValue(RF69_REG_TEST_PA1, RF69_PA1_NORMAL);
  state |= _mod->SPIsetRegValue(RF69_REG_TEST_PA2, RF69_PA2_NORMAL);
  RADIOLIB_ASSERT(state);

  state = setMode(RF69_RX);

  return(state);
}

void RF69::setDio0Action(void (*func)(void)) {
  Module::attachInterrupt(RADIOLIB_DIGITAL_PIN_TO_INTERRUPT(_mod->getIrq()), func, RISING);
}

void RF69::clearDio0Action() {
  Module::detachInterrupt(RADIOLIB_DIGITAL_PIN_TO_INTERRUPT(_mod->getIrq()));
}

void RF69::setDio1Action(void (*func)(void)) {
  if(_mod->getGpio() == RADIOLIB_NC) {
    return;
  }
  Module::pinMode(_mod->getGpio(), INPUT);
  Module::attachInterrupt(RADIOLIB_DIGITAL_PIN_TO_INTERRUPT(_mod->getGpio()), func, RISING);
}

void RF69::clearDio1Action() {
  if(_mod->getGpio() == RADIOLIB_NC) {
    return;
  }
  Module::detachInterrupt(RADIOLIB_DIGITAL_PIN_TO_INTERRUPT(_mod->getGpio()));
}

int16_t RF69::startTransmit(uint8_t* data, size_t len, uint8_t addr) {
  // check packet length
  if(len > RF69_MAX_PACKET_LENGTH) {
    return(ERR_PACKET_TOO_LONG);
  }

  // set mode to standby
  int16_t state = setMode(RF69_STANDBY);
  RADIOLIB_ASSERT(state);

  // set DIO pin mapping
  state = _mod->SPIsetRegValue(RF69_REG_DIO_MAPPING_1, RF69_DIO0_PACK_PACKET_SENT, 7, 6);
  RADIOLIB_ASSERT(state);

  // clear interrupt flags
  clearIRQFlags();

  // optionally write packet length
  if (_packetLengthConfig == RF69_PACKET_FORMAT_VARIABLE) {
    _mod->SPIwriteRegister(RF69_REG_FIFO, len);
  }

  // check address filtering
  uint8_t filter = _mod->SPIgetRegValue(RF69_REG_PACKET_CONFIG_1, 2, 1);
  if((filter == RF69_ADDRESS_FILTERING_NODE) || (filter == RF69_ADDRESS_FILTERING_NODE_BROADCAST)) {
    _mod->SPIwriteRegister(RF69_REG_FIFO, addr);
  }

  // write packet to FIFO
  _mod->SPIwriteRegisterBurst(RF69_REG_FIFO, data, len);

  // enable +20 dBm operation
  if(_power > 17) {
    state = _mod->SPIsetRegValue(RF69_REG_OCP, RF69_OCP_OFF | 0x0F);
    state |= _mod->SPIsetRegValue(RF69_REG_TEST_PA1, RF69_PA1_20_DBM);
    state |= _mod->SPIsetRegValue(RF69_REG_TEST_PA2, RF69_PA2_20_DBM);
    RADIOLIB_ASSERT(state);
  }

  // set RF switch (if present)
  _mod->setRfSwitchState(LOW, HIGH);

  // set mode to transmit
  state = setMode(RF69_TX);

  return(state);
}

int16_t RF69::readData(uint8_t* data, size_t len) {
  // set mode to standby
  int16_t state = standby();
  RADIOLIB_ASSERT(state);

  // get packet length
  size_t length = len;
  if(len == RF69_MAX_PACKET_LENGTH) {
    length = getPacketLength();
  }

  // check address filtering
  uint8_t filter = _mod->SPIgetRegValue(RF69_REG_PACKET_CONFIG_1, 2, 1);
  if((filter == RF69_ADDRESS_FILTERING_NODE) || (filter == RF69_ADDRESS_FILTERING_NODE_BROADCAST)) {
    _mod->SPIreadRegister(RF69_REG_FIFO);
  }

  // read packet data
  _mod->SPIreadRegisterBurst(RF69_REG_FIFO, length, data);

  // clear internal flag so getPacketLength can return the new packet length
  _packetLengthQueried = false;

  // clear interrupt flags
  clearIRQFlags();

  return(ERR_NONE);
}

int16_t RF69::setOOK(bool enableOOK) {
  // set OOK and if successful, save the new setting
  int16_t state = ERR_NONE;
  if(enableOOK) {
    state = _mod->SPIsetRegValue(RF69_REG_DATA_MODUL, RF69_OOK, 4, 3, 5);
  } else {
    state = _mod->SPIsetRegValue(RF69_REG_DATA_MODUL, RF69_FSK, 4, 3, 5);
  }

  if(state == ERR_NONE) {
    _ook = enableOOK;
  }

  return(state);
}

int16_t RF69::setOokThresholdType(uint8_t type) {
  if((type != RF69_OOK_THRESH_FIXED) && (type != RF69_OOK_THRESH_PEAK) && (type != RF69_OOK_THRESH_AVERAGE)) {
    return(ERR_INVALID_OOK_RSSI_PEAK_TYPE);
  }
  return(_mod->SPIsetRegValue(RF69_REG_OOK_PEAK, type, 7, 3, 5));
}

int16_t RF69::setFrequency(float freq) {
  // check allowed frequency range
  if(!(((freq > 290.0) && (freq < 340.0)) ||
       ((freq > 431.0) && (freq < 510.0)) ||
       ((freq > 862.0) && (freq < 1020.0)))) {
    return(ERR_INVALID_FREQUENCY);
  }

  // set mode to standby
  setMode(RF69_STANDBY);

  //set carrier frequency
  uint32_t FRF = (freq * (uint32_t(1) << RF69_DIV_EXPONENT)) / RF69_CRYSTAL_FREQ;
  _mod->SPIwriteRegister(RF69_REG_FRF_MSB, (FRF & 0xFF0000) >> 16);
  _mod->SPIwriteRegister(RF69_REG_FRF_MID, (FRF & 0x00FF00) >> 8);
  _mod->SPIwriteRegister(RF69_REG_FRF_LSB, FRF & 0x0000FF);

  _freq = freq;

  return(ERR_NONE);
}

int16_t RF69::setBitRate(float br) {
  RADIOLIB_CHECK_RANGE(br, 1.2, 300.0, ERR_INVALID_BIT_RATE);

  // check bitrate-bandwidth ratio
  if(!(br < 2000 * _rxBw)) {
    return(ERR_INVALID_BIT_RATE_BW_RATIO);
  }

  // set mode to standby
  setMode(RF69_STANDBY);

  // set bit rate
  uint16_t bitRate = 32000 / br;
  int16_t state = _mod->SPIsetRegValue(RF69_REG_BITRATE_MSB, (bitRate & 0xFF00) >> 8, 7, 0);
  state |= _mod->SPIsetRegValue(RF69_REG_BITRATE_LSB, bitRate & 0x00FF, 7, 0);

  if(state == ERR_NONE) {
    _br = br;
  }

  return(state);
}

int16_t RF69::setRxBandwidth(float rxBw) {
  // check bitrate-bandwidth ratio
  if(!(_br < 2000 * rxBw)) {
    return(ERR_INVALID_BIT_RATE_BW_RATIO);
  }

  // check allowed bandwidth values
  uint8_t bwMant, bwExp;
  if(rxBw == 2.6) {
    bwMant = RF69_RX_BW_MANT_24;
    bwExp = 7;
  } else if(rxBw == 3.1) {
    bwMant = RF69_RX_BW_MANT_20;
    bwExp = 7;
  } else if(rxBw == 3.9) {
    bwMant = RF69_RX_BW_MANT_16;
    bwExp = 7;
  } else if(rxBw == 5.2) {
    bwMant = RF69_RX_BW_MANT_24;
    bwExp = 6;
  } else if(rxBw == 6.3) {
    bwMant = RF69_RX_BW_MANT_20;
    bwExp = 6;
  } else if(rxBw == 7.8) {
    bwMant = RF69_RX_BW_MANT_16;
    bwExp = 6;
  } else if(rxBw == 10.4) {
    bwMant = RF69_RX_BW_MANT_24;
    bwExp = 5;
  } else if(rxBw == 12.5) {
    bwMant = RF69_RX_BW_MANT_20;
    bwExp = 5;
  } else if(rxBw == 15.6) {
    bwMant = RF69_RX_BW_MANT_16;
    bwExp = 5;
  } else if(rxBw == 20.8) {
    bwMant = RF69_RX_BW_MANT_24;
    bwExp = 4;
  } else if(rxBw == 25.0) {
    bwMant = RF69_RX_BW_MANT_20;
    bwExp = 4;
  } else if(rxBw == 31.3) {
    bwMant = RF69_RX_BW_MANT_16;
    bwExp = 4;
  } else if(rxBw == 41.7) {
    bwMant = RF69_RX_BW_MANT_24;
    bwExp = 3;
  } else if(rxBw == 50.0) {
    bwMant = RF69_RX_BW_MANT_20;
    bwExp = 3;
  } else if(rxBw == 62.5) {
    bwMant = RF69_RX_BW_MANT_16;
    bwExp = 3;
  } else if(rxBw == 83.3) {
    bwMant = RF69_RX_BW_MANT_24;
    bwExp = 2;
  } else if(rxBw == 100.0) {
    bwMant = RF69_RX_BW_MANT_20;
    bwExp = 2;
  } else if(rxBw == 125.0) {
    bwMant = RF69_RX_BW_MANT_16;
    bwExp = 2;
  } else if(rxBw == 166.7) {
    bwMant = RF69_RX_BW_MANT_24;
    bwExp = 1;
  } else if(rxBw == 200.0) {
    bwMant = RF69_RX_BW_MANT_20;
    bwExp = 1;
  } else if(rxBw == 250.0) {
    bwMant = RF69_RX_BW_MANT_16;
    bwExp = 1;
  } else if(rxBw == 333.3) {
    bwMant = RF69_RX_BW_MANT_24;
    bwExp = 0;
  } else if(rxBw == 400.0) {
    bwMant = RF69_RX_BW_MANT_20;
    bwExp = 0;
  } else if(rxBw == 500.0) {
    bwMant = RF69_RX_BW_MANT_16;
    bwExp = 0;
  } else {
    return(ERR_INVALID_RX_BANDWIDTH);
  }

  // set mode to standby
  setMode(RF69_STANDBY);

  // set Rx bandwidth
  int16_t state = _mod->SPIsetRegValue(RF69_REG_RX_BW, RF69_DCC_FREQ | bwMant | bwExp, 7, 0);
  if(state == ERR_NONE) {
    _rxBw = rxBw;
  }
  return(state);
}

int16_t RF69::setFrequencyDeviation(float freqDev) {
  // set frequency deviation to lowest available setting (required for digimodes)
  float newFreqDev = freqDev;
  if(freqDev < 0.0) {
    newFreqDev = 0.6;
  }

  // check frequency deviation range
  if(!((newFreqDev + _br/2 <= 500))) {
    return(ERR_INVALID_FREQUENCY_DEVIATION);
  }

  // set mode to standby
  setMode(RF69_STANDBY);

  // set frequency deviation from carrier frequency
  uint32_t base = 1;
  uint32_t fdev = (newFreqDev * (base << 19)) / 32000;
  int16_t state = _mod->SPIsetRegValue(RF69_REG_FDEV_MSB, (fdev & 0xFF00) >> 8, 5, 0);
  state |= _mod->SPIsetRegValue(RF69_REG_FDEV_LSB, fdev & 0x00FF, 7, 0);

  if (state == ERR_NONE) {
    _freqDev = freqDev;
  }

  return(state);
}

int16_t RF69::setOutputPower(int8_t power, bool highPower) {
  if(highPower) {
    RADIOLIB_CHECK_RANGE(power, -2, 20, ERR_INVALID_OUTPUT_POWER);
  } else {
    RADIOLIB_CHECK_RANGE(power, -18, 13, ERR_INVALID_OUTPUT_POWER);
  }

  // set mode to standby
  setMode(RF69_STANDBY);

  // set output power
  int16_t state;
  if(highPower) {
    // check if both PA1 and PA2 are needed
    if(power <= 10) {
      // -2 to 13 dBm, PA1 is enough
      state = _mod->SPIsetRegValue(RF69_REG_PA_LEVEL, RF69_PA0_OFF | RF69_PA1_ON | RF69_PA2_OFF | (power + 18), 7, 0);
    } else if(power <= 17) {
      // 13 to 17 dBm, both PAs required
      state = _mod->SPIsetRegValue(RF69_REG_PA_LEVEL, RF69_PA0_OFF | RF69_PA1_ON | RF69_PA2_ON | (power + 14), 7, 0);
    } else {
      // 18 - 20 dBm, both PAs and hig power settings required
      state = _mod->SPIsetRegValue(RF69_REG_PA_LEVEL, RF69_PA0_OFF | RF69_PA1_ON | RF69_PA2_ON | (power + 11), 7, 0);
    }

  } else {
    // low power module, use only PA0
    state = _mod->SPIsetRegValue(RF69_REG_PA_LEVEL, RF69_PA0_ON | RF69_PA1_OFF | RF69_PA2_OFF | (power + 18), 7, 0);
  }

  // cache the power value
  if(state == ERR_NONE) {
    _power = power;
  }

  return(state);
}

int16_t RF69::setSyncWord(uint8_t* syncWord, size_t len, uint8_t maxErrBits) {
  // check constraints
  if((maxErrBits > 7) || (len > 8)) {
    return(ERR_INVALID_SYNC_WORD);
  }

  // sync word must not contain value 0x00
  for(uint8_t i = 0; i < len; i++) {
    if(syncWord[i] == 0x00) {
      return(ERR_INVALID_SYNC_WORD);
    }
  }

  int16_t state = enableSyncWordFiltering(maxErrBits);
  RADIOLIB_ASSERT(state);

  // set sync word register
  _mod->SPIwriteRegisterBurst(RF69_REG_SYNC_VALUE_1, syncWord, len);

  if(state == ERR_NONE) {
    _syncWordLength = len;
  }

  return(state);
}

int16_t RF69::setPreambleLength(uint8_t preambleLen) {
  // RF69 configures preamble length in bytes
  if(preambleLen % 8 != 0) {
    return(ERR_INVALID_PREAMBLE_LENGTH);
  }

  uint8_t preLenBytes = preambleLen / 8;
  _mod->SPIwriteRegister(RF69_REG_PREAMBLE_MSB, 0x00);

  uint16_t state = _mod->SPIsetRegValue(RF69_REG_PREAMBLE_LSB, preLenBytes);

  if (state == ERR_NONE) {
    _preambleLen = preambleLen;
  }

  return (state);
}

int16_t RF69::setNodeAddress(uint8_t nodeAddr) {
  // enable address filtering (node only)
  int16_t state = _mod->SPIsetRegValue(RF69_REG_PACKET_CONFIG_1, RF69_ADDRESS_FILTERING_NODE, 2, 1);
  RADIOLIB_ASSERT(state);

  // set node address
  return(_mod->SPIsetRegValue(RF69_REG_NODE_ADRS, nodeAddr));
}

int16_t RF69::setBroadcastAddress(uint8_t broadAddr) {
  // enable address filtering (node + broadcast)
  int16_t state = _mod->SPIsetRegValue(RF69_REG_PACKET_CONFIG_1, RF69_ADDRESS_FILTERING_NODE_BROADCAST, 2, 1);
  RADIOLIB_ASSERT(state);

  // set broadcast address
  return(_mod->SPIsetRegValue(RF69_REG_BROADCAST_ADRS, broadAddr));
}

int16_t RF69::disableAddressFiltering() {
  // disable address filtering
  int16_t state = _mod->SPIsetRegValue(RF69_REG_PACKET_CONFIG_1, RF69_ADDRESS_FILTERING_OFF, 2, 1);
  RADIOLIB_ASSERT(state);

  // set node address to default (0x00)
  state = _mod->SPIsetRegValue(RF69_REG_NODE_ADRS, 0x00);
  RADIOLIB_ASSERT(state);

  // set broadcast address to default (0x00)
  return(_mod->SPIsetRegValue(RF69_REG_BROADCAST_ADRS, 0x00));
}

void RF69::setAmbientTemperature(int16_t tempAmbient) {
  _tempOffset = getTemperature() -  tempAmbient;
}

int16_t RF69::getTemperature() {
  // set mode to STANDBY
  setMode(RF69_STANDBY);

  // start temperature measurement
  _mod->SPIsetRegValue(RF69_REG_TEMP_1, RF69_TEMP_MEAS_START, 3, 3);

  // wait until measurement is finished
  while(_mod->SPIgetRegValue(RF69_REG_TEMP_1, 2, 2) == RF69_TEMP_MEAS_RUNNING) {
    // check every 10 us
    Module::delay(10);
  }
  int8_t rawTemp = _mod->SPIgetRegValue(RF69_REG_TEMP_2);

  return(0 - (rawTemp + _tempOffset));
}

size_t RF69::getPacketLength(bool update) {
  if(!_packetLengthQueried && update) {
    if (_packetLengthConfig == RF69_PACKET_FORMAT_VARIABLE) {
      _packetLength = _mod->SPIreadRegister(RF69_REG_FIFO);
    } else {
      _packetLength = _mod->SPIreadRegister(RF69_REG_PAYLOAD_LENGTH);
    }
    _packetLengthQueried = true;
  }

  return(_packetLength);
}

int16_t RF69::fixedPacketLengthMode(uint8_t len) {
  return(setPacketMode(RF69_PACKET_FORMAT_FIXED, len));
}

int16_t RF69::variablePacketLengthMode(uint8_t maxLen) {
  return(setPacketMode(RF69_PACKET_FORMAT_VARIABLE, maxLen));
}

int16_t RF69::enableSyncWordFiltering(uint8_t maxErrBits) {
  // enable sync word recognition
  return(_mod->SPIsetRegValue(RF69_REG_SYNC_CONFIG, RF69_SYNC_ON | RF69_FIFO_FILL_CONDITION_SYNC | (_syncWordLength - 1) << 3 | maxErrBits, 7, 0));
}

int16_t RF69::disableSyncWordFiltering() {
  // disable preamble detection and generation
  int16_t state = _mod->SPIsetRegValue(RF69_REG_PREAMBLE_LSB, 0, 7, 0);
  state |= _mod->SPIsetRegValue(RF69_REG_PREAMBLE_MSB, 0, 7, 0);
  RADIOLIB_ASSERT(state);

  // disable sync word detection and generation
  state = _mod->SPIsetRegValue(RF69_REG_SYNC_CONFIG, RF69_SYNC_OFF | RF69_FIFO_FILL_CONDITION, 7, 6);

  return(state);
}

int16_t RF69::setCrcFiltering(bool crcOn) {
  if (crcOn == true) {
    return(_mod->SPIsetRegValue(RF69_REG_PACKET_CONFIG_1, RF69_CRC_ON, 4, 4));
  } else {
    return(_mod->SPIsetRegValue(RF69_REG_PACKET_CONFIG_1, RF69_CRC_OFF, 4, 4));
  }
}

int16_t RF69::setPromiscuousMode(bool promiscuous) {
  int16_t state = ERR_NONE;

  if (_promiscuous == promiscuous) {
    return(state);
  }

  if (promiscuous == true) {
    // disable preamble and sync word filtering and insertion
    state = disableSyncWordFiltering();
    RADIOLIB_ASSERT(state);

    // disable CRC filtering
    state = setCrcFiltering(false);
  } else {
    // enable preamble and sync word filtering and insertion
    state = enableSyncWordFiltering();
    RADIOLIB_ASSERT(state);

    // enable CRC filtering
    state = setCrcFiltering(true);
  }

  if(state == ERR_NONE) {
    _promiscuous = promiscuous;
  }

  return(state);
}

int16_t RF69::setDataShaping(uint8_t sh) {
  // set mode to standby
  int16_t state = standby();
  RADIOLIB_ASSERT(state);

  // set data shaping
  switch(sh) {
    case RADIOLIB_SHAPING_NONE:
      return(_mod->SPIsetRegValue(RF69_REG_DATA_MODUL, RF69_NO_SHAPING, 1, 0));
    case RADIOLIB_SHAPING_0_3:
      return(_mod->SPIsetRegValue(RF69_REG_DATA_MODUL, RF69_FSK_GAUSSIAN_0_3, 1, 0));
    case RADIOLIB_SHAPING_0_5:
      return(_mod->SPIsetRegValue(RF69_REG_DATA_MODUL, RF69_FSK_GAUSSIAN_0_5, 1, 0));
    case RADIOLIB_SHAPING_1_0:
      return(_mod->SPIsetRegValue(RF69_REG_DATA_MODUL, RF69_FSK_GAUSSIAN_1_0, 1, 0));
    default:
      return(ERR_INVALID_DATA_SHAPING);
  }
}

int16_t RF69::setEncoding(uint8_t encoding) {
  // set mode to standby
  int16_t state = standby();
  RADIOLIB_ASSERT(state);

  // set encoding
  switch(encoding) {
    case RADIOLIB_ENCODING_NRZ:
      return(_mod->SPIsetRegValue(RF69_REG_PACKET_CONFIG_1, RF69_DC_FREE_NONE, 6, 5));
    case RADIOLIB_ENCODING_MANCHESTER:
      return(_mod->SPIsetRegValue(RF69_REG_PACKET_CONFIG_1, RF69_DC_FREE_MANCHESTER, 6, 5));
    case RADIOLIB_ENCODING_WHITENING:
      return(_mod->SPIsetRegValue(RF69_REG_PACKET_CONFIG_1, RF69_DC_FREE_WHITENING, 6, 5));
    default:
      return(ERR_INVALID_ENCODING);
  }
}

int16_t RF69::setLnaTestBoost(bool value) {
  if(value) {
    return (_mod->SPIsetRegValue(RF69_REG_TEST_LNA, RF69_TEST_LNA_BOOST_HIGH, 7, 0));
  }

  return(_mod->SPIsetRegValue(RF69_TEST_LNA_BOOST_NORMAL, RF69_TEST_LNA_BOOST_HIGH, 7, 0));
}

float RF69::getRSSI() {
  return(-1.0 * (_mod->SPIgetRegValue(RF69_REG_RSSI_VALUE)/2.0));
}

void RF69::setRfSwitchPins(RADIOLIB_PIN_TYPE rxEn, RADIOLIB_PIN_TYPE txEn) {
  _mod->setRfSwitchPins(rxEn, txEn);
}

uint8_t RF69::random() {
  // set mode to Rx
  setMode(RF69_RX);

  // wait a bit for the RSSI reading to stabilise
  Module::delay(10);

  // read RSSI value 8 times, always keep just the least significant bit
  uint8_t randByte = 0x00;
  for(uint8_t i = 0; i < 8; i++) {
    randByte |= ((_mod->SPIreadRegister(RF69_REG_RSSI_VALUE) & 0x01) << i);
  }

  // set mode to standby
  setMode(RF69_STANDBY);

  return(randByte);
}

void RF69::setDirectAction(void (*func)(void)) {
  setDio1Action(func);
}

void RF69::readBit(RADIOLIB_PIN_TYPE pin) {
  updateDirectBuffer((uint8_t)digitalRead(pin));
}

int16_t RF69::getChipVersion() {
  return(_mod->SPIgetRegValue(RF69_REG_VERSION));
}

int16_t RF69::config() {
  int16_t state = ERR_NONE;

  // set mode to STANDBY
  state = setMode(RF69_STANDBY);
  RADIOLIB_ASSERT(state);

  // set operation modes
  state = _mod->SPIsetRegValue(RF69_REG_OP_MODE, RF69_SEQUENCER_ON | RF69_LISTEN_OFF, 7, 6);
  RADIOLIB_ASSERT(state);

  // enable over-current protection
  state = _mod->SPIsetRegValue(RF69_REG_OCP, RF69_OCP_ON, 4, 4);
  RADIOLIB_ASSERT(state);

  // set data mode, modulation type and shaping
  state = _mod->SPIsetRegValue(RF69_REG_DATA_MODUL, RF69_PACKET_MODE | RF69_FSK, 6, 3);
  state |= _mod->SPIsetRegValue(RF69_REG_DATA_MODUL, RF69_FSK_GAUSSIAN_0_3, 1, 0);
  RADIOLIB_ASSERT(state);

  // set RSSI threshold
  state = _mod->SPIsetRegValue(RF69_REG_RSSI_THRESH, RF69_RSSI_THRESHOLD, 7, 0);
  RADIOLIB_ASSERT(state);

  // reset FIFO flag
  _mod->SPIwriteRegister(RF69_REG_IRQ_FLAGS_2, RF69_IRQ_FIFO_OVERRUN);

  // disable ClkOut on DIO5
  state = _mod->SPIsetRegValue(RF69_REG_DIO_MAPPING_2, RF69_CLK_OUT_OFF, 2, 0);
  RADIOLIB_ASSERT(state);

  // set packet configuration and disable encryption
  state = _mod->SPIsetRegValue(RF69_REG_PACKET_CONFIG_1, RF69_PACKET_FORMAT_VARIABLE | RF69_DC_FREE_NONE | RF69_CRC_ON | RF69_CRC_AUTOCLEAR_ON | RF69_ADDRESS_FILTERING_OFF, 7, 1);
  state |= _mod->SPIsetRegValue(RF69_REG_PACKET_CONFIG_2, RF69_INTER_PACKET_RX_DELAY, 7, 4);
  state |= _mod->SPIsetRegValue(RF69_REG_PACKET_CONFIG_2, RF69_AUTO_RX_RESTART_ON | RF69_AES_OFF, 1, 0);
  RADIOLIB_ASSERT(state);

  // set payload length
  state = _mod->SPIsetRegValue(RF69_REG_PAYLOAD_LENGTH, RF69_PAYLOAD_LENGTH, 7, 0);
  RADIOLIB_ASSERT(state);

  // set FIFO threshold
  state = _mod->SPIsetRegValue(RF69_REG_FIFO_THRESH, RF69_TX_START_CONDITION_FIFO_NOT_EMPTY | RF69_FIFO_THRESHOLD, 7, 0);
  RADIOLIB_ASSERT(state);

  // set Rx timeouts
  state = _mod->SPIsetRegValue(RF69_REG_RX_TIMEOUT_1, RF69_TIMEOUT_RX_START, 7, 0);
  state |= _mod->SPIsetRegValue(RF69_REG_RX_TIMEOUT_2, RF69_TIMEOUT_RSSI_THRESH, 7, 0);
  RADIOLIB_ASSERT(state);

  // enable improved fading margin
  state = _mod->SPIsetRegValue(RF69_REG_TEST_DAGC, RF69_CONTINUOUS_DAGC_LOW_BETA_OFF, 7, 0);

  return(state);
}

int16_t RF69::setPacketMode(uint8_t mode, uint8_t len) {
  // check length
  if (len > RF69_MAX_PACKET_LENGTH) {
    return(ERR_PACKET_TOO_LONG);
  }

  // set to fixed packet length
  int16_t state = _mod->SPIsetRegValue(RF69_REG_PACKET_CONFIG_1, mode, 7, 7);
  RADIOLIB_ASSERT(state);

  // set length to register
  state = _mod->SPIsetRegValue(RF69_REG_PAYLOAD_LENGTH, len);
  RADIOLIB_ASSERT(state);

  // update the cached value
  _packetLengthConfig = mode;
  return(state);
}

int16_t RF69::setMode(uint8_t mode) {
  return(_mod->SPIsetRegValue(RF69_REG_OP_MODE, mode, 4, 2));
}

void RF69::clearIRQFlags() {
  _mod->SPIwriteRegister(RF69_REG_IRQ_FLAGS_1, 0b11111111);
  _mod->SPIwriteRegister(RF69_REG_IRQ_FLAGS_2, 0b11111111);
}

#endif
