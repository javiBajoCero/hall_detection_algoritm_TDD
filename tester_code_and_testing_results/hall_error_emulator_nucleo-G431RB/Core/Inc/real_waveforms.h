/*
 * real_waveforms.h
 *
 *  Created on: Nov 24, 2023
 *      Author: citcea
 */

#ifndef INC_REAL_WAVEFORMS_H_
#define INC_REAL_WAVEFORMS_H_


extern const uint32_t current_A [132];
extern const uint32_t current_B [132];
extern const uint32_t current_C [132];

extern GPIO_PinState HALL_A [132];
extern GPIO_PinState HALL_B [132];
extern GPIO_PinState HALL_C [132];

extern uint32_t dma_index;

#endif /* INC_REAL_WAVEFORMS_H_ */
