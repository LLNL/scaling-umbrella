from quandary import * 

## Two qubit test case ##

Ne = [2,2]  # Number of essential energy levels
Ng = [0,0]  # Number of extra guard levels

# 01 transition frequencies [GHz] per oscillator
freq01 = [4.10595, 4.8152] 
# Anharmonicities [GHz] per oscillator
selfkerr = [0.2198, 0.2252]
# Coupling strength [GHz] (Format [0<->1, 0<->2, ..., 1<->2, ... ])
crosskerr = [0.01]  # Crossker coupling of qubit 0<->1
# Frequency of rotations for computational frame [GHz] per oscillator
favg = sum(freq01)/len(freq01)
rotfreq = favg*np.ones(len(Ne))
# If Lindblad solver: Specify decay (T1) and dephasing (T2) [ns]
# T1 = [] # [100.0, 110.0]
# T2 = [] # [80.0, 90.0]

# Set the time duration (ns)
T = 150.0

# Bounds on the control pulse (in rotational frame, p and q) [MHz] per oscillator
maxctrl_MHz = 10.0*np.ones(len(Ne))  

# Set the amplitude of initial (randomized) control vector for each oscillator 
amp_frac = 0.9
initctrl_MHz = [amp_frac * maxctrl_MHz[i] for i in range(len(Ne))]

# Set up the CNOT target gate
unitary = np.identity(np.prod(Ne))
unitary[2,2] = 0.0
unitary[3,3] = 0.0
unitary[2,3] = 1.0
unitary[3,2] = 1.0
# print("Target gate: ", unitary)

# Quandary run options
runtype = "optimization" # "simulation", or "gradient", or "optimization"
quandary_exec="/Users/guenther5/Numerics/quandary/main"
# quandary_exec="/cygdrive/c/Users/scada-125/quandary/main.exe"
ncores = np.prod(Ne)  # Number of cores 
datadir = "./CNOT_run_dir"  # Compute and output directory 
verbose = True

# Potentially load initial control parameters from a file
# pcof0_filename = "./CNOT_params.dat"

# Prepare Quandary
myconfig = QuandaryConfig(Ne=Ne, Ng=Ng, freq01=freq01, selfkerr=selfkerr, crosskerr=crosskerr, rotfreq=rotfreq, T=T, maxctrl_MHz=maxctrl_MHz, initctrl_MHz=initctrl_MHz, targetgate=unitary, verbose=verbose)

# Execute quandary
time, pt, qt, ft, expectedEnergy, popt, infidelity, optim_hist = quandary_run(myconfig, quandary_exec=quandary_exec, ncores=ncores, datadir=datadir)


print(f"Fidelity = {1.0 - infidelity}")

# Plot the control pulse and expected energy level evolution
plot_pulse(Ne, time, pt, qt)
plot_expectedEnergy(Ne, time, expectedEnergy)

