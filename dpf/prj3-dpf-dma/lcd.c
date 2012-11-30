
#define GPF0CON		(*(volatile unsigned int *)0xE0200120)
#define GPF1CON		(*(volatile unsigned int *)0xE0200140)
#define GPF2CON		(*(volatile unsigned int *)0xE0200160)
#define GPF3CON		(*(volatile unsigned int *)0xE0200180)

#define GPD0CON		(*(volatile unsigned int *)0xE02000A0)
#define GPD0DAT		(*(volatile unsigned int *)0xE02000A4)

#define CLK_SRC1	(*(volatile unsigned int *)0xe0100204)
#define CLK_DIV1	(*(volatile unsigned int *)0xe0100304)
#define DISPLAY_CONTROL	(*(volatile unsigned int *)0xe0107008)

#define VIDCON0		(*(volatile unsigned int *)0xF8000000)
#define VIDCON1		(*(volatile unsigned int *)0xF8000004)
#define VIDTCON2 	(*(volatile unsigned int *)0xF8000018)
#define WINCON0 	(*(volatile unsigned int *)0xF8000020)
#define WINCON2 	(*(volatile unsigned int *)0xF8000028)
#define SHADOWCON 	(*(volatile unsigned int *)0xF8000034)
#define VIDOSD0A 	(*(volatile unsigned int *)0xF8000040)
#define VIDOSD0B 	(*(volatile unsigned int *)0xF8000044)
#define VIDOSD0C 	(*(volatile unsigned int *)0xF8000048)

#define VIDW00ADD0B0 	(*(volatile unsigned int *)0xF80000A0)
#define VIDW00ADD1B0 	(*(volatile unsigned int *)0xF80000D0)

#define VIDTCON0 	(*(volatile unsigned int *)0xF8000010)
#define VIDTCON1 	(*(volatile unsigned int *)0xF8000014)

#define FB_ADDR		(0x22000000)
#define COL	480
#define ROW	272	

void lcd_init(void)
{
	// GPIO Functional as LCD Signals
	GPF0CON = 0x22222222;		// GPF0[7:0]
	GPF1CON = 0x22222222;		// GPF1[7:0]
	GPF2CON = 0x22222222;		// GPF2[7:0]
	GPF3CON = 0x22222222;		// GPF3[7:0]

	// XpwmTOUT1 GPD0_1 output high level
	// GPD0 Control Register (GPD0CON, R/W, Address = 0xE020_00A0)
	GPD0CON |= 1<<4;
	GPD0DAT |= 1<<1;

	// clock init (CLK_SRC1, CLK_DIV1 are optional)
	DISPLAY_CONTROL = 2<<0;		// 10: RGB=FIMD I80=FIMD ITU=FIMD

	// LCD SFR init
	// ENVID [1] Video output and the logic immediately enable/disable. 
	//	0 = Disable the video output and the Display control signal. 
	//	1 = Enable the video output and the Display control signal. 
	// ENVID_F [0] Video output and the logic enable/disable at current frame end. 
	//	0 = Disable the video output and the Display control signal. 
	//	1 = Enable the video output and the Display control signal.  
	// see 210.pdf p1228
	VIDCON0 |= 1<<0 | 1<<1 ;
	
	// CLKVAL_F [13:6] Determine the rates of VCLK and CLKVAL[7:0]  
	// VCLK = Video Clock Source / (CLKVAL+1)  where CLKVAL >= 1  
	VIDCON0 |= 1<<4;
	
	// LCD module para, see H43-HSD043I9W1.pdf p13
	VIDCON0 |= 14<<6;	// 166M/(14+1) = 11M < 12M(max)

	// LCD module para, see H43-HSD043I9W1.pdf p13
	// IHSYNC  [6]  Specifies the HSYNC pulse polarity. 
	//	0 = Normal               
	//	1 = Inverted 
	// IVSYNC  [5]  Specifies the VSYNC pulse polarity. 
	//	0 = Normal               
	//	1 = Inverted 	
	VIDCON1 |= 1<<5 | 1<<6;

	// LINEVAL [21:11] 
	// HOZVAL [10:0] 
	VIDTCON2 = (ROW - 1)<<11 | (COL - 1)<<0;	// 479*271
	
	// ENWIN_F [0] Video output and the logic immediately enable/disable. 
	//	0 = Disable the video output and the VIDEO control signal. 
	//	1 = Enable the video output and the VIDEO control signal. 
	WINCON0 |= 1<<0;
	
	// BPPMODE_F [5:2] Select the BPP (Bits Per Pixel) mode Window image.  
	// 1011 = unpacked 24 BPP (non-palletized R:8-G:8-B:8 )  
	WINCON0 |= 0xB<<2;

	// WSWP_F  [15]  Specifies the Word swap control bit. 
	// 0 = Swap Disable         
	// 1 = Swap Enable 
	WINCON0 |= 1<<15;	

	// left top pixel (0, 0)
	VIDOSD0A |= 0<<11;
	VIDOSD0A |= 0<<0;
	
	// right bottom pixel (479, 271)
	VIDOSD0B |= (COL - 1)<<11;
	VIDOSD0B |= (ROW - 1)<<0;
	
	// fb address
	VIDW00ADD0B0 = FB_ADDR;
	VIDW00ADD1B0 = FB_ADDR + ROW * COL * 4;
		
	// LCD module para, see H43-HSD043I9W1.pdf p13
#define HSPW 	(40 - 1)
#define HBPD 	(5 - 1)
#define	HFPD 	(2 - 1)
#define VSPW	(8 - 1)
#define VBPD 	(8 - 1)
#define VFPD 	(2 - 1)
	VIDTCON0 = VBPD<<16 | VFPD<<8 | VSPW<<0;
	VIDTCON1 = HBPD<<16 | HFPD<<8 | HSPW<<0;
		
	// C0_EN_F  0  Enables Channel 0. 
	//	0 = Disables          1 = Enables 
	SHADOWCON = 0x1;

	return;
}

#define lcd_draw_pixel(row, col, color)		*((int *)FB_ADDR + (row) * COL + (col)) = color

#define lcd_draw_pixel_v(row, col, color, fb_addr)		*((int *)fb_addr + (row) * COL + (col)) = color

#if 0
void lcd_draw_pixel(int row, int col, int color)
{
	int * pixel = (int *)FB_ADDR;

	*(pixel + row * COL + col) = color;	

	return;
} 
#endif

void lcd_clear_screen(int color)
{
	int i, j;
		
	for (i = 0; i < ROW; i++)
		for (j = 0; j < COL; j++)
		{
			lcd_draw_pixel(i, j, color);
		}

	return;
}

void lcd_draw_hline(int row, int col1, int col2, int color)
{
	int j;
	
	for (j = col1; j <= col2; j++)
	{
		lcd_draw_pixel(row, j, color);
	}

	return;
}

void lcd_draw_vline(int col, int row1, int row2, int color)
{
	int i;
	
	for (i = row1; i <= row2; i++)
	{
		lcd_draw_pixel(i, col, color);
	}

	return;
}

void lcd_draw_cross(int row, int col, int halflen, int color)
{
	lcd_draw_hline(row, col-halflen, col+halflen, color);
	
	lcd_draw_vline(col, row-halflen, row+halflen, color);
	
	return;
}

void lcd_draw_bmp(int bmp_file_addr)
{
	int i, j;
	char * p = (char *)bmp_file_addr;
	int blue, green, red;
	int color;

	// read bmp file
	// bmp file header is 54 bytes
	p += 54;
	
	for (i = 0; i < 272; i++)
		for (j = 0; j < 480; j++)
		{
			blue = *p++;
			green = *p++;
			red = *p++;
		
			color = red << 16 | green << 8 | blue << 0;
			
			lcd_draw_pixel(271-i, j, color);
		}

	return;
}		

void lcd_draw_bmp_v(int bmp_file_addr, int fb_addr)
{
	int i, j;
	char * p = (char *)bmp_file_addr;
	int blue, green, red;
	int color;

	// read bmp file
	// bmp file header is 54 bytes
	p += 54;
	
	for (i = 0; i < 272; i++)
		for (j = 0; j < 480; j++)
		{
			blue = *p++;
			green = *p++;
			red = *p++;
		
			color = red << 16 | green << 8 | blue << 0;
			
			lcd_draw_pixel_v(271-i, j, color, fb_addr);
		}

	return;
}		
