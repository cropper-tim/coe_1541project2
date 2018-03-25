/**************************************************************
	CS/COE 1541				 			
	just compile with gcc -o CPU CPU.c			
	and execute using							

	Will Taylor
	Eric Vitunac
	Tim Cropper
***************************************************************/

//data hazard for mem2 - seg fault
//no-ops in wrong place
//control hazard incorrect ordering

#include <stdio.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include "CPU.h" 
#include "cache.h"

//This is the paramter to change the size of the prediction table
#define prediction_table_size 64 

void trace_viewer(struct trace_item *, int); 

//Prototypes
unsigned int getBitsFrom_To(unsigned, unsigned, unsigned);
int is_hazard(struct trace_item *,struct trace_item *,struct trace_item *);
int is_hazardTwo(struct trace_item *,struct trace_item *,struct trace_item *,struct trace_item *);
int is_structuralHazard(struct trace_item *,struct trace_item *);
unsigned char getDecision(struct trace_item *, int);
unsigned char squash(unsigned char, struct trace_item *, struct trace_item *, int);

// Hash Table
struct prediction{
	unsigned int addr;
	int taken;
	int wrongCount;
};
struct prediction pt[prediction_table_size]; 


int main(int argc, char **argv){
	
	size_t size;
	char *trace_file_name;
	int trace_view_on = 0;
	int prediction_method = 0;
	unsigned char branch_IF1 = 0;
	unsigned char branch_IF2 = 0;
	unsigned char branch_ID = 0;
	unsigned char branch_EX = 0;
	unsigned char t_type = 0;
	unsigned char t_sReg_a= 0;
	unsigned char t_sReg_b= 0;
	unsigned char t_dReg= 0;
	unsigned int t_PC = 0;
	unsigned int t_Addr = 0;
	unsigned int cycle_number = 0;

	if (argc == 1) {
	fprintf(stdout, "\nUSAGE: tv <trace_file> <switch - any character>\n");
	fprintf(stdout, "\n(switch) to turn on or off individual item view.\n\n");
	exit(0);
	}
    
	trace_file_name = argv[1];
	if (argc == 4){
	//
	prediction_method = atoi(argv[2]); //if 0, branches predicted as not taken       
				    //if 1, assume that the architecture uses a one-bit branch predictor which records the last branch condition and address. 
					  //if 2, assume that the architecture uses a 2 bit branch predictor

	trace_view_on = atoi(argv[3]);
	}

	//to hold cache ID variables
	unsigned int I_Size = 0;
	unsigned int I_Associativity = 0;
	unsigned int D_Size = 0;
	unsigned int D_Associativity = 0;
	unsigned int L2_Size = 0;
	unsigned int L2_Associativity = 0;
	unsigned int Cache_Block_size = 0;
	unsigned int L2_Access_Time = 0;
	unsigned int Mem_Access_Time = 0;
	
	// to keep cache statistics
	unsigned int I_accesses = 0;
	unsigned int I_misses = 0;
	unsigned int D_read_accesses = 0;
	unsigned int D_read_misses = 0;
	unsigned int D_write_accesses = 0; 
	unsigned int D_write_misses = 0;
	
	FILE* config_file = fopen ("cache_config.txt", "r");
	if (!config_file) {
		fprintf(stdout, "\n cache_config.txt not found.\n\n");
		exit(0);
	}
	
	while (!feof (config_file))
	{  
	fscanf (file, "%d", &I_Size);      
	fscanf (file, "%d", &I_Associativity);      
	fscanf (file, "%d", &D_Size);      
	fscanf (file, "%d", &D_Associativity);      
	fscanf (file, "%d", &L2_Size);      
	fscanf (file, "%d", &L2_Associativity);      
	fscanf (file, "%d", &Cache_Block_size);      
	fscanf (file, "%d", &L2_Access_Time);      
	fscanf (file, "%d", &Mem_Access_Time);      
	}
	fclose (config_file);
	
	if (I_Size == 0){/*perfect cache, miss penalty = 0;*/}
	if (D_Size == 0){/*perfect cache, miss penalty = 0;*/}
	  
	if(l2_Size != 0){
	  struct cache_t *L2_Cache;
		L2_cache = cache_create(L2_Size, Cache_Block_Size, L2_Associativity, Mem_Access_Time);
	}else{
		L2_Access_Time = Mem_Access_Time;
	}
	struct cache_t *I_cache, *D_cache;
	I_cache = cache_create(I_Size, Cache_Block_Size, I_Associativity, L2_Access_Time);
	D_cache = cache_create(D_Size, Cache_Block_Size, D_Associativity, L2_Access_Time);

  	fprintf(stdout, "\n ** opening file %s\n", trace_file_name);

	trace_fd = fopen(trace_file_name, "rb");

	if (!trace_fd) {
		fprintf(stdout, "\ntrace file %s not opened.\n\n", trace_file_name);
		exit(0);
	}
	
	trace_init();

	//structs for stages	  
	struct trace_item *IF1, *IF2, *ID, *EX, *MEM1, *MEM2, *WB;

	while(1) {
		size = trace_get_item(&IF1);

		if (!size) {/* no more instructions (trace_items) to simulate */
			printf("+ Simulation terminates at cycle : %u\n", cycle_number);
			break;
		}
		else{/* parse the next instruction to simulate */
			cycle_number++;
			t_type = IF1->type;
			t_sReg_a = IF1->sReg_a;
			t_sReg_b = IF1->sReg_b;
			t_dReg = IF1->dReg;
			t_PC = IF1->PC;
			t_Addr = IF1->Addr;

		} 
		
		//read from instruction cache
		cycle_number += cache_access(I_cache, IF1->Addr, 0);

		//data hazard 
		//check for hazard 2)a)
		if (is_hazard(EX,MEM1,ID)) {
			if (trace_view_on) { 
				trace_viewer(WB, cycle_number);      
			}
			WB = MEM2;
			MEM2 = MEM1;
			MEM1 = EX;
			EX->type = 0;
			cycle_number++;
		}

		//This is check for hazard 2)b)
		if (is_hazardTwo(EX,MEM1, MEM2, ID)) {
			if (trace_view_on) { 
				trace_viewer(WB, cycle_number);      
			}
			WB = MEM2;
			MEM2->type = 0;
			cycle_number++;
		}
		


		//This is check for structural hazard
		if (is_structuralHazard(WB,ID)){
			if (trace_view_on && WB != NULL){
				trace_viewer(WB,cycle_number);
			}
			WB=MEM2;
			MEM2=MEM1;
			MEM1=EX;
			EX->type = 0;
			cycle_number++;
		}

		
		//squash
		if (IF1->type == ti_BRANCH) {
			branch_IF1 = getDecision(IF1, prediction_method);
		}

		//move the decision forward in the pipeline
		branch_EX = branch_ID;
		branch_ID = branch_IF2;
		branch_IF2 = branch_IF1;
		
		//check to see if branch decision was correct
		if (EX != NULL && EX->type == ti_BRANCH) {
			if (squash(branch_EX, EX, ID, prediction_method)) {
				if (trace_view_on) {
					printf("[cycle %d] squash\n",cycle_number);
				}
				cycle_number++;
				if (trace_view_on) {
					printf("[cycle %d] squash\n",cycle_number);
				}
				cycle_number++;
				if (trace_view_on) {
					printf("[cycle %d] squash\n",cycle_number);
				}
				cycle_number++;
			} 
		}
		
		//If jump instruction squash previous 3
		if (EX != NULL && (EX->type == ti_JTYPE || EX->type == ti_JRTYPE)) {
			if (trace_view_on) {
				printf("[cycle %d] squash\n",cycle_number);
			}
			cycle_number++;
			if (trace_view_on) {
				printf("[cycle %d] squash\n",cycle_number);
			}
			cycle_number++;
			if (trace_view_on) {
				printf("[cycle %d] squash\n",cycle_number);
			}
			cycle_number++;
		}

		  //print the WB
		  if (trace_view_on && WB != NULL) { 
		   trace_viewer(WB, cycle_number);      
		  }
		  
		  //advance the pipeline
		  WB = MEM2;
		  MEM2 = MEM1;
		  MEM1 = EX;
		  EX = ID;
		  ID = IF2;
		  IF2 = IF1;
	}
	trace_uninit();
	exit(0);
}

//The check for data hazard 2)a)
int is_hazard(struct trace_item *EX,struct trace_item *MEM1,struct trace_item *ID) {
	
  if (EX != NULL && ID != NULL && (EX->type == ti_LOAD) && (ID->type == ti_BRANCH || 
	  ID->type == ti_RTYPE || ID->type == ti_JRTYPE || ID->type == ti_STORE || ID->type == ti_ITYPE)) {
		  if ((EX->dReg == ID->sReg_a) || (EX->dReg == ID->sReg_b)) {
				return 1;
		}
	}

	if (MEM1 != NULL && ID != NULL && (MEM1->type == ti_LOAD) && (ID->type == ti_BRANCH || 
	  ID->type == ti_RTYPE || ID->type == ti_JRTYPE || ID->type == ti_STORE || ID->type == ti_ITYPE)) {

		  if ((MEM1->dReg == ID->sReg_a) || (MEM1->dReg == ID->sReg_b)) {
				return 1;
		}
	}

	if (EX != NULL && MEM1 != NULL && (MEM1->type == ti_LOAD) && (EX->type == ti_BRANCH || 
	EX->type == ti_RTYPE || EX->type == ti_JRTYPE || EX->type == ti_STORE || EX->type == ti_ITYPE)) {
		  if ((MEM1->dReg == EX->sReg_a) || (MEM1->dReg == EX->sReg_b)) {
				return 1;
		}
	}
	return 0;
}




//The check for data hazard 2)b)
int is_hazardTwo(struct trace_item *EX,struct trace_item *MEM1,struct trace_item *MEM2,struct trace_item *ID)
{
	if(MEM1 != NULL){
		if ((MEM1->type == ti_LOAD ) && (ID->type == ti_BRANCH || ID->type == ti_RTYPE || ID->type == ti_JRTYPE || ID->type == ti_STORE || ID->type == ti_ITYPE)){
			if ((MEM1->dReg == ID->sReg_a) || (MEM1->dReg == ID->sReg_b)){
				return 1;
			  }
		 }
		if ((MEM1->type == ti_LOAD) && (EX->type == ti_BRANCH || EX->type == ti_RTYPE || EX->type == ti_JRTYPE || EX->type == ti_STORE || EX->type == ti_ITYPE)){
			if ((MEM1->dReg == EX->sReg_a) || (MEM1->dReg == EX->sReg_b)){
			  return 1;
			}
		}
		if(MEM2 != NULL){
			if ((MEM2->type == ti_LOAD)&& ((ID->type == ti_BRANCH || ID->type == ti_RTYPE || ID->type == ti_JRTYPE || ID->type == ti_STORE || ID->type == ti_ITYPE))){
				if ((MEM2->dReg == ID->sReg_a) || (MEM2->dReg == ID->sReg_b) ){
					return 1;
				}
			}
			if ((MEM2->type == ti_LOAD) && (EX->type == ti_BRANCH || EX->type == ti_RTYPE || EX->type == ti_JRTYPE || EX->type == ti_STORE || EX->type == ti_ITYPE)){
				if ((MEM2->dReg == EX->sReg_a) || (MEM2->dReg == EX->sReg_b)){
				  return 1;
				}
			}
		}
	}
	return 0;
}



//Structural Hazard Test
int is_structuralHazard(struct trace_item *WB,struct trace_item *ID){
	if(WB != NULL){
		if (((WB->type == ti_LOAD || WB->type == ti_ITYPE || WB->type == ti_JRTYPE)) && (ID->type == ti_RTYPE|| ID->type == ti_ITYPE || ID->type == ti_LOAD ||  ID->type == ti_STORE ||  ID->type == ti_BRANCH ||  ID->type == ti_JRTYPE) && ((WB->dReg == ID ->sReg_a) || (WB->dReg == ID->sReg_b))){
			return 1;
		}
		else{
			return 0;
		}
	}
	return 0;
}
    
//get Decision
unsigned char getDecision(struct trace_item *IF1,  int pm) {
    unsigned char result = 0;
    if(pm != 0){
        if (pt[getBits8to3(IF1->Addr) % prediction_table_size].taken != NULL){
           result = pt[getBits8to3(IF1->Addr) % prediction_table_size].taken;
        }
    }
    return result;
}
    
//return bits From to To of an address
unsigned int getBitsFrom_To(unsigned addr, unsigned from, unsigned to){
	if (to > from){
		printf("\nUnable to retreive bits %d to %d, bits must be entered in deacreasing value.\n", from, to);
		exit(0);
	}
	unsigned int r = 0;
	unsigned int i;
	for (i = to; i <= from; i++){
		r |= 1 << i;
	}
	return (addr & r)>>to;
}
    
//squash branch instruction
//return 1 if the branch prediction was wrong
unsigned char squash(unsigned char d, struct trace_item *e, struct trace_item *id, int pm) {
	int table_location = getBits8to3(e->Addr) % prediction_table_size;
	int decision;
	if ((d == 0 && (e->PC != ((id->PC) + 4))) || (d == 1 && e->PC == ((id->PC) + 4))){
	    if (d == 0){
	        decision = 1;
	    }else{
	        decision = 0;
	    }
	    if(pm == 1){
	        pt[table_location].taken = decision;
	    }else if(pm == 2){
	        pt[table_location].wrongCount++;
	        if(pt[table_location].wrongCount == 2){
	            pt[table_location].wrongCount = 0;
	            pt[table_location].taken = decision;
	        }
	    }
		return 1;
	  }
	return 0;
}



    
    //Prints cycle number and data when trace view is on
void trace_viewer(struct trace_item *a, int cycle_number){
            switch(a->type) {
              case ti_NOP:
                printf("[cycle %d] NOP\n",cycle_number) ;
                break;
              case ti_RTYPE:
                printf("[cycle %d] RTYPE:",cycle_number) ;
                printf(" (PC: %x)(sReg_a: %d)(sReg_b: %d)(dReg: %d) \n", a->PC,a->sReg_a,a->sReg_b,a->dReg);
                break;
              case ti_ITYPE:
                printf("[cycle %d] ITYPE:",cycle_number) ;
                printf(" (PC: %x)(sReg_a: %d)(dReg: %d)(addr: %x)\n", a->PC, a->sReg_a, a->dReg, a->Addr);
                break;
              case ti_LOAD:
                printf("[cycle %d] LOAD:",cycle_number) ;      
                printf(" (PC: %x)(sReg_a: %d)(dReg: %d)(addr: %x)\n", a->PC, a->sReg_a, a->dReg, a->Addr);
                break;
              case ti_STORE:
                printf("[cycle %d] STORE:",cycle_number) ;      
                printf(" (PC: %x)(sReg_a: %d)(sReg_b: %d)(addr: %x)\n", a->PC, a->sReg_a, a->sReg_b, a->Addr);
                break; 
              case ti_BRANCH:
                printf("[cycle %d] BRANCH:",cycle_number) ;
                printf(" (PC: %x)(sReg_a: %d)(sReg_b: %d)(addr: %x)\n", a->PC, a->sReg_a, a->sReg_b, a->Addr);
                break;
              case ti_JTYPE:
                printf("[cycle %d] JTYPE:",cycle_number) ;
                printf(" (PC: %x)(addr: %x)\n", a->PC,a->Addr);
                break;
              case ti_SPECIAL:
                printf("[cycle %d] SPECIAL:\n",cycle_number) ;      	
                break;
              case ti_JRTYPE:
                printf("[cycle %d] JRTYPE:",cycle_number) ;
                printf(" (PC: %x) (sReg_a: %d) (addr: %x)\n", a->PC, a->dReg, a->Addr);
                break;        
    			}    
    
    }
	
	
