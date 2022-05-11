#include <sys/param.h>
#include <stdlib.h>
#include <string.h>
#include "gnuboy.h"
#include "hw.h"
#include "cpu.h"


gb_host_t host;


int gnuboy_init(int samplerate, bool stereo, int pixformat, void *vblank_func)
{
	host.snd.samplerate = samplerate;
	host.snd.stereo = stereo;
	host.snd.buffer = malloc(samplerate / 4);
	host.snd.len = samplerate / 8;
	host.snd.pos = 0;
	host.lcd.format = pixformat;
	host.lcd.vblank = vblank_func;
	// gnuboy_reset(true);
	return 0;
}


/*
 * gnuboy_reset is called to initialize the state of the emulated
 * system. It should set cpu registers, hardware registers, etc. to
 * their appropriate values at power up time.
 */
void gnuboy_reset(bool hard)
{
	hw_reset(hard);
	cpu_reset(hard);
}


/*
	Time intervals throughout the code, unless otherwise noted, are
	specified in double-speed machine cycles (2MHz), each unit
	roughly corresponds to 0.477us.

	For CPU each cycle takes 2dsc (0.954us) in single-speed mode
	and 1dsc (0.477us) in double speed mode.

	Although hardware gbc LCDC would operate at completely different
	and fixed frequency, for emulation purposes timings for it are
	also specified in double-speed cycles.

	line = 228 dsc (109us)
	frame (154 lines) = 35112 dsc (16.7ms)
	of which
		visible lines x144 = 32832 dsc (15.66ms)
		vblank lines x10 = 2280 dsc (1.08ms)
*/
void gnuboy_run(bool draw)
{
	host.lcd.enabled = draw;
	host.snd.pos = 0;

	/* FIXME: judging by the time specified this was intended
	to emulate through vblank phase which is handled at the
	end of the loop. */
	cpu_emulate(2280);

	/* Step through visible line scanning phase */
	while (R_LY > 0 && R_LY < 144)
		cpu_emulate(lcd.cycles);

	/* Vblank reached, sync hardware */
	hw_vblank();

	/* LCDC operation stopped */
	if (!(R_LCDC & 0x80))
		cpu_emulate(32832);

	/* Step through vblank phase */
	while (R_LY > 0)
		cpu_emulate(lcd.cycles);
}


void gnuboy_set_pad(uint pad)
{
	if (hw.pad != pad)
	{
		hw_setpad(pad);
	}
}


int gnuboy_load_bios(const char *file)
{
	MESSAGE_INFO("Loading BIOS file: '%s'\n", file);

	if (!(hw.bios = malloc(0x900)))
	{
		MESSAGE_ERROR("Mem alloc failed.\n");
		return -2;
	}

	FILE *fp = fopen(file, "rb");
	if (!fp || fread(hw.bios, 1, 0x900, fp) < 0x100)
	{
		MESSAGE_ERROR("File read failed.\n");
		free(hw.bios);
		hw.bios = NULL;
	}
	fclose(fp);

	return hw.bios ? 0 : -1;
}


void gnuboy_load_bank(int bank)
{
	const size_t BANK_SIZE = 0x4000;
	const size_t OFFSET = bank * BANK_SIZE;

	if (!cart.rombanks[bank])
		cart.rombanks[bank] = malloc(BANK_SIZE);

	if (!cart.romFile)
		return;

	MESSAGE_INFO("loading bank %d.\n", bank);
	while (!cart.rombanks[bank])
	{
		int i = rand() & 0xFF;
		if (cart.rombanks[i])
		{
			MESSAGE_INFO("reclaiming bank %d.\n", i);
			cart.rombanks[bank] = cart.rombanks[i];
			cart.rombanks[i] = NULL;
			break;
		}
	}

	// Load the 16K page
	if (fseek(cart.romFile, OFFSET, SEEK_SET)
		|| !fread(cart.rombanks[bank], BANK_SIZE, 1, cart.romFile))
	{
		MESSAGE_WARN("ROM bank loading failed\n");
		if (!feof(cart.romFile))
			abort(); // This indicates an SD Card failure
	}
}


int gnuboy_load_rom(const char *file)
{
	// Memory Bank Controller names
	const char *mbc_names[16] = {
		"MBC_NONE", "MBC_MBC1", "MBC_MBC2", "MBC_MBC3",
		"MBC_MBC5", "MBC_MBC6", "MBC_MBC7", "MBC_HUC1",
		"MBC_HUC3", "MBC_MMM01", "INVALID", "INVALID",
		"INVALID", "INVALID", "INVALID", "INVALID",
	};
	const char *hw_types[] = {
		"DMG", "CGB", "SGB", "AGB", "???"
	};

	MESSAGE_INFO("Loading file: '%s'\n", file);

	byte header[0x200];

	cart.romFile = fopen(file, "rb");
	if (cart.romFile == NULL)
	{
		MESSAGE_ERROR("ROM fopen failed");
		return -1;
	}

	if (fread(&header, 0x200, 1, cart.romFile) != 1)
	{
		MESSAGE_ERROR("ROM fread failed");
		fclose(cart.romFile);
		return -1;
	}

	int type = header[0x0147];
	int romsize = header[0x0148];
	int ramsize = header[0x0149];

	if (header[0x0143] == 0x80 || header[0x0143] == 0xC0)
		hw.hwtype = GB_HW_CGB; // Game supports CGB mode so we go for that
	else if (header[0x0146] == 0x03)
		hw.hwtype = GB_HW_SGB; // Game supports SGB features
	else
		hw.hwtype = GB_HW_DMG; // Games supports DMG only

	memcpy(&cart.checksum, header + 0x014E, 2);
	memcpy(&cart.name, header + 0x0134, 16);
	cart.name[16] = 0;

	cart.has_battery = (type == 3 || type == 6 || type == 9 || type == 13 || type == 15 ||
						type == 16 || type == 19 || type == 27 || type == 30 || type == 255);
	cart.has_rtc  = (type == 15 || type == 16);
	cart.has_rumble = (type == 28 || type == 29 || type == 30);
	cart.has_sensor = (type == 34);
	cart.colorize = 0;

	if (type >= 1 && type <= 3)
		cart.mbc = MBC_MBC1;
	else if (type >= 5 && type <= 6)
		cart.mbc = MBC_MBC2;
	else if (type >= 11 && type <= 13)
		cart.mbc = MBC_MMM01;
	else if (type >= 15 && type <= 19)
		cart.mbc = MBC_MBC3;
	else if (type >= 25 && type <= 30)
		cart.mbc = MBC_MBC5;
	else if (type == 32)
		cart.mbc = MBC_MBC6;
	else if (type == 34)
		cart.mbc = MBC_MBC7;
	else if (type == 254)
		cart.mbc = MBC_HUC3;
	else if (type == 255)
		cart.mbc = MBC_HUC1;
	else
		cart.mbc = MBC_NONE;

	if (romsize < 9)
	{
		cart.romsize = (2 << romsize);
	}
	else if (romsize > 0x51 && romsize < 0x55)
	{
		cart.romsize = 128; // (2 << romsize) + 64;
	}
	else
	{
		MESSAGE_ERROR("Invalid ROM size: %d\n", romsize);
		return -2;
	}

	if (ramsize < 6)
	{
		const byte ramsize_table[] = {1, 1, 1, 4, 16, 8};
		cart.ramsize = ramsize_table[ramsize];
	}
	else
	{
		MESSAGE_ERROR("Invalid RAM size: %d\n", ramsize);
		cart.ramsize = 1;
	}

	cart.rambanks = malloc(8192 * cart.ramsize);
	if (!cart.rambanks)
	{
		MESSAGE_ERROR("SRAM alloc failed");
		return -3;
	}

	// Detect colorization palette that the real GBC would be using
	if (hw.hwtype != GB_HW_CGB)
	{
		//
		// The following algorithm was adapted from visualboyadvance-m at
		// https://github.com/visualboyadvance-m/visualboyadvance-m/blob/master/src/gb/GB.cpp
		//

		// Title checksums that are treated specially by the CGB boot ROM
		const uint8_t col_checksum[79] = {
			0x00, 0x88, 0x16, 0x36, 0xD1, 0xDB, 0xF2, 0x3C, 0x8C, 0x92, 0x3D, 0x5C,
			0x58, 0xC9, 0x3E, 0x70, 0x1D, 0x59, 0x69, 0x19, 0x35, 0xA8, 0x14, 0xAA,
			0x75, 0x95, 0x99, 0x34, 0x6F, 0x15, 0xFF, 0x97, 0x4B, 0x90, 0x17, 0x10,
			0x39, 0xF7, 0xF6, 0xA2, 0x49, 0x4E, 0x43, 0x68, 0xE0, 0x8B, 0xF0, 0xCE,
			0x0C, 0x29, 0xE8, 0xB7, 0x86, 0x9A, 0x52, 0x01, 0x9D, 0x71, 0x9C, 0xBD,
			0x5D, 0x6D, 0x67, 0x3F, 0x6B, 0xB3, 0x46, 0x28, 0xA5, 0xC6, 0xD3, 0x27,
			0x61, 0x18, 0x66, 0x6A, 0xBF, 0x0D, 0xF4
		};

		// The fourth character of the game title for disambiguation on collision.
		const uint8_t col_disambig_chars[29] = {
			'B', 'E', 'F', 'A', 'A', 'R', 'B', 'E',
			'K', 'E', 'K', ' ', 'R', '-', 'U', 'R',
			'A', 'R', ' ', 'I', 'N', 'A', 'I', 'L',
			'I', 'C', 'E', ' ', 'R'
		};

		// Palette ID | (Flags << 5)
		const uint8_t col_palette_info[94] = {
			0x7C, 0x08, 0x12, 0xA3, 0xA2, 0x07, 0x87, 0x4B, 0x20, 0x12, 0x65, 0xA8,
			0x16, 0xA9, 0x86, 0xB1, 0x68, 0xA0, 0x87, 0x66, 0x12, 0xA1, 0x30, 0x3C,
			0x12, 0x85, 0x12, 0x64, 0x1B, 0x07, 0x06, 0x6F, 0x6E, 0x6E, 0xAE, 0xAF,
			0x6F, 0xB2, 0xAF, 0xB2, 0xA8, 0xAB, 0x6F, 0xAF, 0x86, 0xAE, 0xA2, 0xA2,
			0x12, 0xAF, 0x13, 0x12, 0xA1, 0x6E, 0xAF, 0xAF, 0xAD, 0x06, 0x4C, 0x6E,
			0xAF, 0xAF, 0x12, 0x7C, 0xAC, 0xA8, 0x6A, 0x6E, 0x13, 0xA0, 0x2D, 0xA8,
			0x2B, 0xAC, 0x64, 0xAC, 0x6D, 0x87, 0xBC, 0x60, 0xB4, 0x13, 0x72, 0x7C,
			0xB5, 0xAE, 0xAE, 0x7C, 0x7C, 0x65, 0xA2, 0x6C, 0x64, 0x85
		};

		uint8_t infoIdx = 0;
		uint8_t checksum = 0;

		// Calculate the checksum over 16 title bytes.
		for (int i = 0; i < 16; i++)
		{
			checksum += header[0x0134 + i];
		}

		// Check if the checksum is in the list.
		for (size_t idx = 0; idx < 79; idx++)
		{
			if (col_checksum[idx] == checksum)
			{
				infoIdx = idx;

				// Indexes above 0x40 have to be disambiguated.
				if (idx <= 0x40)
					break;

				// No idea how that works. But it works.
				for (size_t i = idx - 0x41, j = 0; i < 29; i += 14, j += 14) {
					if (header[0x0137] == col_disambig_chars[i]) {
						infoIdx += j;
						break;
					}
				}
				break;
			}
		}

		cart.colorize = col_palette_info[infoIdx];
	}

	MESSAGE_INFO("Cart loaded: name='%s', hw=%s, mbc=%s, romsize=%dK, ramsize=%dK, colorize=%d\n",
		cart.name, hw_types[hw.hwtype], mbc_names[cart.mbc], cart.romsize * 16, cart.ramsize * 8, cart.colorize);

	// Gameboy color games can be very large so we only load 1024K for faster boot
	// Also 4/8MB games do not fully fit, our bank manager takes care of swapping.

	int preload = cart.romsize < 64 ? cart.romsize : 64;

	if (cart.romsize > 64 && (strncmp(cart.name, "RAYMAN", 6) == 0 || strncmp(cart.name, "NONAME", 6) == 0))
	{
		MESSAGE_INFO("Special preloading for Rayman 1/2\n");
		preload = cart.romsize - 40;
	}

	MESSAGE_INFO("Preloading the first %d banks\n", preload);
	for (int i = 0; i < preload; i++)
	{
		gnuboy_load_bank(i);
	}

	// Apply game-specific hacks
	if (strncmp(cart.name, "SIREN GB2 ", 11) == 0 || strncmp(cart.name, "DONKEY KONG", 16) == 0)
	{
		MESSAGE_INFO("HACK: Window offset hack enabled\n");
		lcd.enable_window_offset_hack = 1;
	}

	return 0;
}


void gnuboy_free_rom(void)
{
	for (int i = 0; i < 512; i++)
	{
		if (cart.rombanks[i]) {
			free(cart.rombanks[i]);
			cart.rombanks[i] = NULL;
		}
	}
	free(cart.rambanks);
	cart.rambanks = NULL;

	if (cart.romFile)
	{
		fclose(cart.romFile);
		cart.romFile = NULL;
	}

	if (cart.sramFile)
	{
		fclose(cart.sramFile);
		cart.sramFile = NULL;
	}

	memset(&cart, 0, sizeof(cart));
}


void gnuboy_get_time(int *day, int *hour, int *minute, int *second)
{
	if (day) *day = cart.rtc.d;
	if (hour) *hour = cart.rtc.h;
	if (minute) *minute = cart.rtc.m;
	if (second) *second = cart.rtc.s;
}


void gnuboy_set_time(int day, int hour, int minute, int second)
{
	cart.rtc.d = MIN(MAX(day, 0), 365);
	cart.rtc.h = MIN(MAX(hour, 0), 24);
	cart.rtc.m = MIN(MAX(minute, 0), 60);
	cart.rtc.s = MIN(MAX(second, 0), 60);
	cart.rtc.ticks = 0;
}


int gnuboy_get_hwtype(void)
{
	return hw.hwtype;
}


void gnuboy_set_hwtype(gb_hwtype_t type)
{
	// nothing for now
}


int gnuboy_get_palette(void)
{
	return host.lcd.colorize;
}


void gnuboy_set_palette(gb_palette_t pal)
{
	if (host.lcd.colorize != pal)
	{
		host.lcd.colorize = pal;
		lcd_rebuildpal();
	}
}


/**
 * Save state file format is:
 * GB:
 * 0x0000 - 0x0BFF: svars
 * 0x0CF0 - 0x0CFF: snd.wave
 * 0x0D00 - 0x0DFF: hw.ioregs
 * 0x0E00 - 0x0E80: lcd.pal
 * 0x0F00 - 0x0FFF: lcd.oam
 * 0x1000 - 0x2FFF: RAM
 * 0x3000 - 0x4FFF: VRAM
 * 0x5000 - 0x...:  SRAM
 *
 * GBC:
 * 0x0000 - 0x0BFF: svars
 * 0x0CF0 - 0x0CFF: snd.wave
 * 0x0D00 - 0x0DFF: hw.ioregs
 * 0x0E00 - 0x0EFF: lcd.pal
 * 0x0F00 - 0x0FFF: lcd.oam
 * 0x1000 - 0x8FFF: RAM
 * 0x9000 - 0xCFFF: VRAM
 * 0xD000 - 0x...:  SRAM
 *
 */

#ifndef IS_BIG_ENDIAN
#define LIL(x) (x)
#else
#define LIL(x) ((x<<24)|((x&0xff00)<<8)|((x>>8)&0xff00)|(x>>24))
#endif

#define SAVE_VERSION 0x107

#define I1(s, p) { 1, s, p }
#define I2(s, p) { 2, s, p }
#define I4(s, p) { 4, s, p }
#define END { 0, "\0\0\0\0", 0 }

typedef struct
{
	size_t len;
	char key[4];
	void *ptr;
} svar_t;

typedef struct
{
	void *ptr;
	size_t len;
} sblock_t;

// This is the format used by VBA, maybe others
typedef struct
{
	uint32_t s, m, h, d, flags;
	uint32_t regs[5];
	uint64_t rt;
} srtc_t;

static un32 ver;

static const svar_t svars[] =
{
	I4("GbSs", &ver),

	I2("PC  ", &PC),
	I2("SP  ", &SP),
	I2("BC  ", &BC),
	I2("DE  ", &DE),
	I2("HL  ", &HL),
	I2("AF  ", &AF),

	I4("IME ", &cpu.ime),
	I4("ima ", &cpu.ima),
	I4("spd ", &cpu.double_speed),
	I4("halt", &cpu.halted),
	I4("div ", &cpu.div),
	I4("tim ", &cpu.timer),
	I4("lcdc", &lcd.cycles),
	I4("snd ", &snd.cycles),

	I4("ints", &hw.ilines),
	I4("pad ", &hw.pad),
	I4("hdma", &hw.hdma),
	I4("seri", &hw.serial),

	I4("mbcm", &cart.bankmode),
	I4("romb", &cart.rombank),
	I4("ramb", &cart.rambank),
	I4("enab", &cart.enableram),

	I4("rtcR", &cart.rtc.sel),
	I4("rtcL", &cart.rtc.latch),
	I4("rtcF", &cart.rtc.flags),
	I4("rtcd", &cart.rtc.d),
	I4("rtch", &cart.rtc.h),
	I4("rtcm", &cart.rtc.m),
	I4("rtcs", &cart.rtc.s),
	I4("rtct", &cart.rtc.ticks),
	I1("rtR8", &cart.rtc.regs[0]),
	I1("rtR9", &cart.rtc.regs[1]),
	I1("rtRA", &cart.rtc.regs[2]),
	I1("rtRB", &cart.rtc.regs[3]),
	I1("rtRC", &cart.rtc.regs[4]),

	I4("S1on", &snd.ch[0].on),
	I4("S1p ", &snd.ch[0].pos),
	I4("S1c ", &snd.ch[0].cnt),
	I4("S1ec", &snd.ch[0].encnt),
	I4("S1sc", &snd.ch[0].swcnt),
	I4("S1sf", &snd.ch[0].swfreq),

	I4("S2on", &snd.ch[1].on),
	I4("S2p ", &snd.ch[1].pos),
	I4("S2c ", &snd.ch[1].cnt),
	I4("S2ec", &snd.ch[1].encnt),

	I4("S3on", &snd.ch[2].on),
	I4("S3p ", &snd.ch[2].pos),
	I4("S3c ", &snd.ch[2].cnt),

	I4("S4on", &snd.ch[3].on),
	I4("S4p ", &snd.ch[3].pos),
	I4("S4c ", &snd.ch[3].cnt),
	I4("S4ec", &snd.ch[3].encnt),

	END
};

// Set in the far future for VBA-M support
#define RTC_BASE 1893456000


bool gnuboy_sram_dirty(void)
{
	return cart.sram_dirty != 0;
}

int gnuboy_sram_load(const char *file)
{
	if (!cart.has_battery || !cart.ramsize || !file || !*file)
		return -1;

	FILE *f = fopen(file, "rb");
	if (!f)
		return -2;

	MESSAGE_INFO("Loading SRAM from '%s'\n", file);

	cart.sram_dirty = 0;
	cart.sram_saved = 0;

	for (int i = 0; i < cart.ramsize; i++)
	{
		if (fseek(f, i * 8192, SEEK_SET) == 0 && fread(cart.rambanks[i], 8192, 1, f) == 1)
		{
			MESSAGE_INFO("Loaded SRAM bank %d.\n", i);
			cart.sram_saved = (1 << i);
		}
	}

	if (cart.has_rtc)
	{
		srtc_t rtc_buf;

		if (fseek(f, cart.ramsize * 8192, SEEK_SET) == 0 && fread(&rtc_buf, 48, 1, f) == 1)
		{
			cart.rtc = (gb_rtc_t){
				.s = rtc_buf.s,
				.m = rtc_buf.m,
				.h = rtc_buf.h,
				.d = rtc_buf.d,
				.flags = rtc_buf.flags,
				.regs = {
					rtc_buf.regs[0],
					rtc_buf.regs[1],
					rtc_buf.regs[2],
					rtc_buf.regs[3],
					rtc_buf.regs[4],
				},
			};
			MESSAGE_INFO("Loaded RTC section %03d %02d:%02d:%02d.\n", cart.rtc.d, cart.rtc.h, cart.rtc.m, cart.rtc.s);
		}
	}

	fclose(f);

	return cart.sram_saved ? 0 : -1;
}


/**
 * If quick_save is set to true, gnuboy_sram_save will only save the sectors that
 * changed + the RTC. If set to false then a full sram file is created.
 */
int gnuboy_sram_save(const char *file, bool quick_save)
{
	if (!cart.has_battery || !cart.ramsize || !file || !*file)
		return -1;

	FILE *f = fopen(file, "wb");
	if (!f)
		return -2;

	MESSAGE_INFO("Saving SRAM to '%s'...\n", file);

	// Mark everything as dirty and unsaved (do a full save)
	if (!quick_save)
	{
		cart.sram_dirty = (1 << cart.ramsize) - 1;
		cart.sram_saved = 0;
	}

	for (int i = 0; i < cart.ramsize; i++)
	{
		if (!(cart.sram_saved & (1 << i)) || (cart.sram_dirty & (1 << i)))
		{
			if (fseek(f, i * 8192, SEEK_SET) == 0 && fwrite(cart.rambanks[i], 8192, 1, f) == 1)
			{
				MESSAGE_INFO("Saved SRAM bank %d.\n", i);
				cart.sram_dirty &= ~(1 << i);
				cart.sram_saved |= (1 << i);
			}
		}
	}

	if (cart.has_rtc)
	{
		srtc_t rtc_buf = {
			.s = cart.rtc.s,
			.m = cart.rtc.m,
			.h = cart.rtc.h,
			.d = cart.rtc.d,
			.flags = cart.rtc.flags,
			.regs = {
				cart.rtc.regs[0],
				cart.rtc.regs[1],
				cart.rtc.regs[2],
				cart.rtc.regs[3],
				cart.rtc.regs[4],
			},
			.rt = RTC_BASE + cart.rtc.s + (cart.rtc.m * 60) + (cart.rtc.h * 3600) + (cart.rtc.d * 86400),
		};
		if (fseek(f, cart.ramsize * 8192, SEEK_SET) == 0 && fwrite(&rtc_buf, 48, 1, f) == 1)
		{
			MESSAGE_INFO("Saved RTC section.\n");
		}
	}

	fclose(f);

	return cart.sram_dirty ? -1 : 0;
}


int gnuboy_state_save(const char *file)
{
	void *buf = calloc(1, 4096);
	if (!buf) return -2;

	FILE *fp = fopen(file, "wb");
	if (!fp) goto _error;

	bool is_cgb = hw.hwtype == GB_HW_CGB;

	sblock_t blocks[] = {
		{buf, 1},
		{hw.rambanks, is_cgb ? 8 : 2},
		{lcd.vbank, is_cgb ? 4 : 2},
		{cart.rambanks, cart.ramsize * 2},
		{NULL, 0},
	};

	un32 (*header)[2] = (un32 (*)[2])buf;

	ver = SAVE_VERSION;

	for (int i = 0; svars[i].ptr; i++)
	{
		un32 d = 0;

		switch (svars[i].len)
		{
		case 1:
			d = *(un8 *)svars[i].ptr;
			break;
		case 2:
			d = *(un16 *)svars[i].ptr;
			break;
		case 4:
			d = *(un32 *)svars[i].ptr;
			break;
		}

		header[i][0] = *(un32 *)svars[i].key;
		header[i][1] = LIL(d);
	}

	memcpy(buf+0x0CF0, snd.wave, sizeof snd.wave);
	memcpy(buf+0x0D00, hw.ioregs, sizeof hw.ioregs);
	memcpy(buf+0x0E00, lcd.pal, sizeof lcd.pal);
	memcpy(buf+0x0F00, lcd.oam.mem, sizeof lcd.oam);

	for (int i = 0; blocks[i].ptr != NULL; i++)
	{
		if (fwrite(blocks[i].ptr, 4096, blocks[i].len, fp) < 1)
		{
			MESSAGE_ERROR("Write error in block %d\n", i);
			goto _error;
		}
	}

	fclose(fp);
	free(buf);

	return 0;

_error:
	if (fp) fclose(fp);
	if (buf) free(buf);

	return -1;
}


int gnuboy_state_load(const char *file)
{
	void* buf = calloc(1, 4096);
	if (!buf) return -2;

	FILE *fp = fopen(file, "rb");
	if (!fp) goto _error;

	bool is_cgb = hw.hwtype == GB_HW_CGB;

	sblock_t blocks[] = {
		{buf, 1},
		{hw.rambanks, is_cgb ? 8 : 2},
		{lcd.vbank, is_cgb ? 4 : 2},
		{cart.rambanks, cart.ramsize * 2},
		{NULL, 0},
	};

	for (int i = 0; blocks[i].ptr != NULL; i++)
	{
		if (fread(blocks[i].ptr, 4096, blocks[i].len, fp) < 1)
		{
			MESSAGE_ERROR("Read error in block %d\n", i);
			goto _error;
		}
	}

	un32 (*header)[2] = (un32 (*)[2])buf;

	for (int i = 0; svars[i].ptr; i++)
	{
		un32 d = 0;

		for (int j = 0; header[j][0]; j++)
		{
			if (header[j][0] == *(un32 *)svars[i].key)
			{
				d = LIL(header[j][1]);
				break;
			}
		}

		switch (svars[i].len)
		{
		case 1:
			*(un8 *)svars[i].ptr = d;
			break;
		case 2:
			*(un16 *)svars[i].ptr = d;
			break;
		case 4:
			*(un32 *)svars[i].ptr = d;
			break;
		}
	}

	if (ver != SAVE_VERSION)
		MESSAGE_ERROR("Save file version mismatch!\n");

	memcpy(snd.wave, buf+0x0CF0, sizeof snd.wave);
	memcpy(hw.ioregs, buf+0x0D00, sizeof hw.ioregs);
	memcpy(lcd.pal, buf+0x0E00, sizeof lcd.pal);
	memcpy(lcd.oam.mem, buf+0x0F00, sizeof lcd.oam);

	fclose(fp);
	free(buf);

	// Disable BIOS. This is a hack to support old saves
	R_BIOS = 0x1;

	// Older saves might overflow this
	cart.rambank &= (cart.ramsize - 1);

	lcd_rebuildpal();
	sound_dirty();
	hw_updatemap();

	return 0;

_error:
	if (fp) fclose(fp);
	if (buf) free(buf);

	return -1;
}
