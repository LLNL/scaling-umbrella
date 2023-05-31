import numpy as np
from functools import reduce

# Estimates the number of time steps based on eigenvalues of the system Hamiltonian and maximum control Hamiltonians.
# NOTE: The estimate does not account for quickly varying signals or a large number of splines. Double check that at least 2-3 points per spline are present to resolve control function.
# TODO: Automate this!
def estimate_timesteps(T, Hsys, Hc_re=[], Hc_im=[], maxctrl_MHz=[], *, Pmin=40):
    assert len(maxctrl_MHz) >= len(Hc_re)


    # Set up Hsys + maxctrl*Hcontrol
    K1 = np.copy(Hsys) 
    for i in range(len(Hc_re)):
        max_radns = maxctrl_MHz[i]*2.0*np.pi/1e+3
        K1 += max_radns * Hc_re[i] 
        K1 = K1 + 1j * max_radns * Hc_im[i] # can't use += due to type!
    
    # Estimate time step
    eigenvalues = np.linalg.eigvals(K1)
    maxeig = np.max(np.abs(eigenvalues))
    # ctrl_fac = 1.2  # Heuristic, assuming that the total Hamiltonian is dominated by the system part.
    ctrl_fac = 1.0
    samplerate = ctrl_fac * maxeig * Pmin / (2 * np.pi)
#     print(f"{samplerate=}")
    nsteps = int(np.ceil(T * samplerate))

    return nsteps



# Computes system resonances, to be used as carrier wave frequencies
# Returns resonance frequencies in GHz and corresponding growth rates
def get_resonances(Ne, Hsys, Hc_re, Hc_im, *, cw_amp_thres=6e-2, cw_prox_thres=1e-3, verbose=True):
    if verbose:
        print("\nget_resonances: Ignoring growth rate slower than:", cw_amp_thres, "and frequencies closer than:", cw_prox_thres, "[GHz]")

    nqubits = len(Hc_re)
    n = Hsys.shape[0]

    # Get eigenvalues of system Hamiltonian (GHz)
    Hsys_evals, Utrans = np.linalg.eig(Hsys)
    Hsys_evals = Hsys_evals.real  # Eigenvalues may have a small imaginary part due to numerical precision
    Hsys_evals = Hsys_evals / (2 * np.pi)

    resonances = []
    speed = []
    for q in range(nqubits):
        Hctrl_ad = Hc_re[q] - Hc_im[q]   # divide by 2. Adjust the cw_amp_thres. 
        Hctrl_ad_trans = Utrans.T @ Hctrl_ad @ Utrans

        resonances_a = []
        speed_a = []
        if verbose:
            print("  Resonances in oscillator #", q)
        for i in range(n):
            for j in range(i):
                if abs(Hctrl_ad_trans[i, j]) >= cw_amp_thres:
                    delta_f = Hsys_evals[i] - Hsys_evals[j]
                    if abs(delta_f) < 1e-10:
                        delta_f = 0.0
                    if not any(abs(delta_f - f) < cw_prox_thres for f in resonances_a):
                        resonances_a.append(delta_f)
                        speed_a.append(abs(Hctrl_ad_trans[i, j]))

                        if verbose:
                            print("    Resonance from =", j, "to =", i, ", frequency", delta_f, ", growth rate=", abs(Hctrl_ad_trans[i, j]))
        
        resonances.append(resonances_a)
        speed.append(speed_a)

    Nfreq = np.zeros(nqubits, dtype=int)
    om = [[] for _ in range(nqubits)]
    growth_rate = [[] for _ in range(nqubits)]
    
    for q in range(nqubits):
        Nfreq[q] = max(1, len(resonances[q]))  # at least one being 0.0
        om[q] = np.zeros(Nfreq[q])
        if len(resonances[q]) > 0:
            om[q] = np.array(resonances[q])
        growth_rate[q] = np.ones(Nfreq[q])
        if len(speed[q]) > 0:
            growth_rate[q] = np.array(speed[q])

    # return om, growth_rate, Utrans
    return om, growth_rate


# THE BELOW IS COPIED FROM Juqbox setup_std_model. It sorts and culls the carrier waves. SG: Not sure what to do with it. Needed? TODO: Anders?
    # # allocate and sort the vectors (ascending order)
    # om_p = [[]] * Nosc
    # growth_rate_p = [[]] * Nosc
    # use_p = [[]] * Nosc
    # for q in range(Nosc):
    #     om_p[q] = np.zeros(Nfreq[q])
    #     growth_rate_p[q] = np.zeros(Nfreq[q])
    #     use_p[q] = np.zeros(Nfreq[q], dtype=int)  # By default, don't use any freq's
    #     p = np.argsort(om[q])  # sort indices based on om[q]
    #     om_p[q][:] = om[q][p]
    #     growth_rate_p[q][:] = growth_rate[q][p]

    # print("Rotfreq =", rot_freq)
    # print("omp =", om_p)
    # print("growth_rate =", growth_rate_p)

    # print("Sorted CW freq's:")
    # for q in range(Nosc):
    #     print("Ctrl Hamiltonian #", q, ", lab frame carrier frequencies:", rot_freq[q] + om_p[q] / (2 * np.pi), "[GHz]")
    #     print("Ctrl Hamiltonian #", q, ",                   growth rate:", growth_rate_p[q], "[1/ns]")

    # # Try to identify groups of almost equal frequencies
    # for q in range(Nosc):
    #     seg = 0
    #     rge_q = np.max(om_p[q]) - np.min(om_p[q])  # this is the range of frequencies
    #     k0 = 0
    #     for k in range(1, Nfreq[q]):
    #         delta_k = om_p[q][k] - om_p[q][k0]
    #         if delta_k > 0.1 * rge_q:
    #             seg += 1
    #             # find the highest rate within the range [k0,k-1]
    #             rge = range(k0, k)
    #             om_avg = np.sum(om_p[q][rge]) / len(rge)
    #             print("Osc #", q, "segment #", seg, "Freq-range:", (np.max(om_p[q][rge]) - np.min(om_p[q][rge])) / (2 * np.pi), "Freq-avg:", om_avg / (2 * np.pi) + rot_freq[q])
    #             use_p[q][k0] = 1
    #             # average the cw frequency over the segment
    #             om_p[q][k0] = om_avg
    #             k0 = k  # start a new group
    #     # find the highest rate within the last range [k0,Nfreq[q]]
    #     seg += 1
    #     rge = range(k0, Nfreq[q])
    #     om_avg = np.sum(om_p[q][rge]) / len(rge)
    #     print("Osc #", q, "segment #", seg, "Freq-range:", (np.max(om_p[q][rge]) - np.min(om_p[q][rge])) / (2 * np.pi), "Freq-avg:", om_avg / (2 * np.pi) + rot_freq[q])
    #     use_p[q][k0] = 1
    #     om_p[q][k0] = om_avg

    #     # cull out unused frequencies
    #     om[q] = np.zeros(np.sum(use_p[q]))
    #     growth_rate[q] = np.zeros(np.sum(use_p[q]))
    #     j = 0
    #     for k in range(Nfreq[q]):
    #         if use_p[q][k] == 1:
    #             j += 1
    #             om[q][j] = om_p[q][k]
    #             growth_rate[q][j] = growth_rate_p[q][k]
    #     Nfreq[q] = j  # correct the number of CW frequencies for oscillator 'q'

    # print("\nSorted and culled CW freq's:")
    # for q in range(Nosc):
    #     print("Ctrl Hamiltonian #", q, ", lab frame carrier frequencies:", rot_freq[q] + om[q] / (2 * np.pi), "[GHz]")
    #     print("Ctrl Hamiltonian #", q, ",                   growth rate:", growth_rate[q], "[1/ns]")


# Identity operator of dimension n
def ident(n):
    return np.diag(np.ones(n))

# Lowering operator of dimension n
def lowering(n):
    return np.diag(np.sqrt(np.arange(1, n)), k=1)

# Number operator of dimension n
def number(n):
    return np.diag(np.arange(n))


# Create Hamiltonian operators. Essential levels ONLY!
def hamiltonians(Ne, freq01, selfkerr, crosskerr=[], Jkl = [], *, rotfreq=None, verbose=True):
    if rotfreq==None:
        rotfreq=np.zeros(len(Ne))

    nqubits = len(Ne)
    assert len(selfkerr) == nqubits
    assert len(freq01) == nqubits
    assert len(rotfreq) == nqubits
    assert len(selfkerr) == nqubits

    n = np.prod(Ne)     # System size 

    # Set up lowering operators in full dimension
    Amat = []
    for i in range(len(Ne)):
        # predim = 1
        # for j in range(i):
                # predim*=N[j]
        ai = lowering(Ne[i])
        for j in range(i):
            ai = np.kron(ident(Ne[j]), ai) 
        # postdim = 1
        for j in range(i+1,len(Ne)):
                # postdim*=N[j]
            ai = np.kron(ai, ident(Ne[j])) 
        # a.append(tensor(qeye(predim), lowering(N[i]), ident(postdim)))
        Amat.append(ai)

    # Set up system Hamiltonian: Duffing oscillators
    Hsys = np.zeros((n, n))
    for q in range(nqubits):
        domega_radns =  2.0*np.pi * (freq01[q] - rotfreq[q])
        selfkerr_radns = 2.0*np.pi * selfkerr[q]
        Hsys +=  domega_radns * Amat[q].T @ Amat[q]
        Hsys -= selfkerr_radns/2 * Amat[q].T @ Amat[q].T @ Amat[q] * Amat[q]

    # Add system Hamiltonian coupling terms
    if len(crosskerr)>0:
        idkl = 0 
        for q in range(nqubits):
            for p in range(q + 1, nqubits):
                crosskerr_radns = 2.0*np.pi * crosskerr[idkl]
                Hsys -= crosskerr_radns * Amat[q].T @ Amat[q] @ Amat[p].T @ Amat[p]
                idkl += 1
    if len(Jkl)>0:
        idkl = 0 
        for q in range(nqubits):
            for p in range(q + 1, nqubits):
                Jkl_radns  = 2.0*np.pi*Jkl[idkl]
                Hsys += Jkl_radns * Amat[q].T @ Amat[p] + Amat[q] @ Amat[p].T
                idkl += 1
    
    # Set up control Hamiltonians
    Hc_re = [Amat[q] + Amat[q].T for q in range(nqubits)]
    Hc_im = [Amat[q] - Amat[q].T for q in range(nqubits)]

    if verbose:
        print(f"*** {nqubits} coupled quantum systems setup ***")
        print("System Hamiltonian frequencies [GHz]: f01 =", freq01, "rot. freq =", rotfreq)
        print("Selfkerr=", selfkerr)
        print("Coupling: X-Kerr=", crosskerr, ", J-C=", Jkl)

    return Hsys, Hc_re, Hc_im