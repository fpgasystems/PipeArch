#include <iostream>
#include "ColumnML.h"
#include "FPGA_ColumnML.h"
#include "Server.h"
#include <thread>

// #define FPGA

#define NUM_JOB_TYPES 4

void thread_Loss(
	float* loss,
	ColumnML* columnML,
	ModelType type,
	float* x,
	float lambda,
	AdditionalArguments args)
{
	*loss = columnML->Loss(type, x, lambda, &args);
}

struct JobProperties {
	ModelType m_type;
	uint32_t m_numSamples;
	uint32_t m_numFeatures;
	uint32_t m_numClasses;
	uint32_t m_numEpochs;
	uint32_t m_minibatchSize;
	uint32_t m_partitionSize;
	uint32_t m_priority;
	float m_totalSize;
};

int main(int argc, char* argv[]) {

	JobProperties jobs[NUM_JOB_TYPES];
	float stepSize = 0.001;
	float lambda = 0.0001;

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
			case 0: // IM
				jobs[0].m_type = logreg;
				jobs[0].m_numSamples = 166400;
				jobs[0].m_numFeatures = 2048;
				jobs[0].m_numClasses = 16;
				jobs[0].m_numEpochs = 10;
				jobs[0].m_minibatchSize = 32;
				jobs[0].m_partitionSize = 10400;
				jobs[0].m_priority = 1;
				break;
			case 1: // MNIST
				jobs[1].m_type = logreg;
				jobs[1].m_numSamples = 60000;
				jobs[1].m_numFeatures = 784;
				jobs[1].m_numClasses = 10;
				jobs[1].m_numEpochs = 10;
				jobs[1].m_minibatchSize = 25;
				jobs[1].m_partitionSize = 10000;
				jobs[1].m_priority = 3;
				break;
			case 2: // MUSIC
				jobs[2].m_type = linreg;
				jobs[2].m_numSamples = 448000;
				jobs[2].m_numFeatures = 90;
				jobs[2].m_numClasses = 1;
				jobs[2].m_numEpochs = 20;
				jobs[2].m_minibatchSize = 32;
				jobs[2].m_partitionSize = 16000;
				jobs[2].m_priority = 4;
				break;
			case 3:
				jobs[3].m_type = logreg;
				jobs[3].m_numSamples = 131072;
				jobs[3].m_numFeatures = 1024;
				jobs[3].m_numClasses = 1;
				jobs[3].m_numEpochs = 10;
				jobs[3].m_minibatchSize = 32;
				jobs[3].m_partitionSize = 16384;
				jobs[3].m_priority = 2;
				break;
		}
		jobs[i].m_totalSize = (((float)4*jobs[i].m_numSamples*jobs[i].m_numFeatures*jobs[i].m_numClasses)/1e9)*jobs[i].m_numEpochs;
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

#ifdef FPGA
	ServerWrapper server(enableContextSwitch, enableThreadMigration, enablePriority);
	vector<vector<FPGA_ColumnML*> > columnML(numJobsMultiplier*NUM_JOB_TYPES);
#else
	vector<vector<ColumnML*> > columnML(numJobsMultiplier*NUM_JOB_TYPES);
#endif
	AdditionalArguments args[NUM_JOB_TYPES];

	for (uint32_t i = 0; i < numJobsMultiplier*NUM_JOB_TYPES; i++) {
		for (uint32_t c = 0; c < jobs[i%NUM_JOB_TYPES].m_numClasses; c++) {
#ifdef FPGA
			columnML[i].push_back( new FPGA_ColumnML(server.GetServer()) );
#else
			columnML[i].push_back( new ColumnML() );
#endif
		}
		if (i < NUM_JOB_TYPES)
			columnML[i][0]->m_cstore->GenerateSyntheticData(jobs[i%NUM_JOB_TYPES].m_numSamples, jobs[i%NUM_JOB_TYPES].m_numFeatures, true, ZeroToOne);
		else
			columnML[i][0]->m_cstore->CopyDataset(columnML[i%NUM_JOB_TYPES][0]->m_cstore);

		args[i%NUM_JOB_TYPES].m_firstSample = 0;
		args[i%NUM_JOB_TYPES].m_numSamples = jobs[i%NUM_JOB_TYPES].m_numSamples;
		args[i%NUM_JOB_TYPES].m_constantStepSize = true;
		args[i%NUM_JOB_TYPES].m_useOnehotLabels = false;
	}

	if (sw0hw1 == 0) {
		vector<thread*> cthreads;

		float totalSize = 0;
		double start = get_time();
		for (uint32_t i = 0; i < numJobsMultiplier*NUM_JOB_TYPES; i++) {
			columnML[i][0]->m_cstore->PopulateRowSamples();

			for (uint32_t c = 0; c < jobs[i%NUM_JOB_TYPES].m_numClasses; c++) {
				cthreads.push_back(
					new thread(
						thread_SGD,
						0,
						columnML[i][0],
						jobs[i%NUM_JOB_TYPES].m_type,
						nullptr,
						jobs[i%NUM_JOB_TYPES].m_numEpochs,
						jobs[i%NUM_JOB_TYPES].m_minibatchSize,
						stepSize,
						lambda,
						args[i%NUM_JOB_TYPES])
					);
			}
			totalSize += jobs[i%NUM_JOB_TYPES].m_totalSize;
		}

		for (uint32_t c = 0; c < cthreads.size(); c++) {
			cthreads[c]->join();
		}
		double total = get_time() - start;
		cout << "Total time: " << total << endl;
		cout << "Total size: " << totalSize << " GB" << endl;
		cout << "Processing rate: " << totalSize/total << " GB/s" << endl;
		for (uint32_t i = 0; i < cthreads.size(); i++) {
			delete cthreads[i];
		}
	}
	else {
#ifdef FPGA
		for (uint32_t i = 0; i < numJobsMultiplier*NUM_JOB_TYPES; i++) {
			columnML[i][0]->CreateMemoryLayout(RowStore, jobs[i%NUM_JOB_TYPES].m_partitionSize, false);
			for (uint32_t c = 1; c < jobs[i%NUM_JOB_TYPES].m_numClasses; c++) {
				if (c < iFPGA::MAX_NUM_BANKS) // Copy to new banks
					columnML[i][c]->UseCreatedMemoryLayout(columnML[i][0]);
				else // Use already copied data
					columnML[i][c]->UseCreatedMemoryLayout(columnML[i][c%iFPGA::MAX_NUM_BANKS]);
			}
			for (uint32_t c = 0; c < jobs[i%NUM_JOB_TYPES].m_numClasses; c++) {
				columnML[i][c]->fSGD_minibatch(
					jobs[i%NUM_JOB_TYPES].m_type,
					jobs[i%NUM_JOB_TYPES].m_numEpochs,
					jobs[i%NUM_JOB_TYPES].m_minibatchSize, stepSize, lambda, 0);
			}
		}

		for (uint32_t i = 0; i < numJobsMultiplier*NUM_JOB_TYPES; i++) {
			for (uint32_t c = 0; c < columnML[i].size(); c++) {
				server.PreCopy(columnML[i][c]);
			}
		}

		float totalSize = 0;
		double start = get_time();
		vector<FThread*> fthreads;

		for (uint32_t i = 0; i < numJobsMultiplier*NUM_JOB_TYPES; i++) {
			cout << "columnML[" << i << "].size(): " << columnML[i].size() << endl;
			for (uint32_t c = 0; c < columnML[i].size(); c++) {
				fthreads.push_back( server.Request(columnML[i][c], jobs[i%NUM_JOB_TYPES].m_priority) );
			}
			totalSize += jobs[i%NUM_JOB_TYPES].m_totalSize;
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
		for (uint32_t i = 0; i < NUM_JOB_TYPES; i++) {
			cout << "Job " << i << " convergence: " << endl;
			vector<thread*> threads(jobs[i].m_numClasses);
			for (uint32_t e = 0; e < jobs[i].m_numEpochs; e++) {

				vector<float> losses(jobs[i].m_numClasses);
				for (uint32_t c = 0; c < jobs[i].m_numClasses; c++) {

					auto output = iFPGA::CastToFloat(columnML[i][c]->m_outputHandle);
					float* xHistory = (float*)(output + 16);
					threads[c] =
						new thread(
							thread_Loss,
							&losses[c],
							columnML[i][0],
							jobs[i].m_type,
							xHistory + e*columnML[i][0]->m_alignedNumFeatures,
							lambda,
							args[i]);
				}
				float loss = 0.0;
				for (uint32_t c = 0; c < jobs[i].m_numClasses; c++) {
					threads[c]->join();
					delete threads[c];
					loss += losses[c];
				}
				cout << loss/jobs[i].m_numClasses << endl;
			}
		}
#endif
	}

	return 0;
}