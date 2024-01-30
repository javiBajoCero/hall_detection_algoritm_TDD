/*
 * hall_detection.h
 *
 *  Created on: Jan 30, 2024
 *      Author: javier.munoz.saez@upc.edu
 */

#ifndef INC_HALL_DETECTION_H_
#define INC_HALL_DETECTION_H_

#include "main.h"

typedef enum {
  detection_DISABLED =(uint32_t)0,
  detection_ENABLED  =(uint32_t)1
} detection_state_enum ;

typedef enum {
  rising_polarity   =(uint32_t)0,
  falling_polarity  =(uint32_t)1
} polarity_enum ;

#define MAXZEROCROSSINGS 8

typedef struct{
	uint32_t two_samples_buffer[2];

	uint32_t zerocrossings_tick[MAXZEROCROSSINGS];
	polarity_enum zerocrossings_polarity[MAXZEROCROSSINGS];
	uint32_t numberof_zerocrossings;
}current_measurements_struct;

typedef struct{
	GPIO_PinState two_samples_buffer[2];

	uint32_t zerocrossings_tick[MAXZEROCROSSINGS];
	polarity_enum zerocrossings_polarity[MAXZEROCROSSINGS];
	uint32_t numberof_zerocrossings;
}hall_measurements_struct;

typedef struct{
	float electricPeriod_ms;

}detection_results_struct;

void run_hall_detection_inside_20Khz_interruption(detection_state_enum* enable);



#endif /* INC_HALL_DETECTION_H_ */
