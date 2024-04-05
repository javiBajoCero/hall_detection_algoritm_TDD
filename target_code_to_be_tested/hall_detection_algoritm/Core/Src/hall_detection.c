/*
 * hall_detection.c
 *
 *  Created on: Jan 30, 2024
 *      Author: javier.munoz.saez@upc.edu
 */

#include "hall_detection.h" 		/*!< contains all enums, structs and includes neccesary for this .c, it also exposes the run_hall_detection_inside_20Khz_interruption() function to the outside world */

//#define TESTuart
//uncomment only one
//#define REAL_PHASES_A_B_calculated_C		/*!< our ADC is reading real current signals A and B, C=-A-B */
#define REAL_PHASES_A_C_calculated_B		/*!< our ADC is reading real current signals A and C, B=-A-C */
//#define REAL_PHASES_B_C_calculated_A		/*!< our ADC is reading real current signals B and C, A=-B-C */
#define TESTuart
#ifdef TESTuart
#include "usart.h"
#endif

#define TESTnodelay
#ifdef TESTuart
#define INITIAL_WAIT_TICKS 0
#elif
#define INITIAL_WAIT_TICKS 3*20000
#endif
//local defines

#define MAXTICKs 1*20000 +INITIAL_WAIT_TICKS			/*!< timeout for the algorithm, measured in 0.05s ticks , so 1000*0.05=50ms (it takes only 380 ticks to get 6/2=3 electric periods from torrot emulated) */
#define lowpassfilter_ticks 30 						/*!< minimum number of ticks that should have passed in between zerocrossings (lowpassfilter) */
#define currentAplusBplusC (uint32_t)(3*(4096)/2)	/*!< the sum of the average of each currents should be this number aprox, used to calculate currentC ortogonally*/
#define messageLength (number_of_phases*2)+2		/*!< ASCII results char array length to use with UART*/

//local variables
detection_state_enum detection_state=detection_DISABLED;	/*!< main enable disable variable, when it flips to enabled it starts the detection, when the detection is finished it flips back to disabled*/

uint32_t ticks=0; 								/*!< ticks are the time measurement unit of this algorithm, each run of the algorithm (0.05s) the tick is incremented +1 */
float currentADCoffset=0.0;						/*!< as we are using already processed filtered currents, no offset is take into account */
current_or_hall_measurements_struct currA;		/*!< contains all variables relevant for currA measurements*/
current_or_hall_measurements_struct currB;		/*!< contains all variables relevant for currB measurements*/
current_or_hall_measurements_struct currC;		/*!< contains all variables relevant for currC measurements*/
hall_measurements_struct hallA;		/*!< contains all variables relevant for hallA measurements*/
hall_measurements_struct hallB;		/*!< contains all variables relevant for hallB measurements*/
hall_measurements_struct hallC;		/*!< contains all variables relevant for hallC measurements*/
detection_results_struct results;				/*!< once the algorithm finishes, the detection results will be stored in this struct*/
const detection_results_struct emptyresults={0};
const hall_measurements_struct empty_hall_measurements={0};
const current_or_hall_measurements_struct empty_current_measurements={0};

uint8_t message[messageLength];					/*!< ASCII results char array to use with UART*/


int32_t differences_phaseA[number_of_phases]={0};	/*!< auxiliar variable storing average distances between zerocrossings between each hall and phaseA*/
int32_t differences_phaseB[number_of_phases]={0};	/*!< auxiliar variable storing average distances between zerocrossings between each hall and phaseB*/
int32_t differences_phaseC[number_of_phases]={0};	/*!< auxiliar variable storing average distances between zerocrossings between each hall and phaseC*/

uint32_t toggled_polarity[number_of_phases]={0};	/*!< when phase and hall signals are measured out of phase due to unlucky timming when detecting, it is noted down in this variable for the polarity calculation */

current_or_hall_measurements_struct *currents[number_of_phases]={&currA,&currB,&currC};
hall_measurements_struct *halls[number_of_phases]={&hallA,&hallB,&hallC};

//all functions declared inside this .c are listed here:
void Hall_start_detection();
uint32_t Hall_is_detection_finished();

void Hall_Identification_Test_measurement(
		hall_pin_info* H1_gpio,
		hall_pin_info* H2_gpio,
		hall_pin_info* H3_gpio,
		volatile float* ADCcurr1,
		volatile float* ADCcurr2
		);

void signals_adquisition(
		hall_pin_info* H1_gpio,
		hall_pin_info* H2_gpio,
		hall_pin_info* H3_gpio,
		volatile float* ADCcurr1,
		volatile float* ADCcurr2
		);

void detect_all_zerocrossings();
void detect_current_zerocrossings(current_or_hall_measurements_struct* currx);
void detect_hall_zerocrossings(hall_measurements_struct* hallx);
void end_detection(detection_state_enum* enabled_or_disabled);

void evaluate_and_present_results(	detection_state_enum* enabled_or_disabled,
									hall_pin_info* H1_gpio,
									hall_pin_info* H2_gpio,
									hall_pin_info* H3_gpio
									);
void present_results_uart();

void swap_hall_gpios_with_detected_results(hall_pin_info* pin_infoH1,hall_pin_info* pin_infoH2,hall_pin_info* pin_infoH3);
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
	ticks=0;
	results=emptyresults;
	currA=empty_current_measurements;
	currB=empty_current_measurements;
	currC=empty_current_measurements;
	hallA=empty_hall_measurements;
	hallB=empty_hall_measurements;
	hallC=empty_hall_measurements;
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
		hall_pin_info* H1_gpio,
		hall_pin_info* H2_gpio,
		hall_pin_info* H3_gpio,
		volatile float* ADCcurr1,
		volatile float* ADCcurr2
		){
	if(detection_state==detection_ENABLED){
		if(ticks>INITIAL_WAIT_TICKS){
			signals_adquisition(H1_gpio,H2_gpio,H3_gpio,ADCcurr1,ADCcurr2);
			detect_all_zerocrossings();
			end_detection(&detection_state);
			evaluate_and_present_results(&detection_state,H1_gpio,H2_gpio,H3_gpio);
		}
		ticks++;
	}

}

/**
* \brief
* \param
*/
void signals_adquisition(
		hall_pin_info* H1_gpio,
		hall_pin_info* H2_gpio,
		hall_pin_info* H3_gpio,
		volatile float* ADCcurr1,
		volatile float* ADCcurr2
		){

#ifdef REAL_PHASES_A_B_calculated_C
	currA.two_samples_buffer[1]=currA.two_samples_buffer[0];
	currA.two_samples_buffer[0]= *ADCcurr1; //i suspect ADC measurements are one sample late, because of the ADC being triggered at the end of the TIM interruption

	currB.two_samples_buffer[1]=currB.two_samples_buffer[0];
	currB.two_samples_buffer[0]= *ADCcurr2;
	//c=-a-b, NO REAL MEASUREMENT OF CURRENT C, assuming ABC currents ortogonality:
	currC.two_samples_buffer[1]=currC.two_samples_buffer[0];
	currC.two_samples_buffer[0]= currentAplusBplusC-*ADCcurr1-*ADCcurr2;
#endif

#ifdef REAL_PHASES_A_C_calculated_B
	currA.two_samples_buffer[1]=currA.two_samples_buffer[0];
	currA.two_samples_buffer[0]= *ADCcurr1;
	//b=-a-c, NO REAL MEASUREMENT OF CURRENT B, assuming ABC currents ortogonality:
	currB.two_samples_buffer[1]=currB.two_samples_buffer[0];
	currB.two_samples_buffer[0]= -*ADCcurr1-*ADCcurr2;

	currC.two_samples_buffer[1]=currC.two_samples_buffer[0];
	currC.two_samples_buffer[0]= *ADCcurr2;
#endif

#ifdef REAL_PHASES_B_C_calculated_A
	//a=-b-bc, NO REAL MEASUREMENT OF CURRENT A, assuming ABC currents ortogonality:
	currA.two_samples_buffer[1]=currA.two_samples_buffer[0];
	currA.two_samples_buffer[0]= currentAplusBplusC-*ADCcurr1-*ADCcurr2;

	currB.two_samples_buffer[1]=currB.two_samples_buffer[0];
	currB.two_samples_buffer[0]= *ADCcurr1;

	currC.two_samples_buffer[1]=currC.two_samples_buffer[0];
	currC.two_samples_buffer[0]= *ADCcurr2;
#endif


	hallA.two_samples_buffer[1]=hallA.two_samples_buffer[0];
	hallA.two_samples_buffer[0]= H1_gpio->state;

	hallB.two_samples_buffer[1]=hallB.two_samples_buffer[0];
	hallB.two_samples_buffer[0]= H2_gpio->state;

	hallC.two_samples_buffer[1]=hallC.two_samples_buffer[0];
	hallC.two_samples_buffer[0]= H3_gpio->state;
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
	if(	   (((currx->two_samples_buffer[0]-currentADCoffset)*
			 (currx->two_samples_buffer[1]-currentADCoffset))<=0)
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
void detect_hall_zerocrossings(hall_measurements_struct* hallx){
	if(hallx->two_samples_buffer[0]!=hallx->two_samples_buffer[1]){
		if(hallx->numberof_zerocrossings<MAXZEROCROSSINGS){
			hallx->zerocrossings_tick[hallx->numberof_zerocrossings]=ticks;
			if(hallx->two_samples_buffer[0]!=0){
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
void evaluate_and_present_results(
		detection_state_enum* enabled_or_disabled,
		hall_pin_info* H1_gpio,
		hall_pin_info* H2_gpio,
		hall_pin_info* H3_gpio){
	if(*enabled_or_disabled==detection_DISABLED){
		calculateElectricPeriod_inTicks(&results.electricPeriod_ticks);
		assign_closest_phase_to_hall(&results);
		assign_polarity(&results);
		present_results();
		swap_hall_gpios_with_detected_results(H1_gpio,H2_gpio,H3_gpio);
	}
}

/**
* \brief
* \param
*/
void swap_hall_gpios_with_detected_results(hall_pin_info* pin_infoH1,hall_pin_info* pin_infoH2,hall_pin_info* pin_infoH3){
	hall_pin_info old_pin_infoH1=*pin_infoH1;
	hall_pin_info old_pin_infoH2=*pin_infoH2;
	hall_pin_info old_pin_infoH3=*pin_infoH3;

	switch (results.hall_order[phase_A]) {
		case hall_A:
			*pin_infoH1=old_pin_infoH1;
			break;
		case hall_B:
			*pin_infoH2=old_pin_infoH1;
			break;
		case hall_C:
			*pin_infoH3=old_pin_infoH1;
			break;
		default:
			break;
	}

	switch (results.hall_order[phase_B]) {
		case hall_A:
			*pin_infoH1=old_pin_infoH2;
			break;
		case hall_B:
			*pin_infoH2=old_pin_infoH2;
			break;
		case hall_C:
			*pin_infoH3=old_pin_infoH2;
			break;
		default:
			break;
	}

	switch (results.hall_order[phase_C]) {
		case hall_A:
			*pin_infoH1=old_pin_infoH3;
			break;
		case hall_B:
			*pin_infoH2=old_pin_infoH3;
			break;
		case hall_C:
			*pin_infoH3=old_pin_infoH3;
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

		if(differences_phaseA[i]>((int32_t)(res->electricPeriod_ticks/2)-lowpassfilter_ticks) && differences_phaseA[i]<(int32_t)(res->electricPeriod_ticks)){
			differences_phaseA[i]=(int32_t)(res->electricPeriod_ticks/2)%lowpassfilter_ticks;
			toggled_polarity[hall_A]=1;

		}

		if(differences_phaseB[i]>((int32_t)(res->electricPeriod_ticks/2)-lowpassfilter_ticks) && differences_phaseB[i]<(int32_t)(res->electricPeriod_ticks)){
			differences_phaseB[i]=(int32_t)(res->electricPeriod_ticks/2)%lowpassfilter_ticks;
			toggled_polarity[hall_B]=1;
		}

		if(differences_phaseC[i]>((int32_t)(res->electricPeriod_ticks/2)-lowpassfilter_ticks) && differences_phaseC[i]<(int32_t)(res->electricPeriod_ticks)){
			differences_phaseC[i]=(int32_t)(res->electricPeriod_ticks/2)%lowpassfilter_ticks;
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
			if(toggled_polarity[res->hall_order[i]]==0){
				res->hall_polarity[res->hall_order[i]]=hall_inverse;
			}else{
				res->hall_polarity[res->hall_order[i]]=hall_direct;
			}
		}else{
			if(toggled_polarity[res->hall_order[i]]==0){
				res->hall_polarity[res->hall_order[i]]=hall_direct;
			}else{
				res->hall_polarity[res->hall_order[i]]=hall_inverse;
			}
		}
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



}

void present_results_uart(){
#ifdef TESTuart
	HAL_UART_Transmit_DMA(&huart2, (const uint8_t *)&message, messageLength);
	HAL_Delay(10);
#endif
}
