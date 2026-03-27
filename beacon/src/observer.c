#include <zephyr/sys/printk.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <bluetooth/scan.h>
#include <zephyr/bluetooth/uuid.h>

uint16_t no_packets=0;	

bool scan_stopped = false;

static void scan_filter_match(struct bt_scan_device_info *device_info,
			      struct bt_scan_filter_match *filter_match,
			      bool connectable)
{
	++no_packets;

	//printk("MATCH: No of FILTERED packets %u\n", no_packets);
	
}

struct bt_le_scan_param scan_param;

BT_SCAN_CB_INIT(scan_cb, scan_filter_match, NULL, NULL, NULL);


static int scan_init()
{	

	struct bt_scan_init_param scan_init = {
		.scan_param = &scan_param,
	};

	bt_scan_init(&scan_init); // Stores the scanning parameters we set at scan_param struct and the default conn params in bt_scan struct

	printk("Scan window %u, Scan interval %u\n", scan_param.window, scan_param.interval);

	bt_scan_cb_register(&scan_cb);

	int err;


	static struct bt_uuid_16 my_uuid = BT_UUID_INIT_16(0xBEAA);

	err = bt_scan_filter_add(BT_SCAN_FILTER_TYPE_UUID, &my_uuid);
	if (err) {
		printk("Scanning filters cannot be set (err %d)", err);
		return err;
	}

	err = bt_scan_filter_enable(BT_SCAN_UUID_FILTER, false);
	if (err) {
		printk("Filters cannot be turned on (err %d)", err);
		return err;
	}

	printk("Scan module initialized\n");
	return err;
}

int observer_start(uint16_t interval, uint16_t window)
{
	scan_param.options	= BT_LE_SCAN_OPT_NONE;
	scan_param.interval	= interval;
	scan_param.window	= window;

	int err;

	err = scan_init();

	if (err) {
		printk("scan_init failed (err %d)\n", err);
		return err;
	}

	err = bt_scan_start(BT_SCAN_TYPE_SCAN_PASSIVE);
	if (err) {
		printk("Start scanning failed (err %d)\n", err);
		return err;
	}

	printk("Scanning successfully started\n");

	return err;
}

static void scan_reset_work_handler(struct k_work *work)
{
	int err;

	printk("Scan window %u, Scan interval %u\n", scan_param.window, scan_param.interval);

	if (!scan_stopped)
	{
		err = bt_scan_params_set(&scan_param);

		if (err) {
			printk("Failed to set new scan parameters (err %d)\n", err);
		}

		printk("Reset parameters: SUCCESS\n");
	}

	err = bt_scan_start(BT_SCAN_TYPE_SCAN_PASSIVE);

	if (err) {
		printk("Scanning failed to start (err %d)\n", err);
	}

	if (scan_stopped) scan_stopped = false;

	printk("Exiting observer_reset handler..\n");

	return;
}

K_WORK_DEFINE(scan_reset_work, scan_reset_work_handler);

void scan_reset(uint16_t interval, uint16_t window)
{

	no_packets = 0;
	scan_param.interval	= interval;
	scan_param.window	= window;

    k_work_submit(&scan_reset_work);

	printk("Exiting observer_reset..\n");

	return;
}

static void observer_stop_handler(struct k_work *work)
{
	int err;

	scan_stopped = true;

	err = bt_scan_stop();
	if (err) {
		printk("Stop scanning failed (err %d)\n", err);
	}

	printk("Exiting observer_stop handler..\n");

	return;
}

K_WORK_DEFINE(observer_stop_work, observer_stop_handler);

void observer_stop()
{
	k_work_submit(&observer_stop_work);

	printk("Exiting observer_stop..\n");

	return;
}