#include <iostream>
#include "FPGA_LRMF.h"
#include "Server.h"

using namespace std;

#define FPGA

#define NUM_JOB_TYPES 3

struct JobProperties {
	char* m_pathToDataset;
	uint32_t m_Mdim;
	uint32_t m_Udim;
	float m_sparsenessFactor;
	uint32_t m_numFeatures;
	uint32_t m_numEpochs;
	uint32_t m_priority;
};

int main(int argc, char* argv[]) {

	JobProperties jobs[NUM_JOB_TYPES];
	float stepSize = 0.01;
	float lambda = 0;
	uint32_t tileSize = 256;

	char* pathToDataset;
	uint32_t numJobsMultiplier = 0;
	char config[4] = {'-', '-', '-', '-'};
	bool enableContextSwitch = false;
	bool enableThreadMigration = false;
	bool enablePriority = false;
	bool randomizeJobOrder = false;
	uint32_t sw0hw1 = 0;
	if (!(argc == 4)) {
		cout << "Usage: ./app <pathToDataset> <numJobsMultiplier> <'e'nableContextSwitch 'e'nableThreadMigration 'e'nablePriority 'r'andomizeJobOrder>" << endl;
		return 0;
	}
	pathToDataset = argv[1];
	numJobsMultiplier = atoi(argv[2]);
	for (uint32_t i = 0; i < 4; i++) {
		if (argv[3][i] == '\0') {
			break;
		}
		else {
			config[i] = argv[3][i];
		}
	}
	enableContextSwitch = config[0] == 'e';
	enableThreadMigration = config[1] == 'e';
	enablePriority = config[2] == 'e';
	randomizeJobOrder = config[3] == 'r';

	for (uint32_t i = 0; i < NUM_JOB_TYPES; i++) {
		switch(i) {
			case 0: // NETFLIX
				jobs[0].m_pathToDataset = pathToDataset;
				jobs[0].m_Mdim = -1;
				jobs[0].m_Udim = -1;
				jobs[0].m_numFeatures = 32;
				jobs[0].m_sparsenessFactor = 0;
				jobs[0].m_numEpochs = 10;
				jobs[0].m_priority = 3;
				break;
			case 1: // SYN_LRMF1
				jobs[1].m_pathToDataset = (char*)"syn";
				jobs[1].m_Mdim = 4096;
				jobs[1].m_Udim = 524288;
				jobs[1].m_numFeatures = 32;
				jobs[1].m_sparsenessFactor = 0.05;
				jobs[1].m_numEpochs = 10;
				jobs[1].m_priority = 2;
				break;
			case 2: // SYN_LRMF2
				jobs[2].m_pathToDataset = (char*)"syn";
				jobs[2].m_Mdim = 4096;
				jobs[2].m_Udim = 524288;
				jobs[2].m_numFeatures = 32;
				jobs[2].m_sparsenessFactor = 0.1;
				jobs[2].m_numEpochs = 10;
				jobs[2].m_priority = 1;
				break;
		}
	}

	if (enableContextSwitch) {
		cout << "-> enableContextSwitch" << endl;
	}
	if (enableThreadMigration) {
		cout << "-> enableThreadMigration" << endl;
	}
	if (enablePriority) {
		cout << "-> enablePriority" << endl;
	}
	if (randomizeJobOrder) {
		cout << "-> randomizeJobOrder" << endl;
	}

	ServerWrapper server(enableContextSwitch, enableThreadMigration, enablePriority);

	vector<FPGA_LRMF*> lrmf(numJobsMultiplier*NUM_JOB_TYPES);
	for (uint32_t i = 0; i < numJobsMultiplier*NUM_JOB_TYPES; i++) {
		lrmf[i] = new FPGA_LRMF(server.GetServer(), jobs[i%NUM_JOB_TYPES].m_numFeatures, enableContextSwitch);

		if ( strcmp(jobs[i%NUM_JOB_TYPES].m_pathToDataset, "syn") == 0) {
			lrmf[i]->GenerateSyntheticData(jobs[i%NUM_JOB_TYPES].m_Mdim, jobs[i%NUM_JOB_TYPES].m_Udim, jobs[i%NUM_JOB_TYPES].m_sparsenessFactor);
		}
		else {
			lrmf[i]->ReadNetflixData(jobs[i%NUM_JOB_TYPES].m_pathToDataset, jobs[i%NUM_JOB_TYPES].m_Mdim, jobs[i%NUM_JOB_TYPES].m_Udim);
		}
		lrmf[i]->DivideLBIntoTiles(tileSize);
		lrmf[i]->RandInitMU();
		lrmf[i]->CreateMemoryLayout();

		bool fit = lrmf[i]->fOptimizeRound(
			0, lrmf[i]->GetNumTilesM(),
			0, lrmf[i]->GetNumTilesU(),
			stepSize, lambda, true, false, jobs[i%NUM_JOB_TYPES].m_numEpochs);
		if (!fit)
			return 1;
	}

	for (uint32_t i = 0; i < numJobsMultiplier*NUM_JOB_TYPES; i++) {
		server.PreCopy(lrmf[i]);
	}

	double start = get_time();
	vector<FThread*> fthreads;
	for (uint32_t i = 0; i < numJobsMultiplier*NUM_JOB_TYPES; i++) {
		fthreads.push_back( server.Request(lrmf[i], jobs[i%NUM_JOB_TYPES].m_priority) );
	}
	for (uint32_t i = 0; i < fthreads.size(); i++) {
		fthreads[i]->WaitUntilFinished();
	}
	double end = get_time();
	double total = (end - start);
	cout << "Total time: " << total << endl;

	for (uint32_t i = 0; i < fthreads.size(); i++) {
		cout << "threadId " << fthreads[i]->GetId() << " " << fthreads[i]->GetResponseTime() << endl;
	}

	// Verify
	for (uint32_t i = 0; i < NUM_JOB_TYPES; i++) {
		cout << "Job " << i << " convergence: " << endl;
#ifdef XILINX
		server.GetInputHandleFromFPGA(lrmf[i]);
#endif
		lrmf[i]->CopyModel();
		cout << lrmf[i]->RMSE() << endl;
	}

	return 0;
}