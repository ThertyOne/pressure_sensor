// PROGRAM FOR READING OUTPUT SIGNAL FROM PIRANI GAUGE TPR 265 SENSOR
// Program includes defining messured gas and unit you're messuring

// 6.73 k and 4.7 k  -> used -> 2.4319148936170212765957446808511 -> 0.06405871263 -> 2.36785618098
// 5.15 k and 3.95 k -> real (voltage meter) -> 2.303797468354430379746835443038
// V_in = V_out * k
// k = (R1 + R2) / R2 = 1 + R1/R2

#include <Wire.h>
#include <LCD_I2C.h>

#define CALIBRATION_FACTOR 1 //air

/*
Calibration factors by gas type:

Gas type          | CALIBRATION_FACTOR
------------------|----------------------
Air / N2 / O2 / CO|  1.0   (default)
CO2               |  0.9
Water vapor       |  0.5
H2                |  0.5
He                |  0.8
Ne                |  1.4
Ar                |  1.7
Kr                |  2.4
Xe                |  3.0
Freon 12          |  0.7
*/

#define pressureUnit 5.625
/*
mbar    -> 5.5
ubar    -> 2.5
Pa      -> 3.5
kPa     -> 6.5
Thorr   -> 5.625
mThor   -> 2.625
micron  -> 2.625
*/

// A(0) -> input of messured signal
// Initialization: I2C address (default 0x27), 16 columns, 2 rows
// SDA → A4 (on UNO), SCL → A5 (on UNO), +5V and GND for power
LCD_I2C lcd(0x27, 16, 2);

const char* getPressureUnitLabel() {
  if (pressureUnit == 2.5)    return "µbar";
  if (pressureUnit == 3.5)    return "Pa";
  if (pressureUnit == 5.5)    return "mbar";
  if (pressureUnit == 6.5)    return "kPa";
  if (pressureUnit == 5.625)  return "Torr";
  if (pressureUnit == 2.625)  return "mTorr";
  // fallback
  return "xxx";
}


//Interpolesion function
const int dataSize = 59;

float adc[dataSize] = {
  2.89, 5, 11.67, 15.77, 17.12, 24.44, 27.27, 42.25, 71.07, 80.7,
  85, 88.9, 93.2, 98.5, 100, 103, 110.3, 113.17, 119.54, 122.86,
  122.86, 129.77, 133.37, 140.88, 140.88, 148.8, 152.93, 157.18,
  166.02, 166.02, 176.36, 185.23, 190.37, 195.65, 201.08, 206.66,
  212.39, 218.29, 224.34, 230.57, 236.97, 243.54, 250.3, 257.24,
  264.38, 287.01, 294.97, 303.16, 311.57, 320.21, 329.1, 338.23, 
  347.62, 357.26, 367.17, 468.53, 421.02, 481.55
};

float true_vals[dataSize] = {
  1.86, 3.5, 8, 12, 13, 16, 22, 27.5, 52, 80.7,
  85, 88.9, 93.2, 98.5, 100, 103, 110.3, 113.17, 119.54, 122.86,
  122.86, 129.77, 133.37, 140.88, 140.88, 148.8, 152.93, 157.18,
  166.02, 166.02, 176.36, 185.23, 190.37, 195.65, 201.08, 206.66,
  212.39, 218.29, 224.34, 230.57, 236.97, 243.54, 250.3, 257.24,
  264.38, 288.8, 305.2, 311.4, 336.2, 349.6, 374.9, 397.5, 415.3,
  445.9, 465.9, 710, 733, 747
};

float linearInterp(float x, float* x_points, float* y_points, int size) {
  for (int i = 0; i < size - 1; i++) {
    if (x >= x_points[i] && x <= x_points[i + 1]) {
      float x0 = x_points[i];
      float x1 = x_points[i + 1];
      float y0 = y_points[i];
      float y1 = y_points[i + 1];
      return y0 + (y1 - y0) * (x - x0) / (x1 - x0);
    }
  }
  return 0;  // wartość poza zakresem
}

// Function to convert voltage to pressure, includes gas calibration
float voltageToPressure(float sensorVoltage) {
  float rawPressure = pow(10, sensorVoltage - pressureUnit);    // raw pressure
  //float correctedPressure = linearInterp(rawPressure, adc, true_vals, dataSize);

  return rawPressure;
}

byte arrowUp[8] = {
  0b00100,
  0b01110,
  0b10101,
  0b00100,
  0b00100,
  0b00100,
  0b00100,
  0b00000
};

byte arrowDown[8] = {
  0b00100,
  0b00100,
  0b00100,
  0b00100,
  0b10101,
  0b01110,
  0b00100,
  0b00000
};

void setup() {
  Serial.begin(9600);              // Start UART communication
  analogReference(DEFAULT);        // Default 5V reference 
  lcd.begin();
  lcd.backlight();                 // Turn on LCD backlight
  lcd.createChar(0, arrowUp);
  lcd.createChar(1, arrowDown);
}

void loop() {
// Reading and averaging values from analog pin A0:
// analogRead(A0) reads the voltage from pin A0 and returns a value in the range 0–1023.
// The loop performs 64 readings and accumulates them in the 'sum' variable.
// Then, we perform sum >> 6, which is equivalent to dividing by 64,
// but faster – it's a bitwise right shift by 6 bits (2^6 = 64).

  long sum = 0;
  for (int i = 0; i < 64; i++) {
    sum += analogRead(A0);
  }
  int adcValue = sum >> 6;

  float voltage = ((adcValue * 5.0) / 1023.0) + 0.01;                    // ADC → voltage at Arduino pin
  float sensorVoltage = (voltage * 2.435)+0.002;    // real sensor output voltage to show STATUS
  float pressure = voltageToPressure(sensorVoltage);                   // pressure in mbar

  // Determine status based on sensor voltage (before voltage divider)
  String status;
  int statusCode = 0;  // 0 = OK, 1 = UNDERRANGE, 2 = OVERRANGE, 3 = ERROR

  if (sensorVoltage < 0.5) {
    status = "ERROR";
    statusCode = 3;
  } else if (sensorVoltage < 2.2) {
    status = "RANGE";
    statusCode = 1; // ↓
  } else if (sensorVoltage > 8.6 && sensorVoltage <= 10.3) {
    status = "RANGE";
    statusCode = 2; // ↑
  } else if (sensorVoltage > 10.3) {
    status = "ERROR";
    statusCode = 3;
  } else {
    status = "OK";
    statusCode = 0;
  }


  // LCD output
  lcd.clear();
  lcd.setCursor(0, 0);
  if (status == "OK") {
    lcd.print("Pres:");
    lcd.print(pressure, 2);
    lcd.print(" ");
    lcd.print(getPressureUnitLabel());

  } else {
  lcd.print("Status: ");
  if (statusCode == 1) {
    lcd.write(byte(1)); // ↓
  } else if (statusCode == 2) {
    lcd.write(byte(0)); // ↑
  }
  lcd.print(" ");
  lcd.print(status);
}


  lcd.setCursor(0, 1);
  lcd.print("Volt:");
  lcd.print(sensorVoltage, 3);
  lcd.print(" V");

  delay(500);
}

