#include <../../nrnconf.h>

#include <OS/list.h>
#include <OS/string.h>
#include <OS/math.h>
#if HAVE_IV
#include "ivoc.h"
#endif
#include "nrnoc2iv.h"
#include "ocobserv.h"
#include "ivocvect.h"
#include <stdio.h>

#include "ocpointer.h"
#include "vrecitem.h"
#include "netcvode.h"
#include "cvodeobj.h"

extern "C" {
extern double t;
extern NetCvode* net_cvode_instance;
}

//Vector.play_remove()
void nrn_vecsim_remove(void* v) {
	PlayRecord* pr = net_cvode_instance->playrec_uses(v);
	if (pr) {
		delete pr;
	}
}

void nrn_vecsim_add(void* v, boolean record) {
	IvocVect* yvec, *tvec, *dvec;
	double* pvar = nil;
	char* s = nil;
	double ddt;

	yvec = (IvocVect*)v;
	
	if (record == false && hoc_is_str_arg(1)) {//statement involving $1
		// Vector.play("proced($1)", ...)
		s = gargstr(1);
	}else if (record == false && hoc_is_double_arg(1)) {// play that element
		// Vector.play(index)
		// must be a VecPlayStep and nothing else
			VecPlayStep* vps = (VecPlayStep*)net_cvode_instance->playrec_uses(v);
		if (vps) {
			int j = (int)chkarg(1, 0., yvec->capacity()-1);
			if (vps->si_) {
				vps->si_->play_one(yvec->elem(j));
			}
		}
		return;
	}else{
		// Vector.play(&SEClamp[0].amp1, ...)
		// Vector.record(&SEClamp[0].i, ...)
		pvar = hoc_pgetarg(1);
	}
	tvec = nil;
	dvec = nil;
	ddt = -1.;
	int con = 0;
	if (ifarg(2)) {
		if (hoc_is_object_arg(2)) {
			// Vector...(..., tvec)
			tvec = vector_arg(2);
		}else{
			// Vector...(..., Dt)
			ddt = chkarg(2, 1e-9, 1e10);
		}
		if (ifarg(3)) {
			if (hoc_is_double_arg(3)) {
				con = (int)chkarg(3, 0., 1.);
			}else{
				dvec = vector_arg(3);
				con = 1;
			}
		}
	}	
	
// tvec can be used for many record/play items
//	if (tvec) { nrn_vecsim_remove(tvec); }
	if (record) {
		// yvec can be used only for one record (but many play)
		if (yvec) { nrn_vecsim_remove(yvec); }
		if (tvec) {
			new VecRecordDiscrete(pvar, yvec, tvec);
		} else if (ddt > 0.) {
			new VecRecordDt(pvar, yvec, ddt);
		} else if (pvar == &t) {
			new TvecRecord(chk_access(), yvec);
		} else {
			new YvecRecord(pvar, yvec);
		}
	}else{
		if (con) {
			if (s) {
				new VecPlayContinuous(s, yvec, tvec, dvec);
			}else{
				new VecPlayContinuous(pvar, yvec, tvec, dvec);
			}
		}else{
			if (!tvec && ddt == -1.) {
				chkarg(2, 1e-9, 1e10);
			}
			if (s) {
				new VecPlayStep(s, yvec, tvec, ddt);
			}else{
				new VecPlayStep(pvar, yvec, tvec, ddt);
			}
		}
	}
}

VecPlayStep::VecPlayStep(double* pd, IvocVect* y, IvocVect* t, double dt) : PlayRecord(pd) {
//printf("VecPlayStep\n");
	init(y, t, dt);
}

VecPlayStep::VecPlayStep(const char* s, IvocVect* y, IvocVect* t, double dt) : PlayRecord(&NODEV(chk_access()->pnode[0])) {
//printf("VecPlayStep\n");
	init(y, t, dt);
	si_ = new StmtInfo(s);
}

void VecPlayStep::init(IvocVect* y, IvocVect* t, double dt) {
	y_ = y;
	t_ = t;
	dt_ = dt;
	ObjObservable::Attach(y_->obj_, this);
	if (t_) {
		ObjObservable::Attach(t_->obj_, this);
	}
	e_ = new PlayRecordEvent();
	e_->plr_ = this;
	si_ = nil;
}


VecPlayStep::~VecPlayStep() {
//printf("~VecPlayStep\n");
	ObjObservable::Detach(y_->obj_, this);
	if (t_) {
		ObjObservable::Detach(t_->obj_, this);
	}
	delete e_;
	if (si_) {
		delete si_;
	}
}

void VecPlayStep::disconnect(Observable*) {
//	printf("%s VecPlayStep disconnect\n", hoc_object_name(y_->obj_));
	delete this;
}

void VecPlayStep::install(Cvode* cv) {
	play_add(cv);
}

void VecPlayStep::play_init() {
	current_index_ = 0;
	if (t_) {
		if (t_->capacity() > 0) {
			e_->send(t_->elem(0), net_cvode_instance);
		}
	}else{
			e_->send(0., net_cvode_instance);
	}
}

void VecPlayStep::deliver(double tt, NetCvode* ns) {
	if (cvode_) {
		cvode_->set_init_flag();
	}
	if (si_) {
		si_->play_one(y_->elem(current_index_++));
	}else{
		*pd_ = y_->elem(current_index_++);
	}
	if (current_index_ < y_->capacity()) {
		if (t_) {
			if (current_index_ < t_->capacity()) {
				e_->send(t_->elem(current_index_), ns);
			}
		}else{
			e_->send(tt + dt_, ns);
		}
	}
}

	
void VecPlayStep::pr() {
	printf("VecPlayStep ");
	printf("%s.x[%d]\n", hoc_object_name(y_->obj_), current_index_);
}

VecPlayContinuous::VecPlayContinuous(double* pd, IvocVect* y, IvocVect* t, IvocVect* discon) : PlayRecord(pd) {
//printf("VecPlayContinuous\n");
	init(y, t, discon);
}

VecPlayContinuous::VecPlayContinuous(const char* s, IvocVect* y, IvocVect* t, IvocVect* discon) : PlayRecord(&NODEV(chk_access()->pnode[0])) {
//printf("VecPlayContinuous\n");
	init(y, t, discon);
	si_ = new StmtInfo(s);
}

void VecPlayContinuous::init(IvocVect* y, IvocVect* t, IvocVect* discon) {
	y_ = y;
	t_ = t;
	discon_indices_ = discon;
	ObjObservable::Attach(y_->obj_, this);
	if (t_) {
		ObjObservable::Attach(t_->obj_, this);
	}
	if (discon_indices_) {
		ObjObservable::Attach(discon_indices_->obj_, this);
	}
	e_ = new PlayRecordEvent();
	e_->plr_ = this;
	si_ = nil;
}


VecPlayContinuous::~VecPlayContinuous() {
//printf("~VecPlayContinuous\n");
	ObjObservable::Detach(y_->obj_, this);
	ObjObservable::Detach(t_->obj_, this);
	if (discon_indices_) {
		ObjObservable::Detach(discon_indices_->obj_, this);
	}
	delete e_;
	if (si_) {
		delete si_;
	}
}

void VecPlayContinuous::disconnect(Observable*) {
//	printf("%s VecPlayContinuous disconnect\n", hoc_object_name(y_->obj_));
	delete this;
}

void VecPlayContinuous::install(Cvode* cv) {
	play_add(cv);
}

void VecPlayContinuous::play_init() {
	last_index_ = 0;
	discon_index_ = 0;
	if (discon_indices_) {
		if (discon_indices_->capacity() > 0) {
			ubound_index_ = (int)discon_indices_->elem(discon_index_++);
//printf("play_init %d %g\n", ubound_index_, t_->elem(ubound_index_));
			e_->send(t_->elem(ubound_index_), net_cvode_instance);
		}else{
			ubound_index_ = t_->capacity()-1;
		}
	}else{
		ubound_index_ = 0;
		e_->send(t_->elem(ubound_index_), net_cvode_instance);
	}
}

void VecPlayContinuous::deliver(double tt, NetCvode* ns) {
//printf("deliver %g\n", tt);
	if (cvode_) {
		cvode_->set_init_flag();
	}
	last_index_ = ubound_index_;
	if (discon_indices_) {
		if (discon_index_ < discon_indices_->capacity()) {
			ubound_index_ = (int)discon_indices_->elem(discon_index_++);
//printf("after deliver:send %d %g\n", ubound_index_, t_->elem(ubound_index_));
			e_->send(t_->elem(ubound_index_), ns);
		}
	}else{
		if (ubound_index_ < t_->capacity() - 1) {
			ubound_index_++;
			e_->send(t_->elem(ubound_index_), ns);
		}
	}
	continuous(tt);
}

	
void VecPlayContinuous::continuous(double tt) {
	if (si_) {
		si_->play_one(interpolate(tt));
	}else{
		*pd_ = interpolate(tt);
	}
}

double VecPlayContinuous::interpolate(double tt) {
	if (tt >= t_->elem(ubound_index_)) {
		last_index_ = ubound_index_;
		return y_->elem(last_index_);
	}else if (tt <= t_->elem(0)) {
		last_index_ = 0;
		return y_->elem(0);
	}else{
		search(tt);
	}
	double x0 = y_->elem(last_index_-1);
	double x1 = y_->elem(last_index_);
	double t0 = t_->elem(last_index_ - 1);
	double t1 = t_->elem(last_index_);
//printf("IvocVectRecorder::continuous tt=%g t0=%g t1=%g theta=%g x0=%g x1=%g\n", tt, t0, t1, (tt - t0)/(t1 - t0), x0, x1);
	return interp((tt - t0)/(t1 - t0), x0, x1);
#if 0
	// dt
	double theta = tt/dt_ - last_index_;
	interp(theta, x0, x1);
#endif
}

void VecPlayContinuous::search(double tt) {
//	assert (tt > t_->elem(0) && tt < t_->elem(t_->capacity() - 1))
	while ( tt < t_->elem(last_index_)) {
		--last_index_;
	}
	while ( tt >= t_->elem(last_index_)) {
		++last_index_;
	}
}

void VecPlayContinuous::pr() {
	printf("VecPlayContinuous ");
	printf("%s.x[%d]\n", hoc_object_name(y_->obj_), last_index_);
}

PlayRecordSave* VecPlayStep::savestate_save() {
	return new VecPlayStepSave(this);
}

VecPlayStepSave::VecPlayStepSave(PlayRecord* prl) : PlayRecordSave(prl) {
	curindex_ = ((VecPlayStep*)pr_)->current_index_;
}
VecPlayStepSave::~VecPlayStepSave() {
}
void VecPlayStepSave::savestate_restore() {
	check();
	VecPlayStep* vps = (VecPlayStep*)pr_;
	vps->current_index_ = curindex_;
	if (curindex_ > 0) {
		if (vps->si_) {
			vps->si_->play_one(vps->y_->elem(curindex_ - 1));
		}else{
			*vps->pd_ = vps->y_->elem(curindex_ - 1);
		}
	}
}
void VecPlayStepSave::savestate_write(FILE* f) {
	fprintf(f, "%d\n", curindex_);
}
void VecPlayStepSave::savestate_read(FILE* f) {
	char buf[100];
	fgets(buf, 100, f);
	assert(sscanf(buf, "%d\n", &curindex_) == 1);
}

PlayRecordSave* VecPlayContinuous::savestate_save() {
	return new VecPlayContinuousSave(this);
}

VecPlayContinuousSave::VecPlayContinuousSave(PlayRecord* prl) : PlayRecordSave(prl) {
	VecPlayContinuous* vpc = (VecPlayContinuous*)pr_;
	last_index_ = vpc->last_index_;
	discon_index_ = vpc->discon_index_;
	ubound_index_ = vpc->ubound_index_;
}
VecPlayContinuousSave::~VecPlayContinuousSave() {
}
void VecPlayContinuousSave::savestate_restore() {
	check();
	VecPlayContinuous* vpc = (VecPlayContinuous*)pr_;
	vpc->last_index_ = last_index_;
	vpc->discon_index_ = discon_index_;
	vpc->ubound_index_ = ubound_index_;
	vpc->continuous(t);
}
void VecPlayContinuousSave::savestate_write(FILE* f) {
	fprintf(f, "%d %d %d\n", last_index_, discon_index_, ubound_index_);
}
void VecPlayContinuousSave::savestate_read(FILE* f) {
	char buf[100];
	fgets(buf, 100, f);
	assert(sscanf(buf, "%d %d %d\n", &last_index_, &discon_index_, &ubound_index_) == 3);
}

