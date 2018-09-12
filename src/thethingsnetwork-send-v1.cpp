/*******************************************************************************
 * Copyright (c) 2015 Thomas Telkamp and Matthijs Kooijman
 *
 * Permission is hereby granted, free of charge, to anyone
 * obtaining a copy of this document and accompanying files,
 * to do whatever they want with them without any restriction,
 * including, but not limited to, copying, modification and redistribution.
 * NO WARRANTY OF ANY KIND IS PROVIDED.
 *
 * This example sends a valid LoRaWAN packet with payload "Hello, world!", that
 * will be processed by The Things Network server.
 *
 * Note: LoRaWAN per sub-band duty-cycle limitation is enforced (1% in g1, 
*  0.1% in g2). 
 *
 * Change DEVADDR to a unique address! 
 * See http://thethingsnetwork.org/wiki/AddressSpace
 *
 * Do not forget to define the radio type correctly in config.h, default is:
 *   #define CFG_sx1272_radio 1
 * for SX1272 and RFM92, but change to:
 *   #define CFG_sx1276_radio 1
 * for SX1276 and RFM95.
 *
 *******************************************************************************/
#include <stdio.h>
#include <time.h>
#include <wiringPi.h>
#include <lmic.h>
#include <hal.h>
#include <local_hal.h>

// LoRaWAN Application identifier (AppEUI)
// Not used in this example
static const u1_t APPEUI[8] = {0x02, 0x00, 0x00, 0x00, 0x00, 0xEE, 0xFF, 0xC0};

// LoRaWAN DevEUI, unique device ID (LSBF)
// Not used in this example
static const u1_t DEVEUI[8] = {0x42, 0x42, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};

// LoRaWAN NwkSKey, network session key
// Use this key for The Things Network
static const u1_t DEVKEY[16] = {0x8A, 0x2A, 0xD0, 0xE6, 0xBE, 0x4F, 0x77, 0x58, 0xD3, 0xB1, 0xB6, 0x04, 0x03, 0xD4, 0xAB, 0xAA};

// LoRaWAN AppSKey, application session key
// Use this key to get your data decrypted by The Things Network
static const u1_t ARTKEY[16] = {0xBF, 0x43, 0xCA, 0x95, 0x98, 0x38, 0x54, 0xC1, 0xA6, 0x8F, 0xA0, 0x46, 0xAD, 0x04, 0xF3, 0xC5};

// LoRaWAN end-device address (DevAddr)
// See http://thethingsnetwork.org/wiki/AddressSpace
static const u4_t DEVADDR = 0x26021CD9; // <-- Change this address for every node!

//////////////////////////////////////////////////
// APPLICATION CALLBACKS
//////////////////////////////////////////////////

// provide application router ID (8 bytes, LSBF)
void os_getArtEui(u1_t *buf)
{
    memcpy(buf, APPEUI, 8);
}

// provide device ID (8 bytes, LSBF)
void os_getDevEui(u1_t *buf)
{
    memcpy(buf, DEVEUI, 8);
}

// provide device key (16 bytes)
void os_getDevKey(u1_t *buf)
{
    memcpy(buf, DEVKEY, 16);
}

u4_t cntr = 0;
u1_t mydata[] = {"    "};
static osjob_t sendjob;

// Pin mapping
lmic_pinmap pins = {
    .nss = 6,
    .rxtx = UNUSED_PIN, // Not connected on RFM92/RFM95
    .rst = 0,           // Needed on RFM92/RFM95
    .dio = {7, 4, 5}};

void onEvent(ev_t ev)
{
    //debug_event(ev);
    switch (ev)
    {
    // scheduled data sent (optionally data received)
    // note: this includes the receive window!
    case EV_TXCOMPLETE:
        // use this event to keep track of actual transmissions
        fprintf(stdout, "Event EV_TXCOMPLETE, time: %d\n", millis() / 1000);
        //	  fwrite(&mydata, 1, 5, stdout);
        //	  fflush(stdout);
        if (LMIC.dataLen)
        { // data received in rx slot after tx
            //debug_buf(LMIC.frame+LMIC.dataBeg, LMIC.dataLen);
            fprintf(stdout, "Data Received!\n");
        }
        break;
    default:
        break;
    }
}

// used to read data from python program
void readData()
{
    char buf[8];
    fprintf(stdout, "waiting");
    fscanf(stdin, "%s", &buf);
    int i = 0;
    while (buf[i])
    {
        mydata[i] = buf[i];
        i++;
    }
}

static void do_send(osjob_t *j)
{
    time_t t = time(NULL);
    fprintf(stdout, "[%x] (%ld) %s\n", hal_ticks(), t, ctime(&t));
    // Show TX channel (channel numbers are local to LMIC)
    // Check if there is not a current TX/RX job running
    if (LMIC.opmode & (1 << 7))
    {
        fprintf(stdout, "OP_TXRXPEND, not sending");
    }
    else
    {
        readData();
        LMIC_setTxData2(1, mydata, sizeof(mydata), 0);
    }
    // Schedule a timed job to run at the given timestamp (absolute system time)
    os_setTimedCallback(j, os_getTime() + sec2osticks(15), do_send);
}

void setup()
{
    // LMIC init
    wiringPiSetup();
    os_init();
    // Reset the MAC state. Session and pending data transfers will be discarded.
    LMIC_reset();
    // Set static session parameters. Instead of dynamically establishing a session
    // by joining the network, precomputed session parameters are be provided.
    LMIC_setSession(0x1, DEVADDR, (u1_t *)DEVKEY, (u1_t *)ARTKEY);
    // Set up TTN channels
    LMIC_selectSubBand(1);
    // Disable data rate adaptation
    LMIC_setAdrMode(0);
    // Disable link check validation
    LMIC_setLinkCheckMode(0);
    // Disable beacon tracking
    LMIC_disableTracking();
    // Stop listening for downstream data (periodical reception)
    LMIC_stopPingable();
    // Set data rate and transmit power (note: txpow seems to be ignored by the library)  LMIC_setDrTxpow(DR_SF7,14);
    // TTN uses SF9 for its RX2 window
    LMIC.dn2Dr = DR_SF9;
}

int main()
{

    setup();
    do_send(&sendjob);
    while (1)
    {
        os_runloop();
    }
    return 0;
}

//extern "C" {
//  void start_loop(char * data){setData(data); send();}
//}
