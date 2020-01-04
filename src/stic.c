/*
	This file is part of FreeIntv.

	FreeIntv is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	FreeIntv is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with FreeIntv.  If not, see http://www.gnu.org/licenses/
*/

#include "intv.h"
#include "memory.h"
#include "stic.h"

#include <stdio.h>

void drawBackground(void);
void drawSprites(int scanline);
void drawBorder(int scanline);
void drawBackgroundFGBG(int scanline);
void drawBackgroundColorStack(int scanline);

// Video chip: TMS9927 AY-3-8900-1
// http://spatula-city.org/~im14u2c/intv/jzintv-1.0-beta3/doc/programming/stic.txt
// http://spatula-city.org/~im14u2c/intv/tech/master.html

unsigned int STICMode;

int VBlank1;
int VBlank2;
int Cycles;
int DisplayEnabled;
int VerticalDelay;

unsigned int frame[352*224];

unsigned int scanBuffer[768]; // buffer for current scanline (352+32)*2
unsigned int collBuffer[768]; // buffer for collision -- made larger than needed to save checks

int delayH = 0; // Horizontal Delay
int delayV = 0; // Vertical Delay

int extendTop = 0;
int extendLeft = 0;

unsigned int CSP; // Color Stack Pointer
unsigned int color7 = 0xFFFCFF; // Copy of color 7 (for color squares mode)
unsigned int cscolors[4]; // color squares colors
unsigned int fgcard[20]; // cached colors for cards on current row
unsigned int bgcard[20]; // (used for normal color stack mode)
unsigned int colors[16] =
{
	0x0C0005, /* 0x000000; */ // Black
	0x002DFF, /* 0x0000FF; */ // Blue
	0xFF3E00, /* 0xFF0000; */ // Red
	0xC9D464, /* 0xCBFF65; */ // Tan
	0x00780F, /* 0x007F00; */ // Dark Green
	0x00A720, /* 0x00FF00; */ // Light Green 
	0xFAEA27, /* 0xFFFF00; */ // Yellow
	0xFFFCFF, /* 0xFFFFFF; */ // White
	0xA7A8A8, /* 0x7F7F7F; */ // Gray
	0x5ACBFF, /* 0x00FFFF; */ // Cyan
	0xFFA600, /* 0xFF9F00; */ // Orange
	0x3C5800, /* 0x7F7F00; */ // Brown
	0xFF3276, /* 0xFF3FFF; */ // Pink
	0xBD95FF, /* 0x7F7FFF; */ // Violet
	0x6CCD30, /* 0x7FFF00; */ // Bright green
	0xC81A7D  /* 0xFF007F; */ // Magenta
};

int reverse[256] = // lookup table to reverse the bits in a byte //
{
	0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0, 0x10, 0x90, 0x50, 0xD0, 0x30, 0xB0, 0x70, 0xF0,
	0x08, 0x88, 0x48, 0xC8, 0x28, 0xA8, 0x68, 0xE8, 0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8,
	0x04, 0x84, 0x44, 0xC4, 0x24, 0xA4, 0x64, 0xE4, 0x14, 0x94, 0x54, 0xD4, 0x34, 0xB4, 0x74, 0xF4,
	0x0C, 0x8C, 0x4C, 0xCC, 0x2C, 0xAC, 0x6C, 0xEC, 0x1C, 0x9C, 0x5C, 0xDC, 0x3C, 0xBC, 0x7C, 0xFC,
	0x02, 0x82, 0x42, 0xC2, 0x22, 0xA2, 0x62, 0xE2, 0x12, 0x92, 0x52, 0xD2, 0x32, 0xB2, 0x72, 0xF2,
	0x0A, 0x8A, 0x4A, 0xCA, 0x2A, 0xAA, 0x6A, 0xEA, 0x1A, 0x9A, 0x5A, 0xDA, 0x3A, 0xBA, 0x7A, 0xFA,
	0x06, 0x86, 0x46, 0xC6, 0x26, 0xA6, 0x66, 0xE6, 0x16, 0x96, 0x56, 0xD6, 0x36, 0xB6, 0x76, 0xF6,
	0x0E, 0x8E, 0x4E, 0xCE, 0x2E, 0xAE, 0x6E, 0xEE, 0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE,
	0x01, 0x81, 0x41, 0xC1, 0x21, 0xA1, 0x61, 0xE1, 0x11, 0x91, 0x51, 0xD1, 0x31, 0xB1, 0x71, 0xF1,
	0x09, 0x89, 0x49, 0xC9, 0x29, 0xA9, 0x69, 0xE9, 0x19, 0x99, 0x59, 0xD9, 0x39, 0xB9, 0x79, 0xF9,
	0x05, 0x85, 0x45, 0xC5, 0x25, 0xA5, 0x65, 0xE5, 0x15, 0x95, 0x55, 0xD5, 0x35, 0xB5, 0x75, 0xF5,
	0x0D, 0x8D, 0x4D, 0xCD, 0x2D, 0xAD, 0x6D, 0xED, 0x1D, 0x9D, 0x5D, 0xDD, 0x3D, 0xBD, 0x7D, 0xFD,
	0x03, 0x83, 0x43, 0xC3, 0x23, 0xA3, 0x63, 0xE3, 0x13, 0x93, 0x53, 0xD3, 0x33, 0xB3, 0x73, 0xF3,
	0x0B, 0x8B, 0x4B, 0xCB, 0x2B, 0xAB, 0x6B, 0xEB, 0x1B, 0x9B, 0x5B, 0xDB, 0x3B, 0xBB, 0x7B, 0xFB,
	0x07, 0x87, 0x47, 0xC7, 0x27, 0xA7, 0x67, 0xE7, 0x17, 0x97, 0x57, 0xD7, 0x37, 0xB7, 0x77, 0xF7,
	0x0F, 0x8F, 0x4F, 0xCF, 0x2F, 0xAF, 0x6F, 0xEF, 0x1F, 0x9F, 0x5F, 0xDF, 0x3F, 0xBF, 0x7F, 0xFF
};

void STICReset()
{
	STICMode = 1;
	VBlank1 = 0;
	VBlank2 = 0;
	SR1 = 0;
	Cycles = 0;
	DisplayEnabled = 0;
	CSP = 0x28;
}

void drawBorder(int scanline)
{
	int i;
	int cbit = 1<<9; // bit 9 - border collision 
	int color = colors[Memory[0x2C]]; // border color
	
	if(scanline>112) { return; }
	if(scanline<(8+(8*extendTop)) || scanline>=104) // top and bottom border
	{
		for(i=0; i<352; i++)
		{
			scanBuffer[i] = color;
			scanBuffer[i+384] = color;
			collBuffer[i] = cbit;
			collBuffer[i+384] = cbit;
		}
	}
	else // left and right border
	{
		for(i=0; i<16+(16*extendLeft); i++)
		{
			scanBuffer[i] = color;
			scanBuffer[i+336] = color;
			scanBuffer[i+384] = color;
			scanBuffer[i+384+336] = color;
			collBuffer[i] = cbit;
			collBuffer[i+336] = cbit;
			collBuffer[i+384] = cbit;
			collBuffer[i+384+336] = cbit;
		}
	}
}

void drawBackgroundFGBG(int scanline)
{
	int i; 
	int row, col; // row offset and column of current card
	int cardrow;  // which of the 8 rows of the current card to draw
	int card;     // BACKTAB card info
	unsigned int bgcolor;
	unsigned int fgcolor;
	int gram;     // 0-GROM, 1-GRAM
	int cardnum;  // card number (GRAM/GROM index)
	int gaddress; // card graphic address
	int gdata;    // current card graphic byte
	int cbit = 1<<8;   // bit 8 - collision bit for Background
	int x = delayH; // current pixel offset 

	// Tiled background is 20x12, cards are 8x8
	row = scanline / 8; // Which tile row? (Background is 96 lines high)
	row = row * 20; // find BACKTAB offset 

	cardrow = scanline % 8; // which line of this row of cards to draw

	// Draw cards
	for (col=0; col<20; col++) // for each card on the current row...
	{
		card = Memory[0x200+row+col]; // card info from BACKTAB

		fgcolor = colors[card & 0x07];
		bgcolor = colors[((card>>9)&0x03) | ((card>>11)&0x04) | ((card>>9)&0x08)]; // bits 12,13,10,9
		
		gram = (card>>11) & 0x01; // card is in 0-GROM or 1-GRAM
		cardnum = (card>>3) & 0x3F;
		gaddress = 0x3000 + (cardnum<<3) + (0x800 * gram);
		
		gdata = Memory[gaddress + cardrow]; // fetch current line of current card graphic

		for(i=7; i>=0; i--) // draw one line of card graphic
		{
			if(((gdata>>i)&1)==1)
			{
				// draw pixel
				scanBuffer[x] = fgcolor;
				scanBuffer[x+1] = fgcolor;
				scanBuffer[x+384] = fgcolor;
				scanBuffer[x+384+1] = fgcolor;
				// write to collision buffer 
				collBuffer[x] |= cbit;
				collBuffer[x+1] |= cbit;
				collBuffer[x+384] |= cbit;
				collBuffer[x+384+1] |= cbit;		
			}
			else
			{
				// draw background
				scanBuffer[x] = bgcolor;
				scanBuffer[x+1] = bgcolor;
				scanBuffer[x+384] = bgcolor;
				scanBuffer[x+384+1] = bgcolor;
			}		
			x+=2;
		}
	}
}

void drawBackgroundColorStack(int scanline)
{
	int i;
	unsigned int color1, color2;
	int cbit1, cbit2; 
	int row, col; // row offset and column of current card
	int cardrow;  // which of the 8 rows of the current card to draw
	int card;     // BACKTAB card info
	unsigned int bgcolor;
	unsigned int fgcolor;
	int gram;     // 0-GROM, 1-GRAM
	int cardnum;  // card number (GRAM/GROM index)
	int gaddress; // card graphic address
	int gdata;    // current card graphic byte
	int advcolor; // Flag - Advance CSP
	int cbit = 1<<8;   // bit 8 - collision bit for Background
	int x = delayH; // current pixel offset 

	// Tiled background is 20x12, cards are 8x8
	row = (scanline / 8); // Which tile row? (Background is 96 lines high)
	row = row * 20; // find BACKTAB offset 

	cardrow = scanline % 8; // which line of this row of cards to draw

	if(row==0 && cardrow==0) { CSP = 0x28; } // reset CSP on display of first card on screen
	
	// Draw cards
	for (col=0; col<20; col++) // for each card on the current row...
	{
		card = Memory[0x200+row+col]; // card info from BACKTAB

		if(((card>>11)&0x03)==2) // Color Squares Mode
		{
			// set colors
			colors[7] = colors[Memory[CSP] & 0x0F]; // color 7 is top of color stack
			color1 = card & 0x07;
			color2 = (card>>3) & 0x07;	
			if(cardrow>=4) // switch to lower squares colors
			{
				color1 = (card>>6) & 0x07;	                 // color 3
				color2 = ((card>>11)&0x04)|((card>>9)&0x03); // color 4
			}
			// color 7 does not interact with sprites
			cbit1 = cbit2 = cbit;
			if(color1==7) { cbit1=0; }
			if(color2==7) { cbit2=0; }
			color1 = colors[color1]; // set to rgb24 color
			color2 = colors[color2];
			colors[7] = color7; // restore color 7
			// draw squares
			for(i=0; i<8; i++)
			{
				scanBuffer[x+i] = color1;
				scanBuffer[x+i+8] = color2;
				scanBuffer[x+i+384] = color1;
				scanBuffer[x+i+384+8] = color2;
				collBuffer[x+i] |= cbit1;
				collBuffer[x+i+8] |= cbit2;
				collBuffer[x+i+384] |= cbit1;
				collBuffer[x+i+384+8] |= cbit2;
			}
			x+=16;

		}
		else // Color Stack Mode
		{
			gram = (card>>11) & 0x01; // GRAM or GROM?	
			if(cardrow == 0) // only advance CSP once per card, cache card colors for later scanlines
			{
				advcolor = (card>>13) & 0x01; // do we need to advance the CSP?
				CSP = (CSP+advcolor) & 0x2B; // cycles through 0x28-0x2B
				fgcard[col] = colors[(card&0x07)|((card>>9)&0x08)]; // bits 12, 2, 1, 0
				bgcard[col] = colors[Memory[CSP] & 0x0F];
			}

			fgcolor = fgcard[col];
			bgcolor = bgcard[col];

			cardnum = (card>>3) & 0xFF;
			if(gram) { cardnum = cardnum & 0x3F; }

			gaddress = 0x3000 + (cardnum<<3) + (0x800 * gram);
			
			gdata = Memory[gaddress + cardrow]; // fetch current line of current card graphic
			for(i=7; i>=0; i--) // draw one line of card graphic
			{
				if(((gdata>>i)&1)==1)
				{
					// draw pixel
					scanBuffer[x] = fgcolor;
					scanBuffer[x+1] = fgcolor;
					scanBuffer[x+384] = fgcolor;
					scanBuffer[x+384+1] = fgcolor;
					// write to collision buffer 
					collBuffer[x] |= cbit;
					collBuffer[x+1] |= cbit;
					collBuffer[x+384] |= cbit;
					collBuffer[x+384+1] |= cbit;		
				}
				else
				{
					// draw background
					scanBuffer[x] = bgcolor;
					scanBuffer[x+1] = bgcolor;
					scanBuffer[x+384] = bgcolor;
					scanBuffer[x+384+1] = bgcolor;
				}
				x+=2;
			}
		}
	}
}

void drawSprites(int scanline) // MOBs
{
	int i, j, k, x;
	int fgcolor;    // Foreground Color - (Ra bits 12, 2, 1, 0)
	int Rx, Ry, Ra; // sprite/MOB registers
	int gaddress;   // address of card / sprite data
	int gdata;      // current byte of sprite data
	int gdata2;     // current byte of sprite data (second row for half-height sprites)
	int card;       // card number - Ra bits 10-3
	int gram;       // sprite is in 1-GRAM or 0-GROM (Ra bit 11)
	int sizeX;      // 0-normal, 1-double width (Rx bit 10)
	int sizeY;      // 0-half height, 1-normal, 2-double, 3-quadrupal (Ry bits 9, 8)
	int flipX;      // (Ry bit 10)
	int flipY;      // (Ry bit 11)
	int posX;       // (Rx bits 7-0)
	int posY;       // (Ry bits 6-0)
	int yRes;       // 0-normal, 1-two tiles high (Ry bit 7)
	int priority;   // 0-normal, 1-behind background cards (Ra bit 13)
	int cbit;       // collision bit for collision buffer for collision detection

	int gfxheight;  // sprite is either 8 or 16 bytes (1 or 2 tiles) tall
	int spriterow;  // row of sprite data to draw

	if(scanline>104) { return; } // one line extra for bottom border collision

	for(i=7; i>=0; i--) // draw sprites 0-7 in reverse order
	{
		cbit = 1<<i; // set collision bit

		Rx = Memory[0x00+i]; // 14 bits ; -- -SVI xxxx xxxx ; Size, Visible, Interactive, X Position
		Ry = Memory[0x08+i]; // 14 bits ; -- YX42 Ryyy yyyy ; Flip Y, Flip X, Size 4, Size 2, Y Resolution, Y Position
		Ra = Memory[0x10+i]; // 14 bits ; PF Gnnn nnnn nFFF ; Priority, FG Color Bit 3, GRAM, n Card #, FG Color Bits 2-0

		gram = (Ra>>11) & 0x01;
		card = (Ra>>3) & 0xFF;
		if(gram==1) { card = card & 0x3F; } // ignore bits 6 and 7 if card is in GRAM
		gaddress = 0x3000 + (card<<3) + (0x800 * gram);

		yRes  = (Ry>>7) & 0x01;
		if(yRes==1)
		{
			// for double-y resolution sprites, the first card drops bit 0 from address
			gaddress = gaddress & 0xFFFE;
		}

		fgcolor = colors[((Ra>>9)&0x08)|(Ra&0x07)];
		sizeX = (Rx>>10) & 0x01;
		sizeY = (Ry>>8) & 0x03;
		flipX = (Ry>>10) & 0x01;
		flipY = (Ry>>11) & 0x01;
		posX  = Rx & 0xFF;
		posY  = Ry & 0x7F;
		priority = (Ra>>13) & 0x01;

		// if sprite x coordinate is 0 or >167, it's disabled
		// if it's not visible and not interactive, it's disabled
		if(posX==0 || posX>167 || ((Rx>>8)&0x03)==0) { continue; }

		// sprite height varies by sizeY and yRes.  When yRes is set, the size doubles.
		// sizeY will be 0,1,2,3, corresponding to heights of 4,8, 16, and 32
		// we can find this by left-shifting 4 by sizeY as 4<<0==4, ..., 4<<3==32 
		gfxheight = (4<<sizeY)<<yRes; // yres=0: 4,8,16,32 ; yres=1: 8,16,32,64
		
		// clear collisions in column 167 //
		collBuffer[167] = 0;
		collBuffer[167+384] = 0;

		if( (scanline>=posY) && (scanline<(posY+gfxheight)) ) // if sprite is on current row
		{ 	
			// find sprite graphics data for current row
			spriterow = (scanline - posY); 
			if(sizeY==0)
			{
				spriterow = spriterow * 2;
			}
			else
			{
				spriterow = spriterow >> (sizeY-1);
			}

			if(flipY)
			{
				spriterow = (7+(8*yRes)) - spriterow;
				gaddress = gaddress + spriterow; 
				gdata  = Memory[gaddress];
				gdata2 = Memory[gaddress - (sizeY==0)];
			}
			else
			{
				gaddress = gaddress + spriterow; 
				gdata  = Memory[gaddress];
				gdata2 = Memory[gaddress + (sizeY==0)];
			}

			if(flipX)
			{
				gdata  = reverse[gdata];
				gdata2 = reverse[gdata2];
			}

			// draw sprite row //
			x = (delayH-16) + (posX * 2); // pixels are 2x2 to accomodate half-height pixels

			for(j=0; j<2; j++)
			{
				for(k=7; k>=0; k--)
				{
					if(((gdata>>k) & 1)==0) // skip ahead if pixel is not visible
					{
						x+=2+(2*sizeX);
						continue;
					} 
					
					// set collision and collision buffer bits //
					if((Rx>>8)&1) // if sprite is interactive
					{
						Memory[0x18+i] |= collBuffer[x];
						Memory[0x18+i] |= collBuffer[x+1];
						Memory[0x18+i] |= collBuffer[x+2*sizeX];
						Memory[0x18+i] |= collBuffer[x+3*sizeX];
						collBuffer[x] |= cbit;
						collBuffer[x+1] |= cbit;
						collBuffer[x+2*sizeX] |= cbit; // for double width
						collBuffer[x+3+sizeX] |= cbit;
					}
					
					if(priority && ((collBuffer[x]>>8)&1)) // don't draw if sprite is behind background
					{
						x+=2+(2*sizeX);
						continue;
					} 
					
					// draw sprite //
					if((Rx>>9)&1) // if sprite is visible
					{
						scanBuffer[x] = fgcolor;
						scanBuffer[x+1] = fgcolor;
						scanBuffer[x+2*sizeX] = fgcolor; // for double width
						scanBuffer[x+3*sizeX] = fgcolor;
						x+=2+(2*sizeX);
					}
				}
				gdata = gdata2;  // for second half-pixel row  //
				x = (delayH-16) + 384 + (posX * 2); // for second half-pixel row //
			}
		}
	}
}

void STICDrawFrame()
{
	int i, j;

	extendTop = (Memory[0x32]>>1)&0x01;
	
	extendLeft = (Memory[0x32])&0x01;

	delayV = 8 + ((Memory[0x31])&0x7);

	delayH = 8 + ((Memory[0x30])&0x7);

	delayH = delayH * 2;

	int row, offset;

	offset = 0;
	for(row=0; row<112; row++)
	{
		// draw border for collision
		drawBorder(row);

		// draw backtab
		if(row>=delayV && row<(96+delayV))
		{
			if(STICMode==0) // Foreground/Background Mode
			{
				drawBackgroundFGBG(row-delayV);	
			}
			else // Color Stack Modes
			{
				drawBackgroundColorStack(row-delayV);	
			}
		}

		// draw MOBs
		drawSprites((row-delayV)+8);

		// draw border
		drawBorder(row);

		for(i=0; i<352; i++) // write scan line to frame buffer
		{
			frame[offset] = scanBuffer[i];
			frame[offset+352] = scanBuffer[i+384];
			offset++;
		}
		offset+=352;

		for(i=0; i<768; i++)
		{
			scanBuffer[i] = 0; // clear scanBuffer;
			collBuffer[i] = 0; // clear collbuffer;
		}
	}

	// complete collisions e.g.:
	// if MOB2 hits MOB1, MOB1 should also hit MOB2
	for(i=0; i<8; i++)
	{
		// clear any self-interactions
		Memory[0x18+i] &= (1<<i)^0x3FFF;

		// copy collisions to colliding sprites
		for(j=0; j<8; j++)
		{
			if(i==j) { continue; }
			if(((Memory[0x18+i]>>j) & 1) == 1)
			{
				Memory[0x18+j] |= (1<<i);
			}
		}
	}
}
