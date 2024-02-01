/*
 * hall_detection.c
 *
 *  Created on: Jan 30, 2024
 *      Author: javier.munoz.saez@upc.edu
 */

#include "hall_detection.h" 		/*!< contains all enums, structs and includes neccesary for this .c, it also exposes the run_hall_detection_inside_20Khz_interruption() function to the outside world */

//local defines
#define MAXTICKs 1000 								/*!< timeout for the algorithm, measured in 0.05s ticks , so 1000*0.05=50ms (it takes only 380 ticks to get 6/2=3 electric periods from torrot emulated) */
#define lowpassfilter_ticks 10 						/*!< minimum number of ticks that should have passed in between zerocrossings (lowpassfilter) */
#define currentAplusBplusC (uint32_t)(3*(4096)/2)	/*!< the sum of the average of each currents should be this number aprox, used to calculate currentC ortogonally*/

//local variables
uint32_t ticks=0; 								/*!< ticks are the time measurement unit of this algorithm, each run of the algorithm (0.05s) the tick is incremented +1 */
uint32_t currentADCoffset=(4096-1)/2;			/*!< the current waves zerocross is assumed to be at 3,3v/2= 1,65V, because thats how its defined in the emulator */
current_or_hall_measurements_struct currA;		/*!< contains all variables relevant for currA measurements*/
current_or_hall_measurements_struct currB;		/*!< contains all variables relevant for currB measurements*/
current_or_hall_measurements_struct currC;		/*!< contains all variables relevant for currC measurements*/
current_or_hall_measurements_struct hallA;		/*!< contains all variables relevant for hallA measurements*/
current_or_hall_measurements_struct hallB;		/*!< contains all variables relevant for hallB measurements*/
current_or_hall_measurements_struct hallC;		/*!< contains all variables relevant for hallC measurements*/
detection_results_struct results;				/*!< once the algorithm finishes, the detection results will be stored in this struct*/


//all functions declared inside this .c are listed here:
void run_hall_detection_inside_20Khz_interruption(detection_state_enum* enabled_or_disabled);
void signals_adquisition();
void detect_all_zerocrossings();
void detect_current_zerocrossings(current_or_hall_measurements_struct* currx);
void detect_hall_zerocrossings(current_or_hall_measurements_struct* hallx);
void end_detection(detection_state_enum* enabled_or_disabled);
void evaluate_and_present_results(detection_state_enum* enabled_or_disabled);
void calculateElectricPeriod_inTicks(uint32_t* resulting_period);
float fromTicksToMiliseconds(uint32_t ticks);
void assign_closest_phase_to_hall(detection_results_struct* res);
int32_t my_abs(int32_t x);
void assign_polarity(detection_results_struct* res);


/**
* \brief
* \param
*/
void run_hall_detection_inside_20Khz_interruption(detection_state_enum* enabled_or_disabled){
	if(*enabled_or_disabled==detection_ENABLED){
		signals_adquisition();
		detect_all_zerocrossings();
		end_detection(enabled_or_disabled);
		evaluate_and_present_results(enabled_or_disabled);
		ticks++;
	}

}

/**
* \brief
* \param
*/
void signals_adquisition(){
	currA.two_samples_buffer[1]=currA.two_samples_buffer[0];
	currA.two_samples_buffer[0]= ADCreadings[0]; //i suspect ADC measurements are one sample late, because of the ADC being triggered at the end of the TIM interruption

	currB.two_samples_buffer[1]=currB.two_samples_buffer[0];
	currB.two_samples_buffer[0]= ADCreadings[1];
	//c=-a-b, NO REAL MEASUREMENT OF CURRENT C, assuming ABC currents ortogonality:
	currC.two_samples_buffer[1]=currC.two_samples_buffer[0];
	currC.two_samples_buffer[0]= currentAplusBplusC-ADCreadings[0]-ADCreadings[1];

	hallA.two_samples_buffer[1]=hallA.two_samples_buffer[0];
	hallA.two_samples_buffer[0]=HAL_GPIO_ReadPin(input_hall_A_GPIO_Port, input_hall_A_Pin);

	hallB.two_samples_buffer[1]=hallB.two_samples_buffer[0];
	hallB.two_samples_buffer[0]=HAL_GPIO_ReadPin(input_hall_B_GPIO_Port, input_hall_B_Pin);

	hallC.two_samples_buffer[1]=hallC.two_samples_buffer[0];
	hallC.two_samples_buffer[0]=HAL_GPIO_ReadPin(input_hall_C_GPIO_Port, input_hall_C_Pin);
}

/**
* \brief
* \param
*/
void detect_all_zerocrossings(){
	if(ticks>2){ //skip the first two samples to fill the buffers
		detect_current_zerocrossings(&currA);
		detect_current_zerocrossings(&currB);
		detect_current_zerocrossings(&currC);
		detect_hall_zerocrossings(&hallA);
		detect_hall_zerocrossings(&hallB);
		detect_hall_zerocrossings(&hallC);
	}
}

/**
* \brief
* \param
*/
void detect_current_zerocrossings(current_or_hall_measurements_struct* currx){
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

/**
* \brief
* \param
*/
void detect_hall_zerocrossings(current_or_hall_measurements_struct* hallx){
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

/**
* \brief
* \param
*/
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

/**
* \brief
* \param
*/
void evaluate_and_present_results(detection_state_enum* enabled_or_disabled){
	if(*enabled_or_disabled==detection_DISABLED){
		calculateElectricPeriod_inTicks(&results.electricPeriod_ticks);
		assign_closest_phase_to_hall(&results);
		assign_polarity(&results);
	}
}

/**
* \brief
* \param
*/
void calculateElectricPeriod_inTicks(uint32_t* resulting_period){
	uint32_t averagedsemiPeriod=0;
	for (uint32_t i = 0; i < MAXZEROCROSSINGS-1; ++i) {
		averagedsemiPeriod+=(currA.zerocrossings_tick[i+1]-currA.zerocrossings_tick[i]);
		averagedsemiPeriod+=(currB.zerocrossings_tick[i+1]-currB.zerocrossings_tick[i]);
		averagedsemiPeriod+=(currC.zerocrossings_tick[i+1]-currC.zerocrossings_tick[i]);
		averagedsemiPeriod+=(hallA.zerocrossings_tick[i+1]-hallA.zerocrossings_tick[i]);
		averagedsemiPeriod+=(hallB.zerocrossings_tick[i+1]-hallB.zerocrossings_tick[i]);
		averagedsemiPeriod+=(hallC.zerocrossings_tick[i+1]-hallC.zerocrossings_tick[i]);
	}

	averagedsemiPeriod/=(MAXZEROCROSSINGS-1);
	averagedsemiPeriod/=5;

	*resulting_period=averagedsemiPeriod*2; //FOR torrot emulated this should be 66*2
}

/**
* \brief
* \param
*/
float fromTicksToMiliseconds(uint32_t ticks){
	float miliseconds=0;
	miliseconds=ticks*0.05;
	return miliseconds;
}

/**
* \brief
* \param
*/
void assign_closest_phase_to_hall(detection_results_struct* res){
	int32_t hall_orderA[number_of_phases]={0};
	int32_t hall_orderB[number_of_phases]={0};
	int32_t hall_orderC[number_of_phases]={0};

	for (uint32_t i = 0; i < MAXZEROCROSSINGS; ++i) {// all zerocrossings loop
		hall_orderA[phase_A]+=my_abs((int32_t)currA.zerocrossings_tick[i]-(int32_t)hallA.zerocrossings_tick[i]);
		hall_orderA[phase_B]+=my_abs((int32_t)currA.zerocrossings_tick[i]-(int32_t)hallB.zerocrossings_tick[i]);
		hall_orderA[phase_C]+=my_abs((int32_t)currA.zerocrossings_tick[i]-(int32_t)hallC.zerocrossings_tick[i]);

		hall_orderB[phase_A]+=my_abs((int32_t)currB.zerocrossings_tick[i]-(int32_t)hallA.zerocrossings_tick[i]);
		hall_orderB[phase_B]+=my_abs((int32_t)currB.zerocrossings_tick[i]-(int32_t)hallB.zerocrossings_tick[i]);
		hall_orderB[phase_C]+=my_abs((int32_t)currB.zerocrossings_tick[i]-(int32_t)hallC.zerocrossings_tick[i]);

		hall_orderC[phase_A]+=my_abs((int32_t)currC.zerocrossings_tick[i]-(int32_t)hallA.zerocrossings_tick[i]);
		hall_orderC[phase_B]+=my_abs((int32_t)currC.zerocrossings_tick[i]-(int32_t)hallB.zerocrossings_tick[i]);
		hall_orderC[phase_C]+=my_abs((int32_t)currC.zerocrossings_tick[i]-(int32_t)hallC.zerocrossings_tick[i]);
	}

	for (uint32_t i = 0; i < number_of_phases; ++i) {
		hall_orderA[i]/=MAXZEROCROSSINGS;
		hall_orderB[i]/=MAXZEROCROSSINGS;
		hall_orderC[i]/=MAXZEROCROSSINGS;

		if(hall_orderA[i]>((res->electricPeriod_ticks/2)-lowpassfilter_ticks) && hall_orderA[i]<(res->electricPeriod_ticks)){
			hall_orderA[i]-=(res->electricPeriod_ticks/2)-lowpassfilter_ticks;
		}

		if(hall_orderB[i]>((res->electricPeriod_ticks/2)-lowpassfilter_ticks) && hall_orderB[i]<(res->electricPeriod_ticks)){
			hall_orderB[i]-=(res->electricPeriod_ticks/2)-lowpassfilter_ticks;
		}

		if(hall_orderC[i]>((res->electricPeriod_ticks/2)-lowpassfilter_ticks) && hall_orderC[i]<(res->electricPeriod_ticks)){
			hall_orderC[i]-=(res->electricPeriod_ticks/2)-lowpassfilter_ticks;
		}
	}

	int32_t minimum=0xFF;
	for (uint32_t i = 0; i < number_of_phases; ++i) {
		if(hall_orderA[i]<minimum){
			minimum=hall_orderA[i];
			res->hall_order[phase_A]=i;
		}
	}

	minimum=0xFF;
	for (uint32_t i = 0; i < number_of_phases; ++i) {
		if(hall_orderB[i]<minimum){
			minimum=hall_orderB[i];
			res->hall_order[phase_B]=i;
		}
	}

	minimum=0xFF;
	for (uint32_t i = 0; i < number_of_phases; ++i) {
		if(hall_orderC[i]<minimum){
			minimum=hall_orderC[i];
			res->hall_order[phase_C]=i;
		}
	}
}

/**
* \brief
* \param
*/
int32_t my_abs(int32_t x){
    return (int32_t)x < (int32_t)0 ? -x : x;
}

/**
* \brief
* \param
*/
void assign_polarity(detection_results_struct* res){

	if (res->hall_order[phase_A]==hall_A) {
		if(currA.zerocrossings_polarity[0]==hallA.zerocrossings_polarity[0]){
			res->hall_polarity[phase_A]=hall_direct;
		}else{
			res->hall_polarity[phase_A]=hall_inverse;
		}
	}else if (res->hall_order[phase_A]==hall_B) {
		if(currA.zerocrossings_polarity[0]==hallB.zerocrossings_polarity[0]){
			res->hall_polarity[phase_A]=hall_direct;
		}else{
			res->hall_polarity[phase_A]=hall_inverse;
		}
	}else if (res->hall_order[phase_A]==hall_C) {
		if(currA.zerocrossings_polarity[0]==hallC.zerocrossings_polarity[0]){
			res->hall_polarity[phase_A]=hall_direct;
		}else{
			res->hall_polarity[phase_A]=hall_inverse;
		}
	}

	if (res->hall_order[phase_B]==hall_A) {
		if(currB.zerocrossings_polarity[0]==hallA.zerocrossings_polarity[0]){
			res->hall_polarity[phase_B]=hall_direct;
		}else{
			res->hall_polarity[phase_B]=hall_inverse;
		}
	}else if (res->hall_order[phase_B]==hall_B) {
		if(currB.zerocrossings_polarity[0]==hallB.zerocrossings_polarity[0]){
			res->hall_polarity[phase_B]=hall_direct;
		}else{
			res->hall_polarity[phase_B]=hall_inverse;
		}
	}else if (res->hall_order[phase_B]==hall_C) {
		if(currB.zerocrossings_polarity[0]==hallC.zerocrossings_polarity[0]){
			res->hall_polarity[phase_B]=hall_direct;
		}else{
			res->hall_polarity[phase_B]=hall_inverse;
		}
	}

	if (res->hall_order[phase_C]==hall_A) {
		if(currC.zerocrossings_polarity[0]==hallA.zerocrossings_polarity[0]){
			res->hall_polarity[phase_C]=hall_direct;
		}else{
			res->hall_polarity[phase_C]=hall_inverse;
		}
	}else if (res->hall_order[phase_C]==hall_B) {
		if(currC.zerocrossings_polarity[0]==hallB.zerocrossings_polarity[0]){
			res->hall_polarity[phase_C]=hall_direct;
		}else{
			res->hall_polarity[phase_C]=hall_inverse;
		}
	}else if (res->hall_order[phase_C]==hall_C) {
		if(currC.zerocrossings_polarity[0]==hallC.zerocrossings_polarity[0]){
			res->hall_polarity[phase_C]=hall_direct;
		}else{
			res->hall_polarity[phase_C]=hall_inverse;
		}
	}

}
