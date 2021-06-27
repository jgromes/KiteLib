#if !defined(_RADIOLIB_NRF24_H) && !defined(RADIOLIB_EXCLUDE_NRF24)
#define _RADIOLIB_NRF24_H

#include "../../Module.h"
#include "../../TypeDef.h"

#include "../../protocols/PhysicalLayer/PhysicalLayer.h"

// nRF24 physical layer properties
#define NRF24_FREQUENCY_STEP_SIZE                     1000000.0
#define NRF24_MAX_PACKET_LENGTH                       32

// nRF24 SPI commands
#define NRF24_CMD_READ                                0b00000000
#define NRF24_CMD_WRITE                               0b00100000
#define NRF24_CMD_READ_RX_PAYLOAD                     0b01100001
#define NRF24_CMD_WRITE_TX_PAYLOAD                    0b10100000
#define NRF24_CMD_FLUSH_TX                            0b11100001
#define NRF24_CMD_FLUSH_RX                            0b11100010
#define NRF24_CMD_REUSE_TX_PAXLOAD                    0b11100011
#define NRF24_CMD_READ_RX_PAYLOAD_WIDTH               0b01100000
#define NRF24_CMD_WRITE_ACK_PAYLOAD                   0b10101000
#define NRF24_CMD_WRITE_TX_PAYLOAD_NOACK              0b10110000
#define NRF24_CMD_NOP                                 0b11111111

// nRF24 register map
#define NRF24_REG_CONFIG                              0x00
#define NRF24_REG_EN_AA                               0x01
#define NRF24_REG_EN_RXADDR                           0x02
#define NRF24_REG_SETUP_AW                            0x03
#define NRF24_REG_SETUP_RETR                          0x04
#define NRF24_REG_RF_CH                               0x05
#define NRF24_REG_RF_SETUP                            0x06
#define NRF24_REG_STATUS                              0x07
#define NRF24_REG_OBSERVE_TX                          0x08
#define NRF24_REG_RPD                                 0x09
#define NRF24_REG_RX_ADDR_P0                          0x0A
#define NRF24_REG_RX_ADDR_P1                          0x0B
#define NRF24_REG_RX_ADDR_P2                          0x0C
#define NRF24_REG_RX_ADDR_P3                          0x0D
#define NRF24_REG_RX_ADDR_P4                          0x0E
#define NRF24_REG_RX_ADDR_P5                          0x0F
#define NRF24_REG_TX_ADDR                             0x10
#define NRF24_REG_RX_PW_P0                            0x11
#define NRF24_REG_RX_PW_P1                            0x12
#define NRF24_REG_RX_PW_P2                            0x13
#define NRF24_REG_RX_PW_P3                            0x14
#define NRF24_REG_RX_PW_P4                            0x15
#define NRF24_REG_RX_PW_P5                            0x16
#define NRF24_REG_FIFO_STATUS                         0x17
#define NRF24_REG_DYNPD                               0x1C
#define NRF24_REG_FEATURE                             0x1D

// NRF24_REG_CONFIG                                                   MSB   LSB   DESCRIPTION
#define NRF24_MASK_RX_DR_IRQ_OFF                      0b01000000  //  6     6     RX_DR will not be reflected on IRQ pin
#define NRF24_MASK_RX_DR_IRQ_ON                       0b00000000  //  6     6     RX_DR will be reflected on IRQ pin as active low (default)
#define NRF24_MASK_TX_DS_IRQ_OFF                      0b00100000  //  5     5     TX_DS will not be reflected on IRQ pin
#define NRF24_MASK_TX_DS_IRQ_ON                       0b00000000  //  5     5     TX_DS will be reflected on IRQ pin as active low (default)
#define NRF24_MASK_MAX_RT_IRQ_OFF                     0b00010000  //  4     4     MAX_RT will not be reflected on IRQ pin
#define NRF24_MASK_MAX_RT_IRQ_ON                      0b00000000  //  4     4     MAX_RT will be reflected on IRQ pin as active low (default)
#define NRF24_CRC_OFF                                 0b00000000  //  3     3     CRC calculation: disabled
#define NRF24_CRC_ON                                  0b00001000  //  3     3                      enabled (default)
#define NRF24_CRC_8                                   0b00000000  //  2     2     CRC scheme: CRC8 (default)
#define NRF24_CRC_16                                  0b00000100  //  2     2                 CRC16
#define NRF24_POWER_UP                                0b00000010  //  1     1     power up
#define NRF24_POWER_DOWN                              0b00000000  //  1     1     power down
#define NRF24_PTX                                     0b00000000  //  0     0     enable primary Tx
#define NRF24_PRX                                     0b00000001  //  0     0     enable primary Rx

// NRF24_REG_EN_AA
#define NRF24_AA_ALL_OFF                              0b00000000  //  5     0     auto-ACK on all pipes: disabled
#define NRF24_AA_ALL_ON                               0b00111111  //  5     0                         enabled (default)
#define NRF24_AA_P5_OFF                               0b00000000  //  5     5     auto-ACK on pipe 5: disabled
#define NRF24_AA_P5_ON                                0b00100000  //  5     5                         enabled (default)
#define NRF24_AA_P4_OFF                               0b00000000  //  4     4     auto-ACK on pipe 4: disabled
#define NRF24_AA_P4_ON                                0b00010000  //  4     4                         enabled (default)
#define NRF24_AA_P3_OFF                               0b00000000  //  3     3     auto-ACK on pipe 3: disabled
#define NRF24_AA_P3_ON                                0b00001000  //  3     3                         enabled (default)
#define NRF24_AA_P2_OFF                               0b00000000  //  2     2     auto-ACK on pipe 2: disabled
#define NRF24_AA_P2_ON                                0b00000100  //  2     2                         enabled (default)
#define NRF24_AA_P1_OFF                               0b00000000  //  1     1     auto-ACK on pipe 1: disabled
#define NRF24_AA_P1_ON                                0b00000010  //  1     1                         enabled (default)
#define NRF24_AA_P0_OFF                               0b00000000  //  0     0     auto-ACK on pipe 0: disabled
#define NRF24_AA_P0_ON                                0b00000001  //  0     0                         enabled (default)

// NRF24_REG_EN_RXADDR
#define NRF24_P5_OFF                                  0b00000000  //  5     5     receive pipe 5: disabled (default)
#define NRF24_P5_ON                                   0b00100000  //  5     5                     enabled
#define NRF24_P4_OFF                                  0b00000000  //  4     4     receive pipe 4: disabled (default)
#define NRF24_P4_ON                                   0b00010000  //  4     4                     enabled
#define NRF24_P3_OFF                                  0b00000000  //  3     3     receive pipe 3: disabled (default)
#define NRF24_P3_ON                                   0b00001000  //  3     3                     enabled
#define NRF24_P2_OFF                                  0b00000000  //  2     2     receive pipe 2: disabled (default)
#define NRF24_P2_ON                                   0b00000100  //  2     2                     enabled
#define NRF24_P1_OFF                                  0b00000000  //  1     1     receive pipe 1: disabled
#define NRF24_P1_ON                                   0b00000010  //  1     1                     enabled (default)
#define NRF24_P0_OFF                                  0b00000000  //  0     0     receive pipe 0: disabled
#define NRF24_P0_ON                                   0b00000001  //  0     0                     enabled (default)

// NRF24_REG_SETUP_AW
#define NRF24_ADDRESS_2_BYTES                         0b00000000  //  1     0     address width: 2 bytes
#define NRF24_ADDRESS_3_BYTES                         0b00000001  //  1     0                    3 bytes
#define NRF24_ADDRESS_4_BYTES                         0b00000010  //  1     0                    4 bytes
#define NRF24_ADDRESS_5_BYTES                         0b00000011  //  1     0                    5 bytes (default)

// NRF24_REG_SETUP_RETR
#define NRF24_ARD                                     0b00000000  //  7     4     auto retransmit delay: t[us] = (NRF24_ARD + 1) * 250 us
#define NRF24_ARC_OFF                                 0b00000000  //  3     0     auto retransmit count: auto retransmit disabled
#define NRF24_ARC                                     0b00000011  //  3     0                            up to 3 retransmits on AA fail (default)

// NRF24_REG_RF_CH
#define NRF24_RF_CH                                   0b00000010  //  6     0     RF channel: f_CH[MHz] = 2400 MHz + NRF24_RF_CH

// NRF24_REG_RF_SETUP
#define NRF24_CONT_WAVE_OFF                           0b00000000  //  7     7     continuous carrier transmit: disabled (default)
#define NRF24_CONT_WAVE_ON                            0b10000000  //  7     7                                  enabled
#define NRF24_DR_250_KBPS                             0b00100000  //  5     5     data rate: 250 kbps
#define NRF24_DR_1_MBPS                               0b00000000  //  3     3                1 Mbps (default)
#define NRF24_DR_2_MBPS                               0b00001000  //  3     3                2 Mbps
#define NRF24_PLL_LOCK_ON                             0b00010000  //  4     4     force PLL lock: enabled
#define NRF24_PLL_LOCK_OFF                            0b00000000  //  4     4                     disabled (default)
#define NRF24_RF_PWR_18_DBM                           0b00000000  //  2     1     output power: -18 dBm
#define NRF24_RF_PWR_12_DBM                           0b00000010  //  2     1                   -12 dBm
#define NRF24_RF_PWR_6_DBM                            0b00000100  //  2     1                   -6 dBm
#define NRF24_RF_PWR_0_DBM                            0b00000110  //  2     1                   0 dBm (default)

// NRF24_REG_STATUS
#define NRF24_RX_DR                                   0b01000000  //  6     6     Rx data ready
#define NRF24_TX_DS                                   0b00100000  //  5     5     Tx data sent
#define NRF24_MAX_RT                                  0b00010000  //  4     4     maximum number of retransmits reached (must be cleared to continue)
#define NRF24_RX_FIFO_EMPTY                           0b00001110  //  3     1     Rx FIFO is empty
#define NRF24_RX_P_NO                                 0b00000000  //  3     1     number of data pipe that received data
#define NRF24_TX_FIFO_FULL                            0b00000001  //  0     0     Tx FIFO is full

// NRF24_REG_OBSERVE_TX
#define NRF24_PLOS_CNT                                0b00000000  //  7     4     number of lost packets
#define NRF24_ARC_CNT                                 0b00000000  //  3     0     number of retransmitted packets

// NRF24_REG_RPD
#define NRF24_RP_BELOW_64_DBM                         0b00000000  //  0     0     received power in the current channel: less than -64 dBm
#define NRF24_RP_ABOVE_64_DBM                         0b00000001  //  0     0                                            more than -64 dBm

// NRF24_REG_FIFO_STATUS
#define NRF24_TX_REUSE                                0b01000000  //  6     6     reusing last transmitted payload
#define NRF24_TX_FIFO_FULL_FLAG                       0b00100000  //  5     5     Tx FIFO is full
#define NRF24_TX_FIFO_EMPTY_FLAG                      0b00010000  //  4     4     Tx FIFO is empty
#define NRF24_RX_FIFO_FULL_FLAG                       0b00000010  //  1     1     Rx FIFO is full
#define NRF24_RX_FIFO_EMPTY_FLAG                      0b00000001  //  0     0     Rx FIFO is empty

// NRF24_REG_DYNPD
#define NRF24_DPL_P5_OFF                              0b00000000  //  5     5     dynamic payload length on pipe 5: disabled (default)
#define NRF24_DPL_P5_ON                               0b00100000  //  5     5                                       enabled
#define NRF24_DPL_P4_OFF                              0b00000000  //  4     4     dynamic payload length on pipe 4: disabled (default)
#define NRF24_DPL_P4_ON                               0b00010000  //  4     4                                       enabled
#define NRF24_DPL_P3_OFF                              0b00000000  //  3     3     dynamic payload length on pipe 3: disabled (default)
#define NRF24_DPL_P3_ON                               0b00001000  //  3     3                                       enabled
#define NRF24_DPL_P2_OFF                              0b00000000  //  2     2     dynamic payload length on pipe 2: disabled (default)
#define NRF24_DPL_P2_ON                               0b00000100  //  2     2                                       enabled
#define NRF24_DPL_P1_OFF                              0b00000000  //  1     1     dynamic payload length on pipe 1: disabled (default)
#define NRF24_DPL_P1_ON                               0b00000010  //  1     1                                       enabled
#define NRF24_DPL_P0_OFF                              0b00000000  //  0     0     dynamic payload length on pipe 0: disabled (default)
#define NRF24_DPL_P0_ON                               0b00000001  //  0     0                                       enabled
#define NRF24_DPL_ALL_OFF                             0b00000000  //  5     0     disable all dynamic payloads
#define NRF24_DPL_ALL_ON                              0b00111111  //  5     0     enable all dynamic payloads

// NRF24_REG_FEATURE
#define NRF24_DPL_OFF                                 0b00000000  //  2     2     dynamic payload length: disabled (default)
#define NRF24_DPL_ON                                  0b00000100  //  2     2                             enabled
#define NRF24_ACK_PAY_OFF                             0b00000000  //  1     1     payload with ACK packets: disabled (default)
#define NRF24_ACK_PAY_ON                              0b00000010  //  1     1                               enabled
#define NRF24_DYN_ACK_OFF                             0b00000000  //  0     0     payloads without ACK: disabled (default)
#define NRF24_DYN_ACK_ON                              0b00000001  //  0     0                           enabled

// Defaults
#define NRF24_DEFAULT_FREQ                            2400
#define NRF24_DEFAULT_DR                              1000
#define NRF24_DEFAULT_POWER                           -12
#define NRF24_DEFAULT_ADDRWIDTH                       5

/*!
  \class nRF24

  \brief Control class for %nRF24 module.
*/
class nRF24: public PhysicalLayer {
  public:
    // introduce PhysicalLayer overloads
    using PhysicalLayer::transmit;
    using PhysicalLayer::receive;
    using PhysicalLayer::startTransmit;
    using PhysicalLayer::readData;

    /*!
      \brief Default constructor.

      \param mod Instance of Module that will be used to communicate with the radio.
    */
    nRF24(Module* mod);

    // basic methods

    /*!
      \brief Initialization method.

      \param freq Carrier frequency in MHz. Defaults to 2400 MHz.

      \param dataRate Data rate to be used in kbps. Defaults to 1000 kbps.

      \param power Output power in dBm. Defaults to -12 dBm.

      \param addrWidth Address width in bytes. Defaults to 5 bytes.

      \returns \ref status_codes
    */
    int16_t begin(
      int16_t freq = NRF24_DEFAULT_FREQ,
      int16_t dataRate = NRF24_DEFAULT_DR,
      int8_t power = NRF24_DEFAULT_POWER,
      uint8_t addrWidth = NRF24_DEFAULT_ADDRWIDTH);

    /*!
      \brief Sets the module to sleep mode.

      \returns \ref status_codes
    */
    int16_t sleep();

    /*!
      \brief Sets the module to standby mode.

      \returns \ref status_codes
    */
    int16_t standby() override;

    /*!
      \brief Blocking binary transmit method.
      Overloads for string-based transmissions are implemented in PhysicalLayer.

      \param data Binary data to be sent.

      \param len Number of bytes to send.

      \param addr Dummy address parameter, to ensure PhysicalLayer compatibility.

      \returns \ref status_codes
    */
    int16_t transmit(uint8_t* data, size_t len, uint8_t addr) override;

    /*!
      \brief Blocking binary receive method.
      Overloads for string-based transmissions are implemented in PhysicalLayer.

      \param data Binary data to be sent.

      \param len Number of bytes to send.

      \returns \ref status_codes
    */
    int16_t receive(uint8_t* data, size_t len) override;

    /*!
      \brief Starts direct mode transmission.

      \param frf Raw RF frequency value. Defaults to 0, required for quick frequency shifts in RTTY.

      \returns \ref status_codes
    */
    int16_t transmitDirect(uint32_t frf = 0) override;

    /*!
      \brief Dummy direct mode reception method, to ensure PhysicalLayer compatibility.

      \returns \ref status_codes
    */
    int16_t receiveDirect() override;

    // interrupt methods

    /*!
      \brief Sets interrupt service routine to call when IRQ activates.

      \param func ISR to call.
    */
    void setIrqAction(void (*func)(void));

    /*!
      \brief Interrupt-driven binary transmit method. IRQ will be activated when full packet is transmitted.
      Overloads for string-based transmissions are implemented in PhysicalLayer.

      \param data Binary data to be sent.

      \param len Number of bytes to send.

      \param addr Dummy address parameter, to ensure PhysicalLayer compatibility.

      \returns \ref status_codes
    */
    int16_t startTransmit(uint8_t* data, size_t len, uint8_t addr) override;

    /*!
      \brief Interrupt-driven receive method. IRQ will be activated when full packet is received.

      \returns \ref status_codes
    */
    int16_t startReceive();

    /*!
      \brief Reads data received after calling startReceive method.

      \param data Pointer to array to save the received binary data.

      \param len Number of bytes that will be received. Must be known in advance for binary transmissions.

      \returns \ref status_codes
    */
    int16_t readData(uint8_t* data, size_t len) override;

    // configuration methods

    /*!
      \brief Sets carrier frequency. Allowed values range from 2400 MHz to 2525 MHz.

      \param freq Carrier frequency to be set in MHz.

      \returns \ref status_codes
    */
    int16_t setFrequency(int16_t freq);

    /*!
      \brief Sets data rate. Allowed values are 2000, 1000 or 250 kbps.

      \param dataRate Data rate to be set in kbps.

      \returns \ref status_codes
    */
    int16_t setDataRate(int16_t dataRate);

    /*!
      \brief Sets output power. Allowed values are -18, -12, -6 or 0 dBm.

      \param power Output power to be set in dBm.

      \returns \ref status_codes
    */
    int16_t setOutputPower(int8_t power);

    /*!
      \brief Sets address width of transmit and receive pipes in bytes. Allowed values are 3, 4 or 5 bytes.

      \param addrWidth Address width to be set in bytes.

      \returns \ref status_codes
    */
    int16_t setAddressWidth(uint8_t addrWidth);

    /*!
      \brief Sets address of transmit pipe. The address width must be the same as the same as the configured in setAddressWidth.

      \param addr Address to which the next packet shall be transmitted.

      \returns \ref status_codes
    */
    int16_t setTransmitPipe(uint8_t* addr);

    /*!
      \brief Sets address of receive pipes 0 or 1. The address width must be the same as the same as the configured in setAddressWidth.

      \param pipeNum Number of pipe to which the address shall be set. Either 0 or 1, other pipes are handled using overloaded method.

      \param addr Address from which %nRF24 shall receive new packets on the specified pipe.

      \returns \ref status_codes
    */
    int16_t setReceivePipe(uint8_t pipeNum, uint8_t* addr);

    /*!
      \brief Sets address of receive pipes 2 - 5. The first 2 - 4 address bytes for these pipes are the same as for address pipe 1, only the last byte can be set.

      \param pipeNum Number of pipe to which the address shall be set. Allowed values range from 2 to 5.

      \param addrByte LSB of address from which %nRF24 shall receive new packets on the specified pipe.

      \returns \ref status_codes
    */
    int16_t setReceivePipe(uint8_t pipeNum, uint8_t addrByte);

    /*!
      \brief Disables specified receive pipe.

      \param pipeNum Receive pipe to be disabled.

      \returns \ref status_codes
    */
    int16_t disablePipe(uint8_t pipeNum);

    /*!
      \brief Gets nRF24 status register.

      \param mask Bit mask to be used on the returned register value.

      \returns Status register value or \ref status_codes
    */
    int16_t getStatus(uint8_t mask = 0xFF);

    /*!
      \brief Checks if carrier was detected during last RX

      \returns Whatever the carrier was above threshold.
    */
    bool isCarrierDetected();

    /*!
      \brief Dummy configuration method, to ensure PhysicalLayer compatibility.

      \param freqDev Dummy frequency deviation parameter, no configuration will be changed.

      \returns \ref status_codes
    */
    int16_t setFrequencyDeviation(float freqDev) override;

     /*!
      \brief Query modem for the packet length of received payload.

      \param update Update received packet length. Will return cached value when set to false.

      \returns Length of last received packet in bytes.
    */
    size_t getPacketLength(bool update = true) override;

    /*!
     \brief Enable CRC filtering and generation.

     \param crcOn Set or unset CRC check.

     \returns \ref status_codes
   */
    int16_t setCrcFiltering(bool crcOn = true);

    /*!
     \brief Enable or disable auto-acknowledge packets on all pipes

     \param autoAckOn Enable (true) or disable (false) auto-acks.

     \returns \ref status_codes
   */
    int16_t setAutoAck(bool autoAckOn = true);

    /*!
     \brief Enable or disable auto-acknowledge packets on given pipe.

     \param pipeNum Number of pipe to which enable / disable auto-acks.

     \param autoAckOn Enable (true) or disable (false) auto-acks.

     \returns \ref status_codes
   */
    int16_t setAutoAck(uint8_t pipeNum, bool autoAckOn);

    /*!
      \brief Dummy data shaping configuration method, to ensure PhysicalLayer compatibility.

      \param sh Ignored.

      \returns \ref status_codes
    */
    int16_t setDataShaping(uint8_t sh) override;

    /*!
      \brief Dummy encoding configuration method, to ensure PhysicalLayer compatibility.

      \param sh Ignored.

      \returns \ref status_codes
    */
    int16_t setEncoding(uint8_t encoding) override;

    /*!
     \brief Dummy random method, to ensure PhysicalLayer compatibility.

     \returns Always returns 0.
   */
    uint8_t random();

    /*!
      \brief Dummy method, to ensure PhysicalLayer compatibility.

      \param func Ignored.
    */
    void setDirectAction(void (*func)(void));

    /*!
      \brief Dummy method, to ensure PhysicalLayer compatibility.

      \param pin Ignored.
    */
    void readBit(RADIOLIB_PIN_TYPE pin);

#if !defined(RADIOLIB_GODMODE) && !defined(RADIOLIB_LOW_LEVEL)
  protected:
#endif
    Module* _mod;

    void SPIreadRxPayload(uint8_t* data, uint8_t numBytes);
    void SPIwriteTxPayload(uint8_t* data, uint8_t numBytes);
    void SPItransfer(uint8_t cmd, bool write = false, uint8_t* dataOut = NULL, uint8_t* dataIn = NULL, uint8_t numBytes = 0);

#if !defined(RADIOLIB_GODMODE)
  protected:
#endif

    int16_t _freq = NRF24_DEFAULT_FREQ;
    int16_t _dataRate = NRF24_DEFAULT_DR;
    int8_t _power = NRF24_DEFAULT_POWER;
    uint8_t _addrWidth = NRF24_DEFAULT_ADDRWIDTH;

    int16_t config();
    void clearIRQ();
};

#endif
