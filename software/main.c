#include "common.h"
#include "touch.h"
#include "usb.h"
#include "uart.h"
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
#include "expression.h"

#define COLLECT_MODE 0

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
	// ���usbû�����ӣ����ͻῨס���������Ƿ������ٷ���
	if (DeviceState != DEVSTATE_CONFIGURED)
	{
		return;
	}

	// ���Ż���������ӡ�����
	TxBuffer[0] = c;
	uart_send(1);
	return c;
}

void configBlackLightPWM(u8 brightness)
{
	const u16 period = (u32)MAIN_Fosc / (u32)(20 * 1000);
	PWMx_InitDefine PWMx_InitStructure;

	// PWM3P_2 P0.4
	P0_MODE_IO_PU(GPIO_Pin_4);

	PWMx_InitStructure.PWM_Mode = CCMRn_PWM_MODE1;							// ģʽ,		CCMRn_FREEZE,CCMRn_MATCH_VALID,CCMRn_MATCH_INVALID,CCMRn_ROLLOVER,CCMRn_FORCE_INVALID,CCMRn_FORCE_VALID,CCMRn_PWM_MODE1,CCMRn_PWM_MODE2
	PWMx_InitStructure.PWM_Duty = (u32)brightness * (u32)period / (u32)255; // PWMռ�ձ�ʱ��, 0~Period
	PWMx_InitStructure.PWM_EnoSelect = ENO3P;								// ���ͨ��ѡ��,	ENO1P,ENO1N,ENO2P,ENO2N,ENO3P,ENO3N,ENO4P,ENO4N / ENO5P,ENO6P,ENO7P,ENO8P
	PWM_Configuration(PWM3, &PWMx_InitStructure);							// ��ʼ��PWM3

	PWMx_InitStructure.PWM_Period = period;		   // ����ʱ��,   0~65535
	PWMx_InitStructure.PWM_DeadTime = 0;		   // ��������������, 0~255
	PWMx_InitStructure.PWM_MainOutEnable = ENABLE; // �����ʹ��, ENABLE,DISABLE
	PWMx_InitStructure.PWM_CEN_Enable = ENABLE;	   // ʹ�ܼ�����, ENABLE,DISABLE
	PWM_Configuration(PWMA, &PWMx_InitStructure);  // ��ʼ��PWMͨ�üĴ���,  PWMA,PWMB

	PWMA_PS = (PWMA_PS & ~0x30) | 0x10; // �л��� PWM3_2 ,Ҳ���� C1PS=0b01, P04P05    �����õ���stc32�Ŀ⺯����������ط������ݣ������üĴ���
	NVIC_PWM_Init(PWMA, DISABLE, Priority_0);
}

tm_mdl_t mdl;
tm_mat_t in_uint8;
tm_mat_t in;
tm_mat_t outs[1];
tm_err_t res;
bit busy;
char wptr;
char rptr;
char buffer[16];

#define IMG_L (28)
#define IMG_CH (1)
#define CLASS_N (14)
#include "TinyMaix/mnist_valid_q_be.h"

static uint8_t xdata mdl_buf[MDL_BUF_LEN];
static uint8_t xdata mnist_pic[28 * 28] = {0};
// ���������bitmap�Ż�������û��Ҫ���ڴ湻��
static uint8_t xdata mnist_pic_large[24 * 80] = {0};
static uint8_t xdata expression[16] = {0};
static uint8_t xdata expression_n = 0;
static uint8_t xdata strBuffer[50] = {0};

//
static uint8_t parse_output(tm_mat_t *outs)
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
	return maxi;
}

void clean_mnist_pic()
{
	memset(mnist_pic, 0, 28 * 28 * sizeof(mnist_pic[0]));
}
void clean_mnist_pic_large()
{
	memset(mnist_pic_large, 0, 24 * 80 * sizeof(mnist_pic_large[0]));
}
#define IMG_WIDTH 80
#define IMG_HEIGHT 24
#define CHAR_IMG_SIZE 28
#define BAD_RECOGNIZE 255
// recognize ��������
uint8_t recognize(uint32_t start_col, uint32_t end_col)
{
	uint32_t col, row;
	uint32_t top_row, bottom_row;
	uint32_t hor_offset, ver_offset;
	uint32_t effective_height;

	// ������ֻ��һ�е���������ֵ����Ч�ַ�
	if (end_col - start_col <= 3)
	{
		uint32_t count = 0;
		for (col = start_col; col <= end_col; col++)
		{
			for (row = 0; row < IMG_HEIGHT; row++)
			{
				if (mnist_pic_large[row * IMG_WIDTH + col] > 0)
				{
					count++;
				}
			}
		}

		if (count < 10)
		{
			return BAD_RECOGNIZE; // ��Ϊ�Ǹ��ŵ㣬����
		}
	}

	clean_mnist_pic();

	// ����ˮƽƫ����
	hor_offset = (CHAR_IMG_SIZE - (end_col - start_col + 1)) / 2;

	// ������Ч���ַ��߶ȷ�Χ
	top_row = IMG_HEIGHT;
	bottom_row = 0;
	for (row = 0; row < IMG_HEIGHT; row++)
	{
		for (col = start_col; col <= end_col; col++)
		{
			if (mnist_pic_large[row * IMG_WIDTH + col] > 0)
			{
				if (row < top_row)
					top_row = row;
				if (row > bottom_row)
					bottom_row = row;
			}
		}
	}

	// ������Ч�ַ�����ĸ߶�
	effective_height = bottom_row - top_row + 1;

	// ���㴹ֱƫ������������Ч�߶Ⱦ��У�
	ver_offset = (CHAR_IMG_SIZE - effective_height) / 2;

	// �ָ��ַ�������չ�� 28x28 ��С�����д���
	for (col = start_col; col <= end_col; col++)
	{
		for (row = 0; row < IMG_HEIGHT; row++)
		{
			if (mnist_pic_large[row * IMG_WIDTH + col] > 0)
			{
				// ���ַ�������չ�� 28x28 ��С������
				uint32_t new_col = col - start_col + hor_offset;
				uint32_t new_row = row - top_row + ver_offset;

				// ����ַ��߶Ⱥ�λ�ò��ʺϾ��У����е���
				if (new_row < 0)
				{
					new_row = 0;
				}
				else if (new_row >= CHAR_IMG_SIZE)
				{
					new_row = CHAR_IMG_SIZE - 1;
				}

				mnist_pic[new_row * CHAR_IMG_SIZE + new_col] = mnist_pic_large[row * IMG_WIDTH + col];
			}
		}
	}

#if COLLECT_MODE
	{
		int xxx;
		for (xxx = 0; xxx < 28 * 28; xxx++)
		{
			TM_PRINTF("%3d,", mnist_pic[xxx]);
			if (xxx % 28 == 27)
				printf("\r\n");
		}
		printf("@\r\n");
	}
	return 0;
#else
	{
		// uint32_t xx;
		// printf("@");
		// for (xx = 0; xx < 300; xx++)
		// {
		tm_preprocess(&mdl, TMPP_UINT2INT, &in_uint8, &in);

		tm_run(&mdl, &in, outs);
		// }
		// printf("!");
	}
	return parse_output(outs);
#endif
}

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

int8 btn0;
void interrupt0()
// ������ж��Ƿ�λkeil������������vscode�ﲻ��keil�﷨����
// https://developer.arm.com/documentation/101655/0961/Cx51-User-s-Guide/Preprocessor/Macros/Predefined-Macros
#ifdef __MODEL__
	interrupt INT0_VECTOR
#endif
{
	btn0++;
}

void main(void)
{

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

	{
		// P3.2(interrupt 0)׼˫��
		P3M0 &= ~0x04;
		P3M1 &= ~0x04;
		IT0 = 1; // INT0 �½����ж�
		EX0 = 1; // ʹ�� INT0 �ж�
	}

	uart_init();
	usb_init();
	EA = 1;

	touch_init();

	LCD_Init();			// ��ʼ��LCD
	LCD_Display_Dir(2); // ��Ļ����

	// �ȵ���Ļ���ݣ���������⣬������ʾ��ѩ����
	LCD_Clear(WHITE);
	configBlackLightPWM(255);

	res = tm_load(&mdl, mdl_data, mdl_buf, layer_cb, &in);
	if (res != TM_OK)
	{
		TM_PRINTF("tm model load err %d\r\n", res);
		return;
	}

	while (1)
	{

		if (btn0)
		{
			btn0 = 0;
			{
				uint32_t x, y;
				uint8_t sum;
				int in_char = 0;		// ��ǵ�ǰ�Ƿ����ַ�����
				int char_count = 0;		// �ַ���������
				uint32_t start_col = 0; // �ַ���ʼ��
				expression_n = 0;

				for (x = 0; x < 80; x++)
				{
					sum = 0;
					for (y = 0; y < 24; y++)
					{
						sum |= mnist_pic_large[y * 80 + x];
					}

					if (sum)
					{
						// ��ǰ���з���ֵ���������ַ�������
						if (!in_char)
						{
							in_char = 1;   // ��ǽ����ַ�����
							start_col = x; // ��¼�ַ���ʼ��
							char_count++;  // �ַ���������
						}
					}
					else
					{
						// ��ǰ��ȫΪ�㣬���������ַ�������
						if (in_char)
						{
							uint32_t end_col;
							uint8_t temp;
							in_char = 0; // ����뿪�ַ�����
							end_col = x - 1;
							temp = recognize(start_col, end_col);
							if (temp != BAD_RECOGNIZE)
							{
								expression[expression_n++] = temp;
							}
							// printf("�ַ� %d ", char_count);
							// printf("��ʼ��: %d,", start_col);
							// printf("������: %d\n", end_col);
						}
					}

					// printf("%d ", !!sum);
				}

				// �������һ���ַ���ͼƬĩβ�����
				if (in_char)
				{
					// ���һ���ַ��Ľ�����ΪͼƬĩβ
					uint32_t end_col = 79;
					uint8_t temp;

					if (temp != BAD_RECOGNIZE)
					{
						expression[expression_n++] = temp;
					}
					// printf("�ַ� %d ", char_count);
					// printf("��ʼ��: %d,", start_col);
					// printf("������: %d\n", end_col);
				}

				// printf("\n�ַ���\xFD��: %d\n", char_count);

				LCD_Clear(WHITE);
				clean_mnist_pic_large();
#if COLLECT_MODE
#else
				sprintf(strBuffer, "result: %.2f", expression_calc(expression, expression_n));
				Show_Str(10, 200, strBuffer, 24, 0);
#endif
			}
		}

		if (RxFlag)
		{
			checkISP();
			uart_recv_done(); // �Խ��յ����ݴ�����ɺ�,һ��Ҫ����һ���������,�Ա�CDC������һ�ʴ�������
			btn0++;
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

			// ����û���Ǳ�ԵԽ�磬���Ǻ���û��
			if (x > 0 && x < 80 * 4)
			{
				if (y > 0 && y < 24 * 4)
				{
					LCD_Fill(x, y, x + 4, y + 4, BLACK);
					mnist_pic_large[(x / 4) + (y / 4) * 80] = 255;
				}
			}

			if (y > 24 * 4)
			{
				btn0++;
			}
		}
	}
}