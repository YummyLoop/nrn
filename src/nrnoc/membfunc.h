#ifndef nrn_memb_func_h
#define nrn_memb_func_h
#if defined(__cplusplus)
extern "C" {
#endif

typedef Datum *(*Pfrpdat)();

#define NULL_CUR (Pfri)0
#define NULL_ALLOC (Pfri)0
#define NULL_STATE (Pfri)0
#define NULL_INITIALIZE (Pfri)0

typedef struct Memb_func {
	Pfri	alloc;
	Pfri	current;
	Pfri	jacob;
	Pfri	state;
	Pfri	initialize;
	Pfri	destructor;	/* only for point processes */
	Symbol	*sym;
#if CVODE
	Pfri	ode_count;
	Pfri	ode_map;
	Pfri	ode_spec;
	Pfri	ode_matsol;
	Pfri	ode_synonym;
	Pfri	singchan_; /* managed by kschan for variable step methods */
#endif
	int vectorized;
	int is_point;
	void* hoc_mech;
} Memb_func;

#if VECTORIZE
#include "nrnoc_ml.h"
#endif

#define VINDEX	-1
#define CABLESECTION	1
#define MORPHOLOGY	2
#define CAP	3
#if EXTRACELLULAR
#define EXTRACELL	5
#endif

#define CONST 1
#define DEP 2
#define STATE 3	/*See init.c and cabvars.h for order of CONST, DEP, and STATE */

#define BEFORE_INITIAL 0
#define AFTER_INITIAL 1
#define BEFORE_BREAKPOINT 2
#define AFTER_SOLVE 3
#define BEFORE_STEP 4
#define BEFORE_AFTER_SIZE 5 /* 1 more than the previous */
typedef struct BAMech {
	Pfri f;
	int type;
	struct BAMech* next;
} BAMech;
extern BAMech** bamech_;

extern Memb_func* memb_func;
extern int n_memb_func;
#if VECTORIZE
extern Memb_list* memb_list;
/* for finitialize, order is same up through extracellular, then ions,
then mechanisms that write concentrations, then all others. */
extern short* memb_order_; 
#endif
#define NRNPOINTER 4 /* added on to list of mechanism variables.These are
pointers which connect variables  from other mechanisms via the _ppval array.
*/

#define _AMBIGUOUS 5

#if defined(__cplusplus)
}
#endif

#endif /* nrn_memb_func_h */