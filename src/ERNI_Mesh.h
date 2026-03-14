#ifndef ERNI_MESH_H
#define ERNI_MESH_H

#include "ERNI_Framing.h"
#include <WiFi.h>
#include <esp_now.h>
#include <string.h>

#define MAX_RNS_MTU 500
#define ESP_NOW_PAYLOAD 248 // 250 - 2 byte ERNI header
#define CACHE_SIZE 50
#define REBROADCAST_QUEUE_SIZE 8

extern uint8_t broadcastAddr[6];

namespace ERNI_Mesh {

// --- Deduplication: Circular Buffer ---
extern uint32_t seen_cache[CACHE_SIZE];
extern int cache_idx;

// --- Reassembly State ---
extern uint8_t reassembly_buf[MAX_RNS_MTU];
extern int current_reassembly_len;
extern uint8_t last_packet_id;
extern uint8_t last_sender[6];

// --- Rebroadcast Queue (callback-safe) ---
struct QueuedPacket {
  uint8_t data[250];
  int len;
};
extern volatile int q_head;
extern volatile int q_tail;
extern QueuedPacket rebroadcast_queue[REBROADCAST_QUEUE_SIZE];

/**
 * FNV-1a 32-bit hash for mesh deduplication.
 * Far fewer collisions than a simple polynomial hash.
 */
uint32_t hash_packet(const uint8_t *data, int len) {
  uint32_t hash = 2166136261u;
  for (int i = 0; i < len; i++) {
    hash ^= data[i];
    hash *= 16777619u;
  }
  return hash;
}

/**
 * Check if a hash already exists in the circular dedup cache.
 * Returns true if the packet has been seen before.
 */
bool is_duplicate(uint32_t hash) {
  for (int i = 0; i < CACHE_SIZE; i++) {
    if (seen_cache[i] == hash)
      return true;
  }
  return false;
}

/**
 * Record a hash into the circular dedup cache. O(1) insertion.
 */
void cache_hash(uint32_t hash) {
  seen_cache[cache_idx % CACHE_SIZE] = hash;
  cache_idx++;
}

/**
 * Push a packet into the rebroadcast queue (called from ISR/callback context).
 * Drops the packet silently if the queue is full.
 */
void queue_rebroadcast(const uint8_t *data, int len) {
  int next_head = (q_head + 1) % REBROADCAST_QUEUE_SIZE;
  if (next_head == q_tail)
    return; // Queue full, drop
  memcpy(rebroadcast_queue[q_head].data, data, len);
  rebroadcast_queue[q_head].len = len;
  q_head = next_head;
}

/**
 * Drain the rebroadcast queue from the main loop() context.
 * Safe to call esp_now_send here since we're in the Arduino task.
 */
void process_rebroadcast_queue() {
  while (q_tail != q_head) {
    esp_now_send(broadcastAddr, rebroadcast_queue[q_tail].data,
                 rebroadcast_queue[q_tail].len);
    q_tail = (q_tail + 1) % REBROADCAST_QUEUE_SIZE;
  }
}

/**
 * Handles inbound ESP-NOW packets from the mesh.
 * Executes Managed Flooding (Repeating) and Fragment Reassembly.
 *
 * Called from ESP-NOW internal task — only queues rebroadcasts,
 * does not call esp_now_send directly.
 */
void onDataRecv(const esp_now_recv_info *info, const uint8_t *data, int len) {
  if (len < 2)
    return; // Invalid ERNI payload

  // 1. Managed Flooding Deduplication (FNV-1a + Circular Buffer)
  uint32_t packet_hash = hash_packet(data, len);

  if (is_duplicate(packet_hash))
    return; // Already repeated this packet

  cache_hash(packet_hash);

  // Queue for rebroadcast (processed safely in loop())
  queue_rebroadcast(data, len);

  // 2. Reassembly Logic for Reticulum
  uint8_t p_id = data[0];
  uint8_t seq = data[1];

  // Multi-sender safety: reset if packet ID or sender changed
  if (p_id != last_packet_id || memcmp(info->src_addr, last_sender, 6) != 0) {
    current_reassembly_len = 0;
    last_packet_id = p_id;
    memcpy(last_sender, info->src_addr, 6);
  }

  // Prevent buffer overflow on corrupted multi-part transmissions
  if (current_reassembly_len + (len - 2) > MAX_RNS_MTU) {
    current_reassembly_len = 0;
    return;
  }

  memcpy(&reassembly_buf[current_reassembly_len], &data[2], len - 2);
  current_reassembly_len += (len - 2);

  // 0xFF Sequence marks the final chunk of the MTU
  if (seq == 0xFF) {
    send_to_serial_kiss(reassembly_buf, current_reassembly_len);
    current_reassembly_len = 0;
  }
}
} // namespace ERNI_Mesh

#endif // ERNI_MESH_H
