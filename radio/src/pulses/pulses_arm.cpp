/*
 * Copyright (C) OpenTX
 *
 * Based on code named
 *   th9x - http://code.google.com/p/th9x
 *   er9x - http://code.google.com/p/er9x
 *   gruvin9x - http://code.google.com/p/gruvin9x
 *
 * License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "opentx.h"

uint8_t s_pulses_paused = 0;
uint8_t s_current_protocol[NUM_MODULES] = { MODULES_INIT(255) };
uint16_t failsafeCounter[NUM_MODULES] = { MODULES_INIT(100) };
uint8_t moduleFlag[NUM_MODULES] = { 0 };

ModulePulsesData modulePulsesData[NUM_MODULES] __DMA;
TrainerPulsesData trainerPulsesData __DMA;
OS_FlagID pulseFlag = 0;

int32_t GetChannelValue(uint8_t channel) {
  return channelOutputs[channel] + 2*PPM_CH_CENTER(channel) - 2*PPM_CENTER;
}

#if defined(CROSSFIRE)
uint8_t createCrossfireChannelsFrame(uint8_t * frame, int16_t * pulses);
#endif
#if defined(AFHDS3)
#include "telemetry.h"
afhds3::afhds3 afhds3uart = afhds3::afhds3(
  &modulePulsesData[EXTERNAL_MODULE].flysky, 
  &g_model.moduleData[EXTERNAL_MODULE],
  GetChannelValue,
  processFlySkySensor
);
#endif
//only valid for external
void onBind(bool success) {
  moduleFlag[EXTERNAL_MODULE] = MODULE_NORMAL_MODE;
}
void setModuleFlag(uint8_t port, uint8_t value) {
  if(moduleFlag[port] == value) return;
  moduleFlag[port] = value;
  if (value == MODULE_NORMAL_MODE && isModuleFlysky(port) && s_current_protocol[port] == PROTO_FLYSKY)
    resetPulsesFlySky(port);
#if defined(AFHDS3)
  if (isModuleAFHDS3(port) && s_current_protocol[port] == PROTO_AFHDS3) {
    switch (value) {
    case MODULE_NORMAL_MODE:
      afhds3uart.cancel();
      break;
    case MODULE_BIND:
      afhds3uart.bind(onBind);
      break;
    case MODULE_RANGECHECK:
      afhds3uart.range(nullptr);
      break;
    case MODULE_RESET_SETTINGS:
      afhds3uart.setToDefault();
     break;
    }
  }
#endif
}


uint8_t getRequiredProtocol(uint8_t port)
{
  uint8_t required_protocol;

  switch (port) {
#if defined(INTMODULE)
    case INTERNAL_MODULE:
      switch (g_model.moduleData[INTERNAL_MODULE].type) {
#if defined(TARANIS_INTERNAL_PPM)
        case MODULE_TYPE_PPM:
          required_protocol = PROTO_PPM;
          break;
#endif
#if defined(PCBFLYSKY)
        case MODULE_TYPE_FLYSKY:
          required_protocol = PROTO_FLYSKY;
          break;
#endif
        case MODULE_TYPE_XJT:
          required_protocol = PROTO_PXX;
          break;
        default:
          required_protocol = PROTO_NONE;
          break;
      }
      break;
#endif

    default:
      port = EXTERNAL_MODULE; // ensure it's external module only
      switch (g_model.moduleData[EXTERNAL_MODULE].type) {
        case MODULE_TYPE_PPM:
          required_protocol = PROTO_PPM;
          break;
        case MODULE_TYPE_XJT:
        case MODULE_TYPE_R9M:
          required_protocol = PROTO_PXX;
          break;
#if defined(PCBFLYSKY)
        case MODULE_TYPE_FLYSKY:
          required_protocol = PROTO_FLYSKY;
          break;
#endif
        case MODULE_TYPE_SBUS:
          required_protocol = PROTO_SBUS;
          break;
#if defined(MULTIMODULE)
        case MODULE_TYPE_MULTIMODULE:
          required_protocol = PROTO_MULTIMODULE;
          break;
#endif
#if defined(DSM2)
        case MODULE_TYPE_DSM2:
          required_protocol = limit<uint8_t>(PROTO_DSM2_LP45, PROTO_DSM2_LP45+g_model.moduleData[EXTERNAL_MODULE].rfProtocol, PROTO_DSM2_DSMX);
          // The module is set to OFF during one second before BIND start
          {
            static tmr10ms_t bindStartTime = 0;
            if (moduleFlag[EXTERNAL_MODULE] == MODULE_BIND) {
              if (bindStartTime == 0) bindStartTime = get_tmr10ms();
              if ((tmr10ms_t)(get_tmr10ms() - bindStartTime) < 100) {
                required_protocol = PROTO_NONE;
                break;
              }
            }
            else {
              bindStartTime = 0;
            }
          }
          break;
#endif
#if defined(CROSSFIRE)
        case MODULE_TYPE_CROSSFIRE:
          required_protocol = PROTO_CROSSFIRE;
          break;
#endif
#if defined(AFHDS3)
        case MODULE_TYPE_AFHDS3:
          required_protocol = PROTO_AFHDS3;
          break;
#endif
        default:
          required_protocol = PROTO_NONE;
          break;
      }
      break;
  }

  if (s_pulses_paused) {
    required_protocol = PROTO_NONE;
  }

#if 0
  // will need an EEPROM conversion
  if (moduleFlag[port] == MODULE_OFF) {
    required_protocol = PROTO_NONE;
  }
#endif

  return required_protocol;
}

void setupPulses(uint8_t port)
{
  bool init_needed = false;
  uint8_t required_protocol = getRequiredProtocol(port);

  heartbeat |= (HEART_TIMER_PULSES << port);

  if (s_current_protocol[port] != required_protocol) {
    init_needed = true;
    switch (s_current_protocol[port]) { // stop existing protocol hardware
#if defined(PCBFLYSKY)
      case PROTO_FLYSKY:
#endif

      case PROTO_PXX:
        disable_pxx(port);
        break;
#if defined(AFHDS3)
      case PROTO_AFHDS3:
        afhds3uart.stop();
        disable_afhds3(port);
        break;
#endif
#if defined(DSM2)
      case PROTO_DSM2_LP45:
      case PROTO_DSM2_DSM2:
      case PROTO_DSM2_DSMX:
        disable_dsm2(port);
        break;
#endif

#if defined(CROSSFIRE)
      case PROTO_CROSSFIRE:
        disable_crossfire(port);
        break;
#endif

#if defined(MULTIMODULE)
      case PROTO_MULTIMODULE:
#endif
      case PROTO_SBUS:
        disable_sbusOut(port);
        break;

      case PROTO_PPM:
        disable_ppm(port);
        break;

      default:
        disable_no_pulses(port);
        break;
    }
    s_current_protocol[port] = required_protocol;
  }

  // Set up output data here
  switch (required_protocol) {
#if defined(PCBFLYSKY)
    case PROTO_FLYSKY:
      if (init_needed) resetPulsesFlySky(port);
      setupPulsesFlySky(port);
      scheduleNextMixerCalculation(port, FLYSKY_PERIOD);
      break;
#endif
#if defined(AFHDS3)
    case PROTO_AFHDS3:
      if (init_needed) afhds3uart.reset();
      afhds3uart.setupPulses();
      scheduleNextMixerCalculation(port, AFHDS3_PERIOD);
      break;
#endif
    case PROTO_PXX:
      setupPulsesPXX(port);
      scheduleNextMixerCalculation(port, PXX_PERIOD);
      break;

    case PROTO_SBUS:
      setupPulsesSbus(port);
      scheduleNextMixerCalculation(port, SBUS_PERIOD);
      break;

    case PROTO_DSM2_LP45:
    case PROTO_DSM2_DSM2:
    case PROTO_DSM2_DSMX:
      setupPulsesDSM2(port);
      scheduleNextMixerCalculation(port, DSM2_PERIOD);
      break;

#if defined(CROSSFIRE)
    case PROTO_CROSSFIRE:
      if (telemetryProtocol == PROTOCOL_PULSES_CROSSFIRE && !init_needed) {
        uint8_t * crossfire = modulePulsesData[port].crossfire.pulses;
        uint8_t len;
#if defined(LUA) || defined(CROSSFIRE_NATIVE)
        if (outputTelemetryBufferTrigger != 0x00 && outputTelemetryBufferSize > 0) {
          memcpy(crossfire, outputTelemetryBuffer, outputTelemetryBufferSize);
          len = outputTelemetryBufferSize;
          outputTelemetryBufferTrigger = 0x00;
          outputTelemetryBufferSize = 0;
        }
        else
#endif
        {
          len = createCrossfireChannelsFrame(crossfire, &channelOutputs[g_model.moduleData[port].channelsStart]);
        }
        sportSendBuffer(crossfire, len);
      }
      scheduleNextMixerCalculation(port, CROSSFIRE_FRAME_PERIOD);
      break;
#endif

#if defined(MULTIMODULE)
    case PROTO_MULTIMODULE:
      setupPulsesMultimodule(port);
      scheduleNextMixerCalculation(port, 4);
      break;
#endif

    case PROTO_PPM:
#if defined(PCBSKY9X)
    case PROTO_NONE:
#endif
      setupPulsesPPMModule(port);
      scheduleNextMixerCalculation(port, (45+g_model.moduleData[port].ppm.frameLength)/2);
      break;

    default:
      break;
  }

  if (init_needed) {
    switch (required_protocol) { // Start new protocol hardware here
#if defined(PCBFLYSKY)
      case PROTO_FLYSKY:
        init_serial(port, INTMODULE_USART_AFHDS2_BAUDRATE, FLYSKY_PERIOD_HALF_US);
        break;
#endif
      case PROTO_PXX:
        init_pxx(port);
        break;
#if defined(AFHDS3)
      case PROTO_AFHDS3:
        init_afhds3(port);
        break;
#endif
#if defined(DSM2)
      case PROTO_DSM2_LP45:
      case PROTO_DSM2_DSM2:
      case PROTO_DSM2_DSMX:
        init_dsm2(port);
        break;
#endif

#if defined(CROSSFIRE)
      case PROTO_CROSSFIRE:
        init_crossfire(port);
        break;
#endif

#if defined(MULTIMODULE)
      case PROTO_MULTIMODULE:
#endif
      case PROTO_SBUS:
        init_sbusOut(port);
        break;

      case PROTO_PPM:
        init_ppm(port);
        break;

      default:
        init_no_pulses(port);
        break;
    }
  }
#if !defined(SIMU)
  if(pulseFlag && s_current_protocol[INTERNAL_MODULE] == PROTO_NONE && s_current_protocol[EXTERNAL_MODULE] == PROTO_NONE) {
    CoSetFlag(pulseFlag);
  }
#endif
}

void setCustomFailsafe(uint8_t moduleIndex)
{
  if (moduleIndex < NUM_MODULES) {
    int minChannel = g_model.moduleData[moduleIndex].channelsStart;
    int maxChannel = minChannel + 8 + g_model.moduleData[moduleIndex].channelsCount;
    //we should not set custom values for channels that are not selected!
    for (int ch=0; ch < MAX_OUTPUT_CHANNELS; ch++) {
      if (ch < minChannel || ch >= maxChannel) { //channel not configured
        g_model.moduleData[moduleIndex].failsafeChannels[ch] = 0;
      }
      else if (g_model.moduleData[moduleIndex].failsafeChannels[ch] < FAILSAFE_CHANNEL_HOLD) {
        g_model.moduleData[moduleIndex].failsafeChannels[ch] = channelOutputs[ch];
      }
    }
  }
}
