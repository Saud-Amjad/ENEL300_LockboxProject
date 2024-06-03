#include <avr/io.h>
#include <util/delay.h>
#include <stdbool.h>

#define DEBOUNCE_TIME_MS 1000
#define MAX_CODE_LENGTH 5
#define FLASH_LONG_DURATION_MS 2000
#define FLASH_SHORT_DURATION_MS 250
#define SECRET_CODE_LENGTH 5

uint8_t secretCode[MAX_CODE_LENGTH] = {2, 2, 2, 2, 2};
uint8_t enteredCode[MAX_CODE_LENGTH];
uint8_t codeIndex = 0;
bool resetButtonActive = false;
bool settingNewCode = true;

void initIO(void) {
    PORTA.DIRCLR = 0x0E; // PA1-PA3 as input
    PORTA.DIRSET = 0xF1; // PA5-PA7 as output for LEDs
    PORTA.OUTSET = 0xE0; // Turn off LEDs

    PORTC.DIRCLR = 0x05; // PC0 and PC2 as input
    PORTC.DIRSET = 0x02; // PC1 as output for LED
    PORTC.OUTSET = 0x02; // Turn off LED PC1

    // Enable pull-up resistors
    PORTA.PIN1CTRL = PORT_PULLUPEN_bm;
    PORTA.PIN2CTRL = PORT_PULLUPEN_bm;
    PORTA.PIN3CTRL = PORT_PULLUPEN_bm;
    PORTC.PIN0CTRL = PORT_PULLUPEN_bm;
    PORTC.PIN2CTRL = PORT_PULLUPEN_bm;
}

bool readButton(uint8_t pin) {
    if (pin >= 1 && pin <= 3) return !(PORTA.IN & (1 << pin));
    if (pin == 4) return !(PORTC.IN & (1 << 0));
    if (pin == 5) return !(PORTC.IN & (1 << 2));
    return false;
}

void toggleLED(uint8_t pin) {
    if (pin >= 1 && pin <= 3) PORTA.OUTTGL = (1 << (pin + 4)); // Adjust for PA5-PA7
    if (pin == 4) PORTC.OUTTGL = (1 << 1); // PC1 for the fourth button
}

void debounce(void) {
    _delay_ms(DEBOUNCE_TIME_MS);
}



void variable_delay_ms(uint16_t milliseconds) {
    while(milliseconds > 0) {
        _delay_ms(1); // _delay_ms can only take a compile-time constant.
        milliseconds--;
    }
}


void flashLEDs(uint16_t duration, uint8_t count, bool success) {
    while (count--) {
        if (success) {
            // Success: long flash
            PORTA.OUTCLR = 0xE0; // Turn on LEDs
            PORTC.OUTCLR = (1 << 1);
            variable_delay_ms(duration);
            
        } else {
            // Failure: short flashes
            PORTA.OUTTGL = 0xE0; // Toggle LEDs
            PORTC.OUTTGL = (1 << 1);
            variable_delay_ms(duration);
            PORTA.OUTTGL = 0xE0;
            PORTC.OUTTGL = (1 << 1);
            
        }
        PORTA.OUTSET = 0xE0; // Ensure LEDs are off
        PORTC.OUTSET = (1 << 1);
        if (!success) variable_delay_ms(duration); // Extra delay for failure
    }
}

bool compareCode(uint8_t *code1, uint8_t *code2, uint8_t length) {
    for (uint8_t i = 0; i < length; i++) {
        if (code1[i] != code2[i]) return false;
    }
    return true;
}

void copy_array(void *dest, const void *src, size_t n) {
    // Cast the void pointers to char pointers to handle byte by byte copying
    char *d = (char*)dest;
    const char *s = (const char*)src;

    // Copy each byte from source to destination
    for (size_t i = 0; i < n; ++i) {
        d[i] = s[i];
    }
}

void *my_memset(void *s, int c, size_t n) {
    unsigned char *p = s;
    while (n--) {
        *p++ = (unsigned char)c;
    }
    return s;
}

void rotate(bool success) {
    PORTA.OUTSET &= 0b00000000;
    if (success) {
        PORTA.OUTSET |= 0b00010000;
        _delay_ms(2000);
        
        
        
    }
    else {
        PORTA.OUTSET |= 0b00000001;
        _delay_ms(2000);
        
        
    }
    //PORTA.OUTSET |= 0b11111111;
}




void processButtons(void) {
    static bool waitForNewCode = false;

    // Check for button presses to enter or verify code
    if (!waitForNewCode && !resetButtonActive) {
        for (uint8_t pin = 1; pin < 5; ++pin) {
            if (readButton(pin)) {
                debounce();
                enteredCode[codeIndex] = (pin < 4) ? pin + 1 : 5; // Assign the correct value based on the pressed button
                toggleLED(pin); // Toggle the corresponding LED
                codeIndex++;

                if (codeIndex == MAX_CODE_LENGTH) {
                    if (compareCode(enteredCode, secretCode, MAX_CODE_LENGTH)) {
                        flashLEDs(FLASH_LONG_DURATION_MS, 1, true); // Correct code entered
                        //rotate(true);
                        resetButtonActive = true; // Allow the reset functionality
                    } else {
                        flashLEDs(FLASH_SHORT_DURATION_MS, 3, false); // Incorrect code entered
                        rotate(false);
                    }
                    codeIndex = 0; // Reset for next entry
                    my_memset(enteredCode, 0, MAX_CODE_LENGTH); // Clear the entered code
                }
                break; // Process one button at a time
            }
        }
    }

    // Reset button logic, now separated for clarity
    if (readButton(5) && resetButtonActive) {
        debounce();
        flashLEDs(FLASH_SHORT_DURATION_MS, 3, false); // Indicate reset action
        my_memset(secretCode, 0, MAX_CODE_LENGTH); // Clear the secret code
        codeIndex = 0; // Reset index for new code entry
        resetButtonActive = false; // Disable reset until next correct code entry
        waitForNewCode = true; // Enable new code entry
        return; // Skip further processing in this cycle
    }

    // New code entry after reset
    if (waitForNewCode) {
        for (uint8_t pin = 1; pin < 5; ++pin) {
            if (readButton(pin)) {
                debounce();
                enteredCode[codeIndex] = (pin < 4) ? pin + 1 : 5; // Store the button pressed as part of the code
                toggleLED(pin); // Toggle corresponding LED
                codeIndex++; // Increment codeIndex after processing
                
                if (codeIndex == MAX_CODE_LENGTH) {
                    // New code is fully entered
                    copy_array(secretCode, enteredCode, MAX_CODE_LENGTH); // Copy entered code as new secret code
                    flashLEDs(FLASH_LONG_DURATION_MS, 1, true); // Indicate successful new code setup
                    waitForNewCode = false; // Exit new code entry mode
                    codeIndex = 0; // Reset for future operations
                    my_memset(enteredCode, 0, MAX_CODE_LENGTH); // Clear entered code for security
                }
                break; // Process one button at a time
            }
        }
    }
}




int main(void) {
    initIO();
    while (1) {
        processButtons();
    }
} 