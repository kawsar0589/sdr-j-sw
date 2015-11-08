#
/*
 *    Copyright (C) 2011, 2012, 2013
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Programming
 *
 *    This file is part of the SDR-J (JSDR).
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
 */
#
#include	<QSettings>
#include	"cw-decoder.h"
#include	"oscillator.h"
#include	"iir-filters.h"
#include	"utilities.h"

#define	USECS_PER_SEC		1000000
#define	MODE_IDLE		0100
#define	MODE_IN_TONE		0200
#define	MODE_AFTER_TONE		0300
#define	MODE_END_OF_LETTER	0400

#define	CW_DOT_REPRESENTATION	'.'
#define	CW_DASH_REPRESENTATION	'_'
#define	CW_IF			800
/*
 * 	The standard morse wordlength (i.e.
 * 	PARIS) is 50 bits, then for a wpm of 1,
 * 	one bit takes 1.2 second or 1.2 x 10^6	microseconds
 */
#define	DOT_MAGIC		1200000

extern const char *const codetable [];

QWidget	*cwDecoder::createPluginWindow (int32_t rate, QSettings *s) {
	theRate		= rate;
	cwSettings	= s;
	myFrame		= new QFrame;
	setupUi 	(myFrame);
	fprintf 	(stderr, "cwDecoder is loaded\n");
	setup_cwDecoder	(rate);
	return myFrame;
}

	decoderInterface::~decoderInterface	(void) {
}

	cwDecoder::~cwDecoder			(void) {
	cwSettings	-> beginGroup ("cwDecoder");
	cwSettings -> setValue ("cwSquelchLevel",
	                        SquelchLevel -> value ());
	cwSettings -> setValue ("cwWPM", WPM -> value ());
	cwSettings -> setValue ("cwFilterDegree",
	                        FilterDegree -> value ());
	cwSettings	-> endGroup ();
	delete	LocalOscillator;
	delete	SmoothenSamples;
	delete	thresholdFilter;
	delete	spaceFilter;
	delete	myFrame;
}

void	cwDecoder::setup_cwDecoder (int32_t rate) {
int16_t	temp;

	theRate		= rate;

	LocalOscillator		= new Oscillator (theRate);
	CycleCount		= theRate / 10;
	cw_IF			= CW_IF;

	SmoothenSamples		= new average (4);
	thresholdFilter		= new average (8);
	spaceFilter		= new average (16);
/*
 *	the filter will be set later on
 */
	cw_LowPassFilter	= NULL;
	cwCurrent		= 0;
	agc_peak		= 0;
	noiseLevel		= 0;
	cwState			= MODE_IDLE;
	cwPreviousState		= MODE_IDLE;

	cw_setFilterDegree (FilterDegree -> value ());
	cw_setSquelchValue (SquelchLevel -> value ());
	cw_setWordsperMinute (WPM -> value ());
	cwTracking	= true;		/* default	*/
	CycleCount	= theRate / 10;
	cwSettings	-> beginGroup ("cwDecoder");
	temp		= cwSettings -> value ("cwSquelchLevel",
	                                  SquelchLevel -> value ()). toInt ();
	SquelchLevel	-> setValue (temp);
	cw_setSquelchValue (temp);

	temp		= cwSettings -> value ("cwWPM",
	                                       WPM -> value ()). toInt ();
	WPM		-> setValue (temp);
	cw_setWordsperMinute (temp);

	temp		= cwSettings -> value ("cwFilterDegree",
	                                    FilterDegree -> value ()). toInt ();
	FilterDegree	-> setValue (temp);
	cw_setFilterDegree (temp);
	cwSettings	-> endGroup ();
	connect (SquelchLevel, SIGNAL (valueChanged (int)),
	         this, SLOT (cw_setSquelchValue (int)));
	connect (WPM, SIGNAL (valueChanged (int)),
	         this, SLOT (cw_setWordsperMinute (int)));
	connect (FilterDegree, SIGNAL (valueChanged (int)),
	         this, SLOT (cw_setFilterDegree (int)));
	connect (Tracker, SIGNAL (clicked (void)),
	         this, SLOT (cw_setTracking (void)));
	
	CWrange			= cwDefaultSpeed / 2;
	cwSpeed			= cwDefaultSpeed;
	lowerBoundWPM		= cwDefaultSpeed - CWrange;
	upperBoundWPM		= cwDefaultSpeed + CWrange;
	cwSpeed			= cwDefaultSpeed;	/* initial	*/

	cwDefaultDotLength	= DOT_MAGIC / cwDefaultSpeed;
	cwDotLength		= cwDefaultDotLength;
	cwSpaceLength		= cwDefaultDotLength;
	cwDotLengthMax		= DOT_MAGIC / lowerBoundWPM;
	cwDotLengthMin		= DOT_MAGIC / upperBoundWPM;

	cwDashLength		= 3 * cwDotLength;
	cwDashLengthMax		= 3 * cwDotLengthMax;
	cwDashLengthMin		= 3 * cwDotLengthMin;

	cw_adaptive_threshold	= 2 * cwDefaultDotLength;
	cwNoiseSpike		= cwDotLength / 3;

	cwStartTimestamp	= 0;
	cwEndTimestamp		= 0;

	cwCurrent		= 0;
	agc_peak		= 0;
	noiseLevel		= 0;
	
	memset			(dataBuffer, 0, sizeof (dataBuffer));
	thresholdFilter		-> clear (2 * cwDotLength);
	spaceFilter		-> clear (cwDotLength);
	cw_clrText		();
	cwCharbox	-> setText (QString (" "));
}

bool	cwDecoder::initforRate	(int32_t rate) {
	if (theRate == rate)
	   return true;
	this	-> theRate	= rate;

	delete	LocalOscillator;
	LocalOscillator		= new Oscillator (theRate);
	delete	cw_LowPassFilter;
	cw_LowPassFilter	= new LowPassIIR (cwFilterDegree,
	                                          75,
	                                          theRate,
	                                          S_BUTTERWORTH);
	CycleCount		= theRate / 10;
	return true;
}

int32_t	cwDecoder::rateOut	(void) {
	return theRate;
}

int16_t	cwDecoder::detectorOffset	(void) {
	return CW_IF;
}

void	cwDecoder::cw_setWordsperMinute (int n) {
	cwDefaultSpeed	= n;
	speedAdjust ();
}

void	cwDecoder::cw_setTracking (void) {
	cwTracking = !cwTracking;
	if (cwTracking)
	   Tracker -> setText (QString ("Tracking off"));
	else
	   Tracker -> setText (QString ("Tracking on"));
	speedAdjust ();
}

void	cwDecoder::cw_setSquelchValue (int s) {
	SquelchValue	= s;
}

void	cwDecoder::cw_setFilterDegree (int n) {
	cwFilterDegree	= n;
	if (cw_LowPassFilter != NULL)
	   delete cw_LowPassFilter;
	cw_LowPassFilter	= new LowPassIIR (cwFilterDegree,
	                                          75,
	                                          theRate,
	                                          S_BUTTERWORTH);
}

void	cwDecoder::speedAdjust () {
	cwSpeed			= cwDefaultSpeed;	/* init	*/
	CWrange			= cwSpeed / 2;
	lowerBoundWPM		= cwDefaultSpeed - CWrange;
	upperBoundWPM		= cwDefaultSpeed + CWrange;

	cwDefaultDotLength	= DOT_MAGIC / cwDefaultSpeed;
	cwDotLength		= cwDefaultDotLength;
	cw_adaptive_threshold	= 2 * cwDefaultDotLength;

	cwDotLengthMax		= DOT_MAGIC / lowerBoundWPM;
	cwDotLengthMin		= DOT_MAGIC / upperBoundWPM;

	cwDashLength		= 3 * cwDotLength;
	cwDashLengthMax		= 3 * cwDotLengthMax;
	cwDashLengthMin		= 3 * cwDotLengthMin;

	cwNoiseSpike		= cwDotLength / 3;
	cwStartTimestamp	= 0;
	cwEndTimestamp		= 0;

	cwState			= MODE_IDLE;
	cwCurrent		= 0;
	memset			(dataBuffer, 0, sizeof (dataBuffer));
	thresholdFilter		-> clear (2 * cwDotLength);
	cw_clrText	();
	cwCharbox	-> setText (QString (" "));
}

void	cwDecoder::doDecode (DSPCOMPLEX z) {
DSPCOMPLEX	ret;
int32_t	lengthOfTone;
int32_t	lengthOfSilence;
int32_t	t;
char	buffer [4];
DSPCOMPLEX	s;

	if (++CycleCount > theRate / 10){
	   CycleCount	= 0;
	   cw_showdotLength	(cwDotLength);
	   cw_showspaceLength	(cwSpaceLength);
	   cw_showspeed		(cwSpeed);
	   cw_showagcpeak	(clamp (agc_peak * 1000.0, 0.0, 100.0));
	   cw_shownoiseLevel	(clamp (noiseLevel * 1000.0, 0.0, 100.0));
	   setDetectorMarker	(cw_IF);
	}

	s	= LocalOscillator	-> nextValue (cw_IF) * z;
	s	= cw_LowPassFilter	-> Pass (s);
	value	=  abs (s);

	if (value > agc_peak)
	   agc_peak = decayingAverage (agc_peak, value, 50.0);
	else
	   agc_peak = decayingAverage (agc_peak, value, 500.0);

	currentTime += USECS_PER_SEC / theRate;
	switch (cwState) {
	   case MODE_IDLE:
	      if ((value > 0.67 * agc_peak) &&
	          (value > SquelchValue * noiseLevel)) { 
/*
 *	we seem to start a new tone
 */
	         cwState		= MODE_IN_TONE;
	         currentTime		= 0;
	         cwCurrent		= 0;
	         cwStartTimestamp	= currentTime;
	         cwPreviousState	= cwState;
	      }
	      else 
	         noiseLevel		= decayingAverage (noiseLevel,
	                                                   value, 500.0);
	      break;				/* just wait	*/

	   case MODE_IN_TONE:
/*
 *	we are/were receiving a tone, either continue
 *	or stop it, depending on some threshold value.
 */
	      if (value > 0.33 * agc_peak)
	         break;			/* just go on	*/
/*
 *	if we are here, the tone has ended
 */
	      cwEndTimestamp	= currentTime;
	      lengthOfTone	= currentTime - cwStartTimestamp;

	      if (lengthOfTone < cwNoiseSpike) {
	         cwState = cwPreviousState;
	         break;
	      }

	      noiseLevel	= decayingAverage (noiseLevel, value, 500.0);

	      if (lengthOfTone <= cw_adaptive_threshold)
	         dataBuffer [cwCurrent ++] = CW_DOT_REPRESENTATION;
	      else
	         dataBuffer [cwCurrent ++] = CW_DASH_REPRESENTATION;

/*
 *	if we gathered too many dots and dashes, we end up
 *	with garbage.
 */
	      if (cwCurrent >= CW_RECEIVE_CAPACITY) {
	         cwCurrent = 0;
	         cwState = MODE_IDLE;
	         break;
	      }

	      dataBuffer [cwCurrent] = 0;
	      cwCharbox	-> setText (QString (dataBuffer));
	      cwState = MODE_AFTER_TONE;

	      if (cwTracking) {
	         t = newThreshold (lengthofPreviousTone, lengthOfTone);
	         if (t > 0) {
	            cw_adaptive_threshold = thresholdFilter -> filter (t);
	            cwDotLength		= cw_adaptive_threshold / 2;
	            cwDashLength	= 3 * cwDotLength;
	            cwSpeed		= DOT_MAGIC / cwDotLength;
	            cwNoiseSpike	= cwDotLength / 3;
	         }
	      }

	      lengthofPreviousTone = lengthOfTone;
	      break;

	   case MODE_AFTER_TONE:
/*
 *	following the end of the tone, we might go back for the
 *	next dash or dot, or we might decide that we reached
 *	the end of the letter
 */
	      if ((value > 0.67 * agc_peak) &&
	          (value > SquelchValue * noiseLevel)) {
	         int t = currentTime - cwEndTimestamp;
	         cwSpaceLength		= spaceFilter -> filter (t);
	         cwState		= MODE_IN_TONE;
	         cwStartTimestamp	= currentTime;
	         cwPreviousState	= cwState;
	         break;
	      }	
// 	no tone, adjust noiselevel and handle silence
	      noiseLevel	= decayingAverage (noiseLevel, value, 500.0);
	      lengthOfSilence	= currentTime - cwEndTimestamp;

	      if ((lengthOfSilence >= 2 * (cwDotLength + cwSpaceLength) / 2)) {
	         lookupToken (dataBuffer, buffer);
	         cwCurrent = 0;
	         cwState = MODE_END_OF_LETTER;
	         cw_addText (buffer [0]);
	      }
//	otherwise, silence too short, do nothing as yet
	      break;
/*
 * 	end_of_letter may be followed by another letter or an end
 * 	of word indication
 */
	   case MODE_END_OF_LETTER:	/* almost MODE_IDLE	*/
	      if ((value > 0.67 * agc_peak) &&
	          (value > SquelchValue * noiseLevel)) {
	         cwState		= MODE_IN_TONE;
	         cwStartTimestamp	= currentTime;
	         cwPreviousState	= cwState;
	      }
	      else {
/*
 *	still silence, look what to do
 */
	         noiseLevel		= decayingAverage (noiseLevel,
	                                                   value, 500.0);
	         lengthOfSilence = currentTime - cwEndTimestamp;
	         if (lengthOfSilence > 4.0 * (cwSpaceLength + cwDotLength) / 2) {
	            cw_addText (' ');	/* word space	*/
	            cwState = MODE_IDLE;
	         }
	      }
	      break;

	   default:
	      break;
	}

	outputSample (real (z), imag (z));
}
/*
 * 	check for dot/dash or dash/dot sequence to adapt the
 * 	threshold.
 */
int32_t	cwDecoder::newThreshold (int32_t prev_element, int32_t curr_element) {

	if ((prev_element > 0) && (curr_element > 0)) {
	   if ((curr_element > 2 * prev_element) &&
	       (curr_element < 4 * prev_element))
	      return  getMeanofDotDash (prev_element, curr_element);
/*
 * 	check for dash dot sequence
 */
	   if ((prev_element > 2 * curr_element) &&
	       (prev_element < 4 * curr_element))
	      return getMeanofDotDash (curr_element, prev_element);
	}
	return -1;
}
/*
 * 	if we have a dot/dash sequence, we might adapt
 * 	the adaptive threshold if both dot and dash length
 * 	are reasonable.
 */
int32_t	cwDecoder::getMeanofDotDash (int32_t idot, int32_t idash) {
int32_t	dot, dash;

	if (idot > cwDotLengthMin &&  idot < cwDotLengthMax)
	   dot = idot;
	else
	   dot = cwDotLength;

	if (idash > cwDashLengthMin && idash < cwDashLengthMax)
	   dash = idash;
	else
	   dash = cwDashLength;

	return (dash + dot) / 2;
}

void 	cwDecoder::printChar (char a, char er) {
	if ((a & 0x7f) < 32) {
	   switch (a) {
	      case '\n':		break;
	      case '\r':		return;
	      case 8:			break;
	      case 9:			break;
	      default:			a = ' ';
	   }
	}

	switch (er) {
	   case 0:	printf("%c",a);break;
	   case 1:	printf("\033[01;42m%c\033[m",a); break;
	   case 2:	printf("\033[01;41m%c\033[m",a); break;
	   case 3:	printf("\033[01;43m%c\033[m",a); break;
           case 4:
           case 5:
           case 6:
           case 7:	printf("\033[2J\033[H<BRK>\n"); break;
	   default:
			break;
        }
}

const char * const codetable[] = {
	"A._",
	"B_...",
	"C_._.",
	"D_..",
	"E.",
	"F.._.",
	"G__.",
	"H....",
	"I..",
	"J.___",
	"K_._",
	"L._..",
	"M__",
	"N_.",
	"O___",
	"P.__.",
	"Q__._",
	"R._.",
	"S...",
	"T_",
	"U.._",
	"V..._",
	"W.__",
	"X_.._",
	"Y_.__",
	"Z__..",
	"0_____",
	"1.____",
	"2..___",
	"3...__",
	"4...._",
	"5.....",
	"6_....",
	"7__...",
	"8___..",
	"9____.",
	".._._._",
	",__..__",
        "?..__..",
	"~"
	};

void	cwDecoder::lookupToken (char *in, char *out) {
int	i;

	for (i = 0; codetable [i][0] != '~'; i ++) {
	   if (strcmp (&codetable [i][1], in) == 0) {
	      out [0] = codetable [i][0];
	      return;
	   }
	}
	out [0] = '*';
}

void	cwDecoder::cw_clrText () {
	cwText = QString ("");
	cwTextbox	-> setText (cwText);
}

void	cwDecoder::cw_addText (char c) {
	if (c < ' ') c = ' ';
	cwText.append (QString (QChar (c)));
	if (cwText. length () > 86)
	   cwText. remove (0, 1);
	cwTextbox	-> setText (cwText);
}

void	cwDecoder::cw_showdotLength (int l) {
	dotLengthdisplay	-> display (l);
}

void	cwDecoder::cw_showspaceLength (int l) {
	spaceLengthdisplay	-> display (l);
}

void	cwDecoder::cw_showagcpeak	(int l) {
	agcPeakdisplay		-> display (l);
}

void	cwDecoder::cw_shownoiseLevel (int l) {
	noiseLeveldisplay	-> display (l);
}

void	cwDecoder::cw_showspeed (int l) {
	actualWPM	-> display (l);
}

#if QT_VERSION < 0x050000
QT_BEGIN_NAMESPACE
Q_EXPORT_PLUGIN2(cwdecoder, cwDecoder)
QT_END_NAMESPACE
#endif


