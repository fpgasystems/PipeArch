#include <iostream>
#include "ColumnML.h"
#include "FPGA_ColumnML.h"
#include "Server.h"

#define VALUE_TO_INT_SCALER 10

#define FPGA

#define NUM_JOB_TYPES 4

struct JobProperties {
	ModelType m_type;
	uint32_t m_numSamples;
	uint32_t m_numFeatures;
	uint32_t m_numEpochs;
	uint32_t m_partitionSize;
	uint32_t m_priority;
	float m_totalSize;
	uint32_t m_numThreads;
};

int main(int argc, char* argv[]) {

	JobProperties jobs[NUM_JOB_TYPES];
	float stepSize = 4;
	float lambda = 0.001;

	uint32_t numJobsMultiplier = 0;
	char config[4] = {'-', '-', '-', '-'};
	bool enableContextSwitch = false;
	bool enableThreadMigration = false;
	bool enablePriority = false;
	bool randomizeJobOrder = false;
	uint32_t sw0hw1 = 0;
	if (!(argc == 4)) {
		cout << "Usage: ./app <numJobsMultiplier> <'e'nableContextSwitch 'e'nableThreadMigration 'e'nablePriority 'r'andomizeJobOrder> <sw0hw1>" << endl;
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
	sw0hw1 = atoi(argv[3]);

	for (uint32_t i = 0; i < NUM_JOB_TYPES; i++) {
		switch(i) {
			case 0: // AEA
				jobs[0].m_type = logreg;
				jobs[0].m_numSamples = 32768;
				jobs[0].m_numFeatures = 126;
				jobs[0].m_numEpochs = 50;
				jobs[0].m_partitionSize = 16384;
				jobs[0].m_priority = 4;
				jobs[0].m_numThreads = 2;
				break;
			case 1: // KDD
				jobs[1].m_type = logreg;
				jobs[1].m_numSamples = 131329;
				jobs[1].m_numFeatures = 2330;
				jobs[1].m_numEpochs = 10;
				jobs[1].m_partitionSize = 16384;
				jobs[1].m_priority = 1;
				jobs[1].m_numThreads = 14;
				break;
			case 2: // SYN_Lasso1
				jobs[2].m_type = linreg;
				jobs[2].m_numSamples = 8388608;
				jobs[2].m_numFeatures = 16;
				jobs[2].m_numEpochs = 10;
				jobs[2].m_partitionSize = 16384;
				jobs[2].m_priority = 2;
				jobs[2].m_numThreads = 14;
				break;
			case 3: // SYN_Lasso2
				jobs[3].m_type = linreg;
				jobs[3].m_numSamples = 524288;
				jobs[3].m_numFeatures = 256;
				jobs[3].m_numEpochs = 10;
				jobs[3].m_partitionSize = 16384;
				jobs[3].m_priority = 3;
				jobs[3].m_numThreads = 14;
				break;
		}
		jobs[i].m_totalSize = (((float)4*jobs[i].m_numSamples*jobs[i].m_numFeatures)/1e9)*jobs[i].m_numEpochs;
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
	vector<FPGA_ColumnML*> columnML(numJobsMultiplier*NUM_JOB_TYPES);
	AdditionalArguments args[NUM_JOB_TYPES];

	for (uint32_t i = 0; i < numJobsMultiplier*NUM_JOB_TYPES; i++) {
		columnML[i] = new FPGA_ColumnML(server.GetServer());
		if (i%NUM_JOB_TYPES == 0)
			columnML[i]->m_cstore->GenerateSyntheticData(jobs[i/NUM_JOB_TYPES].m_numSamples, jobs[i/NUM_JOB_TYPES].m_numFeatures, true, ZeroToOne);
		else
			columnML[i]->m_cstore->CopyDataset(columnML[i-1]->m_cstore);

		args[i/NUM_JOB_TYPES].m_firstSample = 0;
		args[i/NUM_JOB_TYPES].m_numSamples = jobs[i/NUM_JOB_TYPES].m_numSamples;
		args[i/NUM_JOB_TYPES].m_constantStepSize = true;
		args[i/NUM_JOB_TYPES].m_useOnehotLabels = false;
	}

	if (sw0hw1 == 0) {
		vector<thread*> cthreads(numJobsMultiplier*NUM_JOB_TYPES);

		float totalSize = 0;
		double start = get_time();
		for (uint32_t i = 0; i < numJobsMultiplier*NUM_JOB_TYPES; i++) {
			cthreads[i] = new thread (
								thread_SCD,
								i,
								(ColumnML*)columnML[i],
								jobs[i/NUM_JOB_TYPES].m_type,
								nullptr,
								jobs[i/NUM_JOB_TYPES].m_numEpochs,
								jobs[i/NUM_JOB_TYPES].m_partitionSize,
								stepSize,
								lambda,
								jobs[i/NUM_JOB_TYPES].m_numThreads,
								args[i/NUM_JOB_TYPES]);
			totalSize += jobs[i/NUM_JOB_TYPES].m_totalSize;
		}
		for (uint32_t i = 0; i < cthreads.size(); i++) {
			cthreads[i]->join();
			delete cthreads[i];
		}
		double total = get_time() - start;
		cout << "Total time: " << total << endl;
		cout << "Total size: " << totalSize << " GB" << endl;
		cout << "Processing rate: " << totalSize/total << "GB/s" << endl;
	}
	else {
		for (uint32_t i = 0; i < numJobsMultiplier*NUM_JOB_TYPES; i++) {
			columnML[i]->CreateMemoryLayout(ColumnStore, jobs[i/NUM_JOB_TYPES].m_partitionSize, jobs[i/NUM_JOB_TYPES].m_numEpochs, false);

			columnML[i]->fSCD(
				0,
				columnML[i]->m_numPartitions,
				jobs[i/NUM_JOB_TYPES].m_type,
				jobs[i/NUM_JOB_TYPES].m_numEpochs,
				stepSize,
				lambda);
		}

		for (uint32_t i = 0; i < numJobsMultiplier*NUM_JOB_TYPES; i++) {
			server.PreCopy(columnML[i]);
		}

		float totalSize = 0;
		double start = get_time();
		vector<FThread*> fthreads;

		for (uint32_t i = 0; i < numJobsMultiplier*NUM_JOB_TYPES; i++) {
			fthreads.push_back( server.Request(columnML[i], jobs[i/NUM_JOB_TYPES].m_priority) );

			totalSize += jobs[i/NUM_JOB_TYPES].m_totalSize;
		}
		for (uint32_t i = 0; i < fthreads.size(); i++) {
			fthreads[i]->WaitUntilFinished();
		}
		double total = get_time() - start;
		cout << "Total time: " << total << endl;
		cout << "Total size: " << totalSize << " GB" << endl;
		cout << "Processing rate: " << totalSize/total << "GB/s" << endl;

		for (uint32_t i = 0; i < fthreads.size(); i++) {
			cout << "threadId " << fthreads[i]->GetId() << " " << fthreads[i]->GetResponseTime() << endl;
		}

		// Verify
		// for (uint32_t i = 0; i < NUM_JOB_TYPES; i++) {
		// 	cout << "Job " << i << " convergence: " << endl;

		// 	for (uint32_t e = 0; e < jobs[i].m_numEpochs; e++) {
		// 		float loss = columnML[i]->Loss(jobs[i].m_type, columnML[i]->GetModelSCD(e).data(), lambda, &args[i]);
		// 		cout << loss << endl;
		// 	}
		// }
	}

	for (uint32_t i = 0; i < numJobsMultiplier*NUM_JOB_TYPES; i++) {
		delete columnML[i];
	}

	return 0;
}