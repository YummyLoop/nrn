#include <../../nrnconf.h>
#undef check
#include <InterViews/resource.h>
#include <stdio.h>
#include "ocfile.h"
#include "nrnoc2iv.h"
#include "classreg.h"
#include "ndatclas.h"

#include "tqueue.h"
#include "netcon.h"
#include "vrecitem.h"

typedef void (*ReceiveFunc)(Point_process*, double*, double);

extern "C" {
#include "membfunc.h"
extern int section_count;
extern Section** secorder;
extern ReceiveFunc* pnt_receive;
extern NetCvode* net_cvode_instance;
extern TQueue* net_cvode_instance_event_queue();
extern void clear_event_queue();
extern hoc_Item* net_cvode_instance_psl();
extern PlayRecList* net_cvode_instance_prl();
extern double t;
extern short* nrn_is_artificial_;
static void tqcallback(const TQItem* tq, int i);
};

class SaveState : public Resource {
public:
	SaveState();
	~SaveState();
	virtual void save();
	virtual void restore(int type);
	virtual void read(OcFile*, boolean close);
	virtual void write(OcFile*, boolean close);
	struct NodeState {
		double v;
		int nmemb;
		int* type;
		int nstate;
		double* state;
	};
	struct SecState {
		Section* sec;
		int nnode;
		struct NodeState* ns;
	};
	struct StateStructInfo {
		int offset;
		int size;
	};
	struct ACellState {
		int type;
		int ncell;
		double* state;		
	};
	struct NetConState {
		int object_index; // for checking
		int nstate;
		double* state;
	};
	struct PreSynState {
		boolean flag; // is it firing?
		double valthresh, valold, told;
		
	};		
	struct TQState {
		int nstate;
		double* tdeliver;
		DiscreteEvent** items;
	};
private:
	boolean check(boolean warn);
	void alloc();
	void ssfree();
	void ssi_def();
private:
	void fread_NodeState(NodeState*, int, FILE*);
	void fwrite_NodeState(NodeState*, int, FILE*);
	void fread_SecState(SecState*, int, FILE*);
	void fwrite_SecState(SecState*, int, FILE*);
private:
	double t_;
	int nrootnode_;
	NodeState* ns_;
	int nsec_;
	SecState* ss_;
	int nacell_; // number of types
	ACellState* acell_;
	int nncs_;
	NetConState* ncs_;
	int npss_;
	PreSynState* pss_;
	TQState* tqs_;
	int tqcnt_; // volatile for index of forall_callback
	int nprs_;
	PlayRecordSave** prs_;
	static StateStructInfo* ssi;
	static cTemplate* nct;
private:
	void savenode(NodeState&, Node*);
	void restorenode(NodeState&, Node*);
	boolean checknode(NodeState&, Node*, boolean);
	void allocnode(NodeState&, Node*);

	void saveacell(ACellState&, int type);
	void restoreacell(ACellState&, int type);
	boolean checkacell(ACellState&, int type, boolean);
	void allocacell(ACellState&, int type);

	void savenet();
	void restorenet();
	void readnet(FILE*);
	void writenet(FILE*);
	boolean checknet(boolean);
	void allocnet();
	void free_tq();
	void alloc_tq();
public:
	void tqcount(const TQItem*, int);
	void tqsave(const TQItem*, int);
};

static SaveState* this_savestate;
static int callback_mode;

void tqcallback(const TQItem* tq, int i) {
	if (callback_mode == 0) {
		this_savestate->tqcount(tq, i);
	}else{
		this_savestate->tqsave(tq, i);
	}
}

#if __sgi && 0
// fixed in version 5 of os
StateStructInfo* SaveState::ssi;
#else
SaveState::StateStructInfo* SaveState::ssi;
#endif
cTemplate* SaveState::nct;

SaveState::SaveState() {
	int i, j;
	ssi_def();
	ns_ = nil;
	nrootnode_ = 0;
	ss_ = nil;
	nsec_ = 0;
	nncs_ = 0;
	ncs_ = nil;
	npss_ = 0;
	pss_ = nil;
	tqs_ = new TQState();
	tqs_->nstate = 0;
	nprs_ = 0;
	prs_ = nil;
	nacell_ = 0;
	for (i=0; i < n_memb_func; ++i) if (nrn_is_artificial_[i]) {
		++nacell_;
	}
	acell_ = new ACellState[nacell_];
	for (i=0; i < nacell_; ++i) {
		acell_[i].ncell = 0;
		acell_[i].state = 0;		
	}
	for (i=0, j=0; i < n_memb_func; ++i) if (nrn_is_artificial_[i]) {
		acell_[j].type = i;
		++j;
	}
}

SaveState::~SaveState() {
	ssfree();
	delete tqs_;
	delete [] acell_;
}

void SaveState::fread_NodeState(NodeState* ns, int cnt, FILE* f) {
	for (int i=0; i < cnt; ++i) {
		fread(&ns[i].v, sizeof(double), 1, f);
		fread(&ns[i].nmemb, sizeof(int), 1, f);
		fread(&ns[i].nstate, sizeof(int), 1, f);
	}
}
void SaveState::fwrite_NodeState(NodeState* ns, int cnt, FILE* f){
	for (int i=0; i < cnt; ++i) {
		fwrite(&ns[i].v, sizeof(double), 1, f);
		fwrite(&ns[i].nmemb, sizeof(int), 1, f);
		fwrite(&ns[i].nstate, sizeof(int), 1, f);
	}
}
void SaveState::fread_SecState(SecState* ss, int cnt, FILE* f){
	for (int i=0; i < cnt; ++i) {
		fread(&ss[i].nnode, sizeof(int), 1, f);
	}
}
void SaveState::fwrite_SecState(SecState* ss, int cnt, FILE* f){
	for (int i=0; i < cnt; ++i) {
		fwrite(&ss[i].nnode, sizeof(int), 1, f);
	}
}

void SaveState::ssi_def() {
	if (nct) { return; }
	Symbol* s = hoc_lookup("NetCon");
	nct = s->u.ctemplate;
	ssi = new StateStructInfo[n_memb_func];
	for (int im=0; im < n_memb_func; ++im) {
		ssi[im].offset = -1;
		ssi[im].size = 0;
		if (!memb_func[im].sym) {
			continue;
		}
		NrnProperty* np = new NrnProperty(memb_func[im].sym->name);
		// generally we only save STATE variables. However for
		// models containing a NET_RECEIVE block, we also need to
		// save everything except the parameters
		// because they often contain
		// logic and analytic state values. Unfortunately, it is often
		// the case that the ASSIGNED variables are not declared as
		// RANGE variables so to avoid missing state, save the whole
		// param array including PARAMETERs.
	    if  (pnt_receive[im]) {
		ssi[im].offset = 0;
		ssi[im].size = np->prop()->param_size;
	    }else{
		int type = STATE;
		for (Symbol* sym = np->first_var(); np->more_var(); sym = np->next_var()) {
			if (np->var_type(sym) == type
			    || np->var_type(sym) == STATE
			    || sym->subtype == _AMBIGUOUS) {
				if (ssi[im].offset < 0) {
					ssi[im].offset = np->prop_index(sym);
				}
				ssi[im].size += hoc_total_array_data(sym, 0);
			}
		}
	    }
	    delete np;
	}
}

boolean SaveState::check(boolean warn) {
	if (nsec_ != section_count) {
		if (warn) {
fprintf(stderr, "SaveState warning: %d sections exist but saved %d\n",
			section_count, nsec_);
		}
		return false;
	}
	if (nrootnode_ != rootnodecount) {
		if (warn) {
fprintf(stderr, "SaveState warning: %d cells exist but saved %d\n",
			rootnodecount, nrootnode_);
		}
		return false;
	}
	if (nsec_ && ss_[0].sec == nil) { // got the data from a read
		for (int isec=0; isec < nsec_; ++isec) {
			ss_[isec].sec = secorder[isec];
			section_ref(ss_[isec].sec);
		}
	}
	for (int irn = 0; irn < nrootnode_; ++irn) {
		if (!checknode(ns_[irn], v_node[irn], warn)) {
			return false;
		}
	}
	for (int i=0, j=0; i < n_memb_func; ++i) if (nrn_is_artificial_[i]) {
		if (!checkacell(acell_[j], i, warn)) {
			return false;
		}
		++j;
	}
	for (int isec=0; isec < nsec_; ++isec) {
		SecState& ss = ss_[isec];
		Section* sec = ss.sec;
		if (!sec->prop || sec->nnode != ss.nnode) {
if (warn) {
if (!sec->prop) {
fprintf(stderr, "SaveState warning: saved section no longer exists\n");
}else{
fprintf(stderr, "SaveState warning: %s has %d nodes but saved %d\n",
				secname(sec), sec->nnode, ss.nnode);
}
}
			return false;
		}
		for (int inode = 0; inode < ss.nnode; ++inode) {
			NodeState& ns = ss.ns[inode];
			Node* nd = sec->pnode[inode];
			int i=0;
			Prop* p;
			for (p = nd->prop; p; p = p->next) {
				if (ssi[p->type].size == 0) {
					continue;
				}
				if (i >= ns.nmemb) {
if (warn) {
fprintf(stderr,"SaveState warning: \
fewer mechanisms saved than exist at node %d of %s\n",
	inode, secname(sec));
}
					return false;
				}
				if (p->type != ns.type[i]) {
if (warn) {
fprintf(stderr, "SaveState warning: mechanisms out of order at node %d of %s\n\
saved %s but need %s\n",
inode, secname(sec), memb_func[i].sym->name, memb_func[p->type].sym->name);
}
					return false;
				}
				++i;
			}
			if (i != ns.nmemb) {
if (warn) {
fprintf(stderr, "SaveState warning: more mechanisms saved than exist at node %d of %s\n",
	inode, secname(sec));
}
				return false;
			}
		}

	}
	if (!checknet(warn)) { return false; }
	return true;
}

boolean SaveState::checknode(NodeState& ns, Node* nd, boolean warn) {
	int i=0;
	Prop* p;
	for (p = nd->prop; p; p = p->next) {
		if (ssi[p->type].size == 0) {
			continue;
		}
		if (i >= ns.nmemb) {
if (warn) {
fprintf(stderr,"SaveState warning: \
fewer mechanisms saved than exist at a root node\n");
}
			return false;
		}
		if (p->type != ns.type[i]) {
if (warn) {
fprintf(stderr, "SaveState warning: mechanisms out of order at a rootnode\n\
saved %s but need %s\n",
memb_func[i].sym->name, memb_func[p->type].sym->name);
}
			return false;
		}
		++i;
	}
	if (i != ns.nmemb) {
if (warn) {
fprintf(stderr, "SaveState warning: more mechanisms saved than exist at a rootnode\n");
}
		return false;
	}
	return true;
}

boolean SaveState::checkacell(ACellState& ac, int type, boolean warn) {
	if (memb_list[type].nodecount != ac.ncell) {
if (warn) {
fprintf(stderr, "SaveState warning: different number of %s saved than exist.\n",
memb_func[type].sym->name);
}
		return false;
	}
	return true;
}

void SaveState::alloc() {
	ssfree();
	int inode;
	nrootnode_ = rootnodecount;
	ns_ = new NodeState[nrootnode_];
	for (inode = 0; inode < nrootnode_; ++inode) {
		allocnode(ns_[inode], v_node[inode]);
	}
	nsec_ = section_count;
	ss_ = new SecState[nsec_];
	for (int isec=0; isec < nsec_; ++isec) {
		SecState& ss = ss_[isec];
		ss.sec = secorder[isec];
		section_ref(ss.sec);
		ss.nnode = ss.sec->nnode;
		ss.ns = new NodeState[ss.nnode];
		for (inode = 0; inode < ss.nnode; ++inode) {
			Node* nd = ss.sec->pnode[inode];
			NodeState& ns = ss.ns[inode];
			allocnode(ns, nd);
		}
	}
	for (int i=0, j=0; i < n_memb_func; ++i) if (nrn_is_artificial_[i]) {
		allocacell(acell_[j], i);
		++j;
	}
	PlayRecList* prl = net_cvode_instance_prl();
	nprs_ = prl->count();
	if (nprs_) {
		prs_ = new PlayRecordSave*[nprs_];
	}
	allocnet();
}

void SaveState::allocnode(NodeState& ns, Node* nd) {
	ns.nmemb = 0;
	ns.type = nil;
	ns.state = nil;
	ns.nstate = 0;
	Prop* p;
	for (p = nd->prop; p; p = p->next) {
		if (ssi[p->type].size == 0) {
			continue;
		}
		++ns.nmemb;
		ns.nstate += ssi[p->type].size;
	}
	if (ns.nmemb) {
		ns.type = new int[ns.nmemb];
	}
	if (ns.nstate) {
		ns.state = new double[ns.nstate];
	}
	int i = 0;
	for (p = nd->prop; p; p = p->next) {
		if (ssi[p->type].size == 0) {
			continue;
		}
		ns.type[i] = p->type;
		++i;
	}
}

void SaveState::allocacell(ACellState& ac, int type) {
	Memb_list& ml = memb_list[type];
	ac.type = type;
	ac.ncell = ml.nodecount;
	ac.state = new double[ac.ncell * ssi[type].size];
}

void SaveState::ssfree() {
	int i, isec, inode;
	for (inode=0; inode < nrootnode_; ++inode) {
		NodeState& ns = ns_[inode];
		if (ns.nmemb) {
			delete [] ns.type;
		}
		if (ns.nstate) {
			delete [] ns.state;
		}
	}
	if (nrootnode_) {
		delete [] ns_;
	}
	for (isec=0; isec < nsec_; ++isec) {
		SecState& ss = ss_[isec];
		for (inode = 0; inode < ss.nnode; ++inode) {
			NodeState& ns = ss.ns[inode];
			if (ns.nmemb) {
				delete [] ns.type;
			}
			if (ns.nstate) {
				delete [] ns.state;
			}
		}
		if (ss.nnode) {
			delete [] ss.ns;
		}
		if (ss.sec) { // got info from an fread but never checked it
			section_unref(ss.sec);
		}
	}
	if (nsec_) {
		delete [] ss_;
	}	
	nsec_ = 0;
	ss_ = nil;
	for (i=0; i < nacell_; ++i) {
		if (acell_[i].ncell) {
			delete [] acell_[i].state;
			acell_[i].state = 0;
			acell_[i].ncell = 0;
		}
	} // note we do not destroy the acell_.
	if (nncs_) {
		for (i = 0; i < nncs_; ++i) {
			if (ncs_[i].nstate) {
				delete [] ncs_[i].state;
			}
		}
		delete [] ncs_;
	}
	nncs_ = 0;
	ncs_ = nil;
	if (npss_) {
		delete [] pss_;
	}
	npss_ = 0;
	pss_ = nil;
	free_tq();
	if (nprs_) {
		for (i=0; i < nprs_; ++i) {
			delete prs_[i];
		}
		delete [] prs_;
	}
	nprs_ = 0;
}

void SaveState::save() {
	if (!check(false)) {
		alloc();
	}
	t_ = t;
	int inode;
	for (inode=0; inode < nrootnode_; ++inode) {
		savenode(ns_[inode], v_node[inode]);
	}
	for (int isec=0; isec < nsec_; ++isec) {
		SecState& ss = ss_[isec];
		Section* sec = ss.sec;
		for (inode = 0; inode < ss.nnode; ++inode) {
			NodeState& ns = ss.ns[inode];
			Node* nd = sec->pnode[inode];
			savenode(ns, nd);
		}
	}
	for (int i=0, j=0; i < n_memb_func; ++i) if (nrn_is_artificial_[i]) {
		saveacell(acell_[j], i);
		++j;
	}
	if (nprs_) {
		PlayRecList* prl = net_cvode_instance_prl();
		int i;
		assert(nprs_ == prl->count());
		for (i=0; i < nprs_; ++i) {
			prs_[i] = prl->item(i)->savestate_save();
		}
	}
	savenet();
}

void SaveState::savenode(NodeState& ns, Node* nd) {
	ns.v = NODEV(nd);
	int istate = 0;
	Prop* p;
	for (p = nd->prop; p; p = p->next) {
		if (ssi[p->type].size == 0) {
			continue;
		}
		int type = p->type;
		int max = ssi[type].offset + ssi[type].size;
#if EXTRACELLULAR
		if (type == EXTRACELL) {
			for (int i=0; i < nlayer; ++i) {
				ns.state[istate++] = nd->extnode->v[i];
			}
		}else
#endif
		{
			for (int ip=ssi[type].offset; ip < max; ++ip) {
				ns.state[istate++] = p->param[ip];
			}
		}
	}
}

void SaveState::saveacell(ACellState& ac, int type) {
	Memb_list& ml = memb_list[type];
	int sz = ssi[type].size;
	double* p = ac.state;
	for (int i = 0; i < ml.nodecount; ++i) {
		double* d = ml.data[i];
		for (int j=0; j < sz; ++j) {
			(*p++) = d[j];
		}
	}
}

void SaveState::restore(int type) {
	if (!check(true)) {
		hoc_execerror("SaveState:",
		"Stored state inconsistent with current neuron structure");
	}
	t = t_;
	int inode;
	for (inode=0; inode < nrootnode_; ++inode) {
		restorenode(ns_[inode], v_node[inode]);
	}
	for (int isec=0; isec < nsec_; ++isec) {
		SecState& ss = ss_[isec];
		Section* sec = ss.sec;
		for (inode = 0; inode < ss.nnode; ++inode) {
			NodeState& ns = ss.ns[inode];
			Node* nd = sec->pnode[inode];
			restorenode(ns, nd);
		}
	}
	for (int i=0, j=0; i < n_memb_func; ++i) if (nrn_is_artificial_[i]) {
		restoreacell(acell_[j], i);
		++j;
	}
	if (type == 1) { return; }
	PlayRecList* prl = net_cvode_instance_prl();
	// during a local step the PlayRecList is augmented with GLineRecord
	// assert(nprs_ == prl->count());
	assert(nprs_ <= prl->count());
	int i;
	for (i = 0; i < nprs_; ++i) {
		prs_[i]->savestate_restore();
	}
	restorenet();
}

void SaveState::restorenode(NodeState& ns, Node* nd) {
	NODEV(nd) = ns.v;;
	int istate = 0;
	Prop* p;
	for (p = nd->prop; p; p = p->next) {
		if (ssi[p->type].size == 0) {
			continue;
		}
		int type = p->type;
		int max = ssi[type].offset + ssi[type].size;
#if EXTRACELLULAR
		if (type == EXTRACELL) {
			for (int i=0; i < nlayer; ++i) {
				nd->extnode->v[i] = ns.state[istate++];
			}
		}else
#endif
		{
			for (int ip=ssi[type].offset; ip < max; ++ip) {
				p->param[ip] = ns.state[istate++];
			}
		}
	}
}

void SaveState::restoreacell(ACellState& ac, int type) {
	Memb_list& ml = memb_list[type];
	int sz = ssi[type].size;
	double* p = ac.state;
	for (int i = 0; i < ml.nodecount; ++i) {
		double* d = ml.data[i];
		for (int j=0; j < sz; ++j) {
			d[j] = (*p++);
		}
	}
}

void SaveState::read(OcFile* ocf, boolean close) {
	if (!ocf->open(ocf->get_name(), "r")) {
		hoc_execerror("Couldn't open file for reading:", ocf->get_name());
	}
	BinaryMode(ocf)
	FILE* f = ocf->file();
	ssfree();
	char buf[200];
	fgets(buf, 200, f);
	if (strcmp(buf, "SaveState binary file version 6.0\n") != 0) {
		ocf->close();
		hoc_execerror("Bad SaveState binary file", " Not version 6.0");
	}
	fread((char*)&t_, sizeof(double), 1, f);
//	fscanf(f, "%d %d\n", &nsec_, &nrootnode_);
// on some os's fscanf leaves file pointer at wrong place for next fread
//	can check it with ftell(f)
	fgets(buf, 200, f);
	sscanf(buf, "%d %d\n", &nsec_, &nrootnode_);
	ns_ = new NodeState[nrootnode_];
	// to enable comparison of SaveState files, we avoid
	// putting pointers in the files and instead explicitly read/write
	// structure elements.
	// fread((char*)ns_, sizeof(NodeState), nrootnode_, f);
	fread_NodeState(ns_, nrootnode_, f);
	for (int inode = 0; inode < nrootnode_; ++inode) {
		NodeState& ns = ns_[inode];
		if (ns.nmemb) {
			ns.type = new int[ns.nmemb];
			fread((char*)ns.type, sizeof(int), ns.nmemb, f);
		}
		if (ns.nstate) {
			ns.state = new double[ns.nstate];
			fread((char*)ns.state, sizeof(double), ns.nstate, f);
		}
	}
	ss_ = new SecState[nsec_];
	fread_SecState(ss_, nsec_, f);
	for (int isec=0; isec < nsec_; ++isec) {
		SecState& ss = ss_[isec];
		ss.sec = nil;
		ss.ns = new NodeState[ss.nnode];
		fread_NodeState(ss.ns, ss.nnode, f);
		for (int inode = 0; inode < ss.nnode; ++inode) {
			NodeState& ns = ss.ns[inode];
			if (ns.nmemb) {
				ns.type = new int[ns.nmemb];
				fread((char*)ns.type, sizeof(int), ns.nmemb, f);
			}
			if (ns.nstate) {
				ns.state = new double[ns.nstate];
				fread((char*)ns.state, sizeof(double), ns.nstate, f);
			}
		}
	}
	int n = 0;
	fgets(buf, 20, f);
	sscanf(buf, "%d\n", &n);
	assert(n == nacell_);
	for (int i=0, j=0; i < n_memb_func; ++i) if (nrn_is_artificial_[i]) {
		int nt = 0, nc = 0, ns = 0;
		fgets(buf, 200, f);
		assert(sscanf(buf, "%d %d %d\n", &nt, &nc, &ns) == 3);
		assert(nt == i && nc == memb_list[i].nodecount);
		assert(ns == nc * ssi[i].size);
		acell_[j].ncell = nc;
		acell_[j].state = new double[ns];
		fread((char*)acell_[j].state, sizeof(double), ns, f);
		++j;
	}
	fgets(buf, 20, f);
	sscanf(buf, "%d\n", &nprs_);
	if (nprs_) {
		prs_ = new PlayRecordSave*[nprs_];
		for (int i=0; i < nprs_; ++i) {
			prs_[i] = PlayRecord::savestate_read(f);
		}
	}
	readnet(f);
	if (close) {
		ocf->close();
	}
}

void SaveState::write(OcFile* ocf, boolean close) {
	if (!ocf->open(ocf->get_name(), "w")) {
		hoc_execerror("Couldn't open file for writing:", ocf->get_name());
	}
	BinaryMode(ocf)
	FILE* f = ocf->file();
	fprintf(f, "SaveState binary file version 6.0\n");
	fwrite((char*)&t_, sizeof(double), 1, f);
	fprintf(f, "%d %d\n", nsec_, nrootnode_);
	fwrite_NodeState(ns_, nrootnode_, f);
	for (int inode = 0; inode < nrootnode_; ++inode) {
		NodeState& ns = ns_[inode];
		if (ns.nmemb) {
			fwrite((char*)ns.type, sizeof(int), ns.nmemb, f);
		}
		if (ns.nstate) {
			fwrite((char*)ns.state, sizeof(double), ns.nstate, f);
		}
	}
	fwrite_SecState(ss_, nsec_, f);
	for (int isec=0; isec < nsec_; ++isec) {
		SecState& ss = ss_[isec];
		fwrite_NodeState(ss.ns, ss.nnode, f);
		for (int inode = 0; inode < ss.nnode; ++inode) {
			NodeState& ns = ss.ns[inode];
			if (ns.nmemb){
				fwrite((char*)ns.type, sizeof(int), ns.nmemb, f);
			}
			if (ns.nstate) {
				fwrite((char*)ns.state, sizeof(double), ns.nstate, f);
			}
		}
	}
	fprintf(f, "%d\n", nacell_);
	for (int i=0, j=0; i < n_memb_func; ++i) if (nrn_is_artificial_[i]) {
		int sz = acell_[j].ncell * ssi[i].size;
		fprintf(f, "%d %d %d\n", acell_[j].type, acell_[j].ncell, sz);
		fwrite((char*)acell_[j].state, sizeof(double), sz, f);
		++j;
	}
	fprintf(f, "%d\n", nprs_);
	for (int i=0; i < nprs_; ++i) {
		fprintf(f, "%d %d\n", prs_[i]->pr_->type(), i);
		prs_[i]->savestate_write(f);
	}
	writenet(f);
	if (close) {
		ocf->close();
	}
}

void SaveState::savenet() {
	int i, j, n;
	double* w;
	hoc_Item* q;
	Object* ob;
	NetCon* d;
	PreSyn* ps;
	i = 0;
	ITERATE(q, nct->olist) {
		ob = OBJ(q);
		d = (NetCon*)ob->u.this_pointer;
		n = ncs_[i].nstate;
		w = ncs_[i].state;
		for (j=0; j < n; ++j) {
			w[j] = d->weight_[j];
		}
		++i;
	}
	i = 0;
	if (net_cvode_instance_psl()) ITERATE(q, net_cvode_instance_psl()) {
		ps = (PreSyn*)VOIDITM(q);
		ps->hi_index_ = i;
		pss_[i].flag = ps->flag_;
		pss_[i].valthresh = ps->valthresh_;
		pss_[i].valold = ps->valold_;
		pss_[i].told = ps->told_;
		++i;
	}
	alloc_tq();
	TQueue* tq = net_cvode_instance_event_queue();
	tqcnt_ = 0;
	this_savestate = this; callback_mode = 1;
	tq->forall_callback(tqcallback);
}

void SaveState::tqcount(const TQItem*, int) {
	++tqcnt_;
}

void SaveState::tqsave(const TQItem* q, int) {
	DiscreteEvent* de = (DiscreteEvent*)q->data_;
	tqs_->tdeliver[tqcnt_] = q->t_;
	tqs_->items[tqcnt_] = de->savestate_save();
	++tqcnt_;
}

void SaveState::restorenet() {
	int i, j, n;
	double* w;
	hoc_Item* q;
	Object* ob;
	NetCon* d;
	PreSyn* ps;
	// NetCon's
	i = 0;
	ITERATE(q, nct->olist) {
		ob = OBJ(q);
		d = (NetCon*)ob->u.this_pointer;
		n = ncs_[i].nstate;
		w = ncs_[i].state;
		for (j=0; j < n; ++j) {
			d->weight_[j] = w[j];
		}
		++i;
	}
	// PreSyn's
	i = 0;
	if (net_cvode_instance_psl()) ITERATE(q, net_cvode_instance_psl()) {
		ps = (PreSyn*)VOIDITM(q);
		ps->hi_index_ == i;
		ps->flag_ = pss_[i].flag;
		ps->valthresh_ = pss_[i].valthresh;
		ps->valold_ = pss_[i].valold;
		ps->told_ = pss_[i].told;
		++i;
	}

	// event queue
	// clear it
	clear_event_queue();
	// restore it
	n = tqs_->nstate;
	for (i=0; i < n; ++i) {
		tqs_->items[i]->savestate_restore(tqs_->tdeliver[i], net_cvode_instance);
	}
}

void SaveState::readnet(FILE* f) {
	free_tq();
	char buf[200];
	fgets(buf, 200, f);
	sscanf(buf, "%d\n", &nncs_);
	if (nncs_ != 0) {
		ncs_ = new NetConState[nncs_];
	}
	int i, n, type;
	for (i=0; i < nncs_; ++i) {
		fgets(buf, 200, f);
		sscanf(buf, "%d %d\n", &ncs_[i].object_index, &ncs_[i].nstate);
		if (ncs_[i].nstate) {
			ncs_[i].state = new double[ncs_[i].nstate];
			fread((char*)ncs_[i].state, sizeof(double), ncs_[i].nstate, f);
		}
	}
	// PreSyn's
	fgets(buf, 200, f);
	sscanf(buf, "%d\n", &npss_);
	if (npss_ != 0) {
		pss_ = new PreSynState[npss_];
		fread((char*)pss_, sizeof(PreSynState), npss_, f);
		PreSyn* ps;
		i = 0;
		hoc_Item* q;
		if (net_cvode_instance_psl()) ITERATE(q, net_cvode_instance_psl()) {
			ps = (PreSyn*)VOIDITM(q);
			ps->hi_index_ = i;
			++i;
		}
		assert(npss_ == i);
	}

	fgets(buf, 200, f);
	sscanf(buf, "%d\n", &n);
	tqs_->nstate = n;
	if (n) {
		tqs_->items = new DiscreteEvent*[n];
		tqs_->tdeliver = new double[n];
		fread((char*)tqs_->tdeliver, sizeof(double), n, f);
		for (i=0; i < n; ++i) {
			DiscreteEvent* de = nil;
			fgets(buf, 200, f);
			sscanf(buf, "%d\n", &type);
			switch(type) {
			case DiscreteEventType:
				de = DiscreteEvent::savestate_read(f);
				break;
			case TstopEventType:
				de = TstopEvent::savestate_read(f);
				break;
			case NetConType:
				de = NetCon::savestate_read(f);
				break;
			case SelfEventType:
				de = SelfEvent::savestate_read(f);
				break;
			case PreSynType:
				de = PreSyn::savestate_read(f);
				break;
			case HocEventType:
				de = HocEvent::savestate_read(f);
				break;
			case PlayRecordEventType:
				de = PlayRecordEvent::savestate_read(f);
				break;
#if NRNMPI
			case NetParEventType:
				de = NetParEvent::savestate_read(f);
				break;
#endif
			default:
				hoc_execerror("SaveState::readnet", "Unimplemented DiscreteEvent type");
				break;
			}
			tqs_->items[i] = de;
		}
	}
}

void SaveState::writenet(FILE* f) {
	fprintf(f, "%d\n", nncs_);
	int i, n;
	for (i=0; i < nncs_; ++i) {
		fprintf(f, "%d %d\n", ncs_[i].object_index, ncs_[i].nstate);
		if (ncs_[i].nstate) {
			fwrite((char*)ncs_[i].state, sizeof(double), ncs_[i].nstate, f);
		}
	}
	fprintf(f, "%d\n", npss_);
	if (npss_) {
		fwrite((char*)pss_, sizeof(PreSynState), npss_, f);
	}
	n = tqs_->nstate;
	fprintf(f, "%d\n", n);
	if (n) {
		fwrite((char*)tqs_->tdeliver, sizeof(double), n, f);
		for (i=0; i < n; ++i) {
			tqs_->items[i]->savestate_write(f);
		}
	}
}

boolean SaveState::checknet(boolean warn) {
	if (nncs_ != nct->count) {
		if (warn) {
fprintf(stderr, "SaveState warning: There are %d NetCon but %d saved\n",
 nct->count, nncs_);
		}
		return false;
	}
	int i;
	hoc_Item* q;
	Object* ob;
	NetCon* d;
	i = 0;
	ITERATE(q, nct->olist) {
		ob = OBJ(q);
		d = (NetCon*)ob->u.this_pointer;
		if (ob->index != ncs_[i].object_index) {
			if (warn) {
fprintf(stderr, "SaveState warning: %s is matched with NetCon[%d]\n",
 hoc_object_name(ob), ncs_[i].object_index);
			}
			return false;
		}
		if (d->cnt_ != ncs_[i].nstate) {
			if (warn) {
fprintf(stderr, "SaveState warning: %s has %d weight states but saved %d\n",
 hoc_object_name(ob), d->cnt_, ncs_[i].nstate);
			}
			return false;
		}
		++i;
	}
	// PreSyn's
	i = 0;
	if (net_cvode_instance_psl()) ITERATE(q, net_cvode_instance_psl()) {
		++i;
	}
	if (npss_ != i) {
		if (warn) {
fprintf(stderr, "SaveState warning: There are %d internal PreSyn but %d saved\n",
 i, npss_);
		}
		return false;
	}
	return true;
}

void SaveState::allocnet() {
	nncs_ = nct->count;
	if (nncs_ != 0) {
		ncs_ = new NetConState[nncs_];
	}
	int i, n;
	hoc_Item* q;
	Object* ob;
	NetCon* d;
	i = 0;
	ITERATE(q, nct->olist) {
		ob = OBJ(q);
		d = (NetCon*)ob->u.this_pointer;
		ncs_[i].object_index = ob->index;
		ncs_[i].nstate = d->cnt_;
		if (d->cnt_) {
			ncs_[i].state = new double[d->cnt_];
		}
		++i;
	}
	PreSyn* ps;
	npss_ = 0;
	if (net_cvode_instance_psl()) ITERATE(q, net_cvode_instance_psl()) {
		ps = (PreSyn*)VOIDITM(q);
		ps->hi_index_ = npss_;
		++npss_;
	}
	if (npss_ != 0) {
		pss_ = new PreSynState[npss_];
	}		
}

//The event TQueue is highly volatile so it needs to be freed and allocated
// on every save and fread
void SaveState::free_tq() {
	int i;
	if (tqs_->nstate) {
		for (i=0; i < tqs_->nstate; ++i) {
			delete tqs_->items[i];
		}
		tqs_->nstate = 0;
		delete [] tqs_->items;
		delete [] tqs_->tdeliver;
	}
}
void SaveState::alloc_tq() {
	int n;
	free_tq();
	TQueue* tq = net_cvode_instance_event_queue();
	tqcnt_ = 0;
	this_savestate = this; callback_mode = 0;
	tq->forall_callback(tqcallback);
	n = tqcnt_;	
	tqs_->nstate = n;
	if (n) {
		tqs_->items = new DiscreteEvent*[n];
		tqs_->tdeliver = new double[n];
	}
}

static void* cons(Object*) {
	SaveState* ss = new SaveState();
	ss->ref();
	return (void*)ss;
}

static void destruct(void* v) {
	SaveState* ss = (SaveState*)v;
	Resource::unref(ss);
}

static double save(void* v) {
	SaveState* ss = (SaveState*)v;
	ss->save();
	return 1.;
}

static double restore(void* v) {
	SaveState* ss = (SaveState*)v;
	int type = 0;
	if (ifarg(1)) {
		type = (int)chkarg(1, 0, 1);
	}
	ss->restore(type);
	return 1.;
}

static double ssread(void* v) {
	boolean close = true;
	SaveState* ss = (SaveState*)v;
	Object* obj = *hoc_objgetarg(1);
	check_obj_type(obj, "File");
	if (ifarg(2)) { close = chkarg(2, 0, 1) ? true : false; }
	OcFile* f = (OcFile*)obj->u.this_pointer;
	ss->read(f, close);
	return 1.;
}

static double sswrite(void* v) {
	boolean close = true;
	SaveState* ss = (SaveState*)v;
	Object* obj = *hoc_objgetarg(1);
	check_obj_type(obj, "File");
	if (ifarg(2)) { close = chkarg(2, 0, 1) ? true : false; }
	OcFile* f = (OcFile*)obj->u.this_pointer;
	ss->write(f, close);
	return 1.;
}

static Member_func members[] = {
	"save", save,
	"restore", restore,
	"fread", ssread,
	"fwrite", sswrite,
	0, 0
};

void SaveState_reg() {
	class2oc("SaveState", cons, destruct, members, 0);
}