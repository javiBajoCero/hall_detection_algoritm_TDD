/*
 * hall_detection.c
 *
 *  Created on: Jan 30, 2024
 *      Author: javier.munoz.saez@upc.edu
 */

#include "hall_detection.h"
#include "usart.h"

#define ADCBITS 12
#define MAXTICKs 1000// each tick is 0.05 ms so 1000*0.05=50ms (it takes only 25ms to get 8/2=4 electric periods from torrot emulated)

#define lowpassfilter_ticks 10
//local variables
uint32_t ticks=0;


uint32_t currentADCoffset=(4096-1)/2;//

current_measurements_struct currA;
current_measurements_struct currB;

hall_measurements_struct hallA;
hall_measurements_struct hallB;
hall_measurements_struct hallC;

detection_results_struct results;

const uint8_t donemessage[]="done\n\r";

//all functions in this .c are listed here:
void run_hall_detection_inside_20Khz_interruption(detection_state_enum* enabled_or_disabled);

void signals_adquisition();
void detect_all_zerocrossings();
void detect_current_zerocrossings(current_measurements_struct* currx);
void detect_hall_zerocrossings(hall_measurements_struct* hallx);

void end_detection(detection_state_enum* enabled_or_disabled);
void evaluate_and_present_results(detection_state_enum* enabled_or_disabled);
uint32_t calculateElectricPeriod_inTicks();
float fromTicksToMiliseconds(uint32_t ticks);




//
void run_hall_detection_inside_20Khz_interruption(detection_state_enum* enabled_or_disabled){
	if(*enabled_or_disabled==detection_ENABLED){
		signals_adquisition();
		detect_all_zerocrossings();
		end_detection(enabled_or_disabled);
		evaluate_and_present_results(enabled_or_disabled);
		ticks++;
	}

}

//
void signals_adquisition(){
	currA.two_samples_buffer[1]=currA.two_samples_buffer[0];
	currA.two_samples_buffer[0]= ADCreadings[0]; //i suspect ADC measurements are one sample late, because of the ADC being triggered at the end of the TIM interruption

	currB.two_samples_buffer[1]=currB.two_samples_buffer[0];
	currB.two_samples_buffer[0]= ADCreadings[1];


	hallA.two_samples_buffer[1]=hallA.two_samples_buffer[0];
	hallA.two_samples_buffer[0]=HAL_GPIO_ReadPin(input_hall_A_GPIO_Port, input_hall_A_Pin);

	hallB.two_samples_buffer[1]=hallB.two_samples_buffer[0];
	hallB.two_samples_buffer[0]=HAL_GPIO_ReadPin(input_hall_B_GPIO_Port, input_hall_B_Pin);

	hallC.two_samples_buffer[1]=hallC.two_samples_buffer[0];
	hallC.two_samples_buffer[0]=HAL_GPIO_ReadPin(input_hall_C_GPIO_Port, input_hall_C_Pin);
}


void detect_all_zerocrossings(){
	if(ticks>2){ //skip the first two samples to fill the buffers
		detect_current_zerocrossings(&currA);
		detect_current_zerocrossings(&currB);

		detect_hall_zerocrossings(&hallA);
		detect_hall_zerocrossings(&hallB);
		detect_hall_zerocrossings(&hallC);
	}
}

void detect_current_zerocrossings(current_measurements_struct* currx){
	if(		((int32_t)(currx->two_samples_buffer[0]-currentADCoffset)*
			 (int32_t)(currx->two_samples_buffer[1]-currentADCoffset))<=0){
		if(currx->numberof_zerocrossings==0){
			if(currx->numberof_zerocrossings<MAXZEROCROSSINGS){
					currx->zerocrossings_tick[currx->numberof_zerocrossings]=ticks;
					if(currx->two_samples_buffer[0]>currx->two_samples_buffer[1]){
						currx->zerocrossings_polarity[currx->numberof_zerocrossings]=rising_polarity;
					}else{
						currx->zerocrossings_polarity[currx->numberof_zerocrossings]=falling_polarity;
					}
					currx->numberof_zerocrossings++;
			}
		}else{
			if((currx->numberof_zerocrossings<MAXZEROCROSSINGS) &&
			   (int32_t)(ticks-(currx->zerocrossings_tick[currx->numberof_zerocrossings-1]))>lowpassfilter_ticks){
					currx->zerocrossings_tick[currx->numberof_zerocrossings]=ticks;
					if(currx->two_samples_buffer[0]>currx->two_samples_buffer[1]){
						currx->zerocrossings_polarity[currx->numberof_zerocrossings]=rising_polarity;
					}else{
						currx->zerocrossings_polarity[currx->numberof_zerocrossings]=falling_polarity;
					}
					currx->numberof_zerocrossings++;
			}
		}
	}
}

void detect_hall_zerocrossings(hall_measurements_struct* hallx){
	if(hallx->two_samples_buffer[0]!=hallx->two_samples_buffer[1]){
		if(hallx->numberof_zerocrossings<MAXZEROCROSSINGS){
			hallx->zerocrossings_tick[hallx->numberof_zerocrossings]=ticks;
			if(hallx->two_samples_buffer[0]==GPIO_PIN_SET){
				hallx->zerocrossings_polarity[hallx->numberof_zerocrossings]=rising_polarity;
			}else{
				hallx->zerocrossings_polarity[hallx->numberof_zerocrossings]=falling_polarity;
			}
			hallx->numberof_zerocrossings++;
		}
	}
}


void end_detection(detection_state_enum* enabled_or_disabled){
	if(
			(currA.numberof_zerocrossings>=MAXZEROCROSSINGS) &&
			(currB.numberof_zerocrossings>=MAXZEROCROSSINGS) &&
			(hallA.numberof_zerocrossings>=MAXZEROCROSSINGS) &&
			(hallB.numberof_zerocrossings>=MAXZEROCROSSINGS) &&
			(hallC.numberof_zerocrossings>=MAXZEROCROSSINGS)
	){
		*enabled_or_disabled=detection_DISABLED; //end of detection
	}else if(ticks>=MAXTICKs){
		*enabled_or_disabled=detection_DISABLED;
	}
}

void evaluate_and_present_results(detection_state_enum* enabled_or_disabled){
	if(*enabled_or_disabled==detection_DISABLED){
		results.electricPeriod_ms=fromTicksToMiliseconds(calculateElectricPeriod_inTicks());
		HAL_UART_Transmit(&huart2, donemessage, sizeof(donemessage), 100);
	}

}


uint32_t calculateElectricPeriod_inTicks(){
	uint32_t averagedsemiPeriod=0;
	for (uint32_t i = 0; i < MAXZEROCROSSINGS-1; ++i) {
		averagedsemiPeriod+=(currA.zerocrossings_tick[i+1]-currA.zerocrossings_tick[i]);
		averagedsemiPeriod+=(currB.zerocrossings_tick[i+1]-currB.zerocrossings_tick[i]);

		averagedsemiPeriod+=(hallA.zerocrossings_tick[i+1]-hallA.zerocrossings_tick[i]);
		averagedsemiPeriod+=(hallB.zerocrossings_tick[i+1]-hallB.zerocrossings_tick[i]);
		averagedsemiPeriod+=(hallC.zerocrossings_tick[i+1]-hallC.zerocrossings_tick[i]);
	}

	averagedsemiPeriod/=(MAXZEROCROSSINGS-1);
	averagedsemiPeriod/=5;

	return averagedsemiPeriod*2; //FOR torrto emulated this should be 66*2
}

float fromTicksToMiliseconds(uint32_t ticks){
	float miliseconds=0;
	miliseconds=ticks*0.05;
	return miliseconds;
}

