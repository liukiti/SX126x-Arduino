#ifndef __LORAEVENTS_H__
#define __LORAEVENTS_H__

typedef enum {
    IRQ_TYPE = 0,
    TIMER_TYPE,
} timeoutType_t;

typedef struct {
    void (*TxDone) (void);
    void (*TxTimeout)(timeoutType_t type);
    void (*RxDone)(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr);
    void (*RxTimeout)(timeoutType_t type);
    void (*RxError)();
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
} lorawanTXParams_t;

void setP2PEvents(loraEvents_t *p2p);
void setLRWEvents(loraEvents_t *lrw);

#endif //__LORAEVENTS_H__