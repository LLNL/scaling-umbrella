function exampleobjfunc()
	N = 4
	#3
	Nguard = 3
	Ntot = N + Nguard
	
	Ident = Matrix{Float64}(I, Ntot, Ntot)   
	utarget = Ident[1:Ntot,1:N]
	utarget[:,3] = Ident[:,4]
	utarget[:,4] = Ident[:,3]
	
	#utarget[:,1] = Ident[:,2]
	#utarget[:,2] = Ident[:,1]
	
	cfl = 0.05

	T = 150

	testadjoint = 0
	maxpar =0.09
	
	params = objfunc.parameters(N,Nguard,T,testadjoint,maxpar,cfl, utarget)
	#pcof = rand(4)

	pcof = [1e-3, 2e-3, -2e-3]

	 m = readdlm("bspline-200-t150.dat")
	pcof = Array{Float64,1}(m[6:end,1])
	pcof  = zeros(250)
	order = 2

    verbose = true
    weights = 1

	if verbose
  	    pl1, pl2, objv, grad = objfunc.traceobjgrad(pcof, params, order, verbose, true, weights)
	else
	    objv, grad  = objfunc.traceobjgrad(pcof, params, order, verbose, true, weights)
	end
	
	println("objv: ", objv)
	println("objgrad: ", grad)
	
	if verbose
	  pl1
	end
end

function example_noguard()
	N = 4
	#3
	Nguard = 0
	Ntot = N + Nguard
	
	Ident = Matrix{Float64}(I, Ntot, Ntot)   
	utarget = Ident[1:Ntot,1:N]
	utarget[:,3] = Ident[:,4]
	utarget[:,4] = Ident[:,3]
	
	#utarget[:,1] = Ident[:,2]
	#utarget[:,2] = Ident[:,1]
	
	cfl = 0.05

	T = 150.0

	testadjoint = 0
	maxpar =0.09
	
	params = objfunc.parameters(N,Nguard,T,testadjoint,maxpar,cfl, utarget)
	#pcof = rand(4)

#	pcof = [1e-3, 2e-3, -2e-3]

#	 m = readdlm("bspline-200-t150.dat")
#	pcof = Array{Float64,1}(m[6:end,1])

	pcof  = zeros(250)

order = 2

    verbose= false
    adjoint= true
    
	if verbose && adjoint
  	    pl1, pl2, objv, grad = objfunc.traceobjgrad(pcof,params,order, verbose, adjoint)
       elseif verbose  
  	    pl1, pl2, objv = objfunc.traceobjgrad(pcof,params,order, verbose, adjoint)
	elseif adjoint
	    objv, grad  = objfunc.traceobjgrad(pcof, params, order, verbose, adjoint)
	else
	    objv  = objfunc.traceobjgrad(pcof, params, order, verbose, adjoint)
	end
	
	println("objv: ", objv)
	if adjoint
          println("objgrad: ", grad)
	end
	
	if verbose
	  pl1
	end
end


function testgrad()
	N = 4
	Nguard = 0
	Ntot = N + Nguard
	
	Ident = Matrix{Float64}(I, Ntot, Ntot)   
	utarget = Ident[1:Ntot,1:N]
	utarget[:,3] = Ident[:,4]
	utarget[:,4] = Ident[:,3]

	
	cfl = 0.05
	T = 150
	testadjoint = 0
	maxpar =0.09
	
	params = objfunc.parameters(N,Nguard,T,testadjoint,maxpar,cfl, utarget)

	pcof1 = [1e-3, 2e-3, -2e-3]
	order = 2
	eps = 1e-9
	pcof2 = pcof1

    verbose = true
    weights = 2

    objv1, grad1  = objfunc.traceobjgrad(pcof1, params, order, verbose, true, weights)
#    @show(grad1)
    pcof2[1] = pcof1[1] + eps
    objv2, grad2  = objfunc.traceobjgrad(pcof2, params, order, verbose, true, weights)

    @show(objv1)
    @show(objv2)
    @show(grad1)
    @show(grad2)
    
    println("Gradient: ", grad1[1], " Approximated by Finite-Differences: ", (objv2-objv1)/eps)

end