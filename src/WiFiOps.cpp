#include "WiFiOps.h"
#include "BatteryInterface.h"
#include "esp_task_wdt.h"

extern BatteryInterface battery;
extern bool g_force_display_redraw;

static const uint8_t BROADCAST_MAC[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
static const char MAGIC[4] = {'E','N','O','W'};

static constexpr uint8_t ESPNOW_CHANNEL = 6;

// Retry behavior for nodes
static constexpr uint32_t REQ_INITIAL_MS = 300;   // first retry delay
static constexpr uint32_t REQ_MAX_MS     = 5000;  // cap retry interval

static uint8_t g_core_mac[6] = {0};
static bool g_have_core = false;
static bool g_secure_ready = false;

static uint32_t g_hb_counter = 0;
static unsigned long g_last_hb_ms = 0;

// Retry state
static unsigned long g_last_req_ms = 0;
static unsigned long g_last_debug_print = 0;
static uint32_t g_req_interval_ms = REQ_INITIAL_MS;

static inline uint16_t rd_le16(const uint8_t *p) {
  return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static inline uint32_t rd_be24(const uint8_t *p) {
  return ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | (uint32_t)p[2];
}

static inline bool oui_type_match(const uint8_t *p, uint8_t b0, uint8_t b1, uint8_t b2, uint8_t type) {
  return p[0] == b0 && p[1] == b1 && p[2] == b2 && p[3] == type;
}

static inline bool rsn_suite_is(const uint8_t *suite, uint8_t type) {
  return suite[0] == 0x00 && suite[1] == 0x0f && suite[2] == 0xac && suite[3] == type;
}

static inline bool ms_wpa_suite_is(const uint8_t *suite, uint8_t type) {
  return suite[0] == 0x00 && suite[1] == 0x50 && suite[2] == 0xf2 && suite[3] == type;
}

static const uint8_t scan_channels[] = {
  // 2.4 GHz
  1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,

  // 5 GHz
  36, 40, 44, 48,
  52, 56, 60, 64,
  100, 112, 116, 120, 124, 128, 132, 136, 140, 144,
  149, 153, 157, 161, 165, 169, 173, 177
};

#define NUM_SCAN_CHANNELS (sizeof(scan_channels) / sizeof(scan_channels[0]))

uint8_t assigned_start_idx = 0;
uint8_t assigned_end_idx = NUM_SCAN_CHANNELS - 1;
uint8_t assigned_node_index = 0;
uint8_t assigned_node_count = 1;
uint8_t assignment_version = 0;

uint8_t pmk[16];
uint8_t lmk[16];

NodeRecord node_table[MAX_NODES];

enum MsgType : uint8_t {
  MSG_CORE_REQUEST   = 1,
  MSG_CORE_REPLY     = 2,
  MSG_HEARTBEAT      = 3,
  MSG_TEXT           = 4,
  MSG_ADMIN          = 5
};

WebServer server(80);


class scanCallbacks : public NimBLEScanCallbacks {

  void onDiscovered(const NimBLEAdvertisedDevice* advertisedDevice) override {
    extern WiFiOps wifi_ops;

    uint8_t macBytes[6];

    if (wifi_ops.run_mode == SOLO_MODE) {
      if ((gps.getGpsModuleStatus()) && (gps.getFixStatus()) && (sd_obj.supported)) {
        
        utils.stringToMac(advertisedDevice->getAddress().toString().c_str(), macBytes);

        if (wifi_ops.seen_mac(macBytes))
          return;

        wifi_ops.save_mac(macBytes);

        wifi_ops.setCurrentBLECount(wifi_ops.getCurrentBLECount() + 1);

        wifi_ops.setTotalBLECount(wifi_ops.getTotalBLECount() + 1);

        bool do_save = false;

        if (gps.getFixStatus())
          do_save = true;

        String wardrive_line = (String)advertisedDevice->getAddress().toString().c_str() + ",,[BLE]," + gps.getDatetime() + ",0," + (String)advertisedDevice->getRSSI() + "," + gps.getLat() + "," + gps.getLon() + "," + gps.getAlt() + "," + gps.getAccuracy() + ",BLE";
        Logger::log(GUD_MSG, (String)wifi_ops.mac_history_cursor + " | " + wardrive_line);

        if (do_save)
          buffer.append(wardrive_line + "\n");
      }
    }
    else if (wifi_ops.run_mode == NODE_MODE) {
      utils.stringToMac(advertisedDevice->getAddress().toString().c_str(), macBytes);

      if (wifi_ops.seen_mac(macBytes))
        return;

      wifi_ops.save_mac(macBytes);

      wifi_ops.setCurrentBLECount(wifi_ops.getCurrentBLECount() + 1);

      wifi_ops.setTotalBLECount(wifi_ops.getTotalBLECount() + 1);

      String enow_line = (String)advertisedDevice->getAddress().toString().c_str() + ",,[BLE],0," + (String)(String)advertisedDevice->getRSSI() + ",B";
      Logger::log(GUD_MSG, (String)wifi_ops.mac_history_cursor + " | " + enow_line);
      if (wifi_ops.use_encryption)
        wifi_ops.sendEncryptedStringToCore(enow_line);
      else
        wifi_ops.sendBroadcastStringPlain(enow_line);
    }
  }
};

int WiFiOps::getAuthType(const wifi_promiscuous_pkt_t *ppkt) {
  if (!ppkt) {
    return WIFI_AUTH_OPEN;
  }

  const uint8_t *frame = ppkt->payload;
  const uint16_t len = ppkt->rx_ctrl.sig_len;

  if (!frame || len < 36) {
    return WIFI_AUTH_OPEN;
  }

  const uint8_t fc0 = frame[0];
  const uint8_t type = (fc0 >> 2) & 0x03;
  const uint8_t subtype = (fc0 >> 4) & 0x0F;

  if (type != 0) {
    return WIFI_AUTH_OPEN;
  }

  if (!(subtype == 8 || subtype == 5)) {
    return WIFI_AUTH_OPEN;
  }

  const size_t hdr_len = 24;
  const size_t fixed_len = 12;

  if (len < hdr_len + fixed_len) {
    return WIFI_AUTH_OPEN;
  }

  const uint8_t *fixed = frame + hdr_len;
  const uint16_t capability = rd_le16(fixed + 10);

  const bool privacy = (capability & 0x0010) != 0;

  const uint8_t *ies = frame + hdr_len + fixed_len;
  size_t ies_len = len - hdr_len - fixed_len;

  bool has_rsn = false;
  bool has_wpa = false;
  bool has_wapi = false;

  bool rsn_has_psk = false;
  bool rsn_has_8021x = false;
  bool rsn_has_sae = false;
  bool rsn_has_owe = false;

  bool wpa_has_psk = false;
  bool wpa_has_8021x = false;

  while (ies_len >= 2) {
    const uint8_t id = ies[0];
    const uint8_t elen = ies[1];

    if (ies_len < (size_t)(2 + elen)) {
      break;
    }

    const uint8_t *data = ies + 2;

    if (id == 48 && elen >= 8) {
      has_rsn = true;

      const uint8_t *p = data;
      size_t rem = elen;

      if (rem < 2) {
        goto next_ie;
      }
      p += 2;
      rem -= 2;

      if (rem < 4) {
        goto next_ie;
      }
      p += 4;
      rem -= 4;

      if (rem < 2) {
        goto next_ie;
      }
      uint16_t pairwise_count = rd_le16(p);
      p += 2;
      rem -= 2;

      if (rem < (size_t)pairwise_count * 4) {
        goto next_ie;
      }
      p += pairwise_count * 4;
      rem -= pairwise_count * 4;

      if (rem < 2) {
        goto next_ie;
      }
      uint16_t akm_count = rd_le16(p);
      p += 2;
      rem -= 2;

      if (rem < (size_t)akm_count * 4) {
        goto next_ie;
      }

      for (uint16_t i = 0; i < akm_count; ++i) {
        const uint8_t *akm = p + (i * 4);

        if (rsn_suite_is(akm, 1) || rsn_suite_is(akm, 5)) {
          rsn_has_8021x = true;
        } else if (rsn_suite_is(akm, 2) || rsn_suite_is(akm, 6)) {
          rsn_has_psk = true;
        } else if (rsn_suite_is(akm, 8) || rsn_suite_is(akm, 9)) {
          rsn_has_sae = true;
        } else if (rsn_suite_is(akm, 18)) {
          rsn_has_owe = true;
        }
      }

      goto next_ie;
    }

    if (id == 221 && elen >= 8) {
      if (oui_type_match(data, 0x00, 0x50, 0xf2, 0x01)) {
        has_wpa = true;

        const uint8_t *p = data + 4;
        size_t rem = elen - 4;

        if (rem < 2) {
          goto next_ie;
        }
        p += 2;
        rem -= 2;

        if (rem < 4) {
          goto next_ie;
        }
        p += 4;
        rem -= 4;

        if (rem < 2) {
          goto next_ie;
        }
        uint16_t ucount = rd_le16(p);
        p += 2;
        rem -= 2;

        if (rem < (size_t)ucount * 4) {
          goto next_ie;
        }
        p += ucount * 4;
        rem -= ucount * 4;

        if (rem < 2) {
          goto next_ie;
        }
        uint16_t akm_count = rd_le16(p);
        p += 2;
        rem -= 2;

        if (rem < (size_t)akm_count * 4) {
          goto next_ie;
        }

        for (uint16_t i = 0; i < akm_count; ++i) {
          const uint8_t *akm = p + (i * 4);

          if (ms_wpa_suite_is(akm, 1)) {
            wpa_has_8021x = true;
          } else if (ms_wpa_suite_is(akm, 2)) {
            wpa_has_psk = true;
          }
        }

        goto next_ie;
      }
    }

    if (id == 68) {
      has_wapi = true;
      goto next_ie;
    }

next_ie:
    ies += (2 + elen);
    ies_len -= (2 + elen);
  }

  // Classification

  #ifdef WIFI_AUTH_WAPI_PSK
  if (has_wapi) {
    return WIFI_AUTH_WAPI_PSK;
  }
  #endif

  #ifdef WIFI_AUTH_OWE
  if (has_rsn && rsn_has_owe) {
    return WIFI_AUTH_OWE;
  }
  #endif

  #ifdef WIFI_AUTH_WPA3_PSK
  if (has_rsn && rsn_has_sae && !rsn_has_psk) {
    return WIFI_AUTH_WPA3_PSK;
  }
  #endif

  #ifdef WIFI_AUTH_WPA2_WPA3_PSK
  if (has_rsn && rsn_has_sae && rsn_has_psk) {
    return WIFI_AUTH_WPA2_WPA3_PSK;
  }
  #endif

  if (has_rsn && rsn_has_8021x) {
    return WIFI_AUTH_WPA2_ENTERPRISE;
  }

  #ifdef WIFI_AUTH_ENTERPRISE
  if (has_wpa && wpa_has_8021x && !has_rsn) {
    return WIFI_AUTH_ENTERPRISE;
  }
  #else
  if (has_wpa && wpa_has_8021x && !has_rsn) {
    return WIFI_AUTH_WPA2_ENTERPRISE;
  }
  #endif

  if ((has_wpa && wpa_has_psk) && (has_rsn && rsn_has_psk)) {
    return WIFI_AUTH_WPA_WPA2_PSK;
  }

  if (has_rsn && rsn_has_psk) {
    return WIFI_AUTH_WPA2_PSK;
  }

  if (has_wpa && wpa_has_psk) {
    return WIFI_AUTH_WPA_PSK;
  }

  // WEP heuristic:
  // privacy bit set, but no WPA/RSN/WAPI IEs
  if (privacy && !has_rsn && !has_wpa && !has_wapi) {
    return WIFI_AUTH_WEP;
  }

  if (!privacy && !has_rsn && !has_wpa && !has_wapi) {
    return WIFI_AUTH_OPEN;
  }

  if (privacy) {
    return WIFI_AUTH_WEP;
  }

  return WIFI_AUTH_OPEN;
}

uint16_t WiFiOps::macToSuffix(const uint8_t* mac) {
  return ((uint16_t)mac[4] << 8) | mac[5];
}

void WiFiOps::macSuffixToStr(uint16_t suffix, char* out6) {
  sprintf(out6, "%02X:%02X", (suffix >> 8) & 0xFF, suffix & 0xFF);
}

int WiFiOps::findNodeByMacSuffix(uint16_t suffix) {
  for (int i = 0; i < MAX_NODES; i++) {
    if ((node_table[i].flags & NODE_FLAG_ACTIVE) &&
        node_table[i].mac_suffix == suffix) {
      return i;
    }
  }
  return -1;
}

int WiFiOps::findNodeByMac(const uint8_t* mac) {
  return this->findNodeByMacSuffix(this->macToSuffix(mac));
}

int WiFiOps::allocateNodeSlot(const uint8_t* mac) {
  uint16_t suffix = macToSuffix(mac);

  for (int i = 0; i < MAX_NODES; i++) {
    if (!(node_table[i].flags & NODE_FLAG_ACTIVE)) {
      node_table[i].mac_suffix = suffix;
      node_table[i].last_seen_ms = millis();
      node_table[i].assigned_index = 0;
      node_table[i].start_channel_idx = 0;
      node_table[i].end_channel_idx = NUM_SCAN_CHANNELS - 1;
      node_table[i].last_admin_version_sent = 0; // force update
      node_table[i].flags = NODE_FLAG_ACTIVE | NODE_FLAG_ADMIN_DIRTY;
      return i;
    }
  }

  return -1;
}

int WiFiOps::touchNode(const uint8_t* mac, bool& isNewNode) {
  isNewNode = false;
  uint16_t suffix = macToSuffix(mac);

  int slot = findNodeByMacSuffix(suffix);
  if (slot >= 0) {
    node_table[slot].last_seen_ms = millis();
    return slot;
  }

  slot = allocateNodeSlot(mac);
  if (slot >= 0) {
    node_table[slot].last_seen_ms = millis();
    isNewNode = true;
    char mac_str[] = "00:00";
    this->macSuffixToStr(suffix, mac_str);
    Serial.print("Node added: ");
    Serial.print(mac_str);
    Serial.println(" | Node count updated: " + (String)this->getActiveNodeCount());
  }
  return slot;
}

bool WiFiOps::removeStaleNodes() {
  bool changed = false;
  uint32_t now = millis();

  for (int i = 0; i < MAX_NODES; i++) {
    if (node_table[i].flags & NODE_FLAG_ACTIVE) {
      if ((uint32_t)(now - node_table[i].last_seen_ms) > NODE_TIMEOUT_MS) {
        memset(&node_table[i], 0, sizeof(NodeRecord));
        changed = true;
      }
    }
  }

  return changed;
}

void WiFiOps::recalculateChannelAssignments() {
  uint8_t active_slots[MAX_NODES];
  uint8_t active_count = 0;

  for (uint8_t i = 0; i < MAX_NODES; i++) {
    if (node_table[i].flags & NODE_FLAG_ACTIVE) {
      active_slots[active_count++] = i;
    }
  }

  if (active_count == 0) {
    return;
  }

  for (uint8_t node_num = 0; node_num < active_count; node_num++) {
    uint8_t slot = active_slots[node_num];

    uint8_t start_idx = (node_num * NUM_SCAN_CHANNELS) / active_count;
    uint8_t end_idx   = (((node_num + 1) * NUM_SCAN_CHANNELS) / active_count) - 1;

    node_table[slot].assigned_index = node_num;
    node_table[slot].start_channel_idx = start_idx;
    node_table[slot].end_channel_idx = end_idx;
    node_table[slot].flags |= NODE_FLAG_ADMIN_DIRTY;
  }
}

uint8_t WiFiOps::getNodeStartChannel(uint8_t slot) {
  return scan_channels[node_table[slot].start_channel_idx];
}

uint8_t WiFiOps::getNodeEndChannel(uint8_t slot) {
  return scan_channels[node_table[slot].end_channel_idx];
}

uint8_t WiFiOps::getActiveNodeCount() {
  uint8_t count = 0;

  for (uint8_t i = 0; i < MAX_NODES; i++) {
    if (node_table[i].flags & NODE_FLAG_ACTIVE) {
      count++;
    }
  }

  return count;
}

void WiFiOps::markAllActiveNodesAdminDirty() {
  for (uint8_t i = 0; i < MAX_NODES; i++) {
    if (node_table[i].flags & NODE_FLAG_ACTIVE) {
      node_table[i].flags |= NODE_FLAG_ADMIN_DIRTY;
    }
  }
}

void WiFiOps::handleNodeTopologyChange() {
  this->current_assignment_version++;
  if (this->current_assignment_version == 0) 
    this->current_assignment_version = 1;

  this->recalculateChannelAssignments();
  this->markAllActiveNodesAdminDirty();
  //this->debugPrintNodeTable();
}

void WiFiOps::debugPrintNodeTable() {
  Serial.println("\n===== NODE TABLE =====");
  Serial.println("Current Assignment Version: " + (String)this->current_assignment_version);

  for (uint8_t i = 0; i < MAX_NODES; i++) {

    if (!(node_table[i].flags & NODE_FLAG_ACTIVE)) {
      continue;
    }

    uint8_t start_ch = scan_channels[node_table[i].start_channel_idx];
    uint8_t end_ch   = scan_channels[node_table[i].end_channel_idx];

    uint32_t age = millis() - node_table[i].last_seen_ms;

    Serial.printf(
      "Slot %u | MAC:%04X | idx:%u | ch:%u-%u | ver:%u | flags:0x%02X | age:%lu ms\n",
      i,
      node_table[i].mac_suffix,
      node_table[i].assigned_index,
      start_ch,
      end_ch,
      node_table[i].last_admin_version_sent,
      node_table[i].flags,
      age
    );
  }

  Serial.println("======================\n");
}

void WiFiOps::setFixedChannel(uint8_t ch) {
  // Disable power save (prevents weird timing/channel behavior)
  esp_wifi_set_ps(WIFI_PS_NONE);

  esp_wifi_set_promiscuous(true);

  // Force primary channel
  esp_err_t e = esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
  if (e != ESP_OK) {
    Serial.printf("esp_wifi_set_channel failed: %d (0x%X)\n", (int)e, (unsigned)e);
    return;
  }

  // Verify
  uint8_t primary = 0;
  wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE;
  esp_wifi_get_channel(&primary, &second);
  Serial.printf("Home channel is now: %u\n", primary);

  esp_wifi_set_promiscuous(false);
}

bool WiFiOps::addPeerWithMode(const uint8_t* mac, bool encrypt, const uint8_t lmk16[16]) {
  // If peer exists (possibly with wrong mode), delete it first
  if (esp_now_is_peer_exist(mac)) {
    esp_now_del_peer(mac);
    delay(10); // small settle
  }

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, mac, 6);
  peerInfo.channel = 0;          // follow home channel (safer)
  peerInfo.encrypt = encrypt;

  if (encrypt) {
    memcpy(peerInfo.lmk, lmk16, 16);
  }

  return (esp_now_add_peer(&peerInfo) == ESP_OK);
}

bool WiFiOps::sendAdminToNodeSlot(uint8_t slot, const uint8_t* dest_mac) {
  extern WiFiOps wifi_ops;

  if (!(node_table[slot].flags & NODE_FLAG_ACTIVE)) return false;

  enow_admin_msg_t msg = {};
  memcpy(msg.magic, MAGIC, 4);
  msg.type = MSG_ADMIN;
  msg.assignment_version = wifi_ops.current_assignment_version;
  msg.node_index = node_table[slot].assigned_index;
  msg.node_count = wifi_ops.getActiveNodeCount();
  msg.start_channel_idx = node_table[slot].start_channel_idx;
  msg.end_channel_idx = node_table[slot].end_channel_idx;

  // Temporary plaintext peer
  if (!esp_now_is_peer_exist(dest_mac)) {
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, dest_mac, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
      return false;
    }
  }

  esp_err_t res = esp_now_send(dest_mac, (uint8_t*)&msg, sizeof(msg));
  if (res != ESP_OK) {
    char macStr[18];
    utils.macToStr(dest_mac, macStr);
    Serial.printf("ESPNOW send failed to %s err=%d\n", macStr, res);
    esp_now_del_peer(dest_mac);
    return false;
  }

  esp_now_del_peer(dest_mac);

  node_table[slot].last_admin_version_sent = wifi_ops.current_assignment_version;
  node_table[slot].flags &= ~NODE_FLAG_ADMIN_DIRTY;
  return true;
}

void WiFiOps::sendCoreRequest() {
  enow_text_msg_t msg = {};
  memcpy(msg.magic, MAGIC, 4);
  msg.type = MSG_CORE_REQUEST;
  msg.counter = 0;

  // Broadcast peer must be unencrypted
  addPeerWithMode(BROADCAST_MAC, false, nullptr);

  esp_err_t res = esp_now_send(BROADCAST_MAC, (uint8_t*)&msg, sizeof(msg));
  Serial.printf("NODE: Broadcast CORE_REQUEST -> %s (next in %lu ms)\n",
                (res == ESP_OK) ? "OK" : "FAIL",
                (unsigned long)g_req_interval_ms);
}

void WiFiOps::sendCoreReply(const uint8_t* destMac) {
  enow_text_msg_t msg = {};
  memcpy(msg.magic, MAGIC, 4);
  msg.type = MSG_CORE_REPLY;
  msg.counter = 0;

  if (!addPeerWithMode(destMac, false, nullptr)) {
    Logger::log(WARN_MSG, "CORE: Failed to add NODE peer (reply)");
    return;
  }

  esp_err_t res = esp_now_send(destMac, (uint8_t*)&msg, sizeof(msg));

  char macStr[18];
  utils.macToStr(destMac, macStr);

  Serial.printf("CORE: Sent CORE_REPLY to %s -> %s\n",
                macStr, (res == ESP_OK) ? "OK" : "FAIL");
}

void WiFiOps::runAdminWindowAfterScanCycle() {
  this->setFixedChannel(ESPNOW_CHANNEL);
  delay(5);
  this->sendHeartbeat();

  // Wait for ADMIN response
  unsigned long start = millis();
  while ((millis() - start) < ADMIN_WAIT_MS) {
    delay(5);
  }
}

void WiFiOps::startNextNodeAssignedScan() {
  if (assigned_start_idx >= NUM_SCAN_CHANNELS ||
      assigned_end_idx >= NUM_SCAN_CHANNELS ||
      assigned_start_idx > assigned_end_idx) {
    WiFi.scanNetworks(true, true, false, 80);
    return;
  }

  if (current_assigned_scan_idx < assigned_start_idx ||
      current_assigned_scan_idx > assigned_end_idx) {
    current_assigned_scan_idx = assigned_start_idx;
  }

  uint8_t channel = scan_channels[current_assigned_scan_idx];
  WiFi.scanNetworks(true, true, false, 80, channel);

  current_assigned_scan_idx++;
  if (current_assigned_scan_idx > assigned_end_idx) {
    current_assigned_scan_idx = assigned_start_idx;
  }
}

void WiFiOps::sendHeartbeat() {
  extern WiFiOps wifi_ops;

  if (wifi_ops.use_encryption) {
    if (!g_secure_ready) return;
  }

  enow_text_msg_t msg = {};
  memcpy(msg.magic, MAGIC, 4);
  msg.type = MSG_HEARTBEAT;
  msg.counter = g_hb_counter++;

  esp_err_t res;
  if (wifi_ops.use_encryption) {
    res = esp_now_send(g_core_mac, (uint8_t*)&msg, sizeof(msg));
  } else {
    res = esp_now_send(BROADCAST_MAC, (uint8_t*)&msg, sizeof(msg));
  }

  if (res != ESP_OK) {
    Logger::log(WARN_MSG, "NODE: Heartbeat send FAIL");
  } else {
    Logger::log(GUD_MSG, "NODE: Sent heartbeat");
  }
}

bool WiFiOps::sendEncryptedStringToCore(const String& s) {
  this->setFixedChannel(ESPNOW_CHANNEL);

  if (!g_secure_ready) {
    Logger::log(WARN_MSG, "NODE: Not secure-ready; cannot send encrypted text");
    return false;
  }

  enow_text_msg_t msg = {};
  memcpy(msg.magic, MAGIC, 4);
  msg.type = MSG_TEXT;
  msg.counter = 0;  // optional for text

  size_t n = s.length();
  if (n > ENOW_TEXT_MAX) n = ENOW_TEXT_MAX;

  memcpy(msg.text, s.c_str(), n);
  msg.text[n] = '\0';
  msg.len = (uint16_t)n;

  // SEND FULL STRUCT (like heartbeat)
  esp_err_t res = esp_now_send(g_core_mac, (uint8_t*)&msg, sizeof(msg));

  if (res != ESP_OK) {
    Serial.printf("NODE: Encrypted text send FAIL (err=%d)\n", (int)res);
    return false;
  }

  return true;
}

bool WiFiOps::sendBroadcastStringPlain(const String& s) {
  this->setFixedChannel(ESPNOW_CHANNEL);

  static const uint8_t bcast_mac[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

  if (!esp_now_is_peer_exist(bcast_mac)) {
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, bcast_mac, 6);
    peerInfo.channel = 0;       // follow current home channel
    peerInfo.encrypt = false;   // plaintext broadcast

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
      Logger::log(WARN_MSG, "NODE: Failed to add broadcast peer");
      return false;
    }
  }

  enow_text_msg_t msg = {};
  memcpy(msg.magic, MAGIC, 4);
  msg.type = MSG_TEXT;
  msg.counter = 0;

  size_t n = s.length();
  if (n > ENOW_TEXT_MAX) n = ENOW_TEXT_MAX;

  memcpy(msg.text, s.c_str(), n);
  msg.text[n] = '\0';
  msg.len = (uint16_t)n;

  esp_err_t res = esp_now_send(bcast_mac, (uint8_t*)&msg, sizeof(msg));

  if (res != ESP_OK) {
    Serial.printf("NODE: Broadcast plaintext send FAIL (err=%d)\n", (int)res);
    return false;
  }

  return true;
}

bool WiFiOps::parseWardriveLine(const enow_text_msg_t& msg, WardriveRecord& out) {
  const char* line = msg.text;   // <-- no copy
  int start = 0;
  int fieldIndex = 0;

  String fields[6];

  while (fieldIndex < 6) {
    const char* comma = strchr(line + start, ',');

    if (!comma) {
      fields[fieldIndex++] = String(line + start);
      break;
    }

    fields[fieldIndex++] = String(line + start).substring(0, comma - (line + start));
    start = (comma - line) + 1;
  }

  if (fieldIndex != 6) return false;

  out.bssid    = fields[0];
  out.essid    = fields[1];
  out.security = fields[2];
  out.channel  = fields[3].toInt();
  out.rssi     = fields[4].toInt();
  out.type     = fields[5];

  return true;
}

void WiFiOps::OnDataRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
  extern WiFiOps wifi_ops;

  bool isNewNode = false;
  int slot = -1;

  // Need at least magic[4] + type[1]
  if (!info || !data || len < 5) return;

  // Validate protocol magic
  if (memcmp(data, MAGIC, 4) != 0) return;

  const uint8_t msgType = data[4];
  const int rssi = (info->rx_ctrl) ? info->rx_ctrl->rssi : 0;

  char srcMacStr[18];
  utils.macToStr(info->src_addr, srcMacStr);

  if (wifi_ops.run_mode == CORE_MODE) {

    if (msgType == MSG_CORE_REQUEST) {
      if (len < (int)sizeof(enow_text_msg_t)) return;

      const enow_text_msg_t* msg = (const enow_text_msg_t*)data;

      Serial.printf("CORE: RX CORE_REQUEST from %s | RSSI %d dBm\n", srcMacStr, rssi);

      // Track node regardless of encryption success
      slot = wifi_ops.touchNode(info->src_addr, isNewNode);

      // Plaintext reply so node can learn CORE MAC
      wifi_ops.sendCoreReply(info->src_addr);

      // Only set up encrypted peer if encryption is enabled
      if (wifi_ops.use_encryption) {
        if (!wifi_ops.addPeerWithMode(info->src_addr, true, lmk)) {
          Logger::log(WARN_MSG, "CORE: Failed to add ENCRYPTED peer for NODE");
        } else {
          Serial.printf("CORE: Encrypted peer ready for %s\n", srcMacStr);
          if (slot >= 0) {
            node_table[slot].flags |= NODE_FLAG_ENCRYPTED;
          }
        }
      }

      if (slot >= 0 && isNewNode) {
        wifi_ops.handleNodeTopologyChange();
      }

      // Initial pairing is a valid rendezvous point for admin
      if (slot >= 0 && (node_table[slot].flags & NODE_FLAG_ADMIN_DIRTY)) {
        if (wifi_ops.sendAdminToNodeSlot(slot, info->src_addr)) {
          Logger::log(GUD_MSG, "Successfully sent Admin message to Node");
          wifi_ops.debugPrintNodeTable();
        }
      }

      (void)msg;
      return;
    }

    if (msgType == MSG_HEARTBEAT) {
      if (len < (int)sizeof(enow_text_msg_t)) return;

      const enow_text_msg_t* msg = (const enow_text_msg_t*)data;

      Serial.printf("CORE: RX HEARTBEAT from %s | RSSI %d dBm | #%lu\n",
                    srcMacStr, rssi, (unsigned long)msg->counter);

      slot = wifi_ops.touchNode(info->src_addr, isNewNode);

      if (slot >= 0 && isNewNode) {
        wifi_ops.handleNodeTopologyChange();
      }

      // Heartbeat is the preferred rendezvous moment for admin
      if (slot >= 0 && (node_table[slot].flags & NODE_FLAG_ADMIN_DIRTY)) {
        if (wifi_ops.sendAdminToNodeSlot(slot, info->src_addr)) {
          Logger::log(GUD_MSG, "Successfully sent Admin message to Node");
          wifi_ops.debugPrintNodeTable();
        }
      }

      return;
    }

    if (msgType == MSG_TEXT) {
      if (len < (int)sizeof(enow_text_msg_t)) return;

      const enow_text_msg_t* t = (const enow_text_msg_t*)data;

      // Track sender as soon as a valid text packet arrives
      /*slot = wifi_ops.touchNode(info->src_addr, isNewNode);

      if (slot >= 0 && isNewNode) {
        wifi_ops.handleNodeTopologyChange();
      }

      if (slot >= 0 && (node_table[slot].flags & NODE_FLAG_ADMIN_DIRTY)) {
        wifi_ops.sendAdminToNodeSlot(slot, info->src_addr);
      }*/

      if ((t->len <= ENOW_TEXT_MAX) && (!wifi_ops.checkGeofences())) {
        Serial.printf("CORE: RX WARDRV TEXT from %s: %s\n", srcMacStr, t->text);

        WardriveRecord rec;
        if (wifi_ops.parseWardriveLine(*t, rec)) {
          uint8_t bssid[6] = {0};
          utils.convertMacStringToUint8(rec.bssid, bssid);

          if (!wifi_ops.seen_mac(bssid)) {
            String type = "WIFI";
            if (rec.type == "B")
              type = "BLE";

            String wardrive_line =
              rec.bssid + "," +
              rec.essid + "," +
              rec.security + "," +
              gps.getDatetime() + "," +
              (String)rec.channel + "," +
              (String)rec.rssi + "," +
              gps.getLat() + "," +
              gps.getLon() + "," +
              gps.getAlt() + "," +
              gps.getAccuracy() + "," +
              type;

            Logger::log(GUD_MSG, wardrive_line);

            if (gps.getFixStatus()) {
              // Mark the MAC as logged only once we have a fix and
              // write the record.
              wifi_ops.save_mac(bssid);

              if (type == "WIFI") {
                wifi_ops.setCurrentNetCount(wifi_ops.getCurrentNetCount() + 1);
                wifi_ops.setTotalNetCount(wifi_ops.getTotalNetCount() + 1);

                if (rec.channel > 14) {
                  wifi_ops.setCurrent5gCount(wifi_ops.getCurrent5gCount() + 1);
                } else {
                  wifi_ops.setCurrent2g4Count(wifi_ops.getCurrent2g4Count() + 1);
                }
              } else if (type == "BLE") {
                wifi_ops.setCurrentBLECount(wifi_ops.getCurrentBLECount() + 1);
                wifi_ops.setTotalBLECount(wifi_ops.getTotalBLECount() + 1);
              }

              buffer.append(wardrive_line + "\n");

              // Roll log file if entry limit reached
              uint32_t total_entries = wifi_ops.getCurrent2g4Count() +
              wifi_ops.getCurrent5gCount() +
              wifi_ops.getCurrentBLECount();
              if (total_entries >= LOG_ROLL_ENTRIES) {
                Logger::log(STD_MSG, "[LOG] Rolling log — " +
                String(LOG_ROLL_ENTRIES) + " entries reached");
                wifi_ops.startLog(LOG_FILE_NAME);
                wifi_ops.setCurrent2g4Count(0);
                wifi_ops.setCurrent5gCount(0);
                wifi_ops.setCurrentBLECount(0);
                wifi_ops.setCurrentNetCount(0);
              }
            }
          }
        }
      } else {
        Logger::log(WARN_MSG, "CORE: RX WARDRV TEXT: text len: " + (String)t->len + " > ENOW_TEXT_MAX");
      }

      //if (slot >= 0 && isNewNode) {
      //  wifi_ops.handleNodeTopologyChange();
      //}

      // Do NOT send admin here; wait for heartbeat/check-in window
      return;
    }

    // Unknown / unhandled type on CORE
    Serial.printf("CORE: RX unknown type %u from %s | RSSI %d dBm\n",
                  msgType, srcMacStr, rssi);
    return;
  }

  if (wifi_ops.run_mode == NODE_MODE) {

    if (msgType == MSG_CORE_REPLY) {
      if (len < (int)sizeof(enow_text_msg_t)) return;

      memcpy(g_core_mac, info->src_addr, 6);
      g_have_core = true;

      char coreStr[18];
      utils.macToStr(g_core_mac, coreStr);
      Serial.printf("NODE: Learned CORE MAC (plaintext reply): %s | RSSI %d dBm\n", coreStr, rssi);

      if (wifi_ops.use_encryption) {
        if (!wifi_ops.addPeerWithMode(g_core_mac, true, lmk)) {
          Logger::log(WARN_MSG, "NODE: Failed to add ENCRYPTED peer for CORE");
          g_secure_ready = false;
        } else {
          g_secure_ready = true;
          Logger::log(GUD_MSG, "NODE: Encrypted peer ready; switching to encrypted heartbeats...");
        }
      } else {
        g_secure_ready = true;
      }

      return;
    }

    if (msgType == MSG_ADMIN) {
      if (len < (int)sizeof(enow_admin_msg_t)) return;

      const enow_admin_msg_t* admin = (const enow_admin_msg_t*)data;

      if (admin->assignment_version != assignment_version) {
        assignment_version = admin->assignment_version;
        assigned_node_index = admin->node_index;
        assigned_node_count = admin->node_count;
        assigned_start_idx = admin->start_channel_idx;
        assigned_end_idx = admin->end_channel_idx;

        Serial.printf("NODE: New admin assignment v%u | node %u/%u | idx %u-%u\n",
                      assignment_version,
                      assigned_node_index,
                      assigned_node_count,
                      assigned_start_idx,
                      assigned_end_idx);
      }

      return;
    }

    // Unknown / unhandled type on NODE
    Serial.printf("NODE: RX unknown type %u from %s | RSSI %d dBm\n",
                  msgType, srcMacStr, rssi);
    return;
  }
}

void WiFiOps::startESPNow() {
  this->setFixedChannel(ESPNOW_CHANNEL);
  this->computeKeysFromEnowKey();

  if (esp_now_init() != ESP_OK) {
    Logger::log(WARN_MSG, "ESP-NOW init failed");
    return;
  }

  if (esp_now_set_pmk(pmk) != ESP_OK) {
    Logger::log(WARN_MSG, "Warning: esp_now_set_pmk failed");
  }

  esp_now_register_recv_cb(OnDataRecv);

  if (this->run_mode == CORE_MODE) {
    Logger::log(STD_MSG, "Role: CORE");
    Logger::log(STD_MSG, "CORE: Fixed channel " + (String)ESPNOW_CHANNEL + ", Waiting for CORE_REQUEST...\n");
  }
  else if (this->run_mode == NODE_MODE) {
    Logger::log(STD_MSG, "Role: NODE");
    Logger::log(STD_MSG, "NODE: Fixed channel " + (String)ESPNOW_CHANNEL + ", probing for CORE...\n");

    g_last_req_ms = millis();

    if (this->use_encryption)
      this->sendCoreRequest();
  }
}

void WiFiOps::derive_key_16(const String& s, uint8_t out16[16]) {
  uint8_t hash[32];
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx, 0);
  mbedtls_sha256_update(&ctx, (const unsigned char*)s.c_str(), s.length());
  mbedtls_sha256_finish(&ctx, hash);
  mbedtls_sha256_free(&ctx);

  memcpy(out16, hash, 16); // first 16 bytes
}

void WiFiOps::computeKeysFromEnowKey() {
  extern WiFiOps wifi_ops;
  Logger::log(STD_MSG, "computeKeysFromEnowKey Compute key: " + wifi_ops.esp_now_key);
  derive_key_16(wifi_ops.esp_now_key + "_pmk", pmk);
  derive_key_16(wifi_ops.esp_now_key + "_lmk", lmk);
}

void WiFiOps::setCurrentScanMode(uint8_t scan_mode) {
  this->current_scan_mode = scan_mode;
}

bool WiFiOps::getHasCore() {
  return g_have_core;
}

bool WiFiOps::getSecureReady() {
  return g_secure_ready;
}

bool WiFiOps::getNodeReady() {
  if (!this->use_encryption)
    return true;
  else if ((this->getHasCore()) && (this->getSecureReady()))
    return true;

  return false;
}

uint8_t WiFiOps::getCurrentScanMode() {
  return this->current_scan_mode;
}

void WiFiOps::setTotalNetCount(uint32_t count) {
  this->total_net_count = count;
}

void WiFiOps::setTotalBLECount(uint32_t count) {
  this->total_ble_count = count;
}

void WiFiOps::setCurrentNetCount(uint32_t count) {
  this->current_net_count = count;
}

void WiFiOps::setCurrent2g4Count(uint32_t count) {
  this->current_2g4_count = count;
}

void WiFiOps::setCurrent5gCount(uint32_t count) {
  this->current_5g_count = count;
}

void WiFiOps::setCurrentBLECount(uint32_t count) {
  this->current_ble_count = count;
}

uint32_t WiFiOps::getTotalNetCount() {
  return this->total_net_count;
}

uint32_t WiFiOps::getTotalBLECount() {
  return this->total_ble_count;
}

uint32_t WiFiOps::getCurrentNetCount() {
  return this->current_net_count;
}

uint32_t WiFiOps::getCurrent2g4Count() {
  return this->current_2g4_count;
}

uint32_t WiFiOps::getCurrent5gCount() {
  return this->current_5g_count;
}

uint32_t WiFiOps::getCurrentBLECount() {
  return this->current_ble_count;
}

void WiFiOps::scanBLE() {
  //Logger::log(STD_MSG, "Starting BLE scan...");
  pBLEScan->clearResults();
  pBLEScan->start(BLE_SCAN_DURATION, false, false);
  //Logger::log(STD_MSG, "Completed BLE scan");
}

int WiFiOps::runWardrive(uint32_t currentTime) {

  int scan_status = -1;

  // ---- Chunk 5: geofence check ----
  // Only check when we have a GPS fix — no position, no geofence.
  // Dock mode (Chunk 6) handles K1T upload trigger while paused.
  if (gps.getGpsModuleStatus() && gps.getFixStatus()) {
    if (this->checkGeofences()) {
      // Chunk 6: while geofence-paused, periodically scan for trigger SSID
      // so K1T can still trigger a dock+upload even when wardriving is paused.
      if (millis() - this->geo_passive_scan_time >= DOCK_SCAN_INTERVAL) {
        this->geo_passive_scan_time = millis();
        //Logger::log(STD_MSG, "Calling scanForTriggerSSID from runWardrive");
        if (this->scanForTriggerSSID()) {
          Logger::log(STD_MSG, "[DOCK] Trigger SSID found during geofence pause");
          this->dock_state            = DOCK_STATE_CONNECTING;
          this->dock_connect_attempts = 0;
        }
      }
      return -1; // inside a geofence — pause wardriving
    }
  }
  // ---- end geofence check ----

  if ((this->run_mode == SOLO_MODE) || (this->run_mode == NODE_MODE)) {

    // Check GPS status
    if (((gps.getGpsModuleStatus()) && (gps.getFixStatus()) && (sd_obj.supported)) || 
        (this->run_mode == NODE_MODE)) {

      scan_status = WiFi.scanComplete();

      // Pause if scan is running already
      if (scan_status == WIFI_SCAN_RUNNING) // Scan is still running
        delay(1);
      else if (scan_status == WIFI_SCAN_FAILED) { // Scan is failed or not started
        if (this->run_mode == NODE_MODE)
          this->startNextNodeAssignedScan();
        else
          WiFi.scanNetworks(true, true, false, CHANNEL_TIMER);
        delay(100);
        if (WiFi.scanComplete() == WIFI_SCAN_FAILED)
          Logger::log(WARN_MSG, "WiFi scan failed to start!");
      }
      else {
        // ---- Chunk 6: check for trigger SSID in scan results ----
        // Do this BEFORE processWardrive so we can abort gracefully
        // if we need to switch to dock mode.
        String trigSSID = settings.loadSetting<String>(TRIGGER_SSID_NAME);
        if (!trigSSID.isEmpty()) {
          for (int j = 0; j < scan_status; j++) {
            if (WiFi.SSID(j) == trigSSID) {
              Logger::log(STD_MSG, "[DOCK] Trigger SSID detected in scan: " + trigSSID);
              WiFi.scanDelete();
              this->dock_state            = DOCK_STATE_CONNECTING;
              this->dock_connect_attempts = 0;
              return scan_status; // main() will call runDockMode next cycle
            }
          }
        }
        // ---- end trigger SSID check ----

        this->current_net_count = 0;
        this->current_ble_count = 0;
        this->current_2g4_count = 0;
        this->current_5g_count = 0;

        // Scan has completed and is number of networks found
        // Handle the scan results
        this->processWardrive(scan_status);

        // Delete the scan data
        WiFi.scanDelete();

        // Scan BLE here
        if (current_assigned_scan_idx == assigned_start_idx)
          this->scanBLE();

        while(pBLEScan->isScanning())
          delay(1);

        if ((this->run_mode == NODE_MODE) && (current_assigned_scan_idx == assigned_start_idx))
          this->runAdminWindowAfterScanCycle();

        // Start a new scan on all channels
        if (this->run_mode == NODE_MODE)
          this->startNextNodeAssignedScan();
        else
          WiFi.scanNetworks(true, true, false, CHANNEL_TIMER);
      }
    }
  }

  return scan_status;
}

// ============================================================
// Chunk 5: Geofence — Haversine distance, cache loader,
//           and zone check with enter/exit logging
// ============================================================

// Haversine formula — returns great-circle distance in metres.
// Accurate to within ~0.5% which is well within any practical
// geofence radius.
float WiFiOps::haversineDistance(float lat1, float lon1,
                                  float lat2, float lon2) {
  const float R = 6371000.0f; // Earth mean radius, metres
  float phi1  = lat1 * DEG_TO_RAD;
  float phi2  = lat2 * DEG_TO_RAD;
  float dphi  = (lat2 - lat1) * DEG_TO_RAD;
  float dlam  = (lon2 - lon1) * DEG_TO_RAD;

  float a = sinf(dphi / 2.0f) * sinf(dphi / 2.0f) +
            cosf(phi1) * cosf(phi2) *
            sinf(dlam / 2.0f) * sinf(dlam / 2.0f);
  float c = 2.0f * atan2f(sqrtf(a), sqrtf(1.0f - a));
  return R * c;
}

// Parse all geo_0..geo_4 settings into geo_cache[].
// Called once from begin() and on-demand via reloadGeofenceCache().
// An entry is marked valid only when radius > 0 and lat/lon are set.
void WiFiOps::loadGeofenceCache() {
  int valid_count = 0;

  for (int i = 0; i < MAX_GEOFENCES; i++) {
    geo_cache[i].valid = false;

    String geoStr = settings.loadSetting<String>("geo_" + String(i));
    if (geoStr.isEmpty()) continue;

    DynamicJsonDocument doc(256);
    if (deserializeJson(doc, geoStr) != DeserializationError::Ok) {
      Logger::log(WARN_MSG, "[GEO] Bad JSON for geo_" + String(i));
      continue;
    }

    float  lat   = doc["lat"]   | 0.0f;
    float  lon   = doc["lon"]   | 0.0f;
    int    rad   = doc["rad"]   | 0;
    String label = doc["label"] | "";

    if (rad <= 0) continue;           // unconfigured slot
    if (lat == 0.0f && lon == 0.0f) continue; // default placeholder

    geo_cache[i].lat   = lat;
    geo_cache[i].lon   = lon;
    geo_cache[i].rad   = rad;
    geo_cache[i].label = label.isEmpty() ? ("Zone " + String(i + 1)) : label;
    geo_cache[i].valid = true;
    valid_count++;

    Logger::log(GUD_MSG, "[GEO] Loaded zone " + String(i + 1) +
                ": \"" + geo_cache[i].label +
                "\" lat=" + String(lat, 6) +
                " lon=" + String(lon, 6) +
                " rad=" + String((int)(rad * 3.28084)) + "ft");
  }

  geo_cache_loaded = true;
  Logger::log(STD_MSG, "[GEO] Cache loaded: " + String(valid_count) +
              " active zone(s)");
}

// Public: invalidate cache so it reloads on next checkGeofences() call.
// Call this after saving new geofence settings.
void WiFiOps::reloadGeofenceCache() {
  geo_cache_loaded = false;
  Logger::log(STD_MSG, "[GEO] Cache invalidated — will reload on next check");
}

// Check whether the current GPS position falls inside any configured zone.
// Handles enter/exit state transitions with logging and TFT feedback.
// Returns true if inside any zone (wardriving should pause).
bool WiFiOps::checkGeofences(char* dist_str, size_t dist_str_len) {
  // Lazy-load cache if not yet populated
  if (!geo_cache_loaded)
    this->loadGeofenceCache();

  // Quick exit if no zones are configured
  bool any_valid = false;
  for (int i = 0; i < MAX_GEOFENCES; i++) {
    if (geo_cache[i].valid) {
      any_valid = true;
      break;
    }
  }
  if (!any_valid)
    return false;

  float cur_lat = gps.getLat().toFloat();
  float cur_lon = gps.getLon().toFloat();

  for (int i = 0; i < MAX_GEOFENCES; i++) {
    if (!geo_cache[i].valid)
      continue;

    float dist = this->haversineDistance(
      cur_lat,
      cur_lon,
      geo_cache[i].lat,
      geo_cache[i].lon);

    if (dist <= (float)geo_cache[i].rad) {
      // Populate caller's distance string if supplied
      if (dist_str && dist_str_len > 0) {
        int dist_ft = (int)(dist * 3.28084f);

        if (dist_ft >= 5280) {
          snprintf(
            dist_str,
            dist_str_len,
            "%.2fmi",
            dist_ft / 5280.0f);
        } else {
          snprintf(
            dist_str,
            dist_str_len,
            "%dft",
            dist_ft);
        }
      }

      // Inside this zone
      bool was_in = this->in_geofence;
      bool label_changed = (this->current_geo_label != geo_cache[i].label);

      if (!was_in || label_changed) {
        this->in_geofence = true;
        this->current_geo_label = geo_cache[i].label;

        Logger::log(
          STD_MSG,
          "[GEO] Entered zone: \"" +
          this->current_geo_label +
          "\"  dist=" +
          String((int)(dist * 3.28084)) +
          "ft  rad=" +
          String((int)(geo_cache[i].rad * 3.28084)) +
          "ft");

        display.clearScreen();
        display.tft->setCursor(0, 0);
        display.tft->setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
        display.tft->println("GEOFENCE PAUSED");
        display.tft->setTextColor(ST77XX_WHITE, ST77XX_BLACK);

        int dist_ft = (int)(dist * 3.28084f);

        String display_dist =
          (dist_ft >= 5280)
            ? String(dist_ft / 5280.0f, 2) + "mi"
            : String(dist_ft) + "ft";

        display.tft->println(
          this->current_geo_label + " " + display_dist);

        this->geo_display_shown = true;
      }

      return true;
    }
  }

  // Not inside any zone — handle exit transition
  if (this->in_geofence) {
    Logger::log(
      STD_MSG,
      "[GEO] Exited zone: \"" +
      this->current_geo_label +
      "\" — resuming wardrive");

    this->in_geofence = false;
    this->current_geo_label = "";
    this->geo_display_shown = false;

    display.clearScreen();
  }

  return false;
}

// ============================================================
// Chunk 4: SSID exclusion helper
// Checks a sanitized SSID against the caller's pre-loaded
// exclusion list. Empty exclusion slots are skipped.
// Comparison is case-sensitive (SSIDs are case-sensitive).
// ============================================================
bool WiFiOps::isSSIDExcluded(const String& ssid,
                              const String* list, int count) {
  if (ssid.isEmpty()) return false;
  for (int i = 0; i < count; i++) {
    if (!list[i].isEmpty() && list[i] == ssid)
      return true;
  }
  return false;
}

void WiFiOps::processWardrive(uint16_t networks) {
  String display_string;
  bool do_save;

  // ---- Chunk 4: load exclusion list once per scan cycle ----
  // Doing this outside the network loop avoids re-parsing the
  // settings JSON for every scanned network.
  String exclusions[MAX_SSID_EXCLUSIONS];
  int exclusion_count = 0;
  for (int e = 0; e < MAX_SSID_EXCLUSIONS; e++) {
    exclusions[e] = settings.loadSetting<String>("sx_" + String(e));
    if (!exclusions[e].isEmpty()) exclusion_count++;
  }

  // Process results if networks found
  if (networks > 0) {
    for (int i = 0; i < networks; i++) {
      if (this->run_mode == SOLO_MODE)
        digitalWrite(LED_PIN, HIGH);
      display_string = "";
      do_save = false;
      uint8_t *this_bssid_raw = WiFi.BSSID(i);
      char this_bssid[18] = {0};
      sprintf(this_bssid, "%02X:%02X:%02X:%02X:%02X:%02X", this_bssid_raw[0], this_bssid_raw[1], this_bssid_raw[2], this_bssid_raw[3], this_bssid_raw[4], this_bssid_raw[5]);

      if (this->seen_mac(this_bssid_raw))
        continue;

      this->save_mac(this_bssid_raw);

      if (this->run_mode == SOLO_MODE) {

        // ---- Chunk 4: SSID exclusion check ----
        // Pull SSID before the counters so excluded networks
        // don't inflate the displayed scan count.
        String ssid = WiFi.SSID(i);
        ssid.replace(",","_");

        if (exclusion_count > 0 &&
            this->isSSIDExcluded(ssid, exclusions, MAX_SSID_EXCLUSIONS)) {
          Logger::log(STD_MSG, "[EXCL] Skipping excluded SSID: \"" + ssid + "\"");
          digitalWrite(LED_PIN, LOW);
          continue;
        }
        // ---- end exclusion check ----

        this->setCurrentNetCount(this->getCurrentNetCount() + 1);
        this->setTotalNetCount(this->getTotalNetCount() + 1);

        if (WiFi.channel(i) > 14)
          this->setCurrent5gCount(this->getCurrent5gCount() + 1);
        else
          this->setCurrent2g4Count(this->getCurrent2g4Count() + 1);

        if (ssid != "") {
          display_string.concat(ssid);
        }
        else {
          display_string.concat(this_bssid);
        }

        if (gps.getFixStatus()) {
          do_save = true;
          display_string.concat(" | Lt: " + gps.getLat());
          display_string.concat(" | Ln: " + gps.getLon());
        }
        else {
          display_string.concat(" | GPS: No Fix");
        }

        int temp_len = display_string.length();

        #ifdef HAS_SCREEN
          for (int i = 0; i < 40 - temp_len; i++)
          {
            display_string.concat(" ");
          }
          
          //display_obj.display_buffer->add(display_string);
        #endif

        String wardrive_line = WiFi.BSSIDstr(i) + "," + ssid + "," + this->security_int_to_string(WiFi.encryptionType(i)) + "," + gps.getDatetime() + "," + (String)WiFi.channel(i) + "," + (String)WiFi.RSSI(i) + "," + gps.getLat() + "," + gps.getLon() + "," + gps.getAlt() + "," + gps.getAccuracy() + ",WIFI";
        Logger::log(GUD_MSG, (String)this->mac_history_cursor + " | " + wardrive_line);

        if (this->run_mode == SOLO_MODE)
          digitalWrite(LED_PIN, LOW);

        if (do_save) {
          buffer.append(wardrive_line + "\n");
        }
      }
      else if (this->run_mode == NODE_MODE) {
        String ssid = WiFi.SSID(i);
        ssid.replace(",","_");

        // ---- Chunk 4: exclusion filter for NODE_MODE too ----
        if (exclusion_count > 0 &&
            this->isSSIDExcluded(ssid, exclusions, MAX_SSID_EXCLUSIONS)) {
          Logger::log(STD_MSG, "[EXCL] Node skipping excluded SSID: \"" + ssid + "\"");
          continue;
        }

        String enow_line = WiFi.BSSIDstr(i) + "," + ssid + "," + this->security_int_to_string(WiFi.encryptionType(i)) + "," + (String)WiFi.channel(i) + "," + (String)WiFi.RSSI(i) + ",W";
        Logger::log(GUD_MSG, (String)this->mac_history_cursor + " | " + enow_line);
        if (this->use_encryption)
          this->sendEncryptedStringToCore(enow_line);
        else
          this->sendBroadcastStringPlain(enow_line);
      }
    }
  }

  if (this->run_mode == SOLO_MODE)
    digitalWrite(LED_PIN, LOW);
}

bool WiFiOps::mac_cmp(struct mac_addr addr1, struct mac_addr addr2) {
  //Return true if 2 mac_addr structs are equal.
  for (int y = 0; y < 6 ; y++) {
    if (addr1.bytes[y] != addr2.bytes[y]) {
      return false;
    }
  }
  return true;
}

bool WiFiOps::seen_mac(unsigned char* mac) {
  //Return true if this MAC address is in the recently seen array.

  struct mac_addr tmp;
  for (int x = 0; x < 6 ; x++) {
    tmp.bytes[x] = mac[x];
  }

  for (int x = 0; x < mac_history_len; x++) {
    if (this->mac_cmp(tmp, mac_history[x])) {
      return true;
    }
  }
  return false;
}

void WiFiOps::save_mac(unsigned char* mac) {
  //Save a MAC address into the recently seen array.
  if (this->mac_history_cursor >= mac_history_len) {
    this->mac_history_cursor = 0;
  }
  struct mac_addr tmp;
  for (int x = 0; x < 6 ; x++) {
    tmp.bytes[x] = mac[x];
  }

  mac_history[this->mac_history_cursor] = tmp;
  this->mac_history_cursor++;
}

String WiFiOps::security_int_to_string(int security_type) {
  //Provide a security type int from WiFi.encryptionType(i) to convert it to a String which Wigle CSV expects.
  String authtype = "";

  switch (security_type) {
    case WIFI_AUTH_OPEN:
      authtype = "[OPEN]";
      break;
  
    case WIFI_AUTH_WEP:
      authtype = "[WEP]";
      break;
  
    case WIFI_AUTH_WPA_PSK:
      authtype = "[WPA_PSK]";
      break;
  
    case WIFI_AUTH_WPA2_PSK:
      authtype = "[WPA2_PSK]";
      break;
  
    case WIFI_AUTH_WPA_WPA2_PSK:
      authtype = "[WPA_WPA2_PSK]";
      break;
  
    case WIFI_AUTH_WPA2_ENTERPRISE:
      authtype = "[WPA2]";
      break;

    //Requires at least v2.0.0 of https://github.com/espressif/arduino-esp32/
    case WIFI_AUTH_WPA3_PSK:
      authtype = "[WPA3_PSK]";
      break;

    case WIFI_AUTH_WPA2_WPA3_PSK:
      authtype = "[WPA2_WPA3_PSK]";
      break;

    case WIFI_AUTH_WAPI_PSK:
      authtype = "[WAPI_PSK]";
      break;
        
    default:
      authtype = "[UNDEFINED]";
  }

  return authtype;
}

void WiFiOps::clearMacHistory() {
    for (int i = 0; i < mac_history_len; ++i) {
        memset(mac_history[i].bytes, 0, sizeof(mac_history[i].bytes));
    }
}

void WiFiOps::startLog(String file_name) {
  // Build WiGLE-convention filename: wigle-YYYY-MM-DDTHHMMSS+0000.log
  // Falls back to base file_name if GPS has no fix yet
  String timestamped_name = file_name;
  if (gps.getFixStatus()) {
    // Pull raw components from GPS (MicroNMEA provides these individually)
    // We build the filename ourselves to get proper zero-padding
    char fname[48];
    // gps.getDatetime() returns "YYYY-M-D H:MM:SS" — not suitable directly,
    // so we reconstruct from the wardrive line fields via the buffer datetime.
    // For now use the formatted datetime string and reformat it.
    // Format stored is "YYYY-M-D H:MM:SS" — parse and reformat.
    String dt = gps.getDatetime(); // e.g. "2026-5-1 13:34:37"
    if (dt.length() >= 10) {
      // Extract fields robustly
      int y = dt.substring(0, 4).toInt();
      int mo = dt.substring(5, dt.indexOf('-', 5)).toInt();
      int rest = dt.indexOf('-', 5);
      int d  = dt.substring(rest + 1, dt.indexOf(' ')).toInt();
      int sp = dt.indexOf(' ');
      int h  = dt.substring(sp + 1, dt.indexOf(':', sp)).toInt();
      int c1 = dt.indexOf(':', sp);
      int mi = dt.substring(c1 + 1, dt.indexOf(':', c1 + 1)).toInt();
      int c2 = dt.indexOf(':', c1 + 1);
      int s  = dt.substring(c2 + 1).toInt();
      snprintf(fname, sizeof(fname), "wigle-%04d-%02d-%02dT%02d%02d%02d+0000",
               y, mo, d, h, mi, s);
      timestamped_name = String(fname);
    }
  }

  buffer.logOpen(
    timestamped_name,
    #if defined(HAS_SD)
      sd_obj.supported ? &SD :
    #endif
    NULL,
    false
  );

  String header_line = "WigleWifi-1.4,appRelease=" + (String)FIRMWARE_VERSION +
                       ",model=" + (String)DEVICE_NAME +
                       ",release=" + (String)FIRMWARE_VERSION +
                       ",device=" + (String)DEVICE_NAME +
                       ",display=SPI TFT,board=ESP32-C5-DevKit,brand=JustCallMeKoko\n"
                       "MAC,SSID,AuthMode,FirstSeen,Channel,RSSI,"
                       "CurrentLatitude,CurrentLongitude,AltitudeMeters,AccuracyMeters,Type";
  buffer.append(header_line + "\n");
  Logger::log(GUD_MSG, "Log started: " + timestamped_name + ".log");
}

void WiFiOps::initWiFi(bool set_country) {
  if (set_country) {
    Logger::log(STD_MSG, "Setting country code...");
    esp_wifi_init(&cfg);
    esp_wifi_set_country(&country);
  }

  WiFi.STA.begin();
  WiFi.setBandMode(WIFI_BAND_MODE_AUTO);
  delay(100);
}

void WiFiOps::deinitWiFi() {
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_OFF);

}

void WiFiOps::deinitBLE() {
  if (!ble_initialized) {
    Logger::log(STD_MSG, "BLE already deinitialized, skipping");
    return;
  }
  ble_initialized = false;
  Logger::log(STD_MSG, "Deinitializing BLE...");   // existing line continues here

  if (pBLEScan != nullptr) {
    if (pBLEScan->isScanning()) {
      Logger::log(STD_MSG, "Stopping ongoing BLE scan...");
      pBLEScan->stop();
      while (pBLEScan->isScanning()) {
        delay(10);
        esp_task_wdt_reset(); // feed watchdog — BLE stop can take several seconds
      }
    }

    // Clear results to release internal memory
    Logger::log(STD_MSG, "Clearing scan results...");
    pBLEScan->clearResults();

    // Delete scan callbacks if dynamically allocated
    Logger::log(STD_MSG, "Releasing scan callbacks...");
    pBLEScan->setScanCallbacks(nullptr);
  }

  // Now safe to deinit BLE
  NimBLEDevice::deinit();
  Logger::log(STD_MSG, "Finished deinitializing BLE");
}

void WiFiOps::initBLE() {
  NimBLEDevice::init("");
  //delete pBLEScan;
  pBLEScan = NimBLEDevice::getScan();

  pBLEScan->setScanCallbacks(new scanCallbacks(), false);
  pBLEScan->setActiveScan(true);
  pBLEScan->setDuplicateFilter(false);       // Disables internal filtering based on MAC
  pBLEScan->setMaxResults(0);                // Prevent storing results in NimBLEScanResults
  ble_initialized = true;
}

bool WiFiOps::tryConnectToWiFi(unsigned long timeoutMs) {

  display.clearScreen();
  display.tft->setCursor(0, 0);
  display.tft->setTextColor(ST77XX_WHITE, ST77XX_BLACK);

  // Check if file exists
  if (!SPIFFS.exists(WIFI_CONFIG)) {
    Logger::log(WARN_MSG, "No saved WiFi config found.");
    return false;
  }

  display.tft->print("Joining WiFi: ");

  this->user_ap_ssid = settings.loadSetting<String>("s");
  this->user_ap_password = settings.loadSetting<String>("p");
  this->wigle_user = settings.loadSetting<String>("wu");
  this->wigle_token = settings.loadSetting<String>("wt");

  Logger::log(STD_MSG, "Attempting to connect with: ");
  Logger::log(STD_MSG, this->user_ap_ssid);
  display.tft->print(this->user_ap_ssid);
  display.tft->println("...");

  // Connect to WiFi with AP credentials
  WiFi.mode(WIFI_STA);
  WiFi.begin(this->user_ap_ssid.c_str(), this->user_ap_password.c_str());

  // Wait while we connect
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
    delay(500);
    Serial.print(".");
  }

  // Output status of connection attempt
  if (WiFi.status() == WL_CONNECTED) {
    display.clearScreen();
    display.tft->setCursor(0, 0);
    display.tft->print("Connected: ");
    display.tft->println(this->user_ap_ssid);
    display.tft->print("IP: ");
    display.tft->println(WiFi.localIP());
    Logger::log(GUD_MSG, "WiFi connected!");
    Logger::log(GUD_MSG, "IP address: " + WiFi.localIP().toString());
    return true;
  } else {
    Logger::log(WARN_MSG, "Failed to connect to WiFi.");
    display.tft->println("Failed to connect");
    WiFi.disconnect(true);
    return false;
  }
}

void WiFiOps::startAccessPoint() {
  display.clearScreen();
  display.tft->setCursor(0, 0);
  display.tft->print("Starting AP: ");
  display.tft->println(this->apSSID);
  WiFi.softAP(this->apSSID, this->apPassword);
  Logger::log(GUD_MSG, "Access Point started");
  Logger::log(GUD_MSG, "IP: ");
  Logger::log(GUD_MSG, WiFi.softAPIP().toString());
  display.tft->print("IP: ");
  display.tft->println(WiFi.softAPIP().toString());
}

bool WiFiOps::wigleUpload(String filePath) {
  //server.begin();

  display.clearScreen();
  display.drawCenteredText("Wigle Upload...", true);

  delay(100);

  if (!SD.exists(filePath)) {
    display.clearScreen();
    display.drawCenteredText(filePath + " not found", true);
    Logger::log(WARN_MSG, "File does not exist: " + filePath);
    return false;
  }

  File fileToUpload = SD.open(filePath);
  if (!fileToUpload) {
    display.clearScreen();
    display.drawCenteredText("Could not open file", true);
    Logger::log(WARN_MSG, "Could not open file: " + filePath);
    return false;
  }

  // Load credentials
  String username = settings.loadSetting<String>("wu");
  String token = settings.loadSetting<String>("wt");
  if (username.isEmpty() || token.isEmpty()) {
    fileToUpload.close();
    display.clearScreen();
    display.drawCenteredText("No wigle creds", true);
    Logger::log(WARN_MSG, "Missing wigle credentials");
    return false;
  }

  Logger::log(STD_MSG, "Username: " + username);
  Logger::log(STD_MSG, "Token: " + token);

  String boundary = "----ESP32BOUNDARY";
  String contentType = "multipart/form-data; boundary=" + boundary;

  // Build parts
  String part1 = "--" + boundary + "\r\n";
  part1 += "Content-Disposition: form-data; name=\"file\"; filename=\"" + filePath + "\"\r\n";
  part1 += "Content-Type: application/octet-stream\r\n\r\n";

  String part2 = "\r\n--" + boundary + "\r\n";
  part2 += "Content-Disposition: form-data; name=\"donate\"\r\n\r\non\r\n";

  String part3 = "--" + boundary + "--\r\n";

  int totalLength = part1.length() + fileToUpload.size() + part2.length() + part3.length();

  Logger::log(STD_MSG, "part1.length(): " + String(part1.length()));
  Logger::log(STD_MSG, "fileToUpload.size(): " + String(fileToUpload.size()));
  Logger::log(STD_MSG, "part2.length(): " + String(part2.length()));
  Logger::log(STD_MSG, "part3.length(): " + String(part3.length()));
  Logger::log(STD_MSG, "Total Content-Length: " + String(totalLength));

  Serial.print("File size: ");
  Serial.println(fileToUpload.size());

  // Connect manually via WiFiClientSecure
  //WiFiClientSecure *client = new WiFiClientSecure();
  //client.stop();
  client->setInsecure();
  client->setTimeout(5000);

  if (!client->connect("api.wigle.net", 443)) {
    fileToUpload.close();
    //delete client;
    client->stop();
    display.clearScreen();
    display.drawCenteredText("Could not connect", true);
    Logger::log(WARN_MSG, "Failed to connected to api.wigle.net");
    return false;
  }

  Serial.println("Connected");

  // Compose headers
  String auth = utils.base64Encode(username + ":" + token);

  Serial.println("Finished encoding");

  client->println("POST /api/v2/file/upload HTTP/1.1");
  client->println("Host: api.wigle.net");
  client->println("User-Agent: ESP32Uploader/1.0");
  client->println("Accept: application/json");
  client->println("Authorization: Basic " + auth);
  client->println("Content-Type: " + contentType);
  client->print("Content-Length: ");
  client->println(totalLength);
  client->println();
  delay(100);

  Serial.println("Finished sending header");

  // Send body
  client->print(part1);
  const size_t BUFFER_SIZE = 4096; // 1KB at a time
  uint8_t buffer[BUFFER_SIZE];

  Serial.println("Finished sending part1");

  uint8_t percent_sent = 0;

  String display_percent = "";

  size_t totalBytesSent = 0;
  while (fileToUpload.available()) {
    size_t bytesRead = fileToUpload.read(buffer, BUFFER_SIZE);
    totalBytesSent += bytesRead;
    Serial.print("Writing ");
    Serial.print(totalBytesSent);
    Serial.println(" bytes...");
    percent_sent = (totalBytesSent * 100) / fileToUpload.size();
    display.tft->drawRect(0, (TFT_HEIGHT / 3) * 2, TFT_WIDTH, TFT_HEIGHT, ST77XX_BLACK);
    display.tft->setCursor(0, (TFT_HEIGHT / 3) * 2);
    display_percent = (String)percent_sent + "%";
    display.drawCenteredText(display_percent, false);
    client->write(buffer, bytesRead);
  }

  Logger::log(STD_MSG, "Uploaded file bytes: " + String(totalBytesSent));

  client->print(part2);
  client->print(part3);
  client->flush();

  Serial.println("Finished sending part2 and part3");

  fileToUpload.close();


  // Read response
  String response;
  unsigned long timeout = millis();
  while (millis() - timeout < 5000) {
    while (client->available()) {
      char c = client->read();
      response += c;
    }

    delay(10);
  }

  if (millis() - timeout > 5000)
    Logger::log(WARN_MSG, "Timeout reached");
  if (!client->connected())
    Logger::log(WARN_MSG, "Client disconnected");
      
  client->stop();

  String respTrunc = response.length() > 200 ? response.substring(0, 200) : response;
  Logger::log(STD_MSG, "[WIGLE] Response: " + respTrunc);

  bool ok = response.indexOf("200 OK") >= 0;

  display.clearScreen();
  display.drawCenteredText(ok ? "WIGLE OK" : "WIGLE Failed", true);

  return ok;
}

// ============================================================
// Chunk 3: WDG Wars upload + sidecar tracking system
// ============================================================

// Check whether a service sidecar exists for a given log file.
// service is "wigle" or "wdg".
// filePath should include leading slash, e.g. "/wardrive.log"
bool WiFiOps::sidecarExists(String filePath, String service) {
  return SD.exists(filePath + "." + service);
}

// Write a sidecar file recording the upload timestamp.
void WiFiOps::writeSidecar(String filePath, String service) {
  String sidecarPath = filePath + "." + service;
  File f = SD.open(sidecarPath, FILE_WRITE);
  if (f) {
    f.println("uploaded=" + gps.getDatetime());
    f.close();
    Logger::log(GUD_MSG, "[UPLOAD] Sidecar written: " + sidecarPath);
  } else {
    Logger::log(WARN_MSG, "[UPLOAD] Could not write sidecar: " + sidecarPath);
  }
}

// Upload one log file to WDG Wars.
// Mirrors backendUpload() but uses X-API-Key auth and wdgwars.pl endpoint.
bool WiFiOps::wdgwarsUpload(String filePath) {
  display.clearScreen();
  display.drawCenteredText("WDG Upload...", true);
  delay(100);

  if (!SD.exists(filePath)) {
    display.drawCenteredText(filePath + " not found", true);
    Logger::log(WARN_MSG, "[WDG] File not found: " + filePath);
    return false;
  }

  String apiKey = settings.loadSetting<String>(WDG_KEY_NAME);
  if (apiKey.isEmpty()) {
    display.clearScreen();
    display.drawCenteredText("No WDG API key", true);
    Logger::log(WARN_MSG, "[WDG] No WDG Wars API key configured");
    return false;
  }

  File fileToUpload = SD.open(filePath);
  if (!fileToUpload) {
    display.clearScreen();
    display.drawCenteredText("Could not open file", true);
    Logger::log(WARN_MSG, "[WDG] Could not open: " + filePath);
    return false;
  }

  // Build multipart body
  String boundary   = "----ESP32BOUNDARY";
  String part1      = "--" + boundary + "\r\n";
  part1 += "Content-Disposition: form-data; name=\"file\"; filename=\"" +
           filePath + "\"\r\n";
  part1 += "Content-Type: application/octet-stream\r\n\r\n";
  String part2      = "\r\n--" + boundary + "--\r\n";
  int totalLength   = part1.length() + fileToUpload.size() + part2.length();

  Logger::log(STD_MSG, "[WDG] File size: " + String(fileToUpload.size()));
  Logger::log(STD_MSG, "[WDG] Total length: " + String(totalLength));

  client->setInsecure();
  client->setTimeout(5000);

  if (!client->connect("wdgwars.pl", 443)) {
    fileToUpload.close();
    client->stop();
    display.clearScreen();
    display.drawCenteredText("WDG connect fail", true);
    Logger::log(WARN_MSG, "[WDG] Failed to connect to wdgwars.pl");
    return false;
  }

  // HTTP request
  client->println("POST /api/v2/upload-csv HTTP/1.1");
  client->println("Host: wdgwars.pl");
  client->println("User-Agent: ESP32Uploader/1.0");
  client->println("Accept: application/json");
  client->println("X-API-Key: " + apiKey);
  client->println("Content-Type: multipart/form-data; boundary=" + boundary);
  client->print("Content-Length: ");
  client->println(totalLength);
  client->println();

  // Send body
  client->print(part1);

  const size_t CHUNK = 4096;
  uint8_t buf[CHUNK];
  size_t totalSent = 0;
  uint8_t pct = 0;
  String pctStr;

  while (fileToUpload.available()) {
    size_t n = fileToUpload.read(buf, CHUNK);
    totalSent += n;
    client->write(buf, n);
    pct = (totalSent * 100) / fileToUpload.size();
    display.tft->drawRect(0, (TFT_HEIGHT / 3) * 2, TFT_WIDTH,
                          TFT_HEIGHT - (TFT_HEIGHT / 3) * 2, ST77XX_BLACK);
    display.tft->setCursor(0, (TFT_HEIGHT / 3) * 2);
    pctStr = String(pct) + "%";
    display.drawCenteredText(pctStr, false);
  }

  client->print(part2);
  client->flush();
  fileToUpload.close();

  Logger::log(STD_MSG, "[WDG] Bytes sent: " + String(totalSent));

  // Read response
  String response;
  unsigned long t = millis();
  while (millis() - t < 5000) {
    while (client->available()) {
      char c = client->read();
      response += c;
    }
    if (!client->connected() && !client->available())
      break;

    delay(10);
  }
  client->stop();

  // Capture first 200 chars of response for log viewer
  String respTrunc = response.length() > 200 ? response.substring(0, 200) : response;
  Logger::log(STD_MSG, "[WDG] Response: " + respTrunc);

  // WDG Wars returns 200 on success
  bool ok = response.indexOf("202 Accepted") >= 0 ||
  response.indexOf("\"ok\":true") >= 0;

  display.clearScreen();
  display.drawCenteredText(ok ? "WDG OK" : "WDG Failed", true);
  delay(1000);

  return ok;
}

// Upload a single file to both WiGLE and WDG Wars.
// Skips any service that already has a sidecar, unless retry=true.
// Writes sidecar for each service that succeeds.
// Returns true only if both uploads succeed (or were already done).
bool WiFiOps::uploadFile(String filePath, bool retry, uint8_t upload_type) {
  display.clearScreen();
  display.drawCenteredText("Uploading " + filePath, true);
  delay(1000);
  
  Logger::log(STD_MSG, "[UPLOAD] uploadFile: " + filePath +
              (retry ? " (retry)" : ""));

  bool wigle_already = !retry && this->sidecarExists(filePath, "wigle");
  bool wdg_already   = !retry && this->sidecarExists(filePath, "wdg");

  bool wigle_ok = wigle_already;
  bool wdg_ok   = wdg_already;

  if ((upload_type == WIGLE_UPLOAD) || (upload_type == BOTH_UPLOAD)) {
    if (!wigle_already) {
      Logger::log(STD_MSG, "[UPLOAD] Uploading to WiGLE: " + filePath);
      wigle_ok = this->wigleUpload(filePath);
      if (wigle_ok) {
        this->writeSidecar(filePath, "wigle");
        Logger::log(GUD_MSG, "[UPLOAD] WiGLE upload succeeded");
      } else {
        Logger::log(WARN_MSG, "[UPLOAD] WiGLE upload failed");
      }
    } else {
      Logger::log(STD_MSG, "[UPLOAD] WiGLE already uploaded, skipping");
    }
  }

  if ((upload_type == WDG_UPLOAD) || (upload_type == BOTH_UPLOAD)) {
    if (!wdg_already) {
      Logger::log(STD_MSG, "[UPLOAD] Uploading to WDG Wars: " + filePath);
      wdg_ok = this->wdgwarsUpload(filePath);
      if (wdg_ok) {
        this->writeSidecar(filePath, "wdg");
        Logger::log(GUD_MSG, "[UPLOAD] WDG Wars upload succeeded");
      } else {
        Logger::log(WARN_MSG, "[UPLOAD] WDG Wars upload failed");
      }
    } else {
      Logger::log(STD_MSG, "[UPLOAD] WDG Wars already uploaded, skipping");
    }
  }

  if (upload_type == WIGLE_UPLOAD)
    return wigle_ok;
  else if (upload_type == WDG_UPLOAD)
    return wdg_ok;
  else if (upload_type == BOTH_UPLOAD)
    return wigle_ok && wdg_ok;
  
  return false;
}

// Scan the SD card root for all .log files that are missing at least
// one service sidecar, and upload them. Used by dock mode (Chunk 6).
void WiFiOps::uploadAllPending() {
  Logger::log(STD_MSG, "[UPLOAD] Scanning SD for pending uploads...");

  if (!sd_obj.supported) {
    Logger::log(WARN_MSG, "[UPLOAD] SD not available");
    return;
  }

  File root = SD.open("/");
  if (!root || !root.isDirectory()) {
    Logger::log(WARN_MSG, "[UPLOAD] Could not open SD root");
    return;
  }

  int uploaded = 0;
  int skipped  = 0;
  int failed   = 0;

  File f = root.openNextFile();
  while (f) {
    if (!f.isDirectory()) {
      String name = f.name();
      if (name.endsWith(".log") && name != "debug.log") {
        String path = "/" + name;
        bool wigle_done = this->sidecarExists(path, "wigle");
        bool wdg_done   = this->sidecarExists(path, "wdg");

        if (wigle_done && wdg_done) {
          skipped++;
        } else {
          Logger::log(STD_MSG, "[UPLOAD] Pending: " + path);
          bool wigle_needed = !wigle_done;
          bool wdg_needed   = !wdg_done;
          bool ok = this->uploadFile(path, false, BOTH_UPLOAD);
          // Count as uploaded if all needed services succeeded
          bool wigle_now = this->sidecarExists(path, "wigle");
          bool wdg_now   = this->sidecarExists(path, "wdg");
          bool fully_done = (!wigle_needed || wigle_now) && (!wdg_needed || wdg_now);
          if (fully_done) uploaded++;
          else            failed++;
        }
      }
    }
    f = root.openNextFile();
  }

  Logger::log(GUD_MSG, "[UPLOAD] Done. Uploaded:" + String(uploaded) +
              " Skipped:" + String(skipped) + " Failed:" + String(failed));
}

// ============================================================
// Returns true if auth passes (or no password is set).
// Sends 401 and returns false if auth fails.
// ============================================================
bool WiFiOps::checkAuth() {
  String adminPass = settings.loadSetting<String>(ADMIN_PASS_NAME);
  if (adminPass.isEmpty()) return true; // no password set — open access

  // Expect "Authorization: Basic <base64("admin:<pass>")>"
  String expected = "Basic " + utils.base64Encode("admin:" + adminPass);
  if (server.hasHeader("Authorization") &&
      server.header("Authorization") == expected) {
    return true;
  }

  server.sendHeader("WWW-Authenticate", "Basic realm=\"C5 Wardriver\"");
  server.send(401, "text/plain", "Authentication required");
  return false;
}

void WiFiOps::serveConfigPage() {

  // ---- GET / : main config page ----
  bool cur_dbg_en = settings.loadSetting<bool>(DEBUG_LOG_NAME);
  server.on("/", HTTP_GET, [this, cur_dbg_en]() {
    if (!this->checkAuth()) return;
    this->last_web_client_activity = millis();

    // Load current values to pre-populate the form
    String cur_ssid        = settings.loadSetting<String>("s");
    String cur_wigle_user  = settings.loadSetting<String>("wu");
    String cur_wdg_key     = settings.loadSetting<String>(WDG_KEY_NAME);
    String cur_t_ssid      = settings.loadSetting<String>(TRIGGER_SSID_NAME);
    bool   cur_dbg_en      = settings.loadSetting<bool>(DEBUG_LOG_NAME);
    int    cur_mode        = settings.loadSetting<int>("m");
    bool   cur_enc         = settings.loadSetting<bool>("e");

    String html = "<html><body>";
    html += "<h2>JCMK C5 Wardriver &mdash; Configuration</h2>";
    html += "<form action=\"/save\" method=\"POST\">";

    // ---- Network ----
    html += "<h3>Network</h3>";
    html += "<small>Network to connect at boot for web UI access (e.g. home WiFi) - separate from Trigger SSID</small><br><br>";
    html += "AP SSID: <input type=\"text\" name=\"ssid\" value=\"" + cur_ssid + "\"><br>";
    html += "AP Password: <input type=\"password\" name=\"password\" placeholder=\"leave blank to keep\"><br>";

    // ---- WiGLE ----
    html += "<h3>WiGLE</h3>";
    html += "API Name: <input type=\"text\" name=\"wigle_user\" value=\"" + cur_wigle_user + "\"><br>";
    html += "API Token: <input type=\"password\" name=\"wigle_token\" placeholder=\"leave blank to keep\"><br>";

    // ---- WDG Wars ----
    html += "<h3>WDG Wars</h3>";
    html += "API Key: <input type=\"password\" name=\"wdg_key\" placeholder=\"leave blank to keep\">";
    if (!cur_wdg_key.isEmpty()) html += " <em>(key saved)</em>";
    html += "<br>";

    // ---- Dock Mode ----
    html += "<h3>Dock Mode</h3>";
    html += "<small>The trigger SSID (e.g. K1T or a phone hotspot) causes the wardriver to pause, ";
    html += "connect, and upload all pending logs to WiGLE and WDG Wars.</small><br><br>";
    html += "Trigger SSID: <input type=\"text\" name=\"trigger_ssid\" value=\"" + cur_t_ssid + "\"><br>";
    html += "Trigger Password: <input type=\"password\" name=\"trigger_pass\" placeholder=\"leave blank to keep\"><br>";

    // ---- Admin ----
    html += "<h3>Admin</h3>";
    html += "<small>Setting a password enables Basic Auth on the web UI. ";
    html += "Recommended when using dock mode web server on a shared network.</small><br><br>";
    html += "Admin Password: <input type=\"password\" name=\"admin_pass\" placeholder=\"leave blank to keep current\"><br>";
    html += "<br>SD Debug Log: <input type=\"checkbox\" name=\"dbg_en\" value=\"true\"";
    if (cur_dbg_en) html += " checked";
    html += "> <small>Write all log entries to " + String(DEBUG_LOG_FILE) + " on SD card</small><br>";

    // ---- SSID Exclusions ----
    html += "<h3>SSID Exclusions (up to " + String(MAX_SSID_EXCLUSIONS) + ")</h3>";
    html += "<small>Networks matching these SSIDs will never be logged.</small><br><br>";
    for (int i = 0; i < MAX_SSID_EXCLUSIONS; i++) {
      String val = settings.loadSetting<String>("sx_" + String(i));
      html += "Exclusion " + String(i + 1) + ": <input type=\"text\" name=\"sx_" + String(i) +
              "\" value=\"" + val + "\"><br>";
    }

    // ---- Geofences ----
    html += "<h3>Geofences (up to " + String(MAX_GEOFENCES) + ")</h3>";
    html += "<small>Wardriving pauses while inside any geofenced zone. ";
    html += "If the trigger SSID is visible inside a zone, upload is attempted.</small><br><br>";
    for (int i = 0; i < MAX_GEOFENCES; i++) {
      String geoStr = settings.loadSetting<String>("geo_" + String(i));
      float  gLat = 0.0, gLon = 0.0;
      int    gRad = 0;
      String gLabel = "";

      // Parse stored JSON geo string
      DynamicJsonDocument geoDoc(256);
      if (!geoStr.isEmpty() && deserializeJson(geoDoc, geoStr) == DeserializationError::Ok) {
        gLat   = geoDoc["lat"]   | 0.0f;
        gLon   = geoDoc["lon"]   | 0.0f;
        gRad   = geoDoc["rad"]   | 0;
        gLabel = geoDoc["label"] | "";
      }

      html += "<strong>Zone " + String(i + 1) + "</strong><br>";
      html += "&nbsp;&nbsp;Label: <input type=\"text\" name=\"geo_" + String(i) + "_label\" value=\"" + gLabel + "\"> ";
      float gRadMiles = gRad > 0 ? gRad / 1609.34 : 0.0;
      char gRadMilesStr[10];
      dtostrf(gRadMiles, 4, 2, gRadMilesStr);
      html += "Radius (mi, min 0.10, max 1.00): <input type=\"number\" name=\"geo_" + String(i) + "_rad\" value=\"" + String(gRadMilesStr) + "\" min=\"0.10\" max=\"1.00\" step=\"0.05\" style=\"width:70px\"><br>";

      html += "&nbsp;&nbsp;Lat: <input type=\"text\" name=\"geo_" + String(i) + "_lat\" value=\"" + String(gLat, 6) + "\" style=\"width:110px\"> ";
      html += "Lon: <input type=\"text\" name=\"geo_" + String(i) + "_lon\" value=\"" + String(gLon, 6) + "\" style=\"width:110px\"><br><br>";
    }

    // ---- Device Mode ----
    html += "<h3>Device Mode</h3>";
    html += "<input type=\"radio\" name=\"device_mode\" value=\"solo\"" + String(cur_mode == SOLO_MODE ? " checked" : "") + "> Solo<br>";
    html += "<input type=\"radio\" name=\"device_mode\" value=\"core\"" + String(cur_mode == CORE_MODE ? " checked" : "") + "> Core<br>";
    html += "<input type=\"radio\" name=\"device_mode\" value=\"node\"" + String(cur_mode == NODE_MODE ? " checked" : "") + "> Node<br><br>";

    // ---- Encryption ----
    html += "<h3>Encryption (ESP-NOW)</h3>";
    html += "ENOW Key: <input type=\"text\" name=\"enow_key\" placeholder=\"leave blank to keep\"><br>";
    html += "Use Encryption: <input type=\"checkbox\" name=\"use_encryption\" value=\"true\"";
    if (cur_enc) html += " checked";
    html += "><br><br>";

    html += "<input type=\"submit\" value=\"Save Settings\">";
    html += "</form>";

    // ---- Files on SD Card ----
    html += "<h2>Files on SD Card</h2>";
    File root = SD.open("/");
    if (root && root.isDirectory()) {
      File file = root.openNextFile();
      while (file) {
        if (!file.isDirectory()) {
          String filename = file.name();
          if (filename.endsWith(".log") && filename != "debug.log") {
            // Check sidecar upload status
            bool wigleDone = SD.exists("/" + filename + ".wigle");
            bool wdgDone   = SD.exists("/" + filename + ".wdg");

            html += "<a href=\"/download?file=" + filename + "\">" + filename + "</a> ";
            html += String(file.size()) + " B";

            if (this->connected_as_client) {
              // Upload links with per-service status
              if (!wigleDone)
                html += " | <a href=\"/upload?file=" + filename + "&svc=wigle\">Upload WiGLE</a>";
              else
                html += " | <em>WiGLE&check;</em>";

              if (!wdgDone)
                html += " | <a href=\"/upload?file=" + filename + "&svc=wdg\">Upload WDG</a>";
              else
                html += " | <em>WDG&check;</em>";

              // Retry link if either failed
              if (wigleDone && wdgDone)
                html += " | <a href=\"/upload?file=" + filename + "&svc=both&retry=1\">Retry All</a>";
              else if (!wigleDone || !wdgDone)
                html += " | <a href=\"/upload?file=" + filename + "&svc=both\">Upload Both</a>";
            }
            html += "<br>";
          } else if (!filename.endsWith(".wigle") && !filename.endsWith(".wdg")) {
            // Show non-log, non-sidecar files for download only
            html += "<a href=\"/download?file=" + filename + "\">" + filename + "</a> ";
            html += String(file.size()) + " B<br>";
          }
        }
        file = root.openNextFile();
      }
    } else {
      html += "Unable to access SD card.<br>";
    }

    html += "<br><a href='/log'>&#128196; View Live Log</a>";
    html += "</body></html>";
    server.send(200, "text/html", html);
  });

  // ---- POST /save : save all settings ----
  server.on("/save", HTTP_POST, [this]() {
    if (!this->checkAuth()) return;
    this->last_web_client_activity = millis();

    bool anyChange = false;

    // Network
    if (server.hasArg("ssid") && server.arg("ssid") != "") {
      this->user_ap_ssid = server.arg("ssid");
      settings.saveSetting<bool>("s", this->user_ap_ssid);
      anyChange = true;
    }
    if (server.hasArg("password") && server.arg("password") != "") {
      this->user_ap_password = server.arg("password");
      settings.saveSetting<bool>("p", this->user_ap_password);
      anyChange = true;
    }

    // WiGLE
    if (server.hasArg("wigle_user") && server.arg("wigle_user") != "") {
      this->wigle_user = server.arg("wigle_user");
      settings.saveSetting<bool>("wu", this->wigle_user);
      anyChange = true;
    }
    if (server.hasArg("wigle_token") && server.arg("wigle_token") != "") {
      this->wigle_token = server.arg("wigle_token");
      settings.saveSetting<bool>("wt", this->wigle_token);
      anyChange = true;
    }

    // WDG Wars
    if (server.hasArg("wdg_key") && server.arg("wdg_key") != "") {
      settings.saveSetting<bool>(WDG_KEY_NAME, server.arg("wdg_key"));
      anyChange = true;
    }

    // Dock Mode
    if (server.hasArg("trigger_ssid")) {
      settings.saveSetting<bool>(TRIGGER_SSID_NAME, server.arg("trigger_ssid"));
      anyChange = true;
    }
    if (server.hasArg("trigger_pass") && server.arg("trigger_pass") != "") {
      settings.saveSetting<bool>(TRIGGER_PASS_NAME, server.arg("trigger_pass"));
      anyChange = true;
    }

    // Admin password
    if (server.hasArg("admin_pass") && server.arg("admin_pass") != "") {
      settings.saveSetting<bool>(ADMIN_PASS_NAME, server.arg("admin_pass"));
      anyChange = true;
    }
    bool dbgEn = server.hasArg("dbg_en") && server.arg("dbg_en") == "true";
    settings.saveSetting<bool>(DEBUG_LOG_NAME, dbgEn);
    Logger::enableSDLog(dbgEn);
    anyChange = true;

    // SSID exclusions
    for (int i = 0; i < MAX_SSID_EXCLUSIONS; i++) {
      String key = "sx_" + String(i);
      if (server.hasArg(key)) {
        settings.saveSetting<bool>(key, server.arg(key));
        anyChange = true;
      }
    }

    // Geofences — reconstruct JSON string from individual form fields
    for (int i = 0; i < MAX_GEOFENCES; i++) {
      String latKey   = "geo_" + String(i) + "_lat";
      String lonKey   = "geo_" + String(i) + "_lon";
      String radKey   = "geo_" + String(i) + "_rad";
      String labelKey = "geo_" + String(i) + "_label";

      if (server.hasArg(latKey)) {
        float  lat   = server.arg(latKey).toFloat();
        float  lon   = server.arg(lonKey).toFloat();
        float radMiles = server.arg(radKey).toFloat();
        if (radMiles < 0.10) radMiles = 0.10;   // floor at 0.1 mi
        if (radMiles > 1.00) radMiles = 1.00;   // cap at 1.0 mi
        int rad = (int)(radMiles * 1609.34);     // convert to meters for storage
        String label = server.arg(labelKey);

        DynamicJsonDocument geoDoc(256);
        geoDoc["lat"]   = lat;
        geoDoc["lon"]   = lon;
        geoDoc["rad"]   = rad;
        geoDoc["label"] = label;
        String geoStr;
        serializeJson(geoDoc, geoStr);

        settings.saveSetting<bool>("geo_" + String(i), geoStr);
        anyChange = true;
      }
    }

    // Device mode
    if (server.hasArg("device_mode") && server.arg("device_mode") != "") {
      int mode_arg = SOLO_MODE;
      if (server.arg("device_mode") == "core") mode_arg = CORE_MODE;
      else if (server.arg("device_mode") == "node") mode_arg = NODE_MODE;
      this->run_mode = mode_arg;
      settings.saveSetting<bool>("m", this->run_mode, true);
      anyChange = true;
    }

    // Encryption
    if (server.hasArg("enow_key") && server.arg("enow_key") != "") {
      this->esp_now_key = server.arg("enow_key");
      settings.saveSetting<bool>("ek", this->esp_now_key);
      anyChange = true;
    }
    if (server.hasArg("use_encryption")) {
      this->use_encryption = (server.arg("use_encryption") == "true");
      settings.saveSetting<bool>("e", this->use_encryption);
      anyChange = true;
    }

    if (anyChange)
      Logger::log(GUD_MSG, "Settings saved successfully");
    else
      Logger::log(WARN_MSG, "Save called but no changes detected");

    // Chunk 5: invalidate geofence cache so changes take effect immediately
    this->reloadGeofenceCache();

    server.send(200, "text/html",
      "<html><body><h3>Settings saved.</h3>"
      "<a href=\"/\">Back</a></body></html>");

    this->last_web_client_activity = 0;
    this->shutdownAccessPoint();
  });

  server.on("/download", HTTP_GET, [this]() {
    if (!this->checkAuth()) return;
    this->last_web_client_activity = millis();
    if (!server.hasArg("file")) {
      server.send(400, "text/plain", "Missing file parameter.");
      return;
    }

    String path = server.arg("file");
    Logger::log(GUD_MSG, "User downloading: " + path);
    if (!SD.exists("/" + path)) {
      server.send(404, "text/plain", "File not found.");
      return;
    }

    File downloadFile = SD.open("/" + path, FILE_READ);
    if (downloadFile) {
      server.sendHeader("Content-Disposition", "attachment; filename=\"" + path + "\"");
      server.streamFile(downloadFile, "application/octet-stream");
    }
    else
      Logger::log(GUD_MSG, "Failed to open file: " + path);
    downloadFile.close();
  });

  // ---- GET /upload : per-service upload with sidecar tracking ----
  server.on("/upload", HTTP_GET, [this]() {
    if (!this->checkAuth()) return;
    this->last_web_client_activity = millis();

    if (!server.hasArg("file")) {
      server.send(400, "text/plain", "Missing file parameter.");
      return;
    }

    String filePath = "/" + server.arg("file");
    if (!SD.exists(filePath)) {
      server.send(404, "text/plain", "File not found: " + filePath);
      return;
    }

    // ?svc=wigle|wdg|both  ?retry=1
    String svc   = server.hasArg("svc")   ? server.arg("svc")   : "both";
    bool   retry = server.hasArg("retry") && server.arg("retry") == "1";

    bool wigle_ok = true;
    bool wdg_ok   = true;

    if (svc == "wigle") {
      bool already = !retry && this->sidecarExists(filePath, "wigle");
      if (!already) {
        wigle_ok = this->wigleUpload(filePath);
        if (wigle_ok) this->writeSidecar(filePath, "wigle");
      }
    } else if (svc == "wdg") {
      bool already = !retry && this->sidecarExists(filePath, "wdg");
      if (!already) {
        wdg_ok = this->wdgwarsUpload(filePath);
        if (wdg_ok) this->writeSidecar(filePath, "wdg");
      }
    } else {
      // both
      bool ok = this->uploadFile(filePath, retry, BOTH_UPLOAD);
      wigle_ok = ok;
      wdg_ok   = ok;
    }

    String result = "<html><body><h3>Upload result for " + filePath + "</h3>";
    result += "WiGLE: " + String(wigle_ok ? "&#10003; OK" : "&#10007; Failed") + "<br>";
    result += "WDG Wars: " + String(wdg_ok  ? "&#10003; OK" : "&#10007; Failed") + "<br><br>";
    result += "<a href=\"/\">Back</a></body></html>";

    server.send(200, "text/html", result);
    this->last_web_client_activity = millis();
  });

  // ---- GET /log : live log viewer ----
  server.on("/log", HTTP_GET, [this]() {
    this->last_web_client_activity = millis();

    // Build log output oldest-first from the ring buffer
    String logHtml = "<!DOCTYPE html><html><head>";
    logHtml += "<meta charset='utf-8'>";
    logHtml += "<meta http-equiv='refresh' content='5'>";
    logHtml += "<title>C5 Wardriver Log</title>";
    logHtml += "<style>";
    logHtml += "body{background:#000;color:#0f0;font-family:monospace;font-size:12px;padding:8px;}";
    logHtml += ".warn{color:#ff0;} .good{color:#0f0;} .std{color:#aaa;}";
    logHtml += "a{color:#08f;}";
    logHtml += "</style></head><body>";
    logHtml += "<b>C5 Wardriver — Live Log</b> ";
    logHtml += "<a href='/'>&#8592; Back</a><br>";
    logHtml += "<small>Auto-refreshes every 5s. Showing last " +
    String(Logger::ring_count) + "/" +
    String(LOG_RING_SIZE) + " lines.</small><hr>";

    // Walk ring buffer oldest-first
    int start = (Logger::ring_count < LOG_RING_SIZE)
    ? 0
    : Logger::ring_head;

    for (int i = 0; i < Logger::ring_count; i++) {
      int idx = (start + i) % LOG_RING_SIZE;
      String line = Logger::ring[idx];

      String css = "std";
      if (line.startsWith("[!]")) css = "warn";
      else if (line.startsWith("[+]")) css = "good";

      logHtml += "<span class='" + css + "'>" + line + "</span><br>";
    }

    logHtml += "</body></html>";
    server.send(200, "text/html", logHtml);
  });
  server.begin();
}

bool WiFiOps::monitorAP(unsigned long timeoutMs) {
  unsigned long start = millis();
  while (millis() - start < timeoutMs) {
    this->showCountdown();
    delay(100);
    if (WiFi.softAPgetStationNum() > 0) {
      Logger::log(GUD_MSG, "Client connected.");
      return true;
    }
  }

  Logger::log(STD_MSG, "Timeout reached with no client.");
  shutdownAccessPoint();
  return false;
}

void WiFiOps::shutdownAccessPoint(bool ap_active) {
  if ((ap_active) && (WiFi.status() != WL_CONNECTED))
    Logger::log(STD_MSG, "Shutting down Access Point and Web Server...");
  else
    Logger::log(STD_MSG, "Shutting down Web Server...");

  server.stop();
  if (ap_active) {
    if (WiFi.status() != WL_CONNECTED)
      WiFi.softAPdisconnect(true);  // true = wipe SSID/password
  }
  delay(100);  // small delay for stability

  this->serving = false;
}

void WiFiOps::showCountdown() {
  if (millis() - this->last_timer > TIMER_UPDATE) {
    this->last_timer = millis();
    display.tft->fillRect(0, SMALL_CHAR_HEIGHT * 2, TFT_WIDTH, TFT_HEIGHT - (SMALL_CHAR_HEIGHT * 2), ST77XX_BLACK);
    display.tft->setCursor(0, SMALL_CHAR_HEIGHT * 4);
    display.tft->println("Wardring starts...\n");
    display.tft->setTextSize(2);
    display.tft->println(60 - ((millis() - this->last_web_client_activity) / 1000));
    display.tft->setTextSize(1);
  }
}

bool WiFiOps::begin(bool skip_admin) {
  this->current_scan_mode = WIFI_STANDBY;

  this->run_mode = settings.loadSetting<int>("m");

  if (!skip_admin) {
    // Init WiFi
    this->initWiFi();

    // Run Admin stuff and wait for clients first
    bool connected = this->tryConnectToWiFi();

    this->connected_as_client = connected;

    if (!connected) {
      delay(1000);
      this->startAccessPoint();
    }

    /*if (connected) {
      Logger::log(STD_MSG, "Attempting upload...");
      this->backendUpload("/wardrive_0.log");
    }*/

    this->serveConfigPage();

    this->serving = true;

    this->last_web_client_activity = millis();

    // Run AP loop
    if (!connected) {
      if (this->monitorAP()) {
        while (true) {
          server.handleClient();

          static bool wasConnected = WiFi.softAPgetStationNum() > 0;
          bool nowConnected = WiFi.softAPgetStationNum() > 0;

          if (wasConnected && !nowConnected) {
            Logger::log(STD_MSG, "Client disconnected.");
            this->shutdownAccessPoint();
            break;
          }

          wasConnected = nowConnected;
        }
      }
    } else { // Or run web server loop
      this->last_timer = millis();

      while (this->serving) {
        server.handleClient();
        battery.main(millis());

        // Stay connected until K1T actually disappears — no inactivity
        // timeout when connected as a client to a known WLAN.
        if (WiFi.status() != WL_CONNECTED) {
          Logger::log(WARN_MSG, "WiFi connection lost — resuming wardriving...");
          this->shutdownAccessPoint(false);
          break;
        }
      }
    }

    this->connected_as_client = false;

    this->deinitWiFi();
  }

  // Sanity check for modes and keys
  if (this->esp_now_key != "") {
    if (!settings.saveSetting<bool>("ek", this->esp_now_key))
      Logger::log(WARN_MSG, "Failed to save setting");
  }
  else
    this->esp_now_key = settings.loadSetting<String>("ek");

  //this->run_mode = settings.loadSetting<int>("m");
  this->use_encryption = settings.loadSetting<bool>("e");

  Logger::log(STD_MSG, "ENOW Key: " + this->esp_now_key);

  if (this->run_mode == SOLO_MODE)
    Logger::log(STD_MSG, "Mode: SOLO");
  if (this->run_mode == NODE_MODE)
    Logger::log(STD_MSG, "Mode: NODE");
  if (this->run_mode == CORE_MODE)
    Logger::log(STD_MSG, "Mode: CORE");

  if (this->use_encryption)
    Logger::log(STD_MSG, "Encryption: Enabled");
  else
    Logger::log(STD_MSG, "Encryption: Disabled");

  this->initWiFi();

  // Init NimBLE
  this->initBLE(); // NimBLE needs to not be init in order to upload to wigle

  if (this->run_mode != SOLO_MODE)
    this->startESPNow();

  if ((this->run_mode == SOLO_MODE) || (this->run_mode == CORE_MODE))
    startLog(LOG_FILE_NAME);

  // Random delay for nodes to stagger channels
  if (this->run_mode == NODE_MODE) {
    delay(1000);
    this->runAdminWindowAfterScanCycle();
    delay(random(100, 5000));
  }

  this->init_time = millis();

  // Chunk 5: load geofence cache now that settings are fully initialised
  this->loadGeofenceCache();

  // Force Screen 1 redraw to clear any AP/admin phase display remnants
  g_force_display_redraw = true;

  return true;
}

// ============================================================
// Chunk 6: Dock mode state machine
// ============================================================

// Synchronous passive scan for the configured trigger SSID.
// Safe to call while in promiscuous mode (cancels ongoing async scan first).
// Returns true if the trigger SSID is visible.
bool WiFiOps::scanForTriggerSSID() {
  String trigSSID = settings.loadSetting<String>(TRIGGER_SSID_NAME);
  if (trigSSID.isEmpty()) return false;

  // Don't interrupt a running async scan — skip this cycle
  if (WiFi.scanComplete() == WIFI_SCAN_RUNNING) return false;
  WiFi.scanDelete();

  //Logger::log(STD_MSG, "Scanning for trigger networks...");
  uint32_t start_time = millis();
  int n = WiFi.scanNetworks(false, true, false, 100); // synchronous, include hidden
  //Logger::log(STD_MSG, "Scan completed in " + String(millis() - start_time) + "ms");

  bool found = false;
  for (int i = 0; i < n; i++) {
    if (WiFi.SSID(i) == trigSSID) { found = true; break; }
  }
  WiFi.scanDelete();
  return found;
}

// Dispatcher — called from main() when dock_state != DOCK_STATE_NONE.
void WiFiOps::runDockMode(uint32_t currentTime) {
  switch (this->dock_state) {
    case DOCK_STATE_CONNECTING:
      this->handleDockConnecting();
      break;
    case DOCK_STATE_UPLOADING:
      this->handleDockUploading();
      break;
    case DOCK_STATE_MONITORING:
      this->handleDockMonitoring(currentTime);
      break;
    case DOCK_STATE_FAILED:
      // Wait for failure display timeout then resume wardriving
      if (currentTime - this->dock_fail_time >= DOCK_FAIL_DISPLAY_MS) {
        Logger::log(STD_MSG, "[DOCK] Failure display expired — resuming wardrive");
        this->dock_state            = DOCK_STATE_NONE;
        this->dock_connect_attempts = 0;
        this->initWiFi();
        this->initBLE();
        display.clearScreen();
      }
      break;
    default:
      this->dock_state = DOCK_STATE_NONE;
      break;
  }
}

// One blocking connection attempt to the trigger SSID.
// Retries up to DOCK_CONNECT_ATTEMPTS times across successive main() cycles.
void WiFiOps::handleDockConnecting() {
  String trigSSID = settings.loadSetting<String>(TRIGGER_SSID_NAME);
  String trigPass = settings.loadSetting<String>(TRIGGER_PASS_NAME);

  Logger::log(STD_MSG, "[DOCK] Connect attempt " +
              String(this->dock_connect_attempts + 1) + "/" +
              String(DOCK_CONNECT_ATTEMPTS) + " to: " + trigSSID);

  display.clearScreen();
  display.tft->setCursor(0, 0);
  display.tft->setTextSize(2);
  display.tft->setTextColor(ST77XX_CYAN, ST77XX_BLACK);
  display.tft->println("DOCK MODE");
  display.tft->setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  display.tft->println("Connecting");
  display.tft->println(trigSSID);
  display.tft->setTextSize(1);
  display.tft->println("Attempt " +
  String(this->dock_connect_attempts + 1) + "/3");

  // Switch WiFi from scan/promiscuous to STA client
  this->deinitBLE();
  this->deinitWiFi();
  this->initWiFi();

  WiFi.mode(WIFI_STA);
  WiFi.begin(trigSSID.c_str(), trigPass.c_str());

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED &&
         millis() - start < DOCK_CONNECT_TIMEOUT) {
    delay(500);
  }

  if (WiFi.status() == WL_CONNECTED) {
    this->dock_ip = WiFi.localIP().toString();
    Logger::log(GUD_MSG, "[DOCK] Connected! IP: " + this->dock_ip);

    // Show dock banner on TFT
    display.clearScreen();
    display.tft->setCursor(0, 0);
    display.tft->setTextColor(ST77XX_GREEN, ST77XX_BLACK);
    display.tft->println("DOCKED");
    display.tft->setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    display.tft->println(trigSSID);
    display.tft->setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
    display.tft->println(this->dock_ip);
    display.tft->setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    display.tft->println("browse to config");

    // Start web server so you can access config/files while docked
    this->serveConfigPage();
    this->serving = true;
    this->last_web_client_activity = millis();

    if (this->dock_webui_only) {
      // Tier 1: no GPS fix — serve web UI only, skip upload
      Logger::log(STD_MSG, "[DOCK] Tier 1 — web UI only (no GPS fix)");
      display.tft->setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
      display.tft->println("No GPS - web UI only");
      this->dock_state = DOCK_STATE_MONITORING;
    } else {
      // Tier 2: GPS fix — full dock mode
      this->dock_state = DOCK_STATE_UPLOADING;
    }

  } else {
    WiFi.disconnect(true);
    this->dock_connect_attempts++;
    Logger::log(WARN_MSG, "[DOCK] Attempt " +
                String(this->dock_connect_attempts) + " failed");

    if (this->dock_connect_attempts >= DOCK_CONNECT_ATTEMPTS) {
      Logger::log(WARN_MSG, "[DOCK] All attempts exhausted — resuming wardrive in " +
                  String(DOCK_FAIL_DISPLAY_MS / 1000) + "s");

      display.clearScreen();
      display.tft->setCursor(0, 0);
      display.tft->setTextColor(ST77XX_RED, ST77XX_BLACK);
      display.tft->println("DOCK FAILED");
      display.tft->setTextColor(ST77XX_WHITE, ST77XX_BLACK);
      display.tft->println(trigSSID);
      display.tft->println("Resuming in");
      display.tft->println(String(DOCK_FAIL_DISPLAY_MS / 1000) + "s");

      this->dock_state    = DOCK_STATE_FAILED;
      this->dock_fail_time = millis();
    }
    // else: stay in DOCK_STATE_CONNECTING — next main() cycle retries
  }
}

// Run once after successful connection: upload all pending files
// then transition to monitoring.
void WiFiOps::handleDockUploading() {
  Logger::log(STD_MSG, "[DOCK] Starting bulk upload of pending files");

  display.clearScreen();
  display.tft->setCursor(0, 0);
  display.tft->setTextColor(ST77XX_CYAN, ST77XX_BLACK);
  display.tft->println("UPLOADING");
  display.tft->setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  display.tft->println(this->dock_ip);

  this->uploadAllPending(); // Chunk 3

  Logger::log(GUD_MSG, "[DOCK] Upload complete — monitoring for departure");

  String trigSSID = settings.loadSetting<String>(TRIGGER_SSID_NAME);

  display.clearScreen();
  display.tft->setCursor(0, 0);
  display.tft->setTextColor(ST77XX_GREEN, ST77XX_BLACK);
  display.tft->println("SYNCED");
  display.tft->setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  display.tft->println(this->dock_ip);
  display.tft->println("Watching for");
  display.tft->println("departure...");

  this->dock_last_scan_time = millis();
  this->dock_depart_count   = 0;
  this->dock_state          = DOCK_STATE_MONITORING;
}

// Called every main() cycle while docked.
// Services web server and runs passive scan every DOCK_SCAN_INTERVAL.
// Departs when trigger SSID is absent for DOCK_DEPART_SCANS consecutive scans.
void WiFiOps::handleDockMonitoring(uint32_t currentTime) {
  // Service any web browser clients
  server.handleClient();
  // Keep battery main running so chargeRate sampling fires
  battery.main(currentTime);

  // Tier 1 → Tier 2 upgrade: GPS fix acquired while in web-UI-only mode
  if (this->dock_webui_only && gps.getFixStatus() && sd_obj.supported) {
    Logger::log(GUD_MSG, "[DOCK] GPS fix acquired — upgrading Tier 1 -> Tier 2");
    this->dock_webui_only = false;
    this->dock_depart_time = millis();
    this->handleDockUploading();
    return;
  }

  if (currentTime - this->dock_last_scan_time < DOCK_SCAN_INTERVAL)
    return; // not time yet

  this->dock_last_scan_time = currentTime;
  Logger::log(STD_MSG, "[DOCK] Passive scan — checking for trigger SSID");

  bool found = this->scanForTriggerSSID();

  if (found) {
    Logger::log(STD_MSG, "[DOCK] Trigger SSID still visible — staying docked");
    this->dock_depart_count = 0;

    display.clearScreen();
    display.tft->setCursor(0, 0);
    display.tft->setTextColor(ST77XX_GREEN, ST77XX_BLACK);
    display.tft->println("DOCKED");
    display.tft->setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    display.tft->println(this->dock_ip);
    display.tft->println("Monitoring...");

  } else {
    this->dock_depart_count++;
    Logger::log(STD_MSG, "[DOCK] Trigger SSID absent (" +
                String(this->dock_depart_count) + "/" +
                String(DOCK_DEPART_SCANS) + ")");

    if (this->dock_depart_count >= DOCK_DEPART_SCANS)
      this->departDock();
  }
}

// Clean teardown: stop web server, disconnect WiFi,
// reinitialise for wardriving, start a fresh log file.
void WiFiOps::departDock() {
  Logger::log(STD_MSG, "[DOCK] Departing — tearing down dock mode");

  // Stop web server (keep STA mode, AP was never started)
  this->shutdownAccessPoint(false);

  // Disconnect from trigger SSID
  WiFi.disconnect(true);
  delay(100);

  // Reinitialise WiFi and BLE for wardriving
  this->deinitWiFi();
  this->initWiFi();
  this->initBLE();

  // Fresh log file so post-dock drive gets its own file
  this->startLog(LOG_FILE_NAME);

  // Reset all dock state
  this->dock_state            = DOCK_STATE_NONE;
  this->dock_connect_attempts = 0;
  this->dock_depart_count     = 0;
  this->dock_ip               = "";
  this->dock_webui_only       = false;
  g_force_display_redraw = true;

  Logger::log(GUD_MSG, "[DOCK] Departed — wardriving resumed");

  display.clearScreen();
  display.tft->setCursor(0, 0);
  display.tft->setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  display.tft->println("WARDRIVE");
  display.tft->println("RESUMED");
}

// ============================================================

void WiFiOps::main(uint32_t currentTime, bool in_sd_files) {
  // Chunk 6: dock mode takes priority over normal wardrive cycle
  if (this->dock_state != DOCK_STATE_NONE) {
    this->runDockMode(currentTime);
    return;
  }

  if (this->current_scan_mode == WIFI_WARDRIVING)
    this->runWardrive(currentTime);

  if ((this->run_mode == NODE_MODE) && (!g_have_core) && (this->use_encryption)) {
    if (currentTime - g_last_req_ms >= g_req_interval_ms) {
      g_last_req_ms = currentTime;
      this->sendCoreRequest();

      // simple backoff: double up to max
      uint32_t nextInterval = g_req_interval_ms * 2;
      g_req_interval_ms = (nextInterval > REQ_MAX_MS) ? REQ_MAX_MS : nextInterval;
    }
    return;
  }
  // Chunk 6: Standby K1T detection — runs even without GPS fix.
  // Tier 1 (no GPS): connect + web UI only.
  // Tier 2 (GPS fix): full dock mode with upload.
  if (this->current_scan_mode == WIFI_STANDBY &&
    this->dock_state == DOCK_STATE_NONE &&
    this->run_mode == SOLO_MODE &&
    !in_sd_files) {
    String trigSSID = settings.loadSetting<String>(TRIGGER_SSID_NAME);
    if (!trigSSID.isEmpty() &&
      currentTime - this->standby_scan_time >= STANDBY_SCAN_INTERVAL &&
      currentTime - this->dock_depart_time >= 60000) { // 60s cooldown after departing
      this->standby_scan_time = currentTime;
      //Logger::log(STD_MSG, "Calling scanForTriggerSSID from main");
      if (this->scanForTriggerSSID()) {
        Logger::log(STD_MSG, "[DOCK] Trigger SSID found in standby: " + trigSSID);
        this->dock_webui_only       = !gps.getFixStatus();
        this->dock_state            = DOCK_STATE_CONNECTING;
        this->dock_connect_attempts = 0;
      }
    }
  }

  if (this->run_mode == CORE_MODE) {
    if (currentTime - g_last_debug_print >= DEBUG_OUTPUT_DELAY) {
      g_last_debug_print = currentTime;
      Logger::log(STD_MSG, "Timed Node Table Output: ");
      this->debugPrintNodeTable();
    }
    if (this->removeStaleNodes()) {
      this->handleNodeTopologyChange();
    }
  }
}
