#include <WiFi.h>
#include <esp_wifi.h>

// WiFi channel range (1-14 for 2.4GHz)
#define MAX_CHANNEL 14
#define MIN_CHANNEL 1

// Packet counters for each channel
uint32_t packetCount[MAX_CHANNEL + 1] = {0};
uint32_t lastPacketCount[MAX_CHANNEL + 1] = {0};

// Current channel being monitored
uint8_t currentChannel = MIN_CHANNEL - 1;

// Timing variables
unsigned long lastChannelSwitch = 0;
unsigned long channelSwitchInterval = 1000; // 1 second per channel
unsigned long lastReport = 0;
unsigned long reportInterval = 15000; // Report every 15 seconds

// Promiscuous mode callback function
void promiscuousCallback(void *buf, wifi_promiscuous_pkt_type_t type)
{
  if (type == WIFI_PKT_MGMT || type == WIFI_PKT_DATA || type == WIFI_PKT_CTRL)
  {
    wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buf;
    uint8_t channel = pkt->rx_ctrl.channel;

    if (channel >= MIN_CHANNEL && channel <= MAX_CHANNEL)
    {
      packetCount[channel]++;
    }
  }
}

void setup()
{
  Serial.begin(115200);

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
  esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);

  Serial.println("Promiscuous mode enabled");
  Serial.println("Starting channel scan...");
  Serial.println();

  lastChannelSwitch = millis();
  lastReport = millis();
}

void loop()
{
  // Go to next channel
  currentChannel++;

  // If channel exceeds MAX_CHANNEL, wrap around to MIN_CHANNEL
  if (currentChannel > MAX_CHANNEL)
  {
    Serial.println("Finished scanning, showing report...");

    reportChannelOccupancy();

    currentChannel = MIN_CHANNEL;
    Serial.println("Reset channel and waiting 30 seconds...");
    delay(30000);
  }

  // Set current channel to scan
  esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);

  // Print current channel to Serial
  Serial.print("Scanning channel: ");
  Serial.println(currentChannel);

  delay(1000);
}

void reportChannelOccupancy()
{
  Serial.println("\n=====================================");
  Serial.println("Channel | Packets | Activity | Status");
  Serial.println("--------|---------|----------|--------");

  uint32_t totalPackets = 0;
  uint8_t activeChannels = 0;

  for (uint8_t ch = MIN_CHANNEL; ch <= MAX_CHANNEL; ch++)
  {
    uint32_t currentPackets = packetCount[ch];
    uint32_t newPackets = currentPackets - lastPacketCount[ch];
    lastPacketCount[ch] = currentPackets;

    totalPackets += currentPackets;

    String status = "Idle";
    String activity = "Low";

    if (newPackets > 0)
    {
      activeChannels++;

      if (newPackets > 100)
      {
        status = "Very Busy";
        activity = "High";
      }
      else if (newPackets > 50)
      {
        status = "Busy";
        activity = "Medium";
      }
      else if (newPackets > 10)
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
                  ch, currentPackets, activity.c_str(), status.c_str());
  }

  Serial.println("=====================================");
  Serial.printf("Total packets detected: %lu\n", totalPackets);
  Serial.printf("Active channels: %d/%d\n", activeChannels, MAX_CHANNEL);
  Serial.printf("Most busy channel: %d\n", findBusiestChannel());
  Serial.println("=====================================\n");
}

uint8_t findBusiestChannel()
{
  uint8_t busiestChannel = MIN_CHANNEL;
  uint32_t maxPackets = 0;

  for (uint8_t ch = MIN_CHANNEL; ch <= MAX_CHANNEL; ch++)
  {
    if (packetCount[ch] > maxPackets)
    {
      maxPackets = packetCount[ch];
      busiestChannel = ch;
    }
  }

  return busiestChannel;
}