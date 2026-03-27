#include <zephyr/types.h>
#include <stddef.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/kernel.h>  // For k_timer 
#include <zephyr/drivers/counter.h>
#include <zephyr/device.h>
#include <bluetooth/scan.h>

#include <zephyr/bluetooth/hci.h>// /* main.c - Application main entry point */
#include <zephyr/bluetooth/hci_vs.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/bluetooth/uuid.h> //extra

#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

#define ALARM_CHANNEL_ID 0
#define TIMER DT_NODELABEL(rtc2)

#define MAX_SLEEP_DUR 12000
#define MIN_SLEEP_DUR 200

/* adv model parameters */
extern int current_min_interval;
extern int current_max_interval;
extern int Tx_id;
extern bool tx_on;

/* Observer model parameters*/
uint16_t scan_interval = (uint16_t) 960; // 600ms
uint16_t scan_window = (uint16_t) 80; // 50ms

extern int no_packets;

uint64_t counter_timeout = 2000000; // 2s
int sleep_dur = 1000; // 1s
int previous_norm=-1;

// advertise functions //
void adv_start();
void adv_reset();
void adv_stop();

// Observer function //
int  observer_start(uint16_t interval, uint16_t window);
void observer_stop();
void scan_reset(uint16_t interval, uint16_t window); 

const struct device *const counter_dev = DEVICE_DT_GET(TIMER);
struct counter_alarm_cfg alarm_cfg;

static void counter_interrupt_fn(const struct device *counter_dev, uint8_t chan_id, uint32_t ticks, void *user_data);

static void sleep_work_handler(struct k_work *work)
{
    k_sleep(K_MSEC(sleep_dur));
    k_sleep(K_MSEC(sleep_dur));
    k_sleep(K_MSEC(sleep_dur));
    k_sleep(K_MSEC(sleep_dur));
    k_sleep(K_MSEC(sleep_dur));
    k_sleep(K_MSEC(sleep_dur));
    k_sleep(K_MSEC(sleep_dur));
    k_sleep(K_MSEC(sleep_dur));
    k_sleep(K_MSEC(sleep_dur));
    k_sleep(K_MSEC(sleep_dur));

    alarm_cfg.flags = 0;
    alarm_cfg.ticks = counter_us_to_ticks(counter_dev, counter_timeout);
    alarm_cfg.callback = counter_interrupt_fn;
    alarm_cfg.user_data = &alarm_cfg;

    int err = counter_set_channel_alarm(counter_dev, ALARM_CHANNEL_ID, &alarm_cfg);
    if (err != 0) {
        printk("Alarm could not be set\n");
    }
}

K_WORK_DEFINE(sleep_work, sleep_work_handler);

static void counter_interrupt_fn(const struct device *counter_dev,
				      uint8_t chan_id, uint32_t ticks,
				      void *user_data)
{

    	struct counter_alarm_cfg *config = user_data;

		printk("Counter interrupt\n");

        int packet_time = (int)((counter_timeout / (scan_interval + scan_window)) * scan_window); 
        int norm_packets = (int)(no_packets * 1000000 / packet_time);  

        printk("No of packets %d Packet time %d Norm packets %d\n", no_packets, packet_time, norm_packets);

        if (previous_norm == -1) {
            previous_norm = norm_packets;
        }
       
        if (norm_packets == 0)
        {
            if (previous_norm == 0) 
            {
                sleep_dur += 200;
                if (sleep_dur > MAX_SLEEP_DUR) sleep_dur = MAX_SLEEP_DUR;
            }
            observer_stop(); // Stop scanning
            adv_stop(); // Stop advertising
            k_work_submit(&sleep_work);
            scan_reset(scan_interval, scan_window);
        }
        else
        {  
            if(norm_packets > previous_norm)
            {   
                sleep_dur -= 100;
                if (sleep_dur < MIN_SLEEP_DUR) sleep_dur = MIN_SLEEP_DUR;
                if (previous_norm == 0) adv_reset();
            }
            else if(norm_packets < previous_norm)
            {
                sleep_dur += 100;
                if (sleep_dur > MAX_SLEEP_DUR) sleep_dur = MAX_SLEEP_DUR;
            }

            alarm_cfg.flags = 0;
            alarm_cfg.ticks = counter_us_to_ticks(counter_dev, counter_timeout);
            alarm_cfg.callback = counter_interrupt_fn;
            alarm_cfg.user_data = &alarm_cfg;

            int err = counter_set_channel_alarm(counter_dev, ALARM_CHANNEL_ID, &alarm_cfg);
            if (err != 0) {
                printk("Alarm could not be set\n");
            }

        }

        previous_norm = norm_packets;
        no_packets = 0;
} 

/* Bluetooth ready callback */

static void bt_ready(int err)
{
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return;
    }

    printk("Bluetooth initialized\n");

    (void)observer_start(scan_interval, scan_window);

    adv_start();
}

int main(void)
{
  //  const struct device *const counter_dev = DEVICE_DT_GET(TIMER);
    int err;

    printk("Starting Beacon DION Demo\n");

    if (!device_is_ready(counter_dev)) {
		printk("device not ready.\n");
		return 0;
	}

    counter_start(counter_dev);

	alarm_cfg.flags = 0;
	alarm_cfg.ticks = counter_us_to_ticks(counter_dev, counter_timeout);
	alarm_cfg.callback = counter_interrupt_fn;
	alarm_cfg.user_data = &alarm_cfg;

    err = counter_set_channel_alarm(counter_dev, ALARM_CHANNEL_ID, &alarm_cfg); 

	if (-EINVAL == err) {
		printk("Alarm settings invalid\n");
	} else if (-ENOTSUP == err) {
		printk("Alarm setting request not supported\n");
	} else if (err != 0) {
		printk("Error\n");
	}

    /* Initialize the Bluetooth Subsystem */
    err = bt_enable(bt_ready);
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
    }  

    return 0;
}