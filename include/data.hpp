#include <string>
#include "config.hpp"
#include <sstream>
#include <cstring>
#include <iostream>
#include <fstream>
#include "util.hpp"
#include "defs.hpp"
#include <assert.h>
#include <petscts.h>
#include <vector>
#include "gellmannbasis.hpp"
#pragma once

/* Abstract base data class */
class Data{
  protected:
    int dim;              // Dimension of full vectorized system: N^2 for Lindblad or N for Schroedinger, or -1 if not learning.
    int npulses;          // Number of pulses
    int npulses_local;    // Number of pulses per optim processor

    double tstart;        /* Time stamp of the first data point [ns] */
    double tstop;         /* Time stamp of the last data point [ns] */
    double dt;             /* Sample rate of the data [ns] */
    std::vector<std::vector<Vec>> data; /* For each pulse_number: List of states at each data time-step */

    std::vector<std::vector<double>> controlparams; /* Control parameters. Could be empty (if synthetic data), or 2 values (if constant p and q), or a list of bspline parameters (if random pulses) */

    MPI_Comm comm_optim;
    int mpirank_optim;
    int mpisize_optim;
    int mpirank_world;
    int mpisize_world;

	public:
    Data();
    Data(MapParam config, MPI_Comm comm_optim, std::vector<std::string>& data_name, int dim);
    virtual ~Data();

    /* Get number of data elements */
    int getNData(){ return data[0].size(); };

    double getDt(){ return dt; };
    double getTStart(){ return tstart; };
    double getTStop(){ return tstop; };
    double getNPulses(){ return npulses; };
    double getNPulses_local(){ return npulses_local; };

    /* Suggest a time-step size that matches to the data sampling (is an integer divisor of the data sampling rate and that is close to dt_old */
    double suggestTimeStepSize(double dt_old);

    /* Get control parameters that were used for data generation */
    std::vector<double> getControls(int ipulse = 0, int ioscillator=0); 

    /* If data point exists at this time, return it. Otherwise, returns NULL */
    Vec getData(double time, int pulse_num);

    /* Write expected energy of a data trajectory to file */
    void writeExpectedEnergy(const char* filename, int pulse_num);

    /* Write full density matrix of a data trajectory to files */
    void writeFullstate(const char* filename_Re, const char* filename_Im, int pulse_num);
};

/* Class for data generated by Quandary simulations - any number of levels */
class SyntheticQuandaryData : public Data {
  public:
    SyntheticQuandaryData (MapParam config, MPI_Comm comm_optim, std::vector<std::string>& data_name, int dim);
    ~SyntheticQuandaryData ();

    /* Loads data and sets first and last time point as well as sampling step size */
    void loadData(std::vector<std::string>& data_name, double* tstart, double* tstop, double* dt);
};


/* Class for data generated on Tant with 2level measurement operators. Compatible with database 231110_SG_Tant_2level */
class Tant2levelData : public Data {
  protected:
    int nshots;    /* Number of shots */
    bool corrected;  /* If true, using physical density matrices */

  public:
    Tant2levelData(MapParam config, MPI_Comm comm_optim, std::vector<std::string>& data_name, int dim);
    ~Tant2levelData();

    /* Loads data and sets first and last time point as well as sampling step size */
    void loadData(std::vector<std::string>& data_name, double* tstart, double* tstop, double* dt);
};

/* Class for data generated on Tant with 3 level measurement operators. Compatible with database 240711/ and 240715/ */
class Tant3levelData : public Data {
  protected:
    bool corrected;  /* If true, using physical density matrices */

  public:
    Tant3levelData(MapParam config, MPI_Comm comm_optim, std::vector<std::string>& data_name, int dim);
    ~Tant3levelData();

    /* Loads data and sets first and last time point as well as sampling step size */
    void loadData(std::vector<std::string>& data_name, double* tstart, double* tstop, double* dt);
};