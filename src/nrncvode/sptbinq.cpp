// splay tree + bin queue limited to fixed step method
// for event-sets or priority queues
// this starts from the sptqueue.cpp file and adds a bin queue

/* Derived from David Brower's c translation of pascal code by
Douglas Jones.
*/
/* The original c code is included from this file but note that instead
of struct _spblk, we are really using TQItem
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define SPBLK TQItem
#define leftlink left_
#define rightlink right_
#define uplink parent_
#define cnt cnt_
#define key t_
#include <sptree.h>

extern "C" {
extern double dt;
}

TQItem::TQItem() {
	left_ = 0;
	right_ = 0;
	parent_ = 0;
}

TQItem::~TQItem() {
}

static void deleteitem(TQItem* i) { // only one, semantics changed
	assert(i);
	tpool_->hpfree(i);
}

boolean TQItem::check() {
#if DOCHECK
#endif
	return true;
}

static void prnt(const TQItem* b, int level) {
	int i;
	for (i=0; i < level; ++i) {
		printf("    ");
	}
	printf("%g %c %d Q=%lx D=%lx\n", b->t_, b->data_?'x':'o', b->cnt_, (long)b, (long)b->data_);
}

static void chk(TQItem* b, int level) {
	if (!b->check()) {
		hoc_execerror("chk failed", errmess_);
	}
}

TQueue::TQueue() {
	if (!tpool_) {
		tpool_ = new TQItemPool(1000);
	}
	nshift_ = 0;
	sptree_ = new SPTREE;
	spinit(sptree_);
	binq_ = new BinQ;
	least_ = 0;

#if COLLECT_TQueue_STATISTICS
	nmove = ninsert = nrem = nleast = nbal = ncmplxrem = 0;
	nfastmove = ncompare = nleastsrch = nfind = nfindsrch = 0;
#endif
}

TQueue::~TQueue() {
	SPBLK* q, *q2;
	while((q = spdeq(&sptree_->root)) != nil) {
		deleteitem(q);
	}
	delete sptree_;
	for (q = binq_->first(); q; q = q2) {
		q2 = binq_->next(q);
		remove(q);
	}
	delete binq_;
}
	
void TQueue::print() {
#if FAST_LEAST
	if (least_) {
		prnt(least_, 0);
	}
#endif
	spscan(prnt, nil, sptree_);
	for (TQItem* q = binq_->first(); q; q = binq_->next(q)) {
		prnt(q, 0);
	}
}

void TQueue::forall_callback(void(*f)(const TQItem*, int)) {
#if FAST_LEAST
	if (least_) {
		f(least_, 0);
	}
#endif
	spscan(f, nil, sptree_);
	for (TQItem* q = binq_->first(); q; q = binq_->next(q)) {
		f(q, 0);
	}
}

void TQueue::check(const char* mes) {
}

void TQueue::move_least(double tnew) {
	TQItem* b = least();
	if (b) {
		b->t_ = tnew;
		TQItem* nl = sphead(sptree_);
		if (nl) {
			if (tnew > nl->t_) {
				least_ = spdeq(&sptree_->root);
				spenq(b, sptree_);
			}
		}
	}
}

void TQueue::move(TQItem* i, double tnew) {
	STAT(nmove)
	if (i == least_) {
		move_least(tnew);
	}else if (tnew < least_->t_) {
		spdelete(i, sptree_);
		i->t_ = tnew;
		spenq(least_, sptree_);
		least_ = i;
	}else{
		spdelete(i, sptree_);
		i->t_ = tnew;
		spenq(i, sptree_);
	}
}

void TQueue::statistics() {
#if COLLECT_TQueue_STATISTICS
	printf("insertions=%lu  moves=%lu removals=%lu calls to least=%lu\n",
		ninsert, nmove, nrem, nleast);
	printf("calls to find=%lu\n",
		nfind);
	printf("comparisons=%lu\n",
		sptree_->enqcmps);
#else
	printf("Turn on COLLECT_TQueue_STATISTICS_ in tqueue.h\n");
#endif
}

void TQueue::spike_stat(double* d) {
#if COLLECT_TQueue_STATISTICS
	d[0] = ninsert;
	d[1] = nmove;
	d[2] = nrem;
//printf("FifoQ spikestat nfenq=%lu nfdeq=%lu nfrem=%lu\n", fifo_->nfenq, fifo_->nfdeq, fifo_->nfrem);
#endif
}

TQItem* TQueue::insert(double t, void* d) {
	STAT(ninsert);
	TQItem* i = tpool_->alloc();
	i->data_ = d;
	i->t_ = t;
	i->cnt_ = -1;
	if (t < least_t()) {
		if (least()) {
			spenq(least(), sptree_);
		}
		least_ = i;
	}else{
		spenq(i, sptree_);
	}
	return i;
}

TQItem* TQueue::enqueue_bin(double td, void* d) {
	STAT(ninsert);
	TQItem* i = tpool_->alloc();
	i->data_ = d;
	binq_->enqueue(td, i);
	return i;
}

void TQueue::release(TQItem* q) {
	tpool_->hpfree(q);
}

void TQueue::remove(TQItem* q) {
	STAT(nrem);
	if (q) {
		if (q == least_) {
			if (sptree_->root) {
				least_ = spdeq(&sptree_->root);
			}else{
				least_ = nil;
			}
		}else if (q->cnt_ >= 0) {
			binq_->remove(q);
		}else{
			spdelete(q, sptree_);
		}
		tpool_->hpfree(q);
	}
}

TQItem* TQueue::find(double t) {
	// search only in the  splay tree. if this is a bug then fix it.
	STAT(nfind)
	if (t == least_t()) {
		return least();
	}
	TQItem* q;
	q = splookup(t, sptree_);
	return(q);
}

BinQ::BinQ() {
	nbin_ = 1000;
	bins_ = new TQItem*[nbin_];
	for (int i=0; i < nbin_; ++i) { bins_[i] = 0; }
	qpt_ = 0;
	tt_ = 0.;
#if COLLECT_TQueue_STATISTICS
	nfenq = nfdeq = nfrem = 0;
#endif
}

BinQ::~BinQ() {
	for (int i=0; i < nbin_; ++i) {
		assert(!bins_[i]);
	}
	delete [] bins_;
}

void BinQ::resize(int size) {
	//printf("BinQ::resize from %d to %d\n", nbin_, size);
	int i, j;
	TQItem* q;
	assert(size >= nbin_);
	TQItem** bins = new TQItem*[size];
	for (i=nbin_; i < size; ++i) { bins[i] = 0; }
	for (i=0, j=qpt_; i < nbin_; ++i, ++j) {
		if (j >= nbin_) { j = 0; }
		bins[i] = bins_[j];
		for (q = bins[i]; q; q = q->left_) {
			q->cnt_ = i;
		}
	}
	delete [] bins_;
	bins_ = bins;
	nbin_ = size;
	qpt_ = 0;
}
void BinQ::enqueue(double td, TQItem* q) {
	int idt = (int)((td - tt_)/dt + 1.e-10);
	assert(idt >= 0);
	if (idt >= nbin_) {
		resize(idt + 100);
	}
	//assert (idt < nbin_);
	idt += qpt_;
	if (idt >= nbin_) { idt -= nbin_; }
//printf("enqueue idt=%d qpt=%d\n", idt, qpt_);
	assert (idt < nbin_);
	q->cnt_ = idt; // only for iteration
	q->left_ = bins_[idt];
	bins_[idt] = q;
#if COLLECT_TQueue_STATISTICS
	++nfenq;
#endif
}
TQItem* BinQ::dequeue() {
	TQItem* q = bins_[qpt_];
	if (q) {
		bins_[qpt_] = q->left_;
#if COLLECT_TQueue_STATISTICS
		++nfdeq;
#endif
	}
	return q;
}

TQItem* BinQ::first() {
	for (int i = 0; i < nbin_; ++i) {
		if (bins_[i]) {
			return bins_[i];
		}
	}
	return 0;
}
TQItem* BinQ::next(TQItem* q) {
	if (q->left_) { return q->left_; }
	for (int i = q->cnt_ + 1; i < nbin_; ++i) {
		if (bins_[i]) {
			return bins_[i];
		}
	}
	return 0;
}

void BinQ::remove(TQItem* q) {
	TQItem* q1, *q2;
	q1 = bins_[q->cnt_];
	if (q1 == q) {
		bins_[q->cnt_] = q->left_;
		return;
	}
	for (q2 = q1->left_; q2; q1 = q2, q2 = q2->left_) {
		if (q2 == q) {
			q1->left_ = q->left_;
			return;
		}
	}
}

#include <spaux.c>
#include <sptree.c>
#include <spdaveb.c>
