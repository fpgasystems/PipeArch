#include <iostream>
#include "ColumnML.h"
#include "FPGA_ColumnML.h"
#include "Server.h"
#include <thread>

#define VALUE_TO_INT_SCALER 10

#define FPGA

void thread_SGD(
	uint32_t threadId,
	ColumnML* columnML,
	ModelType type, 
	float* xHistory, 
	uint32_t numEpochs, 
	uint32_t minibatchSize, 
	float stepSize, 
	float lambda, 
	AdditionalArguments args)
{
	columnML->AVX_SGD(type, xHistory, numEpochs, minibatchSize, stepSize, lambda, &args);
}

int main(int argc, char* argv[]) {

	char* pathToDataset;
	uint32_t numSamples = 0;
	uint32_t numFeatures = 0;
	uint32_t numEpochs = 10;
	uint32_t minibatchSize = 1;
	uint32_t numClasses = 2;
	if (!(argc == 7)) {
		cout << "Usage: ./app <pathToDataset> <numSamples> <numFeatures> <minibatchSize> <numEpochs> <numClasses>" << endl;
		return 0;
	}
	pathToDataset = argv[1];
	numSamples = atoi(argv[2]);
	numFeatures = atoi(argv[3]);
	minibatchSize = atoi(argv[4]);
	numEpochs = atoi(argv[5]);
	numClasses = atoi(argv[6]);

	uint32_t partitionSize = 16000;

#ifdef FPGA
	Server server(false, true);
	vector<FPGA_ColumnML*> columnML;
	for (uint32_t c = 0; c < numClasses; c++) {
		columnML.push_back(new FPGA_ColumnML(&server));
	}
#else
	vector<FPGA_ColumnML*> columnML;
	for (uint32_t c = 0; c < numClasses; c++) {
		columnML.push_back(new ColumnML());
	}
#endif

	float stepSize = 0.01;
	float lambda = 0.001;

	ModelType type;
	if ( strcmp(pathToDataset, "syn") == 0) {
		columnML[0]->m_cstore->GenerateSyntheticData(numSamples, numFeatures, false, MinusOneToOne);
		type = linreg;
	}
	else {
		columnML[0]->m_cstore->LoadRawData(pathToDataset, numSamples, numFeatures, true);
		columnML[0]->m_cstore->NormalizeSamples(ZeroToOne, column);
		if (numClasses > 2) {
			columnML[0]->m_cstore->PopulateOnehotLabels(numClasses);
		}
		else {
			columnML[0]->m_cstore->NormalizeLabels(ZeroToOne, true, 1);
		}
		type = logreg;
	}

	AdditionalArguments args;
	args.m_firstSample = 0;
	args.m_numSamples = columnML[0]->m_cstore->m_numSamples;
	args.m_constantStepSize = true;
	args.m_useOnehotLabels = false;

	// Set memory format / decide on SGD or SCD
	MemoryFormat format = RowStore;


	vector<float*> xHistories;
	xHistories.resize(numClasses);
	if (numClasses == 2) {
		columnML[0]->AVX_SGD(type, nullptr, numEpochs, minibatchSize, stepSize, lambda, &args);
	}
	else {
		vector<thread*> threads;
		for (uint32_t c = 0; c < numClasses; c++) {
			xHistories[c] = (float*)malloc(numEpochs*columnML[0]->m_cstore->m_numFeatures*sizeof(float));
			args.m_useOnehotLabels = true;
			args.m_class = c;
			threads.push_back(new thread(thread_SGD, c, columnML[0], type, xHistories[c], numEpochs, minibatchSize, stepSize, lambda, args));
		}
		for (uint32_t c = 0; c < numClasses; c++) {
			threads[c]->join();
			delete threads[c];
		}
		for (uint32_t c = 0; c < numClasses; c++) {
			cout << "Class " << c << endl;
			float* xHistory = xHistories[c];
			args.m_useOnehotLabels = true;
			args.m_class = c;
			for (uint32_t e = 0; e < numEpochs; e++) {
				cout << columnML[0]->Loss(type, xHistory + e*columnML[0]->m_cstore->m_numFeatures, lambda, &args) << endl;;
			}
			free(xHistories[c]);
		}
	}

#ifdef FPGA
	columnML[0]->CreateMemoryLayout(format, partitionSize, true);
	for (uint32_t c = 1; c < numClasses; c++) {
		columnML[c]->UseCreatedMemoryLayout(columnML[0]);
	}

	for (uint32_t c = 0; c < numClasses; c++) {
		if (minibatchSize == 1) {
			columnML[c]->fSGD(type, numEpochs, stepSize, lambda, c);
		}
		else {
			columnML[c]->fSGD_minibatch(type, numEpochs, minibatchSize, stepSize, lambda, c);
		}
	}

	vector<FThread*> fthreads(numClasses);
	for (uint32_t c = 0; c < numClasses; c++) {
		fthreads[c] = server.Request(columnML[c]);
	}
	for (uint32_t c = 0; c < numClasses; c++) {
		fthreads[c]->WaitUntilFinished();
	}

	// Verify
	for (uint32_t c = 0; c < numClasses; c++) {
		auto output = iFPGA::CastToFloat(columnML[c]->m_outputHandle);
		float* xHistory = (float*)(output + 16);
		args.m_useOnehotLabels = true;
		args.m_class = c;
		for (uint32_t e = 0; e < numEpochs; e++) {
			float loss = columnML[0]->Loss(type, xHistory + e*columnML[0]->m_alignedNumFeatures, lambda, &args);
			cout << "loss " << e << ": " << loss << endl;
		}
	}
#endif

	return 0;
}