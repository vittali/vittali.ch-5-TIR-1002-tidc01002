/******************************************************************************

 @file config.h

 @brief TI-15.4 Stack configuration parameters for Collector applications

 Group: WCS LPC
 Target Device: CC13xx

 ******************************************************************************
 
 Copyright (c) 2016, Texas Instruments Incorporated
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions
 are met:

 *  Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

 *  Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

 *  Neither the name of Texas Instruments Incorporated nor the names of
    its contributors may be used to endorse or promote products derived
    from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 ******************************************************************************
 Release Name: simplelink_cc13x0_sdk_1_00_00_13"
 Release Date: 2016-11-21 18:05:40
 *****************************************************************************/
#ifndef CONFIG_H
#define CONFIG_H

/******************************************************************************
 Includes
 *****************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif

/******************************************************************************
 Constants and definitions
 *****************************************************************************/
/* config parameters */
/*! Security Enable - set to true to turn on security */
#define CONFIG_SECURE                true
/*! PAN ID */
#define CONFIG_PAN_ID                0xFFFF
/*! Coordinator short address */
#define CONFIG_COORD_SHORT_ADDR      0xAABB
/*! FH disabled as default */
#define CONFIG_FH_ENABLE             false
/*! maximum beacons possibly received */
#define CONFIG_MAX_BEACONS_RECD      200
/*! link quality */
#define CONFIG_LINKQUALITY           1
/*! percent filter */
#define CONFIG_PERCENTFILTER         0xFF
/*! scan duration */
#define CONFIG_SCAN_DURATION         5
/*! maximum devices in association table */
#define CONFIG_MAX_DEVICES           8

/*!
 Setting beacon order to 15 will disable the beacon, 8 is a good value for
 beacon mode
 */
#define CONFIG_MAC_BEACON_ORDER      15
/*!
 Setting superframe order to 15 will disable the superframe, 6 is a good value
 for beacon mode
 */
#define CONFIG_MAC_SUPERFRAME_ORDER  15

/*! Setting for channel page */
#define CONFIG_CHANNEL_PAGE          (APIMAC_CHANNEL_PAGE_9)

/*! Setting for Phy ID */
#define CONFIG_PHY_ID                (APIMAC_STD_US_915_PHY_1)

/*! Setting Default Key*/
#define KEY_TABLE_DEFAULT_KEY {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0,\
                               0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
/*!
 Channel mask used when CONFIG_FH_ENABLE is false.
 Each bit indicates if the corresponding channel is to be scanned
 First byte represents channels 0 to 7 and the last byte represents
 channels 128 to 135.
 For byte zero in the bit mask, LSB representing Ch0.
 For byte 1, LSB represents Ch8 and so on.
 e.g., 0x01 0x10 represents Ch0 and Ch12 are included.
 The default of 0x0F represents channels 0-3 are selected.
 APIMAC_STD_US_915_PHY_1 (50kbps/2-FSK/915MHz band) has channels 0 - 128.
 APIMAC_STD_ETSI_863_PHY_3 (50kbps/2-FSK/863MHz band) has channels 0 - 33.
*/
#define CONFIG_CHANNEL_MASK           { 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, \
                                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
                                        0x00, 0x00, 0x00, 0x00, 0x00 }
/*!
 Channel mask used when CONFIG_FH_ENABLE is true.
 Represents the list of channels on which the device can hop.
 The actual sequence used shall be based on DH1CF function.
 It is represented as a bit string with LSB representing Ch0.
 e.g., 0x01 0x10 represents Ch0 and Ch12 are included.
 */
#define CONFIG_FH_CHANNEL_MASK        { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, \
                                        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, \
                                        0xFF, 0xFF, 0xFF, 0xFF, 0xFF }

/*!
 List of channels to target the Async frames
 It is represented as a bit string with LSB representing Ch0
 e.g., 0x01 0x10 represents Ch0 and Ch12 are included
 It should cover all channels that could be used by a target device in its
 hopping sequence. Channels marked beyond number of channels supported by
 PHY Config will be excluded by stack. To avoid interference on a channel,
 it should be removed from Async Mask and added to exclude channels
 (CONFIG_CHANNEL_MASK).
 */
#define FH_ASYNC_CHANNEL_MASK         { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, \
                                        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, \
                                        0xFF, 0xFF, 0xFF, 0xFF, 0xFF }

/* FH related config variables */
/*!
 The number of non sleepy end devices to be supported.
 It is to be noted that the total number of devices supported (sleepy/
 non sleepy) must be less than 50. Stack will allocate memory proportional
 to the number of end devices requested.
 */
#define FH_NUM_NON_SLEEPY_NEIGHBORS  0
/*!
 The number of sleepy end devices to be supported.
 It is to be noted that the total number of devices supported (sleepy/
 non sleepy) must be less than 50. Stack will allocate memory proportional
 to the number of end devices requested.
 */
#define FH_NUM_SLEEPY_NEIGHBORS  50
/*!
 Dwell time: The duration for which the collector will
 stay on a specific channel before hopping to next channel.
 */
#define CONFIG_DWELL_TIME            250
/*!
 The minimum trickle timer window for PAN Advertisement,
 and PAN Configuration frame transmissions.
 Recommended to set this to half of PAS/PCS MIN Timer
*/
#define CONFIG_TRICKLE_MIN_CLK_DURATION    3000
/*!
 The maximum trickle timer window for PAN Advertisement,
 and PAN Configuration frame transmissions.
 */
#define CONFIG_TRICKLE_MAX_CLK_DURATION    6000
/* default value for PAN Size PIB */
#define CONFIG_FH_PAN_SIZE                0x0032
/*!
 To enable Doubling of PA/PC trickle time,
 useful when network has non sleepy nodes and
 there is a requirement to use PA/PC to convey updated
 PAN information
*/
#define CONFIG_DOUBLE_TRICKLE_TIMER    false
/*! value for ApiMac_FHAttribute_netName */
#define CONFIG_FH_NETNAME            {"FHTest"}
/*!
 Value for Transmit Power in dBm
 Default value is 14, allowed values are any value
 between 0 dBm and 14 dBm in 1 dB increments, and -10 dBm
 When the nodes in the network are close to each other
 lowering this value will help reduce saturation */
#define CONFIG_TRANSMIT_POWER        14
/*!
* Enable this mode for certfication.
* For FH certification, CONFIG_FH_ENABLE should
* also be enabled.
*/
#define CERTIFICATION_TEST_MODE     false

/* Check if all the necessary parameters have been set for FH mode */
#if CONFIG_FH_ENABLE
#if !defined(FEATURE_ALL_MODES) && !defined(FEATURE_FREQ_HOP_MODE)
#error "Do you want to build image with frequency hopping mode? \
        Define either FEATURE_FREQ_HOP_MODE or FEATURE_ALL_MODES in features.h"
#endif
#endif

/* Check if stack level security is enabled if application security is enabled */
#if CONFIG_SECURE
#if !defined(FEATURE_MAC_SECURITY)
#error "Define FEATURE_MAC_SECURITY or FEATURE_ALL_MODES in features.h to \
        be able to use security at application level"
#endif
#endif

/* Set beacon order and superframe order to 15 for FH mode to avoid user error */
#if CONFIG_FH_ENABLE
#if (CONFIG_MAC_BEACON_ORDER != 15) && (CONFIG_MAC_SUPERFRAME_ORDER != 15)
#error "Do you want to build image with frequency hopping mode? \
    If yes, CONFIG_MAC_BEACON_ORDER and CONFIG_MAC_SUPERFRAME_ORDER \
    should both be set to 15"
#endif
#endif

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_H */

