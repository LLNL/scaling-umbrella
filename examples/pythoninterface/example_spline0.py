# Make sure you have the location of quandary.py in your PYTHONPATH. E.g. with
#   > export PYTHONPATH=/path/to/quandary/:$PYTHONPATH
# Further, make sure that your quandary executable is in your $PATH variable. E.g. with
#   > export PATH=/path/to/quandary/:$PATH
from quandary import * 

## Two qubit test case: CNOT gate, two levels each, no guard levels, dipole-dipole coupling 5KHz ##

# 01 transition frequencies [GHz] per oscillator
freq01 = [4.80595, 4.8601] 

# Coupling strength [GHz] (Format [0<->1, 0<->2, ..., 1<->2, ... ])
Jkl = [0.005]  # Dipole-Dipole coupling of qubit 0<->1

# Frequency of rotations for computational frame [GHz] per oscillator
favg = sum(freq01)/len(freq01)
rotfreq = favg*np.ones(len(freq01))

# Set the pulse duration (ns)
T = 200.0

# Set up the CNOT target gate
unitary = np.identity(4)
unitary[2,2] = 0.0
unitary[3,3] = 0.0
unitary[2,3] = 1.0
unitary[3,2] = 1.0
# print("Target gate: ", unitary)

# Flag for printing out more information to screen
verbose = False

# For reproducability: Random number generator seed
rand_seed=1234

# Spline order (0 or 2)
spline_order = 0
nsplines = 1000

# Optionally, load initial control parameters from a file. 
pcof0_filename = os.getcwd() + "/BS0_params.dat"  # use absolute path!
#pcof0_filename = os.getcwd() + "/BS0-pert_params.dat"  # use absolute path!

# New penalty paramter for variation of the control parameters
gamma_variation = 1.0

# Carrier freq's
carrier_frequency =[[0.0], [0.0]]

# Set up the Quandary configuration for this test case. Make sure to pass all of the above to the corresponding fields, compare help(Quandary)!
quandary = Quandary(freq01=freq01, Jkl=Jkl, rotfreq=rotfreq, T=T, targetgate=unitary, verbose=verbose, rand_seed=rand_seed, spline_order=spline_order, nsplines=nsplines, carrier_frequency=carrier_frequency, gamma_variation=gamma_variation, pcof0_filename=pcof0_filename) 

# Execute quandary
#t, pt, qt, infidelity, expectedEnergy, population = quandary.optimize()
t, pt, qt, infidelity, expectedEnergy, population = quandary.simulate()

print(f"Fidelity = {1.0 - infidelity}")

# Plot the control pulse and expected energy level evolution
if True:
	plot_pulse(quandary.Ne, t, pt, qt)
	plot_expectedEnergy(quandary.Ne, t, expectedEnergy) 
	# plot_population(quandary.Ne, t, population)


# You can predict the decoherence error of optimized dynamics:
# print("Evaluate accuracy under decay and dephasing decoherence:\n")
# T1 = [100000.0, 10000.0] #[ns] decay for each qubit
# T2 = [80000.0 , 80000.0] #[ns] dephase for each qubit
# quandary_lblad = Quandary(freq01=freq01, Jkl=Jkl, rotfreq=rotfreq, T=T, targetgate=unitary, verbose=verbose, T1=T1, T2=T2)
# quandary_lblad.pcof0 = quandary.popt[:]
# t, pt, qt, infidelity, expect, _ = quandary_lblad.simulate(maxcores=8) # Running on 8 cores

