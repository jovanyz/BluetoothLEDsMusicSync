#include <Adafruit_NeoPixel.h>
#include <math.h>

#ifdef __AVR__
#include <avr/power.h>
#endif

#include <BlynkSimpleCurieBLE.h>
#include <CurieBLE.h>
#include <Wire.h>
#include <avr/pgmspace.h>

//-------------------------------------------------------------------USER INPUTS-------------------------------------------------------------------
// For BLYNK
char auth[] = "???";                                  // What is your Auth Token from Blynk?
char deviceName[] = "BlueToothLEDs";                  // What do you want your Arduino's Bluetooth name to be?

// For Logistics
const int pinNum = 6;
const int audPin = A0;
const double stripLen = ???.0;                        // How many LEDs are you trying to run?

// For Rainbow Pattern [M1]
const int numRainbow = ???.0;                         // (mode 1) How many rainbows do you want in your strip?

// For Color Wipe Pattern [M2]
const int seed = ???;                                 // (mode 2) What pixel number do you want your color wipe to start?

// For Asymmetric Fade Patterns (Lava [M4], Canopy[M5], Ocean[M6], Color Wave[M7])
const double modif = ???;                             // If your stripLen is not easily divisble, enter a close but more divisible number here
const double divBy = ???;                             // How many pixels do you want each guidepoint to control?
const int pts = ??? + 1;                              // How many guide points do you want?, (if you did this correctly, diBy*pts = modif)

// For Fireflies Pattern [M8]
const int numFiFl = ???;                              // What is the maximum number of "fireflies" you want?

// For Confetti Pattern [M9]
const int numConf = ???;                              // What is the number of colored speckles you want?

//Music Sync [M12] ***MIC REQUIRED***
int root1PixNum = ???;                                // Where do you want your music sync pattern to start? (e.g. at opposite corners of a room)
int root2PixNum = ???;                                // Pick another point?  

//-----------------------------------------------------------------END USER INPUTS-----------------------------------------------------------------


// **************************************** WARNING ************ CHANGE CODE BEYOND HERE AT YOUR OWN RISK! ****************************************


//---------------------------------------------------------------------CONTENTS--------------------------------------------------------------------
/*
 * 1. SETUP - START OF NEOPIXEL CLASS.............(L: 74-230)
 * 2. PATTERNS....................................(235-923)
 *  2.1 Rainbow...................................(236-257)
 *  2.2 Color Wipe................................(258-281)
 *  2.3 Color Fade................................(282-308)
 *  2.4 Lava......................................(309-398)
 *  2.5 Canopy....................................(399-487)
 *  2.6 Ocean.....................................(488-576)
 *  2.7 Color Wave................................(577-667)
 *  2.8 Fireflies.................................(668-738)
 *  2.9 Confetti..................................(739-767)
 *  2.10 Comet....................................(777-821)
 *  2.11 Sunset...................................(823-855)
 *  2.12 Music Sync (*mic required)...............(856-905)
 *  2.13 Pixel Finder.............................(906-923)
 * 3. PATTERN TOOLS...............................(928-1031)
 * 4. ARDUINO MAIN LOOP -- END OF CLASS...........(1036-1080)
 * 5. PULLING INFO FROM BLYNK ....................(1085-1296)
 * 6. COMPLETION ROUTINES.........................(1301-1320)
 */


// 1. SETUP-----------------------------------------------------------------------------------------------------------------------------------------

// BLYNK setup
#define BLYNK_PRINT Serial
BLEPeripheral  blePeripheral;

// Internal Command Log
int cmdArr[8];                       // array of commands {0-[ON/OFF]; 1-[Mode Number]; 2,3,4-[RGB]; 5-[Brightness]; ...
                                     //                    ... 6-[Animation Speed (Not Currently Used)]; 7-[Warm White]}
// Overall logistics
uint32_t col1;                       // define a random color (needed in some patterns)
const int totLen = double(stripLen);

// For Asymmetric Fade Patterns [M4], [M5], [M6], [M7]
int fadeFromArr[pts + 1][3];       // 3 for RGB Values
int fadeArr[pts + 1][3];           // fadeFrom is starting configuration ... 
int fadeToArr [pts + 1][3];        // ... fadeTo, is ending configuration
int count = 1;                       // used to track overall colorshift in ColorWave[M7]

// For Fireflies Pattern [M8] 
int stripArr[numFiFl][2];            // [2] because: first datapoint is location, second is direction (right, left, stay)

// For Comet Pattern [M10]
int cometDir;                        // direction: forwards of backwards
int cometStrt;                       // starting pixel
int cometWheel;                      // color

// For BLYNK App Mode Display
WidgetLCD lcd(V6);                   // define a virtual output for the lcd display


// Pattern Types (any new pattern must be added here):
enum  pattern { NONE, RAINBOW, COLOR_WIPE, FADE, LAVA, CANOPY, OCEAN, COLOR_WAVE, FIREFLIES, CONFETTI, COMET, SUNSET, MUSIC_SYNC, PIXEL_FINDER};
// Patern directions supported:
enum  direction { FORWARD, REVERSE };

// NeoPattern Class - Derived from the Adafruit_NeoPixel Class
class NeoPatterns : public Adafruit_NeoPixel
{
  public:
      // Each Pattern may Have These Charachteriztics
    pattern   ActivePattern;  // current pattern
    direction Direction;      // pattern direction (ALL CURRENT PATTERNS MOVE STRICTLTY FORWARD)

    signed long Interval;     // milliseconds between updates
    signed long lastUpdate;   // last update 

    uint32_t Color1, Color2;  // colors in use
    uint16_t TotalSteps;      // total number of steps in a pattern
    uint16_t Index;           // current step within the pattern

    void (*OnComplete)();     // Callback on completion of pattern

    // Constructor: Initializes the Strip
    NeoPatterns(uint16_t pixels, uint8_t pin, uint8_t type, void (*callback)())
      : Adafruit_NeoPixel(pixels, pin, type)
    {
      OnComplete = callback;
    }

    // Calls to Update the Pattern Step by Step
    void Update()
    {
      if ((millis() - lastUpdate) > Interval)    // How Long Each Step is Shown
      {
        lastUpdate = millis();
        switch (ActivePattern)
        {
          case RAINBOW:
            RainbowUpdate();
            break;
          case COLOR_WIPE:
            ColorWipeUpdate();
            break;
          case FADE:
            FadeUpdate();
            break;
          case LAVA:
            LavaUpdate();
            break;
          case CANOPY:
            CanopyUpdate();
            break;
          case OCEAN:
            OceanUpdate();
            break;
          case COLOR_WAVE:
            ColorWaveUpdate();
            break;
          case FIREFLIES:
            FirefliesUpdate();
            break;
          case CONFETTI:
            ConfettiUpdate();
            break;
          case COMET:
            CometUpdate();
            break;
          case SUNSET:
            SunsetUpdate();
            break;
          case MUSIC_SYNC:
            MusicSyncUpdate();
            break;
          case PIXEL_FINDER:
            PixelFinderUpdate();
            break;
          default:
            break;
        }
      }
    }

    // Increment Keeps Track of the Step Number
    void Increment()
    {
      if (Direction == FORWARD)
      {
        Index++;
        if (Index >= TotalSteps) 
        {
          Index = 0;
          if (OnComplete != NULL)
          {
            OnComplete();              // Method gives command when pattern finishes it's cycle (usually repeat)
          }
        }
      }
      else                             // If the Pattern is traveling in reverse the reset count is 0 and you count down. 
      {
        --Index;
        if (Index <= 0)
        {
          Index = TotalSteps - 1;
          if (OnComplete != NULL)
          {
            OnComplete(); 
          }
        }
      }
    }

    // Reverse Pattern Direction
    void Reverse()                     // Not currently used, but can be implemented as a call by 'OnComplete'
    {
      if (Direction == FORWARD)
      {
        Direction = REVERSE;
        Index = TotalSteps - 1;
      }
      else
      {
        Direction = FORWARD;
        Index = 0;
      }
    }





// 2. PATTERNS----------------------------------------------------------------------------------------------------------------------------------------
// 2.1 Rainbow--------------------------------------------------------------
    // Initialize for a RainbowCycle
    void Rainbow(uint8_t interval, direction dir = FORWARD)
    {
      ActivePattern = RAINBOW;
      Interval = interval;
      TotalSteps = 255;
      Index = 0;
      Direction = dir;
    }

    // Update the Rainbow Pattern
    void RainbowUpdate()
    {
      for (int i = 0; i < numPixels(); i++)
      {
          SetPixelColorAdj(i, DimColor(Wheel(int((double(i) * 256.0 / (stripLen / numRainbow)) + double(Index)) & 255), cmdArr[5]));
      }
      show();
      Increment();
    }
    
// 2.2 ColorWipe--------------------------------------------------------------
    // Initialize for a ColorWipe
    void ColorWipe(uint32_t color1, uint32_t color2, uint8_t interval, direction dir = FORWARD)
    {
      ActivePattern = COLOR_WIPE;
      Interval = interval;
      TotalSteps = stripLen + 1;
      Color1 = color2;
      ColorSet(col1);
      Index = 0;
    }

    // Update the Color Wipe Pattern
    void ColorWipeUpdate()
    {
      int loc1 = seed + Index;
      if (loc1 > stripLen) {
        loc1 -= stripLen + 1;
      }
      SetPixelColorAdj(loc1, DimColor(DimColor(Color1, cmdArr[5]), cmdArr[5]));
      show();
      Increment();
    }
    
// 2.3 Fade--------------------------------------------------------------
    // Initialize for a Uniform Fade
    void Fade(uint32_t color1, uint32_t color2, uint16_t steps, uint8_t interval, direction dir = FORWARD)
    {
      ActivePattern = FADE;
      Interval = interval;
      TotalSteps = steps;
      Color1 = color1;
      Color2 = color2;
      Index = 0;
      Direction = dir;
    }

    // Update the Fade Pattern
    void FadeUpdate()
    {
      // Calculate linear interpolation between Color1 and Color2
      // Optimise order of operations to minimize truncation error
      uint8_t red = ((Red(Color1) * (TotalSteps - Index)) + (Red(Color2) * Index)) / TotalSteps;
      uint8_t green = ((Green(Color1) * (TotalSteps - Index)) + (Green(Color2) * Index)) / TotalSteps;
      uint8_t blue = ((Blue(Color1) * (TotalSteps - Index)) + (Blue(Color2) * Index)) / TotalSteps;

      ColorSet(DimColor(Color(red, green, blue), cmdArr[5]));
      show();
      Increment();
    }
    
// 2.4 Lava--------------------------------------------------------------
    // Initialize for Lava
    void Lava(uint16_t steps, uint8_t interval)
    {
      ActivePattern = LAVA;
      Interval = interval;
      TotalSteps = steps;
      Index = 0;
      for (int i = 0; i < pts; i++)
      {
        double randBrightness = double(random(5, 100)) / 100.0;   // random dimmer start array
        double randBrightnessTo = double(random(5, 100)) / 100.0; // random dimmer end array

        fadeFromArr[i][0] = int(double(random(180, 255)) * randBrightness); //red
        fadeFromArr[i][1] = int(double(random(20, 75)) * randBrightness);   // green
        fadeFromArr[i][2] = 0;                                              // blue

        fadeArr[i][0] = fadeFromArr[i][0];
        fadeArr[i][1] = fadeFromArr[i][1];
        fadeArr[i][2] = fadeFromArr[i][2];

        fadeToArr[i][0] = int(double(random(180, 255)) * randBrightness); // red
        fadeToArr[i][1] = int(double(random(20, 75)) * randBrightness);   // green
        fadeToArr[i][2] = 0;                                              // blue
      }
      fadeFromArr[pts][0] = fadeFromArr[0][0]; // red is same as first
      fadeFromArr[pts][1] = fadeFromArr[0][1]; // green is same as first
      fadeFromArr[pts][2] = fadeFromArr[0][2]; // blue is same as first

      fadeArr[pts][0] = fadeFromArr[pts][0];
      fadeArr[pts][1] = fadeFromArr[pts][1];
      fadeArr[pts][2] = fadeFromArr[pts][2];

      fadeToArr[pts][0] = fadeToArr[0][0]; // red is same as first
      fadeToArr[pts][1] = fadeToArr[0][1]; // green is same as first
      fadeToArr[pts][2] = fadeToArr[0][2]; // blue is same as first
    }

    // Update the Lava Pattern
    void LavaUpdate()
    {
      double pixR = stripLen / (modif / divBy);
      for (int j = 0; j < divBy; j++)
      {
        for (int i = 0; i < pts; i++)
        {
          uint8_t red = ((fadeArr[i][0] * (pixR - j)) + (fadeArr[i + 1][0] * j)) / pixR;
          uint8_t green = ((fadeArr[i][1] * (pixR - j)) + (fadeArr[i + 1][1] * j)) / pixR;
          uint8_t blue = ((fadeArr[i][2] * (pixR - j)) + (fadeArr[i + 1][2] * j)) / pixR;
          SetPixelColorAdj(i * pixR + j, DimColor(Color(red, green, blue), cmdArr[5]));
        }
        uint8_t red1 = ((fadeArr[pts][0] * (pixR - j)) + (fadeArr[0][0] * j)) / pixR;
        uint8_t green1 = ((fadeArr[pts][1] * (pixR - j)) + (fadeArr[0][1] * j)) / pixR;
        uint8_t blue1 = ((fadeArr[pts][2] * (pixR - j)) + (fadeArr[0][2] * j)) / pixR;
        SetPixelColorAdj((pts - 1) * pixR + j, DimColor(Color(red1, green1, blue1), cmdArr[5]));
      }
      show();
      Increment();
      
      //timestep
      if (TotalSteps == Index + 1)
      {
        for (int i = 0; i < pts + 1; i++)
        {
          fadeFromArr[i][0] = fadeToArr[i][0];
          fadeFromArr[i][1] = fadeToArr[i][1];
          fadeFromArr[i][2] = fadeToArr[i][2];
        }
        for (int i = 0; i < pts; i++)
        {
          double randBrightnessTo = double(random(5, 100)) / 100.0;

          fadeToArr[i][0] = int(double(random(180, 255)) * randBrightnessTo); // red
          fadeToArr[i][1] = int(double(random(20, 75)) * randBrightnessTo); // green
          fadeToArr[i][2] = 0; // blue
        }
        fadeToArr[pts][0] = fadeToArr[0][0]; // red is same as first
        fadeToArr[pts][1] = fadeToArr[0][1]; // green is same as first
        fadeToArr[pts][2] = fadeToArr[0][2]; // blue is same as first
      }
      else {
        for (int i = 0; i < pts + 1; i++)//ptsL2 + pts; i++)
        {
          fadeArr[i][0] = ((fadeFromArr[i][0] * (TotalSteps - Index)) + (fadeToArr[i][0] * Index)) / TotalSteps;
          fadeArr[i][1] = ((fadeFromArr[i][1] * (TotalSteps - Index)) + (fadeToArr[i][1] * Index)) / TotalSteps;
          fadeArr[i][2] = ((fadeFromArr[i][2] * (TotalSteps - Index)) + (fadeToArr[i][2] * Index)) / TotalSteps;
        }
      }
    }

// 2.5 Canopy--------------------------------------------------------------
    // Initialize for Canopy
    void Canopy(uint16_t steps, uint8_t interval) // Similar to Lava
    {
      ActivePattern = CANOPY;
      Interval = interval;
      TotalSteps = steps;
      Index = 0;
      for (int i = 0; i < pts; i++)
      {
        double randBrightness = double(random(30, 100)) / 100.0;
        double randBrightnessTo = double(random(30, 100)) / 100.0;

        fadeFromArr[i][0] = int(double(random(0, 180)) * randBrightness);   //red
        fadeFromArr[i][1] = int(double(random(180, 255)) * randBrightness); // green
        fadeFromArr[i][2] = int(double(random(0, 50)) * randBrightness);    // blue

        fadeArr[i][0] = fadeFromArr[i][0];
        fadeArr[i][1] = fadeFromArr[i][1];
        fadeArr[i][2] = fadeFromArr[i][2];

        fadeToArr[i][0] = int(double(random(0, 180)) * randBrightnessTo);   // red
        fadeToArr[i][1] = int(double(random(180, 255)) * randBrightnessTo); // green
        fadeToArr[i][2] = int(double(random(0, 50)) * randBrightnessTo);    // blue
      }
      fadeFromArr[pts][0] = fadeFromArr[0][0]; // red is same as first
      fadeFromArr[pts][1] = fadeFromArr[0][1]; // green is same as first
      fadeFromArr[pts][2] = fadeFromArr[0][2]; // blue is same as first

      fadeArr[pts][0] = fadeFromArr[pts][0];
      fadeArr[pts][1] = fadeFromArr[pts][1];
      fadeArr[pts][2] = fadeFromArr[pts][2];

      fadeToArr[pts][0] = fadeToArr[0][0]; // red is same as first
      fadeToArr[pts][1] = fadeToArr[0][1]; // green is same as first
      fadeToArr[pts][2] = fadeToArr[0][2]; // blue is same as first
    }

    // Update the Canopy Pattern
    void CanopyUpdate()
    {
      double pixR = stripLen / (modif / divBy);
      for (int j = 0; j < divBy; j++)
      {
        for (int i = 0; i < pts; i++)
        {
          uint8_t red = ((fadeArr[i][0] * (pixR - j)) + (fadeArr[i + 1][0] * j)) / pixR;
          uint8_t green = ((fadeArr[i][1] * (pixR - j)) + (fadeArr[i + 1][1] * j)) / pixR;
          uint8_t blue = ((fadeArr[i][2] * (pixR - j)) + (fadeArr[i + 1][2] * j)) / pixR;
          SetPixelColorAdj(i * pixR + j, DimColor(Color(red, green, blue), cmdArr[5]));
        }
        uint8_t red1 = ((fadeArr[pts][0] * (pixR - j)) + (fadeArr[0][0] * j)) / pixR;
        uint8_t green1 = ((fadeArr[pts][1] * (pixR - j)) + (fadeArr[0][1] * j)) / pixR;
        uint8_t blue1 = ((fadeArr[pts][2] * (pixR - j)) + (fadeArr[0][2] * j)) / pixR;
        SetPixelColorAdj((pts - 1) * pixR + j, DimColor(Color(red1, green1, blue1), cmdArr[5]));
      }
      show();
      Increment();

      if (TotalSteps == Index + 1)
      {
        for (int i = 0; i < pts + 1; i++)
        {
          fadeFromArr[i][0] = fadeToArr[i][0];
          fadeFromArr[i][1] = fadeToArr[i][1];
          fadeFromArr[i][2] = fadeToArr[i][2];
        }
        for (int i = 0; i < pts; i++)
        {
          double randBrightnessTo = double(random(30, 100)) / 100.0;

          fadeToArr[i][0] = int(double(random(0, 180)) * randBrightnessTo);   // red
          fadeToArr[i][1] = int(double(random(180, 255)) * randBrightnessTo); // green
          fadeToArr[i][2] = int(double(random(0, 50)) * randBrightnessTo);    // blue
        }
        fadeToArr[pts][0] = fadeToArr[0][0]; // red is same as first
        fadeToArr[pts][1] = fadeToArr[0][1]; // green is same as first
        fadeToArr[pts][2] = fadeToArr[0][2]; // blue is same as first
      }
      else {
        for (int i = 0; i < pts + 1; i++) //ptsL2 + pts; i++)
        {
          fadeArr[i][0] = ((fadeFromArr[i][0] * (TotalSteps - Index)) + (fadeToArr[i][0] * Index)) / TotalSteps;
          fadeArr[i][1] = ((fadeFromArr[i][1] * (TotalSteps - Index)) + (fadeToArr[i][1] * Index)) / TotalSteps;
          fadeArr[i][2] = ((fadeFromArr[i][2] * (TotalSteps - Index)) + (fadeToArr[i][2] * Index)) / TotalSteps;
        }
      }
    }

// 2.6 Ocean--------------------------------------------------------------
    // Initialize for Ocean
    void Ocean(uint16_t steps, uint8_t interval) //Similar to Lava
    {
      ActivePattern = OCEAN;
      Interval = interval;
      TotalSteps = steps;
      Index = 0;
      for (int i = 0; i < pts; i++)
      {
        double randBrightness = double(random(10, 100)) / 100.0;
        double randBrightnessTo = double(random(10, 100)) / 100.0;

        fadeFromArr[i][0] = int(double(random(0, 150)) * randBrightness);   //red
        fadeFromArr[i][1] = int(double(random(0, 180)) * randBrightness);   // green
        fadeFromArr[i][2] = int(double(random(120, 255)) * randBrightness); // blue

        fadeArr[i][0] = fadeFromArr[i][0];
        fadeArr[i][1] = fadeFromArr[i][1];
        fadeArr[i][2] = fadeFromArr[i][2];

        fadeToArr[i][0] = int(double(random(0, 150)) * randBrightnessTo);   // red
        fadeToArr[i][1] = int(double(random(0, 180)) * randBrightnessTo);   // green
        fadeToArr[i][2] = int(double(random(120, 255)) * randBrightnessTo); // blue
      }
      fadeFromArr[pts][0] = fadeFromArr[0][0]; // red is same as first
      fadeFromArr[pts][1] = fadeFromArr[0][1]; // green is same as first
      fadeFromArr[pts][2] = fadeFromArr[0][2]; // blue is same as first

      fadeArr[pts][0] = fadeFromArr[pts][0];
      fadeArr[pts][1] = fadeFromArr[pts][1];
      fadeArr[pts][2] = fadeFromArr[pts][2];

      fadeToArr[pts][0] = fadeToArr[0][0]; // red is same as first
      fadeToArr[pts][1] = fadeToArr[0][1]; // green is same as first
      fadeToArr[pts][2] = fadeToArr[0][2]; // blue is same as first
    }

    // Update the Ocean Pattern
    void OceanUpdate()
    {
      double pixR = stripLen / (modif / divBy);
      for (int j = 0; j < divBy; j++)
      {
        for (int i = 0; i < pts; i++)
        {
          uint8_t red = ((fadeArr[i][0] * (pixR - j)) + (fadeArr[i + 1][0] * j)) / pixR;
          uint8_t green = ((fadeArr[i][1] * (pixR - j)) + (fadeArr[i + 1][1] * j)) / pixR;
          uint8_t blue = ((fadeArr[i][2] * (pixR - j)) + (fadeArr[i + 1][2] * j)) / pixR;
          SetPixelColorAdj(i * pixR + j, DimColor(Color(red, green, blue), cmdArr[5]));
        }
        uint8_t red1 = ((fadeArr[pts][0] * (pixR - j)) + (fadeArr[0][0] * j)) / pixR;
        uint8_t green1 = ((fadeArr[pts][1] * (pixR - j)) + (fadeArr[0][1] * j)) / pixR;
        uint8_t blue1 = ((fadeArr[pts][2] * (pixR - j)) + (fadeArr[0][2] * j)) / pixR;
        SetPixelColorAdj((pts - 1) * pixR + j, DimColor(Color(red1, green1, blue1), cmdArr[5]));
      }
      show();
      Increment();

      if (TotalSteps == Index + 1)
      {
        for (int i = 0; i < pts + 1; i++)
        {
          fadeFromArr[i][0] = fadeToArr[i][0];
          fadeFromArr[i][1] = fadeToArr[i][1];
          fadeFromArr[i][2] = fadeToArr[i][2];
        }
        for (int i = 0; i < pts; i++)
        {
          double randBrightnessTo = double(random(10, 100)) / 100.0;

          fadeToArr[i][0] = int(double(random(0, 150)) * randBrightnessTo);   // red
          fadeToArr[i][1] = int(double(random(0, 180)) * randBrightnessTo);   // green
          fadeToArr[i][2] = int(double(random(120, 255)) * randBrightnessTo); // blue
        }
        fadeToArr[pts][0] = fadeToArr[0][0]; // red is same as first
        fadeToArr[pts][1] = fadeToArr[0][1]; // green is same as first
        fadeToArr[pts][2] = fadeToArr[0][2]; // blue is same as first
      }
      else {
        for (int i = 0; i < pts + 1; i++)//ptsL2 + pts; i++)
        {
          fadeArr[i][0] = ((fadeFromArr[i][0] * (TotalSteps - Index)) + (fadeToArr[i][0] * Index)) / TotalSteps;
          fadeArr[i][1] = ((fadeFromArr[i][1] * (TotalSteps - Index)) + (fadeToArr[i][1] * Index)) / TotalSteps;
          fadeArr[i][2] = ((fadeFromArr[i][2] * (TotalSteps - Index)) + (fadeToArr[i][2] * Index)) / TotalSteps;
        }
      }
    }

// 2.7 ColorWave--------------------------------------------------------------
// Initialize for Color Wave
    void ColorWave(uint16_t steps, uint8_t interval)
    {
      ActivePattern = COLOR_WAVE;
      Interval = interval;
      TotalSteps = steps;
      Index = 0;
      for (int i = 0; i < pts; i++)
      {
        double randBrightness = double(random(10, 100)) / 100.0;
        double randBrightnessTo = double(random(10, 100)) / 100.0;

        fadeFromArr[i][0] = int(double(Red(Wheel((0 + random(-30, 30)) % 256))) * randBrightness);   //red
        fadeFromArr[i][1] = int(double(Green(Wheel((0 + random(-30, 30)) % 256))) * randBrightness); // green
        fadeFromArr[i][2] = int(double(Blue(Wheel((0 + random(-30, 30)) % 256))) * randBrightness);  // blue

        fadeArr[i][0] = fadeFromArr[i][0];
        fadeArr[i][1] = fadeFromArr[i][1];
        fadeArr[i][2] = fadeFromArr[i][2];

        fadeToArr[i][0] = int(double(Red(Wheel((17 + random(-30, 30)) % 256))) * randBrightnessTo);   // red
        fadeToArr[i][1] = int(double(Green(Wheel((17 + random(-30, 30)) % 256))) * randBrightnessTo); // green
        fadeToArr[i][2] = int(double(Blue(Wheel((17 + random(-30, 30)) % 256))) * randBrightnessTo);  // blue
      }
      fadeFromArr[pts][0] = fadeFromArr[0][0]; // red is same as first
      fadeFromArr[pts][1] = fadeFromArr[0][1]; // green is same as first
      fadeFromArr[pts][2] = fadeFromArr[0][2]; // blue is same as first

      fadeArr[pts][0] = fadeFromArr[pts][0];
      fadeArr[pts][1] = fadeFromArr[pts][1];
      fadeArr[pts][2] = fadeFromArr[pts][2];

      fadeToArr[pts][0] = fadeToArr[0][0]; // red is same as first
      fadeToArr[pts][1] = fadeToArr[0][1]; // green is same as first
      fadeToArr[pts][2] = fadeToArr[0][2]; // blue is same as first
    }

    // Update the ColorWave Pattern
    void ColorWaveUpdate() // Similar to Lava
    {
      double pixR = stripLen / (modif / divBy);
      for (int j = 0; j < divBy; j++)
      {
        for (int i = 0; i < pts; i++)
        {
          uint8_t red = ((fadeArr[i][0] * (pixR - j)) + (fadeArr[i + 1][0] * j)) / pixR;
          uint8_t green = ((fadeArr[i][1] * (pixR - j)) + (fadeArr[i + 1][1] * j)) / pixR;
          uint8_t blue = ((fadeArr[i][2] * (pixR - j)) + (fadeArr[i + 1][2] * j)) / pixR;
          SetPixelColorAdj(i * pixR + j, DimColor(Color(red, green, blue), cmdArr[5]));
        }
        uint8_t red1 = ((fadeArr[pts][0] * (pixR - j)) + (fadeArr[0][0] * j)) / pixR;
        uint8_t green1 = ((fadeArr[pts][1] * (pixR - j)) + (fadeArr[0][1] * j)) / pixR;
        uint8_t blue1 = ((fadeArr[pts][2] * (pixR - j)) + (fadeArr[0][2] * j)) / pixR;
        SetPixelColorAdj((pts - 1) * pixR + j, DimColor(Color(red1, green1, blue1), cmdArr[5]));
      }
      show();
      Increment();

      if (TotalSteps == Index + 1)
      {
        count++;
        for (int i = 0; i < pts + 1; i++)
        {
          fadeFromArr[i][0] = fadeToArr[i][0];
          fadeFromArr[i][1] = fadeToArr[i][1];
          fadeFromArr[i][2] = fadeToArr[i][2];
        }
        for (int i = 0; i < pts; i++)
        {
          double randBrightnessTo = double(random(10, 100)) / 100.0;

          fadeToArr[i][0] = int(double(Red(Wheel((count * 17 + random(-30, 30)) % 256))) * randBrightnessTo); //red
          fadeToArr[i][1] = int(double(Green(Wheel((count * 17 + random(-30, 30)) % 256))) * randBrightnessTo); // green
          fadeToArr[i][2] = int(double(Blue(Wheel((count * 17 + random(-30, 30)) % 256))) * randBrightnessTo); // blue

        }
        fadeToArr[pts][0] = fadeToArr[0][0]; // red is same as first
        fadeToArr[pts][1] = fadeToArr[0][1]; // green is same as first
        fadeToArr[pts][2] = fadeToArr[0][2]; // blue is same as first
      }
      else {
        for (int i = 0; i < pts + 1; i++) //ptsL2 + pts; i++)
        {
          fadeArr[i][0] = ((fadeFromArr[i][0] * (TotalSteps - Index)) + (fadeToArr[i][0] * Index)) / TotalSteps;
          fadeArr[i][1] = ((fadeFromArr[i][1] * (TotalSteps - Index)) + (fadeToArr[i][1] * Index)) / TotalSteps;
          fadeArr[i][2] = ((fadeFromArr[i][2] * (TotalSteps - Index)) + (fadeToArr[i][2] * Index)) / TotalSteps;
        }
      }
    }
    
// 2.8 Fireflies--------------------------------------------------------------
    // Initialize for Fireflies
    void Fireflies(uint8_t interval)
    {
      ActivePattern = FIREFLIES;
      Interval = interval;
      TotalSteps = random(10, 50);
      Index = 0;
      for (int i = 0; i < numFiFl; i++) {
        stripArr[i][0] = random(0, totLen); // pick the location of fireflies
      }
      ColorSet(Color(0, 0, 0));
    }

    // Update the Fireflies Pattern
    void FirefliesUpdate()
    {
      for (int i = 0; i < numFiFl; i++) {
        stripArr[i][1] = random(0, 3);     // pick left, right, or stay
        if (stripArr[i][1] == 0) {         // left
          SetPixelColorAdj((stripArr[i][0] + 1) % (totLen + 1), DimColor(DimColor(DimColor(Color(66, 74, 19), random(0, 70)), 
                           int((1.0 - abs(double(Index - TotalSteps / 2) / double(TotalSteps / 2))) * 100.0)), cmdArr[5]));
          SetPixelColorAdj(stripArr[i][0], DimColor(DimColor(getPixelColor((stripArr[i][0] + 1) % (totLen + 1)), 30), cmdArr[5]));
          SetPixelColorAdj((stripArr[i][0] + 2) % (totLen + 1), Color(0, 0, 0));
          SetPixelColorAdj((stripArr[i][0] - 1) % (totLen + 1), Color(0, 0, 0));
          stripArr[i][0] = (stripArr[i][0] + 1) % (totLen + 1);
        } else if (stripArr[i][1] == 1) {  // stay
          SetPixelColorAdj((stripArr[i][0] - 1) % (totLen + 1), Color(0, 0, 0));
          SetPixelColorAdj(stripArr[i][0], DimColor(DimColor(DimColor(Color(66, 74, 19), random(0, 70)), 
                           int((1.0 - abs(double(Index - TotalSteps / 2) / double(TotalSteps / 2))) * 100.0)), cmdArr[5]));
          SetPixelColorAdj((stripArr[i][0] + 1) % (totLen + 1), Color(0, 0, 0));
        } else if (stripArr[i][1] == 2) {  // right
          SetPixelColorAdj((stripArr[i][0] - 1) % (totLen + 1), DimColor(DimColor(DimColor(Color(66, 74, 19), random(0, 70)), 
                           int((1.0 - abs(double(Index - TotalSteps / 2) / double(TotalSteps / 2))) * 100.0)), cmdArr[5]));
          SetPixelColorAdj(stripArr[i][0], DimColor(DimColor(getPixelColor((stripArr[i][0] - 1) % (totLen + 1)), 30), cmdArr[5]));
          SetPixelColorAdj((stripArr[i][0] - 2) % (totLen + 1), Color(0, 0, 0));
          SetPixelColorAdj((stripArr[i][0] + 1) % (totLen + 1), Color(0, 0, 0));
          stripArr[i][0] = (stripArr[i][0] - 1) % (totLen + 1);
        }
      }
      if (TotalSteps == Index + 1) {          // prepare the next display
        ColorSet(Color(0, 0, 0));
        for (int i = 0; i < numFiFl; i++) {
          stripArr[i][0] = random(0, totLen);
        }
        for (int i = 0; i < numFiFl; i++) {  // at the end of the last step, start the next cycle
          stripArr[i][1] = random(0, 3);
          if (stripArr[i][1] == 0) {
            SetPixelColorAdj((stripArr[i][0] + 1) % (totLen + 1), DimColor(DimColor(Color(66, 74, 19), random(0, 10)), cmdArr[5]));
            SetPixelColorAdj(stripArr[i][0], DimColor(DimColor(getPixelColor((stripArr[i][0] + 1) % (totLen + 1)), 30), cmdArr[5]));
            SetPixelColorAdj((stripArr[i][0] + 2) % (totLen + 1), Color(0, 0, 0));
            SetPixelColorAdj((stripArr[i][0] - 1) % (totLen + 1), Color(0, 0, 0));
            stripArr[i][0] = (stripArr[i][0] + 1) % (totLen + 1);
          } else if (stripArr[i][1] == 1) {
            SetPixelColorAdj((stripArr[i][0] - 2) % (totLen + 1), Color(0, 0, 0));
            SetPixelColorAdj(stripArr[i][0], DimColor(DimColor(Color(66, 74, 19), random(0, 10)), cmdArr[5]));
            SetPixelColorAdj((stripArr[i][0] + 1) % (totLen + 1), Color(0, 0, 0));
          } else if (stripArr[i][1] == 2) {
            SetPixelColorAdj((stripArr[i][0] - 1) % (totLen + 1), DimColor(DimColor(Color(66, 74, 19), random(0, 10)), cmdArr[5]));
            SetPixelColorAdj(stripArr[i][0], DimColor(DimColor(getPixelColor((stripArr[i][0] - 1) % (totLen + 1)), 30), cmdArr[5]));
            SetPixelColorAdj((stripArr[i][0] - 2) % (totLen + 1), Color(0, 0, 0));
            SetPixelColorAdj((stripArr[i][0] + 1) % (totLen + 1), Color(0, 0, 0));
            stripArr[i][0] = (stripArr[i][0] - 1) % (totLen + 1);
          }
        }
        TotalSteps = random(10, 50);           // pick number of steps for the fireflies
      }
      show();
      Increment();
    }

// 2.9 Confetti--------------------------------------------------------------
//Initialize for Confetti
    void Confetti(int interval, int steps)
    {
      ActivePattern = CONFETTI;
      Interval = interval;
      TotalSteps = steps;
      Index = 0;
      ColorSet(Color(0, 0, 0));
    }

//Update the Confetti Pattern
    void ConfettiUpdate()
    {
      for (int  i = 0; i < numConf/3; i++){
        setPixelColor(random(0, totLen), DimColor(Wheel(int((Index * 256 / TotalSteps)) % 256 + random(-20 ,20)), 
                      int(50.0*(1.0+sin(PI/36.0*double(Index))))));
        setPixelColor(random(0, totLen), DimColor(Wheel(int((Index * 256 / TotalSteps)) % 256 + random(-20 ,20)), 
                      int(50.0*(1.0+sin(PI/36.0*double(Index+24))))));
        setPixelColor(random(0, totLen), DimColor(Wheel(int((Index * 256 / TotalSteps)) % 256 + random(-20 ,20)),
                      int(50.0*(1.0+sin(PI/36.0*double(Index+48))))));
      }
      for (int  i = 0; i < totLen; i++){
        setPixelColor(i, DimColor(DimColor(getPixelColor(i), 90), cmdArr[5]));
      }
      show();
      Increment();
    }

// 2.10 Comet--------------------------------------------------------------
    // Initialize for Comet
    void Comet(uint32_t col1, uint8_t interval)
    {
      ActivePattern = COMET;
      Interval = interval;
      TotalSteps = 100;
      Index = 0;
      cometDir = random(0, 2);
      cometStrt = random(0, totLen);
      cometWheel = random(0, 255);
    }
    // Update the Comet Pattern
    void CometUpdate()
    {
      int loc;
      for (int i = 0; i < totLen; i++) {
        if (i == Index % totLen) {          // if chosen pixel  
            if (cometDir == 0) {            // if forward
              loc = i + cometStrt;          // go forward
              if (loc >= totLen) {
                loc -= int(stripLen);          // prevent out of bounds
              }
            } else if (cometDir == 1) {     // if go backward
              loc = cometStrt - i;          // go backward
              if (loc < 0) {
                loc += int(stripLen);          // prevent out of bounds
              }
            }
           SetPixelColorAdj(loc, DimColor(Wheel(cometWheel + random(0, 30) - 15), cmdArr[5]));
        }
        else // Fading tail
        {
          setPixelColor(i, DimColor(DimColor(getPixelColor(i), 50), cmdArr[5]));
        }
       
      }
      show();
      Increment();
      if (Index + 1 == TotalSteps) {
        cometWheel = random(0, 255);        // prepare next comet
        cometDir = random(0, 2);
        cometStrt = random(0, totLen);
      }
    }
    
// 2.11 Sunset--------------------------------------------------------------
    void Sunset(uint8_t interval, uint8_t steps)
    {
      ActivePattern = SUNSET;
      Interval = interval;
      TotalSteps = steps * 6;
      Index = 0;
    }
    void SunsetUpdate() {
      uint8_t red;
      uint8_t green;
      uint8_t blue;
      if (Index < (TotalSteps / 6)) {         // a fade to colors of the sunset over time
        red = ((200 * ((TotalSteps / 6) - (Index % (TotalSteps / 6)))) + (137 * (Index % (TotalSteps / 6)))) / (TotalSteps / 6);
        green = ((200 * ((TotalSteps / 6) - (Index % (TotalSteps / 6)))) + (46 * (Index % (TotalSteps / 6)))) / (TotalSteps / 6);
        blue = ((70 * ((TotalSteps / 6) - (Index % (TotalSteps / 6)))) + (30 * (Index % (TotalSteps / 6)))) / (TotalSteps / 6);
      } else if (Index < (TotalSteps / 6) * 2) {
        red = ((137 * ((TotalSteps / 6) - (Index % (TotalSteps / 6)))) + (119 * (Index % (TotalSteps / 6)))) / (TotalSteps / 6);
        green = ((46 * ((TotalSteps / 6) - (Index % (TotalSteps / 6)))) + (20 * (Index % (TotalSteps / 6)))) / (TotalSteps / 6);
        blue = ((30 * ((TotalSteps / 6) - (Index % (TotalSteps / 6)))) + (1 * (Index % (TotalSteps / 6)))) / (TotalSteps / 6);
      } else if (Index < (TotalSteps / 6) * 3) {
        red = ((119 * ((TotalSteps / 6) - (Index % (TotalSteps / 6)))) + (30 * (Index % (TotalSteps / 6)))) / (TotalSteps / 6);
        green = ((20 * ((TotalSteps / 6) - (Index % (TotalSteps / 6)))) + (0 * (Index % (TotalSteps / 6)))) / (TotalSteps / 6);
        blue = ((1 * ((TotalSteps / 6) - (Index % (TotalSteps / 6)))) + (16 * (Index % (TotalSteps / 6)))) / (TotalSteps / 6);
      } else if (Index < (TotalSteps / 6) * 4) {
        red = ((30 * ((TotalSteps / 6) - (Index % (TotalSteps / 6)))) + (0 * (Index % (TotalSteps / 6)))) / (TotalSteps / 6);
        green = ((0 * ((TotalSteps / 6) - (Index % (TotalSteps / 6)))) + (0 * (Index % (TotalSteps / 6)))) / (TotalSteps / 6);
        blue = ((16 * ((TotalSteps / 6) - (Index % (TotalSteps / 6)))) + (0 * (Index % (TotalSteps / 6)))) / (TotalSteps / 6);
      } else if (Index < (TotalSteps / 6) * 5) {
        red = 0;
        green = 0;
        blue = 0;
      } else {
        red = ((0 * ((TotalSteps / 6) - (Index % (TotalSteps / 6)))) + (200 * (Index % (TotalSteps / 6)))) / (TotalSteps / 6);
        green = ((0 * ((TotalSteps / 6) - (Index % (TotalSteps / 6)))) + (200 * (Index % (TotalSteps / 6)))) / (TotalSteps / 6);
        blue = ((0 * ((TotalSteps / 6) - (Index % (TotalSteps / 6)))) + (70 * (Index % (TotalSteps / 6)))) / (TotalSteps / 6);
      }
      ColorSet(DimColor(Color(red, green, blue), cmdArr[5]));
      show();
      Increment();
    }
    
// 2.12 Music Sync (*mic required)--------------------------------------------------------------
    void MusicSync(uint8_t interval, uint16_t steps)
    {
      ActivePattern = MUSIC_SYNC;
      Interval = interval;
      TotalSteps = steps;
      Index = 0;
      ColorSet(Color(0, 0, 0));
    }

    void MusicSyncUpdate()
    {
      double volt = audSample(30);                           // sample auido 30 ms
      if (volt > 0.15)                                       // if critical volume
      {
        for (int i = 0; i < int(stripLen); i++) //+ lenL2); i++)
        {
          setPixelColor(i, DimColor(getPixelColor(i), 50));  // slowly wipe the string
        }

        int len = int((double(cmdArr[5]) / 100.0) * pow(volt, 0.8) * ((stripLen - 1) / 2)); // function for fading tail length
        int num = int((volt - 0.15) * 2550.0) % 256;                              // Color distortion based on volume
        uint32_t seedColR = Wheel(int((Index * 256 / TotalSteps)) % 256); // get colors
        uint32_t seedColD = Wheel(int((Index * 256 / TotalSteps) + 128) % 256);
        uint32_t Col = Wheel(num);

        for (int i = 0; i < len / 2; i++ ) { //fades from one color to next and to black
          uint8_t red = (Red(seedColR) * (len / 2 - i) + (Red(Col) * i)) / (len / 2);
          uint8_t green = (Green(seedColR) * (len / 2 - i) + (Green(Col) * i)) / (len / 2);
          uint8_t blue = (Blue(seedColR) * (len / 2 - i) + (Blue(Col) * i)) / (len / 2);
          SetPixelColorAdj((i + root1PixNum) % int(stripLen), DimColor(Color(red, green, blue),
                           int(100.0 * (1.0 - double(i) / double(len / 2)))));
          SetPixelColorAdj((root1PixNum - 1 - i) % int(stripLen), DimColor(Color(red, green, blue), 
                           int(100.0 * (1.0 - double(i) / double(len / 2)))));
          SetPixelColorAdj((i + root2PixNum) % int(stripLen), DimColor(Color(red, green, blue), 
                           int(100.0 * (1.0 - double(i) / double(len / 2)))));
          SetPixelColorAdj((root2PixNum - 1 - i) % int(stripLen), DimColor(Color(red, green, blue), 
                           int(100.0 * (1.0 - double(i) / double(len / 2)))));
        }
      } else {                                                // if no noise detected
        for (int i = 0; i < int(stripLen); i++) //+ lenL2); i++)
        {
          setPixelColor(i, DimColor(getPixelColor(i), 80));   // wipe the strip slowly
        }
      }

      show();
      Increment();
    }

// 2.13 PixelFinder--------------------------------------------------------------
    void PixelFinder()
    {
      ActivePattern = PIXEL_FINDER;
      Interval = 10;
      TotalSteps = 1;
      Index = 0;
      ColorSet(Color(0, 0, 0));
    }
    
    void PixelFinderUpdate()
    {
      ColorSet(Color(0, 0, 0));
      SetPixelColorAdj(cmdArr[5], Color(0, 255, 0));
      show();
      Increment();
    }


    


//    3. PATTERN TOOLS---------------------------------------------------------------------------------------------------------------------------------------

    // Maps parts of the string so Loop 1 and Loop 2 are addressed intuitively
    void SetPixelColorAdj(uint16_t loc, uint32_t color) {
      setPixelColor(loc, color);
    }

    // Calculate percent dimmed version of a color
    uint32_t DimColor(uint32_t color, uint8_t percent)
    {
      uint32_t dimColor = Color(uint8_t(double(Red(color)) * (double(percent) / 100.0)), 
                                uint8_t(double(Green(color)) * (double(percent) / 100.0)), 
                                uint8_t(double(Blue(color)) * (double(percent) / 100.0)));
      return dimColor;
    }

    // Set all pixels to a color (synchronously)
    void ColorSet(uint32_t color)
    {
      for (int i = 0; i < numPixels(); i++)
      {
        SetPixelColorAdj(i, DimColor(color, cmdArr[5]));
      }
      show();
    }

    // Set all pixels in a range to a color
    void ColorSetRange(uint32_t color, uint16_t strt, uint16_t fnsh)
    {
      for (int i = strt; i < fnsh; i++)
      {
        SetPixelColorAdj(i, DimColor(color, cmdArr[5]));
      }
      show();
    }
    // Returns the Red component of a 32-bit color
    uint8_t Red(uint32_t color)
    {
      return (color >> 16) & 0xFF;
    }

    // Returns the Green component of a 32-bit color
    uint8_t Green(uint32_t color)
    {
      return (color >> 8) & 0xFF;
    }

    // Returns the Blue component of a 32-bit color
    uint8_t Blue(uint32_t color)
    {
      return color & 0xFF;
    }

    // Input a value 0 to 255 to get a color value.
    // The colours are a transition r - g - b - back to r.
    uint32_t Wheel(byte WheelPos)
    {
      WheelPos = 255 - WheelPos;
      if (WheelPos < 85)
      {
        return Color(255 - WheelPos * 3, 0, WheelPos * 3);
      }
      else if (WheelPos < 170)
      {
        WheelPos -= 85;
        return Color(0, WheelPos * 3, 255 - WheelPos * 3);
      }
      else
      {
        WheelPos -= 170;
        return Color(WheelPos * 3, 255 - WheelPos * 3, 0);
      }
    }

    // Returns Time Smoothed Audio Signals
    double audSample (int interval) {
      unsigned long startMillis = millis();
      int peakToPeak = 0;

      int signalMax = 0;
      int signalMin = 1024;

      while (millis() - startMillis < interval)
      {
        int sample = analogRead(0);
        if (sample < 1024)        // toss out spurious readings
        {
          if (sample > signalMax)
          {
            signalMax = sample;   // save just the max levels
          }
          else if (sample < signalMin)
          {
            signalMin = sample;  // save just the min levels
          }
        }
      }

      peakToPeak = signalMax - signalMin;        // max - min = peak-peak amplitude
      double volts = (peakToPeak * 5.0) / 1024;  // convert to volts
      return volts;
    }
};





// 4. END OF SET UP -- ARDUINO MAIN LOOP------------------------------------------------------------------------------------------------------------------

void StripComplete();

// Define some NeoPatterns for the two strip sections
NeoPatterns Strip = NeoPatterns(totLen, pinNum, NEO_GRB + NEO_KHZ800, &StripComplete);

// Initialize everything and prepare to start
void setup()
{
  pinMode(A0, INPUT);

  Serial.begin(9600);
  delay(1000);

  // The name your bluetooth service will show up as, customize this if you have multiple devices
  blePeripheral.setLocalName(deviceName);
  blePeripheral.setDeviceName(deviceName);
  blePeripheral.setAppearance(384);

  Blynk.begin(auth, blePeripheral);

  blePeripheral.begin();
  Serial.println("Waiting for connections...");

  Strip.Color2 = Strip.Wheel(random(255));
  Strip.begin();
  Strip.show();

  //set initial on commands
  cmdArr[1] = 1;
  cmdArr[2] = 160;
  cmdArr[3] = 160;
  cmdArr[4] = 160;
  cmdArr[5] = 100;
}

// Main loop
void loop()
{
  Strip.Update();
  Blynk.run();
  blePeripheral.poll();
}





// 5. PULLING INFO FROM BLYNK-----------------------------------------------------------------------------------------------------------------------------

//Sync Controller Values and Actual Values if Connected

// ON/OFF
BLYNK_WRITE(V0) {
  cmdArr[0] = param.asInt();              // assigning incoming value from pin V0 to a variable
  if (cmdArr[0]) {
    ControlPanel();
    Strip.ColorSet(Strip.DimColor(Strip.Color(255, 255, 255), cmdArr[5]));
    Blynk.virtualWrite(V8, "FULL WHITE"); // use: (position X: 0-15, position Y: 0-1, "Message you want to print")
    Blynk.virtualWrite(V9, "");
  } else if (cmdArr[0] == 0) {
    Strip.ActivePattern = NONE;
    Strip.ColorSet(Strip.Color(0, 0, 0));
    Blynk.virtualWrite(V8, "OFF");        // use: (position X: 0-15, position Y: 0-1, "Message you want to print")
    Blynk.virtualWrite(V9, "");
  }
}

// Mode Number from Menu
BLYNK_WRITE(V1)
{
  cmdArr[1] = param.asInt();              // assigning incoming value from pin V1 to a variable
  if (cmdArr[0] && cmdArr[1] != 1) {
    ControlPanel();
  } else if (cmdArr[0] && cmdArr[1] == 1){
    Strip.ActivePattern = NONE;
    Strip.ColorSet(Strip.DimColor(Strip.Color(160, 160, 160), cmdArr[5]));
  }
}

// Red from Color Picker
BLYNK_WRITE(V2)
{
  cmdArr[2] = param.asInt();            // assigning incoming value from pin V2 to a variable
  if (cmdArr[7] && cmdArr[0]) {
    Strip.ColorSet(Strip.DimColor(Strip.Color(cmdArr[2], cmdArr[3], cmdArr[4]), cmdArr[5]));
  }
}

// Green from Color Picker
BLYNK_WRITE(V3)
{
  cmdArr[3] = param.asInt();              // assigning incoming value from pin V3 to a variable
  if (cmdArr[7] && cmdArr[0]) {
    Strip.ColorSet(Strip.DimColor(Strip.Color(cmdArr[2], cmdArr[3], cmdArr[4]), cmdArr[5]));
  }
}

// Blue from Color Picker
BLYNK_WRITE(V4)
{
  cmdArr[4] = param.asInt();              // assigning incoming value from pin V4 to a variable
  if (cmdArr[7] && cmdArr[0]) {
    Strip.ColorSet(Strip.DimColor(Strip.Color(cmdArr[2], cmdArr[3], cmdArr[4]), cmdArr[5]));
  }
}

// Brightness Value From Slider
BLYNK_WRITE(V5)
{
  cmdArr[5] = param.asInt();              // assigning incoming value from pin V5 to a variable
  if (cmdArr[0]) {
    if (cmdArr[7] == 1 || cmdArr[1] == 1 ) {
      Strip.ColorSet(Strip.DimColor(Strip.Color(cmdArr[2], cmdArr[3], cmdArr[4]), cmdArr[5]));
    } else if (cmdArr[1] == 3) {
      // ------------ColorWipe [M2] has special dimming needs-----------------
      if (seed - Strip.Index < 0) {
        Strip.ColorSetRange(Strip.DimColor(Strip.Color1, cmdArr[5]), 0, seed + 1);
        Strip.ColorSetRange(Strip.DimColor(Strip.Color1, cmdArr[5]), int(stripLen)-(Strip.Index-seed), int(stripLen));
        Strip.ColorSetRange(Strip.DimColor(col1, cmdArr[5]), seed, (seed - Strip.Index)+int(stripLen));
      } else if (seed - Strip.Index >= 0) {
        Strip.ColorSetRange(Strip.DimColor(Strip.Color1, cmdArr[5]), seed - Strip.Index, seed + 1);
        Strip.ColorSetRange(Strip.DimColor(col1, cmdArr[5]), seed, int(stripLen));
        Strip.ColorSetRange(Strip.DimColor(col1, cmdArr[5]), 0, seed - Strip.Index);
      }
    }
  }
}

// Animation Speed (NOT USED currently)
BLYNK_WRITE(V6)
{
  cmdArr[6] = param.asInt(); // assigning incoming value from pin V1 to a variable
  if (cmdArr[0]) {
    Serial.print("here7");
  }
}

// Color Picker Button
BLYNK_WRITE(V7) {
  cmdArr[7] = param.asInt();
  pattern SavePttrn;
  if (cmdArr[0]) {
    if (cmdArr[7]) {
      Strip.ActivePattern = NONE;
      Strip.ColorSet(Strip.DimColor(Strip.Color(cmdArr[2], cmdArr[3], cmdArr[4]), cmdArr[5]));
      Blynk.virtualWrite(V8, "Color Picker:"); // use: (position X: 0-15, position Y: 0-1, "Message you want to print")
      Blynk.virtualWrite(V9, "Use Sliders");
    } else {
      Strip.ColorSet(Strip.DimColor(Strip.Color(160, 160, 160), cmdArr[5]));
      ControlPanel();
    }
  }
}

void ControlPanel() {   // translates comands to modes
  switch (int(cmdArr[1] - 1)) {
    case 0:
      {
        Strip.ActivePattern = NONE;
        Blynk.virtualWrite(V8, "Default Mode:");  // use: (position X: 0-15, position Y: 0-1, "Message you want to print")
        Blynk.virtualWrite(V9, "Full White");
        break;
    } case 1:
      {
        Strip.Rainbow(5);
        Blynk.virtualWrite(V8, "Mode 1:");  // use: (position X: 0-15, position Y: 0-1, "Message you want to print")
        Blynk.virtualWrite(V9, "Rainbow");
        break;
    } case 2:
      {
        col1 = Strip.Wheel(random(255));
        uint32_t col2;
        while (true) {
          uint32_t genColor = Strip.Wheel(random(255));
          if (abs(int(Strip.Red(genColor) - Strip.Red(col1))) > 50 
              || abs(int(Strip.Green(genColor) - Strip.Green(col1))) > 50
              || abs(int(Strip.Blue(genColor) - Strip.Blue(col1))) > 50) {
         
            if (int(Strip.Red(genColor) + Strip.Green(genColor) + Strip.Blue(genColor)) > 50) {
              col2 = genColor;
              break;
            }
          }
        }
        Strip.ColorWipe(col1, col2, 30);
        Blynk.virtualWrite(V8, "Mode 2:");  // use: (position X: 0-15, position Y: 0-1, "Message you want to print")
        Blynk.virtualWrite(V9, "Color Wipe");
        break;
    } case 3:
      {
        Strip.Fade(Strip.Color1, Strip.Color2, 50, 20);
        Blynk.virtualWrite(V8, "Mode 3:");  // use: (position X: 0-15, position Y: 0-1, "Message you want to print")
        Blynk.virtualWrite(V9, "Color Fade");
        break;
    } case 4:
      {
        Strip.Lava(20, 30);
        Blynk.virtualWrite(V8, "Mode 4:");  // use: (position X: 0-15, position Y: 0-1, "Message you want to print")
        Blynk.virtualWrite(V9, "Lava");
        break;
    } case 5:
      {
        Strip.Canopy(20, 80);
        Blynk.virtualWrite(V8, "Mode 5:");  // use: (position X: 0-15, position Y: 0-1, "Message you want to print")
        Blynk.virtualWrite(V9, "Canopy");
        break;
    } case 6:
      {
        Strip.Ocean(20, 50);
        Blynk.virtualWrite(V8, "Mode 6:");  // use: (position X: 0-15, position Y: 0-1, "Message you want to print")
        Blynk.virtualWrite(V9, "Ocean");
        break;
    } case 7:
      {
        Strip.ColorWave(20, 50);
        Blynk.virtualWrite(V8, "Mode 7:");  // use: (position X: 0-15, position Y: 0-1, "Message you want to print")
        Blynk.virtualWrite(V9, "Color Wave");
        break;
    } case 8:
      {
        Strip.Fireflies(80);
        Blynk.virtualWrite(V8, "Mode 8:");  // use: (position X: 0-15, position Y: 0-1, "Message you want to print")
        Blynk.virtualWrite(V9, "Fireflies");
        break;
    } case 9:
      {
        Strip.Confetti(80, 128);
        Blynk.virtualWrite(V8, "Mode 9:");  // use: (position X: 0-15, position Y: 0-1, "Message you want to print")
        Blynk.virtualWrite(V9, "Confetti");
        break;
    } case 10:
      {
        Strip.Comet(Strip.Color1, 40);
        Blynk.virtualWrite(V8, "Mode 10:");  // use: (position X: 0-15, position Y: 0-1, "Message you want to print")
        Blynk.virtualWrite(V9, "Comet");
        break;
    } case 11:
      {
        Strip.Sunset(20, 500);
        Blynk.virtualWrite(V8, "Mode 11:"); // use: (position X: 0-15, position Y: 0-1, "Message you want to print")
        Blynk.virtualWrite(V9, "Sunset");
        break;
    } case 12:
      {
        Strip.MusicSync(60, 100);
        Blynk.virtualWrite(V8, "Mode 12:"); // use: (position X: 0-15, position Y: 0-1, "Message you want to print")
        Blynk.virtualWrite(V9, "Music Sync");
        break;
    } case 13:
      {
        Strip.PixelFinder();
        Blynk.virtualWrite(V8, "Pix Fndr:"); // use: (position X: 0-15, position Y: 0-1, "Message you want to print")
        Blynk.virtualWrite(V9, "See Brghtnss");
    }
    default:
      break;
  }
}





// 6. COMPLETION ROUTINES-----------------------------------------------------------------------------------------------------------------------------

// Completion Callback
void StripComplete()
{
  uint32_t genColor;            // will store a new Color
  col1 = Strip.Color1;          // Saves the previous color
  Strip.Color1 = Strip.Color2;  // Sets current Color as startpoint
  while (true) {                // makes sure new color isn't same as current and sets to new variable genColor.
    genColor = Strip.Wheel(random(255));
    if (abs(int(Strip.Red(genColor) - Strip.Red(Strip.Color1))) > 50 || abs(int(Strip.Green(genColor) - Strip.Green(Strip.Color1))) > 50
        || abs(int(Strip.Blue(genColor) - Strip.Blue(Strip.Color1))) > 50) {
      if (int(Strip.Red(genColor) + Strip.Green(genColor) + Strip.Blue(genColor)) > 50) {
        Strip.Color2 = genColor;
        break;
      }
    }
  }
}

