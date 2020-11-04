// Include Tesla radar safety code
#include "safety_teslaradar.h"

// Safety-relevant steering constants for Volkswagen
const int VOLKSWAGEN_MAX_STEER = 300;               // 3.0 Nm (EPS side max of 3.0Nm with fault if violated)
const int VOLKSWAGEN_MAX_RT_DELTA = 188;             // 4 max rate up * 50Hz send rate * 250000 RT interval / 1000000 = 50 ; 50 * 1.5 for safety pad = 75
const uint32_t VOLKSWAGEN_RT_INTERVAL = 250000;     // 250ms between real time checks
const int VOLKSWAGEN_MAX_RATE_UP = 10;               // 2.0 Nm/s RoC limit (EPS rack has own soft-limit of 5.0 Nm/s)
const int VOLKSWAGEN_MAX_RATE_DOWN = 10;            // 5.0 Nm/s RoC limit (EPS rack has own soft-limit of 5.0 Nm/s)
const int VOLKSWAGEN_DRIVER_TORQUE_ALLOWANCE = 80;
const int VOLKSWAGEN_DRIVER_TORQUE_FACTOR = 3;
const int VOLKSWAGEN_GAS_INTERCEPTOR_THRSLD = 475;  // ratio between offset and gain from dbc file

// Safety-relevant CAN messages for the Volkswagen MQB platform
#define MSG_ESP_19      0x0B2   // RX from ABS, for wheel speeds
#define MSG_EPS_01      0x09F   // RX from EPS, for driver steering torque
#define MSG_ESP_05      0x106   // RX from ABS, for brake switch state
#define MSG_TSK_06      0x120   // RX from ECU, for ACC status from drivetrain coordinator
#define MSG_MOTOR_20    0x121   // RX from ECU, for driver throttle input
#define MSG_HCA_01      0x126   // TX by OP, Heading Control Assist steering torque
#define MSG_GRA_ACC_01  0x12B   // TX by OP, ACC control buttons for cancel/resume
#define MSG_LDW_02      0x397   // TX by OP, Lane line recognition and text alerts

// Transmit of GRA_ACC_01 is allowed on bus 0 and 2 to keep compatibility with gateway and camera integration
const AddrBus VOLKSWAGEN_MQB_TX_MSGS[] = {{MSG_HCA_01, 0}, {MSG_GRA_ACC_01, 0}, {MSG_GRA_ACC_01, 2}, {MSG_LDW_02, 0}};
const int VOLKSWAGEN_MQB_TX_MSGS_LEN = sizeof(VOLKSWAGEN_MQB_TX_MSGS) / sizeof(VOLKSWAGEN_MQB_TX_MSGS[0]);

AddrCheckStruct volkswagen_mqb_rx_checks[] = {
  {.addr = {MSG_ESP_19},   .bus = 0, .check_checksum = false, .max_counter = 0U,  .expected_timestep = 10000U},
  {.addr = {MSG_EPS_01},   .bus = 0, .check_checksum = true,  .max_counter = 15U, .expected_timestep = 10000U},
  {.addr = {MSG_ESP_05},   .bus = 0, .check_checksum = true,  .max_counter = 15U, .expected_timestep = 20000U},
  {.addr = {MSG_TSK_06},   .bus = 0, .check_checksum = true,  .max_counter = 15U, .expected_timestep = 20000U},
  {.addr = {MSG_MOTOR_20}, .bus = 0, .check_checksum = true,  .max_counter = 15U, .expected_timestep = 20000U},
};
const int VOLKSWAGEN_MQB_RX_CHECKS_LEN = sizeof(volkswagen_mqb_rx_checks) / sizeof(volkswagen_mqb_rx_checks[0]);

// Safety-relevant CAN messages for the Volkswagen PQ35/PQ46/NMS platforms
#define MSG_LENKHILFE_3 0x0D0   // RX from EPS, for steering angle and driver steering torque
#define MSG_HCA_1       0x0D2   // TX by OP, Heading Control Assist steering torque
#define MSG_MOB_1       0x284   // TX by OP, Braking Control
#define MSG_GAS_COMMAND 0x200   // TX by OP, Gas Control
#define MSG_GAS_SENSOR  0x201   // RX from Pedal
#define MSG_MOTOR_2     0x288   // RX from ECU, for CC state and brake switch state
#define MSG_MOTOR_3     0x380   // RX from ECU, for driver throttle input
#define MSG_GRA_NEU     0x38A   // TX by OP, ACC control buttons for cancel/resume
#define MSG_BREMSE_3    0x4A0   // RX from ABS, for wheel speeds
#define MSG_LDW_1       0x5BE   // TX by OP, Lane line recognition and text alerts

// CAN Messages for Tesla Radar
#define MSG_TESLA_VIN   0x560   // TX by OP, Tesla VIN Message
#define MSG_TESLA_x2B9  0x2B9
#define MSG_TESLA_x159  0x159
#define MSG_TESLA_x219  0x219
#define MSG_TESLA_x149  0x149
#define MSG_TESLA_x129  0x129
#define MSG_TESLA_x1A9  0x1A9
#define MSG_TESLA_x199  0x199
#define MSG_TESLA_x169  0x169
#define MSG_TESLA_x119  0x119
#define MSG_TESLA_x109  0x109

// Transmit of GRA_Neu is allowed on bus 0 and 2 to keep compatibility with gateway and camera integration
const AddrBus VOLKSWAGEN_PQ_TX_MSGS[] = {{MSG_HCA_1, 0}, {MSG_GRA_NEU, 0}, {MSG_GRA_NEU, 1}, {MSG_GRA_NEU, 2}, {MSG_LDW_1, 0}, {MSG_MOB_1, 1}, {MSG_GAS_COMMAND, 2}, {MSG_TESLA_x2B9, 2}, {MSG_TESLA_x159, 2}, {MSG_TESLA_x219, 2}, {MSG_TESLA_x149 , 2}, {MSG_TESLA_x129 ,2}, {MSG_TESLA_x1A9 ,2}, {MSG_TESLA_x199 ,2}, {MSG_TESLA_x169 ,2}, {MSG_TESLA_x119 ,2}, {MSG_TESLA_x109 ,2}, {MSG_TESLA_VIN, 2}};
const int VOLKSWAGEN_PQ_TX_MSGS_LEN = sizeof(VOLKSWAGEN_PQ_TX_MSGS) / sizeof(VOLKSWAGEN_PQ_TX_MSGS[0]);

AddrCheckStruct volkswagen_pq_rx_checks[] = {
  {.addr = {MSG_LENKHILFE_3}, .bus = 0, .check_checksum = true,  .max_counter = 15U, .expected_timestep = 10000U},
  {.addr = {MSG_MOTOR_2},     .bus = 0, .check_checksum = false, .max_counter = 0U,  .expected_timestep = 20000U},
  {.addr = {MSG_MOTOR_3},     .bus = 0, .check_checksum = false, .max_counter = 0U,  .expected_timestep = 10000U},
  {.addr = {MSG_BREMSE_3},    .bus = 0, .check_checksum = false, .max_counter = 0U,  .expected_timestep = 10000U},
};
const int VOLKSWAGEN_PQ_RX_CHECKS_LEN = sizeof(volkswagen_pq_rx_checks) / sizeof(volkswagen_pq_rx_checks[0]);

struct sample_t volkswagen_torque_driver; // Last few driver torques measured
int volkswagen_rt_torque_last = 0;
int volkswagen_desired_torque_last = 0;
uint32_t volkswagen_ts_last = 0;
bool volkswagen_moving = false;
int volkswagen_torque_msg = 0;
int volkswagen_lane_msg = 0;
uint8_t volkswagen_crc8_lut_8h2f[256]; // Static lookup table for CRC8 poly 0x2F, aka 8H2F/AUTOSAR


static uint8_t volkswagen_get_checksum(CAN_FIFOMailBox_TypeDef *to_push) {
  return (uint8_t)GET_BYTE(to_push, 0);
}

static uint8_t volkswagen_mqb_get_counter(CAN_FIFOMailBox_TypeDef *to_push) {
  // MQB message counters are consistently found at LSB 8.
  return (uint8_t)GET_BYTE(to_push, 1) & 0xFU;
}

static uint8_t volkswagen_pq_get_counter(CAN_FIFOMailBox_TypeDef *to_push) {
  // Few PQ messages have counters, and their offsets are inconsistent. This
  // function works only for Lenkhilfe_3 at this time.
  return (uint8_t)(GET_BYTE(to_push, 1) & 0xF0U) >> 4;
}

static uint8_t volkswagen_mqb_compute_crc(CAN_FIFOMailBox_TypeDef *to_push) {
  int addr = GET_ADDR(to_push);
  int len = GET_LEN(to_push);

  // This is CRC-8H2F/AUTOSAR with a twist. See the OpenDBC implementation
  // of this algorithm for a version with explanatory comments.

  uint8_t crc = 0xFFU;
  for (int i = 1; i < len; i++) {
    crc ^= (uint8_t)GET_BYTE(to_push, i);
    crc = volkswagen_crc8_lut_8h2f[crc];
  }

  uint8_t counter = volkswagen_mqb_get_counter(to_push);
  switch(addr) {
    case MSG_EPS_01:
      crc ^= (uint8_t[]){0xF5,0xF5,0xF5,0xF5,0xF5,0xF5,0xF5,0xF5,0xF5,0xF5,0xF5,0xF5,0xF5,0xF5,0xF5,0xF5}[counter];
      break;
    case MSG_ESP_05:
      crc ^= (uint8_t[]){0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07}[counter];
      break;
    case MSG_TSK_06:
      crc ^= (uint8_t[]){0xC4,0xE2,0x4F,0xE4,0xF8,0x2F,0x56,0x81,0x9F,0xE5,0x83,0x44,0x05,0x3F,0x97,0xDF}[counter];
      break;
    case MSG_MOTOR_20:
      crc ^= (uint8_t[]){0xE9,0x65,0xAE,0x6B,0x7B,0x35,0xE5,0x5F,0x4E,0xC7,0x86,0xA2,0xBB,0xDD,0xEB,0xB4}[counter];
      break;
    default: // Undefined CAN message, CRC check expected to fail
      break;
  }
  crc = volkswagen_crc8_lut_8h2f[crc];

  return crc ^ 0xFFU;
}

static uint8_t volkswagen_pq_compute_checksum(CAN_FIFOMailBox_TypeDef *to_push) {
  int len = GET_LEN(to_push);
  uint8_t checksum = 0U;

  for (int i = 1; i < len; i++) {
    checksum ^= (uint8_t)GET_BYTE(to_push, i);
  }

  return checksum;
}

static void volkswagen_mqb_init(int16_t param) {
  UNUSED(param);

  controls_allowed = false;
  relay_malfunction = false;
  volkswagen_torque_msg = MSG_HCA_01;
  volkswagen_lane_msg = MSG_LDW_02;
  gen_crc_lookup_table(0x2F, volkswagen_crc8_lut_8h2f);
}

static void volkswagen_pq_init(int16_t param) {
  UNUSED(param);

  controls_allowed = false;
  relay_malfunction = false;
  volkswagen_torque_msg = MSG_HCA_1;
  volkswagen_lane_msg = MSG_LDW_1;

  return;
}

static int volkswagen_mqb_rx_hook(CAN_FIFOMailBox_TypeDef *to_push) {

  bool valid = addr_safety_check(to_push, volkswagen_mqb_rx_checks, VOLKSWAGEN_MQB_RX_CHECKS_LEN,
                                 volkswagen_get_checksum, volkswagen_mqb_compute_crc, volkswagen_mqb_get_counter);

  if (valid && (GET_BUS(to_push) == 0)) {
    int addr = GET_ADDR(to_push);

    // Update in-motion state by sampling front wheel speeds
    // Signal: ESP_19.ESP_VL_Radgeschw_02 (front left) in scaled km/h
    // Signal: ESP_19.ESP_VR_Radgeschw_02 (front right) in scaled km/h
    if (addr == MSG_ESP_19) {
      int wheel_speed_fl = GET_BYTE(to_push, 4) | (GET_BYTE(to_push, 5) << 8);
      int wheel_speed_fr = GET_BYTE(to_push, 6) | (GET_BYTE(to_push, 7) << 8);
      // Check for average front speed in excess of 0.3m/s, 1.08km/h
      // DBC speed scale 0.0075: 0.3m/s = 144, sum both wheels to compare
      volkswagen_moving = (wheel_speed_fl + wheel_speed_fr) > 288;
    }

    // Update driver input torque samples
    // Signal: EPS_01.Driver_Strain (absolute torque)
    // Signal: EPS_01.Driver_Strain_VZ (direction)
    if (addr == MSG_EPS_01) {
      int torque_driver_new = GET_BYTE(to_push, 5) | ((GET_BYTE(to_push, 6) & 0x1F) << 8);
      int sign = (GET_BYTE(to_push, 6) & 0x80) >> 7;
      if (sign == 1) {
        torque_driver_new *= -1;
      }
      update_sample(&volkswagen_torque_driver, torque_driver_new);
    }

    // Update ACC status from drivetrain coordinator for controls-allowed state
    // Signal: TSK_06.TSK_Status
    if (addr == MSG_TSK_06) {
      int acc_status = (GET_BYTE(to_push, 3) & 0x7);
      controls_allowed = ((acc_status == 3) || (acc_status == 4) || (acc_status == 5)) ? 1 : 0;
    }

    // Exit controls on rising edge of gas press
    // Signal: Motor_20.MO_Fahrpedalrohwert_01
    if (addr == MSG_MOTOR_20) {
      bool gas_pressed = ((GET_BYTES_04(to_push) >> 12) & 0xFF) != 0;
      if (gas_pressed && !gas_pressed_prev) {
        controls_allowed = 0;
      }
      gas_pressed_prev = gas_pressed;
    }

    // Exit controls on rising edge of brake press
    // Signal: ESP_05.ESP_Fahrer_bremst
    if (addr == MSG_ESP_05) {
      bool brake_pressed = (GET_BYTE(to_push, 3) & 0x4) >> 2;
      if (brake_pressed && (!brake_pressed_prev || volkswagen_moving)) {
        controls_allowed = 0;
      }
      brake_pressed_prev = brake_pressed;
    }

    // If there are HCA messages on bus 0 not sent by OP, there's a relay problem
    if ((safety_mode_cnt > RELAY_TRNS_TIMEOUT) && (addr == MSG_HCA_01)) {
      relay_malfunction = true;
    }
  }
  return valid;
}

static int volkswagen_pq_rx_hook(CAN_FIFOMailBox_TypeDef *to_push) {

  bool valid = addr_safety_check(to_push, volkswagen_pq_rx_checks, VOLKSWAGEN_PQ_RX_CHECKS_LEN,
                                volkswagen_get_checksum, volkswagen_pq_compute_checksum, volkswagen_pq_get_counter);

  teslaradar_rx_hook(to_push);

  if (valid) {
    int bus = GET_BUS(to_push);
    int addr = GET_ADDR(to_push);

    // Update in-motion state by sampling front wheel speeds
    // Signal: Bremse_3.Radgeschw__VL_4_1 (front left)
    // Signal: Bremse_3.Radgeschw__VR_4_1 (front right)
    if ((bus == 0) && (addr == MSG_BREMSE_3)) {
      int wheel_speed_fl = (GET_BYTE(to_push, 0) | (GET_BYTE(to_push, 1) << 8)) >> 1;
      int wheel_speed_fr = (GET_BYTE(to_push, 2) | (GET_BYTE(to_push, 3) << 8)) >> 1;
      // Check for average front speed in excess of 0.3m/s, 1.08km/h
      // DBC speed scale 0.01: 0.3m/s = 108, sum both wheels to compare
      volkswagen_moving = (wheel_speed_fl + wheel_speed_fr) > 216;
      // Current KP/h for tesla radar
      actual_speed_kph = (int)((wheel_speed_fl + wheel_speed_fr) * 0.005);
    }

    // Update driver input torque samples
    // Signal: Lenkhilfe_3.LH3_LM (absolute torque)
    // Signal: Lenkhilfe_3.LH3_LMSign (direction)
    if ((bus == 0) && (addr == MSG_LENKHILFE_3)) {
      int torque_driver_new = GET_BYTE(to_push, 2) | ((GET_BYTE(to_push, 3) & 0x3) << 8);
      int sign = (GET_BYTE(to_push, 3) & 0x4) >> 2;
      if (sign == 1) {
        torque_driver_new *= -1;
      }
      update_sample(&volkswagen_torque_driver, torque_driver_new);
    }

    // Exit controls on rising edge of interceptor gas press
    if ((bus == 2) && (addr == MSG_GAS_SENSOR)) {
      gas_interceptor_detected = 1;
      controls_allowed = 1;
      int gas_interceptor = GET_INTERCEPTOR(to_push);
      if ((gas_interceptor > VOLKSWAGEN_GAS_INTERCEPTOR_THRSLD) &&
          (gas_interceptor_prev <= VOLKSWAGEN_GAS_INTERCEPTOR_THRSLD)) {
        controls_allowed = 0;
      }
      gas_interceptor_prev = gas_interceptor;
    }

    // Update ACC status from ECU for controls-allowed state
    // Signal: Motor_2.GRA_Status
    if ((bus == 0) && (addr == MSG_MOTOR_2) && !gas_interceptor_detected) {
      int acc_status = (GET_BYTE(to_push, 2) & 0xC0) >> 6;
      controls_allowed = ((acc_status == 1) || (acc_status == 2)) ? 1 : 0;
    }

    // Exit controls on rising edge of gas press
    // Signal: Motor_3.Fahrpedal_Rohsignal
    if ((bus == 0) && (addr == MSG_MOTOR_3)) {
      int gas_pressed = (GET_BYTE(to_push, 2));
      if ((gas_pressed > 0) && (gas_pressed_prev == 0) && !gas_interceptor_detected) {
        controls_allowed = 0;
      }
      gas_pressed_prev = gas_pressed;
    }

    // Exit controls on rising edge of brake press
    // Signal: Motor_2.Bremslichtschalter
    if ((bus == 0) && (addr == MSG_MOTOR_2)) {
      bool brake_pressed = (GET_BYTE(to_push, 2) & 0x1);
      if (brake_pressed && (!(brake_pressed_prev) || volkswagen_moving)) {
        controls_allowed = 0;
      }
      brake_pressed_prev = brake_pressed;
    }

    // If there are HCA messages on bus 0 not sent by OP, there's a relay problem
    if ((safety_mode_cnt > RELAY_TRNS_TIMEOUT) && (bus == 0) && (addr == MSG_HCA_1)) {
      relay_malfunction = true;
    }
  }
  return valid;
}

static bool volkswagen_steering_check(int desired_torque) {
  bool violation = false;
  uint32_t ts = TIM2->CNT;

  if (controls_allowed) {
    // *** global torque limit check ***
    violation |= max_limit_check(desired_torque, VOLKSWAGEN_MAX_STEER, -VOLKSWAGEN_MAX_STEER);

    // *** torque rate limit check ***
    violation |= driver_limit_check(desired_torque, volkswagen_desired_torque_last, &volkswagen_torque_driver,
      VOLKSWAGEN_MAX_STEER, VOLKSWAGEN_MAX_RATE_UP, VOLKSWAGEN_MAX_RATE_DOWN,
      VOLKSWAGEN_DRIVER_TORQUE_ALLOWANCE, VOLKSWAGEN_DRIVER_TORQUE_FACTOR);
    volkswagen_desired_torque_last = desired_torque;

    // *** torque real time rate limit check ***
    violation |= rt_rate_limit_check(desired_torque, volkswagen_rt_torque_last, VOLKSWAGEN_MAX_RT_DELTA);

    // every RT_INTERVAL set the new limits
    uint32_t ts_elapsed = get_ts_elapsed(ts, volkswagen_ts_last);
    if (ts_elapsed > VOLKSWAGEN_RT_INTERVAL) {
      volkswagen_rt_torque_last = desired_torque;
      volkswagen_ts_last = ts;
    }
  }

  // no torque if controls is not allowed
  if (!controls_allowed && (desired_torque != 0)) {
    violation = true;
  }

  // reset to 0 if either controls is not allowed or there's a violation
  if (violation || !controls_allowed) {
    volkswagen_desired_torque_last = 0;
    volkswagen_rt_torque_last = 0;
    volkswagen_ts_last = ts;
  }

  return violation;
}

static int volkswagen_mqb_tx_hook(CAN_FIFOMailBox_TypeDef *to_send) {
  int addr = GET_ADDR(to_send);
  int bus = GET_BUS(to_send);
  int tx = 1;

  if (!msg_allowed(addr, bus, VOLKSWAGEN_MQB_TX_MSGS, VOLKSWAGEN_MQB_TX_MSGS_LEN) || relay_malfunction) {
    tx = 0;
  }

  // Safety check for HCA_01 Heading Control Assist torque
  // Signal: HCA_01.Assist_Torque (absolute torque)
  // Signal: HCA_01.Assist_VZ (direction)
  if (addr == MSG_HCA_01) {
    int desired_torque = GET_BYTE(to_send, 2) | ((GET_BYTE(to_send, 3) & 0x3F) << 8);
    int sign = (GET_BYTE(to_send, 3) & 0x80) >> 7;
    if (sign == 1) {
      desired_torque *= -1;
    }

    if (volkswagen_steering_check(desired_torque)) {
      tx = 0;
    }
  }

  // FORCE CANCEL: ensuring that only the cancel button press is sent when controls are off.
  // This avoids unintended engagements while still allowing resume spam
  if ((addr == MSG_GRA_ACC_01) && !controls_allowed) {
    // disallow resume and set: bits 16 and 19
    if ((GET_BYTE(to_send, 2) & 0x9) != 0) {
      tx = 0;
    }
  }

  // 1 allows the message through
  return tx;
}

static int volkswagen_pq_tx_hook(CAN_FIFOMailBox_TypeDef *to_send) {
  int addr = GET_ADDR(to_send);
  int bus = GET_BUS(to_send);
  int tx = 1;

  if (!msg_allowed(addr, bus, VOLKSWAGEN_PQ_TX_MSGS, VOLKSWAGEN_PQ_TX_MSGS_LEN) || relay_malfunction) {
    tx = 0;
  }

  // Safety check for HCA_1 Heading Control Assist torque
  // Signal: HCA_1.LM_Offset (absolute torque)
  // Signal: HCA_1.LM_Offsign (direction)
  if (addr == MSG_HCA_1) {
    int desired_torque = GET_BYTE(to_send, 2) | ((GET_BYTE(to_send, 3) & 0x7F) << 8);
    desired_torque = desired_torque >> 5;  // DBC scale from PQ network to centi-Nm
    int sign = (GET_BYTE(to_send, 3) & 0x80) >> 7;
    if (sign == 1) {
      desired_torque *= -1;
    }

    if (volkswagen_steering_check(desired_torque)) {
      tx = 0;
    }
  }

  // Tesla Radar TX hook
   //check if this is a teslaradar vin message
 //capture message for radarVIN and settings
 if (addr == MSG_TESLA_VIN) {
   int id = (to_send->RDLR & 0xFF);
   int radarVin_b1 = ((to_send->RDLR >> 8) & 0xFF);
   int radarVin_b2 = ((to_send->RDLR >> 16) & 0xFF);
   int radarVin_b3 = ((to_send->RDLR >> 24) & 0xFF);
   int radarVin_b4 = (to_send->RDHR & 0xFF);
   int radarVin_b5 = ((to_send->RDHR >> 8) & 0xFF);
   int radarVin_b6 = ((to_send->RDHR >> 16) & 0xFF);
   int radarVin_b7 = ((to_send->RDHR >> 24) & 0xFF);
   if (id == 0) {
     tesla_radar_should_send = (radarVin_b2 & 0x01);
     radarPosition =  ((radarVin_b2 >> 1) & 0x03);
     radarEpasType = ((radarVin_b2 >> 3) & 0x07);
     tesla_radar_trigger_message_id = (radarVin_b3 << 8) + radarVin_b4;
     tesla_radar_can = radarVin_b1;
     radar_VIN[0] = radarVin_b5;
     radar_VIN[1] = radarVin_b6;
     radar_VIN[2] = radarVin_b7;
     tesla_radar_vin_complete = tesla_radar_vin_complete | 1;
   }
   if (id == 1) {
     radar_VIN[3] = radarVin_b1;
     radar_VIN[4] = radarVin_b2;
     radar_VIN[5] = radarVin_b3;
     radar_VIN[6] = radarVin_b4;
     radar_VIN[7] = radarVin_b5;
     radar_VIN[8] = radarVin_b6;
     radar_VIN[9] = radarVin_b7;
     tesla_radar_vin_complete = tesla_radar_vin_complete | 2;
   }
   if (id == 2) {
     radar_VIN[10] = radarVin_b1;
     radar_VIN[11] = radarVin_b2;
     radar_VIN[12] = radarVin_b3;
     radar_VIN[13] = radarVin_b4;
     radar_VIN[14] = radarVin_b5;
     radar_VIN[15] = radarVin_b6;
     radar_VIN[16] = radarVin_b7;
     tesla_radar_vin_complete = tesla_radar_vin_complete | 4;
   }
   else {
     return 0;
   }
 }

  // GAS PEDAL: safety check
  if (addr == MSG_GAS_COMMAND) {
      if (!controls_allowed) {
        if (GET_BYTE(to_send, 0) || GET_BYTE(to_send, 1)) {
          tx = 0;
        }
      }
    }

  // 1 allows the message through
  return tx;
}

static int volkswagen_fwd_hook(int bus_num, CAN_FIFOMailBox_TypeDef *to_fwd) {
  int addr = GET_ADDR(to_fwd);
  int bus_fwd = -1;

  if (!relay_malfunction) {
    switch (bus_num) {
      case 0:
        // Forward all traffic from the Extended CAN onward
        bus_fwd = 2;
        break;
      case 2:
        if ((addr == volkswagen_torque_msg) || (addr == volkswagen_lane_msg)) {
          // OP takes control of the Heading Control Assist and Lane Departure Warning messages from the camera
          bus_fwd = -1;
        } else {
          // Forward all remaining traffic from Extended CAN devices to J533 gateway
          bus_fwd = 0;
        }
        break;
      default:
        // No other buses should be in use; fallback to do-not-forward
        bus_fwd = -1;
        break;
    }
  }
  return bus_fwd;
}

// Volkswagen MQB platform
const safety_hooks volkswagen_mqb_hooks = {
  .init = volkswagen_mqb_init,
  .rx = volkswagen_mqb_rx_hook,
  .tx = volkswagen_mqb_tx_hook,
  .tx_lin = nooutput_tx_lin_hook,
  .fwd = volkswagen_fwd_hook,
  .addr_check = volkswagen_mqb_rx_checks,
  .addr_check_len = sizeof(volkswagen_mqb_rx_checks) / sizeof(volkswagen_mqb_rx_checks[0]),
};

// Volkswagen PQ35/PQ46/NMS platforms
const safety_hooks volkswagen_pq_hooks = {
  .init = volkswagen_pq_init,
  .rx = volkswagen_pq_rx_hook,
  .tx = volkswagen_pq_tx_hook,
  .tx_lin = nooutput_tx_lin_hook,
  .fwd = volkswagen_fwd_hook,
  .addr_check = volkswagen_pq_rx_checks,
  .addr_check_len = sizeof(volkswagen_pq_rx_checks) / sizeof(volkswagen_pq_rx_checks[0]),
};
