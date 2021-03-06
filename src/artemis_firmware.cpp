#include "artemis_firmware.h"
#include "Wire.h"

superpower::artemis_firmware() {
	a = B00000000;
	b = B00000000;
}

int artemis_firmware::init() {
	Wire.begin();
	byte exp_config = wire_RX_8(EXP_addr, EXP_CONFIG);
	if(exp_config == B11111111) {
		wire_TX(EXP_addr, EXP_CONFIG, B00001000);						//set all exept Charge disable to outputs
		set_MCU(true);
		
		wire_TX(FG_addr, FG_POWERMODE, 0x0001 , true);					//sets FG power mode to active
		wire_TX(FG_addr, FG_APA, 0x0036 , true);						//sets battery pack to 3000mah
		wire_TX(FG_addr, FG_BATTPROF, 0x0001 , true);					//sets battery profile to 4.2v
		wire_TX(FG_addr, FG_INITRSOC, 0xAA55 , true);					//init RSOC
		wire_TX(FG_addr, FG_STATUSBIT, 0x0001 , true);					//sets thermistor mode for temperature
		wire_TX(FG_addr, FG_THERMISTORB, 3900 , true);					//sets thermistor sensitivity
		return 0;
	}
	
	set_MCU(true);
	wire_TX(FG_addr, FG_POWERMODE, 0x0001 , true);						//sets FG power mode to active
	if((wire_RX_8(RTC_addr, RTC_CONTROL_2) & B00000100) == B00000100) {
		a = wire_RX_8(RTC_addr, RTC_CONTROL_2) & B11111010;				//resets Timer flag, clear TIE
		wire_TX(RTC_addr, RTC_CONTROL_2, a);
		return 1;
	}
	else if((wire_RX_8(RTC_addr, RTC_CONTROL_2) & B00001000) == B00001000) {
		a = wire_RX_8(RTC_addr, RTC_CONTROL_2) & B11110101;				//resets Alarm flag, disable AIE
		wire_TX(RTC_addr, RTC_CONTROL_2, a);
		return 2;
	}
	return 3;
}

//fuel gauge functions
double artemis_firmware::get_temperature() {
	return (double)map(wire_RX_16(FG_addr, FG_CELLTEMPERATURE), 0x9E4, 0xD04, -200, 600) / 10;
}

double artemis_firmware::get_voltage() {
	return ((double)wire_RX_16(FG_addr, FG_CELLVOLTAGE)) / 1000;
}

double artemis_firmware::get_percentage() {
	return ((double)wire_RX_16(FG_addr, FG_CELLITE)) / 10;
}

//RTC functions
byte artemis_firmware::sleep(int seconds) {
	wire_TX(RTC_addr, RTC_CONTROL_2, (wire_RX_8(RTC_addr, RTC_CONTROL_2) | B00000001) & B11111011);	//activate TIE and clear TF
	wire_TX(RTC_addr, RTC_TIMER_C, B00000011);														//disables timer
	a = 0;
	if(seconds == 0) {
		wire_TX(RTC_addr, RTC_CONTROL_2, (wire_RX_8(RTC_addr, RTC_CONTROL_2) & B11111110)); 		//deactivate TIE
	}
	else if(seconds >= 256) {
		b = seconds / 60;
		wire_TX(RTC_addr, RTC_TIMER, b);															//set timer value
		wire_TX(RTC_addr, RTC_TIMER_C, B10000011);													//enable timer with 1/60HZ
		if(seconds % 60) {
			a = 1;
		}
	}
	else {
		b = seconds;
		wire_TX(RTC_addr, RTC_TIMER, b);															//set timer value
		wire_TX(RTC_addr, RTC_TIMER_C, B10000010);													//enable timer with 1HZ
	}
	
	wire_TX(FG_addr, FG_POWERMODE, 0x0002, true);													//sets FG power mode to sleep
	set_MCU(false);
	return a;
}

byte artemis_firmware::set_alarm(byte minute) {
	return set_alarm(minute, B10000000);
}

byte artemis_firmware::set_alarm(byte minute, byte hour) {
	return set_alarm(minute, hour, B10000000);
}

byte artemis_firmware::set_alarm(byte minute, byte hour, byte weekday) {
	return set_alarm(minute, hour, weekday, B10000000);
}

//sets alarm according to the input parameters. parameter B10000000 deactivates that comparison
//Note: does not power off the MCU
byte artemis_firmware::set_alarm(byte minute, byte hour, byte weekday, byte day_of_month) {
	//check minute format
	if(minute > 60) {
		a = 0;
	}
	else {
		a = wire_TX(RTC_addr, RTC_AL_MIN, decToBcd(minute));
	}
	//check hour format
	if(hour > 23) {
		a = 0;
	}
	else if(hour != B10000000) {
		a = wire_TX(RTC_addr, RTC_AL_HOUR, decToBcd(hour));
	}
	else {
		wire_TX(RTC_addr, RTC_AL_HOUR, B10000000);
	}
	//check weekday format
	if(weekday > 6) {
		a = 0;
	}
	else if(weekday != B10000000) {
		a = wire_TX(RTC_addr, RTC_AL_WDAY, decToBcd(weekday));
	}
	else {
		wire_TX(RTC_addr, RTC_AL_WDAY, B10000000);
	}
	//check day of month format
	if(day_of_month > 31 || day_of_month == B00000000) {
		a = 0;
	}
	else if(day_of_month != B10000000) {
		a = wire_TX(RTC_addr, RTC_AL_DAY, decToBcd(day_of_month));
	}
	else {
		wire_TX(RTC_addr, RTC_AL_DAY, B10000000);
	}
	//enable the alarm
	wire_TX(RTC_addr, RTC_CONTROL_2,  wire_RX_8(RTC_addr, RTC_CONTROL_2) | B00000010); //activate AIE
	
	return a;
}

void artemis_firmware::set_time(byte second, byte minute, byte hour, byte weekday, byte dayofmonth, byte month, byte year) {
	Wire.beginTransmission(RTC_addr);
    Wire.write(RTC_VL_SECOND);
    Wire.write(decToBcd(second));  
    Wire.write(decToBcd(minute));
    Wire.write(decToBcd(hour));     
    Wire.write(decToBcd(weekday));
    Wire.write(decToBcd(dayofmonth));  
    Wire.write(decToBcd(month));
    Wire.write(decToBcd(year));
    Wire.endTransmission();
}

String artemis_firmware::get_timestamp() {
	String stamp = "";
	Wire.beginTransmission(RTC_addr);
	Wire.write(RTC_VL_SECOND);
	Wire.endTransmission();
	Wire.requestFrom(RTC_addr, 7);
	int seconds = bcdToDec(Wire.read() & B01111111);					// remove VL error bit
	int minutes = bcdToDec(Wire.read() & B01111111);					// remove unwanted bits from MSB
	stamp += (String)bcdToDec(Wire.read() & B00111111) + ":" + minutes + ":" + seconds + ";"; 
	stamp += (String)bcdToDec(Wire.read() & B00111111) + "/";
	int dayofweek = bcdToDec(Wire.read() & B00000111);
	stamp += (String)bcdToDec(Wire.read() & B00011111) + "/20";			// remove century bit
	stamp += (String)bcdToDec(Wire.read()) + ";";
	stamp += (String)dayofweek;
	return stamp;
}

byte artemis_firmware::get_second() {
	return bcdToDec(wire_RX_8(RTC_addr, RTC_VL_SECOND) & B01111111);
}

byte artemis_firmware::get_minute() {
	return bcdToDec(wire_RX_8(RTC_addr, RTC_MINUTE) & B01111111);
}

byte artemis_firmware::get_hour() {
	return bcdToDec(wire_RX_8(RTC_addr, RTC_HOUR) & B00111111);
}

byte artemis_firmware::get_weekday() {
	return bcdToDec(wire_RX_8(RTC_addr, RTC_Weekday) & B00000111);
}

byte artemis_firmware::get_day() {
	return bcdToDec(wire_RX_8(RTC_addr, RTC_DAY) & B00111111);
}

byte artemis_firmware::get_month() {
	return bcdToDec(wire_RX_8(RTC_addr, RTC_MONTH) & B00011111);
}

byte artemis_firmware::get_year() {
	return bcdToDec(wire_RX_8(RTC_addr, RTC_YEAR));
}

//IO Expander functions
void artemis_firmware::set_AUX3(bool state) {
	set_EXP(1, state);
}

void artemis_firmware::set_AUX5(bool state) {
	set_EXP(2, state);
}

bool artemis_firmware::get_AUX3() {
	return get_EXP(1);
}

bool artemis_firmware::get_AUX5() {
	return get_EXP(2);
}

void artemis_firmware::set_Charging(bool state) {
	if(!state) {
		config_EXP(3, 1); //sets expander charge disable pin as a output
		set_EXP(3, state);
	}
	else {
		config_EXP(3, 0); //sets expander charge disable pin as a input to reduce consumption even further
	}
}

///////////////////////////////////Private Functions//////////////////////////////////////

bool artemis_firmware::get_EXP(byte pin) {
	return wire_RX_8(EXP_addr, EXP_INPUT) & (B00000001 << pin);
}

void artemis_firmware::set_EXP(byte pin, bool state) {
	if(!state) {
		a = wire_RX_8(EXP_addr, EXP_OUTPUT) & (B11111111 - (B00000001 << pin));
	}
	else {
		a = wire_RX_8(EXP_addr, EXP_OUTPUT) | (B00000001 << pin);
	}
	wire_TX(EXP_addr, EXP_OUTPUT, a);
}

//type 1: OUTPUT, 0: INPUT
void artemis_firmware::config_EXP(byte pin, bool type) {
	if(type) {
		a = (wire_RX_8(EXP_addr, EXP_CONFIG) & (B11111111 - (B00000001 << pin)));
	}
	else {
		a = (wire_RX_8(EXP_addr, EXP_CONFIG) | (B00000001 << pin));
	}
	wire_TX(EXP_addr, EXP_CONFIG, a);
}

void artemis_firmware::set_MCU(bool state) {
	set_EXP(0, state);
}


byte artemis_firmware::decToBcd(byte value) {
	return (value / 10 * 16 + value % 10);
}

byte artemis_firmware::bcdToDec(byte value) {
	return ((value / 16) * 10 + value % 16);
}

uint8_t artemis_firmware::crc8(uint8_t *data, int len) {
	const uint8_t POLYNOMIAL(0x07);
	uint8_t crc(0x00);
	for (int j = len; j; --j) {
		crc ^= *data++;
		for (int i = 8; i; --i) {
			crc = (crc & 0x80) ? (crc << 1) ^ POLYNOMIAL : (crc << 1);
		}
	}
	return crc;
}


uint8_t artemis_firmware::wire_TX(uint8_t address, uint8_t command, uint16_t msg, bool crc) {
	uint8_t dat[5];
	dat[0] = address * 2;
	dat[1] = command;
	dat[2] = msg & 0xFF;
	dat[3] = msg >> 8;
	dat[4] = crc8(dat, 4);
	Wire.beginTransmission(address);
	Wire.write(dat[1]);
	Wire.write(dat[2]);
	Wire.write(dat[3]);
	Wire.write(dat[4]);
	uint8_t i = Wire.endTransmission();
	return i;
}

uint8_t artemis_firmware::wire_TX(uint8_t address, uint8_t command, uint8_t msg) {
	Wire.beginTransmission(address);
	Wire.write(command);
	Wire.write(msg);
	uint8_t i = Wire.endTransmission();
	return i;
}

uint16_t artemis_firmware::wire_RX_16(uint8_t address, uint8_t command) {
	uint16_t dat;
	uint8_t reply[3];
	Wire.beginTransmission(address);
	Wire.write(command);
	Wire.endTransmission(false);
	Wire.requestFrom(address, 3);
	reply[0] = Wire.read();
	reply[1] = Wire.read();
	reply[2] = Wire.read();
	dat = reply[1];
	dat <<= 8;
	dat |= reply[0];
	return dat;
}

uint8_t artemis_firmware::wire_RX_8(uint8_t address,  uint8_t command) {
	Wire.beginTransmission(address);
	Wire.write(command);
	Wire.endTransmission();
	Wire.requestFrom(address, 1);
	return Wire.read();
}
