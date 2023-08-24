#include <Arduino.h>

#define EPD_SCK_PIN  13
#define EPD_MOSI_PIN 14
#define EPD_CS_PIN   15
#define EPD_RST_PIN  26
#define EPD_DC_PIN   27
#define EPD_BUSY_PIN 4

// #define EPD_SCK_PIN  15
// #define EPD_MOSI_PIN 12
// #define EPD_CS_PIN   14
// #define EPD_RST_PIN  26
// #define EPD_DC_PIN   2
// #define EPD_BUSY_PIN 13

#define GPIO_PIN_SET   1
#define GPIO_PIN_RESET 0

#define epd_digitalWrite(_pin, _value) digitalWrite(_pin, _value == 0? LOW:HIGH)
#define epd_digitalRead(_pin) digitalRead(_pin)

#define EPD_WIDTH   600
#define EPD_HEIGHT  448

#define EPD_BLACK   0x0	/// 000
#define EPD_WHITE   0x1	///	001
#define EPD_GREEN   0x2	///	010
#define EPD_BLUE    0x3	///	011
#define EPD_RED     0x4	///	100
#define EPD_YELLOW  0x5	///	101
#define EPD_ORANGE  0x6	///	110
#define EPD_CLEAN   0x7	///	111   unavailable  Afterimage

void epd_writebyte(uint8_t data)
{
    //SPI.beginTransaction(spi_settings);
    digitalWrite(EPD_CS_PIN, GPIO_PIN_RESET);

    for (int i = 0; i < 8; i++)
    {
        if ((data & 0x80) == 0) digitalWrite(EPD_MOSI_PIN, GPIO_PIN_RESET); 
        else                    digitalWrite(EPD_MOSI_PIN, GPIO_PIN_SET);

        data <<= 1;
        digitalWrite(EPD_SCK_PIN, GPIO_PIN_SET);     
        digitalWrite(EPD_SCK_PIN, GPIO_PIN_RESET);
    }

    //SPI.transfer(data);
    digitalWrite(EPD_CS_PIN, GPIO_PIN_SET);
    //SPI.endTransaction();	
}

void epd_reset(void)
{
    epd_digitalWrite(EPD_RST_PIN, 1);
    delay(200);
    epd_digitalWrite(EPD_RST_PIN, 0);
    delay(1);
    epd_digitalWrite(EPD_RST_PIN, 1);
    delay(200);
}

 void epd_sendcommand(uint8_t Reg)
{
    epd_digitalWrite(EPD_DC_PIN, 0);
    epd_digitalWrite(EPD_CS_PIN, 0);
    epd_writebyte(Reg);
    epd_digitalWrite(EPD_CS_PIN, 1);
}

 void epd_senddata(uint8_t Data)
{
    epd_digitalWrite(EPD_DC_PIN, 1);
    epd_digitalWrite(EPD_CS_PIN, 0);
    epd_writebyte(Data);
    epd_digitalWrite(EPD_CS_PIN, 1);
}

 void epd_busyhigh(void)// If BUSYN=0 then waiting
{
    while(!(epd_digitalRead(EPD_BUSY_PIN)));
}

 void epd_busylow(void)// If BUSYN=1 then waiting
{
    while(epd_digitalRead(EPD_BUSY_PIN));
}

void epd_init(void)
{
    // Init GPIO
    pinMode(EPD_BUSY_PIN,  INPUT);
    pinMode(EPD_RST_PIN , OUTPUT);
    pinMode(EPD_DC_PIN  , OUTPUT);
    
    pinMode(EPD_SCK_PIN, OUTPUT);
    pinMode(EPD_MOSI_PIN, OUTPUT);
    pinMode(EPD_CS_PIN , OUTPUT);

    digitalWrite(EPD_CS_PIN , HIGH);
    digitalWrite(EPD_SCK_PIN, LOW);

    // Send commands
	epd_reset();
    epd_busyhigh();
    epd_sendcommand(0x00);
    epd_senddata(0xEF);
    epd_senddata(0x08);
    epd_sendcommand(0x01);
    epd_senddata(0x37);
    epd_senddata(0x00);
    epd_senddata(0x23);
    epd_senddata(0x23);
    epd_sendcommand(0x03);
    epd_senddata(0x00);
    epd_sendcommand(0x06);
    epd_senddata(0xC7);
    epd_senddata(0xC7);
    epd_senddata(0x1D);
    epd_sendcommand(0x30);
    epd_senddata(0x3C);
    epd_sendcommand(0x41);
    epd_senddata(0x00);
    epd_sendcommand(0x50);
    epd_senddata(0x37);
    epd_sendcommand(0x60);
    epd_senddata(0x22);
    epd_sendcommand(0x61);
    epd_senddata(0x02);
    epd_senddata(0x58);
    epd_senddata(0x01);
    epd_senddata(0xC0);
    epd_sendcommand(0xE3);
    epd_senddata(0xAA);
	
	delay(100);
    epd_sendcommand(0x50);
    epd_senddata(0x37);
}

void epd_clear(uint8_t color)
{
    epd_sendcommand(0x61);//Set Resolution setting
    epd_senddata(0x02);
    epd_senddata(0x58);
    epd_senddata(0x01);
    epd_senddata(0xC0);
    epd_sendcommand(0x10);
    for(int i=0; i<EPD_HEIGHT; i++) {
        for(int j=0; j<EPD_WIDTH/2; j++)
            epd_senddata((color<<4)|color);
    }
    epd_sendcommand(0x04);//0x04
    epd_busyhigh();
    epd_sendcommand(0x12);//0x12
    epd_busyhigh();
    epd_sendcommand(0x02);  //0x02
    epd_busylow();
    delay(500);
}

void epd_display(const uint8_t *image)
{
    uint16_t i, j, WIDTH;
	uint8_t Rdata, Rdata1, shift;
	uint32_t Addr;
	WIDTH = EPD_WIDTH*3%8 == 0 ? EPD_WIDTH*3/8 : EPD_WIDTH*3/8+1;
    epd_sendcommand(0x61);//Set Resolution setting
    epd_senddata(0x02);
    epd_senddata(0x58);
    epd_senddata(0x01);
    epd_senddata(0xC0);
    epd_sendcommand(0x10);
    for(i=0; i<EPD_HEIGHT; i++) {
        for(j=0; j<EPD_WIDTH/2; j++) {
					shift = (j + i*EPD_WIDTH/2) % 4;
					Addr = (j*3/4 + i * WIDTH);

					if(shift == 0) {
						Rdata = image[Addr];
						epd_senddata(((Rdata >> 1) & 0x70) | ((Rdata >> 2) & 0x07));
					}
					else if(shift == 1) {
						Rdata = image[Addr];
						Rdata1 = image[Addr + 1];
						epd_senddata(((Rdata << 5) & 0x60) | ((Rdata1 >> 3) & 0x10) | ((Rdata1 >> 4) & 0x07));
					}
					else if(shift == 2) {
						Rdata = image[Addr];
						Rdata1 = image[Addr + 1];
						epd_senddata(((Rdata << 3) & 0x70) | ((Rdata << 2) & 0x04) | ((Rdata1 >> 6) & 0x03));
					}
					else if(shift == 3) {
						Rdata = image[Addr];
						epd_senddata(((Rdata << 1) & 0x70) | (Rdata & 0x07));
					}
				}
    }
    epd_sendcommand(0x04);//0x04
    epd_busyhigh();
    epd_sendcommand(0x12);//0x12
    epd_busyhigh();
    epd_sendcommand(0x02);  //0x02
    epd_busylow();
	delay(200);
	
}

void epd_display_fullimage(const uint8_t *image)
{
    uint16_t i, j, WIDTH;
	uint8_t Rdata, Rdata1, shift;
	uint32_t Addr;
	WIDTH = EPD_WIDTH*3%8 == 0 ? EPD_WIDTH*3/8 : EPD_WIDTH*3/8+1;
    epd_sendcommand(0x61);//Set Resolution setting
    epd_senddata(0x02);
    epd_senddata(0x58);
    epd_senddata(0x01);
    epd_senddata(0xC0);
    epd_sendcommand(0x10);
    for(i=0; i<EPD_HEIGHT; i++) {
        for(j=0; j<EPD_WIDTH/2; j++) {
					epd_senddata(image[i*EPD_WIDTH/2 + j]);
				}
    }
    epd_sendcommand(0x04);//0x04
    epd_busyhigh();
    epd_sendcommand(0x12);//0x12
    epd_busyhigh();
    epd_sendcommand(0x02);  //0x02
    epd_busylow();
	delay(200);
	
}

void epd_display_part(const uint8_t *image, uint16_t xstart, uint16_t ystart, 
									uint16_t image_width, uint16_t image_heigh)
{
    unsigned long i,j;
    epd_sendcommand(0x61);//Set Resolution setting
    epd_senddata(0x02);
    epd_senddata(0x58);
    epd_senddata(0x01);
    epd_senddata(0xC0);
    epd_sendcommand(0x10);
    for(i=0; i<EPD_HEIGHT; i++) {
        for(j=0; j< EPD_WIDTH/2; j++) {
						if(i<image_heigh+ystart && i>=ystart && j<(image_width+xstart)/2 && j>=xstart/2) {
							epd_senddata(image[(j-xstart/2) + (image_width/2*(i-ystart))]);
						}
						else {
							epd_senddata(0x11);
						}
				}
    }
    epd_sendcommand(0x04);//0x04
    epd_busyhigh();
    epd_sendcommand(0x12);//0x12
    epd_busyhigh();
    epd_sendcommand(0x02);  //0x02
    epd_busylow();
	delay(200);
	
}

void epd_sleep(void)
{
    delay(100);
    epd_sendcommand(0x07);
    epd_senddata(0xA5);
    delay(100);
	epd_digitalWrite(EPD_RST_PIN, 0); // Reset
}
