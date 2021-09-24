/*
 * main.c
 *
 * Created: 3/22/2021 8:46:12 PM
 * Author : Keanan
 */

#define F_CPU 16000000UL
#define UBRR_9600 103
#define BAUDRATE 9600
#define BAUD_PRESCALLER ((F_CPU / (BAUDRATE * 16UL))-1)

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include <stdlib.h>

//////////////////////////////////////////////////////////
// function prototypes
void systemInit(void);
void timerOneInit();
void timerThreeInit();
void changeDutyCycle(int dutyIn);
void changeFrequency(int freqIn);
void read_adc(void);
void adc_init(void);
void USART_init(unsigned int ubrr);
void USART_tx_string(char* data);

void uart_putchar(char c, FILE *stream);
char uart_getchar(FILE *stream);

unsigned char USART_receive(void);
unsigned char * getstring(unsigned char *string);


float getTempC(float adc_t);
float getTempF(float adc_t);

//////////////////////////////////////////////////////////
// global variable declaration
FILE uart_output = FDEV_SETUP_STREAM(uart_putchar, NULL, _FDEV_SETUP_WRITE);
FILE uart_input = FDEV_SETUP_STREAM(NULL, uart_getchar, _FDEV_SETUP_READ);

volatile float adc_temp;
char outs[20];
char strin[10];
volatile int dutyCycleIn;

//////////////////////////////////////////////////////////
int main(void)
{
	char input;
	int dutyInput;
	int freqInput;
	float ctemp;
	float ftemp;
	stdout = &uart_output;
	stdin  = &uart_input;

	systemInit();
	adc_init();
	USART_init(BAUD_PRESCALLER);
	USART_tx_string("Connected!\r\n");

	// enable interrupts (for timers)
	sei();

	printf("'h' - help screen \r\n");
	while (1)
	{
		input = getchar();
		printf("You wrote %c\n", input);
		switch (input) {
		// display menu
		case 'h':
			printf("'t' - display temperature in C\r\n'f' - display temperature in F\r\n'o' - turns ON LED at PB5\r\n'd' - duty cycle\r\n'i' - frequency\r\n");
			break;
		// get temperature in Centigrade
		case 't':
			ctemp = getTempC(adc_temp);
			snprintf(outs, sizeof(outs), "C = %3f\r\n", ctemp);
			USART_tx_string(outs);
			break;
		// get temperature in farenheit
		case 'f':
			ftemp = getTempF(adc_temp);
			snprintf(outs, sizeof(outs), "F = %3f\r\n", ftemp);
			USART_tx_string(outs);
			break;
		// turn on LED at PB5
		case'o':
			PORTB &= ~(1 << 5);
			printf("LED on\r\n");
			break;
		// turn off LED at PB5
		case 'O':
			PORTB |= (1 << 5);
			printf("LED OFF\r\n");
			break;
		// set DC% blink at PB3
		case'd':
			printf("Please enter a value between 0 and 100 \r\n");
			// turn on LED
			PORTB &= ~(1 << 3);
			// read input from user
			//scanf("%d",&dutyInput);
			for (int i = 0; i < 2; i++) {	// build string from characters
				input = getchar();
				if (input == '\n') break;
				strin[i] = input;
			}
			dutyInput = atoi(strin);
			printf("You wrote %d\n", dutyInput);

			changeDutyCycle(dutyInput);
			break;
		// send frequency value to terminal, use integer to blink LED PB2
		case 'i':
			printf("Please enter a value between 1 Hz and 7.813kHz\r\n");
			//scanf("%d",&freqInput);
			for (int i = 0; i < 4; i++) {
				input = getchar();
				if (input == '\n') break;
				strin[i] = input;
			}
			freqInput = atoi(strin);
			printf("You wrote %d\n", freqInput);

			changeFrequency(freqInput);

		default:
			printf("Unrecognized command, send 'h' for help\r\n");
			break;
		}
	}
}
//////////////////////////////////////////////////////////
// Initialize ports and registers
void systemInit(void) {
	// set portb2,3,5 to outputs
	DDRB |= (1 << DDB5) | (1 << DDB3) | (1 << DDB2);
	// initialize portb5 OFF
	PORTB |= (1 << 5);
	timerOneInit();
	timerThreeInit();
}


// timer1 initialization to vary the duty cycle
// toggle between OCRA and ISR using output compare interrupt
void timerOneInit() {
	// CTC Mode OCR1A as TOP, 1024 prescaler
	TCCR1A |= (1 << WGM12) | (1 << CS12) | (1 << CS10);
	TCCR1B &= ~(1 << WGM13);	// ensure OCR1A as TOP
	TIMSK1 |= (1 << OCIE1A);	// set interrupt on compare match
	TCNT1 = 0;				// initialize counter
	// initial ON F = 20 Hz, T = 50ms, 50% DC
	OCR1A = 390;
	ICR1 = 390;

}

void changeDutyCycle(int dutyIn) {
	int newDc = 1 - (dutyIn * .10);
	ICR1 = newDc;
	printf("Duty cycle changed\r\n");
}

// clear on compare match A
ISR(TIMER1_COMPA_vect) {
	// turn led off for duration
	PORTB |= (1 << 3);
	// set ICR1 as TOP aka change to mode 12
	TCCR1B |= (1 << WGM13);
	// tcnt is reset and restart timer
	TCNT1 = 0;
	// wait until the timer overflow flag is set
	while ((TIFR1 & (1 << OCF1A)) == 0);
	// turn LED on
	PORTB &= ~(1 << 3);
	// TOP value = OCRA1, go back to mode 2
	TCCR1B &= ~(1 << WGM13);
}

void timerThreeInit() {
	// CTC Mode OCR3A as TOP, 1024 prescaler
	TCCR3B |= (1 << WGM32) | (1 << CS32) | (1 << CS30);
	// set interrupt on compare match
	TIMSK3 |= (1 << OCIE3A);
	// initialize counter
	TCNT3 = 0;
	// initial ON F = .119 Hz, T = 8s, 50% DC
	OCR3A = 65530;
}

void changeFrequency(int freqIn) {
	OCR3A = (16000000 / (2 * 1024 * freqIn));
	printf("OCR3A=%d\n", freqIn);
	printf("Frequency changed\r\n");
}

ISR(TIMER3_COMPA_vect) {
	// Toggle PB2 LED
	PORTB ^= (1 << 2);
}
//////////////////////////////////////////////////////////
// Read from ADC
void read_adc(void) {
	unsigned char i = 4;
	adc_temp = 0;
	while (i--) {
		ADCSRA |= (1 << ADSC);
		while (ADCSRA & (1 << ADSC));
		adc_temp += ADC;
	}
	adc_temp = adc_temp / 4;
}

// Initialize ADC
void adc_init(void) {
	ADMUX = (1 << REFS0) | (0 << MUX1) | (1 << MUX0);
	ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS0);
}
//////////////////////////////////////////////////////////
// Initialize UART module
void USART_init(unsigned int ubrr) {
	UBRR0H = (unsigned char)(ubrr >> 8);
	UBRR0L = (unsigned char)ubrr;
	UCSR0B = _BV(RXEN0) | _BV(TXEN0);		// enable tx and rx
	UCSR0C = _BV(UCSZ01) | _BV(UCSZ00);		// 8 data bits
}

// send a string to uart
void USART_tx_string(char* data) {
	while ((*data != '\0')) {
		while (!(UCSR0A & (1 << UDRE0)));
		UDR0 = *data;
		data++;
	}
}


void uart_putchar(char c, FILE *stream) {
	if (c == '\n') {
		//uart_putchar('r',stream);
	}
	loop_until_bit_is_set(UCSR0A, UDRE0);
	UDR0 = c;
}

char uart_getchar(FILE *stream) {
	loop_until_bit_is_set(UCSR0A, RXC0);
	return UDR0;
}

//////////////////////////////////////////////////////////
// temperature conversion functions
float getTempC(float adc_t) {
	float ctemp = 0;

	read_adc();
	ctemp = adc_t;
	ctemp = (ctemp / 1023.0) * 5;
	ctemp /= 0.01;
	return ctemp;

}

float getTempF(float adc_t) {
	float ctemp = 0;
	float ftemp = 0;
	ctemp = getTempC(adc_t);
	ftemp = (ctemp * 1.8) + 32;
	return ftemp;

}