#include <xc.h>
#include <stdint.h>
#include <stdio.h>
#include "lcd.h"

typedef enum {TEM, CDM, TSM} state_s;
state_s global_state = TEM; // Initial state of the game

// 16-bit adc_value for storing the ADRESH:ADRESL
uint16_t adc_value;

// Interrupt flags
uint8_t adc_interrupt_recieved = 0, tmr0_interrupt_recieved = 0;

// Variables for storing the current DDRAM and CGRAM addresses
uint8_t curr_ddram_address = 0, curr_cgram_address = 0;

// Initialized as -1 (not a value in [0:15]) to ensure that the current DDRAM address is properly calculated
// by adc_isr() at the beginning of TEM (both at the beginning of the game and at switches from CDM)
uint8_t text_iterator = -1;

// Flags that are used to prevent a button firing repeatedly during a push.
uint8_t re0_pressed = 0, re1_pressed = 0, re2_pressed = 0, re3_pressed = 0, re4_pressed = 0, re5_pressed = 0;

// Global variables that are used for iteration in PREDEFINED[] and custom_char_codes[] arrays.
uint8_t predefined_chars_iterator = 0, custom_chars_iterator = 0;

// The array that are used for keeping the created custom chars starts with a space character, so its size is initialized to 1.
uint8_t custom_chars_size = 1;

uint8_t cdm_cursor_x = 0, cdm_cursor_y = 0;
uint8_t ssdisplay_porth_selection = 0;
uint8_t cursor_temp, dot_matrix_temp;
uint8_t portd_temp, portb_temp;

// The variable for determining the current index of the text[] array to be shown on the first cell of the second line during TSM at every 500ms.
uint8_t  scroll_count = 0;

/*
 ------------- The math behind 76 -------------

 40 MHz oscillator -> 10 MHz instruction cycle
 1 / 10 MHz = 0.1 us -> time needed for 1 incrementation in TMR0L without prescaling
 Prescale 1:256 -> 0.1 x 256 = 25.6 us -> time passes for 1 incrementation in TMR0L
 TMR0L increments 256 times before the timer interrupt occurs:
 25.6 x 256 = 6553.6 us = 6.5536 ms -> time passes between two TMR0 interrupts

 We need the tmr0 interrupt occuring n times to obtain our desired delay (which 500ms between each scroll to left during TSM). The best approximation is below:
 6.5536 x 76 = 498.073 ms ~ 500 ms
 */
uint8_t cycles_left_for_scroll = 76;


// High priority interrupt
void __interrupt(high_priority) highPriorityISR(void) {
    
    // Timer0 interrupt sets TMR0IF
    if (INTCONbits.TMR0IF) {
        INTCONbits.TMR0IF = 0;
        tmr0_interrupt_recieved = 1;
    }
    
    // AD interrupt sets ADIF
    if (PIR1bits.ADIF) {
        PIR1bits.ADIF = 0;
        adc_interrupt_recieved = 1;
    }
    
}

// Low priority interrupt, no usage
void __interrupt(low_priority) lowPriorityISR(void) {}


// Port initializations
void init_ports() {
    
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

    TRISA = 0b11111111; // PORTA is all set as analog inputs
    TRISB = 0b00000000; // LCD enable bit and LCD RS set as outputs
    TRISC = 0b00000000;
    TRISD = 0b00000000; // LCD data bus set as output
    TRISE = 0b00111111; // Game related buttons are set as inputs
    TRISF = 0b00000000;
    TRISG = 0b00000000;
    TRISJ = 0b00000000; // Seven segment display is set as output
    TRISH = 0b00010000; // Potentiometer (AN12 - PORTH[4]) set as input, PORTH[0:3] is set as output (7segment display)
    
}


// Timer initializations
void init_timer() {
    T0CON = 0b11000111; // 8-bit timer, internal clock with 1:256 prescaler, TMR0ON = 1
    TMR0L = 0x00;
}


// ADC initializations
void init_adc() {
    ADCON0 = 0b00110001; // Potentiometer channel (AN12) selected, GO_DONE = 0, ADON = 1
    ADCON1 = 0b00001110; // Voltage references selected as AVdd-AVss, port configurations selected as all analog inputs
    ADCON2 = 0b10101010; // ADFM is right justified, acquisition time selected as 12 Tad, conversion clock selected as Fosc/32
}


// Interrupt initializations
void init_interrupts() {
    RCONbits.IPEN = 0; // Low priority interrupts are disabled
    INTCONbits.PEIE = 1; // Peripheral interrupts are enabled
    PIE1bits.ADIE = 1; // AD interrupt is enabled
    INTCONbits.TMR0IE = 1; // Timer0 interrupt is enabled
    INTCONbits.GIE = 1; // All unmasked interrupts are enabled
}

// Function used for scrolling the text left in the second line of the LCD during TSM. It is called by tmr0_isr() every 500 ms.
void scroll_left() {
    
    cycles_left_for_scroll = 76; // The cycle_count is reset back to 76 for the next 500ms delay
    
    // scroll_count increments (and gets back to 0 after 15) at every 500ms and determines the first char's
    // index to be written in the first cell of the second line during the current 500ms period
    if (scroll_count == 15)
        scroll_count = 0;
    else
        scroll_count++;
    
    uint8_t i = scroll_count;
    uint8_t add = 0xC0;
    while (add < 0xD0) {
        
        // the address of the current address of the cell in the second line is sent to the LCD.
        lcd_cmd(add);
        
        // The data stored in the text[] is written according to the scroll_left (e.g. if scroll_left = 7 at the moment, then the data
        // written in the second line of the LCD is: (text[7], text[8], text[9] ... text[15], text[0], text[1] ... text[5], text[6])
        lcd_data(text[i]);
        
        // PORTB and PORTD is unset for preventing the MCU from lighting the LEDs.
        PORTB = 0x00;
        PORTD = 0x00;
        add++;
        if (i == 15) i = 0;
        else i++;
        
    }
    
}

// Timer0 ISR, has 3 usages
void tmr0_isr() {
    
    // Interrupt received, 6.5536 ms has passed, the flag is unset for preventing further calls of tmr0_isr().
    tmr0_interrupt_recieved = 0;
    
    // A read is made at every 6.5536 ms on AN12 (potentiometer).
    ADCON0bits.GO_DONE = 1;
    
    // The cell that is being displayed is changed at every 6.5536 ms on the 7segment display.
    if (ssdisplay_porth_selection == 3)
        ssdisplay_porth_selection = 0;
    else
        ssdisplay_porth_selection++;
    
    // If the game is in the TSM state, the scroll_left function is called at every 500ms (cycles_left_for_scroll is initialized as 76).
    if (global_state == TSM) {
        if (--cycles_left_for_scroll == 0)  // If no counts left, then the desired delay is obtained.
            scroll_left();
    }
    
}


// A/D conversion ISR
void adc_isr() {
    
    // Interrupt received, A/D conversion is complete, GO_DONE bit is unset automatically, the flag is unset for preventing further calls of adc_isr().
    adc_interrupt_recieved = 0;
    
    switch (global_state) {
        
        // If the game is in the TEM state, the cursor should be on the cell which the potentiometer is pointing.
        case TEM:
            
            // ADRESH:ADRESL value is kept on the 16-bit long adc_value variable.
            adc_value = ADRESH;
            adc_value = adc_value << 8;
            adc_value = adc_value + ADRESL;
            
            // adc_value gets a value between 0-1023 and it is divided into 16 equal parts, each of them is of size 64
            if (0 <= adc_value && adc_value < 64) {
                
                // this condition ensures that the code below is run only once when the potentiometer is rotated, not every 6.5536 seconds.
                if (text_iterator != 0) {
                    
                    // the related ddram_address is sent to the lcd to make the cursor point to the correct cell.
                    lcd_cmd(0x80);
                    
                    // current ddram address is saved into a global variable for further use.
                    curr_ddram_address = 0x80;
                    
                    // text_iterator global variable saves the current selected cell's index. It is used while saving the current text into the text[] array.
                    text_iterator = 0;
                    
                    // Predefined and custom chars each have their own gloabal iterators which point to PREDEFINED[] and custom_char_codes[] arrays consecutively.
                    // When the cursor is set to a new cell position by rotating the potentiometer, these variables are assigned to zero to make sure that the new
                    // cell starts with 0th indices in the arrays.
                    predefined_chars_iterator = 0;
                    custom_chars_iterator = 0;
                    
                }
                
            }
            
            else if (64 <= adc_value && adc_value < 128) {
                if (text_iterator != 1) {
                    lcd_cmd(0x81); curr_ddram_address = 0x81; text_iterator = 1; predefined_chars_iterator = 0; custom_chars_iterator = 0;
                }
            }
            else if (128 <= adc_value && adc_value < 192) {
                if (text_iterator != 2) {
                    lcd_cmd(0x82); curr_ddram_address = 0x82; text_iterator = 2; predefined_chars_iterator = 0; custom_chars_iterator = 0;
                }
            }
            else if (192 <= adc_value && adc_value < 256) {
                if (text_iterator != 3) {
                    lcd_cmd(0x83); curr_ddram_address = 0x83; text_iterator = 3; predefined_chars_iterator = 0; custom_chars_iterator = 0;
                }
            }
            else if (256 <= adc_value && adc_value < 320) {
                if (text_iterator != 4) {
                    lcd_cmd(0x84); curr_ddram_address = 0x84; text_iterator = 4; predefined_chars_iterator = 0; custom_chars_iterator = 0;
                }
            }
            else if (320 <= adc_value && adc_value < 384) {
                if (text_iterator != 5) {
                    lcd_cmd(0x85); curr_ddram_address = 0x85; text_iterator = 5; predefined_chars_iterator = 0; custom_chars_iterator = 0;
                }
            }
            else if (384 <= adc_value && adc_value < 448) {
                if (text_iterator != 6) {
                    lcd_cmd(0x86); curr_ddram_address = 0x86; text_iterator = 6; predefined_chars_iterator = 0; custom_chars_iterator = 0;
                }
            }
            else if (448 <= adc_value && adc_value < 512) {
                if (text_iterator != 7) {
                    lcd_cmd(0x87); curr_ddram_address = 0x87; text_iterator = 7;  predefined_chars_iterator = 0; custom_chars_iterator = 0;
                }
            }
            else if (512 <= adc_value && adc_value < 576) {
                if (text_iterator != 8) {
                    lcd_cmd(0x88); curr_ddram_address = 0x88; text_iterator = 8; predefined_chars_iterator = 0; custom_chars_iterator = 0;
                }
            }
            else if (576 <= adc_value && adc_value < 640) {
                if (text_iterator != 9) {
                    lcd_cmd(0x89); curr_ddram_address = 0x89; text_iterator = 9;  predefined_chars_iterator = 0; custom_chars_iterator = 0;
                }
            }
            else if (640 <= adc_value && adc_value < 704) {
                if (text_iterator != 10) {
                    lcd_cmd(0x8A); curr_ddram_address = 0x8A; text_iterator = 10; predefined_chars_iterator = 0; custom_chars_iterator = 0;
                }
            }
            else if (704 <= adc_value && adc_value < 768) {
                if (text_iterator != 11) {
                    lcd_cmd(0x8B); curr_ddram_address = 0x8B; text_iterator = 11; predefined_chars_iterator = 0; custom_chars_iterator = 0;
                }
            }
            else if (768 <= adc_value && adc_value < 832) {
                if (text_iterator != 12) {
                    lcd_cmd(0x8C); curr_ddram_address = 0x8C; text_iterator = 12; predefined_chars_iterator = 0; custom_chars_iterator = 0;
                }
            }
            else if (832 <= adc_value && adc_value < 896) {
                if (text_iterator != 13) {
                    lcd_cmd(0x8D); curr_ddram_address = 0x8D; text_iterator = 13; predefined_chars_iterator = 0; custom_chars_iterator = 0;
                }
            }
            else if (896 <= adc_value && adc_value < 960) {
                if (text_iterator != 14) {
                    lcd_cmd(0x8E); curr_ddram_address = 0x8E; text_iterator = 14; predefined_chars_iterator = 0; custom_chars_iterator = 0;
                }
            }
            else if (960 <= adc_value && adc_value < 1024) {
                if (text_iterator != 15) {
                    lcd_cmd(0x8F); curr_ddram_address = 0x8F; text_iterator = 15; predefined_chars_iterator = 0; custom_chars_iterator = 0;
                }
            }
            
            break;
            
        case CDM:
            
            // When the game is swithced to CDM state, the DDRAM address is set to point to the first cell of the first line to demonstrate the current situation
            // of the custom char creation.
            if (curr_ddram_address != 0x80) {
                lcd_cmd(0x80);
                curr_ddram_address = 0x80;
            }
            
            break;
            
            // In TSM mode, the process is run only by timer0 interrupts. So there is no need of task here.
        case TSM:
            break;
        
    }
    
}

// The function is called when RE5 button is pressed during CDM.
void switch_to_tem() {
    
    global_state = TEM;
    
    // Dot matrix cursors are unset.
    cdm_cursor_x = 0;
    cdm_cursor_y = 0;
    
    // Turning of the LEDs.
    PORTA = 0x00;
    PORTB &= 0b00100100; // Not unsetting PORTB.RB2 and PORTB.RB5 to prevent a false communication between the PIC and the LCD.
    PORTC = 0x00;
    PORTD = 0x00;
    
    // Incrementing the custom char size varaible after saving the created char in the previous CDM state.
    custom_chars_size++;
    
    // PORTA is all set as analog inputs again.
    TRISA = 0b11111111;
    
    // The saved text data is sent back to the first line addresses.
    uint8_t i = 0;
    for (uint8_t add = 0x80; add < 0x90; add++) {
        lcd_cmd(add);
        lcd_data(text[i]);
        i++;
    }
    
    // Assigned to -1 again (not a value in [0:15]) to ensure that the current DDRAM address is properly calculated
    // by adc_isr() at the beginning of TEM
    text_iterator = -1;
    
}

// The function is called when RE4 button is pressed during TEM.
void switch_to_cdm() {
    
    global_state = CDM;
    
    // The charmap is fully assigned back to 0 to clear the previously created custom chars (if this is not the first switch to CDM).
    for (int i = 0; i < 8; i++)
        charmap[i] = 0b00000;
    
    // All ports are unset.
    PORTA = 0x00;
    PORTB = 0x00;
    PORTC = 0x00;
    PORTD = 0x00;
    lcd_cmd(0x01);  // Clear display
    
    // RA[0:7] LEDs set as outputs for the dot matrix.
    TRISA = 0b00000000;
    
}

// The function is called when RE5 button is pressed during TEM.
void switch_to_tsm() {
    
    global_state = TSM;
    
    // All ports are unset.
    PORTA = 0x00;
    PORTB = 0x00;
    PORTC = 0x00;
    PORTD = 0x00;
    
    // The text 'finished' is written on the first line.
    lcd_cmd(0x80);
    lcd_data(' ');
    lcd_data(' ');
    lcd_data(' ');
    lcd_data('f');
    lcd_data('i');
    lcd_data('n');
    lcd_data('i');
    lcd_data('s');
    lcd_data('h');
    lcd_data('e');
    lcd_data('d');
    lcd_data(' ');
    lcd_data(' ');
    lcd_data(' ');
    lcd_data(' ');
    lcd_data(' ');
    
    // The data on the text is written correctly on the second line at t=0, it is to be changed at each call of scroll_left() every 500ms.
    uint8_t i = 0;
    for (uint8_t add = 0xC0; add < 0xD0; add++, i++) {
        
        // the address of the current address of the cell in the second line is sent to the LCD.
        lcd_cmd(add);
        
        // The data stored in the text[] is written according to the scroll_left (e.g. if scroll_left = 7 at the moment, then the data
        // written in the second line of the LCD is: (text[7], text[8], text[9] ... text[15], text[0], text[1] ... text[5], text[6])
        lcd_data(text[i]);
        
        // PORTB and PORTD is unset for preventing the MCU from lighting the LEDs.
        PORTB = 0x00;
        PORTD = 0x00;
        
    }
    
    
}

// State changes by pressing RE4 and RE5
void state_task() {
    
    if (PORTEbits.RE5 == 1 && re5_pressed == 0) {
        re5_pressed = 1;
        if (global_state == TEM)
            switch_to_tsm();
        
        else if (global_state == CDM)
            switch_to_tem();
    }

    else if (PORTEbits.RE5 == 0 && re5_pressed == 1) {
        re5_pressed = 0;
    }

    if (PORTEbits.RE4 == 1 && re4_pressed == 0) {
        re4_pressed = 1;
        if (global_state == TEM)
            switch_to_cdm();
    }

    else if (PORTEbits.RE4 == 0 && re4_pressed == 1) {
        re4_pressed = 0;
    }
    
}

// Seven segment display task
void ssdisplay_task() {
    
    // ssdisplay_porth_selection is moved between [0:3] at every timer0 interrupt which is 6.5536 ms
    switch (ssdisplay_porth_selection) {
        
        case 0:
            LATH = 0b00000001;
            LATJ = SSEGMENT_NUMBERS_GLYPHS[custom_chars_size - 1]; // Displays the total number of custom chars created.
            break;
            
        case 1:
            LATH = 0b00000010;
            LATJ = 0b00000000; // Displays nothing.
            break;
            
        case 2:
            LATH = 0b00000100;
            LATJ = SSEGMENT_NUMBERS_GLYPHS[cdm_cursor_x]; // Displays the current CDM cursor's x coordinate.
            break;
            
        case 3:
            LATH = 0b00001000;
            LATJ = SSEGMENT_NUMBERS_GLYPHS[cdm_cursor_y]; // Displays the current CDM cursor's x coordinate.
            break;
            
    }
    
}

// LCD task
void lcd_task() {
    
    switch (global_state) {
        
        // In TEM, the code waits for inputs RE[0:5]. 4 of them are LCD related.
        case TEM:
            
            // Scroll forward in custom chars
            if (PORTEbits.RE0 == 1 && re0_pressed == 0) {
                re0_pressed = 1;
                
                // custom_chars_iterator is moved between [0:custom_chars_size - 1].
                if (custom_chars_iterator == custom_chars_size - 1) custom_chars_iterator = 0;
                else custom_chars_iterator++;
                
                // The selected custom char is sent to the current DDRAM address chosen by the adc_isr() (potentiometer).
                lcd_data(custom_char_codes[custom_chars_iterator]);
                
                // The DDRAM address automatically increments by an internal operation of the LCD after a data write, so this command sets the DDRAM address back to
                // the currently selected cell.
                lcd_cmd(curr_ddram_address);
                
                // The displayed custom char on the current cell is also saved on the text[] array's text_iterator'th position which is determined by the adc_isr().
                text[text_iterator] = custom_char_codes[custom_chars_iterator];
                
            }

            else if (PORTEbits.RE0 == 0 && re0_pressed == 1) {
                re0_pressed = 0;
            }

            // Scroll backward in predefined chars
            if (PORTEbits.RE1 == 1 && re1_pressed == 0) {
                re1_pressed = 1;
                
                // predefined_chars_iterator is moved between [0:custom_chars_size - 1].
                if (predefined_chars_iterator == 0) predefined_chars_iterator = 36;
                else predefined_chars_iterator--;
                
                // The selected predefined char is sent to the current DDRAM address chosen by the adc_isr() (potentiometer).
                lcd_data(PREDEFINED[predefined_chars_iterator]);
                
                // The DDRAM address automatically increments by an internal operation of the LCD after a data write, so this command sets the DDRAM address back to
                // the currently selected cell.
                lcd_cmd(curr_ddram_address);
                
                // The displayed predefined char on the current cell is also saved on the text[] array's text_iterator'th position which is determined by the adc_isr().
                text[text_iterator] = PREDEFINED[predefined_chars_iterator];

            }

            else if (PORTEbits.RE1 == 0 && re1_pressed == 1) {
                re1_pressed = 0;
            }

            // Scroll forward in predefined chars
            if (PORTEbits.RE2 == 1 && re2_pressed == 0) {
                re2_pressed = 1;
                if (predefined_chars_iterator == 36) predefined_chars_iterator = 0;
                else predefined_chars_iterator++;
                lcd_data(PREDEFINED[predefined_chars_iterator]);
                lcd_cmd(curr_ddram_address);
                text[text_iterator] = PREDEFINED[predefined_chars_iterator];
            }

            else if (PORTEbits.RE2 == 0 && re2_pressed == 1) {
                re2_pressed = 0;
            }

            // Scroll backward in custom chars
            if (PORTEbits.RE3 == 1 && re3_pressed == 0) {
                re3_pressed = 1;
                if (custom_chars_iterator == 0) custom_chars_iterator = custom_chars_size - 1;
                else custom_chars_iterator--;
                lcd_data(custom_char_codes[custom_chars_iterator]);
                lcd_cmd(curr_ddram_address);
                text[text_iterator] = custom_char_codes[custom_chars_iterator];
            }

            else if (PORTEbits.RE3 == 0 && re3_pressed == 1) {
                re3_pressed = 0;
            }
            
            break;
        
        // In CDM, the code waits for inputs RE[0:5]. 4 of them are LCD related.
        case CDM:
            
            // Move cursor right
            if (PORTEbits.RE0 == 1 && re0_pressed == 0) {
                re0_pressed = 1;
                if (cdm_cursor_x != 3) cdm_cursor_x++;
            }

            else if (PORTEbits.RE0 == 0 && re0_pressed == 1) {
                re0_pressed = 0;
            }

            // Move cursor down
            if (PORTEbits.RE1 == 1 && re1_pressed == 0) {
                re1_pressed = 1;
                if (cdm_cursor_y != 7) cdm_cursor_y++;
            }

            else if (PORTEbits.RE1 == 0 && re1_pressed == 1) {
                re1_pressed = 0;
            }

            // Move cursor up
            if (PORTEbits.RE2 == 1 && re2_pressed == 0) {
                re2_pressed = 1;
                if (cdm_cursor_y != 0) cdm_cursor_y--;
            }

            else if (PORTEbits.RE2 == 0 && re2_pressed == 1) {
                re2_pressed = 0;
            }

            // Move cursor left
            if (PORTEbits.RE3 == 1 && re3_pressed == 0) {
                re3_pressed = 1;
                if (cdm_cursor_x != 0) cdm_cursor_x--;
            }

            else if (PORTEbits.RE3 == 0 && re3_pressed == 1) {
                re3_pressed = 0;
            }
            
            // Toggle LED state
            if (PORTEbits.RE4 == 1 && re4_pressed == 0) {
                re4_pressed = 1;
                
                // Two temporary variables which are initialized as 1 are shifted accordingly to determine the position of the LED to be toggled
                // in the dot matrix R[A:D][0:7]
                cursor_temp = 0x01;
                cursor_temp = cursor_temp << cdm_cursor_y;
                dot_matrix_temp = 0b10000;
                dot_matrix_temp = dot_matrix_temp >> cdm_cursor_x;
            
                // The chosen PORT is XOR'ed with the cursor_temp to toggle the correct LED.
                switch (cdm_cursor_x) {
                    case 0:
                        LATA = LATA ^ cursor_temp;
                        break;

                    case 1:
                        LATB = LATB ^ cursor_temp;
                        break;

                    case 2:
                        LATC = LATC ^ cursor_temp;
                        break;

                    case 3:
                        LATD = LATD ^ cursor_temp;
                        break;
                }
                
                // This is the charmap used during the CDM. at every press of R[A:D][0:7], this charmap is updated accordingly and sent to the LCD as the data to be stored into the current CGROM address and then sent to the first DDRAM address for demonstrating the current situation of the ongoing custom char creation at the first cell of the first line of the LCD.
                // The charmap[] array is updated by using the dot_matrix_temp variable.
                charmap[cdm_cursor_y] = charmap[cdm_cursor_y] ^ dot_matrix_temp;
                
                // Since PORTB and PORTD are going to be used for communicating with the LCD, the current sitatuion of the dot matrix's
                // PORTB and PORTD columns are saved in two temporary variables.
                portd_temp = PORTD;
                portb_temp = PORTB;
                
                // The next CGRAM address into which the currently being created custom char's data is going to be written is selected by adding
                // (number_of_previously_created_custom_chars * 8) to 0x40, which is the first CGRAM address.
                curr_cgram_address = 0x40 + (custom_chars_size - 1) * 8;
                lcd_cmd(curr_cgram_address);
                
                // After sending the selected CGRAM address with lcd_cmd, the current charmap is written into that address.
                for(int i=0; i<8; i++){
                    lcd_data(charmap[i]);
                }
                
                lcd_cmd(0x02); // Return home
                lcd_cmd(0x80); // First cell of the first line's address is sent with lcd_cmd for simultaneous demonstration with LED dot matrix.
                lcd_data(custom_chars_size - 1); // CGRAM char codes change between [0:8], so the data is sent into DDRAM as custom_chars_size - 1.
                lcd_cmd(0x80); // Automatic incrementation is prevented.
                
                // PORTB and PORTD are assigned back to their state before lcd_cmd and lcd_data calls.
                PORTD = portd_temp;
                PORTB = portb_temp;
                
            }

            else if (PORTEbits.RE4 == 0 && re4_pressed == 1) {
                re4_pressed = 0;
            }
            
            break;
            
        // In TSM mode, the process is run only by timer0 interrupts. So there is no need of task here.
        case TSM:
            break;
    }
    
}

void main(void) {
    
    // Initializing INTCON and RCON as 0 to prevent any interrupts from occurring during initializations
    INTCON = 0x00;
    RCON = 0x00;
    
    init_ports();
    init_timer();
    init_adc();
    init_lcd();
    init_interrupts();
    
    while (1) {
        
        if (tmr0_interrupt_recieved == 1)
            tmr0_isr();
        
        if (adc_interrupt_recieved == 1)
            adc_isr();
        
        state_task();
        ssdisplay_task();
        lcd_task();
        
    }
    
}
