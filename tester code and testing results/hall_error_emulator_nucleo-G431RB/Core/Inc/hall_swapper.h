/*
 * emulator_GPIOS_and_DACs.h
 *
 *  Created on: Nov 17, 2023
 *      Author: javier
 */

#ifndef INC_HALL_SWAPPER_H_
#define INC_HALL_SWAPPER_H_

#include "main.h"

typedef enum {		//this enum is used as an index for arrays, so it should start in 0
  hall_A = (uint32_t) 0,
  hall_B = (uint32_t) 1,
  hall_C = (uint32_t) 2,
  hall_order_unknown =(uint32_t) 0xFFFF
} hall_signals_order;

typedef enum {		//this enum is used inside various if elses, the values could be changed
  hall_direct = (uint32_t) 0,
  hall_inverse= (uint32_t) 1,
  hall_polarity_unknown =(uint32_t) 0xFFFF
} hall_signals_polarity;


//lets make available those two local variables to external files.
extern hall_signals_order 		signal_order[3];
extern hall_signals_polarity 	signal_polarity[3];

//public functions
void hall_swapper_twentyKHzinterruptionIRQ( void );

#endif /* INC_HALL_SWAPPER_H_ */
