# Quandary - Optimal control for open and closed quantum systems
Quandary implements an optimization solver for open and closed optimal quantum control. The underlying quantum dynamics model open or closed quantum systems, using either Schroedinger's equation for a state vector (closed), or Lindblad master equation for a density matrix (open). The control problem aims to find control pulses that drive the system to a desired target state. Quandary targets deployment on High-Performance Computing platforms, offering various levels for parallelization using the message passing paradigm. 

A documentation is under development. In the meantime, refer to the user guide in the `doc/` folder for information on the underlying mathematical models as well as details on their implementation in Quandary, and parallel distribution.

For questions, feel free to reach out to Stefanie Guenther [guenther5@llnl.gov].

## Dependencies
This project relies on Petsc [https://petsc.org/release/] to handle (parallel) linear algebra. Optionally Slepsc [https://slepc.upv.es] can be used to solve some eigenvalue problems if desired (e.g. for the Hessian...)
* **Required:** Install Petsc:

    Check out [https://petsc.org/release/] for the latest installation guide. On MacOS, you can also `brew install petsc`. As a quick start, you can also try the below:
    * Download tarball for Petsc here [https://ftp.mcs.anl.gov/pub/petsc/release-snapshots/]. Recommended version is petsc-3.17, currently.  
    * `tar -xf petsc-<version>.tar.gz`
    * `cd petsc-<version>`
    * Configure Petsc with `./configure`, check [https://petsc.org/release/install/install_tutorial] for optional arguments. Note that Petsc compiles in debug mode by default. To configure petsc with compiler optimization, consider configuration such as
        `./configure --prefix=/YOUR/INSTALL/DIR --with-debugging=0 --with-fc=0 --with-cxx=mpicxx --with-cc=mpicc COPTFLAGS='-O3' CXXOPTFLAGS='-O3'`
    * The output of `./configure` reports on how to set the `PETSC_DIR` and `PETSC_ARCH` variables
        * `export PETSC_DIR=/YOUR/INSTALL/DIR`
        * `export PETSC_ARCH=/YOUR/ARCH/PREFIX`
    * Compile petsc with `make all check'
    * Append Petsc directory to the `LD_LIBRARY_PATH`:
        * `export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$PETSC_DIR/$PETSC_ARCH`

* **Optional:** Install Slepsc
    * Read the docs here: [https://slepc.upv.es/documentation/slepc.pdf]

* **Optional:** Enable the python interface:
    - Have a working python interpreter and numpy installed, and set your `PYTHONPATH`
    - If you want to use spline-based transfer functions within the python interface, you'll need to install fitpackpp (C++ bindings for FITPACK) from here: [https://github.com/jbaayen/fitpackpp]: In the fitpackpp installation directory:
        * `mkdir build`
        * `cd build`
        * `cmake ..`
        * `make`
 
###  Petsc on LLNL's LC
Petc is already installed on LLNL LC machines, see here [https://hpc.llnl.gov/software/mathematical-software/petsc]. It is located at '/usr/tce/packages/petsc/<version>'. To use it, export the 'PETSC_DIR' variable to point to the Petsc folder, and add the 'lib' subfolder to the 'LD_LIBRARY_PATH` variable: 
* `export PETSC_DIR=/usr/tce/packages/petsc/<version>` (check the folder name for version number)
* `export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$PETSC_DIR/lib`
The 'PETSC_ARCH' variable is not needed in this case. 

Depending on your setup, you might need to load some additional modules, such as openmpi, e.g. as so:
* `module load openmpi`

## Installation
Adapt the beginning of the 'Makefile' to set the path to your Petsc (and possibly Slepsc, python path, and fitpackpp) installation. Then,
* `make cleanup` to clean the build directory. (Note the *up* in *cleanup*.)
* `make main` to build the code (or 'make -j main' for faster build using multiple threads)


## Running
The code builds into the executable `main`. It takes one argument being the name of the test-case's configuration file. The file `config_template.cfg`, lists all possible configuration options. It is currently set to simulate a bipartite system with 3x20 levels (Alice - cavity testcase "AxC"). The configuration file is filled with comments that should help users set up their test case and match the options to the description in the user guide.
* `./main config_template.cfg`


## Community and Contributing

Quandary is an open source project that is under heavy development. Contributions in all forms are very welcome, and can be anything from new features to bugfixes, documentation, or even discussions. Contributing is easy, work on your branch, create a pull request to master when you're good to go and the regression tests in 'tests/' pass.

## Publications
* S. Guenther, N.A. Petersson, J.L. DuBois: "Quantum Optimal Control for Pure-State Preparation Using One Initial State", AVS Quantum Science, vol. 3, arXiv preprint <https://arxiv.org/abs/2106.09148> (2021)
* S. Guenther, N.A. Petersson, J.L. DuBois: "Quandary: An open-source C++ package for High-Performance Optimal Control of Open Quantum Systems", submitted to IEEE Supercomputing 2021, arXiv preprint <https://arxiv.org/abs/2110.10310> (2021)

## License

Quandary is distributed under the terms of the MIT license. All new contributions must be made under this license. See LICENSE, and NOTICE, for details. 

SPDX-License-Identifier: MIT
