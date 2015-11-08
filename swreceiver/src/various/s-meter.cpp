#
/*
 *    Copyright (C) 2010, 2011, 2012
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Programming
 *
 *    This file is part of the SDR-J.
 *    Many of the ideas as implemented in SDR-J are derived from
 *    other work, made available through the GNU general Public License. 
 *    All copyrights of the original authors are recognized.
 *
 *    SDR-J is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    SDR-J is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with SDR-J; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#include	"s-meter.h"
#include	"utilities.h"

		S_Meter::S_Meter (int32_t rate) {
	s_attack	= rate / 100;
	s_decay		= rate / 2;
}

		S_Meter::~S_Meter	(void) {
}

DSPFLOAT	S_Meter::MeterValue (DSPCOMPLEX sample) {
DSPFLOAT	mag	= 20 * abs (sample);
DSPFLOAT	S_average;

	S_attack	= decayingAverage (S_attack, mag, s_attack);
	S_decay		= decayingAverage (S_decay, mag, s_decay);

	if (S_attack > S_decay) {		//increasing signal strength
	   S_average	= S_attack;
	   S_decay	= S_attack;
	}
	else 
	   S_average	= S_decay;

	return fromDBtoS (get_db (S_average) + 20);
}
/*
 */
int16_t	S_Meter::fromDBtoS		(float db) {
	if (db > -73)
	   return 9;
	if (db > -79)
	   return 8;
	if (db > -85)
	   return 7;
	if (db > -91)
	   return 6;
	if (db > -97)
	   return 5;
	if (db > -103)
	   return 4;
	if (db > -109)
	   return 3;
	if (db > -115)
	   return 2;
	return 1;
}
