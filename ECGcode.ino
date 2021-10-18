//#include <Adafruit_GFX.h>
//#include <Adafruit_TFTLCD.h> 
//#include <Fonts/FreeMonoBoldOblique12pt7b.h>
//#include <Fonts/FreeSerif9pt7b.h>

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

int  Threshhold=0;
long LoopCount=0;
long  BaselineSum = 0;
int   BaselineCount=0;
int   Baseline=512;  
long  PeakAvg=0;
int   PeakCounter=0;
int   PeakThreshold;
int   MaxValue=0;
int   BaselineScreenAdj=0;
int   HeartBeatSum=0;
int   HeartBeatCount=0;
int   HeartBeat;
int   HeartBeatX;

// Define the SD card object
#define SD_CARD_CS  A5
Sd2Card SDCard;
// Define the File object
File DataFile;
bool fFileOpen = false; 

//Number of samples before the next Beat can occur
//  set to 200 BPM = 60/200 = 0.3 = 300ms = 300ms/10ms per Sample = 30 samples
int   BlankingPeriod=30;
int   BlankingCounter=0;

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

  //init the SDCard
//if (!SD.begin(SD_CARD_CS))
//  {
//      //print error message
//      Serial.println("Serial card failure or not inserted...");
//  }
//  else
//  {
//    Serial.println("SD Card initialized.");
//    DataFile = SD.open("Ecg.csv",FILE_WRITE);
//
//    //see if datafile was opened properly
//    if (DataFile == 0)
//    {
//      //datafile not opened
//      Serial.println("Output file could not be opened...");
//      fFileOpen = false; 
//    }
//    else
//    {
//      //data file opened properly - ready to write
//      Serial.println("Data file opened properly...");
//      fFileOpen = true;
//    }
//  }

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

    //reset the heartbeat variables
    HeartBeatSum=0;
    HeartBeatCount=0;
    HeartBeatX=0;

    MaxValue=1024;

    //Reset the data pointer into the data array
    DataIndex = 0;

    //Process one-screen full of data
    for (count=UpperLeft.X;count < LowerRight.X;count++)
    {
      //get the sample from the front end
      Sample = 1024 - analogRead(analogPin);
      String dataString = String(Sample) + ",";
      // save data to file
//      if (fFileOpen)
//      {
//          DataFile.println(dataString);
//      }
      
      //draw the line from the previous to the next point
      DrawNextEcgPoint(count,Sample);

      //store the data in the data array
      Data[DataIndex]=Sample;
      DataIndex++;

      //Process samples 
      switch(State)
      {
          //WAIT_STABILIZATION - wait 3 screens of data for front-end
          //  to settle down
          case WAIT_STABILIZATION:
            if (LoopCount > 2)
            {
              State = FIND_BASELINE;
            }
            break;

          //FIND_BASELINE - find the baseline.  First order this is the
          //  average of 3 screens
          case FIND_BASELINE:
            if (LoopCount < 4)
            {
              //Sum samples for 3 screens
              BaselineSum+=Sample;
              BaselineCount++;
            }
            else
            {
              //compute the baseline
              Baseline = BaselineSum/BaselineCount;
              Serial.print("Baseline: ");
              Serial.println(Baseline);
              State = FIND_THRESHHOLD;
            }
            break;

          //Find the average Peak threshold by averaging three screens
          //  of beats.  Theshold is 75% of peak value.
          case FIND_THRESHHOLD:
            if (LoopCount < 6)
            {
              if (MaxValue > Sample)
              {
                MaxValue = Sample;
                DeltaNegCount = 25;
              }
              else
              {
                //looking for going negative
                DeltaNegCount--;
                if (DeltaNegCount == 0)
                {
                  //found a meximum
                  PeakAvg+= MaxValue;
                  PeakCounter++;
                  MaxValue = 1024;
                }
              }
            }
            else
            {
              //Done with 3 screens - compute the threshold
              Serial.print("PeakAvg:");
              Serial.println(PeakAvg);
              Serial.print("PeakCounter: ");
              Serial.println(PeakCounter);
              PeakThreshold = PeakAvg/PeakCounter;

              
              PeakThreshold = Baseline-PeakThreshold;
              PeakThreshold *=3;
              PeakThreshold = PeakThreshold / 4;
              PeakThreshold = Baseline - PeakThreshold;
              State = FIND_PEAK;
              Serial.print("Peak Threshold: ");
              Serial.println(PeakThreshold);
            }
            break;
             
          case FIND_PEAK:
            if (Sample < PeakThreshold)
            {
              //Found a peak
              // - cmpute the BPM
              //Mark the beat with a circle
              //wait the Blanking Period
              BlankingCounter=BlankingPeriod;
              State = WAIT_BLANKING_PERIOD;

              //Draw the beat marker
              DrawBeatmarker(count);

              //Compute the heartbeat
              if (HeartBeatCount==0)
              {
                //first sample, just capture the X
                HeartBeatX=count;
                HeartBeatCount++;
              }
              else
              {
                HeartBeatSum += count-HeartBeatX;
                HeartBeatCount++;
                if (HeartBeatCount==4)
                //tft.setFont(&FreeMonoBoldOblique12pt7b);
                //printline (HeartBeatCount);
                
                {
                  HeartBeat = HeartBeatSum/4;
                  HeartBeat *=10;
                  HeartBeat = 60000/HeartBeat;

Serial.println(HeartBeat);
void setCursor(unit16_tx0, unit16_t y0);
Serial.println(HeartBeat);
tft.drawRect(tft.width()-85, 10, tft width()-20, 50, Hx8357-WHITE);
tft.SetCursor(tft.width()-80, 10);
tft.setTextSize(2);
tft.SetTextColor(Hx8357 - RED);
tft.println(HeartBeat);

tft.println(
                }
              }
              
              
            }
            break;
            
          case WAIT_BLANKING_PERIOD:
            BlankingCounter--;
            if (BlankingCounter <= 0)
            {
              State = FIND_PEAK;
            }
            break;
      }


      //Sample rate - this approximates sampling every 10ms
      //  We may have to adjust this as we do more processing
      delay(10);
    }  

    //flush the file's data to force writing
//    if (fFileOpen)
//      DataFile.flush();
    
  }
}

//**************************
// Clear the Screen
//**************************
void ClearScreen(void)
{
  tft.fillScreen(WHITE);
  
  //create a box around the display
  
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

