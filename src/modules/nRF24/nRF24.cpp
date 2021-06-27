#include "nRF24.h"
#if !defined(RADIOLIB_EXCLUDE_NRF24)

nRF24::nRF24(Module* mod) : PhysicalLayer(NRF24_FREQUENCY_STEP_SIZE, NRF24_MAX_PACKET_LENGTH) {
  _mod = mod;
}

int16_t nRF24::begin(int16_t freq, int16_t dataRate, int8_t power, uint8_t addrWidth) {
  // set module properties
  _mod->SPIreadCommand = NRF24_CMD_READ;
  _mod->SPIwriteCommand = NRF24_CMD_WRITE;
  _mod->init(RADIOLIB_USE_SPI);
  Module::pinMode(_mod->getIrq(), INPUT);

  // set pin mode on RST (connected to nRF24 CE pin)
  Module::pinMode(_mod->getRst(), OUTPUT);
  Module::digitalWrite(_mod->getRst(), LOW);

  // wait for minimum power-on reset duration
  Module::delay(100);

  // check SPI connection
  int16_t val = _mod->SPIgetRegValue(NRF24_REG_SETUP_AW);
  if(!((val >= 0) && (val <= 3))) {
    RADIOLIB_DEBUG_PRINTLN(F("No nRF24 found!"));
    _mod->term(RADIOLIB_USE_SPI);
    return(ERR_CHIP_NOT_FOUND);
  }
  RADIOLIB_DEBUG_PRINTLN(F("M\tnRF24"));

  // configure settings inaccessible by public API
  int16_t state = config();
  RADIOLIB_ASSERT(state);

  // set mode to standby
  state = standby();
  RADIOLIB_ASSERT(state);

  // set frequency
  state = setFrequency(freq);
  RADIOLIB_ASSERT(state);

  // set data rate
  state = setDataRate(dataRate);
  RADIOLIB_ASSERT(state);

  // set output power
  state = setOutputPower(power);
  RADIOLIB_ASSERT(state);

  // set address width
  state = setAddressWidth(addrWidth);
  RADIOLIB_ASSERT(state);

  // set CRC
  state = setCrcFiltering(true);
  RADIOLIB_ASSERT(state);

  // set auto-ACK on all pipes
  state = setAutoAck(true);
  RADIOLIB_ASSERT(state);

  return(state);
}

int16_t nRF24::sleep() {
  return(_mod->SPIsetRegValue(NRF24_REG_CONFIG, NRF24_POWER_DOWN, 1, 1));
}

int16_t nRF24::standby() {
  // make sure carrier output is disabled
  _mod->SPIsetRegValue(NRF24_REG_RF_SETUP, NRF24_CONT_WAVE_OFF, 7, 7);
  _mod->SPIsetRegValue(NRF24_REG_RF_SETUP, NRF24_PLL_LOCK_OFF, 4, 4);
  Module::digitalWrite(_mod->getRst(), LOW);

  // use standby-1 mode
  return(_mod->SPIsetRegValue(NRF24_REG_CONFIG, NRF24_POWER_UP, 1, 1));
}

int16_t nRF24::transmit(uint8_t* data, size_t len, uint8_t addr) {
  // start transmission
  int16_t state = startTransmit(data, len, addr);
  RADIOLIB_ASSERT(state);

  // wait until transmission is finished
  uint32_t start = Module::micros();
  while(Module::digitalRead(_mod->getIrq())) {
    Module::yield();

    // check maximum number of retransmits
    if(getStatus(NRF24_MAX_RT)) {
      standby();
      clearIRQ();
      return(ERR_ACK_NOT_RECEIVED);
    }

    // check timeout: 15 retries * 4ms (max Tx time as per datasheet)
    if(Module::micros() - start >= 60000) {
      standby();
      clearIRQ();
      return(ERR_TX_TIMEOUT);
    }
  }

  // clear interrupts
  clearIRQ();

  return(state);
}

int16_t nRF24::receive(uint8_t* data, size_t len) {
  // start reception
  int16_t state = startReceive();
  RADIOLIB_ASSERT(state);

  // wait for Rx_DataReady or timeout
  uint32_t start = Module::micros();
  while(Module::digitalRead(_mod->getIrq())) {
    Module::yield();

    // check timeout: 15 retries * 4ms (max Tx time as per datasheet)
    if(Module::micros() - start >= 60000) {
      standby();
      clearIRQ();
      return(ERR_RX_TIMEOUT);
    }
  }

  // read the received data
  return(readData(data, len));
}

int16_t nRF24::transmitDirect(uint32_t frf) {
  // set raw frequency value
  if(frf != 0) {
    uint8_t freqRaw = frf - 2400;
    _mod->SPIwriteRegister(NRF24_REG_RF_CH, freqRaw & 0b01111111);
  }

  // output carrier
  int16_t state = _mod->SPIsetRegValue(NRF24_REG_CONFIG, NRF24_PTX, 0, 0);
  state |= _mod->SPIsetRegValue(NRF24_REG_RF_SETUP, NRF24_CONT_WAVE_ON, 7, 7);
  state |= _mod->SPIsetRegValue(NRF24_REG_RF_SETUP, NRF24_PLL_LOCK_ON, 4, 4);
  Module::digitalWrite(_mod->getRst(), HIGH);
  return(state);
}

int16_t nRF24::receiveDirect() {
  // nRF24 is unable to directly output demodulated data
  // this method is implemented only for PhysicalLayer compatibility
  return(ERR_NONE);
}

void nRF24::setIrqAction(void (*func)(void)) {
  Module::attachInterrupt(RADIOLIB_DIGITAL_PIN_TO_INTERRUPT(_mod->getIrq()), func, FALLING);
}

int16_t nRF24::startTransmit(uint8_t* data, size_t len, uint8_t addr) {
  // suppress unused variable warning
  (void)addr;

  // check packet length
  if(len > NRF24_MAX_PACKET_LENGTH) {
    return(ERR_PACKET_TOO_LONG);
  }

  // set mode to standby
  int16_t state = standby();
  RADIOLIB_ASSERT(state);

  // enable primary Tx mode
  state = _mod->SPIsetRegValue(NRF24_REG_CONFIG, NRF24_PTX, 0, 0);

  // clear interrupts
  clearIRQ();

  // enable Tx_DataSent interrupt
  state |= _mod->SPIsetRegValue(NRF24_REG_CONFIG, NRF24_MASK_TX_DS_IRQ_ON, 5, 5);
  RADIOLIB_ASSERT(state);

  // flush Tx FIFO
  SPItransfer(NRF24_CMD_FLUSH_TX);

  // fill Tx FIFO
  uint8_t buff[32];
  memset(buff, 0x00, 32);
  memcpy(buff, data, len);
  SPIwriteTxPayload(data, len);

  // CE high to start transmitting
  Module::digitalWrite(_mod->getRst(), HIGH);
  Module::delay(1);
  Module::digitalWrite(_mod->getRst(), LOW);

  return(state);
}

int16_t nRF24::startReceive() {
  // set mode to standby
  int16_t state = standby();
  RADIOLIB_ASSERT(state);

  // enable primary Rx mode
  state = _mod->SPIsetRegValue(NRF24_REG_CONFIG, NRF24_PRX, 0, 0);
  RADIOLIB_ASSERT(state);

  // enable Rx_DataReady interrupt
  clearIRQ();
  state = _mod->SPIsetRegValue(NRF24_REG_CONFIG,  NRF24_MASK_RX_DR_IRQ_ON, 6, 6);
  RADIOLIB_ASSERT(state);

  // flush Rx FIFO
  SPItransfer(NRF24_CMD_FLUSH_RX);

  // CE high to start receiving
  Module::digitalWrite(_mod->getRst(), HIGH);

  // wait to enter Rx state
  Module::delay(1);

  return(state);
}

int16_t nRF24::readData(uint8_t* data, size_t len) {
  // set mode to standby
  int16_t state = standby();
  RADIOLIB_ASSERT(state);

  // get packet length
  size_t length = len;
  if(len == NRF24_MAX_PACKET_LENGTH) {
    length = getPacketLength();
  }

  // read packet data
  SPIreadRxPayload(data, length);

  // clear interrupt
  clearIRQ();

  return(ERR_NONE);
}

int16_t nRF24::setFrequency(int16_t freq) {
  RADIOLIB_CHECK_RANGE(freq, 2400, 2525, ERR_INVALID_FREQUENCY);

  // set frequency
  uint8_t freqRaw = freq - 2400;
  uint16_t state = _mod->SPIsetRegValue(NRF24_REG_RF_CH, freqRaw, 6, 0);

  if(state == ERR_NONE) {
    _freq = freq;
  }

  return(state);
}

int16_t nRF24::setDataRate(int16_t dataRate) {
  // set mode to standby
  int16_t state = standby();
  RADIOLIB_ASSERT(state);

  // set data rate
  if(dataRate == 250) {
    state = _mod->SPIsetRegValue(NRF24_REG_RF_SETUP, NRF24_DR_250_KBPS, 5, 5);
    state |= _mod->SPIsetRegValue(NRF24_REG_RF_SETUP, NRF24_DR_250_KBPS, 3, 3);
  } else if(dataRate == 1000) {
    state = _mod->SPIsetRegValue(NRF24_REG_RF_SETUP, NRF24_DR_1_MBPS, 5, 5);
    state |= _mod->SPIsetRegValue(NRF24_REG_RF_SETUP, NRF24_DR_1_MBPS, 3, 3);
  } else if(dataRate == 2000) {
    state = _mod->SPIsetRegValue(NRF24_REG_RF_SETUP, NRF24_DR_2_MBPS, 5, 5);
    state |= _mod->SPIsetRegValue(NRF24_REG_RF_SETUP, NRF24_DR_2_MBPS, 3, 3);
  } else {
    return(ERR_INVALID_DATA_RATE);
  }

  if(state == ERR_NONE) {
    _dataRate = dataRate;
  }

  return(state);
}

int16_t nRF24::setOutputPower(int8_t power) {
  // set mode to standby
  int16_t state = standby();
  RADIOLIB_ASSERT(state);

  // check allowed values
  uint8_t powerRaw = 0;
  switch(power) {
    case -18:
      powerRaw = NRF24_RF_PWR_18_DBM;
      break;
    case -12:
      powerRaw = NRF24_RF_PWR_12_DBM;
      break;
    case -6:
      powerRaw = NRF24_RF_PWR_6_DBM;
      break;
    case 0:
      powerRaw = NRF24_RF_PWR_0_DBM;
      break;
    default:
      return(ERR_INVALID_OUTPUT_POWER);
  }

  // write new register value
  state = _mod->SPIsetRegValue(NRF24_REG_RF_SETUP, powerRaw, 2, 1);

  if(state == ERR_NONE) {
    _power = power;
  }

  return(state);
}

int16_t nRF24::setAddressWidth(uint8_t addrWidth) {
  // set mode to standby
  int16_t state = standby();
  RADIOLIB_ASSERT(state);

  // set address width
  switch(addrWidth) {
    case 2:
      // Even if marked as 'Illegal' on the datasheet this will work:
      // http://travisgoodspeed.blogspot.com/2011/02/promiscuity-is-nrf24l01s-duty.html
      state = _mod->SPIsetRegValue(NRF24_REG_SETUP_AW, NRF24_ADDRESS_2_BYTES, 1, 0);
      break;
    case 3:
      state = _mod->SPIsetRegValue(NRF24_REG_SETUP_AW, NRF24_ADDRESS_3_BYTES, 1, 0);
      break;
    case 4:
      state = _mod->SPIsetRegValue(NRF24_REG_SETUP_AW, NRF24_ADDRESS_4_BYTES, 1, 0);
      break;
    case 5:
      state = _mod->SPIsetRegValue(NRF24_REG_SETUP_AW, NRF24_ADDRESS_5_BYTES, 1, 0);
      break;
    default:
      return(ERR_INVALID_ADDRESS_WIDTH);
  }

  if(state == ERR_NONE) {
    _addrWidth = addrWidth;
  }

  return(state);
}

int16_t nRF24::setTransmitPipe(uint8_t* addr) {
  // set mode to standby
  int16_t state = standby();
  RADIOLIB_ASSERT(state);

  // set transmit address
  _mod->SPIwriteRegisterBurst(NRF24_REG_TX_ADDR, addr, _addrWidth);

  // set Rx pipe 0 address (for ACK)
  _mod->SPIwriteRegisterBurst(NRF24_REG_RX_ADDR_P0, addr, _addrWidth);
  state |= _mod->SPIsetRegValue(NRF24_REG_EN_RXADDR, NRF24_P0_ON, 0, 0);

  return(state);
}

int16_t nRF24::setReceivePipe(uint8_t pipeNum, uint8_t* addr) {
  // set mode to standby
  int16_t state = standby();
  RADIOLIB_ASSERT(state);

  // write full pipe 0 - 1 address and enable the pipe
  switch(pipeNum) {
    case 0:
      _mod->SPIwriteRegisterBurst(NRF24_REG_RX_ADDR_P0, addr, _addrWidth);
      state |= _mod->SPIsetRegValue(NRF24_REG_EN_RXADDR, NRF24_P0_ON, 0, 0);
      break;
    case 1:
      _mod->SPIwriteRegisterBurst(NRF24_REG_RX_ADDR_P1, addr, _addrWidth);
      state |= _mod->SPIsetRegValue(NRF24_REG_EN_RXADDR, NRF24_P1_ON, 1, 1);
      break;
    default:
      return(ERR_INVALID_PIPE_NUMBER);
  }

  return(state);
}

int16_t nRF24::setReceivePipe(uint8_t pipeNum, uint8_t addrByte) {
  // set mode to standby
  int16_t state = standby();
  RADIOLIB_ASSERT(state);

  // write unique pipe 2 - 5 address and enable the pipe
  switch(pipeNum) {
    case 2:
      state = _mod->SPIsetRegValue(NRF24_REG_RX_ADDR_P2, addrByte);
      state |= _mod->SPIsetRegValue(NRF24_REG_EN_RXADDR, NRF24_P2_ON, 2, 2);
      break;
    case 3:
      state = _mod->SPIsetRegValue(NRF24_REG_RX_ADDR_P3, addrByte);
      state |= _mod->SPIsetRegValue(NRF24_REG_EN_RXADDR, NRF24_P3_ON, 3, 3);
      break;
    case 4:
      state = _mod->SPIsetRegValue(NRF24_REG_RX_ADDR_P4, addrByte);
      state |= _mod->SPIsetRegValue(NRF24_REG_EN_RXADDR, NRF24_P4_ON, 4, 4);
      break;
    case 5:
      state = _mod->SPIsetRegValue(NRF24_REG_RX_ADDR_P5, addrByte);
      state |= _mod->SPIsetRegValue(NRF24_REG_EN_RXADDR, NRF24_P5_ON, 5, 5);
      break;
    default:
      return(ERR_INVALID_PIPE_NUMBER);
  }

  return(state);
}

int16_t nRF24::disablePipe(uint8_t pipeNum) {
  // set mode to standby
  int16_t state = standby();
  RADIOLIB_ASSERT(state);

  switch(pipeNum) {
    case 0:
      state = _mod->SPIsetRegValue(NRF24_REG_EN_RXADDR, NRF24_P0_OFF, 0, 0);
      break;
    case 1:
      state = _mod->SPIsetRegValue(NRF24_REG_EN_RXADDR, NRF24_P1_OFF, 1, 1);
      break;
    case 2:
      state = _mod->SPIsetRegValue(NRF24_REG_EN_RXADDR, NRF24_P2_OFF, 2, 2);
      break;
    case 3:
      state = _mod->SPIsetRegValue(NRF24_REG_EN_RXADDR, NRF24_P3_OFF, 3, 3);
      break;
    case 4:
      state = _mod->SPIsetRegValue(NRF24_REG_EN_RXADDR, NRF24_P4_OFF, 4, 4);
      break;
    case 5:
      state = _mod->SPIsetRegValue(NRF24_REG_EN_RXADDR, NRF24_P5_OFF, 5, 5);
      break;
    default:
      return(ERR_INVALID_PIPE_NUMBER);
  }

  return(state);
}

int16_t nRF24::getStatus(uint8_t mask) {
  return(_mod->SPIgetRegValue(NRF24_REG_STATUS) & mask);
}

bool nRF24::isCarrierDetected() {
  return(_mod->SPIgetRegValue(NRF24_REG_RPD, 0, 0) == 1);
}

int16_t nRF24::setFrequencyDeviation(float freqDev) {
  // nRF24 is unable to set frequency deviation
  // this method is implemented only for PhysicalLayer compatibility
  (void)freqDev;
  return(ERR_NONE);
}

size_t nRF24::getPacketLength(bool update) {
  (void)update;
  uint8_t length = 0;
  SPItransfer(NRF24_CMD_READ_RX_PAYLOAD_WIDTH, false, NULL, &length, 1);
  return((size_t)length);
}

int16_t nRF24::setCrcFiltering(bool crcOn) {
  // Auto Ack needs to be disabled in order to disable CRC.
  if (!crcOn) {
    int16_t status = setAutoAck(false);
    RADIOLIB_ASSERT(status)
  }

  // Disable CRC
  return _mod->SPIsetRegValue(NRF24_REG_CONFIG, (crcOn ? NRF24_CRC_ON : NRF24_CRC_OFF), 3, 3);
}

int16_t nRF24::setAutoAck(bool autoAckOn){
  return _mod->SPIsetRegValue(NRF24_REG_EN_AA, (autoAckOn ? NRF24_AA_ALL_ON : NRF24_AA_ALL_OFF), 5, 0);
}

int16_t nRF24::setAutoAck(uint8_t pipeNum, bool autoAckOn){
  switch(pipeNum) {
    case 0:
      return _mod->SPIsetRegValue(NRF24_REG_EN_AA, (autoAckOn ? NRF24_AA_P0_ON : NRF24_AA_P0_OFF), 0, 0);
      break;
    case 1:
      return _mod->SPIsetRegValue(NRF24_REG_EN_AA, (autoAckOn ? NRF24_AA_P1_ON : NRF24_AA_P1_OFF), 1, 1);
      break;
    case 2:
      return _mod->SPIsetRegValue(NRF24_REG_EN_AA, (autoAckOn ? NRF24_AA_P2_ON : NRF24_AA_P2_OFF), 2, 2);
      break;
    case 3:
      return _mod->SPIsetRegValue(NRF24_REG_EN_AA, (autoAckOn ? NRF24_AA_P3_ON : NRF24_AA_P3_OFF), 3, 3);
      break;
    case 4:
      return _mod->SPIsetRegValue(NRF24_REG_EN_AA, (autoAckOn ? NRF24_AA_P4_ON : NRF24_AA_P4_OFF), 4, 4);
      break;
    case 5:
      return _mod->SPIsetRegValue(NRF24_REG_EN_AA, (autoAckOn ? NRF24_AA_P5_ON : NRF24_AA_P5_OFF), 5, 5);
      break;
    default:
      return (ERR_INVALID_PIPE_NUMBER);
  }
}

int16_t nRF24::setDataShaping(uint8_t sh) {
  // nRF24 is unable to set data shaping
  // this method is implemented only for PhysicalLayer compatibility
  (void)sh;
  return(ERR_NONE);
}

int16_t nRF24::setEncoding(uint8_t encoding) {
  // nRF24 is unable to set encoding
  // this method is implemented only for PhysicalLayer compatibility
  (void)encoding;
  return(ERR_NONE);
}

uint8_t nRF24::random() {
  // nRF24 is unable to measure RSSI, hence no TRNG
  // this method is implemented only for PhysicalLayer compatibility
  return(0);
}

void nRF24::setDirectAction(void (*func)(void)) {
  // nRF24 is unable to perform direct mode actions
  // this method is implemented only for PhysicalLayer compatibility
  (void)func;
}

void nRF24::readBit(RADIOLIB_PIN_TYPE pin) {
  // nRF24 is unable to perform direct mode actions
  // this method is implemented only for PhysicalLayer compatibility
  (void)pin;
}

void nRF24::clearIRQ() {
  // clear status bits
  _mod->SPIsetRegValue(NRF24_REG_STATUS, NRF24_RX_DR | NRF24_TX_DS | NRF24_MAX_RT, 6, 4);

  // disable interrupts
  _mod->SPIsetRegValue(NRF24_REG_CONFIG, NRF24_MASK_RX_DR_IRQ_OFF | NRF24_MASK_TX_DS_IRQ_OFF | NRF24_MASK_MAX_RT_IRQ_OFF, 6, 4);
}

int16_t nRF24::config() {
  // enable 16-bit CRC
  int16_t state = _mod->SPIsetRegValue(NRF24_REG_CONFIG, NRF24_CRC_ON | NRF24_CRC_16, 3, 2);
  RADIOLIB_ASSERT(state);

  // set 15 retries and delay 1500 (5*250) us
  _mod->SPIsetRegValue(NRF24_REG_SETUP_RETR, (5 << 4) | 5);

  // set features: dynamic payload on, payload with ACK packets off, dynamic ACK off
  state = _mod->SPIsetRegValue(NRF24_REG_FEATURE, NRF24_DPL_ON | NRF24_ACK_PAY_OFF | NRF24_DYN_ACK_OFF, 2, 0);
  RADIOLIB_ASSERT(state);

  // enable dynamic payloads
  state = _mod->SPIsetRegValue(NRF24_REG_DYNPD, NRF24_DPL_ALL_ON, 5, 0);
  RADIOLIB_ASSERT(state);

  // reset IRQ
  clearIRQ();

  // clear status
  _mod->SPIsetRegValue(NRF24_REG_STATUS, NRF24_RX_DR | NRF24_TX_DS | NRF24_MAX_RT, 6, 4);

  // flush FIFOs
  SPItransfer(NRF24_CMD_FLUSH_TX);
  SPItransfer(NRF24_CMD_FLUSH_RX);

  // power up
  _mod->SPIsetRegValue(NRF24_REG_CONFIG, NRF24_POWER_UP, 1, 1);
  Module::delay(5);

  return(state);
}

void nRF24::SPIreadRxPayload(uint8_t* data, uint8_t numBytes) {
  SPItransfer(NRF24_CMD_READ_RX_PAYLOAD, false, NULL, data, numBytes);
}

void nRF24::SPIwriteTxPayload(uint8_t* data, uint8_t numBytes) {
  SPItransfer(NRF24_CMD_WRITE_TX_PAYLOAD, true, data, NULL, numBytes);
}

void nRF24::SPItransfer(uint8_t cmd, bool write, uint8_t* dataOut, uint8_t* dataIn, uint8_t numBytes) {
  // get pointer to used SPI interface and the settings
  SPIClass* spi = _mod->getSpi();
  SPISettings spiSettings = _mod->getSpiSettings();

  // start transfer
  Module::digitalWrite(_mod->getCs(), LOW);
  spi->beginTransaction(spiSettings);

  // send command
  spi->transfer(cmd);

  // send data
  if(write) {
    for(uint8_t i = 0; i < numBytes; i++) {
      spi->transfer(dataOut[i]);
    }
  } else {
    for(uint8_t i = 0; i < numBytes; i++) {
      dataIn[i] = spi->transfer(0x00);
    }
  }

  // stop transfer
  spi->endTransaction();
  Module::digitalWrite(_mod->getCs(), HIGH);
}

#endif
