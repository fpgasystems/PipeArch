#include <iostream>
#include "FPGA_ColumnML.h"
#include "Server.h"

// State from the AFU's JSON file, extracted using OPAE's afu_json_mgr script
#include "afu_json_info.h"

/*
1. Overhead of context-switch: Enqueue 1 job type (that job should be high priority,
so it will not be paused) on a busy server, measure response time. Compare this to
that job running standalone on the server: How much is the overhead for different job types?
-> This will of course depend on the model size.
*/

// #define VALIDATE_LOSS

#define VALUE_TO_INT_SCALER 10
#define MAX_NUM_JOBS 100
#define NUM_JOB_TYPES 3
#define NUM_TIMES_TO_EXECUTE_HIGH_JOB 3

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
	uint32_t partitionSize = 16000;
	MemoryFormat format = RowStore;
	float stepSize = 0.01;
	float lambda = 0;

	uint32_t numJobs = 0;
	char config[3] = {'-', '-', '-'};
	bool enableContextSwitch = false;
	bool enableThreadMigration = false;
	bool randomizeJobOrder = false;
	uint32_t numSamples = 0;
	uint32_t numFeatures = 0;
	uint32_t numEpochs = 10;
	uint32_t minibatchSize = 1;
	if (!(argc == 7)) {
		cout << "Usage: ./app <numJobs> <'e'nableContextSwitch 'e'nableThreadMigration 'r'andomizeJobOrder> <numSamples> <numFeatures> <minibatchSize> <numEpochs>" << endl;
		return 0;
	}
	numJobs = atoi(argv[1]);
	for (uint32_t i = 0; i < 3; i++) {
		if (argv[2][i] == '\0') {
			break;
		}
		else {
			config[i] = argv[2][i];
		}
	}
	enableContextSwitch = config[0] == 'e';
	enableThreadMigration = config[1] == 'e';
	randomizeJobOrder = config[2] == 'r';
	numSamples = atoi(argv[3]);
	numFeatures = atoi(argv[4]);
	minibatchSize = atoi(argv[5]);
	numEpochs = atoi(argv[6]);
	
	float highTotalSize = (((float)4*numSamples*numFeatures)/1e9)*numEpochs;

	if (numJobs > MAX_NUM_JOBS) {
		cout << "numJobs is larger than MAX_NUM_JOBS" << endl;
		return 1;
	}

	for (uint32_t i = 0; i < NUM_JOB_TYPES; i++) {
		switch(i) {
			case 0:
				jobs[0].m_type = linreg;
				jobs[0].m_numSamples = 32000;
				jobs[0].m_numFeatures = 126;
				jobs[0].m_numEpochs = 100;
				jobs[0].m_minibatchSize = 32;
				break;
			case 1:
				jobs[1].m_type = linreg;
				jobs[1].m_numSamples = 64000;
				jobs[1].m_numFeatures = 784;
				jobs[1].m_numEpochs = 80;
				jobs[1].m_minibatchSize = 16;
			case 2:
				jobs[2].m_type = linreg;
				jobs[2].m_numSamples = 80000;
				jobs[2].m_numFeatures = 1024;
				jobs[2].m_numEpochs = 100;
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

	Server server(AFU_ACCEL_UUID, enableContextSwitch, enableThreadMigration, 1);

	FPGA_ColumnML* columnML[MAX_NUM_JOBS];
	for (uint32_t i = 0; i < numJobs; i++) {
		columnML[i] = new FPGA_ColumnML(&server);
	}

	AdditionalArguments args[NUM_JOB_TYPES];
	for (uint32_t i = 0; i < NUM_JOB_TYPES; i++) {
		columnML[i]->m_cstore->GenerateSyntheticData(jobs[i].m_numSamples, jobs[i].m_numFeatures, false, MinusOneToOne);

		args[i].m_firstSample = 0;
		args[i].m_numSamples = columnML[i]->m_cstore->m_numSamples;
		args[i].m_constantStepSize = true;
	}
	for (uint32_t i = NUM_JOB_TYPES; i < numJobs; i++) {
		columnML[i]->m_cstore = columnML[i%NUM_JOB_TYPES]->m_cstore;
	}
	for (uint32_t i = 0; i < numJobs; i++) {
		columnML[i]->CreateMemoryLayout(format, partitionSize);
	}
	for (uint32_t i = 0; i < numJobs; i++) {
		if (jobs[i%NUM_JOB_TYPES].m_minibatchSize == 1) {
			columnML[i]->fSGD(
				jobs[i%NUM_JOB_TYPES].m_type,
				jobs[i%NUM_JOB_TYPES].m_numEpochs,
				stepSize,
				lambda);
		}
		else {
			columnML[i]->fSGD_minibatch(
				jobs[i%NUM_JOB_TYPES].m_type,
				jobs[i%NUM_JOB_TYPES].m_numEpochs,
				jobs[i%NUM_JOB_TYPES].m_minibatchSize,
				stepSize, 
				lambda);
		}
	}

	FPGA_ColumnML* highColumnML = new FPGA_ColumnML(&server, false);
	highColumnML->m_cstore->GenerateSyntheticData(numSamples, numFeatures, false, MinusOneToOne);
	highColumnML->CreateMemoryLayout(format, partitionSize);
	if (minibatchSize == 1) {
		highColumnML->fSGD(linreg, numEpochs, stepSize, lambda);
	}
	else {
		highColumnML->fSGD_minibatch(linreg, numEpochs, minibatchSize, stepSize, lambda);
	}

	FThread* thread[MAX_NUM_JOBS];

	// Get standalone runtimes for each job type
	start = get_time();
	for (uint32_t i = 0; i < NUM_JOB_TYPES; i++) {
		thread[i] = server.Request(columnML[i]);
		thread[i]->WaitUntilFinished();
		cout << "Standalone time job " << i << " is: " << thread[i]->GetResponseTime() << endl;
		cout << "Size: " << jobs[i].m_totalSize << " GB" << endl;
		cout << "Processing rate: " << jobs[i].m_totalSize/thread[i]->GetResponseTime() << " GB/s" <<endl;
	}
	end = get_time();
	cout << "Total time: " << end-start << endl;

	double highTime[NUM_TIMES_TO_EXECUTE_HIGH_JOB+1];
	FThread* highThread = server.Request(highColumnML, 1);
	highThread->WaitUntilFinished();
	cout << "Standalone time for high job is: " << highThread->GetResponseTime() << endl;
	cout << "Size: " << highTotalSize << " GB" << endl;
	cout << "Processing rate: " << highTotalSize/highThread->GetResponseTime() << " GB/s" <<endl;
	highTime[0] = highThread->GetResponseTime();

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

	for (uint32_t t = 0; t < NUM_TIMES_TO_EXECUTE_HIGH_JOB; t++) {
		highThread = server.Request(highColumnML, 1);
		highThread->WaitUntilFinished();
		double rt = highThread->GetResponseTime();
		cout << "Busy time for high job is: " << rt << endl;
		highTime[t+1] = rt;
		sleep(0.5);
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
		auto output = columnML[i]->CastToFloat('o');
		float* xHistory = (float*)(output + 16);
		for (uint32_t e = 0; e < jobs[i%NUM_JOB_TYPES].m_numEpochs; e++) {
			float loss = columnML[i]->Loss(jobs[i%NUM_JOB_TYPES].m_type, xHistory + e*columnML[i]->m_alignedNumFeatures, lambda, &args[i%NUM_JOB_TYPES]);
			cout << "loss " << e << ": " << loss << endl;
		}
#endif
		cout << "jobType " << order[i]%NUM_JOB_TYPES << " ";
		cout << "threadId " << thread[i]->GetId() << " " << thread[i]->GetResponseTime() << endl;
	}

	// Validate highColumnML loss
	AdditionalArguments highArgs;
	highArgs.m_firstSample = 0;
	highArgs.m_numSamples = numSamples;
	highArgs.m_constantStepSize = true;

#ifdef VALIDATE_LOSS
	auto output = highColumnML->CastToFloat('o');
	float* xHistory = (float*)(output + 16);
	for (uint32_t e = 0; e < numEpochs; e++) {
		float loss = highColumnML->Loss(linreg, xHistory + e*highColumnML->m_alignedNumFeatures, lambda, &highArgs);
		cout << "loss " << e << ": " << loss << endl;
	}
#endif

	cout << "highTime:" << endl;
	for (uint32_t t = 0; t < NUM_TIMES_TO_EXECUTE_HIGH_JOB+1; t++) {
		cout << highTime[t] << "\t";
	}

	return 0;
}