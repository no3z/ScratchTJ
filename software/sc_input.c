// SC1000 input handler
// Thread that grabs data from the rotary sensor and PIC input processor and processes it

#include <assert.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <fcntl.h>		   //Needed for I2C port
#include <sys/ioctl.h>	   //Needed for I2C port
#include <linux/i2c-dev.h> //Needed for I2C port
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/time.h>
#include "sc_playlist.h"
#include "alsa.h"
#include "controller.h"
#include "device.h"
#include "dummy.h"
#include "realtime.h"
#include "thread.h"
#include "rig.h"
#include "track.h"
#include "xwax.h"
#include "sc_input.h"
#include "sc_midimap.h"
#include "dicer.h"
#include "midi.h"
#include "lcd_menu.h"
#include <termios.h> // For serial port settings
#include "shared_variables.h"

int serial_fd; // File descriptor for the serial port

bool shifted = 0;
bool shiftLatched = 0;

extern struct rt rt;

struct controller midiControllers[32];
int numControllers = 0;

#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)       \
	(byte & 0x80 ? '1' : '0'),     \
		(byte & 0x40 ? '1' : '0'), \
		(byte & 0x20 ? '1' : '0'), \
		(byte & 0x10 ? '1' : '0'), \
		(byte & 0x08 ? '1' : '0'), \
		(byte & 0x04 ? '1' : '0'), \
		(byte & 0x02 ? '1' : '0'), \
		(byte & 0x01 ? '1' : '0')

extern struct mapping *maps;
void init_serial(const char *port_name) {
    serial_fd = open(port_name, O_RDWR | O_NOCTTY | O_NDELAY);
    if (serial_fd < 0) {
        perror("Error opening serial port");
        exit(EXIT_FAILURE);
    }
    printf("Serial port %s opened successfully.\n", port_name);

    struct termios tty;
    memset(&tty, 0, sizeof(tty));
    if (tcgetattr(serial_fd, &tty) != 0) {
        perror("Error getting terminal attributes");
        close(serial_fd);
        exit(EXIT_FAILURE);
    }
    printf("Terminal attributes fetched successfully.\n");

    // Set baud rate
    cfsetospeed(&tty, B115200);
    cfsetispeed(&tty, B115200);

    // Enable receiver and set local mode
    tty.c_cflag |= (CLOCAL | CREAD);

    // Set character size to 8 bits
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;

    // No parity
    tty.c_cflag &= ~PARENB;

    // One stop bit
    tty.c_cflag &= ~CSTOPB;

    // No hardware flow control
    tty.c_cflag &= ~CRTSCTS;

    // Raw input
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP |
                     INLCR | IGNCR | ICRNL | IXON | IXOFF | IXANY);

    // Raw output
    tty.c_oflag &= ~OPOST;

    // Non-canonical mode, disable echo, signals, etc.
    tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);

    // Non-blocking read
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 1; // 0.1 second read timeout

    // Apply the settings
    if (tcsetattr(serial_fd, TCSANOW, &tty) != 0) {
        perror("Error setting terminal attributes");
        close(serial_fd);
        exit(EXIT_FAILURE);
    }
    printf("Terminal attributes applied successfully.\n");

    // Flush the serial port buffers
    tcflush(serial_fd, TCIOFLUSH);
}

void i2c_read_address(int file_i2c, unsigned char address, unsigned char *result)
{

	*result = address;
	if (write(file_i2c, result, 1) != 1)
	{
		printf("I2C read error\n");
		exit(1);
	}

	if (read(file_i2c, result, 1) != 1)
	{
		printf("I2C read error\n");
		exit(1);
	}
}

int i2c_write_address(int file_i2c, unsigned char address, unsigned char value)
{
	char buf[2];
	buf[0] = address;
	buf[1] = value;
	if (write(file_i2c, buf, 2) != 2)
	{
		printf("I2C Write Error\n");
		return 0;
	}
	else
		return 1;
}

void dump_maps()
{
	struct mapping *new_map = maps;
	while (new_map != NULL)
	{
		printf("Dump Mapping - ty:%d po:%d pn%x pl:%x ed%x mid:%x:%x:%x- dn:%d, a:%d, p:%d\n", new_map->Type, new_map->port, new_map->Pin, new_map->Pullup, new_map->Edge, new_map->MidiBytes[0], new_map->MidiBytes[1], new_map->MidiBytes[2], new_map->DeckNo, new_map->Action, new_map->Param);
		new_map = new_map->next;
	}
}

int setupi2c(char *path, unsigned char address)
{

	int file = 0;

	if ((file = open(path, O_RDWR)) < 0)
	{
		printf("%s - Failed to open\n", path);
		return -1;
	}
	else if (ioctl(file, I2C_SLAVE, address) < 0)
	{
		printf("%s - Failed to acquire bus access and/or talk to slave.\n", path);
		return -1;
	}
	else
		return file;
}

void AddNewMidiDevices(char mididevices[64][64], int mididevicenum)
{
	bool alreadyAdded;
	// Search to see which devices we've already added
	for (int devc = 0; devc < mididevicenum; devc++)
	{

		alreadyAdded = 0;

		for (int controlc = 0; controlc < numControllers; controlc++)
		{
			char *controlName = ((struct dicer *)(midiControllers[controlc].local))->PortName;
			if (strcmp(mididevices[devc], controlName) == 0)
				alreadyAdded = 1;
		}

		if (!alreadyAdded)
		{
			if (dicer_init(&midiControllers[numControllers], &rt, mididevices[devc]) != -1)
			{
				printf("Adding MIDI device %d - %s\n", numControllers, mididevices[devc]);
				controller_add_deck(&midiControllers[numControllers], &deck[0]);
				controller_add_deck(&midiControllers[numControllers], &deck[1]);
				numControllers++;
			}
		}
	}
}
unsigned char gpiopresent = 1;
unsigned char mmappresent = 1;
int file_i2c_gpio;
volatile void *gpio_addr;

bool firstTimeRound = 1;
void init_io()
{
	int i, j;
	struct mapping *map;

			gpiopresent = 0;
	// Configure external IO
	if (gpiopresent)
	{

		// default to pulled up and input
		unsigned int pullups = 0xFFFF;
		unsigned int iodirs = 0xFFFF;

		// For each pin
		for (i = 0; i < 16; i++)
		{
			map = find_IO_mapping(maps, 0, i, 1);
			// If pin is marked as ground
			if (map != NULL && map->Action == ACTION_GND)
			{
				//printf("Grounding pin %d\n", i);
				iodirs &= ~(0x0001 << i);
			}

			// If pin's pullup is disabled
			if (map != NULL && !map->Pullup)
			{
				//printf("Disabling pin %d pullup\n", i);
				pullups &= ~(0x0001 << i);
			}
			else printf ("Pulling up pin %d\n", i);
		}

		unsigned char tmpchar;

		// Bank A pullups
		tmpchar = (unsigned char)(pullups & 0xFF);
		i2c_write_address(file_i2c_gpio, 0x0C, tmpchar);

		// Bank B pullups
		tmpchar = (unsigned char)((pullups >> 8) & 0xFF);
		i2c_write_address(file_i2c_gpio, 0x0D, tmpchar);

		/*printf("PULLUPS - B");
		printf(BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY((pullups >> 8) & 0xFF));
		printf("A");
		printf(BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY((pullups & 0xFF)));
		printf("\n");*/

		// Bank A direction
		tmpchar = (unsigned char)(iodirs & 0xFF);
		i2c_write_address(file_i2c_gpio, 0x00, tmpchar);

		// Bank B direction
		tmpchar = (unsigned char)((iodirs >> 8) & 0xFF);
		i2c_write_address(file_i2c_gpio, 0x01, tmpchar);

		/*printf("IODIRS  - B");
		printf(BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY((iodirs >> 8) & 0xFF));
		printf("A");
		printf(BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(iodirs & 0xFF));
		printf("\n");*/
	}

	// Configure A13 GPIO

	int fd = open("/dev/mem", O_RDWR | O_SYNC);
	if (fd < 0)
	{
		fprintf(stderr, "Unable to open port\n\r");
		//exit(fd);
		mmappresent = 0;
	}
	gpio_addr = mmap(NULL, 65536, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0x01C20800 & 0xffff0000);
	if (gpio_addr == MAP_FAILED)
	{
		fprintf(stderr, "Unable to open mmap\n\r");
		//exit(fd);
		mmappresent = 0;
	}
	gpio_addr += 0x0800;

	if (mmappresent)
	{
		// For each port
		for (j = 1; j <= 6; j++)
		{
			// For each pin (max number of pins on each port is 28)
			for (i = 0; i < 28; i++)
			{

				map = find_IO_mapping(maps, j, i, 1);

				if (map != NULL)
				{
					// dirty hack, don't map J7 SCL/SDA pins if MCP is present
					if (gpiopresent && j == 1 && (i == 15 || i == 16)){
						map->Action = ACTION_NOTHING;
					}
					else {
						//printf("Pulling %d %d %d\n", j, i, map->Pullup);
						// which config register to use, 0-3
						uint32_t configregister = i >> 3;

						// which pull register to use, 0-1
						uint32_t pullregister = i >> 4;

						// how many bits to shift the config register
						uint32_t configShift = (i % 8) * 4;

						// how many bits to shift the pull register
						uint32_t pullShift = (i % 16) * 2;

						volatile uint32_t *PortConfigRegister = gpio_addr + (j * 0x24) + (configregister * 0x04);
						volatile uint32_t *PortPullRegister = gpio_addr + (j * 0x24) + 0x1C + (pullregister * 0x04);
						uint32_t portConfig = *PortConfigRegister;
						uint32_t portPull = *PortPullRegister;

						// mask to unset the relevant pins in the registers
						uint32_t configMask = ~(0b1111 << configShift);
						uint32_t pullMask = ~(0b11 << pullShift);

						// Set port as input
						// portConfig = (portConfig & configMask) | (0b0000 << configShift); (not needed because input is 0 anyway)
						portConfig = (portConfig & configMask);

						portPull = (portPull & pullMask) | (map->Pullup << pullShift);
						*PortConfigRegister = portConfig;
						*PortPullRegister = portPull;
					}
				}
			}
		}
	}

}

void process_io()
{ // Iterate through all digital input mappings and check the appropriate pin
	unsigned int gpios = 0x00000000;
	unsigned char result;
	// if (gpiopresent)
	// {
	// 	i2c_read_address(file_i2c_gpio, 0x13, &result); // Read bank B
	// 	gpios = ((unsigned int)result) << 8;
	// 	//printf(BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(result));
	// 	i2c_read_address(file_i2c_gpio, 0x12, &result); // Read bank A
	// 	gpios |= result;
	// 	//printf(" - ");
	// 	//printf(BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(result));
	// 	//printf("\n");

	// 	// invert logic
	// 	gpios ^= 0xFFFF;
	// }
	struct mapping *last_map = maps;
	while (last_map != NULL)
	{
		//printf("arses : %d %d\n", last_map->port, last_map->Pin);

		// Only digital pins
		if (last_map->Type == MAP_IO && (!(last_map->port == 0 && !gpiopresent)))
		{

			bool pinVal = 0;
			if (last_map->port == 0 && gpiopresent) // port 0, I2C GPIO expander
			{
				pinVal = (bool)((gpios >> last_map->Pin) & 0x01);
			}
			else if (mmappresent) // Ports 1-6, olimex GPIO
			{
				volatile uint32_t *PortDataReg = gpio_addr + (last_map->port * 0x24) + 0x10;
				uint32_t PortData = *PortDataReg;
				PortData ^= 0xffffffff;
				pinVal = (bool)((PortData >> last_map->Pin) & 0x01);
			}
			else
			{
				pinVal = 0;
			}

			// iodebounce = 0 when button not pressed,
			// > 0 and < scsettings.debouncetime when debouncing positive edge
			// > scsettings.debouncetime and < scsettings.holdtime when holding
			// = scsettings.holdtime when continuing to hold
			// > scsettings.holdtime when waiting for release
			// > -scsettings.debouncetime and < 0 when debouncing negative edge

			// Button not pressed, check for button
			if (last_map->debounce == 0)
			{
				if (pinVal)
				{
					printf("Button %d pressed\n", last_map->Pin);
					if (firstTimeRound && last_map->DeckNo == 1 && (last_map->Action == ACTION_VOLUP || last_map->Action == ACTION_VOLDOWN))
					{
						player_set_track(&deck[0].player, track_acquire_by_import(deck[0].importer, "/var/os-version.mp3"));
						cues_load_from_file(&deck[0].cues, deck[0].player.track->path);
						deck[1].player.setVolume = 0.0;
					}
					else
					{
						if ((!shifted && last_map->Edge == 1) || (shifted && last_map->Edge == 3))
							IOevent(last_map, NULL);

						// start the counter
						last_map->debounce++;
					}
				}
			}

			// Debouncing positive edge, increment value
			else if (last_map->debounce > 0 && last_map->debounce < scsettings.debouncetime)
			{
				last_map->debounce++;
			}

			// debounce finished, keep incrementing until hold reached
			else if (last_map->debounce >= scsettings.debouncetime && last_map->debounce < scsettings.holdtime)
			{
				// check to see if unpressed
				if (!pinVal)
				{
					printf("Button %d released\n", last_map->Pin);
					if (last_map->Edge == 0)
						IOevent(last_map, NULL);
					// start the counter
					last_map->debounce = -scsettings.debouncetime;
				}

				else
					last_map->debounce++;
			}
			// Button has been held for a while
			else if (last_map->debounce == scsettings.holdtime)
			{
				printf("Button %d-%d held\n", last_map->port, last_map->Pin);
				if ((!shifted && last_map->Edge == 2) || (shifted && last_map->Edge == 4))
					IOevent(last_map, NULL);
				last_map->debounce++;
			}

			// Button still holding, check for release
			else if (last_map->debounce > scsettings.holdtime)
			{
				if (pinVal)
				{
					if (last_map->Action == ACTION_VOLUHOLD || last_map->Action == ACTION_VOLDHOLD)
					{
						// keep running the vol up/down actions if they're held
						if ((!shifted && last_map->Edge == 2) || (shifted && last_map->Edge == 4))
							IOevent(last_map, NULL);
					}
				}
				// check to see if unpressed
				else
				{
					printf("Button %d released\n", last_map->Pin);
					if (last_map->Edge == 0)
						IOevent(last_map, NULL);
					// start the counter
					last_map->debounce = -scsettings.debouncetime;
				}
			}

			// Debouncing negative edge, increment value - will reset when zero is reached
			else if (last_map->debounce < 0)
			{
				last_map->debounce++;
			}
		}

		last_map = last_map->next;
	}

	// Dumb hack to process MIDI commands in this thread rather than the realtime one
	if (QueuedMidiCommand != NULL)
	{
		IOevent(QueuedMidiCommand, QueuedMidiBuffer);
		QueuedMidiCommand = NULL;
	}
}

int file_i2c_rot, file_i2c_pic;

int pitchMode = 0; // If we're in pitch-change mode
int oldPitchMode = 0;
bool capIsTouched = 0;
unsigned char buttons[4] = {0, 0, 0, 0}, totalbuttons[4] = {0, 0, 0, 0};
unsigned int ADCs[4] = {0, 0, 0, 0};
unsigned char buttonState = 0;
unsigned int butCounter = 0;
unsigned char faderOpen1 = 0, faderOpen2 = 0;

#define BUFFER_SIZE 256
char read_buffer[BUFFER_SIZE];
char line_buffer[BUFFER_SIZE];
int line_index = 0;

void read_serial_data() {
    int bytes_read = read(serial_fd, read_buffer, sizeof(read_buffer) - 1);

    if (bytes_read > 0) {
        read_buffer[bytes_read] = '\0'; // Null-terminate the string

        for (int i = 0; i < bytes_read; i++) {
            if (read_buffer[i] == '\n') {
                // Process complete line
                line_buffer[line_index] = '\0';
                line_index = 0;
				float curveSwitch;
				bool switchFader = false;
				get_variable_value("Fad Switch", &curveSwitch);
				if(curveSwitch > 0.5) { switchFader = true; }

                // Parse the fader and encoder values
                int fader_value, encoder_value, capacitive_value;
                if (sscanf(line_buffer, "%d %d %d", &fader_value, &encoder_value, &capacitive_value) == 3) {
					ADCs[0] = switchFader ? fader_value : (1023 - fader_value);
					ADCs[1] = switchFader ? (1023 - fader_value) : fader_value;
					ADCs[2] = switchFader ? fader_value : (1023 - fader_value);
					ADCs[3] = switchFader ? (1023 - fader_value) : fader_value;
					capIsTouched = 0;
					encoder_value = 4096 - encoder_value;

					if (capacitive_value > 5000) {
						capIsTouched = 1; // Set touched if capacitance exceeds threshold
					} 
                    // printf("Fader: %d, Encoder: %d Capcitative: %d Touched: %d diff: %d\n", ADCs[0], encoder_value, capacitive_value, capIsTouched,  (abs(encoder_value - deck[1].newEncoderAngle) > )  );

                    deck[1].newEncoderAngle = encoder_value;
                } else {
                    printf("Malformed line: %s\n", line_buffer);
                }
            } else {
                // Accumulate data in the line buffer
                if (line_index < sizeof(line_buffer) - 1) {
                    line_buffer[line_index++] = read_buffer[i];
                }
            }
        }
    }
}
void process_pic()
{
    if (ADCs[0] < 0) ADCs[0] = 0;
    if (ADCs[0] > 1023) ADCs[0] = 1023;

    double fader = ADCs[0] / 1023.0; // Normalize fader to range [0, 1]
	float curveFactor;
    get_variable_value("Fad Factor", &curveFactor);
	float curvePower;
    get_variable_value("Fad Power", &curvePower);

    if (fader <= 0.5) {
        // Left side: Deck 0 stays at full volume, Deck 1 fades in
        deck[1].player.faderTarget = 1.0;
        deck[0].player.faderTarget = pow(fader / curveFactor, curvePower); // Use curveFactor to shift the transition
    } else {
        // Right side: Deck 1 stays at full volume, Deck 0 fades in
        deck[1].player.faderTarget = pow((1.0 - fader) / curveFactor, curvePower); // Use curveFactor to shift the transition
        deck[0].player.faderTarget = 1.0;
    }

    // Ensure faderTarget does not exceed bounds
    if (deck[0].player.faderTarget > 1.0) deck[0].player.faderTarget = 1.0;
    if (deck[1].player.faderTarget > 1.0) deck[1].player.faderTarget = 1.0;

	//printf("%d %d %d %d", fadertarget0,fadertarget1,deck[0].player.setVolume,deck[1].player.setVolume);
}

// Keep a running average of speed so if we suddenly let go it keeps going at that speed
double averageSpeed = 0.0;
unsigned int numBlips = 0;
void process_rot()
{
	int8_t crossedZero; // 0 when we haven't crossed zero, -1 when we've crossed in anti-clockwise direction, 1 when crossed in clockwise
	int wrappedAngle = 0x0000;
	// Handle rotary sensor

	if (scsettings.jogReverse) {
		//
		deck[1].newEncoderAngle = 4095 - deck[1].newEncoderAngle;
		//printf("%d\n",deck[1].newEncoderAngle);
	}

	// First time, make sure there's no difference
	if (deck[1].encoderAngle == 0xffff)
		deck[1].encoderAngle = deck[1].newEncoderAngle;

	// Handle wrapping at zero

	if (deck[1].newEncoderAngle < 1024 && deck[1].encoderAngle >= 3072)
	{ // We crossed zero in the positive direction

		crossedZero = 1;
		wrappedAngle = deck[1].encoderAngle - 4096;
	}
	else if (deck[1].newEncoderAngle >= 3072 && deck[1].encoderAngle < 1024)
	{ // We crossed zero in the negative direction
		crossedZero = -1;
		wrappedAngle = deck[1].encoderAngle + 4096;
	}
	else
	{
		crossedZero = 0;
		wrappedAngle = deck[1].encoderAngle;
	}

	// rotary sensor sometimes returns incorrect values, if we skip more than 100 ignore that value
	// If we see 3 blips in a row, then I guess we better accept the new value

	// if (abs(deck[1].newEncoderAngle - wrappedAngle) > 100 && numBlips < 1)
	// {
	// 	// printf("blip! %d %d %d %d\n", deck[1].newEncoderAngle-deck[1].encoderAngle, deck[1].newEncoderAngle, wrappedAngle, numBlips);
	// 	numBlips++;
	// }
	// else
	{
		numBlips = 0;
		deck[1].encoderAngle = deck[1].newEncoderAngle;

		if (pitchMode)
		{

			if (!oldPitchMode)
			{ // We just entered pitchmode, set offset etc

				deck[(pitchMode - 1)].player.note_pitch = 1.0;
				deck[1].angleOffset = -deck[1].encoderAngle;
				oldPitchMode = 1;
				deck[1].player.capTouch = 0;
			}

			// Handle wrapping at zero

			if (crossedZero > 0)
			{
				deck[1].angleOffset += 4096;
			}
			else if (crossedZero < 0)
			{
				deck[1].angleOffset -= 4096;
			}

			// Use the angle of the platter to control sample pitch

			deck[(pitchMode - 1)].player.note_pitch = (((double)(deck[1].encoderAngle + deck[1].angleOffset)) / 16384) + 1.0;
			printf("pitch! %d %d %d\n", deck[(pitchMode - 1)].player.note_pitch, deck[1].encoderAngle, deck[1].angleOffset);

		}
		else
		{

			if (scsettings.platterenabled)
			{
				// Handle touch sensor
				if (capIsTouched || deck[1].player.motor_speed == 0.0)
				{

					// Positive touching edge
					if (!deck[1].player.capTouch || oldPitchMode && !deck[1].player.stopped)
					{

						float platterspeed;
						get_variable_value("platterspeed", &platterspeed);
						deck[1].angleOffset = (deck[1].player.position * platterspeed) - deck[1].encoderAngle;
						// printf("touch!\n");
						deck[1].player.target_position = deck[1].player.position;
						deck[1].player.capTouch = 1;
					}
				}
				else
				{
					deck[1].player.capTouch = 0;
				}
			}

			else
				deck[1].player.capTouch = 1;

			/*if (deck[1].player.capTouch) we always want to dump the target position so we can do lasers etc
			{*/

			// Handle wrapping at zero

			if (crossedZero > 0)
			{
				deck[1].angleOffset += 4096;
			}
			else if (crossedZero < 0)
			{
				deck[1].angleOffset -= 4096;
			}

			// Convert the raw value to track position and set player to that pos
				float platterspeed;
			get_variable_value("platterspeed", &platterspeed);
			deck[1].player.target_position = (double)(deck[1].encoderAngle + deck[1].angleOffset) / platterspeed;

			// Loop when track gets to end

			/*if (deck[1].player.target_position > ((double)deck[1].player.track->length / (double)deck[1].player.track->rate))
					{
						deck[1].player.target_position = 0;
						angleOffset = encoderAngle;
					}*/
		}
		//}
		oldPitchMode = pitchMode;
	}
}

void *SC_InputThread(void *ptr)
{
	unsigned char picskip = 0;
	unsigned char picpresent = 0;
	unsigned char rotarypresent = 1;

	char mididevices[64][64];
	int mididevicenum = 0, oldmididevicenum = 0;

	// Initialise rotary sensor on I2C0

	// if ((file_i2c_rot = setupi2c("/dev/i2c-0", 0x36)) < 0)
	// {
	// 	printf("Couldn't init rotary sensor\n");
	// 	//rotarypresent = 0;
	// }

	// // Initialise PIC input processor on I2C2

	// if ((file_i2c_pic = setupi2c("/dev/i2c-2", 0x69)) < 0)
	// {
	// 	printf("Couldn't init input processor\n");
	// 	picpresent = 0;
	// }
	
	init_serial("/dev/serial0");
	// init_io();

	srand(time(NULL)); // TODO - need better entropy source, SoC is starting up annoyingly deterministically

	struct timeval tv;
	unsigned long lastTime = 0;
	unsigned int frameCount = 0;
	struct timespec ts;
	double inputtime = 0, lastinputtime = 0;


	int secondCount = 0;
	deck[0].player.volume = 0.5;
			deck[1].player.capTouch = 0;
			deck[1].player.faderTarget = 0.5;
			
			deck[0].player.faderTarget = 0.5;
			deck[0].player.justPlay = 0;
			deck[0].player.pitch = 1;

		deck_random_file(&deck[0]);
		// deck_random_file(&deck[1]);

	while (1) // Main input loop
	{

		frameCount++;

		// Update display every second
		gettimeofday(&tv, NULL);
		if (tv.tv_sec != lastTime)
		{
			lastTime = tv.tv_sec;
			// printf("\033[H\033[J"); // Clear Screen
			// printf("\nFPS: %06u - ADCS: %04u, %04u, %04u, %04u, %04u\nButtons: %01u,%01u,%01u,%01u,%01u\nTP: %f, P : %f\n\nTP: %f, P : %f\n%f -- %f\n",
			// 	   frameCount, ADCs[0], ADCs[1], ADCs[2], ADCs[3], deck[1].encoderAngle,
			// 	   buttons[0], buttons[1], buttons[2], buttons[3], capIsTouched,
			// 	   deck[0].player.target_position, deck[0].player.position,
			// 	   deck[1].player.target_position, deck[1].player.position,					
			// 	   deck[0].player.volume, deck[1].player.volume);
			
			//dump_maps();

			//printf("\nFPS: %06u\n", frameCount);
			frameCount = 0;

			// list midi devices
			// for (int cunt = 0; cunt < numControllers; cunt++)
			// {
			// 	printf("MIDI : %s\n", ((struct dicer *)(midiControllers[cunt].local))->PortName);
			// }

			// Wait 10 seconds to enumerate MIDI devices
			// Give them a little time to come up properly
			if (secondCount < scsettings.mididelay)
				secondCount++;
			else if (secondCount == scsettings.mididelay)
			{
				// Check for new midi devices
				mididevicenum = listdev("rawmidi", mididevices);

				// If there are more MIDI devices than last time, add them
				if (mididevicenum > oldmididevicenum)
				{
					AddNewMidiDevices(mididevices, mididevicenum);
					oldmididevicenum = mididevicenum;
				}
				secondCount = 999;
			}
		}

		// Get info from input processor registers
		// First the ADC values
		// 5 = XFADER1, 6 = XFADER2, 7 = POT1, 8 = POT2
		read_serial_data();
		picpresent = 1;

process_pic();
			process_rot();
		if (picpresent)
		{
			picskip++;
			if (picskip > 4)
			{
				picskip = 0;
				
				firstTimeRound = 0;
			}

		}
		else // couldn't find input processor, just play the tracks
		{
			deck[1].player.capTouch = 1;
			deck[1].player.faderTarget = 0.5;
			
			deck[0].player.faderTarget = 0.5;
			deck[0].player.justPlay = 1;
			deck[0].player.pitch = 1;

			clock_gettime(CLOCK_MONOTONIC, &ts);
			inputtime = (double)ts.tv_sec + ((double)ts.tv_nsec / 1000000000.0);

			if (lastinputtime != 0)
			{
				deck[1].player.target_position += (inputtime - lastinputtime);
			}

			lastinputtime = inputtime;
		}
		float update_rate;
		get_variable_value("update_rate", &update_rate);
		usleep((int)update_rate);
	}
}

// Start the input thread
void SC_Input_Start()
{

	pthread_t thread1;
	const char *message1 = "Thread 1";
	int iret1;

	iret1 = pthread_create(&thread1, NULL, SC_InputThread, (void *)message1);

	if (iret1)
	{
		fprintf(stderr, "Error - pthread_create() return code: %d\n", iret1);
		exit(EXIT_FAILURE);
	}
}
