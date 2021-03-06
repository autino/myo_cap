// Includes /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Standard Libraries
#include <stdint.h>
#include <stdbool.h>

// Address and Names for Tiva
#include "inc/hw_memmap.h"
#include "inc/hw_timer.h"
#include "inc/hw_ints.h"

// Drivers for Tiva Hardware
#include "driverlib/sysctl.h"
#include "driverlib/gpio.h"
#include "driverlib/uart.h"
#include "driverlib/pin_map.h"
#include "driverlib/interrupt.h"
#include "driverlib/timer.h"
#include "driverlib/adc.h"

// Project Modules
#include "tiva_HAL.h"
#include "function_gen.h"
#include "comunication_protocol.h"
#include "string_commun_prot.h"
#include "streaming_protocol.h"
#include "debug.h"

//

// Functions Prototypes  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void reset_acquisition(void);

// Global Variables /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



// Actual State of Configuration of the Tiva

tiva_status         tiva_actual_state;


// Sample Memory for String Transmission

uint32_t            acquired_sample        = 0;
char                string_streaming_buffer[LENGTH_STRING_STREAMING_BUFFER];


// Sample Memory for Streaming Transmission
//------------------------------------------------------------------------------

//Ping Pong Buffers

uint16_t            acquis_pingpong_buf_0[NUM_MAX_SAMPLES_BUFFER];
uint16_t            acquis_pingpong_buf_1[NUM_MAX_SAMPLES_BUFFER];

streaming_buffer    stream_pigpong_buf_0;
streaming_buffer    stream_pigpong_buf_1;


// Pointers for Acquisition, Packing (IN , OUT) and Transmission Buffers

uint16_t*           acquisition_buffer = acquis_pingpong_buf_1 ;
uint16_t*           packing_buffer_in  = acquis_pingpong_buf_0 ;

streaming_buffer*   packing_buffer_out = &stream_pigpong_buf_1 ;
streaming_buffer*   transmit_buffer    = &stream_pigpong_buf_0 ;

//------------------------------------------------------------------------------


// Indexes for Streaming Streaming Transmission

uint16_t            board_index            = 0;
uint16_t            channel_index          = 0;
uint16_t            sample_index           = 0;


// Transmission Variables

uint8_t             state_of_tx_pkt      = TRANSMITTED;          // [ TRANSMITTED  | PENDING    | TRANSMITTING ]
uint8_t             type_of_transmission = UNPACKED   ;          // [ UNPACKED = 0 | PACKED = 1 ]

// Processing Variables

uint8_t             state_of_processing  = PROCESSED  ;          // [  PROCESSED   |  PENDING   |  PROCESSING  ]

// Multiplexer Variables

uint8_t             mux_channel_index      = 0;
uint8_t             mux_channel[]          = { MUX_CHANNEL_0, MUX_CHANNEL_1, MUX_CHANNEL_2, MUX_CHANNEL_3 };




// ISR (Interruption Service Routines) Functions /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


// Routine Called always that a Sampling Period Passes.

void timer0AInterrupt(void)
{

    TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT);                                                                                     // Clears the Interruption.


    if( tiva_actual_state.wave_form == ADC_Acquisition){                                                                                // ADC Acquisition Mode.

        acquired_sample = adc_sample_acquisition();
    }

    if( (tiva_actual_state.wave_form == Sine_Wave    ) ||                                                                               // Function Generator Mode.
        (tiva_actual_state.wave_form == Square_Wave  ) ||
        (tiva_actual_state.wave_form == Sawtooth_Wave)    )

    {

        tiva_actual_state.timestamp = ( tiva_actual_state.period_Func_Gen - TimerValueGet(TIMER1_BASE, TIMER_A) );                      // Calculates the real time inside a Function Generator Period.
        acquired_sample = function_gen(tiva_actual_state.wave_form, tiva_actual_state.timestamp, tiva_actual_state.func_gen_frequency); // Generates e Store a Sample of the Function Generator with the Wave Form Chosen.

    }


    // For Unpacked Transmission
    if( type_of_transmission == UNPACKED ){

        uintToStr(mux_channel_index, acquired_sample, string_streaming_buffer);                                                             // Codifies the Sample

        mux_channel_index++;

        if(mux_channel_index == tiva_actual_state.num_channels_per_board)
        {
            mux_channel_index = 0;
            string_streaming_buffer[LENGTH_STRING_STREAMING_BUFFER - 1] = '\n';
            uartSend(string_streaming_buffer, LENGTH_STRING_STREAMING_BUFFER);                                                              // Transmit the 4 Samples
        }

        set_mux_channel(mux_channel_index);                                                                                                 // Change the value of the Mux Selection Pins.

    }


    // For Packed Transmission
    if( type_of_transmission == PACKED ){

        acquisition_buffer[ board_index   *  (tiva_actual_state.num_samples_per_chn_buf) * (tiva_actual_state.num_channels_per_board)       // Stores the Acquired Sample in the Acquisition Buffer.
                          + sample_index  *  (tiva_actual_state.num_channels_per_board)
                          + channel_index ] = acquired_sample;

        mux_channel_index++; sample_index++;

        if(mux_channel_index == tiva_actual_state.num_channels_per_board)   { mux_channel_index = 0;   sample_index++;                }     // Checks if all Channels of one board was Sampled.

        set_mux_channel(mux_channel_index);                                                                                                 // Change the value of the Multiplexer Selection Pins.

        if(sample_index == tiva_actual_state.num_samples_per_chn_buf)       { sample_index = 0;        board_index++;                 }     // Checks if all Samples of one board was Sampled.

        if(board_index  == tiva_actual_state.nums_of_acquis_boards  )       {                                                               // Checks if all Boards was Sampled.

            uint16_t* temp_ptr = acquisition_buffer ;                                                                                       // Toggles the Buffer
            acquisition_buffer = packing_buffer_in  ;
            packing_buffer_in  = temp_ptr           ;

            board_index  = 0;        state_of_processing = PENDING; }                                                                       // Trigger a Packing Process

    }


}


// Routine Called always that a Byte is Received through the UART0. It Call the Command Handler with the Packet received.

void uart0Interrupt(void)
{

    UARTIntClear(UART0_BASE, UART_INT_RX | UART_INT_RT);

    comunication_packet pkt_rcvd;

    stop_trasmission();
    recieve_packet(&pkt_rcvd);
    reset_acquisition();
    command_handler( &pkt_rcvd, &tiva_actual_state );

}



// Tiva Configuration Function //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void configurations()
{
    tiva_actual_state_init(&tiva_actual_state);

    int baudrate = DEFAULT_BAUDRATE;

    SysCtlClockSet(SYSCTL_SYSDIV_5 | SYSCTL_USE_PLL | SYSCTL_XTAL_16MHZ | SYSCTL_OSC_MAIN);                            // Sets the Tiva frequency as 40 MHz.
    IntMasterEnable();                                                                                                 // Enables the Interruptions.

    configureUART(baudrate);
    configureTimer(&tiva_actual_state);
    configureSelectPins();
    configureADC();


    // Clear the Buffer Used in String Protocol

    clearStr(string_streaming_buffer, LENGTH_STRING_STREAMING_BUFFER);


    // Clear the Buffers Used in Streaming Protocol

    clear_acquisition_buffer( acquis_pingpong_buf_0 , NUM_MAX_SAMPLES_BUFFER);
    clear_acquisition_buffer( acquis_pingpong_buf_1 , NUM_MAX_SAMPLES_BUFFER);

    clear_streaming_buffer  ( &stream_pigpong_buf_0 , NUM_MAX_BYTES_BUFFER );
    clear_streaming_buffer  ( &stream_pigpong_buf_1 , NUM_MAX_BYTES_BUFFER );

}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void reset_acquisition(void)
{

    // Clear the Buffer Used in String Protocol

    clearStr(string_streaming_buffer, LENGTH_STRING_STREAMING_BUFFER);


    // Clear the Buffers Used in Streaming Protocol

    clear_acquisition_buffer( acquis_pingpong_buf_0 , NUM_MAX_SAMPLES_BUFFER);
    clear_acquisition_buffer( acquis_pingpong_buf_1 , NUM_MAX_SAMPLES_BUFFER);

    clear_streaming_buffer  ( &stream_pigpong_buf_0 , NUM_MAX_BYTES_BUFFER );
    clear_streaming_buffer  ( &stream_pigpong_buf_1 , NUM_MAX_BYTES_BUFFER );

    // Sample Memory for String Transmission

    acquired_sample        = 0;

    // Pointers for Acquisition, Packing (IN , OUT) and Transmission Buffers

    acquisition_buffer = acquis_pingpong_buf_1 ;
    packing_buffer_in  = acquis_pingpong_buf_0 ;

    packing_buffer_out = &stream_pigpong_buf_1 ;
    transmit_buffer    = &stream_pigpong_buf_0 ;

    //------------------------------------------------------------------------------


    // Indexes for Streaming Streaming Transmission

    board_index            = 0;
    channel_index          = 0;
    sample_index           = 0;


    // Transmission Variables

    state_of_tx_pkt      = TRANSMITTED;          // [ TRANSMITTED  | PENDING    | TRANSMITTING ]
    type_of_transmission = UNPACKED   ;          // [ UNPACKED = 0 | PACKED = 1 ]

    // Processing Variables

    state_of_processing  = PROCESSED  ;          // [  PROCESSED   |  PENDING   |  PROCESSING  ]

    // Multiplexer Variables

    mux_channel_index      = 0;



}


void main(void)
{

    configurations();

    // Finite State Machine that Controls the Tiva Behavior.
    while(1) {

        // The Acquisition of the Samples happends by a Interruption Periodically. That can be done interrupting the Processing, Transmission of the Samples.

        // Packs the Samples Acquired
        if( state_of_processing == PENDING ){

            state_of_processing = PROCESSING;

            // Pack all Sample in the buffer.
            pack_samples_buffer(packing_buffer_in,
                                tiva_actual_state.nums_of_acquis_boards,
                                tiva_actual_state.num_channels_per_board,
                                tiva_actual_state.num_samples_per_chn_buf,
                                tiva_actual_state.bits_per_sample,
                                packing_buffer_out);

            state_of_processing = PROCESSED;

            // Toggles the Buffer
            streaming_buffer*  temp_ptr = transmit_buffer    ;
            transmit_buffer             = packing_buffer_out ;
            packing_buffer_out          = temp_ptr           ;

            state_of_tx_pkt = PENDING;                                                                          // Trigger a Transmission of Samples.
        }

        // Send the Packed Samples in Transmission Buffer.
        if( state_of_tx_pkt == PENDING ){

            state_of_tx_pkt = TRANSMITTING;
            send_stream_packet(transmit_buffer, &state_of_tx_pkt, &tiva_actual_state);
            state_of_tx_pkt = TRANSMITTED;

        }

        // If a Command through the UART was received, that can be processed interrupting the Processing, or Transmission of the Samples.
    }

}
