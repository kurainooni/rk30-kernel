/* arch/arm/mach-rockchip/rk28_battery.c
 *
 * Copyright (C) 2009 Rockchip Corporation.
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
 */
#include <linux/module.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/regulator/consumer.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <asm/io.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <mach/gpio.h>
#include <linux/adc.h>
#include <mach/iomux.h>
#include <mach/board.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/ktime.h>
#include <linux/irq.h>

#define BATT_VOLTAGE_MAX              8140
#define BATT_VOLTAGE_MIN              6200


/*use adc sample battery capacity*/
#define AC_INSERT_VALUE       100

#if defined(CONFIG_M79TG_3) || defined(CONFIG_M79TG_2D) || defined(CONFIG_M79TG_2)
#define BATT_FULL_VALUE	       4200
#define BATT_EMPTY_VALUE	     3500
#else
#define BATT_FULL_VALUE	       8140
#define BATT_EMPTY_VALUE	     6200
#endif

#define PERCENT				100
#define BATT_LEVEL_FULL		100
#define BATT_LEVEL_EMPTY	0
#define BATT_PRESENT_TRUE	 1
#define BATT_PRESENT_FALSE  0
#if defined(CONFIG_M79TG_3) || defined(CONFIG_M79TG_2D) || defined(CONFIG_M79TG_2)
#define BATT_NOMAL_VOL_VALUE	4000
#else
#define BATT_NOMAL_VOL_VALUE	7300
#endif
#define AD_SAMPLE_TIMES       6
#define AD_NO_BATT_VALE       100

struct battery_info
{
	int state;
	int health;
	int online;
	int voltage;
	int present;
	int capacity;
	int capacity_level;
};

struct rk3066_battery_data {
	int adc_val;
	int changed;
	int capacity;
	int dc_det_pin;
	spinlock_t lock;
	int batt_low_irq;
	int batt_low_pin;
	int charge_ok_pin;
	int dc_charge_irq;
	struct timeval tv;
	struct timeval s_tv;
	int backup_capacity;
	int charge_ok_number;
	struct power_supply	ac;
	int notebook_dc_dec_pin;
	struct delayed_work work;
	struct delayed_work wake;
	int dc_det_active_level;
	struct battery_info info;
	struct adc_client *client; 
	int charge_ok_active_level;
	struct workqueue_struct *wq;
	struct power_supply battery;
	struct completion completion;
	struct work_struct dcwakeup_work;
	struct work_struct lowerpower_work;
};

typedef enum {
	CHARGER_BATTERY = 0,
	CHARGER_USB,
	CHARGER_AC
} charger_type_t;

static struct rk3066_battery_data *m_battery_data = NULL;

extern int get_msc_connect_flag( void );

int rk3066_battery_get_dc_status( void )
{
	if ( NULL == m_battery_data ){
		return 0;
	}
	if ( gpio_get_value( m_battery_data->dc_det_pin ) == m_battery_data->dc_det_active_level ){
		return 1;
	}else{
		return 0;
	}
}

#ifdef CONFIG_NOTEBOOK
static int get_notebook_ac_status( struct rk3066_battery_data *battery_data )
{
	if ( gpio_get_value( battery_data->notebook_dc_dec_pin ) == battery_data->dc_det_active_level ){
		return 1;
	}else{
		return 0;
	}
}
#endif


static int get_ac_charge_status( struct rk3066_battery_data *battery_data )
{
	if ( gpio_get_value( battery_data->dc_det_pin ) == battery_data->dc_det_active_level ){
		return 1;
	}else{
		return 0;
	}
}

static int get_ac_all_status( struct rk3066_battery_data *battery_data )
{
	if ( get_ac_charge_status( battery_data ) ){
		return 1;
	}
#ifdef CONFIG_NOTEBOOK	
	return get_notebook_ac_status( battery_data );
#else
	return 0;
#endif
}

static void rk3066_battery_callback( struct adc_client *client, void *param, int result )
{
	struct rk3066_battery_data *battery_data = (struct rk3066_battery_data *)param;
	battery_data->adc_val = result;
	complete( &battery_data->completion );
}

static unsigned short sync_read_adc( struct rk3066_battery_data *battery_data )
{
	int err;
	init_completion( &battery_data->completion );
	adc_async_read( battery_data->client );
	err = wait_for_completion_timeout( &battery_data->completion, msecs_to_jiffies( 1000 ) );
	if ( err == 0 ){
		return 0;
	}
	return battery_data->adc_val;
}

static void bubble( unsigned short *buffer, unsigned short size )
{
	int i;
	int index;
	for ( i = 0; i < size - 1; i++ ){
		for ( index = 0; index < size - i - 1; index++ ){
			if ( buffer[index + 1] < buffer[index] ){
				unsigned short value = buffer[index];
				buffer[index] = buffer[index + 1];
				buffer[index + 1] = value;
			}
		}
	}
}

static unsigned short average( unsigned short *Buffer, unsigned short Size )
{
	int i;
	int result = 0;
	bubble( Buffer, Size );
	for ( i = 2; i < Size - 2; i++ ){
		result += Buffer[i];
	}
	return result / (Size - 4);
}

struct adc_voltage
{
	unsigned short adc;
	unsigned short vol;
};

struct battery_capacity
{
	unsigned short voltage;
	unsigned short capacity;
};

static const struct adc_voltage m_adc_voltage_table[] = {
	{588, 6000},
	{599, 6100},
	{610, 6200},
	{621, 6300},
	{632, 6400},
	{641, 6500},
	{650, 6600},
	{658, 6700},
	{668, 6800},
	{678, 6900},
	{686, 7000},
	{695, 7100},
	{704, 7200},
	{714, 7300},
	{723, 7400},
	{733, 7500},
	{741, 7600},
	{751, 7700},
	{760, 7800},
	{770, 7900},
	{780, 8000},
	{788, 8100},
	{798, 8200},
	{806, 8300},
	{816, 8400},
};
static const struct battery_capacity m_battery_table[] = {
	{6800, 1},
	{7200, 15},
	{7450, 30},
	{7700, 50},
	{7900, 75},
	{8100, 98},
	{8300, 100},
};

static int calc_voltage_ab( const struct adc_voltage *min, const struct adc_voltage *max, int *a, int *b )
{
	int add;
	int mul;
	int vol;
	int adc;
	if ( NULL == min || NULL == max ){
		return -1;
	}
	vol = max->vol - min->vol;
	adc = max->adc - min->adc;
	mul = (vol * 1000) / adc;
	add = (max->vol * 1000) - (max->adc * mul);
	if ( NULL != a ){
		*a = mul;
	}
	if ( NULL != b ){
		*b = add;
	}
	return 0;
}

static int adc_to_voltage( const struct adc_voltage *table, int size, unsigned short adc )
{
	int i;
	int a = 0;
	int b = 0;
	int err = 0;
	if ( NULL == table || size < 2 ){
		return 0;
	}
	for ( i = 0; i < size; i++ ){
		if ( adc < table[i].adc ){
			break;
		}
	}
	if ( 0 == i ){
		err = calc_voltage_ab( &table[0], &table[1], &a, &b );
	}else if ( i < size ){
		err = calc_voltage_ab( &table[i - 1], &table[i], &a, &b );
	}else{
		err = calc_voltage_ab( &table[size - 2], &table[size - 1], &a, &b );
	}
	if ( 0 != err ){
		return 0;
	}
	i = (adc * a + b);
	err = i % 1000;
	if ( err > 500 ){
		i += 1000;
	}
	return i / 1000;
}

static int calc_capacity_ab( const struct battery_capacity *min, const struct battery_capacity *max, int *a, int *b )
{
	int add;
	int mul;
	int voltage;
	int capacity;
	if ( NULL == min || NULL == max ){
		return -1;
	}
	capacity = max->capacity - min->capacity;
	voltage = max->voltage - min->voltage;
	mul = (capacity * 1000) / voltage;
	add = (max->capacity * 1000) - (max->voltage * mul);
	if ( NULL != a ){
		*a = mul;
	}
	if ( NULL != b ){
		*b = add;
	}
	return 0;	
}

static int voltage_to_capacity( const struct battery_capacity *table, int size, unsigned short voltage )
{
	int i;
	int a = 0;
	int b = 0;
	int err = 0;
	if ( NULL == table || size < 2 ){
		return 0;
	}
	for ( i = 0; i < size; i++ ){
		if ( voltage < table[i].voltage ){
			break;
		}
	}
	if ( 0 == i ){
		err = calc_capacity_ab( &table[0], &table[1], &a, &b );
	}else if ( i < size ){
		err = calc_capacity_ab( &table[i - 1], &table[i], &a, &b );
	}else{
		err = calc_capacity_ab( &table[size - 2], &table[size - 1], &a, &b );
	}
	if ( 0 != err ){
		return 0;
	}
	i = (voltage * a + b);
	err = i % 1000;
	if ( err > 500 ){
		i += 1000;
	}
	return i / 1000;
}


extern void fx5x6_set_dc_status( int status );
#include <linux/syscalls.h>
#include <linux/rtc.h>
#include <linux/miscdevice.h>

static void add_log( int adc_value )
{
	int fd = -1;
	struct timeval tv;
	struct rtc_time tm;
	char buffer[100] = {0};
	fd = sys_open( "/data/battery.txt", O_CREAT | O_RDWR, 0 );
	if(fd < 0){
		printk( "add_log: open file /data/battery.txt failed\n" );
		return;
	}
	do_gettimeofday( &tv ); 
	rtc_time_to_tm( tv.tv_sec, &tm );
	sprintf( buffer, "%02d-%02d-%02d\t%d\r\n", tm.tm_hour, tm.tm_min, tm.tm_sec, adc_value );
	sys_lseek( fd, 0, SEEK_END );
	sys_write( fd, (const char __user *)buffer, strlen(buffer) );
	sys_close( fd );
}

struct dev_battery_info
{
	int status;
	char buffer[100];
};

struct battery_dev
{
	int use;
	int flags;
	int status;
	wait_queue_head_t wait;
	struct dev_battery_info info;
};

struct battery_dev m_battery_dev;

static int battery_open( struct inode *inode, struct file *file )
{
	return 0;
}

static int battery_release( struct inode *inode, struct file *file )
{
	return 0;
}

static ssize_t battery_read( struct file *file, char __user *buf, size_t count, loff_t *offset )
{
	struct battery_dev *battery = &m_battery_dev;
	if ( count != sizeof(battery->info) ){
		return 0;
	}
	battery->use = 1;
	battery->flags = 0;
	wait_event_interruptible( battery->wait, 0 != battery->flags );
	battery->use = 0; 
	printk( "battery_ioctl wait_event_interruptible ok\n" );	
	if ( copy_to_user( buf, &battery->info, sizeof(battery->info) ) ){
		return -EFAULT;
	}
	return sizeof(battery->info);
}

static long battery_ioctl( struct file *file, unsigned int cmd, unsigned long arg )
{
	struct battery_dev *battery = &m_battery_dev;
	printk( "battery_ioctl entry %d, 0x%x\n", cmd, arg );
	if ( NULL == battery ){
		return -1;
	}
	switch( cmd )
	{
		case 0:
			if ( arg == 0 ){
				if ( battery->status ){
					battery->status = 0;
					gpio_direction_output( RK30_PIN4_PD2, GPIO_LOW );
				}
			}else{
				if ( !battery->status ){
					battery->status = 1;
					gpio_direction_output( RK30_PIN4_PD2, GPIO_HIGH );
				}
			}
			break;
		default:
			break;
	}
	return 0;
}

static void set_battery_info( int status, int adc_value )
{
	struct timeval tv;
	struct rtc_time tm;
	struct battery_dev *battery = &m_battery_dev;
	if ( 0 == battery->use ){
		return;
	}
	do_gettimeofday( &tv ); 
	rtc_time_to_tm( tv.tv_sec, &tm );
	sprintf( battery->info.buffer, "%02d-%02d-%02d\t%d\r\n", tm.tm_hour, tm.tm_min, tm.tm_sec, adc_value );
	battery->info.status = status;
	battery->flags = 1;
	wake_up_interruptible( &battery->wait );
}

static struct file_operations m_battery_fops = {
	.owner = THIS_MODULE,
	.open = battery_open,
	.read = battery_read,
	.release = battery_release,
	.unlocked_ioctl = battery_ioctl
};

static struct miscdevice m_battery_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "battery_test",
	.fops = &m_battery_fops
};

extern int notebook_is_connect( void );
static int rockchip_get_battery_status( struct rk3066_battery_data *battery_data )
{
	int i;
	struct timeval tv;
	int current_voltage;
	unsigned short adc_value;
	unsigned short adc_buffer[8] = {0};
	for ( i = 0; i < sizeof(adc_buffer) / sizeof(adc_buffer[0]); i++ ){
		adc_buffer[i] = sync_read_adc( battery_data );
		mdelay( 1 );
	}
	adc_value = average( adc_buffer, sizeof(adc_buffer) / sizeof(adc_buffer[0]) );
	//printk( "adc_value = %d\n", adc_value );
	//add_log( adc_value );
	do_gettimeofday( &tv ); 
	if ( get_ac_all_status( battery_data ) ){
		if ( gpio_get_value( battery_data->charge_ok_pin ) == battery_data->charge_ok_active_level ){
			battery_data->charge_ok_number++;
		}else{
			battery_data->charge_ok_number = 0;
		}
		//printk( "charge_ok_number = %d\n", battery_data->charge_ok_number );
		if ( gpio_get_value( battery_data->charge_ok_pin ) == battery_data->charge_ok_active_level && battery_data->charge_ok_number >= 3){
			battery_data->info.state = POWER_SUPPLY_STATUS_FULL; //充完
			//printk( "POWER_SUPPLY_STATUS_FULL\r\n" );
		}else{
			battery_data->info.state = POWER_SUPPLY_STATUS_CHARGING; //正在充电
			adc_value -= 22; //测试实际数据,恒流充电 
			if ( 0 == notebook_is_connect() ){
				adc_value -= 10;
			}
		}
		set_battery_info( 1, adc_value );
		if ( get_ac_charge_status( battery_data ) ){
			fx5x6_set_dc_status( 1 );
		}
	}else{
		battery_data->charge_ok_number = 0;
		set_battery_info( 0, adc_value );
		battery_data->info.state = POWER_SUPPLY_STATUS_DISCHARGING;
		fx5x6_set_dc_status( 0 );
	}
	if ( adc_value < AD_NO_BATT_VALE ){
		battery_data->info.online = 0;	
		battery_data->info.present = BATT_PRESENT_FALSE;	
		goto nobattery;
	}else{
		battery_data->info.online = 1;	
		battery_data->info.present = BATT_PRESENT_TRUE;	
	}
	/*get present voltage*/
	current_voltage = adc_to_voltage( m_adc_voltage_table, sizeof(m_adc_voltage_table) / sizeof(m_adc_voltage_table[0]), adc_value );
	//printk( "adc_value = %d, current_voltage = %d\n", adc_value, current_voltage );
	battery_data->info.voltage = current_voltage * 1000;
	/*calc battery capacity*/
	if ( get_ac_all_status( battery_data ) ){
		int array_size = 0;
		//printk( "cao...........\n" );
		if ( gpio_get_value( battery_data->charge_ok_pin ) == battery_data->charge_ok_active_level && battery_data->charge_ok_number >= 3 ){
			//充满
			battery_data->capacity = 100;
			battery_data->info.capacity = 100;
			battery_data->backup_capacity = 100;
			//printk( "charge ok\r\n" );
		}else{
			//没有充满
			array_size = sizeof(m_battery_table) / sizeof(m_battery_table[0]);
			battery_data->info.capacity = voltage_to_capacity( m_battery_table, array_size , current_voltage );
			//没有充满
			if ( battery_data->info.capacity > 99 ){
				battery_data->info.capacity = 99;
			}
			if ( battery_data->info.capacity < 1 ){
				battery_data->info.capacity = 1;
			}

			if ( -1 == battery_data->capacity ){
				//计算充电器所获得充电百分比，3000ma的电池, 3.5小时充满
				int step = 0;
        //printk( "battery_data->info.capacity = %d, step = %d, %d\n", battery_data->info.capacity, battery_data->backup_capacity, step );
				if ( battery_data->s_tv.tv_sec > 0 ){
					step = (tv.tv_sec - battery_data->s_tv.tv_sec) / 100;
				}
				battery_data->s_tv.tv_sec = 0;
        printk( "battery_data->info.capacity = %d, step = %d, %d\n", battery_data->info.capacity, battery_data->backup_capacity, step );
				if ( abs((step + battery_data->backup_capacity) - battery_data->info.capacity) > 5 ){
					//新采的值小于旧的值，用旧的值,防止唤醒后，电量变底
					if ( battery_data->info.capacity < battery_data->backup_capacity ){
						battery_data->backup_capacity += step;
						if ( battery_data->backup_capacity > 99 ){
							battery_data->backup_capacity = 99;
						} 
						battery_data->info.capacity = battery_data->backup_capacity;
						battery_data->capacity = battery_data->backup_capacity;
					}else{
						battery_data->backup_capacity = battery_data->info.capacity;
						battery_data->capacity = battery_data->backup_capacity;
					}
				}else{
					//如果差值小于5
					if ( battery_data->info.capacity < battery_data->backup_capacity ){
						//新采的值小于旧的值，用旧的值,防止唤醒后，电量变底
						battery_data->backup_capacity += step; 
						if ( battery_data->backup_capacity > 99 ){
							battery_data->backup_capacity = 99;
						} 
						battery_data->info.capacity = battery_data->backup_capacity;
						battery_data->capacity = battery_data->backup_capacity;
					}else{
						battery_data->backup_capacity = battery_data->info.capacity;
						battery_data->capacity = battery_data->backup_capacity;
					}
				}
				//battery_data->capacity = battery_data->info.capacity;
			}else{
				//消抖
				if ( battery_data->info.capacity < battery_data->capacity ){
					battery_data->info.capacity = battery_data->capacity;
				}else{
					if ( battery_data->info.capacity > battery_data->capacity + 1 ){
						if ( 0 == battery_data->tv.tv_sec || tv.tv_sec - battery_data->tv.tv_sec >= 60 ){
							battery_data->capacity++;
							battery_data->tv.tv_sec = tv.tv_sec;
						}
						battery_data->info.capacity = battery_data->capacity;
					}else{
						battery_data->capacity = battery_data->info.capacity;
					}
				}
			}
			//adc 失效,用时间算电池百分比
			if ( battery_data->info.capacity < 99 ){
				if ( battery_data->s_tv.tv_sec == 0 ){
					battery_data->s_tv.tv_sec = tv.tv_sec;
					battery_data->backup_capacity = battery_data->info.capacity;
				}
				if ( tv.tv_sec - battery_data->s_tv.tv_sec >= 90 ){
					battery_data->s_tv.tv_sec = tv.tv_sec;
					if ( battery_data->backup_capacity == battery_data->info.capacity ){
						battery_data->backup_capacity++;
						battery_data->capacity = battery_data->backup_capacity;
						battery_data->info.capacity = battery_data->backup_capacity;
					}
				}
			}else{
				battery_data->backup_capacity = 100;
			}
			//printk( "charge battery_data->info.capacity = %d, %d\n", battery_data->info.capacity, battery_data->capacity );
		}
	}else{
		int array_size;
		battery_data->s_tv.tv_sec = 0;
		array_size = sizeof(m_battery_table) / sizeof(m_battery_table[0]);
		battery_data->info.capacity = voltage_to_capacity( m_battery_table, array_size, current_voltage );
		//printk( "not charge battery_data->info.capacity = %d, %d\n", battery_data->info.capacity, battery_data->capacity );
		if ( battery_data->info.capacity > 100 ){
			battery_data->info.capacity = 99;
		}
		if ( battery_data->info.capacity < 0 ){
			battery_data->info.capacity = 0;
		}

		if ( -1 == battery_data->capacity ){
			printk( "battery_data->backup_capacity = %d\n", battery_data->backup_capacity );
			if ( battery_data->backup_capacity > 0 ){
				if ( battery_data->info.capacity < battery_data->backup_capacity ){
					battery_data->capacity = battery_data->info.capacity;
				}else{
					battery_data->capacity = battery_data->backup_capacity;
				}
			}else{
				battery_data->capacity = battery_data->info.capacity;
			}
		}
		if ( battery_data->info.capacity < battery_data->capacity ){
		  //printk( "<<<< battery_data->info.capacity = %d, %d\n", battery_data->info.capacity, battery_data->capacity );
			if ( abs( battery_data->info.capacity - battery_data->capacity ) > 1 ){
	    	//printk( "abs battery_data->info.capacity = %d, %d\n", battery_data->info.capacity, battery_data->capacity );
				if ( 0 == battery_data->tv.tv_sec || tv.tv_sec - battery_data->tv.tv_sec >= 30 ){
					battery_data->capacity--;
					battery_data->tv.tv_sec = tv.tv_sec;
				}
				battery_data->info.capacity = battery_data->capacity;
			}else{
				battery_data->capacity = battery_data->info.capacity;
			}
		}else{
			battery_data->info.capacity = battery_data->capacity;
		}
		//printk( "aaa--battery_data->info.capacity = %d, battery_data->capacity = %d\n", battery_data->info.capacity, battery_data->capacity );
	}
	/*calc battery capacity level*/
	if ( battery_data->info.capacity > 90 ){
		battery_data->info.capacity_level = POWER_SUPPLY_CAPACITY_LEVEL_FULL;
	}else if ( battery_data->info.capacity > 60 ){
		battery_data->info.capacity_level = POWER_SUPPLY_CAPACITY_LEVEL_HIGH;
	}else if ( battery_data->info.capacity > 30 ){
		battery_data->info.capacity_level = POWER_SUPPLY_CAPACITY_LEVEL_NORMAL;
	}else if ( battery_data->info.capacity > 15 ){
		battery_data->info.capacity_level = POWER_SUPPLY_CAPACITY_LEVEL_LOW;
	}else{
		battery_data->info.capacity_level = POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL;
	}
	/*get battery health status*/
	return 0;
nobattery:
	//if ( !get_msc_connect_flag() || get_ac_charge_status( battery_data ) ){
	if ( get_ac_all_status( battery_data ) ){
		battery_data->info.state = POWER_SUPPLY_STATUS_CHARGING ;
	}else{
		battery_data->info.state = POWER_SUPPLY_STATUS_DISCHARGING;
	}
	return 1;
}

static void init_battery_info( struct battery_info *info )
{
	if ( NULL == info ){
		return;
	}
	info->online = 0;
	info->voltage = 0;
	info->present = BATT_PRESENT_TRUE;
	info->state = POWER_SUPPLY_STATUS_UNKNOWN;
	info->health = POWER_SUPPLY_HEALTH_GOOD;
	info->capacity = 0;
	info->capacity_level = POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL;
}

static int rockchip_battery_get_property( struct power_supply *psy, enum power_supply_property psp, union power_supply_propval *val )
{
	struct battery_info *info = NULL;
	if ( NULL == m_battery_data ){
		return -EINVAL;
	}
	info = &m_battery_data->info;
	switch( psp )
	{
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = info->online;
		break;
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = info->state;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = info->health;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		/* get power supply */
		val->intval = info->present;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		/* Todo return battery level */	
		if ( info->capacity > 100 ){
			val->intval = 99;
		}else if( info->capacity < 0 ){
			val->intval = 0;
		}else{
			val->intval = info->capacity;
		}
		break;
	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		val->intval = info->capacity_level;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = info->voltage;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = BATT_VOLTAGE_MAX;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MIN:
		val->intval = BATT_VOLTAGE_MIN;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int rockchip_ac_get_property( struct power_supply *psy, enum power_supply_property psp, union power_supply_propval *val )
{
	switch ( psp )
	{
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = get_ac_all_status( m_battery_data );
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static void rk3066_battery_work( struct work_struct *work )
{
	//struct rk3066_battery_data *battery_data = container_of( work, struct rk3066_battery_data, work );
	struct rk3066_battery_data *battery_data = container_of( to_delayed_work(work), struct rk3066_battery_data, work );

	if ( rockchip_get_battery_status( battery_data ) ){
		return;
	}
	/*if have usb supply power*/
	if ( battery_data->changed ){
		power_supply_changed( &battery_data->ac );
		power_supply_changed( &battery_data->battery );
		//printk( "rk3066_battery level = %d\n", battery_data->info.capacity );
	}else{
		printk( "rk3066_battery wakeup delay.........\n" );
	}
	queue_delayed_work( battery_data->wq, &battery_data->work, msecs_to_jiffies(2000) );
}


static void rk3066_battery_wake( struct work_struct *work )
{
	struct rk3066_battery_data *battery_data = container_of( to_delayed_work(work), struct rk3066_battery_data, wake );
	battery_data->changed = 1;
}

static irqreturn_t battery_low_isr(int irq, void *dev_id)
{
	struct rk3066_battery_data *data = (struct rk3066_battery_data *)dev_id;
	disable_irq_nosync( data->batt_low_irq );
	queue_work( data->wq, &data->lowerpower_work );
	return IRQ_HANDLED;
}

static irqreturn_t battery_dc_wakeup(int irq, void *dev_id)
{   
	struct rk3066_battery_data *data = (struct rk3066_battery_data *)dev_id;
	queue_work( data->wq, &data->dcwakeup_work );
	return IRQ_HANDLED;
}

static void battery_lowerpower_delaywork( struct work_struct *work )
{
	struct rk3066_battery_data *data = container_of( work, struct rk3066_battery_data, lowerpower_work );
	printk( "lowerpower\n" );
	//rk28_send_wakeup_key( ); // wake up the system	
	return;
}

static void battery_dcdet_delaywork( struct work_struct *work )
{
	int ret;
	struct rk3066_battery_data *data = container_of( work, struct rk3066_battery_data, dcwakeup_work );
	rk28_send_wakeup_key(); // wake up the system
}

static const enum power_supply_property m_rockchip_battery_properties[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MIN,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
};

static const enum power_supply_property m_rockchip_power_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static const char *m_supply_list[] = {
	"battery",
};

static const struct power_supply rockchip_power_supplies[] = {
	{
		.name = "battery",
		.type = POWER_SUPPLY_TYPE_BATTERY,
		.properties = (enum power_supply_property *)m_rockchip_battery_properties,
		.num_properties = ARRAY_SIZE(m_rockchip_battery_properties),
		.get_property = rockchip_battery_get_property,
	},
	{
		.name = "ac",
		.type = POWER_SUPPLY_TYPE_MAINS,
		.supplied_to = (char **)m_supply_list,
		.num_supplicants = ARRAY_SIZE(m_supply_list),
		.properties = (enum power_supply_property *)m_rockchip_power_properties,
		.num_properties = ARRAY_SIZE(m_rockchip_power_properties),
		.get_property = rockchip_ac_get_property,
	},
};

void rk3066_battery_power( int state )
{
	if ( m_battery_data != NULL ){
		m_battery_data->changed = state;
	}
	printk( "rk3066_battery_power = %d\n", state );
}

#ifdef CONFIG_NOTEBOOK
int rk3066_get_battery_low( void )
{
	if ( NULL != m_battery_data ){
		if ( 0 == gpio_get_value( m_battery_data->batt_low_pin ) ){
			return 1;
		}
	}
	return 0;
}
#endif

extern int rk3066_notebook_interface_init( void );

static int rk3066_battery_probe( struct platform_device *pdev )
{
	int i;
	int irq;
	int ret;
	int rc = -1;
	struct adc_client *client;
	struct rk3066_battery_data *data = NULL;
	struct rk3066_battery_platform_data *pdata = pdev->dev.platform_data;
	if ( pdata && pdata->io_init ){
		ret = pdata->io_init();
		if ( 0 != ret ){
			if ( pdata->io_deinit ){
				pdata->io_deinit( );
			}
			printk( "rk3066_battery_probe  error\n" );
			return 0;
		}
	}
	printk( "rk3066_battery_probe entry\n" );
	//dc det
	ret = gpio_request( pdata->dc_det_pin, NULL );
	if ( ret ){
		printk( "failed to request dc_det gpio\n" );
		goto err_free_gpio_detpin;
	}
	gpio_pull_updown( pdata->dc_det_pin, GPIOPullUp );//important
	ret = gpio_direction_input( pdata->dc_det_pin );
	if ( ret ){
		printk("failed to set gpio dc_det input\n");
		goto err_free_gpio_detpin;
	}
	//charge_ok
	ret = gpio_request( pdata->charge_ok_pin, NULL );
	if ( ret ){
		printk( "failed to request charge_ok gpio\n" );
		goto err_free_gpio_chargepin;
	}
	gpio_pull_updown( pdata->charge_ok_pin, GPIOPullUp );//important
	ret = gpio_direction_input( pdata->charge_ok_pin );
	if ( ret ){
		printk( "failed to set gpio charge_ok input\n" );
		goto err_free_gpio_chargepin;
	}
	//batt_low
	ret = gpio_request( pdata->batt_low_pin, NULL );
	if ( ret ){
		printk( "failed to request batt_low_pin gpio\n" );
		goto err_free_gpio_battlowpin;
	}
	gpio_pull_updown( pdata->batt_low_pin, GPIOPullUp );//important
	ret = gpio_direction_input( pdata->batt_low_pin );
	if ( ret ){
		printk( "failed to set gpio batt_low_pin input\n" );
		goto err_free_gpio_battlowpin;
	}
#ifdef CONFIG_NOTEBOOK
	ret = gpio_request( pdata->notebook_dc_dec_pin, NULL );
	if ( ret ){
		printk( "failed to request notebook_dc_dec_pin gpio\n" );
		goto err_free_gpio_chargepin;
	}
	gpio_pull_updown( pdata->notebook_dc_dec_pin, GPIOPullUp );//important
	ret = gpio_direction_input( pdata->notebook_dc_dec_pin );
	if ( ret ){
		printk( "failed to set gpio notebook_dc_dec_pin input\n" );
		goto err_free_gpio_chargepin;
	}
#endif;

	data = (struct rk3066_battery_data *)kzalloc(sizeof(*data), GFP_KERNEL);
	if ( !data ){
		printk( "battery struct malloc failed\n" );
		goto err_free_gpio_battlowpin;
	}
	INIT_WORK( &data->lowerpower_work, battery_lowerpower_delaywork );
	irq = gpio_to_irq(pdata->batt_low_pin);
	/* init power supplier framework */

	m_battery_data = data;
	data->batt_low_irq = irq;

	data->wq = create_singlethread_workqueue( "rk3066_battery_wq" );
	INIT_DELAYED_WORK( &data->work, rk3066_battery_work );
	INIT_DELAYED_WORK( &data->wake, rk3066_battery_wake );
	//INIT_WORK( &data->work, rk3066_battery_work );
	spin_lock_init( &data->lock );
	data ->battery = rockchip_power_supplies[0];
	data ->ac = rockchip_power_supplies[1];
	data->dc_det_pin = pdata->dc_det_pin;
	data->batt_low_pin = pdata->batt_low_pin;
	data->dc_det_active_level = pdata->dc_det_level;
	data->charge_ok_pin = pdata->charge_ok_pin;
	data->charge_ok_active_level = pdata->charge_ok_level;
#ifdef CONFIG_NOTEBOOK
	data->notebook_dc_dec_pin = pdata->notebook_dc_dec_pin;
#endif
	client = adc_register( 0, rk3066_battery_callback, data );
	if ( !client ){
		printk("adc register failed");
		goto adc_register_failed;
	}
	data->client = client;
	data->adc_val = 0;
	platform_set_drvdata( pdev, data );
	rc = power_supply_register( &pdev->dev, &data->battery );
	if ( rc ){
		printk( KERN_ERR "Failed to register battery power supply (%d)\n", rc );
		goto err_battery_fail;
	}
	rc = power_supply_register( &pdev->dev, &data->ac );
	if ( rc ){
		printk( KERN_ERR "Failed to register ac power supply (%d)\n", rc );
		goto err_ac_fail;
	}
#if 0
	for ( i = 0; i < AD_SAMPLE_TIMES; i++ ){
		rockchip_get_battery_status( data );
		mdelay( 100 );
	}
#endif
	//低电不开机
#ifdef CONFIG_NOTEBOOK
	if ( 0 == notebook_is_connect() ){
#endif
		//如果没有接底坐
		if (  0 == gpio_get_value( pdata->batt_low_pin ) && !get_ac_charge_status( data ) ){
			printk( "rk3066_battery_probe battery low power off..\n" );
			pm_power_off( );
			mdelay( 1000 );
			return 0;
		}
#ifdef CONFIG_NOTEBOOK
	}
#endif
	init_battery_info( &data->info );
	data->changed = 1;
	data->capacity = -1;
	printk( "rk3066_battery start misc_register\n" );
	mdelay( 100 );
	m_battery_dev.use = 0;
	m_battery_dev.status = get_ac_charge_status( data );
	init_waitqueue_head( &m_battery_dev.wait );
	queue_delayed_work( data->wq, &data->work, msecs_to_jiffies(18000) );
	if ( misc_register( &m_battery_misc ) ){
		printk( "rk3066_battery misc_register err\n" );
	}
	if ( pdata->dc_det_pin != INVALID_GPIO ){
		int irq_flag;
		INIT_WORK( &data->dcwakeup_work, battery_dcdet_delaywork );
		irq = gpio_to_irq( pdata->dc_det_pin );
		irq_flag = gpio_get_value( pdata->dc_det_pin ) ? IRQF_TRIGGER_FALLING : IRQF_TRIGGER_RISING;
		ret = request_irq( irq, battery_dc_wakeup, irq_flag, "ac_charge_irq", data );
		if ( ret ){
			printk("failed to request dc det irq\n");
			goto err_battery_fail;
		}
		enable_irq_wake( irq );
	}
	data->dc_charge_irq = irq;
	data->charge_ok_number = 0;
	data->tv.tv_sec = 0;
	data->s_tv.tv_sec = 0;
	ret = request_irq( data->batt_low_irq, battery_low_isr, IRQF_TRIGGER_LOW, "battery_low", data );
	if ( ret ){
		gpio_free( pdata->batt_low_pin );
		goto err_free_gpio_battlowpin;
	}
	if (  1 == gpio_get_value( pdata->batt_low_pin ) ){
		disable_irq( data->batt_low_irq );
	}
	printk( "rk3066_battery_probe ok..\n" );
#ifdef CONFIG_NOTEBOOK
	rk3066_notebook_interface_init( );
#endif
	return 0;
err_battery_fail:
	power_supply_unregister( &data->battery );
err_ac_fail:
	power_supply_unregister( &data->ac );
adc_register_failed:
	kfree( data );
err_free_gpio_battlowpin:
	gpio_free( pdata->batt_low_pin );
err_free_gpio_chargepin:
	gpio_free( pdata->charge_ok_pin );
err_free_gpio_detpin:
	gpio_free( pdata->dc_det_pin );
	return rc;	
}

static int rk3066_battery_remove( struct platform_device *pdev )
{
	struct rk3066_battery_data *data = platform_get_drvdata( pdev );
	struct rk3066_battery_platform_data *pdata = pdev->dev.platform_data;
	power_supply_unregister( &data->battery );
	power_supply_unregister( &data->ac );
	gpio_free( pdata->charge_ok_pin );
	gpio_free( pdata->dc_det_pin );
	gpio_free( pdata->batt_low_pin );
	kfree( data );
	return 0;
}


#ifdef CONFIG_PM
static int battery_suspend( struct device *dev )
{
	int irq_flag;
	struct platform_device *pdev = to_platform_device( dev );
	struct rk3066_battery_data *data = platform_get_drvdata( pdev );
	struct rk3066_battery_platform_data *pdata = pdev->dev.platform_data;
	data->changed = 0;
	cancel_delayed_work_sync( &data->work );
	if ( pdata->batt_low_pin != INVALID_GPIO ){
		int irq = gpio_to_irq( pdata->batt_low_pin );
		enable_irq( irq );
		enable_irq_wake( irq );
	}
	irq_flag = gpio_get_value( data->dc_det_pin ) ? IRQF_TRIGGER_FALLING : IRQF_TRIGGER_RISING;
	irq_set_irq_type( data->dc_charge_irq, irq_flag );
	data->backup_capacity = data->capacity;
	//msleep( 1500 );
	return 0;
}

static int battery_resume( struct device *dev )
{
	struct platform_device *pdev = to_platform_device( dev );
	struct rk3066_battery_data *data = platform_get_drvdata( pdev );
	struct rk3066_battery_platform_data *pdata = pdev->dev.platform_data;
	data->capacity = -1;
	if ( pdata->batt_low_pin != INVALID_GPIO ){
		int irq = gpio_to_irq( pdata->batt_low_pin );
		disable_irq_wake( irq );
		disable_irq(irq);
	}
	queue_delayed_work( data->wq, &data->wake, msecs_to_jiffies(2000) );
	queue_delayed_work( data->wq, &data->work, msecs_to_jiffies(3000) );
	return 0;
}

static const struct dev_pm_ops battery_pm_ops = {
	.suspend	= battery_suspend,
	.resume	= battery_resume,
};
#endif


static struct platform_driver rockchip_battery_driver = {
	.probe	= rk3066_battery_probe,
	.remove	= rk3066_battery_remove,
	.driver	= {
		.name	= "rk3066-battery",
		.owner	= THIS_MODULE,
#ifdef CONFIG_PM
		.pm= &battery_pm_ops,
#endif
	},
};

static int __init rk3066_battery_init( void )
{
	return platform_driver_register(&rockchip_battery_driver);
}

static void __exit rk3066_battery_exit( void )
{
	platform_driver_register( &rockchip_battery_driver );
}

late_initcall( rk3066_battery_init );
module_exit( rk3066_battery_exit );
MODULE_DESCRIPTION("Rockchip Battery Driver");
MODULE_LICENSE("GPL");
