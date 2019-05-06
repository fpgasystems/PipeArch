#include <iostream>
#include "ColumnML.h"
#include "FPGA_ColumnML.h"
#include "Server.h"

#define VALUE_TO_INT_SCALER 10

#define FPGA

int main(int argc, char* argv[]) {

	char* pathToDataset;
	uint32_t numSamples = 0;
	uint32_t numFeatures = 0;
	uint32_t numEpochs = 10;
	uint32_t partitionSize = 1;
	uint32_t numInstances = 1;
	if (!(argc == 7)) {
		cout << "Usage: ./app <pathToDataset> <numSamples> <numFeatures> <partitionSize> <numEpochs> <numInstances>" << endl;
		return 0;
	}
	pathToDataset = argv[1];
	numSamples = atoi(argv[2]);
	numFeatures = atoi(argv[3]);
	partitionSize = atoi(argv[4]);
	numEpochs = atoi(argv[5]);
	numInstances = atoi(argv[6]);

	if (numInstances > iFPGA::MAX_NUM_INSTANCES) {
		cout << "numInstances is larger than iFPGA::MAX_NUM_INSTANCES (" << iFPGA::MAX_NUM_INSTANCES << ")" << endl;
		return 1;
	}

#ifdef FPGA
	Server server(false, true);
	vector<FPGA_ColumnML*> columnML;
	for (uint32_t i = 0; i < numInstances; i++) {
		columnML.push_back( new FPGA_ColumnML(&server) );
	}
#else
	vector<ColumnML*> columnML;
	for (uint32_t i = 0; i < numInstances; i++) {
		columnML.push_back( new ColumnML() );
	}
#endif

	float stepSize = 1;
	float lambda = 0.001;

	ModelType type;
	if ( strcmp(pathToDataset, "syn") == 0) {
		columnML[0]->m_cstore->GenerateSyntheticData(numSamples, numFeatures, false, MinusOneToOne);
		type = linreg;
	}
	else {
		columnML[0]->m_cstore->LoadRawData(pathToDataset, numSamples, numFeatures, true);
		columnML[0]->m_cstore->NormalizeSamples(ZeroToOne, column);
		columnML[0]->m_cstore->NormalizeLabels(ZeroToOne, true, 1);
		type = logreg;
	}
	columnML[0]->m_cstore->PrintSamples(2);

	AdditionalArguments args;
	args.m_firstSample = 0;
	args.m_numSamples = columnML[0]->m_cstore->m_numSamples;
	args.m_constantStepSize = true;

	// Set memory format / decide on SGD or SCD
	MemoryFormat format = ColumnStore;

	columnML[0]->SCD(type, nullptr, numEpochs, partitionSize, stepSize, lambda, 1000, false, false, VALUE_TO_INT_SCALER, &args);

#ifdef FPGA
	columnML[0]->CreateMemoryLayout(format, partitionSize, 1);
	for (uint32_t i = 1; i < numInstances; i++) {
		columnML[i]->UseCreatedMemoryLayout(columnML[0]);
	}

	vector<uint32_t> numPartitionsPerInstance(numInstances);
	for (uint32_t i = 0; i < numInstances; i++) {
		if (i == numInstances-1) {
			numPartitionsPerInstance[i] = columnML[0]->m_numPartitions - i*(columnML[0]->m_numPartitions/numInstances);
		}
		else {
			numPartitionsPerInstance[i] = columnML[0]->m_numPartitions/numInstances;
		}
		cout << "numPartitionsPerInstance[" << i << "]: " << numPartitionsPerInstance[i] << endl;

		if (numPartitionsPerInstance[i] > 0) {
			columnML[i]->fSCD(i*(columnML[0]->m_numPartitions/numInstances), numPartitionsPerInstance[i], type, 1, stepSize, lambda);
		}
	}
#endif

#ifdef FPGA
	vector<vector<float> > modelsPerEpoch;
	modelsPerEpoch.reserve(numEpochs);
	for (uint32_t e = 0; e < numEpochs; e++) {
		vector<FThread*> fthreads(numInstances);
		for (uint32_t i = 0; i < numInstances; i++) {
			if (numPartitionsPerInstance[i] > 0) {
				fthreads[i] = server.Request(columnML[i]);
			}
		}
		for (uint32_t i = 0; i < numInstances; i++) {
			if (numPartitionsPerInstance[i] > 0) {
				fthreads[i]->WaitUntilFinished();
			}
		}
		modelsPerEpoch.push_back(columnML[0]->GetModelSCD(0));
	}

	// Verify
	for (uint32_t e = 0; e < numEpochs; e++) {
		float loss = columnML[0]->Loss(type, modelsPerEpoch[e].data(), lambda, &args);
		cout << "loss " << e << ": " << loss << endl;
	}
#endif

	return 0;
}