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
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <time.h>
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
#include <termios.h>
#include "shared_variables.h"
#include "oled_display.h"

/* ── Serial protocol constants ─────────────────────────────────── */
#define SERIAL_DEV    "/dev/serial0"
#define SERIAL_BAUD   B500000
#define SYNC_BYTE     0xAA
#define HANDSHAKE_MAGIC 0x53
#define PKT_LEN       7

/* ── MT6701 constants ──────────────────────────────────────────── */
#define MT6701_ADDR      0x06
#define MT6701_ANGLE_H   0x03
#define MT6701_ANGLE_L   0x04

/* ── Encoder constants (14-bit) ────────────────────────────────── */
#define ENCODER_CPR      16384

/* ── Globals ───────────────────────────────────────────────────── */
int serial_fd;
static int i2c_mt6701_fd = -1;

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

/* ── Serial init (binary protocol, 500000 baud) ───────────────── */
void init_serial(const char *port_name) {
    serial_fd = open(port_name, O_RDWR | O_NOCTTY | O_NONBLOCK);
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

    cfmakeraw(&tty);
    cfsetispeed(&tty, SERIAL_BAUD);
    cfsetospeed(&tty, SERIAL_BAUD);
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~CRTSCTS;
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 0;

    if (tcsetattr(serial_fd, TCSANOW, &tty) != 0) {
        perror("Error setting terminal attributes");
        close(serial_fd);
        exit(EXIT_FAILURE);
    }
    tcflush(serial_fd, TCIOFLUSH);

    /* Send handshake */
    uint8_t hs = HANDSHAKE_MAGIC;
    write(serial_fd, &hs, 1);
    usleep(100000);

    printf("Serial init done (500000 baud, binary protocol).\n");
}

/* ── MT6701 I2C init ───────────────────────────────────────────── */
static int mt6701_init(void) {
    i2c_mt6701_fd = open("/dev/i2c-1", O_RDWR);
    if (i2c_mt6701_fd < 0) {
        perror("MT6701: open i2c");
        return -1;
    }
    if (ioctl(i2c_mt6701_fd, I2C_SLAVE, MT6701_ADDR) < 0) {
        perror("MT6701: set addr");
        close(i2c_mt6701_fd);
        i2c_mt6701_fd = -1;
        return -1;
    }
    printf("MT6701 initialized on /dev/i2c-1 addr 0x%02X\n", MT6701_ADDR);
    return 0;
}

/* Read 14-bit angle (0-16383) from MT6701. Returns -1 on error. */
static int mt6701_read_angle(void) {
    if (i2c_mt6701_fd < 0) return -1;
    uint8_t reg = MT6701_ANGLE_H;
    if (write(i2c_mt6701_fd, &reg, 1) != 1) return -1;
    uint8_t buf[2];
    if (read(i2c_mt6701_fd, buf, 2) != 2) return -1;
    /* 14-bit angle: reg03[7:0] = bits 13..6, reg04[7:2] = bits 5..0 */
    return ((uint16_t)buf[0] << 6) | (buf[1] >> 2);
}

/* ── Legacy I2C helpers (kept for GPIO expander compatibility) ── */
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
				iodirs &= ~(0x0001 << i);
			}

			// If pin's pullup is disabled
			if (map != NULL && !map->Pullup)
			{
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

		// Bank A direction
		tmpchar = (unsigned char)(iodirs & 0xFF);
		i2c_write_address(file_i2c_gpio, 0x00, tmpchar);

		// Bank B direction
		tmpchar = (unsigned char)((iodirs >> 8) & 0xFF);
		i2c_write_address(file_i2c_gpio, 0x01, tmpchar);
	}

	// Configure A13 GPIO

	int fd = open("/dev/mem", O_RDWR | O_SYNC);
	if (fd < 0)
	{
		fprintf(stderr, "Unable to open port\n\r");
		mmappresent = 0;
	}
	gpio_addr = mmap(NULL, 65536, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0x01C20800 & 0xffff0000);
	if (gpio_addr == MAP_FAILED)
	{
		fprintf(stderr, "Unable to open mmap\n\r");
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
						uint32_t configregister = i >> 3;
						uint32_t pullregister = i >> 4;
						uint32_t configShift = (i % 8) * 4;
						uint32_t pullShift = (i % 16) * 2;

						volatile uint32_t *PortConfigRegister = gpio_addr + (j * 0x24) + (configregister * 0x04);
						volatile uint32_t *PortPullRegister = gpio_addr + (j * 0x24) + 0x1C + (pullregister * 0x04);
						uint32_t portConfig = *PortConfigRegister;
						uint32_t portPull = *PortPullRegister;

						uint32_t configMask = ~(0b1111 << configShift);
						uint32_t pullMask = ~(0b11 << pullShift);

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
{
	unsigned int gpios = 0x00000000;
	unsigned char result;
	struct mapping *last_map = maps;
	while (last_map != NULL)
	{
		// Only digital pins
		if (last_map->Type == MAP_IO && (!(last_map->port == 0 && !gpiopresent)))
		{

			bool pinVal = 0;
			if (last_map->port == 0 && gpiopresent)
			{
				pinVal = (bool)((gpios >> last_map->Pin) & 0x01);
			}
			else if (mmappresent)
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

						last_map->debounce++;
					}
				}
			}
			else if (last_map->debounce > 0 && last_map->debounce < scsettings.debouncetime)
			{
				last_map->debounce++;
			}
			else if (last_map->debounce >= scsettings.debouncetime && last_map->debounce < scsettings.holdtime)
			{
				if (!pinVal)
				{
					printf("Button %d released\n", last_map->Pin);
					if (last_map->Edge == 0)
						IOevent(last_map, NULL);
					last_map->debounce = -scsettings.debouncetime;
				}
				else
					last_map->debounce++;
			}
			else if (last_map->debounce == scsettings.holdtime)
			{
				printf("Button %d-%d held\n", last_map->port, last_map->Pin);
				if ((!shifted && last_map->Edge == 2) || (shifted && last_map->Edge == 4))
					IOevent(last_map, NULL);
				last_map->debounce++;
			}
			else if (last_map->debounce > scsettings.holdtime)
			{
				if (pinVal)
				{
					if (last_map->Action == ACTION_VOLUHOLD || last_map->Action == ACTION_VOLDHOLD)
					{
						if ((!shifted && last_map->Edge == 2) || (shifted && last_map->Edge == 4))
							IOevent(last_map, NULL);
					}
				}
				else
				{
					printf("Button %d released\n", last_map->Pin);
					if (last_map->Edge == 0)
						IOevent(last_map, NULL);
					last_map->debounce = -scsettings.debouncetime;
				}
			}
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

int pitchMode = 0;
int oldPitchMode = 0;
bool capIsTouched = 0;
unsigned char buttons[4] = {0, 0, 0, 0}, totalbuttons[4] = {0, 0, 0, 0};
unsigned int ADCs[4] = {0, 0, 0, 0};
unsigned char buttonState = 0;
unsigned int butCounter = 0;
unsigned char faderOpen1 = 0, faderOpen2 = 0;

/* ── Cue button state ──────────────────────────────────────────── */
static uint8_t last_button_byte = 0;
static unsigned long cue_press_time[4] = {0, 0, 0, 0};
#define CUE_LONG_PRESS_MS 1000
int cue_display_states[4] = {CUE_STATE_EMPTY, CUE_STATE_EMPTY,
                              CUE_STATE_EMPTY, CUE_STATE_EMPTY};

/* ── Cue button processing ─────────────────────────────────────── */
static unsigned long millis_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (unsigned long)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

/* Remap Arduino pins to cue indices: A0→CUE1, A3→CUE2, A1→CUE3, A2→CUE4 */
static const int cue_pin_map[4] = {0, 2, 3, 1}; /* bit index → cue index */

void process_cue_buttons(uint8_t button_byte) {
    unsigned long now = millis_now();

    for (int bit = 0; bit < 4; bit++) {
        int i = cue_pin_map[bit]; /* cue index */
        int cur = (button_byte >> bit) & 1;
        int prev = (last_button_byte >> bit) & 1;

        /* Rising edge - button pressed */
        if (cur && !prev) {
            cue_press_time[i] = now;
        }

        /* Long-press threshold reached while held — set cue immediately */
        if (cur && cue_press_time[i] > 0 &&
            (now - cue_press_time[i]) >= CUE_LONG_PRESS_MS &&
            cue_press_time[i] != 1) {
            cues_set(&deck[1].cues, i, player_get_elapsed(&deck[1].player));
            if (deck[1].player.track && deck[1].player.track->path)
                cues_save_to_file(&deck[1].cues, deck[1].player.track->path);
            cue_display_states[i] = CUE_STATE_SET;
            cue_press_time[i] = 1; /* mark as fired this press */
            printf("Cue %d set\n", i + 1);
        }

        /* Falling edge - button released */
        if (!cur && prev) {
            unsigned long held = (cue_press_time[i] == 1) ? CUE_LONG_PRESS_MS : (now - cue_press_time[i]);
            if (held < CUE_LONG_PRESS_MS) {
                /* Short press: jump to cue if set */
                double pos = cues_get(&deck[1].cues, i);
                if (pos != CUE_UNSET) {
                    player_seek_to(&deck[1].player, pos);
                    /* Move position to cue point and resync encoder */
                    deck[1].player.position = deck[1].player.position - deck[1].player.offset;
                    deck[1].player.offset = 0.0;
                    float platterspeed;
                    get_variable_value("platterspeed", &platterspeed);
                    deck[1].angleOffset = (deck[1].player.position * platterspeed) - deck[1].encoderAngle;
                    deck[1].player.target_position = deck[1].player.position;
                    cue_display_states[i] = CUE_STATE_ACTIVE;
                    printf("Cue %d triggered\n", i + 1);
                }
            }
        }
    }

    last_button_byte = button_byte;
}

/* ── Binary serial read (ring buffer with sync byte scanning) ── */
static void read_serial_data(void) {
    static uint8_t ring[256];
    static int rpos = 0, wpos = 0;

    /* Fill ring buffer */
    uint8_t tmp[64];
    int n = read(serial_fd, tmp, sizeof(tmp));
    if (n > 0) {
        for (int i = 0; i < n; i++)
            ring[wpos++ & 0xFF] = tmp[i];
    }

    /* Scan for valid packets (7 bytes: SYNC fHi fLo cHi cLo buttons checksum) */
    int avail = (wpos - rpos) & 0xFF;
    while (avail >= PKT_LEN) {
        if (ring[rpos & 0xFF] != SYNC_BYTE) {
            rpos++; avail--; continue;
        }

        uint8_t p[PKT_LEN];
        for (int i = 0; i < PKT_LEN; i++)
            p[i] = ring[(rpos + i) & 0xFF];

        uint8_t chk = p[1] ^ p[2] ^ p[3] ^ p[4] ^ p[5];
        if (chk == p[6]) {
            int fader_value = (p[1] << 8) | p[2];
            int cap_value   = (p[3] << 8) | p[4];
            uint8_t btn     = p[5];

            /* Process fader */
            float curveSwitch;
            bool switchFader = false;
            get_variable_value("Fad Switch", &curveSwitch);
            if (curveSwitch > 0.5) switchFader = true;

            ADCs[0] = switchFader ? fader_value : (1023 - fader_value);
            ADCs[1] = switchFader ? (1023 - fader_value) : fader_value;
            ADCs[2] = switchFader ? fader_value : (1023 - fader_value);
            ADCs[3] = switchFader ? (1023 - fader_value) : fader_value;

            /* Process capacitive touch */
            capIsTouched = (cap_value > 5000) ? 1 : 0;

            /* Process cue buttons (edge detection) */
            process_cue_buttons(btn);

            rpos += PKT_LEN;
            avail = (wpos - rpos) & 0xFF;
        } else {
            rpos++; avail--;
        }
    }
}

/* ── Read MT6701 angle and set deck encoder ────────────────────── */
static void read_mt6701_angle(void) {
    int angle = mt6701_read_angle();
    if (angle >= 0) {
        deck[1].newEncoderAngle = angle;
    }
}

void process_pic()
{
    if (ADCs[0] < 0) ADCs[0] = 0;
    if (ADCs[0] > 1023) ADCs[0] = 1023;

    double fader = ADCs[0] / 1023.0;
	float curveFactor;
    get_variable_value("Fad Factor", &curveFactor);
	float curvePower;
    get_variable_value("Fad Power", &curvePower);

    /* Deadzone at extremes: hard-cut below 2% or above 98% */
    if (fader < 0.02) {
        deck[0].player.faderTarget = 0.0;
        deck[1].player.faderTarget = 1.0;
    } else if (fader > 0.98) {
        deck[0].player.faderTarget = 1.0;
        deck[1].player.faderTarget = 0.0;
    } else if (fader <= 0.5) {
        deck[1].player.faderTarget = 1.0;
        deck[0].player.faderTarget = pow(fader / curveFactor, curvePower);
        if (deck[0].player.faderTarget > 1.0) deck[0].player.faderTarget = 1.0;
    } else {
        deck[0].player.faderTarget = 1.0;
        deck[1].player.faderTarget = pow((1.0 - fader) / curveFactor, curvePower);
        if (deck[1].player.faderTarget > 1.0) deck[1].player.faderTarget = 1.0;
    }
}

// Keep a running average of speed so if we suddenly let go it keeps going at that speed
double averageSpeed = 0.0;
unsigned int numBlips = 0;
void process_rot()
{
	int8_t crossedZero;
	int wrappedAngle = 0x0000;

	if (scsettings.jogReverse) {
		deck[1].newEncoderAngle = (ENCODER_CPR - 1) - deck[1].newEncoderAngle;
	}

	// First time, make sure there's no difference
	if (deck[1].encoderAngle == 0xffff)
		deck[1].encoderAngle = deck[1].newEncoderAngle;

	// Handle wrapping at zero (14-bit: quarter = CPR/4)
	if (deck[1].newEncoderAngle < (ENCODER_CPR / 4) && deck[1].encoderAngle >= (ENCODER_CPR * 3 / 4))
	{
		crossedZero = 1;
		wrappedAngle = deck[1].encoderAngle - ENCODER_CPR;
	}
	else if (deck[1].newEncoderAngle >= (ENCODER_CPR * 3 / 4) && deck[1].encoderAngle < (ENCODER_CPR / 4))
	{
		crossedZero = -1;
		wrappedAngle = deck[1].encoderAngle + ENCODER_CPR;
	}
	else
	{
		crossedZero = 0;
		wrappedAngle = deck[1].encoderAngle;
	}

	// Blip threshold (configurable, needs 4x increase for 14-bit vs 12-bit)
	float blipthreshold;
	get_variable_value("blipthreshold", &blipthreshold);
	if (abs(deck[1].newEncoderAngle - wrappedAngle) > (int)blipthreshold && numBlips < 2)
	{
		numBlips++;
	}
	else
	{
		numBlips = 0;
		deck[1].encoderAngle = deck[1].newEncoderAngle;

		if (pitchMode)
		{
			if (!oldPitchMode)
			{
				deck[(pitchMode - 1)].player.note_pitch = 1.0;
				deck[1].angleOffset = -deck[1].encoderAngle;
				oldPitchMode = 1;
				deck[1].player.capTouch = 0;
			}

			if (crossedZero > 0)
			{
				deck[1].angleOffset += ENCODER_CPR;
			}
			else if (crossedZero < 0)
			{
				deck[1].angleOffset -= ENCODER_CPR;
			}

			deck[(pitchMode - 1)].player.note_pitch = (((double)(deck[1].encoderAngle + deck[1].angleOffset)) / (ENCODER_CPR * 4)) + 1.0;
		}
		else
		{
			if (scsettings.platterenabled)
			{
				if (capIsTouched || deck[1].player.motor_speed == 0.0)
				{
					if (!deck[1].player.capTouch || oldPitchMode && !deck[1].player.stopped)
					{
						float platterspeed;
						get_variable_value("platterspeed", &platterspeed);
						deck[1].angleOffset = (deck[1].player.position * platterspeed) - deck[1].encoderAngle;
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

			if (crossedZero > 0)
			{
				deck[1].angleOffset += ENCODER_CPR;
			}
			else if (crossedZero < 0)
			{
				deck[1].angleOffset -= ENCODER_CPR;
			}

				float platterspeed;
			get_variable_value("platterspeed", &platterspeed);
			deck[1].player.target_position = (double)(deck[1].encoderAngle + deck[1].angleOffset) / platterspeed;
		}
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

	/* Init serial (binary protocol, 500k baud) */
	init_serial(SERIAL_DEV);

	/* Init MT6701 hall sensor on I2C */
	if (mt6701_init() < 0) {
		printf("Warning: MT6701 not available, platter disabled\n");
	}

	srand(time(NULL));

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

	while (1) // Main input loop
	{

		frameCount++;

		// Update display every second
		gettimeofday(&tv, NULL);
		if (tv.tv_sec != lastTime)
		{
			lastTime = tv.tv_sec;
			frameCount = 0;

			if (secondCount < scsettings.mididelay)
				secondCount++;
			else if (secondCount == scsettings.mididelay)
			{
				mididevicenum = listdev("rawmidi", mididevices);

				if (mididevicenum > oldmididevicenum)
				{
					AddNewMidiDevices(mididevices, mididevicenum);
					oldmididevicenum = mididevicenum;
				}
				secondCount = 999;
			}
		}

		/* Read serial data (fader + cap + buttons) */
		read_serial_data();

		/* Read MT6701 angle (replaces Arduino encoder) */
		read_mt6701_angle();

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
		else
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

		usleep(scsettings.updaterate);
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
