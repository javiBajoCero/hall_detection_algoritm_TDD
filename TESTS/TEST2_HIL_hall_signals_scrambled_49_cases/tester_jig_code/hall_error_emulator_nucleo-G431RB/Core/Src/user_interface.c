/*
 * user_interface.c
 *
 *  Created on: Nov 17, 2023
 *      Author: javi
 */


#include "usart.h"

#include <string.h> //only for strstr()

#include "user_interface.h"
#include "hall_swapper.h"   //to access the variables signal_order and  signal_polarity
#include "wave_emulator.h"
#include "main.h" //for the reset pin

const uint8_t initialmessage[]=
		"\n\r%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%HOLA%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%\n\r"
		"This code \"hall_error_emulator\" was made by javier.munoz.saez@upc.edu\n\r"
		"compiled on: 22-nov-2023\n\r"
		"stored in the internal CITCEA's gitlab repo: \n\r "
		"http://serv-citcea-100.upc.edu:30000/javi/algoritmo_para_determinar_errores_en_sensores_hall\n\r"
		"\n\r"
		"This code is meant to be run in a NUCLEO-FG431RB board and has two main functions:\n\r"
		"1-swap real incoming hall signals changing its order and/or polarity"
		"2-emulate a constant speed motor with all three halls and two real current signals"
		"-INPUTS: uart-USB(stlink) bridge from the user with text commands\n\r"
		"-INPUTS: three (alegedly healthy) square signals from three real hall sensors\n\r"
		"-OUTPUTS: same three square signals with the designated order,polarity, and phase errors\n\r"
		"\n\r%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%\n\r"
		;
const uint8_t helpcommand[]="help";
const uint8_t helpmessage[]=
		"\n\r%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%HELP%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%\n\r"
		"How to use this hall_error_emulator:\n\r"
		"type \"ABC\" + enter for the emulator to foward the hall signals as they come, no errors\n\r"
		"-order error: \"BAC\" + enter for the emulator to swap A and B inputs\n\r"
		"-polarity error: \"!ABC\" + enter for the emulator to logically negate input A and leave inputs B and C as they come\n\r"
		"example: typing \"!AC!B\"+ enter will swap the order of signals B and C and negate signals A and B \n\r"
		"\n\r%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%\n\r"
		;
const uint8_t emulationcommand[]="emulation";
const uint8_t on_emulationmessage[]=">>    emulation on\n\r";
const uint8_t off_emulationmessage[]=">>    emulation off\n\r";

const uint8_t resetcommand[]="reset target";
const uint8_t resetmessage[]=">>	target just reseted";

const uint8_t okmessage[]=		">>    ok\n\r";
const uint8_t notokmessage[]=	">>    what was that?, try again\n\r";

#define RX_BUFFER_SIZE 0x100 //0x100 is a quarter kilobyte or 256 uint8_t
uint8_t rx_buffer[RX_BUFFER_SIZE]={0};
uint8_t rx_flag=0;	//1= we received some data
uint8_t rx_size=0;	//size
uint8_t echoed_size=0;

char hall_status_string[]={" A B C"};



//functions within this .c file
void ui_uart_init();
void ui_uart_transmit_initial_message();
void ui_uart_listen();
uint32_t ui_uart_echo_and_breaklinedetect_rx();

uint32_t decode_ABCcommands(uint8_t *pData, uint16_t Size);
uint32_t decode_othercommands(uint8_t *pData, uint16_t Size);
uint16_t apply_delete_character_to_buffer(uint8_t *pData, uint16_t Size);
hall_signals_order decode_char_to_hall_signals_order(uint8_t *pData);
hall_signals_polarity decode_char_to_hall_signals_polarity(uint8_t *pData);
void reset_or_start_uart_DMA_communications();


void ui_uart_init(){
	__HAL_DMA_DISABLE_IT(&hdma_lpuart1_tx,DMA_IT_HT);						//disable DMA half transfer interruption
	__HAL_DMA_DISABLE_IT(&hdma_lpuart1_rx,DMA_IT_HT);						//disable DMA half transfer interruption
	reset_or_start_uart_DMA_communications();
	ui_uart_transmit_initial_message();
}

void ui_uart_transmit_initial_message(){
	HAL_UART_Transmit_DMA(&hlpuart1, initialmessage, sizeof(initialmessage));	//just order the DMA to spit out initialmessage trough uart
}

void ui_uart_listen(){
	if(ui_uart_echo_and_breaklinedetect_rx()==1){

		if(decode_othercommands(rx_buffer, echoed_size)!=1){
			decode_ABCcommands(rx_buffer, echoed_size);
		}
		rx_size=0;
		echoed_size=0;
		reset_or_start_uart_DMA_communications();
	}
}

uint32_t ui_uart_echo_and_breaklinedetect_rx(){
	uint32_t linejumpdetected_flag=0;//1= we detected a line jump

	if(rx_flag==1){											//uart received something
		if((rx_size-echoed_size)>0){						//check only that new something not the whole RX buffer each time
			for(uint32_t i=0;i<rx_size-echoed_size;i++){
				if(rx_buffer[echoed_size+i]=='\r'){			//detect a breakline from the user, that means start decoding
					linejumpdetected_flag=1;
					break;
				}
			}

			if(linejumpdetected_flag==1){
				rx_buffer[rx_size]='\n';
				HAL_UART_Transmit_DMA(&hlpuart1, &rx_buffer[echoed_size], (rx_size-echoed_size)+1);	//the line jump from putty is actually only '/r', for readability we will add also a '/n'
				HAL_Delay(25);
			}else{
				HAL_UART_Transmit_DMA(&hlpuart1, &rx_buffer[echoed_size], (rx_size-echoed_size));
				HAL_Delay(25);
			}
		}
		echoed_size+=rx_size-echoed_size;				//actualise already echoed and checked for breakline buffer.
		rx_flag=0;										//reset the uart received flag
	}

	return linejumpdetected_flag;

}

uint32_t decode_ABCcommands(uint8_t *pData, uint16_t Size){

	hall_signals_order aux_signal_order[3]={hall_order_unknown, hall_order_unknown, hall_order_unknown};
	hall_signals_polarity aux_signal_polarity[3]={hall_polarity_unknown, hall_polarity_unknown, hall_polarity_unknown};
	uint32_t number_of_identified_phases=0;

	uint32_t hall_order_unknown_flag=0;
	uint32_t hall_polarity_unknown_flag=0;

	uint16_t newSize_after_all_deletions=apply_delete_character_to_buffer(pData,Size);


	for (uint32_t i = 0; i < newSize_after_all_deletions; ++i) {
		hall_signals_order just_identified_hall_order=decode_char_to_hall_signals_order((uint8_t *)&pData[i]);
		if(just_identified_hall_order!=hall_order_unknown){//we got a phase character match!
			aux_signal_order[number_of_identified_phases]=just_identified_hall_order;
			if(i==0){//take care of the 0 index, we dont want to access the array out of bounds
				aux_signal_polarity[number_of_identified_phases]=hall_direct;
			}else{
				aux_signal_polarity[number_of_identified_phases]=decode_char_to_hall_signals_polarity((uint8_t *)&pData[i-1]);
			}

			number_of_identified_phases++;
		}
	}

	for (uint32_t i = 0; i < 3; ++i) {//check if all values are acceptable
		if(aux_signal_order[i]==hall_order_unknown){
			hall_order_unknown_flag=1;
			break;			//we found an unacceptable value, early break
		}

		if(aux_signal_polarity[i]==hall_polarity_unknown){
			hall_polarity_unknown_flag=1;
			break;			//we found an unacceptable value, early break
		}
	}

	if(number_of_identified_phases==3 && hall_order_unknown_flag==0 && hall_polarity_unknown_flag==0){
		//everything went well, unload out new values into the real order and polarity arrays
		for(uint32_t i = 0; i < sizeof(aux_signal_order); ++i){
			signal_order[i]=aux_signal_order[i];
			signal_polarity[i]=aux_signal_polarity[i];
		}
		HAL_Delay(50);
		HAL_UART_Transmit_DMA(&hlpuart1, okmessage, sizeof(okmessage));
		return 1;
	}else{
		HAL_Delay(50);
		HAL_UART_Transmit_DMA(&hlpuart1, notokmessage, sizeof(notokmessage));
		return 0;
	}

}

uint32_t decode_othercommands(uint8_t *pData, uint16_t Size){
	char* aux_variable_searching_strings=NULL;
	//check for the help command
	aux_variable_searching_strings=strstr((char *)pData, (char *)helpcommand);
	if(aux_variable_searching_strings!=NULL){
		HAL_Delay(50);
		HAL_UART_Transmit_DMA(&hlpuart1, helpmessage, sizeof(helpmessage));
		return 1;
	}

	aux_variable_searching_strings=strstr((char *)pData, (char *)resetcommand);
	if(aux_variable_searching_strings!=NULL){
		HAL_Delay(50);
		HAL_UART_Transmit_DMA(&hlpuart1, resetmessage, sizeof(resetmessage));
		HAL_GPIO_Write(RESET_TARGET_BOARD_GPIO_Port, RESET_TARGET_BOARD_Pin,GPIO_PIN_RESET);
		HAL_Delay(100);
		HAL_GPIO_Write(RESET_TARGET_BOARD_GPIO_Port, RESET_TARGET_BOARD_Pin,GPIO_PIN_SET);
		return 1;
	}

	aux_variable_searching_strings=strstr((char *)pData, (char *)emulationcommand);
	if(aux_variable_searching_strings!=NULL){
		if(emulator_enabled==0){
			emulator_enabled=1;
			HAL_Delay(50);
			HAL_UART_Transmit_DMA(&hlpuart1, on_emulationmessage, sizeof(on_emulationmessage));
		}else{
			emulator_enabled=0;
			HAL_Delay(50);
			HAL_UART_Transmit_DMA(&hlpuart1, off_emulationmessage, sizeof(off_emulationmessage));
		}
		return 1;
	}

	return 0;
}


uint16_t apply_delete_character_to_buffer(uint8_t *pData, uint16_t Size){
	uint16_t newSize_after_all_deletions=Size;
	for (uint32_t i = 0; i < Size; ++i) { //apply the "delete"=='/127' characters to received buffer
		if(pData[i]=='\177'){		//delete found
			if(i==0){				//if the delete was right at the start of the buffer
				newSize_after_all_deletions-=1;
				for (uint32_t j = i; j < Size; ++j) {
					pData[j]=pData[j+1]; //just shift the entire buffer effectively deleting the 'delete' character
				}
				i--;					 //recheck this index, it might have yet another 'del'
			}else{					//if the delete was NOT at the start of the buffer
				newSize_after_all_deletions-=2;
				for (uint32_t j = i; j < Size; ++j) {
					pData[j-1]=pData[j+1]; //shift the entire buffer effectively deleting the 'delete' character and the previous one
				}
			}
		}
	}
	return newSize_after_all_deletions;
}

hall_signals_order decode_char_to_hall_signals_order(uint8_t *pData){
	hall_signals_order returning_hallx=hall_order_unknown;

	switch (*pData) {
		case 'a':
		case 'A':
			returning_hallx=hall_A;
			break;
		case 'b':
		case 'B':
			returning_hallx=hall_B;
			break;
		case 'c':
		case 'C':
			returning_hallx=hall_C;
			break;
		default:
			returning_hallx=hall_order_unknown;
			break;
	}

	return returning_hallx;
}

hall_signals_polarity decode_char_to_hall_signals_polarity(uint8_t *pData){
	hall_signals_polarity returning_hall_polarity=hall_polarity_unknown;

	switch (*pData) {
		case '!':
			returning_hall_polarity=hall_inverse;
			break;
		default:
			returning_hall_polarity=hall_direct;
			break;
	}

	return returning_hall_polarity;
}

void reset_or_start_uart_DMA_communications(){
	HAL_UART_AbortReceive(&hlpuart1);
	HAL_UARTEx_ReceiveToIdle_DMA(&hlpuart1, rx_buffer, sizeof (rx_buffer));	//assign the dma+uart with the reception buffer
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart){

}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size){
	rx_flag=1;
	rx_size=Size;

}
