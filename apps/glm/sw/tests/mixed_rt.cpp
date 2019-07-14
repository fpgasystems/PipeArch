#include <iostream>
#include "FPGA_ColumnML.h"
#include "FPGA_LRMF.h"
#include "Server.h"

/*
1. Have NUM_JOB_TYPES job types, each with a standalone runtime, affected by the algorithm, minibatch size, dataset size etc.
First measure standalone runtimes for NUM_JOB_TYPES jobs.
Then, enqueue numJobs jobs, start measuring time when the job is enqueued.
Measure response time for each job. Plot two histograms: 1 with context-switch enabled, 1 with context-switch disabled.
Expected observation:
When context-switch is enabled, we expect to see NUM_JOB_TYPES peaks in the histogram,
close to the job's standalone runtimes with some overhead.
This shows that each job can complete close to its runtime.
When context-switch is disabled, we expect to see 1 peak, dominated by the longest running job.

2. Enable context-switch, change thread migration capability. Run the same workload as before.
Expected observation: With thread-migration enabled we hope to see shorter total
runtime for all the jobs, since there is better load balancing.

3. Context-switch granularity: Change partition size and compare total runtime for
running N jobs. How does the overhead increase?
*/

// #define VALIDATE_LOSS

#define NUM_COLUMNML_JOB_TYPES 3
#define NUM_LRMF_JOB_TYPES 0

struct ColumnMLJobProperties {
	MemoryFormat m_format;
	ModelType m_type;
	uint32_t m_numSamples;
	uint32_t m_partitionSize;
	uint32_t m_minibatchSize;

	// Common
	uint32_t m_numFeatures;
	uint32_t m_numEpochs;
	uint32_t m_priority;
	float m_totalSize;
};

struct LRMFJobProperties {
	uint32_t m_Mdim;
	uint32_t m_Udim;
	float m_sparsenessFactor;
	uint32_t m_tileSize;
	
	// Common
	uint32_t m_numFeatures;
	uint32_t m_numEpochs;
	uint32_t m_priority;
	float m_totalSize;
};

int main(int argc, char* argv[]) {

	double start, end;
	ColumnMLJobProperties cjobs[NUM_COLUMNML_JOB_TYPES];
	LRMFJobProperties ljobs[NUM_LRMF_JOB_TYPES];

	uint32_t numJobsMultiplier = 0;
	char config[4] = {'-', '-', '-', '-'};
	bool enableContextSwitch = false;
	bool enableThreadMigration = false;
	bool enablePriority = false;
	bool randomizeJobOrder = false;
	if (!(argc == 3)) {
		cout << "Usage: ./app <numJobsMultiplier> <'e'nableContextSwitch 'e'nableThreadMigration 'e'nablePriority 'r'andomizeJobOrder>" << endl;
		return 0;
	}
	numJobsMultiplier = atoi(argv[1]);
	for (uint32_t i = 0; i < 4; i++) {
		if (argv[2][i] == '\0') {
			break;
		}
		else {
			config[i] = argv[2][i];
		}
	}
	enableContextSwitch = config[0] == 'e';
	enableThreadMigration = config[1] == 'e';
	enablePriority = config[2] == 'e';
	randomizeJobOrder = config[3] == 'r';

	for (uint32_t i = 0; i < NUM_COLUMNML_JOB_TYPES; i++) {
		switch(i) {
			case 0: // SYN_Logreg
				cjobs[0].m_format = RowStore;
				cjobs[0].m_type = logreg;
				cjobs[0].m_numSamples = 131072;
				cjobs[0].m_partitionSize = 16384;
				cjobs[0].m_minibatchSize = 32;
				cjobs[0].m_numFeatures = 1024;
				cjobs[0].m_numEpochs = 20;
				cjobs[0].m_priority = 2;
			case 1: // SYN_Lasso1
				cjobs[1].m_format = ColumnStore;
				cjobs[1].m_type = linreg;
				cjobs[1].m_numSamples = 8388608;
				cjobs[1].m_partitionSize = 16384;
				cjobs[1].m_numFeatures = 16;
				cjobs[1].m_numEpochs = 10;
				cjobs[1].m_priority = 2;
				break;
			case 2: // SYN_Lasso2
				cjobs[2].m_format = ColumnStore;
				cjobs[2].m_type = linreg;
				cjobs[2].m_numSamples = 524288;
				cjobs[2].m_partitionSize = 16384;
				cjobs[2].m_numFeatures = 256;
				cjobs[2].m_numEpochs = 30;
				cjobs[2].m_priority = 2;
				break;
		}
		cjobs[i].m_totalSize = (((float)4*cjobs[i].m_numSamples*cjobs[i].m_numFeatures)/1e9)*cjobs[i].m_numEpochs;
	}
	for (uint32_t i = 0; i < NUM_LRMF_JOB_TYPES; i++) {
		switch(i) {
			case 0: // SYN_LRMF1
				ljobs[0].m_Mdim = 4096;
				ljobs[0].m_Udim = 524288;
				ljobs[0].m_sparsenessFactor = 0.05;
				ljobs[0].m_tileSize = 256;
				ljobs[0].m_numFeatures = 32;
				ljobs[0].m_numEpochs = 4;
				ljobs[0].m_priority = 2;
				break;
			case 1: // SYN_LRMF2
				ljobs[1].m_Mdim = 4096;
				ljobs[1].m_Udim = 524288;
				ljobs[1].m_sparsenessFactor = 0.1;
				ljobs[1].m_tileSize = 256;
				ljobs[1].m_numFeatures = 32;
				ljobs[1].m_numEpochs = 4;
				ljobs[1].m_priority = 2;
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
	vector<FPGA_ColumnML*> columnML(numJobsMultiplier*NUM_COLUMNML_JOB_TYPES);
	vector<FPGA_LRMF*> lrmf(numJobsMultiplier*NUM_LRMF_JOB_TYPES);

	for (uint32_t i = 0; i < numJobsMultiplier*NUM_COLUMNML_JOB_TYPES; i++) {
		columnML[i] = new FPGA_ColumnML(server.GetServer(), enableContextSwitch);
	}
	for (uint32_t i = 0; i < numJobsMultiplier*NUM_LRMF_JOB_TYPES; i++) {
		lrmf[i] = new FPGA_LRMF(server.GetServer(), ljobs[i%NUM_LRMF_JOB_TYPES].m_numFeatures, enableContextSwitch);
	}

	// Generate data
	for (uint32_t i = 0; i < numJobsMultiplier*NUM_COLUMNML_JOB_TYPES; i++) {
		if (i < NUM_COLUMNML_JOB_TYPES)
			columnML[i]->m_cstore->GenerateSyntheticData(cjobs[i%NUM_COLUMNML_JOB_TYPES].m_numSamples, cjobs[i%NUM_COLUMNML_JOB_TYPES].m_numFeatures, true, ZeroToOne);
		else
			columnML[i]->m_cstore->CopyDataset(columnML[i%NUM_COLUMNML_JOB_TYPES]->m_cstore);
	}

	for (uint32_t i = 0; i < numJobsMultiplier*NUM_LRMF_JOB_TYPES; i++) {
		if (i < NUM_LRMF_JOB_TYPES) {
			lrmf[i]->GenerateSyntheticData(ljobs[i%NUM_LRMF_JOB_TYPES].m_Mdim, ljobs[i%NUM_LRMF_JOB_TYPES].m_Udim, ljobs[i%NUM_LRMF_JOB_TYPES].m_sparsenessFactor);
			lrmf[i]->DivideLBIntoTiles(ljobs[i%NUM_LRMF_JOB_TYPES].m_tileSize);
			ljobs[i].m_totalSize = lrmf[i]->GetDataSize()/1e9;
		}
		else {
			lrmf[i]->CopyDataset(lrmf[i%NUM_LRMF_JOB_TYPES]);
		}
	}

	// Prepare data
	for (uint32_t i = 0; i < numJobsMultiplier*NUM_COLUMNML_JOB_TYPES; i++) {
		if (cjobs[i%NUM_COLUMNML_JOB_TYPES].m_format == RowStore) {
			float stepSize = 0.01;
			float lambda = 0.001;

			columnML[i]->CreateMemoryLayout(RowStore, cjobs[i%NUM_COLUMNML_JOB_TYPES].m_partitionSize, false);
			columnML[i]->fSGD_minibatch(
				cjobs[i%NUM_COLUMNML_JOB_TYPES].m_type,
				cjobs[i%NUM_COLUMNML_JOB_TYPES].m_numEpochs,
				cjobs[i%NUM_COLUMNML_JOB_TYPES].m_minibatchSize, stepSize, lambda, 0);
		}
		else if(cjobs[i%NUM_COLUMNML_JOB_TYPES].m_format == ColumnStore) {
			float stepSize = 4;
			float lambda = 0.001;

			columnML[i]->CreateMemoryLayout(ColumnStore, cjobs[i%NUM_COLUMNML_JOB_TYPES].m_partitionSize, cjobs[i%NUM_COLUMNML_JOB_TYPES].m_numEpochs, false);
			columnML[i]->fSCD(
				0,
				columnML[i]->m_numPartitions,
				cjobs[i%NUM_COLUMNML_JOB_TYPES].m_type,
				cjobs[i%NUM_COLUMNML_JOB_TYPES].m_numEpochs, stepSize, lambda);
		}
	}
	for (uint32_t i = 0; i < numJobsMultiplier*NUM_LRMF_JOB_TYPES; i++) {
		float stepSize = 0.01;
		float lambda = 0;

		lrmf[i]->CreateMemoryLayout();
		bool fit = lrmf[i]->fOptimizeRound(
			0, lrmf[i]->GetNumTilesM(),
			0, lrmf[i]->GetNumTilesU(),
			stepSize, lambda, true, false, ljobs[i%NUM_LRMF_JOB_TYPES].m_numEpochs);
		if (!fit)
			return 1;
	}

	vector<FPGA_Program*> programs;
	vector<float> datasetSizes;
	for (uint32_t n = 0; n < numJobsMultiplier; n++) {
		for(uint32_t i = 0; i < NUM_COLUMNML_JOB_TYPES; i++) {
			programs.push_back((FPGA_Program*)columnML[n*NUM_COLUMNML_JOB_TYPES + i]);
			datasetSizes.push_back(cjobs[i].m_totalSize);
		}
		for (uint32_t i = 0; i < NUM_LRMF_JOB_TYPES; i++) {
			programs.push_back((FPGA_Program*)lrmf[n*NUM_LRMF_JOB_TYPES + i]);
			datasetSizes.push_back(ljobs[i].m_totalSize);
		}
	}

	for (uint32_t i = 0; i < programs.size(); i++) {
		server.PreCopy(programs[i]);
	}

	// Get standalone runtimes
	for (uint32_t i = 0; i < NUM_COLUMNML_JOB_TYPES+NUM_LRMF_JOB_TYPES; i++) {
		FThread* fthread = server.Request(programs[i], 0);
		fthread->WaitUntilFinished();
		cout << "Standalone time job " << i << " is: " << fthread->GetResponseTime() << endl;
		cout << "Processing rate: " << datasetSizes[i]/fthread->GetResponseTime() << " GB/s" <<endl;
	}
	server.ResetNumThreads();

	uint32_t numJobs = programs.size();

	vector<uint32_t> order;
	for (uint32_t i = 0; i < numJobs; i++) {
		order.push_back(i);
	}
	if (randomizeJobOrder) {
		srand(3);
		for (uint32_t i = 0; i < numJobs; i++) {
			uint32_t pos = rand()%numJobs;
			uint32_t temp = order[pos];
			order[pos] = order[i];
			order[i] = temp;
		}
	}

	vector<FThread*> fthreads(numJobs);
	start = get_time();
	float totalSize = 0;
	for (uint32_t i = 0; i < numJobs; i++) {
		cout << "Starting job: " << order[i] << endl;
		fthreads[i] = server.Request(programs[order[i]]);
	}
	for (uint32_t i = 0; i < numJobs; i++) {
		fthreads[i]->WaitUntilFinished();
		totalSize += datasetSizes[i];
	}
	end = get_time();
	cout << "Total time: " << end-start << endl;
	cout << "Total processing rate: " << totalSize/(end-start) << " GB/s" << endl;

	for (uint32_t i = 0; i < numJobs; i++) {
		cout << "jobType " << order[i]%(NUM_COLUMNML_JOB_TYPES+NUM_LRMF_JOB_TYPES) << " ";
		cout << "threadId " << fthreads[i]->GetId() << " " << fthreads[i]->GetResponseTime() << endl;
	}

	// for (uint32_t i = 0; i < columnML.size(); i++) {
	// 	delete columnML[i];
	// }
	// for (uint32_t i = 0; i < lrmf.size(); i++) {
	// 	delete lrmf[i];
	// }

	return 0;
}