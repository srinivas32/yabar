/*
 * Yabar - A modern and lightweight status bar for X window managers.
 *
 * Copyright (c) 2016, George Badawi
 * See LICENSE for more information.
 *
 */

#include "yabar.h"

void ya_int_date(ya_block_t * blk);
void ya_int_uptime(ya_block_t * blk);
void ya_int_memory(ya_block_t * blk);
void ya_int_thermal(ya_block_t *blk);
void ya_int_brightness(ya_block_t *blk);
void ya_int_bandwidth(ya_block_t *blk);
void ya_int_cpu(ya_block_t *blk);
void ya_int_loadavg(ya_block_t *blk);
void ya_int_diskio(ya_block_t *blk);
void ya_int_network(ya_block_t *blk);
void ya_int_battery(ya_block_t *blk);
void ya_int_volume(ya_block_t *blk);
void ya_int_wifi(ya_block_t *blk);
void ya_int_diskspace(ya_block_t *blk);

struct reserved_blk ya_reserved_blks[YA_INTERNAL_LEN] = {
	{"YABAR_DATE", ya_int_date},
	{"YABAR_UPTIME", ya_int_uptime},
	{"YABAR_THERMAL", ya_int_thermal},
	{"YABAR_BRIGHTNESS", ya_int_brightness},
	{"YABAR_BANDWIDTH", ya_int_bandwidth},
	{"YABAR_MEMORY", ya_int_memory},
	{"YABAR_CPU", ya_int_cpu},
	{"YABAR_LOADAVG", ya_int_loadavg},
	{"YABAR_DISKIO", ya_int_diskio},
	{"YABAR_NETWORK", ya_int_network},
	{"YABAR_BATTERY", ya_int_battery},
	{"YABAR_VOLUME", ya_int_volume},
	{"YABAR_WIFI", ya_int_wifi},
	{"YABAR_DISKSPACE", ya_int_diskspace},
#ifdef YA_INTERNAL_EWMH
	{"YABAR_TITLE", NULL},
	{"YABAR_WORKSPACE", NULL}
#endif
};

//#define YA_INTERNAL

#ifdef YA_INTERNAL

#include <stdarg.h>

/* generic function to print error messages from blocks and exit */
void ya_block_error(ya_block_t *blk, char *error_msg, ...) {
	va_list args;
	if(blk == NULL)
		return;

	strcpy(blk->buf, "BLOCK ERROR!");
	ya_draw_pango_text(blk);

	fprintf(stderr, "E: %s.%s: ", blk->bar->name, blk->name);
	va_start(args, error_msg);
	fprintf(stderr, error_msg, args);
	va_end(args);
	fprintf(stderr, "\n");

	fflush(stderr);

	pthread_detach(blk->thread);
	pthread_exit(NULL);
}

/* generic function to print warning messages from blocks (non-fatal) */
void ya_block_warning(ya_block_t *blk, char *error_msg, ...) {
	va_list args;
	if(blk == NULL)
		return;

	fprintf(stderr, "W: %s.%s: ", blk->bar->name, blk->name);
	va_start(args, error_msg);
	fprintf(stderr, error_msg, args);
	va_end(args);
	fprintf(stderr, "\n");

	fflush(stderr);
}



FILE* ya_fopen(char* fpath, ya_block_t *blk) {
	FILE* tfile = fopen(fpath, "r");
	if (tfile == NULL)
		ya_block_error(blk, "Failed to open file %s", fpath);

	return tfile;
}

int vya_fscanf(char* fpath, ya_block_t *blk, char *fmt, va_list args) {
	int ret;
	FILE* tfile = ya_fopen(fpath, blk);
	ret = vfscanf(tfile, fmt, args);

	if (ferror(tfile))
		fprintf(stderr, "Error getting values from file (%s)\n", fpath);
	fclose(tfile);
	return ret;
}

int ya_fscanf(char* fpath, ya_block_t *blk, char *fmt, ...) {
	int ret;
	va_list args;
	va_start(args, fmt);
	ret = vya_fscanf(fpath, blk, fmt, args);
	va_end(args);
	return ret;
}

inline void ya_setup_prefix_suffix(ya_block_t *blk, size_t * prflen, size_t *suflen, char **startstr) {
	if(blk->internal->prefix) {
		*prflen = strlen(blk->internal->prefix);
		if(*prflen) {
			strcpy(blk->buf, blk->internal->prefix);
			*startstr += *prflen;
		}
	}
	if(blk->internal->suffix) {
		*suflen = strlen(blk->internal->suffix);
	}
}

#include <time.h>
void ya_int_date(ya_block_t * blk) {
	time_t rawtime;
	struct tm * ya_tm;
	char *startstr = blk->buf;
	size_t prflen=0,suflen=0;
	ya_setup_prefix_suffix(blk, &prflen, &suflen, &startstr);
	if(blk->internal->option[0]==NULL) {
		blk->internal->option[0] = "%c";
	}
	while(1) {
		time(&rawtime);
		ya_tm = localtime(&rawtime);
		strftime(startstr, 100, blk->internal->option[0], ya_tm);
		if(suflen)
			strcat(blk->buf, blk->internal->suffix);
		ya_draw_pango_text(blk);
		sleep(blk->sleep);
	}
}

#include <sys/sysinfo.h>

void ya_int_uptime(ya_block_t *blk) {
	struct sysinfo ya_sysinfo;
	long hr, tmp;
	uint8_t min;
	char *startstr = blk->buf;
	size_t prflen=0,suflen=0;
	ya_setup_prefix_suffix(blk, &prflen, &suflen, &startstr);
	while(1) {
		sysinfo(&ya_sysinfo);
		tmp = ya_sysinfo.uptime;
		hr = tmp/3600;
		tmp %= 3600;
		min = tmp/60;
		//tmp %= 60;
		sprintf(startstr, "%ld:%02d", hr, min);
		if(suflen)
			strcat(blk->buf, blk->internal->suffix);
		ya_draw_pango_text(blk);
		sleep(blk->sleep);
	}
}


void ya_int_thermal(ya_block_t *blk) {
	int temp, wrntemp, crttemp;
	uint32_t wrnbg, wrnfg; //warning colors
	uint32_t crtbg, crtfg; //critical colors
	char *startstr = blk->buf;
	size_t prflen=0,suflen=0;
	ya_setup_prefix_suffix(blk, &prflen, &suflen, &startstr);
	uint8_t space = blk->internal->spacing ? 3 : 0;
	char fpath[128];
	snprintf(fpath, 128, "/sys/class/thermal/%s/temp", blk->internal->option[0]);

	if((blk->internal->option[1]==NULL) ||
			(sscanf(blk->internal->option[1], "%d %x %x", &crttemp, &crtfg, &crtbg)!=3)) {
		crttemp = 70;
		crtbg = 0xFFED303C;
		crtfg = blk->fgcolor;
	}
	if((blk->internal->option[2]==NULL) ||
			(sscanf(blk->internal->option[2], "%d %x %x", &wrntemp, &wrnfg, &wrnbg)!=3)) {
		wrntemp = 58;
		wrnbg = 0xFFF4A345;
		wrnfg = blk->fgcolor;
	}
	while (1) {
		if(ya_fscanf(fpath, blk, "%d", &temp) == 0)
			ya_block_error(blk, "Reading %s failed", fpath);

		temp/=1000;

#ifdef YA_DYN_COL
		if(temp > crttemp) {
			blk->bgcolor = crtbg;
			xcb_change_gc(ya.c, blk->gc, XCB_GC_FOREGROUND, (const uint32_t[]){crtbg});
			blk->attr |= BLKA_DIRTY_COL;
			blk->fgcolor = crtfg;
		}
		else if (temp > wrntemp) {
			blk->bgcolor = wrnbg;
			xcb_change_gc(ya.c, blk->gc, XCB_GC_FOREGROUND, (const uint32_t[]){wrnbg});
			blk->attr |= BLKA_DIRTY_COL;
			blk->fgcolor = wrnfg;
		}
		else {
			blk->bgcolor = blk->bgcolor_old;
			xcb_change_gc(ya.c, blk->gc, XCB_GC_FOREGROUND, (const uint32_t[]){blk->bgcolor});
			blk->attr &= ~BLKA_DIRTY_COL;
			blk->fgcolor = blk->fgcolor_old;
		}
#endif //YA_DYN_COL

		sprintf(startstr, "%*d", space, temp);
		if(suflen)
			strcat(blk->buf, blk->internal->suffix);
		ya_draw_pango_text(blk);
		sleep(blk->sleep);
	}

}

void ya_int_brightness(ya_block_t *blk) {
	int bright;
	char *startstr = blk->buf;
	size_t prflen=0,suflen=0;
	ya_setup_prefix_suffix(blk, &prflen, &suflen, &startstr);
	uint8_t space = blk->internal->spacing ? 3 : 0;
	char fpath[128];
	snprintf(fpath, 128, "/sys/class/backlight/%s/brightness", blk->internal->option[0]);
	while(1) {
		if(ya_fscanf(fpath, blk, "%d", &bright) == 0)
			ya_block_error(blk, "Reading %s failed", fpath);

		sprintf(startstr, "%*d", space, bright);
		if(suflen)
			strcat(blk->buf, blk->internal->suffix);
		ya_draw_pango_text(blk);
		sleep(blk->sleep);
	}
}

void ya_int_bandwidth(ya_block_t * blk) {
	unsigned long rx, tx, orx, otx;
	unsigned int rxrate, txrate;
	char rxpath[128];
	char txpath[128];
	char rxc, txc;
	char *startstr = blk->buf;
	size_t prflen=0,suflen=0;
	char dnstr[20], upstr[20];
	char ifname[16];
	uint8_t space = blk->internal->spacing ? 4 : 0;
	ya_setup_prefix_suffix(blk, &prflen, &suflen, &startstr);
	if ( strncmp(blk->internal->option[0], "default", 7) == 0 ) {
		FILE *routefile;
		char line[100], *r_ifname, *r_dest;
		routefile = fopen("/proc/net/route", "r");
		while ( fgets(line, 100, routefile) != NULL ) {
			r_ifname = strtok(line, "\t");
			r_dest = strtok(NULL, "\t");
			if ( r_ifname != NULL && r_dest != NULL) {
				if ( strncmp(r_dest, "00000000", 8) == 0 ) {
					strncpy(ifname, r_ifname, 16);
				}
			}
		}
		fclose(routefile);
	} else {
		strncpy(ifname, blk->internal->option[0], 16);
	}
	snprintf(rxpath, 128, "/sys/class/net/%s/statistics/rx_bytes", ifname);
	snprintf(txpath, 128, "/sys/class/net/%s/statistics/tx_bytes", ifname);
	if(blk->internal->option[1]) {
		sscanf(blk->internal->option[1], "%s %s", dnstr, upstr);
	}
	ya_fscanf(rxpath, blk, "%lu", &orx);
	ya_fscanf(txpath, blk, "%lu", &otx);
	while(1) {
		txc = rxc = 'K';
		ya_fscanf(rxpath, blk, "%lu", &rx);
		ya_fscanf(txpath, blk, "%lu", &tx);

		rxrate = (rx - orx)/((blk->sleep)*1024);
		txrate = (tx - otx)/((blk->sleep)*1024);

		if(rxrate > 1024) {
			rxrate/=1024;
			rxc = 'M';
		}
		if(txrate > 1024) {
			txrate/=1024;
			txc = 'M';
		}

		orx = rx;
		otx = tx;

		sprintf(startstr, "%s%*u%c %s%*u%c", dnstr, space, rxrate, rxc, upstr, space, txrate, txc);
		if(suflen)
			strcat(blk->buf, blk->internal->suffix);
		ya_draw_pango_text(blk);
		sleep(blk->sleep);
	}

}

void ya_int_memory(ya_block_t *blk) {
	unsigned long total, free, cached, buffered;
	float used;
	FILE *tfile;
	char tline[50];
	char unit;
	char *startstr = blk->buf;
	size_t prflen=0,suflen=0;
	uint8_t space = blk->internal->spacing ? 6 : 0;
	ya_setup_prefix_suffix(blk, &prflen, &suflen, &startstr);
	tfile = ya_fopen("/proc/meminfo", blk);
	fclose(tfile);
	while(1) {
		tfile = fopen("/proc/meminfo", "r");
		while(fgets(tline, 50, tfile) != NULL) {
			sscanf(tline, "MemTotal: %lu kB\n", &total);
			sscanf(tline, "MemFree: %lu kB\n", &free);
			sscanf(tline, "Buffers: %lu kB\n", &buffered);
			sscanf(tline, "Cached: %lu kB\n", &cached);
		}
		used = (float)(total - free - buffered - cached)/1024.0;
		unit = 'M';
		if(((int)used)>1024) {
			used = used/1024.0;
			unit = 'G';
		}
		sprintf(startstr, "%*.1f%c", space, used, unit);
		if(suflen)
			strcat(blk->buf, blk->internal->suffix);
		ya_draw_pango_text(blk);
		fclose(tfile);
		sleep(blk->sleep);
	}
}


void ya_int_cpu(ya_block_t *blk) {
	char fpath[] = "/proc/stat";
	long double old[4], cur[4], ya_avg=0.0;
	char *startstr = blk->buf;
	size_t prflen=0,suflen=0;
	char cpustr[20];
	uint8_t space = blk->internal->spacing ? 5 : 0;
#ifdef YA_DYN_COL
	long double crttemp;
	uint32_t crtbg, crtfg;
	if((blk->internal->option[1]==NULL) ||
			(sscanf(blk->internal->option[1], "%Lf %x %x", &crttemp, &crtfg, &crtbg)!=3)) {
		crttemp = 80.0;
		crtbg = 0xFFED303C;
		crtfg = blk->fgcolor;
	}
#endif
	ya_setup_prefix_suffix(blk, &prflen, &suflen, &startstr);
	ya_fscanf(fpath, blk, "%s %Lf %Lf %Lf %Lf",cpustr, &old[0],&old[1],&old[2],&old[3]);
	while(1) {
		if(ya_fscanf(fpath, blk, "%s %Lf %Lf %Lf %Lf",
					 cpustr, &cur[0], &cur[1], &cur[2], &cur[3]) == 0)
			ya_block_error(blk, "Reading %s failed", fpath);

		ya_avg = ((cur[0]+cur[1]+cur[2]) - (old[0]+old[1]+old[2])) / ((cur[0]+cur[1]+cur[2]+cur[3]) - (old[0]+old[1]+old[2]+old[3]));
		for(int i=0; i<4;i++)
			old[i]=cur[i];
		ya_avg *= 100.0;
#ifdef YA_DYN_COL
		if(ya_avg > crttemp) {
			blk->bgcolor = crtbg;
			xcb_change_gc(ya.c, blk->gc, XCB_GC_FOREGROUND, (const uint32_t[]){crtbg});
			blk->attr |= BLKA_DIRTY_COL;
			blk->fgcolor = crtfg;
		}
		else {
			blk->bgcolor = blk->bgcolor_old;
			xcb_change_gc(ya.c, blk->gc, XCB_GC_FOREGROUND, (const uint32_t[]){blk->bgcolor});
			blk->attr &= ~BLKA_DIRTY_COL;
			blk->fgcolor = blk->fgcolor_old;
		}
#endif
		sprintf(startstr, "%*.1Lf", space, ya_avg);
		if(suflen)
			strcat(blk->buf, blk->internal->suffix);
		ya_draw_pango_text(blk);
		sleep(blk->sleep);
	}
}

void ya_int_loadavg(ya_block_t *blk) {
	int avg = 0;
	char fpath[] = "/proc/loadavg";
	FILE *tfile;
	long double cur[4], ya_avg = 0.0;
	char *startstr = blk->buf;
	size_t prflen = 0,suflen = 0;
	char avgstr[20];
	ya_setup_prefix_suffix(blk, &prflen, &suflen, &startstr);
	uint8_t space = blk->internal->spacing ? 3 : 0;
#ifdef YA_DYN_COL
	long double crttemp;
	uint32_t crtbg, crtfg;
	if(blk->internal->option[0]) {
		avg = atoi(blk->internal->option[0]);
		switch (avg) {
			case 15:
				avg = 2;
				break;
			case 5:
				avg = 1;
				break;
			default:
				avg = 0;
				break;
		}
	}
	if((blk->internal->option[1]==NULL) ||
			(sscanf(blk->internal->option[1], "%Lf %x %x", &crttemp, &crtfg, &crtbg)!=3)) {
		crttemp = 1.0;
		crtbg = 0xFFED303C;
		crtfg = blk->fgcolor;
	}
#endif
	while(1) {
		tfile = fopen(fpath, "r");
		if(fscanf(tfile,"%Lf %Lf %Lf %s %Lf",
				  &cur[0], &cur[1], &cur[2], avgstr, &cur[3]) != 5)
			ya_block_error(blk, "Getting values from %s failed", fpath);

		ya_avg = cur[avg];
#ifdef YA_DYN_COL
		if(ya_avg > crttemp) {
			blk->bgcolor = crtbg;
			xcb_change_gc(ya.c, blk->gc, XCB_GC_FOREGROUND, (const uint32_t[]){crtbg});
			blk->attr |= BLKA_DIRTY_COL;
			blk->fgcolor = crtfg;
		}
		else {
			blk->bgcolor = blk->bgcolor_old;
			xcb_change_gc(ya.c, blk->gc, XCB_GC_FOREGROUND, (const uint32_t[]){blk->bgcolor});
			blk->attr &= ~BLKA_DIRTY_COL;
			blk->fgcolor = blk->fgcolor_old;
		}
#endif
		sprintf(startstr, "%*.2Lf", space, ya_avg);
		if(suflen)
			strcat(blk->buf, blk->internal->suffix);
		ya_draw_pango_text(blk);
		fclose(tfile);
		sleep(blk->sleep);
	}
}


void ya_int_diskio(ya_block_t *blk) {
	unsigned long tdo[11], tdc[11];
	unsigned long drd=0, dwr=0;
	char crd, cwr;
	char tpath[100];
	char *startstr = blk->buf;
	size_t prflen=0,suflen=0;
	char dnstr[20], upstr[20];
	uint8_t space = blk->internal->spacing ? 4 : 0;
	ya_setup_prefix_suffix(blk, &prflen, &suflen, &startstr);
	if(blk->internal->option[1]) {
		sscanf(blk->internal->option[1], "%s %s", dnstr, upstr);
	}
	snprintf(tpath, 100, "/sys/class/block/%s/stat", blk->internal->option[0]);

	if(ya_fscanf(tpath, blk, "%lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu", &tdo[0], &tdo[1], &tdo[2], &tdo[3], &tdo[4], &tdo[5], &tdo[6], &tdo[7], &tdo[8], &tdo[9], &tdo[10]) == 0) {
		   ya_block_error(blk, "Getting values from %s failed", tpath);
	   }

	while(1) {
		if(ya_fscanf(tpath, blk, "%lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu",
					 &tdc[0], &tdc[1], &tdc[2], &tdc[3], &tdc[4], &tdc[5], &tdc[6], &tdc[7], &tdc[8], &tdc[9], &tdc[10]) == 0)
		   ya_block_error(blk, "Getting values from %s failed", tpath);

		drd = (unsigned long)(((float)(tdc[2] - tdo[2])*0.5)/((float)(blk->sleep)));
		dwr = (unsigned long)(((float)(tdc[6] - tdo[6])*0.5)/((float)(blk->sleep)));
		crd = cwr = 'K';
		if(drd >1024) {
			drd /= 1024;
			crd = 'M';
		}
		if(dwr >1024) {
			dwr /= 1024;
			cwr = 'M';
		}
		sprintf(startstr, "%s%*lu%c %s%*lu%c", dnstr, space, drd, crd, upstr, space, dwr, cwr);
		for(int i=0; i<11;i++)
			tdo[i] = tdc[i];
		if(suflen)
			strcat(blk->buf, blk->internal->suffix);

		ya_draw_pango_text(blk);
		sleep(blk->sleep);
	}

}

void ya_int_battery(ya_block_t *blk) {
	int bat;
	char stat;
	char bat_25str[20], bat_50str[20], bat_75str[20], bat_100str[20], bat_chargestr[20];
	char *startstr = blk->buf;
	size_t prflen=0,suflen=0;
	ya_setup_prefix_suffix(blk, &prflen, &suflen, &startstr);
	uint8_t space = blk->internal->spacing ? 3 : 0;
	char cpath[128], spath[128];
	snprintf(cpath, 128, "/sys/class/power_supply/%s/capacity", blk->internal->option[0]);
	snprintf(spath, 128, "/sys/class/power_supply/%s/status", blk->internal->option[0]);
	if(blk->internal->option[1]) {
		sscanf(blk->internal->option[1], "%s %s %s %s %s", bat_25str, bat_50str, bat_75str, bat_100str, bat_chargestr);
	}

	while(1) {
		if(ya_fscanf(cpath, blk, "%d", &bat) == 0)
			ya_block_error(blk, "Getting values from %s failed", cpath);
		if(ya_fscanf(spath, blk, "%c", &stat) == 0)
			ya_block_error(blk, "Getting values from %s failed", cpath);

		if(bat <= 25)
			strcpy(startstr, bat_25str);
		else if(bat <= 50)
			strcpy(startstr, bat_50str);
		else if(bat <= 75)
			strcpy(startstr, bat_75str);
		else
			strcpy(startstr, bat_100str);
		if(stat == 'C' && blk->internal->option[1])
			strcat(strcat(startstr, " "), bat_chargestr);
		sprintf(startstr+strlen(startstr), "%*d", space, bat);
		if(suflen)
			strcat(blk->buf, blk->internal->suffix);
		ya_draw_pango_text(blk);
		sleep(blk->sleep);
	}
}

#include <alsa/asoundlib.h>
#include <math.h>
#define MAX_LINEAR_DB_SCALE 24
#define exp10(x) (exp((x) * log(10)))
void ya_int_volume(ya_block_t *blk) {
	bool mapped = false;
	char *startstr = blk->buf;
	size_t prflen = 0, suflen = 0;
	ya_setup_prefix_suffix(blk, &prflen, &suflen, &startstr);
	char on[20], off[20], device[64], mixer_name[64];
	int mixer_index = 0, avg_vol = 0, ret = 0, pb_switch = 0;
	long range_min = 0, range_max = 0, pb_volume = 0;
	snd_mixer_t *mixer_handle = NULL;
	snd_mixer_elem_t *elem;
	snd_mixer_selem_id_t *sid;
	uint8_t space = blk->internal->spacing ? 3 : 0;
	if( blk->internal->option[0] ) {
		sscanf(blk->internal->option[0], "%s %s %d", device, mixer_name, &mixer_index);
	} else {
		ya_block_error(blk, "internal-option1 (device) is mandatory");
	}
	if( blk->internal->option[1] &&
        strcmp(blk->internal->option[1], "mapped") == 0 )
		mapped = true;
	if( blk->internal->option[2] ) {
		sscanf(blk->internal->option[2], "%s %s", on, off);
	}
	if ( (ret = snd_mixer_open(&mixer_handle, 0)) < 0 ) {
		ya_block_error(blk, "Unable to open mixer: %s", snd_strerror(ret));
	}
	if ( (ret = snd_mixer_attach(mixer_handle, device)) < 0 ) {
		ya_block_error(blk, "Unable to attach mixer to device: %s", snd_strerror(ret));
	}
	if ( (ret = snd_mixer_selem_register(mixer_handle, NULL, NULL)) < 0 ) {
		ya_block_error(blk, "Unable to register mixer: %s", snd_strerror(ret));
	}
	if ( (ret = snd_mixer_load(mixer_handle)) < 0 ) {
		ya_block_error(blk, "Unable to load mixer elements: %s", snd_strerror(ret));
	}
	snd_mixer_selem_id_malloc(&sid);
	if ( sid == NULL ) {
		ya_block_error(blk, "Failed to allocate an invalid snd_mixer_selem_id using standard malloc");
	}
	snd_mixer_selem_id_set_index(sid, mixer_index);
	snd_mixer_selem_id_set_name(sid, mixer_name);
	elem = snd_mixer_find_selem(mixer_handle, sid);
	if ( elem == NULL ) {
		ya_block_error(blk, "Unable to find index %i of mixer %s",
				snd_mixer_selem_id_get_index(sid), snd_mixer_selem_id_get_name(sid));
	}
	mapped ? snd_mixer_selem_get_playback_dB_range(elem, &range_min, &range_max) :
		snd_mixer_selem_get_playback_volume_range(elem, &range_min, &range_max);
	while (1) {
		snd_mixer_handle_events(mixer_handle);
		if (mapped) {
			if (range_max - range_min <= MAX_LINEAR_DB_SCALE * 100) {
				avg_vol = (pb_volume - range_min) / (double)(range_max - range_min);
			}
			else {
			//map to logarithmic scale using alsa-util's get_normalized_volume from volume_mapping.c
				snd_mixer_selem_get_playback_dB(elem, 0, &pb_volume);
				double norm = exp10((pb_volume - range_max) / 6000.0);
				if (range_min != SND_CTL_TLV_DB_GAIN_MUTE) {
					double min_norm = exp10((range_min - range_max) / 6000.0);
					norm = (norm - min_norm) / (1 - min_norm);
				}
				avg_vol = (int) rint(norm * 100);
			}
		}
		else {
			snd_mixer_selem_get_playback_volume(elem, 0, &pb_volume);
			if ( range_max != 100 ) {
				float avg_volf = ( (float) pb_volume / range_max ) * 100;
				avg_vol = (int) avg_volf;
				avg_vol = (avg_volf - avg_vol < 0.5 ? avg_vol : (avg_vol + 1));
			} else {
				avg_vol = (int) pb_volume;
			}
		}
		if ( snd_mixer_selem_has_playback_switch(elem) ) {
			snd_mixer_selem_get_playback_switch(elem, 0, &pb_switch);
		}
		if ( pb_switch == 0 ) {
			sprintf(startstr, "%s -", off);
		} else {
			sprintf(startstr, "%s %*d", on, space, avg_vol);
		}
		if ( suflen )
			strcat(blk->buf, blk->internal->suffix);
		ya_draw_pango_text(blk);
		sleep(blk->sleep);
	}
	strncpy(blk->buf, "BLOCK ERROR!", strlen("BLOCK ERROR!"));
	ya_draw_pango_text(blk);
	pthread_detach(blk->thread);
	pthread_exit(NULL);
}

#include <netinet/in.h>
#include <net/if.h>
#include <linux/wireless.h>
#include <iwlib.h>

struct wireless_stats {
    struct sockaddr addr;
    char name[IFNAMSIZ + 1];
    char essid[IW_ESSID_MAX_SIZE + 1];
    char mode[16];
    char freq[16];
    int channel;
    char bitrate[16];
    int link_qual;  /* calculate percentage from link_qual and link_qual_max */
    int link_qual_max;
    int link_level; /* dBm */
    int link_noise; /* dBm */
};

static int ya_int_get_wireless_info(struct wireless_stats* ws, char *dev_name) {
    int skfd;
    struct wireless_info winfo;

    /* not all fields may be initialized, so set them to zero */
    memset(ws, 0, sizeof(struct wireless_stats));
    memset(&winfo, 0, sizeof(struct wireless_info));

    skfd = iw_sockets_open();

    snprintf(ws->name, IFNAMSIZ+1, "%s", dev_name);

    if (iw_get_basic_config(skfd, dev_name, &(winfo.b)) > -1) {

        /* Check availability of variables */
        if (iw_get_range_info(skfd, dev_name, &(winfo.range)) >= 0) {
            winfo.has_range = 1;
        }

        if (iw_get_stats(skfd, dev_name, &(winfo.stats), &winfo.range, winfo.has_range) >= 0) {
            winfo.has_stats = 1;
        }

        /* Link Quality */
        if (winfo.has_range && winfo.has_stats && ((winfo.stats.qual.level != 0)
                || (winfo.stats.qual.updated & IW_QUAL_DBM))) {
            if (!(winfo.stats.qual.updated & IW_QUAL_QUAL_INVALID)) {
                ws->link_qual = winfo.stats.qual.qual;
                ws->link_qual_max = winfo.range.max_qual.qual;
                ws->link_noise = winfo.stats.qual.noise;
                ws->link_level = winfo.stats.qual.level;
            }
        }

        /* ESSID */
        if (winfo.b.has_essid && winfo.b.essid_on) {
            snprintf(ws->essid, IW_ESSID_MAX_SIZE+1, "%s", winfo.b.essid);
        }

        /* Channel and Frequency */
        if (winfo.b.has_freq && winfo.has_range == 1) {
            ws->channel = iw_freq_to_channel(winfo.b.freq, &(winfo.range));
            iw_print_freq_value(ws->freq, 16, winfo.b.freq);
        }

        snprintf(ws->mode, 16, "%s", iw_operation_mode[winfo.b.mode]);

    }
    iw_sockets_close(skfd);

    return 0;
}

void ya_int_wifi(ya_block_t *blk) {
    struct wireless_stats ws;
    char *startstr = blk->buf;
    size_t prflen = 0, suflen = 0;
    ya_setup_prefix_suffix(blk, &prflen, &suflen, &startstr);

    while(1) {
        if (ya_int_get_wireless_info(&ws, blk->internal->option[0]) || ws.link_qual_max == 0) {
			ya_block_warning(blk, "Failed to retrieve wireless info for device %s", blk->internal->option[0]);
			sprintf(startstr, "- (0%%)");
        } else {
			sprintf(startstr, "%s (%d%%)", ws.essid, ws.link_qual*100 / ws.link_qual_max);
			if(suflen)
				strcat(blk->buf, blk->internal->suffix);
        }

        ya_draw_pango_text(blk);
        sleep(blk->sleep);
    }

}


/* -- Disk usage block -- */
static const char const symbols[5] = {0, 'K', 'M', 'G', 'T'};
//bytes to human-readable str: convert an amount of bytes to a string with the
//corresponding suffix. e.g. "123456789" -> "117.7M" (bytes)

static int btohstr(char *str, uint64_t bytes)
{
	double size = bytes;
	int exp = 0;
	while (size >= 1024 && exp < 4) {
		size /= 1024;
		exp++;
	}
	return sprintf(str, "%.1f%c", size, symbols[exp]);
}
#include <mntent.h>
#include <sys/statvfs.h>
#define MAX_MOUNTPOINTS 15
void ya_int_diskspace(ya_block_t *blk) {
	char *startstr = blk->buf;
	size_t prflen = 0, suflen = 0;
	ya_setup_prefix_suffix(blk, &prflen, &suflen, &startstr);
	char *mountpoints[MAX_MOUNTPOINTS];
	int8_t mntpntcount = -1;
	FILE *mntentfile = setmntent("/etc/mtab", "r");;
	struct mntent *m;
	uint8_t space = blk->internal->spacing ? 7 : 0;
	//read /etc/mtab to get all mountpoints where the underlying device or
	//volume group matches the internal-option1
	while ( (m = getmntent(mntentfile)) != NULL ) {
		if (strncmp(m->mnt_fsname, blk->internal->option[0],
					strlen(blk->internal->option[0])) == 0) {
			mountpoints[++mntpntcount] = strdup(m->mnt_dir);
		}
		if ( mntpntcount == (MAX_MOUNTPOINTS - 1)) {
			fprintf(stderr, "max mount points reached");
			break;
		}
	}
	endmntent(mntentfile);
	if ( mntpntcount == -1 ) {
		ya_block_error(blk, "no mount points found for prefix \"%s\"",
					   blk->internal->option[0]);
	}
	uint64_t free, total;
	struct statvfs stat;
	char sizebuf[7];
	while (1) {
		free = 0;
		total = 0;
		//get and sum used / total space of every mountpoints
		for( int i = 0; i <= mntpntcount; i++) {
			if ( statvfs(mountpoints[i], &stat) != -1 ) {
				free += (uint64_t)(stat.f_bfree * stat.f_bsize);
				total += (uint64_t)(stat.f_blocks * stat.f_bsize);
			}
		}
		btohstr(sizebuf, total - free);
		sprintf(startstr, "%*s", space, sizebuf);
		strcat(startstr, "/");
		btohstr(sizebuf, total);
		sprintf(startstr+strlen(startstr), "%*s", space, sizebuf);
		if(suflen)
			strcat(blk->buf, blk->internal->suffix);
		ya_draw_pango_text(blk);
		sleep(blk->sleep);
	}
}

#define _GNU_SOURCE
#include <sys/socket.h>
#include <ifaddrs.h>
#include <linux/if_link.h>
#include <arpa/inet.h>
#include <netdb.h>

void ya_int_network(ya_block_t *blk) {
	pthread_detach(blk->thread);
	pthread_exit(NULL);
	/*
	struct ifaddrs *ifaddr, *ifa;
	int family, s, n;
	char host[1025];
	if(getifaddrs(&ifaddr) == -1) {
		fprintf(stderr, "error in getifaddrs\n");
		pthread_exit(NULL);
	}
	for(ifa = ifaddr; ifa; ifa = ifa->ifa_next, n++) {
		if(ifa == NULL)
			continue;
		family = ifa->ifa_addr->sa_family;
		//printf("%s\n", ifa->ifa_name);
		if (family == AF_INET || family == AF_INET6) {
			s = getnameinfo(ifa->ifa_addr,
					(family == AF_INET) ? sizeof(struct sockaddr_in) :
					sizeof(struct sockaddr_in6), host, 1025,
					NULL, 0, NI_NUMERICHOST);
			printf("%s %s\n", ifa->ifa_name, host);
		}
	}
	while(1) {

		ya_draw_pango_text(blk);
		sleep(blk->sleep);
		memset(blk->buf, '\0', 12);
	}
	*/
}
#endif //YA_INTERNAL
