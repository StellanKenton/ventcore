#include "RGBGui.h"
#include "RGBLCD.h"

/**********************************************************
 * 函 数 名 称：tli_draw_point
 * 函 数 功 能：画点
 * 传 入 参 数：(x,y)：起点坐标
 * 				color：点的颜色
 * 函 数 返 回：无
 * 作       者：LCKFB
 * 备       注：无
**********************************************************/
void tli_draw_point(uint16_t x,uint16_t y,uint16_t color)
{ 
    *(ltdc_lcd_framebuf + (LCD_WIDTH * y + x ) ) = color;
}

/**********************************************************
 * 函 数 名 称：tli_draw_line
 * 函 数 功 能：画线
 * 传 入 参 数：(sx,sy)：起点坐标
 * 				(ex,ey)：终点坐标
 * 函 数 返 回：无
 * 作       者：LCKFB
 * 备       注：无
**********************************************************/
void tli_draw_line(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey,uint16_t color)
{
	uint16_t t; 
	int xerr=0,yerr=0,delta_x,delta_y,distance; 
	int incx,incy,uRow,uCol; 

	delta_x=ex-sx; //计算坐标增量 
	delta_y=ey-sy; 
	uRow=sx; 
	uCol=sy; 
	if(delta_x>0)incx=1; //设置单步方向 
	else if(delta_x==0)incx=0;//垂直线 
	else {incx=-1;delta_x=-delta_x;} 
	if(delta_y>0)incy=1; 
	else if(delta_y==0)incy=0;//水平线 
	else{incy=-1;delta_y=-delta_y;} 
	if( delta_x>delta_y)distance=delta_x; //选取基本增量坐标轴 
	else distance=delta_y; 
	for(t=0;t<=distance+1;t++ )//画线输出 
	{  
		tli_draw_point(uRow,uCol,color);//画点 
		xerr+=delta_x ; 
		yerr+=delta_y ; 
		if(xerr>distance) 
		{ 
			xerr-=distance; 
			uRow+=incx; 
		} 
		if(yerr>distance) 
		{ 
			yerr-=distance; 
			uCol+=incy; 
		} 
	}  
} 
/**********************************************************
 * 函 数 名 称：tli_draw_Rectangle
 * 函 数 功 能：画矩形填充
 * 传 入 参 数：(sx,sy) ：起点坐标
 * 			    (sx,sy) ：终点坐标
 * 				color：笔画颜色
* 				fill：填充标志  =1填充颜色  =0不填充
 * 函 数 返 回：无
 * 作       者：LCKFB
 * 备       注：无
**********************************************************/
void tli_draw_Rectangle(uint16_t sx,uint16_t sy,uint16_t ex,uint16_t ey,uint16_t color, uint16_t fill)
{
	int i=0, j=0;
	if( fill )
	{
		for( i = sx; i < ex; i++ )
		{
			for( j = sy; j < ey; j++ )
			{
				tli_draw_point(i,j,color);
			}
		}
	}
	else
	{
		tli_draw_line(sx,sy,ex,sy,color);
		tli_draw_line(sx,sy,sx,ey,color);
		tli_draw_line(sx,ey,ex,ey,color);
		tli_draw_line(ex,sy,ex,ey,color);
	}
}

/**********************************************************
 * 函 数 名 称：_draw_circle_8
 * 函 数 功 能：8对称性画圆算法(内部调用)
 * 传 入 参 数：  (xc,yc):圆中心坐标
                    (x,y):光标相对于圆心的坐标
 * 					    c:点的颜色
 * 函 数 返 回：无
 * 作       者：LCKFB
 * 备       注：内部调用
**********************************************************/
static void _draw_circle_8(int xc, int yc, int x, int y, uint16_t c)
{
	tli_draw_point(xc + x, yc + y, c);

	tli_draw_point(xc - x, yc + y, c);

	tli_draw_point(xc + x, yc - y, c);

	tli_draw_point(xc - x, yc - y, c);

	tli_draw_point(xc + y, yc + x, c);

	tli_draw_point(xc - y, yc + x, c);

	tli_draw_point(xc + y, yc - x, c);

	tli_draw_point(xc - y, yc - x, c);
}

/**********************************************************
 * 函 数 名 称：tli_draw_circle
 * 函 数 功 能：在指定位置画一个指定大小的圆
 * 传 入 参 数：  (xc,yc) :圆中心坐标
 * 						c:填充的颜色
 *                  	r:圆半径
 *                   fill:填充判断标志，1-填充，0-不填充
 * 函 数 返 回：无
 * 作       者：LCKFB
 * 备       注：无
**********************************************************/
void tli_draw_circle(int xc, int yc,uint16_t c,int r, int fill)
{
	int x = 0, y = r, yi, d;

	d = 3 - 2 * r;

	if (fill) 
	{
		// 如果填充（画实心圆）
		while (x <= y) {
			for (yi = x; yi <= y; yi++)
				_draw_circle_8(xc, yc, x, yi, c);

			if (d < 0) {
				d = d + 4 * x + 6;
			} else {
				d = d + 4 * (x - y) + 10;
				y--;
			}
			x++;
		}
	} else 
	{
		// 如果不填充（画空心圆）
		while (x <= y) {
			_draw_circle_8(xc, yc, x, y, c);
			if (d < 0) {
				d = d + 4 * x + 6;
			} else {
				d = d + 4 * (x - y) + 10;
				y--;
			}
			x++;
		}
	}
}

/**********************************************************
 * 函 数 名 称：_swap
 * 函 数 功 能：数据交换
 * 传 入 参 数：a=交换方1  b=交换方2
 * 函 数 返 回：无
 * 作       者：LCKFB
 * 备       注：仅内部使用
**********************************************************/
static void _swap(uint16_t *a, uint16_t *b)
{
	uint16_t tmp;
	tmp = *a;
	*a = *b;
	*b = tmp;
}

/**********************************************************
 * 函 数 名 称：tli_draw_triange
 * 函 数 功 能：画三角形
 * 传 入 参 数：(x0,y0)：第一个边角的坐标
 * 				(x1,y1)：顶点的坐标
 * 				(x2,y2)：第二个边角的坐标
 * 				color：三角形颜色
 * 				fill:填充判断标志，1-填充，0-不填充
 * 函 数 返 回：
 * 作       者：LCKFB
 * 备       注：
**********************************************************/

void tli_draw_triange(uint16_t x0,uint16_t y0,uint16_t x1,uint16_t y1,uint16_t x2,uint16_t y2, uint16_t color,uint16_t fill)
{
	uint16_t a, b, y, last;
	int dx01, dy01, dx02, dy02, dx12, dy12;
	long sa = 0;
	long sb = 0;

	if( fill == 0 )
	{
		tli_draw_line(x0,y0,x1,y1,color);
		tli_draw_line(x1,y1,x2,y2,color);
		tli_draw_line(x2,y2,x0,y0,color);
	}
	else
	{
		if (y0 > y1) 
		{
			_swap(&y0,&y1); 
			_swap(&x0,&x1);
		}
		if (y1 > y2) 
		{
			_swap(&y2,&y1); 
			_swap(&x2,&x1);
		}
		if (y0 > y1) 
		{
			_swap(&y0,&y1); 
			_swap(&x0,&x1);
		}
		if(y0 == y2) 
		{ 
			a = b = x0;
			if(x1 < a)
		{
			a = x1;
		}
		else if(x1 > b)
		{
			b = x1;
		}
		if(x2 < a)
		{
			a = x2;
		}
			else if(x2 > b)
		{
			b = x2;
		}
		tli_draw_Rectangle(a,y0,b,y0,color,0);
		return;
		}
		dx01 = x1 - x0;
		dy01 = y1 - y0;
		dx02 = x2 - x0;
		dy02 = y2 - y0;
		dx12 = x2 - x1;
		dy12 = y2 - y1;

		if(y1 == y2)
		{
			last = y1; 
		}
		else
		{
			last = y1-1; 
		}
		for(y=y0; y<=last; y++) 
		{
			a = x0 + sa / dy01;
			b = x0 + sb / dy02;
			sa += dx01;
		sb += dx02;
		if(a > b)
		{
				_swap(&a,&b);
			}
			tli_draw_Rectangle(a,y,b,y,color,0);
		}
		sa = dx12 * (y - y1);
		sb = dx02 * (y - y0);
		for(; y<=y2; y++) 
		{
			a = x1 + sa / dy12;
			b = x0 + sb / dy02;
			sa += dx12;
			sb += dx02;
			if(a > b)
			{
				_swap(&a,&b);
			}

			tli_draw_Rectangle(a,y,b,y,color,0);
		}
	}
}



/**********************************************************
 * 函 数 名 称：point_enlargement
 * 函 数 功 能：将一个点放大
 * 传 入 参 数：(x,y)：点的起始坐标
 * 				color：点的颜色
 * 				magnify：点的放大倍数 最小为1
 * 函 数 返 回：无
 * 作       者：LCKFB
 * 备       注：无
**********************************************************/
void point_enlargement(uint16_t x, uint16_t y, uint16_t color, char magnify)
{
	tli_draw_Rectangle(x-magnify,y-magnify,x,y,color,1);
	
	tli_draw_Rectangle(x, y-magnify,x+magnify,y,color,1);
	
	tli_draw_Rectangle(x-magnify, y,x,y+magnify,color,1);
	
	tli_draw_Rectangle(x, y,x+magnify,y+magnify,color,1);
}

///**********************************************************
// * 函 数 名 称：LCD_ShowChar
// * 函 数 功 能：显示一个字符
// * 传 入 参 数：(x,y)：字符显示的起点坐标
//				fc：笔画颜色
//				bc：背景颜色
//				num：显示的字符
//				size：字符放大倍数 最小为1
//				mode：是否叠加显示	0=非叠加方式  1=叠加方式
// * 函 数 返 回：无
// * 作       者：LCKFB
// * 备       注：以16x8大小的字符作为放大模板
//**********************************************************/
//void tli_show_char(uint16_t x,uint16_t y,uint16_t fc, uint16_t bc, uint8_t num,uint8_t size,uint8_t mode)
//{
//    uint8_t temp;
//    uint8_t pos,t;
//	uint16_t x0=0;
//	uint16_t y0=0;
//	num=num-' ';//得到偏移后的值
//
//	for(pos=0;pos<16;pos++)
//	{
//		temp=asc2_1608[num][pos];//调用1608字体
//		for(t=0;t<16/2;t++)
//		{
//
//			if( !mode )
//			{
//				if(temp&0x01)point_enlargement(x+x0,y+y0,fc,size);//画一个点
//				else 		 point_enlargement(x+x0,y+y0,bc,size);//画一个点
//			}
//			else
//			{
//				if(temp&0x01)point_enlargement(x+x0,y+y0,fc,size);   //画一个点
//			}
//			temp>>=1;
//			x0 = x0+size;
//		}
//		x0 = 0;
//		y0 = y0+size;
//	}
//}
//
///**********************************************************
// * 函 数 名 称：tli_Show_String
// * 函 数 功 能：显示字符串
// * 传 入 参 数：(x,y)：起始坐标
// * 				fc：笔画颜色
// * 				bc：背景颜色
// * 				size：字符放大倍数（以16作为基数放大 16*size 16*size 16*size）
// * 				p：显示的字符串
// * 				mode：是否叠加显示	0=非叠加方式  1=叠加方式
// * 函 数 返 回：无
// * 作       者：LCKFB
// * 备       注：无
//**********************************************************/
//void tli_show_string(uint16_t x,uint16_t y,uint16_t fc,uint16_t bc,uint8_t size,uint8_t *p,uint8_t mode)
//{
//    while((*p<='~')&&(*p>=' '))//判断是不是非法字符
//    {
//		if(x>(LCD_WIDTH-1)||y>(LCD_HEIGHT-1)) return;
//
//        tli_show_char(x,y,fc,bc,*p,size,mode);
//        x+=16*size/2;
//        p++;
//    }
//}


///**********************************************************
// * 函 数 名 称：tli_show_font16
// * 函 数 功 能：显示单个16*16大小的字体
// * 传 入 参 数：(x,y)：字体显示的起始坐标
// *   			   	fc：字体笔画颜色
// * 					bc：字体背景颜色
// * 					s：字体索引
// * 					mode：是否叠加显示	0=非叠加方式  1=叠加方式
// * 函 数 返 回：无
// * 作       者：LCKFB
// * 备       注：无
//**********************************************************/
//void tli_show_font16(uint16_t x, uint16_t y, uint16_t fc, uint16_t bc, uint8_t *s,uint8_t mode)
//{
//	uint8_t i,j;
//	uint16_t k;
//	uint16_t HZnum;
//	uint16_t x0=x;
//	HZnum=sizeof(tfont16)/sizeof(typFNT_GB16);	//自动统计汉字数目
//
//	for (k=0;k<HZnum;k++)
//	{
//		if ((tfont16[k].Index[0]==*(s))&&(tfont16[k].Index[1]==*(s+1)))
//		{
//		    for(i=0;i<16*2;i++)
//		    {
//				for(j=0;j<8;j++)
//		    	{
//					if( !mode )
//					{
//						if(tfont16[k].Msk[i]&(0x80>>j))	tli_draw_point(x,y,fc);//画一个点
//						else 							tli_draw_point(x,y,bc);//画一个点
//					}
//					else
//					{
//						if(tfont16[k].Msk[i]&(0x80>>j))	tli_draw_point(x,y,fc);//画一个点
//					}
//					x++;
//					if((x-x0)==16)
//					{
//						x=x0;
//						y++;
//						break;
//					}
//				}
//			}
//		}
//		continue;  //查找到对应点阵字库立即退出，防止多个汉字重复取模带来影响
//	}
//}
//
///**********************************************************
// * 函 数 名 称：tli_show_font24
// * 函 数 功 能：显示单个24*24大小的字体
// * 传 入 参 数：(x,y)：字体显示的起始坐标
// *   			   	fc：字体笔画颜色
// * 					bc：字体背景颜色
// * 					s：字体索引
// * 					mode：是否叠加显示	0=非叠加方式  1=叠加方式
// * 函 数 返 回：无
// * 作       者：LCKFB
// * 备       注：无
//**********************************************************/
//void tli_show_font24(uint16_t x, uint16_t y, uint16_t fc, uint16_t bc, uint8_t *s,uint8_t mode)
//{
//	uint8_t i,j;
//	uint16_t k;
//	uint16_t HZnum;
//	uint16_t x0=x;
//	HZnum=sizeof(tfont24)/sizeof(typFNT_GB24);	//自动统计汉字数目
//
//	for (k=0;k<HZnum;k++)
//	{
//	  if ((tfont24[k].Index[0]==*(s))&&(tfont24[k].Index[1]==*(s+1)))
//	  {
//			for(i=0;i<24*3;i++)
//			{
//				for(j=0;j<8;j++)
//				{
//					if( !mode )
//					{
//						if(tfont24[k].Msk[i]&(0x80>>j))	tli_draw_point(x,y,fc);//画一个点
//						else 							tli_draw_point(x,y,bc);//画一个点
//					}
//					else
//					{
//						if(tfont24[k].Msk[i]&(0x80>>j))	tli_draw_point(x,y,fc);//画一个点
//					}
//					x++;
//					if((x-x0)==24)
//					{
//						x=x0;
//						y++;
//						break;
//					}
//				}
//			}
//		}
//		continue;  //查找到对应点阵字库立即退出，防止多个汉字重复取模带来影响
//	}
//}
//
//
///**********************************************************
// * 函 数 名 称：tli_show_font32
// * 函 数 功 能：显示单个32*32大小的字体
// * 传 入 参 数：(x,y)：字体显示的起始坐标
// *   			   	fc：字体笔画颜色
// * 					bc：字体背景颜色
// * 					s：字体索引
// * 					mode：是否叠加显示	0=非叠加方式  1=叠加方式
// * 函 数 返 回：无
// * 作       者：LCKFB
// * 备       注：无
//**********************************************************/
//void tli_show_font32(uint16_t x, uint16_t y, uint16_t fc, uint16_t bc, uint8_t *s,uint8_t mode)
//{
//	uint8_t i,j;
//	uint16_t k;
//	uint16_t HZnum;
//	uint16_t x0=x;
//	HZnum=sizeof(tfont32)/sizeof(typFNT_GB32);	//自动统计汉字数目
//	for (k=0;k<HZnum;k++)
//	{
//	  if ((tfont32[k].Index[0]==*(s))&&(tfont32[k].Index[1]==*(s+1)))
//	  {
//		for(i=0;i<32*4;i++)
//		{
//			for(j=0;j<8;j++)
//			{
//				if( !mode )
//				{
//					if(tfont32[k].Msk[i]&(0x80>>j))	tli_draw_point(x,y,fc);//画一个点
//					else 							tli_draw_point(x,y,bc);//画一个点
//				}
//				else
//				{
//					if(tfont32[k].Msk[i]&(0x80>>j))	tli_draw_point(x,y,fc);//画一个点
//				}
//				x++;
//				if((x-x0)==32)
//				{
//					x=x0;
//					y++;
//					break;
//				}
//			}
//		}
//	 }
//	 continue;  //查找到对应点阵字库立即退出，防止多个汉字重复取模带来影响
//	}
//}
//
//
///**********************************************************
// * 函 数 名 称：tli_show_Chinese_string
// * 函 数 功 能：显示中文字符串
// * 传 入 参 数：(x,y)：字符串显示的起始坐标
// *   			   	fc：字符串笔画颜色
// * 					bc：字符串背景颜色
// * 					str：字符串
// * 					size：字符串大小 16，24，32
// * 					mode：是否叠加显示	0=非叠加方式  1=叠加方式
// * 函 数 返 回：无
// * 作       者：LCKFB
// * 备       注：显示的中文请确保有其字模，可去font.h查看
//**********************************************************/
//void tli_show_Chinese_string(uint16_t x, uint16_t y, uint16_t fc, uint16_t bc, uint8_t *str,uint8_t size,uint8_t mode)
//{
//    while(*str!=0)//数据未结束
//    {
//		if(x>(LCD_WIDTH-size)||y>(LCD_HEIGHT-size)) return;
//
//		if(size==32)
//			tli_show_font32(x,y,fc,bc,str,mode);
//		else if(size==24)
//			tli_show_font24(x,y,fc,bc,str,mode);
//		else
//			tli_show_font16(x,y,fc,bc,str,mode);
//
//		str+=2;
//		x+=size;//下一个汉字偏移
//    }
//}
/**********************************************************
 * 函 数 名 称：tli_show_picture
 * 函 数 功 能：显示图片
 * 传 入 参 数：(x,y)：显示图片的起点坐标
 * 				w：图片宽度
 * 				h：图片高度
 * 				pic：图片地址
 * 函 数 返 回：无
 * 作       者：LCKFB
 * 备       注：无
**********************************************************/
void tli_show_picture(uint16_t x,uint16_t y,uint16_t w, uint16_t h,const unsigned char pic[]) 
{
	uint16_t i,j;
	uint32_t k=0;
	uint16_t x0=x,y0=y;
	for(i=y;i<h+y0;i++)
	{
		for(j=x;j<w+x0;j++)
		{			
			tli_draw_point(j,i,pic[k*2]<<8|pic[k*2+1]);
			k++;
		}
	}		
}
