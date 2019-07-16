#include <iostream>
#include "FPGA_ColumnML.h"
#include "Server.h"

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
#define NUM_TIMES_TO_EXECUTE_HIGH_JOB 5

struct JobProperties {
	ModelType m_type;
	uint32_t m_numSamples;
	uint32_t m_numFeatures;
	uint32_t m_numEpochs;
	uint32_t m_minibatchSize;
	uint32_t m_priority;
	float m_totalSize;
};

int main(int argc, char* argv[]) {

	double start, end;
	JobProperties jobs[NUM_JOB_TYPES];
	uint32_t partitionSize = 16384;
	MemoryFormat format = RowStore;
	float stepSize = 0.01;
	float lambda = 0;

	uint32_t numJobs = 0;
	char config[4] = {'-', '-', '-', '-'};
	bool enableContextSwitch = false;
	bool enableThreadMigration = false;
	bool enablePriority = false;
	bool randomizeJobOrder = false;
	uint32_t numSamples = 0;
	uint32_t numFeatures = 0;
	uint32_t numEpochs = 10;
	uint32_t minibatchSize = 1;
	if (!(argc == 7)) {
		cout << "Usage: ./app <numJobs> <'e'nableContextSwitch 'e'nableThreadMigration 'e'nablePriority 'r'andomizeJobOrder> <numSamples> <numFeatures> <minibatchSize> <numEpochs>" << endl;
		return 0;
	}
	numJobs = atoi(argv[1]);
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
				jobs[0].m_type = logreg;
				jobs[0].m_numSamples = 131072;
				jobs[0].m_numFeatures = 1024;
				jobs[0].m_numEpochs = 3;
				jobs[0].m_minibatchSize = 32;
				jobs[0].m_priority = 0;
				break;
			case 1:
				jobs[1].m_type = logreg;
				jobs[1].m_numSamples = 131072;
				jobs[1].m_numFeatures = 1024;
				jobs[1].m_numEpochs = 30;
				jobs[1].m_minibatchSize = 32;
				jobs[1].m_priority = 0;
				break;
			case 2:
				jobs[2].m_type = logreg;
				jobs[2].m_numSamples = 131072;
				jobs[2].m_numFeatures = 1024;
				jobs[2].m_numEpochs = 80;
				jobs[2].m_minibatchSize = 32;
				jobs[2].m_priority = 0;
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

	FPGA_ColumnML* columnML[MAX_NUM_JOBS];
	for (uint32_t i = 0; i < numJobs; i++) {
		columnML[i] = new FPGA_ColumnML(server.GetServer());
	}

	AdditionalArguments args[NUM_JOB_TYPES];
	for (uint32_t i = 0; i < NUM_JOB_TYPES; i++) {
		columnML[i]->m_cstore->GenerateSyntheticData(jobs[i].m_numSamples, jobs[i].m_numFeatures, true, ZeroToOne);

		args[i].m_firstSample = 0;
		args[i].m_numSamples = columnML[i]->m_cstore->m_numSamples;
		args[i].m_constantStepSize = true;
	}
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

	FPGA_ColumnML* highColumnML = new FPGA_ColumnML(server.GetServer());
	highColumnML->m_cstore->GenerateSyntheticData(numSamples, numFeatures, true, ZeroToOne);
	highColumnML->CreateMemoryLayout(format, partitionSize);
	if (minibatchSize == 1) {
		highColumnML->fSGD(logreg, numEpochs, stepSize, lambda, 0);
	}
	else {
		highColumnML->fSGD_minibatch(logreg, numEpochs, minibatchSize, stepSize, lambda, 0);
	}

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

	for (uint32_t i = 0; i < NUM_JOB_TYPES; i++) {
		server.PreCopy(columnML[i]);
	}

	FThread* thread[MAX_NUM_JOBS];
	start = get_time();
	float totalSize = 0;
	for (uint32_t i = 0; i < numJobs; i++) {
		cout << "Starting job: " << order[i] << endl;
		thread[i] = server.Request(columnML[order[i]], jobs[i%NUM_JOB_TYPES].m_priority);
	}
	thread[0]->WaitUntilFinished(); // Wait just for the first job to finish

	for (uint32_t t = 0; t < NUM_TIMES_TO_EXECUTE_HIGH_JOB; t++) {
		cout << "Starting high job..." << endl;
		highThread = server.Request(highColumnML, 4);
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
		auto output = iFPGA::CastToFloat(columnML[i]->m_outputHandle);
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
	auto output = iFPGA::CastToFloat(highColumnML->m_outputHandle);
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