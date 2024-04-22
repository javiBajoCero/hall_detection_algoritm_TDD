/*
 * hall_detection.c
 *
 *  Created on: Jan 30, 2024
 *      Author: javier.munoz.saez@upc.edu
 */

#include "hall_detection.h" 		/*!< contains all enums, structs and includes neccesary for this .c, it also exposes the run_hall_detection_inside_20Khz_interruption() function to the outside world */

//Uncomment only one
//#define REAL_PHASES_A_B_calculated_C		/*!< our ADC is reading real current signals A and B, C=-A-B */
#define REAL_PHASES_A_C_calculated_B		/*!< our ADC is reading real current signals A and C, B=-A-C */
//#define REAL_PHASES_B_C_calculated_A		/*!< our ADC is reading real current signals B and C, A=-B-C */

#define TESTuart


#ifdef TESTuart
#include "usart.h"
#endif

#define MAXTICKs 1*888888		/*!< timeout for the algorithm, measured in 0.05s ticks , so 1000*0.05=50ms (it takes only 380 ticks to get 6/2=3 electric periods from torrot emulated) */
#define lowpassfilter_ticks 15 						/*!< minimum number of ticks that should have passed in between zerocrossings (lowpassfilter) */
#define currentAplusBplusC (uint32_t)(3*(4096)/2)	/*!< the sum of the average of each currents should be this number aprox, used to calculate currentC ortogonally*/
#define currentADCoffset (float) 0.0				/*!< as we are using already processed filtered currents, no offset is take into account */
#define TOLERANCE_FACTOR_FOR_STATIONARY_CURRENTS 0.1
#define WAITING_STATIONARY_MAXZEROCROSSINGS 3
#define NUMBER_OF_VALID_MATCHING_RESULTS 3




//LOCAL VARIABLES (only declared inside this .c file)
uint32_t ticks; 								/*!< ticks are the time measurement unit of this algorithm, each run of the algorithm (0.05s) the tick is incremented +1 */
detection_state_enum detection_state=detection_DISABLED;	/*!< main enable disable variable, when it flips to enabled it starts the detection, when the detection is finished it flips back to disabled*/
hall_detection_general_struct general={0};
const hall_detection_general_struct empty_general={0};









//PUBLIC FUNCTIONS (also declared in the .h file)
void Hall_start_detection();
uint32_t Hall_is_detection_finished();
void Hall_Identification_Test_measurement(
		hall_pin_info* H1_gpio,
		hall_pin_info* H2_gpio,
		hall_pin_info* H3_gpio,
		volatile float* ADCcurr1,
		volatile float* ADCcurr2
		);



//PRIVATE FUNCTIONS (only declared inside this .c file)
//state machine
void resetVariables					(hall_detection_general_struct *gen);
void wait_for_the_current_stationary(detection_state_enum* state,hall_detection_general_struct *gen,hall_pin_info* H1_gpio,hall_pin_info* H2_gpio,hall_pin_info* H3_gpio,volatile float* ADCcurr1,volatile float* ADCcurr2);
void adquisition					(detection_state_enum* state,hall_detection_general_struct *gen,hall_pin_info* H1_gpio,hall_pin_info* H2_gpio,hall_pin_info* H3_gpio,volatile float* ADCcurr1,volatile float* ADCcurr2);
void interpretation					(detection_state_enum* state,hall_detection_general_struct *gen);
void validation						(detection_state_enum* state,hall_detection_general_struct *gen);
void present_and_finish				(detection_state_enum* state,hall_detection_general_struct *gen);

void fill_buffers(
		hall_detection_general_struct *gen,
		hall_pin_info* H1_gpio,
		hall_pin_info* H2_gpio,
		hall_pin_info* H3_gpio,
		volatile float* ADCcurr1,
		volatile float* ADCcurr2
		);
detection_YES_NO detect_N_zerocrossings	(hall_detection_general_struct *gen,uint32_t N);
void detect_N_current_zerocrossings	(uint32_t ticks,current_or_hall_measurements_struct* currx,uint32_t N);
void detect_N_hall_zerocrossings	(uint32_t ticks,current_or_hall_measurements_struct* hallx,uint32_t N);
void calculateElectricPeriod_inTicks(hall_detection_general_struct *gen, uint32_t samples);
detection_YES_NO is_deviation_from_period_acceptable(hall_detection_general_struct *gen, float tolerance_factor,uint32_t samples);
detection_YES_NO are_all_periods_stable(hall_detection_general_struct *gen, uint32_t samples);
int32_t absolute(int32_t x);
void assign_closest_phase_to_hall(hall_detection_general_struct *gen);
void assign_polarity(hall_detection_general_struct *gen);










/**
* \brief public function , this should me called by anyone otside this .c .h to start the hall detection show
*/
void Hall_start_detection(){
	detection_state=detection_ENABLED;
}

/**
* \brief public function , this should me called by anyone otside this .c .h to figure out if the detection finished
*/
uint32_t Hall_is_detection_finished(){
	if(detection_state==detection_DISABLED){
		return YES;
	}else{
		return NO;
	}
}

//https://dot-to-ascii.ggerganov.com/
/*
 *
+-------------------------+
|          start          |
+-------------------------+
  |
  |
  v
+-------------------------+
|                         | ---+
| WAIT_CURRENT_STATIONARY |    | still not stationary
|                         | <--+
+-------------------------+
  |
  |	the openloop stabilished
  v
+-------------------------+
|       ADQUISITION       | <+
+-------------------------+  |
  |                          |
  | zerocrossings detected   |
  v                          |
+-------------------------+  | still not 3 matching X!X!X! type results
|     INTERPRETATION      |  |
+-------------------------+  |
  |                          |
  | X!X!X! type result       |
  v                          |
+-------------------------+  |
|       VALIDATION        | -+
+-------------------------+
  |
  |	3 matching results, we are confident now
  v
+-------------------------+
|   PRESENTATION_FINISH   |
+-------------------------+
  |
  |
  v
+-------------------------+
|           end           |
+-------------------------+

*/
/**
* \brief public function, to be included inside a 20Khz interruption routine, it manages the state machine used by the detection
* \param hall_pin_info * H1_gpio, pointer to gpio info about the hall A
* \param hall_pin_info * H2_gpio, pointer to gpio info about the hall B
* \param hall_pin_info * H3_gpio, pointer to gpio info about the hall C
* \param volatile float* ADCcurr1, pointer to current value in amps for phase A
* \param volatile float* ADCcurr2, pointer to current value in amps for phase C? or B?
*/
void Hall_Identification_Test_measurement(
		hall_pin_info* H1_gpio,
		hall_pin_info* H2_gpio,
		hall_pin_info* H3_gpio,
		volatile float* ADCcurr1,
		volatile float* ADCcurr2
		){
	switch (detection_state) {
		case detection_ENABLED:
			ticks=0;
			resetVariables(&general);
			detection_state=detection_WAIT_CURRENT_STATIONARY;
			break;
		case detection_WAIT_CURRENT_STATIONARY:
			wait_for_the_current_stationary(&detection_state,&general,H1_gpio,H2_gpio,H3_gpio,ADCcurr1,ADCcurr2);
			ticks++;
			break;
		case detection_ADQUISITION:
			adquisition(&detection_state,&general,H1_gpio,H2_gpio,H3_gpio,ADCcurr1,ADCcurr2);
			ticks++;
			break;
		case detection_INTERPRETATION:
			interpretation(&detection_state, &general);
			ticks++;
			break;
		case detection_VALIDATION:
			ticks++;
			validation(&detection_state,&general);
			ticks++;
			break;
		case detection_PRESENTATION_FINISH:
			present_and_finish(&detection_state,&general);
			ticks++;
			break;
		case detection_ERROR_OR_TIMEOUT:
			detection_state=detection_DISABLED;
		case detection_DISABLED://do nothing
		default:
			break;
	}
}

/**
* \brief just resets to 0 the huge structure used by the detection
* \param hall_detection_general_struct *gen, pointer to the huge structure.
*/
void resetVariables(hall_detection_general_struct *gen){
	*gen=empty_general;		//i dont want to use memset, so we sacrifice flash memory space filled with 0's for this.
}


/**
* \brief the motor running in open loop control needs a bit of time to lock in place so current signals stop being so ugly.
* \param detection_state_enum* state,			pointer to the variable controling the state machine
* \param hall_detection_general_struct *gen, 	pointer to the huge structure containing everything the detection needs.
* \param hall_pin_info * H1_gpio, pointer to gpio info about the hall A
* \param hall_pin_info * H2_gpio, pointer to gpio info about the hall B
* \param hall_pin_info * H3_gpio, pointer to gpio info about the hall C
* \param volatile float* ADCcurr1, pointer to current value in amps for phase A
* \param volatile float* ADCcurr2, pointer to current value in amps for phase C? or B?
*/
void wait_for_the_current_stationary(detection_state_enum* state,hall_detection_general_struct *gen,
		hall_pin_info* H1_gpio,
		hall_pin_info* H2_gpio,
		hall_pin_info* H3_gpio,
		volatile float* ADCcurr1,
		volatile float* ADCcurr2
		){
	//timeout or currentisstationary
	fill_buffers(gen, H1_gpio, H2_gpio, H3_gpio, ADCcurr1, ADCcurr2);
	//timeout
	if( ticks>MAXTICKs){
		*state=detection_ERROR_OR_TIMEOUT;
		return;
	}

    //endofadquisition
	if(ticks>general.start_adquisition_ticks+2){//run alone signals_adquisition() to pre-fill the buffers
		if(detect_N_zerocrossings(gen,WAITING_STATIONARY_MAXZEROCROSSINGS)==YES){
			if(are_all_periods_stable(gen,WAITING_STATIONARY_MAXZEROCROSSINGS)==YES){
				resetVariables(gen);
				general.start_adquisition_ticks=ticks;
				*state=detection_ADQUISITION;
				return;
			}else{
				resetVariables(gen);
			}
		}
	}
}

/**
* \brief similar to wait_for_the_current_stationary(), but this time we mean it, we are adquiring the zerocrossings that will be used to detect everything. not just checking stable signal periods.
* \param detection_state_enum* state,			pointer to the variable controling the state machine
* \param hall_detection_general_struct *gen, 	pointer to the huge structure containing everything the detection needs.
* \param hall_pin_info * H1_gpio, pointer to gpio info about the hall A
* \param hall_pin_info * H2_gpio, pointer to gpio info about the hall B
* \param hall_pin_info * H3_gpio, pointer to gpio info about the hall C
* \param volatile float* ADCcurr1, pointer to current value in amps for phase A
* \param volatile float* ADCcurr2, pointer to current value in amps for phase C? or B?
*/
void adquisition(
		detection_state_enum* state,
		hall_detection_general_struct *gen,
		hall_pin_info* H1_gpio,
		hall_pin_info* H2_gpio,
		hall_pin_info* H3_gpio,
		volatile float* ADCcurr1,
		volatile float* ADCcurr2
		){

	fill_buffers(gen, H1_gpio, H2_gpio, H3_gpio, ADCcurr1, ADCcurr2);

	//timeout
	if( ticks>MAXTICKs){
		*state=detection_ERROR_OR_TIMEOUT;
		return;
	}

	if(ticks>general.start_adquisition_ticks+2){//run alone signals_adquisition() to pre-fill the buffers
		if(detect_N_zerocrossings(gen,MAXZEROCROSSINGS)==YES){
			calculateElectricPeriod_inTicks(gen,MAXZEROCROSSINGS);
			*state=detection_INTERPRETATION;
		}
	}
}

/**
* \brief once we adquired the data we are trying to make sense out of it.
* \param detection_state_enum* state,			pointer to the variable controling the state machine
* \param hall_detection_general_struct *gen, 	pointer to the huge structure containing everything the detection needs.
*/
void interpretation(detection_state_enum* state,hall_detection_general_struct *gen){
	//timeout
	if( ticks>MAXTICKs){
		*state=detection_ERROR_OR_TIMEOUT;
		return;
	}
	assign_closest_phase_to_hall(gen);
	assign_polarity(gen);
	gen->numberOfresults++;
	*state=detection_VALIDATION;
}

/**
* \brief once we interpreted enough data, we validate results
* this function has a cyclomatic complexity of 19, that should be improved
* \param detection_state_enum* state,			pointer to the variable controling the state machine
* \param hall_detection_general_struct *gen, 	pointer to the huge structure containing everything the detection needs.
*/
void validation(detection_state_enum* state,hall_detection_general_struct *gen){
	//timeout or 	//reached results buffer full capacity
	if( ticks>MAXTICKs || gen->numberOfresults>TOTAL_NUMBEROFRESULTS){
		*state=detection_ERROR_OR_TIMEOUT;
		return;
	}

	uint32_t all_phases_not_repeated=0;
	for (uint32_t i = 0;  i < NUMBEROFPHASES; ++ i) {
		if(gen->results[gen->numberOfresults-1].hall_order[i]==hall_A){
			all_phases_not_repeated|=(1<<hall_A);
		}else if(gen->results[gen->numberOfresults-1].hall_order[i]==hall_B){
			all_phases_not_repeated|=(1<<hall_B);
		}else if(gen->results[gen->numberOfresults-1].hall_order[i]==hall_C){
			all_phases_not_repeated|=(1<<hall_C);
		}
	}

	if(all_phases_not_repeated==((1<<hall_A)+(1<<hall_B)+(1<<hall_C))){
		gen->results[gen->numberOfresults-1].is_valid=YES;
	}else{
		gen->results[gen->numberOfresults-1].is_valid=NO;
	}

	if(gen->numberOfresults>=NUMBER_OF_VALID_MATCHING_RESULTS){			//only if enough adquisitions were made
		for (uint32_t i = 0; i < gen->numberOfresults; ++i) {			//loop trough results
			if(gen->results[i].is_valid==YES){	//skip the not valid results.
				uint32_t number_of_results_matching=0;
				for (uint32_t j = i+1; j < gen->numberOfresults; ++j) {	//compare that one result with the rest
					if(
							gen->results[i].hall_order[0]==gen->results[j].hall_order[0] &&
							gen->results[i].hall_order[1]==gen->results[j].hall_order[1] &&
							gen->results[i].hall_order[2]==gen->results[j].hall_order[2] &&
							gen->results[i].hall_polarity[0]==gen->results[j].hall_polarity[0] &&
							gen->results[i].hall_polarity[1]==gen->results[j].hall_polarity[1] &&
							gen->results[i].hall_polarity[2]==gen->results[j].hall_polarity[2]
							){//do they match?
							number_of_results_matching++;
						if(number_of_results_matching>=NUMBER_OF_VALID_MATCHING_RESULTS-1){
							gen->indexOfcorrectResult=i;
							*state=detection_PRESENTATION_FINISH;
							return;
						}
					}
				}
			}

		}
	}else{
		*state=detection_ADQUISITION;
		return;
	}
}

/**
* \brief if the resutls are validated, lets present the data, load the ASCII message for uart and/or manage the logic swap for hall gpios.
* \param detection_state_enum* state,			pointer to the variable controling the state machine
* \param hall_detection_general_struct *gen, 	pointer to the huge structure containing everything the detection needs.
*/
void present_and_finish(detection_state_enum* state,hall_detection_general_struct *gen){
	for (uint32_t i = 0; i < messageLength; ++i) {
		gen->message[i]=' ';
	}

	for (uint32_t i = 0; i < NUMBEROFPHASES; ++i) {
		if(gen->results[gen->indexOfcorrectResult].hall_order[i]==hall_A){
			gen->message[(i*2)+1]='A';
		}else if(gen->results[gen->indexOfcorrectResult].hall_order[i]==hall_B){
			gen->message[(i*2)+1]='B';
		}else if(gen->results[gen->indexOfcorrectResult].hall_order[i]==hall_C){
			gen->message[(i*2)+1]='C';
		}

		if(gen->results[gen->indexOfcorrectResult].hall_polarity[i]==hall_direct){
			gen->message[i*2]=' ';
		}else if(gen->results[gen->indexOfcorrectResult].hall_polarity[i]==hall_inverse){
			gen->message[i*2]='!';
		}
	}

	gen->message[messageLength-2]='\n';//two last characters
	gen->message[messageLength-1]='\r';

#ifdef TESTuart
	HAL_UART_Transmit(&huart2, (const uint8_t *)&general.message, messageLength,100);
#endif

	*state=detection_DISABLED;
}

/**
* \brief these buffers need to be filled, not much to this function, manages the "circularity" of the buffers and calculates the missing current signal.
* \param hall_detection_general_struct *gen, 	pointer to the huge structure containing everything the detection needs.
* \param hall_pin_info * H1_gpio, pointer to gpio info about the hall A
* \param hall_pin_info * H2_gpio, pointer to gpio info about the hall B
* \param hall_pin_info * H3_gpio, pointer to gpio info about the hall C
* \param volatile float* ADCcurr1, pointer to current value in amps for phase A
* \param volatile float* ADCcurr2, pointer to current value in amps for phase C? or B?
*/
void fill_buffers(
		hall_detection_general_struct *gen,
		hall_pin_info* H1_gpio,
		hall_pin_info* H2_gpio,
		hall_pin_info* H3_gpio,
		volatile float* ADCcurr1,
		volatile float* ADCcurr2
		){

#ifdef REAL_PHASES_A_B_calculated_C
	gen->currA.two_samples_buffer[1]=gen->currA.two_samples_buffer[0];
	gen->currA.two_samples_buffer[0]= *ADCcurr1; //i suspect ADC measurements are one sample late, because of the ADC being triggered at the end of the TIM interruption

	gen->currB.two_samples_buffer[1]=gen->currB.two_samples_buffer[0];
	gen->currB.two_samples_buffer[0]= *ADCcurr2;
	//c=-a-b, NO REAL MEASUREMENT OF CURRENT C, assuming ABC currents ortogonality:
	gen->currC.two_samples_buffer[1]=gen->currC.two_samples_buffer[0];
	gen->currC.two_samples_buffer[0]= currentAplusBplusC-*ADCcurr1-*ADCcurr2;
#endif

#ifdef REAL_PHASES_A_C_calculated_B
	gen->currA.two_samples_buffer[1]=gen->currA.two_samples_buffer[0];
	gen->currA.two_samples_buffer[0]= *ADCcurr1;
	//b=-a-c, NO REAL MEASUREMENT OF CURRENT B, assuming ABC currents ortogonality:
	gen->currB.two_samples_buffer[1]=gen->currB.two_samples_buffer[0];
	gen->currB.two_samples_buffer[0]= -*ADCcurr1-*ADCcurr2;

	gen->currC.two_samples_buffer[1]=gen->currC.two_samples_buffer[0];
	gen->currC.two_samples_buffer[0]= *ADCcurr2;
#endif

#ifdef REAL_PHASES_B_C_calculated_A
	//a=-b-bc, NO REAL MEASUREMENT OF CURRENT A, assuming ABC currents ortogonality:
	gen->currA.two_samples_buffer[1]=gen->currA.two_samples_buffer[0];
	gen->currA.two_samples_buffer[0]= currentAplusBplusC-*ADCcurr1-*ADCcurr2;

	gen->currB.two_samples_buffer[1]=gen->currB.two_samples_buffer[0];
	gen->currB.two_samples_buffer[0]= *ADCcurr1;

	gen->currC.two_samples_buffer[1]=gen->currC.two_samples_buffer[0];
	gen->currC.two_samples_buffer[0]= *ADCcurr2;
#endif

	gen->hallA.two_samples_buffer[1]=gen->hallA.two_samples_buffer[0];
	gen->hallA.two_samples_buffer[0]=(float) H1_gpio->state;

	gen->hallB.two_samples_buffer[1]=gen->hallB.two_samples_buffer[0];
	gen->hallB.two_samples_buffer[0]=(float) H2_gpio->state;

	gen->hallC.two_samples_buffer[1]=gen->hallC.two_samples_buffer[0];
	gen->hallC.two_samples_buffer[0]=(float) H3_gpio->state;
}

/**
* \brief reading the buffers we just filled detects if a zero crossing happened in between them, it has a low pass filter just in case spurious stuff happens, once all zerocrossings are filled it signals done detecting
* \param hall_detection_general_struct *gen, 	pointer to the huge structure containing everything the detection needs.
* \return YES//done detecting or NO//still ongoing
*/
detection_YES_NO detect_N_zerocrossings(hall_detection_general_struct *gen,uint32_t N){
	if((ticks-general.start_adquisition_ticks)>2){ //skip the first two samples to fill the buffers
		detect_N_current_zerocrossings((ticks-general.start_adquisition_ticks),&gen->currA,MAXZEROCROSSINGS);
		detect_N_current_zerocrossings((ticks-general.start_adquisition_ticks),&gen->currB,MAXZEROCROSSINGS);
		detect_N_current_zerocrossings((ticks-general.start_adquisition_ticks),&gen->currC,MAXZEROCROSSINGS);
		detect_N_hall_zerocrossings	((ticks-general.start_adquisition_ticks),&gen->hallA,MAXZEROCROSSINGS);
		detect_N_hall_zerocrossings	((ticks-general.start_adquisition_ticks),&gen->hallB,MAXZEROCROSSINGS);
		detect_N_hall_zerocrossings	((ticks-general.start_adquisition_ticks),&gen->hallC,MAXZEROCROSSINGS);
	}
	if(//if all buffers are full
			(gen->currA.numberof_zerocrossings>=N) &&
			(gen->currB.numberof_zerocrossings>=N) &&
			(gen->currC.numberof_zerocrossings>=N) &&
			(gen->hallA.numberof_zerocrossings>=N) &&
			(gen->hallB.numberof_zerocrossings>=N) &&
			(gen->hallC.numberof_zerocrossings>=N)
			){
		return YES;//done detecting
	}else{
		return NO;//still ongoing
	}
}

/**
* \brief reading the current buffers detects 0 crossings and takes notes of the tick number.
* \param hall_detection_general_struct *gen, 	pointer to the huge structure containing everything the detection needs.
*/
void detect_N_current_zerocrossings(uint32_t ticks,current_or_hall_measurements_struct* currx,uint32_t N){
	if(	   (((currx->two_samples_buffer[0]-currentADCoffset)*
			 (currx->two_samples_buffer[1]-currentADCoffset))<=0)
			 && currx->numberof_zerocrossings<N
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
* \brief similar to detect_N_current_zerocrossings() reading the hall buffers detects logic changes and notes down the tick number
* \param hall_detection_general_struct *gen, 	pointer to the huge structure containing everything the detection needs.
*/
void detect_N_hall_zerocrossings(uint32_t ticks,current_or_hall_measurements_struct* hallx,uint32_t N){
	if(
		hallx->two_samples_buffer[0]!=hallx->two_samples_buffer[1]
		&& hallx->numberof_zerocrossings<N)
	{
			hallx->zerocrossings_tick[hallx->numberof_zerocrossings]=ticks;
			if(hallx->two_samples_buffer[0]!=0){
				hallx->zerocrossings_polarity[hallx->numberof_zerocrossings]=rising_polarity;
			}else{
				hallx->zerocrossings_polarity[hallx->numberof_zerocrossings]=falling_polarity;
			}
			hallx->numberof_zerocrossings++;
	}
}

/**
* \brief reading differences between noted ticks zerocrossigns averages our motor electric period
* \param hall_detection_general_struct *gen, 	pointer to the huge structure containing everything the detection needs.
* \param samples number of zerocrossings to be used in the diff calculation
*/
void calculateElectricPeriod_inTicks(hall_detection_general_struct *gen, uint32_t samples){
	uint32_t averagedsemiPeriod=0;
	for (uint32_t i = 0; i < samples-1; ++i) {
		averagedsemiPeriod+=(gen->currA.zerocrossings_tick[i+1]-gen->currA.zerocrossings_tick[i]);
		averagedsemiPeriod+=(gen->currB.zerocrossings_tick[i+1]-gen->currB.zerocrossings_tick[i]);
		averagedsemiPeriod+=(gen->hallA.zerocrossings_tick[i+1]-gen->hallA.zerocrossings_tick[i]);
		averagedsemiPeriod+=(gen->hallB.zerocrossings_tick[i+1]-gen->hallB.zerocrossings_tick[i]);
		averagedsemiPeriod+=(gen->hallC.zerocrossings_tick[i+1]-gen->hallC.zerocrossings_tick[i]);
	}

	averagedsemiPeriod/=(samples-1);
	averagedsemiPeriod/=5;
	gen->results[gen->numberOfresults].electricPeriod_ticks=averagedsemiPeriod*2; //FOR torrot emulated this should be 66*2
}

/**
* \brief just checks if all zerocrossings fall between acceptable deviation from average period.
* \param hall_detection_general_struct *gen, 	pointer to the huge structure containing everything the detection needs.
* \param float tolerance_factor, 0.1 would mean +-10%tolerance of deviation from average
* \param samples number of zerocrossings to be used in the diff calculation
* \return YES if all deviations are accceptable, or NO if they are not
*/
detection_YES_NO is_deviation_from_period_acceptable(hall_detection_general_struct *gen, float tolerance_factor,uint32_t samples){
	float _semiperiod=gen->results[gen->numberOfresults].electricPeriod_ticks/2.0;
	uint32_t _deviation_top=	_semiperiod +_semiperiod*tolerance_factor;
	uint32_t _deviation_bottom=	_semiperiod	-_semiperiod*tolerance_factor;

	for (uint32_t i = 0; i < samples-1; ++i) {
		uint32_t _sampled_period=(gen->currA.zerocrossings_tick[i+1]-gen->currA.zerocrossings_tick[i]);
		if((_sampled_period<(_deviation_bottom))||(_sampled_period>(_deviation_top))){
			return NO;//early return, bad news
		}

		_sampled_period=(gen->currC.zerocrossings_tick[i+1]-gen->currC.zerocrossings_tick[i]);
		if((_sampled_period<(_deviation_bottom))||(_sampled_period>(_deviation_top))){
			return NO;//early return, bad news
		}
	}
	return YES;//acceptable
}


/**
* \brief a wrapper to tidy up and make sure we calculate the electric period before calculating deviations.
* \param hall_detection_general_struct *gen, 	pointer to the huge structure containing everything the detection needs.
* \param samples number of zerocrossings to be used in the diff calculation
* \return YES if we are in the stable zone of steady periods. NO otherwise
*/
detection_YES_NO are_all_periods_stable(hall_detection_general_struct *gen, uint32_t samples){
			calculateElectricPeriod_inTicks(gen,samples);
	return 	is_deviation_from_period_acceptable(gen,TOLERANCE_FACTOR_FOR_STATIONARY_CURRENTS,samples);
}


///**
//* \brief
//* \param
//*/
//void swap_hall_gpios_with_detected_results(hall_pin_info* pin_infoH1,hall_pin_info* pin_infoH2,hall_pin_info* pin_infoH3){
//	hall_pin_info old_pin_infoH1=*pin_infoH1;
//	hall_pin_info old_pin_infoH2=*pin_infoH2;
//	hall_pin_info old_pin_infoH3=*pin_infoH3;
//
//	switch (results.hall_order[phase_A]) {
//		case hall_A:
//			*pin_infoH1=old_pin_infoH1;
//			break;
//		case hall_B:
//			*pin_infoH2=old_pin_infoH1;
//			break;
//		case hall_C:
//			*pin_infoH3=old_pin_infoH1;
//			break;
//		default:
//			break;
//	}
//
//	switch (results.hall_order[phase_B]) {
//		case hall_A:
//			*pin_infoH1=old_pin_infoH2;
//			break;
//		case hall_B:
//			*pin_infoH2=old_pin_infoH2;
//			break;
//		case hall_C:
//			*pin_infoH3=old_pin_infoH2;
//			break;
//		default:
//			break;
//	}
//
//	switch (results.hall_order[phase_C]) {
//		case hall_A:
//			*pin_infoH1=old_pin_infoH3;
//			break;
//		case hall_B:
//			*pin_infoH2=old_pin_infoH3;
//			break;
//		case hall_C:
//			*pin_infoH3=old_pin_infoH3;
//			break;
//		default:
//			break;
//	}
//
//}

/**
* \brief small and util to calculate signed absolute values
* \param the number to be absoluted
* \return |x|
*/
int32_t absolute(int32_t x){
    return (int32_t)x < (int32_t)0 ? -x : x;
}

/**
* \brief magic function, from zerocrossings decides actuall order of hall/current signals, this function is super critical and should be optimiced
* right now has a cyclomatic complexity of 21, that should be reduced.
* \param hall_detection_general_struct *gen, 	pointer to the huge structure containing everything the detection needs.
*/
void assign_closest_phase_to_hall(hall_detection_general_struct *gen){

	for (uint32_t i = 0; i < MAXZEROCROSSINGS; ++i) {// all zerocrossings loop
		gen->differences_phaseA[phase_A]+=absolute((int32_t)gen->currA.zerocrossings_tick[i]-(int32_t)gen->hallA.zerocrossings_tick[i]);
		gen->differences_phaseA[phase_B]+=absolute((int32_t)gen->currA.zerocrossings_tick[i]-(int32_t)gen->hallB.zerocrossings_tick[i]);
		gen->differences_phaseA[phase_C]+=absolute((int32_t)gen->currA.zerocrossings_tick[i]-(int32_t)gen->hallC.zerocrossings_tick[i]);

		gen->differences_phaseB[phase_A]+=absolute((int32_t)gen->currB.zerocrossings_tick[i]-(int32_t)gen->hallA.zerocrossings_tick[i]);
		gen->differences_phaseB[phase_B]+=absolute((int32_t)gen->currB.zerocrossings_tick[i]-(int32_t)gen->hallB.zerocrossings_tick[i]);
		gen->differences_phaseB[phase_C]+=absolute((int32_t)gen->currB.zerocrossings_tick[i]-(int32_t)gen->hallC.zerocrossings_tick[i]);

		gen->differences_phaseC[phase_A]+=absolute((int32_t)gen->currC.zerocrossings_tick[i]-(int32_t)gen->hallA.zerocrossings_tick[i]);
		gen->differences_phaseC[phase_B]+=absolute((int32_t)gen->currC.zerocrossings_tick[i]-(int32_t)gen->hallB.zerocrossings_tick[i]);
		gen->differences_phaseC[phase_C]+=absolute((int32_t)gen->currC.zerocrossings_tick[i]-(int32_t)gen->hallC.zerocrossings_tick[i]);
	}

	int32_t _bottom_limit	=(int32_t)((gen->results[gen->numberOfresults].electricPeriod_ticks/2)*0.95);
	int32_t _top_limit		=(int32_t)(gen->results[gen->numberOfresults].electricPeriod_ticks);
	int32_t minimum[NUMBEROFPHASES]={0xFF,0xFF,0xFF};

	for (uint32_t i = 0; i < NUMBEROFPHASES; ++i) {
		gen->differences_phaseA[i]/=MAXZEROCROSSINGS;
		gen->differences_phaseB[i]/=MAXZEROCROSSINGS;
		gen->differences_phaseC[i]/=MAXZEROCROSSINGS;

		//sometimes the toggled polarity is detected from a non relevant phase, in this case we are wrongly applying te toggle, TODO lets increase the toogle array, and make it case sensitive, if the togled signals is actually the one we use do something if not just do nothing
		if(gen->differences_phaseA[i]>_bottom_limit && gen->differences_phaseA[i]<_top_limit){
			gen->differences_phaseA[i]%=_bottom_limit;
			gen->shifted_polarity[hall_A][i]=1;
		}
		if(gen->differences_phaseB[i]>_bottom_limit && gen->differences_phaseB[i]<_top_limit){
			gen->differences_phaseB[i]%=_bottom_limit;
			gen->shifted_polarity[hall_B][i]=1;
		}
		if(gen->differences_phaseC[i]>_bottom_limit && gen->differences_phaseC[i]<_top_limit){
			gen->differences_phaseC[i]%=_bottom_limit;
			gen->shifted_polarity[hall_C][i]=1;
		}
		//takes notes of the minimum difference
		if(gen->differences_phaseA[i]<minimum[phase_A]){
			minimum[phase_A]=gen->differences_phaseA[i];
		}

		if(gen->differences_phaseB[i]<minimum[phase_B]){
			minimum[phase_B]=gen->differences_phaseB[i];
		}

		if(gen->differences_phaseC[i]<minimum[phase_C]){
			minimum[phase_C]=gen->differences_phaseC[i];
		}
	}
	//finds out whicone is the minimum diff
	for (uint32_t i = 0; i < NUMBEROFPHASES; ++i) {
		if(gen->differences_phaseA[i]==minimum[phase_A]){
			gen->results[gen->numberOfresults].hall_order[i]=phase_A;
		}

		if(gen->differences_phaseB[i]==minimum[phase_B]){
			gen->results[gen->numberOfresults].hall_order[i]=phase_B;
		}

		if(gen->differences_phaseC[i]==minimum[phase_C]){
			gen->results[gen->numberOfresults].hall_order[i]=phase_C;
		}
	}
}



/**
* \brief the other magic function, asigns polarity from risign or falling edges, it also takes into account tricky shifted signals
* right now has a cyclomatic complexity of 21, that should be reduced.
* \param hall_detection_general_struct *gen, 	pointer to the huge structure containing everything the detection needs.
*/
void assign_polarity(hall_detection_general_struct *gen){
	current_or_hall_measurements_struct*	currents[NUMBEROFPHASES]={&gen->currA,&gen->currB,&gen->currC};
	current_or_hall_measurements_struct*	halls[NUMBEROFPHASES]	={&gen->hallA,&gen->hallB,&gen->hallC};
	detection_results_struct *pointerOfcurrentResult=&gen->results[gen->numberOfresults];

	for (uint32_t i = 0; i < NUMBEROFPHASES; ++i) {
		if(currents[pointerOfcurrentResult->hall_order[i]]->zerocrossings_polarity[1]==halls[i]->zerocrossings_polarity[1]){
			if(gen->shifted_polarity[pointerOfcurrentResult->hall_order[i]][i]==0){
				pointerOfcurrentResult->hall_polarity[pointerOfcurrentResult->hall_order[i]]=hall_direct;
			}else{
				pointerOfcurrentResult->hall_polarity[pointerOfcurrentResult->hall_order[i]]=hall_inverse;
			}
		}else{
			if(gen->shifted_polarity[pointerOfcurrentResult->hall_order[i]][i]==0){
				pointerOfcurrentResult->hall_polarity[pointerOfcurrentResult->hall_order[i]]=hall_inverse;
			}else{
				pointerOfcurrentResult->hall_polarity[pointerOfcurrentResult->hall_order[i]]=hall_direct;
			}
		}
	}

}




