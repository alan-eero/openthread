/*
 *  Copyright (c) 2025, The OpenThread Authors.
 */

/**
 * @file
 *   This file implements the Platform Radio API for TREL-only operation.
 */

#include <common/code_utils.hpp>
#include "instance/instance.hpp"
#include <openthread/logging.h>
#include <openthread/platform/dnssd.h>
#include <openthread/platform/radio.h>
#include <openthread/platform/trel.h>
#include <openthread/platform/udp.h>


using namespace ot;

#if OPENTHREAD_CONFIG_RADIO_LINK_IEEE_802_15_4_ENABLE == 0 && OPENTHREAD_CONFIG_RADIO_LINK_TREL_ENABLE == 1 

// Transmit buffer
static otRadioFrame sTransmitFrame;
static uint8_t sTransmitPsdu[OT_RADIO_FRAME_MAX_SIZE];
static bool sTransmitFrameInitialized = false;

// Radio state tracking
static bool sRadioInitialized = false;
static bool sMdnsInitialized = false;
static bool sTrelEnabled = false;
static uint8_t sCurrentChannel = 0;
static otRadioState sState = OT_RADIO_STATE_DISABLED;

// TREL specific
static constexpr uint16_t kTrelUdpPort = 9200;

static char sInterfaceName[IFNAMSIZ + 1] = {0};

//READY to ROCK
extern "C" void otPlatRadioReceiveDone(otInstance *aInstance, otRadioFrame *aFrame, otError aError)
{
    Instance &instance = AsCoreType(aInstance);

    VerifyOrExit(instance.IsInitialized());
    VerifyOrExit(sState == OT_RADIO_STATE_RECEIVE);

    instance.Get<Radio::Callbacks>().HandleReceiveDone(static_cast<Mac::RxFrame *>(aFrame), aError);

exit:
    return;
}

//READY to ROCK
extern "C" void otPlatRadioTxStarted(otInstance *aInstance, otRadioFrame *aFrame)
{
    Instance     &instance = AsCoreType(aInstance);
    Mac::TxFrame &txFrame  = *static_cast<Mac::TxFrame *>(aFrame);

    VerifyOrExit(instance.IsInitialized());
    instance.Get<Radio::Callbacks>().HandleTransmitStarted(txFrame);

exit:
    return;
}

//READY to ROCK
extern "C" void otPlatRadioTxDone(otInstance *aInstance, otRadioFrame *aFrame, otRadioFrame *aAckFrame, otError aError)
{
    Instance &instance = AsCoreType(aInstance);
    VerifyOrExit(instance.IsInitialized());
    if (sState == OT_RADIO_STATE_TRANSMIT)
    {
        sState = OT_RADIO_STATE_RECEIVE;
    }
    instance.Get<Radio::Callbacks>().HandleTransmitDone(*static_cast<Mac::TxFrame *>(aFrame),
                                                      static_cast<Mac::RxFrame *>(aAckFrame),
                                                      aError);
exit:
    return;
}

//READY
extern "C" void otPlatRadioEnergyScanDone(otInstance *aInstance, int8_t aEnergyScanMaxRssi)
{
    Instance &instance = AsCoreType(aInstance);

    VerifyOrExit(instance.IsInitialized());

    // Since TREL doesn't support energy scan, we just notify with a default value
    otLogDebgPlat("Radio: Energy scan done (not supported in TREL)");
    instance.Get<Radio::Callbacks>().HandleEnergyScanDone(OT_RADIO_RSSI_INVALID);

exit:
    return;
}

//READY
extern "C" void otPlatRadioBusLatencyChanged(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);
    // This callback is not applicable for TREL operation
    otLogDebgPlat("Radio: Bus latency change ignored (not applicable for TREL)");
}

//READY
otRadioCaps otPlatRadioGetCaps(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);
    // Which capabilities shall we use here?
    return static_cast<otRadioCaps>(
        OT_RADIO_CAPS_ACK_TIMEOUT |
        OT_RADIO_CAPS_TRANSMIT_RETRIES |
        OT_RADIO_CAPS_CSMA_BACKOFF |
        OT_RADIO_CAPS_SLEEP_TO_TX
    );
}

// READY to ROCK
otError otPlatRadioEnable(otInstance *aInstance)
{
    otError error = OT_ERROR_NONE;
    uint16_t port = kTrelUdpPort;

    VerifyOrExit(!sRadioInitialized, error = OT_ERROR_ALREADY);

    // Get interface name if not already set
    if (sInterfaceName[0] == '\0')
    {
        // You'll need to implement a way to get the interface name
        // or pass it in through configuration
    }

    error = otPlatTrelEnable(aInstance, &port);
    SuccessOrExit(error);

    sRadioInitialized = true;
    sState = OT_RADIO_STATE_SLEEP;
    sTrelEnabled = true;

exit:
    return error;
}

// READY to ROCK
otError otPlatRadioDisable(otInstance *aInstance)
{
    otError error = OT_ERROR_NONE;

    VerifyOrExit(sRadioInitialized, error = OT_ERROR_ALREADY);

    otPlatTrelDisable(aInstance);
    sRadioInitialized = false;
    sTrelEnabled = false;
    sState = OT_RADIO_STATE_DISABLED;

exit:
    return error;
}

// READY to ROCK
bool otPlatRadioIsEnabled(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);
    return sRadioInitialized && sTrelEnabled;
}

// READY to ROCK
otError otPlatRadioEnergyScan(otInstance *aInstance, uint8_t aScanChannel, uint16_t aScanDuration)
{
    OT_UNUSED_VARIABLE(aInstance);
    OT_UNUSED_VARIABLE(aScanChannel);
    OT_UNUSED_VARIABLE(aScanDuration);
    
    // For TREL-only operation, we can return NOT_IMPLEMENTED
    return OT_ERROR_NOT_IMPLEMENTED;
}

//READY
uint8_t otPlatRadioGetCurrentChannel(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);
    return sCurrentChannel;
}

//READY
int8_t otPlatRadioGetRssi(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);
    // TREL doesn't have real RSSI, return a reasonable default
    return -60;  // dBm
}

//READY
uint8_t otPlatRadioGetCcaEnergyDetectThreshold(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);
    return 0;
}

//READY
otError otPlatRadioSetCcaEnergyDetectThreshold(otInstance *aInstance, uint8_t aThreshold)
{
    OT_UNUSED_VARIABLE(aInstance);
    OT_UNUSED_VARIABLE(aThreshold);
    return OT_ERROR_NONE;
}

// READY to ROCK
otError otPlatRadioSleep(otInstance *aInstance)
{
    otError error = OT_ERROR_NONE;

    VerifyOrExit(sState != OT_RADIO_STATE_DISABLED, error = OT_ERROR_INVALID_STATE);

    sState = OT_RADIO_STATE_SLEEP;

exit:
    return error;
}

// READY to ROCK
otError otPlatRadioReceive(otInstance *aInstance, uint8_t aChannel)
{
    otError error = OT_ERROR_NONE;

    VerifyOrExit(sState != OT_RADIO_STATE_DISABLED, error = OT_ERROR_INVALID_STATE);

    sCurrentChannel = aChannel;
    sState = OT_RADIO_STATE_RECEIVE;

exit:
    return error;
}

// READY to ROCK
otRadioFrame *otPlatRadioGetTransmitBuffer(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);
    
    if (!sTransmitFrameInitialized)
    {
        sTransmitFrame.mPsdu = sTransmitPsdu;
        sTransmitFrame.mLength = 0;
        sTransmitFrame.mChannel = sCurrentChannel;  // Use current channel
        sTransmitFrame.mInfo.mTxInfo.mMaxCsmaBackoffs = OPENTHREAD_CONFIG_MAC_MAX_CSMA_BACKOFFS_DIRECT;
        sTransmitFrame.mInfo.mTxInfo.mMaxFrameRetries = OPENTHREAD_CONFIG_MAC_MAX_FRAME_RETRIES_DIRECT;
        sTransmitFrameInitialized = true;
    }
    
    return &sTransmitFrame;
}

// READY to ROCK
otError otPlatRadioTransmit(otInstance *aInstance, otRadioFrame *aFrame)
{
    otError error = OT_ERROR_NONE;
    
    VerifyOrExit(sState != OT_RADIO_STATE_DISABLED, error = OT_ERROR_INVALID_STATE);
    VerifyOrExit(sTrelEnabled, error = OT_ERROR_INVALID_STATE);
    
    sState = OT_RADIO_STATE_TRANSMIT;
    otPlatRadioTxStarted(aInstance, aFrame);
    
    error = otPlatTrelSend(aInstance, aFrame->mPsdu, aFrame->mLength, 
                          &aFrame->mInfo.mTrelInfo.mDestSockAddr);
    
    if (error != OT_ERROR_NONE)
    {
        otPlatRadioTxDone(aInstance, aFrame, NULL, error);
        sState = OT_RADIO_STATE_RECEIVE;
    }

exit:
    return error;
}

/* I should probably move this compile conditional to the top, because DNSSD is 
*  kind of mandatory for TREL to work. */
#if OPENTHREAD_CONFIG_TREL_MANAGE_DNSSD_ENABLE

otError otPlatMdnsInit(otInstance *aInstance)
{
    otError error = OT_ERROR_NONE;
    
    VerifyOrExit(!sMdnsInitialized, error = OT_ERROR_ALREADY);
    
    // Use MdnsSocket for initialization
    ot::Posix::MdnsSocket::Get().Init();
    trelDnssdInitialize(sInterfaceName);
    
    sMdnsInitialized = true;

exit:
    return error;
}

otError otPlatMdnsRegisterService(otInstance                    *aInstance,
                                 const otPlatDnssdService   *aInfo,
                                 otPlatMdnsRegisterCallback      aCallback,
                                 void                           *aContext)
{
    // Use MdnsSocket for registration
    ot::Posix::MdnsSocket::Get().RegisterService(aInfo);
    
    if (aCallback != NULL)
    {
        aCallback(aContext, OT_ERROR_NONE);
    }
    
    return OT_ERROR_NONE;
}

otError otPlatMdnsUnregisterService(otInstance                    *aInstance,
                                   const otPlatDnssdService   *aInfo,
                                   otPlatMdnsRegisterCallback      aCallback,
                                   void                           *aContext)
{
    // Use MdnsSocket for unregistration
    ot::Posix::MdnsSocket::Get().UnregisterService();
    return OT_ERROR_NONE;
}

otError otPlatMdnsBrowse(otInstance *aInstance,
                        const char *aServiceName,
                        otPlatMdnsBrowseCallback aCallback,
                        void *aContext)
{
    otError error = OT_ERROR_NONE;

    VerifyOrExit(aServiceName != NULL, error = OT_ERROR_INVALID_ARGS);
    VerifyOrExit(aCallback != NULL, error = OT_ERROR_INVALID_ARGS);

    otLogInfoPlat("MDNS: Browse for service %s", aServiceName);

    // In real implementation, start actual mDNS browsing
    // For now, just call callback with no results
    aCallback(aContext, NULL, 0, OT_ERROR_NONE);

exit:
    return error;
}

otError otPlatMdnsResolve(otInstance *aInstance,
                         const otPlatMdnsServiceInfo *aServiceInfo,
                         otPlatMdnsResolveCallback aCallback,
                         void *aContext)
{
    otError error = OT_ERROR_NONE;

    VerifyOrExit(aServiceInfo != NULL, error = OT_ERROR_INVALID_ARGS);
    VerifyOrExit(aCallback != NULL, error = OT_ERROR_INVALID_ARGS);

    otLogInfoPlat("MDNS: Resolve service %s.%s", aServiceInfo->mHostName, aServiceInfo->mServiceName);

    // In real implementation, resolve service to IP
    // For now, just call callback with no results
    aCallback(aContext, NULL, OT_ERROR_NONE);

exit:
    return error;
}

#endif // OPENTHREAD_CONFIG_TREL_MANAGE_DNSSD_ENABLE

#endif // OPENTHREAD_CONFIG_RADIO_LINK_IEEE_802_15_4_ENABLE == 0 && OPENTHREAD_CONFIG_RADIO_LINK_TREL_ENABLE == 1