#include <../../nrnconf.h>

/* this was removed from the scopmath library since there could be
multiple copies of the static value below. One in neuron.exe and the
other in nrnmech.dll.
*/

/******************************************************************************
 *
 * File: random.c
 *
 * Copyright (c) 1984, 1985, 1986, 1987, 1988, 1989, 1990
 *   Duke University
 *
 ******************************************************************************/

#ifndef LINT
static char RCSid[] =
    "random.c,v 1.4 1999/01/04 12:46:49 hines Exp" ;
#endif

#include <math.h>
static unsigned long value = 1;

/*-----------------------------------------------------------------------------
 *
 *  SCOP_RANDOM()
 *
 *	Selects a random number from the uniform distribution on
 *	the interval [0,1].  A seed number can be specified by a
 *	call to the function set_seed(seed).  Otherwise, a seed
 *	of 1 will be used.
 *
 *  Calling sequence:
 *	scop_random()
 *
 *  Arguments:
 *	none for random; for set_seed
 *	Input:	seed, int	value of the seed
 *
 *	Output:	argument unchanged
 *
 *
 *  Returns:
 *	Double precision value of the random number
 *
 *  Functions called:
 *	none
 *
 *  Files accessed:
 *	none
 *
 *--------------------------------------------------------------------------- */

double 
scop_random()
{
  extern int use_mcell_ran4_;
  if (use_mcell_ran4_) {
	/*perhaps 4 times slower but much higher quality*/
	double x, mcell_ran4();
	return mcell_ran4(&value, &x, 1, 1.);
  }else{
    unsigned long a = 2147437301, c = 453816981,
    /* m = 2^32 - 1, the largest long int value that can be represented */
    /*m = 0xFFFFFFFF;*/ /* limited to 32 bit integers*/
    m = ~0;
    value = a * value + c;
    return (fabs((double) value / (double) m));
  }
}

/*-----------------------------------------------------------------------------
 *
 *  SET_SEED()
 *
 *	Set random number seed
 *
 *  Calling sequence:
 *	set_seed(seed)
 *
 *  Arguments:
 *	seed - integer random number seed
 *
 *  Returns:
 *	nothing
 *
 *  Functions called:
 *	none
 *
 *  Files accessed:
 *	none
 *
 */

set_seed(seed)
double seed;
{
    value = (unsigned int)seed;
}
