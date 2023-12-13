/*
 * wave_emulator.c
 *
 *  Created on: Nov 24, 2023
 *      Author: citcea
 */

#include "main.h"
#include "dac.h"
#include "dma.h"
#include "tim.h"

#include "wave_emulator.h"
#include "real_waveforms.h"


uint32_t emulator_enabled=0;			/*!< this variable is public, acts as a semaphore for the emulator  */
uint32_t old_emulator_enabled=0;		/*!< auxiliar local variable to store emulator_enabled values to be compared to*/

//all functions contained in this .c
void enable_emulator();
void disable_emulator();

/**
* \brief to be placed in main() superloop, constanly on the lookout for changes in emulator_enabled.
* manages the enable/disable function logic.
*/
void emulation(){
	if(emulator_enabled!=old_emulator_enabled){	//there is a change in emulator_enabled
		if(emulator_enabled==0){
			disable_emulator();
		}else{
			enable_emulator();
		}
		old_emulator_enabled=emulator_enabled;	//update old_emulator_enabled
	}
}

/**
* \brief disables tim8 , configures DAC+DMA in circular mode, reenables tim8
*/
void enable_emulator(){
		HAL_TIM_Base_Stop_IT(&htim8);
		HAL_DAC_Start_DMA(&hdac1, DAC_CHANNEL_1, (uint32_t *)&current_A, sizeof(current_A)/sizeof(current_A[0]), DAC_ALIGN_12B_R);
		HAL_DAC_Start_DMA(&hdac1, DAC_CHANNEL_2, (uint32_t *)&current_B, sizeof(current_B)/sizeof(current_B[0]), DAC_ALIGN_12B_R);
		HAL_TIM_Base_Start_IT(&htim8); //start 20Khz timer with enabled interruption (DAC+DMA trigger)

}

/**
* \brief stops DAC+DMA in circular mode
*/
void disable_emulator(){
	    HAL_TIM_Base_Stop_IT(&htim8);
		HAL_DAC_Stop_DMA(&hdac1, DAC_CHANNEL_1);
		HAL_DAC_Stop_DMA(&hdac1, DAC_CHANNEL_2);
		HAL_TIM_Base_Start_IT(&htim8); //start 20Khz timer with enabled interruption (DAC+DMA trigger)
}
