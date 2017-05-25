/* 
 * drivers/input/touchscreen/ft5x0x_ts.c
 *
 * FocalTech ft5x0x TouchScreen driver. 
 *
 * Copyright (c) 2010  Focal tech Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * VERSION      	DATE			AUTHOR
 *    1.0		  2010-01-05			WenFS
 *
 * note: only support mulititouch	Wenfs 2010-10-01
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/freezer.h>
#include <linux/proc_fs.h>
#include <linux/clk.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <asm/io.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <linux/poll.h>
#include <linux/kfifo.h>
#include <linux/version.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/irq.h>
#include <linux/async.h>
#include <mach/board.h>
#include <linux/regulator/consumer.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#include "ft5x06_ts.h"
static struct i2c_client *this_client;
static int m_init = 0;

#define CFG_MAX_TOUCH_POINTS 5
#define CONFIG_FT5X0X_MULTITOUCH 1

struct ts_event {
	u16	x[CFG_MAX_TOUCH_POINTS];
	u16	y[CFG_MAX_TOUCH_POINTS];
	u16 id[CFG_MAX_TOUCH_POINTS];
	u16 valid[CFG_MAX_TOUCH_POINTS];
	u16	pressure;
	u8  touch_point;
};

struct ft5x0x_ts_data {
	int    irq;
	int    key;
	int    sleep;
	int    dc_status;
	int    points_last_state;
	struct ts_event		event;
	struct input_dev	*input_dev;
	struct work_struct 	pen_event_work;
	struct early_suspend	early_suspend;
	struct workqueue_struct *ts_workqueue;
	struct timer_list touch_release_timer;
};

int ft5x0x_i2c_Read(struct i2c_client *client, char *writebuf,
		    int writelen, char *readbuf, int readlen)
{
	int ret;

	if (writelen > 0) {
		struct i2c_msg msgs[] = {
			{
			 .addr = client->addr,
			 .flags = 0,
			 .len = writelen,
			 .buf = writebuf,
			 .scl_rate = 100000,
			 },
			{
			 .addr = client->addr,
			 .flags = I2C_M_RD,
			 .len = readlen,
			 .buf = readbuf,
			 .scl_rate = 200000,
			 },
		};
		ret = i2c_transfer(client->adapter, msgs, 2);
		if (ret < 0)
			dev_err(&client->dev, "f%s: i2c read error.\n",
				__func__);
	} else {
		struct i2c_msg msgs[] = {
			{
			 .addr = client->addr,
			 .flags = I2C_M_RD,
			 .len = readlen,
			 .buf = readbuf,
			 .scl_rate = 200000,
			 },
		};
		ret = i2c_transfer(client->adapter, msgs, 1);
		if (ret < 0)
			dev_err(&client->dev, "%s:i2c read error.\n", __func__);
	}
	return ret;
}
/*write data by i2c*/
int ft5x0x_i2c_Write(struct i2c_client *client, char *writebuf, int writelen)
{
	int ret;

	struct i2c_msg msg[] = {
		{
		 .addr = client->addr,
		 .flags = 0,
		 .len = writelen,
		 .buf = writebuf,
		 .scl_rate = 200000,
		 },
	};

	ret = i2c_transfer(client->adapter, msg, 1);
	if (ret < 0)
		dev_err(&client->dev, "%s i2c write error.\n", __func__);

	return ret;
}

/***********************************************************************************************
Name	:	ft5x0x_i2c_rxdata 

Input	:	*rxdata
                     *length

Output	:	ret

function	:	

***********************************************************************************************/
static int ft5x0x_i2c_rxdata(char *rxdata, int length)
{
	int ret;

	struct i2c_msg msgs[] = {
		{
			.addr	= this_client->addr,
			.flags	= 0,
			.len	= 1,
			.buf	= rxdata,
			.scl_rate = 100000,
		},
		{
			.addr	= this_client->addr,
			.flags	= I2C_M_RD,
			.len	= length,
			.buf	= rxdata,
			.scl_rate = 100000,
		},
	};

    //msleep(1);
	ret = i2c_transfer(this_client->adapter, msgs, 2);
	if (ret < 0)
		pr_err("msg %s i2c read error: %d\n", __func__, ret);
	
	return ret;
}
/***********************************************************************************************
Name	:	 

Input	:	
                     

Output	:	

function	:	

***********************************************************************************************/
static int ft5x0x_i2c_txdata(char *txdata, int length)
{
	int ret;

	struct i2c_msg msg[] = {
		{
			.addr	= this_client->addr,
			.flags	= 0,
			.len	= length,
			.buf	= txdata,
			.scl_rate = 100000,
		},
	};

   	//msleep(1);
	ret = i2c_transfer(this_client->adapter, msg, 1);
	if (ret < 0)
		pr_err("%s i2c write error: %d\n", __func__, ret);

	return ret;
}
/***********************************************************************************************
Name	:	 ft5x0x_write_reg

Input	:	addr -- address
                     para -- parameter

Output	:	

function	:	write register of ft5x0x

***********************************************************************************************/
static int ft5x0x_write_reg(u8 addr, u8 para)
{
    u8 buf[3];
    int ret = -1;

    buf[0] = addr;
    buf[1] = para;
    ret = ft5x0x_i2c_txdata(buf, 2);
    if (ret < 0) {
        pr_err("write reg failed! %#x ret: %d", buf[0], ret);
        return -1;
    }
    
    return 0;
}


/***********************************************************************************************
Name	:	ft5x0x_read_reg 

Input	:	addr
                     pdata

Output	:	

function	:	read register of ft5x0x

***********************************************************************************************/
static int ft5x0x_read_reg(u8 addr, u8 *pdata)
{
	int ret;
	u8 recv[2] = {0};
	u8 buf[2] = {addr, 0};
	struct i2c_msg msgs[] = {
		{
			.addr	= this_client->addr,
			.flags	= 0,
			.len	= 1,
			.buf	= buf,
			.scl_rate = 100000,
		},
		{
			.addr	= this_client->addr,
			.flags	= I2C_M_RD,
			.len	= 1,
			.buf	= recv,
			.scl_rate = 100000,
		},
	};
	//msleep(1);
	ret = i2c_transfer( this_client->adapter, msgs, 2 );
	if (ret < 0)
		pr_err("msg %s i2c read error: %d\n", __func__, ret);
	*pdata = recv[0];
	return ret;
  
}


/***********************************************************************************************
Name	:	 ft5x0x_read_fw_ver

Input	:	 void
                     

Output	:	 firmware version 	

function	:	 read TP firmware version

***********************************************************************************************/
static unsigned char ft5x0x_read_fw_ver(void)
{
	unsigned char ver;
	ft5x0x_read_reg(FT5X0X_REG_FIRMID, &ver);
	return(ver);
}

static unsigned char ft5x0x_read_mode(void)
{
	unsigned char ver;
	ft5x0x_read_reg( 0x00, &ver);
	return(ver);
}

#define CONFIG_SUPPORT_FTS_CTP_UPG


#ifdef CONFIG_SUPPORT_FTS_CTP_UPG

typedef enum
{
    ERR_OK,
    ERR_MODE,
    ERR_READID,
    ERR_ERASE,
    ERR_STATUS,
    ERR_ECC,
    ERR_DL_ERASE_FAIL,
    ERR_DL_PROGRAM_FAIL,
    ERR_DL_VERIFY_FAIL
}E_UPGRADE_ERR_TYPE;

typedef unsigned char         FTS_BYTE;     //8 bit
typedef unsigned short        FTS_WORD;    //16 bit
typedef unsigned int          FTS_DWRD;    //16 bit
typedef unsigned char         FTS_BOOL;    //8 bit

#define FTS_NULL                0x0
#define FTS_TRUE                0x01
#define FTS_FALSE              0x0

#define I2C_CTPM_ADDRESS       0xFF //no reference!


void delay_qt_ms(unsigned long  w_ms)
{
    unsigned long i;
    unsigned long j;

    for (i = 0; i < w_ms; i++)
    {
        for (j = 0; j < 1000; j++)
        {
            udelay(1);
        }
    }
}


/*
[function]: 
    callback: read data from ctpm by i2c interface,implemented by special user;
[parameters]:
    bt_ctpm_addr[in]    :the address of the ctpm;
    pbt_buf[out]        :data buffer;
    dw_lenth[in]        :the length of the data buffer;
[return]:
    FTS_TRUE     :success;
    FTS_FALSE    :fail;
*/
FTS_BOOL i2c_read_interface(FTS_BYTE bt_ctpm_addr, FTS_BYTE* pbt_buf, FTS_DWRD dw_lenth)
{
    int ret;
    
    ret=i2c_master_recv(this_client, pbt_buf, dw_lenth);

    if(ret<=0)
    {
        printk("[TSP]i2c_read_interface error\n");
        return FTS_FALSE;
    }
  
    return FTS_TRUE;
}

/*
[function]: 
    callback: write data to ctpm by i2c interface,implemented by special user;
[parameters]:
    bt_ctpm_addr[in]    :the address of the ctpm;
    pbt_buf[in]        :data buffer;
    dw_lenth[in]        :the length of the data buffer;
[return]:
    FTS_TRUE     :success;
    FTS_FALSE    :fail;
*/
FTS_BOOL i2c_write_interface(FTS_BYTE bt_ctpm_addr, FTS_BYTE* pbt_buf, FTS_DWRD dw_lenth)
{
    int ret;
    ret=i2c_master_send(this_client, pbt_buf, dw_lenth);
    if(ret<=0)
    {
        printk("[TSP]i2c_write_interface error line = %d, ret = %d\n", __LINE__, ret);
        return FTS_FALSE;
    }

    return FTS_TRUE;
}

/*
[function]: 
    send a command to ctpm.
[parameters]:
    btcmd[in]        :command code;
    btPara1[in]    :parameter 1;    
    btPara2[in]    :parameter 2;    
    btPara3[in]    :parameter 3;    
    num[in]        :the valid input parameter numbers, if only command code needed and no parameters followed,then the num is 1;    
[return]:
    FTS_TRUE    :success;
    FTS_FALSE    :io fail;
*/
FTS_BOOL cmd_write(FTS_BYTE btcmd,FTS_BYTE btPara1,FTS_BYTE btPara2,FTS_BYTE btPara3,FTS_BYTE num)
{
    FTS_BYTE write_cmd[4] = {0};

    write_cmd[0] = btcmd;
    write_cmd[1] = btPara1;
    write_cmd[2] = btPara2;
    write_cmd[3] = btPara3;
    return i2c_write_interface(I2C_CTPM_ADDRESS, write_cmd, num);
}

/*
[function]: 
    write data to ctpm , the destination address is 0.
[parameters]:
    pbt_buf[in]    :point to data buffer;
    bt_len[in]        :the data numbers;    
[return]:
    FTS_TRUE    :success;
    FTS_FALSE    :io fail;
*/
FTS_BOOL byte_write(FTS_BYTE* pbt_buf, FTS_DWRD dw_len)
{
    
    return i2c_write_interface(I2C_CTPM_ADDRESS, pbt_buf, dw_len);
}

/*
[function]: 
    read out data from ctpm,the destination address is 0.
[parameters]:
    pbt_buf[out]    :point to data buffer;
    bt_len[in]        :the data numbers;    
[return]:
    FTS_TRUE    :success;
    FTS_FALSE    :io fail;
*/
FTS_BOOL byte_read(FTS_BYTE* pbt_buf, FTS_BYTE bt_len)
{
    return i2c_read_interface(I2C_CTPM_ADDRESS, pbt_buf, bt_len);
}


/*
[function]: 
    burn the FW to ctpm.
[parameters]:(ref. SPEC)
    pbt_buf[in]    :point to Head+FW ;
    dw_lenth[in]:the length of the FW + 6(the Head length);    
    bt_ecc[in]    :the ECC of the FW
[return]:
    ERR_OK        :no error;
    ERR_MODE    :fail to switch to UPDATE mode;
    ERR_READID    :read id fail;
    ERR_ERASE    :erase chip fail;
    ERR_STATUS    :status error;
    ERR_ECC        :ecc error.
*/


#define    FTS_PACKET_LENGTH        32

static unsigned char CTPM_FW[]=
{
  //#include "ft5606_all_Ver0x14_20120508_app.i"
};

E_UPGRADE_ERR_TYPE  fts_ctpm_fw_upgrade(FTS_BYTE* pbt_buf, FTS_DWRD dw_lenth)
{
    FTS_BYTE reg_val[2] = {0};
    FTS_DWRD i = 0;

    FTS_DWRD  packet_number;
    FTS_DWRD  j;
    FTS_DWRD  temp;
    FTS_DWRD  lenght;
    FTS_BYTE  packet_buf[FTS_PACKET_LENGTH + 6];
    FTS_BYTE  auc_i2c_write_buf[10];
    FTS_BYTE bt_ecc;
    int      i_ret;

    /*********Step 1:Reset  CTPM *****/
    /*write 0xaa to register 0xfc*/
    ft5x0x_write_reg(0xfc,0xaa);
    delay_qt_ms(50);
     /*write 0x55 to register 0xfc*/
    ft5x0x_write_reg(0xfc,0x55);
    printk("[TSP] Step 1: Reset CTPM test\n");
   
    delay_qt_ms(30);   


    /*********Step 2:Enter upgrade mode *****/
    auc_i2c_write_buf[0] = 0x55;
    auc_i2c_write_buf[1] = 0xaa;
    do
    {
        i ++;
        i_ret = ft5x0x_i2c_txdata(auc_i2c_write_buf, 2);
        delay_qt_ms(5);
    }while(i_ret <= 0 && i < 5 );

    /*********Step 3:check READ-ID***********************/    
    delay_qt_ms(55);   

	auc_i2c_write_buf[0] = 0x90;	
	auc_i2c_write_buf[1] = auc_i2c_write_buf[2] = auc_i2c_write_buf[3] = 0x00;
	ft5x0x_i2c_Read(this_client, auc_i2c_write_buf, 4,reg_val, 2);
    //cmd_write(0x90,0x00,0x00,0x00,4);
    //byte_read(reg_val,2);
	printk("CTPM ID,ID1 = 0x%x,ID2 = 0x%x\n",reg_val[0],reg_val[1]);
    if (reg_val[0] == 0x79 && reg_val[1] == 0x6)
    {
        printk("[TSP] Step 3: CTPM ID,ID1 = 0x%x,ID2 = 0x%x\n",reg_val[0],reg_val[1]);
    }
    else
    {
        return ERR_READID;
        //i_is_new_protocol = 1;
    }

     /*********Step 4:erase app*******************************/
	auc_i2c_write_buf[0] = 0xcd;	
	ft5x0x_i2c_Read(this_client, auc_i2c_write_buf, 1, reg_val, 1);    
	//cmd_write(0x61,0x00,0x00,0x00,1);
   
    //delay_qt_ms(1500);
    printk("[TSP] Step 4: erase. \n");
	auc_i2c_write_buf[0] = 0x61;	
	ft5x0x_i2c_Write(this_client, auc_i2c_write_buf, 1); 
	/*erase app area */ 
    delay_qt_ms(2000);
	/*erase panel parameter area */ 
	auc_i2c_write_buf[0] = 0x63;	
	ft5x0x_i2c_Write(this_client, auc_i2c_write_buf, 1);
    delay_qt_ms(100);

    /*********Step 5:write firmware(FW) to ctpm flash*********/
    bt_ecc = 0;
    printk("[TSP] Step 5: start upgrade. \n");
    dw_lenth = dw_lenth - 8;
    packet_number = (dw_lenth) / FTS_PACKET_LENGTH;
    packet_buf[0] = 0xbf;
    packet_buf[1] = 0x00;
    for (j=0;j<packet_number;j++)
    {
        temp = j * FTS_PACKET_LENGTH;
        packet_buf[2] = (FTS_BYTE)(temp>>8);
        packet_buf[3] = (FTS_BYTE)temp;
        lenght = FTS_PACKET_LENGTH;
        packet_buf[4] = (FTS_BYTE)(lenght>>8);
        packet_buf[5] = (FTS_BYTE)lenght;

        for (i=0;i<FTS_PACKET_LENGTH;i++)
        {
            packet_buf[6+i] = pbt_buf[j*FTS_PACKET_LENGTH + i]; 
            bt_ecc ^= packet_buf[6+i];
        }
        
        //byte_write(&packet_buf[0],FTS_PACKET_LENGTH + 6);
        ft5x0x_i2c_Write(this_client, packet_buf, FTS_PACKET_LENGTH + 6);
        delay_qt_ms(FTS_PACKET_LENGTH/6 + 1);
        if ((j * FTS_PACKET_LENGTH % 1024) == 0)
        {
              printk("[TSP] upgrade the 0x%x th byte.\n", ((unsigned int)j) * FTS_PACKET_LENGTH);
        }
    }

    if ((dw_lenth) % FTS_PACKET_LENGTH > 0)
    {
        temp = packet_number * FTS_PACKET_LENGTH;
        packet_buf[2] = (FTS_BYTE)(temp>>8);
        packet_buf[3] = (FTS_BYTE)temp;

        temp = (dw_lenth) % FTS_PACKET_LENGTH;
        packet_buf[4] = (FTS_BYTE)(temp>>8);
        packet_buf[5] = (FTS_BYTE)temp;

        for (i=0;i<temp;i++)
        {
            packet_buf[6+i] = pbt_buf[ packet_number*FTS_PACKET_LENGTH + i]; 
            bt_ecc ^= packet_buf[6+i];
        }

        //byte_write(&packet_buf[0],temp+6);    
        ft5x0x_i2c_Write(this_client, packet_buf, temp + 6);
        delay_qt_ms(20);
    }

    //send the last six byte
    for (i = 0; i<6; i++)
    {
        temp = 0x6ffa + i;
        packet_buf[2] = (FTS_BYTE)(temp>>8);
        packet_buf[3] = (FTS_BYTE)temp;
        temp =1;
        packet_buf[4] = (FTS_BYTE)(temp>>8);
        packet_buf[5] = (FTS_BYTE)temp;
        packet_buf[6] = pbt_buf[ dw_lenth + i]; 
        bt_ecc ^= packet_buf[6];

        //byte_write(&packet_buf[0],7);  
        ft5x0x_i2c_Write(this_client, packet_buf, 7);
        delay_qt_ms(20);
    }

    /*********Step 6: read out checksum***********************/
    /*send the opration head*/
    //cmd_write(0xcc,0x00,0x00,0x00,1);
    //byte_read(reg_val,1);
    auc_i2c_write_buf[0] = 0xcc;	
    ft5x0x_i2c_Read(this_client, auc_i2c_write_buf, 1, reg_val, 1);
    printk("[TSP] Step 6:  ecc read 0x%x, new firmware 0x%x. \n", reg_val[0], bt_ecc);
    if(reg_val[0] != bt_ecc)
    {
        //return ERR_ECC;
    }

    /*********Step 7: reset the new FW***********************/
    //cmd_write(0x07,0x00,0x00,0x00,1);
	auc_i2c_write_buf[0] = 0x07;	
	ft5x0x_i2c_Write(this_client, auc_i2c_write_buf, 1); 
	delay_qt_ms(300);	/*make sure CTP startup normally */
    printk("[TSP] Step 7:  Update ok. \n");

    return ERR_OK;
}


int fts_ctpm_fw_upgrade_with_i_file(void)
{
   FTS_BYTE*     pbt_buf = FTS_NULL;
   int i_ret;
    
    //=========FW upgrade========================*/
   pbt_buf = CTPM_FW;
   /*call the upgrade function*/
   i_ret =  fts_ctpm_fw_upgrade(pbt_buf,sizeof(CTPM_FW));
   if (i_ret != 0)
   {
       //error handling ...
       //TBD
   }

   return i_ret;
}

unsigned char fts_ctpm_get_upg_ver(void)
{
    unsigned int ui_sz;
    ui_sz = sizeof(CTPM_FW);
    if (ui_sz > 2)
    {
        return CTPM_FW[ui_sz - 2];
    }
    else
    {
        //TBD, error handling?
        return 0xff; //default value
    }
}

#endif


/***********************************************************************************************
Name	:	 

Input	:	
                     

Output	:	

function	:	

***********************************************************************************************/
static void ft5x0x_ts_release(void)
{
	int i;
	struct ft5x0x_ts_data *data = i2c_get_clientdata(this_client);
	data->key = 0;
#ifdef CONFIG_FT5X0X_MULTITOUCH	
	for ( i = 0; i < 5; i++ ){
		if ( data->points_last_state & (1 << i) ){
				input_mt_slot( data->input_dev, i );
				input_mt_report_slot_state( data->input_dev, MT_TOOL_FINGER, false );
				data->points_last_state &= (~(1 << i));
				//printk( "ft5x0x_ts_release = %d\n", i );				
		}
	}
	//input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, 0);
#else
	input_report_abs(data->input_dev, ABS_PRESSURE, 0);
	input_report_key(data->input_dev, BTN_TOUCH, 0);
#endif
	input_sync(data->input_dev);
//	printk("%s..data->points_last_state = %d\n", __FUNCTION__, data->points_last_state);
}


void fx5x6_set_dc_status( int status )
{
	struct ft5x0x_ts_data *data = NULL;
	if ( 0 == m_init ){
		return;
	}
	data = i2c_get_clientdata(this_client);
	if ( data->dc_status != status ){
		char value = 0;
		ft5x0x_write_reg( 0xb0, status ? 1 : 0 );
		ft5x0x_read_reg( 0xb0, &value );
		data->dc_status = status;
		printk( "ft5x0x_read_reg 0xb0 = 0x%x\n", value );
	}
}

static int ft5x0x_read_data(void)
{
	int i;
	struct ft5x0x_ts_data *data = i2c_get_clientdata(this_client);
	struct ts_event *event = &data->event;
//	u8 buf[14] = {0};
	u8 buf[32] = {0};
	int ret = -1;

#ifdef CONFIG_FT5X0X_MULTITOUCH
//	ret = ft5x0x_i2c_rxdata(buf, 13);
	ret = ft5x0x_i2c_rxdata(buf, 31);
#else
	ret = ft5x0x_i2c_rxdata(buf, 7);
#endif
	if (ret < 0) {
		printk("%s read_data i2c_rxdata failed: %d\n", __func__, ret);
		return ret;
	}

	memset(event, 0, sizeof(struct ts_event));
//	event->touch_point = buf[2] & 0x03;// 0000 0011
	event->touch_point = buf[2] & 0x07;// 000 0111
	
//	printk( "event->touch_point = %d\n", event->touch_point );
	if (event->touch_point == 0) {
		ft5x0x_ts_release();
		return 1; 
	}
#ifdef CONFIG_FT5X0X_MULTITOUCH
	for( i = 0; i < 5; i++ ){
		int offset = i * 6 + 3;
		event->x[i] = (((s16)(buf[offset+0] & 0x0F))<<8) | ((s16)buf[offset+1]);
		event->y[i] = (((s16)(buf[offset+2] & 0x0F))<<8) | ((s16)buf[offset+3]);
		event->id[i] = (s16)(buf[offset+2] & 0xF0)>>4;
		event->valid[i] = ((buf[offset+0] & 0xc0) >> 6);
		event->pressure = 200;
		//printk( "event->valid[i] = 0x%x, %d, %d\n", event->valid[i], event->x[i], event->y[i]  );
	}
#else
    if (event->touch_point == 1) {
    	event->x[0] = (s16)(buf[3] & 0x0F)<<8 | (s16)buf[4];
    	event->y[0] = (s16)(buf[5] & 0x0F)<<8 | (s16)buf[6];
			//event->y1 = SCREEN_MAX_Y - event->y1;
    }
#endif
    event->pressure = 200;

	dev_dbg(&this_client->dev, "%s: 1:%d %d 2:%d %d \n", __func__,
		event->x[0], event->y[0], event->x[1], event->y[1]);
	//printk("%d (%d, %d), (%d, %d)\n", event->touch_point, event->x[0], event->y[0], event->x[1], event->y[1]);

    return 0;
}
/***********************************************************************************************
Name	:	 

Input	:	
                     

Output	:	

function	:	

***********************************************************************************************/
static void ft5x0x_report_value(void)
{
	int i;
	struct ft5x0x_ts_data *data = i2c_get_clientdata(this_client);
	struct ts_event *event = &data->event;

	//printk("==ft5x0x_report_value = %d\n", event->touch_point);
#ifdef CONFIG_FT5X0X_MULTITOUCH
	for ( i = 0; i < 5; i++ ){
		if ( (data->event.valid[i] != 2) && (data->points_last_state & (1 << i)) ){
			input_mt_slot( data->input_dev, i );
			input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, 0);
			input_mt_report_slot_state( data->input_dev, MT_TOOL_FINGER, false );
			data->points_last_state &= (~(1 << i));
		}else if( data->event.valid[i] == 2 ){
			input_mt_slot(data->input_dev, i);
			input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, event->pressure);
			input_report_abs(data->input_dev, ABS_MT_WIDTH_MAJOR, 1);
			input_report_abs(data->input_dev, ABS_MT_POSITION_X,  event->x[i]);
			input_report_abs(data->input_dev, ABS_MT_POSITION_Y,  event->y[i]);	
			input_report_abs(data->input_dev, ABS_MT_TRACKING_ID, event->id[i]);	
			input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, true);
			data->points_last_state |= (1 << i);
			//printk( "data->points_last_state = %d\n", data->points_last_state );						  		
		}
	}
#else	/* CONFIG_FT5X0X_MULTITOUCH*/
	if (event->touch_point == 1) {
			input_mt_slot( data->input_dev, 0 );
			input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, event->pressure);
			input_report_abs(data->input_dev, ABS_MT_WIDTH_MAJOR, 1);
			input_report_abs(data->input_dev, ABS_MT_POSITION_X, event->x1);
			input_report_abs(data->input_dev, ABS_MT_POSITION_Y, event->y1);
			input_report_abs(data->input_dev, ABS_MT_TRACKING_ID, event->id[0]);	
			input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, true);
	}
#endif	/* CONFIG_FT5X0X_MULTITOUCH*/
	input_sync(data->input_dev);

	dev_dbg(&this_client->dev, "%s: 1:%d %d 2:%d %d \n", __func__,
		event->x[0], event->y[0], event->x[1], event->y[1]);
}	/*end ft5x0x_report_value*/

static void touch_release_timer(unsigned long _data)
{
	struct ft5x0x_ts_data *ft5x0x_ts = (struct ft5x0x_ts_data *)_data;
	if ( ft5x0x_ts->points_last_state == 0 ){
		return;
	}
	//printk("%s\n", __FUNCTION__);
	ft5x0x_ts_release();
}

/***********************************************************************************************
Name	:	 

Input	:	
                     

Output	:	

function	:	

***********************************************************************************************/
static void ft5x0x_ts_pen_irq_work(struct work_struct *work)
{
	int ret = -1;
	struct ft5x0x_ts_data *ft5x0x_ts = container_of( work, struct ft5x0x_ts_data, pen_event_work );
	if ( 1 == ft5x0x_ts->sleep ){
		return;
	}

	ret = ft5x0x_read_data();
	if (ret == 0) {	
		mod_timer(&ft5x0x_ts->touch_release_timer,
				jiffies + msecs_to_jiffies(300));
		ft5x0x_report_value();
	}
//	else printk("data package read error\n");
//	printk("==work 2=\n");
//    	msleep(1);
	if ( 0 == ft5x0x_ts->sleep ){
		//enable_irq( ft5x0x_ts->irq );
	}
}
/***********************************************************************************************
Name	:	 

Input	:	
                     

Output	:	

function	:	

***********************************************************************************************/
static irqreturn_t ft5x0x_ts_interrupt(int irq, void *dev_id)
{
	struct ft5x0x_ts_data *ft5x0x_ts = dev_id;
	//disable_irq_nosync( ft5x0x_ts->irq );
	//printk("==%s=\n", __FUNCTION__);
	//if (!work_pending(&ft5x0x_ts->pen_event_work)) {
	queue_work(ft5x0x_ts->ts_workqueue, &ft5x0x_ts->pen_event_work);
	//}

	return IRQ_HANDLED;
}
#ifdef CONFIG_HAS_EARLYSUSPEND
/***********************************************************************************************
Name	:	 

Input	:	
                     

Output	:	

function	:	

***********************************************************************************************/
static void ft5x0x_ts_suspend(struct early_suspend *handler)
{
	struct ft5x0x_ts_data *ts = container_of(handler, struct ft5x0x_ts_data, early_suspend);
	ts->sleep = 1;
	cancel_work_sync( &ts->pen_event_work );
	flush_workqueue( ts->ts_workqueue );
	ft5x0x_ts_release();
	printk("==ft5x0x_ts_suspend=\n");
	//disable_irq_nosync( ts->irq );
	//==set mode ==, 
	//ft5x0x_set_reg( FT5X0X_REG_PMODE, PMODE_HIBERNATE );
}
/***********************************************************************************************
Name	:	 

Input	:	
                     

Output	:	

function	:	

***********************************************************************************************/
static void ft5x0x_ts_resume(struct early_suspend *handler)
{
	struct ft5x0x_ts_data *ts = container_of(handler, struct ft5x0x_ts_data, early_suspend);
	struct ft5x0x_platform_data *pdata = this_client->dev.platform_data;
	printk("==ft5x0x_ts_resume=\n");
	ts->sleep = 0;
	if ( pdata->platform_wakeup ){
		pdata->platform_wakeup();
	}
	ts->dc_status = -1;
	queue_work( ts->ts_workqueue, &ts->pen_event_work );
}
#endif  //CONFIG_HAS_EARLYSUSPEND
/***********************************************************************************************
Name	:	 

Input	:	
                     

Output	:	

function	:	

***********************************************************************************************/
static unsigned char m_ft5x06_keycode[] __initdata = {
	KEY_F1,
	KEY_BACK,
	KEY_HOME,
	KEY_SEARCH,
};

static int 
ft5x0x_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int err = 0;
	char reset = 0x07;
	unsigned char uc_reg_value, fw_ver; 
	struct input_dev *input_dev;
	struct ft5x0x_ts_data *ft5x0x_ts;
	struct ft5x0x_platform_data *pdata = client->dev.platform_data;

	printk("==ft5x0x_ts_probe=\n");
	
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		err = -ENODEV;
		goto exit_check_functionality_failed;
	}
	ft5x0x_ts = (struct ft5x0x_ts_data *)kzalloc(sizeof(*ft5x0x_ts), GFP_KERNEL);
	if (!ft5x0x_ts)	{
		err = -ENOMEM;
		goto exit_alloc_data_failed;
	}
	this_client = client;
	i2c_set_clientdata(client, ft5x0x_ts);


	INIT_WORK(&ft5x0x_ts->pen_event_work, ft5x0x_ts_pen_irq_work);

	ft5x0x_ts->ts_workqueue = create_workqueue("touch");
	//ft5x0x_ts->ts_workqueue = create_singlethread_workqueue(dev_name(&client->dev));
	if (!ft5x0x_ts->ts_workqueue) {
		err = -ESRCH;
		goto exit_create_singlethread;
	}
	ft5x0x_ts->irq = client->irq;
	if ( pdata->init_platform_hw ){
		pdata->init_platform_hw();
	}
	if ( !ft5x0x_ts->irq ){
		dev_dbg( &client->dev, "no IRQ?\n" );
		err = -ESRCH;
		goto exit_platform_data_null;
	}else{
		ft5x0x_ts->irq = gpio_to_irq(ft5x0x_ts->irq);
	}
	err = request_irq( ft5x0x_ts->irq, ft5x0x_ts_interrupt, GPIOEdgelFalling, client->dev.driver->name, ft5x0x_ts );
	if (err < 0) {
		dev_err(&client->dev, "ft5x0x_probe: request irq failed\n");
		goto exit_irq_request_failed;
	}

	disable_irq( ft5x0x_ts->irq );

	input_dev = input_allocate_device();
	if (!input_dev) {
		err = -ENOMEM;
		dev_err(&client->dev, "failed to allocate input device\n");
		goto exit_input_dev_alloc_failed;
	}
	
	ft5x0x_ts->input_dev = input_dev;

#ifdef CONFIG_FT5X0X_MULTITOUCH
#if 0
	set_bit(ABS_MT_TOUCH_MAJOR, input_dev->absbit);
	set_bit(ABS_MT_POSITION_X, input_dev->absbit);
	set_bit(ABS_MT_POSITION_Y, input_dev->absbit);
	set_bit(ABS_MT_WIDTH_MAJOR, input_dev->absbit);

	input_set_abs_params(input_dev,
			     ABS_MT_POSITION_X, 0, SCREEN_MAX_X, 0, 0);
	input_set_abs_params(input_dev,
			     ABS_MT_POSITION_Y, 0, SCREEN_MAX_Y, 0, 0);
	input_set_abs_params(input_dev,
			     ABS_MT_TOUCH_MAJOR, 0, PRESS_MAX, 0, 0);
	input_set_abs_params(input_dev,
			     ABS_MT_WIDTH_MAJOR, 0, 200, 0, 0);
#else
	set_bit(INPUT_PROP_DIRECT, input_dev->propbit);
	set_bit(EV_ABS, input_dev->evbit);
	input_mt_init_slots(input_dev, 5);
	input_set_abs_params(input_dev, ABS_MT_POSITION_X, 0, (int)SCREEN_MAX_X, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, 0, (int)SCREEN_MAX_Y, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_WIDTH_MAJOR, 0, 200, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TRACKING_ID, 0, CFG_MAX_TOUCH_POINTS, 0, 0);
#endif
#else
	set_bit(ABS_X, input_dev->absbit);
	set_bit(ABS_Y, input_dev->absbit);
	set_bit(ABS_PRESSURE, input_dev->absbit);
	set_bit(BTN_TOUCH, input_dev->keybit);

	input_set_abs_params(input_dev, ABS_X, 0, SCREEN_MAX_X, 0, 0);
	input_set_abs_params(input_dev, ABS_Y, 0, SCREEN_MAX_Y, 0, 0);
	input_set_abs_params(input_dev,
			     ABS_PRESSURE, 0, PRESS_MAX, 0 , 0);
#endif

	set_bit(EV_ABS, input_dev->evbit);
	set_bit(EV_KEY, input_dev->evbit);

	input_dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS) | BIT_MASK(EV_MSC);
	input_dev->mscbit[0] = BIT_MASK(MSC_SCAN);
	set_bit(EV_SYN, input_dev->evbit);
	
	input_dev->keycode = m_ft5x06_keycode;
	input_dev->keycodesize = sizeof(unsigned char);
	input_dev->keycodemax = ARRAY_SIZE(m_ft5x06_keycode);

	set_bit( KEY_F1, input_dev->keybit );
	set_bit( KEY_BACK, input_dev->keybit );
	set_bit( KEY_HOME, input_dev->keybit );
	set_bit( KEY_SEARCH, input_dev->keybit );


	input_dev->name		= FT5X0X_NAME;		//dev_name(&client->dev)
	err = input_register_device(input_dev);
	if (err) {
		dev_err(&client->dev,
		"ft5x0x_ts_probe: failed to register input device: %s\n",
		dev_name(&client->dev));
		goto exit_input_register_device_failed;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	printk("==register_early_suspend =\n");
	ft5x0x_ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN - 2,
	//ft5x0x_ts->early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 1;
	ft5x0x_ts->early_suspend.suspend = ft5x0x_ts_suspend;
	ft5x0x_ts->early_suspend.resume	= ft5x0x_ts_resume;
	register_early_suspend(&ft5x0x_ts->early_suspend);
#endif
	msleep(500);
	//get some register information
	uc_reg_value = ft5x0x_read_fw_ver();
	fw_ver = uc_reg_value;
	printk("[FST] Firmware version = 0x%x\n", uc_reg_value);
	uc_reg_value = ft5x0x_read_mode( );
	printk("[FST] mode = 0x%x\n", uc_reg_value);
#if 0
	printk("[FST] Update Test\n");
	if (fw_ver < 0x14){
		printk("[FST] Start Update\n");
		fts_ctpm_fw_upgrade_with_i_file();
	}
#endif
	//wake the CTPM
	//	__gpio_as_output(GPIO_FT5X0X_WAKE);		
	//	__gpio_clear_pin(GPIO_FT5X0X_WAKE);		//set wake = 0,base on system
	//	 msleep(100);
	//	__gpio_set_pin(GPIO_FT5X0X_WAKE);			//set wake = 1,base on system
	//	msleep(100);
	//	ft5x0x_set_reg(0x88, 0x05); //5, 6,7,8
	//	ft5x0x_set_reg(0x80, 30);
	//	msleep(50);
	ft5x0x_ts->key = 0;
	memset( &ft5x0x_ts->event.valid, 0, sizeof(ft5x0x_ts->event.valid) );
	ft5x0x_ts->points_last_state = 0;
	setup_timer(&ft5x0x_ts->touch_release_timer,
	touch_release_timer, (unsigned long)ft5x0x_ts);
	enable_irq( ft5x0x_ts->irq );
	ft5x0x_i2c_Write(this_client, &reset, 1); 

	ft5x0x_ts->sleep = 0;
	ft5x0x_ts->dc_status = -1;
	m_init = 1;
	printk("==probe over =\n");
  return 0;

exit_input_register_device_failed:
	input_free_device(input_dev);
exit_input_dev_alloc_failed:
	free_irq( ft5x0x_ts->irq, ft5x0x_ts);
exit_irq_request_failed:
exit_platform_data_null:
	cancel_work_sync(&ft5x0x_ts->pen_event_work);
	destroy_workqueue(ft5x0x_ts->ts_workqueue);
exit_create_singlethread:
	printk("==singlethread error =\n");
	i2c_set_clientdata(client, NULL);
	kfree(ft5x0x_ts);
exit_alloc_data_failed:
exit_check_functionality_failed:
	return err;
}
/***********************************************************************************************
Name	:	 

Input	:	
                     

Output	:	

function	:	

***********************************************************************************************/
static int __devexit ft5x0x_ts_remove(struct i2c_client *client)
{
	struct ft5x0x_ts_data *ft5x0x_ts = i2c_get_clientdata(client);
	printk("==ft5x0x_ts_remove=\n");
	unregister_early_suspend(&ft5x0x_ts->early_suspend);
	free_irq( ft5x0x_ts->irq, ft5x0x_ts );
	input_unregister_device(ft5x0x_ts->input_dev);
	kfree(ft5x0x_ts);
	cancel_work_sync(&ft5x0x_ts->pen_event_work);
	destroy_workqueue(ft5x0x_ts->ts_workqueue);
	i2c_set_clientdata(client, NULL);
	return 0;
}

static const struct i2c_device_id ft5x0x_ts_id[] = {
	{ FT5X0X_NAME, 0 },{ }
};

#ifdef CONFIG_PM
static int ft5x0x_suspend( struct i2c_client *client, pm_message_t mesg )
{
	struct ft5x0x_platform_data *pdata = client->dev.platform_data;
	if ( pdata->platform_sleep ){
		pdata->platform_sleep();
	}
	return 0;
}


static int ft5x0x_resume(struct i2c_client *client)
{
#if 0
	struct ft5x0x_ts_data *ts = i2c_get_clientdata(client);
	struct ft5x0x_platform_data *pdata =  client->dev.platform_data;
	if ( pdata->platform_wakeup ){
		pdata->platform_wakeup();
	}
	enable_irq( ts->irq );
#endif
	return 0;
}
#else
#define ft5x0x_suspend NULL
#define ft5x0x_resume NULL
#endif

MODULE_DEVICE_TABLE(i2c, ft5x0x_ts_id);

static struct i2c_driver ft5x0x_ts_driver = {
	.probe		= ft5x0x_ts_probe,
	.remove		= __devexit_p(ft5x0x_ts_remove),
	.id_table	= ft5x0x_ts_id,
	.driver	= {
		.name	= FT5X0X_NAME,
		.owner	= THIS_MODULE,
	},
	.suspend  = ft5x0x_suspend,
	.resume   = ft5x0x_resume,
};

/***********************************************************************************************
Name	:	 

Input	:	
                     

Output	:	

function	:	

***********************************************************************************************/
static int __init ft5x0x_ts_init(void)
{
	int ret;
	printk("==ft5x0x_ts_init==\n");
	ret = i2c_add_driver(&ft5x0x_ts_driver);
	printk("ret=%d\n",ret);
	return ret;
//	return i2c_add_driver(&ft5x0x_ts_driver);
}

/***********************************************************************************************
Name	:	 

Input	:	
                     

Output	:	

function	:	

***********************************************************************************************/
static void __exit ft5x0x_ts_exit(void)
{
	printk("==ft5x0x_ts_exit==\n");
	i2c_del_driver(&ft5x0x_ts_driver);
}

module_init(ft5x0x_ts_init);
module_exit(ft5x0x_ts_exit);

MODULE_AUTHOR("<wenfs@Focaltech-systems.com>");
MODULE_DESCRIPTION("FocalTech ft5x0x TouchScreen driver");
MODULE_LICENSE("GPL");

