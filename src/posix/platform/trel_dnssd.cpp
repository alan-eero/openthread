/*
 *  Copyright (c) 2025, The OpenThread Authors.
 */

 /**
 * @file
 *   This file implements ... 
 */

#include <openthread/platform/dnssd.h>
#include <openthread/platform/trel.h>
#include "mdns_socket.hpp"
#include "platform-posix.h"
#include <openthread/logging.h>

#include <net/if.h>
#include <string.h>

// Static variables to track state
static bool sInitialized = false;
static char sInterfaceName[IFNAMSIZ];
static uint32_t sInfraIfIndex = 0;
static otInstance *sInstance = nullptr;
static otPlatDnssdService sRegisterInfo;

// Constants
static constexpr char kTrelServiceName[] = "_trel._udp.local.";
static constexpr char kTrelInstanceName[] = "OpenThread TREL Node";

void trelDnssdInitialize(const char *aTrelNetif)
{
    OT_ASSERT(aTrelNetif != nullptr);

    if (!sInitialized)
    {
        strncpy(sInterfaceName, aTrelNetif, sizeof(sInterfaceName) - 1);
        sInterfaceName[sizeof(sInterfaceName) - 1] = '\0';
        
        sInfraIfIndex = if_nametoindex(sInterfaceName);
        
        if (sInfraIfIndex == 0)
        {
            otLogCritPlat("Failed to get interface index for %s", sInterfaceName);
            return;
        }

        // Initialize MdnsSocket
        ot::Posix::MdnsSocket::Get().Init();
        
        sInitialized = true;
        otLogInfoPlat("TREL DNS-SD: Initialized on interface %s (index %u)", 
                     sInterfaceName, sInfraIfIndex);
    }
}

void trelDnssdStartBrowse(void)
{
    otError error;

    if (!sInitialized)
    {
        otLogWarnPlat("TREL DNS-SD: Not initialized");
        return;
    }

    error = ot::Posix::MdnsSocket::Get().SetListeningEnabled(sInstance, true, sInfraIfIndex);
    
    if (error == OT_ERROR_NONE)
    {
        otLogInfoPlat("TREL DNS-SD: Started browsing for peers");
    }
    else
    {
        otLogWarnPlat("TREL DNS-SD: Failed to start browsing, error: %s", 
                      otThreadErrorToString(error));
    }
}

void trelDnssdStopBrowse(void)
{
    if (sInitialized)
    {
        ot::Posix::MdnsSocket::Get().SetListeningEnabled(sInstance, false, sInfraIfIndex);
        otLogInfoPlat("TREL DNS-SD: Stopped browsing");
    }
}

void trelDnssdRegisterService(uint16_t aPort, const uint8_t *aTxtData, uint8_t aTxtLength)
{
    if (!sInitialized)
    {
        otLogWarnPlat("TREL DNS-SD: Not initialized");
        return;
    }

    // Prepare registration info
    sRegisterInfo.mHostName = sInterfaceName;
    sRegisterInfo.mServiceName = kTrelServiceName;
    sRegisterInfo.mInstanceName = kTrelInstanceName;
    sRegisterInfo.mSubTypeLabels = nullptr;
    sRegisterInfo.mPort = aPort;
    sRegisterInfo.mTxtData = aTxtData;
    sRegisterInfo.mTxtDataLength = aTxtLength;

    otLogInfoPlat("TREL DNS-SD: Registering service %s.%s port %u", 
                  sRegisterInfo.mHostName, 
                  sRegisterInfo.mServiceName,
                  aPort);

    ot::Posix::MdnsSocket::Get().RegisterService(&sRegisterInfo);
}

void trelDnssdRemoveService(void)
{
    if (sInitialized)
    {
        otLogInfoPlat("TREL DNS-SD: Removing service");
        ot::Posix::MdnsSocket::Get().UnregisterService();
    }
}

void trelDnssdUpdateFdSet(otSysMainloopContext *aContext)
{
    if (sInitialized)
    {
        ot::Posix::MdnsSocket::Get().Update(*aContext);
    }
}

void trelDnssdProcess(otInstance *aInstance, const otSysMainloopContext *aContext)
{
    if (sInitialized)
    {
        sInstance = aInstance;  // Store instance for callbacks
        ot::Posix::MdnsSocket::Get().Process(*aContext);
    }
}

void trelDnssdNotifyPeerSocketAddressDifference(const otSockAddr *aPeerSockAddr,
                                               const otSockAddr *aRxSockAddr)
{
    if (sInitialized)
    {
        otLogInfoPlat("TREL DNS-SD: Peer address difference detected");
        // Could trigger a re-browse here if needed
        trelDnssdStopBrowse();
        trelDnssdStartBrowse();
    }
}