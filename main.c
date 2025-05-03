#include <avr/io.h>
#include <avr/interrupt.h>

#define F_CPU 16000000UL
#define delay 2

// Zmienne
volatile int16_t encoderValue = 0;
volatile uint8_t lastStateCLK;
volatile uint8_t digits[4];
volatile uint8_t position = 0;
volatile uint8_t currentDigit = 0;
volatile uint8_t step = 0; // krok
volatile uint8_t state = 0; // Stany : 0 - wpisywanie kodu, 1 - odblokowany, 2 - zablokowany, 3 - zmiana has³a
volatile int number[6]; // Number - wpisywany
volatile int code[6]; // Code - has³o
volatile int8_t lastencoderValue = 0;
volatile int8_t direction = 1; // 1 - prawo, te¿ domyœlnie
volatile int8_t lastdirection = 1;

void setup(void) { 
	DDRC = 0x00; // WEJ
	PORTC = 0xff; // PULL UP RES
	DDRB = 0xff; // WYJ
	DDRD = 0xff; // WYJ
	PORTD = 0x00; // WARTOŒÆ WYJŒCIA
	
	// Stan pocz¹tkowy
	lastStateCLK = PINC & (1 << 0); 
	
	// Konfiguracja przerwañ dla pinów C
	PCICR |= (1 << PCIE1);
	PCMSK1 |= (1 << PCINT8) | (1 << PCINT10);
	
	// Konfiguracja timera 2
	TCCR2A |= (1 << WGM21);
	TCCR2B |= (1 << CS22) | (1 << CS21);
	OCR2A = 61; // 16000000/(256*1000) = 61
	TIMSK2 |= (1 << OCIE2A);

	// Odczytywanie has³a z pamiêci EEPROM
	eeprom_read_array(0x0F, code, 6);
	
	//Jeœli zapomni siê has³a to wgrywanie nowego
	//int initialCode[6] = {1, 2, 3, 4, 5, 6};
	//eeprom_write_array(0x0F, initialCode, 6);
	//for (int i = 0; i < 6; ++i) {
	//    code[i] = initialCode[i]; // Jakbysmy zgubili kod to mozna odkomentowac i z automatu 12345 has³o
	//}

}

void wait_ms(uint16_t milliseconds) {
	while (milliseconds > 0) {
		// Ustaw Timer1 w tryb CTC
		TCCR1A = 0; // Wyzerowanie rejestru kontrolnego A
		TCCR1B = (1 << WGM12) | (1 << CS11) | (1 << CS10); // WGM12: CTC tryb, CS11 | CS10: Preskaler 64

		// Ustaw wartoœæ porównania dla OCR1A, aby uzyskaæ 1 ms
		OCR1A = 249; // Liczba cykli na 1 ms - 1

		// Wyczyœæ licznik
		TCNT1 = 0;

		// Poczekaj na ustawienie flagi porównania
		while (!(TIFR1 & (1 << OCF1A)));

		// Wyczyœæ flagê porównania
		TIFR1 = (1 << OCF1A);

		// Zmniejsz licznik milisekund 
		milliseconds--;
	}
}

// Zapis pamiêci 
void eeprom_write_byte(uint16_t address, uint8_t data) {
	while (EECR & (1 << EEPE));
	EEAR = address;
	EEDR = data;
	EECR |= (1 << EEMPE);
	EECR |= (1 << EEPE); 
}

// Odczyt pamiêci
uint8_t eeprom_read_byte(uint16_t address) {
	while (EECR & (1 << EEPE));
	EEAR = address;
	EECR |= (1 << EERE);
	return EEDR;
}
// Zapis tablicy liczba po liczbie
void eeprom_write_array(uint16_t address, int* data, uint8_t length) {
	for (uint8_t i = 0; i < length; i++) {
		eeprom_write_byte(address + i, (uint8_t)data[i]);
	}
}
// Odczyt tablicy liczba po liczbie
void eeprom_read_array(uint16_t address, int* data, uint8_t length) {
	for (uint8_t i = 0; i < length; i++) {
		data[i] = (int)eeprom_read_byte(address + i);
	}
}

// Tablica z numerami do wyœwietlacza
const uint8_t digit[12] = {
	0b11000000, // 0
	0b11111001, // 1
	0b10100100, // 2
	0b10110000, // 3
	0b10011001, // 4
	0b10010010, // 5
	0b10000010, // 6
	0b11111000, // 7
	0b10000000, // 8
	0b10010000, // 9
	0b01000001, // U
	0b01000111  // L
};

// Ustalanie wartoœci na wyœwietlaczu
void show_digit(uint8_t digit_value, uint8_t position) {
	PORTB = 0xff; // 1 - gasi, 0 - zapala
	PORTD = digit[digit_value];
	switch (position) {
		case 0:
		PORTB = ~(1 << 0);
		break;
		case 1:
		PORTB = ~(1 << 1);
		break;
		case 2:
		PORTB = ~(1 << 2);
		break;
		case 3:
		PORTB &= ~(1 << 3);
		break;
	}
}
// Interupt do wyœwietlacza
ISR(TIMER2_COMPA_vect) { 
	if ((state == 0)||(state==3)) { 
		digits[0] = currentDigit / 1000; // Tysi¹ce
		digits[1] = (currentDigit / 100) % 10; // Setki
		digits[2] = (currentDigit / 10) % 10; // Dziesi¹tki
		digits[3] = currentDigit % 10; // Jednoœci
		show_digit(digits[position], position);
		if (position == 3) { // 0 1 2 3 itd
			position = 0;
			} else {
			position++;
		}
		} else if (state == 1) { // Odblokowany
		PORTB = 0x00;
		PORTD = digit[10];
		} else if (state == 2) { // Zablokowany
		PORTB = 0x00;
		PORTD = digit[11];
	}
}
// Interupt do przycisku
ISR(PCINT1_vect) {
	uint8_t currentStateCLK = PINC & (1 << 0); 
	if (currentStateCLK != lastStateCLK) { 
		uint8_t stateDT = PINC & (1 << 1);
		if (currentStateCLK == 0) { 
			if (stateDT == 0) {
				encoderValue++;
				} else {
				encoderValue--;
			}
		}
	}
	lastStateCLK = currentStateCLK;
	wait_ms(delay); //Debounce time
	
	if ((state==2)&&((PINC&(1<<2)))==0){ // Stan 2 L , wciœniêcie -> znowu wpisywanie
		encoderValue=0;
		lastencoderValue=0;
		state=0;
		step=0;
		direction=1;
		for (int i = 0; i < 6; i++) {
			number[i] = 0;
		}
	}
	if ((state==1)&&((PINC&(1<<2)))==0){ // Stan 1 U, wciœniêæie -> ustawianie has³a
		encoderValue=0;
		lastencoderValue=0;
		state=3;
		step=0;
		direction=1;
		for (int i = 0; i < 6; i++) {
			number[i] = 0;
		}
	}
}
// MAIN
int main(void) {
	setup();
	sei(); // Odblokowanie interuptów
	currentDigit = 0;
	for (int i = 0; i < 6; i++) { 
		number[i] = 0;
	}

	while (1) { // Stan 0 - wpisywanie kodu
		if((encoderValue!=lastencoderValue)&&((state==0)||(state==3))){ // Sprawdzenie czy siê zmieni³a wartoœæ na encoderze
			if (lastencoderValue < encoderValue) {
				direction = 1;
			}
			if (lastencoderValue > encoderValue) {
				direction = -1;
			}
			if (lastdirection != direction) {
				if (state == 3){
					code[step]=number[step];
				}
				step++; 
				lastdirection = direction;
				encoderValue = 0;
				lastencoderValue = encoderValue;
				} else {
				lastencoderValue = encoderValue;
				number[step] = abs(encoderValue); // Wyœwietlenie dodatniej wartoœci jak jest na minusie
					if (state == 3){
						code[step]=number[step];  
				}
				
			}
		} // Limit na wyœwietlaczu
			if (number[step] >= 100 || number[step] <= -100) {
			encoderValue = 0;
			lastencoderValue = encoderValue;
			number[step] = 0;
		}
		
		currentDigit = (number[step]); // Do wyœwietlacza
		
		if (step == 6) { // Sprawdzanie numeru z has³em
			uint8_t correct = 1; 
			for (int i = 0; i < 6; i++) {
				if (number[i] != code[i]) {
					correct = 0;
					break;
				}
			}
			if (correct==1) {
				eeprom_write_array(0x0F, code, 6); // Przy poprawnym odblokowaniu, i ustawieniu nowego has³a zapis na eprom
				state = 1;
				step++;
			}
			if(correct==0){
				state = 2;
				step++;
			}
		}
		if((state == 1)&&(encoderValue!=lastencoderValue)){ // Przy odblokowaniu ka¿de poruszenie znowu zablokuje
			encoderValue=0;
			lastencoderValue=0;
			state=0;
			step=0;
			direction=1;
			for (int i = 0; i < 6; i++) {
				number[i] = 0;
			}
		}
	}
}
