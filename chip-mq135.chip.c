// Wokwi Custom Chip - MQ-135 Gas Sensor
// https://docs.wokwi.com/chips-api/getting-started

#include "wokwi-api.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct {
  pin_t pin;
  uint32_t ppm_attr;
} chip_data_t;

void chip_timer_callback(void *data) {
  chip_data_t *chip_data = (chip_data_t*)data;
  uint32_t ppm = attr_read(chip_data->ppm_attr);

  // ESP32 ADC: 0 - 3.3V corresponds to 0 - 4095
  // Map ppm (0 - 1000) to 0 - 3.3V
  float volts = ((float)ppm / 1000.0f) * 3.3f;
  if (volts > 3.3f) volts = 3.3f;
  if (volts < 0.0f) volts = 0.0f;

  pin_dac_write(chip_data->pin, volts);
}

void chip_init() {
  chip_data_t *chip_data = (chip_data_t*)malloc(sizeof(chip_data_t));
  chip_data->ppm_attr = attr_init("ppm", 400); 
  chip_data->pin = pin_init("A0", ANALOG);

  const timer_config_t config = {
    .callback = chip_timer_callback,
    .user_data = chip_data,
  };

  timer_t timer_id = timer_init(&config);
  // Update every 100ms (100,000 microseconds)
  timer_start(timer_id, 100000, true); 

  printf("MQ-135 custom chip initialized!\n");
}
