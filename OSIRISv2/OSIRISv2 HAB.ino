/*

Name: OSIRISv2 Hab Integration
Author: Felix Abbott and Michael Tran
Date: 07/11/2025

Overview: This algorithm converts ADC data from three OSIRIS modules into the sun vector using spherical coordinates.
It first converts the ADC data to the angle of incidence, calculates the Lambertian value and obtains the intersection of all three light cones,
i.e., where the source is located. It represents this using spherical coordinates (θ and φ).

Interpretation: Please ensure all modules are placed perpendicular to each other. φ represents the angle of elevation/depression to the horizon / angle between the XY plane and the +Z axis.
θ represents the azimuth angle from +X to +Y. Module "X" points in θ=0deg and module "Y" points in θ=90deg.

Reads algorithm values onto connected SD card

*/

#include <math.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"

/*
Pinout for ESP32S3 to SD Card Breakout
CS / 10
D1 / 11
VCC / 3V3
SCK / 12
GND / G
D0 / 13
*/

void listDir(fs::FS &fs, const char *dirname, uint8_t levels) {
  Serial.printf("Listing directory: %s\n", dirname);

  File root = fs.open(dirname);
  if (!root) {
    Serial.println("Failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    Serial.println("Not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if (levels) {
        listDir(fs, file.path(), levels - 1);
      }
    } else {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("  SIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}

void removeDir(fs::FS &fs, const char *path) {
  Serial.printf("Removing Dir: %s\n", path);
  if (fs.rmdir(path)) {
    Serial.println("Dir removed");
  } else {
    Serial.println("rmdir failed");
  }
}

void readFile(fs::FS &fs, const char *path) {
  Serial.printf("Reading file: %s\n", path);

  File file = fs.open(path);
  if (!file) {
    Serial.println("Failed to open file for reading");
    return;
  }

  Serial.print("Read from file: ");
  while (file.available()) {
    Serial.write(file.read());
  }
  file.close();
}

void writeFile(fs::FS &fs, const char *path, const char *message) {
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  if (file.print(message)) {
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
  file.close();
}

void appendFile(fs::FS &fs, const char *path, const char *message) {
  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if (!file) {
    Serial.println("Failed to open file for appending");
    return;
  }
  if (file.print(message)) {
    Serial.println("Message appended");
  } else {
    Serial.println("Append failed");
  }
  file.close();
}

void renameFile(fs::FS &fs, const char *path1, const char *path2) {
  Serial.printf("Renaming file %s to %s\n", path1, path2);
  if (fs.rename(path1, path2)) {
    Serial.println("File renamed");
  } else {
    Serial.println("Rename failed");
  }
}

#define BOOT_BUTTON_PIN 0
#define SAMPLES_PER_SECOND 50
#define SAMPLE_SECONDS 3
#define NUM_MODULES 5

const int pins[7] = {4, 5, 6, 7, 15, 16, 17}; // DO NOT CHANGE
const char* connectors[7] = {"OAA", "OAB", "OAC", "OAD", "OAE", "OAF", "OAG"}; // DO NOT CHANGE
const char* axis[7] = {"+X", "-Y", "-X", "+Z", "+Y", "?", "?"}; // Change according to orientation

int maxIntensity[NUM_MODULES]; // Maximum sensor values
int minIntensity[NUM_MODULES]; // Minimum sensor values
int readings[NUM_MODULES] = {0}; // Current sensor readings

void resetCalibration() {
    for (int i = 0; i < NUM_MODULES; i++) {
        maxIntensity[i] = 0;
        minIntensity[i] = 4095; // 12-bit ADC range
    }
    Serial.println("Calibration values reset.");
}

void setup() {
    //SD Card Setup
    Serial.begin(115200);
#ifdef REASSIGN_PINS
  SPI.begin(sck, miso, mosi, cs);
  if (!SD.begin(cs)) {
#else
  if (!SD.begin()) {
#endif
    Serial.println("Card Mount Failed");
    return;
  }
  uint8_t cardType = SD.cardType();

  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    return;
  }

  Serial.print("SD Card Type: ");
  if (cardType == CARD_MMC) {
    Serial.println("MMC");
  } else if (cardType == CARD_SD) {
    Serial.println("SDSC");
  } else if (cardType == CARD_SDHC) {
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }

  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);

// listDir(SD, "/", 0);
writeFile(SD, "/osiris_values.csv", ""); // USED TO CLEAR SSD DATA

//OSIRIS
    pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP); // Active LOW
    delay(1000);

    Serial.print("Sun Sensor Algorithm Initialising");
    for (int i = 0; i < NUM_MODULES; i++) {
        pinMode(pins[i], INPUT);
        Serial.print(".");
        delay(500);
    }
    Serial.println();

    resetCalibration();

    Serial.println("Move sensors to extremes.");
    Serial.println("→ Press and release BOOT to reset calibration values.");
    Serial.println("→ Hold BOOT for 5 seconds and release to exit calibration.");

    unsigned long bootPressedAt = 0;
    bool bootHeld = false;

    // Calibration loop
    while (true) {
        int bootState = digitalRead(BOOT_BUTTON_PIN);

        if (bootState == LOW) {
            if (bootPressedAt == 0) {
                bootPressedAt = millis(); // Start timing
            } else if (millis() - bootPressedAt > 2000) {
                bootHeld = true;
            }
        } else {
            if (bootPressedAt != 0) {
                if (bootHeld) {
                    Serial.println("Calibration complete.");
                    break; // Exit calibration
                } else {
                    resetCalibration(); // Reset values
                }
                bootPressedAt = 0;
                bootHeld = false;
            }
        }

        // Read sensor values
        for (int i = 0; i < NUM_MODULES; i++) {
            readings[i] = analogRead(pins[i]);

            if (readings[i] > maxIntensity[i]) {
                maxIntensity[i] = readings[i];
            }

            if (readings[i] < minIntensity[i]) {
                minIntensity[i] = readings[i];
            }

            Serial.print(axis[i]);
            Serial.print(" (");
            Serial.print(connectors[i]);
            Serial.print(") → [");
            Serial.print(minIntensity[i]);
            Serial.print(", ");
            Serial.print(maxIntensity[i]);
            Serial.print("] | ");
        }
        Serial.println("");
        delay(500);
    }

    delay(1000); // Wait before entering main loop
}

void loop() {
    float x_sum = 0, y_sum = 0, z_sum = 0;

    for (int i = 0; i < SAMPLES_PER_SECOND * SAMPLE_SECONDS; i++) {
        for (int j = 0; j < NUM_MODULES; j++) {
            readings[j] = analogRead(pins[j]);
        }

        x_sum += getLambertian(readings[0], minIntensity[0], maxIntensity[0]);
        x_sum -= getLambertian(readings[2], minIntensity[2], maxIntensity[2]);
        
        y_sum += getLambertian(readings[5], minIntensity[5], maxIntensity[5]);
        y_sum -= getLambertian(readings[1], minIntensity[1], maxIntensity[1]);

        z_sum += getLambertian(readings[4], minIntensity[4], maxIntensity[4]);

        delay(1000 / SAMPLES_PER_SECOND); // Evenly spaced within 1 second
    }

    // Average Lambertian components
    float x = x_sum / (SAMPLES_PER_SECOND * SAMPLE_SECONDS);
    float y = y_sum / (SAMPLES_PER_SECOND * SAMPLE_SECONDS);
    float z = z_sum / (SAMPLES_PER_SECOND * SAMPLE_SECONDS);

    // Normalize vector
    float r = sqrt(x * x + y * y + z * z);
    if (r == 0) r = 1;

    x /= r;
    y /= r;
    z /= r;

    // Calculate angles
    float phi = degrees(atan2(y, x)); // Azimuth
    float theta = degrees(acos(z));       // Elevation from vertical
    float elevation = 90.0 - theta;

    // Output
    Serial.print("Vector → X: ");
    Serial.print(x);    
    Serial.print(", Y: ");
    Serial.print(y);            
    Serial.print(", Z: ");
    Serial.println(z);

    Serial.print("Sun Position → Azimuth (φ): ");
    Serial.print(phi, 2);
    Serial.print("°, Elevation (θ): ");
    Serial.print(elevation, 2);
    Serial.println("°");

    Serial.println("-------------");
    
    // USING SD CARD
    appendFile(SD, "/osiris_values.csv", (String(x) + "," + String(y) + "," + String(z) + "," + String(phi) + "," + String(elevation)).c_str());
    appendFile(SD, "/osiris_values.csv", "\n");
    //readFile(SD, "/osiris_values.csv");
}

float getLambertian(int adc, int minIntensity, int maxIntensity) {
    float value = (float)clamp_map(adc, minIntensity, maxIntensity, 0, 1000);
    float angle = getAngle(value); // Convert to angle using intensity curve
    return cos(radians(angle));    // Lambertian response
}

float getAngle(float intensity) {
    float val = 1940 / (intensity + 940);
    return 88 * pow(val - 1, (1.0 / 3.3)); // Empirical model
}

int clamp_map(int x, int in_min, int in_max, int out_min, int out_max) {
    if (x < in_min) {x = in_min;}
    else if (x > in_max) {x = in_max;}
    return map(x, in_min, in_max, out_min, out_max);
}
