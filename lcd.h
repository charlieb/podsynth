#pragma once
#include "daisy_pod.h"
#include "daisysp.h"

// commands
#define LCD_CLEARDISPLAY 0x01
#define LCD_RETURNHOME 0x02
#define LCD_ENTRYMODESET 0x04
#define LCD_DISPLAYCONTROL 0x08
#define LCD_CURSORSHIFT 0x10
#define LCD_FUNCTIONSET 0x20
#define LCD_SETCGRAMADDR 0x40
#define LCD_SETDDRAMADDR 0x80

// flags for display entry mode
// use with LCD_ENTRYMODESET
#define LCD_ENTRYRIGHT 0x00
#define LCD_ENTRYLEFT 0x02
#define LCD_ENTRYSHIFTINCREMENT 0x01
#define LCD_ENTRYSHIFTDECREMENT 0x00

// flags for display on/off control
// use with LCD_DISPLAYCONTROL
#define LCD_DISPLAYON 0x04
#define LCD_DISPLAYOFF 0x00
#define LCD_CURSORON 0x02
#define LCD_CURSOROFF 0x00
#define LCD_BLINKON 0x01
#define LCD_BLINKOFF 0x00

// flags for display/cursor shift
#define LCD_DISPLAYMOVE 0x08
#define LCD_CURSORMOVE 0x00
#define LCD_MOVERIGHT 0x04
#define LCD_MOVELEFT 0x00

// flags for function set
// use with LCD_FUNCTIONSET
#define LCD_8BITMODE 0x10
#define LCD_4BITMODE 0x00
#define LCD_2LINE 0x08
#define LCD_1LINE 0x00
#define LCD_5x10DOTS 0x04
#define LCD_5x8DOTS 0x00

// flags for backlight control
// use as-is
#define LCD_BACKLIGHT 0x08
#define LCD_NOBACKLIGHT 0x00

#define En 0b00000100  // Enable bit
#define Rw 0b00000010  // Read/Write bit
#define Rs 0b00000001  // Register select bit

class LCD {
public:
static constexpr uint32_t timeout{500};
static constexpr uint8_t addr{0x27};
static constexpr uint8_t cols{16};
static constexpr uint8_t rows{2};

private:
uint8_t lcd_function{0};
uint8_t lcd_display_ctrl{0};
uint8_t lcd_display_entry_mode{0};
uint8_t backlight{LCD_NOBACKLIGHT};
daisy::I2CHandle i2c;

  public:
LCD() {
  static constexpr daisy::I2CHandle::Config i2c_config
    = {
      .periph = daisy::I2CHandle::Config::Peripheral::I2C_1,
      .pin_config = {
        .scl = {DSY_GPIOB, 8},
        .sda = {DSY_GPIOB, 9}},
      .speed = daisy::I2CHandle::Config::Speed::I2C_100KHZ,
      .mode = daisy::I2CHandle::Config::Mode::I2C_MASTER,
    };

    i2c.Init(i2c_config);
}

/************ low level data pushing commands **********/

void pulseEnable(uint8_t data){
  uint8_t byte{static_cast<uint8_t>(data | En)};
	i2c.TransmitBlocking(addr, &byte, 1, timeout);
  daisy::System::DelayUs(1);		// enable pulse must be >450ns
	
	byte = data & ~En;
	i2c.TransmitBlocking(addr, &byte, 1, timeout);
	daisy::System::DelayUs(50);		// commands need > 37us to settle
} 

void send_nibble(uint8_t nib) {
  uint8_t bl_nib{static_cast<uint8_t>(nib | backlight)};
  i2c.TransmitBlocking(addr, &bl_nib, 1, timeout);
  pulseEnable(bl_nib);
}

void send(uint8_t b, uint8_t mode) {
  uint8_t hi{static_cast<uint8_t>((b & 0xF0) | mode)};
  uint8_t lo{static_cast<uint8_t>(((b << 4) & 0xF0) | mode)};
  send_nibble(hi);
  send_nibble(lo);
}

void command(uint8_t b) {
  send(b, 0);
}

// Turn the (optional) backlight off/on
void backlight_off(void) {
	backlight = LCD_NOBACKLIGHT;
  constexpr uint32_t timeout{500};
	i2c.TransmitBlocking(addr, &backlight, 1, timeout);
}

void backlight_on(void) {
	backlight = LCD_BACKLIGHT;
  constexpr uint32_t timeout{500};
	i2c.TransmitBlocking(addr, &backlight, 1, timeout);
}
// Based on the work by DFRobot

// When the display powers up, it is configured as follows:
//
// 1. Display clear
// 2. Function set: 
//    DL = 1; 8-bit interface data 
//    N = 0; 1-line display 
//    F = 0; 5x8 dot character font 
// 3. Display on/off control: 
//    D = 0; Display off 
//    C = 0; Cursor off 
//    B = 0; Blinking off 
// 4. Entry mode set: 
//    I/D = 1; Increment by 1
//    S = 0; No shift 
//
// Note, however, that resetting the Arduino doesn't reset the LCD, so we
// can't assume that its in that state when a sketch starts (and the
// LiquidCrystal constructor is called).


void init() {
	// SEE PAGE 45/46 FOR INITIALIZATION SPECIFICATION!
	// according to datasheet, we need at least 40ms after power rises above 2.7V
	// before sending commands. Arduino can turn on way befer 4.5V so we'll wait 50
  daisy::DaisySeed::Print("LCD::init\n");
	daisy::System::Delay(50); 
  
	// Now we pull both RS and R/W low to begin commands
	backlight_off();	// reset expanderand turn backlight off (Bit 8 =1)
	daisy::System::Delay(1000);

  	//put the LCD into 4 bit mode
	// this is according to the hitachi HD44780 datasheet
	// figure 24, pg 46
	
	  // we start in 8bit mode, try to set 4 bit mode
   send_nibble(0x03 << 4);
   daisy::System::DelayUs(4500); // wait min 4.1ms
   
   // second try
   send_nibble(0x03 << 4);
   daisy::System::DelayUs(4500); // wait min 4.1ms
   
   // third go!
   send_nibble(0x03 << 4); 
   daisy::System::DelayUs(150);
   
   // finally, set to 4-bit interface
   send_nibble(0x02 << 4); 


	// set # lines, font size, etc.
   lcd_function = LCD_4BITMODE | LCD_2LINE | LCD_5x10DOTS;
	command(LCD_FUNCTIONSET | lcd_function);
	
	// turn the display on with no cursor or blinking default
  lcd_display_ctrl = LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF;
  command(LCD_DISPLAYCONTROL | lcd_display_ctrl);
	
	// clear it off
	clear();
	
	// Initialize to default text direction (for roman languages)
	lcd_display_entry_mode = LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT;
	
	// set the entry mode
	command(LCD_ENTRYMODESET | lcd_display_entry_mode);
	
	home();

  backlight_on();
  
}

/********** high level commands, for the user! */
void clear(){
	command(LCD_CLEARDISPLAY);// clear display, set cursor position to zero
	daisy::System::DelayUs(2000);  // this command takes a long time!
}

void home(){
	command(LCD_RETURNHOME);  // set cursor position to zero
	daisy::System::DelayUs(2000);  // this command takes a long time!
}

void setCursor(uint8_t col, uint8_t row){
	int row_offsets[] = { 0x00, 0x40, 0x14, 0x54 };
	if ( row > rows ) {
		row = rows - 1;    // we count rows starting w/0
	}
	command(LCD_SETDDRAMADDR | (col + row_offsets[row]));
}

// Turn the display on/off (quickly)
void noDisplay() {
	lcd_display_ctrl &= ~LCD_DISPLAYON;
	command(LCD_DISPLAYCONTROL | lcd_display_ctrl);
}
void display() {
	lcd_display_ctrl |= LCD_DISPLAYON;
	command(LCD_DISPLAYCONTROL | lcd_display_ctrl);
}

// Turns the underline cursor on/off
void noCursor() {
	lcd_display_ctrl &= ~LCD_CURSORON;
	command(LCD_DISPLAYCONTROL | lcd_display_ctrl);
}
void cursor() {
	lcd_display_ctrl |= LCD_CURSORON;
	command(LCD_DISPLAYCONTROL | lcd_display_ctrl);
}

// Turn on and off the blinking cursor
void noBlink() {
	lcd_display_ctrl &= ~LCD_BLINKON;
	command(LCD_DISPLAYCONTROL | lcd_display_ctrl);
}
void blink() {
	lcd_display_ctrl |= LCD_BLINKON;
	command(LCD_DISPLAYCONTROL | lcd_display_ctrl);
}

// These commands scroll the display without changing the RAM
void scrollDisplayLeft(void) {
	command(LCD_CURSORSHIFT | LCD_DISPLAYMOVE | LCD_MOVELEFT);
}
void scrollDisplayRight(void) {
	command(LCD_CURSORSHIFT | LCD_DISPLAYMOVE | LCD_MOVERIGHT);
}

// This is for text that flows Left to Right
void leftToRight(void) {
	lcd_display_entry_mode |= LCD_ENTRYLEFT;
	command(LCD_ENTRYMODESET | lcd_display_entry_mode);
}

// This is for text that flows Right to Left
void rightToLeft(void) {
	lcd_display_entry_mode &= ~LCD_ENTRYLEFT;
	command(LCD_ENTRYMODESET | lcd_display_entry_mode);
}

// This will 'right justify' text from the cursor
void autoscroll(void) {
	lcd_display_entry_mode |= LCD_ENTRYSHIFTINCREMENT;
	command(LCD_ENTRYMODESET | lcd_display_entry_mode);
}

// This will 'left justify' text from the cursor
void noAutoscroll(void) {
	lcd_display_entry_mode &= ~LCD_ENTRYSHIFTINCREMENT;
	command(LCD_ENTRYMODESET | lcd_display_entry_mode);
}

void print(std::string str) {
  for(auto ch : str) {
    send(ch, Rs);
  }
}

// TODO figure this out and put it back for custom 
// characters
// Allows us to fill the first 8 CGRAM locations
// with custom characters
//void createChar(uint8_t location, uint8_t charmap[]) {
//	location &= 0x7; // we only have 8 locations 0-7
//	command(LCD_SETCGRAMADDR | (location << 3));
//	for (int i=0; i<8; i++) {
//		write(charmap[i]);
//	}
//}
//
////createChar with PROGMEM input
//void createChar(uint8_t location, const char *charmap) {
//	location &= 0x7; // we only have 8 locations 0-7
//	command(LCD_SETCGRAMADDR | (location << 3));
//	for (int i=0; i<8; i++) {
//	    	write(pgm_read_byte_near(charmap++));
//	}
//}
//void load_custom_character(uint8_t char_num, uint8_t *rows){
//		createChar(char_num, rows);
//}

/*********** mid level commands, for sending data/cmds */

// Alias functions

void cursor_on(){
	cursor();
}

void cursor_off(){
	noCursor();
}

void blink_on(){
	blink();
}

void blink_off(){
	noBlink();
}

};


//void test() {
//    while(1) {
//        // Ask for 31 bytes.
//        uint8_t number = 32;
//        pod.PrintLine("%05ld Asking for %d bytes on address %0x.", counter, number, address);
//        daisy::I2CHandle::Result i2cResult = _i2c.TransmitBlocking(addr, &number, 1, 500);
//        if (i2cResult == daisy::I2CHandle::Result::OK) {
//            pod.PrintLine("%05ld Successfully transmitted requested number of bytes: %0x.", counter, number);
//
//            // Receive the requested number of bytes + CRC
//            pod.PrintLine("%05ld Receiving %0d bytes from address %0x.", counter, number + 1, address);                                  
//            i2cResult = _i2c.ReceiveBlocking(address, testData, number + 1, 500);
//            if(i2cResult == daisy::I2CHandle::Result::OK) {
//                pod.PrintLine("%05ld %0x bytes were received from address 0x%x.", counter, address);
//            }
//        } else {
//            pod.PrintLine("%05ld Request for data not acknowledged.", counter);
//        }
//        System::Delay(1000);
//        counter++;
//    }
//}
