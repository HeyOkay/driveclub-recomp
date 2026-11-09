/* Caller receives binding METADATA only; no point/value/epoch is resurrected.
 * Real native ctor/dtor/source rebind retirement clears active and fails this.
 * Ordinary DATA invalidation preserves active binding with UNAVAILABLE status. */
static int lsq_unavailable_binding(const LsqSelector*x,LsqBinding*out,uint64_t*token){
 if(!lsq_enabled||!out||!token)return 0;const LsqTrack*t=NULL;int ok=0;
 AcquireSRWLockShared(&lsq_lock);int status=lsq_select_track(&lsq_store,x,&t);
 if(status!=LSQ_AMBIGUOUS&&t&&t->active&&!t->count&&t->status==LSQ_UNAVAILABLE){*out=t->binding;*token=t->epoch;ok=1;}
 ReleaseSRWLockShared(&lsq_lock);return ok;
}
