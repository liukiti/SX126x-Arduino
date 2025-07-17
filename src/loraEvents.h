#ifndef __LORAEVENTS_H__
#define __LORAEVENTS_H__

typedef enum {
    IRQ_TYPE = 0,
    TIMER_TYPE,
} timeoutType_t;

typedef struct {
    void (*TxDone) (bool isPublic);
    void (*TxTimeout)(bool isPublic, timeoutType_t type);
    void (*RxDone)(bool isPublic, uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr);
    void (*RxTimeout)(bool isPublic, timeoutType_t type);
    void (*RxError)(bool isPublic);
    void *param;
} loraEvents_t;

typedef struct {
    uint32_t UpLinkCounter;
    uint8_t channel;
	int8_t Datarate;
	int8_t TxPower;
	float MaxEirp;
	float AntennaGain;
	uint16_t PktLen;
    TimerEvent_t *Rx1Timer;
    TimerEvent_t *Rx2Timer;
} lorawanParams_t;

void setLoRaEvents(loraEvents_t *events);

#endif //__LORAEVENTS_H__