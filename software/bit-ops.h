#define SET_BIT(reg, bit)    ((reg) |= (1 << (bit)))    // ����ָ����λ
#define CLEAR_BIT(reg, bit)  ((reg) &= ~(1 << (bit)))   // ���ָ����λ
#define TOGGLE_BIT(reg, bit) ((reg) ^= (1 << (bit)))    // �л�ָ����λ
#define CHECK_BIT(reg, bit)  (((reg) >> (bit)) & 1)     // ���ָ����λ
#define IS_BIT_SET(reg, bit) ((reg) & (1 << (bit)))      // ���λ�Ƿ�Ϊ1
#define IS_BIT_CLEAR(reg, bit) (!((reg) & (1 << (bit)))) // ���λ�Ƿ�Ϊ0
