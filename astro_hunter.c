#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "lib/SMSlib.h"
#include "lib/PSGlib.h"
#include "actor.h"
#include "shots.h"
#include "map.h"
#include "data.h"

#define PLAYER_TOP (0)
#define PLAYER_LEFT (0)
#define PLAYER_RIGHT (256 - 16)
#define PLAYER_BOTTOM (SCREEN_H - 16)
#define PLAYER_SPEED (3)

#define ENEMY_MAX (3)
#define FOR_EACH_ENEMY(enm) enm = enemies; for (int i = ENEMY_MAX; i; i--, enm++)
	
#define POWERUP_BASE_TILE (100)
#define POWERUP_LIGHTINING_TILE (POWERUP_BASE_TILE)
#define POWERUP_FIRE_TILE (POWERUP_BASE_TILE + 8)
#define POWERUP_WIND_TILE (POWERUP_BASE_TILE + 16)
#define POWERUP_NONE_TILE (POWERUP_BASE_TILE + 24)
#define POWERUP_LIGHTINING (1)
#define POWERUP_FIRE (2)
#define POWERUP_WIND (3)

actor player;
actor enemies[ENEMY_MAX];

struct ply_ctl {
	char shot_delay;
	char shot_type;
	char pressed_shot_selection;
	
	char powerup1, powerup2;
	char powerup1_active, powerup2_active;

	char death_delay;
} ply_ctl;

struct enemy_spawner {
	char type;
	char x;
	char flags;
	char delay;
	char next;
	path_step *path;
	char all_dead;
} enemy_spawner;

void load_standard_palettes() {
	SMS_loadBGPalette(sprites_palette_bin);
	SMS_loadSpritePalette(sprites_palette_bin);
	SMS_setSpritePaletteColor(0, 0);
}

void handle_player_input() {
	static unsigned char joy;	
	joy = SMS_getKeysStatus();

	if (joy & PORT_A_KEY_LEFT) {
		if (player.x > PLAYER_LEFT) player.x -= PLAYER_SPEED;
	} else if (joy & PORT_A_KEY_RIGHT) {
		if (player.x < PLAYER_RIGHT) player.x += PLAYER_SPEED;
	}

	if (joy & PORT_A_KEY_UP) {
		if (player.y > PLAYER_TOP) player.y -= PLAYER_SPEED;
	} else if (joy & PORT_A_KEY_DOWN) {
		if (player.y < PLAYER_BOTTOM) player.y += PLAYER_SPEED;
	}
	
	if (joy & PORT_A_KEY_2) {
		if (!ply_ctl.shot_delay) {
			if (fire_player_shot(&player, ply_ctl.shot_type)) {
				ply_ctl.shot_delay = player_shot_infos[ply_ctl.shot_type].firing_delay;
			}
		}
	}
	
	if (ply_ctl.shot_delay) ply_ctl.shot_delay--;
	if (ply_ctl.death_delay) ply_ctl.death_delay--;

}

void draw_player() {
	if (!(ply_ctl.death_delay & 0x08)) draw_actor(&player);
}

char is_colliding_against_player(actor *_act) {
	static actor *act;
	static int act_x, act_y;
	
	act = _act;
	act_x = act->x;
	act_y = act->y;
	
	if (player.x > act_x - 12 && player.x < act_x + 12 &&
		player.y > act_y - 12 && player.y < act_y + 12) {
		return 1;
	}
	
	return 0;
}

void init_enemies() {
	static actor *enm;

	enemy_spawner.x = 0;	
	enemy_spawner.delay = 0;
	enemy_spawner.next = 0;
	
	FOR_EACH_ENEMY(enm) {
		enm->active = 0;
	}
}

void handle_enemies() {
	static actor *enm, *sht;	
	
	if (enemy_spawner.delay) {
		enemy_spawner.delay--;
	} else if (enemy_spawner.next != ENEMY_MAX) {
		if (!enemy_spawner.x) {
			enemy_spawner.type = rand() & 1;
			enemy_spawner.x = 8 + rand() % 124;
			enemy_spawner.flags = 0;
			enemy_spawner.path = (path_step *) path1_path;
			if (rand() & 1) {
				enemy_spawner.x += 124;
				enemy_spawner.flags |= PATH_FLIP_X;
			}
		}
		
		enm = enemies + enemy_spawner.next;
		
		init_actor(enm, enemy_spawner.x, 0, 2, 1, 66, 1);
		enm->path_flags = enemy_spawner.flags;
		enm->path = enemy_spawner.path;
		enm->state = enemy_spawner.type;

		enemy_spawner.delay = 10;
		enemy_spawner.next++;
	}
	
	enemy_spawner.all_dead = 1;
	FOR_EACH_ENEMY(enm) {
		move_actor(enm);
		
		if (enm->x < -32 || enm->x > 287 || enm->y < -16 || enm->y > 192) {
			enm->active = 0;
		}

		if (enm->active) {
			sht = check_collision_against_shots(enm);
			if (sht) {
				sht->active = 0;
				enm->active = 0;
			}
			
			if (!ply_ctl.death_delay && is_colliding_against_player(enm)) {
				enm->active = 0;
				ply_ctl.death_delay = 60;
			}
		}
		
		if (enm->active) enemy_spawner.all_dead = 0;
	}	
	
	if (enemy_spawner.all_dead) {
		enemy_spawner.x = 0;
		enemy_spawner.next = 0;
	}
}

void draw_enemies() {
	static actor *enm;
	
	FOR_EACH_ENEMY(enm) {
		draw_actor(enm);
	}
}

void draw_background() {
	unsigned int *ch = background_tilemap_bin;
	
	SMS_setNextTileatXY(0, 0);
	for (char y = 0; y != 30; y++) {
		// Repeat pattern every two lines
		if (!(y & 0x01)) {
			ch = background_tilemap_bin;
		}
		
		for (char x = 0; x != 32; x++) {
			unsigned int tile_number = *ch + 256;
			SMS_setTile(tile_number);
			ch++;
		}
	}
}

void main() {	
	SMS_useFirstHalfTilesforSprites(1);
	SMS_setSpriteMode(SPRITEMODE_TALL);
	SMS_VDPturnOnFeature(VDPFEATURE_HIDEFIRSTCOL);

	SMS_displayOff();
	SMS_loadPSGaidencompressedTiles(sprites_tiles_psgcompr, 0);
	SMS_loadPSGaidencompressedTiles(tileset_tiles_psgcompr, 256);
	load_standard_palettes();
	
	init_map(level1_bin);
	draw_map_screen();

	SMS_displayOn();
	
	init_actor(&player, 116, PLAYER_BOTTOM - 16, 3, 1, 2, 4);
	player.animation_delay = 20;
	ply_ctl.shot_delay = 0;
	ply_ctl.shot_type = 0;
	ply_ctl.powerup1 = 1;
	ply_ctl.powerup2 = 0;
	ply_ctl.powerup1_active = 1;
	ply_ctl.powerup2_active = 0;
	ply_ctl.death_delay = 0;

	init_enemies();
	init_player_shots();
	
	while (1) {	
		handle_player_input();
		handle_enemies();
		handle_icons();
		handle_player_shots();
	
		SMS_initSprites();

		draw_player();
		draw_enemies();
		draw_player_shots();		
		
		SMS_finalizeSprites();
		SMS_waitForVBlank();
		SMS_copySpritestoSAT();
		
		// Scroll two lines per frame
		draw_map();		
		draw_map();		
	}
}

SMS_EMBED_SEGA_ROM_HEADER(9999,0); // code 9999 hopefully free, here this means 'homebrew'
SMS_EMBED_SDSC_HEADER(0,1, 2021,11,27, "Haroldo-OK\\2021", "Astro Hunter",
  "A space shoot-em-up.\n"
  "Originally made for the Lost Cartridge Jam 2021 - https://itch.io/jam/lostcartridgejam3\n"
  "Built using devkitSMS & SMSlib - https://github.com/sverx/devkitSMS");