#ifndef objcmd_h
#define objcmd_h

#include <OS/string.h>
#include <InterViews/observe.h>
#if HAVE_IV
#include <InterViews/action.h>
#include "rubband.h"
#endif

struct Object;

// command to be executed within scope of object.

class HocCommand : public Observer{
public:
	HocCommand(const char*);
	HocCommand(const char*, Object*);
	virtual ~HocCommand();
	int execute(boolean notify = true);
	int execute(const char*, boolean notify = true);
	const char* name();
	virtual void update(Observable*);
	virtual void audit();
	virtual void help();
	double func_call(int narg);
	Object* object() { return obj_; }
private:
	void init(const char*, Object*);
private:
	Object* obj_;
	CopyString* s_;
};

#if HAVE_IV
class HocCommandAction : public Action {
public:
	HocCommandAction(HocCommand*);
	virtual ~HocCommandAction();
	virtual void execute();
private:
	HocCommand* hc_;
};

class HocCommandTool : public Rubberband {
public:
	HocCommandTool(HocCommand*);
	virtual ~HocCommandTool();
	virtual boolean event(Event&);
	HocCommand* hc_;
};
#endif

#endif