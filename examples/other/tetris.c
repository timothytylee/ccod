#!/usr/bin/ccod
#pragma CCOD:script no

/* tetris.c
******************************************************************************
	tetris.c - yet another tetris game
	Christopher Giese <geezer[AT]execpc.com>

	Release date 8/12/98. Distribute freely. ABSOLUTELY NO WARRANTY.

	Compile with DJGPP, Turbo C, or GCC under Linux.

	To run under DOS, you need ANSI.SYS.
*****************************************************************************/
#include	<stdlib.h>	/* rand() */
#include	<stdio.h>	/* printf(), setvbuf(), stdout */

#if defined(linux)
/* struct termios (etc.), tcgetattr(), tcsetattr() */
#include	<termios.h>
#include	<sys/time.h>	/* struct timeval */
/* ??? - fd_set, FD_ZERO(), FD_SET() select() */

#elif defined(__TURBOC__) || defined(__DJGPP__)
#include	<conio.h>	/* kbhit() getch() */
#include	<dos.h>		/* delay() */

#else
#error Not Linux/GCC, DJGPP, nor Turbo C. Sorry.
#endif

#define		KEY_QUIT	1
#define		KEY_QUIT	1
#define		KEY_CW		2
#define		KEY_CCW		3
#define		KEY_RIGHT	4
#define		KEY_LEFT	5
#define		KEY_UP		6
#define		KEY_DOWN	7

/* dimensions of playing area */
#define		SCN_WID		15
#define		SCN_HT		20
/* direction vectors */
#define		DIR_UP		{ 0, -1 }
#define		DIR_DN		{ 0, +1 }
#define		DIR_LT		{ -1, 0 }
#define		DIR_RT		{ +1, 0 }
#define		DIR_UP2		{ 0, -2 }
#define		DIR_DN2		{ 0, +2 }
#define		DIR_LT2		{ -2, 0 }
#define		DIR_RT2		{ +2, 0 }
/* ANSI colors */
#define		COLOR_BLACK	0
#define		COLOR_RED	1
#define		COLOR_GREEN	2
#define		COLOR_YELLOW	3
#define		COLOR_BLUE	4
#define		COLOR_MAGENTA	5
#define		COLOR_CYAN	6
#define		COLOR_WHITE	7

typedef struct
{	short int DeltaX, DeltaY; } vector;

typedef struct
{	char Plus90, Minus90;	/* pointer to shape rotated +/- 90 degrees */
	char Color;		/* shape color */
	vector Dir[4]; } shape;	/* drawing instructions for this shape */

shape Shapes[]=
/* shape #0:			cube */
{	{ 0, 0, COLOR_BLUE, { DIR_UP, DIR_RT, DIR_DN, DIR_LT }},
/* shapes #1 & #2:		bar */
	{ 2, 2, COLOR_GREEN, { DIR_LT, DIR_RT, DIR_RT, DIR_RT }},
	{ 1, 1, COLOR_GREEN, { DIR_UP, DIR_DN, DIR_DN, DIR_DN }},
/* shapes #3 & #4:		'Z' shape */
	{ 4, 4, COLOR_CYAN, { DIR_LT, DIR_RT, DIR_DN, DIR_RT }},
	{ 3, 3, COLOR_CYAN, { DIR_UP, DIR_DN, DIR_LT, DIR_DN }},
/* shapes #5 & #6:		'S' shape */
	{ 6, 6, COLOR_RED, { DIR_RT, DIR_LT, DIR_DN, DIR_LT }},
	{ 5, 5, COLOR_RED, { DIR_UP, DIR_DN, DIR_RT, DIR_DN }},
/* shapes #7, #8, #9, #10:	'J' shape */
	{ 8, 10, COLOR_MAGENTA, { DIR_RT, DIR_LT, DIR_LT, DIR_UP }},
	{ 9, 7, COLOR_MAGENTA, { DIR_UP, DIR_DN, DIR_DN, DIR_LT }},
	{ 10, 8, COLOR_MAGENTA, { DIR_LT, DIR_RT, DIR_RT, DIR_DN }},
	{ 7, 9, COLOR_MAGENTA, { DIR_DN, DIR_UP, DIR_UP, DIR_RT }},
/* shapes #11, #12, #13, #14:	'L' shape */
	{ 12, 14, COLOR_YELLOW, { DIR_RT, DIR_LT, DIR_LT, DIR_DN }},
	{ 13, 11, COLOR_YELLOW, { DIR_UP, DIR_DN, DIR_DN, DIR_RT }},
	{ 14, 12, COLOR_YELLOW, { DIR_LT, DIR_RT, DIR_RT, DIR_UP }},
	{ 11, 13, COLOR_YELLOW, { DIR_DN, DIR_UP, DIR_UP, DIR_LT }},
/* shapes #15, #16, #17, #18:	'T' shape */
	{ 16, 18, COLOR_WHITE, { DIR_UP, DIR_DN, DIR_LT, DIR_RT2 }},
	{ 17, 15, COLOR_WHITE, { DIR_LT, DIR_RT, DIR_UP, DIR_DN2 }},
	{ 18, 16, COLOR_WHITE, { DIR_DN, DIR_UP, DIR_RT, DIR_LT2 }},
	{ 15, 17, COLOR_WHITE, { DIR_RT, DIR_LT, DIR_DN, DIR_UP2 }}};

char Dirty[SCN_HT], Screen[SCN_WID][SCN_HT];
/*////////////////////////////////////////////////////////////////////////////
			ANSI GRAPHIC OUTPUT
////////////////////////////////////////////////////////////////////////////*/
/*****************************************************************************
	name:	refresh
	action:	updates display device based on contents of global
		char array Screen[][]. Updates only those rows
		marked Dirty[]
*****************************************************************************/
void refresh(void)
{	char XPos, YPos;

	for(YPos=0; YPos < SCN_HT; YPos++)
	{	if(!Dirty[YPos]) continue;
/* gotoxy(0, YPos) */
		printf("\x1B[%d;1H", YPos + 1);
		for(XPos=0; XPos < SCN_WID; XPos++)
/* 0xDB is a solid rectangular block in the PC character set */
			printf("\x1B[%dm\xDB\xDB", 30 + Screen[XPos][YPos]);
//			printf("\x1B[%dm**", 30 + Screen[XPos][YPos]);
		Dirty[YPos]=0; }
/* reset foreground color to gray */
	printf("\x1B[37m"); }
/*////////////////////////////////////////////////////////////////////////////
			GRAPHIC CHUNK DRAW & HIT DETECT
////////////////////////////////////////////////////////////////////////////*/
/*****************************************************************************
	name:	blockDraw
	action:	draws one graphic block in display buffer at
		position (XPos, YPos)
*****************************************************************************/
void blockDraw(char XPos, char YPos, unsigned Color)
{	if(XPos >= SCN_WID) XPos=SCN_WID - 1;
	if(YPos >= SCN_HT) YPos=SCN_HT - 1;
	Color &= 7;

	Screen[XPos][YPos]=Color;
	Dirty[YPos]=1; }	/* this row has been modified */
/*****************************************************************************
	name:	blockHit
	action:	determines if coordinates (XPos, YPos) are already
		occupied by a graphic block
	returns:color of graphic block at (XPos, YPos) (zero if black/
		empty)
*****************************************************************************/
char blockHit(char XPos, char YPos)
{	return(Screen[XPos][YPos]); }
/*////////////////////////////////////////////////////////////////////////////
			SHAPE DRAW & HIT DETECT
////////////////////////////////////////////////////////////////////////////*/
/*****************************************************************************
	name:	shapeDraw
	action:	draws shape WhichShape in display buffer at
		position (XPos, YPos)
*****************************************************************************/
void shapeDraw(unsigned XPos, unsigned YPos, char WhichShape)
{	char Index;

	for(Index=0; Index < 4; Index++)
	{	blockDraw(XPos, YPos, Shapes[WhichShape].Color);
		XPos += Shapes[WhichShape].Dir[Index].DeltaX;
		YPos += Shapes[WhichShape].Dir[Index].DeltaY; }
	blockDraw(XPos, YPos, Shapes[WhichShape].Color); }
/*****************************************************************************
	name:	shapeErase
	action:	erases shape WhichShape in display buffer at
		position (XPos, YPos) by setting its color to zero
*****************************************************************************/
void shapeErase(unsigned XPos, unsigned YPos, char WhichShape)
{	char Index;

	for(Index=0; Index < 4; Index++)
	{	blockDraw(XPos, YPos, COLOR_BLACK);
		XPos += Shapes[WhichShape].Dir[Index].DeltaX;
		YPos += Shapes[WhichShape].Dir[Index].DeltaY; }
	blockDraw(XPos, YPos, COLOR_BLACK); }
/*****************************************************************************
	name:	shapeHit
	action:	determines if shape WhichShape would collide with
		something already drawn in display buffer if it
		were drawn at position (XPos, YPos)
	returns:nonzero if hit, zero if nothing there
*****************************************************************************/
char shapeHit(unsigned XPos, unsigned YPos, char WhichShape)
{	char Index;

	for(Index=0; Index < 4; Index++)
	{	if(blockHit(XPos, YPos)) return(1);
		XPos += Shapes[WhichShape].Dir[Index].DeltaX;
		YPos += Shapes[WhichShape].Dir[Index].DeltaY; }
	if(blockHit(XPos, YPos)) return(1);
	return(0); }
/*////////////////////////////////////////////////////////////////////////////
			MAIN ROUTINES
////////////////////////////////////////////////////////////////////////////*/
/*****************************************************************************
	name:	screenInit
	action:	clears display buffer, marks all rows dirty,
		set raw keyboard mode
*****************************************************************************/
void screenInit(void)
{	unsigned XPos, YPos;

	for(YPos=0; YPos < SCN_HT; YPos++)
	{	Dirty[YPos]=1;	/* force entire screen to be redrawn */
		for(XPos=1; XPos < (SCN_WID - 1); XPos++)
			Screen[XPos][YPos]=0;
		Screen[0][YPos]=Screen[SCN_WID - 1][YPos]=COLOR_BLUE; }
	for(XPos=0; XPos < SCN_WID; XPos++)
		Screen[XPos][0]=Screen[XPos][SCN_HT - 1]=COLOR_BLUE; }
/*****************************************************************************
*****************************************************************************/
void collapse(void)
{	char SolidRow[SCN_HT], SolidRows;
	int Row, Col, Temp;

/* determine which rows are solidly filled */
	SolidRows=0;
	for(Row=1; Row < SCN_HT - 1; Row++)
	{	Temp=0;
		for(Col=1; Col < SCN_WID - 1; Col++)
			if(Screen[Col][Row]) Temp++;
		if(Temp == SCN_WID - 2)
		{	SolidRow[Row]=1;
			SolidRows++; }
		else SolidRow[Row]=0; }
	if(SolidRows == 0) return;
/* collapse them */
	for(Temp=Row=SCN_HT - 2; Row > 0; Row--, Temp--)
	{	while(SolidRow[Temp]) Temp--;
		if(Temp < 1)
		{	for(Col=1; Col < SCN_WID - 1; Col++)
				Screen[Col][Row]=COLOR_BLACK; }
		else
		{	for(Col=1; Col < SCN_WID - 1; Col++)
				Screen[Col][Row]=Screen[Col][Temp]; }
		Dirty[Row]=1; }
	refresh(); }
/*****************************************************************************
	name:	start
*****************************************************************************/
#if defined(linux)
struct termios OldKbdMode, NewKbdMode;

void start(void)
{/* set raw console mode (not canonical mode, with buffer size = 1 byte */
	tcgetattr(0, &OldKbdMode);
	memcpy(&NewKbdMode, &OldKbdMode, sizeof(struct termios));
	NewKbdMode.c_lflag &= (~ICANON);
	NewKbdMode.c_lflag &= (~ECHO);
	NewKbdMode.c_cc[VTIME]=0;
	NewKbdMode.c_cc[VMIN]=1;
	tcsetattr(0, TCSANOW, &NewKbdMode);
	setvbuf(stdout, NULL, _IONBF, 0);
/* set PC character set */
	printf("\x1B[11m"); }

#elif defined(__DJGPP__)
void start(void)
{	setvbuf(stdout, NULL, _IONBF, 0); }

#elif defined (__TURBOC__)
void start(void)
{	}

#endif
/*****************************************************************************
	name:	end
*****************************************************************************/
#if defined(linux)
void end(void)
{/* restore Linux charset and console */
	printf("\x1B[10m");
	tcsetattr(0, TCSANOW, &OldKbdMode); }

#elif defined(__TURBOC__) || defined(__DJGPP__)
void end(void)
{	}

#endif
/*****************************************************************************
	name:	getKey
*****************************************************************************/
#if defined(linux)
int getKey(void)
{	static struct timeval Timeout1;
	struct timeval Timeout2;
	fd_set ReadHandles;
	int Temp;

/* Watch stdin (fd 0) to see when it has input. */
	FD_ZERO(&ReadHandles);
	FD_SET(0, &ReadHandles);
	Temp=select(1, &ReadHandles, NULL, NULL, &Timeout1);
/* uh oh */
	if(Temp < 0)
BAD_SEL:{	end();
		printf("select() failed in getKey()\n");
		exit(1); }
/* timed out, reset Timeout1 and return 0 */
	if(Temp == 0)
	{	Timeout1.tv_sec=0;
		Timeout1.tv_usec=250000ul;
		return(0); }
/* activity at stdin, read char (getchar() won't work) */
	if(read(0, &Temp, 1) != 1)
BAD_RD:	{	end();
		printf("read() failed in getKey()\n");
		exit(1); }
/* 1, 2, q, or Q ? */
	if(Temp == 'q' || Temp == 'Q') return(KEY_QUIT);
	if(Temp == '1') return(KEY_CCW);
	if(Temp == '2') return(KEY_CW);
/* if Esc, check for arrow keys */
	if(Temp == 27)
	{	FD_ZERO(&ReadHandles);
		FD_SET(0, &ReadHandles);
/* return immediately if another byte is not available */
		Timeout2.tv_sec=Timeout2.tv_usec=0;
		Temp=select(1, &ReadHandles, NULL, NULL, &Timeout2);
		if(Temp < 0) goto BAD_SEL;
/* only Esc, no other key */
		if(Temp == 0) return(KEY_QUIT);
/* there's something after the Esc... */
		if(read(0, &Temp, 1) != 1) goto BAD_RD;
/* ...but it's not the ANSI [ character: return 0 */
		if(Temp != '[') return(0);
		if(read(0, &Temp, 1) != 1) goto BAD_RD;
		if(Temp == 'D') return(KEY_LEFT);
		if(Temp == 'C') return(KEY_RIGHT);
		if(Temp == 'A') return(KEY_UP);
		if(Temp == 'B') return(KEY_DOWN); }
	return(0); }

#elif defined(__TURBOC__) || defined(__DJGPP__)
int getKey(void)
{	static int Timeout;
	int Temp;

/* await keypress with timeout */
	for(; Timeout; Timeout--)
	{	delay(1);
		if(kbhit()) break; }
/* timed out, reset Timeout and return 0 */
	if(Timeout == 0)
	{	Timeout=225;
		return(0); }
/* get key */
	Temp=getch();
	if(Temp == 0) Temp=0x100 | getch();
	switch(Temp)
	{case 27:
	case 'q':
	case 'Q':
		return(KEY_QUIT);
	case '1':
		return(KEY_CCW);
	case '2':
		return(KEY_CW);
	case 0x148:
		return(KEY_UP);
	case 0x14B:
		return(KEY_LEFT);
	case 0x150:
		return(KEY_DOWN);
	case 0x14D:
		return(KEY_RIGHT); }
	return(0); }

#endif
/*****************************************************************************
	name:	main
*****************************************************************************/
void main(void)
{	char Fell, NewShape, NewX, NewY;
	char Shape, X, Y;
	int Key;

	start();
#if defined(linux) || defined(__DJGPP__)
	srandom(time(NULL));
#else
	randomize();
#endif
/* banner screen */
	printf("\x1B[2J""\x1B[1;%dH""TETRIS by Alexei Pazhitnov",
		SCN_WID * 2 + 2);
	printf("\x1B[2;%dH""Software by Chris Giese", SCN_WID * 2 + 2);
	printf("\x1B[4;%dH""'1' and '2' rotate shape", SCN_WID * 2 + 2);
	printf("\x1B[5;%dH""Arrow keys move shape", SCN_WID * 2 + 2);
	printf("\x1B[6;%dH""Esc or Q quits", SCN_WID * 2 + 2);
NEW:	printf("\x1B[9;%dH""Press any key to begin", SCN_WID * 2 + 2);
//	read(0, &Key, 1);
	printf("\x1B[8;%dH""                      ", SCN_WID * 2 + 2);
	printf("\x1B[9;%dH""                      ", SCN_WID * 2 + 2);
	screenInit();
	goto FOO;

	while(1)
	{	Fell=0;
		NewShape=Shape;
		NewX=X;
		NewY=Y;
		Key=getKey();
		if(Key == 0)
		{	NewY++;
			Fell=1; }
		else
		{	if(Key == KEY_QUIT) break;
			if(Key == KEY_CCW)
				NewShape=Shapes[Shape].Plus90;
			else if(Key == KEY_CW)
				NewShape=Shapes[Shape].Minus90;
			else if(Key == KEY_LEFT)
			{	if(X) NewX=X - 1; }
			else if(Key == KEY_RIGHT)
			{	if(X < SCN_WID - 1) NewX=X + 1; }
/*			else if(Key == KEY_UP)
			{	if(Y) NewY=Y - 1; } 	cheat */
			else if(Key == KEY_DOWN)
			{	if(Y < SCN_HT - 1) NewY=Y + 1; }
			Fell=0; }
/* if nothing has changed, skip the bottom half of this loop */
		if((NewX == X) && (NewY == Y) && (NewShape == Shape))
			continue;
/* otherwise, erase old shape from the old pos'n */
		shapeErase(X, Y, Shape);
/* hit anything? */
		if(shapeHit(NewX, NewY, NewShape) == 0)
/* no, update pos'n */
		{	X=NewX;
			Y=NewY;
			Shape=NewShape; }
/* yes -- did the piece hit something while falling on its own? */
		else if(Fell)
/* yes, draw it at the old pos'n... */
		{	shapeDraw(X, Y, Shape);
/* ... and spawn new shape */
FOO:			Y=3;
			X=SCN_WID / 2;
			Shape=rand() % 19;
			collapse();
/* if newly spawned shape hits something, game over */
			if(shapeHit(X, Y, Shape))
			{	printf("\x1B[8;%dH\x1B[1m       "
					"GAME OVER\x1B[0m", SCN_WID * 2 + 2);
				goto NEW; }}
/* hit something because of user movement/rotate OR no hit: just redraw it */
		shapeDraw(X, Y, Shape);
		refresh(); }
	end(); }
