#ifndef _LCD_UI_H__
#define _LCD_UI_H__

#include "gd32f4xx.h"

//画笔颜色
#define WHITE         	 0xFFFF
#define BLACK         	 0x0000	  
#define BLUE         	 0x001F  
#define BRED             0XF81F
#define GRED 			 0XFFE0
#define GBLUE			 0X07FF
#define RED           	 0xF800
#define MAGENTA       	 0xF81F
#define GREEN         	 0x07E0
#define CYAN          	 0x7FFF
#define YELLOW        	 0xFFE0
#define BROWN 			 0XBC40 //棕色
#define BRRED 			 0XFC07 //棕红色
#define GRAY  			 0X8430 //灰色
//GUI颜色

#define DARKBLUE      	 0X01CF	//深蓝色
#define LIGHTBLUE      	 0X7D7C	//浅蓝色  
#define GRAYBLUE       	 0X5458 //灰蓝色
//以上三色为PANEL的颜色 
 
#define LIGHTGREEN     	 0X841F //浅绿色
//#define LIGHTGRAY        0XEF5B //浅灰色(PANNEL)
#define LGRAY 			 0XC618 //浅灰色(PANNEL),窗体背景色

#define LGRAYBLUE        0XA651 //浅灰蓝色(中间层颜色)
#define LBBLUE           0X2B12 //浅棕蓝色(选择条目的反色)


//画点
void tli_draw_point(uint16_t x,uint16_t y,uint16_t color);	
//画线
void tli_draw_line(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey,uint16_t color);
//画矩形
void tli_draw_Rectangle(uint16_t sx,uint16_t sy,uint16_t ex,uint16_t ey,uint16_t color, uint16_t fill);
//画圆
void tli_draw_circle(int xc, int yc,uint16_t c,int r, int fill);
//画三角形
void tli_draw_triange(uint16_t x0,uint16_t y0,uint16_t x1,uint16_t y1,uint16_t x2,uint16_t y2, uint16_t color,uint16_t fill);
//将一个点放大
void point_enlargement(uint16_t x, uint16_t y, uint16_t color, char magnify);
//显示单个字符
void tli_show_char(uint16_t x,uint16_t y,uint16_t fc, uint16_t bc, uint8_t num,uint8_t size,uint8_t mode);
//显示字符串
void tli_show_string(uint16_t x,uint16_t y,uint16_t fc,uint16_t bc,uint8_t size,uint8_t *p,uint8_t mode);
//显示单个16*16大小的字体
void tli_show_font16(uint16_t x, uint16_t y, uint16_t fc, uint16_t bc, uint8_t *s,uint8_t mode);
//显示单个24*24大小的字体
void tli_show_font24(uint16_t x, uint16_t y, uint16_t fc, uint16_t bc, uint8_t *s,uint8_t mode);
//显示单个32*32大小的字体
void tli_show_font32(uint16_t x, uint16_t y, uint16_t fc, uint16_t bc, uint8_t *s,uint8_t mode);
//显示中文字符串
void tli_show_Chinese_string(uint16_t x, uint16_t y, uint16_t fc, uint16_t bc, uint8_t *str,uint8_t size,uint8_t mode);

void tli_show_picture(uint16_t x,uint16_t y,uint16_t w, uint16_t h,const unsigned char pic[]); //显示40*40 QQ图片
#endif
