#include <stdio.h>
#include <stdlib.h>
//#include <conio.h>
//#include <windows.h>
#include "rs232.h"
#include "serial.h"

#define bdrate 115200               /* 115200 baud */

// definition for the writing parameters
#define MAX_WORD_LENGTH     64
#define LEFT_MARGIN_MM      10.0f
#define TOP_LINE_Y_MM       80.0f
#define LINE_SPACING_MM     5.0f
#define MAX_LINE_WIDTH_MM   180.0f

// definition for font structures
typedef struct {
    float dx;
    float dy;
    int penDown;   // 0 = pen up, non-zero = pen down
} Stroke;

typedef struct {
    Stroke strokes[128];
    int   numStrokes;
    float widthUnits;
    int   defined;
} FontChar;

// structure to hold the state of the text layout
typedef struct {
    float scaleFactor;
    float cursorX;
    float cursorY;
    float currentLineWidth;
    float maxLineWidth;
    float lineSpacing;
} TextLayoutState;

//global variables
FontChar FontData[256];

TextLayoutState Layout = {
    1.0f,
    LEFT_MARGIN_MM,
    TOP_LINE_Y_MM,
    0.0f,
    MAX_LINE_WIDTH_MM,
    LINE_SPACING_MM
};

static char WordBuffer[MAX_WORD_LENGTH];

// functions
int   GetTextHeight(void);
float CalculateScaleFactor(int textHeightMm, int baseUnits);

int   LoadFontData(const char *filename, FontChar fontData[256]);
void  ScaleFont(FontChar *fontData, float scaleFactor);

int   ReadTextFile(char *buffer, const char *filename, int maxLen);

void  InitialiseTextPosition(void);
void  AdvanceToNextLine(void);
int   GetNextWord(FILE *textFile, char *buffer, int maxLen);
float CalculateWordWidth(const char *word);

void  GenerateGCode(const char *text);
int   GenerateTextGCodeFromFile(const char *filename);

void  RenderWord(const char *word);
void  RenderCharacter(char c);

void  DrawEndShape(void);

void  SendGCodeToRobot(const char *command);
void  MoveToOrigin(void);


void SendCommands (char *buffer );

int main()
{

    //char mode[]= {'8','N','1',0};
    char buffer[100];

    // If we cannot open the port then give up immediately
    if ( CanRS232PortBeOpened() == -1 )
    {
        printf ("\nUnable to open the COM port (specified in serial.h) ");
        exit (0);
    }

    // Time to wake up the robot
    printf ("\nAbout to wake up the robot\n");

    // We do this by sending a new-line
    sprintf (buffer, "\n");
     // printf ("Buffer to send: %s", buffer); // For diagnostic purposes only, normally comment out
    PrintBuffer (&buffer[0]);
    Sleep(100);

    // This is a special case - we wait  until we see a dollar ($)
    WaitForDollar();

    printf ("\nThe robot is now ready to draw\n");

        //These commands get the robot into 'ready to draw mode' and need to be sent before any writing commands
    sprintf (buffer, "G1 X0 Y0 F1000\n");
    SendCommands(buffer);
    sprintf (buffer, "M3\n");
    SendCommands(buffer);
    sprintf (buffer, "S0\n");
    SendCommands(buffer);


    // These are sample commands to draw out some information - these are the ones you will be generating.
    sprintf (buffer, "G0 X-13.41849 Y0.000\n");
    SendCommands(buffer);
    sprintf (buffer, "S1000\n");
    SendCommands(buffer);
    sprintf (buffer, "G1 X-13.41849 Y-4.28041\n");
    SendCommands(buffer);
    sprintf (buffer, "G1 X-13.41849 Y0.0000\n");
    SendCommands(buffer);
    sprintf (buffer, "G1 X-13.41089 Y4.28041\n");
    SendCommands(buffer);
    sprintf (buffer, "S0\n");
    SendCommands(buffer);
    sprintf (buffer, "G0 X-7.17524 Y0\n");
    SendCommands(buffer);
    sprintf (buffer, "S1000\n");
    SendCommands(buffer);
    sprintf (buffer, "G0 X0 Y0\n");
    SendCommands(buffer);

    // Before we exit the program we need to close the COM port
    CloseRS232Port();
    printf("Com port now closed\n");

    return (0);
}

// Send the data to the robot - note in 'PC' mode you need to hit space twice
// as the dummy 'WaitForReply' has a getch() within the function.
void SendCommands (char *buffer )
{
    // printf ("Buffer to send: %s", buffer); // For diagnostic purposes only, normally comment out
    PrintBuffer (&buffer[0]);
    WaitForReply();
    Sleep(100); // Can omit this when using the writing robot but has minimal effect
    // getch(); // Omit this once basic testing with emulator has taken place
}
