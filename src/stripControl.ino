// stripControl.ino
// Arduino program to control RGB LED strips via Arduino PWM pins
// Adam Inskip, 12/06/2021

// pin definitions
#define TRIG_PIN A3   // SR04 Ultrasonic Sensor Trig
#define ECHO_PIN A2   // SR04 Ultrasonic Sensor Echo

#define RED_LED 5     // PWM pins
#define GREEN_LED 6   // PWM pins
#define BLUE_LED 3    // PWM pins

#define POWER_BTN 4
#define BTN2 0

#define LCD_RS 8
#define LCD_E 9
#define LCD_D4 10
#define LCD_D5 11
#define LCD_D6 12
#define LCD_D7 13

#define SW_pin 7 // digital pin connected to switch output
#define X_pin A1 // analog pin connected to X output
#define Y_pin A0 // analog pin connected to Y output

//includes 
#include "LiquidCrystal.h"
#include "SR04.h"

// set up ultrasonic sensor
SR04 sr04 = SR04(ECHO_PIN,TRIG_PIN);
long ultraDistance;                 // the distance read by the sensor
int paceLEDs = 20;                  // milliseconds between updating LED colors (20ms assumes 50Hz update. human perception ~50-90ms)
int ultraDelay = 500;               // time delay between ultrasonic sensor readings, s
float ultraLowerLimit = 1.0;        // lower limit of distance window to trigger ultrasonic slider, cm
float ultraUpperLimit = 20.0;       // upper limit of distance window to trigger ultrasonic slider, cm

// set up LCD screen
LiquidCrystal lcd(LCD_RS, LCD_E, LCD_D4, LCD_D5, LCD_D6, LCD_D7);
int LCDX = 0;                                         // cursor x position on screen
int LCDY = 0;                                         // cursor y position on screen 

// LED control
bool powerStat = 0;                                   // power status - either 0 off or 1 on, allow toggling
float brightness = 1.0;                               //brightness multiplier 0.1 - 1, default full
int rVal = 255;                                       // controls red level, 0-255
int gVal = 255;                                       // controls green level, 0-255
int bVal = 255;                                       // controls blue level, 0-255
enum fadejumps {OFF, FADE1, FADE2, JUMP1, JUMP2};     // all available fade or jump modes
fadejumps fadeJumpStatus = OFF;                       // active fade or jump mode
float fadeJumpSpeed = 0.5;                            // 0.1-1, applied as a multiplier to the fade/jump speeds

// menu navigation
int currentMenuScreen = 0;                            // the currently displayed menu screen
int totalMenuScreens = 5;                             // the total number of menu screens

// misc
unsigned long timeNow = 0;                            // for storing the current millis() time at any point needed
int buttonPressWindow = 500;                          // time window to discard double presses






// LED CONTROL -----------------------------------------------------------------
// sets the LEDs to the currently stored RGB & brightness levels
void setLEDS() {
  analogWrite(RED_LED, rVal*brightness);
  analogWrite(GREEN_LED, gVal*brightness);
  analogWrite(BLUE_LED, bVal*brightness);
}

// changes stored RGB values, then sets the LEDs to the new colour
void changeLEDs(int R, int G, int B) {
  rVal = R;
  gVal = G;
  bVal = B;
  setLEDS();
}

// provides control of LED brightness via ultrasonic slider between an lower-upper range, for min-max brightness
bool ultrasonicMode() {
  
  int activation = 1500;      // time required to hold hand between lower-upper to activate slider
  timeNow = millis();         // track current time
  while(millis() < timeNow + activation) {    // block for activation time. if hand falls out of lower-upper range, exit function
    ultraDistance = sr04.Distance();
    if (ultraDistance < ultraLowerLimit || ultraDistance > ultraUpperLimit) {
      return 0; // failure
    }
  }
  
  // if we made it this far, we activated the ultrasonic slider
  timeNow = millis();

  // read ultrasonic distance and convert to brightness as a linear percentage of the hand position between lower and upper limits
  long ultraDistanceNew;
  while(sr04.Distance() > ultraLowerLimit && sr04.Distance() < ultraUpperLimit) {
    ultraDistanceNew = sr04.Distance();
    Serial.print(ultraDistanceNew);
    Serial.print("cm    Brightness: ");
    brightness = abs((ultraDistanceNew-ultraLowerLimit)/(ultraUpperLimit-ultraLowerLimit));
    if (ultraDistanceNew < 5) {
      brightness = 0.0;
    }
    setLEDS();
    ultraDistance = ultraDistanceNew;
  }
  return 1; // success
}

// allows two buttons to be used to manipulate LEDs. One acts as an LED on/off switch, the other a hardcoded colour change.
void buttons() {

    // button 1 - power button 
    // on click, either turn on or off depending on current status
    if(digitalRead(POWER_BTN)==LOW)
    {
      timeNow = millis();
      if(powerStat==0)
      {
        brightness = 1;
        powerStat = 1;
        setLEDS();
      }
      else {
        brightness = 0;
        powerStat = 0;
        setLEDS();
      }
      while(millis() < timeNow + buttonPressWindow) {
        // triggers time-based delay until next button press can be resumed, does nothing
      }
    }

    // button 2 - Set pink colour
    if(digitalRead(BTN2)==LOW)
    {
      rVal = 200;
      gVal = 10;
      bVal = 255;
      brightness = 1.0;
      setLEDS();
    }
  }




// LCD printing  -----------------------------------------------------------------
// clears LCD, prints "RGB: RRR/GGG/BBB" on first line
void printColorsLCD() {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("RGB:");
  lcd.print(rVal);
  lcd.print("/");
  lcd.print(gVal);
  lcd.print("/");
  lcd.print(bVal);
}

// prints the currently stored menu screen to LCD
void printMenuLCD(int menuScreen) {
  lcd.clear();
  lcd.setCursor(0,0);
  switch (menuScreen) {
    case 0:
      lcd.print("RGB SCROLLER");
      break;
    case 1:
      lcd.print("PRESET COLOURS");
      break;
    case 2:
      lcd.print("FADE");
      break;
    case 3:
      lcd.print("JUMP");
      break;
    case 4:
      lcd.print("SPEED CONTROL");
      break;
    default:
      lcd.print("ERROR");
      break;
  }
  lcd.setCursor(0,1);
  lcd.print("<--  SELECT  -->");
  lcd.cursor();
  lcd.blink();
  LCDX = 5;
  LCDY = 1;
  lcd.setCursor(LCDX,LCDY);
}

// prints "Click to save" on the lower line of the LCD
void printClickToSave() {
  lcd.setCursor(0,1);
  lcd.print("Click to save");
  lcd.setCursor(LCDX,LCDY);
}





// MENUS ----------------------------------------------------------------------------
// displays current menu screen and permits navigation between screens
void menu() {
  int scrollDelay = 250;      // prevent double-scroll joystick reads
  
  // if joystick is clicked, select the current menu option
  if (!digitalRead(SW_pin)) {
    timeNow = millis();
    while(millis() < timeNow + scrollDelay) { /* delay to stop double click */
      }
    // choose active menu screen & display it
    switch (currentMenuScreen) {
      case 0: //RGB scroller
        colourChangeMode();
        printMenuLCD(currentMenuScreen);
        break;
      case 1: // 6 colour selector
        sevenColours();
        printMenuLCD(currentMenuScreen);
        break;
      case 2: //fade menu
        fadeMenu();
        break;
      case 3: //jump menu
        jumpMenu();
        break;
      case 4: //speed control
        speedMenu();
        break;
      default:
        //statements
        break;
    }
  }

  // menu navigation code for joystick left/right/up/down movements
  if(analogRead(X_pin) < 400) {   // moving right, increase option by 1 with wraparound to first screen
    timeNow = millis();
    while(millis() < timeNow + scrollDelay) { /* delay to stop double click */
    }
    currentMenuScreen = abs((currentMenuScreen + 1) % totalMenuScreens);
    printMenuLCD(currentMenuScreen);

  } else if (analogRead(X_pin) > 600) {    // moving left, decrease option by 1 with wraparound to last screen
    timeNow = millis();
    while(millis() < timeNow + scrollDelay) { /* delay to stop double click */
    }
    currentMenuScreen = abs((currentMenuScreen - 1 + totalMenuScreens) % totalMenuScreens);
    printMenuLCD(currentMenuScreen);
  }
} 

// provides a menu for adjusting the currently active fade mode
void fadeMenu() {
  // print menu
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("FADE: RGB | RGBW");
  lcd.setCursor(0,1);
  lcd.print("      BACK| OFF ");
  lcd.setCursor(0,0);
  lcd.blink();

  int scrollDelay = 250;
  int currentSelection = 0;
  
  // blocks, lets user select a menu option
  while(true) {
    if (!digitalRead(SW_pin)) {
      timeNow = millis();
      while(millis() < timeNow + scrollDelay) {/* delay to prevent reading double-click*/}
      switch (currentSelection) {
        case 0: //RGB
          fadeJumpStatus = FADE1;
          printMenuLCD(currentMenuScreen);
          return;
        case 1: //RGBW
          fadeJumpStatus = FADE2;
          printMenuLCD(currentMenuScreen);
          return;
        case 2: //BACK
          printMenuLCD(currentMenuScreen);
          return;
        case 3: //OFF
          fadeJumpStatus = OFF;
          printMenuLCD(currentMenuScreen);
          return;
      }
    }

    //navigation of options
    if(analogRead(X_pin) < 400) {     // moving right      
      timeNow = millis();
      while(millis() < timeNow + scrollDelay) {/* delay to prevent double click*/}
      switch(currentSelection) {
        case 0:
          currentSelection = 1;
          break;
        case 1:
          currentSelection = 0;
          break;
        case 2:
          currentSelection = 3;
          break;
        case 3:
          currentSelection = 2;
          break;
      }
    } else if (analogRead(X_pin) > 600) {     // moving left
      timeNow = millis();
      while(millis() < timeNow + scrollDelay) {/* delay to prevent double click*/}
      switch(currentSelection) {
        case 0:
          currentSelection = 1;
          break;
        case 1:
          currentSelection = 0;
          break;
        case 2:
          currentSelection = 3;
          break;
        case 3:
          currentSelection = 2;
          break;
      }
    } else if (analogRead(Y_pin) > 600) {     // moving down
      timeNow = millis();
      while(millis() < timeNow + scrollDelay) {/* delay to prevent double click*/}
      switch(currentSelection) {
        case 0:
          currentSelection = 2;
          break;
        case 1:
          currentSelection = 3;
          break;
        case 2:
          currentSelection = 0;
          break;
        case 3:
          currentSelection = 1;
          break;
      }
    } else if (analogRead(Y_pin) < 400) {       // moving up
      timeNow = millis();
      while(millis() < timeNow + scrollDelay) {/* delay to prevent double click*/}
      switch(currentSelection) {
        case 0:
          currentSelection = 2;
          break;
        case 1:
          currentSelection = 3;
          break;
        case 2:
          currentSelection = 0;
          break;
        case 3:
          currentSelection = 1;
          break;
      }
    }
  
    //print/move cursor
    switch(currentSelection) {
      case 0:
          lcd.setCursor(6,0);
          break;
        case 1:
          lcd.setCursor(12,0);
          break;
        case 2:
          lcd.setCursor(6,1);
          break;
        case 3:
          lcd.setCursor(12,1);
        default:
          break;
    }
  }
}

// provides a menu for adjusting the currently active jump mode
void jumpMenu() {
  // print menu
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("JUMP: RGB | RGBW");
  lcd.setCursor(0,1);
  lcd.print("      BACK| OFF ");
  lcd.setCursor(0,0);
  lcd.blink();

  int scrollDelay = 250;
  int currentSelection = 0;
  
  // blocks, lets user select a menu option
  while(true) {
    if (!digitalRead(SW_pin)) {
      timeNow = millis();
      while(millis() < timeNow + scrollDelay) {/* delay to prevent reading double-click*/}
      switch (currentSelection) {
        case 0: //RGB
          fadeJumpStatus = JUMP1;
          printMenuLCD(currentMenuScreen);
          return;
        case 1: //RGBW
          fadeJumpStatus = JUMP2;
          printMenuLCD(currentMenuScreen);
          return;
        case 2: //BACK
          printMenuLCD(currentMenuScreen);
          return;
        case 3: //OFF
          fadeJumpStatus = OFF;
          printMenuLCD(currentMenuScreen);
          return;
      }
    }

    //navigation of options
    if(analogRead(X_pin) < 400) {           // moving right
      timeNow = millis();
      while(millis() < timeNow + scrollDelay) {/* delay to prevent double click*/}
      switch(currentSelection) {
        case 0:
          currentSelection = 1;
          break;
        case 1:
          currentSelection = 0;
          break;
        case 2:
          currentSelection = 3;
          break;
        case 3:
          currentSelection = 2;
          break;
      }
    } else if (analogRead(X_pin) > 600) {             // moving left
      timeNow = millis();
      while(millis() < timeNow + scrollDelay) {/* delay to prevent double click*/}
      switch(currentSelection) {
        case 0:
          currentSelection = 1;
          break;
        case 1:
          currentSelection = 0;
          break;
        case 2:
          currentSelection = 3;
          break;
        case 3:
          currentSelection = 2;
          break;
      }
    } else if (analogRead(Y_pin) > 600) {         // moving down
      timeNow = millis();
      while(millis() < timeNow + scrollDelay) {/* delay to prevent double click*/}
      switch(currentSelection) {
        case 0:
          currentSelection = 2;
          break;
        case 1:
          currentSelection = 3;
          break;
        case 2:
          currentSelection = 0;
          break;
        case 3:
          currentSelection = 1;
          break;
      }
    } else if (analogRead(Y_pin) < 400) {           // moving up
      timeNow = millis();
      while(millis() < timeNow + scrollDelay) {/* delay to prevent double click*/}
      switch(currentSelection) {
        case 0:
          currentSelection = 2;
          break;
        case 1:
          currentSelection = 3;
          break;
        case 2:
          currentSelection = 0;
          break;
        case 3:
          currentSelection = 1;
          break;
      }
    }
  
    //print/move cursor
    switch(currentSelection) {
      case 0:
          lcd.setCursor(6,0);
          break;
        case 1:
          lcd.setCursor(12,0);
          break;
        case 2:
          lcd.setCursor(6,1);
          break;
        case 3:
          lcd.setCursor(12,1);
        default:
          break;
    }
  }
}

// provides a menu for selecting one of seven preset (hardcoded) colours
void sevenColours() {
  
  //print menu
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("1   2   3   4   ");
  lcd.setCursor(0,1);
  lcd.print("5   6   7   BACK");
  lcd.setCursor(0,0);
  lcd.blink();

  int scrollDelay = 250;
  int currentSelection = 0;
  
  while(true) {
    // if jojystick clicked, select chosen colour
    if (!digitalRead(SW_pin)) {
      timeNow = millis();
      while(millis() < timeNow + scrollDelay) {/* delay to prevent reading double-click*/}
      switch (currentSelection) {
        case 0: //red
          rVal = 255;
          gVal = 0;
          bVal = 0;
          setLEDS();
          break;
        case 1: //green
          rVal = 0;
          gVal = 255;
          bVal = 0;
          setLEDS();
          break;
        case 2: //blue
          rVal = 0;
          gVal = 0;
          bVal = 255;
          setLEDS();
          break;
        case 3: //white
          rVal = 255;
          gVal = 255;
          bVal = 255;
          setLEDS();
          break;
        case 4: // orange
          rVal = 255;
          gVal = 140;
          bVal = 100;
          setLEDS();
          break;
        case 5: //purple
          rVal = 166;
          gVal = 70;
          bVal = 255;
          setLEDS();
          break;
        case 6: //turquoise
          rVal = 60;
          gVal = 255;
          bVal = 255;
          setLEDS();
          break;
        case 7:
          //exit method
          return;
      }
    }

    //navigation of options
    if(analogRead(X_pin) < 400) {
      // moving right
      timeNow = millis();
      while(millis() < timeNow + scrollDelay) {/* delay to prevent double click*/}
      currentSelection = abs((currentSelection + 1) % 8);
    } else if (analogRead(X_pin) > 600) {
      // moving left
      timeNow = millis();
      while(millis() < timeNow + scrollDelay) {/* delay to prevent double click*/}
      currentSelection = abs((currentSelection - 1 + 8) % 8);
    } else if (analogRead(Y_pin) > 600) {
      // moving down
      timeNow = millis();
      while(millis() < timeNow + scrollDelay) {/* delay to prevent double click*/}
      currentSelection = abs((currentSelection + 4) % 8);
    } else if (analogRead(Y_pin) < 400) {
      // moving up
      timeNow = millis();
      while(millis() < timeNow + scrollDelay) {/* delay to prevent double click*/}
      currentSelection = abs((currentSelection - 4 + 8) % 8);
    }
  
    //print/move cursor
    switch(currentSelection) {
      case 0:
          lcd.setCursor(0,0);
          break;
        case 1:
          lcd.setCursor(4,0);
          break;
        case 2:
          lcd.setCursor(8,0);
          break;
        case 3:
          lcd.setCursor(12,0);
          break;
        case 4:
          lcd.setCursor(0,1);
          break;
        case 5:
          lcd.setCursor(4,1);
          break;
        case 6:
          lcd.setCursor(8,1);
          break;
        case 7:
          lcd.setCursor(12,1);
          break;
        default:
          break;
    }
  }  
}

// provides a menu for adjusting the speed of the fade and jump modes
void speedMenu() {
  
  // print menu
  lcd.cursor();
  lcd.blink();
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Speed 0-1: ");
  lcd.print(fadeJumpSpeed);
  lcd.setCursor(0,1);
  lcd.print("Click to save");
  
  int scrollDelay = 250;
  LCDX = 13;
  LCDY = 0;
  lcd.setCursor(LCDX,LCDY);

  // blocks and allows user to adjust speed before confirming to return to menu
  while(true) {

    // if joystick clicked, accept user's speed and return to menu 
    if (!digitalRead(SW_pin)) {
      timeNow = millis();
      while(millis() < timeNow + scrollDelay) {  /* delay to prevent reading double-click*/
      }
      printMenuLCD(currentMenuScreen);
      return;
    }

    if (analogRead(Y_pin) > 600) {    // moving down, decrease fadeJumpSpeed by 0.1
      timeNow = millis();
      if (fadeJumpSpeed > 0.1) {
        fadeJumpSpeed = fadeJumpSpeed - 0.1;
      }
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Speed 0-1: ");
      lcd.print(fadeJumpSpeed);
      lcd.setCursor(0,1);
      lcd.print("Click to save");
      lcd.setCursor(LCDX, LCDY);
      while(millis() < timeNow + scrollDelay) { /* delay to prevent reading double-click*/
      }  
    } else if (analogRead(Y_pin) < 400) {   // moving up, increase fadeJumpSpeed by 0.1
      timeNow = millis();
      if (fadeJumpSpeed < 1) {
        fadeJumpSpeed = fadeJumpSpeed + 0.1;
      }
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Speed 0-1: ");
      lcd.print(fadeJumpSpeed);
      lcd.setCursor(0,1);
      lcd.print("Click to save");
      lcd.setCursor(LCDX, LCDY);
      while(millis() < timeNow + scrollDelay) { /* delay to prevent reading double-click*/
      }
    }
  }
  return;
}

// provides a menu for adjusting R, G, B values individually to manipulate the LED colours
void colourChangeMode() {
  
  // print initial menu screen
  lcd.cursor();
  lcd.blink();
  printColorsLCD();
  lcd.setCursor(0,1);
  lcd.print("Click to save");

  int scrollDelay = 250; // prevent double scrolling menu (double reads of joystick)
  LCDX = 0;
  LCDY = 0;
  lcd.setCursor(LCDX,LCDY);

  // blocks and allows user to choose colours
  while(true) {

    // if joystick clicked, accept colours and return to menu
    if (!digitalRead(SW_pin)) {
      timeNow = millis();
      while(millis() < timeNow + scrollDelay) { /* delay to prevent reading double-click*/
      }
      break;
    }

    if(analogRead(X_pin) < 400) { // moving right, move between R/G/B selection
      timeNow = millis();
      if (LCDX < 2) {
        LCDX = LCDX + 1;
      } else {
        LCDX = 0;
      }
      lcd.setCursor(LCDX,LCDY);
      while(millis() < timeNow + scrollDelay) { /* delay to prevent reading double-click*/
      }
    } else if (analogRead(X_pin) > 600) { // moving left, move between R/G/B selection
      timeNow = millis();
      if (LCDX > 0) {
        LCDX = LCDX - 1;
      } else {
        LCDX = 2;
      }
      lcd.setCursor(LCDX,LCDY);
      while(millis() < timeNow + scrollDelay) { /* delay to prevent reading double-click*/
      }
    } else if (analogRead(Y_pin) > 600) { // moving down. whichever of R/G/B is highlighted, decrease by 10
      timeNow = millis();
      switch(LCDX) {
        case 0:
          if (rVal > 10) {
            rVal = rVal - 10;
          }
          setLEDS();
          printColorsLCD();
          printClickToSave();
          break;
        case 1:
          if (gVal > 10) {
            gVal = gVal - 10;
          }
          setLEDS();
          printColorsLCD();
          printClickToSave();
          break;
        case 2:
          if (bVal > 10) {
            bVal = bVal - 10;
          }
          setLEDS();
          printColorsLCD();
          printClickToSave();
          break;
      }
      lcd.setCursor(LCDX,LCDY);
      while(millis() < timeNow + scrollDelay) { /* delay to prevent reading double-click*/
      }
    } else if (analogRead(Y_pin) < 400) {  // moving up. whichever of R/G/B is highlighted, increase by 10
      timeNow = millis();
      switch(LCDX) {
        case 0:
          if (rVal < 246) {
            rVal = rVal + 10;
          }
          setLEDS();
          printColorsLCD();
          printClickToSave();
          break;
        case 1:
          if (gVal < 246) {
            gVal = gVal + 10;
          }
          setLEDS();
          printColorsLCD();
          printClickToSave();
          break;
        case 2:
          if (bVal < 246) {
            bVal = bVal + 10;
          }
          setLEDS();
          printColorsLCD();
          printClickToSave();
          break;
      }
      lcd.setCursor(LCDX,LCDY);
      while(millis() < timeNow + scrollDelay) { /* delay to prevent reading double-click*/
      }
    }
  
  }

}





// FADE MODES --------------------------------------------------------------------------------
// provides a non-blocking, time-based fade through R-G-B-W
void fadeRGBW(long period) {
  float time = 0;
  time = millis() % period;
  float R = 0;
  float G = 0;
  float B = 0;
  float freq = 2*PI/period;

  if (time >= 0 && time < period/4) {
    //first quarter
    R = 255;
    G = 128 + 127*sin(2*freq*(time + period/8));
    B = 128 + 127*sin(2*freq*(time + period/8));
  } else if (time >= period/4 && time < period/2) {
    //second quarter
    R = 128 + 127*sin(2*freq*(time - period/8));
    G = 128 + 127*sin(2*freq*(time + period/8));
    B = 0;
  } else if (time >= period/2 && time < 3*period/4) {
    //third quarter
    R = 0;
    G = 128 + 127*sin(2*freq*(time + period/8));
    B = 128 + 127*sin(2*freq*(time - period/8));
  } else if (time >= 3*period/4 && time < period) {
    //fourth quarter
    R = 128 + 127*sin(2*freq*(time + period/8));
    G = 128 + 127*sin(2*freq*(time + period/8));
    B = 255;
  } else {
    //should never enter here, but catch bugs
    Serial.println("Error occured in fadeRGBW");
  }
  changeLEDs(R,G,B);
  setLEDS();
}

// provides a non-blocking, time-based fade through R-G-B
void fadeRGB(long period) {
  float time = 0;
  time = millis() % period;
  float R = 0;
  float G = 0;
  float B = 0;
  float freq = 2*PI/period;
  
  if (time >= 0 && time < period/3) {
    //red to green
    R = 128 + 127*sin(3.0/2.0*freq*(time + period/6));
    G = 128 + 127*sin(3.0/2.0*freq*(time + period/2));
    B = 0;
  } else if (time >= period/3 && time < 2*period/3) {
    //green to blue
    R = 0;
    G = 128 + 127*sin(3.0/2.0*freq*(time + period/2));
    B = 128 + 127*sin(3.0/2.0*freq*(time + period/6));
  } else if (time >= 2*period/3 && time < period) {
    //blue to red
    R = 128 + 127*sin(3.0/2.0*freq*(time + period/2));
    G = 0;
    B = 128 + 127*sin(3.0/2.0*freq*(time + period/6));
  } else {
    //should never enter here, but catch bugs
    Serial.println("Error occured in fadeRGB");
  }

  changeLEDs(R,G,B);
  setLEDS();
}

// provides a non-blocking, time-based jump through R-G-B-W
void jumpRGBW(long period) {
  float time = 0;
  time = millis() % period;
  float R = 0;
  float G = 0;
  float B = 0;

  if (time >= 0 && time < period/4) {
    //first quarter
    R = 255;
    G = 0;
    B = 0;
  } else if (time >= period/4 && time < period/2) {
    //second quarter
    R = 0;
    G = 255;
    B = 0;
  } else if (time >= period/2 && time < 3*period/4) {
    //third quarter
    R = 0;
    G = 0;
    B = 255;
  } else if (time >= 3*period/4 && time < period) {
    //fourth quarter
    R = 255;
    G = 255;
    B = 255;
  } else {
    //should never enter here, but catch bugs
    Serial.println("Error occured in jumpRGBW");
  }

  changeLEDs(R,G,B);
  setLEDS();
}

// provides a non-blocking, time-jump fade through R-G-B
void jumpRGB(long period) {
  float time = 0;
  time = millis() % period;
  float R = 0;
  float G = 0;
  float B = 0;
  
  if (time >= 0 && time < period/3) {
    //red
    R = 255;
    G = 0;
    B = 0;
  } else if (time >= period/3 && time < 2*period/3) {
    //green
    R = 0;
    G = 255;
    B = 0;
  } else if (time >= 2*period/3 && time < period) {
    //blue
    R = 0;
    G = 0;
    B = 255;
  } else {
    //should never enter here, but catch bugs
    Serial.println("Error occured in jumpRGB");
  }

  changeLEDs(R,G,B);
  setLEDS();
}





// SETUP -------------------------------------------------------------------------------------------------
// runs once to initialize program
void setup() {

  Serial.begin(9600);

  // ------------- JOYSTICK SETUP ------------
  pinMode(SW_pin, INPUT);
  digitalWrite(SW_pin, HIGH);
  
  // ------------- LED SETUP -----------------
  //set up pins to output.
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(BLUE_LED, OUTPUT);

  // ------------ POWER & PRESET COLOUR BUTTON SETUP ----------
  pinMode(POWER_BTN, INPUT_PULLUP);
  pinMode(BTN2, INPUT_PULLUP);

  // -------------- LCD SETUP -----------------
  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  printMenuLCD(currentMenuScreen);
}





// LOOP ---------------------------------------------------------------------------------------------------
// runs repeatedly, controls arduino function
void loop() {
  
  // read ultrasonic sensor distance every interval & check if user is activating ultrasonic slider
  if (millis() > ultraDelay) {
    ultraDistance=sr04.Distance();
    if (ultraDistance > ultraLowerLimit && ultraDistance < ultraUpperLimit) {
      ultrasonicMode();
      printMenuLCD(currentMenuScreen);
    }
  }
  
  // prints current menu & allows user navigation of full program functionality
  menu();
  
  // checks for button presses & executes actions if pressed
  buttons();
  
  // calculate and output current colour if a fade or jump mode is active
  switch (fadeJumpStatus) {
    case FADE1:
      fadeRGB(900/fadeJumpSpeed);
      break;
    case FADE2:
      fadeRGBW(800/fadeJumpSpeed);
      break;
    case JUMP1:
      jumpRGB(900/fadeJumpSpeed);
      break;
    case JUMP2:
      jumpRGBW(800/fadeJumpSpeed);
      break;
    case OFF:
      break;
    default:
      break;
  }

}
