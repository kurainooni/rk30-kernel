#ifndef _RK610_LCD_H
#define _RK610_LCD_H
#include <linux/mfd/rk610_core.h>
#include "../screen/screen.h"
#include <linux/earlysuspend.h>
#define ENABLE      1
#define DISABLE     0

/*      LVDS config         */
/*                  LVDS 外部连线接法                       */
/*          LVDS_8BIT_1    LVDS_8BIT_2     LVDS_8BIT_3     LVDS_6BIT
----------------------------------------------------------------------
    TX0     R0              R2              R2              R0
    TX1     R1              R3              R3              R1
    TX2     R2              R4              R4              R2
Y   TX3     R3              R5              R5              R3
0   TX4     R4              R6              R6              R4
    TX6     R5              R7              R7              R5
    TX7     G0              G2              G2              G0
----------------------------------------------------------------------
    TX8     G1              G3              G3              G1
    TX9     G2              G4              G4              G2
Y   TX12    G3              G5              G5              G3
1   TX13    G4              G6              G6              G4
    TX14    G5              G7              G7              G5
    TX15    B0              B2              B2              B0
    TX18    B1              B3              B3              B1
----------------------------------------------------------------------
    TX19    B2              B4              B4              B2
    TX20    B3              B5              B5              B3
    TX21    B4              B6              B6              B4
Y   TX22    B5              B7              B7              B5
2   TX24    HSYNC           HSYNC           HSYNC           HSYNC
    TX25    VSYNC           VSYNC           VSYNC           VSYNC
    TX26    ENABLE          ENABLE          ENABLE          ENABLE
----------------------------------------------------------------------    
    TX27    R6              R0              GND             GND
    TX5     R7              R1              GND             GND
    TX10    G6              G0              GND             GND
Y   TX11    G7              G1              GND             GND
3   TX16    B6              B0              GND             GND
    TX17    B7              B1              GND             GND
    TX23    RSVD            RSVD            RSVD            RSVD
----------------------------------------------------------------------        
*/
#define LVDS_8BIT_1     0x00
#define LVDS_8BIT_2     0x01
#define LVDS_8BIT_3     0x10
#define LVDS_6BIT       0x11
//LVDS lane input format
#define DATA_D0_MSB         0
#define DATA_D7_MSB         1
//LVDS input source
#define FROM_LCD1           0
#define FROM_LCD0_OR_SCL    1

/*      LCD1 config         */
#define LCD1_AS_IN      0
#define LCD1_AS_OUT     1

//LCD1 output source
#define LCD1_FROM_LCD0  0
#define LCD1_FROM_SCL   1

//SCALER config
#define NOBYPASS    0
#define BYPASS      1

//SCALER PLL config
#define S_PLL_PWR_ON    0
#define S_PLL_PWR_DOWN  1

/*      clock config        */
#define S_PLL_FROM_DIV      0
#define S_PLL_FROM_CLKIN    1
#define S_PLL_DIV(x)        ((x)&0x7)
/*********S_PLL_CON************/
//S_PLL_CON0
#define S_DIV_N(x)              (((x)&0xf)<<4)
#define S_DIV_OD(x)             (((x)&3)<<0)
//S_PLL_CON1
#define S_DIV_M(x)              ((x)&0xff)
//S_PLL_CON2
#define S_PLL_UNLOCK            (0<<7)    //0:unlock 1:pll_lock
#define S_PLL_LOCK              (1<<7)    //0:unlock 1:pll_lock
#define S_PLL_PWR(x)            (((x)&1)<<2)    //0:POWER UP 1:POWER DOWN
#define S_PLL_RESET(x)          (((x)&1)<<1)    //0:normal  1:reset M/N dividers
#define S_PLL_BYPASS(x)          (((x)&1)<<0)    //0:normal  1:bypass
//LVDS_CON0
#define LVDS_OUT_CLK_PIN(x)     (((x)&1)<<7)    //clk enable pin, 0: enable
#define LVDS_OUT_CLK_PWR_PIN(x) (((x)&1)<<6)    //clk pwr enable pin, 1: enable 
#define LVDS_PLL_PWR_PIN(x)     (((x)&1)<<5)    //pll pwr enable pin, 0:enable 
#define LVDS_BIASE_PWR(x)       (((x)&1)<<4)    //0: power down     1: normal work
#define LVDS_LANE_IN_FORMAT(x)  (((x)&1)<<3)    //0: msb on D0  1:msb on D7
#define LVDS_INPUT_SOURCE(x)    (((x)&1)<<2)    //0: from lcd1  1:from lcd0 or scaler
#define LVDS_OUTPUT_FORMAT(x)   (((x)&3)<<0)    //00:8bit format-1  01:8bit format-2  10:8bit format-3   11:6bit format  
//LVDS_CON1
#define LVDS_OUT_ENABLE(x)      (((x)&0xf)<<4)  //0:output enable 1:output disable
#define LVDS_TX_PWR_ENABLE(x)   (((x)&0xf)<<0)  //0:working mode  1:power down
//LCD1_CON
#define LCD1_OUT_ENABLE(x)      (((x)&1)<<1)    //0:lcd1 as input 1:lcd1 as output
#define LCD1_OUT_SRC(x)         (((x)&1)<<0)    //0:from lcd0   1:from scaler
//SCL_CON0
#define SCL_BYPASS(x)           (((x)&1)<<4)    //0:not bypass  1:bypass
#define SCL_DEN_INV(x)          (((x)&1)<<3)    //scl_den_inv
#define SCL_H_V_SYNC_INV(x)     (((x)&1)<<2)    //scl_sync_inv
#define SCL_OUT_CLK_INV(x)      (((x)&1)<<1)    //scl_dclk_inv
#define SCL_ENABLE(x)           (((x)&1)<<0)    //scaler enable
//SCL_CON1
#define SCL_H_FACTOR_LSB(x)     ((x)&0xff)      //scl_h_factor[7:0]
//SCL_CON2
#define SCL_H_FACTOR_MSB(x)     (((x)>>8)&0x3f)      //scl_h_factor[13:8]
//SCL_CON3
#define SCL_V_FACTOR_LSB(x)     ((x)&0xff)      //scl_v_factor[7:0]
//SCL_CON4
#define SCL_V_FACTOR_MSB(x)     (((x)>>8)&0x3f)      //scl_v_factor[13:8]
//SCL_CON5
#define SCL_DSP_HST_LSB(x)      ((x)&0xff)      //dsp_frame_hst[7:0]
//SCL_CON6
#define SCL_DSP_HST_MSB(x)      (((x)>>8)&0xf)       //dsp_frame_hst[11:8]
//SCL_CON7
#define SCL_DSP_VST_LSB(x)      ((x)&0xff)      //dsp_frame_vst[7:0]
//SCL_CON8
#define SCL_DSP_VST_MSB(x)      (((x)>>8)&0xf)       //dsp_frame_vst[11:8]
//SCL_CON9
#define SCL_DSP_HTOTAL_LSB(x)   ((x)&0xff)      //dsp_frame_htotal[7:0]
//SCL_CON10
#define SCL_DSP_HTOTAL_MSB(x)   (((x)>>8)&0xf)       //dsp_frame_htotal[11:8]
//SCL_CON11
#define SCL_DSP_HS_END(x)       ((x)&0xff)      //dsp_hs_end
//SCL_CON12
#define SCL_DSP_HACT_ST_LSB(x)      ((x)&0xff)      //dsp_hact_st[7:0]
//SCL_CON13
#define SCL_DSP_HACT_ST_MSB(x)      (((x)>>8)&0x3)      //dsp_hact_st[9:8]
//SCL_CON14
#define SCL_DSP_HACT_END_LSB(x)   ((x)&0xff)      //dsp_hact_end[7:0]
//SCL_CON15
#define SCL_DSP_HACT_END_MSB(x)   (((x)>>8)&0xf)       //dsp_frame_htotal[11:8]
//SCL_CON16
#define SCL_DSP_VTOTAL_LSB(x)   ((x)&0xff)      //dsp_frame_vtotal[7:0]
//SCL_CON17
#define SCL_DSP_VTOTAL_MSB(x)   (((x)>>8)&0xf)       //dsp_frame_vtotal[11:8]
//SCL_CON18
#define SCL_DSP_VS_END(x)       ((x)&0xff)      //dsp_vs_end
//SCL_CON19
#define SCL_DSP_VACT_ST(x)      ((x)&0xff)      //dsp_vact_st[7:0]
//SCL_CON20
#define SCL_DSP_VACT_END_LSB(x)   ((x)&0xff)      //dsp_vact_end[7:0]
//SCL_CON21
#define SCL_DSP_VACT_END_MSB(x)   (((x)>>8)&0xf)       //dsp_frame_vtotal[11:8]
//SCL_CON22
#define SCL_H_BORD_ST_LSB(x)        ((x)&0xff)      //dsp_hbord_st[7:0]
//SCL_CON23
#define SCL_H_BORD_ST_MSB(x)        (((x)>>8)&0x3)      //dsp_hbord_st[9:8]
//SCL_CON24
#define SCL_H_BORD_END_LSB(x)        ((x)&0xff)      //dsp_hbord_end[7:0]
//SCL_CON25
#define SCL_H_BORD_END_MSB(x)        (((x)>>8)&0xf)      //dsp_hbord_end[11:8]
//SCL_CON26
#define SCL_V_BORD_ST(x)            ((x)&0xff)      //dsp_vbord_st[7:0]
//SCL_CON27
#define SCL_V_BORD_END_LSB(x)              ((x)&0xff)      //dsp_vbord_end[7:0]
//SCL_CON25
#define SCL_V_BORD_END_MSB(x)        (((x)>>8)&0xf)      //dsp_vbord_end[11:8]

/* Scaler PLL CONFIG */
#define S_PLL_NO_1	0
#define S_PLL_NO_2	1
#define S_PLL_NO_4	2
#define S_PLL_NO_8	3
#define S_PLL_M(x)  (((x)&0xff)<<8)
#define S_PLL_N(x)  (((x)&0xf)<<4)
#define S_PLL_NO(x) ((S_PLL_NO_##x)&0x3)

enum{
    HDMI_RATE_148500000,
    HDMI_RATE_74250000,
    HDMI_RATE_27000000,
};
/*     Scaler   clk setting */
#define SCALE_PLL(_parent_rate,_rate,_m,_n,_no) \
        HDMI_RATE_ ## _parent_rate ##_S_RATE_ ## _rate \
        =  S_PLL_M(_m) | S_PLL_N(_n) | S_PLL_NO(_no)    
#define SCALE_RATE(_parent_rate , _rate) \
        (HDMI_RATE_ ## _parent_rate ## _S_RATE_ ## _rate)
        
enum{
    SCALE_PLL(148500000,	108000000,	16, 11, 2),
    SCALE_PLL(148500000,	99000000,	16, 6, 4),
    SCALE_PLL(148500000,    67500000,   20, 11, 4),	
    SCALE_PLL(148500000,    66000000,   16, 9,  4),
    SCALE_PLL(148500000,    57375000,   17, 11, 4),
    SCALE_PLL(148500000,    54000000,   16, 11, 4),   
    SCALE_PLL(148500000,    41250000,   20,  9, 8), 
    SCALE_PLL(148500000,    33000000,   16, 9,  8),
    SCALE_PLL(148500000,    30375000,   18, 11, 8),
    SCALE_PLL(148500000,    29700000,   16, 10, 8),
    SCALE_PLL(148500000,    25312500,   15, 11, 8),
    SCALE_PLL(148500000,    74250000,   12, 6, 4),
     SCALE_PLL(148500000,    35062500,   17, 9,  8),

    SCALE_PLL(74250000,     108000000,  32, 11,  2),       
    SCALE_PLL(74250000,     99000000,   16, 3,  4),
    SCALE_PLL(74250000,     66000000,   32, 9,  4),
    SCALE_PLL(74250000,     57375000,   34, 11, 4),
    SCALE_PLL(74250000,     54000000,   32, 11, 4),
    SCALE_PLL(74250000,     41250000,   40, 9, 8),
    SCALE_PLL(74250000,     33000000,   32, 9,  8),
    SCALE_PLL(74250000,     30375000,   36, 11, 8),
    SCALE_PLL(74250000,     25312500,   30, 11, 8),
    SCALE_PLL(74250000,     74250000,   12, 3, 4),
    SCALE_PLL(74250000,    67500000,   40, 11, 4),
    SCALE_PLL(74250000,     35062500,   34,9,8),

    SCALE_PLL(27000000,     108000000,   16, 2,  2),     
    SCALE_PLL(27000000,     95250000,   127, 9,  4),
    SCALE_PLL(27000000,     75000000,   100, 9,  4),
    SCALE_PLL(27000000,     72000000,   32, 3,  4),
    SCALE_PLL(27000000,     63281250,   75, 4,  8),
    SCALE_PLL(27000000,     54375000,   145, 9,  8),
    SCALE_PLL(27000000,     31500000,   28, 3,  8),
    SCALE_PLL(27000000,     30000000,   80, 9,  8),
    SCALE_PLL(27000000,     42187500,   25, 2,  8),   
    SCALE_PLL(27000000,     45000000,   40, 3,  8),
    SCALE_PLL(27000000,     35156250,   125,12, 8),//m=125 n=12 no=8:wq
    SCALE_PLL(27000000,     70312500,   125, 6,  8)
};

enum {
    LCD_OUT_SCL,
    LCD_OUT_BYPASS,
    LCD_OUT_DISABLE,
};
struct rk610_pll_info{
    u32 parent_rate;
    u32 rate;
    int m;
    int n;
    int od;
};
struct lcd_mode_inf{
	int h_pw;
	int h_bp;
	int h_vd;
	int h_fp;
	int v_pw;
	int v_bp;
	int v_vd;
	int v_fp;
	int f_hst;
	int f_vst;
    struct rk610_pll_info pllclk;
};
struct scl_hv_info{
    int scl_h ;
    int scl_v;
    };

struct scl_info{
    bool pll_pwr;
    bool scl_pwr;
    struct scl_hv_info scl_hv;
};
struct rk610_lcd_info{
    int disp_mode;
    
    struct rk29fb_screen *screen;
    struct scl_info scl_inf;
    struct i2c_client *client;

#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend		early_suspend;
#endif
};
extern int rk610_lcd_init(struct rk610_core_info *rk610_core_info);
extern int rk610_lcd_scaler_set_param(struct rk29fb_screen *screen,bool enable );
#endif
