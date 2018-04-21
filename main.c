#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/dom_pk_codes.h>

#define TEXT_WIDTH 61

typedef struct {
  char data[8];
} Glyph;

#define LETTER_PERIOD 26
#define LETTER_QUESTION 27
#define LETTER_EXCLIMATION 28
#define LETTER_APOSTROPHE 29
#define LETTER_COMMA 30
#define LETTER_PROMPT 31

#define GLYPH_GUY 32
#define GLYPH_PLANE 33
#define GLYPH_CHUTE 34
#define GLYPH_GUN 35

Glyph letters[36];
int drawColor = 0xFFFFFFFF;
int screenWidth;
int screenHeight;
char* promptText = "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";
int promptLength = 0;

#define LINE_GAMEOVER -2
#define LINE_BLANK -1
#define LINE_HELP_1 0
#define LINE_HELP_2 1

#define LINE_ERROR_COMMAND 11
#define LINE_ERROR_USE 12
#define LINE_ERROR_OPEN 13
#define LINE_ERROR_GET 14
#define LINE_ERROR_TAKE 15

#define LINE_ACTION_PROMPT 2
#define LINE_FALLING_1 3
#define LINE_FALLING_2 4
#define LINE_PARACHUTE 6
#define LINE_GROUND_1 7
#define LINE_GROUND_2 8

#define LINE_DEATH_FALL 5
#define LINE_DEATH_GUN_1 9
#define LINE_DEATH_GUN_2 10
#define LINE_DEATH_SHOOT 16

#define MAX_LINES 44
int lineCount = 3;
int lines[] = {LINE_FALLING_1, LINE_FALLING_2, LINE_ACTION_PROMPT, LINE_BLANK, LINE_BLANK, LINE_BLANK, LINE_BLANK, LINE_BLANK, LINE_BLANK, LINE_BLANK, LINE_BLANK, LINE_BLANK, LINE_BLANK, LINE_BLANK, LINE_BLANK, LINE_BLANK, LINE_BLANK, LINE_BLANK, LINE_BLANK, LINE_BLANK, LINE_BLANK, LINE_BLANK, LINE_BLANK, LINE_BLANK, LINE_BLANK, LINE_BLANK, LINE_BLANK, LINE_BLANK, LINE_BLANK, LINE_BLANK, LINE_BLANK, LINE_BLANK, LINE_BLANK, LINE_BLANK, LINE_BLANK, LINE_BLANK, LINE_BLANK, LINE_BLANK, LINE_BLANK, LINE_BLANK, LINE_BLANK, LINE_BLANK, LINE_BLANK, LINE_BLANK};

char* gameOverResponse = "i would like that very much";
char* promptLines[] = {
/*  |_____________________________________________________________| */
    "Help | Message Syntax | COMMAND OBJECT",
    "Valid Commands | Use, Open, Get, Take",
    "What do you do?",
    "You leap out of the airplane, and can feel the wind wipping",
    "at your face as you fall towards the ground.",
    "You hit the ground going too fast, and die.",
    "You open your parachute and your decent slows down.",
    "You reach the ground and can see a gun on the ground a little",
    "ways away.",
    "You try to get to the gun in time, but another player beats",
    "you to it. Bang, Bang! You are dead.",
    "You are not sure how to do that.",
    "You are not sure how to use that.",
    "You are not sure how to open that.",
    "You are not sure how to get that.",
    "You are not sure how to take that.",
    "Another player picks the gun up and shoots you, Bang, Bang!"
  };

int animTimer = 0;
int falling = 0;
int planeMovement = 18;
int parachute = 0;
int gameover = 0;
int enemy = 0;

int fallAcc = 0;
int planeAcc = 0;
int enemyAcc = 0;

////////////////////////////////////////////////////////////////////////////////////////////////////

void drawRect(unsigned int* canvasPixels, int screenX, int screenY, int w, int h)
{
  int x, y;
  for(y = 0; y < h; ++y)
  {
    for(x = 0; x < w; ++x)
    {
      canvasPixels[(screenX + x) + ((screenY + y) * screenWidth)] = drawColor;
    }
  }
}

void drawGlyph(unsigned int* canvasPixels, int id, int screenX, int screenY)
{
  char* spriteData = letters[id].data;
  int sX, sY;
  for(sY = 0; sY < 8; ++sY)
  {
    for(sX = 0; sX < 8; ++sX)
    {
      if ((*spriteData) & (1 << sX))
      {
        canvasPixels[(screenX + 8 - 1 - sX) + ((screenY + sY) * screenWidth)] = drawColor;
      }
    }
    ++spriteData;
  }
}

void drawLetter(unsigned int* canvasPixels, char c, int screenX, int screenY)
{
  char* spriteData;
  if(c == '.')
  {
    drawGlyph(canvasPixels, LETTER_PERIOD, screenX, screenY);
  }
  else if(c == '?')
  {
    drawGlyph(canvasPixels, LETTER_QUESTION, screenX, screenY);
  }
  else if(c == '!')
  {
    drawGlyph(canvasPixels, LETTER_EXCLIMATION, screenX, screenY);
  }
  else if(c == '\'')
  {
    drawGlyph(canvasPixels, LETTER_APOSTROPHE, screenX, screenY);
  }
  else if(c == ',')
  {
    drawGlyph(canvasPixels, LETTER_COMMA, screenX, screenY);
  }
  else if(c == '|')
  {
    drawGlyph(canvasPixels, LETTER_PROMPT, screenX, screenY);
  }
  else if(c >= 'a')
  {
    drawGlyph(canvasPixels, c - 'a', screenX, screenY);
  }
  else
  {
    drawGlyph(canvasPixels, c - 'A', screenX, screenY);
  }
}

void drawString(unsigned int* canvasPixels, char* str, int screenX, int screenY)
{
  int sX = 0;
  while(*str != '\0')
  {
    if(*str != ' ')
    {
      drawLetter(canvasPixels, *str, screenX + (8 * sX), screenY);
    }
    ++str;
    ++sX;
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void addLine(int line)
{
  if (lineCount >= MAX_LINES)
  {
    int i;
    for (i = 0; i < MAX_LINES - 1; ++i)
    {
      lines[i] = lines[i + 1];
    }
    lineCount = MAX_LINES - 1;
  }
  lines[lineCount] = line;
  ++lineCount;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

#define KEY_BACKSPACE 8
#define KEY_DELETE 46

#define KEY_ENTER 13

#define KEY_SPACE 32

#define KEY_LETTERS 65

int promptTest(int start, int length, char* test)
{
  if (promptLength < start + length)
  {
    return 0;
  }

  int i;
  for (i = 0; i < length; ++i)
  {
    if (promptText[start + i] != test[i])
    {
      return 0;
    }
  }

  return 1;
}

void clearPrompt()
{
  for(; promptLength >= 0; --promptLength)
  {
    promptText[promptLength] = '\0';
  }
  promptLength = 0;
}

EM_BOOL keyCallback(int eventType, const EmscriptenKeyboardEvent *e, void *userData)
{
  if (e->ctrlKey || e->altKey || e->metaKey)
  {
    return EM_FALSE;
  }

  if (gameover)
  {
    if (promptLength < 27)
    {
      if ((e->which == KEY_SPACE) || (e->which >= KEY_LETTERS && e->which < KEY_LETTERS + 26))
      {
        promptText[promptLength] = gameOverResponse[promptLength];
        ++promptLength;
        return EM_TRUE;
      }
    }
    else if (e->which == KEY_ENTER)
    {
      clearPrompt();
      animTimer = 0;
      falling = 0;
      planeMovement = 18;
      parachute = 0;
      gameover = 0;
      enemy = 0;
      fallAcc = 0;
      planeAcc = 0;
      enemyAcc = 0;
      addLine(LINE_BLANK);
      addLine(LINE_BLANK);
      addLine(LINE_FALLING_1);
      addLine(LINE_FALLING_2);
      addLine(LINE_ACTION_PROMPT);
    }
  }
  else
  {
    if (e->which == KEY_ENTER && promptLength > 0)
    {
      if (promptTest(0, 4, "help"))
      {
        addLine(LINE_HELP_1);
        addLine(LINE_HELP_2);
        addLine(LINE_ACTION_PROMPT);
      }
      else if (promptTest(0, 3, "use"))
      {
        if (parachute == 0 && promptTest(4, 9, "parachute"))
        {
          addLine(LINE_PARACHUTE);
          addLine(LINE_ACTION_PROMPT);
          parachute = 1;
        }
        else
        {
          addLine(LINE_ERROR_USE);
          addLine(LINE_ACTION_PROMPT);
        }
      }
      else if (promptTest(0, 4, "open"))
      {
        if (parachute == 0 && promptTest(5, 9, "parachute"))
        {
          addLine(LINE_PARACHUTE);
          addLine(LINE_ACTION_PROMPT);
          parachute = 1;
        }
        else
        {
          addLine(LINE_ERROR_OPEN);
          addLine(LINE_ACTION_PROMPT);
        }
      }
      else if (promptTest(0, 3, "get"))
      {
        if (parachute && falling > 165 && promptTest(4, 3, "gun"))
        {
          addLine(LINE_DEATH_GUN_1);
          addLine(LINE_DEATH_GUN_2);
          addLine(LINE_GAMEOVER);
          enemy = 70;
          gameover = 1;
        }
        else
        {
          addLine(LINE_ERROR_GET);
          addLine(LINE_ACTION_PROMPT);
        }
      }
      else if (promptTest(0, 3, "take"))
      {
        if (parachute && falling > 165 && promptTest(5, 3, "gun"))
        {
          addLine(LINE_DEATH_GUN_1);
          addLine(LINE_DEATH_GUN_2);
          addLine(LINE_GAMEOVER);
          enemy = 70;
          gameover = 1;
        }
        else
        {
          addLine(LINE_ERROR_TAKE);
          addLine(LINE_ACTION_PROMPT);
        }
      }
      else
      {
        addLine(LINE_ERROR_COMMAND);
        addLine(LINE_ACTION_PROMPT);
      }
      clearPrompt();
      return EM_TRUE;
    }

    if (promptLength < TEXT_WIDTH - 1)
    {
      if (e->which == KEY_SPACE)
      {
        promptText[promptLength] = ' ';
        ++promptLength;
        return EM_TRUE;
      }

      if (e->which >= KEY_LETTERS && e->which < KEY_LETTERS + 26)
      {
        promptText[promptLength] = e->which - KEY_LETTERS + 'a';
        ++promptLength;
        return EM_TRUE;
      }
    }
  }

  if (e->which == KEY_BACKSPACE && promptLength > 0)
  {
    --promptLength;
    promptText[promptLength] = '\0';
    return EM_TRUE;
  }

  printf("You pressed key %lu\n", e->which);
  return EM_FALSE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void EMSCRIPTEN_KEEPALIVE load()
{
  letters['a'-'a'].data[0] = 0b01111100;
  letters['a'-'a'].data[1] = 0b10000010;
  letters['a'-'a'].data[2] = 0b10000010;
  letters['a'-'a'].data[3] = 0b11111110;
  letters['a'-'a'].data[4] = 0b10000010;
  letters['a'-'a'].data[5] = 0b10000010;
  letters['a'-'a'].data[6] = 0b10000010;
  letters['a'-'a'].data[7] = 0b00000000;

  letters['b'-'a'].data[0] = 0b11111100;
  letters['b'-'a'].data[1] = 0b10000010;
  letters['b'-'a'].data[2] = 0b10000010;
  letters['b'-'a'].data[3] = 0b11111100;
  letters['b'-'a'].data[4] = 0b10000010;
  letters['b'-'a'].data[5] = 0b10000010;
  letters['b'-'a'].data[6] = 0b11111110;
  letters['b'-'a'].data[7] = 0b00000000;

  letters['c'-'a'].data[0] = 0b01111110;
  letters['c'-'a'].data[1] = 0b10000000;
  letters['c'-'a'].data[2] = 0b10000000;
  letters['c'-'a'].data[3] = 0b10000000;
  letters['c'-'a'].data[4] = 0b10000000;
  letters['c'-'a'].data[5] = 0b10000000;
  letters['c'-'a'].data[6] = 0b01111110;
  letters['c'-'a'].data[7] = 0b00000000;

  letters['d'-'a'].data[0] = 0b11111100;
  letters['d'-'a'].data[1] = 0b10000010;
  letters['d'-'a'].data[2] = 0b10000010;
  letters['d'-'a'].data[3] = 0b10000010;
  letters['d'-'a'].data[4] = 0b10000010;
  letters['d'-'a'].data[5] = 0b10000010;
  letters['d'-'a'].data[6] = 0b11111100;
  letters['d'-'a'].data[7] = 0b00000000;

  letters['e'-'a'].data[0] = 0b11111110;
  letters['e'-'a'].data[1] = 0b10000000;
  letters['e'-'a'].data[2] = 0b10000000;
  letters['e'-'a'].data[3] = 0b11111110;
  letters['e'-'a'].data[4] = 0b10000000;
  letters['e'-'a'].data[5] = 0b10000000;
  letters['e'-'a'].data[6] = 0b11111110;
  letters['e'-'a'].data[7] = 0b00000000;

  letters['f'-'a'].data[0] = 0b11111110;
  letters['f'-'a'].data[1] = 0b10000000;
  letters['f'-'a'].data[2] = 0b10000000;
  letters['f'-'a'].data[3] = 0b11111110;
  letters['f'-'a'].data[4] = 0b10000000;
  letters['f'-'a'].data[5] = 0b10000000;
  letters['f'-'a'].data[6] = 0b10000000;
  letters['f'-'a'].data[7] = 0b00000000;

  letters['g'-'a'].data[0] = 0b01111110;
  letters['g'-'a'].data[1] = 0b10000000;
  letters['g'-'a'].data[2] = 0b10000000;
  letters['g'-'a'].data[3] = 0b10011110;
  letters['g'-'a'].data[4] = 0b10000010;
  letters['g'-'a'].data[5] = 0b10000010;
  letters['g'-'a'].data[6] = 0b01111100;
  letters['g'-'a'].data[7] = 0b00000000;

  letters['h'-'a'].data[0] = 0b10000010;
  letters['h'-'a'].data[1] = 0b10000010;
  letters['h'-'a'].data[2] = 0b10000010;
  letters['h'-'a'].data[3] = 0b11111110;
  letters['h'-'a'].data[4] = 0b10000010;
  letters['h'-'a'].data[5] = 0b10000010;
  letters['h'-'a'].data[6] = 0b10000010;
  letters['h'-'a'].data[7] = 0b00000000;

  letters['i'-'a'].data[0] = 0b11111110;
  letters['i'-'a'].data[1] = 0b00010000;
  letters['i'-'a'].data[2] = 0b00010000;
  letters['i'-'a'].data[3] = 0b00010000;
  letters['i'-'a'].data[4] = 0b00010000;
  letters['i'-'a'].data[5] = 0b00010000;
  letters['i'-'a'].data[6] = 0b11111110;
  letters['i'-'a'].data[7] = 0b00000000;

  letters['j'-'a'].data[0] = 0b00001110;
  letters['j'-'a'].data[1] = 0b00000010;
  letters['j'-'a'].data[2] = 0b00000010;
  letters['j'-'a'].data[3] = 0b00000010;
  letters['j'-'a'].data[4] = 0b00000010;
  letters['j'-'a'].data[5] = 0b10000010;
  letters['j'-'a'].data[6] = 0b01111100;
  letters['j'-'a'].data[7] = 0b00000000;

  letters['k'-'a'].data[0] = 0b10001000;
  letters['k'-'a'].data[1] = 0b10010000;
  letters['k'-'a'].data[2] = 0b10100000;
  letters['k'-'a'].data[3] = 0b11000000;
  letters['k'-'a'].data[4] = 0b10110000;
  letters['k'-'a'].data[5] = 0b10001100;
  letters['k'-'a'].data[6] = 0b10000010;
  letters['k'-'a'].data[7] = 0b00000000;

  letters['l'-'a'].data[0] = 0b10000000;
  letters['l'-'a'].data[1] = 0b10000000;
  letters['l'-'a'].data[2] = 0b10000000;
  letters['l'-'a'].data[3] = 0b10000000;
  letters['l'-'a'].data[4] = 0b10000000;
  letters['l'-'a'].data[5] = 0b10000000;
  letters['l'-'a'].data[6] = 0b11111110;
  letters['l'-'a'].data[7] = 0b00000000;

  letters['m'-'a'].data[0] = 0b11000110;
  letters['m'-'a'].data[1] = 0b10101010;
  letters['m'-'a'].data[2] = 0b10010010;
  letters['m'-'a'].data[3] = 0b10010010;
  letters['m'-'a'].data[4] = 0b10010010;
  letters['m'-'a'].data[5] = 0b10010010;
  letters['m'-'a'].data[6] = 0b10010010;
  letters['m'-'a'].data[7] = 0b00000000;

  letters['n'-'a'].data[0] = 0b10000010;
  letters['n'-'a'].data[1] = 0b11000010;
  letters['n'-'a'].data[2] = 0b10100010;
  letters['n'-'a'].data[3] = 0b10010010;
  letters['n'-'a'].data[4] = 0b10001010;
  letters['n'-'a'].data[5] = 0b10000110;
  letters['n'-'a'].data[6] = 0b10000010;
  letters['n'-'a'].data[7] = 0b00000000;

  letters['o'-'a'].data[0] = 0b01111100;
  letters['o'-'a'].data[1] = 0b10000010;
  letters['o'-'a'].data[2] = 0b10000010;
  letters['o'-'a'].data[3] = 0b10000010;
  letters['o'-'a'].data[4] = 0b10000010;
  letters['o'-'a'].data[5] = 0b10000010;
  letters['o'-'a'].data[6] = 0b01111100;
  letters['o'-'a'].data[7] = 0b00000000;

  letters['p'-'a'].data[0] = 0b11111100;
  letters['p'-'a'].data[1] = 0b10000010;
  letters['p'-'a'].data[2] = 0b10000010;
  letters['p'-'a'].data[3] = 0b11111100;
  letters['p'-'a'].data[4] = 0b10000000;
  letters['p'-'a'].data[5] = 0b10000000;
  letters['p'-'a'].data[6] = 0b10000000;
  letters['p'-'a'].data[7] = 0b00000000;

  letters['q'-'a'].data[0] = 0b01111100;
  letters['q'-'a'].data[1] = 0b10000010;
  letters['q'-'a'].data[2] = 0b10000010;
  letters['q'-'a'].data[3] = 0b10000010;
  letters['q'-'a'].data[4] = 0b10001000;
  letters['q'-'a'].data[5] = 0b10000100;
  letters['q'-'a'].data[6] = 0b01110010;
  letters['q'-'a'].data[7] = 0b00000000;

  letters['r'-'a'].data[0] = 0b11111100;
  letters['r'-'a'].data[1] = 0b10000010;
  letters['r'-'a'].data[2] = 0b10000010;
  letters['r'-'a'].data[3] = 0b11111100;
  letters['r'-'a'].data[4] = 0b10100000;
  letters['r'-'a'].data[5] = 0b10010000;
  letters['r'-'a'].data[6] = 0b10001000;
  letters['r'-'a'].data[7] = 0b00000000;

  letters['s'-'a'].data[0] = 0b01111110;
  letters['s'-'a'].data[1] = 0b10000000;
  letters['s'-'a'].data[2] = 0b10000000;
  letters['s'-'a'].data[3] = 0b01111100;
  letters['s'-'a'].data[4] = 0b00000010;
  letters['s'-'a'].data[5] = 0b00000010;
  letters['s'-'a'].data[6] = 0b11111100;
  letters['s'-'a'].data[7] = 0b00000000;

  letters['t'-'a'].data[0] = 0b11111110;
  letters['t'-'a'].data[1] = 0b00010000;
  letters['t'-'a'].data[2] = 0b00010000;
  letters['t'-'a'].data[3] = 0b00010000;
  letters['t'-'a'].data[4] = 0b00010000;
  letters['t'-'a'].data[5] = 0b00010000;
  letters['t'-'a'].data[6] = 0b00010000;
  letters['t'-'a'].data[7] = 0b00000000;

  letters['u'-'a'].data[0] = 0b10000010;
  letters['u'-'a'].data[1] = 0b10000010;
  letters['u'-'a'].data[2] = 0b10000010;
  letters['u'-'a'].data[3] = 0b10000010;
  letters['u'-'a'].data[4] = 0b10000010;
  letters['u'-'a'].data[5] = 0b10000010;
  letters['u'-'a'].data[6] = 0b01111100;
  letters['u'-'a'].data[7] = 0b00000000;

  letters['v'-'a'].data[0] = 0b10000010;
  letters['v'-'a'].data[1] = 0b01000100;
  letters['v'-'a'].data[2] = 0b01000100;
  letters['v'-'a'].data[3] = 0b00101000;
  letters['v'-'a'].data[4] = 0b00101000;
  letters['v'-'a'].data[5] = 0b00010000;
  letters['v'-'a'].data[6] = 0b00010000;
  letters['v'-'a'].data[7] = 0b00000000;

  letters['w'-'a'].data[0] = 0b10000010;
  letters['w'-'a'].data[1] = 0b10010010;
  letters['w'-'a'].data[2] = 0b10010010;
  letters['w'-'a'].data[3] = 0b10010010;
  letters['w'-'a'].data[4] = 0b10010010;
  letters['w'-'a'].data[5] = 0b10010010;
  letters['w'-'a'].data[6] = 0b01101100;
  letters['w'-'a'].data[7] = 0b00000000;

  letters['x'-'a'].data[0] = 0b10000010;
  letters['x'-'a'].data[1] = 0b10000010;
  letters['x'-'a'].data[2] = 0b01000100;
  letters['x'-'a'].data[3] = 0b00111000;
  letters['x'-'a'].data[4] = 0b01000100;
  letters['x'-'a'].data[5] = 0b10000010;
  letters['x'-'a'].data[6] = 0b10000010;
  letters['x'-'a'].data[7] = 0b00000000;

  letters['y'-'a'].data[0] = 0b10000010;
  letters['y'-'a'].data[1] = 0b10000010;
  letters['y'-'a'].data[2] = 0b01000100;
  letters['y'-'a'].data[3] = 0b00101000;
  letters['y'-'a'].data[4] = 0b00010000;
  letters['y'-'a'].data[5] = 0b00010000;
  letters['y'-'a'].data[6] = 0b00010000;
  letters['y'-'a'].data[7] = 0b00000000;

  letters['z'-'a'].data[0] = 0b11111110;
  letters['z'-'a'].data[1] = 0b00000100;
  letters['z'-'a'].data[2] = 0b00001000;
  letters['z'-'a'].data[3] = 0b00010000;
  letters['z'-'a'].data[4] = 0b00100000;
  letters['z'-'a'].data[5] = 0b01000000;
  letters['z'-'a'].data[6] = 0b11111110;
  letters['z'-'a'].data[7] = 0b00000000;

  letters[LETTER_PERIOD].data[0] = 0b00000000;
  letters[LETTER_PERIOD].data[1] = 0b00000000;
  letters[LETTER_PERIOD].data[2] = 0b00000000;
  letters[LETTER_PERIOD].data[3] = 0b00000000;
  letters[LETTER_PERIOD].data[4] = 0b00000000;
  letters[LETTER_PERIOD].data[5] = 0b01100000;
  letters[LETTER_PERIOD].data[6] = 0b01100000;
  letters[LETTER_PERIOD].data[7] = 0b00000000;

  letters[LETTER_QUESTION].data[0] = 0b01111000;
  letters[LETTER_QUESTION].data[1] = 0b00001100;
  letters[LETTER_QUESTION].data[2] = 0b00111000;
  letters[LETTER_QUESTION].data[3] = 0b01100000;
  letters[LETTER_QUESTION].data[4] = 0b00000000;
  letters[LETTER_QUESTION].data[5] = 0b01100000;
  letters[LETTER_QUESTION].data[6] = 0b01100000;
  letters[LETTER_QUESTION].data[7] = 0b00000000;

  letters[LETTER_EXCLIMATION].data[0] = 0b01100000;
  letters[LETTER_EXCLIMATION].data[1] = 0b01100000;
  letters[LETTER_EXCLIMATION].data[2] = 0b01100000;
  letters[LETTER_EXCLIMATION].data[3] = 0b01100000;
  letters[LETTER_EXCLIMATION].data[4] = 0b00000000;
  letters[LETTER_EXCLIMATION].data[5] = 0b01100000;
  letters[LETTER_EXCLIMATION].data[6] = 0b01100000;
  letters[LETTER_EXCLIMATION].data[7] = 0b00000000;

  letters[LETTER_APOSTROPHE].data[0] = 0b01100000;
  letters[LETTER_APOSTROPHE].data[1] = 0b01100000;
  letters[LETTER_APOSTROPHE].data[2] = 0b01100000;
  letters[LETTER_APOSTROPHE].data[3] = 0b00000000;
  letters[LETTER_APOSTROPHE].data[4] = 0b00000000;
  letters[LETTER_APOSTROPHE].data[5] = 0b00000000;
  letters[LETTER_APOSTROPHE].data[6] = 0b00000000;
  letters[LETTER_APOSTROPHE].data[7] = 0b00000000;

  letters[LETTER_COMMA].data[0] = 0b00000000;
  letters[LETTER_COMMA].data[1] = 0b00000000;
  letters[LETTER_COMMA].data[2] = 0b00000000;
  letters[LETTER_COMMA].data[3] = 0b00000000;
  letters[LETTER_COMMA].data[4] = 0b01100000;
  letters[LETTER_COMMA].data[5] = 0b01100000;
  letters[LETTER_COMMA].data[6] = 0b00100000;
  letters[LETTER_COMMA].data[7] = 0b00000000;

  letters[LETTER_PROMPT].data[0] = 0b01000000;
  letters[LETTER_PROMPT].data[1] = 0b01000000;
  letters[LETTER_PROMPT].data[2] = 0b01000000;
  letters[LETTER_PROMPT].data[3] = 0b01000000;
  letters[LETTER_PROMPT].data[4] = 0b01000000;
  letters[LETTER_PROMPT].data[5] = 0b01000000;
  letters[LETTER_PROMPT].data[6] = 0b01000000;
  letters[LETTER_PROMPT].data[7] = 0b01000000;

  letters[GLYPH_PLANE].data[0] = 0b00000000;
  letters[GLYPH_PLANE].data[1] = 0b00000000;
  letters[GLYPH_PLANE].data[2] = 0b00011001;
  letters[GLYPH_PLANE].data[3] = 0b01111111;
  letters[GLYPH_PLANE].data[4] = 0b11111111;
  letters[GLYPH_PLANE].data[5] = 0b00111000;
  letters[GLYPH_PLANE].data[6] = 0b00011000;
  letters[GLYPH_PLANE].data[7] = 0b00001000;

  letters[GLYPH_GUY].data[0] = 0b00011000;
  letters[GLYPH_GUY].data[1] = 0b00011000;
  letters[GLYPH_GUY].data[2] = 0b01111110;
  letters[GLYPH_GUY].data[3] = 0b01011010;
  letters[GLYPH_GUY].data[4] = 0b00011000;
  letters[GLYPH_GUY].data[5] = 0b00100100;
  letters[GLYPH_GUY].data[6] = 0b01000010;
  letters[GLYPH_GUY].data[7] = 0b01000010;

  letters[GLYPH_CHUTE].data[0] = 0b00000000;
  letters[GLYPH_CHUTE].data[1] = 0b00000000;
  letters[GLYPH_CHUTE].data[2] = 0b00000000;
  letters[GLYPH_CHUTE].data[3] = 0b00000000;
  letters[GLYPH_CHUTE].data[4] = 0b00111100;
  letters[GLYPH_CHUTE].data[5] = 0b11111111;
  letters[GLYPH_CHUTE].data[6] = 0b11000011;
  letters[GLYPH_CHUTE].data[7] = 0b00000000;

  letters[GLYPH_GUN].data[0] = 0b00000000;
  letters[GLYPH_GUN].data[1] = 0b00000000;
  letters[GLYPH_GUN].data[2] = 0b11111111;
  letters[GLYPH_GUN].data[3] = 0b11111111;
  letters[GLYPH_GUN].data[4] = 0b00001011;
  letters[GLYPH_GUN].data[5] = 0b00000111;
  letters[GLYPH_GUN].data[6] = 0b00000011;
  letters[GLYPH_GUN].data[7] = 0b00000000;

  /*
  letters[GLYPH_PLANE].data[0] = 0b00000000;
  letters[GLYPH_PLANE].data[1] = 0b00000000;
  letters[GLYPH_PLANE].data[2] = 0b00000000;
  letters[GLYPH_PLANE].data[3] = 0b00000000;
  letters[GLYPH_PLANE].data[4] = 0b00000000;
  letters[GLYPH_PLANE].data[5] = 0b00000000;
  letters[GLYPH_PLANE].data[6] = 0b00000000;
  letters[GLYPH_PLANE].data[7] = 0b00000000;
  */

  emscripten_set_keydown_callback(0, 0, EM_TRUE, keyCallback);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void EMSCRIPTEN_KEEPALIVE tick(unsigned char* u8pixels, int width, int height, int time)
{
  screenWidth = width;
  screenHeight = height;
  unsigned int* canvasPixels = (unsigned int*) u8pixels;

  ++animTimer;
  if (animTimer > 60)
  {
    animTimer = 0;
  }

  fallAcc += time;
  planeAcc += time;
  if (fallAcc > (parachute ? 800 : 400) && falling < 165)
  {
    fallAcc = 0;
    falling += 8;
    if (parachute > 0)
    {
      ++parachute;
    }
    if (falling >= 165)
    {
      clearPrompt();
      if (parachute)
      {
        addLine(LINE_GROUND_1);
        addLine(LINE_GROUND_2);
        addLine(LINE_ACTION_PROMPT);
      }
      else
      {
        addLine(LINE_DEATH_FALL);
        addLine(LINE_GAMEOVER);
        gameover = 1;
      }
    }
  }
  if (planeAcc > 100)
  {
    planeAcc = 0;
    planeMovement += 4;
  }
  if (parachute && falling >= 165 && enemy < 70)
  {
    enemyAcc += time;
    if (enemyAcc > 300)
    {
      enemyAcc = 0;
      enemy += 4;
      if (enemy >= 70)
      {
        addLine(LINE_DEATH_SHOOT);
        addLine(LINE_GAMEOVER);
        gameover = 1;
        enemy = 70;
      }
    }
  }

  // CLEAR SCREEN:
  drawColor = 0xFF000000;
  drawRect(canvasPixels, 0, 0, 500, screenHeight);

  // INVENTORY BOX:
  drawColor = 0xFF333333;
  drawRect(canvasPixels, 500, 0, screenWidth - 500, screenHeight - 200);

  // SKY BOX:
  drawColor = 0xFFAA5555;
  drawRect(canvasPixels, 500, screenHeight - 200, screenWidth - 500, 192);

  // GROUND BOX:
  drawColor = 0xFF115511;
  drawRect(canvasPixels, 500, screenHeight - 8, screenWidth - 500, 8);

  // AIRPLANE:
  if (planeMovement < (screenWidth - 500))
  {
    drawColor = 0xFF333333;
    drawGlyph(canvasPixels, GLYPH_PLANE, screenWidth - planeMovement, screenHeight - 192);
  }

  // GUY:
  drawColor = 0xFF6666FF;
  if (parachute && falling < 165)
  {
    drawGlyph(canvasPixels, GLYPH_CHUTE, screenWidth - 24 - parachute, screenHeight - 182 + falling - 8);
  }
  drawGlyph(canvasPixels, GLYPH_GUY, screenWidth - 24 - parachute, screenHeight - 182 + falling);

  if (parachute && falling >= 165)
  {
    drawColor = 0xFF2222FF;
    drawGlyph(canvasPixels, GLYPH_GUY, 502 + enemy, screenHeight - 182 + falling);

    if (enemy < 70)
    {
      drawColor = 0xFF222222;
      drawGlyph(canvasPixels, GLYPH_GUN, 575, screenHeight - 182 + falling);
    }
  }

  // INVENTORY:
  drawColor = 0xFF000000;
  /*                       |_________________| */
  drawString(canvasPixels, "Inventory", 509, 9);
  drawString(canvasPixels, "Inventory", 510, 9);
  drawString(canvasPixels, "Inventory", 509, 10);
  drawString(canvasPixels, "Inventory", 510, 10);
  drawColor = 0xFFFFFFFF;
  drawString(canvasPixels, "Inventory", 508, 8);

  if (parachute == 0)
  {
    drawColor = 0xFF000000;
    drawString(canvasPixels, "Parachute", 516, 20);
    drawString(canvasPixels, "Parachute", 516, 21);
    drawString(canvasPixels, "Parachute", 517, 20);
    drawString(canvasPixels, "Parachute", 517, 21);
    drawColor = 0xFF225555;
    drawString(canvasPixels, "Parachute", 516, 20);
  }

  // TITLE:
  /*                       |_____________________________________________________________| */
  drawColor = 0xFF5555FF;
  drawString(canvasPixels, "             TextUnkown's AdventureGrounds", 5, 5);
  drawColor = 0xFF55FFFF;
  drawString(canvasPixels, "             TextUnkown's AdventureGrounds", 4, 4);

  // GAME:
  int textHeight = 24;

  int iLine;
  for(iLine = 0; iLine < lineCount; ++iLine)
  {
    int line = lines[iLine];
    if (line >= 0)
    {
      if (line == LINE_DEATH_FALL || line == LINE_DEATH_GUN_1 || line == LINE_DEATH_GUN_2 || line == LINE_DEATH_SHOOT)
      {
        drawColor = 0xFF8888FF;
      }
      else if (line == LINE_HELP_1 || line == LINE_HELP_2)
      {
        drawColor = 0xFFAAFFAA;
      }
      else
      {
        drawColor = 0xFFFFFFFF;
      }
      drawString(canvasPixels, promptLines[line], 4, textHeight);
      textHeight += 10;
    }
    else if (line == LINE_BLANK)
    {
      textHeight += 10;
    }
    else if (line == LINE_GAMEOVER)
    {
      drawColor = 0xFF6666DD;
      drawString(canvasPixels, "GAME OVER,", 5, textHeight);
      drawColor = 0xFF8888FF;
      drawString(canvasPixels, "GAME OVER  Would you like to try again?", 4, textHeight);
      textHeight += 10;
    }
  }

  drawColor = 0xFF55DD55;
  drawString(canvasPixels, promptText, 8, textHeight);
  if (animTimer > 30)
  {
    drawString(canvasPixels, "|", 8 + (promptLength * 8), textHeight);
  }
  textHeight += 10;

}
