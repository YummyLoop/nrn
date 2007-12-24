#ifndef nrnmenu_h
#define nrnmenu_h

#include <OS/string.h>
#include "ndatclas.h"
#include <ivstream.h>
class MechTypeImpl;

class MechanismStandard : public Resource {
public:
	MechanismStandard(const char*, int vartype);
	virtual ~MechanismStandard();

	void panel(const char* label = nil);
	void action(const char*);

   int count();
   const char* name();
   const char* name(int, int&); // returns array dimension and name
   
	// from arg (section.node(x) (0 if x < 0) to this
	void in(Section*, double x = -1.);
	void in(Point_process*);
	void in(MechanismStandard*);
	void set(const char*, double val, int arrayindex=0);

	// from this to segement containing x (uniformly if x < 0)
	void out(Section*, double x = -1.);
	void out(Point_process*);
	void out(MechanismStandard*);
	double get(const char*, int arrayindex=0);

	void save(const char*, ostream*); // for session files
private:
	NrnProperty* np_;
	int name_cnt_;
	int offset_;
	int vartype_;
	CopyString action_;
	Symbol** glosym_;
	void mschk(const char*);
};

class MechanismType : public Resource {
public:
	MechanismType(boolean point_process);
	virtual ~MechanismType();
	boolean is_point();
	boolean is_netcon_target(int);
	boolean has_net_event(int);
	boolean is_artificial(int);
	void select(const char*);
	const char* selected();
	void insert(Section*);
	void remove(Section*);
	void point_process(Object**);
	void action(const char*);
	void menu();

	int count();
	int selected_item();
	void select(int);

	Point_process* pp_begin();
	Point_process* pp_next();
private:
	MechTypeImpl* mti_;
};

#endif