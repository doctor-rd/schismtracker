/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 Storlek <storlek@rigelseven.com>
 * copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
 * copyright (c) 2009 Storlek & Mrs. Brisby
 * copyright (c) 2010-2012 Storlek
 * URL: http://schismtracker.org/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#define NEED_BYTESWAP
#include "headers.h"
#include "slurp.h"
#include "fmt.h"

#include "sndfile.h"

#include "version.h"
#include "disko.h"
#include "log.h"




/* --------------------------------------------------------------------- */

/* TODO: WOW files */

/* Ugh. */
static const char *valid_tags[][2] = {
	/* M.K. must be the first tag! (to test for WOW files) */
	/* the first 5 descriptions are a bit weird */
	{"M.K.", "Amiga-NewTracker"},
	{"M!K!", "Amiga-ProTracker"},
	{"M&K!", "Amiga-NoiseTracker"},
	{"N.T.", "Amiga-NoiseTracker"},
	{"FEST", "Amiga-NoiseTracker"}, /* jobbig.mod */
	{"FLT4", "4 Channel Startrekker"}, /* xxx */
	{"EXO4", "4 Channel Startrekker"}, /* ??? */
	{"CD81", "8 Channel Falcon"},      /* "Falcon"? */
	{"FLT8", "8 Channel Startrekker"}, /* xxx */
	{"EXO8", "8 Channel Startrekker"}, /* ??? */

	{"8CHN", "8 Channel MOD"},  /* what is the difference */
	{"OCTA", "8 Channel MOD"},  /* between these two? */
	{"TDZ1", "1 Channel MOD"},
	{"2CHN", "2 Channel MOD"},
	{"TDZ2", "2 Channel MOD"},
	{"TDZ3", "3 Channel MOD"},
	{"5CHN", "5 Channel MOD"},
	{"6CHN", "6 Channel MOD"},
	{"7CHN", "7 Channel MOD"},
	{"9CHN", "9 Channel MOD"},
	{"10CH", "10 Channel MOD"},
	{"11CH", "11 Channel MOD"},
	{"12CH", "12 Channel MOD"},
	{"13CH", "13 Channel MOD"},
	{"14CH", "14 Channel MOD"},
	{"15CH", "15 Channel MOD"},
	{"16CH", "16 Channel MOD"},
	{"18CH", "18 Channel MOD"},
	{"20CH", "20 Channel MOD"},
	{"22CH", "22 Channel MOD"},
	{"24CH", "24 Channel MOD"},
	{"26CH", "26 Channel MOD"},
	{"28CH", "28 Channel MOD"},
	{"30CH", "30 Channel MOD"},
	{"32CH", "32 Channel MOD"},
	{NULL, NULL}
};

enum {
	WARN_MAXPATTERNS,
	WARN_CHANNELVOL,
	WARN_LINEARSLIDES,
	WARN_SAMPLEVOL,
	WARN_LOOPS,
	WARN_SAMPLEVIB,
	WARN_INSTRUMENTS,
	WARN_PATTERNLEN,
	WARN_MAXCHANNELS,
	WARN_MAXPCM,
	WARN_MAXADLIB,
	WARN_PCMADLIBMIX,
	WARN_MUTED,
	WARN_NOTERANGE,
	WARN_VOLEFFECTS,
	WARN_MAXSAMPLES,

	MAX_WARN
};

static const char *mod_warnings[] = {
//	[WARN_MAXPATTERNS]  = "Over 64 patterns", // M!K!
//	[WARN_CHANNELVOL]   = "Channel volumes",
	[WARN_LINEARSLIDES] = "Linear slides", // done
//	[WARN_SAMPLEVOL]    = "Sample volumes",
//	[WARN_LOOPS]        = "Sustain and Ping Pong loops",
//	[WARN_SAMPLEVIB]    = "Sample vibrato",
	[WARN_INSTRUMENTS]  = "Instrument functions", // done
//	[WARN_PATTERNLEN]   = "Pattern lengths other than 64 rows",
	[WARN_MAXCHANNELS]  = "Data outside 4 channels", // done
	[WARN_NOTERANGE]    = "Notes outside the range C-3 to B-5", // done
//	[WARN_VOLEFFECTS]   = "Extended volume column effects",
	[WARN_MAXSAMPLES]   = "Over 31 samples", // done

	[MAX_WARN]          = NULL
};

const uint16_t amigaperiod_table[256] = {
	0,
	0,    0,    0,    0,    0,    0,    0,    0,    0,   0,    0,    0,
	0,    0,    0,    0,    0,    0,    0,    0,    0,   0,    0,    0,
	0,    0,    0,    0,    0,    0,    0,    0,    0,   0,    0,    0,
	1712, 1616, 1524, 1440, 1356, 1280, 1208, 1140, 1076, 1016, 960, 906,
	856,  808,  762,  720,  678,  640,  604,  570,  538,  508,  480, 453,
	428,  404,  381,  360,  339,  320,  302,  285,  269,  254,  240, 226,
	214,  202,  190,  180,  170,  160,  151,  143,  135,  127,  120, 113,
	107,  101,  95,   90,   85,   80,   75,   71,   67,   63,   60,  56,
	0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,   0,
	0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,   0,
	0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,   0,
	0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,   0,
	0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,   0,
	0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,   0,
	0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,   0,
	0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,   0,
	0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,   0,
	0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,   0,
	0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,   0,
	0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,   0,
	0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,   0,
	0,    0,    0
};

int fmt_mod_read_info(dmoz_file_t *file, const uint8_t *data, size_t length)
{
	char tag[4];
	int i = 0;

	if (length < 1085)
		return 0;

	memcpy(tag, data + 1080, 4);

	for (i = 0; valid_tags[i][0] != NULL; i++) {
		if (memcmp(tag, valid_tags[i][0], 4) == 0) {
			/* if (i == 0) {
				Might be a .wow; need to calculate some crap to find out for sure.
				For now, since I have no wow's, I'm not going to care.
			} */

			file->description = valid_tags[i][1];
			/*file->extension = str_dup("mod");*/
			file->title = strn_dup((const char *)data, 20);
			file->type = TYPE_MODULE_MOD;
			return 1;
		}
	}

	return 0;
}

/* --------------------------------------------------------------------------------------------------------- */

/* force determines whether the loader will force-read untagged files as
   15-sample mods */
static int fmt_mod_load_song(song_t *song, slurp_t *fp, unsigned int lflags, int force)
{
	uint8_t tag[4];
	int n, npat, pat, chan, nchan, nord;
	song_note_t *note;
	uint16_t tmp;
	int startrekker = 0;
	int test_wow = 0;
	int mk = 0;
	int maybe_st3 = 0;
	int maybe_ft2 = 0;
	int his_masters_noise = 0;
	uint8_t restart;
	long samplesize = 0;
	const char *tid = NULL;
	int nsamples = 31; /* default; tagless mods have 15 */

	/* check the tag (and set the number of channels) -- this is ugly, so don't look */
	slurp_seek(fp, 1080, SEEK_SET);
	slurp_read(fp, tag, 4);
	if (!memcmp(tag, "M.K.", 4)) {
		/* M.K. = Protracker etc., or Mod's Grave (*.wow) */
		nchan = 4;
		test_wow = 1;
		mk = 1;
		maybe_ft2 = 1;
		tid = "Amiga-NewTracker";
	} else if (!memcmp(tag, "M!K!", 4)) {
		nchan = 4;
		tid = "Amiga-ProTracker";
	} else if (!memcmp(tag, "M&K!", 4) || !memcmp(tag, "N.T.", 4) || !memcmp(tag, "FEST", 4)) {
		nchan = 4;
		tid = "Amiga-NoiseTracker";
		if (!memcmp(tag, "M&K!", 4) || !memcmp(tag, "FEST", 4)) {
			// Alternative finetuning
			his_masters_noise = 1;
		}
	} else if ((!memcmp(tag, "FLT", 3) || !memcmp(tag, "EXO", 3)) && (tag[3] == '4' || tag[3] == '8')) {
		// Hopefully EXO8 is stored the same way as FLT8
		nchan = tag[3] - '0';
		startrekker = (nchan == 8);
		tid = "%d Channel Startrekker";
		//log_appendf(4, " Warning: Startrekker AM synth is not supported");
	} else if (!memcmp(tag, "OCTA", 4)) {
		nchan = 8;
		tid = "Amiga Oktalyzer"; // IT just identifies this as "8 Channel MOD"
	} else if (!memcmp(tag, "CD81", 4)) {
		nchan = 8;
		tid = "8 Channel Falcon"; // Atari Oktalyser
	} else if (tag[0] > '0' && tag[0] <= '9' && !memcmp(tag + 1, "CHN", 3)) {
		/* nCHN = Fast Tracker (if n is even) or TakeTracker (if n = 5, 7, or 9) */
		nchan = tag[0] - '0';
		if (nchan == 5 || nchan == 7 || nchan == 9) {
			tid = "%d Channel TakeTracker";
		} else {
			if (!(nchan & 1))
				maybe_ft2 = 1;
			tid = "%d Channel MOD"; // generic
		}
		maybe_st3 = 1;
	} else if (tag[0] > '0' && tag[0] <= '9' && tag[1] >= '0' && tag[1] <= '9'
		   && tag[2] == 'C' && (tag[3] == 'H' || tag[3] == 'N')) {
		/* nnCH = Fast Tracker (if n is even and <= 32) or TakeTracker (if n = 11, 13, 15)
		 * Not sure what the nnCN variant is. */
		nchan = 10 * (tag[0] - '0') + (tag[1] - '0');
		if (nchan == 11 || nchan == 13 || nchan == 15) {
			tid = "%d Channel TakeTracker";
		} else {
			if ((nchan & 1) == 0 && nchan <= 32 && tag[3] == 'H')
				maybe_ft2 = 1;
			tid = "%d Channel MOD"; // generic
		}
		if (tag[3] == 'H')
			maybe_st3 = 1;
	} else if (!memcmp(tag, "TDZ", 3) && tag[3] > '0' && tag[3] <= '9') {
		/* TDZ[1-3] = TakeTracker */
		nchan = tag[3] - '0';
		if (nchan < 4)
			tid = "%d Channel TakeTracker";
		else
			tid = "%d Channel MOD";
	} else if (force) {
		/* some old modules don't have tags, so try loading anyway */
		nchan = 4;
		nsamples = 15;
		tid = "%d Channel MOD";
	} else {
		return LOAD_UNSUPPORTED;
	}

	/* suppose the tag is 90CH :) */
	if (nchan > 64) {
		//fprintf(stderr, "%s: Too many channels!\n", filename);
		return LOAD_FORMAT_ERROR;
	}

	/* read the title */
	slurp_rewind(fp);
	slurp_read(fp, song->title, 20);
	song->title[20] = 0;

	/* sample headers */
	for (n = 1; n < nsamples + 1; n++) {
		slurp_read(fp, song->samples[n].name, 22);
		song->samples[n].name[22] = 0;

		slurp_read(fp, &tmp, 2);
		song->samples[n].length = bswapBE16(tmp) * 2;

		/* this is only necessary for the wow test... */
		samplesize += song->samples[n].length;

		if (his_masters_noise) {
			song->samples[n].c5speed = transpose_to_frequency(0, -(signed char)(slurp_getc(fp) << 3));
		} else {
			song->samples[n].c5speed = MOD_FINETUNE(slurp_getc(fp));
		}

		song->samples[n].volume = slurp_getc(fp);
		if (song->samples[n].volume > 64)
			song->samples[n].volume = 64;
		if (!song->samples[n].length && song->samples[n].volume)
			maybe_ft2 = 0;
		song->samples[n].volume *= 4; //mphack
		song->samples[n].global_volume = 64;

		slurp_read(fp, &tmp, 2);
		song->samples[n].loop_start = bswapBE16(tmp) * 2;
		slurp_read(fp, &tmp, 2);
		tmp = bswapBE16(tmp) * 2;
		if (tmp > 2)
			song->samples[n].flags |= CHN_LOOP;
		else if (tmp == 0)
			maybe_st3 = 0;
		else if (!song->samples[n].length)
			maybe_ft2 = 0;
		song->samples[n].loop_end = song->samples[n].loop_start + tmp;
		song->samples[n].vib_type = 0;
		song->samples[n].vib_rate = 0;
		song->samples[n].vib_depth = 0;
		song->samples[n].vib_speed = 0;
	}

	/* pattern/order stuff */
	nord = slurp_getc(fp);
	restart = slurp_getc(fp);

	slurp_read(fp, song->orderlist, 128);
	npat = 0;
	if (startrekker) {
		/* from mikmod: if the file says FLT8, but the orderlist
		has odd numbers, it's probably really an FLT4 */
		for (n = 0; n < 128; n++) {
			if (song->orderlist[n] & 1) {
				startrekker = 0;
				nchan = 4;
				break;
			}
		}
	}
	if (startrekker) {
		for (n = 0; n < 128; n++)
			song->orderlist[n] >>= 1;
	}
	for (n = 0; n < 128; n++) {
		if (song->orderlist[n] >= MAX_PATTERNS)
			song->orderlist[n] = ORDER_SKIP;
		else if (song->orderlist[n] > npat)
			npat = song->orderlist[n];
	}
	/* set all the extra orders to the end-of-song marker */
	memset(song->orderlist + nord, ORDER_LAST, MAX_ORDERS - nord);

	if (restart == 0x7f && maybe_st3)
		tid = "Scream Tracker 3?";
	else if (restart == 0x7f && mk)
		tid = "%d Channel ProTracker";
	else if (restart <= npat && maybe_ft2)
		tid = "%d Channel FastTracker";
	else if (restart == npat && mk)
		tid = "%d Channel Soundtracker";

	/* hey, is this a wow file? */
	if (test_wow) {
		slurp_seek(fp, 0, SEEK_END);
		if (slurp_tell(fp) == 2048 * npat + samplesize + 3132) {
			nchan = 8;
			tid = "Mod's Grave WOW";
		}
	}


	// http://llvm.org/viewvc/llvm-project?view=rev&revision=91888
	sprintf(song->tracker_id, tid ? tid : "%d Channel MOD", nchan);
	/* 15-sample mods don't have a 4-byte tag… or the other 16 samples */
	slurp_seek(fp, nsamples == 15 ? 600 : 1084, SEEK_SET);

	/* pattern data */
	if (startrekker) {
		for (pat = 0; pat <= npat; pat++) {
			note = song->patterns[pat] = csf_allocate_pattern(64);
			song->pattern_size[pat] = song->pattern_alloc_size[pat] = 64;
			for (n = 0; n < 64; n++, note += 60) {
				for (chan = 0; chan < 4; chan++, note++) {
					uint8_t p[4];
					slurp_read(fp, p, 4);
					mod_import_note(p, note);
					csf_import_mod_effect(note, 0);
				}
			}
			note = song->patterns[pat] + 4;
			for (n = 0; n < 64; n++, note += 60) {
				for (chan = 0; chan < 4; chan++, note++) {
					uint8_t p[4];
					slurp_read(fp, p, 4);
					mod_import_note(p, note);
					csf_import_mod_effect(note, 0);
				}
			}
		}
	} else {
		for (pat = 0; pat <= npat; pat++) {
			note = song->patterns[pat] = csf_allocate_pattern(64);
			song->pattern_size[pat] = song->pattern_alloc_size[pat] = 64;
			for (n = 0; n < 64; n++, note += 64 - nchan) {
				for (chan = 0; chan < nchan; chan++, note++) {
					uint8_t p[4];
					slurp_read(fp, p, 4);
					mod_import_note(p, note);
					csf_import_mod_effect(note, 0);
				}
			}
		}
	}
	if (restart < npat)
		csf_insert_restart_pos(song, restart);

	/* sample data */
	if (!(lflags & LOAD_NOSAMPLES)) {
		for (n = 1; n < nsamples + 1; n++) {
			if (song->samples[n].length == 0)
				continue;

			/* check for ADPCM compression */
			uint32_t pcmflag = SF_PCMS;
			char sstart[5];
			slurp_peek(fp, sstart, sizeof(sstart));
			if (!strncmp(sstart, "ADPCM", sizeof(sstart))) {
				slurp_seek(fp, sizeof(sstart), SEEK_CUR);
				pcmflag = SF_PCMD16;
			}

			uint32_t ssize = csf_read_sample(song->samples + n,
				SF_8 | SF_M | SF_LE | pcmflag,
				fp->data + fp->pos, fp->length - fp->pos);
			slurp_seek(fp, ssize, SEEK_CUR);
		}
	}

	/* set some other header info that's always the same for .mod files */
	song->flags = (SONG_ITOLDEFFECTS | SONG_COMPATGXX);
	for (n = 0; n < nchan; n++)
		song->channels[n].panning = PROTRACKER_PANNING(n);
	for (; n < MAX_CHANNELS; n++)
		song->channels[n].flags = CHN_MUTE;

	song->pan_separation = 64;

//      if (slurp_error(fp)) {
//              return LOAD_FILE_ERROR;
//      }

	/* done! */
	return LOAD_SUCCESS;
}

/* loads everything but old 15-instrument mods... yes, even FLT8 and WOW files
   (and the definition of "everything" is always changing) */
int fmt_mod31_load_song(song_t *song, slurp_t *fp, unsigned int lflags)
{
	return fmt_mod_load_song(song, fp, lflags, 0);
}

/* loads everything including old 15-instrument mods. this is a separate
   function so that it can be called later in the format-checking sequence. */
int fmt_mod15_load_song(song_t *song, slurp_t *fp, unsigned int lflags)
{
	return fmt_mod_load_song(song, fp, lflags, 1);
}

/* incomplete 31 sample M.K. amiga mod saving routine */
int fmt_mod_save_song(disko_t *fp, song_t *song)
{
	uint8_t mod_songtitle[20];
	uint8_t mod_sampleheader[30];
	uint8_t mod_orders[128];
	uint8_t tmp[128];
	uint8_t mod_pattern[1024];

	int nord, nsmp, maxpat, jmax, joutpos;
	int i, j, n, period;
	unsigned int warn = 0;
	song_note_t *m;

	if (song->flags & SONG_INSTRUMENTMODE)
		warn |= 1 << WARN_INSTRUMENTS;
	if (song->flags & SONG_LINEARSLIDES)
		warn |= 1 << WARN_LINEARSLIDES;

	nsmp = csf_get_num_samples(song); // Getting number of samples
	if (nsmp > 31) {
		nsmp = 31;
		warn |= 1 << WARN_MAXSAMPLES;
	}

	if (3 < csf_get_highest_used_channel(song))
		warn |= 1 << WARN_MAXCHANNELS;

	memcpy(mod_songtitle, song->title, 20);
	disko_write(fp, mod_songtitle, 20); // writing song title

	// Now writing sample headers
	for(n = 1; n <= 31; ++n) {
		for(i = 0; i < 30; ++i) mod_sampleheader[i] = 0;
		if(n <= nsmp) {
			memcpy(mod_sampleheader, song->samples[n].name, 22); // sample name
			mod_sampleheader[22] = song->samples[n].length >> 9; // sample 11th word MSB length/2
			mod_sampleheader[23] = song->samples[n].length >> 1; // sample 11th word LSB length/2
			for(j = 15; j && (finetune_table[j] > song->samples[n].c5speed); --j)
				if(((song->samples[n].c5speed) > 10000) && (j == 8))
					break; // determine from finetune_table entry
			mod_sampleheader[24] = (j ^ 8) & 0x0f; // sample 24th byte finetune value
			mod_sampleheader[25] = (song->samples[n].volume + 1) / 4; // sample 25th byte sample volume value 0..64 scaled
			if( song->samples[n].flags & CHN_LOOP ) {
				mod_sampleheader[26] = song->samples[n].loop_start >> 9;// loop start MSB /2
				mod_sampleheader[27] = song->samples[n].loop_start >> 1;// loop start LSB /2
				mod_sampleheader[28] = (song->samples[n].loop_end - song->samples[n].loop_start) >> 9;// loop length MSB /2
				mod_sampleheader[29] = (song->samples[n].loop_end - song->samples[n].loop_start) >> 1;// loop length LSB /2
			}
		}
		disko_write(fp, mod_sampleheader, 30); // writing current sample header
	}

	tmp[0] = nord = csf_get_num_orders(song); // or "csf_get_num_orders(song_t *csf);"
	tmp[1] = 0x7f;
	disko_write(fp, tmp, 2);

	for(maxpat = i = 0; (i < nord) && (i < 128); ++i) {
		mod_orders[i] = song->orderlist[i];
		if(maxpat < mod_orders[i]) maxpat = mod_orders[i];
	}
	for(; i < 128; ++i)
		mod_orders[i] = 0;

	disko_write(fp, mod_orders, 128);

	disko_write(fp, valid_tags[0][0], 4);

	for(n = 0; n <= maxpat; ++n) {
		m = song->patterns[n];
		for(i = 0; i < 1024; ++i)
			mod_pattern[i] = 0;
		jmax = song->pattern_size[n];
		jmax = (jmax > 64 ? 64 : jmax) * MAX_CHANNELS;
		for (j = joutpos = 0; j < jmax; ++j, ++m) {
			if ((j % MAX_CHANNELS) < 4) {
				//if (m->note || m->instrument || m->voleffect || m->volparam || m->effect || m->param) {...
				period = amigaperiod_table[(m->note) & 0xff];
				if (((m->note) & 0xff) && ((period < 113) || (period > 856)))
					warn |= 1 << WARN_NOTERANGE;
				mod_pattern[joutpos] = ((m->instrument) & 0x10) | (period >> 8);
				mod_pattern[joutpos + 1] = period & 0xff;
				mod_pattern[joutpos + 2] = ((m->instrument & 0xf) << 4) | ((csf_export_mod_effect(m, 0) >> 8) & 0x0f); // vvvvvvvvvvvv
				mod_pattern[joutpos + 3] = csf_export_mod_effect(m, 0) & 0xff; // TODO: replace lame and buggy csf_export_mod_effect()
				joutpos += 4;
			}
		}
		disko_write(fp, mod_pattern, 1024);
	}

	// TODO : store samples, truncate to even length per sample //Signed 8-bit mono sample data.
	// Now writing sample data
	for (n = 0; (n < nsmp) && (n < 31); ++n) {
		song_sample_t *smp = song->samples + (n + 1);
		if (smp->data)
			csf_write_sample(fp, smp, RS_PCM8S); // last argument is a compound flag: PCMS,8,M,LE (endianness is obsolete)
	}

	/* announce all the things we broke - ripped from s3m.c */
	for (n = 0; n < MAX_WARN; ++n) {
		if (warn & (1 << n))
			log_appendf(4, " Warning: %s unsupported in MOD format", mod_warnings[n]);
	}

	return SAVE_SUCCESS;
}
