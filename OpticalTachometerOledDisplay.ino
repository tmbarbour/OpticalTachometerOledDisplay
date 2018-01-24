/*********************************************************************
RPM Tachometer with OLED digital and analog display
 *********************************************************************/
 
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Math.h>

namespace {
  const int OLED_RESET = 4;
  const int TEXT_SIZE_SMALL = 1;
  const int TEXT_SIZE_LARGE = 2;
  const int ONE_K = 1000;
  
  const int OLED_HEIGHT = 64;
  const int OLED_WIDTH = 128;
  const int YELLOW_SEGMENT_HEIGHT = 16;
  const int DISPLAY_FULL_BRIGHTNESS = 255;
  const int DISPLAY_DIM_BRIGHTNESS = 0;
  
  const int IR_LED_PIN_3 = 3;
  const int PHOTODIODE_PIN_2 = 2;
  const int INTERRUPT_ZERO_ON_PIN_2 = 0;
  
  const uint16_t DIAL_CENTER_X = OLED_WIDTH / 2;
  const uint16_t DIAL_RADIUS = (OLED_HEIGHT - YELLOW_SEGMENT_HEIGHT) - 1;
  const uint16_t DIAL_CENTER_Y = OLED_HEIGHT - 1;
  const uint16_t INDICATOR_LENGTH = DIAL_RADIUS - 5;
  const uint16_t INDICATOR_WIDTH = 5;
  const uint16_t LABEL_RADIUS = DIAL_RADIUS - 18;
  const int DIAL_LABEL_Y_OFFSET = 6;
  const int DIAL_LABEL_X_OFFSET = 4;
  
  const long MAJOR_TICKS[] = { 0, 10000, 20000, 30000 };
  const int MAJOR_TICK_COUNT = sizeof(MAJOR_TICKS) / sizeof(MAJOR_TICKS[0]);
  const int  MAJOR_TICK_LENGTH = 7;
  const long MINOR_TICKS[] = {5000, 15000, 25000};
  const int MINOR_TICK_COUNT = sizeof(MINOR_TICKS) / sizeof(MINOR_TICKS[0]);
  const int MINOR_TICK_LENGTH = 3;
  
  const uint16_t DIAL_MAX_RPM = MAJOR_TICKS[MAJOR_TICK_COUNT-1];
  
  const int HALF_CIRCLE_DEGREES = 180;
  const float PI_RADIANS = PI/HALF_CIRCLE_DEGREES;
  
  const double MILLIS_PER_SECOND = 1000.0;
  const double SECONDS_PER_MINUTE = 60.0;
  const long DISPLAY_TIMEOUT_INTERVAL = 120 * MILLIS_PER_SECOND;
  const long DISPLAY_DIM_INTERVAL = DISPLAY_TIMEOUT_INTERVAL/2;
  const long DISPLAY_UPDATE_INTERVAL = 150;
  const int  DISPLAY_AVERAGE_INTERVALS = 6;
  
  volatile unsigned long revolutions;
  
  unsigned long previous_revolutions = 0;
  unsigned long revolution_count[DISPLAY_AVERAGE_INTERVALS]; 
  unsigned long interval_millis[DISPLAY_AVERAGE_INTERVALS]; 
  unsigned int interval_index = 0;
  unsigned long previous_millis = 0;
  unsigned long last_sensor_time = 0;
  bool is_oled_display_on = false;
  bool is_oled_display_dim = false;
}

Adafruit_SSD1306 display(OLED_RESET);

void setup() {
  Serial.begin(9600);
  initOledDisplayWithI2CAddress(0x3C);
  display.setTextColor(WHITE);
  initArrays();
	
  turnOnIrLED();
  attachPhotodiodeToInterrruptZero();
  last_sensor_time = millis();
  turnOnDisplay();
}

void initArrays() {
  memset(revolution_count,0,sizeof(revolution_count));
  memset(interval_millis,0,sizeof(interval_millis));
}

void loop() {
  unsigned long current_millis = millis();
  if (current_millis - last_sensor_time >= DISPLAY_TIMEOUT_INTERVAL) {
    turnOffDisplay();
  } else if (current_millis - last_sensor_time >= DISPLAY_DIM_INTERVAL) {
    dimDisplay();
  }  
  
  if (current_millis - previous_millis >= DISPLAY_UPDATE_INTERVAL) {
    previous_millis = current_millis;
    updateDisplay();
	}
}

void initOledDisplayWithI2CAddress(uint8_t i2c_address) {
  display.begin(SSD1306_SWITCHCAPVCC, i2c_address);
}

void turnOnDisplay() {
  display.ssd1306_command(SSD1306_DISPLAYON); 
  display.dim(false);;
  is_oled_display_on = true;
  is_oled_display_dim = false;
}

void turnOffDisplay() {
  display.ssd1306_command(SSD1306_DISPLAYOFF); 
  is_oled_display_on = false;
  is_oled_display_dim = false;
}

void dimDisplay() {
  display.dim(true);
  is_oled_display_dim = true;
}

void turnOnIrLED() {
  pinMode(IR_LED_PIN_3, OUTPUT);
  digitalWrite(IR_LED_PIN_3, HIGH);
}

void attachPhotodiodeToInterrruptZero() {
  pinMode(PHOTODIODE_PIN_2, INPUT_PULLUP);
  attachInterrupt(INTERRUPT_ZERO_ON_PIN_2, incrementRevolution, FALLING);
}

void incrementRevolution() {
  revolutions++;
}

void updateDisplay() {
  long rpm = calculateRpm();
  if (rpm > 0) {
    last_sensor_time = millis();
    if (!is_oled_display_on || is_oled_display_dim) {
      turnOnDisplay();
    }
  }
  if (is_oled_display_on) {
    display.clearDisplay();
    drawRpmBanner(rpm);
    drawDial(rpm);
    display.display();
  }
}

long calculateRpm() {
  unsigned long current_millis = millis();
  unsigned long current_revolutions = revolutions;
  unsigned long previous_display_millis;
  unsigned long previous_revolutions;
    
  queueIntervalRevolution(current_revolutions, current_millis);
  previous_display_millis = getIntervalMillis();
  previous_revolutions = getIntervalRevolutions();

  unsigned long elapsed_millis =  current_millis - previous_display_millis;
  float elapsed_seconds = ((elapsed_millis * 1.0) / MILLIS_PER_SECOND);
  float delta_revolutions = (current_revolutions - previous_revolutions) * 1.0;

  long rpm = (long) ((delta_revolutions / elapsed_seconds) * SECONDS_PER_MINUTE);
  return rpm;
}

void queueIntervalRevolution(unsigned long revolution_value, unsigned long milliseconds) {
  interval_index++;
  int queue_index = (int)(interval_index % DISPLAY_AVERAGE_INTERVALS);
  revolution_count[queue_index] = revolution_value; 
  interval_millis[queue_index] = milliseconds;
}

unsigned long getIntervalMillis() {
  int index_front_of_queue = (int)((interval_index + 1)  % DISPLAY_AVERAGE_INTERVALS);
  return interval_millis[index_front_of_queue];
}

unsigned long getIntervalRevolutions() {
  int index_front_of_queue = (int)((interval_index + 1)  % DISPLAY_AVERAGE_INTERVALS);
  return revolution_count[index_front_of_queue];
}

void drawRpmBanner(long rpm_value) {
  display.setCursor(0, 0);

  display.setTextSize(TEXT_SIZE_LARGE);
  display.print("RPM: ");
  display.print((long)rpm_value);
}

void drawDial(long rpm_value) {
  display.drawCircle(DIAL_CENTER_X, DIAL_CENTER_Y, DIAL_RADIUS, WHITE);
  drawTickMarks();
  drawMajorTickLabels();
  drawIndicatorHand(rpm_value);
}

void drawTickMarks() {
  drawTicks(MAJOR_TICKS, MAJOR_TICK_COUNT, MAJOR_TICK_LENGTH);
  drawTicks(MINOR_TICKS, MINOR_TICK_COUNT, MINOR_TICK_LENGTH);
}

void drawTicks(const long ticks[], int tick_count, int tick_length) {
  for (int tick_index = 0; tick_index < tick_count; tick_index++) {
		long rpm_tick_value = ticks[tick_index];
		float tick_angle = (HALF_CIRCLE_DEGREES * getPercentMaxRpm(rpm_tick_value)) + HALF_CIRCLE_DEGREES;
		uint16_t dial_x = getCircleXWithLengthAndAngle(DIAL_RADIUS - 1, tick_angle);
		uint16_t dial_y = getCircleYWithLengthAndAngle(DIAL_RADIUS - 1, tick_angle);
		uint16_t tick_x = getCircleXWithLengthAndAngle(DIAL_RADIUS - tick_length, tick_angle);
		uint16_t tick_y = getCircleYWithLengthAndAngle(DIAL_RADIUS - tick_length, tick_angle);
		display.drawLine(dial_x, dial_y, tick_x, tick_y, WHITE);
	}
}

float getPercentMaxRpm(long value) {
	float ret_value = (value * 1.0)/(DIAL_MAX_RPM * 1.0);
	return ret_value;
}

float getCircleXWithLengthAndAngle(uint16_t radius, float angle) {
	return DIAL_CENTER_X + radius * cos(angle*PI_RADIANS);
};

float getCircleYWithLengthAndAngle(uint16_t radius, float angle) {
	return DIAL_CENTER_Y + radius * sin(angle*PI_RADIANS);
};

void drawMajorTickLabels() {
	display.setTextSize(TEXT_SIZE_SMALL);
	for (int label_index = 0; label_index < MAJOR_TICK_COUNT; label_index++) {
		long rpm_tick_value = MAJOR_TICKS[label_index];
		float tick_angle = (HALF_CIRCLE_DEGREES	* getPercentMaxRpm(rpm_tick_value)) + HALF_CIRCLE_DEGREES;
		uint16_t dial_x = getCircleXWithLengthAndAngle(LABEL_RADIUS, tick_angle);
		uint16_t dial_y = getCircleYWithLengthAndAngle(LABEL_RADIUS, tick_angle);
		display.setCursor(dial_x - DIAL_LABEL_X_OFFSET, dial_y - DIAL_LABEL_Y_OFFSET);
		int label_value = rpm_tick_value / ONE_K;
		display.print(label_value);
	}
}

void drawIndicatorHand(long rpm_value) {
    float indicator_angle = (HALF_CIRCLE_DEGREES * getPercentMaxRpm(rpm_value)) + HALF_CIRCLE_DEGREES;
    uint16_t indicator_top_x = getCircleXWithLengthAndAngle(INDICATOR_LENGTH, indicator_angle);
    uint16_t indicator_top_y = getCircleYWithLengthAndAngle(INDICATOR_LENGTH, indicator_angle);

	display.drawTriangle(DIAL_CENTER_X - INDICATOR_WIDTH / PHOTODIODE_PIN_2,
	                     DIAL_CENTER_Y,DIAL_CENTER_X + INDICATOR_WIDTH / PHOTODIODE_PIN_2,
	                     DIAL_CENTER_Y,
	                     indicator_top_x, 
	                     indicator_top_y, 
	                     WHITE);
}
