/*

Name: OSIRISv2 Horus Algorithm
Author: Felix Abbott
Date: 29/05/2025

Overview: This algorithm converts ADC data from three OSIRIS modules into the sun vector using spherical coordinates.
It first converts the ADC data to the angle of incidence, calculates the Lambertian value and obtains the intersection of all three light cones,
i.e., where the source is located. It represents this using spherical coordinates (θ and φ).

Interpretation: Please ensure all three modules are placed perpendiculat to each other. φ represents the angle of elevation/depression to the horizon.
θ represents the azimuth angle. Module "X" points in θ=0deg and module "Y" points in θ=90deg.

*/

#include <math.h>

#define BOOT_BUTTON_PIN 0
#define SAMPLES_PER_SECOND 10
#define SAMPLE_SECONDS 3
#define NUM_MODULES 4

const int pins[] = {4, 5, 7, 16, 17};
const char* axis[5] = {"-Y", "+Z", "+X", "+Y", "-X"};
const char* connectors[5] = {"OAA", "OAB", "OAD", "OAF"};

int maxIntensity[5]; // Maximum sensor values
int minIntensity[5]; // Minimum sensor values
int readings[5] = {0, 0, 0, 0, 0}; // Current sensor readings

void resetCalibration() {
    for (int i = 0; i < NUM_MODULES; i++) {
        maxIntensity[i] = 0;
        minIntensity[i] = 4095; // 12-bit ADC range
    }
    Serial.println("Calibration values reset.");
}

void setup() {
    Serial.begin(9600);
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

        x_sum += getLambertian(readings[2], minIntensity[2], maxIntensity[2]);
        x_sum -= getLambertian(readings[4], minIntensity[4], maxIntensity[4]);
        
        y_sum += getLambertian(readings[0], minIntensity[0], maxIntensity[0]);
        y_sum -= getLambertian(readings[3], minIntensity[3], maxIntensity[3]);

        z_sum += getLambertian(readings[1], minIntensity[1], maxIntensity[1]);

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