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
#define TOP_LINE_Y_MM       0.0f
#define LINE_SPACING_MM     10.0f
#define MAX_LINE_WIDTH_MM   140.0f

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

char WordBuffer[MAX_WORD_LENGTH];

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

int main(void)
{

    //char mode[]= {'8','N','1',0};
    char buffer[100];
    int textHeight;

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
    SendGCodeToRobot("G1 X0 Y0 F1000\n");
    SendGCodeToRobot("M3\n");
    SendGCodeToRobot("S0\n");  // pen up

    // user input
    int textHeight = GetTextHeight();
    Layout.scaleFactor = CalculateScaleFactor(textHeight, 18);
    Layout.lineSpacing = (float)textHeight + 5.0f;
    
    // load the font data from the txt file
    if (!LoadFontData("SingleStrokeFont.txt", FontData)) {
        CloseRS232Port();
        return 1;
    }

    ScaleFont(FontData, Layout.scaleFactor);

    // process the text file and generate g code
    char textBuffer[4096];
    if (ReadTextFile(textBuffer, "Test.txt", sizeof(textBuffer))) {
        GenerateGCode(textBuffer);
    } else {
        GenerateTextGCodeFromFile("Test.txt");
    }   

    DrawEndShape();
    MoveToOrigin();

    // Before we exit the program we need to close the COM port
    CloseRS232Port();
    printf("Com port now closed\n");

    return (0);
}


// user input for text height
int GetTextHeight(void)
{
    int h = 0;
    printf("Enter text height (4–10 mm): ");
    scanf("%d", &h);

    if (h < 4 || h > 10) {
        printf("Invalid height. Please use a value between 4 and 10 mm.\n");
        CloseRS232Port();
        exit(1);
    }
    return h;
}

// calculate scale factor
float CalculateScaleFactor(int textHeightMm, int baseUnits)
{
    return (float)textHeightMm / (float)baseUnits;
}

void ScaleFont(FontChar *fontData, float scaleFactor)
{
    (void)fontData;
    (void)scaleFactor;
}

// format the text position
void InitialiseTextPosition(void)
{
    Layout.cursorX = LEFT_MARGIN_MM;
    Layout.cursorY = TOP_LINE_Y_MM;
    Layout.currentLineWidth = 0.0f;
}

void AdvanceToNextLine(void)
{
    Layout.cursorX = LEFT_MARGIN_MM;
    Layout.cursorY -= Layout.lineSpacing;
    Layout.currentLineWidth = 5.0f;
}

int ReadTextFile(char *buffer, const char *filename, int maxLen)
{
    FILE *fp = fopen(filename, "r");
    if (!fp) return 0;

    int i = 0, c;
    while ((c = fgetc(fp)) != EOF && i < maxLen - 1)
        buffer[i++] = (char)c;

    buffer[i] = '\0';
    fclose(fp);
    return i;
}

int GetNextWord(FILE *textFile, char *buffer, int maxLen)
{
    int c, i = 0;
    do {
        c = fgetc(textFile);
        if (c == EOF) return 0;
    } while (isspace(c));

    while (c != EOF && !isspace(c)) {
        if (i < maxLen - 1) buffer[i++] = (char)c;
        c = fgetc(textFile);
    }
    buffer[i] = '\0';
    return 1;
}

float CalculateWordWidth(const char *word)
{
    float width = 0.0f;
    for (int i = 0; word[i]; i++) {
        unsigned char c = (unsigned char)word[i];
        if (FontData[c].defined)
            width += FontData[c].widthUnits * Layout.scaleFactor;
    }
    width += FontData[' '].widthUnits * Layout.scaleFactor;
    return width;
}

void GenerateGCode(const char *text)
{
    InitialiseTextPosition();
    const char *p = text;

    while (*p) {
        while (*p && isspace(*p)) {
            if (*p == '\n') AdvanceToNextLine();
            p++;
        }
        if (!*p) break;

        int i = 0;
        while (*p && !isspace(*p) && i < MAX_WORD_LENGTH - 1)
            WordBuffer[i++] = *p++;
        WordBuffer[i] = '\0';

        if (CalculateWordWidth(WordBuffer) >
            (Layout.maxLineWidth - Layout.currentLineWidth)) {
            AdvanceToNextLine();
        }

        RenderWord(WordBuffer);
    }
}

int GenerateTextGCodeFromFile(const char *filename)
{
    FILE *fp = fopen(filename, "r");
    if (!fp) return 0;

    InitialiseTextPosition();

    while (GetNextWord(fp, WordBuffer, MAX_WORD_LENGTH)) {
        if (CalculateWordWidth(WordBuffer) >
            (Layout.maxLineWidth - Layout.currentLineWidth)) {
            AdvanceToNextLine();
        }
        RenderWord(WordBuffer);
    }

    fclose(fp);
    return 1;
}

// rendering the words
void RenderWord(const char *word)
{
    for (int i = 0; word[i]; i++) {
        RenderCharacter(word[i]);
        float w = FontData[(unsigned char)word[i]].widthUnits * Layout.scaleFactor;
        Layout.cursorX += w;
        Layout.currentLineWidth += w;
    }

    float space = FontData[' '].widthUnits * Layout.scaleFactor;
    Layout.cursorX += space;
    Layout.currentLineWidth += space;
}

void RenderCharacter(char c)
{
    FontChar *fc = &FontData[(unsigned char)c];
    if (!fc->defined) return;

    char buffer[100];
    int lastPen = -1;

    for (int i = 0; i < fc->numStrokes; i++) {
        Stroke *s = &fc->strokes[i];
        float x = Layout.cursorX + s->dx * Layout.scaleFactor;
        float y = Layout.cursorY + s->dy * Layout.scaleFactor;

        if (s->penDown != lastPen) {
            sprintf(buffer, s->penDown ? "S1000\n" : "S0\n");
            SendGCodeToRobot(buffer);
            lastPen = s->penDown;
        }

        sprintf(buffer,
                s->penDown ? "G1 X%.2f Y%.2f F1000\n"
                           : "G0 X%.2f Y%.2f F1000\n",
                x, y);
        SendGCodeToRobot(buffer);
    }
}

//loading the font data
int LoadFontData(const char *filename, FontChar fontData[256])
{
    FILE *fp = fopen(filename, "r");
    if (!fp) return 0;

    for (int i = 0; i < 256; i++)
        fontData[i].defined = 0;

    int marker, code, count;
    while (fscanf(fp, "%d %d %d", &marker, &code, &count) == 3 && marker == 999) {
        FontChar *fc = &fontData[code];
        fc->numStrokes = count - 1;

        for (int i = 0; i < count; i++) {
            int x, y, pen;
            fscanf(fp, "%d %d %d", &x, &y, &pen);
            if (i < fc->numStrokes) {
                fc->strokes[i].dx = x;
                fc->strokes[i].dy = y;
                fc->strokes[i].penDown = pen;
            } else {
                fc->widthUnits = (float)x;
            }
        }
        fc->defined = 1;
    }

    fclose(fp);
    return 1;
}

// Send the data to the robot - note in 'PC' mode you need to hit space twice
// as the dummy 'WaitForReply' has a getch() within the function.
void SendCommands (char *buffer )

void SendGCodeToRobot(const char *command)
{
    PrintBuffer((char*)command);
    WaitForReply();
    Sleep(100);
}