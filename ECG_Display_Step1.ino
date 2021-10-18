//**************************
// *** Include Files ***
//**************************
#include <SPI.h>
#include "Adafruit_GFX.h"
#include "Adafruit_HX8357.h"
#include <SD.h>

//**************************
// *** Display Defines ***
//**************************
// Display Setup
#define TFT_CS 10
#define TFT_DC 9
#define TFT_RST -1

// Colors
#define WHITE 0xFFFF
#define BLACK 0x0000
#define RED   0xF800


// ECG Input
#define analogPin A0

// SD card CS
#define chip_select A9
//**************************
// *** Types ***
//**************************


// Display Coordinates
typedef struct
{
  int X;
  int Y;
}Point_t;


typedef enum 
{
  WAIT_STABILIZATION,
  FIND_BASELINE,
  FIND_THRESHHOLD,
  FIND_PEAK,
  WAIT_BLANKING_PERIOD
} State_t;

//**************************
// *** Variables ***
//**************************

Point_t CurPoint;
Point_t NextPoint;
Point_t UpperLeft;
Point_t LowerRight;

unsigned long Height;
unsigned int Data[450];

int   Baseline=512;  
long  LoopCount=0;

//Define the display object
// Use hardware SPI (on Uno, #13, #12, #11) and the above for CS/DC
Adafruit_HX8357 tft = Adafruit_HX8357(TFT_CS, TFT_DC, TFT_RST);

State_t State = WAIT_STABILIZATION;

//**************************
// *** SETUP             ***
//**************************
void setup() 
{
  //ECG Input setup
  pinMode(A0,INPUT);
  pinMode(A9,OUTPUT);
  pinMode(A8,INPUT);
  
  //setup serial port
  Serial.begin(115200);

   //start the display
  tft.begin(HX8357D);

  //Clear the screen
  ClearScreen();

  //set rotation vertical
  tft.setRotation(1);
  UpperLeft.X = 11;
  UpperLeft.Y = 26;
  LowerRight.X = tft.width()-26;
  LowerRight.Y = tft.height()-26;
  Height = (LowerRight.Y - UpperLeft.Y);
}


int DataIndex;

//**************************
// *** Main Loop         ***
//**************************
void loop() 
{
  // put your main code here, to run repeatedly:
  Baseline = 512;
  
  float AxisConversion = Height/1024; 
  unsigned long Sample =0;
  int DeltaNegCount=0;

  Serial.print("Wait Stablilzation: ");
  
  while (1)
  {
    int Y;
    int count;

    //Clear the screen and paint the border
    RefreshECGArea();    

    //increment the loop counter
    LoopCount++;
    Serial.print("LoopCount: ");
    Serial.println(LoopCount);

    //Reset the current and next ECG data pointers
    CurPoint.X = UpperLeft.X;
    NextPoint.X = UpperLeft.X;

    //Reset the data pointer into the data array
    DataIndex = 0;

    //Process one-screen full of data
    for (count=UpperLeft.X;count < LowerRight.X;count++)
    {
      //get the sample from the front end
      Sample = 1024 - analogRead(analogPin);
       
      //draw the line from the previous to the next point
      DrawNextEcgPoint(count,Sample);

      //store the data in the data array
      Data[DataIndex]=Sample;
      DataIndex++;

       //Sample rate - this approximates sampling every 10ms
      //  We may have to adjust this as we do more processing
      delay(10);
    }     
  }
}

//**************************
// Clear the Screen
//**************************
void ClearScreen(void)
{
  tft.fillScreen(WHITE);
}

//**************************
// Clear the Screen
//**************************
void RefreshECGArea(void)
{
  tft.fillRect(UpperLeft.X,UpperLeft.Y,LowerRight.X-UpperLeft.X,LowerRight.Y-UpperLeft.Y,WHITE);
  tft.drawRect(UpperLeft.X-1,UpperLeft.Y-1,LowerRight.X-UpperLeft.X+2,LowerRight.Y-UpperLeft.Y+2,RED);
}

//**************************
// draw the next point
//**************************
void DrawNextEcgPoint(int X,int Sample)
{
  NextPoint.X = X;
  Sample += 512-Baseline;
  NextPoint.Y = (int)((((unsigned long)Sample*Height)/1024L))+UpperLeft.Y;
  tft.drawLine(CurPoint.X,CurPoint.Y,NextPoint.X,NextPoint.Y,RED);
  CurPoint=NextPoint;

}

void DrawBeatmarker(int X)
{
  tft.fillCircle(X,100,4,RED);
}

