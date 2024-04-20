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
#define NUMBEROFPHASES 3
#define messageLength (NUMBEROFPHASES*2)+2		/*!< ASCII results char array length to use with UART*/

#define TOTAL_NUMBEROFRESULTS 6
#define VALID_NUMBEROFRESULTS 3

typedef enum {
  detection_DISABLED 					=(uint32_t)0,
  detection_ENABLED  					=(uint32_t)1,

  detection_WAIT_CURRENT_STATIONARY  	=(uint32_t)2,
  detection_ADQUISITION  				=(uint32_t)3,
  detection_INTERPRETATION  			=(uint32_t)4,
  detection_VALIDATION  				=(uint32_t)5,
  detection_PRESENTATION_FINISH  		=(uint32_t)6,
  detection_ERROR_OR_TIMEOUT  			=(uint32_t)7
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
  phase_C = (uint32_t) 2
} phase_order_enum;


typedef enum {		//this enum is used inside various if elses, the values could be changed
  rising_polarity = (uint32_t) 0,
  falling_polarity= (uint32_t) 1
} signals_polarity_enum;

typedef enum {
  hall_direct = (uint8_t) 0,
  hall_inverse= (uint8_t) 1,
  hall_polarity_unknown =(uint8_t) 0xF
} hall_curr_relation_enum;

typedef struct{
	volatile float two_samples_buffer[2];
	uint32_t zerocrossings_tick[MAXZEROCROSSINGS];
	signals_polarity_enum zerocrossings_polarity[MAXZEROCROSSINGS];
	uint32_t numberof_zerocrossings;
}current_or_hall_measurements_struct;

typedef enum{
	NO	= (uint32_t) 0,
	YES	= (uint32_t) 1
}detection_YES_NO;

typedef struct{
	detection_YES_NO is_valid;
	uint32_t electricPeriod_ticks;
	hall_signals_order_enum hall_order[3];
	hall_curr_relation_enum hall_polarity[3];
}detection_results_struct;



typedef struct{
	uint32_t start_adquisition_ticks;			/*!< used as a timestamp when adquisition starts*/
	current_or_hall_measurements_struct currA;	/*!< contains zerocrossings and pre-distiled info for phaseA current*/
	current_or_hall_measurements_struct currB;	/*!< contains zerocrossings and pre-distiled info for phaseB current*/
	current_or_hall_measurements_struct currC;	/*!< contains zerocrossings and pre-distiled info for phaseC current*/
	current_or_hall_measurements_struct hallA;	/*!< contains zerocrossings and pre-distiled info for hallA signal*/
	current_or_hall_measurements_struct hallB;	/*!< contains zerocrossings and pre-distiled info for hallA signal*/
	current_or_hall_measurements_struct hallC;	/*!< contains zerocrossings and pre-distiled info for hallA signal*/

	int32_t differences_phaseA[NUMBEROFPHASES];	/*!< auxiliar variable storing average distances between zerocrossings between each hall and phaseA*/
	int32_t differences_phaseB[NUMBEROFPHASES];	/*!< auxiliar variable storing average distances between zerocrossings between each hall and phaseB*/
	int32_t differences_phaseC[NUMBEROFPHASES];	/*!< auxiliar variable storing average distances between zerocrossings between each hall and phaseC*/
	uint32_t shifted_polarity[NUMBEROFPHASES][NUMBEROFPHASES];	/*!< when phase and hall signals are measured out of phase due to unlucky timming when detecting, it is noted down in this variable for the polarity calculation */

	uint32_t numberOfresults;
	uint32_t indexOfcorrectResult;
	detection_results_struct results[TOTAL_NUMBEROFRESULTS];
	uint8_t message[messageLength];					/*!< ASCII results char array to use with UART*/

}hall_detection_general_struct;


void Hall_start_detection();
uint32_t Hall_is_detection_finished();

void Hall_Identification_Test_measurement(
		hall_pin_info* H1_gpio,
		hall_pin_info* H2_gpio,
		hall_pin_info* H3_gpio,
		volatile float* ADCcurr1,
		volatile float* ADCcurr2
		);
void present_results_uart();

#endif /* INC_HALL_DETECTION_H_ */
