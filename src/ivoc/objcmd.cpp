#include <../../nrnconf.h>

#if HAVE_IV
#include <InterViews/window.h>
#include "ivoc.h"
#include "scenevie.h"
#endif

#include <stdio.h>
#include "objcmd.h"
#include "oc2iv.h"

extern "C" {
extern Object* hoc_thisobject;
}

HocCommand::HocCommand(const char* cmd) {
	init(cmd, hoc_thisobject);
}
HocCommand::HocCommand(const char* cmd, Object* obj) {
	init(cmd, obj);
}

void HocCommand::init(const char* cmd, Object* obj) {
	s_ = new CopyString(cmd);
	obj_ = obj;
#if HAVE_IV
	if (obj_) {
		Oc oc;
		oc.notify_when_freed((void*)obj_, this);
	}
#endif
}

void HocCommand::update(Observable*) { // obj_ has been freed
	obj_ = nil;
	delete s_;
	s_ = new CopyString("");
}

HocCommand::~HocCommand() {
#if HAVE_IV
	if (obj_) {
		Oc oc;
		oc.notify_pointer_disconnect(this);
	}
#endif
	delete s_;
}

void HocCommand::help() {
#if HAVE_IV
	char buf[200];
	if (obj_) {
		sprintf(buf,"%s %s",
			s_->string(),
			obj_->ctemplate->sym->name
		);
	}else{
		sprintf(buf, "%s", s_->string());
	}
	Oc::help(buf);
#endif
}

const char* HocCommand::name() {
	return s_->string();
}

void HocCommand::audit() {
	if (!s_) {
		return;
	}
	char buf[256];
	if (obj_) {
		sprintf(buf, "// execute(\"%s\", %lx)\n", name(), (long)obj_);
	}else{
		sprintf(buf, "{%s}\n", name());
	}
	hoc_audit_command(buf);
}

int HocCommand::execute(boolean notify) {
	if (!s_) {
		return 0;
	}
	char buf[256];
	sprintf(buf, "{%s}\n", s_->string());
	int err = hoc_obj_run(buf, obj_);
#if HAVE_IV
	if (notify) {
		Oc oc;
		oc.notify();
	}
#endif
	return err;
}
int HocCommand::execute(const char* s, boolean notify) {
	char buf[256];
	sprintf(buf, "{%s}\n", s);
	int err = hoc_obj_run(buf, obj_);
#if HAVE_IV
	if (notify) {
		Oc oc;
		oc.notify();
	}
#endif
	return err;
}

double HocCommand::func_call(int narg) {
	Symbol* s = nil;
	if (obj_ && obj_->ctemplate) {
		s = hoc_table_lookup(name(), obj_->ctemplate->symtable);
	}
	if (!s) {
		s = hoc_lookup(name());
	}
	if (!s) {
		hoc_execerror(name(), "is not a symbol in HocCommand::func_call");
	}
	return hoc_call_objfunc(s, narg, obj_);
}

#if HAVE_IV // to end of file

HocCommandAction::HocCommandAction(HocCommand* hc) {
	hc_ = hc;
}

HocCommandAction::~HocCommandAction() {delete hc_;}

void HocCommandAction::execute() {
	hc_->execute();
}

HocCommandTool::HocCommandTool(HocCommand* hc) : Rubberband() {
	hc_ = hc;
}

HocCommandTool::~HocCommandTool() {
	delete hc_;
}

boolean HocCommandTool::event(Event& e) {
	char buf[256];
	Coord x, y;
	int kd;
#ifdef WIN32
	if (e.type() != Event::down && e.type() != Event::up && e.window()->canvas()->any_damage()) {
		return true;
	}
#endif
	if (e.type() == Event::down) {
		Resource::ref(this);
		e.grab(this);
#ifdef WIN32
		e.window()->grab_pointer();
#endif
	}
	kd = e.control_is_down() + e.shift_is_down()*2 + e.meta_is_down()*4;
//	transformer().inverse_transform(e.pointer_x(), e.pointer_y(), x, y);
//	the hoc callback may change the size of the view
	const Transformer& t = XYView::current_pick_view()->s2o();
	t.transform(e.pointer_x(), e.pointer_y(), x, y);
	sprintf(buf, "%s(%d, %g, %g, %d)", hc_->name(), e.type(), x, y, kd);
//printf("%g %g %g %g\n", e.pointer_x(), e.pointer_y(), x, y);
	if (e.type() == Event::up) {
		e.ungrab(this);
#ifdef WIN32
		e.window()->ungrab_pointer();
#endif
	}
	hc_->execute(buf, true);
	if (e.type() == Event::up) {
		Resource::unref(this);
	}
	return true;
}
#endif