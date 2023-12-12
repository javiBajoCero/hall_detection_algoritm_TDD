/*
 * emulator_GPIOS_and_DACs.c
 *
 *  Created on: Nov 17, 2023
 *      Author: javier
 */
#include "hall_swapper.h"

#include "wave_emulator.h"

#include "gpio.h" /*!< to access all those HAL_GPIO... functions */

hall_signals_order 		signal_order[3]		={hall_A,hall_B,hall_C};	            /*!< HALL signals order */
hall_signals_polarity 	signal_polarity[3]	={hall_direct,hall_direct,hall_direct};	/*!< HALL signals polarity */

GPIO_PinState signal_inputs[3]	={GPIO_PIN_RESET,GPIO_PIN_RESET,GPIO_PIN_RESET};	/*!< once we read gpio inputs the detected value is stored here. */
GPIO_PinState signal_outputs[3]	={GPIO_PIN_RESET,GPIO_PIN_RESET,GPIO_PIN_RESET};	/*!< contains our translated signal_input_values, swapped and polariced. */



//all funtions inside this .c are listed here:
void hall_swapper_twentyKHzinterruptionIRQ( void );
void read_input_signals(void);
void emulated_read_input_signals(void);
void polarice_signals(hall_signals_order hall_x);
void swap_signals(hall_signals_order hall_x);
void write_output_signals();



/**
* \brief does the hall swapping and polarizing thing
*   this function should be called inside a 20Khz TIM interruption, in our case inside TIM8_UP_IRQHandler(void)
*/
void hall_swapper_twentyKHzinterruptionIRQ( void ){

	if(emulator_enabled==0){
		read_input_signals();		//read all gpio inputs into signal_inputs
	}else{
		emulated_read_input_signals();
	}

	polarice_signals(hall_A);	//apply signal inversions, polarice should come first
	polarice_signals(hall_B);
	polarice_signals(hall_C);
	swap_signals(hall_A);		//apply signal order swapping, swapping should come second
	swap_signals(hall_B);
	swap_signals(hall_C);

	write_output_signals();		//write signal outputs into signal_outputs

}

/**
* \brief simple GPIO read of all halll signals, stores readings into signal_inputs
*/
void read_input_signals(void){
	signal_inputs[hall_A]=HAL_GPIO_ReadPin(input_HALLA_GPIO_Port, input_HALLA_Pin);
	signal_inputs[hall_B]=HAL_GPIO_ReadPin(input_HALLB_GPIO_Port, input_HALLB_Pin);
	signal_inputs[hall_C]=HAL_GPIO_ReadPin(input_HALLC_GPIO_Port, input_HALLC_Pin);
}

/**
* \brief simple GPIO read of all halll signals, stores readings into signal_inputs
*/
void emulated_read_input_signals(void){
	signal_inputs[hall_A]=HALL_A[dma_index];
	signal_inputs[hall_B]=HALL_B[dma_index];
	signal_inputs[hall_C]=HALL_C[dma_index];

	if(dma_index>=(sizeof(HALL_A)/sizeof(HALL_A[0]))-1){//taking care of the circular buffer index reset
		dma_index=0;
	}else{
		dma_index++;
	}
}

/**
* \brief For a single hall_x signal: apply to signal_inputs the polarization noted by signal_polarity, direct or inverse.
* polarizing should come before swapping.
* \param hall_signals_order hall_x , reffers to the hall signal, could be hall_A/B/C
*/
void polarice_signals(hall_signals_order hall_x){

	if(signal_polarity[hall_x]==hall_inverse){		//check for inverse polarity flag
		if( signal_inputs[hall_x]==GPIO_PIN_RESET){	//inverted signal
			signal_inputs[hall_x]=GPIO_PIN_SET;
		}else{
			signal_inputs[hall_x]=GPIO_PIN_RESET;
		}
	}else{											//polarity is not inverted
		//not inverted signal
	}

}

/**
* \brief For a single hall_x signal: load signal_outputs with  signal_inputs values in the order noted by signal_order
* swapping should come after polarizing
* \param hall_signals_order hall_x , reffers to the hall signal, could be hall_A/B/C
*/
void swap_signals(hall_signals_order hall_x){
	if(signal_order[hall_x]==hall_A){
		signal_outputs[hall_x]=signal_inputs[hall_A];
	}else if(signal_order[hall_x]==hall_B){
		signal_outputs[hall_x]=signal_inputs[hall_B];
	}else if(signal_order[hall_x]==hall_C){
		signal_outputs[hall_x]=signal_inputs[hall_C];
	}
}

/**
* \brief simple GPIO write of all hall signals, writes gpios with signal_outputs values
*/
void write_output_signals(){
	HAL_GPIO_WritePin(output_HALLA_GPIO_Port, output_HALLA_Pin, signal_outputs[hall_A]);
	HAL_GPIO_WritePin(output_HALLB_GPIO_Port, output_HALLB_Pin, signal_outputs[hall_B]);
	HAL_GPIO_WritePin(output_HALLC_GPIO_Port, output_HALLC_Pin, signal_outputs[hall_C]);
}
