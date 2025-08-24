#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

// Pins for components
#define PHOTORESISTOR_PIN A0           // Analog pin for light sensor
#define LED_INDICATOR 2                // On-board LED for signal indication
#define LED_MASTER 16                  // On-board LED to indicate Master status

// WiFi credentials and UDP communication settings
const char* ssid = "PHL";              // Network SSID
const char* password = "Lanlanlan";    // Network password
const unsigned int localPort = 4210;   // Port for receiving UDP messages
const IPAddress broadcastIP(255, 255, 255, 255); // Broadcast IP for UDP communication
WiFiUDP udp;                           // UDP instance for network communication

// Communication timing parameters
unsigned long lastReceivedTime = 0;    // Tracks time of the last received message
const unsigned long silentTime = 200;  // Broadcast if no message is received within 200 ms

// Device-specific variables
int swarmID = -1;                      // Unique ID based on IP address
int analogValue = 0;                   // Stores light sensor reading
bool isMaster = true;                  // Status flag for Master device
String role = "Master";                // Role of this ESP8266 (Master or Slave)

// Array to store readings from other devices by Swarm ID
int readings[10] = {-1};               // Initialize readings array with invalid (-1) values

// Delimiters for packet structure
const String ESP_startBit = "~~~";     // Start delimiter for ESP messages
const String ESP_endBit = "---";       // End delimiter for ESP messages
const String RPi_startBit = "+++";     // Start delimiter for RPi messages
const String RPi_endBit = "***";       // End delimiter for RPi messages

// LED flashing interval parameters
int Y_threshold = 24;                  // Minimum light threshold for flashing
int Y_interval = 2010;                 // Max interval for flashing based on light intensity
int Z_threshold = 1024;                // Maximum threshold for light sensor reading
int Z_interval = 10;                   // Minimum interval for flashing based on light intensity
int slope, intercept;                  // Slope and intercept for calculating LED intervals

// LED states and timing
bool ledIndicatorState = LOW;
unsigned long ledIndicatorPreviousMillis = 0;
bool ledMasterState = LOW;
unsigned long ledMasterPreviousMillis = 0;

// Function to calculate slope and intercept for linear LED flashing rate
void getSlopeIntercept(int x1, int y1, int x2, int y2, int *a, int *b) {
  /* Input:
     - x1, y1: First point for linear relationship (Y_threshold, Y_interval)
     - x2, y2: Second point for linear relationship (Z_threshold, Z_interval)
     - a, b: Pointers to store the calculated slope and intercept
  
     Process: Calculates slope and intercept using two-point formula to create a linear mapping 
              between sensor readings and LED flash intervals.
              
     Output: Updated values of 'a' and 'b' for slope and intercept.
  */
  *a = (y2 - y1) / (x2 - x1);
  *b = y1 - (*a) * x1;
}

// Function to flash indicator LED based on sensor reading
void ledIndicatorFlash(int analog_value) {
  /* Input:
     - analog_value: Light sensor reading, determines flashing rate.
  
     Process: Calculates the flash interval based on the reading, then toggles LED state if the
              required time has elapsed since last toggle.
              
     Output: Updates the LED indicator to flash at an interval corresponding to light intensity.
  */
  int ledInterval = slope * analog_value + intercept;
  unsigned long currentMillis = millis();
  if (currentMillis - ledIndicatorPreviousMillis >= ledInterval) {
    ledIndicatorPreviousMillis = currentMillis;
    ledIndicatorState = !ledIndicatorState;
    digitalWrite(LED_INDICATOR, ledIndicatorState);
  }
}

// Function to flash Master status LED if this device is the Master
void ledMasterFlash(int analog_value) {
  /* Input:
     - analog_value: Light sensor reading, determines flashing rate if device is Master.
  
     Process: Only if device is Master, calculates the flash interval based on the reading,
              then toggles Master LED state if the required time has elapsed.
              
     Output: Updates the Master LED to flash at an interval corresponding to light intensity.
  */
  int ledInterval = slope * analog_value + intercept;
  unsigned long currentMillis = millis();
  if (currentMillis - ledMasterPreviousMillis >= ledInterval) {
    ledMasterPreviousMillis = currentMillis;
    ledMasterState = !ledMasterState;
    digitalWrite(LED_MASTER, ledMasterState);
  }
}

// Setup function to initialize WiFi, LEDs, and calculate slope-intercept for LED intervals
void setup() {
  /* Input: None
  
     Process: Initializes Serial communication, WiFi connection, LED pin modes, and 
              UDP communication. Assigns a unique swarmID based on the last digit of IP.
              Also calculates slope and intercept for flashing intervals.
              
     Output: Sets up the device with connected WiFi, UDP, and initialized LEDs.
  */
  Serial.begin(115200);

  // Initialize LED pins
  pinMode(LED_INDICATOR, OUTPUT);
  pinMode(LED_MASTER, OUTPUT);
  digitalWrite(LED_INDICATOR, HIGH);
  digitalWrite(LED_MASTER, HIGH);

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Assign a swarm ID based on the last digit of IP address
  IPAddress ip = WiFi.localIP();
  swarmID = ip[3] % 10;
  Serial.print("Swarm ID assigned: ");
  Serial.println(swarmID);

  // Begin UDP communication
  udp.begin(localPort);
  Serial.printf("Listening on UDP port %d\n", localPort);

  // Calculate LED flashing rate parameters
  getSlopeIntercept(Y_threshold, Y_interval, Z_threshold, Z_interval, &slope, &intercept);
}

// Main loop to handle flashing LEDs, broadcasting, and receiving messages
void loop() {
  /* Input: None
  
     Process: Flashes LEDs based on role and sensor readings, broadcasts sensor reading if no 
              message has been received within silent time, updates Master status based on 
              highest reading among devices.
              
     Output: LED flashing, sensor reading broadcasts, Master determination.
  */
  // Flash the indicator LED based on current sensor reading
  ledIndicatorFlash(analogValue);
  
  // Flash the Master LED if this device is the Master
  if (isMaster) {
    ledMasterFlash(analogValue);
  } else {
    digitalWrite(LED_MASTER, HIGH);  // Turn off Master LED if not Master
  }

  // Check if any packet has been received
  int packetSize = udp.parsePacket();
  if (packetSize) {
    char incomingPacket[255];
    int len = udp.read(incomingPacket, 255);
    if (len > 0) {
      incomingPacket[len] = '\0';
    }

    // Convert incoming packet to String for easier manipulation
    String packetStr = String(incomingPacket);

    // Process ESP-to-ESP message format
    if (packetStr.startsWith(ESP_startBit) && packetStr.endsWith(ESP_endBit)) {
      String data = packetStr.substring(ESP_startBit.length(), packetStr.length() - ESP_endBit.length());
      Serial.printf("Received packet: %s\n", data);
      int receivedSwarmID, receivedReading;
      sscanf(data.c_str(), "%d,%d", &receivedSwarmID, &receivedReading);

      // Store the reading in the array and update last received time
      readings[receivedSwarmID] = receivedReading;
      lastReceivedTime = millis();
    }

    // Process RPi reset message format
    if (packetStr.startsWith(RPi_startBit) && packetStr.endsWith(RPi_endBit)) {
      String data = packetStr.substring(RPi_startBit.length(), packetStr.length() - RPi_endBit.length());
      if (data == "RESET_REQUESTED") {
        digitalWrite(LED_INDICATOR, HIGH);
        digitalWrite(LED_MASTER, HIGH);
        isMaster = true;
        Serial.println("RESET REQUESTED BY RPI5");
        delay(3000);  // Hold reset state for 3 seconds
      }
    }
  }

  // Check if silent time has elapsed and broadcast reading if needed
  if (millis() - lastReceivedTime > silentTime) {
    analogValue = analogRead(PHOTORESISTOR_PIN);

    // Broadcast this device's reading to other devices
    String message = ESP_startBit + String(swarmID) + "," + String(analogValue) + ESP_endBit;
    Serial.printf("Broadcasting message: %s\n", message.c_str());
    udp.beginPacket(broadcastIP, localPort);
    udp.write(message.c_str());
    udp.endPacket();
    lastReceivedTime = millis();

    // Determine if this device is the Master
    isMaster = true;
    for (int i = 0; i < 10; i++) {
      if (i != swarmID && readings[i] >= 0 && readings[i] > analogValue) {
        isMaster = false;
        break;
      }
    }

    // Update role and broadcast to RPi if Master
    if (isMaster) {
      role = "Master";
      String masterMessage = RPi_startBit + role + "," + String(swarmID) + "," + String(analogValue) + RPi_endBit;
      Serial.printf("Master to RPi: %s\n", message.c_str());
      udp.beginPacket(broadcastIP, localPort);
      udp.write(masterMessage.c_str());
      udp.endPacket();
    } else {
      role = "Slave";
    }
    Serial.printf("Current role: %s (Reading: %d)\n", role.c_str(), analogValue);
    Serial.println("");
  }
}
