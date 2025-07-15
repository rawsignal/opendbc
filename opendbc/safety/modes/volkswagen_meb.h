#pragma once

#include "opendbc/safety/safety_declarations.h"
#include "opendbc/safety/modes/volkswagen_common.h"

#define MSG_ESC_51           0xFC    // RX, for wheel speeds
#define MSG_Motor_54         0x14C   // RX, for accel pedal
#define MSG_HCA_03           0x303   // TX by OP, Heading Control Assist steering torque
#define MSG_QFK_01           0x13D   // RX, for steering angle
#define MSG_MEB_ACC_01       0x300   // RX from ECU, for ACC status
#define MSG_ACC_18           0x14D   // RX from ECU, for ACC status
#define MSG_GRA_ACC_01       0x12B   // TX by OP, ACC control buttons for cancel/resume
#define MSG_MOTOR_14         0x3BE   // RX from ECU, for brake switch status
#define MSG_LDW_02           0x397   // TX by OP, Lane line recognition and text alerts
#define MSG_Motor_51         0x10B   // RX for TSK state
#define MSG_TA_01            0x26B   // TX for Travel Assist status
#define MSG_EA_01            0x1A4   // TX, for EA mitigation
#define MSG_EA_02            0x1F0   // TX, for EA mitigation
#define MSG_KLR_01           0x25D   // TX, for capacitive steering wheel
#define MSG_Panda_Data_01    0x50A6EDA   // internal use, data for panda from OP sensors

static uint8_t volkswagen_crc8_lut_8h2f[256]; // Static lookup table for CRC8 poly 0x2F, aka 8H2F/AUTOSAR

static bool vw_meb_get_longitudinal_allowed_override(void) {
  return controls_allowed && gas_pressed_prev;
}

static bool vw_meb_max_limit_check(int val, const int MAX_VAL, const int MIN_VAL) {
  return (val > MAX_VAL) || (val < MIN_VAL);
}

// Safety checks for longitudinal actuation
static bool vw_meb_longitudinal_accel_checks(int desired_accel, const LongitudinalLimits limits, const int override_accel) {
  bool accel_valid = get_longitudinal_allowed() && !vw_meb_max_limit_check(desired_accel, limits.max_accel, limits.min_accel);
  bool accel_valid_override = vw_meb_get_longitudinal_allowed_override() && desired_accel == override_accel;
  bool accel_inactive = desired_accel == limits.inactive_accel;
  return !(accel_valid || accel_inactive || accel_valid_override);
}

static uint32_t volkswagen_meb_get_checksum(const CANPacket_t *to_push) {
  return (uint8_t)GET_BYTE(to_push, 0);
}

static uint8_t volkswagen_meb_get_counter(const CANPacket_t *to_push) {
  // MQB message counters are consistently found at LSB 8.
  return (uint8_t)GET_BYTE(to_push, 1) & 0xFU;
}

static uint32_t volkswagen_meb_compute_crc(const CANPacket_t *to_push) {
  int addr = GET_ADDR(to_push);
  int len = GET_LEN(to_push);

  // This is CRC-8H2F/AUTOSAR with a twist. See the OpenDBC implementation
  // of this algorithm for a version with explanatory comments.

  uint8_t crc = 0xFFU;
  for (int i = 1; i < len; i++) {
    crc ^= (uint8_t)GET_BYTE(to_push, i);
    crc = volkswagen_crc8_lut_8h2f[crc];
  }

  uint8_t counter = volkswagen_meb_get_counter(to_push);
  if (addr == MSG_LH_EPS_03) {
    crc ^= (uint8_t[]){0xF5,0xF5,0xF5,0xF5,0xF5,0xF5,0xF5,0xF5,0xF5,0xF5,0xF5,0xF5,0xF5,0xF5,0xF5,0xF5}[counter];
  } else if (addr == MSG_GRA_ACC_01) {
    crc ^= (uint8_t[]){0x6A,0x38,0xB4,0x27,0x22,0xEF,0xE1,0xBB,0xF8,0x80,0x84,0x49,0xC7,0x9E,0x1E,0x2B}[counter];
  } else if (addr == MSG_QFK_01) {
    crc ^= (uint8_t[]){0x20,0xCA,0x68,0xD5,0x1B,0x31,0xE2,0xDA,0x08,0x0A,0xD4,0xDE,0x9C,0xE4,0x35,0x5B}[counter];
  } else if (addr == MSG_ESC_51) {
    crc ^= (uint8_t[]){0x77,0x5C,0xA0,0x89,0x4B,0x7C,0xBB,0xD6,0x1F,0x6C,0x4F,0xF6,0x20,0x2B,0x43,0xDD}[counter];
  } else if (addr == MSG_Motor_54) {
    crc ^= (uint8_t[]){0x16,0x35,0x59,0x15,0x9A,0x2A,0x97,0xB8,0x0E,0x4E,0x30,0xCC,0xB3,0x07,0x01,0xAD}[counter];
  } else if (addr == MSG_Motor_51) {
    crc ^= (uint8_t[]){0x77,0x5C,0xA0,0x89,0x4B,0x7C,0xBB,0xD6,0x1F,0x6C,0x4F,0xF6,0x20,0x2B,0x43,0xDD}[counter];
  } else if (addr == MSG_MOTOR_14) {
    crc ^= (uint8_t[]){0x1F,0x28,0xC6,0x85,0xE6,0xF8,0xB0,0x19,0x5B,0x64,0x35,0x21,0xE4,0xF7,0x9C,0x24}[counter];
  } else if (addr == MSG_KLR_01) {
    crc ^= (uint8_t[]){0xDA,0x6B,0x0E,0xB2,0x78,0xBD,0x5A,0x81,0x7B,0xD6,0x41,0x39,0x76,0xB6,0xD7,0x35}[counter];
  } else if (addr == MSG_EA_02) {
    crc ^= (uint8_t[]){0x2F,0x3C,0x22,0x60,0x18,0xEB,0x63,0x76,0xC5,0x91,0x0F,0x27,0x34,0x04,0x7F,0x02}[counter];
  }
  else {
    // Undefined CAN message, CRC check expected to fail
  }
  crc = volkswagen_crc8_lut_8h2f[crc];

  return (uint8_t)(crc ^ 0xFFU);
}

static safety_config volkswagen_meb_init(uint16_t param) {
  // Transmit of GRA_ACC_01 is allowed on bus 0 and 2 to keep compatibility with gateway and camera integration
  static const CanMsg VOLKSWAGEN_MEB_STOCK_TX_MSGS[] = {{MSG_HCA_03, 0, 24, .check_relay = true}, {MSG_GRA_ACC_01, 0, 8, .check_relay = false},
                                                       {MSG_EA_01, 0, 8, .check_relay = false}, {MSG_EA_02, 0, 8, .check_relay = true},
                                                       {MSG_GRA_ACC_01, 2, 8, .check_relay = false}, {MSG_LDW_02, 0, 8, .check_relay = true}};

  static const CanMsg VOLKSWAGEN_MEB_LONG_TX_MSGS[] = {{MSG_MEB_ACC_01, 0, 48, .check_relay = true}, {MSG_ACC_18, 0, 32, .check_relay = true}, {MSG_HCA_03, 0, 24, .check_relay = true},
                                                       {MSG_EA_01, 0, 8, .check_relay = false}, {MSG_EA_02, 0, 8, .check_relay = true},
                                                       {MSG_LDW_02, 0, 8, .check_relay = true}, {MSG_TA_01, 0, 8, .check_relay = true}};

  static RxCheck volkswagen_meb_rx_checks[] = {
    {.msg = {{MSG_LH_EPS_03, 0, 8, .max_counter = 15U, .ignore_quality_flag = true, .frequency = 100U}, { 0 }, { 0 }}},
    {.msg = {{MSG_MOTOR_14, 0, 8, .max_counter = 15U, .ignore_quality_flag = true, .frequency = 10U}, { 0 }, { 0 }}},
    {.msg = {{MSG_Motor_51, 0, 32, .max_counter = 15U, .ignore_quality_flag = true, .frequency = 50U}, { 0 }, { 0 }}},
    {.msg = {{MSG_GRA_ACC_01, 0, 8, .max_counter = 15U, .ignore_quality_flag = true, .frequency = 33U}, { 0 }, { 0 }}},
    {.msg = {{MSG_QFK_01, 0, 32, .max_counter = 15U, .ignore_quality_flag = true, .frequency = 100U}, { 0 }, { 0 }}},
    {.msg = {{MSG_ESC_51, 0, 48, .max_counter = 15U, .ignore_quality_flag = true, .frequency = 100U}, { 0 }, { 0 }}},
    {.msg = {{MSG_Motor_54, 0, 32, .max_counter = 15U, .ignore_quality_flag = true, .frequency = 10U}, { 0 }, { 0 }}},
  };

  UNUSED(param);

  volkswagen_set_button_prev = false;
  volkswagen_resume_button_prev = false;

#ifdef ALLOW_DEBUG
  volkswagen_longitudinal = GET_FLAG(param, FLAG_VOLKSWAGEN_LONG_CONTROL);
#endif
  gen_crc_lookup_table_8(0x2F, volkswagen_crc8_lut_8h2f);
  return volkswagen_longitudinal ? BUILD_SAFETY_CFG(volkswagen_meb_rx_checks, VOLKSWAGEN_MEB_LONG_TX_MSGS) : \
                                   BUILD_SAFETY_CFG(volkswagen_meb_rx_checks, VOLKSWAGEN_MEB_STOCK_TX_MSGS);
}

// lateral limits for curvature
static const CurvatureSteeringLimits VOLKSWAGEN_MEB_STEERING_LIMITS = {
  .max_curvature = 29105, // 0.195 rad/m
  .curvature_to_can = 149253.7313, // 1 / 6.7e-6 rad/m to can
  .send_rate = 0.02,
  .inactive_curvature_is_zero = true,
  .roll_to_can = 10000.0,
};

static void volkswagen_meb_rx_hook(const CANPacket_t *to_push) {
  if (GET_BUS(to_push) == 0U) {
    int addr = GET_ADDR(to_push);

    // Update in-motion state by sampling wheel speeds
    if (addr == MSG_ESC_51) {
      uint32_t fr = GET_BYTE(to_push, 10U) | GET_BYTE(to_push, 11U) << 8;
      uint32_t rr = GET_BYTE(to_push, 14U) | GET_BYTE(to_push, 15U) << 8;
      uint32_t rl = GET_BYTE(to_push, 12U) | GET_BYTE(to_push, 13U) << 8;
      uint32_t fl = GET_BYTE(to_push, 8U) | GET_BYTE(to_push, 9U) << 8;

      vehicle_moving = (fr > 0U) || (rr > 0U) || (rl > 0U) || (fl > 0U);

      UPDATE_VEHICLE_SPEED(((fr + rr + rl + fl) / 4 ) * 0.0075 / 3.6);
    }

    if (addr == MSG_QFK_01) { // we do not need conversion deg to can, same scaling as HCA_03 curvature
      int current_curvature = ((GET_BYTE(to_push, 5U) & 0x7F) << 8 | GET_BYTE(to_push, 4U));

      bool current_curvature_sign = GET_BIT(to_push, 55U);
      if (!current_curvature_sign) {
        current_curvature *= -1;
      }

      update_sample(&curvature_meas, current_curvature);
    }

    // Update driver input torque samples
    // Signal: LH_EPS_03.EPS_Lenkmoment (absolute torque)
    // Signal: LH_EPS_03.EPS_VZ_Lenkmoment (direction)
    if (addr == MSG_LH_EPS_03) {
      int torque_driver_new = GET_BYTE(to_push, 5U) | ((GET_BYTE(to_push, 6U) & 0x1FU) << 8);
      int sign_driver_torque = (GET_BYTE(to_push, 6U) & 0x80U) >> 7;
      if (sign_driver_torque == 1) {
        torque_driver_new *= -1;
      }
      update_sample(&torque_driver, torque_driver_new);
    }

    // Update cruise state
    if (addr == MSG_Motor_51) {
      // When using stock ACC, enter controls on rising edge of stock ACC engage, exit on disengage
      // Always exit controls on main switch off
      // Signal: TSK_06.TSK_Status
      int acc_status = ((GET_BYTE(to_push, 11U) >> 0) & 0x07U);
      bool cruise_engaged = (acc_status == 3) || (acc_status == 4) || (acc_status == 5);
      acc_main_on = cruise_engaged || (acc_status == 2);

      if (!volkswagen_longitudinal) {
        pcm_cruise_check(cruise_engaged);
      }

      if (!acc_main_on) {
        controls_allowed = false;
      }
    }

    // update cruise buttons
    if (addr == MSG_GRA_ACC_01) {
      // If using openpilot longitudinal, enter controls on falling edge of Set or Resume with main switch on
      // Signal: GRA_ACC_01.GRA_Tip_Setzen
      // Signal: GRA_ACC_01.GRA_Tip_Wiederaufnahme
      if (volkswagen_longitudinal) {
        bool set_button = GET_BIT(to_push, 16U);
        bool resume_button = GET_BIT(to_push, 19U);
        if ((volkswagen_set_button_prev && !set_button) || (volkswagen_resume_button_prev && !resume_button)) {
          controls_allowed = acc_main_on;
        }
        volkswagen_set_button_prev = set_button;
        volkswagen_resume_button_prev = resume_button;
      }
      // Always exit controls on rising edge of Cancel
      // Signal: GRA_ACC_01.GRA_Abbrechen
      if (GET_BIT(to_push, 13U)) {
        controls_allowed = false;
      }
    }

    // update brake pedal
    if (addr == MSG_MOTOR_14) {
      brake_pressed = GET_BIT(to_push, 28U);
    }

    // update accel pedal
    if (addr == MSG_Motor_54) {
      int accel_pedal_value = GET_BYTE(to_push, 21U) - 37;
      gas_pressed = accel_pedal_value != 0;
    }

  }
}

static bool volkswagen_meb_tx_hook(const CANPacket_t *to_send) {
  // longitudinal limits
  // acceleration in m/s2 * 1000 to avoid floating point math
  const LongitudinalLimits VOLKSWAGEN_MEB_LONG_LIMITS = {
    .max_accel = 2000,
    .min_accel = -3500,
    .inactive_accel = 3010,  // VW sends one increment above the max range when inactive
  };

  const int volkswagen_accel_override = 0;

  int addr = GET_ADDR(to_send);
  bool tx = true;

  // PANDA DATA is a custom CAN messages for internal use only, transferring roll from OP for safety checks
  if (addr == MSG_Panda_Data_01) {
    int current_roll = (GET_BYTE(to_send, 0U) | ((GET_BYTE(to_send, 1U) & 0x7F) << 8));

    bool current_roll_sign = GET_BIT(to_send, 15U);
    if (!current_roll_sign) {
      current_roll *= -1;
    }

    update_sample(&roll, current_roll);
    tx = false;
  }

  // Safety check for HCA_03 Heading Control Assist curvature
  if (addr == MSG_HCA_03) {
    int desired_curvature_raw = GET_BYTES(to_send, 3, 2) & 0x7FFFU;

    bool desired_curvature_sign = GET_BIT(to_send, 39U);
    if (!desired_curvature_sign) {
      desired_curvature_raw *= -1;
    }

    bool steer_req = (((GET_BYTE(to_send, 1U) >> 4) & 0x0FU) == 4U);
    int steer_power = (GET_BYTE(to_send, 2U) >> 0) & 0x7FU;

    if (steer_curvature_cmd_checks(desired_curvature_raw, steer_power, steer_req, VOLKSWAGEN_MEB_STEERING_LIMITS)) {
      tx = false;
    }
  }

  // Safety check for MSG_ACC_18 acceleration requests
  // To avoid floating point math, scale upward and compare to pre-scaled safety m/s2 boundaries
  if (addr == MSG_ACC_18) {
    // WARNING: IF WE TAKE THE SIGNAL FROM THE CAR WHILE ACC ACTIVE AND BELOW ABOUT 3km/h, THE CAR ERRORS AND PUTS ITSELF IN PARKING MODE WITH EPB!
    int desired_accel = ((((GET_BYTE(to_send, 4) & 0x7U) << 8) | GET_BYTE(to_send, 3)) * 5U) - 7220U;

    if (vw_meb_longitudinal_accel_checks(desired_accel, VOLKSWAGEN_MEB_LONG_LIMITS, volkswagen_accel_override)) {
      tx = false;
    }
  }

  // FORCE CANCEL: ensuring that only the cancel button press is sent when controls are off.
  // This avoids unintended engagements while still allowing resume spam
  if ((addr == MSG_GRA_ACC_01) && !controls_allowed) {
    // disallow resume and set: bits 16 and 19
    if ((GET_BYTE(to_send, 2) & 0x9U) != 0U) {
      tx = false;
    }
  }

  return tx;
}

const safety_hooks volkswagen_meb_hooks = {
  .init = volkswagen_meb_init,
  .rx = volkswagen_meb_rx_hook,
  .tx = volkswagen_meb_tx_hook,
  .get_counter = volkswagen_meb_get_counter,
  .get_checksum = volkswagen_meb_get_checksum,
  .compute_checksum = volkswagen_meb_compute_crc,
};
