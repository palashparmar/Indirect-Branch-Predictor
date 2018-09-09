// my_predictor.h
// This file contains a sample my_predictor class.
// It has a simple 32,768-entry gshare with a history length of 15 and a
// ITTAGE Predictor for indirect branch prediction.

//Header files
#include <bitset>
#include <iostream>
#include <stdlib.h>

using namespace std;


//some constants, set values after performing several simulations
#define TABLE_NO 7
#define CTR_MAX 3
#define CTR_MIN 0
#define CTR_INIT 0
#define USE_ALT_MAX 16
#define USE_ALT_MIN 0
#define HISTORYLEN 512


//parameters for various tables in predictor, choose these values after several simulations 
int geolength[TABLE_NO + 1] = { 0, 8, 16, 32, 64, 128, 256, 512};
int indexlength[TABLE_NO + 1] = { 12, 12, 12, 12, 12, 12, 12, 12};
int taglength[TABLE_NO + 1] = { 0, 12, 13, 14, 15, 16, 17, 18 };

class my_update : public branch_update {
public:
	unsigned int index;
	/*start from here*/
};

class my_predictor : public branch_predictor {
public:
#define HISTORY_LENGTH	15
#define TABLE_BITS	15
	my_update u;
	branch_info bi;
	unsigned int history;
	unsigned char tab[1<<TABLE_BITS];
	unsigned int targets[1<<TABLE_BITS];
	
	
	//Class of indirect Predictor starts from here
	bitset<HISTORYLEN> ghist;
	struct table								//structure of single entry in predictor tables
	{
		unsigned char ctr;
		unsigned char u;
		unsigned int target;
		unsigned int tag;
	};
	
	
	table *taggedTable[TABLE_NO + 1];			//constructor of tables entries
	unsigned int gindex[TABLE_NO + 1];			//index of table in predictor
	unsigned int gtag[TABLE_NO + 1];			//tag of table in predictor						
	unsigned int altPred;						//alternate prediction
	unsigned int finalPred;						//final Prediction given by predictor
	unsigned int mainPred;						//prediction given by longest history table
	int mainTable;								//table number of longest history prediction
	int altTable;								//table number of alternate longest history prediction
	int useAlt;									//variable for judging whether alternate predict to be used or not
	
	//function for history folding, takes history length of table, folding bit, and global history and return folded history
	int tagIndexCal(int geoLen, int targetLen, bitset<HISTORYLEN> ghistory)
	{
		int rem = geoLen % targetLen;
		unsigned int CSR = 0;
		bitset<HISTORYLEN> temp;
		int numFolds = geoLen / targetLen;
		bitset<HISTORYLEN> mask = (1 << targetLen) - 1;
		bitset<HISTORYLEN> mask1 = (1 << rem) - 1;
		for (int i = 0; i < numFolds; i++)
		{
			temp = temp ^ (mask & ghistory);
			ghistory = ghistory >> targetLen;
		}
		temp = temp ^ (ghistory & mask1);
		CSR = temp.to_ulong();
		return CSR;
	}

	//predictor class constructor
	my_predictor (void) : history(0) { 
		memset (tab, 0, sizeof (tab));
		memset (targets, 0, sizeof (targets));
		// indirect predictor constructor
		for (int i = 0; i < TABLE_NO + 1; i++)
		{
			taggedTable[i] = new table[1 << indexlength[i]];		//constructing entries in predictor table according to index bits
			for (int j = 0; j < (1 << indexlength[i]); j++)			//initializing table entries
			{
				taggedTable[i][j].tag = 0;
				taggedTable[i][j].ctr = 0;
				taggedTable[i][j].u = 0;
				
			}
		}
		useAlt = 9;				//initializing alternate usage counter in the middle
		ghist.reset();			//reseting global history bits initially
		
		
	}
	
	// predict function
	branch_update *predict (branch_info & b) {
		bi = b;
		
		if (b.br_flags & BR_CONDITIONAL) {
			u.index = 
				  (history << (TABLE_BITS - HISTORY_LENGTH)) 
				^ (b.address & ((1<<TABLE_BITS)-1));
			u.direction_prediction (tab[u.index] >> 1);
		} else {
			u.direction_prediction (true);
		}
		// indirect predict function starts here
		if (b.br_flags & BR_INDIRECT) 
		{
			gindex[0] = b.address & ((1 << indexlength[0]) - 1);		//index calculation for tagless table (=0), 
			gtag[0] = 0;												//making tag of tagless tables 0, helps in further identifying as tagless tables
			
			//calculation of tags and indexes for all tagged table using history folding function and present PC
			for (int i = 1; i <= TABLE_NO; i++)		
			{
				
				gindex[i] = ((b.address ^ (b.address >> indexlength[i])) & ((1 << indexlength[i]) - 1)) ^ tagIndexCal(geolength[i], indexlength[i],ghist);
				gtag[i] = (b.address & ((1 << indexlength[i]) - 1)) ^ (tagIndexCal(geolength[i], taglength[i],ghist));
				
			}
			//some initializations
			altPred = 0;
			mainTable = -1;
			altTable = -1;
			
			//calculating main prediction table number; if nothing matches, return entry from tagless tables by default
			//always provides atleast one match
			for (int i = TABLE_NO; i >= 0; i--)
			{
				if (taggedTable[i][gindex[i]].tag == gtag[i])
				{
					mainTable = i;
					break;
				}
			}
			
			//calculation of table number from second longest prediction
			for (int i = mainTable - 1; i >= 0; i--)
			{
				if (taggedTable[i][gindex[i]].tag == gtag[i])
				{
					altTable = i;
					break;
				}
			}
			
			//here final prediction is given by longest history match as of now
			mainPred = taggedTable[mainTable][gindex[mainTable]].target;
			
			finalPred = mainPred;
			
			//if alternate table number is present; calculate alternate prediciton else alternate prediction would be 0
			if (altTable >= 0)
			{
				altPred = taggedTable[altTable][gindex[altTable]].target;
				//check if the main prediction entry is now, the use alternate prediciton as final prediciton
				if ((useAlt >= 9) & (taggedTable[mainTable][gindex[mainTable]].ctr == 0))
					finalPred = altPred;
			}
			
			
			
			
			//return the prediction
			u.target_prediction(finalPred);
		}
		
		return &u;
	}
	
	//funtion for updating predictor
	void update (branch_update *u, bool taken, unsigned int target) {
		
		if (bi.br_flags & BR_CONDITIONAL) {
		
			unsigned char *c = &tab[((my_update*)u)->index];
			if (taken) {
				if (*c < 3) (*c)++;
			} else {
				if (*c > 0) (*c)--;
			}
			history <<= 1;
			history |= taken;
			history &= (1<<HISTORY_LENGTH)-1;
			
			ghist=ghist<<1;
			ghist.set(0,taken);
		}
		
		//updating indirect branch predictor starts here
		if (bi.br_flags & BR_INDIRECT) 
		{
			
			

			//provide new entry with longer history table if longest history prediciton is wrong
			// provide atmost 3 entry in alternate tables
			
			if (mainPred != target)
			{
				int newTable = mainTable + 1;

				int noOfEntry = 2;
				for (int i = newTable; i <= TABLE_NO; i++)
				{
					if (taggedTable[i][gindex[i]].u == 0)
					{
						taggedTable[i][gindex[i]].tag = gtag[i];
						taggedTable[i][gindex[i]].target = target;
						taggedTable[i][gindex[i]].u = 0;
						taggedTable[i][gindex[i]].ctr = CTR_INIT;

						
						if (noOfEntry == 0)
							break;
						noOfEntry--;
						i++;
					}
					
				}
				
				//if no table found suitable for entry, just replace the new entry in next longest history table
				
				if (noOfEntry == 2)
				{
					newTable = (mainTable>TABLE_NO-1)? TABLE_NO : mainTable + 1;
					taggedTable[newTable][gindex[newTable]].ctr = CTR_INIT;
					taggedTable[newTable][gindex[newTable]].u = 0;
					taggedTable[newTable][gindex[newTable]].tag = gtag[newTable];
					taggedTable[newTable][gindex[newTable]].target = target;
				}

			}
			
			//if longest history prediciton is correct, increase the counter else decrease
			//if counter becomes zero, replace the entry with new entry
			if (mainPred == target)
			{
				if (taggedTable[mainTable][indexlength[mainTable]].ctr < CTR_MAX)
					taggedTable[mainTable][gindex[mainTable]].ctr++;
			}
			else
			{
				if (taggedTable[mainTable][gindex[mainTable]].ctr > CTR_MIN)
					taggedTable[mainTable][gindex[mainTable]].ctr--;
				else
				{
					taggedTable[mainTable][gindex[mainTable]].target = target;
				}
			}
			
			//update the alternate use counder if alternate prediciton predict correctly
			if ((mainTable > 0) & (altTable >= 0))
			{
				if (taggedTable[mainTable][indexlength[mainTable]].ctr == 0)
				{
					
					if (mainPred != altPred)
					{
						if ((altPred == target) || (mainPred == target))
						{
							if (altPred == target)
							{
								if (useAlt < USE_ALT_MAX)
									useAlt++;
							}
							else
							{
								if (useAlt >= USE_ALT_MIN)
									useAlt--;
							}
						}
					}
					
				}
			}
			
			// if main prediciton and alternate prediciton is different, and main prediciton provides correct prediction, update the useful bit of entry to 1
			if (mainTable != 0)
			{
				if (mainPred != altPred)
				{
					if (mainPred == target)
					{
						taggedTable[mainTable][indexlength[mainTable]].u = 1;
					}
				}
			}
			
			//updating the global history
			//10 bits mixed target and address history
			
			//bitset<HISTORYLEN> tempVal = (target ^ (target >> 5) ^ bi.address) & ((1 << 10) - 1);
			//bitset<HISTORYLEN> tempVal = (target ^ (target >> 1) ^ bi.address) & ((1 << 10) - 1);
			//bitset<HISTORYLEN> tempVal = (target ^ bi.address) & ((1 << 10) - 1);
			
			bitset<HISTORYLEN> tempVal = (target ^ (target >> 3) ^ bi.address) & ((1 << 10) - 1);
			ghist = ghist << 10;
			ghist = ghist | tempVal;
	

			
			
			
			//targets[bi.address & ((1<<TABLE_BITS)-1)] = target;
		}
	}
};
