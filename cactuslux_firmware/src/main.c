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
} SensorData;

static const struct device *get_veml7700_device(void)
{  
    /* You can use either of these to get the device */
    const struct device *dev = DEVICE_DT_GET(DT_INST(0,vishay_veml7700));
    //const struct device *const dev = DEVICE_DT_GET_ANY(st_lsm6dsl);

    //veml7700_init(dev);
    if (dev == NULL) {
        /* No such node, or the node does not have status "okay". */
        printk("\nError: no device found.\n");
        return NULL;
    }

    if (!device_is_ready(dev)) {
        printk("\nError: Device \"%s\" is not ready; "
               "check the driver initialization logs for errors.\n",
               dev->name);
        return NULL;
    }

    printk("Found device \"%s\", getting sensor data\n", dev->name);
    return dev;
}

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
	printk("Base path before directory created: %s\n", path);

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
	printk("Base path after directory created: %s\n", path);

	if (fs_open(&file, path, FS_O_CREATE) != 0) {
		LOG_ERR("Failed to create file %s", path);
		return false;
	}
	fs_close(&file);

	return true;
}

/* Function to write data to directory */
static bool write_data_to_sd(const char *base_path, SensorData *data) {
	struct fs_file_t file;
	fs_file_t_init(&file);
	int buffer_size = 64;
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

	printk("The lux inside the write function is: %"PRId32"\n", data->lux);
	snprintf(logging_data, buffer_size, "Test, %"PRId32"\n", data->lux); //copy the sensor data into the buffer

	//printk("The size of sizeof(logging_data) is: %zu, where the size of strlen() is: %zu\n", sizeof(logging_data), strlen(logging_data));

	// Open up the SD Card
	if (fs_open(&file, path, FS_O_WRITE | FS_O_APPEND) != 0) {
		LOG_ERR("Failed to create file within write function %s\n", path);
		return false;
	}
	else {
		fs_write(&file, logging_data, strlen(logging_data));
		printk("Successfully opened up file within write function in path: %s\n", path);
		fs_close(&file);
	}
	return true;
}

int main(void)
{
	//retrieve the API-specific device structure
	const struct device *dev = get_veml7700_device();

	//get the device for the SHT45
	const struct device *sht45_dev = DEVICE_DT_GET(DT_INST(0,sensirion_sht4x));
	if (sht45_dev == NULL) {
		/* No such node, or the node does not have status "okay". */
		printk("\nError: no sht45 found.\n");
	}
	if (!device_is_ready(dev)) {
		printk("\nError: Device \"%s\" is not ready; "
				"check the driver initialization logs for errors.\n",
				dev->name);
	}
	printk("Found device \"%s\", getting sensor data\n", sht45_dev->name);

	bool start_flag = false;
	double corrected_lux;
	//coefficients for correction function
	double coeff_a = pow(6.0135, -13);
	double coeff_b = pow(9.3924, -9);
	double coeff_c = pow(8.1488, -5);
	double coeff_d = 1.0023;
	//printk("The coefficients for the correction formula are:\ta: %.15lf, b: %.15lf, c: %.15lf, d: %.15lf\n", coeff_a, coeff_b, coeff_c, coeff_d);

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
		printk("Sector size %u\n", block_size);

		memory_size_mb = (uint64_t)block_count * block_size;
		printk("Memory Size(MB) %u\n", (uint32_t)(memory_size_mb >> 20));
	} while (0);

	//define the mounting point
	mp.mnt_point = disk_mount_pt;

	// define sensor_value struct
	static struct sensor_value lux;    
	static struct sensor_value gain;
	static struct sensor_value it;
	gain.val1 = VEML7700_ALS_GAIN_1_8; //gain of 1/8
	it.val1 = VEML7700_ALS_IT_25; //25 ms integration time
	SensorData sensor_data;

	// sensor readings for sht45
	static struct sensor_value temp;
	static struct sensor_value rh;

	//configure the sensor registers
	sensor_attr_set(dev, SENSOR_CHAN_LIGHT, SENSOR_ATTR_VEML7700_GAIN, &gain);
	printk("The gain attribute has been set to %d.\n", gain.val1);
	sensor_attr_set(dev, SENSOR_CHAN_LIGHT, SENSOR_ATTR_VEML7700_ITIME, &it);
	printk("The integration time has been set to %d\n", it.val1);
	//sensor_attr_get(dev, SENSOR_CHAN_LIGHT, SENSOR_ATTR_VEML7700_ITIME, &it2);
	//printk("The integration time returned is %d\n", it2.val1);

	// while loop for data logging
	while (1){
		int res = fs_mount(&mp);
		if (res == FR_OK) {
			printk("Disk mounted.\n");
			if (start_flag == false) { //do this once only
				if (lsdir(disk_mount_pt) < 0) {
					LOG_ERR("Error opening directory using lsdir()\n");
				}
				if (create_directory(disk_mount_pt)) {
					printk("create_directory() returned true\n");
				}
				start_flag = true;
			}
		}
		else {
			printk("Error mounting disk.\n");
		}

		sensor_sample_fetch_chan(dev, SENSOR_CHAN_LIGHT);
        sensor_channel_get(dev, SENSOR_CHAN_LIGHT, &lux);
		sensor_data.lux = lux.val1;
		corrected_lux = coeff_a * pow(lux.val1, 4) - coeff_b * pow(lux.val1, 3) + coeff_c * pow(lux.val1, 2) + coeff_d * lux.val1;

		sensor_sample_fetch_chan(sht45_dev, SENSOR_CHAN_AMBIENT_TEMP);
		sensor_channel_get(sht45_dev, SENSOR_CHAN_AMBIENT_TEMP, &temp);
		sensor_sample_fetch_chan(sht45_dev, SENSOR_CHAN_HUMIDITY);
		sensor_channel_get(sht45_dev, SENSOR_CHAN_HUMIDITY, &rh);

		if (write_data_to_sd(disk_mount_pt, &sensor_data)){
			printk("Data successfully written to file\n");
		}
        printk("lux: %d.%03d\n",lux.val1, lux.val2);
		printk("temp: %d.%03d\n",temp.val1, temp.val2);
		printk("rh: %d.%03d\n",rh.val1, rh.val2);

		fs_unmount(&mp);//unmount the filesystem for safety
		printk("Filesystem unmounted\n\n");
        	k_sleep(K_MSEC(5000));
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
		printk("Error opening dir %s [%d]\n", path, res);
		return res;
	}

	printk("\nListing dir %s ...\n", path);
	for (;;) {
		/* Verify fs_readdir() */
		res = fs_readdir(&dirp, &entry);

		/* entry.name[0] == 0 means end-of-dir */
		if (res || entry.name[0] == 0) {
			break;
		}

		if (entry.type == FS_DIR_ENTRY_DIR) {
			printk("[DIR ] %s\n", entry.name);
		} else {
			printk("[FILE] %s (size = %zu)\n",
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