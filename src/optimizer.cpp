#include "optimizer.hpp"

OptimProblem::OptimProblem() {
    primalbraidapp  = NULL;
    adjointbraidapp = NULL;
    objective_curr = 0.0;
    regul = 0.0;
    alpha_max = 0.0;
    beta_max = 0.0;
    x0filename = "none";
    mpirank_braid = 0;
    mpisize_braid = 0;
    mpirank_space = 0;
    mpisize_space = 0;
    mpirank_optim = 0;
    mpisize_optim = 0;
    mpirank_world = 0;
    mpisize_world = 0;
}

OptimProblem::OptimProblem(myBraidApp* primalbraidapp_, myAdjointBraidApp* adjointbraidapp_, MPI_Comm comm_hiop_, double optim_regul_, double alpha_max_, double beta_max_, std::string x0filename_){
    primalbraidapp  = primalbraidapp_;
    adjointbraidapp = adjointbraidapp_;
    comm_hiop = comm_hiop_;
    regul = optim_regul_;
    alpha_max = alpha_max_;
    beta_max = beta_max_;
    x0filename = x0filename_;

    MPI_Comm_rank(primalbraidapp->comm_braid, &mpirank_braid);
    MPI_Comm_size(primalbraidapp->comm_braid, &mpisize_braid);
    MPI_Comm_rank(PETSC_COMM_WORLD, &mpirank_space);
    MPI_Comm_size(PETSC_COMM_WORLD, &mpisize_space);
    MPI_Comm_rank(comm_hiop, &mpirank_optim);
    MPI_Comm_size(comm_hiop, &mpisize_optim);
    MPI_Comm_rank(MPI_COMM_WORLD, &mpirank_world);
    MPI_Comm_size(MPI_COMM_WORLD, &mpisize_world);
}

OptimProblem::~OptimProblem() {}



void OptimProblem::setDesign(int n, const double* x) {

  Hamiltonian* hamil = primalbraidapp->hamiltonian;

  /* Pass design vector x to oscillator */
  int nparam;
  double *paramRe, *paramIm;
  int j = 0;
  /* Iterate over oscillators */
  for (int ioscil = 0; ioscil < hamil->getNOscillators(); ioscil++) {
      /* Get number of parameters of oscillator i */
      nparam = hamil->getOscillator(ioscil)->getNParam();
      /* Get pointers to parameters of oscillator i */
      paramRe = hamil->getOscillator(ioscil)->getParamsRe();
      paramIm = hamil->getOscillator(ioscil)->getParamsIm();
      /* Design storage: x = (ReParams, ImParams)_iOscil
      /* Set Re parameters */
      for (int iparam=0; iparam<nparam; iparam++) {
          paramRe[iparam] = x[j]; j++;
      }
      /* Set Im parameters */
      for (int iparam=0; iparam<nparam; iparam++) {
          paramIm[iparam] = x[j]; j++;
      }
  }
}


void OptimProblem::getDesign(int n, double* x){

  double *paramRe, *paramIm;
  int nparam;
  int j = 0;
  /* Iterate over oscillators */
  Hamiltonian* hamil = primalbraidapp->hamiltonian;
  for (int ioscil = 0; ioscil < hamil->getNOscillators(); ioscil++) {
      /* Get number of parameters of oscillator i */
      nparam = hamil->getOscillator(ioscil)->getNParam();
      /* Get pointers to parameters of oscillator i */
      paramRe = hamil->getOscillator(ioscil)->getParamsRe();
      paramIm = hamil->getOscillator(ioscil)->getParamsIm();
      /* Set Re params */
      for (int iparam=0; iparam<nparam; iparam++) {
          x[j] = paramRe[iparam]; j++;
      }
      /* Set Im params */
      for (int iparam=0; iparam<nparam; iparam++) {
          x[j] = paramIm[iparam]; j++;
      }
  }
}

bool OptimProblem::get_prob_sizes(long long& n, long long& m) {

  // n - number of design variables 
  n = 0;
  Hamiltonian* hamil = primalbraidapp->hamiltonian;
  for (int ioscil = 0; ioscil < hamil->getNOscillators(); ioscil++) {
      n += 2 * hamil->getOscillator(ioscil)->getNParam(); // Re and Im params for the i-th oscillator
  }
  
  // m - number of constraints 
  m = 0;          

  return true;
}


bool OptimProblem::get_vars_info(const long long& n, double *xlow, double* xupp, NonlinearityType* type) {


  /* Iterate over oscillators */
  int j = 0;
  Hamiltonian* hamil = primalbraidapp->hamiltonian;
  for (int ioscil = 0; ioscil < hamil->getNOscillators(); ioscil++) {
      /* Get number of parameters of oscillator i */
      int nparam = hamil->getOscillator(ioscil)->getNParam();
      for (int iparam=0; iparam<nparam; iparam++) {
          /* Set boundary on real part */
          xlow[j] = - alpha_max;
          xupp[j] =   alpha_max; 
          j++;
          /* Set boundary on imaginary part */
          xlow[j] = - beta_max; 
          xupp[j] =   beta_max; 
          j++;
      }
  }

  for (int i=0; i<n; i++) {
    type[i] =  hiopNonlinear;
  }

  return true;
}

bool OptimProblem::get_cons_info(const long long& m, double* clow, double* cupp, NonlinearityType* type){
  assert(m==0);
  return true;
}


bool OptimProblem::eval_f(const long long& n, const double* x_in, bool new_x, double& obj_value){
// bool OptimProblem::eval_f(Index n, const Number* x, bool new_x, Number& obj_value){

  if (mpirank_world == 0) printf(" EVAL F... ");
  double obj_Re_local;
  double obj_Im_local;
  Hamiltonian* hamil = primalbraidapp->hamiltonian;
  int dim = hamil->getDim();

  /* Run simulation, only if x_in is new. Otherwise, f(x_in) has been computed already and stored in objective_curr. */
  if (new_x) { 

    /* Pass design vector x to oscillator */
    setDesign(n, x_in);

    /*  Iterate over initial condition */
    objective_curr = 0.0;
    for (int iinit = 0; iinit < dim; iinit++) {
      if (mpirank_world == 0) printf(" %d FWD. ", iinit);
      /* Set initial condition for index iinit */
      primalbraidapp->PreProcess(iinit, 0.0, 0.0);
      /* Solve forward problem */
      primalbraidapp->Drive();
      /* Eval objective function for initial condition i */
      primalbraidapp->PostProcess(iinit, &obj_Re_local, &obj_Im_local);

      /* Add to objective function (trace) */
      objective_curr += pow(obj_Re_local,2.0) + pow(obj_Im_local, 2.0);
    }
    if (mpirank_world == 0) printf("\n");
  }

  /* Sum up objective from all braid processors */
  double myobj = objective_curr;
  MPI_Allreduce(&myobj, &objective_curr, 1, MPI_DOUBLE, MPI_SUM, primalbraidapp->comm_braid);

  /* J = 1 - 1/N^4 * obj + gamma * ||x||^2*/
  objective_curr = 1. - 1./(dim*dim) * objective_curr;
  for (int i=0; i<n; i++) {
    objective_curr += regul / 2.0 * pow(x_in[i], 2.0);
  }

  /* Return objective value */
  obj_value = objective_curr;

  return true;
}


bool OptimProblem::eval_grad_f(const long long& n, const double* x_in, bool new_x, double* gradf){
// bool OptimProblem::eval_grad_f(Index n, const Number* x, bool new_x, Number* grad_f){
  if (mpirank_world == 0) printf(" EVAL GRAD F...");

  Hamiltonian* hamil = primalbraidapp->hamiltonian;
  double obj_Re_local, obj_Im_local;
  double obj_Re_bar, obj_Im_bar;
  int dim = hamil->getDim();

  /* Pass x to Oscillator */
  setDesign(n, x_in);

  /* Derivative of J = 1 - 1/N^4 * obj + gamma * ||x||^2 */
  double objective_curr_bar = -1./(dim*dim);
  for (int i=0; i<n; i++) {
    gradf[i] = regul * x_in[i];
  }

  /* Iterate over initial conditions */
  objective_curr = 0.0;
  for (int iinit = 0; iinit < dim; iinit++) {

    /* --- Solve primal --- */
    if (mpirank_world == 0) printf(" %d FWD -", iinit);
    primalbraidapp->PreProcess(iinit, 0.0, 0.0);
    primalbraidapp->Drive();
    primalbraidapp->PostProcess(iinit, &obj_Re_local, &obj_Im_local);
    /* Add to global objective value */
    objective_curr += pow(obj_Re_local,2.0) + pow(obj_Im_local, 2.0);

    /* Derivative of local trace objective */
    obj_Re_bar =  2. * obj_Re_local * objective_curr_bar;
    obj_Im_bar =  2. * obj_Im_local * objective_curr_bar;

    /* --- Solve adjoint --- */
    if (mpirank_world == 0) printf(" BWD. ");
    adjointbraidapp->PreProcess(iinit, obj_Re_bar, obj_Im_bar);
    adjointbraidapp->Drive();
    adjointbraidapp->PostProcess(iinit, NULL, NULL);

    /* Add to Ipopt's gradient */
    const double* grad_ptr = adjointbraidapp->getReducedGradientPtr();
    for (int i=0; i<n; i++) {
        gradf[i] += grad_ptr[i]; 
    }
  }
  if (mpirank_world == 0) printf("\n");

  /* Sum up objective from all braid processors */
  double myobj = objective_curr;
  MPI_Allreduce(&myobj, &objective_curr, 1, MPI_DOUBLE, MPI_SUM, primalbraidapp->comm_braid);
  /* J = 1 - 1/N^4 * obj + gamma*||x||^2 */
  objective_curr = 1. - 1./(dim*dim) * objective_curr;
  for (int i=0; i<n; i++) {
    objective_curr += regul / 2.0 * pow(x_in[i], 2.0);
  }

  /* Sum up the gradient from all braid processors */
  double* mygrad = new double[n];
  for (int i=0; i<n; i++) {
    mygrad[i] = gradf[i];
  }
  MPI_Allreduce(mygrad, gradf, n, MPI_DOUBLE, MPI_SUM, primalbraidapp->comm_braid);


    
  return true;
}


bool OptimProblem::eval_cons(const long long& n, const long long& m, const long long& num_cons, const long long* idx_cons, const double* x_in, bool new_x, double* cons) {
    assert(m==0);
    /* No constraints. Nothing to be done. */
    return true;
}


bool OptimProblem::eval_Jac_cons(const long long& n, const long long& m, const long long& num_cons, const long long* idx_cons, const double* x_in, bool new_x, double** Jac){
    assert(m==0);
    /* No constraints. Nothing to be done. */
    return true;
}

bool OptimProblem::get_starting_point(const long long &global_n, double* x0) {

  /* Set initial parameters. */
  // Do this on one processor only, then broadcast, to make sure that every processor starts with the same initial guess. 
  if (mpirank_world == 0) {
    if (x0filename.compare("none") != 0)  {
        /* read from file */
        read_vector(x0filename.c_str(), x0, global_n); 
    }
    else {
      /* Set to random initial guess. */
      srand (1.0);    // seed 1.0 only for code debugging! seed with time(NULL) otherwise!
      for (int i=0; i<global_n; i++) {
        x0[i] = (double) rand() / ((double)RAND_MAX);
      }
    }
  }

  MPI_Bcast(x0, global_n, MPI_DOUBLE, 0, MPI_COMM_WORLD);

  /* Pass to oscillator */
  setDesign(global_n, x0);
  
  /* Flush control functions */
  if (mpirank_world == 0 ) {
    int ntime = primalbraidapp->ntime;
    double dt = primalbraidapp->total_time / ntime;
    char filename[255];
    Hamiltonian* hamil = primalbraidapp->hamiltonian;
    for (int ioscil = 0; ioscil < hamil->getNOscillators(); ioscil++) {
        sprintf(filename, "control_init_%02d.dat", ioscil+1);
        hamil->getOscillator(ioscil)->flushControl(ntime, dt, filename);
    }
  }

 
  return true;
}

/* This is called after HiOp finishes. x is LOCAL to each processor ! */
void OptimProblem::solution_callback(hiop::hiopSolveStatus status, int n, const double* x, const double* z_L, const double* z_U, int m, const double* g, const double* lambda, double obj_value) {
  
  if (mpirank_world == 0) {
    /* Print optimized parameters */
    FILE *optimfile;
    optimfile = fopen("param_optimized.dat", "w");
    for (int i=0; i<n; i++){
      fprintf(optimfile, "%1.14e\n", x[i]);
    }
    fclose(optimfile);

    /* Print out control functions */
    setDesign(n, x);
    int ntime = primalbraidapp->ntime;
    double dt = primalbraidapp->total_time / ntime;
    char filename[255];
    Hamiltonian* hamil = primalbraidapp->hamiltonian;
    for (int ioscil = 0; ioscil < hamil->getNOscillators(); ioscil++) {
        sprintf(filename, "control_optimized_%02d.dat", ioscil+1);
        hamil->getOscillator(ioscil)->flushControl(ntime, dt, filename);
    }
  }
}


/* This is called after each iteration. x is LOCAL to each processor ! */
bool OptimProblem::iterate_callback(int iter, double obj_value, int n, const double* x, const double* z_L, const double* z_U, int m, const double* g, const double* lambda, double inf_pr, double inf_du, double mu, double alpha_du, double alpha_pr, int ls_trials) {

  // /* Add to state output files. */
  // if (primalbraidapp->ufile != NULL)  fprintf(primalbraidapp->ufile, "\n\n# Iteration %d\n", iter);
  // if (primalbraidapp->vfile != NULL)  fprintf(primalbraidapp->vfile, "\n\n# Iteration %d\n", iter);
  // if (adjointbraidapp->ufile != NULL) fprintf(adjointbraidapp->ufile, "\n\n# Iteration %d\n", iter);
  // if (adjointbraidapp->vfile != NULL) fprintf(adjointbraidapp->vfile, "\n\n# Iteration %d\n", iter);

  return true;
}


bool OptimProblem::get_MPI_comm(MPI_Comm& comm_out){
  comm_out = comm_hiop;
  return true;
}