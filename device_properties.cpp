/* Copyright 2017 Advanced Detector Centre, Department of Electronic and
Electrical Engineering, University of Sheffield, UK.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.*/

#include "model.h"
#include "SMC.h"
#include "device.h"
#include "functions.h"
#include "dev_prop_func.h"
#include "tools.h"
#include "carrier.h"
#include <stdio.h>
#include <tchar.h>
#include <math.h>

void device_properties(int material){
	int timearray, Highest;  
    double cumulative, voltage;
    SMC constants; // constants will be the parameter set
    constants.mat(material); // tell constants what material to use
    SMC *pointSMC = &constants;
    device diode(pointSMC);
    FILE *out;  
    double BreakdownCurrent=1e-4; //define the current threshold for avalanche breakdown as 0.1mA
    if ((out=fopen("multiplication.txt","w"))==NULL)//Opens and error checks
    {   printf("Error: multiplication.txt can't open\n");
    }
	//read in bias
	int bias_count=biascounter(); //counts number of voltages to be simulated
	FILE *bias;	
    if ((bias=fopen("bias_input.txt","r"))==NULL)//Opens and error checks
    {
        	printf("Error: bias.txt can't be opened'\n");
    }
	double V[bias_count];
	bias_count=0;
	while(fscanf(bias,"%lf",&voltage)>0){
    	V[bias_count]=voltage;
    	bias_count++;
	}
    fclose(bias);
    			
	int timeslice = timesliceread();
	int usDevice = usDeviceread();	    
    double simulationtime = simulationtimeread();    
    double Ntrials = trialsread();
    tools simulation(pointSMC);
    simulation.scattering_probability();//this function returns 0 if no output can be generated and the user wants to quit
    sgenrand(835800);//seeds the random number generator      	
    int num;    
    double Efield,nth,npha,nph,nphe,nii,nsse,Energy,z_pos,dE,kf,kxy,kz,nssh;
    double drift_t;
	
	// create electron and hole classes, too large for stack so have been created with new.
    carrier* electron=new carrier(pointSMC);
	carrier* hole=new carrier(pointSMC);

	double breakdown, Pbreakdown, Vsim;
    /**** BEGIN SIMULATION LOOP VOLTAGE ****/
    int bias_array=0;
    printf("%d %d \n", bias_count, bias_array);
    for(bias_array=0;bias_array<bias_count;bias_array++)
    {
    Vsim=V[bias_array];
    
    diode.profiler(Vsim);//generates field profile for diode
    printf("Width = %e \n", diode.Get_width());
    double timestep=diode.Get_width()/((double)timeslice*1e5); //Time tracking step size in seconds
    printf("timestep = %e \n", timestep);
    int CurrentArray=simulationtime/timestep; // calculates number of timesteps required for simulationtime
    double cutofftime=(CurrentArray-5)*timestep; // prevents overflow
    int cutoff =0;
	double* I=new double[CurrentArray];
	double* Inum=new double[CurrentArray];
	int Iarray;
    for (Iarray=0;Iarray<CurrentArray;Iarray++){
    	I[Iarray]=0;    	
	}
	FILE *tbout;
	FILE *Mout;
	char nametb[] = "time_to_breakdown.txt";
	char nameM[]= "gain_out.txt";
	char voltagetb[8];
	snprintf(voltagetb,sizeof(voltagetb),"%g",Vsim);
	char filetb[strlen(voltagetb)+strlen(nametb)+1];
	snprintf(filetb,sizeof(filetb),"%s%s",voltagetb,nametb);
	if ((tbout=fopen(filetb,"w"))==NULL)
    {
        printf("Error: tbout can't be opened'\n");
    }
    char fileM[strlen(voltagetb)+strlen(nameM)+1];
    snprintf(fileM,sizeof(fileM),"%s%s",voltagetb,nameM);
    if ((Mout=fopen(fileM,"w"))==NULL)
    {
        printf("Error: gain out can't be opened'\n");

    }
    
    char namecounter[] = "eventcounter.txt";
    FILE *counter;
    char countname[strlen(namecounter)+strlen(voltagetb)+1];
    snprintf(countname,sizeof(countname),"%s%s",voltagetb,namecounter);
    if((counter=fopen(countname,"w"))==NULL){
    	printf("Error: Can't open counter dump file \n'");

	}
    Highest=0;
    cumulative=0;

    breakdown=0;
    double gain,Ms,F,tn,time;
    double dt,dx;
    Ms=0;
    F=0;
	gain=0;    
	double globaltime=0;
    int num_electron,num_hole,prescent_carriers, pair;
    /**** BEGIN SIMULATION LOOP TRIALS****/
    for(num=1;num<=Ntrials;num++)
    {    
    	 for (Iarray=0;Iarray<CurrentArray;Iarray++){
    		Inum[Iarray]=0;
		 }
		 num_electron=1;
         num_hole=1;
         tn=1;
         prescent_carriers=2;
         kf=0;
         kxy=0;
         kz=0;
         dE=0;
         Energy=0;         
         z_pos=0;
         drift_t=0;
         time=0;
         dt=0;
         dx=0;
         nph=0;//total phonon interactions
         npha=0;//photon absorption counter
         nphe=0;//photon emission counter
         nii=0;//impact ionization event counter
         nsse=0;//scattering event counter
         nssh=0;
         int cut2=0;
         globaltime=timestep;
         

		 /* Device 1-PIN
		    Device 2-NIP
		    Device 3-PN*/

         if (usDevice==1){
            electron->Input_pos(1,1e-10);
            hole->Input_pos(1,-1);
            prescent_carriers=1;
         }
         if (usDevice==2){
             electron->Input_pos(1,(diode.Get_width()+1e-10));
             hole->Input_pos(1,(diode.Get_width()-1e-10));
             prescent_carriers=1;
         }

         double carrierlimit=BreakdownCurrent*diode.Get_width()/(5*constants.Get_q()*1e5);

         /****TRACKS CARRIERS WHILE IN DIODE****/
         while(prescent_carriers>0 && cut2==0)
         {  /****LOOPS OVER ALL PAIRS ****/  
		 	for(pair=1;pair<=num_electron;pair++)         
              {    
			  	   int flag=0;
				   // ELECTRON PROCESS
                   z_pos=electron->Get_pos(pair);
                   time=electron->Get_time(pair);
                   dt=electron->Get_dt(pair);
                   dx=electron->Get_dx(pair);
                   if(z_pos<0) z_pos=1e-10; // resets a bad trial
                   if(z_pos<diode.Get_width() && time<globaltime)//checks inside field
                   {    flag++;                
				   		Energy=electron->Get_Egy(pair);						                   		
                        if((electron->Get_scattering(pair)==0))//if not selfscattering scatters in random direction
                        {   electron->scatter(pair,0);
                        }
                        
                            kxy=electron->Get_kxy(pair);
                        	kz=electron->Get_kz(pair);
                        
                        //electron drift process starts
                        //drifts for a random time
                        double random1;                        
                        random1=genrand();
                        drift_t= -log(random1)/(simulation.Get_rtotal());                       
                        time+=drift_t;
                        dt+=drift_t;
                        
                       //updates parameters based on random drift time
                        Efield=diode.Efield_at_x(z_pos);
                        kz+=(constants.Get_q()*drift_t*Efield)/(constants.Get_hbar());
                        dE=((constants.Get_hbar()*constants.Get_hbar())/(2*constants.Get_e_mass()))*(kxy+kz*kz)-Energy;
                        Energy=((constants.Get_hbar()*constants.Get_hbar())/(2*constants.Get_e_mass()))*(kxy+kz*kz);
                        z_pos+=dE/(constants.Get_q()*Efield);
                        dx+=dE/(constants.Get_q()*Efield);
                        if(time>cutofftime){
                        	//cuts off electron and removes it from device if user spec. timelimit exceeded
                   			z_pos=diode.Get_width()+10;
                   			cutoff=1;
					   	}
                        if(dt>=timestep){
							//calc current  if time since last calculated  >timestep                    	
                        	timearray=floor(time/timestep);
                        	int previous;
                        	previous=electron->Get_timearray(pair);
                        	int test;
                        	for(test=(previous+1);test<(timearray+1);test++){
                        		I[test]+=constants.Get_q()*dx/(dt*diode.Get_width());
                        		Inum[test]+=constants.Get_q()*dx/(dt*diode.Get_width());
							}
                        	electron->Input_timearray(pair,timearray);                        	
                        	dt=0;
                        	dx=0;
						}
						electron->Input_time(pair,time);  
						electron->Input_dt(pair,dt);
						electron->Input_dx(pair,dx);

                        //electron drift process ends
                        
                        //update electron position and energy
                       	electron->Input_pos(pair,z_pos);
                        electron->Input_Egy(pair,Energy);
                        
                        //electron scattering
                        if(z_pos<0) z_pos=1e-10;
                        if((z_pos<=diode.Get_width()))
                        {    //electron scattering process starts                        
                             double random2;
                             int Eint;
                             Eint=floor(Energy*1000.0/constants.Get_q()+0.5);
                             if (Energy>constants.Get_Emax())
                             {    Eint= constants.Get_NUMPOINTS();
                                  random2=simulation.Get_pb(2,constants.Get_NUMPOINTS());
                             }
                             else if (Energy == constants.Get_Emax()) random2=simulation.Get_pb(2,constants.Get_NUMPOINTS());
                             else{
                                  random2=genrand();
							 } 
                             
                             if(random2<=simulation.Get_pb(0,Eint)) //phonon absorption
                             {    Energy+=constants.Get_hw();
                                  npha++;
                                  nph++;
                                  electron->Input_scattering(pair,0);
                             }
                             else if(random2<=simulation.Get_pb(1,Eint)) //phonon emission
                             {    Energy-=constants.Get_hw();
                                  nphe++;
                                  nph++;
                                  electron->Input_scattering(pair,0);
                             }
                             else if(random2<=simulation.Get_pb(2,Eint)) //impact ionization
                             {    
							      Energy=(Energy-constants.Get_e_Eth())/3.0;
                                  num_electron++;
                                  electron->generation(num_electron,z_pos,Energy,time,0,floor(time/timestep));
                                  num_hole++;
                                  hole->generation(num_hole,z_pos,Energy,time,0,floor(time/timestep));
                                  tn++;
                                  nii++;
                                  prescent_carriers+=2;
                                  electron->Input_scattering(pair,0);
                             }
                             else if(random2>simulation.Get_pb(2,Eint)) //selfscattering
                             {    nsse++;
                                  electron->Input_scattering(pair,1);
                                  electron->Input_kxy(pair,kxy);
                                  electron->Input_kz(pair,kz);
                             }
                             //electron scattering process ends
                                                  
                        }
                        else prescent_carriers--;
                        
                        electron->Input_Egy(pair,Energy);
                        if(time>globaltime) flag--;

                   }
                   
                   //HOLE PROCESS
                   z_pos=hole->Get_pos(pair);
                   time=hole->Get_time(pair);
                   dt=hole->Get_dt(pair);
                   dx=hole->Get_dx(pair);
                   if(z_pos>diode.Get_width()) z_pos=diode.Get_width()-1e-10;
                   if(z_pos>=0 && time<globaltime)
                   {    Energy=hole->Get_Egy(pair);
				   		flag++;				     				          		
                   	    if((hole->Get_scattering(pair)==0))
                        {    hole->scatter(pair,2);
                        }
                        
                    	kxy=hole->Get_kxy(pair);
                        kz=hole->Get_kz(pair);
                        
                        
                        //Hole drift starts here
                        double random11;                        
                        random11=genrand();
                        drift_t= -log(random11)/simulation.Get_rtotal2();                      
                        time+=drift_t;
                        dt+=drift_t;
                        Efield=diode.Efield_at_x(z_pos);
                        kz-=((constants.Get_q()*drift_t*Efield)/(constants.Get_hbar()));
                        dE=((constants.Get_hbar()*constants.Get_hbar())/(2*constants.Get_h_mass()))*(kxy+kz*kz)-Energy;
                        Energy=((constants.Get_hbar()*constants.Get_hbar())/(2*constants.Get_h_mass()))*(kxy+kz*kz);
                        z_pos-=dE/(Efield*constants.Get_q());
                        dx+=dE/(Efield*constants.Get_q());
                        if(time>cutofftime){
							z_pos=-10;
							cutoff=1;
						}
                        if(dt>=timestep){                        	
                        	timearray=floor(time/timestep);
                        	int previous;
                        	previous=hole->Get_timearray(pair);
                        	int test;
                        	for(test=(previous+1);test<(timearray+1);test++){
                        		I[test]+=constants.Get_q()*dx/(dt*diode.Get_width());
                        		Inum[test]+=constants.Get_q()*dx/(dt*diode.Get_width());
							}                    
                        	dt=0;
                        	dx=0;
                        	hole->Input_timearray(pair,timearray);
						}
						hole->Input_time(pair,time);
                        hole->Input_dt(pair,dt);
						hole->Input_dx(pair,dx);

                        //Hole drift finishes here
                        hole->Input_pos(pair,z_pos);
                        hole->Input_Egy(pair,Energy);
                        
                        
                        if(z_pos>diode.Get_width()) z_pos=diode.Get_width()-1e-10;
                        if(z_pos>=0)
                        {    //Hole scattering starts here
                             double random22;
                             int Eint2;
                             Eint2=floor(Energy*1000.0/constants.Get_q()+0.5);
                             if (Energy>constants.Get_Emax())
                             {    Eint2=constants.Get_NUMPOINTS();
                                  random22=simulation.Get_pb2(2,constants.Get_NUMPOINTS());                                  
                             }
                             else if (Energy==constants.Get_Emax())
                             {    random22=simulation.Get_pb2(2,constants.Get_NUMPOINTS());
                             }
                             else { 
                                  random22=genrand();
							 }
                             
                             if(random22<=simulation.Get_pb2(0,Eint2)) //phonon absorption
                             { 
							 	  Energy+=constants.Get_hw();
                                  npha++;
                                  nph++;
                                  hole->Input_scattering(pair,0);
                             }
                             else if(random22<=simulation.Get_pb2(1,Eint2)) //phonon emission
                             {    
							 	  Energy-=constants.Get_hw();
                                  nphe++;
                                  nph++;
                                  hole->Input_scattering(pair,0);
                             }
                             else if(random22<=simulation.Get_pb2(2,Eint2)) //impact ionization
                             {   
							 	  Energy=(Energy-constants.Get_h_Eth())/3.0;
                                  num_electron++;
                                  electron->generation(num_electron,z_pos,Energy,time,0,floor(time/timestep));
                                  num_hole++;
								  hole->generation(num_hole,z_pos,Energy,time,0,floor(time/timestep));
                                  tn++;
                                  nii++;
                                  prescent_carriers+=2;
                                  hole->Input_scattering(pair,0);
                             }
                             else if(random22>simulation.Get_pb2(2,Eint2)) //selfscattering
                             {    
							      nssh++;
                                  hole->Input_scattering(pair,1);
                                  hole->Input_kxy(pair,kxy);
                                  hole->Input_kz(pair,kz);
                             }
                             //hole scattering ends here
                             
                        }
                        else prescent_carriers--;
                        
                        hole->Input_Egy(pair,Energy);
                        if(time>globaltime) flag--;

                   }
                   Highest=_max(Highest,pair);
                   
				   if(flag==0){
				    globaltime+=timestep;
					//printf("Step through time\n");
				   }
              }
              int scan=0;
              int scanlimit=0;
              //scans current array to detect breakdown current and stops sim early
              if(pair>carrierlimit){
	                while(scan==0){
	              		for(Iarray=0;Iarray<CurrentArray;Iarray++){
		              		if(Inum[Iarray]>BreakdownCurrent){
		              			scan=1;
		              			cut2=1;
							}						
							if(Iarray==CurrentArray-1) scan=1;
						}						
						if(Inum[Iarray]==0) ++scanlimit;						
						if(Inum[Iarray]!=0) scanlimit=0;						
						if(scanlimit>50) scan=1;					  
				  }
				}            
         }         
         gain+=tn/Ntrials; //accumilates average gain 
         Ms+=(tn*tn/Ntrials); //accumilates average Ms, used to calculate noise
         cumulative+=tn; //tracks average gain so far in simulation
         double printer=cumulative/num;        

       electron->reset();
	   hole->reset(); 
	   //checks for breakdown at end of sim
	   for (Iarray=0;Iarray<CurrentArray;Iarray++){
			if(Inum[Iarray] > BreakdownCurrent){
				breakdown++;
				fflush(counter);			
				int Iprint;
				double tb = timestep*Iarray;
				fprintf(tbout,"%d %g\n",num,tb);
				fflush(tbout);			
				break;
    		}
		}
	   
	    int arealimitnum=0;
	    double totalareanum=0;
	    double area,totalarea,x1,x2,y1,y2;
	    int i;
		for(i=0;i<(CurrentArray-1);i++)
		{				
			y1=Inum[i];
			x1=timestep*i;
			y2=Inum[i+1];
			x2=timestep*(i+1);
			area=y1*(x2-x1)+0.5*(y2-y1)*(x2-x1);
			totalareanum+=area;			
		}
		fprintf(Mout, "%d %g\n",num, totalareanum);
		fflush(Mout);
		double Pbprint=breakdown/num;
		 if(!(num%100)){
            if(cutoff==0)printf("Completed trial: %d Gain=%f Pb=%f . Max array index=%d\n",num,printer,Pbprint,Highest);
         	if(cutoff==1)printf("Completed trial: %d Cutoff Pb=%f  Max array index=%d\n",num,Pbprint,Highest);
         }
    }
	
    F=Ms/(gain*gain);
    Pbreakdown=breakdown/Ntrials;
    
    if(cutoff==0){
    printf("V= %f M= %f, F= %f, Pb= %f \n",Vsim,gain,F,Pbreakdown);
    fprintf(out,"V= %f M= %f F= %f, Pb= %f \n",Vsim,gain,F,Pbreakdown);
	}
	else{
		printf("V= %f M= cutoff, F= cutoff, Pb= %f \n",Vsim,Pbreakdown);
    	fprintf(out,"V= %f M= cutoff F= cutoff, Pb= %f \n",Vsim,Pbreakdown);
	}
    fflush(out);  
	fclose(tbout);
	FILE *Iout;
    char name[] = "current.txt";
    char Vsimchar[8];
    snprintf(Vsimchar, sizeof(Vsimchar),"%g", Vsim);
    char fileSpec[strlen(name)+strlen(Vsimchar)+1];
    snprintf(fileSpec,sizeof(fileSpec), "%s%s", Vsimchar, name);
    if ((Iout=fopen(fileSpec,"w"))==NULL)
    {
        printf("Error: current.txt\n");
    }
    fprintf(Iout,"V= %f \n", Vsim);
    fprintf(Iout,"time units in %e s\n",timestep);
    fprintf(Iout,"t                I                I/e \n");   
    int Ioutprint =0;
	int i;
    for(i=0;i<CurrentArray;i++)
    {   double timeprint=timestep*i;
        double current = I[i]/(double)Ntrials;
    	double count=I[i]/(constants.Get_q()*(double)Ntrials);
        if(I[i]>0 && Ioutprint <50){
			fprintf(Iout,"%g %g %g \n",timeprint,current,count);
			Ioutprint=0;
		}
		else if(Ioutprint <50){
			fprintf(Iout,"%g %g %g \n",timeprint,current,count);
			Ioutprint++;
		}		
	}
	fflush(Iout);
	fclose(Iout);
	delete [] I;
	delete [] Inum;
	fclose(counter);
	}
	electron->~carrier();
    hole->~carrier();
    delete electron;
    delete hole;
    diode.~device();
    fclose(out);
	postprocess(V, simulationtime, bias_count);
}
