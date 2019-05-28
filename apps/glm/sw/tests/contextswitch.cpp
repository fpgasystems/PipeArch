#include <iostream>
#include "FPGA_ColumnML.h"
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

#define VALUE_TO_INT_SCALER 10
#define MAX_NUM_JOBS 100
#define NUM_JOB_TYPES 3

struct JobProperties {
	ModelType m_type;
	uint32_t m_numSamples;
	uint32_t m_numFeatures;
	uint32_t m_numEpochs;
	uint32_t m_minibatchSize;
	float m_totalSize;
};

int main(int argc, char* argv[]) {

	double start, end;
	JobProperties jobs[NUM_JOB_TYPES];
	float stepSize = 0.01;
	float lambda = 0;

	uint32_t numJobs = 0;
	uint32_t partitionSize = 16000;
	char config[3] = {'-', '-', '-'};
	bool enableContextSwitch = false;
	bool enableThreadMigration = false;
	bool randomizeJobOrder = false;
	if (!(argc == 4)) {
		cout << "Usage: ./app <numJobs> <partitionSize> <'e'nableContextSwitch 'e'nableThreadMigration 'r'andomizeJobOrder>" << endl;
		return 0;
	}
	numJobs = atoi(argv[1]);
	partitionSize = atoi(argv[2]);
	for (uint32_t i = 0; i < 3; i++) {
		if (argv[3][i] == '\0') {
			break;
		}
		else {
			config[i] = argv[3][i];
		}
	}
	enableContextSwitch = config[0] == 'e';
	enableThreadMigration = config[1] == 'e';
	randomizeJobOrder = config[2] == 'r';

	if (numJobs > MAX_NUM_JOBS) {
		cout << "numJobs is larger than MAX_NUM_JOBS" << endl;
		return 1;
	}

	for (uint32_t i = 0; i < NUM_JOB_TYPES; i++) {
		switch(i) {
			case 0:
				jobs[0].m_type = linreg;
				jobs[0].m_numSamples = 64000;
				jobs[0].m_numFeatures = 126;
				jobs[0].m_numEpochs = 50;
				jobs[0].m_minibatchSize = 32;
				break;
			case 1:
				jobs[1].m_type = linreg;
				jobs[1].m_numSamples = 64000;
				jobs[1].m_numFeatures = 784;
				jobs[1].m_numEpochs = 80;
				jobs[1].m_minibatchSize = 32;
			case 2:
				jobs[2].m_type = logreg;
				jobs[2].m_numSamples = 80000;
				jobs[2].m_numFeatures = 2048;
				jobs[2].m_numEpochs = 50;
				jobs[2].m_minibatchSize = 1;
		}
		jobs[i].m_totalSize = (((float)4*jobs[i].m_numSamples*jobs[i].m_numFeatures)/1e9)*jobs[i].m_numEpochs;
	}

	if (enableContextSwitch) {
		cout << "-> enableContextSwitch" << endl;
	}
	if (enableThreadMigration) {
		cout << "-> enableThreadMigration" << endl;
	}
	if (randomizeJobOrder) {
		cout << "-> randomizeJobOrder" << endl;
	}

	ServerWrapper server(enableContextSwitch, enableThreadMigration);

	FPGA_ColumnML* columnML[MAX_NUM_JOBS];
	for (uint32_t i = 0; i < numJobs; i++) {
		columnML[i] = new FPGA_ColumnML(server.GetServer());
	}

	AdditionalArguments args[NUM_JOB_TYPES];
	for (uint32_t i = 0; i < NUM_JOB_TYPES; i++) {
		columnML[i]->m_cstore->GenerateSyntheticData(jobs[i].m_numSamples, jobs[i].m_numFeatures, false, MinusOneToOne);

		args[i].m_firstSample = 0;
		args[i].m_numSamples = columnML[i]->m_cstore->m_numSamples;
		args[i].m_constantStepSize = true;
		args[i].m_useOnehotLabels = false;
	}

	MemoryFormat format = RowStore;
	for (uint32_t i = 0; i < NUM_JOB_TYPES; i++) {
		columnML[i]->CreateMemoryLayout(format, partitionSize);
	}
	for (uint32_t i = NUM_JOB_TYPES; i < numJobs; i++) {
		columnML[i]->UseCreatedMemoryLayout(columnML[i%NUM_JOB_TYPES]);
	}

	for (uint32_t i = 0; i < numJobs; i++) {
		if (jobs[i%NUM_JOB_TYPES].m_minibatchSize == 1) {
			columnML[i]->fSGD(
				jobs[i%NUM_JOB_TYPES].m_type,
				jobs[i%NUM_JOB_TYPES].m_numEpochs,
				stepSize,
				lambda,
				0);
		}
		else {
			columnML[i]->fSGD_minibatch(
				jobs[i%NUM_JOB_TYPES].m_type,
				jobs[i%NUM_JOB_TYPES].m_numEpochs,
				jobs[i%NUM_JOB_TYPES].m_minibatchSize,
				stepSize, 
				lambda,
				0);
		}
	}

	FThread* thread[MAX_NUM_JOBS];

	for (uint32_t i = 0; i < numJobs; i++) {
		server.PreCopy(columnML[i]);
	}

	// Get standalone runtimes for each job type
	start = get_time();
	for (uint32_t i = 0; i < NUM_JOB_TYPES; i++) {
		thread[i] = server.Request(columnML[i]);
		thread[i]->WaitUntilFinished();
		cout << "Standalone time job " << i << " is: " << thread[i]->GetResponseTime() << endl;
		cout << "Processing rate: " << jobs[i].m_totalSize/thread[i]->GetResponseTime() << " GB/s" <<endl;
	}
	end = get_time();
	cout << "Total time: " << end-start << endl;
	server.ResetNumThreads();

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

	start = get_time();
	float totalSize = 0;
	for (uint32_t i = 0; i < numJobs; i++) {
		cout << "Starting job: " << order[i] << endl;
		thread[i] = server.Request(columnML[order[i]]);
	}
	for (uint32_t i = 0; i < numJobs; i++) {
		thread[i]->WaitUntilFinished();
		totalSize += jobs[i%NUM_JOB_TYPES].m_totalSize;
	}
	end = get_time();
	cout << "Total time: " << end-start << endl;
	cout << "Total processing rate: " << totalSize/(end-start) << endl;

	for (uint32_t i = 0; i < numJobs; i++) {
#ifdef VALIDATE_LOSS
		auto output1 = iFPGA::CastToFloat(columnML[i]->m_outputHandle);
		float* xHistory1 = (float*)(output1 + 16);
		for (uint32_t e = 0; e < jobs[i%NUM_JOB_TYPES].m_numEpochs; e++) {
			float loss = columnML[i]->Loss(jobs[i%NUM_JOB_TYPES].m_type, xHistory1 + e*columnML[i]->m_alignedNumFeatures, lambda, &args[i%NUM_JOB_TYPES]);
			cout << "loss " << e << ": " << loss << endl;
		}
#endif
		cout << "jobType " << order[i]%NUM_JOB_TYPES << " ";
		cout << "threadId " << thread[i]->GetId() << " " << thread[i]->GetResponseTime() << endl;
	}

	for (uint32_t i = 0; i < numJobs; i++) {
		delete columnML[i];
	}

	return 0;
}