/*!
 * @file      radio.cpp
 *
 * @brief     Radio driver API definition
 *
 * @copyright Revised BSD License, see file LICENSE.
 *
 * @code
 *                ______                              _
 *               / _____)             _              | |
 *              ( (____  _____ ____ _| |_ _____  ____| |__
 *               \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 *               _____) ) ____| | | || |_| ____( (___| | | |
 *              (______/|_____)_|_|_| \__)_____)\____)_| |_|
 *              (C)2013-2017 Semtech
 *
 * @endcode
 *
 * @author    Miguel Luis ( Semtech )
 *
 * @author    Gregory Cristian ( Semtech )
 */
#include <math.h>
#include <string.h>
#include "boards/mcu/board.h"
#include "radio/radio.h"
#include "sx126x.h"
#include "boards/sx126x/sx126x-board.h"
#include "boards/mcu/timer.h"
#include "loraEvents.h"

loraEvents_t *_events;
/* Tx and Rx timers
 */
TimerEvent_t TxTimeoutTimer;
TimerEvent_t RxTimeoutTimer;

/** Enforce low datarate optimization */
bool force_low_dr_opt = false;

/*!
 * @brief Initializes the radio
 *
 * @param  events Structure containing the driver callback functions
 */
void RadioInit(RadioEvents_t *events);

/*!
 * @brief Initializes the radio after deep sleep of the CPU
 *
 * @param  events Structure containing the driver callback functions
 */
void RadioReInit(RadioEvents_t *events);

/*!
 * Return current radio status
 *
 * @retval Radio status.[RF_IDLE, RF_RX_RUNNING, RF_TX_RUNNING]
 */
RadioState_t RadioGetStatus(void);

/*!
 * @brief Configures the radio with the given modem
 *
 * @param  modem Modem to be used [0: FSK, 1: LoRa]
 */
void RadioSetModem(RadioModems_t modem);

/*!
 * @brief Sets the channel frequency
 *
 * @param  freq         Channel RF frequency
 */
void RadioSetChannel(uint32_t freq);

/*!
 * @brief Checks if the channel is free for the given time
 *
 * @param  modem      Radio modem to be used [0: FSK, 1: LoRa]
 * @param  freq       Channel RF frequency
 * @param  rssiThresh RSSI threshold
 * @param  maxCarrierSenseTime Max time while the RSSI is measured
 *
 * @retval isFree         [true: Channel is free, false: Channel is not free]
 */
bool RadioIsChannelFree(RadioModems_t modem, uint32_t freq, int16_t rssiThresh, uint32_t maxCarrierSenseTime);

/*!
 * @brief Generates a 32 bits random value based on the RSSI readings
 *
 * @remark This function sets the radio in LoRa modem mode and disables
 *         all interrupts.
 *         After calling this function either Radio.SetRxConfig or
 *         Radio.SetTxConfig functions must be called.
 *
 * @retval randomValue    32 bits random value
 */
uint32_t RadioRandom(void);

/*!
 * @brief Sets the reception parameters
 *
 * @param  modem        Radio modem to be used [0: FSK, 1: LoRa]
 * @param  bandwidth    Sets the bandwidth
 *                          FSK : >= 2600 and <= 250000 Hz
 *                          LoRa: [0: 125 kHz, 1: 250 kHz,
 *                                 2: 500 kHz, 3: Reserved]
 * @param  datarate     Sets the Datarate
 *                          FSK : 600..300000 bits/s
 *                          LoRa: [6: 64, 7: 128, 8: 256, 9: 512,
 *                                10: 1024, 11: 2048, 12: 4096  chips]
 * @param  coderate     Sets the coding rate (LoRa only)
 *                          FSK : N/A ( set to 0 )
 *                          LoRa: [1: 4/5, 2: 4/6, 3: 4/7, 4: 4/8]
 * @param  bandwidthAfc Sets the AFC Bandwidth (FSK only)
 *                          FSK : >= 2600 and <= 250000 Hz
 *                          LoRa: N/A ( set to 0 )
 * @param  preambleLen  Sets the Preamble length
 *                          FSK : Number of bytes
 *                          LoRa: Length in symbols (the hardware adds 4 more symbols)
 * @param  symbTimeout  Sets the RxSingle timeout value
 *                          FSK : timeout in number of bytes
 *                          LoRa: timeout in symbols
 * @param  fixLen       Fixed length packets [0: variable, 1: fixed]
 * @param  payloadLen   Sets payload length when fixed length is used
 * @param  crcOn        Enables/Disables the CRC [0: OFF, 1: ON]
 * @param  FreqHopOn    Enables disables the intra-packet frequency hopping
 *                          FSK : N/A ( set to 0 )
 *                          LoRa: [0: OFF, 1: ON]
 * @param  HopPeriod    Number of symbols between each hop
 *                          FSK : N/A ( set to 0 )
 *                          LoRa: Number of symbols
 * @param  iqInverted   Inverts IQ signals (LoRa only)
 *                          FSK : N/A ( set to 0 )
 *                          LoRa: [0: not inverted, 1: inverted]
 * @param  rxContinuous Sets the reception in continuous mode
 *                          [false: single mode, true: continuous mode]
 */
void RadioSetRxConfig(RadioModems_t modem, uint32_t bandwidth,
					  uint32_t datarate, uint8_t coderate,
					  uint32_t bandwidthAfc, uint16_t preambleLen,
					  uint16_t symbTimeout, bool fixLen,
					  uint8_t payloadLen,
					  bool crcOn, bool FreqHopOn, uint8_t HopPeriod,
					  bool iqInverted, bool rxContinuous);

/*!
 * \brief Sets the transmission parameters
 *
 * @param  modem        Radio modem to be used [0: FSK, 1: LoRa]
 * @param  power        Sets the output power [dBm]
 * @param  fdev         Sets the frequency deviation (FSK only)
 *                          FSK : [Hz]
 *                          LoRa: 0
 * @param  bandwidth    Sets the bandwidth (LoRa only)
 *                          FSK : 0
 *                          LoRa: [0: 125 kHz, 1: 250 kHz,
 *                                 2: 500 kHz, 3: Reserved]
 * @param  datarate     Sets the Datarate
 *                          FSK : 600..300000 bits/s
 *                          LoRa: [6: 64, 7: 128, 8: 256, 9: 512,
 *                                10: 1024, 11: 2048, 12: 4096  chips]
 * @param  coderate     Sets the coding rate (LoRa only)
 *                          FSK : N/A ( set to 0 )
 *                          LoRa: [1: 4/5, 2: 4/6, 3: 4/7, 4: 4/8]
 * @param  preambleLen  Sets the preamble length
 *                          FSK : Number of bytes
 *                          LoRa: Length in symbols (the hardware adds 4 more symbols)
 * @param  fixLen       Fixed length packets [0: variable, 1: fixed]
 * @param  crcOn        Enables disables the CRC [0: OFF, 1: ON]
 * @param  FreqHopOn    Enables disables the intra-packet frequency hopping
 *                          FSK : N/A ( set to 0 )
 *                          LoRa: [0: OFF, 1: ON]
 * @param  HopPeriod    Number of symbols between each hop
 *                          FSK : N/A ( set to 0 )
 *                          LoRa: Number of symbols
 * @param  iqInverted   Inverts IQ signals (LoRa only)
 *                          FSK : N/A ( set to 0 )
 *                          LoRa: [0: not inverted, 1: inverted]
 * @param  timeout      Transmission timeout [ms]
 */
void RadioSetTxConfig(RadioModems_t modem, int8_t power, uint32_t fdev,
					  uint32_t bandwidth, uint32_t datarate,
					  uint8_t coderate, uint16_t preambleLen,
					  bool fixLen, bool crcOn, bool FreqHopOn,
					  uint8_t HopPeriod, bool iqInverted, uint32_t timeout);

/*!
 * @brief Checks if the given RF frequency is supported by the hardware
 *
 * @param  frequency RF frequency to be checked
 * @retval isSupported [true: supported, false: unsupported]
 */
bool RadioCheckRfFrequency(uint32_t frequency);

/*!
 * @brief Computes the packet time on air in ms for the given payload
 *
 * @remark Can only be called once SetRxConfig or SetTxConfig have been called
 *
 * @param  modem      Radio modem to be used [0: FSK, 1: LoRa]
 * @param  pktLen     Packet payload length
 *
 * @retval airTime        Computed airTime (ms) for the given packet payload length
 */
uint32_t RadioTimeOnAir(RadioModems_t modem, uint8_t pktLen);

/*!
 * @brief Sends the buffer of size. Prepares the packet to be sent and sets
 *        the radio in transmission
 *
 * @param : buffer     Buffer pointer
 * @param : size       Buffer size
 */
void RadioSend(uint8_t *buffer, uint8_t size);

/*!
 * @brief Sets the radio in sleep mode
 */
void RadioSleep(void);

/*!
 * @brief Sets the radio in standby mode
 */
void RadioStandby(void);

/*!
 * @brief Sets the radio in reception mode for the given time
 * @param  timeout Reception timeout [ms]
 *                     [0: continuous, others timeout]
 */
void RadioRx(uint32_t timeout);

/*!
 * @brief Set Channel Activity Detection parameters
 *
 * \param   cadSymbolNum   The number of symbol to use for CAD operations
 *                             [LORA_CAD_01_SYMBOL, LORA_CAD_02_SYMBOL,
 *                              LORA_CAD_04_SYMBOL, LORA_CAD_08_SYMBOL,
 *                              LORA_CAD_16_SYMBOL]
 * \param   cadDetPeak     Limit for detection of SNR peak used in the CAD
 * \param   cadDetMin      Set the minimum symbol recognition for CAD
 * \param   cadExitMode    Operation to be done at the end of CAD action
 *                             [LORA_CAD_ONLY, LORA_CAD_RX, LORA_CAD_LBT]
 * \param   cadTimeout     Defines the timeout value to abort the CAD activity
 */
void RadioSetCadParams(uint8_t cadSymbolNum, uint8_t cadDetPeak, uint8_t cadDetMin, uint8_t cadExitMode, uint32_t cadTimeout);

/*!
 * @brief Start a Channel Activity Detection
 *
 * @remark Before calling this function CAD parameters need to be set first! \
 * 		Use RadioSetCadParams to setup the parameters or directly in the SX126x low level functions
 *      RadioSetCadParams(uint8_t cadSymbolNum, uint8_t cadDetPeak, uint8_t cadDetMin, uint8_t cadExitMode, uint32_t cadTimeout);
 *              IRQ_CAD_DONE | IRQ_CAD_ACTIVITY_DETECTED, \
 *              IRQ_RADIO_NONE, IRQ_RADIO_NONE); \
 */
void RadioStartCad(void);

/*!
 * @brief Sets the radio in continuous wave transmission mode
 *
 * @param : freq       Channel RF frequency
 * @param : power      Sets the output power [dBm]
 * @param : time       Transmission mode timeout [s]
 */
void RadioSetTxContinuousWave(uint32_t freq, int8_t power, uint16_t time);

/*!
 * @brief Reads the current RSSI value
 *
 * @retval rssiValue Current RSSI value in [dBm]
 */
int16_t RadioRssi(RadioModems_t modem);

/*!
 * @brief Writes the radio register at the specified address
 *
 * @param : addr Register address
 * @param : data New register value
 */
void RadioWrite(uint16_t addr, uint8_t data);

/*!
 * @brief Reads the radio register at the specified address
 *
 * @param : addr Register address
 * @retval data Register value
 */
uint8_t RadioRead(uint16_t addr);

/*!
 * @brief Writes multiple radio registers starting at address
 *
 * @param  addr   First Radio register address
 * @param  buffer Buffer containing the new register's values
 * @param  size   Number of registers to be written
 */
void RadioWriteBuffer(uint16_t addr, uint8_t *buffer, uint8_t size);

/*!
 * @brief Reads multiple radio registers starting at address
 *
 * @param  addr First Radio register address
 * @param  buffer Buffer where to copy the registers data
 * @param  size Number of registers to be read
 */
void RadioReadBuffer(uint16_t addr, uint8_t *buffer, uint8_t size);

/*!
 * @brief Sets the maximum payload length.
 *
 * @param  modem      Radio modem to be used [0: FSK, 1: LoRa]
 * @param  max        Maximum payload length in bytes
 */
void RadioSetMaxPayloadLength(RadioModems_t modem, uint8_t max);

/*!
 * @brief Sets the network to public or private. Updates the sync byte.
 *
 * @remark Applies to LoRa modem only
 *
 * @param  enable if true, it enables a public network
 */
void RadioSetPublicNetwork(bool enable);

/*!
 * \brief Sets a custom Sync-Word. Updates the sync byte.
 *
 * \remark ATTENTION, changes the LoRaWAN sync word as well. Use with care.
 *
 * \param  syncword 2 byte custom Sync-Word to be used
 */
void RadioSetCustomSyncWord(uint16_t syncword);

/*!
 * \brief Get current Sync-Word.
 *
 * \param  syncword 2 byte custom Sync-Word in use
 */
uint16_t RadioGetSyncWord(void);

/*!
 * @brief Gets the time required for the board plus radio to get out of sleep.[ms]
 *
 * @retval time Radio plus board wakeup time in ms.
 */
uint32_t RadioGetWakeupTime(void);

/*!
 * @brief Process radio irq in background task (nRF52 & ESP32)
 */
void RadioBgIrqProcess(void);

/*!
 * @brief Process radio irq
 */
void RadioIrqProcess(void);

/*!
 * @brief Process radio irq after deep sleep of the CPU
 */
void RadioIrqProcessAfterDeepSleep(void);

/*!
 * @brief Sets the radio in reception mode with Max LNA gain for the given time
 * @param  timeout Reception timeout [ms]
 *                     [0: continuous, others timeout]
 */
void RadioRxBoosted(uint32_t timeout);

/*!
 * @brief Sets the Rx duty cycle management parameters
 *
 * @param   rxTime        Structure describing reception timeout value
 * @param   sleepTime     Structure describing sleep timeout value
 */
void RadioSetRxDutyCycle(uint32_t rxTime, uint32_t sleepTime);

/*!
 * @brief Enforce usage of Low Datarate optimization
 *
 * @param   enforce       True = Enforce usage of Low Datarate optimization
 */
void RadioEnforceLowDRopt(bool enforce);

/*!
 * Radio driver structure initialization
 */
const struct Radio_s Radio =
	{
		RadioInit,
		RadioReInit,
		RadioGetStatus,
		RadioSetModem,
		RadioSetChannel,
		RadioIsChannelFree,
		RadioRandom,
		RadioSetRxConfig,
		RadioSetTxConfig,
		RadioCheckRfFrequency,
		RadioTimeOnAir,
		RadioSend,
		RadioSleep,
		RadioStandby,
		RadioRx,
		RadioSetCadParams,
		RadioStartCad,
		RadioSetTxContinuousWave,
		RadioRssi,
		RadioWrite,
		RadioRead,
		RadioWriteBuffer,
		RadioReadBuffer,
		RadioSetMaxPayloadLength,
		RadioSetPublicNetwork,
		RadioSetCustomSyncWord,
		RadioGetSyncWord,
		RadioGetWakeupTime,
		RadioBgIrqProcess,
		RadioIrqProcess,
		RadioIrqProcessAfterDeepSleep,
		// Available on SX126x only
		RadioRxBoosted,
		RadioEnforceLowDRopt,
		RadioSetRxDutyCycle,
};

/*
 * Local types definition
 */

/*!
 * FSK bandwidth definition
 */
typedef struct
{
	uint32_t bandwidth;
	uint8_t RegValue;
} FskBandwidth_t;

/*!
 * Precomputed FSK bandwidth registers values
 */
const FskBandwidth_t FskBandwidths[] =
	{
		{4800, 0x1F},
		{5800, 0x17},
		{7300, 0x0F},
		{9700, 0x1E},
		{11700, 0x16},
		{14600, 0x0E},
		{19500, 0x1D},
		{23400, 0x15},
		{29300, 0x0D},
		{39000, 0x1C},
		{46900, 0x14},
		{58600, 0x0C},
		{78200, 0x1B},
		{93800, 0x13},
		{117300, 0x0B},
		{156200, 0x1A},
		{187200, 0x12},
		{234300, 0x0A},
		{312000, 0x19},
		{373600, 0x11},
		{467000, 0x09},
		{500000, 0x00}, // Invalid Bandwidth
};

// const RadioLoRaBandwidths_t Bandwidths[] = {LORA_BW_125, LORA_BW_250, LORA_BW_500};
const RadioLoRaBandwidths_t Bandwidths[] = {LORA_BW_125, LORA_BW_250, LORA_BW_500, LORA_BW_062, LORA_BW_041, LORA_BW_031, LORA_BW_020, LORA_BW_015, LORA_BW_010, LORA_BW_007};

//                                          SF12    SF11    SF10    SF9    SF8    SF7
static double RadioLoRaSymbTime[3][6] = {{32.768, 16.384, 8.192, 4.096, 2.048, 1.024}, // 125 KHz
										 {16.384, 8.192, 4.096, 2.048, 1.024, 0.512},  // 250 KHz
										 {8.192, 4.096, 2.048, 1.024, 0.512, 0.256}};  // 500 KHz

uint8_t MaxPayloadLength = 0xFF;

uint32_t TxTimeout = 0;
uint32_t RxTimeout = 0;

bool RxContinuous = false;

PacketStatus_t RadioPktStatus;
uint8_t RadioRxPayload[255];

#if defined(ESP32)
bool DRAM_ATTR IrqFired = false;
#else
bool IrqFired = false;
#endif

bool TimerRxTimeout = false;
bool TimerTxTimeout = false;

RadioModems_t _modem;

bool hasCustomSyncWord = false;

/*
 * SX126x DIO IRQ callback functions prototype
 */

/*!
 * @brief DIO 0 IRQ callback
 */
void RadioOnDioIrq(void);

/*!
 * @brief Tx timeout timer callback
 */
void RadioOnTxTimeoutIrq(void);

/*!
 * @brief Rx timeout timer callback
 */
void RadioOnRxTimeoutIrq(void);

/*
 * Private global variables
 */

/*!
 * Holds the current network type for the radio
 */
typedef struct
{
	bool Previous;
	bool Current;
} RadioPublicNetwork_t;

static RadioPublicNetwork_t RadioPublicNetwork = {false};

/*!
 * Radio callbacks variable
 */
static RadioEvents_t *RadioEvents;

/*
 * Public global variables
 */

/*!
 * Radio hardware and global parameters
 */
SX126x_t SX126x;

/*!
 * Returns the known FSK bandwidth registers value
 *
 * @param  bandwidth Bandwidth value in Hz
 * @retval regValue Bandwidth register value.
 */
static uint8_t RadioGetFskBandwidthRegValue(uint32_t bandwidth)
{
	uint8_t i;

	if (bandwidth == 0)
	{
		return (0x1F);
	}

	for (i = 0; i < (sizeof(FskBandwidths) / sizeof(FskBandwidth_t)) - 1; i++)
	{
		if ((bandwidth >= FskBandwidths[i].bandwidth) && (bandwidth < FskBandwidths[i + 1].bandwidth))
		{
			return FskBandwidths[i + 1].RegValue;
		}
	}
	// In case value not found, return bandwidth 0
	return (0x1F);
	// ERROR: Value not found
	// while (1)
	// 	;
}

void RadioInit(RadioEvents_t *events)
{
	RadioEvents = events;
	SX126xInit(RadioOnDioIrq);
	SX126xSetStandby(STDBY_RC);
	if (_hwConfig.USE_LDO)
	{
		SX126xSetRegulatorMode(USE_LDO);
	}
	else
	{
		SX126xSetRegulatorMode(USE_DCDC);
	}

	SX126xSetBufferBaseAddress(0x00, 0x00);
	SX126xSetTxParams(0, RADIO_RAMP_200_US);
	SX126xSetDioIrqParams(IRQ_RADIO_ALL, IRQ_RADIO_ALL, IRQ_RADIO_NONE, IRQ_RADIO_NONE);

	// Initialize driver timeout timers
	TxTimeoutTimer.oneShot = true;
	RxTimeoutTimer.oneShot = true;
	TimerInit(&TxTimeoutTimer, RadioOnTxTimeoutIrq);
	TimerInit(&RxTimeoutTimer, RadioOnRxTimeoutIrq);

	IrqFired = false;
}

void RadioReInit(RadioEvents_t *events)
{
	RadioEvents = events;
	SX126xReInit(RadioOnDioIrq);

	// Initialize driver timeout timers
	TxTimeoutTimer.oneShot = true;
	RxTimeoutTimer.oneShot = true;
	TimerInit(&TxTimeoutTimer, RadioOnTxTimeoutIrq);
	TimerInit(&RxTimeoutTimer, RadioOnRxTimeoutIrq);

	IrqFired = false;
}

RadioState_t RadioGetStatus(void)
{
	switch (SX126xGetOperatingMode())
	{
	case MODE_TX:
		return RF_TX_RUNNING;
	case MODE_RX:
		return RF_RX_RUNNING;
	case MODE_CAD:
		return RF_CAD;
	default:
		return RF_IDLE;
	}
}

void RadioSetModem(RadioModems_t modem)
{
	switch (modem)
	{
	default:
	case MODEM_FSK:
		SX126xSetPacketType(PACKET_TYPE_GFSK);
		// When switching to GFSK mode the LoRa SyncWord register value is reset
		// Thus, we also reset the RadioPublicNetwork variable
		RadioPublicNetwork.Current = false;
		_modem = modem;
		break;
	case MODEM_LORA:
		SX126xSetPacketType(PACKET_TYPE_LORA);
		// check first if a custom SyncWord is set
		if (!hasCustomSyncWord)
		{
			// Public/Private network register is reset when switching modems
			if (RadioPublicNetwork.Current != RadioPublicNetwork.Previous)
			{
				RadioPublicNetwork.Current = RadioPublicNetwork.Previous;
				RadioSetPublicNetwork(RadioPublicNetwork.Current);
			}
		}

		_modem = modem;
		break;
	}
}

void RadioSetChannel(uint32_t freq)
{
	SX126xSetRfFrequency(freq);
}

bool RadioIsChannelFree(RadioModems_t modem, uint32_t freq, int16_t rssiThresh, uint32_t maxCarrierSenseTime)
{
	bool status = true;
	int16_t rssi = 0;
	uint32_t carrierSenseTime = 0;

	if (RadioGetStatus() != RF_IDLE)
	{
		return false;
	}

	RadioSetModem(modem);

	RadioSetChannel(freq);

	RadioRx(0);

	delay(1);

	carrierSenseTime = TimerGetCurrentTime();

	// Perform carrier sense for maxCarrierSenseTime
	while (TimerGetElapsedTime(carrierSenseTime) < maxCarrierSenseTime)
	{
		rssi = RadioRssi(modem);

		if (rssi > rssiThresh)
		{
			status = false;
			break;
		}
	}
	RadioSleep();
	return status;
}

uint32_t RadioRandom(void)
{
	uint32_t rnd = 0;

	/*
	 * Radio setup for random number generation
	 */
	// Set LoRa modem ON
	RadioSetModem(MODEM_LORA);

	// Set radio in continuous reception
	SX126xSetRx(0);

	rnd = SX126xGetRandom();
	RadioSleep();

	return rnd;
}

void RadioSetRxConfig(RadioModems_t modem, uint32_t bandwidth,
					  uint32_t datarate, uint8_t coderate,
					  uint32_t bandwidthAfc, uint16_t preambleLen,
					  uint16_t symbTimeout, bool fixLen,
					  uint8_t payloadLen,
					  bool crcOn, bool freqHopOn, uint8_t hopPeriod,
					  bool iqInverted, bool rxContinuous)
{

	RxContinuous = rxContinuous;
	if (rxContinuous == true)
	{
		symbTimeout = 0;
	}
	if (fixLen == true)
	{
		MaxPayloadLength = payloadLen;
	}
	else
	{
		MaxPayloadLength = 0xFF;
	}

	switch (modem)
	{
	case MODEM_FSK:
		SX126xSetStopRxTimerOnPreambleDetect(false);
		SX126x.ModulationParams.PacketType = PACKET_TYPE_GFSK;

		SX126x.ModulationParams.Params.Gfsk.BitRate = datarate;
		SX126x.ModulationParams.Params.Gfsk.ModulationShaping = MOD_SHAPING_G_BT_1;
		SX126x.ModulationParams.Params.Gfsk.Bandwidth = RadioGetFskBandwidthRegValue(bandwidth);

		SX126x.PacketParams.PacketType = PACKET_TYPE_GFSK;
		SX126x.PacketParams.Params.Gfsk.PreambleLength = (preambleLen << 3); // convert byte into bit
		SX126x.PacketParams.Params.Gfsk.PreambleMinDetect = RADIO_PREAMBLE_DETECTOR_08_BITS;
		SX126x.PacketParams.Params.Gfsk.SyncWordLength = 3 << 3; // convert byte into bit
		SX126x.PacketParams.Params.Gfsk.AddrComp = RADIO_ADDRESSCOMP_FILT_OFF;
		SX126x.PacketParams.Params.Gfsk.HeaderType = (fixLen == true) ? RADIO_PACKET_FIXED_LENGTH : RADIO_PACKET_VARIABLE_LENGTH;
		SX126x.PacketParams.Params.Gfsk.PayloadLength = MaxPayloadLength;
		if (crcOn == true)
		{
			SX126x.PacketParams.Params.Gfsk.CrcLength = RADIO_CRC_2_BYTES_CCIT;
		}
		else
		{
			SX126x.PacketParams.Params.Gfsk.CrcLength = RADIO_CRC_OFF;
		}
		SX126x.PacketParams.Params.Gfsk.DcFree = RADIO_DC_FREEWHITENING;

		RadioStandby();
		RadioSetModem((SX126x.ModulationParams.PacketType == PACKET_TYPE_GFSK) ? MODEM_FSK : MODEM_LORA);
		SX126xSetModulationParams(&SX126x.ModulationParams);
		SX126xSetPacketParams(&SX126x.PacketParams);
		uint8_t syncWord[8];
		syncWord[0] = 0xC1;
		syncWord[1] = 0x94;
		syncWord[2] = 0xC1;
		syncWord[3] = 0x00;
		syncWord[4] = 0x00;
		syncWord[5] = 0x00;
		syncWord[6] = 0x00;
		syncWord[7] = 0x00;
		SX126xSetSyncWord(syncWord);
		// SX126xSetSyncWord( ( uint8_t[] ){ 0xC1, 0x94, 0xC1, 0x00, 0x00, 0x00, 0x00, 0x00 } );
		SX126xSetWhiteningSeed(0x01FF);

		RxTimeout = (uint32_t)(symbTimeout * ((1.0 / (double)datarate) * 8.0) * 1000);
		break;

	case MODEM_LORA:
		SX126xSetStopRxTimerOnPreambleDetect(false);
		SX126xSetLoRaSymbNumTimeout(symbTimeout);
		SX126x.ModulationParams.PacketType = PACKET_TYPE_LORA;
		SX126x.ModulationParams.Params.LoRa.SpreadingFactor = (RadioLoRaSpreadingFactors_t)datarate;
		SX126x.ModulationParams.Params.LoRa.Bandwidth = Bandwidths[bandwidth];
		SX126x.ModulationParams.Params.LoRa.CodingRate = (RadioLoRaCodingRates_t)coderate;

		if (((bandwidth == 0) && ((datarate == 11) || (datarate == 12))) ||
			((bandwidth == 1) && (datarate == 12)) || force_low_dr_opt)
		{
			SX126x.ModulationParams.Params.LoRa.LowDatarateOptimize = 0x01;
		}
		else
		{
			SX126x.ModulationParams.Params.LoRa.LowDatarateOptimize = 0x00;
		}

		SX126x.PacketParams.PacketType = PACKET_TYPE_LORA;

		if ((SX126x.ModulationParams.Params.LoRa.SpreadingFactor == LORA_SF5) ||
			(SX126x.ModulationParams.Params.LoRa.SpreadingFactor == LORA_SF6))
		{
			if (preambleLen < 12)
			{
				SX126x.PacketParams.Params.LoRa.PreambleLength = 12;
			}
			else
			{
				SX126x.PacketParams.Params.LoRa.PreambleLength = preambleLen;
			}
		}
		else
		{
			SX126x.PacketParams.Params.LoRa.PreambleLength = preambleLen;
		}

		SX126x.PacketParams.Params.LoRa.HeaderType = (RadioLoRaPacketLengthsMode_t)fixLen;

		SX126x.PacketParams.Params.LoRa.PayloadLength = MaxPayloadLength;
		SX126x.PacketParams.Params.LoRa.CrcMode = (RadioLoRaCrcModes_t)crcOn;
		SX126x.PacketParams.Params.LoRa.InvertIQ = (RadioLoRaIQModes_t)iqInverted;

		RadioSetModem((SX126x.ModulationParams.PacketType == PACKET_TYPE_GFSK) ? MODEM_FSK : MODEM_LORA);
		SX126xSetModulationParams(&SX126x.ModulationParams);
		SX126xSetPacketParams(&SX126x.PacketParams);

		// WORKAROUND - Optimizing the Inverted IQ Operation, see DS_SX1261-2_V1.2 datasheet chapter 15.4
		if (SX126x.PacketParams.Params.LoRa.InvertIQ == LORA_IQ_INVERTED)
		{
			// RegIqPolaritySetup = @address 0x0736
			SX126xWriteRegister(0x0736, SX126xReadRegister(0x0736) & ~(1 << 2));
		}
		else
		{
			// RegIqPolaritySetup @address 0x0736
			SX126xWriteRegister(0x0736, SX126xReadRegister(0x0736) | (1 << 2));
		}
		// WORKAROUND END

		// Timeout Max, Timeout handled directly in SetRx function
		RxTimeout = RXTIMEOUT_LORA_MAX;

		break;
	}
}

void RadioSetTxConfig(RadioModems_t modem, int8_t power, uint32_t fdev,
					  uint32_t bandwidth, uint32_t datarate,
					  uint8_t coderate, uint16_t preambleLen,
					  bool fixLen, bool crcOn, bool freqHopOn,
					  uint8_t hopPeriod, bool iqInverted, uint32_t timeout)
{

	switch (modem)
	{
	case MODEM_FSK:
		SX126x.ModulationParams.PacketType = PACKET_TYPE_GFSK;
		SX126x.ModulationParams.Params.Gfsk.BitRate = datarate;

		SX126x.ModulationParams.Params.Gfsk.ModulationShaping = MOD_SHAPING_G_BT_1;
		SX126x.ModulationParams.Params.Gfsk.Bandwidth = RadioGetFskBandwidthRegValue(bandwidth);
		SX126x.ModulationParams.Params.Gfsk.Fdev = fdev;

		SX126x.PacketParams.PacketType = PACKET_TYPE_GFSK;
		SX126x.PacketParams.Params.Gfsk.PreambleLength = (preambleLen << 3); // convert byte into bit
		SX126x.PacketParams.Params.Gfsk.PreambleMinDetect = RADIO_PREAMBLE_DETECTOR_08_BITS;
		SX126x.PacketParams.Params.Gfsk.SyncWordLength = 3 << 3; // convert byte into bit
		SX126x.PacketParams.Params.Gfsk.AddrComp = RADIO_ADDRESSCOMP_FILT_OFF;
		SX126x.PacketParams.Params.Gfsk.HeaderType = (fixLen == true) ? RADIO_PACKET_FIXED_LENGTH : RADIO_PACKET_VARIABLE_LENGTH;

		if (crcOn == true)
		{
			SX126x.PacketParams.Params.Gfsk.CrcLength = RADIO_CRC_2_BYTES_CCIT;
		}
		else
		{
			SX126x.PacketParams.Params.Gfsk.CrcLength = RADIO_CRC_OFF;
		}
		SX126x.PacketParams.Params.Gfsk.DcFree = RADIO_DC_FREEWHITENING;

		RadioStandby();
		RadioSetModem((SX126x.ModulationParams.PacketType == PACKET_TYPE_GFSK) ? MODEM_FSK : MODEM_LORA);
		SX126xSetModulationParams(&SX126x.ModulationParams);
		SX126xSetPacketParams(&SX126x.PacketParams);
		uint8_t syncWord[8];
		syncWord[0] = 0xC1;
		syncWord[1] = 0x94;
		syncWord[2] = 0xC1;
		syncWord[3] = 0x00;
		syncWord[4] = 0x00;
		syncWord[5] = 0x00;
		syncWord[6] = 0x00;
		syncWord[7] = 0x00;
		SX126xSetSyncWord(syncWord);
		// SX126xSetSyncWord( ( uint8_t[] ){ 0xC1, 0x94, 0xC1, 0x00, 0x00, 0x00, 0x00, 0x00 } );
		SX126xSetWhiteningSeed(0x01FF);
		break;

	case MODEM_LORA:
		SX126x.ModulationParams.PacketType = PACKET_TYPE_LORA;
		SX126x.ModulationParams.Params.LoRa.SpreadingFactor = (RadioLoRaSpreadingFactors_t)datarate;
		SX126x.ModulationParams.Params.LoRa.Bandwidth = Bandwidths[bandwidth];
		SX126x.ModulationParams.Params.LoRa.CodingRate = (RadioLoRaCodingRates_t)coderate;

		if (((bandwidth == 0) && ((datarate == 11) || (datarate == 12))) ||
			((bandwidth == 1) && (datarate == 12)) || force_low_dr_opt)
		{
			SX126x.ModulationParams.Params.LoRa.LowDatarateOptimize = 0x01;
		}
		else
		{
			SX126x.ModulationParams.Params.LoRa.LowDatarateOptimize = 0x00;
		}

		SX126x.PacketParams.PacketType = PACKET_TYPE_LORA;

		if ((SX126x.ModulationParams.Params.LoRa.SpreadingFactor == LORA_SF5) ||
			(SX126x.ModulationParams.Params.LoRa.SpreadingFactor == LORA_SF6))
		{
			if (preambleLen < 12)
			{
				SX126x.PacketParams.Params.LoRa.PreambleLength = 12;
			}
			else
			{
				SX126x.PacketParams.Params.LoRa.PreambleLength = preambleLen;
			}
		}
		else
		{
			SX126x.PacketParams.Params.LoRa.PreambleLength = preambleLen;
		}

		SX126x.PacketParams.Params.LoRa.HeaderType = (RadioLoRaPacketLengthsMode_t)fixLen;
		SX126x.PacketParams.Params.LoRa.PayloadLength = MaxPayloadLength;
		SX126x.PacketParams.Params.LoRa.CrcMode = (RadioLoRaCrcModes_t)crcOn;
		SX126x.PacketParams.Params.LoRa.InvertIQ = (RadioLoRaIQModes_t)iqInverted;

		RadioStandby();
		RadioSetModem((SX126x.ModulationParams.PacketType == PACKET_TYPE_GFSK) ? MODEM_FSK : MODEM_LORA);
		SX126xSetModulationParams(&SX126x.ModulationParams);
		SX126xSetPacketParams(&SX126x.PacketParams);
		break;
	}

	// WORKAROUND - Modulation Quality with 500 kHz LoRa Bandwidth, see DS_SX1261-2_V1.2 datasheet chapter 15.1
	if ((modem == MODEM_LORA) && (SX126x.ModulationParams.Params.LoRa.Bandwidth == LORA_BW_500))
	{
		// RegTxModulation = @address 0x0889
		SX126xWriteRegister(0x0889, SX126xReadRegister(0x0889) & ~(1 << 2));
	}
	else
	{
		// RegTxModulation = @address 0x0889
		SX126xWriteRegister(0x0889, SX126xReadRegister(0x0889) | (1 << 2));
	}
	// WORKAROUND END

	SX126xSetRfTxPower(power);
	TxTimeout = timeout;
}

bool RadioCheckRfFrequency(uint32_t frequency)
{
	return true;
}

uint32_t RadioTimeOnAir(RadioModems_t modem, uint8_t pktLen)
{
	uint32_t airTime = 0;

	switch (modem)
	{
	case MODEM_FSK:
	{
		// CRC Length calculation, catering for each type of CRC Calc offered in libary
		uint8_t crcLength = (uint8_t)(SX126x.PacketParams.Params.Gfsk.CrcLength);
		if ((crcLength == RADIO_CRC_2_BYTES) || (crcLength == RADIO_CRC_2_BYTES_INV) || (crcLength == RADIO_CRC_2_BYTES_IBM) || (crcLength == RADIO_CRC_2_BYTES_CCIT))
		{
			crcLength = 2;
		}
		else if ((crcLength == RADIO_CRC_1_BYTES) || (crcLength == RADIO_CRC_1_BYTES_INV))
		{
			crcLength = 1;
		}
		else
		{
			crcLength = 0;
		}
		airTime = rint((8 * (SX126x.PacketParams.Params.Gfsk.PreambleLength + (SX126x.PacketParams.Params.Gfsk.SyncWordLength >> 3) + ((SX126x.PacketParams.Params.Gfsk.HeaderType == RADIO_PACKET_FIXED_LENGTH) ? 0.0 : 1.0) + pktLen + (crcLength)) /
						SX126x.ModulationParams.Params.Gfsk.BitRate) *
					   1e3);
	}
	break;
	case MODEM_LORA:
	{
		double ts = RadioLoRaSymbTime[SX126x.ModulationParams.Params.LoRa.Bandwidth - 4][12 - SX126x.ModulationParams.Params.LoRa.SpreadingFactor];
		// time of preamble
		double tPreamble = (SX126x.PacketParams.Params.LoRa.PreambleLength + 4.25) * ts;
		// Symbol length of payload and time
		double tmp = ceil((8 * pktLen - 4 * SX126x.ModulationParams.Params.LoRa.SpreadingFactor +
						   28 + 16 * SX126x.PacketParams.Params.LoRa.CrcMode -
						   ((SX126x.PacketParams.Params.LoRa.HeaderType == LORA_PACKET_FIXED_LENGTH) ? 20 : 0)) /
						  (double)(4 * (SX126x.ModulationParams.Params.LoRa.SpreadingFactor -
										((SX126x.ModulationParams.Params.LoRa.LowDatarateOptimize > 0) ? 2 : 0)))) *
					 ((SX126x.ModulationParams.Params.LoRa.CodingRate % 4) + 4);
		double nPayload = 8 + ((tmp > 0) ? tmp : 0);
		double tPayload = nPayload * ts;
		// Time on air
		double tOnAir = tPreamble + tPayload;
		// return milli seconds
		airTime = floor(tOnAir + 0.999);
	}
	break;
	}
	return airTime;
}

/*!
 * \brief Sends the buffer of size. Prepares the packet to be sent and sets
 *        the radio in transmission
 *
 * \param buffer     Buffer pointer
 * \param size       Buffer size
 */
void RadioSend(uint8_t *buffer, uint8_t size)
{
	SX126xTXena();
	SX126xSetDioIrqParams(IRQ_TX_DONE | IRQ_RX_TX_TIMEOUT,
						  IRQ_TX_DONE | IRQ_RX_TX_TIMEOUT,
						  IRQ_RADIO_NONE,
						  IRQ_RADIO_NONE);

	if (SX126xGetPacketType() == PACKET_TYPE_LORA)
	{
		SX126x.PacketParams.Params.LoRa.PayloadLength = size;
	}
	else
	{
		SX126x.PacketParams.Params.Gfsk.PayloadLength = size;
	}
	SX126xSetPacketParams(&SX126x.PacketParams);

	SX126xSendPayload(buffer, size, 0);
	TimerSetValue(&TxTimeoutTimer, TxTimeout);
	TimerStart(&TxTimeoutTimer);
}

void RadioSleep(void)
{
	SleepParams_t params = {0};

	params.Fields.WarmStart = 1;
	SX126xSetSleep(params);

	delay(2);
}

void RadioStandby(void)
{
	SX126xSetStandby(STDBY_RC);
}

void RadioRx(uint32_t timeout)
{
	SX126xRXena();
	SX126xSetDioIrqParams(IRQ_RX_DONE | IRQ_RX_TX_TIMEOUT | IRQ_HEADER_ERROR | IRQ_CRC_ERROR, // IRQ_RADIO_ALL
						  IRQ_RX_DONE | IRQ_RX_TX_TIMEOUT | IRQ_HEADER_ERROR | IRQ_CRC_ERROR, // IRQ_RADIO_ALL
						  IRQ_RADIO_NONE,
						  IRQ_RADIO_NONE);

	LOG_LIB("RADIO", "RX window timeout = %ld", timeout);
	// Even Continous mode is selected, put a timeout here
	if (timeout != 0)
	{
		TimerSetValue(&RxTimeoutTimer, timeout);
		TimerStart(&RxTimeoutTimer);
	}
	if (RxContinuous == true)
	{
		SX126xSetRx(0xFFFFFF); // Rx Continuous
	}
	else
	{
		SX126xSetRx(RxTimeout << 6);
	}
}

void RadioRxBoosted(uint32_t timeout)
{
	SX126xSetDioIrqParams(IRQ_RX_DONE | IRQ_RX_TX_TIMEOUT | IRQ_HEADER_ERROR | IRQ_CRC_ERROR, // IRQ_RADIO_ALL
						  IRQ_RX_DONE | IRQ_RX_TX_TIMEOUT | IRQ_HEADER_ERROR | IRQ_CRC_ERROR, // IRQ_RADIO_ALL
						  IRQ_RADIO_NONE,
						  IRQ_RADIO_NONE);

	if (RxContinuous == true)
	{
		// Even Continous mode is selected, put a timeout here
		if (timeout != 0)
		{
			TimerSetValue(&RxTimeoutTimer, timeout);
			TimerStart(&RxTimeoutTimer);
		}
		SX126xSetRxBoosted(0xFFFFFF); // Rx Continuous
	}
	else
	{
		SX126xSetRxBoosted(RxTimeout << 6);
	}
}

void RadioSetRxDutyCycle(uint32_t rxTime, uint32_t sleepTime)
{
	SX126xSetDioIrqParams(IRQ_RADIO_ALL | IRQ_RX_TX_TIMEOUT,
						  IRQ_RADIO_ALL | IRQ_RX_TX_TIMEOUT,
						  IRQ_RADIO_NONE, IRQ_RADIO_NONE);
	SX126xSetRxDutyCycle(rxTime, sleepTime);
}

void RadioSetCadParams(uint8_t cadSymbolNum, uint8_t cadDetPeak, uint8_t cadDetMin, uint8_t cadExitMode, uint32_t cadTimeout)
{
	SX126xSetCadParams((RadioLoRaCadSymbols_t)cadSymbolNum, cadDetPeak, cadDetMin, (RadioCadExitModes_t)cadExitMode, cadTimeout);
}

void RadioStartCad(void)
{
	SX126xRXena();
	SX126xSetDioIrqParams(IRQ_CAD_DONE | IRQ_CAD_ACTIVITY_DETECTED,
						  IRQ_CAD_DONE | IRQ_CAD_ACTIVITY_DETECTED,
						  IRQ_RADIO_NONE, IRQ_RADIO_NONE);
	SX126xSetCad();
}

void RadioTx(uint32_t timeout)
{
	SX126xTXena();
	SX126xSetTx(timeout << 6);
}

/*!
 * \brief Sets the radio in continuous wave transmission mode
 *
 * \param freq       Channel RF frequency
 * \param power      Sets the output power [dBm]
 * \param time       Transmission mode timeout [s]
 */
void RadioSetTxContinuousWave(uint32_t freq, int8_t power, uint16_t time)
{
	SX126xSetRfFrequency(freq);
	SX126xSetRfTxPower(power);
	SX126xSetTxContinuousWave();

	TimerSetValue(&TxTimeoutTimer, time * 1e3);
	TimerStart(&TxTimeoutTimer);
}

int16_t RadioRssi(RadioModems_t modem)
{
	return SX126xGetRssiInst();
}

/*!
 * \brief Writes the radio register at the specified address
 *
 * \param  addr Register address
 * \param  data New register value
 */
void RadioWrite(uint16_t addr, uint8_t data)
{
	SX126xWriteRegister(addr, data);
}

/*!
 * \brief Reads the radio register at the specified address
 *
 * \param  addr Register address
 */
uint8_t RadioRead(uint16_t addr)
{
	return SX126xReadRegister(addr);
}

void RadioWriteBuffer(uint16_t addr, uint8_t *buffer, uint8_t size)
{
	SX126xWriteRegisters(addr, buffer, size);
}

void RadioReadBuffer(uint16_t addr, uint8_t *buffer, uint8_t size)
{
	SX126xReadRegisters(addr, buffer, size);
}

void RadioWriteFifo(uint8_t *buffer, uint8_t size)
{
	SX126xWriteBuffer(0, buffer, size);
}

void RadioReadFifo(uint8_t *buffer, uint8_t size)
{
	SX126xReadBuffer(0, buffer, size);
}

void RadioSetMaxPayloadLength(RadioModems_t modem, uint8_t max)
{
	if (modem == MODEM_LORA)
	{
		SX126x.PacketParams.Params.LoRa.PayloadLength = MaxPayloadLength = max;
		SX126xSetPacketParams(&SX126x.PacketParams);
	}
	else
	{
		if (SX126x.PacketParams.Params.Gfsk.HeaderType == RADIO_PACKET_VARIABLE_LENGTH)
		{
			SX126x.PacketParams.Params.Gfsk.PayloadLength = MaxPayloadLength = max;
			SX126xSetPacketParams(&SX126x.PacketParams);
		}
	}
}

void RadioSetPublicNetwork(bool enable)
{
	hasCustomSyncWord = false;
	RadioPublicNetwork.Current = RadioPublicNetwork.Previous = enable;

	RadioSetModem(MODEM_LORA);
	if (enable == true)
	{
		// Change LoRa modem SyncWord
		SX126xWriteRegister(REG_LR_SYNCWORD, (LORA_MAC_PUBLIC_SYNCWORD >> 8) & 0xFF);
		SX126xWriteRegister(REG_LR_SYNCWORD + 1, LORA_MAC_PUBLIC_SYNCWORD & 0xFF);
	}
	else
	{
		// Change LoRa modem SyncWord
		SX126xWriteRegister(REG_LR_SYNCWORD, (LORA_MAC_PRIVATE_SYNCWORD >> 8) & 0xFF);
		SX126xWriteRegister(REG_LR_SYNCWORD + 1, LORA_MAC_PRIVATE_SYNCWORD & 0xFF);
	}
}

void RadioSetCustomSyncWord(uint16_t syncword)
{
	hasCustomSyncWord = true;
	RadioSetModem(MODEM_LORA);
	SX126xWriteRegister(REG_LR_SYNCWORD, (syncword >> 8) & 0xFF);
	SX126xWriteRegister(REG_LR_SYNCWORD + 1, syncword & 0xFF);
}

uint16_t RadioGetSyncWord(void)
{
	uint8_t syncWord[8];
	RadioSetModem(MODEM_LORA);
	syncWord[0] = SX126xReadRegister(REG_LR_SYNCWORD);
	syncWord[1] = SX126xReadRegister(REG_LR_SYNCWORD + 1);

	return (uint16_t)(syncWord[0] << 8) + (uint16_t)(syncWord[1]);
}

uint32_t RadioGetWakeupTime(void)
{
	if (_hwConfig.USE_DIO3_TCXO)
	{
		return (RADIO_TCXO_SETUP_TIME + RADIO_WAKEUP_TIME);
	}
	else
	{
		return (RADIO_WAKEUP_TIME);
	}
}

void RadioOnTxTimeoutIrq(void)
{
	// if ((RadioEvents != NULL) && (RadioEvents->TxTimeout != NULL))
	// {
	// 	RadioEvents->TxTimeout();
	// }
	BoardDisableIrq();
	TimerTxTimeout = true;
	BoardEnableIrq();
	TimerStop(&TxTimeoutTimer);

	RadioBgIrqProcess();
	RadioStandby();
	RadioSleep();
}

void RadioOnRxTimeoutIrq(void)
{
	// if ((RadioEvents != NULL) && (RadioEvents->RxTimeout != NULL))
	// {
	// 	RadioEvents->RxTimeout();
	// }
	BoardDisableIrq();
	TimerRxTimeout = true;
	BoardEnableIrq();
	TimerStop(&RxTimeoutTimer);

	RadioBgIrqProcess();
	RadioStandby();
	RadioSleep();
}

void RadioEnforceLowDRopt(bool enforce)
{
	if (enforce)
	{
		force_low_dr_opt = true;
	}
	else
	{
		force_low_dr_opt = false;
	}
}

#if defined NRF52_SERIES || defined ESP32 || defined ARDUINO_RAKWIRELESS_RAK11300
/** Semaphore used by SX126x IRQ handler to wake up LoRaWAN task */
extern SemaphoreHandle_t _lora_sem;
static BaseType_t xHigherPriorityTaskWoken = pdTRUE;
#endif

#if defined(ESP8266)
void ICACHE_RAM_ATTR RadioOnDioIrq(void)
#elif defined(ESP32)
void IRAM_ATTR RadioOnDioIrq(void)
#else
void RadioOnDioIrq(void)
#endif
{
	BoardDisableIrq();
	IrqFired = true;
	BoardEnableIrq();
#if defined NRF52_SERIES || defined ESP32 || defined ARDUINO_RAKWIRELESS_RAK11300
	// Wake up LoRa event handler on nRF52 and ESP32
	xSemaphoreGiveFromISR(_lora_sem, &xHigherPriorityTaskWoken);
#elif defined(ARDUINO_ARCH_RP2040)
	// Wake up LoRa event handler on RP2040
	if (_lora_task_thread != NULL)
	{
		osSignalSet(_lora_task_thread, 0x1);
	}
#else
#pragma error "Board not supported"
#endif
}

void RadioBgIrqProcess(void)
{
	bool rx_timeout_handled = false;
	bool tx_timeout_handled = false;
	if (IrqFired == true)
	{
		BoardDisableIrq();
		IrqFired = false;
		BoardEnableIrq();

		uint16_t irqRegs = SX126xGetIrqStatus();
		SX126xClearIrqStatus(IRQ_RADIO_ALL);

		if ((irqRegs & IRQ_TX_DONE) == IRQ_TX_DONE)
		{
			LOG_LIB("RADIO", "IRQ_TX_DONE");
			tx_timeout_handled = true;
			TimerStop(&TxTimeoutTimer);
			//!< Update operating mode state to a value lower than \ref MODE_STDBY_XOSC
			SX126xSetOperatingMode(MODE_STDBY_RC);

			if(RadioPublicNetwork.Current == true)
			{
				if ((RadioEvents != NULL) && (RadioEvents->TxDone != NULL))
				{
					RadioEvents->TxDone();
				}

			}
			
			if((_events != NULL) && (_events->TxDone != NULL))
			{
				_events->TxDone(RadioPublicNetwork.Current);
			}
		}

		if ((irqRegs & IRQ_RX_DONE) == IRQ_RX_DONE)
		{
			LOG_LIB("RADIO", "IRQ_RX_DONE");
			uint8_t size;

			rx_timeout_handled = true;
			if (RadioPublicNetwork.Current)
				TimerStop(&RxTimeoutTimer);
		
			if (RxContinuous == false)
			{
				//!< Update operating mode state to a value lower than \ref MODE_STDBY_XOSC
				SX126xSetOperatingMode(MODE_STDBY_RC);

				// WORKAROUND - Implicit Header Mode Timeout Behavior, see DS_SX1261-2_V1.2 datasheet chapter 15.3
				// RegRtcControl = @address 0x0902
				SX126xWriteRegister(0x0902, 0x00);
				// RegEventMask = @address 0x0944
				SX126xWriteRegister(0x0944, SX126xReadRegister(0x0944) | (1 << 1));
				// WORKAROUND END
			}
			memset(RadioRxPayload, 0, 255);

			if ((irqRegs & IRQ_CRC_ERROR) == IRQ_CRC_ERROR)
			{
				LOG_LIB("RADIO", "IRQ_CRC_ERROR");

				uint8_t size;
				// Discard buffer
				memset(RadioRxPayload, 0, 255);
				SX126xGetPayload(RadioRxPayload, &size, 255);
				SX126xGetPacketStatus(&RadioPktStatus);
				if(RadioPublicNetwork.Current == true)
				{
					if ((RadioEvents != NULL) && (RadioEvents->RxError))
					{
						RadioEvents->RxError();
					}
					
				}
				if((_events != NULL) && (_events->RxError != NULL))
				{
					_events->RxError(RadioPublicNetwork.Current);
				}
			}
			else
			{
				SX126xGetPayload(RadioRxPayload, &size, 255);
				SX126xGetPacketStatus(&RadioPktStatus);
				if(RadioPublicNetwork.Current == true)
				{
					if ((RadioEvents != NULL) && (RadioEvents->RxDone != NULL))
					{
						RadioEvents->RxDone(RadioRxPayload, size, RadioPktStatus.Params.LoRa.RssiPkt, RadioPktStatus.Params.LoRa.SnrPkt);

					}

				}
				if((_events != NULL) && (_events->RxDone != NULL))
				{
					_events->RxDone(RadioPublicNetwork.Current, RadioRxPayload, size, RadioPktStatus.Params.LoRa.RssiPkt, RadioPktStatus.Params.LoRa.SnrPkt);
				}
			}
		}

		if ((irqRegs & IRQ_CAD_DONE) == IRQ_CAD_DONE)
		{
			LOG_LIB("RADIO", "IRQ_CAD_DONE");
			//!< Update operating mode state to a value lower than \ref MODE_STDBY_XOSC
			SX126xSetOperatingMode(MODE_STDBY_RC);
			if ((RadioEvents != NULL) && (RadioEvents->CadDone != NULL))
			{
				RadioEvents->CadDone(((irqRegs & IRQ_CAD_ACTIVITY_DETECTED) == IRQ_CAD_ACTIVITY_DETECTED));
			}
		}

		if ((irqRegs & IRQ_RX_TX_TIMEOUT) == IRQ_RX_TX_TIMEOUT)
		{
			// LOG_LIB("RADIO", "RadioIrqProcess => IRQ_RX_TX_TIMEOUT");

			if (SX126xGetOperatingMode() == MODE_TX)
			{
				LOG_LIB("RADIO", "IRQ_TX_TIMEOUT");
				tx_timeout_handled = true;
				TimerStop(&TxTimeoutTimer);
				//!< Update operating mode state to a value lower than \ref MODE_STDBY_XOSC
				SX126xSetOperatingMode(MODE_STDBY_RC);
				if(RadioPublicNetwork.Current == true)
				{
					if ((RadioEvents != NULL) && (RadioEvents->TxTimeout != NULL))
					{
						RadioEvents->TxTimeout();
					}
				}
				if((_events != NULL) && (_events->TxTimeout != NULL))
				{
					_events->TxTimeout(RadioPublicNetwork.Current, IRQ_TYPE);
				}
			}
			else if (SX126xGetOperatingMode() == MODE_RX)
			{
				LOG_LIB("RADIO", "IRQ_RX_TIMEOUT");
				rx_timeout_handled = true;
				TimerStop(&RxTimeoutTimer);
				//!< Update operating mode state to a value lower than \ref MODE_STDBY_XOSC
				SX126xSetOperatingMode(MODE_STDBY_RC);
				if(RadioPublicNetwork.Current == true)
				{
					if ((RadioEvents != NULL) && (RadioEvents->RxTimeout != NULL))
					{
						RadioEvents->RxTimeout();
					}

				}
				if((_events != NULL) && (_events->RxTimeout != NULL))
				{
					_events->RxTimeout(RadioPublicNetwork.Current, IRQ_TYPE);
				}
			}
		}

		if ((irqRegs & IRQ_PREAMBLE_DETECTED) == IRQ_PREAMBLE_DETECTED)
		{
			LOG_LIB("RADIO", "IRQ_PREAMBLE_DETECTED");
			if ((RadioEvents != NULL) && (RadioEvents->PreAmpDetect != NULL))
			{
				RadioEvents->PreAmpDetect();
			}
		}

		if ((irqRegs & IRQ_SYNCWORD_VALID) == IRQ_SYNCWORD_VALID)
		{
			//__NOP( );
		}

		if ((irqRegs & IRQ_HEADER_VALID) == IRQ_HEADER_VALID)
		{
			//__NOP( );
		}

		if ((irqRegs & IRQ_HEADER_ERROR) == IRQ_HEADER_ERROR)
		{
			LOG_LIB("RADIO", "RadioIrqProcess => IRQ_HEADER_ERROR");

			TimerStop(&RxTimeoutTimer);
			if (RxContinuous == false)
			{
				//!< Update operating mode state to a value lower than \ref MODE_STDBY_XOSC
				SX126xSetOperatingMode(MODE_STDBY_RC);
			}
			if(RadioPublicNetwork.Current == true)
			{
				if ((RadioEvents != NULL) && (RadioEvents->RxError != NULL))
				{
					RadioEvents->RxError();
				}
			}
			if((_events != NULL) && (_events->RxError != NULL))
			{
				_events->RxError(RadioPublicNetwork.Current);
			}
		}
	}
	if (TimerRxTimeout)
	{
		TimerRxTimeout = false;
		if (!rx_timeout_handled)
		{
			LOG_LIB("RADIO", "TimerRxTimeout");
			TimerStop(&RxTimeoutTimer);
			if(RadioPublicNetwork.Current == true)
			{
				if ((RadioEvents != NULL) && (RadioEvents->RxTimeout != NULL))
				{
					RadioEvents->RxTimeout();
				}
			}
			if((_events != NULL) && (_events->RxTimeout != NULL))
			{
				_events->RxTimeout(RadioPublicNetwork.Current, TIMER_TYPE);
			}
		}
	}
	if (TimerTxTimeout)
	{
		TimerTxTimeout = false;
		if (!tx_timeout_handled)
		{
			LOG_LIB("RADIO", "TimerTxTimeout");
			TimerStop(&TxTimeoutTimer);
			if(RadioPublicNetwork.Current == true)
			{
				if ((RadioEvents != NULL) && (RadioEvents->TxTimeout != NULL))
				{
					RadioEvents->TxTimeout();
				}

			}
			if((_events != NULL) && (_events->TxTimeout != NULL))
			{
				_events->TxTimeout(RadioPublicNetwork.Current, TIMER_TYPE);
			}
		}
	}
}

void RadioIrqProcess(void)
{
#if defined(ESP8266)
	RadioBgIrqProcess();
#endif
}

void RadioIrqProcessAfterDeepSleep(void)
{
	BoardDisableIrq();
	IrqFired = true;
	BoardEnableIrq();
	RadioBgIrqProcess();
}

void setLoRaEvents(loraEvents_t *events)
{
	_events = events;
}