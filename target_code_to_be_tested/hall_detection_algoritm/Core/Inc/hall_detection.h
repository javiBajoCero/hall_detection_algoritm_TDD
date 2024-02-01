/*
 * hall_detection.h
 *
 *  Created on: Jan 30, 2024
 *      Author: javier.munoz.saez@upc.edu
 */

#ifndef INC_HALL_DETECTION_H_
#define INC_HALL_DETECTION_H_


#include "main.h"

#define MAXZEROCROSSINGS 6

typedef enum {
  detection_DISABLED =(uint32_t)0,
  detection_ENABLED  =(uint32_t)1
} detection_state_enum ;

typedef enum {		//this enum is used as an index for arrays, so it should start in 0
  hall_A = (uint32_t) 0,
  hall_B = (uint32_t) 1,
  hall_C = (uint32_t) 2,
  number_of_halls =(uint32_t) 3
} hall_signals_order_enum;

typedef enum {		//this enum is used as an index for arrays, so it should start in 0
  phase_A = (uint32_t) 0,
  phase_B = (uint32_t) 1,
  phase_C = (uint32_t) 2,
  number_of_phases = (uint32_t) 3
} phase_order_enum;


typedef enum {		//this enum is used inside various if elses, the values could be changed
  rising_polarity = (uint32_t) 0,
  falling_polarity= (uint32_t) 1
} signals_polarity_enum;

typedef enum {
  hall_direct = (uint32_t) 0,
  hall_inverse= (uint32_t) 1,
  hall_polarity_unknown =(uint32_t) 0xFFFF
} hall_curr_relation_enum;

typedef struct{
	uint32_t two_samples_buffer[2];

	uint32_t zerocrossings_tick[MAXZEROCROSSINGS];
	signals_polarity_enum zerocrossings_polarity[MAXZEROCROSSINGS];
	uint32_t numberof_zerocrossings;
}current_or_hall_measurements_struct;

typedef struct{
	uint32_t electricPeriod_ticks;
	hall_signals_order_enum hall_order[3];
	hall_curr_relation_enum hall_polarity[3];
}detection_results_struct;


void run_hall_detection_inside_20Khz_interruption(detection_state_enum* enable);
//devolver punteros a H1 H2 H3 gpio->IDR , las polaridades son otro asunto

//void Hall_Identification_Test_measurement(
//		detection_state_enum* enable,
//		uint8_t *H1,
//		uint8_t *H2,
//		uint8_t *H3,
//		uint8_t *polarityH1,
//		uint8_t *polarityH2,
//		uint8_t *polarityH3,
//		);


#endif /* INC_HALL_DETECTION_H_ */
