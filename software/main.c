#include "STC32G.H"
#include "stc.h"
#include "usb.h"
#include "uart.h"
#include "stdio.h"
#include "intrins.h"
#include "STC32G_GPIO.h"
#include "LCD.h"
#include "test code.h"
#include "STC32G_SPI.h"
#include "STC32G_Switch.h"
#include "STC32G_NVIC.h"
#include "STC32G_Clock.h"
#include <string.h>
#include "STC32G_PWM.h"
#include "STC32G_Timer.h"
#include "tinymaix.h"

/****************  SPI��ʼ������ *****************/
void SPI_config1(void)
{
#ifdef LOONG
	P2_MODE_IO_PU(GPIO_Pin_3 | GPIO_Pin_5 | GPIO_Pin_0 | GPIO_Pin_1);
	P2_SPEED_HIGH(GPIO_Pin_3 | GPIO_Pin_5); // ��ƽת���ٶȿ죨���IO�ڷ�ת�ٶȣ�
#else
	P1_MODE_IO_PU(GPIO_Pin_3 | GPIO_Pin_5 | GPIO_Pin_6);
	P4_MODE_IO_PU(GPIO_Pin_7);
	P1_SPEED_HIGH(GPIO_Pin_3 | GPIO_Pin_5); // ��ƽת���ٶȿ죨���IO�ڷ�ת�ٶȣ�
#endif
}

/**
 * ����Ƿ���յ��ض���ISP�̼���������
 * ��Ҫ���usb cdcʹ��,ÿ�ν��յ���Ϣ,�ȼ�����
 */
void checkISP()
{
	// �±������Ż�̫ɵ�������Ż�strlen,ֱ��д8
	if (memcmp("@STCISP#", RxBuffer, 8) == 0)
	{
		usb_write_reg(OUTCSR1, 0);
		USBCON = 0x00;
		USBCLK = 0x00;
		IRC48MCR = 0x00;

		P3M0 &= ~0x03; // ����Ϊ����
		P3M1 |= 0x03;
		delay_ms(10); // ����ʱ���usb�߲��
		// ��λ��bootloader
		IAP_CONTR = 0x60;
		while (1)
			;
	}
}

char putchar(char c)
{
	TxBuffer[0] = c;
	uart_send(1);
	return c;
}

void configBlackLightPWM(u8 brightness)
{
	const u16 period = (u32)MAIN_Fosc / (u32)(20 * 1000);
	PWMx_InitDefine PWMx_InitStructure;

	// PWM1P_3 P6.0
	P6_MODE_IO_PU(GPIO_Pin_0);

	PWMx_InitStructure.PWM_Mode = CCMRn_PWM_MODE1;							// ģʽ,		CCMRn_FREEZE,CCMRn_MATCH_VALID,CCMRn_MATCH_INVALID,CCMRn_ROLLOVER,CCMRn_FORCE_INVALID,CCMRn_FORCE_VALID,CCMRn_PWM_MODE1,CCMRn_PWM_MODE2
	PWMx_InitStructure.PWM_Duty = (u32)brightness * (u32)period / (u32)255; // PWMռ�ձ�ʱ��, 0~Period
	PWMx_InitStructure.PWM_EnoSelect = ENO1P;								// ���ͨ��ѡ��,	ENO1P,ENO1N,ENO2P,ENO2N,ENO3P,ENO3N,ENO4P,ENO4N / ENO5P,ENO6P,ENO7P,ENO8P
	PWM_Configuration(PWM1, &PWMx_InitStructure);							// ��ʼ��PWM1

	PWMx_InitStructure.PWM_Period = period;		   // ����ʱ��,   0~65535
	PWMx_InitStructure.PWM_DeadTime = 0;		   // ��������������, 0~255
	PWMx_InitStructure.PWM_MainOutEnable = ENABLE; // �����ʹ��, ENABLE,DISABLE
	PWMx_InitStructure.PWM_CEN_Enable = ENABLE;	   // ʹ�ܼ�����, ENABLE,DISABLE
	PWM_Configuration(PWMA, &PWMx_InitStructure);  // ��ʼ��PWMͨ�üĴ���,  PWMA,PWMB

	PWM1_USE_P60P61();
	NVIC_PWM_Init(PWMA, DISABLE, Priority_0);
}
u16 Get_ADC12bitResult(u8 channel) // channel = 0~15
{
	ADC_RES = 0;
	ADC_RESL = 0;

	ADC_CONTR = (ADC_CONTR & 0xf0) | channel; // ����ADCת��ͨ��
	ADC_START = 1;							  // ����ADCת��
	_nop_();
	_nop_();
	_nop_();
	_nop_();
	while (ADC_FLAG == 0)
		;		  // wait for ADC finish
	ADC_FLAG = 0; // ���ADC������־
	ADC_CONTR = (ADC_CONTR & 0xf0) | (u8)15;
	return (((u16)ADC_RES << 8) | ADC_RESL);
}

#define ADC_SPEED 15	 /* 0~15, ADCת��ʱ��(CPUʱ����) = (n+1)*32  ADCCFG */
#define RES_FMT (1 << 5) /* ADC�����ʽ 0: �����, ADC_RES: D11 D10 D9 D8 D7 D6 D5 D4, ADC_RESL: D3 D2 D1 D0 0 0 0 0 */
						 /* ADCCFG      1: �Ҷ���, ADC_RES: 0 0 0 0 D11 D10 D9 D8, ADC_RESL: D7 D6 D5 D4 D3 D2 D1 D0 */

void xy_reset()
{
	// 0123����
	P0M0 |= 0x0f;
	P0M1 &= ~0x0f;
	// 0123����
	P0 &= ~0x0f;
	NOP4();
}

u16 x_read()
{
	xy_reset();

	// 0��2���죬1��3����
	P0M0 = (P0M0 & ~0x0a) | 0x05;
	P0M1 = (P0M1 & ~0x05) | 0x0a;
	P00 = 0;
	P02 = 1;

	return Get_ADC12bitResult(11);
}

u16 y_read()
{
	xy_reset();

	// 1��3���죬0��2����
	P0M0 = (P0M0 & ~0x05) | 0x0a;
	P0M1 = (P0M1 & ~0x0a) | 0x05;
	P01 = 1;
	P03 = 0;

	return Get_ADC12bitResult(8);
}

// ӳ��adc��ʵ������
u16 remap(u16 adc_value, u16 adc_min, u16 adc_max, u16 output_min, u16 output_max)
{
	u32 normalized;
	// ȷ�� ADC ���뷶Χ��Ч
	if (adc_min >= adc_max)
	{
		return output_min; // ������Сֵ
	}

	// ��� adc_value �Ƿ�����Ч��Χ��
	if (adc_value < adc_min)
	{
		adc_value = adc_min; // ���������Сֵ����Ϊ��Сֵ
	}
	else if (adc_value > adc_max)
	{
		adc_value = adc_max; // ����������ֵ����Ϊ���ֵ
	}

	// �� ADC ֵӳ�䵽 [0, 1] ���䣨ʹ���������㣩
	normalized = (u32)(adc_value - adc_min) * (output_max - output_min) / (adc_max - adc_min);

	// ӳ�䵽ʵ�ʵ������Χ
	return output_min + normalized;
}

#define SAMP_CNT 4
#define SAMP_CNT_DIV2 2
u16 X, Y;
// ����adc�˲�����  https://www.cnblogs.com/yuphone/archive/2010/11/28/1890239.html
u8 touch_scan(void)
{
	u8 i, j, k, min;
	u16 temp;
	u16 tempXY[2][SAMP_CNT], XY[2];
	// ����
	for (i = 0; i < SAMP_CNT; i++)
	{
		tempXY[0][i] = x_read();
		tempXY[1][i] = y_read();
	}
	// �˲�
	for (k = 0; k < 2; k++)
	{ // ��������
		for (i = 0; i < SAMP_CNT - 1; i++)
		{
			min = i;
			for (j = i + 1; j < SAMP_CNT; j++)
			{
				if (tempXY[k][min] > tempXY[k][j])
					min = j;
			}
			temp = tempXY[k][i];
			tempXY[k][i] = tempXY[k][min];
			tempXY[k][min] = temp;
		}
		// �趨��ֵ
		if ((tempXY[k][SAMP_CNT_DIV2] - tempXY[k][SAMP_CNT_DIV2 - 1]) > 5)
			return 0;
		// ���м�ֵ�ľ�ֵ
		XY[k] = (tempXY[k][SAMP_CNT_DIV2] + tempXY[k][SAMP_CNT_DIV2 - 1]) / 2;
	}
	X = XY[0];
	Y = XY[1];
	return 1;
}

static uint8_t xdata mnist_pic[28 * 28] = {0};

#include "TinyMaix/mnist_valid_q_be.h"
bit busy;
char wptr;
char rptr;
char buffer[16];

#define IMG_L (28)
#define IMG_CH (1)
#define CLASS_N (14)
static uint8_t xdata mdl_buf[MDL_BUF_LEN];
static tm_err_t layer_cb(tm_mdl_t *mdl, tml_head_t *lh)
{
#if TM_ENABLE_STAT
	// dump middle result
	int x, y, c;
	int h = lh->out_dims[1];
	int w = lh->out_dims[2];
	int ch = lh->out_dims[3];
	mtype_t *output = TML_GET_OUTPUT(mdl, lh);
	return TM_OK;
	TM_PRINTF("Layer %d callback ========\n", mdl->layer_i);
#if 1
	for (y = 0; y < h; y++)
	{
		TM_PRINTF("[");
		for (x = 0; x < w; x++)
		{
			TM_PRINTF("[");
			for (c = 0; c < ch; c++)
			{
#if TM_MDL_TYPE == TM_MDL_FP32
				TM_PRINTF("%.3f,", output[(y * w + x) * ch + c]);
#else
				TM_PRINTF("%.3f,", TML_DEQUANT(lh, output[(y * w + x) * ch + c]));
#endif
			}
			TM_PRINTF("],");
		}
		TM_PRINTF("],\n");
	}
	TM_PRINTF("\n");
#endif
#endif
	return TM_OK;
}

static void parse_output(tm_mat_t *outs)
{
	tm_mat_t out = outs[0];
	float *dat = (float *)out.dat;
	float maxp = 0;
	int maxi = -1;
	int i = 0;
	for (; i < CLASS_N; i++)
	{
		if (dat[i] > maxp)
		{
			maxi = i;
			maxp = dat[i];
		}
	}
	TM_PRINTF("### Predict output is: Number %d , Prob %.3f\r\n", maxi, maxp);
	return;
}

// ����usb��ʱ������usb�ӿڻ�е��ƣ�vcc���Ƚ�ͨ��Ȼ�����d+d-
// ��stc���߼��ǿ������d+d-�Ӻã���boot����P3.2Ϊ0���Ž���usb����ģʽ
// ������סboot����usb����������������е�ṹ���ڼ��d+d-ʱ��d+d-�����û�в嵽λ
// ���½���bootʧ��
// �ڳ�����ǰ������ִ��������������¼��boot��Ȼ��ǿ��ת��������ģʽ
// �Ϳ���ʵ�֣���סboot��usb��һ����ת������ģʽ
// ͬʱ���������ڴ�����ǰ��ִ�У���ʹ�����������������δ���Ҳ����ִ�У�������ש
void check_boot()
{
	// P3.2 ����ģʽ  ������������
	P3M0 &= ~0x04;
	P3M1 |= 0x04;
	P3PU |= 0x04;
	delay_ms(1);
	if (P32 == 0)
	{
		usb_write_reg(OUTCSR1, 0);
		USBCON = 0x00;
		USBCLK = 0x00;
		IRC48MCR = 0x00;

		P3M0 &= ~0x03; // ����Ϊ����
		P3M1 |= 0x03;
		delay_ms(500); // ����ʱ���usb�߲��
		// ��λ��bootloader
		IAP_CONTR = 0x60;
		while (1)
			;
	}
	// �ر���������
	P3PU &= ~0x04;
}

void main(void)
{
	tm_mdl_t mdl;
	tm_mat_t in_uint8;
	tm_mat_t in;
	tm_mat_t outs[1];
	tm_err_t res;

	in_uint8.dims = 3;
	in_uint8.h = IMG_L;
	in_uint8.w = IMG_L;
	in_uint8.c = IMG_CH;
	in_uint8.dat = (mtype_t *)mnist_pic;

	in.dims = 3;
	in.h = IMG_L;
	in.w = IMG_L;
	in.c = IMG_CH;
	in.dat = NULL;

	WTST = 0;  // ���ó���ָ����ʱ��������ֵΪ0�ɽ�CPUִ��ָ����ٶ�����Ϊ���
	EAXFR = 1; // ��չ�Ĵ���(XFR)����ʹ��
	CKCON = 0; // ��߷���XRAM�ٶ�
	check_boot();

	uart_init();
	usb_init();
	EA = 1;

	ADCTIM = 0x3f; // ����ͨ��ѡ��ʱ�䡢����ʱ�䡢����ʱ��
	ADCCFG = RES_FMT + ADC_SPEED;
	// ADCģ���Դ�򿪺���ȴ�1ms��MCU�ڲ�ADC��Դ�ȶ����ٽ���ADת��
	ADC_CONTR = 0x80 + 0; // ADC on + channel

	SPI_config1();

	LCD_Init();			// ��ʼ��LCD
	LCD_Display_Dir(2); // ��Ļ����

	LCD_Clear(GREEN);
	configBlackLightPWM(255);
	LCD_Set_Window(0, 0, 320, 240);
	LCD_WriteRAM_Prepare();
	SPI_DC = 1;
	configBlackLightPWM(255);

	res = tm_load(&mdl, mdl_data, mdl_buf, layer_cb, &in);
	if (res != TM_OK)
	{
		TM_PRINTF("tm model load err %d\r\n", res);
		return;
	}

	while (1)
	{

		if (RxFlag)
		{
			checkISP();
			// printf("touch_x: %d touch_y:%d\n", X, Y);
			uart_recv_done(); // �Խ��յ����ݴ�����ɺ�,һ��Ҫ����һ���������,�Ա�CDC������һ�ʴ�������
							  // TM_DBGT_START();
#if TM_ENABLE_STAT
			{
				int xxx;
				tm_stat((tm_mdlbin_t *)mdl_data);
				for (xxx = 0; xxx < 28 * 28; xxx++)
				{
					TM_PRINTF("%3d,", mnist_pic[xxx]);
					if (xxx % 28 == 27)
						printf("\r\n");
				}
			}
#endif
			res = tm_preprocess(&mdl, TMPP_UINT2INT, &in_uint8, &in);

			res = tm_run(&mdl, &in, outs);

			if (res == TM_OK)
			{
				parse_output(outs);
			}
			{
				int xxx;
				for (xxx = 0; xxx < 28 * 28; xxx++)
				{

					mnist_pic[xxx] = 0;
				}
			}
			LCD_Clear(GREEN);
		}

		if (touch_scan())
		{
			u16 x, y;
			x = remap(X, 535, 3600, 0, 320);
			y = remap(Y, 600, 3300, 0, 240);
			if ((x == 0) || (y == 0))
			{
				continue;
			}

			if (x > 16 && x < 96)
			{
				if (y > 16 && y < 96)
				{
					// 			if ( x < 112)
					// {
					// 	if (y < 112)
					// 	{
					// todo �Ż�
					LCD_Fill(x, y, x + 4, y + 4, BLACK);
					// mnist_pic[(x / 4 + 1) + (y / 4) * 28] = 255;
					// mnist_pic[(x / 4) + (y / 4 + 1) * 28] = 255;
					// mnist_pic[(x / 4 + 1) + (y / 4 + 1) * 28] = 255;
					mnist_pic[(x / 4) + (y / 4) * 28] = 255;
				}
			}
		}
	}
}