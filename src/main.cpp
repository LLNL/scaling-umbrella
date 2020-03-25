#include "braid_wrapper.hpp"
#include "timestepper.hpp"
#include "braid.h"
#include "braid_test.h"
#include "bspline.hpp"
#include "oscillator.hpp" 
#include "mastereq.hpp"
#include "config.hpp"
#include "optimizer.hpp"
#include "_braid.h"
#include <stdlib.h>
#include <sys/resource.h>
#include "hiopAlgFilterIPM.hpp"

#define EPS 1e-4

#define TEST_DRHSDP 0
#define TEST_FD_TS 0
#define TEST_FD_SPLINE 0
#define TEST_DT 0

enum RunType {
  primal,            // Runs one objective function evaluation (forward)
  adjoint,           // Runs one gradient computation (forward & backward)
  optimization,      // Run optimization 
  none               // Don't run anything.
};


int main(int argc,char **argv)
{
  PetscInt       nlvl;         // Number of levels for each oscillator
  PetscInt       nosci;        // Number of oscillators
  PetscInt       ntime;        // Number of time steps
  int            ninit;        // Total number of initial conditions that are considered (N^2, N or 1)
  int            ninit_local;  // Number of initial conditions per processor 
  PetscReal      dt;           // Time step size
  PetscReal      total_time;   // Total end time T
  TS             ts;           // Timestepping context
  PetscInt       nspline;      // Number of spline basis functions
  MasterEq*      mastereq;     // Master equation
  PetscBool      monitor;      // If true: Print out additional time-stepper information
  RunType        runtype;      // Decides if forward only, forward+backward, or optimization
  /* Braid */
  myBraidApp *primalbraidapp;
  myAdjointBraidApp *adjointbraidapp;

  Vec            x;          // solution vector
  // bool           tj_save;    // Determines wether trajectory should be stored in primal run

  /* Optimization */
  double objective;        // Objective function value f
  Vec* lambda = new Vec;   // Adjoint solution in lambda[0]
  Vec* mu = new Vec;       // Reduced gradient in mu[0]


  char filename[255];
  PetscErrorCode ierr;
  PetscMPIInt    mpisize_world, mpirank_world;

  /* Initialize MPI */
  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &mpirank_world);
  MPI_Comm_size(MPI_COMM_WORLD, &mpisize_world);
  if (mpirank_world == 0) printf("# Running on %d cores.\n", mpisize_world);

  /* Split aside communicators for petsc and hiop for later development. Size 1 for now. */
  MPI_Comm comm = MPI_COMM_WORLD;
  MPI_Comm comm_petsc, comm_hiop;
  MPI_Comm_split(MPI_COMM_WORLD, mpirank_world, mpirank_world, &comm_hiop);
  MPI_Comm_split(MPI_COMM_WORLD, mpirank_world, mpirank_world, &comm_petsc);

  // int mpirank_petsc, mpirank_hiop;
  // int mpisize_petsc, mpisize_hiop;
  // MPI_Comm_rank(comm_petsc, &mpirank_petsc);
  // MPI_Comm_size(comm_petsc, &mpisize_petsc);
  // MPI_Comm_rank(comm_hiop, &mpirank_hiop);
  // MPI_Comm_size(comm_hiop, &mpisize_hiop);
  // printf("world %d/%d\n", mpirank_world, mpisize_world);
  // printf("petsc %d/%d\n", mpirank_petsc, mpisize_petsc);
  // printf("hiop  %d/%d\n", mpirank_hiop, mpisize_hiop );


  /* Initialize Petsc using petsc's communicator */
  PETSC_COMM_WORLD = comm_petsc;
  ierr = PetscInitialize(&argc,&argv,(char*)0,NULL);if (ierr) return ierr;

  /* Read config file */
  if (argc != 2) {
    if (mpirank_world == 0) {
      printf("\nUSAGE: ./main </path/to/configfile> \n");
    }
    MPI_Finalize();
    return 0;
  }
  MapParam config(MPI_COMM_WORLD);
  config.ReadFile(argv[1]);

  /* Get some options from the config file */
  nlvl  = config.GetIntParam("nlevels", 2);
  nosci = config.GetIntParam("noscillators", 2);
  ntime = config.GetIntParam("ntime", 1000);
  dt    = config.GetDoubleParam("dt", 0.01);
  nspline = config.GetIntParam("nspline", 10);
  monitor = (PetscBool) config.GetBoolParam("monitor", false);
  std::string runtypestr = config.GetStrParam("runtype", "primal");
  if      (runtypestr.compare("primal")      == 0) runtype = primal;
  else if (runtypestr.compare("adjoint")     == 0) runtype = adjoint;
  else if (runtypestr.compare("optimization")== 0) runtype = optimization;
  else {
    printf("\n\n WARNING: Unknown runtype: %s.\n\n", runtypestr.c_str());
    runtype = none;
  }
  
  /* Initialize time horizon */
  total_time = ntime * dt;

  /* Initialize the Oscillators */
  Oscillator** oscil_vec = new Oscillator*[nosci];
  for (int i = 0; i < nosci; i++){
    oscil_vec[i] = new SplineOscillator(nlvl, nspline, total_time);
  }

  /* So far, these frequencies are not used anywhere... */
  std::vector<double> f;
  config.GetVecDoubleParam("frequencies", f, 1e20);

  /* Initialize the Master Equation  */
  std::vector<double> xi, t_collapse;
  config.GetVecDoubleParam("xi", xi, 2.0);
  config.GetVecDoubleParam("lindblad_collapsetime", t_collapse, 0.0);
  std::string lindblad = config.GetStrParam("lindblad_type", "none");
  LindbladType lindbladtype;
  if      (lindblad.compare("none")      == 0 ) lindbladtype = NONE;
  else if (lindblad.compare("decay")     == 0 ) lindbladtype = DECAY;
  else if (lindblad.compare("dephase")   == 0 ) lindbladtype = DEPHASE;
  else if (lindblad.compare("both")      == 0 ) lindbladtype = BOTH;
  else {
    printf("\n\n ERROR: Unnown lindblad type: %s.\n", lindblad.c_str());
    printf(" Choose either 'none', 'decay', 'dephase', or 'both'\n");
    exit(1);
  }
  mastereq = new MasterEq(nosci, oscil_vec, xi, lindbladtype, t_collapse);


  /* Initialize the target gate */
  Gate* targetgate;
  std::string gatetype = config.GetStrParam("gate_type", "none");
  if      (gatetype.compare("none") == 0) targetgate = new Gate(); // dummy gate. do nothing
  else if (gatetype.compare("xgate") == 0) targetgate = new XGate(); 
  else if (gatetype.compare("ygate") == 0) targetgate = new YGate(); 
  else if (gatetype.compare("zgate") == 0) targetgate = new ZGate();
  else if (gatetype.compare("hadamard") == 0) targetgate = new HadamardGate();
  else if (gatetype.compare("cnot") == 0) targetgate = new CNOT(); 
  else if (gatetype.compare("groundstate") == 0) targetgate = new GroundstateGate((int)sqrt(mastereq->getDim())); 
  else {
    printf("\n\n ERROR: Unnown gate type: %s.\n", gatetype.c_str());
    printf(" Available gates are 'none', 'xgate', 'ygate', 'zgate', 'hadamard', 'cnot'\n");
    exit(1);
  }

  /* Create solution vector x */
  MatCreateVecs(mastereq->getRHS(), &x, NULL);

  /* Initialize reduced gradient and adjoints */
  MatCreateVecs(mastereq->getRHS(), lambda, NULL);  // adjoint 
  MatCreateVecs(mastereq->getdRHSdp(), mu, NULL);   // reduced gradient

  /* Screen output */
  if (mpirank_world == 0)
  {
    printf("# System with %d oscillators, %d levels. \n", nosci, nlvl);
    printf("# Time horizon:   [0,%.1f]\n", total_time);
    printf("# Number of time steps: %d\n", ntime);
    printf("# Time step size: %f\n", dt );
  }

  TimeStepper *mytimestepper = new ImplMidpoint(mastereq);
  // TimeStepper *mytimestepper = new ExplEuler(mastereq);

  /* Allocate and initialize Petsc's Time-stepper */
  TSCreate(PETSC_COMM_SELF,&ts);CHKERRQ(ierr);
  TSInit(ts, mastereq, ntime, dt, total_time, x, lambda, mu, monitor);

  /* Get the total number of initial conditions 'ninit' */
  std::string initcond_type; 
  initcond_type = config.GetStrParam("initialconditions", "all");
  if      (initcond_type.compare("all")      == 0 ) ninit = mastereq->getDim();              // N^2
  else if (initcond_type.compare("diagonal") == 0 ) ninit = (int) sqrt(mastereq->getDim());  // N
  else if (initcond_type.compare("one")      == 0 ) ninit = 1;
  else {
    printf("Wrong initial condition type: %s \n", initcond_type.c_str());
    exit(1);
  }

  /* --- Get processor distribution for initial condition and braid --- */
  int np_braid;     // Number of cores for each braid instance
  int np_init;      // Number of cores for distributing initial conditions 
  np_init  = config.GetIntParam("np_init", 1);// default: all cores for braid only, no initial condition parallelism
  /* Sanity check */
  if (ninit % np_init != 0){
    printf("ERROR: Wrong processor distribution! \n Size of communicator for distributing initial conditions (%d) must be integer divisor of the total number of initial conditions (%d)!!\n", np_init, ninit);
    exit(1);
  }
  if (mpisize_world % np_init != 0) {
    printf("ERROR: Wrong number of threads! \n Total number of threads (%d) must be integer multiple of the size of initial condition communicator (%d)!\n", mpisize_world, np_init);
    exit(1);
  }

  ninit_local = ninit / np_init;      // Number of initial conditions per init-processor group
  np_braid = mpisize_world / np_init; // Number of cores for braid

  /* Split communicator for braid and initial condition */
  int mpirank_init, mpisize_init, mpirank_braid, mpisize_braid;
  MPI_Comm comm_braid;
  MPI_Comm comm_init;
  MPI_Comm_split(MPI_COMM_WORLD, mpirank_world % np_braid, mpirank_world, &comm_init);
  MPI_Comm_split(MPI_COMM_WORLD, mpirank_world / np_braid, mpirank_world, &comm_braid);
  MPI_Comm_rank(comm_init, &mpirank_init);
  MPI_Comm_size(comm_init, &mpisize_init);
  MPI_Comm_rank(comm_braid, &mpirank_braid);
  MPI_Comm_size(comm_braid, &mpisize_braid);


  /* Get distribution of initial conditions */
  int ilower = mpirank_init * ninit_local;          // id of first initial condition on this processor 
  int iupper = (mpirank_init+1) * ninit_local - 1;  // id of last initial condition on this processor

  printf("%d: np_init %d/%d: ninit_local %d/%d: [%d,%d] , np_braid %d/%d\n", mpirank_world, mpirank_init, mpisize_init, ninit_local,ninit, ilower, iupper, mpirank_braid, mpisize_braid);
  ierr = PetscFinalize();
  MPI_Finalize();
  exit(1);
  

  // int dimN = mastereq->getDim();
  // int p;
  // if (dimN >= mpisize_world) { 
  //   // Distribute initial conditions only
  //   // each braid instance runs with 1 processor
  //   np_braid = 1;
  //   // Get distribution of initial conditions
  //   int quo = dimN / mpisize_world;
  //   int rem = dimN % mpisize_world;
  //   p = mpirank_world;
  //   ilower = p * quo + (p < rem ? p : rem);
  //   p = mpirank_world + 1;
  //   iupper = p * quo + (p < rem ? p : rem) - 1;
  //   ninit    = iupper - ilower + 1;
  // } else { 
  //   // Each initial condition uses separate braid instance
  //   // Each braid instance uses nprocs / dimN processors 
  //   if (mpisize_world % dimN != 0) {
  //     printf("ERROR: #nprocs should be a multiple of the system dimensions %d.\n", dimN);
  //     exit(1);
  //   }
  //   ninit    = 1;    
  //   np_braid = (int) mpisize_world / dimN;  
  //   ilower = mpirank_world / np_braid ;
  //   iupper = mpirank_world / np_braid ;
  // }


  /* Create braid instances */
  primalbraidapp = new myBraidApp(comm_braid, total_time, ntime, ts, mytimestepper, mastereq, &config);
  adjointbraidapp = new myAdjointBraidApp(comm_braid, total_time, ntime, ts, mytimestepper, mastereq, *mu, &config, primalbraidapp->getCore());
  primalbraidapp->InitGrids();
  adjointbraidapp->InitGrids();

  /* Initialize the optimization */
  OptimProblem optimproblem(config, primalbraidapp, adjointbraidapp, targetgate, comm_hiop, comm_init, ninit, ilower, iupper);
  hiop::hiopNlpDenseConstraints nlp(optimproblem);
  long long int ndesign,m;
  optimproblem.get_prob_sizes(ndesign, m);
  /* Set options */
  double optim_tol = config.GetDoubleParam("optim_tol", 1e-4);
  nlp.options->SetNumericValue("tolerance", optim_tol);
  double optim_maxiter = config.GetIntParam("optim_maxiter", 200);
  nlp.options->SetIntegerValue("max_iter", optim_maxiter);
  if (mpirank_world != 0) nlp.options->SetIntegerValue("verbosity_level", 0);
  /* Create solver */
  hiop::hiopAlgFilterIPM optimsolver(&nlp);
  hiop::hiopSolveStatus  optimstatus;

  /* --- Test optimproblem --- */
  if (mpirank_world == 0) printf("# ndesign=%d\n", ndesign);
  double* myinit = new double[ndesign];
  double* optimgrad = new double[ndesign];
  optimproblem.get_starting_point(ndesign, myinit);

   /* Start timer */
  double StartTime = MPI_Wtime();

  /* --- Solve primal --- */
  if (runtype == primal || runtype == adjoint) {
    optimproblem.eval_f(ndesign, myinit, true, objective);
    if (mpirank_world == 0) printf("%d: Primal Only: Objective %1.14e\n", mpirank_world, objective);
  } 
  
  /* --- Solve adjoint --- */
  if (runtype == adjoint) {
    optimproblem.eval_grad_f(ndesign, myinit, true, optimgrad);
    double gnorm = 0.0;
    if (mpirank_world == 0) {
      printf("\n%d: My awesome gradient:\n", mpirank_world);
      for (int i=0; i<ndesign; i++) {
        gnorm += pow(optimgrad[i], 2.0);
        printf("%1.14e\n", optimgrad[i]);
      }
      printf("Gradient norm: %1.14e\n", gnorm);
    }
  }

  /* Solve the optimization  */
  if (runtype == optimization) {
    if (mpirank_world == 0) printf("Now starting HiOp... \n");
    optimstatus = optimsolver.run();

  }

  /* Get timings */
  double UsedTime = MPI_Wtime() - StartTime;
  /* Get memory usage */
  struct rusage r_usage;
  getrusage(RUSAGE_SELF, &r_usage);
  double myMB = (double)r_usage.ru_maxrss / 1024.0;
  double globalMB;
  MPI_Allreduce(&myMB, &globalMB, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);

  /* Print statistics */
  if (mpirank_world == 0) {
    printf("\n");
    printf(" Used Time:        %.2f seconds\n", UsedTime);
    printf(" Global Memory:    %.2f MB\n", globalMB);
    printf(" Processors used:  %d\n", mpisize_world);
    printf("\n");
  }
  printf("Rank %d: %.2fMB\n", mpirank_world, myMB );

  /* Print timing to file */
  if (mpirank_world == 0) {
    sprintf(filename, "timing.dat");
    FILE* timefile = fopen(filename, "w");
    fprintf(timefile, "%d  %1.8e\n", mpisize_world, UsedTime);
    fclose(timefile);
    printf("%s written.\n", filename);
  }




#if TEST_DRHSDP
  printf("\n\n#########################\n");
  printf(" dRHSdp Testing... \n");
  printf("#########################\n\n");

  double t = 0.345;
  double err_i = 0.0;
  Vec Ax, Bx, Cx, fd, err, grad_col;
  Mat G;
  int size;
  VecGetSize(x, &size);
  VecDuplicate(x, &Ax);
  VecDuplicate(x, &Bx);
  VecDuplicate(x, &Cx);
  VecDuplicate(x, &fd);
  VecDuplicate(x, &err);
  VecDuplicate(x, &grad_col);

  /* Set x to last time step */
  // x = primalbraidapp->getStateVec(total_time);
  /* Set x to all ones */
  VecZeroEntries(x);
  VecShift(x, 1.0);

  /* Evaluate RHS(p)x */
  optimproblem->setDesign(ndesign, myinit);
  mastereq->assemble_RHS(t);
  mastereq->getRHS();
  MatMult(mastereq->getRHS(), x, Ax);

  /* Evaluate dRHSdp(t,p)x */
  mastereq->assemble_dRHSdp(t, x);
  G = mastereq->getdRHSdp();
  // MatView(G, PETSC_VIEWER_STDOUT_WORLD);

  /* Flush control functions */
  for (int i = 0; i < nosci; i++){
    sprintf(filename, "control_%04d.dat", i);
    oscil_vec[i]->flushControl(ntime, dt, filename);
  }

  /* FD loop */
  // for (int i=0; i<ndesign; i++) {
  {int i=3;

    /* Eval RHS(p+eps)x */
    myinit[i] -= EPS;
    optimproblem->setDesign(ndesign, myinit);
    mastereq->assemble_RHS(t);
    MatMult(mastereq->getRHS(), x, Bx);

    /* Eval RHS(p-eps)x */
    myinit[i] += 2.*EPS;
    optimproblem->setDesign(ndesign, myinit);
    mastereq->assemble_RHS(t);
    MatMult(mastereq>getRHS(), x, Cx);

    /* Eval central finite differences (Bx - Cx) / 2eps */
    VecAXPBYPCZ(fd, 1.0, -1.0, 0.0, Bx, Cx);
    VecScale(fd, 1./(2.*EPS));

    /* Compute error */
    MatGetColumnVector(G, grad_col, i);
    VecAXPBYPCZ(err, 1.0, -1.0, 0.0, grad_col, fd); 
    VecPointwiseDivide(err, err, fd);
    VecNorm(err, NORM_2, &err_i);

    /* Output */
    printf(" %d: || e_i|| = %1.4e\n", i, err_i);
    if (err_i > 1e-5) {
      const PetscScalar *err_ptr, *grad_ptr, *fd_ptr, *ax_ptr, *bx_ptr, *cx_ptr;
      VecGetArrayRead(err, &err_ptr);
      VecGetArrayRead(fd, &fd_ptr);
      VecGetArrayRead(Ax, &ax_ptr);
      VecGetArrayRead(Bx, &bx_ptr);
      VecGetArrayRead(Cx, &cx_ptr);
      VecGetArrayRead(grad_col, &grad_ptr);
      printf("ERR    Ax[i]     Bx[i]     Cx[i]    FD       GRADCOL\n");
      for (int j=0; j<size; j++){
        printf("%1.14e  %1.20e  %1.20e  %1.20e  %1.14e  %1.14e\n", err_ptr[j], ax_ptr[i], bx_ptr[i], cx_ptr[i], fd_ptr[j], grad_ptr[j]);
      }
    }


    // printf("FD column:\n");
    // VecView(fd, PETSC_VIEWER_STDOUT_WORLD);
    // printf("dRHSdp column:\n");
    // VecView(grad_col, PETSC_VIEWER_STDOUT_WORLD);

    /* Reset design */
    myinit[i] -= EPS;
  }


#endif

#if TEST_FD_TS
  if (mpirank_world == 0)  {
    printf("\n\n#########################\n");
    printf(" FD Testing... \n");
    printf("#########################\n\n");
  }

  double obj_org;
  double obj_pert1, obj_pert2;

  long long int n,l;
  optimproblem.get_prob_sizes(n, m);

  double* myx = new double[n];
  optimproblem.get_starting_point(n, myx);


  // /* --- Solve primal --- */
  if (mpirank_world == 0) printf("\nRunning optimizer eval_f... ");
  optimproblem.eval_f(n, myx, true, obj_org);
  if (mpirank_world == 0) printf(" Obj_orig %1.14e\n", obj_org);

  /* --- Solve adjoint --- */
  if (mpirank_world == 0) printf("\nRunning optimizer eval_grad_f...\n");
  double* testgrad = new double[n];
  optimproblem.eval_grad_f(n, myx, true, testgrad);
  for (int i=0; i<ndesign; i++) {
        if (mpirank_world == 0) printf("%1.14e\n", testgrad[i]);
  }

  /* Finite Differences */
  if (mpirank_world == 0) printf("\nFD...\n");
  for (int i=0; i<n; i++){
  // {int i=0;

    /* Evaluate f(p+eps)*/
    myx[i] += EPS;
    optimproblem.eval_f(n, myx, true, obj_pert1);

    /* Evaluate f(p-eps)*/
    myx[i] -= 2.*EPS;
    optimproblem.eval_f(n, myx, true, obj_pert2);

    /* Eval FD and error */
    double fd = (obj_pert1 - obj_pert2) / (2.*EPS);
    double err = 0.0;
    if (fd != 0.0) err = (testgrad[i] - fd) / fd;
    if (mpirank_world == 0) printf(" %d: obj %1.14e, obj_pert1 %1.14e, obj_pert2 %1.14e, fd %1.14e, grad %1.14e, err %1.14e\n", i, obj_org, obj_pert1, obj_pert2, fd, testgrad[i], err);

    /* Restore parameter */
    myx[i] += EPS;
  }
  
  delete [] testgrad;
  delete [] myx;

#endif

#if TEST_FD_SPLINE
  printf("\n\n Finite-differences for Spline discretization...\n\n");
  
  // double t = 0.345;
  double t = 0.345;
  double f, g;
  double f_pert1, g_pert1, f_pert2, g_pert2;

  int nparam = oscil_vec[0]->getNParam();

  /* Init derivative */
  double *dfdw = new double[nparam];
  double *dgdw = new double[nparam];

  for (int iosc=0; iosc<nosci; iosc++)
  {
    printf("FD for oscillator %d:\n", iosc);

    /* Eval gradients */
    optimproblem->setDesign(ndesign, myinit);
    oscil_vec[iosc]->evalControl(t, &f, &g);
    for (int iparam=0; iparam< nparam; iparam++) {
      dfdw[iparam] = 0.0;
      dgdw[iparam] = 0.0;
    }
    oscil_vec[iosc]->evalDerivative(t, dfdw, dgdw);

    /* FD testing for all parameters of oscil 1 */
    for (int iparam = 0; iparam < nparam; iparam++)
    {
      int alpha_id = iosc * 2 * nparam + iparam;
      int beta_id  = iosc * 2 * nparam + nparam + iparam;
      printf("  param %d: \n", iparam);

      /* Eval perturbed objectives */
      myinit[alpha_id] += EPS;
      myinit[beta_id]  += EPS;
      optimproblem->setDesign(ndesign, myinit);
      oscil_vec[iosc]->evalControl(t, &f_pert1, &g_pert1);

      myinit[alpha_id] -= 2.*EPS;
      myinit[beta_id]  -= 2.*EPS;
      optimproblem->setDesign(ndesign, myinit);
      oscil_vec[iosc]->evalControl(t, &f_pert2, &g_pert2);

      /* Eval FD and error */
      double f_fd = f_pert1/(2.*EPS) - f_pert2 / (2.*EPS);
      double g_fd = g_pert1/(2.*EPS) - g_pert2 / (2.*EPS);
      double f_err = 0.0;
      double g_err = 0.0;
      if (f_fd != 0.0) f_err = (dfdw[iparam] - f_fd) / f_fd;
      if (g_fd != 0.0) g_err = (dgdw[iparam] - g_fd) / g_fd;
      printf("    f %1.12e  f1 %1.12e  f2 %1.12e  f_fd %1.12e, dfdw %1.12e, f_err %1.8e\n", f, f_pert1, f_pert2, f_fd, dfdw[iparam],  f_err);
      printf("    g %1.12e  g1 %1.12e  f2 %1.12e  g_fd %1.12e, dgdw %1.12e, g_err %1.8e\n", g, g_pert1, g_pert2, g_fd, dgdw[iparam],  g_err);

      /* Restore parameter */
      myinit[alpha_id] += EPS;
      myinit[beta_id]  += EPS;
    }
  }
#endif



#if TEST_DT
  /* 
   * Testing time stepper convergence (dt-test) 
   */  

  Vec exact;  // exact solution
  Vec error;  // error  
  double t;
  double error_norm, exact_norm;

  int nreal = 2*mastereq->getDim();
  VecCreateSeq(PETSC_COMM_WORLD,nreal,&x);
  VecCreateSeq(PETSC_COMM_WORLD,nreal,&exact);
  VecCreateSeq(PETSC_COMM_WORLD,nreal,&error);

  /* Destroy old time stepper */
  TSDestroy(&ts);

  /* Set time horizon */
  total_time = 10.0;
  printf("\n\n Running time-stepping convergence test... \n\n");
  printf(" Time horizon: [0, %.1f]\n\n", total_time);

  /* Decrease time step size */
  printf("   ntime      dt    error\n");
  for (int ntime = 10; ntime <= 1e+5; ntime = ntime * 10)
  {
    dt = total_time / ntime;

    /* Create and set up the time stepper */
    TSCreate(PETSC_COMM_SELF,&ts);CHKERRQ(ierr);
    TSInit(ts, mastereq, ntime, dt, total_time, x, lambda, mu, monitor);
    TSSetSolution(ts, x);

    // /* Set the initial condition */
    mastereq->initialCondition(0,x);

    /* Run time-stepping loop */
    for(PetscInt istep = 0; istep <= ntime; istep++) 
    {
      TSStep(ts);
    }

    /* Compute the relative error at last time step (max-norm) */
    TSGetTime(ts, &t);
    mastereq->ExactSolution(t,exact);
    VecWAXPY(error,-1.0,x, exact);
    VecNorm(error, NORM_INFINITY,&error_norm);
    VecNorm(exact, NORM_INFINITY,&exact_norm);
    error_norm = error_norm / exact_norm;

    /* Print error norm */
    printf("%8d   %1.e   %1.14e\n", ntime, dt, error_norm);

  }

  VecDestroy(&x);
  VecDestroy(&exact);
  VecDestroy(&error);

#endif

#ifdef SANITY_CHECK
  printf("\n\n Sanity checks have been performed. Check output for warnings and errors!\n\n");
#endif

  /* Clean up */
  // TSDestroy(&ts);  /* TODO */

  /* Clean up Oscillator */
  for (int i=0; i<nosci; i++){
    delete oscil_vec[i];
  }
  delete [] oscil_vec;


  delete lambda;
  delete mu;

  /* Clean up Master equation*/
  delete mastereq;
  delete targetgate;

  /* Cleanup */
  // TSDestroy(&braid_app->ts);

  delete mytimestepper;
  
  delete primalbraidapp;
  delete adjointbraidapp;

  /* Finallize Petsc */
  ierr = PetscFinalize();

  MPI_Finalize();
  return ierr;
}


