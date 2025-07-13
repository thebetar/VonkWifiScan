#include <limits.h>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <WiFi.h>
#include <esp_wifi.h>

// WiFi channel range (1-14 for 2.4GHz)
#define MIN_CHANNEL 1
#define MAX_CHANNEL 14

#define DIN_PIN 7
#define CLK_PIN 6
#define CS_PIN 10

#define HARDWARE_TYPE MD_MAX72XX::FC16_HW  // Most common 8x8 MAX7219 module

// Packet counters for each channel
uint32_t packet_count[MAX_CHANNEL + 1] = {0};
uint32_t last_packet_count[MAX_CHANNEL + 1] = {0};


// Timing variables
uint16_t channel_switch_interval = 1000;
uint16_t result_show_time = 3000;

// Current channel being monitored
uint8_t current_channel = MIN_CHANNEL - 1;

struct channel_scan_result_t {
  uint8_t least_busy_channel;
  uint8_t busiest_channel;    
};

struct channel_scan_result_t channel_scan_result;

// Initialise MD display for result showing
MD_Parola display = MD_Parola(HARDWARE_TYPE, DIN_PIN, CLK_PIN, CS_PIN, 1);

void setup()
{
  display.begin();
  display.setIntensity(3);

  Serial.begin(9600);

  // Wait for serial connection
  while (!Serial)
  {
    delay(100);
  }

  // Initialize WiFi in station mode
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  // Enable promiscuous mode
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(&promiscuousCallback);

  // Set initial channel
  esp_wifi_set_channel(current_channel, WIFI_SECOND_CHAN_NONE);

  // Set starting channels
  channel_scan_result.least_busy_channel = 0;
  channel_scan_result.busiest_channel = 0;

  Serial.println("Promiscuous mode enabled");
  Serial.println("Starting channel scan...");
  Serial.println();
}

// Promiscuous mode callback function
void promiscuousCallback(void *buf, wifi_promiscuous_pkt_type_t type)
{
  if (type == WIFI_PKT_MGMT || type == WIFI_PKT_DATA || type == WIFI_PKT_CTRL)
  {
    wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buf;
    uint8_t channel = pkt->rx_ctrl.channel;

    if (channel >= MIN_CHANNEL && channel <= MAX_CHANNEL)
    {
      packet_count[channel]++;
    }
  }
}

unsigned long last_channel_switch = 0;
unsigned long last_report_time = 0;
bool min_max_display_toggle = 0;

void loop() {
  unsigned long now = millis();

  // Always update display to keep it refreshed
  display.displayAnimate();

  // Switch channel every channel_switch_interval (e.g. 1 sec)
  if (now - last_channel_switch < channel_switch_interval) {
    return;
  }

  last_channel_switch = now;

  current_channel++;

  if (current_channel > MAX_CHANNEL) {
    // Time to report and reset channel
    reportChannelOccupancy();

    current_channel = MIN_CHANNEL;
    last_report_time = now;
    min_max_display_toggle = 0;

    display_min_result();
  }

  if (now - last_report_time >= result_show_time) {
    if (min_max_display_toggle == 0) {
      last_report_time = now;

      display_max_result();
      min_max_display_toggle = 1;
    } else {
      display_loading();
    }
  }

  esp_wifi_set_channel(current_channel, WIFI_SECOND_CHAN_NONE);
  Serial.print("Scanning channel: ");
  Serial.println(current_channel);
}

void reportChannelOccupancy()
{
  Serial.println("\n=====================================");
  Serial.println("Channel | Packets | Activity | Status");
  Serial.println("--------|---------|----------|--------");

  uint32_t total_packets = 0;
  uint32_t current_total_packets = 0;
  uint8_t active_channels = 0;

  for (uint8_t ch = MIN_CHANNEL; ch <= MAX_CHANNEL; ch++)
  {
    uint32_t current_packets = packet_count[ch];
    uint32_t new_packets = current_packets - last_packet_count[ch];
    last_packet_count[ch] = current_packets;

    total_packets += current_packets;
    current_total_packets += new_packets;

    String status = "Idle";
    String activity = "Low";

    if (new_packets > 0)
    {
      active_channels++;

      if (new_packets > 100)
      {
        status = "Very Busy";
        activity = "High";
      }
      else if (new_packets > 50)
      {
        status = "Busy";
        activity = "Medium";
      }
      else if (new_packets > 10)
      {
        status = "Active";
        activity = "Medium";
      }
      else
      {
        status = "Light";
        activity = "Low";
      }
    }

    Serial.printf("   %2d   | %7lu | %8s | %s\n",
                  ch, current_packets, activity.c_str(), status.c_str());
  }

  channel_scan_result = find_min_max_channels();

  Serial.println("=====================================");
  Serial.printf("Total packets detected: %lu\n", total_packets);
  Serial.printf("Current packets detected: %lu\n", current_total_packets);
  Serial.printf("Active channels: %d/%d\n", active_channels, MAX_CHANNEL);
  Serial.printf("Least busy channel: %d\n", channel_scan_result.least_busy_channel);
  Serial.printf("Most busy channel: %d\n", channel_scan_result.busiest_channel);
  Serial.println("=====================================\n");
}

struct channel_scan_result_t find_min_max_channels()
{
  uint8_t least_busy_channel = MIN_CHANNEL;
  uint8_t busiest_channel = MIN_CHANNEL;
  uint32_t min_packets = UINT_MAX;
  uint32_t max_packets = 0;

  for (uint8_t ch = MIN_CHANNEL; ch <= MAX_CHANNEL; ch++)
  {
    if (packet_count[ch] > max_packets)
    {
      max_packets = packet_count[ch];
      busiest_channel = ch;
    }

    if (packet_count[ch] < min_packets) {
      min_packets = packet_count[ch];
      least_busy_channel = ch;
    }
  }

  struct channel_scan_result_t current_channel_scan_result;

  current_channel_scan_result.least_busy_channel = least_busy_channel;
  current_channel_scan_result.busiest_channel = busiest_channel;

  return current_channel_scan_result;
}

uint8_t loading_step = 1;

void display_loading() {
  display.displayClear();

  char buffer[3];

  loading_step += 1;

  if (loading_step == 1) {
    display.setZoneEffect(0, 0, PA_FLIP_UD);
    sprintf(buffer, ". ");
  } else if (loading_step == 2) {
    display.setZoneEffect(0, 1, PA_FLIP_UD);
    sprintf(buffer, ". ");
  } else if (loading_step == 3) {
    display.setZoneEffect(0, 1, PA_FLIP_UD);
    sprintf(buffer, " .");
  } else {
    display.setZoneEffect(0, 0, PA_FLIP_UD);
    sprintf(buffer, " .");
    loading_step = 0;
  }

  // Display the number centered, no scrolling, just print once
  display.displayText(buffer, PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
  display.displayAnimate();
}

void display_min_result() {
  display.displayClear();

  char buffer[3];  // enough for "14" + null terminator

  display.setZoneEffect(0, 0, PA_FLIP_UD);
  sprintf(buffer, "%u", channel_scan_result.least_busy_channel);
  

  // Display the number centered, no scrolling, just print once
  display.displayText(buffer, PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
  display.displayAnimate();

  Serial.printf("Displayed least busy channel: %d\n", channel_scan_result.least_busy_channel);
}

void display_max_result() {
  display.displayClear();

  char buffer[3];  // enough for "14" + null terminator

  display.setZoneEffect(0, 0, PA_FLIP_UD);
  sprintf(buffer, "%u", channel_scan_result.busiest_channel);
  

  // Display the number centered, no scrolling, just print once
  display.displayText(buffer, PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
  display.displayAnimate();

  Serial.printf("Displayed busiest channel: %d\n", channel_scan_result.busiest_channel);
}
