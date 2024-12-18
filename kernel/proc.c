#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#define compLT(a,b) (a < b)
#define compEQ(a,b) (a == b)
#define NIL (&sentiproc)  

struct cpu cpus[NCPU];

struct proc proc[NPROC];

struct proc *initproc;

int nextpid = 1;
struct spinlock pid_lock;
int flag=0;
int timerflag=0;
int printflag;
int flagval;
int firstexit;
struct spinlock rbtree_lock;
void pr();
struct proc* getminproc();
// Added code here
int totweight=0;  // Total weight of all runnable processes
uint64 timeepoch=200000;
uint64 min_gran=10000; //minimum granularity
int numproc=0; 
int just_scheduled[NCPU];
int counter[NPROC];

int pri_to_weight[40] = {
 /* -20 */     88761,     71755,     56483,     46273,     36291,
 /* -15 */     29154,     23254,     18705,     14949,     11916,
 /* -10 */      9548,      7620,      6100,      4904,      3906,
 /*  -5 */      3121,      2501,      1991,      1586,      1277,
 /*   0 */      1024,       820,       655,       526,       423,
 /*   5 */       335,       272,       215,       172,       137,
 /*  10 */       110,        87,        70,        56,        45,
 /*  15 */        36,        29,        23,        18,        15,
};
struct proc sentiproc;      //sentinel proc for RB tree
struct proc *min_proc;      //pointer to the process with min_vruntime
struct proc *root;          // root of Red-Black tree 

// checks equality of p with NIL in Red-Black tree
int eqNIL(struct proc* p){
  if(p->ln==NIL->ln && p->rn==NIL->rn && p->pn==NIL->pn
    && p->vruntime==NIL->vruntime && p->color==NIL->color)
    return 1;
  return 0;
}
struct proc* getminproc();

//swaps the values of the processes pointed at by u and v
void swapval(struct proc *u, struct proc *v) {     
    struct proc temp;

    temp.lock = u->lock;
    temp.state = u->state;
    temp.chan = u->chan;
    temp.killed = u->killed;
    temp.xstate = u->xstate;
    temp.pid = u->pid;
    temp.parent = u->parent;
    temp.kstack = u->kstack;
    temp.sz = u->sz;
    temp.pagetable = u->pagetable;
    temp.tf = u->tf;
    temp.context = u->context;
    for(int i=0; i < NOFILE; i++){
        temp.ofile[i] = u->ofile[i];
    }
    temp.cwd = u->cwd;
    for(int i=0 ; i<16; i++){
        temp.name[i] = u->name[i];
    }
    temp.pri = u->pri;
    temp.weight = u->weight;
    temp.vruntime = u->vruntime;
    temp.aruntime = u->aruntime;
    temp.vruntime2 = u->vruntime2;
    temp.aruntime2 = u->aruntime2;
    temp.starttime = u->starttime;
    temp.interval = u->interval;
    temp.noof=u->noof;
    temp.avgvrun=u->avgvrun;
    temp.avgarun=u->avgarun;
    temp.invrun=u->invrun;
    temp.startstarttime=u->startstarttime;    

    u->lock = v->lock;
    u->state = v->state;
    u->chan = v->chan;
    u->killed = v->killed;
    u->xstate = v->xstate;
    u->pid = v->pid;
    u->parent = v->parent;
    u->kstack = v->kstack;
    u->sz = v->sz;
    u->pagetable = v->pagetable;
    u->tf = v->tf;
    u->context = v->context;
    for(int i=0; i < NOFILE; i++){
        u->ofile[i] = v->ofile[i];
    }
    u->cwd = v->cwd;
    for(int i=0 ; i<16; i++){
        u->name[i] = v->name[i];
    }
    u->pri = v->pri;
    u->weight = v->weight;
    u->vruntime = v->vruntime;
    u->aruntime = v->aruntime;
    u->vruntime2 = v->vruntime2;
    u->aruntime2 = v->aruntime2;
    u->starttime = v->starttime;
    u->interval = v->interval;
    u->noof=v->noof;
    u->avgvrun=v->avgvrun;
    u->avgarun=v->avgarun;
    u->invrun=v->invrun;
    u->startstarttime=v->startstarttime;

    v->lock = temp.lock;
    v->state = temp.state;
    v->chan = temp.chan;
    v->killed = temp.killed;
    v->xstate = temp.xstate;
    v->pid = temp.pid;
    v->parent = temp.parent;
    v->kstack = temp.kstack;
    v->sz = temp.sz;
    v->pagetable = temp.pagetable;
    v->tf = temp.tf;
    v->context = temp.context;
    for(int i=0; i < NOFILE; i++){
        v->ofile[i] = temp.ofile[i];
    }
    v->cwd = temp.cwd;
    for(int i=0 ; i<16; i++){
        v->name[i] = temp.name[i];
    }
    v->pri = temp.pri;
    v->weight = temp.weight;
    v->vruntime = temp.vruntime;
    v->aruntime = temp.aruntime;
    v->vruntime2 = temp.vruntime2;
    v->aruntime2 = temp.aruntime2;
    v->starttime = temp.starttime;
    v->interval = temp.interval;
    v->noof=temp.noof;
    v->avgvrun=temp.avgvrun;
    v->avgarun=temp.avgarun;
    v->invrun=temp.invrun;
    v->startstarttime=temp.startstarttime;
}

//rotate node x to left
void rotateLeft(struct proc *x) { 
    struct proc *y = x->rn;
    
    x->rn = y->ln;   //establish x->rn link
    if (!eqNIL(y->ln)) y->ln->pn = x;
    
    if (!eqNIL(y)) y->pn = x->pn;  //establish y->pn link
    if (x->pn) {
        if (x == x->pn->ln)
            x->pn->ln = y;
        else
            x->pn->rn = y;
    } else {
        root = y;
    }

    y->ln = x;  //link x and y
    if (!eqNIL(x)) x->pn = y;
}

//rotate node x to right
void rotateRight(struct proc *x) { 
    struct proc *y = x->ln;
    
    x->ln = y->rn;  // establish x->ln link 
    if (!eqNIL(y->rn)) y->rn->pn = x;
    
    if (!eqNIL(y)) y->pn = x->pn;  //establish y->pn link
    if (x->pn) {
        if (x == x->pn->rn)
            x->pn->rn = y;
        else
            x->pn->ln = y;
    } else {
        root = y;
    }
    
    y->rn = x;   //link x and y
    if (!eqNIL(x)) x->pn = y;
}

//maintain Red-Black tree balance 
//after inserting node x
void insertFixup(struct proc *x) { 
    while (x != root && x->pn->color == 1) { //check Red-Black properties 
        if (x->pn == x->pn->pn->ln) {   // we have a violation
            struct proc *y = x->pn->pn->rn;
            if (y->color == 1) {  //uncle is 1
                x->pn->color = 0;
                y->color = 0;
                x->pn->pn->color = 1;
                x = x->pn->pn;
            } else {  //uncle is 0
                if (x == x->pn->rn) {  //make x a left child
                    x = x->pn;
                    rotateLeft(x);
                }
                x->pn->color = 0;  //recolor and rotate
                x->pn->pn->color = 1;
                rotateRight(x->pn->pn);
            }
        } else {  //mirror image of above code
            struct proc *y = x->pn->pn->ln;
            if (y->color == 1) {  //uncle is 1
                x->pn->color = 0;
                y->color = 0;
                x->pn->pn->color = 1;
                x = x->pn->pn;
            } else {  //uncle is 0 
                if (x == x->pn->ln) {
                    x = x->pn;
                    rotateRight(x);
                }
                x->pn->color = 0;
                x->pn->pn->color = 1;
                rotateLeft(x->pn->pn);
            }
        }
    }
    root->color = 0;
    return; 
}

//allocate node for process and insert in tree
void insertproc(struct proc *x) {  
    struct proc *current, *parent;
    uint64 vruntime;
    vruntime = x->vruntime;
   
    // find where node belongs 
    current = root;
    parent = 0;
    while (!eqNIL(current)) {
        if (compEQ(vruntime, current->vruntime)){
            panic("added same vruntime"); 
            return;
        }
        parent = current;
        current = compLT(vruntime, current->vruntime) ?
        current->ln : current->rn;
    }
    // setup the node for insertion
    x->pn = parent;
    x->ln = NIL;
    x->rn = NIL;
    x->color = 1;
    
    if(parent) {  //insert node in tree
        if(compLT(vruntime, parent->vruntime))
            parent->ln = x;
        else
            parent->rn = x;
    } else {
        root = x;
    }

    insertFixup(x);
    min_proc = getminproc();
    return;
}

// maintain Red-Black tree balance
// after deleting node x 
void deleteFixup(struct proc *x) {  
    //Fix x and then the parent and so on
    while (x != root && x->color == 0) {
        if (x == x->pn->ln) {
            struct proc *w = x->pn->rn;
            if (w->color == 1) {    // Red-black property violation
                w->color = 0;
                x->pn->color = 1;
                rotateLeft (x->pn);
                w = x->pn->rn;
            }
            if (w->ln->color == 0 && w->rn->color == 0) {
                w->color = 1;
                x = x->pn;
                // sibling is fixed, fix parent
            } else {        //property violation
                if (w->rn->color == 0) {
                    w->ln->color = 0;
                    w->color = 1;
                    rotateRight (w);
                    w = x->pn->rn;
                }
                w->color = x->pn->color;
                x->pn->color = 0;
                w->rn->color = 0;
                rotateLeft (x->pn);
                x = root;
            }
        } else {        //mirror of code above
            struct proc *w = x->pn->ln;
            if (w->color == 1) {
                w->color = 0;
                x->pn->color = 1;
                rotateRight (x->pn);
                w = x->pn->ln;
            }
            if (w->rn->color == 0 && w->ln->color == 0) {
                w->color = 1;
                x = x->pn;
            } else {
                if (w->ln->color == 0) {
                    w->rn->color = 0;
                    w->color = 1;
                    rotateLeft (w);
                    w = x->pn->ln;
                }
                w->color = x->pn->color;
                x->pn->color = 0;
                w->ln->color = 0;
                rotateRight (x->pn);
                x = root;
            }
        }
    }
    x->color = 0;
}

//delete node z from tree
void deleteproc(struct proc *z) {   
    struct proc *x, *y;
    if (eqNIL(z)) return;

    if (eqNIL(z->ln) || eqNIL(z->rn)) {  //y has a NIL node as a child
        y = z;
    } else {    // find tree successor with a NULL node as a child
        y = z->rn;
        while (!eqNIL(y->ln)) y = y->ln;
    }
    // x is y's only child 
    if (!eqNIL(y->ln))
        x = y->ln;
    else
        x = y->rn;
    
    x->pn = y->pn;     // remove y from the parent chain 
    if (y->pn)
        if (y == y->pn->ln)
            y->pn->ln = x;
        else
            y->pn->rn = x;
        else
            root = x;
    
    if (y != z) swapval(y,z);
    
    //Now, x is such that it has at-most one non-leaf child 
    if (y->color == 0)
        deleteFixup (x);
}

//find process with given vruntime in tree
struct proc* findproc(int vruntime) {  
    struct proc *current = root;
    while(!eqNIL(current)){
        if(compEQ(vruntime, current->vruntime))
            return (current);
        else{
            current = compLT(vruntime, current->vruntime) ? current->ln : current->rn;
        }
    }
    return(0);
}

//Returns the process with the minimum vruntime 
//and deletes it from the process tree
struct proc* getminproc(){  
    struct proc* current;
    current = root;
    while(!eqNIL(current->ln)){
        current = current->ln;
    }

    return current;
}

//Finding the scheduling period
uint64 get_timeepoch(){
    uint64 interval = timeepoch;
    if(numproc<0)  panic("numproc negative");
    if((numproc)&&(min_gran>(interval/numproc))){
        interval=min_gran*numproc;
    }
    if(!interval)  panic("interval negative");
    return interval;
}

//This function gets the dynamic timeslice for a process
uint64 get_timeslice(struct proc *p){
  uint64 timeslice;
  if(p->weight<=0)  panic("weight negative");

  timeslice = (get_timeepoch()*p->weight)/totweight;
  return timeslice;
}

//Updates the virtual runtime (vruntime) and the
//actual runtime(aruntime) of a process
void update_runtime(struct proc *p, uint64 time_run){
  p->vruntime = p->vruntime + (100 + ((time_run*1024)/p->weight) * 100 );
  p->aruntime = p->aruntime + time_run;
}

//Initializes the vruntime of a process
void initial_vruntime(struct proc *p){
  if(min_proc->vruntime > 100)
    p->vruntime=(min_proc->vruntime/100)*100+p->index;
  p->invrun=p->vruntime;
}

//Calls functions to update the runtime of a process 
//and adds the process to the Red-Black tree
void add_proc(struct proc *p, int i, uint64 time_run){
  if(i == 0){  //the process is being added to the tree for the first time
    initial_vruntime(p);
  } else if(i == 1) {  //the process is being added back into the tree
    update_runtime(p,time_run);
  }

  acquire(&rbtree_lock);
  numproc++;
  totweight = totweight + p->weight;

  if(p->prevweight != p->weight){
    totweight = totweight + p->prevweight - p->weight;
    p->weight=p->prevweight;
  }

  insertproc(p);  //inserts the process with updated vruntime in the tree
  release(&rbtree_lock);
}

// This function is called in scheduler to 
// remove the process with min vruntime from the 
//  process tree and return it
struct proc* get_next_proc(){
    struct proc* current;
    current = min_proc;
    if(eqNIL(current))  return current;
    
    deleteproc(min_proc);    //deleting min_proc from the RB tree
    min_proc = getminproc();  //finding the next min_proc in tree

    return current;
}


void prntree(struct proc* p){  //printing tree inorder for debugging
    if(eqNIL(p))  return;
    if(p->ln != NIL) 
        prntree(p->ln);
    int vruntime = (int) p->vruntime;
    printf("%d, %s %d\n", p->pid, p->name, vruntime);
    if(p->rn != NIL) 
        prntree(p->rn);
    return;
}

void pr(){  //calls printtree
    printf("printing tree\n");
    printf("root is %s\n",root->name);
    prntree(root);
    printf("\n");
}
/////////////////////////////////

extern void forkret(void);
static void wakeup1(struct proc *chan);

extern char trampoline[]; // trampoline.S
void
initializeCPU(void)
{
  for(int id = 0;id<NCPU;id++)
  {
    cpus[id].just_scheduled = 0;
  }
}

void
procinit(void)
{
  struct proc *p;
  
  initlock(&pid_lock, "nextpid");
  int ind=0;
  for(p = proc; p < &proc[NPROC]; p++) {
    initlock(&p->lock, "proc");
     // Allocate a page for the process's kernel stack.
     // Map it high in memory, followed by an invalid
     // guard page.
    char *pa = kalloc();
    if(pa == 0)
      panic("kalloc");
    uint64 va = KSTACK((int) (p - proc));
    kvmmap(va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
    p->kstack = va;
    // Code added here
    p->ln=NIL;
    p->rn=NIL;
    p->pn=0;
    p->color=0;
    p->vruntime=ind+1;
    p->aruntime=0;
    p->starttime=0;
    p->interval=0;
    p->weight=1024;
    p->noof=0;
    p->invrun=0;
    p->avgvrun=0;
    p->avgarun=0;
    p->prevweight=1024;
    p->pri=0;
    p->startstarttime=0;
    p->index=ind++;
  }

  flag=0;
  firstexit=0;
  timerflag=0;
  int i=0;
  for(i=0;i<NCPU;i++){
    just_scheduled[i]=0;
  }
  sentiproc.ln = NIL;
  sentiproc.rn = NIL;
  sentiproc.pn = 0;
  sentiproc.vruntime = 0;
  sentiproc.color = 0;
  min_proc=NIL;
  root=NIL;

  kvminithart();
}

// Must be called with interrupts disabled,
// to prevent race with process being moved
// to a different CPU.
int
cpuid()
{
  int id = r_tp();
  return id;
}

// Return this CPU's cpu struct.
// Interrupts must be disabled.
struct cpu*
mycpu(void) {
  int id = cpuid();
  struct cpu *c = &cpus[id];
  return c;
}

// Return the current struct proc *, or zero if none.
struct proc*
myproc(void) {
  push_off();
  struct cpu *c = mycpu();
  struct proc *p = c->proc;
  pop_off();
  return p;
}

int
allocpid() {
  int pid;
  
  acquire(&pid_lock);
  pid = nextpid;
  nextpid = nextpid + 1;
  release(&pid_lock);

  return pid;
}

// Look in the process table for an UNUSED proc.
// If found, initialize state required to run in the kernel,
// and return with p->lock held.
// If there are no free procs, return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state == UNUSED) {
      goto found;
    } else {
      release(&p->lock);
    }
  }
  return 0;

found:
  p->pid = allocpid();
  // Allocate a trapframe page.
  if((p->tf = (struct trapframe *)kalloc()) == 0){
    release(&p->lock);
    return 0;
  }

  // An empty user page table.
  p->pagetable = proc_pagetable(p);

  // Set up new context to start executing at forkret,
  // which returns to user space.
  memset(&p->context, 0, sizeof p->context);
  p->context.ra = (uint64)forkret;
  p->context.sp = p->kstack + PGSIZE;

  return p;
}

// free a proc structure and the data hanging from it,
// including user pages.
// p->lock must be held.
static void
freeproc(struct proc *p)
{
  if(p->tf)
    kfree((void*)p->tf);
  p->tf = 0;
  if(p->pagetable)
    proc_freepagetable(p->pagetable, p->sz);
  p->pagetable = 0;
  p->sz = 0;
  p->pid = 0;
  p->parent = 0;
  p->name[0] = 0;
  p->chan = 0;
  p->killed = 0;
  p->xstate = 0;
  p->state = UNUSED;

// Added Code Here
  p->ln=NIL;
  p->rn=NIL;
  p->pn=0;
  p->color=0;
  p->vruntime=1+p->index;
  p->aruntime=0;
  p->starttime=0;
  p->interval=0;
  p->noof=0;
  p->weight=1024;
  p->prevweight=1024;
  p->pri=0;
  p->invrun=0;
  p->avgvrun=0;
  p->avgarun=0;
  p->startstarttime=0;

}

// Create a page table for a given process,
// with no user pages, but with trampoline pages.
pagetable_t
proc_pagetable(struct proc *p)
{
  pagetable_t pagetable;

  // An empty page table.
  pagetable = uvmcreate();

  // map the trampoline code (for system call return)
  // at the highest user virtual address.
  // only the supervisor uses it, on the way
  // to/from user space, so not PTE_U.
  mappages(pagetable, TRAMPOLINE, PGSIZE,
           (uint64)trampoline, PTE_R | PTE_X);

  // map the trapframe just below TRAMPOLINE, for trampoline.S.
  mappages(pagetable, TRAPFRAME, PGSIZE,
           (uint64)(p->tf), PTE_R | PTE_W);

  return pagetable;
}

// Free a process's page table, and free the
// physical memory it refers to.
void
proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
  uvmunmap(pagetable, TRAMPOLINE, PGSIZE, 0);
  uvmunmap(pagetable, TRAPFRAME, PGSIZE, 0);
  if(sz > 0)
    uvmfree(pagetable, sz);
}

// a user program that calls exec("/init")
// od -t xC initcode
uchar initcode[] = {
  0x17, 0x05, 0x00, 0x00, 0x13, 0x05, 0x05, 0x02,
  0x97, 0x05, 0x00, 0x00, 0x93, 0x85, 0x05, 0x02,
  0x9d, 0x48, 0x73, 0x00, 0x00, 0x00, 0x89, 0x48,
  0x73, 0x00, 0x00, 0x00, 0xef, 0xf0, 0xbf, 0xff,
  0x2f, 0x69, 0x6e, 0x69, 0x74, 0x00, 0x00, 0x01,
  0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00
};

// Set up first user process.
void
userinit(void)
{
  struct proc *p;

  p = allocproc();
  initproc = p;
  
  // allocate one user page and copy init's instructions
  // and data into it.
  uvminit(p->pagetable, initcode, sizeof(initcode));
  p->sz = PGSIZE;

  // prepare for the very first "return" from kernel to user.
  p->tf->epc = 0;      // user program counter
  p->tf->sp = PGSIZE;  // user stack pointer

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  p->startstarttime=r_time();  //starting time of the process
  add_proc(p,0,0);   //adds the process to runnable processes Red-Black tree
  p->state = RUNNABLE;  //sets the state of the process to runnable

  release(&p->lock);
}

// Grow or shrink user memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *p = myproc();

  sz = p->sz;
  if(n > 0){
    if((sz = uvmalloc(p->pagetable, sz, sz + n)) == 0) {
      return -1;
    }
  } else if(n < 0){
    sz = uvmdealloc(p->pagetable, sz, sz + n);
  }
  p->sz = sz;
  return 0;
}

// Create a new process, copying the parent.
// Sets up child kernel stack to return as if from fork() system call.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *p = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy user memory from parent to child.
  if(uvmcopy(p->pagetable, np->pagetable, p->sz) < 0){
    freeproc(np);
    release(&np->lock);
    return -1;
  }
  np->sz = p->sz;

  np->parent = p;

  // copy saved user registers.
  *(np->tf) = *(p->tf);

  // Cause fork to return 0 in the child.
  np->tf->a0 = 0;

  // increment reference counts on open file descriptors.
  for(i = 0; i < NOFILE; i++)
    if(p->ofile[i])
      np->ofile[i] = filedup(p->ofile[i]);
  np->cwd = idup(p->cwd);

  safestrcpy(np->name, p->name, sizeof(p->name));

  pid = np->pid;
  
  // setting up the child process to add to RB tree
  np->startstarttime=r_time();
  np->pri=p->pri;
  np->weight=p->weight;
  np->prevweight=np->weight;

  // printing forking statements for testing
  if(printflag==2){
    printf("Forked a process with pid = %d with Nice = %d\n",np->pid,20-(np->pid-p->pid));}
  else if(printflag==1){
    printf("Forked ");
    if(np->pid%2)
      printf("I/O Bound ");
    else
      printf("CPU Bound ");
    printf("Process\n");
  }

  add_proc(np,0,0);   //adds child process to RB tree
  np->state = RUNNABLE;

  release(&np->lock);

  return pid;
}

// Pass p's abandoned children to init.
// Caller must hold p->lock.
void
reparent(struct proc *p)
{
  struct proc *pp;

  for(pp = proc; pp < &proc[NPROC]; pp++){
    // this code uses pp->parent without holding pp->lock.
    // acquiring the lock first could cause a deadlock
    // if pp or a child of pp were also in exit()
    // and about to try to lock p.
    if(pp->parent == p){
      // pp->parent can't change between the check and the acquire()
      // because only the parent changes it, and we're the parent.
      acquire(&pp->lock);
      pp->parent = initproc;
      // we should wake up init here, but that would require
      // initproc->lock, which would be a deadlock, since we hold
      // the lock on one of init's children (pp). this is why
      // exit() always wakes init (before acquiring any locks).
      release(&pp->lock);
    }
  }
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait().
void
exit(int status)
{
  struct proc *p = myproc();

  if(p == initproc)
    panic("init exiting");

  // Close all open files.
  for(int fd = 0; fd < NOFILE; fd++){
    if(p->ofile[fd]){
      struct file *f = p->ofile[fd];
      fileclose(f);
      p->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(p->cwd);
  end_op();
  p->cwd = 0;

  // we might re-parent a child to init. we can't be precise about
  // waking up init, since we can't acquire its lock once we've
  // acquired any other proc lock. so wake up init whether that's
  // necessary or not. init may miss this wakeup, but that seems
  // harmless.
  acquire(&initproc->lock);
  wakeup1(initproc);
  release(&initproc->lock);

  // grab a copy of p->parent, to ensure that we unlock the same
  // parent we locked. in case our parent gives us away to init while
  // we're waiting for the parent lock. we may then race with an
  // exiting parent, but the result will be a harmless spurious wakeup
  // to a dead or wrong process; proc structs are never re-allocated
  // as anything else.
  acquire(&p->lock);
  struct proc *original_parent = p->parent;
  release(&p->lock);
  
  // we need the parent's lock in order to wake it up from wait().
  // the parent-then-child rule says we have to lock it first.
  acquire(&original_parent->lock);

  acquire(&p->lock);

  // Give any children to init.
  reparent(p);

  // Parent might be sleeping in wait().
  wakeup1(original_parent);
  
  // Calculate the runtime of the process.
  p->startstarttime = r_time()-p->startstarttime;
  p->xstate = status;
  update_runtime(p, r_time()-p->starttime);

  // Average virtual and actual runtimes for testing.
  p->avgvrun=((p->vruntime2-p->invrun))/(100*p->noof);
  p->avgarun=(p->aruntime2)/(p->noof);

  p->state = ZOMBIE;

  release(&original_parent->lock);

  // Jump into the scheduler, never to return.
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(uint64 addr)
{
  struct proc *np;
  int havekids, pid;
  struct proc *p = myproc();

  // hold p->lock for the whole time to avoid lost
  // wakeups from a child's exit().
  acquire(&p->lock);

  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(np = proc; np < &proc[NPROC]; np++){
      // this code uses np->parent without holding np->lock.
      // acquiring the lock first would cause a deadlock,
      // since np might be an ancestor, and we already hold p->lock.
      if(np->parent == p){
        // np->parent can't change between the check and the acquire()
        // because only the parent changes it, and we're the parent.
        acquire(&np->lock);
        havekids = 1;
        if(np->state == ZOMBIE){
          // Found one.
          pid = np->pid;
          if(addr != 0 && copyout(p->pagetable, addr, (char *)&np->xstate,
                                  sizeof(np->xstate)) < 0) {
            release(&np->lock);
            release(&p->lock);
            return -1;
          }
          // Printing information about the exiting process
          // (for testing)
          if(flag){
            if(printflag==2){
              printf("Process with Nice = %d is exiting; vruntime = ",np->pri);
              printf("%l",np->vruntime/100); printf("; actual_runtime = "); printf("%l",np->aruntime);
              printf("; turnaroundtime = "); printf("%l",np->startstarttime);
              //printf("; avgdeltavrun = ");printf("%l",np->avgvrun); printf("; avgdeltaarun = ");
              //printf("%l",np->avgarun); printf("; scheduled %d times; avgvrun*scheduledno = ",np->noof);
              //printf("%l",np->avgvrun*p->noof); 
              printf("\n");
            }
            else if (printflag==1){
              if(np->pid%2)
                printf("I/O Bound ");
              else
                printf("CPU Bound ");

              printf("Process is exiting; actual_runtime = "); printf("%l",np->aruntime);
              printf("; turnaroundtime = %l; ",np->startstarttime);
              printf("; avg_vruntime_increment = "); printf("%l",np->avgvrun);
              printf("; scheduled %d times\n",np->noof);
            }
          }
          freeproc(np);
          release(&np->lock);
          release(&p->lock);
          return pid;
        }
        release(&np->lock);
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || p->killed){
      release(&p->lock);
      return -1;
    }
    
    // Wait for a child to exit.
    sleep(p, &p->lock);  //DOC: wait-sleep
  }
}

int
wait2(uint64 addr)
{
  struct proc *np;
  int havekids, pid;
  struct proc *p = myproc();

  // hold p->lock for the whole time to avoid lost
  // wakeups from a child's exit().
  acquire(&p->lock);

  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(np = proc; np < &proc[NPROC]; np++){
      // this code uses np->parent without holding np->lock.
      // acquiring the lock first would cause a deadlock,
      // since np might be an ancestor, and we already hold p->lock.
      if(np->parent == p){
        // np->parent can't change between the check and the acquire()
        // because only the parent changes it, and we're the parent.
        acquire(&np->lock);
        havekids = 1;
        if(np->state == ZOMBIE){
          // Found one.
          pid = np->pid;
          if(addr != 0 && copyout(p->pagetable, addr, (char *)&np->xstate,
                                  sizeof(np->xstate)) < 0) {
            release(&np->lock);
            release(&p->lock);
            return -1;
          }
          freeproc(np);
          release(&np->lock);
          release(&p->lock);
          return pid;
        }
        release(&np->lock);
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || p->killed){
      release(&p->lock);
      return -1;
    }
    
    // Wait for a child to exit.
    sleep(p, &p->lock);  //DOC: wait-sleep
  }
}

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run from the runnable processes Red-Black tree.
//  - swtch to start running that process.
//  - eventually that process transfers control
//    via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  
  c->proc = 0;
  for(;;){
    // Avoid deadlock by ensuring that devices can interrupt.
    intr_on();
    
    // Choosing the process with the minimum vruntime
    // and removing it from the tree
    acquire(&rbtree_lock);
    p = get_next_proc();


    if(eqNIL(p)){
      release(&rbtree_lock); continue;
    }

    acquire(&p->lock);
    if(p->state==RUNNABLE){    // Found a runnable process.
      // preparing for scheduling
      p->interval=get_timeslice(p);
      p->starttime=r_time();
      just_scheduled[cpuid()]=1;

      // For testing.
      if(!firstexit){
        p->noof++; p->vruntime2=p->vruntime; p->aruntime2=p->aruntime;
      }
      totweight=totweight-p->weight;
      numproc--;
      release(&rbtree_lock);
      
      p->state=RUNNING;
      c->proc=p;
      swtch(&c->scheduler,&p->context);
      c->proc=0;
    }
    else{
      release(&rbtree_lock);
    }
    release(&p->lock);
  }
}

// Switch to scheduler.  Must hold only p->lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->noff, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&p->lock))
    panic("sched p->lock");
  if(mycpu()->noff != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(intr_get())
    panic("sched interruptible");

  intena = mycpu()->intena;
  swtch(&p->context, &mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  struct proc *p = myproc();
  
  acquire(&p->lock);
  uint64 time_run;
  time_run = r_time()-(p->starttime);
  // Update runtime for the process and 
  // add it back into the runnable process RB tree
  add_proc(p,1,time_run);
  p->state = RUNNABLE; 
  sched();
  release(&p->lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch to forkret.
void
forkret(void)
{
  static int first = 1;

  // Still holding p->lock from scheduler.
  release(&myproc()->lock);

  if (first) {
    // File system initialization must be run in the context of a
    // regular process (e.g., because it calls sleep), and thus cannot
    // be run from main().
    first = 0;
    fsinit(ROOTDEV);
  }

  usertrapret();
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  // Must acquire p->lock in order to
  // change p->state and then call sched.
  // Once we hold p->lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup locks p->lock),
  // so it's okay to release lk.
  if(lk != &p->lock){  //DOC: sleeplock0
    acquire(&p->lock);  //DOC: sleeplock1
    release(lk);
  }

  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  uint64 time_run;
  time_run=r_time()-(p->starttime);
  update_runtime(p,time_run);
  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &p->lock){
    release(&p->lock);
    acquire(lk);
  }
}

// Wake up all processes sleeping on chan.
// Must be called without any p->lock.
void
wakeup(void *chan)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state == SLEEPING && p->chan == chan) {
      // Make process RUNNABLE and add to tree.
      add_proc(p,-1,0);
      p->state = RUNNABLE;      
    }
    release(&p->lock);
  }
}

// Wake up p if it is sleeping in wait(); used by exit().
// Caller must hold p->lock.
static void
wakeup1(struct proc *p)
{
  if(!holding(&p->lock))
    panic("wakeup1");
  if(p->chan == p && p->state == SLEEPING) {
    // Make process RUNNABLE and add to tree.
    add_proc(p,-1,0);
    p->state = RUNNABLE;
  }
}

// Kill the process with the given pid.
// The victim won't exit until it tries to return
// to user space (see usertrap() in trap.c).
int
kill(int pid)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++){
    acquire(&p->lock);
    if(p->pid == pid){
      p->killed = 1;
      if(p->state == SLEEPING){
        // Wake process from sleep().
        add_proc(p,-1,0);
        p->state = RUNNABLE;
      }
      release(&p->lock);
      return 0;
    }
    release(&p->lock);
  }
  return -1;
}

// Copy to either a user address, or kernel address,
// depending on usr_dst.
// Returns 0 on success, -1 on error.
int
either_copyout(int user_dst, uint64 dst, void *src, uint64 len)
{
  struct proc *p = myproc();
  if(user_dst){
    return copyout(p->pagetable, dst, src, len);
  } else {
    memmove((char *)dst, src, len);
    return 0;
  }
}

// Copy from either a user address, or kernel address,
// depending on usr_src.
// Returns 0 on success, -1 on error.
int
either_copyin(void *dst, int user_src, uint64 src, uint64 len)
{
  struct proc *p = myproc();
  if(user_src){
    return copyin(p->pagetable, dst, src, len);
  } else {
    memmove(dst, (char*)src, len);
    return 0;
  }
}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  struct proc *p;
  char *state;

  printf("\n");
  for(p = proc; p < &proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    printf("%d %s %s", p->pid, state, p->name);
    printf("\n");
  }
}
