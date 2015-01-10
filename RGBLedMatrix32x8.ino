#include <ColorCalc.h>
#include <SPI.h>

// Pin configurations.
const short DISP_RST_N = 48;
const short DISP_UPDATE = 46;
const short DISP_EN_N = 47;
const short ROW_EN[8] = {43, 41, 39, 37, 35, 33, 31, 29};

const short COLOR_BITS = 8;    // Number of bits in a color code.

// Times to wait between each write of a given color bit.
const uint8_t COLOR_BIT_WAIT[8] = {
    0x01,
    0x02,
    0x04,
    0x08,
    0x10,
    0x20,
    0x40,
    0x80
};

uint8_t red[32][8] = {};
uint8_t blue[32][8] = {};
uint8_t green[32][8] = {};

// Global data.
unsigned long writeTimeUs = 0; // The last time data was shifted to the display.
uint8_t row = 7;               // The row to write to.
uint8_t lastRow = 6;           // The last row that was written to.
uint8_t colorBit = 7;          // The set of color bits to be written.
uint8_t colorBitMask;
uint8_t startCol;


void setup()
{
    Serial.begin(9600);
    SPI.begin();
    pinMode(DISP_RST_N, OUTPUT);
    pinMode(DISP_UPDATE, OUTPUT);
    pinMode(DISP_EN_N, OUTPUT);
    
    // Set SPI for fastest data transfer setting.
    SPI.setClockDivider(SPI_CLOCK_DIV2);
    
    // Set the reset pin low to clear shift registers.
    digitalWrite(DISP_RST_N, LOW);
    
    // Set update pin low initially.
    digitalWrite(DISP_UPDATE, LOW);
    
    // Enable the display.
    digitalWrite(DISP_EN_N, LOW);
    
    // Set each row enable low.
    for (int i = 0; i < 8; i++)
    {
        pinMode(ROW_EN[i], OUTPUT);
        digitalWrite(ROW_EN[i], LOW);
    }
    
    // Set resest high to allow registers to be written.
    digitalWrite(DISP_RST_N, HIGH);
    
    
    for (int row = 0; row < 8; row++)
    {
        for (int col = 0; col < 16; col++)
        {
            uint8_t r, g, b;
            
            CalcRGBFromHue((row+col)*20, r, g, b);
            
            red[row][col] = r;
            green[row][col] = g;
            blue[row][col] = b;
        }
    }
}


void loop()
{
    if (IsTimeToUpdate())
    {
        // Record the starting time of the current write.
        writeTimeUs = micros();
        
        // Record the last row that was written to.
        lastRow = row;
        DetermineNextWrite();
        WriteToDisplay();
    }
}


// Check if enough time has passed since last time the display was written to,
// so that it can be written again.
bool IsTimeToUpdate()
{
    bool timeToUpdate = false;
    unsigned long currTimeUs = micros();
    
    //Serial.print(writeTimeUs);
    //Serial.print("\t");
    //Serial.println(currTimeUs);
    
    if ((currTimeUs - writeTimeUs) >= COLOR_BIT_WAIT[colorBit])
    {
        //Serial.println("Time");
        timeToUpdate = true;
    }
    
    return timeToUpdate;
}

// Determine the next row and color bit set to write to the display.
void DetermineNextWrite()
{    
    if (colorBit >= 7)
    {
        colorBit = 0;
        row = (row + 1) % 8;
    }
    else
    {
        colorBit++;
    }
    
    // Create a bit mask for grabbing the new color bit from color values.
    colorBitMask = 0x1 << colorBit;
}

// Writes a set of color bits to each register of the display, enabling the
void WriteToDisplay()
{
    
    // If we have switched rows, disable the last row and enable the new row.
    // Also, to prevent the last row's data from being displayed on the new row,
    // disable the display until the writes in this function are complete.
    if (lastRow != row)
    {
        digitalWrite(DISP_EN_N, HIGH);
        
        digitalWrite(ROW_EN[lastRow], LOW);
        digitalWrite(ROW_EN[row], HIGH);
    }
    
    
    // Perform four sets of three writes starting at columns 24, 16, 8, and 0,
    // in that order. Each subset of three writes is done in the order blue,
    // green, red.  This is because the shift registers on the display are wired
    // in the following order:
    //                 -------------------------------------------------------
    // shift data --> | Red 0-8 | Green 0-8 | Blue 0-8| R9-16 | G9-16 | B9-16 | etc
    //                 -------------------------------------------------------
    // To accommodate for this wiring and the fact that all data must be shifted
    // through each register that comes before the destination register, the bytes
    // are shifted in the reverse of the order in which the registers are wired.
    startCol = 24;
    
    for (uint8_t writeSetNum = 0; writeSetNum < 4; writeSetNum++)
    {
        uint8_t shiftByte;    // Byte to shift to the display
        
        shiftByte = GetShiftByte(blue);
        SPI.transfer(shiftByte);
        
        shiftByte = GetShiftByte(green);
        SPI.transfer(shiftByte);
        
        shiftByte = GetShiftByte(red);
        SPI.transfer(shiftByte);
        
        startCol -= 8;
    }
    
    // Latch the new data that has been shifted to the registers.
    digitalWrite(DISP_UPDATE, HIGH);
    digitalWrite(DISP_UPDATE, LOW);
    
    // Reenable the display if we disabled it at the beginning of this function.
    if (lastRow != row)
    {
        digitalWrite(DISP_EN_N, LOW);
    }
}

// Determine the byte to write to a specific register given the color
// matrix, starting column (after which the next 7 columns will be used),
// the row, and the color bit position.
uint8_t GetShiftByte(uint8_t color[][8])
{
    uint8_t shiftByte = 0xff;
    
    /***   TEST CODE   ***/
    // To check if we can properly address each column, write a different
    // byte depending on the start column.
    if (color == red)
    {
        switch (startCol)
        {
        case 0:
            shiftByte = 0b10000000;
            break;
            
        case 8:
            shiftByte = 0b11000000;
            break;
            
        case 16:
            shiftByte = 0b11100000;
            break;
            
        case 24:
            shiftByte = 0b11110000;
            break;
        default:
            shiftByte = 0b11111111;
            break;
        }
    }
    
    /*** END TEST CODE ***/
    
    return shiftByte;
}
