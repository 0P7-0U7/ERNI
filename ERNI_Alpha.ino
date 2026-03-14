/*
 * ERNI: ESP-NOW Reticulum Network Interface Engine
 * Created for low-latency, high-bandwidth mesh communication.
 * ---------------------------------------------------------
 * Acts as a transparent bridge between the Reticulum Network Stack (RNS)
 * and an ESP-NOW decentralized mesh.
 */

#include "../../src/ERNI_Framing.h"
#include "../../src/ERNI_Mesh.h"

// --- ERNI Configuration ---
#define BAUD_RATE 921600
#define FRAGMENT_DELAY_US 500

uint8_t broadcastAddr[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Storage allocations for ERNI_Mesh namespace
namespace ERNI_Mesh {
uint32_t seen_cache[CACHE_SIZE] = {0};
int cache_idx = 0;
uint8_t reassembly_buf[MAX_RNS_MTU];
int current_reassembly_len = 0;
uint8_t last_packet_id = 0;
uint8_t last_sender[6] = {0};
volatile int q_head = 0;
volatile int q_tail = 0;
QueuedPacket rebroadcast_queue[REBROADCAST_QUEUE_SIZE];
} // namespace ERNI_Mesh

void setup() {
  Serial.begin(BAUD_RATE);

  // ERNI requires Station Mode for active ESP-NOW transmission
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (esp_now_init() != ESP_OK) {
    Serial.println("ERNI FATAL: ESP-NOW INIT FAILED");
    return;
  }

  esp_now_register_recv_cb(ERNI_Mesh::onDataRecv);

  // Register global broadcast peer for flooding
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, broadcastAddr, 6);
  peer.channel = 0;
  peer.encrypt = false;
  esp_now_add_peer(&peer);
}

void loop() {
  static uint8_t serial_buf[MAX_RNS_MTU];
  static int s_idx = 0;
  static bool esc = false;
  static bool kiss_cmd_skip = false;
  static uint8_t p_id_counter = 0;

  // Drain any queued rebroadcast packets from mesh callbacks
  ERNI_Mesh::process_rebroadcast_queue();

  // Rapidly handle incoming data from the Reticulum Network Stack (Computer)
  while (Serial.available()) {
    uint8_t b = Serial.read();

    if (b == FEND) {
      // End of KISS frame detected - Ship it to the mesh
      if (s_idx > 0) {
        p_id_counter++;

        // Fragment large RNS packets (up to 500 MTU) for ESP-NOW (250 limit)
        for (int i = 0; i < s_idx; i += ESP_NOW_PAYLOAD) {
          int chunk = min(s_idx - i, ESP_NOW_PAYLOAD);
          uint8_t f_buf[250];

          f_buf[0] = p_id_counter; // Unique ID tying chunks together
          f_buf[1] = (i + chunk >= s_idx)
                         ? 0xFF
                         : (i / ESP_NOW_PAYLOAD); // 0xFF marks EOF

          memcpy(&f_buf[2], &serial_buf[i], chunk);
          esp_now_send(broadcastAddr, f_buf, chunk + 2);

          // Brief pacing between fragments for receiver stability
          if (i + chunk < s_idx) {
            delayMicroseconds(FRAGMENT_DELAY_US);
          }
        }
        s_idx = 0; // Reset buffer for next packet
      }
      // Next byte after FEND is the KISS command byte — skip it
      kiss_cmd_skip = true;
    } else if (kiss_cmd_skip) {
      // Consume the KISS command byte (0x00 = Data) without buffering
      kiss_cmd_skip = false;
    } else if (b == FESC) {
      esc = true;
    } else {
      // Unescape bytes if necessary before buffering
      if (esc) {
        b = (b == TFEND) ? FEND : FESC;
        esc = false;
      }
      if (s_idx < MAX_RNS_MTU) {
        serial_buf[s_idx++] = b;
      }
    }
  }

  // Yield to the underlying RTOS processes
  delay(1);
}
