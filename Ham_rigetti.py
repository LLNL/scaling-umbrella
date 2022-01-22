import numpy as np


def getLoweringOperators():

    # lowering oscillator for 3 levels, dimension 3x3:
    lower3 = np.matrix([[0,1,0],[0,0,np.sqrt(2)],[0,0,0]])
    
    # Set up lowering operators for qubit 0 and 1 in full dimension 9x9:
    id3 = np.matrix(np.eye(3))
    a = []
    a.append( np.kron(lower3, id3) )
    a.append( np.kron(id3, lower3) )
    
    return a

def evalHd_mat():
    a = getLoweringOperators()

    # Qubit 0: Hd in full dimension 9x9:
    omega0 = 3.887 # in GHz
    xi0 = 0.187 # in GHz  
    Hd = omega0 * 2*np.pi * ( a[0].getH() * a[0] ) - xi0/2.0 * 2*np.pi * (a[0].getH() * a[0].getH() * a[0] * a[0]) 
    
    # Add coupling to Hd in full dimensions 9x9
    g01 = 0.01
    Hd += g01 * 2*np.pi * (a[0] + a[0].getH()) * (a[1] + a[1].getH())
    
    #print("Hd=\n", Hd)

    return Hd


## THIS IS THE FUNCTION THAT QUANDARY CALLS! ##
# Return the vectorized Hamiltonian, column major vectorization (order='F')
# Quandary requires a *list* of elements. Therefore, need to cast to an array, than flatten it, then cast to a list.
def getHd():

    Hd = evalHd_mat()
    Hdlist = list(np.array(Hd).flatten(order='F'))
    return Hdlist



def main():
    Hd = evalHd()

    with open('Bd_test.txt', 'r') as f:
        l = [[float(num) for num in line.split()] for line in f]
    #print(l)
    Bd_test = np.matrix(l)
    print("Bd_test =", Bd_test)

    for i in range(9):
        for j in range(9):
            Hd[i,j] = round(Hd[i,j], 4)
    print("Hd=", Hd)

    # test -In \kron Hd + Hd \kron In
    #idn = np.eye(9)
    #Hd = - np.kron(idn, Hd) + np.kron(Hd, idn)
    
    err = np.linalg.norm(Hd- Bd_test)
    print("ERROR = ",err);
    print("Diff = ",Hd-Bd_test)
    
    ### Time-dependent Hamiltonian terms Hc ###
    # Qubit 0: No controls, no time-dependent terms
    # Qubit 1: 
    #Put matrices for time-dependent terms into a vector of matrices.
    #Hc = [N1, - (N1*N1 - N1)]

def omega1(flux):
    # magic function. TODO.
    return np.sin(flux)

def xi1(flux):
    # magic function. TODO.
    return np.cos(flux)




if __name__ == "__main__":
    main()
