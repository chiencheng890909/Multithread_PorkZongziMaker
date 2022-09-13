#include <iostream>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h> 
#include <stdlib.h>
#include <string.h>
#include <queue>
using namespace std;

struct Pork
{
	int Num = 0; //number of meat
	int next_Time;
};
int M, N, C, P, F, T, Error, Total_Cut = 0,Total_Pack = 0, finished_Factory, total_Factory, finished_Meat, total_Meat;
int Time = 0;
bool *Cut_Working;
bool *Pack_Working;
sem_t Spot;
sem_t New;
sem_t Cutted;
sem_t *Is_Cut;
sem_t *Is_Pack;
sem_t *Try_in;

pthread_mutex_t Is_New_queue;
pthread_mutex_t Is_Cutted_queue;
pthread_mutex_t Is_finished_Factory;
pthread_mutex_t Is_total_Factory;
pthread_mutex_t Is_finished_Meat;
pthread_mutex_t Is_total_Meat;
pthread_mutex_t Is_waiting_queue;
pthread_mutex_t Is_cout;

queue <Pork> New_Slot;
queue <Pork> Cutted_Slot;
queue <Pork> waiting;

void *CLK(void *arg) {
	while(total_Factory) {
		while(finished_Factory < total_Factory);
		while(finished_Meat < total_Meat);
		usleep(10000); //us * 1000
		Time += 10;
		finished_Factory = 0;
		finished_Meat = 0;
		for(int i = 0; i < M; i++)
			sem_post(&(Try_in[i]));
		for(int i = 0; i < C; i++)
			sem_post(&(Is_Cut[i]));
		for(int i = 0; i < P; i++)
			sem_post(&(Is_Pack[i]));
	}
	pthread_exit(NULL);
}

bool Over_Time(Pork Target) {
	if((Time - Target.next_Time) > 3000) { // if the meat in slot is over 3000ms, it will spoil
		pthread_mutex_lock(&Is_cout); // protect cout
		cout << Time << "ms -- Pork#" << Target.Num << " in the slots is spoilt(over 3000ms)" << endl;
		pthread_mutex_unlock(&Is_cout); // protect cout
		return true;
	}
	else
		return false;
}

void *Pack(void *arg) 
{	
	int  millisecond = 0, last_Time = 0, Pack_Num = *((int *) arg);
	Pork Meat, check;
	while(Total_Pack < M) {
		sem_wait(&Is_Pack[Pack_Num - 1]);
		if(Meat.Num != 0 && Time >= Meat.next_Time) {
			pthread_mutex_lock(&Is_cout); // protect cout
			cout << Time << "ms -- Pork#" << Meat.Num << ": leaves PACKER#"<< Pack_Num <<" (Complete)" << endl;
			pthread_mutex_unlock(&Is_cout); // protect cout
			Total_Pack++;
			Meat.Num = 0;
		}
		if(Meat.Num == 0) {
			if(sem_trywait(&Cutted) == 0) {
				if(!Pack_Working[Pack_Num - 1]) {//wake up the cutter
					sem_post(&Cutted); // not count
					Pack_Working[Pack_Num - 1] = true;
				}
				else {
					pthread_mutex_lock(&Is_Cutted_queue); // protect Cutted_Slot
					Meat = Cutted_Slot.front();	//take from slots
					Cutted_Slot.pop();
					sem_post(&Spot);
					pthread_mutex_unlock(&Is_Cutted_queue);
					pthread_mutex_lock(&Is_cout); // protect cout
					cout << Time<< "ms -- Pork#" << Meat.Num << ": enters to the factory (PACKER#" << Pack_Num << ")" << endl;
					pthread_mutex_unlock(&Is_cout); // protect cout
					millisecond = ((rand() % 51) + 50) * 10;
					Meat.next_Time = Time + millisecond; //update time
					
					cout << Time << "ms -- PACKER#"<< Pack_Num <<": processing & Packing the Pork#" << Meat.Num << " -- " << millisecond << "ms"<< endl;
				}
			}
		}
		if(Meat.Num == 0) { //no works
			if(Time >= last_Time) {
				for(int i = 0; i < C; i++) {
					if(!Cut_Working[i]) {
						pthread_mutex_lock(&Is_cout); // protect cout
						cout << Time <<"ms – PACKER#"<< Pack_Num <<": under reviewing together..." << endl;
						pthread_mutex_unlock(&Is_cout); // protect cout
						break;
					}
					if(i == C - 1) {
						pthread_mutex_lock(&Is_cout); // protect cout
						cout << Time <<"ms -- PACKER#"<< Pack_Num <<": under maintenance." << endl;
						pthread_mutex_unlock(&Is_cout); // protect cout
					}
				}
				Pack_Working[Pack_Num - 1] = false;
				last_Time = Time + ((rand() % 10) + 1) * 10;
			}
			int Rest = Total_Pack;
			for(int i = 0; i < P; i++)
				if(Pack_Working[i] && i != Pack_Num - 1) // find the rest one
					Rest ++;
			if(Rest == M)
				break;
		}
		if(T) {
			pthread_mutex_lock(&Is_Cutted_queue); // protect Cutted_Slot
			int size = Cutted_Slot.size();
			if(size > 0) {
			    for (int i = 0; i < size; i++) {
				check = Cutted_Slot.front();
				Cutted_Slot.pop();
				if(!Over_Time(check))
					Cutted_Slot.push(check);
				else {
					sem_wait(&Cutted);
					sem_post(&Spot);
					Total_Pack++;
				}
			    }
			}
			pthread_mutex_unlock(&Is_Cutted_queue);
		}
		pthread_mutex_lock(&Is_finished_Factory); // protect finished_Factory
		finished_Factory ++;
		pthread_mutex_unlock(&Is_finished_Factory); // protect finished_Factory
	}
	pthread_mutex_lock(&Is_total_Factory); // protect finished_Factory
	total_Factory --;
	pthread_mutex_unlock(&Is_total_Factory); // protect finished_Factory
	Pack_Working[Pack_Num - 1] = false;
	pthread_exit(NULL);
}

void *Cut(void *arg) 
{
	int  millisecond = 0, last_Time = 0, Cut_Num = *((int *) arg);
	Pork Meat, last_Meat, check, BONUSII;
	while(Total_Cut < M) {
		sem_wait(&Is_Cut[Cut_Num - 1]);
		if(last_Meat.Num != 0) { //put it to slot latter
			if(Cutted_Slot.size() + New_Slot.size() < N * C) {
				pthread_mutex_lock(&Is_Cutted_queue); // protect Cutted_Slot
				Cutted_Slot.push(last_Meat); //Put in the Cutted Slot
				sem_post(&Cutted);
				pthread_mutex_unlock(&Is_Cutted_queue);
				pthread_mutex_lock(&Is_cout); // protect cout
				cout << Time << "ms -- Pork#" << last_Meat.Num << ": waiting in the slot (cutted)" << endl;				
				pthread_mutex_unlock(&Is_cout); // protect cout
				last_Meat.Num = 0;
				Total_Cut++; //finish one
			}
		}
		if(Meat.Num != 0 && Time >= Meat.next_Time) {
			pthread_mutex_lock(&Is_cout); // protect cout
			cout << Time << "ms -- Pork#" << Meat.Num << ": leaves CUTTER#"<< Cut_Num <<" (complete 1st stage)" << endl;
			pthread_mutex_unlock(&Is_cout); // protect cout
			last_Meat = Meat;
			Meat.Num = 0;
		}
		if(Meat.Num == 0) {
			if(sem_trywait(&New) == 0) {
				if(!Cut_Working[Cut_Num - 1]) {//wake up the cutter
					sem_post(&New); // not count
					Cut_Working[Cut_Num - 1] = true;
				}
				else {
					pthread_mutex_lock(&Is_New_queue); // protect New_Slot
					Meat = New_Slot.front();	//take from slots
					New_Slot.pop();
					pthread_mutex_unlock(&Is_New_queue); // protect New_Slot
					pthread_mutex_lock(&Is_cout); // protect cout
					cout << Time << "ms -- Pork#" << Meat.Num << ": enters the CUTTER#"<< Cut_Num << endl;					
					pthread_mutex_unlock(&Is_cout); // protect cout
					millisecond = ((rand() % 21) + 10) * 10;
					Meat.next_Time = Time + millisecond;//update time
					pthread_mutex_lock(&Is_cout); // protect cout
					cout << Time << "ms -- CUTTER#"<< Cut_Num <<": cutting... cutting... Pork#" << Meat.Num << " -- " << millisecond << "ms"<< endl;
					pthread_mutex_unlock(&Is_cout); // protect cout
				}
			}
		}
		if(Meat.Num == 0 && last_Meat.Num == 0) { //no works
			if(Time >= last_Time) {
				for(int i = 0; i < P; i++) {
					if(!Pack_Working[i]) {
						pthread_mutex_lock(&Is_cout); // protect cout
						cout << Time <<"ms – CUTTER#"<< Cut_Num <<": under reviewing together..." << endl;
						pthread_mutex_unlock(&Is_cout); // protect cout
						break;
					}
					if(i == P - 1) {
						pthread_mutex_lock(&Is_cout); // protect cout
						cout << Time <<"ms -- CUTTER#"<< Cut_Num <<": under maintenance." << endl;
						pthread_mutex_unlock(&Is_cout); // protect cout
					}
				}
				Cut_Working[Cut_Num - 1] = false;
				last_Time = Time + ((rand() % 10) + 1) * 10;
			}
			int Rest = Total_Cut;
			for(int i = 0; i < P; i++)
				if(Cut_Working[i] && i != Cut_Num - 1) // find the rest one
					Rest ++;
			if(Rest == M)
				break;
		}
		if(waiting.size() > 0) {
			if(sem_trywait(&Spot) == 0) {
				pthread_mutex_lock(&Is_waiting_queue);  // protect waiting_queue
				BONUSII = waiting.front();
				waiting.pop();
				pthread_mutex_unlock(&Is_waiting_queue);  // protect waiting_queue
				
				pthread_mutex_lock(&Is_New_queue); // protect New_Slot
				New_Slot.push(BONUSII);// uncutted meat
				sem_post(&New); 
				pthread_mutex_unlock(&Is_New_queue);
				pthread_mutex_lock(&Is_cout); // protect cout
				cout << Time << "ms -- Pork#" << BONUSII.Num << ": waiting in the slot(BONUSII)" << endl;
				pthread_mutex_unlock(&Is_cout); // protect cout
			}
		}
		if(T) {
			pthread_mutex_lock(&Is_New_queue); // protect New_Slot
			int size = New_Slot.size();
			if(size > 0) {
			    for (int i = 0; i < size; i++) {
				check = New_Slot.front();
				New_Slot.pop();
				if(!Over_Time(check))
					New_Slot.push(check);
				else {
					sem_wait(&New); 
					sem_post(&Spot);
					Total_Cut++;
				}
			    }
			}
			pthread_mutex_unlock(&Is_New_queue);
		}
		pthread_mutex_lock(&Is_finished_Factory); // protect finished_Factory
		finished_Factory ++;
		pthread_mutex_unlock(&Is_finished_Factory); // protect finished_Factory
	}
	pthread_mutex_lock(&Is_total_Factory); // protect total_Factory
	total_Factory --;
	pthread_mutex_unlock(&Is_total_Factory); // protect total_Factory
	Cut_Working[Cut_Num - 1] = false;
	pthread_exit(NULL);
}

void *Add_to_Slot(void *arg) 
{	
	int  millisecond, First_in = 0;
	bool Finished = false;
	Pork Meat;
	Meat.Num = ((Pork *) arg)->Num;
	Meat.next_Time = ((Pork *) arg)->next_Time;
	while(!Finished) {
		sem_wait(&(Try_in[Meat.Num - 1]));
		if(Time >= Meat.next_Time) {
			if(sem_trywait(&Spot) == 0) {
				pthread_mutex_lock(&Is_New_queue); // protect New_Slot
				New_Slot.push(Meat);// uncutted meat
				sem_post(&New); 
				pthread_mutex_unlock(&Is_New_queue);
				pthread_mutex_lock(&Is_cout); // protect cout
				cout << Time << "ms -- Pork#" << Meat.Num << ": waiting in the slot" << endl;
				pthread_mutex_unlock(&Is_cout); // protect cout
				Finished = true;
			}
			else if((First_in != 0 && (Time - First_in) > 1490) && F == 1) { // BONUS II
				pthread_mutex_lock(&Is_waiting_queue);  // protect New_Slot
				Meat.next_Time = Time; // update the enter time
				waiting.push(Meat);
				pthread_mutex_unlock(&Is_waiting_queue); // protect New_Slot
				pthread_mutex_lock(&Is_cout); // protect cout
				cout << Time << "ms -- Pork#" << Meat.Num << ": exits freezer and waits beside the slot" << endl;
				pthread_mutex_unlock(&Is_cout); // protect cout
				Finished = true;
			}
			else {
				if(First_in == 0)
					First_in = Meat.next_Time;
				millisecond = ((rand() % 21) + 30) * 10;
				Meat.next_Time = Time + millisecond;
				pthread_mutex_lock(&Is_cout); // protect cout
				cout << Time << "ms -- Pork#" << Meat.Num << " has been sent to the Freezer - " << millisecond << "ms"<< endl;
				pthread_mutex_unlock(&Is_cout); // protect cout
			}
		}
		pthread_mutex_lock(&Is_finished_Meat); // protect finished_Meat
		finished_Meat ++;
		pthread_mutex_unlock(&Is_finished_Meat); // protect finished_Meat
		
	}
	pthread_mutex_lock(&Is_total_Meat); // protect total_Meat
	total_Meat --;
	pthread_mutex_unlock(&Is_total_Meat); // protect total_Meat
	pthread_exit(NULL);
}

int main(int argc, char* argv[]) 
{
	int millisecond;
	if(argc != 7){
		cout << "input number is not right (it need 7 parameters)" << endl;
		return 0;
	}
		
	for(int i = 0; i < strlen(argv[1]); i++)
		if(argv[1][i] > '9' || argv[1][i] < '0') {
			cout << "Meat number is not right" << endl;
			return 0;
		}
	for(int i = 0; i < strlen(argv[2]); i++)
		if(argv[2][i] > '9' || argv[2][i] < '0') {
			cout << "Slot number is not right" << endl;
			return 0;
		}
	for(int i = 0; i < strlen(argv[3]); i++)
		if(argv[3][i] > '9' || argv[3][i] < '0') {
			cout << "CUTTER number is not right" << endl;
			return 0;
		}
	for(int i = 0; i < strlen(argv[4]); i++)
		if(argv[4][i] > '9' || argv[4][i] < '0') {
			cout << "PACKER number is not right" << endl;
			return 0;
		}
	if(strlen(argv[5]) == 1) {
		if(argv[5][0] != '1' && argv[5][0] != '0') {
			cout << "BONUSII number is not right (only 1 or 0)" << endl;
			return 0;
		}
	}
	else {
		cout << "BONUSII number is not right (only 1 or 0)" << endl;
		return 0;
	}
	if(strlen(argv[6]) == 1) {
		if(argv[6][0] != '1' && argv[6][0] != '0') {
			cout << "BONUSIII number is not right (only 1 or 0)" << endl;
			return 0;
		}
	}
	else {
		cout << "BONUSIII number is not right (only 1 or 0)" << endl;
		return 0;
	}
	M = atoi(argv[1]);
	N = atoi(argv[2]);
	C = atoi(argv[3]);
	P = atoi(argv[4]);
	F = atoi(argv[5]);
	T = atoi(argv[6]);
	if(M == 0) {
		cout << "There is not any meat(must >= 1)" << endl;
		return 0;
	}
	if(N == 0) {
		cout << "There is not any slot(must >= 1)" << endl;
		return 0;
	}
	if(C == 0) {
		cout << "There is not any CUTTER(must >= 1)" << endl;
		return 0;
	}
	if(P == 0) {
		cout << "There is not any PACKER(must >= 1)" << endl;
		return 0;
	}
	srand(N);
	finished_Factory = C + P;
	total_Factory  = C + P;
	finished_Meat = M;
	total_Meat  = M;
	
	pthread_t Meat[M];
	pthread_t Cutter[C];
	pthread_t Packer[P];
	pthread_t Clk;
	Pork In[M];
	int C_Num[C]; 
	int P_Num[P]; 
	Cut_Working = new bool[C];
	for(int i = 0; i < C; i++)
		Cut_Working[i] = true;
	Pack_Working = new bool[P];
	for(int i = 0; i < P; i++)
		Pack_Working[i] = true;
	sem_init(&Spot,0,(N + 1) * C);	//N + 1 the meat in process
	sem_init(&Cutted,0,0);
	sem_init(&New,0,0);
	Is_Cut = new sem_t [C];
	for(int i = 0; i < C; i++)
		sem_init(&(Is_Cut[i]),0,0);
	
	Is_Pack = new sem_t [P];	
	for(int i = 0; i < P; i++)
		sem_init(&(Is_Pack[i]),0,0);
	
	Try_in = new sem_t [M];
	for(int i = 0; i < M; i++)
		sem_init(&(Try_in[i]),0,0);
	pthread_mutex_init(&Is_New_queue, 0);
	pthread_mutex_init(&Is_Cutted_queue, 0);
	pthread_mutex_init(&Is_finished_Factory, 0);
	pthread_mutex_init(&Is_total_Factory, 0);
	pthread_mutex_init(&Is_finished_Meat, 0);
	pthread_mutex_init(&Is_total_Meat, 0);
	pthread_mutex_init(&Is_waiting_queue, 0);
	pthread_mutex_init(&Is_cout, 0);
	//initialize the time
	In[0].Num = 1;
	In[0].next_Time = ((rand() % 6) + 5) * 10;
	for(int i = 1; i < M; i++ ) {
		millisecond = ((rand() % 6) + 5) * 10;
		In[i].Num = i + 1;
		In[i].next_Time = In[i - 1].next_Time + millisecond;

	}
	for(int i = 0; i < C; i++ )
		C_Num[i] = i + 1;
	for(int i = 0; i < P; i++ )
		P_Num[i] = i + 1;
	for(int i = 0; i < M; i++ ) {
		Error = pthread_create(&(Meat[i]), NULL,Add_to_Slot,&(In[i]));
		if(Error != 0) {
			cout << "Couldn't Create Meat Pthread" << endl;
			return 0;
		}
		//cout << In[i].Num << " " << In[i].next_Time << endl;
	}
	Error = pthread_create(&(Clk), NULL, CLK, NULL);
	if(Error != 0) {
		cout << "Couldn't Create Clk Pthread" << endl;
		return 0;
	}
	for(int i = 0; i < C; i++ ) {
		Error = pthread_create(&(Cutter[i]), NULL, Cut, &(C_Num[i]));
		if(Error != 0) {
			cout << "Couldn't Create Cutter Pthread" << endl;
			return 0;
		}
	}
	for(int i = 0; i < P; i++ ) {
		Error = pthread_create(&(Packer[i]), NULL, Pack, &(P_Num[i]));
		if(Error != 0) {
			cout << "Couldn't Create Packer Pthread" << endl;
			return 0;
		}
	}
	for(size_t i = 0; i < M; i++ )
		pthread_join(Meat[i], NULL);
	for(size_t i = 0; i < C; i++ )
		pthread_join(Cutter[i], NULL);
	for(size_t i = 0; i < P; i++ )
		pthread_join(Packer[i], NULL);
	pthread_join(Clk, NULL);
	
	         /*Debug*/
	int size = New_Slot.size();
	if(size > 0) {
	    for (int i = 0; i < size; i++) {
		cout << New_Slot.front().Num << " ";
		New_Slot.pop();
	    }
	    cout << "\n";
	}
	size = Cutted_Slot.size();
	if(size > 0) {
	    for (int i = 0; i < size; i++) {
		cout << Cutted_Slot.front().Num << " ";
		Cutted_Slot.pop();
	    }
	    cout << "\n";
	}
	size = waiting.size();
	if(size > 0) {
	    for (int i = 0; i < size; i++) {
		cout << waiting.front().Num << " ";
		waiting.pop();
	    }
	    cout << "\n";
	}
	return 0;
}
