#
/*
 *    Copyright (C) 2009, 2010, 2011, 2012
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
#ifndef __AGC_HANDLER
#define	__AGC_HANDLER
#include	"swradio-constants.h"
#include	"fft.h"


class	agcHandler {
public:
			agcHandler	(int32_t);
			~agcHandler	(void);
	void		setMode		(uint8_t);
	void		setLevel	(int16_t);	// deprecated
	void		setThreshold	(int16_t);
	void		setSamplerate	(int32_t);
	DSPFLOAT	doAgc		(DSPCOMPLEX);

	enum Mode {
	   AGC_OFF	= 0,
	   AGC_SLOW	= 1,
	   AGC_FAST	= 2
	};

private:
	int32_t		cardrate;
	uint8_t		agcMode;
	DSPFLOAT	*buffer;
	int32_t		bufferP;
	int32_t		bufferSize;
	DSPFLOAT	currentPeak;
	DSPFLOAT	attackAverage;
	DSPFLOAT	decayAverage;
	int16_t		agcHangInterval;
	int16_t		agcHangtime;
	DSPFLOAT	gain_for	(DSPFLOAT);
	DSPFLOAT	ThresholdValue;
};
#endif

