#pragma config OSC = HSPLL, FCMEN = OFF, IESO = OFF, PWRT = OFF, BOREN = OFF, WDT = OFF, MCLRE = ON, LPT1OSC = OFF, LVP = OFF, XINST = OFF, DEBUG = OFF
#define _XTAL_FREQ 40000000
#include <xc.h>
#include <stdint.h>
#include <stdio.h>

typedef enum {TMR_IDLE, TMR_RUN, TMR_DONE} tmr_state_t;
tmr_state_t tmr_state = TMR_IDLE;   // Current timer state

typedef enum {G_INIT,G_RUN} game_state_g;
game_state_g game_state = G_INIT;   // Current game state

typedef enum {NOT_IN_GAME, LEVEL_1, LEVEL_2, LEVEL_3} curr_level_l;
curr_level_l curr_level = NOT_IN_GAME; // Current level state

typedef enum {EMPTY, END, LOSE, IN_GAME} ssdisplay_state_s;
ssdisplay_state_s ssdisplay_state = EMPTY; // Current seven segment display state

uint8_t tmr_startreq = 0;
uint8_t cycles_left;
uint8_t health_point;
uint8_t notes_to_be_generated;
uint8_t ssdisplay_porth_selection = 0;

uint8_t note_caught=0, rc0_waiting_for_release=0, rc0_pressed_and_released=0, rg0_waiting_for_release=0;
uint8_t rg0_pressed_and_released=0, rg1_waiting_for_release=0, rg1_pressed_and_released=0, rg2_waiting_for_release=0;
uint8_t rg2_pressed_and_released=0, rg3_waiting_for_release=0, rg3_pressed_and_released=0, rg4_waiting_for_release=0, rg4_pressed_and_released=0;

uint8_t timer_value, least3, result;
uint16_t holder_16bit;

void tmr0_ISR();
void tmr1_ISR();

void initADC(){
    /* Configuration for ADC Module */
    /* ADCON0 -> b'00101000', which means, we have used 10th channel and GODONE, ADON is disabled for now
     * ADCON1 -> b'00000000', which means, we have set all channels as analog input channels
     * ADCON2 -> b'10101010', which means, we have used 12Tad for Acquisition Time and Fosc/32 for Conversion Time
     */
    //TRISCbits.TRISC4 = 1 ;
    ADCON0 = 0b00101000 ;
    ADCON1 = 0 ;
    ADCON2 = 170 ;

    /* Enable ADON bit for Usage of ADC Module in future */
    ADON = 1 ;
}

// High priority interrupt
void __interrupt(high_priority) highPriorityISR(void) {

    // Timer0 interrupt, just calling the timer0 ISR
    if (INTCONbits.TMR0IF)
        tmr0_ISR();

    // Timer1 interrupt, just calling the timer0 ISR
    if (PIR1bits.TMR1IF)
        tmr1_ISR();

}

// Low priority interrupt, no usage
void __interrupt(low_priority) lowPriorityISR(void) {}

// Interrupt initializations
void init_interrupts() {
    RCONbits.IPEN = 0;
    INTCONbits.TMR0IE = 1;
    PIE1bits.TMR1IE = 1;
    INTCONbits.GIE = 1;
}

// Port initializations
void init_ports() {

    TRISA = 0b00000000;
    TRISB = 0b00000000;
    TRISC = 0b00000000;
    TRISD = 0b00000000;
    TRISE = 0b00000000;
    TRISF = 0b00000000;
    TRISG = 0b00011111;  // RG0 - RG4 are inputs
    TRISJ = 0b00000000;
    TRISH = 0b00000000;

    PORTA = 0x00;
    PORTB = 0x00;
    PORTC = 0x00;
    PORTD = 0x00;
    PORTE = 0x00;
    PORTF = 0x00;
    PORTG = 0x00;
    PORTH = 0x00;
    PORTJ = 0x00;

    LATA = 0x00;
    LATB = 0x00;
    LATC = 0x00;
    LATD = 0x00;
    LATE = 0x00;
    LATF = 0x00;
    LATG = 0x00;
    LATH = 0x00;
    LATJ = 0x00;
    
    ADCON1bits.PCFG = 0b1111;
    SWDTEN = 0;

}

// Timer initializations
void init_timers() {

    T0CON = 0b01000111; // 8-bit timer, internal clock with 1:256 prescaler, TMR0ON = 0
    T0CONbits.TMR0ON = 1;
    TMR0L = 0x00;
    PIR1bits.TMR1IF = 0;
    T1CON = 0x00;
    T1CONbits.RD16 = 1;
    T1CONbits.TMR1ON = 1;   // 16-bit timer, internal clock without prescaler, TMR10N = 1
    TMR1L = 0x00;
    TMR1H = 0x00;

}

// Timer0 ISR, should be called "cycle_count" times to achieve the desired delay
void tmr0_ISR() {

    INTCONbits.TMR0IF = 0;
    
    if (curr_level != NOT_IN_GAME) {
        if (--cycles_left == 0)      //  If no counts left, then the desired delay is obtained. If not, continue staying in TMR_RUN and delaying
            tmr_state = TMR_DONE;
        else
            TMR0L = 0x00;
    }

}

// Timer1 ISR, increments ssdisplay_porth_selection to choose which display to show inside the time interval
void tmr1_ISR() {

    PIR1bits.TMR1IF = 0;
    
    if (ssdisplay_porth_selection == 3)
        ssdisplay_porth_selection = 0;
    else
        ssdisplay_porth_selection++;
    
}

// This function is called at every and of time interval to start the next time delay. It makes sure that the desired delay is obtained by "cycle_count" values for each level:
// level_1 -> 76 (500 ms), level_2 -> 61 (400 ms), level_3 -> 46 (300 ms)
void tmr0_start(uint8_t cycle_count) {

    cycles_left = cycle_count;
    tmr_startreq = 1;
    tmr_state = TMR_IDLE;

}

// This function aborts timer0, called only when the game ends
void tmr0_abort() {

    tmr_startreq = 0;
    tmr_state = TMR_IDLE;

}

// Seven segment display task
void ssdisplay_task() {

    if (ssdisplay_porth_selection == 0) {
        LATH = 0b00000001;
        if( ssdisplay_state == 0 ){
            LATJ = 0b00000000;
        }
        else if( ssdisplay_state == 1 ){
            LATJ = 0b01111001;
        }
        else if( ssdisplay_state == 2 ){
            LATJ = 0b00111000;
        }
        else{
            if (health_point == 9)
                LATJ = 0b01101111;
            else if (health_point == 8)
                LATJ = 0b01111111;
            else if (health_point == 7)
                LATJ = 0b00000111;
            else if (health_point == 6)
                LATJ = 0b01111101;
            else if (health_point == 5)
                LATJ = 0b01101101;
            else if (health_point == 4)
                LATJ = 0b01100110;
            else if (health_point == 3)
                LATJ = 0b01001111;
            else if (health_point == 2)
                LATJ = 0b01011011;
            else if (health_point == 1)
                LATJ = 0b00000110;
            }
    }

    else if (ssdisplay_porth_selection == 1) {
        LATH = 0b00000010;
        if( ssdisplay_state == 0 ){
            LATJ = 0b00000000;
        }
        else if( ssdisplay_state == 1 ){
            LATJ = 0b01010100;
        }
        else if( ssdisplay_state == 2 ){
            LATJ = 0b00111111;
        }
        else{
            LATJ = 0b00000000;
        }
    }

    else if (ssdisplay_porth_selection == 2) {
        LATH = 0b00000100;
        if( ssdisplay_state == 0 ){
            LATJ = 0b00000000;
        }
        else if( ssdisplay_state == 1 ){
            LATJ = 0b01011110;
        }
        else if( ssdisplay_state == 2 ){
            LATJ = 0b01101101;
        }
        else{
            LATJ = 0b00000000;
        }
    }

    else if (ssdisplay_porth_selection == 3) {
        LATH = 0b00001000;
        if( ssdisplay_state == 0 ){
            LATJ = 0b00000000;
        }
        else if( ssdisplay_state == 1 ){
            LATJ = 0b00000000;
        }
        else if( ssdisplay_state == 2 ){
            LATJ = 0b01111001;
        }
        else{
            if( curr_level == LEVEL_1 ){
                LATJ = 0b00000110;
            }
            else if( curr_level == LEVEL_2 ){
                LATJ = 0b01011011;
            }
            else{
                LATJ = 0b01001111;
            }
        }
    }

}


// Timer task
void timer_task() {

    switch (tmr_state) {

        case TMR_IDLE:
            if (tmr_startreq) {
                // If tmr0_start() is called, i.e. a tmr_startreq has been issued, go to the TMR_RUN state
                tmr_startreq = 0;
                TMR0L = 0x00;
                INTCONbits.T0IF = 0; // Making sure that the timer interrupt flag is unset
                tmr_state = TMR_RUN;
            }
            break;

        case TMR_RUN:
            // State waits here until the timer interrupt fires
            break;

        case TMR_DONE:
            // State comes here at the end of each time interval
            break;

    }

}

void finish_game() {

    PORTA = 0x00;
    PORTB = 0x00;
    PORTC = 0x00;
    PORTD = 0x00;
    PORTE = 0x00;
    PORTF = 0x00;
    PORTG = 0x00;

    LATA = 0x00;
    LATB = 0x00;
    LATC = 0x00;
    LATD = 0x00;
    LATE = 0x00;
    LATF = 0x00;
    LATG = 0x00;
    LATH = 0x00;

    tmr0_abort();
    game_state = G_INIT;
    curr_level = NOT_IN_GAME;

}

// This function decreases health point by 1, handles the seven segment display accordingly and returns 1 if there is no health point left
uint8_t decrease_health() {

    health_point--;

    // If there is no health point left; display LOSE on seven segment display, clear all ports, abort the timer and return the game state to G_INIT
    if (health_point == 0) {
        finish_game();
        ssdisplay_state = LOSE;
        return 1;
    }
    return 0;

}

// This function shifts the global variable "holder_16bit" to the right by 1 and places the 0th bit to the 15th bit
void rotate() {

    if (holder_16bit % 2 == 0)
        holder_16bit = holder_16bit >> 1 ;
    else
        holder_16bit = (holder_16bit >> 1) | 0x8000;

}

// This function is called by the shift_notes() function and generates a random note on RA0-RA4
void generate_note() {

    // Taking the least significant 3 bits of TMR1L and applying modulo 5 to generate a random note
    timer_value = TMR1L;
    least3 = timer_value % 8;
    result = least3 % 5;
    LATA = 0x00;

    switch(result) {
        case 0:
            LATAbits.LA0 = 1;
            break;
        case 1:
            LATAbits.LA1 = 1;
            break;
        case 2:
            LATAbits.LA2 = 1;
            break;
        case 3:
            LATAbits.LA3 = 1;
            break;
        case 4:
            LATAbits.LA4 = 1;
            break;
    }

    // Applying shifting according to the level and determining the new TMR1H:TMR1L value

    holder_16bit = TMR1H;
    holder_16bit = (holder_16bit << 8) & 0xFF00;
    holder_16bit |= TMR1L;

    switch (curr_level) {

        case LEVEL_1:
            rotate();
            break;
        case LEVEL_2:
            rotate();
            rotate();
            rotate();
            break;
        case LEVEL_3:
            rotate();
            rotate();
            rotate();
            rotate();
            rotate();
            break;

    }

    TMR1L = holder_16bit % 256;
    holder_16bit = holder_16bit >> 8;
    TMR1H = holder_16bit % 256;

}

// This function generates a note on RA0-RA4 if necessary, shifts all notes to right by one and if necessary, lets the game pass to next level or ends the game on win condition.
void shift_notes() {

    LATF = LATE;
    LATE = LATD;
    LATD = LATC;
    LATC = LATB;
    LATB = LATA;

    if (notes_to_be_generated != 0) {
        generate_note();
        notes_to_be_generated--;
    }
    else {
        LATA = 0x00;
    }

    // If there is no notes left to be shifted, then game is transitioned into the next level (or ends if current level is 3)
    if (notes_to_be_generated == 0 && LATE == 0x00 && LATF == 0x00) {

        if (curr_level == LEVEL_1) {
            curr_level = LEVEL_2;
            notes_to_be_generated = 10;
        }
        else if (curr_level == LEVEL_2) {
            curr_level = LEVEL_3;
            notes_to_be_generated = 15;
        }
        else if (curr_level == LEVEL_3) {
            finish_game();
            ssdisplay_state = END;
        }
    }

}

// Game task
void game_task() {

    switch(game_state) {

        case G_INIT:
            // The case where the game is waiting for the RC4 button to be pressed

            TRISC = 0x01;   // Assigning RC0 as an input until it is pressed
            if (PORTCbits.RC0 == 1) {
                if (rc0_waiting_for_release == 0) {
                    rc0_waiting_for_release = 1;
                }
            }

            else {

                if (rc0_waiting_for_release == 1) {
                    rc0_waiting_for_release = 0;
                    TRISC = 0x00;   // Assigning RC0 as an output again for the game
                    health_point = 9;
                    game_state = G_RUN;
                    curr_level = LEVEL_1;
                    ssdisplay_state = IN_GAME;
                    notes_to_be_generated = 5;
                    shift_notes();
                    tmr0_start(76);  // Starting the timer for 500ms delay
                }
            }
            //T1CONbits.T1OSCEN = 1;
            break;

        case G_RUN:
            // The case where the game is running

            if (tmr_state == TMR_IDLE || tmr_state == TMR_RUN) {

                // Checking all pins of PORTG whether if they are pressed or not during the time interval. If pressed, checking if there is a mismatch and decreasing health accordingly or unsetting the RFx pin if the note is correctly caught

                if (PORTGbits.RG0 == 1) {
                    if (rg0_waiting_for_release == 0) {
                        rg0_waiting_for_release = 1;
                    }
                }

                else {
                    if (rg0_waiting_for_release == 1) {
                        rg0_waiting_for_release = 0;
                        rg0_pressed_and_released = 1;
                    }
                }

                if (rg0_pressed_and_released == 1) {
                    if (LATFbits.LF0 == 1) {
                        note_caught = 1;
                        LATFbits.LF0 = 0;
                    }
                    else {
                        if (decrease_health() == 1) return;
                    }
                    rg0_pressed_and_released = 0;
                }
                // ----------------------

                if (PORTGbits.RG1 == 1) {
                    if (rg1_waiting_for_release == 0) {
                        rg1_waiting_for_release = 1;
                    }
                }

                else {
                    if (rg1_waiting_for_release == 1) {
                        rg1_waiting_for_release = 0;
                        rg1_pressed_and_released = 1;
                    }
                }

                if (rg1_pressed_and_released == 1) {
                    if (LATFbits.LF1 == 1) {
                        note_caught = 1;
                        LATFbits.LF1 = 0;
                    }
                    else {
                        if (decrease_health() == 1) return;
                    }
                    rg1_pressed_and_released = 0;
                }
                // ----------------------

                if (PORTGbits.RG2 == 1) {
                    if (rg2_waiting_for_release == 0) {
                        rg2_waiting_for_release = 1;
                    }
                }

                else {
                    if (rg2_waiting_for_release == 1) {
                        rg2_waiting_for_release = 0;
                        rg2_pressed_and_released = 1;
                    }
                }

                if (rg2_pressed_and_released == 1) {
                    if (LATFbits.LF2 == 1) {
                        note_caught = 1;
                        LATFbits.LF2 = 0;
                    }
                    else {
                        if (decrease_health() == 1) return;
                    }
                    rg2_pressed_and_released = 0;
                }
                // ----------------------

                if (PORTGbits.RG3 == 1) {
                    if (rg3_waiting_for_release == 0) {
                        rg3_waiting_for_release = 1;
                    }
                }

                else {
                    if (rg3_waiting_for_release == 1) {
                        rg3_waiting_for_release = 0;
                        rg3_pressed_and_released = 1;
                    }
                }

                if (rg3_pressed_and_released == 1) {
                    if (LATFbits.LF3 == 1) {
                        note_caught = 1;
                        LATFbits.LF3 = 0;
                    }
                    else {
                        if (decrease_health() == 1) return;
                    }
                    rg3_pressed_and_released = 0;
                }
                // ----------------------

                if (PORTGbits.RG4 == 1) {
                    if (rg4_waiting_for_release == 0) {
                        rg4_waiting_for_release = 1;
                    }
                }

                else {
                    if (rg4_waiting_for_release == 1) {
                        rg4_waiting_for_release = 0;
                        rg4_pressed_and_released = 1;
                    }
                }

                if (rg4_pressed_and_released == 1) {
                    if (LATFbits.LF4 == 1) {
                        note_caught = 1;
                        LATFbits.LF4 = 0;
                    }
                    else {
                        if (decrease_health() == 1) return;
                    }
                    rg4_pressed_and_released = 0;
                }
                // ----------------------

            }

            else if (tmr_state == TMR_DONE) {

                // Checking if the note has caught else decreasing health
                if (note_caught == 1) {
                    note_caught = 0;
                }
                else {
                    if (LATF != 0x00) {
                        if (decrease_health() == 1) return;
                    }
                }

                // Shifting the notes, also handling the level transitions
                shift_notes();

                // Starting the next timer delay according to the current level
                //
                if (curr_level == LEVEL_1) {
                    tmr0_start(76);
                }
                else if (curr_level == LEVEL_2) {
                    tmr0_start(61);
                }
                else if (curr_level == LEVEL_3) {
                    tmr0_start(46);
                }

                /*
                 ------------- The math behind these three numbers -------------

                 40 MHz oscillator -> 10 MHz instruction cycle
                 1 / 10 MHz = 0.1 us -> time needed for 1 incrementation in TMR0L without prescaling
                 Prescale 1:256 -> 0.1 x 256 = 25.6 us -> time passes for 1 incrementation in TMR0L
                 TMR0L increments 256 times before the timer interrupt occurs:
                 25.6 x 256 = 6553.6 us = 6.5536 ms -> time passes between two TMR0 interrupts

                 We need the tmr0 interrupt occuring n times to obtain our desired delay. We find n values which provides us with approximate delays:
                 6.5536 x 76 = 498.073 ms ~ 500 ms -> LEVEL_1
                 6.5536 x 61 = 399.769 ms ~ 400 ms -> LEVEL_2
                 6.5536 x 46 = 301.465 ms ~ 300 ms -> LEVEL_3

                 */

            }
            break;

    }

}

// Main routine
int main(void) {

    init_ports();
    init_interrupts();
    init_timers();
    
    while (1) {
        
        timer_task();
        game_task();
        ssdisplay_task();
    
    }

}


