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
	uint32_t sw0hw1 = 0;
	if (!(argc == 8)) {
		cout << "Usage: ./app <pathToDataset> <numSamples> <numFeatures> <partitionSize> <numEpochs> <numInstances> <sw0hw1>" << endl;
		return 0;
	}
	pathToDataset = argv[1];
	numSamples = atoi(argv[2]);
	numFeatures = atoi(argv[3]);
	partitionSize = atoi(argv[4]);
	numEpochs = atoi(argv[5]);
	numInstances = atoi(argv[6]);
	sw0hw1 = atoi(argv[7]);

#ifdef FPGA
	xDevice xdevice;
	Server* server[iFPGA::MAX_NUM_BANKS];
	for (uint32_t i = 0; i < iFPGA::MAX_NUM_BANKS; i++) {
		server[i] = new Server(false, false, i, &xdevice);
	}
	vector<FPGA_ColumnML*> columnML;
	for (uint32_t i = 0; i < numInstances; i++) {
		columnML.push_back( new FPGA_ColumnML(server[i%iFPGA::MAX_NUM_BANKS]) );
	}
#else
	vector<ColumnML*> columnML;
	for (uint32_t i = 0; i < numInstances; i++) {
		columnML.push_back( new ColumnML() );
	}
#endif

	float stepSize = 4;
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

	AdditionalArguments args;
	args.m_firstSample = 0;
	args.m_numSamples = columnML[0]->m_cstore->m_numSamples;
	args.m_constantStepSize = true;
	args.m_useOnehotLabels = false;

	// Set memory format / decide on SGD or SCD
	MemoryFormat format = ColumnStore;

	if (sw0hw1 == 0) {
		// columnML[0]->SCD(type, nullptr, numEpochs, partitionSize, stepSize, lambda, 1000, false, false, VALUE_TO_INT_SCALER, &args);
		columnML[0]->AVXmulti_SCD(type, false, nullptr, numEpochs, partitionSize, stepSize, lambda, 1000, false, false, VALUE_TO_INT_SCALER, &args, numInstances);
	}
	else {
#ifdef FPGA

		columnML[0]->CreateMemoryLayout(format, partitionSize);
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

		vector<vector<float> > modelsPerEpoch;
		modelsPerEpoch.reserve(numEpochs);

		for (uint32_t i = 0; i < numInstances; i++) {
			server[i%iFPGA::MAX_NUM_BANKS]->PreCopy(columnML[i]);
		}

		vector<FThread*> fthreads(numInstances);

		double total = 0.0;
		for (uint32_t e = 0; e < numEpochs; e++) {
			double start = get_time();
			for (uint32_t i = 0; i < numInstances; i++) {
				if (numPartitionsPerInstance[i] > 0) {
					fthreads[i] = server[i%iFPGA::MAX_NUM_BANKS]->Request(columnML[i]);
				}
			}
			for (uint32_t i = 0; i < numInstances; i++) {
				if (numPartitionsPerInstance[i] > 0) {
					fthreads[i]->WaitUntilFinished();
				}
			}
			double end = get_time();
			total += end - start;
			vector<float> avgModel(columnML[0]->m_alignedNumFeatures, 0);
			for (uint32_t i = 0; i < numInstances; i++) {
#ifdef XILINX
				server[i%iFPGA::MAX_NUM_BANKS]->GetInputHandleFromFPGA(columnML[i]);
#endif
				vector<float> temp = columnML[i]->GetModelSCD(0, i*(columnML[0]->m_numPartitions/numInstances), numPartitionsPerInstance[i]);
				for (uint32_t j = 0; j < columnML[0]->m_alignedNumFeatures; j++) {
					avgModel[j] += temp[j];
				}
			}
			for (uint32_t j = 0; j < columnML[0]->m_alignedNumFeatures; j++) {
				avgModel[j] /= numInstances;
			}
			modelsPerEpoch.push_back(avgModel);
		}
		cout << "Avg time per epoch: " << total/numEpochs << endl;
		cout << "Processing rate: " << (numEpochs*columnML[0]->GetDataSize())/total/1e9 << "GB/s" << endl;

		// Verify
		for (uint32_t e = 0; e < numEpochs; e++) {
			float loss = columnML[0]->Loss(type, modelsPerEpoch[e].data(), lambda, &args);
			cout << loss << endl;
		}
#endif
	}

	for (uint32_t i = 0; i < numInstances; i++) {
		delete columnML[i];
	}
	for (uint32_t i = 0; i < iFPGA::MAX_NUM_BANKS; i++) {
		delete server[i];
	}

	return 0;
}