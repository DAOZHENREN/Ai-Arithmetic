#include "common.h"
#include "STC32G_NVIC.h"
#include "STC32G_Clock.h"
#include "STC32G_PWM.h"
#include "STC32G_Timer.h"
#include "STC32G_GPIO.h"


void configBackLightPWM(u8 brightness)
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