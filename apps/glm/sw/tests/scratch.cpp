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
	uint32_t minibatchSize = 1;
	if (!(argc == 6)) {
		cout << "Usage: ./app <pathToDataset> <numSamples> <numFeatures> <minibatchSize> <numEpochs>" << endl;
		return 0;
	}
	pathToDataset = argv[1];
	numSamples = atoi(argv[2]);
	numFeatures = atoi(argv[3]);
	minibatchSize = atoi(argv[4]);
	numEpochs = atoi(argv[5]);

	uint32_t partitionSize = 16000;

#ifdef FPGA
	xDevice xdevice;
	Server server(false, true, 0, &xdevice);
	FPGA_ColumnML* columnML = new FPGA_ColumnML(&server);
#else
	ColumnML* columnML = new ColumnML();
#endif

	float stepSize;
	float lambda = 0.001;

	ModelType type;
	if ( strcmp(pathToDataset, "syn") == 0) {
		columnML->m_cstore->GenerateSyntheticData(numSamples, numFeatures, false, MinusOneToOne);
		type = linreg;
	}
	else {
		columnML->m_cstore->LoadRawData(pathToDataset, numSamples, numFeatures, true);
		columnML->m_cstore->NormalizeSamples(ZeroToOne, column);
		columnML->m_cstore->NormalizeLabels(ZeroToOne, true, 1);
		type = logreg;
	}

	AdditionalArguments args;
	args.m_firstSample = 0;
	args.m_numSamples = columnML->m_cstore->m_numSamples;
	args.m_constantStepSize = true;
	args.m_useOnehotLabels = false;

	// Set memory format / decide on SGD or SCD
	MemoryFormat format = RowStore;

	if (format == RowStore) {
		stepSize = 0.001;
		columnML->SGD(type, nullptr, numEpochs, minibatchSize, stepSize, lambda, &args);

#ifdef FPGA
		columnML->CreateMemoryLayout(format, partitionSize);
		server.PreCopy(columnML);
		if (minibatchSize == 1) {
			columnML->fSGD(type, numEpochs, stepSize, lambda, 0);
		}
		else {
			columnML->fSGD_minibatch(type, numEpochs, minibatchSize, stepSize, lambda, 0);
		}
#endif
	}
	else {
		stepSize = 1;
		columnML->SCD(type, nullptr, numEpochs, partitionSize, stepSize, lambda, 1000, false, false, VALUE_TO_INT_SCALER, &args);

#ifdef FPGA
		columnML->CreateMemoryLayout(format, partitionSize, numEpochs, false);
		server.PreCopy(columnML);
		columnML->fSCD(0, columnML->m_numPartitions, type, numEpochs, stepSize, lambda);
#endif
	}

#ifdef FPGA
	FThread* fthread = server.Request(columnML);
	fthread->WaitUntilFinished();

	double total = fthread->GetResponseTime();
	cout << "Time per epoch: " << total/numEpochs << endl;
	cout << "Processing rate: " << ((float)columnML->m_cstore->m_numSamples*(float)columnML->m_cstore->m_numFeatures*4*numEpochs)/total/1e9;
	cout << " GB/s" << endl;

	// Verify
	if (format == RowStore) {
		auto output = iFPGA::CastToFloat(columnML->m_outputHandle);
		float* xHistory = (float*)(output + 16);
		for (uint32_t e = 0; e < numEpochs; e++) {
			float loss = columnML->Loss(type, xHistory + e*columnML->m_alignedNumFeatures, lambda, &args);
			cout << "loss " << e << ": " << loss << endl;
		}
	}
	else {
#ifdef XILINX
		server.GetInputHandleFromFPGA(columnML);
#endif
		for (uint32_t e = 0; e < numEpochs; e++) {
			float loss = columnML->Loss(type, columnML->GetModelSCD(e).data(), lambda, &args);
			cout << "loss " << e << ": " << loss << endl;
		}
	}
#endif

	delete columnML;

	return 0;
}
