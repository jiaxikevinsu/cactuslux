#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/sensor/veml7700.h> //required for enums
#include <zephyr/drivers/i2c.h>
#include <zephyr/device.h>
#include <zephyr/storage/disk_access.h>
#include <zephyr/logging/log.h>
#include <zephyr/fs/fs.h>
#include <ff.h>
#include <math.h>
#include <zephyr/drivers/rtc/maxim_ds3231.h>
#include <zephyr/drivers/counter.h>
#include <time.h>

// Defines for Lux sensor
#define I2C0_NODE DT_NODELABEL(veml7700)

// Defines for SD Card
#define MAX_PATH 128
#define SOME_FILE_NAME "data.txt"
#define SOME_DIR_NAME "data"
#define SOME_REQUIRED_LEN MAX(sizeof(SOME_FILE_NAME), sizeof(SOME_DIR_NAME))

// Setup for SD Card with fatfs library
static const char *disk_mount_pt = "/SD:";
LOG_MODULE_REGISTER(main);
static int lsdir(const char *path);

static FATFS fat_fs;
/* mounting info */
static struct fs_mount_t mp = {
	.type = FS_FATFS,
	.fs_data = &fat_fs,
};

typedef struct {
	int32_t lux;
	float temp;
	float rh;
	char datetime[128];
} SensorData;

/* Function to create data directory and file if it doesn't exist */
static bool create_directory(const char *base_path) {
	char path[MAX_PATH];
	struct fs_file_t file;
	int base = strlen(base_path);

	fs_file_t_init(&file);

	if (base >= (sizeof(path) - SOME_REQUIRED_LEN)) {
		LOG_ERR("Not enough concatenation buffer to create file paths");
		return false;
	}

	strncpy(path, base_path, sizeof(path)); //creates the base path

	path[base++] = '/';
	path[base] = 0; //null terminates after the "/"
	strcat(&path[base], SOME_DIR_NAME); //Adds the directory name
	//LOG_INF("Base path before directory created: %s\n", path);

	if (fs_mkdir(path) == -EEXIST){
		LOG_ERR("Failed to create dir because already exists, %s", path);
	}
	else {
		LOG_ERR("Failed to create dir, %s", path);
	}

	//Update the path to where the file is created
	base += strlen(SOME_DIR_NAME);
	path[base++] = '/';
	path[base] = 0;
	strcat(&path[base], SOME_FILE_NAME);
	//LOG_INF("Base path after directory created: %s\n", path);

	if (fs_open(&file, path, FS_O_CREATE) != 0) {
		LOG_ERR("Failed to create file %s", path);
		return false;
	}
	fs_close(&file);

	return true;
}

/* Format times as: YYYY-MM-DD HH:MM:SS DOW DOY */
static char *format_time(struct tm *time){ 
	static char buf[64]; //buffer to store formatted time
	char *bp = buf; //pointer to the current write position
	char *const bpe = bp + sizeof(buf); //pointer to the end of the buffer
	//struct tm *tp = time;
	bp += strftime(bp, bpe - bp, "%Y-%m-%d %H:%M:%S", time); //format date and time as a string, adjusts the write location of bp by N*sizeof(char)
	bp += strftime(bp, bpe - bp, " %a", time);
	return buf;
}

struct tm get_rtc_time(const struct device *i2c_dev){
	// Directly reads and interprets the date time registers of the DS3231, outputs a tm struct
	uint8_t buf_sec = 0, buf_min = 0, buf_hour = 0, buf_day = 0, buf_date = 0, buf_month = 0, buf_year = 0;
	struct tm current_time = {0};
	int ret = i2c_reg_read_byte(i2c_dev, 0x68, 0x00, &buf_sec);
	ret = i2c_reg_read_byte(i2c_dev, 0x68, 0x01, &buf_min);
	ret = i2c_reg_read_byte(i2c_dev, 0x68, 0x02, &buf_hour);
	ret = i2c_reg_read_byte(i2c_dev, 0x68, 0x03, &buf_day);
	ret = i2c_reg_read_byte(i2c_dev, 0x68, 0x04, &buf_date);
	ret = i2c_reg_read_byte(i2c_dev, 0x68, 0x05, &buf_month);
	ret = i2c_reg_read_byte(i2c_dev, 0x68, 0x06, &buf_year);
	// if (ret != 0){
	// 	printk("There was an error reading from the register");
	// }
	current_time.tm_sec = (((buf_sec & 112) >> 4) * 10) + (buf_sec & 15);//((buf_sec / 10) << 4) | (buf_sec % 10);
	current_time.tm_min = (((buf_min & 112) >> 4) * 10) + (buf_min & 15);

	int hour_calc = (((buf_hour & 16) >> 4) * 10) + (buf_hour & 15);
	if (((buf_hour & 64) >> 6) == 1){ //this means 12 hour format
		if (((buf_hour & 32) >> 5) == 1){ //this means PM
			//handle edge cases:
			if (hour_calc == 12){
				current_time.tm_hour = 12; //convert to 24 hour time
			}
		} else { //this means AM time
			if (hour_calc == 12){
				current_time.tm_hour = 0; //convert to 24 hour time
			}
		}
	} else {
		current_time.tm_hour = hour_calc + (((buf_hour & 32) >> 5) * 20);  //24 hour time
	}
	current_time.tm_wday = buf_day;
	current_time.tm_mday = (((buf_date & 48) >> 4) * 10) + (buf_date & 15);
	current_time.tm_mon = (((buf_month & 16) >> 4) * 10) + (buf_month & 15); //ignore bit 7
	current_time.tm_year = (((buf_year & 240) >> 4) * 10) + (buf_year & 15);
	//printk("The following buffers contains: sec: %hhu, min: %hhu, hour: %hhu, wday: %hhu, mday: %hhu, mon: %hhu, year: %hhu\n", buf_sec, buf_min, buf_hour, buf_day, buf_date, buf_month, buf_year);
	//printk("The time formatted is: sec: %d, min: %d, hour: %d, wday: %d, mday: %d, mon: %d, year: %d\n", current_time.tm_sec, current_time.tm_min, current_time.tm_hour, current_time.tm_wday, current_time.tm_mday, current_time.tm_mon, current_time.tm_year);
	return current_time;
}

/* Function to write data to directory */
static bool write_data_to_sd(const char *base_path, SensorData *data) {
	struct fs_file_t file;
	fs_file_t_init(&file);
	int buffer_size = 256;
	char logging_data[buffer_size];
	
	//Generate the path for writing the data based on macros for filenames
	char path[MAX_PATH];
	int base = strlen(base_path);
	strncpy(path, base_path, sizeof(path)); //creates the base path
	path[base++] = '/';
	path[base] = 0; //null terminates after the "/"
	strcat(&path[base], SOME_DIR_NAME); //Adds the directory name
	base += strlen(SOME_DIR_NAME);
	path[base++] = '/';
	path[base] = 0;
	strcat(&path[base], SOME_FILE_NAME);

	//printk("The lux inside the write function is: %"PRId32"\n", data->lux);
	snprintf(logging_data, buffer_size, "[%s], %"PRId32" lux, %f F, %f%%\n", data->datetime, data->lux, data->temp, data->rh); //copy the sensor data into the buffer

	//printk("The size of sizeof(logging_data) is: %zu, where the size of strlen() is: %zu\n", sizeof(logging_data), strlen(logging_data));

	// Open up the SD Card
	if (fs_open(&file, path, FS_O_WRITE | FS_O_APPEND) != 0) {
		LOG_ERR("Failed to create file within write function %s", path);
		return false;
	}
	else {
		fs_write(&file, logging_data, strlen(logging_data));
		LOG_INF("Successfully opened up file within write function in path: %s", path);
		fs_close(&file);
	}
	return true;
}

// void update_sensor_data(SensorData *sensor_data){

// }

int main(void)
{
	SensorData sensor_data; //struct for storing data
	SensorData *sd_ptr = &sensor_data;
	bool start_flag = false;
	//double corrected_lux;
	//coefficients for correction function
	// double coeff_a = pow(6.0135, -13);
	// double coeff_b = pow(9.3924, -9);
	// double coeff_c = pow(8.1488, -5);
	// double coeff_d = 1.0023;
	//printk("The coefficients for the correction formula are:\ta: %.15lf, b: %.15lf, c: %.15lf, d: %.15lf\n", coeff_a, coeff_b, coeff_c, coeff_d);

	//retrieve the API-specific device structures
	const struct device *i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c0));
	const struct device *veml_dev = DEVICE_DT_GET(DT_INST(0,vishay_veml7700));
	const struct device *sht45_dev = DEVICE_DT_GET(DT_INST(0,sensirion_sht4x));
	const struct device *ds3231_dev = DEVICE_DT_GET(DT_INST(0,maxim_ds3231));

	// if (sht45_dev == NULL) {
	// 	/* No such node, or the node does not have status "okay". */
	// 	LOG_INF("\nError: no sht45 found.\n");
	// }
	// if (!device_is_ready(veml_dev)) {
	// 	LOG_INF("\nError: Device \"%s\" is not ready; "
	// 			"check the driver initialization logs for errors.\n",
	// 			veml_dev->name);
	// }
	// LOG_INF("Found device \"%s\", getting sensor data\n", sht45_dev->name);

	/* raw disk i/o */
	do {
		static const char *disk_pdrv = "SD";
		uint64_t memory_size_mb;
		uint32_t block_count;
		uint32_t block_size;

		if (disk_access_init(disk_pdrv) != 0) {
			LOG_ERR("Storage init ERROR!");
			break;
		}

		if (disk_access_ioctl(disk_pdrv,
				DISK_IOCTL_GET_SECTOR_COUNT, &block_count)) {
			LOG_ERR("Unable to get sector count");
			break;
		}
		LOG_INF("Block count %u", block_count);

		if (disk_access_ioctl(disk_pdrv,
				DISK_IOCTL_GET_SECTOR_SIZE, &block_size)) {
			LOG_ERR("Unable to get sector size");
			break;
		}
		LOG_INF("Sector size %u", block_size);

		memory_size_mb = (uint64_t)block_count * block_size;
		LOG_INF("Memory Size(MB) %u", (uint32_t)(memory_size_mb >> 20));
	} while (0);

	//define the mounting point
	mp.mnt_point = disk_mount_pt;

	// define sensor_value struct
	static struct sensor_value lux;
	static struct sensor_value gain;
	static struct sensor_value it;
	// sensor readings for sht45
	static struct sensor_value temp;
	static struct sensor_value rh;

	//configure the sensor registers
	gain.val1 = VEML7700_ALS_GAIN_1_8; //gain of 1/8
	it.val1 = VEML7700_ALS_IT_25; //25 ms integration time
	sensor_attr_set(veml_dev, SENSOR_CHAN_LIGHT, SENSOR_ATTR_VEML7700_GAIN, &gain);
	LOG_INF("The gain attribute has been set to %d.", gain.val1);
	sensor_attr_set(veml_dev, SENSOR_CHAN_LIGHT, SENSOR_ATTR_VEML7700_ITIME, &it);
	LOG_INF("The integration time has been set to %d", it.val1);
	//sensor_attr_get(dev, SENSOR_CHAN_LIGHT, SENSOR_ATTR_VEML7700_ITIME, &it2);
	//printk("The integration time returned is %d\n", it2.val1);

	// while loop for data logging
	while (1){
		int res = fs_mount(&mp);
		if (res == FR_OK) {
			LOG_INF("Disk mounted.");
			if (start_flag == false) { //do this once only
				if (lsdir(disk_mount_pt) < 0) {
					LOG_ERR("Error opening directory using lsdir()");
				}
				if (create_directory(disk_mount_pt)) {
					LOG_INF("create_directory() returned true");
				}
				start_flag = true;
			}
		}
		else {
			LOG_ERR("Error mounting disk.");
		}

		sensor_sample_fetch_chan(veml_dev, SENSOR_CHAN_LIGHT);
        sensor_channel_get(veml_dev, SENSOR_CHAN_LIGHT, &lux);
		sensor_sample_fetch_chan(sht45_dev, SENSOR_CHAN_AMBIENT_TEMP);
		sensor_channel_get(sht45_dev, SENSOR_CHAN_AMBIENT_TEMP, &temp);
		sensor_sample_fetch_chan(sht45_dev, SENSOR_CHAN_HUMIDITY);
		sensor_channel_get(sht45_dev, SENSOR_CHAN_HUMIDITY, &rh);

		struct tm tm_curr_time = get_rtc_time(i2c_dev);

		//corrected_lux = coeff_a * pow(lux.val1, 4) - coeff_b * pow(lux.val1, 3) + coeff_c * pow(lux.val1, 2) + coeff_d * lux.val1;

		sensor_data.lux = lux.val1;
		sensor_data.temp = (float)(((temp.val1 + (temp.val2 * 0.000001)) * 9 / 5) + 32.0 );
		sensor_data.rh = rh.val1 + (rh.val2 * 0.000001);
		 // Optionally clear the array (not strictly necessary before overwriting)
		memset(sd_ptr->datetime, 0, sizeof(sd_ptr->datetime));
		// Safely format and write the new datetime string
		snprintf(sd_ptr->datetime, sizeof(sd_ptr->datetime), "%s", format_time(&tm_curr_time));

		if (write_data_to_sd(disk_mount_pt, &sensor_data)){
			LOG_INF("Data successfully written to file");
		}
        printk("Datetime: %s, Lux: %"PRId32", TempF: %f, RH: %f%%\n", sensor_data.datetime, sensor_data.lux, sensor_data.temp, sensor_data.rh);

		fs_unmount(&mp);//unmount the filesystem for safety
		LOG_INF("Filesystem unmounted\n");
        	k_sleep(K_MSEC(3000));
	}

	return 0;
}

static int lsdir(const char *path)
{
	int res;
	struct fs_dir_t dirp;
	static struct fs_dirent entry;
	int count = 0;

	fs_dir_t_init(&dirp);

	/* Verify fs_opendir() */
	res = fs_opendir(&dirp, path);
	if (res) {
		LOG_ERR("Error opening dir %s [%d]", path, res);
		return res;
	}

	LOG_INF("Listing dir %s ...", path);
	for (;;) {
		/* Verify fs_readdir() */
		res = fs_readdir(&dirp, &entry);

		/* entry.name[0] == 0 means end-of-dir */
		if (res || entry.name[0] == 0) {
			break;
		}

		if (entry.type == FS_DIR_ENTRY_DIR) {
			LOG_INF("[DIR ] %s", entry.name);
		} else {
			LOG_INF("[FILE] %s (size = %zu)",
				entry.name, entry.size);
		}
		count++;
	}

	/* Verify fs_closedir() */
	fs_closedir(&dirp);
	if (res == 0) {
		res = count;
	}

	return res;
}