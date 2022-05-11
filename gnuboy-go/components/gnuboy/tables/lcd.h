/**
 * Precomputed LCD tables
 */

#include <stdint.h>

// Our custom colorization palettes
static const uint16_t custom_palettes[][4] = {
	{ 0x6BDD, 0x3ED4, 0x1D86, 0x0860 }, // GB_PALETTE_DEFAULT
	{ 0x7FFF, 0x5AD6, 0x318C, 0x0000 }, // GB_PALETTE_2BGRAYS
	{ 0x5BFF, 0x3F0F, 0x222D, 0x10EB }, // GB_PALETTE_LINKSAW
	{ 0x639E, 0x263A, 0x10D4, 0x2866 }, // GB_PALETTE_NSUPRGB
	{ 0x36D5, 0x260E, 0x1D47, 0x18C4 }, // GB_PALETTE_NGBARNE
	{ 0x6FDF, 0x36DE, 0x4996, 0x34AC }, // GB_PALETTE_GRAPEFR
	{ 0x6739, 0x6E6D, 0x4588, 0x1882 }, // GB_PALETTE_MEGAMAN
	{ 0x7FBF, 0x46DE, 0x4DD0, 0x0843 }, // GB_PALETTE_POKEMON
	{ 0x0272, 0x0DCA, 0x0D45, 0x0102 }, // GB_PALETTE_DMGREEN
};

// Game-specific colorization palettes extracted from GBC's BIOS
static const uint16_t colorization_palettes[32][3][4] = {
	{{0x7FFF, 0x01DF, 0x0112, 0x0000}, {0x7FFF, 0x7EEB, 0x001F, 0x7C00}, {0x7FFF, 0x42B5, 0x3DC8, 0x0000}},
	{{0x231F, 0x035F, 0x00F2, 0x0009}, {0x7FFF, 0x421F, 0x1CF2, 0x0000}, {0x4FFF, 0x7ED2, 0x3A4C, 0x1CE0}},
	{{0x7FFF, 0x7FFF, 0x7E8C, 0x7C00}, {0x7FFF, 0x32BF, 0x00D0, 0x0000}, {0x03ED, 0x7FFF, 0x255F, 0x0000}},
	{{0x7FFF, 0x7FFF, 0x7E8C, 0x7C00}, {0x7FFF, 0x421F, 0x1CF2, 0x0000}, {0x036A, 0x021F, 0x03FF, 0x7FFF}},
	{{0x7FFF, 0x421F, 0x1CF2, 0x0000}, {0x7FFF, 0x421F, 0x1CF2, 0x0000}, {0x7FFF, 0x03EF, 0x01D6, 0x0000}},
	{{0x7FFF, 0x421F, 0x1CF2, 0x0000}, {0x7FFF, 0x7EEB, 0x001F, 0x7C00}, {0x7FFF, 0x03EA, 0x011F, 0x0000}},
	{{0x7FFF, 0x421F, 0x1CF2, 0x0000}, {0x7FFF, 0x7EEB, 0x001F, 0x7C00}, {0x7FFF, 0x027F, 0x001F, 0x0000}},
	{{0x7FFF, 0x7E8C, 0x7C00, 0x0000}, {0x7FFF, 0x7EEB, 0x001F, 0x7C00}, {0x7FFF, 0x03FF, 0x001F, 0x0000}},
	{{0x299F, 0x001A, 0x000C, 0x0000}, {0x7C00, 0x7FFF, 0x3FFF, 0x7E00}, {0x7E74, 0x03FF, 0x0180, 0x0000}},
	{{0x7FFF, 0x01DF, 0x0112, 0x0000}, {0x7FFF, 0x7E8C, 0x7C00, 0x0000}, {0x67FF, 0x77AC, 0x1A13, 0x2D6B}},
	{{0x0000, 0x7FFF, 0x421F, 0x1CF2}, {0x0000, 0x7FFF, 0x421F, 0x1CF2}, {0x7ED6, 0x4BFF, 0x2175, 0x0000}},
	{{0x7FFF, 0x421F, 0x1CF2, 0x0000}, {0x7FFF, 0x3FFF, 0x7E00, 0x001F}, {0x7FFF, 0x7E8C, 0x7C00, 0x0000}},
	{{0x231F, 0x035F, 0x00F2, 0x0009}, {0x7FFF, 0x7EEB, 0x001F, 0x7C00}, {0x7FFF, 0x6E31, 0x454A, 0x0000}},
	{{0x7FFF, 0x421F, 0x1CF2, 0x0000}, {0x7FFF, 0x32BF, 0x00D0, 0x0000}, {0x7FFF, 0x6E31, 0x454A, 0x0000}},
	{{0x7FFF, 0x421F, 0x1CF2, 0x0000}, {0x7FFF, 0x7E8C, 0x7C00, 0x0000}, {0x7FFF, 0x1BEF, 0x0200, 0x0000}},
	{{0x7FFF, 0x7E8C, 0x7C00, 0x0000}, {0x7FFF, 0x1BEF, 0x0200, 0x0000}, {0x7FFF, 0x32BF, 0x00D0, 0x0000}},
	{{0x7FFF, 0x1BEF, 0x0200, 0x0000}, {0x7FFF, 0x7E8C, 0x7C00, 0x0000}, {0x7FFF, 0x421F, 0x1CF2, 0x0000}},
	{{0x7FFF, 0x03E0, 0x0206, 0x0120}, {0x7FFF, 0x7E8C, 0x7C00, 0x0000}, {0x7FFF, 0x421F, 0x1CF2, 0x0000}},
	{{0x7FFF, 0x1BEF, 0x0200, 0x0000}, {0x7FFF, 0x7E8C, 0x7C00, 0x0000}, {0x7FFF, 0x32BF, 0x00D0, 0x0000}},
	{{0x7FFF, 0x421F, 0x1CF2, 0x0000}, {0x7FFF, 0x1BEF, 0x0200, 0x0000}, {0x0000, 0x4200, 0x037F, 0x7FFF}},
	{{0x03FF, 0x001F, 0x000C, 0x0000}, {0x7FFF, 0x1BEF, 0x0200, 0x0000}, {0x7FFF, 0x7E8C, 0x7C00, 0x0000}},
	{{0x7FFF, 0x32BF, 0x00D0, 0x0000}, {0x7FFF, 0x7E8C, 0x7C00, 0x0000}, {0x7FFF, 0x42B5, 0x3DC8, 0x0000}},
	{{0x7FFF, 0x5294, 0x294A, 0x0000}, {0x7FFF, 0x5294, 0x294A, 0x0000}, {0x7FFF, 0x5294, 0x294A, 0x0000}},
	{{0x7FFF, 0x1BEF, 0x0200, 0x0000}, {0x7FFF, 0x7E8C, 0x7C00, 0x0000}, {0x53FF, 0x4A5F, 0x7E52, 0x0000}},
	{{0x7FFF, 0x421F, 0x1CF2, 0x0000}, {0x7FFF, 0x1BEF, 0x0200, 0x0000}, {0x7FFF, 0x7E8C, 0x7C00, 0x0000}},
	{{0x7FFF, 0x32BF, 0x00D0, 0x0000}, {0x7FFF, 0x32BF, 0x00D0, 0x0000}, {0x639F, 0x4279, 0x15B0, 0x04CB}},
	{{0x7FFF, 0x7E8C, 0x7C00, 0x0000}, {0x7FFF, 0x1BEF, 0x0200, 0x0000}, {0x7FFF, 0x03FF, 0x012F, 0x0000}},
	{{0x7FFF, 0x033F, 0x0193, 0x0000}, {0x7FFF, 0x033F, 0x0193, 0x0000}, {0x7FFF, 0x033F, 0x0193, 0x0000}},
	{{0x7FFF, 0x421F, 0x1CF2, 0x0000}, {0x7FFF, 0x7E8C, 0x7C00, 0x0000}, {0x7FFF, 0x1BEF, 0x6180, 0x0000}},
	{{0x2120, 0x8022, 0x8281, 0x1110}, {0xFF7F, 0xDF7F, 0x1201, 0x0001}, {0xFF00, 0xFF7F, 0x1F03, 0x0000}},
	{{0x7FFF, 0x32BF, 0x00D0, 0x0000}, {0x7FFF, 0x32BF, 0x00D0, 0x0000}, {0x7FFF, 0x32BF, 0x00D0, 0x0000}},
	{{0x7FFF, 0x32BF, 0x00D0, 0x0000}, {0x7FFF, 0x32BF, 0x00D0, 0x0000}, {0x7FFF, 0x32BF, 0x00D0, 0x0000}}
};
