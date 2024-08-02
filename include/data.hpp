#include <string>
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

    double tstart;        /* Time stamp of the first data point */
    double tstop;         /* Time stamp of the last data point */
    double dt;             /* Sample rate of the data */
    std::vector<std::vector<Vec>> data; /* For each pulse_number: List of states at each data time-step */
    std::vector<std::string> data_name; /* Name of the data files */

	public:
    Data();
    Data(std::vector<std::string> data_name, double tstop, int dim, int npulses = 1);
    virtual ~Data();

    /* Get number of data elements */
    int getNData(){ return data[0].size(); };

    /* Get data time-spacing and first and last time point */
    double getDt(){ return dt; };
    double getTStart(){ return tstart; };
    double getTStop(){ return tstop; };

    /* If data point exists at this time, return it. Otherwise, returns NULL */
    Vec getData(double time, int pulse_num=0);

    /* Write expected energy of a data trajectory to file */
    void writeExpectedEnergy(const char* filename, int pulse_num=0);
};

/* Class for data generated by Quandary simulations - any number of levels */
class SyntheticQuandaryData : public Data {
  public:
    SyntheticQuandaryData (std::vector<std::string> data_name, double data_tstop, int dim);
    ~SyntheticQuandaryData ();

    /* Loads data and sets first and last time point as well as sampling step size */
    void loadData(double* tstart, double* tstop, double* dt);
};


/* Class for data generated on Tant with 2level measurement operators. Compatible with database 231110_SG_Tant_2level */
class Tant2levelData : public Data {
  protected:
    int nshots;    /* Number of shots */
    bool corrected;  /* If true, using physical density matrices */

  public:
    Tant2levelData(std::vector<std::string> data_name, double data_tstop, int dim, bool corrected, int npulses);
    ~Tant2levelData();

    /* Loads data and sets first and last time point as well as sampling step size */
    void loadData(double* tstart, double* tstop, double* dt);
};

/* Class for data generated on Tant with 3 level measurement operators. Compatible with database 240711/ and 240715/ */
class Tant3levelData : public Data {
  protected:
    bool corrected;  /* If true, using physical density matrices */

  public:
    Tant3levelData(std::vector<std::string> data_name, double data_tstop, int dim, bool corrected, int npulses);
    ~Tant3levelData();

    /* Loads data and sets first and last time point as well as sampling step size */
    void loadData(double* tstart, double* tstop, double* dt);
};