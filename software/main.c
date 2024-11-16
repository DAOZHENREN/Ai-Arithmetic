#include "common.h"
#include "touch.h"
#include "usb.h"
#include "uart.h"
#include "STC32G_GPIO.h"
#include "LCD.h"
#include "STC32G_SPI.h"
#include "STC32G_Switch.h"
#include "STC32G_NVIC.h"
#include "STC32G_Clock.h"
#include <string.h>
#include "STC32G_PWM.h"
#include "STC32G_Timer.h"
#include "expression.h"
#include "backLight.h"
#include "Ai.h"
#include "canvas.h"

uint8_t xdata expression[16] = {0};
uint8_t xdata expression_n = 0;
static uint8_t xdata strBuffer[50] = {0};

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
		return c;
	}

	// ���usb�����ӣ���������û�д򿪸ô��ڣ���ʱ����Ҳ�Ῠס
	// �۲쵽��win11�ϣ��򿪴��ں�CDC_DTR����1����֮Ϊ0
	// ��֪�������Ϊ�ܷ��ƹ㵽����ϵͳ��
	if (CDC_DTR == 0)
	{
		return c;
	}

	// ���Ż���������ӡ�����
	TxBuffer[0] = c;
	uart_send(1);
	return c;
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

void interrupt0()
// ������ж��Ƿ�λkeil������������vscode�ﲻ��keil�﷨����
// https://developer.arm.com/documentation/101655/0961/Cx51-User-s-Guide/Preprocessor/Macros/Predefined-Macros
#ifdef __MODEL__
	interrupt INT0_VECTOR
#endif
{
	// �����ťûɶ�ã���������Ϊ���½�����ģʽ
	check_boot();
}

int8 btn0;
void main(void)
{
	WTST = 0;  // ���ó���ָ����ʱ��������ֵΪ0�ɽ�CPUִ��ָ����ٶ�����Ϊ���
	EAXFR = 1; // ��չ�Ĵ���(XFR)����ʹ��
	CKCON = 0; // ��߷���XRAM�ٶ�
	check_boot();

	{
		// ����boot��ť����������ť
		// ���º������ⲿ�ж� interrupt0
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
	// �������⣬�������
	configBackLightPWM(255);

	Ai_init();

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

				for (x = 0; x < CANVAS_WIDTH; x++)
				{
					sum = 0;
					for (y = 0; y < CANVAS_HEIGHT; y++)
					{
						sum |= canvas[y * CANVAS_WIDTH + x];
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
							in_char = 0; // ����뿪�ַ�����
							end_col = x - 1;

							if (canvas_process_character(start_col, end_col))
							{
								expression[expression_n++] = Ai_run();
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
					uint32_t end_col = CANVAS_WIDTH-1;

					if (canvas_process_character(start_col, end_col))
					{
						expression[expression_n++] = Ai_run();
					}
					// printf("�ַ� %d ", char_count);
					// printf("��ʼ��: %d,", start_col);
					// printf("������: %d\n", end_col);
				}

				// printf("\n�ַ���\xFD��: %d\n", char_count);
	// LCD_Fill(0, 0, 320, 240, BLUE);

				LCD_Clear(BLUE);
				clean_canvas();
				
				sprintf(strBuffer, "result: %.2f", expression_calc(expression, expression_n));
				Show_Str(10, 200, strBuffer, 24, 0);
			}
		}

		if (RxFlag)
		{
			checkISP();
			uart_recv_done(); // �Խ��յ����ݴ�����ɺ�,һ��Ҫ����һ���������,�Ա�CDC������һ�ʴ�������
#if TM_ENABLE_STAT
			{
				tm_stat((tm_mdlbin_t *)mdl_data);
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
			if (x > 0 && x < CANVAS_WIDTH * 4)
			{
				if (y > 0 && y < CANVAS_HEIGHT * 4)
				{
					LCD_Fill(x, y, x + 4, y + 4, BLACK);
					canvas[(x / 4) + (y / 4) * CANVAS_WIDTH] = 255;
				}
			}

			if (y > CANVAS_HEIGHT * 4)
			{
				btn0++;
			}
		}
	}
}