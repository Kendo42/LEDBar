#include <DMXSerial.h>
#include <FastLED.h>
#include <U8glib.h>
#include <EEPROM.h>

// note to self: would effect stacking work?

/*
    Program changelog
    -----------------
    Change 17/01/21
        - Changed #define > const (seen as better practice according to arduino)
        - Implemented DMX new channels (freq, start + stop led)
        - Modified effects to utilise primary colours and effect frequencies
        - Removed unused variables
        - Reworked effect IDs
*/

U8GLIB_SSD1306_128X64 u8g(U8G_I2C_OPT_NONE | U8G_I2C_OPT_DEV_0); // I2C / TWI <-- Constructor call for the LCD display library

/*   +-------------+
 *   |  DMX SETUP  |
 *   +-------------+
 */

int START_ADDRESS; // = 0
char START_ADDRESS_STR[3];

/*   +-----------------+
 *   |  Button  SETUP  |
 *   +-----------------+
 */

int DOWN_BUTTON_STATE = 0;
int UP_BUTTON_STATE = 0;

/*   +-------------+
 *   |  LED SETUP  |
 *   +-------------+
 */

const LED_COUNT 27;   // Total number of LED's in lightbar  | NOTE: THIS MUST BE HARDCODED FOR EVERY DIFFERENT LIGHTBAR
const LED_DATA_PIN 7; // Data pin number

int BOTTOM_INDEX = 0;
int TOP_INDEX = int(LED_COUNT / 2);
int EVENODD = LED_COUNT % 2;
struct CRGB LEDS[LED_COUNT];
int LED_X[LED_COUNT][3]; // Array for copying what is currently in the leds (for CELL-AUTOMATA, MARCH, ETC)

int FX_DELAY = 20;          // FX loops delay var
int FX_STEP = 10;           //
int FX_HUE = 0;             //
int FX_SAT = 255;           //
int FX_BRIGHTNESS = 255;    //
int FX_MAX_BRIGHTNESS = 64; // Set max brigthness to 1/4
int FX_START_LED = 0;
int FX_END_LED = LED_COUNT;
// LED FX vars
int I_DEX = 0;             // led index (0 to LED_COUNT-1)
int I_HUE = 0;             // Hue (0-255)
int I_BRIGHTNESS = 0;      // Brightness (0-255)
int BOUNCED_DIRECTION = 0; // Switch for colour bounce (0-1)
float T_COUNT = 0.0;       // Inc var for sin loops

// Function to find the index of horizontal opposite LED
int horizontal_index(int i)
{
    // Only works with INDEX < TOP_INDEX
    if (i == BOTTOM_INDEX)
    {
        return BOTTOM_INDEX;
    }
    if (i == TOP_INDEX && EVENODD == 1)
    {
        return TOP_INDEX + 1;
    }
    if (i == TOP_INDEX && EVENODD == 0)
    {
        return TOP_INDEX;
    }
    return LED_COUNT - i;
}

void setup()
{
    DMXSerial.init(DMXReceiver);     // initialising our DMX object?
    START_ADDRESS = EEPROMReadInt(0) // persistant start address between reboots

        pinMode(8, OUTPUT); // Pin is for display
    pinMode(10, INPUT);     // Pin is for DMX addr up button
    pinMode(11, INPUT);     // Pin is for DMX addr up button

    // Defining default DMX Channel values
    DMXSerial.write(START_ADDRESS, 224);           // Hue
    DMXSerial.write(START_ADDRESS + 1, 255);       // Saturation
    DMXSerial.write(START_ADDRESS + 2, 255);       // Brightness
    DMXSerial.write(START_ADDRESS + 3, 0);         // Effect ID
    DMXSerial.write(START_ADDRESS + 4, 0);         // Effect frequency
    DMXSerial.write(START_ADDRESS + 5, 0);         // Start LED
    DMXSerial.write(START_ADDRESS + 6, LED_COUNT); // End LED

    LEDS.setBrightness(FX_MAX_BRIGHTNESS);
    LEDS.addLeds<WS2811, LED_DATA_PIN, GRB>(LEDS, LED_COUNT); // Initialising LED strip

    u8g.firstPage();
    do
    {
        DisplayAddress();
    } while (u8g.nextPage());
}

void loop()
{

    DOWN_BUTTON_STATE = digitalRead(10); // Reading in button
    UP_BUTTON_STATE = digitalRead(11);   // ^

    if (1 == 2) //if (DownButtonState == HIGH) {
    {
        START_ADDRESS = EEPROMReadInt(0);
        START_ADDRESS--;
        EEPROMWriteInt(0, START_ADDRESS);
        u8g.firstPage();
        do
        {
            DisplayAddress();
        } while (u8g.nextPage());
    }

    if (1 == 2) //if (UpButtonState == HIGH) {
    {
        START_ADDRESS = EEPROMReadInt(0);
        START_ADDRESS++;
        EEPROMWriteInt(0, START_ADDRESS);
        u8g.firstPage();
        do
        {
            DisplayAddress();
        } while (u8g.nextPage());
    }

    unsigned long LAST_PACKET = DMXSerial.noDataSince(); // Calculating how long since no data packet was received (in ms?)

    /*  DMX channels
        -------------------------
        Channel 0: Hue (Equal to START_ADDRESS)
        Channel 1: Saturation
        Channel 2: Brightness
        Channel 3: Effect ID ( see effects data sheet )
        Channel 4: Effect delay
        Channel 5: Start Led
        Channel 6: End Led
    */

    if (LAST_PACKET < 5000)
    {                                                      // If a packet has been received
        FX_HUE = DMXSerial.read(START_ADDRESS);            // Read recent DMX values and set pwm levels
        FX_SAT = DMXSerial.read(START_ADDRESS + 1);        //
        FX_BRIGHTNESS = DMXSerial.read(START_ADDRESS + 2); //
        FX_DELAY = DMXSerial.read(START_ADDRESS + 4);      //
        FX_START_LED = DMXSerial.read(START_ADDRESS + 5);  //
        FX_END_LED = DMXSerial.read(START_ADDRESS + 6);    //
        if (DMXSerial.read(START_ADDRESS + 3) == 0)        // If there is no effect ID then solid colour, but effect 0 is clear colour?
        {

            SolidColour(); // <--- might cause problems because if effect only applies to half of strip, solidColour() will apply to the background
        }
        else
        {
            switch (DMXSerial.read(START_ADDRESS + 3))
            {
            case 0:
                ClearLEDs();
                break;
            case 1:
                BallBounce();
                break;
            case 2:
                RainbowFade();
                break;
            case 3:
                RainbowLoop();
                break;
            case 4:
                RainbowLoop2();
                break;
            case 5:
                RandomColour();
                break;
            case 6:
                FadeColour();
                break;
            case 7:
                MoveToCenter();
                break;
            case 8:
                RainbowMoveToCenter();
                break;
            case 9:
                RandomStrobe();
                break;
            case 10:
                RandomRainbowStrobe();
                break;
            }
        }
    }
    else
    {
        FastLED.clear();
        LEDS.show();
    }
}

void DisplayAddress(void)
{
    itoa(StartingAddress, StartingAddressStr, 10);
    u8g.setFont(u8g_font_unifont);
    u8g.drawStr(1, 15, "DMX Address:");
    u8g.drawStr(98, 15, START_ADDRESS_STR);
}

void EEPROMWriteInt(int address, int value)
{
    byte two = (value & 0xFF);
    byte one = ((value >> 8) & 0xFF);
    EEPROM.update(address, two);
    EEPROM.update(address + 1, one);
}

int EEPROMReadInt(int address)
{
    long two = EEPROM.read(address);
    long one = EEPROM.read(address + 1);
    return ((two << 0) & 0xFFFFFF) + ((one << 8) & 0xFFFFFFFF);
}

/*  +---------------+
 *  |  LED EFFECTS  |
 *  +---------------+
 *  
 *  Effects always require the following parameters:
 *    - Hue
 *    - Saturation
 *    - Brightness
 *    - Start
 *    - End
 *   
 *  Effects might require the following parameters:
 *    - Primary Colour
 *    - Secondary colour
 *    - Frequency
 *    
 *    For more info on effects see data sheet or <URL>
 */

// Clears every LED's value
void ClearLEDs()
{
    FastLED.clear();
    LEDS.show();
}

// Applies a solid colour to range of LEDs
void SolidColour()
{
    for (int i = FX_START_LED; i < FX_END_LED; i++)
    {
        LEDS[i] = CHSV(FX_HUE, FX_SAT, FX_BRIGHTNESS)
    }
    LEDS.show();
}

void BallBounce()
{ //Ball Bounce
    if (BOUNCED_DIRECTION == 0)
    {
        I_DEX = I_DEX + 1;
        if (I_DEX == LED_COUNT)
        {
            BOUNCED_DIRECTION = 1;
            I_DEX = I_DEX - 1;
        }
    }
    if (BOUNCED_DIRECTION == 1)
    {
        I_DEX = I_DEX - 1;
        if (I_DEX == 0)
        {
            BOUNCED_DIRECTION = 0;
        }
    }
    for (int i = 0; i < LED_COUNT; i++)
    {
        if (i == I_DEX)
        {
            LEDS[i] = CHSV(FX_HUE, FX_SAT, FX_BRIGHTNESS);
        }
        else
        {
            LEDS[i] = CHSV(0, 0, 0);
        }
    }
    LEDS.show();
    delay(FX_DELAY);
}

void RainbowFade()
{ //Rainbow Fade
    I_HUE++;
    if (I_HUE > 255)
    {
        I_HUE = 0;
    }
    for (int I_DEX = 0; I_DEX < LED_COUNT; I_DEX++)
    {
        LEDS[I_DEX] = CHSV(I_HUE, FX_SAT, FX_BRIGHTNESS);
    }
    LEDS.show();
    delay(FX_DELAY);
}

void RainbowLoop()
{ //Rainbow Loop
    I_DEX++;
    I_HUE = I_HUE + FX_STEP;
    if (I_DEX >= LED_COUNT)
    {
        I_DEX = 0;
    }
    if (I_HUE > 255)
    {
        I_HUE = 0;
    }
    LEDS[I_DEX] = CHSV(I_HUE, FX_SAT, FX_BRIGHTNESS);
    LEDS.show();
    delay(FX_DELAY);
}

void RainbowLoop2()
{ //Rainbow Loop 2
    I_HUE -= 1;
    fill_rainbow(LEDS, LED_COUNT, I_HUE);
    LEDS.show();
    delay(FX_DELAY);
}

void RandomColour()
{ //Random Colour
    I_DEX = random(0, LED_COUNT);
    I_HUE = random(0, 255);
    LEDS[I_DEX] = CHSV(I_HUE, FX_SAT, FX_BRIGHTNESS);
    LEDS.show();
    delay(FX_DELAY);
}

void FadeColour()
{ //Fade One Colour
    if (BOUNCED_DIRECTION == 0)
    {
        I_BRIGHTNESS++;
        if (I_BRIGHTNESS >= 255)
        {
            BOUNCED_DIRECTION = 1;
        }
    }
    if (BOUNCED_DIRECTION == 1)
    {
        I_BRIGHTNESS = I_BRIGHTNESS - 1;
        if (I_BRIGHTNESS <= 1)
        {
            BOUNCED_DIRECTION = 0;
        }
    }
    for (int I_DEX = 0; I_DEX < LED_COUNT; I_DEX++)
    {
        LEDS[I_DEX] = CHSV(FX_HUE, FX_SAT, I_BRIGHTNESS);
    }
    LEDS.show();
    delay(FX_DELAY);
}

void MoveToCenter()
{ //Move To Center
    I_DEX++;
    if (I_DEX > TOP_INDEX)
    {
        I_DEX = 0;
    }
    int idexA = I_DEX;
    int idexB = horizontal_index(idexA);
    I_BRIGHTNESS = I_BRIGHTNESS + 10;
    if (I_BRIGHTNESS > 255)
    {
        I_BRIGHTNESS = 0;
    }
    LEDS[idexA] = CHSV(FX_HUE, FX_SAT, I_BRIGHTNESS);
    LEDS[idexB] = CHSV(FX_HUE, FX_SAT, I_BRIGHTNESS);
    LEDS.show();
    delay(FX_DELAY);
}

void RainbowMoveToCenter()
{ //Rainbow Move To Center
    I_DEX++;
    if (I_DEX > TOP_INDEX)
    {
        I_DEX = 0;
    }
    I_HUE = I_HUE + FX_STEP;
    if (I_HUE > 255)
    {
        I_HUE = 0;
    }
    int idexA = I_DEX;
    int idexB = horizontal_index(idexA);
    LEDS[idexA] = CHSV(I_HUE, FX_SAT, FX_BRIGHTNESS);
    LEDS[idexB] = CHSV(I_HUE, FX_SAT, FX_BRIGHTNESS);
    LEDS.show();
    delay(FX_DELAY);
}

void RandomStrobe()
{ //Random Strobe
    int temprand;
    for (int i = 0; i < LED_COUNT; i++)
    {
        temprand = random(0, 100);
        if (temprand > 50)
        {
            LEDS[i].r = 255;
        }
        if (temprand <= 50)
        {
            LEDS[i].r = 0;
        }
        LEDS[i].b = 0;
        LEDS[i].g = 0;
    }
    LEDS.show();
}

void RandomRainbowStrobe()
{ //Random Rainbow Strobe
    I_DEX = random(0, LED_COUNT);
    I_HUE = random(0, 255);
    FastLED.clear(); // clear all pixel data
    LEDS[I_DEX] = CHSV(I_HUE, FX_SAT, FX_BRIGHTNESS);
    LEDS.show();
    delay(FX_DELAY);
}
