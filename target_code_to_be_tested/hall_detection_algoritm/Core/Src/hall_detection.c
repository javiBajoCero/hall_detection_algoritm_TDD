/*
 * hall_detection.c
 *
 *  Created on: Jan 30, 2024
 *      Author: javier.munoz.saez@upc.edu
 */

#include "hall_detection.h" 		/*!< contains all enums, structs and includes neccesary for this .c, it also exposes the run_hall_detection_inside_20Khz_interruption() function to the outside world */

#define TESTuart

#ifdef TESTuart
#include "usart.h"
#endif

//local defines
#define MAXTICKs 1000 								/*!< timeout for the algorithm, measured in 0.05s ticks , so 1000*0.05=50ms (it takes only 380 ticks to get 6/2=3 electric periods from torrot emulated) */
#define lowpassfilter_ticks 10 						/*!< minimum number of ticks that should have passed in between zerocrossings (lowpassfilter) */
#define currentAplusBplusC (uint32_t)(3*(4096)/2)	/*!< the sum of the average of each currents should be this number aprox, used to calculate currentC ortogonally*/
#define messageLength (number_of_phases*2)+2		/*!< ASCII results char array length to use with UART*/

//local variables
detection_state_enum detection_state=detection_DISABLED;	/*!< main enable disable variable, when it flips to enabled it starts the detection, when the detection is finished it flips back to disabled*/

uint32_t ticks=0; 								/*!< ticks are the time measurement unit of this algorithm, each run of the algorithm (0.05s) the tick is incremented +1 */
uint32_t currentADCoffset=(4096-1)/2;			/*!< the current waves zerocross is assumed to be at 3,3v/2= 1,65V, because thats how its defined in the emulator */
current_or_hall_measurements_struct currA;		/*!< contains all variables relevant for currA measurements*/
current_or_hall_measurements_struct currB;		/*!< contains all variables relevant for currB measurements*/
current_or_hall_measurements_struct currC;		/*!< contains all variables relevant for currC measurements*/
current_or_hall_measurements_struct hallA;		/*!< contains all variables relevant for hallA measurements*/
current_or_hall_measurements_struct hallB;		/*!< contains all variables relevant for hallB measurements*/
current_or_hall_measurements_struct hallC;		/*!< contains all variables relevant for hallC measurements*/
detection_results_struct results;				/*!< once the algorithm finishes, the detection results will be stored in this struct*/

uint8_t message[messageLength];					/*!< ASCII results char array to use with UART*/


int32_t differences_phaseA[number_of_phases]={0};	/*!< auxiliar variable storing average distances between zerocrossings between each hall and phaseA*/
int32_t differences_phaseB[number_of_phases]={0};	/*!< auxiliar variable storing average distances between zerocrossings between each hall and phaseB*/
int32_t differences_phaseC[number_of_phases]={0};	/*!< auxiliar variable storing average distances between zerocrossings between each hall and phaseC*/

uint32_t toggled_polarity[number_of_phases]={0};	/*!< when phase and hall signals are measured out of phase due to unlucky timming when detecting, it is noted down in this variable for the polarity calculation */

current_or_hall_measurements_struct *currents[number_of_phases]={&currA,&currB,&currC};
current_or_hall_measurements_struct *halls[number_of_phases]={&hallA,&hallB,&hallC};

//all functions declared inside this .c are listed here:
void Hall_start_detection();
uint32_t Hall_is_detection_finished();

void Hall_Identification_Test_measurement(
		hall_pin_info* H1,
		hall_pin_info* H2,
		hall_pin_info* H3,
		uint16_t* ADCcurrA,
		uint16_t* ADCcurrB
		);

void signals_adquisition(
		hall_pin_info* H1,
		hall_pin_info* H2,
		hall_pin_info* H3,
		uint16_t* ADCcurrA,
		uint16_t* ADCcurrB
		);

void detect_all_zerocrossings();
void detect_current_zerocrossings(current_or_hall_measurements_struct* currx);
void detect_hall_zerocrossings(current_or_hall_measurements_struct* hallx);
void end_detection(detection_state_enum* enabled_or_disabled);

void evaluate_and_present_results(	detection_state_enum* enabled_or_disabled,
									hall_pin_info* H1,
									hall_pin_info* H2,
									hall_pin_info* H3
									);

void swap_hall_gpios_with_detected_results(hall_pin_info* H1,hall_pin_info* H2,hall_pin_info* H3);
void calculateElectricPeriod_inTicks(uint32_t* resulting_period);
void assign_closest_phase_to_hall(detection_results_struct* res);
int32_t absolute(int32_t x);
void assign_polarity(detection_results_struct* res);
void present_results();

/**
* \brief
* \param
*/
void Hall_start_detection(){
	detection_state=detection_ENABLED;
}

/**
* \brief
* \param
*/
uint32_t Hall_is_detection_finished(){
	if(detection_state==detection_ENABLED){
		return 0;
	}else{
		return 1;
	}
}
/**
* \brief
* \param
*/
void Hall_Identification_Test_measurement(
		hall_pin_info* H1,
		hall_pin_info* H2,
		hall_pin_info* H3,
		uint16_t* ADCcurrA,
		uint16_t* ADCcurrB
		){
	if(detection_state==detection_ENABLED){
		signals_adquisition(H1,H2,H3,ADCcurrA,ADCcurrB);
		detect_all_zerocrossings();
		end_detection(&detection_state);
		evaluate_and_present_results(&detection_state,H1,H2,H3);
		ticks++;
	}

}

/**
* \brief
* \param
*/
void signals_adquisition(
		hall_pin_info* H1,
		hall_pin_info* H2,
		hall_pin_info* H3,
		uint16_t* ADCcurrA,
		uint16_t* ADCcurrB
		){
	currA.two_samples_buffer[1]=currA.two_samples_buffer[0];
	currA.two_samples_buffer[0]= *ADCcurrA; //i suspect ADC measurements are one sample late, because of the ADC being triggered at the end of the TIM interruption

	currB.two_samples_buffer[1]=currB.two_samples_buffer[0];
	currB.two_samples_buffer[0]= *ADCcurrB;
	//c=-a-b, NO REAL MEASUREMENT OF CURRENT C, assuming ABC currents ortogonality:
	currC.two_samples_buffer[1]=currC.two_samples_buffer[0];
	currC.two_samples_buffer[0]= currentAplusBplusC-*ADCcurrA-*ADCcurrB;

	hallA.two_samples_buffer[1]=hallA.two_samples_buffer[0];
	hallA.two_samples_buffer[0]=HAL_GPIO_ReadPin(H1->gpio_port, H1->gpio_pin);

	hallB.two_samples_buffer[1]=hallB.two_samples_buffer[0];
	hallB.two_samples_buffer[0]=HAL_GPIO_ReadPin(H2->gpio_port, H2->gpio_pin);

	hallC.two_samples_buffer[1]=hallC.two_samples_buffer[0];
	hallC.two_samples_buffer[0]=HAL_GPIO_ReadPin(H3->gpio_port, H3->gpio_pin);
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
	if(	   (((int32_t)(currx->two_samples_buffer[0]-currentADCoffset)*
			 (int32_t)(currx->two_samples_buffer[1]-currentADCoffset))<=0)
			 && currx->numberof_zerocrossings<MAXZEROCROSSINGS
			){

		if(currx->numberof_zerocrossings==0){			//only if this is the first zerocrossing detected
					currx->zerocrossings_tick[currx->numberof_zerocrossings]=ticks;
					if(currx->two_samples_buffer[0]>currx->two_samples_buffer[1]){
						currx->zerocrossings_polarity[currx->numberof_zerocrossings]=rising_polarity;
					}else{
						currx->zerocrossings_polarity[currx->numberof_zerocrossings]=falling_polarity;
					}
					currx->numberof_zerocrossings++;
		}else{
			if((int32_t)(ticks-(currx->zerocrossings_tick[currx->numberof_zerocrossings-1]))>lowpassfilter_ticks){ //not the first zerocrossing, compare with the previous one to filter noisy signals
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
			(currC.numberof_zerocrossings>=MAXZEROCROSSINGS) &&
			(hallA.numberof_zerocrossings>=MAXZEROCROSSINGS) &&
			(hallB.numberof_zerocrossings>=MAXZEROCROSSINGS) &&
			(hallC.numberof_zerocrossings>=MAXZEROCROSSINGS)
	){
		*enabled_or_disabled=detection_DISABLED; //end of detection because the zerocrossing buffers are full
	}else if(ticks>=MAXTICKs){
		*enabled_or_disabled=detection_DISABLED; //end of detection because of timeout
	}
}

/**
* \brief
* \param
*/
void evaluate_and_present_results(detection_state_enum* enabled_or_disabled,hall_pin_info* H1,hall_pin_info* H2,hall_pin_info* H3){
	if(*enabled_or_disabled==detection_DISABLED){
		calculateElectricPeriod_inTicks(&results.electricPeriod_ticks);
		assign_closest_phase_to_hall(&results);
		assign_polarity(&results);
		//swap_hall_gpios_with_detected_results(H1,H2,H3);
		present_results();
	}
}

/**
* \brief
* \param
*/
void swap_hall_gpios_with_detected_results(hall_pin_info* H1,hall_pin_info* H2,hall_pin_info* H3){
	hall_pin_info _H1=*H1;
	hall_pin_info _H2=*H2;
	hall_pin_info _H3=*H3;

	switch (results.hall_order[phase_A]) {
		case hall_A:
			*H1=_H1;
			H1->polarity=results.hall_polarity[hall_A];
			break;
		case hall_B:
			*H1=_H2;
			H1->polarity=results.hall_polarity[hall_B];
			break;
		case hall_C:
			*H1=_H3;
			H1->polarity=results.hall_polarity[hall_C];
			break;
		default:
			break;
	}

	switch (results.hall_order[phase_B]) {
		case hall_A:
			*H2=_H1;
			H2->polarity=results.hall_polarity[hall_A];
			break;
		case hall_B:
			*H2=_H2;
			H2->polarity=results.hall_polarity[hall_B];
			break;
		case hall_C:
			*H2=_H3;
			H2->polarity=results.hall_polarity[hall_C];
			break;
		default:
			break;
	}

	switch (results.hall_order[phase_C]) {
		case hall_A:
			*H3=_H1;
			H3->polarity=results.hall_polarity[hall_A];
			break;
		case hall_B:
			*H3=_H2;
			H3->polarity=results.hall_polarity[hall_B];
			break;
		case hall_C:
			*H3=_H3;
			H3->polarity=results.hall_polarity[hall_C];
			break;
		default:
			break;
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
void assign_closest_phase_to_hall(detection_results_struct* res){


	for (uint32_t i = 0; i < MAXZEROCROSSINGS; ++i) {// all zerocrossings loop
		differences_phaseA[phase_A]+=absolute((int32_t)currA.zerocrossings_tick[i]-(int32_t)hallA.zerocrossings_tick[i]);
		differences_phaseA[phase_B]+=absolute((int32_t)currA.zerocrossings_tick[i]-(int32_t)hallB.zerocrossings_tick[i]);
		differences_phaseA[phase_C]+=absolute((int32_t)currA.zerocrossings_tick[i]-(int32_t)hallC.zerocrossings_tick[i]);

		differences_phaseB[phase_A]+=absolute((int32_t)currB.zerocrossings_tick[i]-(int32_t)hallA.zerocrossings_tick[i]);
		differences_phaseB[phase_B]+=absolute((int32_t)currB.zerocrossings_tick[i]-(int32_t)hallB.zerocrossings_tick[i]);
		differences_phaseB[phase_C]+=absolute((int32_t)currB.zerocrossings_tick[i]-(int32_t)hallC.zerocrossings_tick[i]);

		differences_phaseC[phase_A]+=absolute((int32_t)currC.zerocrossings_tick[i]-(int32_t)hallA.zerocrossings_tick[i]);
		differences_phaseC[phase_B]+=absolute((int32_t)currC.zerocrossings_tick[i]-(int32_t)hallB.zerocrossings_tick[i]);
		differences_phaseC[phase_C]+=absolute((int32_t)currC.zerocrossings_tick[i]-(int32_t)hallC.zerocrossings_tick[i]);
	}

	for (uint32_t i = 0; i < number_of_phases; ++i) {
		differences_phaseA[i]/=MAXZEROCROSSINGS;
		differences_phaseB[i]/=MAXZEROCROSSINGS;
		differences_phaseC[i]/=MAXZEROCROSSINGS;

		if(differences_phaseA[i]>((res->electricPeriod_ticks/2)-lowpassfilter_ticks) && differences_phaseA[i]<(res->electricPeriod_ticks)){
			differences_phaseA[i]=(res->electricPeriod_ticks/2)%lowpassfilter_ticks;
			toggled_polarity[hall_A]=1;

		}

		if(differences_phaseB[i]>((res->electricPeriod_ticks/2)-lowpassfilter_ticks) && differences_phaseB[i]<(res->electricPeriod_ticks)){
			differences_phaseB[i]=(res->electricPeriod_ticks/2)%lowpassfilter_ticks;
			toggled_polarity[hall_B]=1;
		}

		if(differences_phaseC[i]>((res->electricPeriod_ticks/2)-lowpassfilter_ticks) && differences_phaseC[i]<(res->electricPeriod_ticks)){
			differences_phaseC[i]=(res->electricPeriod_ticks/2)%lowpassfilter_ticks;
			toggled_polarity[hall_C]=1;
		}
	}

	int32_t minimum=0xFF;
	for (uint32_t i = 0; i < number_of_phases; ++i) {
		if(differences_phaseA[i]<minimum){
			minimum=differences_phaseA[i];
		}
	}
	for (uint32_t i = 0; i < number_of_phases; ++i) {
		if(differences_phaseA[i]==minimum){
			res->hall_order[i]=phase_A;
		}
	}

	minimum=0xFF;
	for (uint32_t i = 0; i < number_of_phases; ++i) {
		if(differences_phaseB[i]<minimum){
			minimum=differences_phaseB[i];
		}
	}
	for (uint32_t i = 0; i < number_of_phases; ++i) {
		if(differences_phaseB[i]==minimum){
			res->hall_order[i]=phase_B;
		}
	}

	minimum=0xFF;
	for (uint32_t i = 0; i < number_of_phases; ++i) {
		if(differences_phaseC[i]<minimum){
			minimum=differences_phaseC[i];
		}
	}
	for (uint32_t i = 0; i < number_of_phases; ++i) {
		if(differences_phaseC[i]==minimum){
			res->hall_order[i]=phase_C;
		}
	}
}

/**
* \brief
* \param
*/
int32_t absolute(int32_t x){
    return (int32_t)x < (int32_t)0 ? -x : x;
}

/**
* \brief
* \param
*/
void assign_polarity(detection_results_struct* res){

	for (uint32_t i = 0; i < number_of_phases; ++i) {
		if(currents[res->hall_order[i]]->zerocrossings_polarity[0]==halls[i]->zerocrossings_polarity[0]){
				res->hall_polarity[res->hall_order[i]]=hall_direct;
		}else{
				res->hall_polarity[res->hall_order[i]]=hall_inverse;
		}
		res->hall_polarity[i]^=toggled_polarity[i];//resolve toggled polarity due to aliasis?
	}
}

/**
* \brief
* \param
*/
void present_results(){

	for (uint32_t i = 0; i < messageLength; ++i) {
		message[i]=' ';
	}

	for (uint32_t i = 0; i < number_of_phases; ++i) {
		if(results.hall_order[i]==hall_A){
			message[(i*2)+1]='A';
		}else if(results.hall_order[i]==hall_B){
			message[(i*2)+1]='B';
		}else if(results.hall_order[i]==hall_C){
			message[(i*2)+1]='C';
		}

		if(results.hall_polarity[i]==hall_inverse){//the "inverse polarity" from the hall with the currents is actually the normal logic.
				message[i*2]=' ';
		}else if(results.hall_polarity[i]==hall_direct){
				message[i*2]='!';
		}
	}

	message[messageLength-2]='\n';//two last characters
	message[messageLength-1]='\r';

#ifdef TESTuart
	HAL_UART_Transmit_DMA(&huart2, (const uint8_t *)&message, messageLength);
#endif

}
