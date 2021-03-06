/*
 * comunication_protocol.c
 *
 *  Created on: 9 de dez de 2019
 *      Author: Lar00
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/sysctl.h"
#include "driverlib/gpio.h"
#include "driverlib/uart.h"
#include "driverlib/pin_map.h"
#include "utils/uartstdio.h"
#include "driverlib/interrupt.h"
#include "inc/hw_ints.h"
#include "driverlib/timer.h"
#include "inc/hw_timer.h"

#include "function_gen.h"           // New module
#include "comunication_protocol.h"  // New module
#include "tiva_HAL.h"


//  Prototypes  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void start_trasmission ();
void stop_trasmission  ();
void acknowledgment    ();
void not_acknowledgment();

void set_func_gen_frequency( tiva_status* tiva_actual_status, uint32_t frequency );
void set_sample_rate       (tiva_status*  tiva_actual_status, uint32_t sample_rate  );
void set_number_of_channels(tiva_status*  tiva_actual_status, uint32_t num_channels );
void set_number_of_acquis_boards (tiva_status*  tiva_actual_status, uint32_t num_acquis_boards );
void set_number_of_bytes_in_packet (tiva_status*  tiva_actual_status, uint32_t num_bytes_in_pkt );
void set_bits_per_sample (tiva_status*  tiva_actual_status, uint32_t bits_per_sample );

void get_sample_rate (tiva_status*  tiva_actual_status );
void get_number_of_channels (tiva_status*  tiva_actual_status );
void get_number_of_boards (tiva_status*  tiva_actual_status );
void get_number_of_bytes_in_packet (tiva_status*  tiva_actual_status );
void get_bits_per_sample (tiva_status*  tiva_actual_status );
void get_func_gen_wave_form (tiva_status*  tiva_actual_status );
void get_func_gen_frequency (tiva_status*  tiva_actual_status );
uint32_t get_maximum_fucgen_freq(tiva_status*  tiva_actual_status );
uint32_t get_maximum_bytes_in_pkt(tiva_status*  tiva_actual_status);
uint32_t get_minimum_bytes_in_pkt(tiva_status*  tiva_actual_status);



void command_handler(comunication_packet *pkt_received, tiva_status *tiva_actual_state){


    switch(pkt_received->uint32_param_pkt.command){                                // Checks the command in the received packet


        // Flow Control Commands

        case START_TRANSMISSION         :    start_trasmission();                                                                             acknowledgment();      break;

        case STOP_TRANSMISSION          :    stop_trasmission();                                                                              acknowledgment();      break;


        // Set Parameters of the Capture and Transmission

        case SET_SAMPLE_RATE           :    set_sample_rate (tiva_actual_state, pkt_received->uint32_param_pkt.uint32_operand);                                      break;

        case SET_NUMBER_CHANNELS       :    set_number_of_channels (tiva_actual_state, pkt_received->uint32_param_pkt.uint32_operand);                               break;

        case SET_NUMBER_BOARDS         :    set_number_of_acquis_boards (tiva_actual_state, pkt_received->uint32_param_pkt.uint32_operand);                          break;

        case SET_NUMBER_BYTES_IN_PACKET:    set_number_of_bytes_in_packet (tiva_actual_state, pkt_received->uint32_param_pkt.uint32_operand);                        break;

        case SET_BITS_PER_SAMPLE       :    set_bits_per_sample (tiva_actual_state, pkt_received->uint32_param_pkt.uint32_operand);                                  break;


        // Get Parameters of the Capture and Transmission

        case GET_SAMPLE_RATE           :    get_sample_rate (tiva_actual_state);                                                               acknowledgment();      break;

        case GET_NUMBER_CHANNELS       :    get_number_of_channels (tiva_actual_state);                                                        acknowledgment();      break;

        case GET_NUMBER_BOARDS         :    get_number_of_boards (tiva_actual_state);                                                          acknowledgment();      break;

        case GET_NUMBER_BYTES_IN_PACKET:    get_number_of_bytes_in_packet (tiva_actual_state);                                                 acknowledgment();      break;

        case GET_BITS_PER_SAMPLE       :    get_bits_per_sample (tiva_actual_state);                                                           acknowledgment();      break;


        // Function Generator

        case SET_FUNC_GEN_FREQUENCY    :    set_func_gen_frequency(tiva_actual_state, pkt_received->uint32_param_pkt.uint32_operand);                                 break;

        case SET_ADC_ACQUISITION       :    tiva_actual_state->wave_form = ADC_Acquisition;                                                    acknowledgment();      break;

        case SET_FUNC_GEN_SQUARE_WAVE  :    tiva_actual_state->wave_form = Square_Wave;                                                        acknowledgment();      break;

        case SET_FUNC_GEN_SAWTOOTH_WAVE:    tiva_actual_state->wave_form = Sawtooth_Wave;                                                      acknowledgment();      break;

        case SET_FUNC_GEN_SIN_WAVE     :    tiva_actual_state->wave_form = Sine_Wave;                                                          acknowledgment();      break;


        case GET_FUNC_GEN_WAVE_FORM    :    get_func_gen_wave_form ( tiva_actual_state );                                                      acknowledgment();      break;

        case GET_FUNC_GEN_FREQUENCY    :    get_func_gen_frequency ( tiva_actual_state );                                                      acknowledgment();      break;


        // Not Recognised Command

        default                        :    not_acknowledgment();

    }

}


// Sends a uint32 Parameter Packet

void send_packet(comunication_packet *pkt_to_send){

        // Sends Command

        UARTCharPut(UART0_BASE, pkt_to_send->bytes[ OFFSET_COMMAND  + ( SIZE_COMMAND - 1 ) ] );
        UARTCharPut(UART0_BASE, pkt_to_send->bytes[ OFFSET_COMMAND  + ( SIZE_COMMAND - 2 ) ] );

        // Sends uint32 Parameter

        UARTCharPut(UART0_BASE, pkt_to_send->bytes[ OFFSET_OPERAND  + ( SIZE_OPERAND - 1 ) ] );
        UARTCharPut(UART0_BASE, pkt_to_send->bytes[ OFFSET_OPERAND  + ( SIZE_OPERAND - 2 ) ] );
        UARTCharPut(UART0_BASE, pkt_to_send->bytes[ OFFSET_OPERAND  + ( SIZE_OPERAND - 3 ) ] );
        UARTCharPut(UART0_BASE, pkt_to_send->bytes[ OFFSET_OPERAND  + ( SIZE_OPERAND - 4 ) ] );

}


void recieve_packet(comunication_packet* pkt_to_be_received){

        uint8_t buffer_in[LENGTH_BUFFER_IN];

        // Reads the Serial Buffer
        int i = 0;    while(UARTCharsAvail(UART0_BASE)) buffer_in[i++] = (char)UARTCharGetNonBlocking(UART0_BASE);


        // Receives a Packet

        // Receives a Command
        pkt_to_be_received->bytes[ OFFSET_COMMAND  + ( SIZE_COMMAND - 1 ) ]  =  buffer_in[0] ;
        pkt_to_be_received->bytes[ OFFSET_COMMAND  + ( SIZE_COMMAND - 2 ) ]  =  buffer_in[1] ;

        // Receives uint32 or float32 Parameter
        pkt_to_be_received->bytes[ OFFSET_OPERAND  + ( SIZE_OPERAND - 1 ) ]  =  buffer_in[2] ;
        pkt_to_be_received->bytes[ OFFSET_OPERAND  + ( SIZE_OPERAND - 2 ) ]  =  buffer_in[3] ;
        pkt_to_be_received->bytes[ OFFSET_OPERAND  + ( SIZE_OPERAND - 3 ) ]  =  buffer_in[4] ;
        pkt_to_be_received->bytes[ OFFSET_OPERAND  + ( SIZE_OPERAND - 4 ) ]  =  buffer_in[5] ;

}




void start_trasmission(){

    TimerEnable(TIMER0_BASE, TIMER_A);
    IntEnable(INT_TIMER0A);

}


void stop_trasmission(){

    TimerDisable(TIMER0_BASE, TIMER_A);
    IntDisable(INT_TIMER0A);
    GPIOPinWrite(GPIO_PORTE_BASE, GPIO_PIN_1 | GPIO_PIN_2, 0);

}


void acknowledgment(){

    comunication_packet pkt_send;
    pkt_send.uint32_param_pkt.command        = ACK   ;
    pkt_send.uint32_param_pkt.uint32_operand = 0x0000;
    send_packet(&pkt_send);

}


void not_acknowledgment(){

    comunication_packet pkt_send;
    pkt_send.uint32_param_pkt.command        = NAK   ;
    pkt_send.uint32_param_pkt.uint32_operand = 0x0000;
    send_packet(&pkt_send);

}


void set_func_gen_frequency( tiva_status* tiva_actual_status, uint32_t frequency ){


    if( ( frequency <= get_maximum_fucgen_freq(tiva_actual_status) ) && ( frequency > 0 ) ){

        (*tiva_actual_status).func_gen_frequency  =  frequency                   ;
        (*tiva_actual_status).period_Func_Gen     =  SysCtlClockGet() / frequency;            // Calculates the Period of the Function Generator in terms of multiples of the System Period.
        TimerLoadSet(TIMER1_BASE, TIMER_A, (*tiva_actual_status).period_Func_Gen - 1);        // Load the Period of the Signal to be generated by the Function Generator.

        // Sends a Message of Success
        acknowledgment();

    }

    // Sends a Error Message
    else{

        comunication_packet pkt_send;
        pkt_send.uint32_param_pkt.command        = ERROR_MSG;
        pkt_send.uint32_param_pkt.uint32_operand = sizeof(MSG_OOR_FUNC_GEN_FREQUENCY);
        send_packet(&pkt_send);

        // Transmit the Samples
        uint32_t byte_index;
        for(byte_index = 0; byte_index <  pkt_send.uint32_param_pkt.uint32_operand ; byte_index++)      UARTCharPut(UART0_BASE, MSG_OOR_FUNC_GEN_FREQUENCY[byte_index]);


    }
}


void set_sample_rate (tiva_status*  tiva_actual_status, uint32_t sample_rate ){

    // Calculates the new Sampling Rate of the ADC => (Sample Rate by Channel) * (Number of Channels to be Sampled)
    uint32_t samplerate_mux = ( sample_rate * ( (*tiva_actual_status).nums_of_acquis_boards * (*tiva_actual_status).num_channels_per_board )  );

    // Checks if the new Sample Rate is Achievable
    if( samplerate_mux <  ADC_MAX_SAMPLING_RATE  ){

       (*tiva_actual_status).samplerate =  sample_rate;

       int period = SysCtlClockGet() / samplerate_mux;

       TimerLoadSet(TIMER0_BASE, TIMER_A, period - 1);

       // Sends a Message of Success
       acknowledgment();

    }


    // Sends a Error Message
    else{

        comunication_packet pkt_send;
        pkt_send.uint32_param_pkt.command        = ERROR_MSG;
        pkt_send.uint32_param_pkt.uint32_operand = sizeof(MSG_OOR_SAMPLE_RATE);
        send_packet(&pkt_send);

        // Transmit the Samples
        uint32_t byte_index;
        for(byte_index = 0; byte_index <  pkt_send.uint32_param_pkt.uint32_operand ; byte_index++)      UARTCharPut(UART0_BASE, MSG_OOR_SAMPLE_RATE[byte_index]);


    }
}


void set_number_of_channels (tiva_status*  tiva_actual_status, uint32_t num_channels ){


    if( (num_channels >= NUM_MIN_CHANNELS_PER_BOARD)  && (num_channels <= NUM_MAX_CHANNELS_PER_BOARD) ){

       (*tiva_actual_status).num_channels_per_board =  num_channels;

       // Recalculate and set the Number of Instants of Capture that will be transmitted in a packet.
       (*tiva_actual_status).num_samples_per_chn_buf = (*tiva_actual_status).num_bytes_in_packet / ( tiva_actual_status->nums_of_acquis_boards * tiva_actual_status->num_channels_per_board * tiva_actual_status->bits_per_sample );

       // Sends a Message of Success
       acknowledgment();

    }

    // Sends a Error Message
    else{

        comunication_packet pkt_send;
        pkt_send.uint32_param_pkt.command        = ERROR_MSG;
        pkt_send.uint32_param_pkt.uint32_operand = sizeof(MSG_OOR_NUMBER_CHANNELS);
        send_packet(&pkt_send);

        // Transmit the Samples
        uint32_t byte_index;
        for(byte_index = 0; byte_index <  pkt_send.uint32_param_pkt.uint32_operand ; byte_index++)      UARTCharPut(UART0_BASE, MSG_OOR_NUMBER_CHANNELS[byte_index]);


    }

}


void set_number_of_acquis_boards (tiva_status*  tiva_actual_status, uint32_t num_acquis_boards ){

    if( (num_acquis_boards >= NUM_MIN_ACQUISITION_BOARDS)  && (num_acquis_boards <= NUM_MAX_ACQUISITION_BOARDS) ){

       (*tiva_actual_status).nums_of_acquis_boards =  num_acquis_boards;

       // Recalculate and set the Number of Instants of Capture that will be transmitted in a packet.
       (*tiva_actual_status).num_samples_per_chn_buf = (*tiva_actual_status).num_bytes_in_packet / ( tiva_actual_status->nums_of_acquis_boards * tiva_actual_status->num_channels_per_board * tiva_actual_status->bits_per_sample );

       // Sends a Message of Success
       acknowledgment();

    }

    // Sends a Error Message
    else{

        comunication_packet pkt_send;
        pkt_send.uint32_param_pkt.command        = ERROR_MSG;
        pkt_send.uint32_param_pkt.uint32_operand = sizeof(MSG_OOR_NUMBER_BOARDS);
        send_packet(&pkt_send);

        // Transmit the Samples
        uint32_t byte_index;
        for(byte_index = 0; byte_index <  pkt_send.uint32_param_pkt.uint32_operand ; byte_index++)      UARTCharPut(UART0_BASE, MSG_OOR_NUMBER_BOARDS[byte_index]);

    }

}


void set_number_of_bytes_in_packet (tiva_status*  tiva_actual_status, uint32_t num_bytes_in_pkt ){


    uint32_t num_max_bytes_in_pkt = get_maximum_bytes_in_pkt(tiva_actual_status);
    uint32_t num_min_bytes_in_pkt = get_minimum_bytes_in_pkt(tiva_actual_status);

    if(  (num_bytes_in_pkt >= num_min_bytes_in_pkt)  &&  (num_bytes_in_pkt <= num_max_bytes_in_pkt)  ){

       // Sets the Number of Bytes in a Packet
       (*tiva_actual_status).num_bytes_in_packet =  num_bytes_in_pkt;

       // Calculate and set the Number of Instants of Capture that will be transmitted in a packet.
       (*tiva_actual_status).num_samples_per_chn_buf = num_bytes_in_pkt / ( tiva_actual_status->nums_of_acquis_boards * tiva_actual_status->num_channels_per_board * tiva_actual_status->bits_per_sample );

       // Sends a Message of Success
       acknowledgment();
    }

    // Sends a Error Message
    else{

        comunication_packet pkt_send;
        pkt_send.uint32_param_pkt.command        = ERROR_MSG;
        pkt_send.uint32_param_pkt.uint32_operand = sizeof(MSG_OOR_BYTES_IN_PACKET);
        send_packet(&pkt_send);

        // Transmit the Samples
        uint32_t byte_index;
        for(byte_index = 0; byte_index <  pkt_send.uint32_param_pkt.uint32_operand ; byte_index++)      UARTCharPut(UART0_BASE, MSG_OOR_BYTES_IN_PACKET[byte_index]);

    }


}


void set_bits_per_sample (tiva_status*  tiva_actual_status, uint32_t bits_per_sample ){



    if(  (bits_per_sample >= NUM_MIN_BITS_PER_SAMPLE)  &&  (bits_per_sample <= NUM_MAX_BITS_PER_SAMPLE)  ){

        (*tiva_actual_status).bits_per_sample =  bits_per_sample;

        // Recalculate and set the Number of Instants of Capture that will be transmitted in a packet.
        (*tiva_actual_status).num_samples_per_chn_buf = (*tiva_actual_status).num_bytes_in_packet / ( tiva_actual_status->nums_of_acquis_boards * tiva_actual_status->num_channels_per_board * tiva_actual_status->bits_per_sample );


        // Sends a Message of Success
        acknowledgment();
    }

    // Sends a Error Message
    else{

        comunication_packet pkt_send;
        pkt_send.uint32_param_pkt.command        = ERROR_MSG;
        pkt_send.uint32_param_pkt.uint32_operand = sizeof(MSG_OOR_BITS_PER_SAMPLE);
        send_packet(&pkt_send);

        // Transmit the Samples
        uint32_t byte_index;
        for(byte_index = 0; byte_index <  pkt_send.uint32_param_pkt.uint32_operand ; byte_index++)      UARTCharPut(UART0_BASE, MSG_OOR_BITS_PER_SAMPLE[byte_index]);

    }



}


void get_sample_rate (tiva_status*  tiva_actual_status ){

   comunication_packet pkt_send;
   pkt_send.uint32_param_pkt.command        = INT32_VALUE;
   pkt_send.uint32_param_pkt.uint32_operand = (*tiva_actual_status).samplerate;
   send_packet(&pkt_send);

}


void get_number_of_channels (tiva_status*  tiva_actual_status ){

   comunication_packet pkt_send;
   pkt_send.uint32_param_pkt.command        = INT32_VALUE;
   pkt_send.uint32_param_pkt.uint32_operand = (*tiva_actual_status).num_channels_per_board;
   send_packet(&pkt_send);

}


void get_number_of_boards (tiva_status*  tiva_actual_status ){

   comunication_packet pkt_send;
   pkt_send.uint32_param_pkt.command        = INT32_VALUE;
   pkt_send.uint32_param_pkt.uint32_operand = (*tiva_actual_status).nums_of_acquis_boards;
   send_packet(&pkt_send);

}


void get_number_of_bytes_in_packet (tiva_status*  tiva_actual_status ){

   comunication_packet pkt_send;
   pkt_send.uint32_param_pkt.command        = INT32_VALUE;
   pkt_send.uint32_param_pkt.uint32_operand = (*tiva_actual_status).num_bytes_in_packet;
   send_packet(&pkt_send);

}


void get_func_gen_frequency (tiva_status*  tiva_actual_status ){

   comunication_packet pkt_send;
   pkt_send.uint32_param_pkt.command        = INT32_VALUE;
   pkt_send.uint32_param_pkt.uint32_operand = (*tiva_actual_status).func_gen_frequency;
   send_packet(&pkt_send);

}


void get_func_gen_wave_form (tiva_status*  tiva_actual_status ){

   comunication_packet pkt_send;
   pkt_send.uint32_param_pkt.command        = INT32_VALUE;
   pkt_send.uint32_param_pkt.uint32_operand = (*tiva_actual_status).wave_form;
   send_packet(&pkt_send);

}


void get_bits_per_sample (tiva_status*  tiva_actual_status ){

   comunication_packet pkt_send;
   pkt_send.uint32_param_pkt.command        = INT32_VALUE;
   pkt_send.uint32_param_pkt.uint32_operand = (*tiva_actual_status).bits_per_sample;
   send_packet(&pkt_send);

}


uint32_t get_maximum_fucgen_freq(tiva_status*  tiva_actual_status ){

    return (uint32_t)( ((*tiva_actual_status).samplerate)/ 2 );

}


uint32_t get_maximum_bytes_in_pkt(tiva_status*  tiva_actual_status ){

    return (uint32_t)(  ( NUM_MAX_SAMPLES_PER_CHANNEL * ( (*tiva_actual_status).bits_per_sample ) * ( (*tiva_actual_status).nums_of_acquis_boards ) * ( (*tiva_actual_status).num_channels_per_board )  ) / 8   );

}


uint32_t get_minimum_bytes_in_pkt(tiva_status*  tiva_actual_status ){

    return (uint32_t)(  (   ( (*tiva_actual_status).bits_per_sample )  *  ( (*tiva_actual_status).nums_of_acquis_boards )  *  ( (*tiva_actual_status).num_channels_per_board )  ) / 8   );

}


